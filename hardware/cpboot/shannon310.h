#ifndef __CBD_SHANNON_H__
#define __CBD_SHANNON_H__

enum shannon_image_type {
	IMG_TOC = 0,
	IMG_BOOT,
	IMG_MAIN,
	IMG_VSS,
	IMG_NV,
	MAX_IMAGE_TYPE
};

enum shannon_dl_stage {
	TOC_STAGE = 0,
	BOOT_STAGE,
	MAIN_STAGE,
	VSS_STAGE,
	NV_STAGE,
	SHANNON_MAX_DL_STAGE
};

#define SHANNON_LEGACY_MAX_DL_STAGE 3 /* BOOT, MAIN, NV */

struct shannon_boot_args {
	struct boot_args *cbd_args;
	struct std_boot_args *std_args;
	int bin_fd;
	int nv_fd;
} __packed;

struct modem_img {
	unsigned long long binary; /* Pointer to binary buffer */
	u32 size;		   /* Binary size */
	u32 m_offset;
	u32 b_offset;
	u32 mode;
	u32 len;
} __packed;

#endif

