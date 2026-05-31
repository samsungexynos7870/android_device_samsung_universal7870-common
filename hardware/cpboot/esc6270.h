/* CP Binary offsets */

#define FILE_MODE				00700
#define S_IRUSR					00400
#define S_IWUSR					00200
#define S_IRGRP					00040
#define S_IROTH					00004

#define CMD_READY				0x1234
#define CMD_START				0x4567
#define CMD_DONE				0xabcd

#define MAX_PORT_OPEN_RETRY		10
#define MAX_PSI_SIZE			(0x8000)
#define MAX_DBL_SIZE			(22 * 1024)

#define MAX_NVDATA_SIZE	(512 * 1024)
#define DBL_START_ADDR			0x70800000


#define MODEM_CODE_OFFSET		(0x0000)
#define MODEM_NV_OFFSET			(0xC00000)
#define MODEM_CODE_SIZE			(MODEM_NV_OFFSET - MODEM_CODE_OFFSET)
#define MODEM_CODE_SIZE_DIVIDER	8
#define MODEM_CODE_SIZE_FRAME	(MODEM_CODE_SIZE/MODEM_CODE_SIZE_DIVIDER)
#define MODEM_NV_SIZE			(512 * 1024)

#define PLD_BUFFER_SIZE			0x1000
#define DPRAM_BUFFER_SIZE		0x3C00

#define UART_PATH						"/dev/ttySAC3"
#define NVDATA_FILE						"/efs_gsm/nv_gsm_data.bin"

/* ioctl command definitions. */
#define IOC_MZ_MAGIC					('o')
#define IOCTL_DPRAM_PHONE_POWON			_IO(IOC_MZ_MAGIC, 0xd0)
#define IOCTL_DPRAM_PHONEIMG_LOAD		_IO(IOC_MZ_MAGIC, 0xd1)
#define IOCTL_DPRAM_NVDATA_LOAD			_IO(IOC_MZ_MAGIC, 0xd2)
#define IOCTL_DPRAM_PHONE_BOOTSTART		_IO(IOC_MZ_MAGIC, 0xd3)

#define DPRAM_PHONE_UPLOAD_STEP1		_IO(IOC_MZ_MAGIC, 0xde)
#define DPRAM_PHONE_UPLOAD_STEP2		_IO(IOC_MZ_MAGIC, 0xdf)

#if 0
#define IOCTL_MODEM_ON					_IO(IOC_MZ_MAGIC, 0x19)
#define IOCTL_MODEM_OFF					_IO(IOC_MZ_MAGIC, 0x20)
#define IOCTL_MODEM_RESET				_IO(IOC_MZ_MAGIC, 0x21)
#define IOCTL_MODEM_BOOT_ON				_IO(IOC_MZ_MAGIC, 0x22)
#define IOCTL_MODEM_BOOT_OFF			_IO(IOC_MZ_MAGIC, 0x23)
#endif

#define IOCTL_MODEM_SEND				_IO(IOC_MZ_MAGIC, 0x25)
#define IOCTL_MODEM_RECV				_IO(IOC_MZ_MAGIC, 0x26)

#define IOCTL_MODEM_DUMP_START			_IO('o', 0x32)
#define IOCTL_MODEM_DUMP_UPDATE			_IO('o', 0x33)
#define IOCTL_MODEM_FORCE_CRASH_EXIT	_IO('o', 0x34)
#define IOCTL_MODEM_CP_UPLOAD			_IO('o', 0x35)
#define IOCTL_MODEM_DUMP_RESET			_IO('o', 0x36)

#define IOCTL_DPRAM_SEND_BOOT			_IO('o', 0x40)
#define IOCTL_DPRAM_INIT_STATUS			_IO('o', 0x43)

/* ioctl command for IPC Logger */
#define IOCTL_MIF_LOG_DUMP				_IO('o', 0x51)
#define IOCTL_MIF_DPRAM_DUMP			_IO('o', 0x52)

struct qc_args {
	struct boot_args *cbd_args;
	struct cp_imgmap *img_tab;
	int bin_fd;
	int boot_fd;
	int link_fd;
};

struct _param_nv {
	unsigned char *addr;
	unsigned int size;
	unsigned int count;
	unsigned int tag;
};

enum dpram_img_type {
	IMG_DBL,	/*primary signed image*/
	IMG_MAIN,
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


