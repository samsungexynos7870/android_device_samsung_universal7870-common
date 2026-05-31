/* CP Binary offsets */
#define NV_FILE_MODE	00700
#define MAX_NVDATA_SIZE	(2 * 1024 * 1024)

#define IOCTL_LINK_CONTROL_ENABLE	_IO('o', 0x30)
#define IOCTL_LINK_CONTROL_ACTIVE	_IO('o', 0x31)
#define IOCTL_LINK_GET_HOSTWAKE		_IO('o', 0x32)
#define IOCTL_LINK_CONNECTED		_IO('o', 0x33)
#define IOCTL_LINK_SET_BIAS_CLEAR	_IO('o', 0x34)
#define IOCTL_MODEM_CP_UPLOAD		_IO('o', 0x35)

#define IMG_TABLE_MAX_SIZE		512 /* fixedi by cp */

/* EHCI node */
#define EXYNOS_EHCI		"/dev/ehci_power"
/*#define EXYNOS_EHCI		"/sys/devices/platform/s5p-ehci/ehci_power"*/
#define EXYNOS_PORT_POWER	"/sys/devices/platform/s5p-ehci/port_power"
#define EXYNOS_OHCI		"/sys/devices/platform/exynos-ohci/ohci_power"
#define TEGRA_EHCI		"/sys/devices/platform/tegra-ehci.1/ehci_power"
#define USB_REMOVE		"/sys/bus/usb/devices/1-2/remove"
#define LINK_IDVENDOR		"/sys/bus/usb/devices/1-2/idVendor"
#define LINK_IDPRODUCT		"/sys/bus/usb/devices/1-2/idProduct"
#define MIPI_LLI_CONTROL	"/dev/mipi-lli/lli_control"

#define MODEM_LINK		"/dev/link_pm"
#define NV_PATH			"/mnt/vendor/efs/nv_data.bin"
#define NVMD5_PATH		"/mnt/vendor/efs/nv_data.bin.md5"
#define FILE_MODE		0755
#define HSIC_PAYLOAD_SIZE	16384
#define EBL_PAYLOAD_SIZE	0x2000
#define READ_BUF_SIZE		16384
#define ERROR_INFO_SIZE		150
#define ID_VENDOR		"8087"
#define ID_PRODUCT		"07ef"
#define HSIC_ENUM_COUNTER	10
#define BOOT_4KB		4096

#define MAGIC_PSI_UPLOAD	0xDEADDEAD;
#define MAGIC_DUMP_NACK		0xFEFEFEFE;
#define MAGIC_DUMP_ACK		0xABCDABCD;

enum hsic_img_type {
	IMG_PSI,	/*primary signed image*/
	IMG_EBL,
	IMG_MAIN0,
	IMG_MAIN1,
	IMG_MAIN2,
	IMG_SECPACK,
	IMG_LTE,
	IMG_LTESECPACK,
	IMG_USPC,
	IMG_USPCSECPACK,
	IMG_FW,
	IMG_FWSECPACK,
	IMG_NV,
	IMG_MAX_IDX,
};

struct cp_imgmap {
	char name[12];		/*Binary name*/
	unsigned bin_offset;	/*Binary offset*/
	unsigned mem_offset;	/*Memory offset*/
	unsigned size;		/*size*/
	unsigned reserved[2];	/*reserved*/
};

struct imc_args {
	struct boot_args *cbd_args;
	struct cp_imgmap *img_tab;
	int bin_fd;
	int boot_fd;
	int chip_id;
};

struct image_buf {
	unsigned int length;
	unsigned char *buf;
};
/*
 * IMC xmm626x boot protocol definitions
 */

#define PSI_BOOTINFO_BYTE 26
struct bootloader_info {
	unsigned int boot_mode;
	unsigned int major;
	unsigned int minor;
	unsigned char name[32];
	unsigned char capabilities[12];
	unsigned char boot_cfg_value;
	unsigned char reserved_byte;
	unsigned short expected_memory_class;
	unsigned int reserved_long;
	unsigned char ext_capabilities[12];
};

/* change the __packet to __attribute__ ((packed))
  gcc and SLP environment was not defind __packed at user space build.
  PSI header format was changed from XMM6360. */
union psi_header {
	struct {
		unsigned char indication;
		unsigned short length;
		unsigned char dummy;
	} __attribute__ ((packed)) psi62;
	struct {
		unsigned char indication;
		unsigned long length:24;
	} __attribute__ ((packed)) psi63;
};

enum package_type {
	exits				= 0,
	RspChecksumexit,
	RspRxTimeoutexit,

	exitHandling			= 0x40,
	IndexitMsg,
	IndWarningMsg,
	IndInfoMsg,
	IndDebugPackage,

	MainStuff			= 0x80,
	ReqRetransmission,
	CnfBaudChange,
	ErrWrongBaudrate,
	ReqCfiInfo_1,
	ReqCfiInfo_2,
	ReqSetProtConf,
	ReqProtocolChange,
	ErrWrongProtocol,

	TestAndDebug			= 0x0100,
	RspRxLoopback,
	ReqSetLed,

	LowLevelService			= 0x0200,
	ReqSetRamWorkAddress,
	ReqWriteToRam,
	ReqExecuteFromRam,
	ReqSecStart,
	ReqSecEnd,
	ReqCloseHandle,
	ReqBootloaderVer,
	ReqForceHwReset,

	FlashFunctions			= 0x800,
	ReqFlashId,
	ReqFlashSetAddress,
	ReqFlashReadBlock,
	ReqFlashWriteBlock,
	ReqFlashEraseStart,
	ReqFlashEraseCheck,
	ReqFlashReadChecksum,
	ReqCompressedPackage,
	ReqSetNandCtrlBitField,
	ReqSetNvmOptions,
	ReqFlashSetReadAddress,
	ReqOutOfSessionControl,
	ReqOutOfSessionDataWrite,
	ReqOutOfSessionDataRead,

	IndFotaexitMsg				= 0x10EF,
	IndFotaAlive				= 0x11EE,
	IndFotaChangeBaudrate			= 0x12ED,
	IndFotaReset				= 0x13EC,
	IndFotaUtaFlashInitialize		= 0x14EB,
	IndFotaUtaFlashDeinitialize		= 0x15EA,
	IndFotaUtaFlashGetUpdateBlockSize	= 0x16E9,
	IndFotaUtaFlashWriteBlock		= 0x17E8,
	IndFotaUtaFlashReadBlock		= 0x18E7,
	IndFotaUtaFlashEraseBlock		= 0x19E6,
	IndFotaUtaFlashEraseBlockStatusCheck	= 0x1AE5
};

struct hsic_cmd_header {
	unsigned short crc;
	unsigned short type;
	unsigned int length;
	char payload[0];
};

enum imc_chip_id {
	IMC_CHIPID_XMM626X = 0xf0,
	IMC_CHIPID_XMM6360 = 0xf1,
};

struct imc_secpack_bin {
	unsigned char undefined0[1920];
	unsigned addr;
	unsigned undefined1;
	unsigned len;
	unsigned char undefined2[12];
	unsigned len2;
	unsigned char undefined3[12];
	unsigned len3;
};
