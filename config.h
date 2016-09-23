#define SPL_IMAGE_A                 "/dev/mtd0"
#define SPL_IMAGE_B                 "/dev/mtd1"

#define SPLPARMS_DEVICE_A           "/dev/mtd2"
#define SPLPARMS_DEVICE_B           "/dev/mtd3"

#define FLASH_DEVICE_A              "/dev/mtd4"     /* u-boot env */
#define FLASH_DEVICE_B              "/dev/mtd5"

#define UBOOT_IMAGE_A               "/dev/mtd6"
#define UBOOT_IMAGE_B               "/dev/mtd7"

#define PARAMETERS_MAGIC            0x12af34ec
#define IH_MAGIC	                0x27051956	    /* Image Magic Number		*/
#define SBB_MAGIC                   0x53424211      /* SBB Magic Number */
#define IH_NMLEN		            32	            /* Image Name Length		*/
#define DEFAULT_ENVIRONMENT_SIZE    (256 * 1024)
#define SMALL_ENVIRONMENT_SIZE      (128 * 1024)
#define ENVIRONMENT_SIZE_1_2_X      DEFAULT_ENVIRONMENT_SIZE

#define ENVIRONMENT_DATA_SIZE(size) (size - (2 * sizeof(uint32_t)))
#define INPUT_BUFFER_SIZE           256
