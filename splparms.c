/*
 * splparms.c
 *
 * Copyright (C) 2014 Agere Systems Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <linux/limits.h>
#include <unistd.h>
#include <stropts.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#define __user
#include <mtd/mtd-user.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "util.h"
#include "config.h"

/*
  ==============================================================================
  Data types and Macros
  ==============================================================================
*/


/*
  ==============================================================================
  Local
  ==============================================================================
*/

static int watchdog_timeout;


typedef struct {
	uint32_t magic;
	uint32_t size;
	uint32_t checksum;
	uint32_t version;
	uint32_t chipType;
	uint32_t globalOffset;
	uint32_t globalSize;
	uint32_t pciesrioOffset;
	uint32_t pciesrioSize;
	uint32_t voltageOffset;
	uint32_t voltageSize;
	uint32_t clocksOffset;
	uint32_t clocksSize;
	uint32_t systemMemoryOffset;
	uint32_t systemMemorySize;
	uint32_t classifierMemoryOffset;
	uint32_t classifierMemorySize;
	uint32_t systemMemoryRetentionOffset;
	uint32_t systemMemoryRetentionSize;
} __attribute__((packed)) parameters_header_t;

typedef struct {
	unsigned version;
	unsigned flags;
	unsigned baud_rate;
	unsigned memory_ranges[16];
	unsigned long sequence;
	char description[128];
} __attribute__((packed)) parameters_global_t;

/*
  ------------------------------------------------------------------------------
  splparms_read

  NOTE: This function will allocate memory for the output; remember to free it!
*/

static int
splparms_read(void **output, const char *device)
{
	struct mtd_info_user splparms_info;
	parameters_header_t *header;
	int i;
	uint32_t *temp;
	uint32_t size;

	if (0 != get_mtd_partition_info(device, &splparms_info)) {
		fprintf(stderr, "Error getting MTD partition information!\n");

		return -1;
	}

	*output = malloc(splparms_info.size);

	if (NULL == *output) {
		fprintf(stderr, "Error allocating memory!\n");

		return -1;
	}

	if (0 != get_mtd_partition(*output, splparms_info.size, device)) {
		free(*output);
		fprintf(stderr, "Error getting MTD partition!\n");

		return -1;
	}

	header = (parameters_header_t *)(*output);
	size = ntohl(header->size);

	if (PARAMETERS_MAGIC != ntohl(header->magic)) {
		fprintf(stderr, "Magic number is wrong: 0x%08x != 0x%08x\n",
			PARAMETERS_MAGIC, ntohl(header->magic));
		free(*output);

		return -1;
	}

	if (ntohl(header->checksum) !=
	    get_crc32((*output + 12), (size - 12))) {
		fprintf(stderr, "Bad Checksum: 0x%08x != 0x%08x\n",
			ntohl(header->checksum),
			get_crc32((*output + 12), (size - 12)));

		return -1;
	}

	temp = *output;

	for (i = 0; i < (size / 4); ++i, ++temp)
		*temp = ntohl(*temp);

	return 0;
}

/*
  ------------------------------------------------------------------------------
  splparms_write
*/

static int
splparms_write(const char *device, unsigned long sequence, const char *input)
{
	struct erase_info_user erase;
	int mtd_fd = 0;
	struct stat input_stat;
	unsigned long input_size;
	FILE *parameter_file = NULL;
	void *parameters = NULL;
	parameters_header_t *header;
	parameters_global_t *global;
	unsigned long *temp;
	int i;
	struct mtd_info_user mtd_info;

	if (0 != get_mtd_partition_info(device, &mtd_info)) {	
		fprintf(stderr, "Error Getting MTD Info!\n");
		goto cleanup;
	}

	if (0 != stat(input, &input_stat)) {
		fprintf(stderr, "Error reading %s: %s\n",
			input, strerror(errno));
		goto cleanup;
	}

	input_size = input_stat.st_size;

	if (NULL == (parameter_file = fopen(input, "rb"))) {
		fprintf(stderr, "Error opening %s: %s\n",
			input, strerror(errno));
		goto cleanup;
	}

	parameters = malloc(input_size);

	if (input_size != fread(parameters, 1, input_size, parameter_file)) {
		fprintf(stderr, "Error reading %s: %s\n",
			input, strerror(errno));
		goto cleanup;
	}

	header = (parameters_header_t *)parameters;

	if (PARAMETERS_MAGIC != ntohl(header->magic)) {
		fprintf(stderr, "Bad Input Magic!\n");

		goto cleanup;
	}
	
	if (ntohl(header->checksum) !=
	    get_crc32((parameters + 12), (ntohl(header->size) - 12))) {
		fprintf(stderr, "Bad Input Checksum!\n");
		goto cleanup;
	}

	global = (parameters + ntohl(header->globalOffset));
	global->sequence = htonl(sequence);
	header->checksum =
		htonl(get_crc32((void *)(parameters + 12),
				(ntohl(header->size) - 12)));
	mtd_fd = open(device, O_RDWR);

	if (0 > (mtd_fd = open(device, O_RDWR))) {
		fprintf(stderr, "Error opening %s: %s\n",
			device, strerror(errno));
		goto cleanup;
	}

	erase.start = 0;
	erase.length = mtd_info.size;

	if (0 > ioctl(mtd_fd, MEMERASE, &erase)) {
		fprintf(stderr, "Error erasing %s: %s\n",
			device, strerror(errno));
		goto cleanup;
	}

	if (0 > lseek(mtd_fd, 0, SEEK_SET)) {
		fprintf(stderr, "Error rewinding %s: %s\n",
			device, strerror(errno));
		goto cleanup;
	}

	if (input_size != write(mtd_fd, parameters, input_size)) {
		fprintf(stderr, "Error writing %s: %s\n",
			device, strerror(errno));
		goto cleanup;
	}

cleanup:

	if (NULL != parameters)
		free(parameters);
	
	if (NULL != parameter_file)
		fclose(parameter_file);

	if (0 != mtd_fd)
		close(mtd_fd);

	return 0;
}

/*
  ------------------------------------------------------------------------------
  splparms_erase
*/

static int
splparms_erase(const char *device)
{
	struct mtd_info_user mtd_info;
	int mtd_fd = 0;
	struct erase_info_user erase;

	if (0 != get_mtd_partition_info(device, &mtd_info)) {	
		fprintf(stderr, "Error Getting MTD Info!\n");
		goto cleanup;
	}

	mtd_fd = open(device, O_RDWR);

	if (0 > mtd_fd) {
		fprintf(stderr, "Error opening %s: %s\n",
			device, strerror(errno));

		return -1;
	}

	erase.start = 0;
	erase.length = mtd_info.size;

	if (0 > ioctl(mtd_fd, MEMERASE, &erase)) {
		fprintf(stderr, "Error erasing %s: %s\n",
			device, strerror(errno));
		goto cleanup;
	}

cleanup:

	if (0 != mtd_fd)
		close(mtd_fd);

	return 0;
}

/*
  ------------------------------------------------------------------------------
  splparms_select
*/

static int
splparms_select(int *selected, int verbose)
{
	void *a;
	void *b;
	parameters_header_t *header_a;
	parameters_header_t *header_b;
	parameters_global_t *global_a;
	parameters_global_t *global_b;
	int a_valid = 0;
	int b_valid = 0;

	if (0 == splparms_read(&a, SPLPARMS_DEVICE_A)) {
		a_valid = 1;
		header_a = (parameters_header_t *)a;
		global_a = (parameters_global_t *)((uint8_t *)a + header_a->globalOffset);
	}

	if (0 != verbose) {
		if (0 != a_valid)
			printf("A: sequence: 0x%x description: %s\n",
			       global_a->sequence, global_a->description);
		else
			printf("A: INVALID\n");
	}

	if (0 == splparms_read(&b, SPLPARMS_DEVICE_B)) {
		b_valid = 1;
		header_b = (parameters_header_t *)b;
		global_b = (parameters_global_t *)((uint8_t *)b + header_b->globalOffset);
	}

	if (0 != verbose) {
		if (0 != b_valid)
			printf("B: sequence: 0x%x description: %s\n",
			       global_b->sequence, global_b->description);
		else
			printf("B: INVALID\n");
	}

	if (0 == a_valid && 0 != b_valid) {
		*selected = 1;
	} else if (0 != a_valid && 0 == b_valid) {
		*selected = 0;
	} else {
		if (0xffffffff == global_a->sequence &&
		    global_b->sequence == 0) {
			*selected = 1;
		} else if (global_b->sequence > global_a->sequence) {
			*selected = 1;
		} else {
			*selected = 0;
		}
	}

	if (0 != watchdog_timeout)
		*selected ^= 1;

	if (0 != verbose)
		printf("Selected Parameters %s\n",
		       (0 == *selected) ? "A" : "B");

	if (0 != a_valid)
		free(a);

	if (0 != b_valid)
		free(b);

	return 0;
}

/*
  ------------------------------------------------------------------------------
  splparms_update
*/

static int
splparms_update(const char *parameters)
{
	int selected;
	void *current;
	char *current_device;
	char *next_device;
	parameters_header_t *header;
	parameters_global_t *global;
	unsigned long sequence;

	if (0 != splparms_select(&selected, 0)) {
		fprintf(stderr, "Can't determine selected parameters.\n");

		return -1;
	}

	current_device = (0 == selected) ? SPLPARMS_DEVICE_A : SPLPARMS_DEVICE_B;
	next_device = (0 != selected) ? SPLPARMS_DEVICE_A : SPLPARMS_DEVICE_B;

	if (0 != splparms_read(&current, current_device)) {
		fprintf(stderr, "Can't read current parameters.\n");

		return -1;
	}

	header = (parameters_header_t *)current;
	global = (parameters_global_t *)(current + header->globalOffset);

	if (0xffffffff == global->sequence)
		sequence = 0;
	else
		sequence = global->sequence + 1;

	if (0 != splparms_write(next_device, sequence, parameters)) {
		fprintf(stderr, "Faild writing new parameters.\n");

		return -1;
	}

	return 0;
}

/*
  ------------------------------------------------------------------------------
  usage
*/

static void
usage(int exit_code)
{
	fprintf(stderr,
		"Usage\n"
		"splparms [-e A|B] [-h] [-i] [-t] [-u file]\n"
		"     [-w A|B sequence file]\n"
		"\t-e : erase parameters\n"
		"\t-h : display this help message\n"
		"\t-i : display parameter info\n"
		"\t-t : last reset was caused by a watchdog timeout\n"
		"\t-u : update parameter file\n"
		"\t-w : write a parameter file\n");

	exit(exit_code);
}

/*
  ==============================================================================
  Public
  ==============================================================================
*/

/*
  ------------------------------------------------------------------------------
  main
*/

int
main(int argc, char *argv[])
{
	int save = 0;
	int long_option = 0;
	int option;
	char *file_name = NULL;
	char *device = NULL;
	unsigned long sequence;
	char *name;
	char *value;
	int image;
	char action;

	struct option long_options[] = {
		{"erase", no_argument, &long_option, 'E'},
		{"help", no_argument, &long_option, 'H'},
		{"info", no_argument, &long_option, 'I'},
		{"timeout", no_argument, &long_option, 'T'},
		{"update", no_argument, &long_option, 'U'},
		{"write", no_argument, &long_option, 'W'},
		{0, 0, 0, 0}
	};

	watchdog_timeout = 0;

	/*
	  Handle Options
	*/

	while (-1 != (option =
		      getopt_long_only(argc, argv, "", long_options, NULL))) {
		switch (option) {
		case 0:
			switch(long_option) {
			case 'H':
				usage(EXIT_SUCCESS);
				break;

			case 'T':
				watchdog_timeout = 1;
				break;

			case 'I':
			case 'E':
			case 'U':
			case 'W':
				action = long_option;
				break;

			default:
				usage(EXIT_FAILURE);
				break;
			}
			break;

		default:
			usage(EXIT_FAILURE);
			break;
		}
	}

	switch (action) {
	case 'I':
		if (0 != splparms_select(&image, 1))
			printf("Selection Failed!\n");
		break;

	case 'E':
		if (3 != argc) {
			fprintf(stderr,
				"update requires A|B!\n");
			usage(EXIT_FAILURE);
		}

		if (1 != strlen(argv[2])) {
			fprintf(stderr, "Bank must be either A or B!\n");
			usage(EXIT_FAILURE);
		}

		if ('A' == toupper(*(argv[2]))) {
			device = SPLPARMS_DEVICE_A;
		} else if ('B' == toupper(*(argv[2]))) {
			device = SPLPARMS_DEVICE_B;
		} else {
			fprintf(stderr, "Bank must be either A or B!\n");
			usage(EXIT_FAILURE);
		}

		if (0 != splparms_erase(device)) {
			fprintf(stderr, "Erase Failed!\n");

			return EXIT_FAILURE;
		}

		break;

	case 'U':
		if (3 != argc) {
			fprintf(stderr,
				"update requires file!\n");
			usage(EXIT_FAILURE);
		}

		file_name = argv[2];

		if (0 != splparms_update(file_name))	{
			fprintf(stderr, "Update Failed!\n");

			return EXIT_FAILURE;
		}
		break;

	case 'W':
		if (5 != argc) {
			fprintf(stderr,
				"write requires a|b, sequence, and file!\n");
			usage(EXIT_FAILURE);
		}

		if (1 != strlen(argv[2])) {
			fprintf(stderr, "Bank must be either A or B!\n");
			usage(EXIT_FAILURE);
		}

		if ('A' == toupper(*(argv[2]))) {
			device = SPLPARMS_DEVICE_A;
		} else if ('B' == toupper(*(argv[2]))) {
			device = SPLPARMS_DEVICE_B;
		} else {
			fprintf(stderr, "Bank must be either A or B!\n");
			usage(EXIT_FAILURE);
		}

		sequence = strtoul(argv[3], NULL, 0);

		file_name = argv[4];

		if (0 != splparms_write(device, sequence, file_name)) {
			fprintf(stderr, "Write Failed!\n");

			return EXIT_FAILURE;
		}
		break;

	default:
		break;
	}

	return EXIT_SUCCESS;
}
