#ifndef SHANNON_H
#define SHANNON_H

#include "boot.h"

#define CMD_DIR_AP2CP		0x9000
#define CMD_DIR_CP2AP		0xA000

#define CMD_STEP_START		0x0
#define CMD_SEND_DATA		0x1
#define CMD_SEND_CRC		0xC
#define CMD_STEP_COMPLETE	0xD
#define CMD_STEP_FAIL		0xF
#define CMD_BOOT_COMPLETE	0x0

#define DATA_PAYLOAD_SIZE	2048
#define DUMP_CAUSE_MAX_LEN	512

#define GPIO_CP2AP_STATUS	"/sys/class/sec/linkpm/cp2ap_status/value"
#define GPIO_AP2CP_STATUS	"/sys/class/sec/linkpm/ap2cp_status/value"
#define GPIO_HOST_WAKEUP	"/sys/class/sec/linkpm/host_wakeup/value"

/* EHCI node */
#define EXYNOS_EHCI		"/dev/ehci_power"
#define EXYNOS_PORT_POWER	"/sys/devices/platform/s5p-ehci/port_power"
#define EXYNOS_OHCI		"/sys/devices/platform/s5p-ohci/ohci_power"
#define TEGRA_EHCI		"/sys/devices/platform/tegra-ehci.1/ehci_power"
#define USB_REMOVE		"/sys/bus/usb/devices/1-2/remove"

const char *bin_name[] = {
	"TOC",
	"BOOT",
	"LOADER",
	"MAIN",
	"NV",
};

enum image_type {
	IMAGE_TYPE_TOC,
	IMAGE_TYPE_BOOT,
	IMAGE_TYPE_LOADER,
	IMAGE_TYPE_MAIN,
	IMAGE_TYPE_NV,
	IMAGE_TYPE_MAX_IDX,
};

enum bootstrap_step {
	STEP_BOOT,
	STEP_LOADER,
	STEP_TOC,
	STEP_MAIN,
	STEP_NV,
	STEP_FIN,
	STEP_MAX_IDX,
};

enum dumpstrap_step {
	STEP_DUMP = 0x0D,
};

struct data_frame {
	u32 cmd;
	u32 num_frame;
	u32 curr_frame;
	u32 len;
	u8 data[DATA_PAYLOAD_SIZE];
};

struct crc_frame {
	u32 cmd;
	u32 crc;
};

struct dump_info {
	u32 size;
	u32 step_num;
	u32 cause_len;
};

struct image_map {
	char name[MAX_IMG_NAME_LEN];	/* bin name */
	u32 bin_offset;			/* bin offset in the file */
	u32 mem_offset;			/* memory offset to be loaded*/
	u32 size;			/* bin size */
	u32 crc;			/* CRC */
	u8 reserved[4];			/* reserved */
};

struct shannon_args {
	struct boot_args *cbd_args;
	struct image_map toc[IMAGE_TYPE_MAX_IDX];
	int boot_fd;
	int bin_fd;
	int nv_fd;

	int info_fd;
	int dump_fd;

	enum modem_state state;
};
#endif
