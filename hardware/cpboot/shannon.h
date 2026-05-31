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
	BOOT_STAGE = 0,
	TOC_STAGE,
	MAIN_STAGE,
	VSS_STAGE,
	NV_STAGE,
	FIN_STAGE,
	SHANNON_MAX_DL_STAGE
};

struct shannon_boot_args {
	struct boot_args *cbd_args;
	struct std_boot_args *std_args;
	int load_fd;
	int bin_fd;
	int nv_fd;
} __packed;

#endif

