/*
 * ubootimages.c
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307	 USA
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
  Local
  ==============================================================================
*/

typedef struct environment {
	uint32_t crc32;
	uint32_t flags;
	uint32_t size;
	void *data;
} __attribute__ ((packed))environment_t;



static environment_t environment_a;
static environment_t environment_b;
static environment_t *environment_ = (environment_t *)0;
static void *a;
static void *b;



struct image_header {
	uint32_t ih_magic;	/* Image Header Magic Number	*/
	uint32_t ih_hcrc;	/* Image Header CRC Checksum	*/
	uint32_t ih_time;	/* Image Creation Timestamp	*/
	uint32_t ih_size;	/* Image Data Size		*/
	uint32_t ih_load;	/* Data	 Load  Address		*/
	uint32_t ih_ep;		/* Entry Point Address		*/
	uint32_t ih_dcrc;	/* Image Data CRC Checksum	*/
	uint32_t ih_os;		/* Operating System		*/
	uint32_t ih_arch;	/* CPU architecture		*/
	uint32_t ih_type;	/* Image Type			*/
	uint32_t ih_comp;	/* Compression Type		*/
	uint32_t ih_name[IH_NMLEN]; /* Image Name		*/
};





static const char *sequence_env[] = {
	"uboot_a_sequence",
	"uboot_b_sequence"
};

static const char *image_name[] = {
	"A",
	"B"
};

static int watchdog_timeout;

/*
  ------------------------------------------------------------------------------
  ubenv_initialize
*/

static int
ubenv_initialize(void)
{
	int size_a_;
	int size_b_;
	uint32_t crc32_a_;
	uint32_t crc32_b_;
	struct mtd_info_user mtd_info_a_;
	struct mtd_info_user mtd_info_b_;
	

	get_mtd_partition_info(FLASH_DEVICE_A, &mtd_info_a_);
	get_mtd_partition_info(FLASH_DEVICE_B, &mtd_info_b_);
	environment_a.size = mtd_info_a_.size;
	environment_b.size = mtd_info_b_.size;

	if (environment_a.size != environment_b.size) {
		fprintf(stderr, "Partitions must be the same size!\n");

		return -1;
	}

	if (NULL == (a = malloc(environment_a.size))) {
		fprintf(stderr, "Unable to allocate %d bytes\n",
			environment_a.size);

		return -1;
	}

	if (NULL == (b = malloc(environment_b.size))) {
		fprintf(stderr, "Unable to allocate %d bytes\n",
			environment_b.size);
		free(a);

		return -1;
	}

	if (0 != get_mtd_partition(a, environment_a.size, FLASH_DEVICE_A)) {
		fprintf(stderr, "Unable to read %s\n", FLASH_DEVICE_A);
		free(a);
		free(b);

		return -1;
	}

	if (0 != get_mtd_partition(b, environment_b.size, FLASH_DEVICE_B)) {
		fprintf(stderr, "Unable to read %s\n", FLASH_DEVICE_B);
		free(a);
		free(b);


		return -1;
	}

	environment_a.crc32 = *((uint32_t *)a);
	environment_b.crc32 = *((uint32_t *)b);
	environment_a.flags = *((uint32_t *)(a + 4));
	environment_b.flags = *((uint32_t *)(b + 4));
	environment_a.data = (uint32_t *)(a + 8);
	environment_b.data = (uint32_t *)(b + 8);

	crc32_a_ = get_crc32(environment_a.data,
			     ENVIRONMENT_DATA_SIZE(environment_a.size));
	crc32_b_ = get_crc32(environment_b.data,
			     ENVIRONMENT_DATA_SIZE(environment_b.size));

	if ((crc32_a_ == environment_a.crc32) &&
	    (crc32_b_ != environment_b.crc32)) {
		/* Use a_ */
		environment_ = &environment_a;
	} else if ((crc32_a_ != environment_a.crc32) &&
		   (crc32_b_ == environment_b.crc32)) {
		/* Use b_ */
		environment_ = &environment_b;
	} else if ((crc32_a_ != environment_a.crc32) &&
		   (crc32_b_ != environment_b.crc32)) {
		/* No Environment Available */
		fprintf(stderr, "No Valid U-Boot Environment Found!\n");

		return -1;
	} else if ((environment_a.flags != 0) &&
		   (environment_b.flags == 0)) {
		/* Use a_ */
		environment_ = &environment_a;
	} else if ((environment_a.flags == 0) &&
		   (environment_b.flags != 0)) {
		/* Use b_ */
		environment_ = &environment_b;
	} else if (environment_a.flags == environment_b.flags) {
		/* Use Either */
		environment_ = &environment_a;
	} else {
		/* No Environment Available */
		fprintf(stderr, "No Valid U-Boot Environment Found!\n");

		return -1;
	}

	return 0;
}

/*
  ------------------------------------------------------------------------------
  ubenv_finalize
*/

static void
ubenv_finalize(int save)
{
	if (0 != save) {
		int fd_a_ = -1;
		int fd_b_ = -1;
		int *fd_old_;
		int *fd_new_;
		struct erase_info_user erase_;

		if (0 > (fd_a_ = open(FLASH_DEVICE_A, O_RDWR))) {
			fprintf(stderr, "Unable to open %s : %s\n",
				FLASH_DEVICE_A, strerror(errno));

			goto done;
		}

		if (0 > (fd_b_ = open(FLASH_DEVICE_B, O_RDWR))) {
			fprintf(stderr, "Unable to open %s : %s\n",
				FLASH_DEVICE_B, strerror(errno));

			goto done;
		}

		if (environment_ == & environment_a) {
			fd_new_ = &fd_b_;
			fd_old_ = &fd_a_;
		} else if (environment_ == &environment_b) {
			fd_new_ = &fd_a_;
			fd_old_ = &fd_b_;
		} else {
			fprintf(stderr,
				"Not Saving, environment_ is not valid!\n");

			goto done;
		}

		/*
		  Erase the new environment.
		*/

		erase_.start = 0;
		erase_.length = environment_->size;

		if (0 > ioctl(*fd_new_, MEMERASE, &erase_)) {
			fprintf(stderr, "Error erasing %s : %s\n",
				FLASH_DEVICE_B, strerror(errno));

			goto done;
		}

		/*
		  Write the new environment.
		*/

		if (0 > (lseek(*fd_new_, 0, SEEK_SET))) {
			fprintf(stderr, "Seek Failed: %s\n", strerror(errno));

			goto done;
		}

		environment_->crc32 =
			get_crc32(environment_->data,
				  ENVIRONMENT_DATA_SIZE(environment_->size));

		if (sizeof(uint32_t) !=
		    write(*fd_new_, (void *)&(environment_->crc32),
			  sizeof(uint32_t))) {
			fprintf(stderr, "Write Failed: %s\n", strerror(errno));

			goto done;
		}

		environment_->flags = 1;

		if (sizeof(uint32_t) !=
		    write(*fd_new_, (void *)&(environment_->flags),
			  sizeof(uint32_t))) {
			fprintf(stderr, "Write Failed: %s\n", strerror(errno));

			goto done;
		}

		if (ENVIRONMENT_DATA_SIZE(environment_->size) !=
		    write(*fd_new_, (void *)(environment_->data),
			  ENVIRONMENT_DATA_SIZE(environment_->size))) {
			fprintf(stderr, "Write Failed: %s\n", strerror(errno));

			goto done;
		}

		close(*fd_new_);
		*fd_new_ = -1;

		/*
		  Set the obsolete flag in the old environment.
		*/

		if (0 > (lseek(*fd_old_, sizeof(uint32_t), SEEK_SET))) {
			fprintf(stderr, "Seek Failed: %s\n", strerror(errno));

			goto done;
		}

		environment_->flags = 0;

		if (sizeof(uint32_t) != write(*fd_old_, (void *)&environment_->flags, sizeof(uint32_t))) {
			fprintf(stderr, "Write Failed : %s\n", strerror(errno));
			goto done;
		}

	done:
		if (-1 != fd_a_)
			close(fd_a_);

		if (-1 != fd_b_)
			close(fd_b_);
	}

	free(a);
	free(b);

	return;
}

/*
  ------------------------------------------------------------------------------
  ubenv_add
*/

static int
ubenv_add(const char *name, const char *value)
{
	char *string;

	string = environment_->data;

	while (0x00 != string[0])
		string += (strlen(string) + 1);

	if (environment_->size >=
	    (strlen(environment_->data) + strlen(name) + strlen(value) + 4)) {
		memcpy(string, name, strlen(name));
		string += strlen(name);
		memcpy(string, "=", 1);
		++string;
		memcpy(string, value, strlen(value));
	}

	return 0;
}

/*
  ------------------------------------------------------------------------------
  ubenv_add_file
*/

static int
ubenv_add_file(const char *file_name)
{
	FILE *file_;
	char line_[INPUT_BUFFER_SIZE];

	/*
	
       Clear the existing environment.
	*/

	memset(environment_->data, 0,
	       ENVIRONMENT_DATA_SIZE(environment_->size));

	/*
	  Open the file.
	*/

	if (NULL == (file_ = fopen(file_name, "r"))) {
		fprintf(stderr, "Unable to open \"%s\": %s\n",
			file_name, strerror(errno));

		return -1;
	}

	/*
	  Write each line.
	*/

	while (NULL != fgets(line_, INPUT_BUFFER_SIZE, file_)) {
		char *name_;
		char *value_;

		line_ [strlen(line_) - 1] = 0x00;
		name_ = line_;

		if ((char *)0 != (value_ = strchr(line_, '='))) {
			*value_ = 0x00;
			++value_;
			value_[strlen(value_)] = 0;
			ubenv_add(name_, value_);
		}
	}

	/*
	  Close the file.
	*/

	fclose(file_);

	return 0;
}

/*
  ------------------------------------------------------------------------------
  ubenv_remove
*/

static int
ubenv_remove(const char *name)
{
	char *new_;
	char *old_;
	char *from_;
	char *to_;
	char line_[INPUT_BUFFER_SIZE];
	char *name_;
	char *value_;

	if (NULL == (new_ =
		     malloc(ENVIRONMENT_DATA_SIZE(environment_->size)))) {
		fprintf(stderr, "Allocation Failed!\n");
		return -1;
	}

	memset(new_, 0, ENVIRONMENT_DATA_SIZE(environment_->size));
	old_ = environment_->data;
	from_ = old_;
	to_ = new_;

	while (0x00 != from_[0]) {
		strcpy(line_, from_);
		name_ = line_;

		if (NULL != (value_ = strchr(line_, '='))) {
			*value_ = 0;
			++value_;

			if ((strlen(name_) != strlen(name)) ||
			    (0 != strcmp(name, name_))) {
				memcpy(to_, from_, (strlen(from_) + 1));
				to_  += strlen(from_) + 1;
			}
		}

		from_ += (strlen(from_) + 1);
	}

	memcpy(old_, new_, ENVIRONMENT_DATA_SIZE(environment_->size));
	free(new_);

	return 0;

}

/*
  ------------------------------------------------------------------------------
  ubenv_get
*/

static char *
ubenv_get(const char *name)
{
	char *string;
	char *value = (char *)0;

	string = environment_->data;

	while (0x00 != string[0]) {
		if (0 == strncmp(name, string, strlen(name))) {
			char *temp = strchr(string, '=');
			++temp;
			value = strdup(temp);
			break;
		}

		string += strlen(string) + 1;
	}

	return value;
}

/*
  ------------------------------------------------------------------------------
  ubenv_display
*/

static int
ubenv_display(const char *name)
{
	char *string;

	string = environment_->data;

	while (0x00 != string[0]) {
		if (0 == strncmp(name, string, strlen(name)))
			printf("%s\n", string);

		string += (strlen(string) + 1);
	}

	return 0;
}

/*
  ------------------------------------------------------------------------------
  ubenv_print
*/

static int
ubenv_print(void)
{
	char *string;

	string = environment_->data;

	while (0x00 != string[0]) {
		printf("%s\n", string);
		string += (strlen(string) + 1);
	}

	return 0;
}

/*
  ------------------------------------------------------------------------------
  uboot_read
*/

static int
uboot_read(void **output, const char *device)
{
	struct mtd_info_user mtd_info;
	struct image_header *header;
	char sbb_image_name[] = "ubootimages_XXXXXX";
	int sbb_image_fd;
	FILE *sbb_image_file;

	get_mtd_partition_info(device, &mtd_info);
	*output = malloc(mtd_info.size);

	get_mtd_partition(*output, mtd_info.size, device);
    /*
	get_mtd_partition(*output, 0x1ffff, device);
    */
	header = (struct image_header *)(*output);

	if (SBB_MAGIC == ntohl(header->ih_magic)) {
		sbb_image_fd = mkstemp(sbb_image_name);

		if (-1 == sbb_image_fd) {
			fprintf(stderr, "mkstemp() failed: %s\n",
				strerror(errno));
			free(*output);

			return -1;
		}

		sbb_image_file = fdopen(sbb_image_fd, "wb");

		if (NULL == sbb_image_file) {
			fprintf(stderr, "Error creating %s!\n");
			close(sbb_image_fd);
			free(*output);

			return -1;
		}

		if (mtd_info.size !=
		    fwrite(*output, 1, mtd_info.size, sbb_image_file)) {
			fprintf(stderr,
				"fwrite() failed: %s\n", strerror(errno));
			fclose(sbb_image_file);
			close(sbb_image_fd);
			free(*output);

			return -1;
		}

		fclose(sbb_image_file);
		close(sbb_image_fd);
		printf("Secure Image are Assumed to be Valid\n"
		       "Saved as %s, %lu bytes\n",
		       sbb_image_name, ntohl(*((uint32_t *)(*output + 4))));

		return 0;
	}

	if (IH_MAGIC != ntohl(header->ih_magic)) {
		fprintf(stderr, "Magic number is wrong: 0x%08x != 0x%08x\n",
			IH_MAGIC, ntohl(header->ih_magic));
		free(*output);

		return -1;
	}

	if (ntohl(header->ih_dcrc) !=
	    get_crc32((*output + sizeof(struct image_header)),
		      ntohl(header->ih_size))) {
		fprintf(stderr, "Bad Checksum: 0x%08x != 0x%08x\n",
			ntohl(header->ih_dcrc),
			get_crc32((*output + sizeof(struct image_header)),
				  (ntohl(header->ih_size))));

		return -1;
	}

	return 0;
}

/*
  ------------------------------------------------------------------------------
  uboot_select
*/

static int
uboot_select(int *selected, int verbose)
{
	void *image_a = NULL;
	void *image_b = NULL;
	struct image_header *header_a;
	struct image_header *header_b;
	int a_valid = 0;
	int a_secure = 0;
	int b_valid = 0;
	int b_secure = 0;
	int a_sequence;
	int b_sequence;
	char *temp;

	if (0 == uboot_read(&image_a, UBOOT_IMAGE_A)) {
		a_valid = 1;

		if (SBB_MAGIC == ntohl(*((uint32_t *)image_a))) 
			a_secure = 1;
		else
			header_a = (struct image_header *)image_a;
	} else {
		image_a = NULL;
		fprintf(stderr, "Error reading U-Boot Image A!\n");
	}

	temp = ubenv_get("uboot_a_sequence");

	if (NULL == temp) {
		fprintf(stderr,
			"** U-Boot Image A Sequence is Not Set, Assuming 0!\n");
		a_sequence = 0;
	} else {
		a_sequence = strtoul(temp, NULL, 0);
	}

	if (0 != verbose) {
		if (0 != a_valid) {
			if (0 == a_secure)
				printf("A: sequence: %d description: %.*s\n",
				       a_sequence, IH_NMLEN, header_a->ih_name);
			else
				printf("A: sequence: %d description: Encrypted\n",
				       a_sequence);
		} else {
			printf("A: Not Valid\n");
		}
	}

	if (0 == uboot_read(&image_b, UBOOT_IMAGE_B)) {
		b_valid = 1;

		if (SBB_MAGIC == ntohl(*((uint32_t *)image_b))) 
			b_secure = 1;
		else
			header_b = (struct image_header *)image_b;
	} else {
		image_b = NULL;
		fprintf(stderr, "Error reading U-Boot Image B!\n");
	}

	temp = ubenv_get("uboot_b_sequence");

	if (NULL == temp) {
		fprintf(stderr,
			"** U-Boot Image B Sequence is Not Set, Assuming 0!\n");
		b_sequence = 0;
	} else {
		b_sequence = strtoul(temp, NULL, 0);
	}

	if (0 != verbose) {
		if (0 != b_valid) {
			if (0 == b_secure)
				printf("B: sequence: %d description: %.*s\n",
				       b_sequence, IH_NMLEN, header_b->ih_name);
			else
				printf("B: sequence: %d description: Encrypted\n",
				       b_sequence);
		} else {
			printf("B: Not Valid\n");
		}
	}

	if (0 == a_valid && 0 != b_valid) {
		*selected = 1;
	} else if (0 != a_valid && 0 == b_valid) {
		*selected = 0;
	} else {
		if (0xffffffff == a_sequence && b_sequence == 0) {
			*selected = 1;
		} else if (b_sequence > a_sequence) {
			*selected = 1;
		} else {
			*selected = 0;
		}
	}

	if (0 != watchdog_timeout)
		*selected ^= 1;

	if (0 != verbose)
		printf("Selected Image %s\n",
		       (0 == *selected) ? "A" : "B");

	if (NULL != image_a)
		free(image_a);

	if (NULL != image_b)
		free(image_b);

	return 0;
}

/*
  ------------------------------------------------------------------------------
  uboot_write
*/

static int
uboot_write(const char *device, uint32_t sequence, const char *input)
{
	struct mtd_info_user mtd_info;
	struct stat input_stat;
	FILE *image_file = NULL;
	void *image = NULL;
	int mtd_fd;
	struct erase_info_user erase;
	int selected;
	char buffer[11];

	if (0 == strcmp(UBOOT_IMAGE_A, device))
		selected = 0;
	else
		selected = 1;

	if (0 != get_mtd_partition_info(device, &mtd_info)) {	
		fprintf(stderr, "Error Getting MTD Info!\n");
		goto cleanup;
	}

	if (0 != stat(input, &input_stat)) {
		fprintf(stderr, "Error reading %s: %s\n",
			input, strerror(errno));
		goto cleanup;
	}

	if (NULL == (image_file = fopen(input, "rb"))) {
		fprintf(stderr, "Error opening %s: %s\n",
			input, strerror(errno));
		goto cleanup;
	}

	image = malloc(input_stat.st_size);

	if (input_stat.st_size !=
	    fread(image, 1, input_stat.st_size, image_file)) {
		fprintf(stderr, "Error reading %s: %s\n",
			input, strerror(errno));
		goto cleanup;
	}

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

	if (input_stat.st_size != write(mtd_fd, image, input_stat.st_size)) {
		fprintf(stderr, "Error writing %s: %s\n",
			device, strerror(errno));
		goto cleanup;
	}

cleanup:

	if (NULL != image_file)
		fclose(image_file);
	
	if (NULL != image)
		free(image);

	if (0 != mtd_fd)
		close(mtd_fd);

	return 0;
}

/*
  ------------------------------------------------------------------------------
  uboot_update
*/

static int
uboot_update(const char *input)
{
	int selected;
	char *device;
	uint32_t sequence;
	char *temp;

	if (0 != uboot_select(&selected, 0)) {
		fprintf(stderr, "Can't determine selected image.\n");

		return -1;
	}

	temp = ubenv_get(sequence_env[selected]);

	if (NULL == temp) {
		sequence = 0;
		ubenv_add(sequence_env[selected], "0");
	} else {
		sequence = strtoul(temp, NULL, 0);
	}

	if (0xffffffff == sequence)
		sequence = 0;
	else
		++sequence;

	selected ^= 1;
	device = (0 == selected) ? UBOOT_IMAGE_A : UBOOT_IMAGE_B;

	if (0 != uboot_write(device ,sequence, input)) {
		fprintf(stderr, "Error Writing Image!\n");

		return -1;
	}
		

	return 0;
}

/*
  ------------------------------------------------------------------------------
  uboot_spl_write
*/

static int
uboot_spl_write(const char *device, const char *input)
{
	struct mtd_info_user mtd_info;
	struct stat input_stat;
	FILE *image_file = NULL;
	void *image = NULL;
	int mtd_fd;
	struct erase_info_user erase;
	char buffer[11];

	if (0 != get_mtd_partition_info(device, &mtd_info)) {	
		fprintf(stderr, "Error Getting MTD Info!\n");
		goto cleanup;
	}

	if (0 != stat(input, &input_stat)) {
		fprintf(stderr, "Error reading %s: %s\n",
			input, strerror(errno));
		goto cleanup;
	}

	if (NULL == (image_file = fopen(input, "rb"))) {
		fprintf(stderr, "Error opening %s: %s\n",
			input, strerror(errno));
		goto cleanup;
	}

	image = malloc(input_stat.st_size);

	if (input_stat.st_size !=
	    fread(image, 1, input_stat.st_size, image_file)) {
		fprintf(stderr, "Error reading %s: %s\n",
			input, strerror(errno));
		goto cleanup;
	}

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

	if (input_stat.st_size != write(mtd_fd, image, input_stat.st_size)) {
		fprintf(stderr, "Error writing %s: %s\n",
			device, strerror(errno));
		goto cleanup;
	}

cleanup:

	if (NULL != image_file)
		fclose(image_file);
	
	if (NULL != image)
		free(image);

	if (0 != mtd_fd)
		close(mtd_fd);

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
		"ubootimages [-d name] [-f file] [-g name] [-h] [-i] [-p]\n"
		"     [-s name=value] [-t] [-u file] [-w uboot|spl A|B file]\n"
		"\t-d : delete the given name from the environment\n"
		"\t-f : replace the environment with the contents of file\n"
		"\t-g : get the value of name from the environment\n"
		"\t-h : display this help message\n"
		"\t-i : display U-Boot image info\n"
		"\t-p : print the environment\n"
		"\t-s : set the given name=value in the environemnt\n"
		"\t-t : last reset was caused by a watchdog timeout\n"
		"\t-u : update the U-Boot image\n"
		"\t-w : write the U-Boot or U-Boot SPL image\n");
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
	char *name;
	char *value;
	char action;
	int selected;
	char *device;
	uint32_t sequence;

	struct option long_options[] = {
		{"delete", no_argument, &long_option, 'D'},
		{"file", no_argument, &long_option, 'F'},
		{"get", no_argument, &long_option, 'G'},
		{"help", no_argument, &long_option, 'H'},
		{"info", no_argument, &long_option, 'I'},
		{"print", no_argument, &long_option, 'P'},
		{"timeout", no_argument, &long_option, 'T'},
		{"set", no_argument, &long_option, 'S'},
		{"update", no_argument, &long_option, 'U'},
		{"write", no_argument, &long_option, 'W'},
		{0, 0, 0, 0}
	};

#if 0
	/*
	  Initialize the environment.
	*/
	if (0 != ubenv_initialize()) {
		fprintf(stderr, "Unable to Intialize the Environment\n");

		return EXIT_FAILURE;
	}
	watchdog_timeout = 0;
#endif


	/*
	  Handle Options
	*/

	while (-1 != (option =
		      getopt_long_only(argc, argv, "",
				       long_options, NULL))) {
		switch (option) {
		case 0:
			switch(long_option) {
			case 'H':
				usage(EXIT_SUCCESS);
				break;

			case 'T':
				watchdog_timeout = 1;
				break;

			case 'S':
			case 'D':
			case 'I':
			case 'F':
			case 'G':
			case 'P':
			case 'U':
			case 'W':
				action = long_option;
				break;

			default:
				usage(EXIT_FAILURE);
				break;

			}
		}
	}

	switch(action) {
	case 'D':
		/*
		  Delete the given name from the environment.
		*/
		if (0 != ubenv_remove(argv[2]))
			fprintf(stderr, "ubenv_remove( ) failed!\n");
		else
			save = 1;

	    ubenv_finalize(save);
		break;

	case 'F':
		/*
		  Replace the environment with the
		  contents of the given file.
		*/
	    if (0 != ubenv_initialize()) {
		    fprintf(stderr, "Unable to Intialize the Environment\n");
    		return EXIT_FAILURE;
	    }
		if (NULL == (file_name =
			     (char *)malloc((strlen(argv[2]) + 1)))) {
			fprintf(stderr, "Unable to allocate memory!\n");

			return EXIT_FAILURE;
		}

		strcpy( file_name, optarg );

		if (0 != ubenv_add_file(file_name))
			fprintf(stderr, "ubenv_add_file( ) failed!\n");
		else
			save = 1;

		free(file_name);
	    ubenv_finalize(save);
		break;

	case 'G':
	    if (0 != ubenv_initialize()) {
		    fprintf(stderr, "Unable to Intialize the Environment\n");
    		return EXIT_FAILURE;
	    }
		value = ubenv_get(argv[2]);

		if (NULL != value)
			printf("%s is %s\n", argv[2], value);
		else
			printf("%s is not set\n", optarg);

		free( value );
		break;

	case 'I':
		uboot_select(&selected, 1);
		break;

	case 'P':
	    if (0 != ubenv_initialize()) {
		    fprintf(stderr, "Unable to Intialize the Environment\n");
    		return EXIT_FAILURE;
	    }
		ubenv_print();
		break;

	case 'S':
		/*
		  Set the given name=value in the environment.
		*/
	    if (0 != ubenv_initialize()) {
		    fprintf(stderr, "Unable to Intialize the Environment\n");
    		return EXIT_FAILURE;
	    }
		name = argv[2];
		value = strchr(argv[2], '=');

		if (NULL != value) {
			*value = 0;
			++value;

			if ((0 != ubenv_remove(name)) ||
			    (0 != ubenv_add(name, value)))
				fprintf(stderr, "Set Failed!\n");
			else
				save = 1;
		} else {
			fprintf(stderr,
				"Please provide a string of the form name=value\n");
			usage(EXIT_FAILURE);
		}
	    ubenv_finalize(save);
		break;

	case 'W':


        if (0 == strcmp(argv[2], "uboot")) {

		    if (5 != argc) {
		    	fprintf(stderr,
		    		"write requires uboot|spl, A|B, and file!\n");
		    	usage(EXIT_FAILURE);
		    }

            if (1 != strlen(argv[3])) {
                fprintf(stderr, "Bank must be either A or B!\n");
                usage(EXIT_FAILURE);
            }

            if ('A' == toupper(*(argv[3]))) {
                device = UBOOT_IMAGE_A;
            } else if ('B' == toupper(*(argv[3]))) {
                device = UBOOT_IMAGE_B;
            } else {
                fprintf(stderr, "Bank must be either A or B!\n");
                usage(EXIT_FAILURE);
            }

            file_name = argv[4];

            if (0 != uboot_write(device, sequence, file_name))
                fprintf(stderr, "U-Boot Write Failed!\n");

        } else if (0 == strcmp(argv[2], "spl")) {

		    if (5 != argc) {
		    	fprintf(stderr,
		    		"write requires spl, a|b, and file!\n");
		    	usage(EXIT_FAILURE);
		    }
            if (1 != strlen(argv[3])) {
                fprintf(stderr, "Bank must be either A or B!\n");
                usage(EXIT_FAILURE);
            }

            if ('A' == toupper(*(argv[3]))) {
                device = SPL_IMAGE_A;
            } else if ('B' == toupper(*(argv[3]))) {
                device = SPL_IMAGE_B;
            } else {
                fprintf(stderr, "Bank must be either A or B!\n");
                usage(EXIT_FAILURE);
            }

            file_name = argv[4];

            if (0 != uboot_spl_write(device, file_name))
                fprintf(stderr, "SPL Write Failed!\n");
        }

		break;

	case 'U':
		if (0 != uboot_update(argv[2]))
			fprintf(stderr, "Update Failed!\n");
		else
			save = 1;

		break;

	default:
		usage(EXIT_FAILURE);
		break;
	}

	return EXIT_SUCCESS;
}
