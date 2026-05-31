#ifndef __CBD_BOOT_H__
#define __CBD_BOOT_H__

#define DEBUG_RADIO_MSG
#define DEBUG_KERNEL_MSG

#ifndef s8
typedef char		s8;
#endif

#ifndef s16
typedef short		s16;
#endif

#ifndef s32
typedef int		s32;
#endif

#ifndef u8
typedef unsigned char	u8;
#endif

#ifndef u16
typedef unsigned short	u16;
#endif

#ifndef u32
typedef unsigned int	u32;
#endif

/*============================================================================*\
	Definitions for printing CP BOOT/DUMP log messages
\*============================================================================*/
#define ERR2STR		strerror(errno)

#define CBD_ID		LOG_ID_RADIO
#define CBD_TAG		"boot"

#define PRI_ERR		ANDROID_LOG_ERROR
#define PRI_DBG		ANDROID_LOG_DEBUG

#define FMT_ERR		"cbd: (ERR! %s) %s: "
#define FMT_INFO	"cbd: %s: "
#define FMT_KERN	"<3>mif: cbd: %s: "

void print_data(char *data, int len);
int get_kmsg_fd(void);

#ifdef DEBUG_RADIO_MSG
#include <log/log.h>
#define cbd_err(s, args...) \
	__android_log_buf_print(CBD_ID, PRI_ERR, CBD_TAG, FMT_ERR s, ERR2STR, __func__, ##args)
#define cbd_info(s, args...) \
	__android_log_buf_print(CBD_ID, PRI_DBG, CBD_TAG, FMT_INFO s, __func__, ##args)
#define cbd_kernel(s, args...)	dprintf(get_kmsg_fd(), FMT_KERN s, __func__, ##args)
#else
#define cbd_err(s, args...)	printf(FMT_ERR s, ERR2STR, __func__, ##args)
#define cbd_info(s, args...)	printf(FMT_INFO s, __func__, ##args)
#define cbd_kernel(s, args...)	printf(FMT_KERN s, __func__, ##args)
#endif

#define cbd_log(s, args...) \
	do { \
		if (errno == 0) { \
			cbd_info(s, ##args); \
		} else { \
			cbd_err(s, ##args); \
			errno = 0; \
		} \
		cbd_kernel(s, ##args); \
	} while (0)

#define IOCTL_MODEM_ON			_IO('o', 0x19)
#define IOCTL_MODEM_OFF			_IO('o', 0x20)
#define IOCTL_MODEM_RESET		_IO('o', 0x21)
#define IOCTL_MODEM_BOOT_ON		_IO('o', 0x22)
#define IOCTL_MODEM_BOOT_OFF		_IO('o', 0x23)
#define IOCTL_MODEM_BOOT_DONE		_IO('o', 0x24)

#define IOCTL_MODEM_STATUS		_IO('o', 0x27)

#define IOCTL_MODEM_DL_START		_IO('o', 0x28)
#define IOCTL_MODEM_FW_UPDATE		_IO('o', 0x29)

#define IOCTL_MODEM_DUMP_START		_IO('o', 0x32)
#define IOCTL_MODEM_DUMP_UPDATE		_IO('o', 0x33)
#define IOCTL_MODEM_FORCE_CRASH_EXIT	_IO('o', 0x34)
#define IOCTL_MODEM_CP_UPLOAD		_IO('o', 0x35)
#define IOCTL_MODEM_DUMP_RESET		_IO('o', 0x36)

#define IOCTL_MODEM_SWITCH_MODEM	_IO('o', 0x37)

#define IOCTL_MODEM_SET_AP_STATE	_IO('o', 0x38)
#define IOCTL_MODEM_CLEAR_AP_STATE	_IO('o', 0x39)
#define IOCTL_MODEM_GET_CP_STATE	_IO('o', 0x3A)

#define IOCTL_LINK_CONNECTED		_IO('o', 0x33)
#define IOCTL_LINK_PORT_ON		_IO('o', 0x35)
#define IOCTL_LINK_PORT_OFF		_IO('o', 0x36)

#define IOCTL_MODEM_SET_TX_LINK		_IO('o', 0x37)
#define IOCTL_MODEM_WATCHDOG_CRASH	_IO('o', 0x38)

#define IOCTL_MODEM_XMIT_BOOT		_IO('o', 0x40)
#ifdef CONFIG_SEC_CP_SECURE_BOOT
#define IOCTL_MODEM_GET_SHMEM_INFO	_IO('o', 0x41)
#endif

#define IOCTL_DPRAM_INIT_STATUS		_IO('o', 0x43)

#define IOCTL_LINK_DEVICE_RESET		_IO('o', 0x44)

#define IOCTL_MODEM_GET_SHMEM_SRINFO	_IO('o', 0x45)
#define IOCTL_MODEM_SET_SHMEM_SRINFO	_IO('o', 0x46)

#define IOCTL_MODEM_GET_CP_BOOTLOG	_IO('o', 0x47)
#define IOCTL_MODEM_CLR_CP_BOOTLOG	_IO('o', 0x48)

#define IOCTL_MIF_LOG_DUMP		_IO('o', 0x51)
#define IOCTL_MIF_DPRAM_DUMP		_IO('o', 0x52)

#define IOCTL_SECURITY_REQ		_IO('o', 0x53)	/* Change secure mode */
#define IOCTL_SHMEM_FULL_DUMP		_IO('o', 0x54)	/* for shared memory full dump */
#define IOCTL_MODEM_CRASH_REASON    	_IO('o', 0x55)  /* Get Crash Reason */

/* 0x56 reserved for Airplane mode */
#define IOCTL_VSS_FULL_DUMP		_IO('o', 0x57)	/* for shared memory vss dump */
#define IOCTL_ACPM_FULL_DUMP		_IO('o', 0x58)	/* for acpm memory dump */
#define IOCTL_CPLOG_FULL_DUMP		_IO('o', 0x59)	/* for cplog memory dump */
#define IOCTL_DATABUF_FULL_DUMP		_IO('o', 0x5A)	/* for databuf memory dump */

#define IOCTL_REGISTER_PCIE		_IO('o', 0x65)

#define CPDUMP_ROOT		"/data"
#define CPDUMP_PATH		"/sdcard/log"
#define FACTORY_CPDUMP_PATH	"/data/vendor/log/cbd"

#define SWITCH_PATH		"/sys/class/sec/switch/attached_dev"

/* property for vendor */
#define VPROP_CPBOOT            "vendor.cbd.cpboot"
#define VPROP_CPBOOT_DONE       "vendor.cbd.boot_done"
#define VPROP_CPRESET_DONE      "vendor.cbd.reset_done"
#define VPROP_RFS_CHECKDONE     "vendor.cbd.rfs_check_done"
#define VPROP_FIRST_XMIT_DONE   "vendor.cbd.first_xmit_done"
#define VPROP_DT_REVISION       "vendor.cbd.dt_revision"
#define VPROP_MODE_CHANGE       "vendor.cbd.modechan"
#define VPROP_DEV_OFFRES        "vendor.cbd.deviceOffRes"

/* property for system */
#define PROP_SERIAL_NO          "ro.serialno"
#define PROP_SALES_CODE         "ro.csc.sales_code"
#define PROP_DEV_OFFREQ         "sys.shutdown.requested"
#define PROP_SYS_POWERCTL       "sys.powerctl"

#define STAGE_VSS		2

#define MAX_CMD_LINE_LEN	1024

#define MAX_NAME_LEN		32
#define MAX_PREFIX_LEN		64
#define MAX_SUFFIX_LEN		64
#define MAX_PATH_LEN		512

#define MAX_TOC_INDEX		16
#define MAX_TOC_ELEMENT_SIZE	32
#define MAX_TOC_SIZE		(MAX_TOC_INDEX * MAX_TOC_ELEMENT_SIZE)	/* 512 */
#define MAX_IMG_NAME_LEN	12

#define MAX_ERROR_INFO_BUF_SIZE 512

#define CRASH_REASON_SIZE	512
enum crash_type {
	CRASH_REASON_CP_ACT_CRASH = 0,
	CRASH_REASON_RIL_MNR,
	CRASH_REASON_RIL_REQ_FULL,
	CRASH_REASON_RIL_PHONE_DIE,
	CRASH_REASON_RIL_RSV_MAX,
	CRASH_REASON_USER = 5,
	CRASH_REASON_MIF_TX_ERR = 6,
	CRASH_REASON_MIF_RIL_BAD_CH,
	CRASH_REASON_MIF_RX_BAD_DATA,
	CRASH_REASON_MIF_ZMC,
	CRASH_REASON_MIF_RSV_0,
	CRASH_REASON_MIF_RSV_1,
	CRASH_REASON_MIF_RSV_MAX = 12,
	CRASH_REASON_CP_SRST,
	CRASH_REASON_CP_RSV_0,
	CRASH_REASON_CP_RSV_MAX = 15,
};

enum cp_boot_mode {
	CP_BOOT_MODE_NORMAL,
	CP_BOOT_MODE_DUMP,
	CP_BOOT_RE_INIT,
	CP_BOOT_REQ_CP_RAM_LOGGING = 5,
	CP_BOOT_MODE_MANUAL = 7,
	MAX_CP_BOOT_MODE
};

enum modem_state {
	STATE_OFFLINE,
	STATE_CRASH_RESET,	/* silent reset */
	STATE_CRASH_EXIT,	/* cp ramdump */
	STATE_BOOTING,
	STATE_ONLINE,
	STATE_NV_REBUILDING,	/* NV rebuilding start */
	STATE_LOADER_DONE,
	STATE_SIM_ATTACH,
	STATE_SIM_DETACH,
	STATE_CRASH_WATCHDOG,	/* cp watchdog crash */
};

enum modem_t {
	MODEM_INVALID = 0,
	IMC_XMM626X,
	IMC_XMM7160,
	SEC_CMC22X,
	VIA_CBP72,
	SEC_SS222,
	QC_ESC6270,
	SEC_SHANNON_HSIC,
	SEC_SS300,
	SEC_SS333,
	IMC_XMM72XX,
	IMC_XMM72XX_LLI,
	SEC_SS310,
	SEC_MODAP_AP,
	SEC_S5100,
	DUMMY,
	MAX_MODEM_TYPE
};

enum modem_link {
	LINKDEV_UNDEFINED,
	LINKDEV_MIPI,
	LINKDEV_DPRAM,
	LINKDEV_SPI,
	LINKDEV_USB,
	LINKDEV_HSIC,
	LINKDEV_C2C,
	LINKDEV_UART,
	LINKDEV_PLD,
	LINKDEV_SHMEM,
	LINKDEV_LLI,
	LINKDEV_PCIE,
	LINKDEV_MAX,
};

enum sec_debug_level {
	DBG_LOW,
	DBG_MID,
	DBG_HIGH,
	DBG_AUTO,
};

enum sec_cp_debug {
	DBG_CP_NORMAL,
	DBG_CP_NOCRASH,
	DBG_CP_NORESET,
	DBG_CP_NOBOOT,
	DBG_CP_AUTORESET,
	DBG_CP_FORCEPANIC,
};

enum TYPE_LOG {
	LOG_DMESG,
	LOG_DUMPSTATE,
	LOG_BOOT_FAIL,
};

#define LOGB_DMESG		(0x1 << LOG_DMESG)
#define LOGB_DUMPSTATE		(0x1 << LOG_DUMPSTATE)
#define LOGB_BOOTFAIL		(0x1 << LOG_BOOT_FAIL)

struct boot_args {
	enum modem_t type;
	enum modem_link lnk_boot;
	enum modem_link lnk_main;
	unsigned daemon;
	struct modem_comp *cpn; /*component*/
	unsigned options;	/*wildcard?*/
	void *modem_data;	/*extentions*/

	unsigned flb_mode; /* FLB MODE 1 for ITP and 0 for NORMAL MODE */
	char *printf_level; /* For IBP, holds the value to reply when modem asks for debug level */

	int debug_level;
	
	char reason[CRASH_REASON_SIZE];
};

struct modem_comp {
	enum modem_t type;
	char *name;
	char *rat;

	int (*start_boot)(struct boot_args *args);
	int (*start_dump)(struct boot_args *args);
	int (*shutdown)(struct boot_args *args);
	int (*upload_modem)(struct boot_args *args);

	char *node_boot;
	char *node_main;
	char *node_status;
	char *node_dump;
	char *path_bin;
	char *dnt_bin;

	char *path_nv;
	u32 nv_size;

	char *prop_boot_done;
	char *prop_reset_done;
};

struct modem_firmware {
	u8 *binary; /* Pointer to binary buffer */
	u32 size;		   /* Binary size */
	u32 m_offset;
	u32 b_offset;
	u32 mode;
	u32 len;
} __packed;

struct modem_sec_req {
	u32 mode;
	u32 param2;
	u32 param3;
	u32 param4;
} __packed;

struct crash_reason {
	u32 owner;
	char string[CRASH_REASON_SIZE];
} __packed;

/*============================================================================*\
	Definitions for file system (directory & file) management
\*============================================================================*/
/* void set_log_root(char *path);*/
char *get_log_root(void);
int create_log_directory(char *path);
/* void set_log_dir(char *path); */
char *get_log_dir(void);

/*============================================================================*\
	Definitions for saving debug log to a file
\*============================================================================*/
struct save_logs_arg {
	int type;
	char *prefix;
};
void save_logs(int type, char *prefix);

/* boot option*/
#define BOPT_ROOT		0x100
#define BOPT_EHCI_TEGRA		0x2000
#define BOPT_CPUPLOAD		0x10000

#define CP_MEMORY_MASK	(0x40000000 - 1)

int start_xmm626x_boot(struct boot_args *args);
int start_xmm626x_dump(struct boot_args *args);
int shutdown_xmm626x_modem(struct boot_args *args);

int start_cmc221_boot(struct boot_args *args);
int start_cmc221_dump(struct boot_args *args);
int start_cmc221_shutdown(struct boot_args *args);

int start_cbp72_boot(struct boot_args *args);
int start_cbp72_dump(struct boot_args *args);
int start_cbp72_shutdown(struct boot_args *args);

int start_esc6270_boot(struct boot_args *args);
int start_esc6270_dump(struct boot_args *args);
int shutdown_esc6270_modem(struct boot_args *args);

int start_shannon_hsic_boot(struct boot_args *args);
int start_shannon_hsic_dump(struct boot_args *args);
int shutdown_shannon_hsic(struct boot_args *args);

int start_shannon_boot(struct boot_args *args);
int start_shannon_dump(struct boot_args *args);
int shutdown_shannon_modem(struct boot_args *args);

int start_shannon333_boot(struct boot_args *args);
int start_shannon333_dump(struct boot_args *args);
int shutdown_shannon333_modem(struct boot_args *args);

int start_xmm72xx_boot(struct boot_args *args);
int start_xmm72xx_dump(struct boot_args *args);
int shutdown_xmm72xx_modem(struct boot_args *args);

int start_xmm72xx_lli_boot(struct boot_args *args);
int start_xmm72xx_lli_dump(struct boot_args *args);
int shutdown_xmm72xx_lli_modem(struct boot_args *args);

int start_shannon310_boot(struct boot_args *args);
int start_shannon310_dump(struct boot_args *args);
int shutdown_shannon310_modem(struct boot_args *args);
int start_shannon310_dummy_dump(struct boot_args *args);
int upload_shannon310_modem(struct boot_args *args);

int start_shannon5100_boot(struct boot_args *args);
int start_shannon5100_dump(struct boot_args *args);
int shutdown_shannon5100_modem(struct boot_args *args);
int upload_shannon5100_modem(struct boot_args *args);

#endif
