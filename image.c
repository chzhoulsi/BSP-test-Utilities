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
#define _GNU_SOURCE
#define __USE_GNU
#include <string.h>
#include <getopt.h>
#include <linux/limits.h>
#include <unistd.h>
/*
#include <stropts.h>
*/
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

/* u-boot environment data structure */
typedef struct environment {
	uint32_t crc32;
	uint32_t flags;
	uint32_t size;
	void *data;
} __attribute__ ((packed))environment_t;


/* u-boot image data structure */
typedef struct image_header {
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
} __attribute__((packed))uboot_header_t;

/* parameter image data structure */

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
} __attribute__((packed)) parameter_header_t;

typedef struct {
	unsigned version;
	unsigned flags;
	unsigned baud_rate;
	unsigned memory_ranges[16];
	unsigned long sequence;
	char description[128];
} __attribute__((packed)) parameters_global_t;


typedef struct image_dev
{
    const char *location; 
    const char *type; 
    const char *asic; 
    char  select;
    const char *input;
    int (*write)(struct image_dev *); 
    int (*delete)(struct image_dev *); 
    int (*print_mtd_image)(struct image_dev *); 
    int (*check_file_image)(struct image_dev *); 
} image_t;


static int 
print_uboot_info(void * data, uint32_t size, const char *asic)
{
    char *needle = UBOOT_KEY; 
    void *match = NULL;
    uboot_header_t *header = (uboot_header_t *)data;

    if ((0 == strcmp(asic, "56xx")) || (0 == strcmp(asic, "xlf"))) {
        if (IH_MAGIC != ntohl(header->ih_magic)){
            fprintf(stderr, "uboot magic number doesn't match\n");
            fprintf(stderr, "no a valid uboot\n");
            return -1;
        }
        else {
            printf("\tmagic number = 0x%x\n", ntohl(header->ih_magic));
            printf("\tcrc = 0x%x\n", ntohl(header->ih_hcrc));
            printf("\ttime = 0x%x\n", ntohl(header->ih_time));

            match = memmem((const void *)header, size, 
                    (const void *)needle, strlen(needle));
            if (match) 
                printf("\tuboot version=%s\n", match);
        }
    } else if (0 == strcmp(asic, "55xx")) {
        if (IH_MAGIC != ntohl(header->ih_magic)){
            fprintf(stderr, "uboot magic number doesn't match\n");
            fprintf(stderr, "no a valid uboot\n");
            return -1;
        }
        else {
            printf("\tmagic number = 0x%x\n", ntohl(header->ih_magic));
            printf("\tcrc = 0x%x\n", ntohl(header->ih_hcrc));
            printf("\ttime = 0x%x\n", ntohl(header->ih_time));
        }
        needle = UBOOT_KEY_55XX;
        match = memmem((const void *)header, size, 
                (const void *)needle, strlen(needle));
        if (match) 
            printf("\tuboot version=%s\n", match);
    }
    return 0;
}


static int 
check_uboot_img(const char * input)
{

	struct stat input_stat;
	unsigned long input_size;
	FILE *file = NULL;
	void *file_data = NULL;
	uboot_header_t *header;
    uint32_t return_value = 0;

	if (0 != stat(input, &input_stat)) {
		fprintf(stderr, "Error reading %s: %s\n",
			input, strerror(errno));
        return_value = -1;
		goto cleanup;
	}

	input_size = input_stat.st_size;

	if (NULL == (file = fopen(input, "rb"))) {
		fprintf(stderr, "Error opening %s: %s\n",
			input, strerror(errno));
        return_value = -1;
		goto cleanup;
	}

	file_data = malloc(input_size);

	if (input_size != fread(file_data, 1, input_size, file)) {
		fprintf(stderr, "Error reading %s: %s\n",
			input, strerror(errno));
        return_value = -1;
		goto cleanup;
	}

	header = (uboot_header_t *)file_data;

	if (IH_MAGIC != ntohl(header->ih_magic)) {
		fprintf(stderr, "Bad Input Magic!\n");
        return_value = -1;
		goto cleanup;
	}
	
cleanup:

	if (NULL != file_data)
		free(file_data);
	
	return return_value;
}

static int
check_uboot_bin(const char * input)
{
    const char *dot = strrchr(input, '.');
    if ((!dot) || (0 == strcmp(dot, ".bin")))
        return 0;
    else    
        return -1;
}

static int 
print_spl_info(void * data, uint32_t size, const char *asic)
{
    char *needle = SPL_KEY; 
    void *match = NULL;
    uboot_header_t *header = (uboot_header_t *)data;

    if (0 == strcmp(asic, "56xx") || 0 == strcmp(asic, "xlf"))
    {
        if (IH_MAGIC != ntohl(header->ih_magic)){
            fprintf(stderr, "spl magic number doesn't match\n");
            fprintf(stderr, "no a valid spl\n");
            return -1;
        }
        else {
            printf("\tmagic number = 0x%x\n", ntohl(header->ih_magic));
            printf("\tcrc = 0x%x\n", ntohl(header->ih_hcrc));
            printf("\ttime = 0x%x\n", ntohl(header->ih_time));

            match = memmem((const void *)header, size, 
                    (const void *)needle, strlen(needle));
            if (match) 
                printf("\tspl version=%s\n", match);
            needle = ATF_KEY;
            match = memmem((const void *)header, size, 
                    (const void *)needle, strlen(needle));
            if (match) 
                printf("\tatf version=%s\n", match);

        }
    } else if (0 == strcmp(asic, "55xx")) {
        needle = SPL_KEY_55XX;
        match = memmem((const void *)header, size, 
                (const void *)needle, strlen(needle));
        if (match) 
            printf("\tSPL version=%s\n", match);
    }
    return 0;
}

static int 
print_param_info(void * data, uint32_t size)
{
    uint32_t i;
    uint32_t *ptr = NULL;
	parameter_header_t *header = (parameter_header_t *)data;
    if (PARAMETERS_MAGIC != ntohl(header->magic)){
	    fprintf(stderr, "parameter magic number doesn't match\n");
        fprintf(stderr, "no a valid parameter file\n");
        return -1;
    } else {
        printf( "\tversion = 0x%x\n", ntohl(header->version));
        printf( "\tchipType = 0x%x\n", ntohl(header->chipType));

        printf("\tglobal setting, version %d\n",
                ntohl(*((uint32_t *)data + ntohl(header->globalOffset)/4)));
        ptr = (uint32_t *)data + (ntohl(header->globalOffset)/4 + 1);
        for (i=0; i<(ntohl(header->globalSize)-1); i++)
        {
            if (0 ==(i%4))
                printf("\t\t");
            printf("0x%08x    ",ntohl(*ptr));
            if (0 ==((i+1)%4))
                printf("\n");
            ptr++;
        }

        printf("\n\n\tpciesrio setting, version %d\n",
                ntohl(*((uint32_t *)data + ntohl(header->pciesrioOffset)/4)));
        ptr = (uint32_t *)data + (ntohl(header->pciesrioOffset)/4 + 1);
        for (i=0; i<(ntohl(header->pciesrioSize)-1); i++)
        {
            if (0 ==(i%4))
                printf("\t\t");
            printf("0x%08x    ",ntohl(*ptr));
            if (0 ==((i+1)%4))
                printf("\n");
            ptr++;
        }

        printf("\n\n\tvoltage setting, version %d\n",
                ntohl(*((uint32_t *)data + ntohl(header->voltageOffset)/4)));
        ptr = (uint32_t *)data + (ntohl(header->voltageOffset)/4 + 1);
        for (i=0; i<(ntohl(header->voltageSize)-1); i++)
        {
            if (0 ==(i%4))
                printf("\t\t");
            printf("0x%08x    ",ntohl(*ptr));
            if (0 ==((i+1)%4))
                printf("\n");
            ptr++;
        }

        printf("\n\n\tclock setting, version %d\n",
                ntohl(*((uint32_t *)data + ntohl(header->clocksOffset)/4)));
        ptr = (uint32_t *)data + (ntohl(header->clocksOffset)/4 + 1);
        for (i=0; i<(ntohl(header->clocksSize)-1); i++)
        {
            if (0 ==(i%4))
                printf("\t\t");
            printf("0x%08x    ",ntohl(*ptr));
            if (0 ==((i+1)%4))
                printf("\n");
            ptr++;
        }


        printf("\n\n\tsystemMemory setting, version %d\n",
                ntohl(*((uint32_t *)data + ntohl(header->systemMemoryOffset)/4)));
        ptr = (uint32_t *)data + (ntohl(header->systemMemoryOffset)/4 + 1);
        for (i=0; i<(ntohl(header->systemMemorySize)-1); i++)
        {
            if (0 ==(i%4))
                printf("\t\t");
            printf("0x%08x    ",ntohl(*ptr));
            if (0 ==((i+1)%4))
                printf("\n");
            ptr++;
        }
#if 0
        printf("\tclassifier Memory setting, version %d\n",
                ntohl(*((uint32_t *)data + ntohl(header->classifierMemoryOffset)/4)));
        ptr = (uint32_t *)data + (ntohl(header->classifierMemoryOffset)/4 + 1);
        for (i=0; i<(ntohl(header->classifierMemorySize)-1); i++)
        {
            if (0 ==(i%4))
                printf("\t\t");
            printf("0x%08x    ",ntohl(*ptr));
            if (0 ==((i+1)%4))
                printf("\n");
            ptr++;
        }


        printf("\tsystem Memory Retention setting, version %d\n",
                ntohl(*((uint32_t *)data + ntohl(header->systemMemoryRetentionOffset)/4)));
        ptr = (uint32_t *)data + (ntohl(header->systemMemoryRetentionOffset)/4 + 1);
        for (i=0; i<(ntohl(header->systemMemoryRetentionSize)-1); i++)
        {
            if (0 ==(i%4))
                printf("\t\t");
            printf("0x%08x    ",ntohl(*ptr));
            if (0 ==((i+1)%4))
                printf("\n");
            ptr++;
        }
#endif 
    }
    printf("\n");
    return 0;
}


static int 
print_env_info(void * data, uint32_t size)
{
    char *string;
    environment_t header;
    header.size = size;
    header.crc32 = *((uint32_t *)data);
    header.flags = *((uint32_t *)(data + 4));
    header.data = (uint32_t *)(data + 8);
    uint32_t crc32 = get_crc32(header.data,
            ENVIRONMENT_DATA_SIZE(header.size));
    if (crc32 != header.crc32){
        fprintf(stderr, "%s crc32 doesn't match\n");
        fprintf(stderr, "no a valid environment file\n");
        return -1;
    } else {
        /* print env */
        string = header.data;
        while (0x00 != string[0]) {
            printf("%s\n", string);
            string += (strlen(string) + 1);
        }
    }
    return 0;
}



int
print_mtd_image(image_t *image)
{
	struct mtd_info_user mtd_info;
    void *output = NULL;

	get_mtd_partition_info(image->location, &mtd_info);
	output = malloc(mtd_info.size);

    /* TODO Temporary solution to Fix a SSP timeout issue */
#if 1
    uint32_t reduced_size; 
    if ((0 == strcmp(image->asic, "56xx")) || (0 == strcmp(image->asic, "xlf")))
    {
        reduced_size = (mtd_info.size > 0x60000) ? (reduced_size = 0x60000) :
                        (reduced_size = mtd_info.size);
    }else
        reduced_size = mtd_info.size;
                                               
	get_mtd_partition(output, reduced_size, image->location);
#endif 

    printf("%s info on bank %c on %s:\n", image->type, image->select, image->asic);

    if ((0 == strcmp("spl" ,image->type)) && 
        (0 == strcmp("55xx" ,image->asic)) &&
        ('B' == image->select))
        return 0; 
    if (0 == strcmp("uboot", image->type)) {

        if(0 != print_uboot_info(output, reduced_size, image->asic))
            goto cleanup;
    }
    else if (0 == strcmp("spl", image->type)) {
        if(0 != print_spl_info(output, reduced_size, image->asic))
            goto cleanup;
    }
    else if (0 == strcmp("param", image->type)) {
        if(0 != print_param_info(output, reduced_size))
            goto cleanup;
    }
    else if (0 == strcmp("env", image->type)) {
        if(0 != print_env_info(output, mtd_info.size))
            goto cleanup;
    }
    else {
	    fprintf(stderr, "no header found!\n");
        goto cleanup;
    }

cleanup:
    if (NULL != output)
        free(output);
    
	return 0;
}


int 
image_write(image_t *image) 
{
    const char *location = image->location; 
    const char *input = image->input;
    if( 0 == mtd_write(location, input))
        return 0;
    else
		return EXIT_FAILURE;
}


int 
image_check(image_t *image) 
{
    if((0 == strcmp(image->asic,"55xx")) && (0 == strcmp(image->type,"spl")))
        return check_uboot_bin(image->input);
    else if ((0 == strcmp(image->type,"uboot")) || (0 == strcmp(image->type,"spl")))
		return check_uboot_img(image->input);
    else
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
        "\timage ACTION IMAGE_TYPE [BANK_LOCATION] [FILE]\n"
		"\t-h : display this help message\n"
		"\t-i uboot|spl|param|env A|B : display image info\n"
		"\t-w uboot|spl|param|env A|B file: write the image\n");
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
    char mtd_loc[20];
	char *value;
    char name[20];
	char action;
	int selected;
	char *device;
	uint32_t sequence;
    image_t image; 

	struct option long_options[] = {
		{"help", no_argument, &long_option, 'H'},
		{"delete", no_argument, &long_option, 'D'},
		{"info", no_argument, &long_option, 'I'},
		{"write", no_argument, &long_option, 'W'},
		{"file", required_argument, &long_option, 'F'},
		{0, 0, 0, 0}
	};



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
			case 'D':
			case 'I':
			case 'W':
				action = long_option;
				break;

			default:
				usage(EXIT_FAILURE);
				break;

			}
		}
	}

	/*
	  Initialize 
	*/
    if ( 0 != gethostname(&name[0], sizeof(name)))
        printf("host name = %s",(char *)&name[0]);
    if ( 0 == strncmp(HOSTNAME_55XX, &name[0], strlen(HOSTNAME_55XX)))
        image.asic = "55xx";
    else if ( 0 == strncmp(HOSTNAME_56XX, &name[0], strlen(HOSTNAME_55XX)))
        image.asic = "56xx";
    else if ( 0 == strncmp(HOSTNAME_XLF, &name[0], strlen(HOSTNAME_55XX)))
        image.asic= "xlf";
    else {
        fprintf(stderr, "Can only be used on 55xx, 56xx and XLF");
        usage(EXIT_FAILURE);
    }

    if ((0 == strcmp(argv[2], "uboot")) || (0 == strcmp(argv[2], "spl")) ||
            (0 == strcmp(argv[2], "param")) || (0 == strcmp(argv[2], "env"))) {
        image.type = argv[2];
    } else {
	    fprintf(stderr,
	    	"image type should be uboot, spl, param or env!\n");
	    usage(EXIT_FAILURE);
    }

    if (!argv[3])
        image.select = 'A';
    else {
        if (1 != strlen(argv[3])) {
            fprintf(stderr, "Bank must be either A or B!\n");
            usage(EXIT_FAILURE);
        } else {
            if ('A' == toupper(*(argv[3])) || 'a' == *(argv[3]) ) {
                image.select = 'A';

            } else if ('B' == toupper(*(argv[3])) || 'b' == *(argv[3])) {
                image.select = 'B';
            } else {
                fprintf(stderr, "Bank must be either A or B!\n");
                usage(EXIT_FAILURE);
            }
        }
    }
    /* u-boot */
    if ((0 == strcmp(image.type, "uboot")) &&  (0 == strcmp(image.asic, "55xx"))) 
        ('A' == image.select) ? (image.location = UBOOT_A_55XX) : (image.location = UBOOT_B_55XX);
    else if ((0 == strcmp(image.type, "uboot")) &&  (0 == strcmp(image.asic, "56xx"))) 
        ('A' == image.select) ? (image.location = UBOOT_A_56XX) : (image.location = UBOOT_B_56XX);
    else if ((0 == strcmp(image.type, "uboot")) &&  (0 == strcmp(image.asic, "xlf"))) 
        ('A' == image.select) ? (image.location = UBOOT_A_XLF) : (image.location = UBOOT_B_XLF);

    /* spl */
    else if ((0 == strcmp(image.type, "spl")) &&  (0 == strcmp(image.asic, "55xx"))) { 
         if ('B' == image.select) {
            fprintf(stderr, "No bank B exists for SPL image on 55xx hardware\n");
	        usage(EXIT_FAILURE);
         } else    
            image.location = SPL_A_55XX;
    }
    else if ((0 == strcmp(image.type, "spl")) &&  (0 == strcmp(image.asic, "56xx"))) 
        ('A' == image.select) ? (image.location = SPL_A_56XX) : (image.location = SPL_B_56XX);
    else if ((0 == strcmp(image.type, "spl")) &&  (0 == strcmp(image.asic, "xlf"))) 
        ('A' == image.select) ? (image.location = SPL_A_XLF) : (image.location = SPL_B_XLF);

    /* param */
    else if ((0 == strcmp(image.type, "param")) &&  (0 == strcmp(image.asic, "55xx"))) 
        ('A' == image.select) ? (image.location = PARAM_A_55XX) : (image.location = PARAM_B_55XX);
    else if ((0 == strcmp(image.type, "param")) &&  (0 == strcmp(image.asic, "56xx"))) 
        ('A' == image.select) ? (image.location = PARAM_A_56XX) : (image.location = PARAM_B_56XX);
    else if ((0 == strcmp(image.type, "param")) &&  (0 == strcmp(image.asic, "xlf"))) 
        ('A' == image.select) ? (image.location = PARAM_A_XLF) : (image.location = PARAM_B_XLF);

    /* env */
    else if ((0 == strcmp(image.type, "env")) &&  (0 == strcmp(image.asic, "55xx"))) 
        ('A' == image.select) ? (image.location = ENV_A_55XX) : (image.location = ENV_B_55XX);
    else if ((0 == strcmp(image.type, "env")) &&  (0 == strcmp(image.asic, "56xx"))) 
        ('A' == image.select) ? (image.location = ENV_A_56XX) : (image.location = ENV_B_56XX);
    else if ((0 == strcmp(image.type, "env")) &&  (0 == strcmp(image.asic, "xlf"))) 
        ('A' == image.select) ? (image.location = ENV_A_XLF) : (image.location = ENV_B_XLF);



	switch(action) {
	case 'D':
		/* TODO
		  Delete the given name from the environment.

		if (0 != ubenv_remove(argv[2]))
			fprintf(stderr, "ubenv_remove( ) failed!\n");
		else
			save = 1;

	    ubenv_finalize(save);
		*/
		break;

    case 'I':
            image.print_mtd_image = print_mtd_image; 
            image.input = NULL;
            if (0 != image.print_mtd_image(&image))
                fprintf(stderr, "Write Failed!\n");
		break;

	case 'W':
            if (5 != argc) {
                fprintf(stderr, "Must have 4 arguments for write\n");
                usage(EXIT_FAILURE);
            }
            image.input = argv[4];
            image.write = image_write;
            image.check_file_image = image_check;
            if (0 != image.check_file_image(&image)) {
                fprintf(stderr, "Image Check Failed!\n");
                usage(EXIT_FAILURE);
            }
            if (0 != image.write(&image))
                fprintf(stderr, "Write Failed!\n");
		break;

	case 'V':
            image.write = image_write;
            if (0 != image.write(&image))
            fprintf(stderr, "Write Failed!\n");
		break;

	default:
		usage(EXIT_FAILURE);
		break;

	return EXIT_SUCCESS;
    }
}
