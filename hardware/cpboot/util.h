#ifndef __CBD_UTIL_H__
#define __CBD_UTIL_H__

#include "boot.h"

/*============================================================================*\
	Definitions for UART
\*============================================================================*/
#ifndef TIOCMODG
#  ifdef TIOCMGET
#    define TIOCMODG TIOCMGET
#  else
#    ifdef MCGETA
#      define TIOCMODG MCGETA
#    endif
#  endif
#endif

#ifndef TIOCMODS
#  ifdef TIOCMSET
#    define TIOCMODS TIOCMSET
#  else
#    ifdef MCSETA
#      define TIOCMODS MCSETA
#    endif
#  endif
#endif

int open_serial(const char *dev, char *baudr, char *par, char *bits,
		char *stop, int hwf, int swf);
int open_serial_nonblockingmode(const char *dev, char *baudr, char *par, char *bits,
		char *stop, int hwf, int swf);
void close_serial(int fp);

/*============================================================================*\
	Definitions for MIF Logger
\*============================================================================*/
#define MAX_LOG_SIZE 64

#define MAX_IPC_LOG_SIZE \
	(MAX_LOG_SIZE - sizeof(enum mif_log_id) - sizeof(unsigned long long) - sizeof(size_t))
#define MAX_IRQ_LOG_SIZE \
	(MAX_LOG_SIZE - sizeof(enum mif_log_id) - sizeof(unsigned long long) - sizeof(struct mif_irq_map))
#define MAX_COM_LOG_SIZE \
	(MAX_LOG_SIZE - sizeof(enum mif_log_id) - sizeof(unsigned long long))
#define MAX_TIM_LOG_SIZE \
	(MAX_LOG_SIZE - sizeof(enum mif_log_id) - sizeof(unsigned long long) - sizeof(struct timespec))

#define MAX_BUF_SIZE 4096
#define MAX_PATH_SIZE 256
#define DIR_PATH "/dev/"
//#define FILE_PATH "/data/log/"
#define FILE_PATH "/sdcard/log/"

#define rest_len(x) (sizeof(x) - strnlen(x, sizeof(x)) - 1)

enum mif_log_id {
	MIF_IPC_RL2AP = 1,
	MIF_IPC_AP2CP,
	MIF_IPC_CP2AP,
	MIF_IPC_AP2RL,
	MIF_IPC_FLAG,
	MIF_IRQ,
	MIF_COM,
	MIF_TIME
};

struct mif_irq_map {
	u16 magic;
	u16 access;

	u16 fmt_tx_in;
	u16 fmt_tx_out;
	u16 fmt_rx_in;
	u16 fmt_rx_out;

	u16 raw_tx_in;
	u16 raw_tx_out;
	u16 raw_rx_in;
	u16 raw_rx_out;

	u16 cp2ap;
};

struct kernel_time {
	unsigned long long sec;
	unsigned long nanosec;
};

struct mif_ipc_block {
	enum mif_log_id id;
	unsigned long long time;
	size_t len;
	char buff[MAX_IPC_LOG_SIZE];
};

struct mif_irq_block {
	enum mif_log_id id;
	unsigned long long time;
	struct mif_irq_map map;
	char buff[MAX_IRQ_LOG_SIZE];
};

struct mif_common_block {
	enum mif_log_id id;
	unsigned long long time;
	char buff[MAX_COM_LOG_SIZE];
};

struct mif_time_block {
	enum mif_log_id id;
	unsigned long long time;
	struct timespec epoch;
	char buff[MAX_TIM_LOG_SIZE];
};

int exec_mif_logger();

/*============================================================================*\
	Definitions for Error Check (Detection and/or Correction)
\*============================================================================*/
unsigned long update_crc32(unsigned long crc, unsigned char *buff, unsigned long len);

/*============================================================================*\
	Definitions for NV data file management
\*============================================================================*/
int create_empty_nv(char *path, size_t size);

/*============================================================================*\
	Definitions for debug log
\*============================================================================*/
int dmesg_to_file(char *of);
void remove_logs(int type, char *log_dir, char *reason);

/*============================================================================*\
	Functions for file system (directory & file) management
\*============================================================================*/
#define MIN_FS_FREE_SPACE	256	/* 256 MB */
int check_fs_free_space(char *root);
int check_directory(char *path);

int set_file_value(char* file, char* strval);
int get_file_value(char* file, char* ch, int len);
int wait_file_value(char* file, char *ch, int cnt);

#define ROOT_PATH		"/sys/bus/platform/drivers"
int get_device_path(const char *drv_name, const char *key, char *fpath);
int set_gpio_value(char *drv_name, char *key, char *gpio, char *strval);
int wait_gpio_value(char *drv_name, char *key, char* gpio, char *val, int cnt);
#endif
