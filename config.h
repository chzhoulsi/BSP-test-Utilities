#define SPL_A_56XX        ("/dev/mtd0")
#define SPL_B_56XX        ("/dev/mtd1")
#define PARAM_A_56XX      ("/dev/mtd2")
#define PARAM_B_56XX      ("/dev/mtd3")
#define ENV_A_56XX        ("/dev/mtd4")     /* u-boot env */
#define ENV_B_56XX        ("/dev/mtd5") 
#define UBOOT_A_56XX      ("/dev/mtd6")
#define UBOOT_B_56XX      ("/dev/mtd7")
                                          
#define SPL_A_XLF         ("/dev/mtd0")
#define SPL_B_XLF         ("/dev/mtd1")
#define PARAM_A_XLF       ("/dev/mtd2")
#define PARAM_B_XLF       ("/dev/mtd3")
#define ENV_A_XLF         ("/dev/mtd4")     /* u-boot env */
#define ENV_B_XLF         ("/dev/mtd5") 
#define UBOOT_A_XLF       ("/dev/mtd6")
#define UBOOT_B_XLF       ("/dev/mtd7")
                                         
#define SPL_A_55XX        ("/dev/mtd0")
#define PARAM_A_55XX      ("/dev/mtd1")
#define PARAM_B_55XX      ("/dev/mtd2")
#define ENV_A_55XX        ("/dev/mtd3")      /* u-boot env */
#define ENV_B_55XX        ("/dev/mtd4")     
#define UBOOT_A_55XX      ("/dev/mtd5") 
#define UBOOT_B_55XX      ("/dev/mtd6")

#define PARAMETERS_MAGIC            0x12af34ec
#define IH_MAGIC	                0x27051956	    /* Image Magic Number		*/
#define SBB_MAGIC                   0x53424211      /* SBB Magic Number */
#define IH_NMLEN		            32	            /* Image Name Length		*/
#define DEFAULT_ENVIRONMENT_SIZE    (256 * 1024)
#define SMALL_ENVIRONMENT_SIZE      (128 * 1024)
#define ENVIRONMENT_SIZE_1_2_X      DEFAULT_ENVIRONMENT_SIZE

#define ENVIRONMENT_DATA_SIZE(size) (size - (2 * sizeof(uint32_t)))
#define INPUT_BUFFER_SIZE           256

#define UBOOT_KEY      "u-boot_"
#define UBOOT_KEY_55XX "lsi_axxia_u-boot_"
#define SPL_KEY_55XX   "lsi_"
#define SPL_KEY        "u-boot_"
#define ATF_KEY        "atf_"

#define HOSTNAME_55XX  "axx-a"
#define HOSTNAME_56XX  "axx-v"
#define HOSTNAME_XLF   "axx-w"
