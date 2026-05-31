/* CP Binary offsets */
#define CBL1_OFFSET			0x0	/*CP Bootloader 1 */
#define CIMG_OFFSET			0x5000	/*CP Main IMG */
#define CBL1_SIZE 			(CIMG_OFFSET - CBL1_OFFSET)
#define CIMG_SIZE			0x1F00000	/* 31 MB */
#define CNV_UNIT_SIZE			0x20000 /* 128KB */

#define NV_FILE_MODE			00700
#define MAX_NVDATA_SIZE			(2 * 1024 * 1024)

#define CMC22x_AP_BOOT_DOWN_DONE	0x54329876
#define CMC22x_CP_REQ_TOC		0xA3A3A3A3
#define CMC22x_CP_REQ_MAIN_BIN		0xA5A5A5A5
#define CMC22x_CP_REQ_NV_DATA		0x5A5A5A5A
#define CMC22x_CP_DUMP_MAGIC		0xDEADDEAD

#define CMC22x_HOST_DOWN_START		0x1234
#define CMC22x_HOST_DOWN_END		0x4321
#define CMC22x_REG_NV_DOWN_END		0xABCD
#define CMC22x_CAL_NV_DOWN_END		0xDCBA

#define CMC22x_1ST_BUFF_READY		0xAAAA
#define CMC22x_2ND_BUFF_READY		0xBBBB
#define CMC22x_1ST_BUFF_FULL		0x1111
#define CMC22x_2ND_BUFF_FULL		0x2222

#define CMC22x_CP_RECV_NV_END		0x8888
#define CMC22x_CP_CAL_OK		0x4F4B
#define CMC22x_CP_CAL_BAD		0x4552
#define CMC22x_CP_DUMP_END		0xFADE

#define CMC22x_DUMP_BUFF_SIZE		(8 * 1024)	/* 8 KB */
#define MAX_CPINFO_SIZE			512

enum dpram_img_type {
	IMG_BOOT,	/*primary signed image*/
	IMG_MAIN,
	IMG_MAX_IDX,
};

struct cp_imgmap {
	char name[12];		/* Binary name                */
	unsigned bin_offset;	/* Binary offset in the file  */
	unsigned mem_offset;	/* Memory Offset to be loaded */
	unsigned size;		/* Binary size                */
	unsigned crc;		/* CRC value                  */
	unsigned reserved;	/* Reserved                   */
};

struct cmc_args {
	struct boot_args *cbd_args;
	struct cp_imgmap *img_tab;
	int toc_valid;
	int crc_check;
	int bin_fd;
	int boot_fd;
	int link_fd;
};

struct image_buf {
	unsigned int length;
	unsigned char *buf;
};

struct dpram_boot_img {
	unsigned char *addr;
	unsigned long size;
	enum cp_boot_mode mode;
	unsigned req;
	unsigned resp;
};

#define MAX_PAYLOAD_SIZE 0x2000
struct dpram_boot_frame {
	unsigned req; 	/* AP to CP Message */
	unsigned res; 	/* CP to AP Response */
	ssize_t len; 		/* request size*/
	unsigned offset; 	/* offset to write */
	char data[MAX_PAYLOAD_SIZE];
};

struct dpram_dump_arg {
	char *buff;
	int buff_size;	/* AP->CP: Buffer size */
};

