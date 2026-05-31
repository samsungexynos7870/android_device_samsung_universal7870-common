#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <linux/capability.h>
#include <sys/capability.h>
#include <linux/prctl.h>
#include <cutils/android_filesystem_config.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/prctl.h>
#include <cutils/properties.h>
#include <inttypes.h>

#include "boot.h"
#include "config.h"
#include "util.h"

/*
 * supported modem list
 */
static struct modem_comp m_list[MAX_MODEM_TYPE] = {
	[IMC_XMM626X] = {
		.type = IMC_XMM626X,
		.name = "xmm626x",
		.start_boot = start_xmm626x_boot,
		.start_dump = start_xmm626x_dump,
		.shutdown = shutdown_xmm626x_modem,
		.node_boot = "/dev/umts_boot0",
		.node_status = "/dev/umts_boot0",
		.path_bin = "/dev/mbin0",
	},
	[IMC_XMM7160] = {
		.type = IMC_XMM7160,
		.name = "xmm7160",
		.start_boot = start_xmm626x_boot,
		.start_dump = start_xmm626x_dump,
		.shutdown = shutdown_xmm626x_modem,
		.node_boot = "/dev/umts_boot0",
		.node_status = "/dev/umts_boot0",
		.path_bin = "/dev/mbin0",
	},
	[SEC_CMC22X] = {
		.type = SEC_CMC22X,
		.name = "cmc22x",
		.start_boot = start_cmc221_boot,
		.start_dump = start_cmc221_dump,
		.shutdown = start_cmc221_shutdown,
		.node_boot = "/dev/umts_boot0",
		.node_status = "/dev/umts_boot0",
		.path_bin = "/dev/block/mmcblk0p7",
	},
	[VIA_CBP72] = {
		.type = VIA_CBP72,
		.name = "cbp72",
		.start_boot = start_cbp72_boot,
		.start_dump = start_cbp72_dump,
		.shutdown = start_cbp72_shutdown,
		.node_boot = "/dev/cdma_boot0",
		.node_status = "/dev/cdma_boot0",
		.path_bin = "/dev/block/mmcblk0p8",
	},
	[QC_ESC6270] = {
		.type = QC_ESC6270,
		.name = "esc6270",
		.start_boot = start_esc6270_boot,
		.start_dump = start_esc6270_dump,
		.shutdown = shutdown_esc6270_modem,
		.node_boot = "/dev/gsm_boot0",
		.node_status = "/dev/gsm_ipc0",
		.path_bin = "/dev/block/mmcblk0p22",
	},
	[SEC_SHANNON_HSIC] = {
		.type = SEC_SHANNON_HSIC,
		.name = "cmc222",
		.start_boot = start_shannon_hsic_boot,
		.start_dump = start_shannon_hsic_dump,
		.shutdown = shutdown_shannon_hsic,
		.node_boot = "/dev/umts_boot0",
		.node_status = "/dev/umts_boot0",
		.path_bin = "/dev/block/mmcblk0p7",
		.path_nv = "/mnt/vendor/efs/nv_data.bin",
		.nv_size = (512 << 10),
	},
	[SEC_SS222] = {
		.type = SEC_SS222,
		.name = "ss222",
		.rat = "umts",
		.start_boot = start_shannon_boot,
		.start_dump = start_shannon_dump,
		.shutdown = shutdown_shannon_modem,
		.node_boot = "/dev/umts_boot0",
		.node_status = "/dev/umts_boot0",
		.node_dump = "/dev/umts_ramdump0",
		.path_bin = "/dev/block/mmcblk0p13",
		.path_nv = "/mnt/vendor/efs/nv_data.bin",
		.nv_size = (512 << 10),
	},
	[SEC_SS300] = {
		.type = SEC_SS300,
		.name = "ss300",
		.rat = "umts",
		.start_boot = start_shannon_boot,
		.start_dump = start_shannon_dump,
		.shutdown = shutdown_shannon_modem,
		.node_boot = "/dev/umts_boot0",
		.node_status = "/dev/umts_boot0",
		.node_dump = "/dev/umts_ramdump0",
		.path_bin = "/dev/mbin0",
		.path_nv = "/mnt/vendor/efs/nv_data.bin",
		.nv_size = (512 << 10),
	},
	[SEC_SS333] = {
		.type = SEC_SS333,
		.name = "ss333",
		.rat = "umts",
		.start_boot = start_shannon333_boot,
		.start_dump = start_shannon333_dump,
		.shutdown = shutdown_shannon333_modem,
		.node_boot = "/dev/umts_boot0",
		.node_status = "/dev/umts_boot0",
		.node_dump = "/dev/umts_ramdump0",
		.path_bin = "/dev/mbin0",
		.path_nv = "/mnt/vendor/efs/nv_data.bin",
		.nv_size = (512 << 10),
	},
	[IMC_XMM72XX] = {
		.type = IMC_XMM72XX,
		.name = "xmm72xx",
		.start_boot = start_xmm72xx_boot,
		.start_dump = start_xmm72xx_dump,
		.shutdown = shutdown_xmm72xx_modem,
		.node_boot = "/dev/umts_boot0",
		.node_status = "/dev/umts_boot0",
		.path_bin = "/dev/mbin0",
	},
	[IMC_XMM72XX_LLI] = {
		.type = IMC_XMM72XX_LLI,
		.name = "xmm72xx_lli",
		.start_boot = start_xmm72xx_lli_boot,
		.start_dump = start_xmm72xx_lli_dump,
		.shutdown = shutdown_xmm72xx_lli_modem,
		.node_boot = "/dev/umts_boot0",
		.node_status = "/dev/umts_boot0",
		.path_bin = "/dev/mbin0",
	},
	[SEC_SS310] = {
		.type = SEC_SS310,
		.name = "ss310",
		.rat = "umts",
		.start_boot = start_shannon310_boot,
		.start_dump = start_shannon310_dump,
		.shutdown = shutdown_shannon310_modem,
		.upload_modem = upload_shannon310_modem,
		.node_boot = "/dev/umts_boot0",
		.node_status = "/dev/umts_boot0",
		.node_dump = "/dev/umts_ramdump0",
		.path_bin = "/dev/block/mmcblk0p13",
		.path_nv = "/mnt/vendor/efs/nv_data.bin",
		.nv_size = (512 << 10),
	},
	[SEC_MODAP_AP] = {
		.type = SEC_MODAP_AP,
		.name = "ModAP_AP_only",
		.rat = "umts",
		.start_boot = start_shannon310_boot,
		.start_dump = start_shannon310_dummy_dump,
		.shutdown = shutdown_shannon310_modem,
		.node_boot = "/dev/umts_boot0",
		.node_status = "/dev/umts_boot0",
		.node_dump = "",
		.path_bin = "/dev/block/mmcblk0p13",
		.path_nv = "",
		.nv_size = (512 << 10),
	},
	[SEC_S5100] = {
		.type = SEC_S5100,
		.name = "s5100",
		.rat = "nr",
		.start_boot = start_shannon5100_boot,
		.start_dump = start_shannon5100_dump,
		.shutdown = shutdown_shannon5100_modem,
		.upload_modem = upload_shannon5100_modem,
		.node_boot = "/dev/nr_boot0",
		.node_status = "/dev/nr_boot0",
		.node_dump = "/dev/nr_ramdump0",
		.path_bin = "/dev/block/mmcblk0p13",
		.path_nv = "/mnt/vendor/efs/nv_5g_data.bin",
		.nv_size = (512 << 10),
	},
};

char printf_level[6];

/*
 * print buf
 */
void print_data(char *data, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (i && !(i % 16))
			printf("\n");
		printf("%02x ", *((unsigned char *)data + i));
	}
	printf("\n");
}

/*
 * kprintf - kernel printf
 *
 * Printout message to kmsg for syncing with radio log
 * if not defined BOOT_KERNEL_MSG, dprintf(kmsg_fd, fmt ...) will be printout to
 * STDOUT
 */
int kmsg_fd = STDOUT_FILENO;
#define kprintf(fmt) dprintf(kmsg_fd, fmt)

#ifdef DEBUG_KERNEL_MSG
int get_kmsg_fd(void)
{
	return kmsg_fd;
}

static void kprintf_init(void)
{
	char *name = "/dev/kmsg";
	kmsg_fd = open(name, O_RDWR);
}

static void kprintf_deinit(void)
{
	if (kmsg_fd != STDOUT_FILENO)
		close(kmsg_fd);
}
#else
#define kprintf_init() { do {} while(0); }
#define kprintf_deinit() { do {} while(0); }
#endif

/* support below capability since Linux 3.5 */
#define CAP_SYSLOG	34
#define CAP_BLOCK_SUSPEND 36
/*
 * switchUser - Switches UID to radio, preserving CAP_NET_ADMIN capabilities.
 * Our group, cache, was set by init.
 * get from rild.c
 */
void switch_user(void)
{
	prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);
	setuid(AID_RADIO);
	struct __user_cap_header_struct header;
	struct __user_cap_data_struct cap[2];

	header.version = _LINUX_CAPABILITY_VERSION_2;
	header.pid = 0;

	cap[0].effective = cap[0].permitted = (1 << CAP_NET_ADMIN)
			| (1 << CAP_SYS_ADMIN) | (1 << CAP_NET_RAW)
			| (1 << CAP_SYS_BOOT);
	cap[0].inheritable = 0;

	cap[1].effective = cap[1].permitted =
		CAP_TO_MASK(CAP_SYSLOG) | CAP_TO_MASK(CAP_BLOCK_SUSPEND);
	cap[1].inheritable = 0;

	capset(&header, cap);
}

/*
 * Parsing cmdline strings
 */
#define STR_CMDLINE "/proc/cmdline"
#define CMDLINE_BUF_SIZE 1024
static char *get_cmdline_str(char *buf, unsigned size,  char *find)
{
	char *ptr;
	int fd;

	fd = open(STR_CMDLINE, O_RDONLY);
	if (fd >= 0) {
		int n = read(fd, buf, size - 1);
		if (n < 0)
			n = 0;

		/* get rid of trailing newline, it happens */
		if (n > 0 && buf[n-1] == '\n')
			n--;

		buf[n] = 0;
		close(fd);
	} else
		buf[0] = 0;

	ptr = buf;
	while (ptr && *ptr) {
		char *x = strchr(ptr, ' ');
		if (x != 0)
			*x++ = 0;
		if (strncmp(ptr, find, strlen(find)) == 0) {
			cbd_log("find str %s form %s\n", ptr, STR_CMDLINE);
			return ptr;
		}
		ptr = x;
	}
	return NULL;
}

/*
 * check_debug_level
 */
int debug_level = DBG_LOW;
int debug_cp_opt = DBG_CP_NORMAL;
#ifdef CONFIG_USE_LFS
#define FILE_DEBUG_LEVEL "/mnt/.lfs/debug_level.inf"
static void check_debug_level(void)
{
	int fd;
	int err;
	char rdbuf[4];

	fd = open(FILE_DEBUG_LEVEL, O_RDONLY);
	if (fd < 0) {
		cbd_log("%s file open fail err = %d\n", FILE_DEBUG_LEVEL, fd);
		goto exit;
	}
	err = read(fd, rdbuf, 4);
	if (err < 0) {
		cbd_log("debug level read fail err = %d\n", err);
		goto exit;
	}
	if (!strncmp("DLOW", rdbuf, 4))
		debug_level = DBG_LOW;
	else if (!strncmp("DAUT", rdbuf, 4))
		debug_level = DBG_AUTO;
	else if (!strncmp("DMID", rdbuf, 4))
		debug_level = DBG_MID;
	else if (!strncmp("DHIG", rdbuf, 4))
		debug_level = DBG_HIGH;

	cbd_log("debug level = %d\n", debug_level);

exit:
	return;
}
#else
static char cmdline[CMDLINE_BUF_SIZE];
static void check_debug_level(void)
{
	char *str, *cmd;

	/* Check the CP debug option first */
	cmd = "sec_debug.cp=";
	memset(cmdline, 0x00, CMDLINE_BUF_SIZE);
	str = get_cmdline_str(cmdline, CMDLINE_BUF_SIZE, cmd);
	if (str) {
		debug_cp_opt = *(str + strlen(cmd)) - '0';
		cbd_log("%s, %d\n", str, debug_cp_opt);
	}

	/* check androidboot.debug_level= */
	cmd = "androidboot.debug_level=";
	memset(cmdline, 0x00, CMDLINE_BUF_SIZE);
	str = get_cmdline_str(cmdline, CMDLINE_BUF_SIZE, cmd);
	if (str) {
		switch (strtol(str + strlen(cmd), NULL, 16)) {
		case 0x4F4C: /*LOW - 0x4f4c*/
			debug_level = DBG_LOW;
			cbd_log("%s, %d\n", str, debug_level);
			goto exit;
		case 0x4945: /*MID - 0x4945*/
			debug_level = DBG_MID;
			cbd_log("%s, %d\n", str, debug_level);
			goto exit;
		case 0x4948: /*HIGH - 0x4948*/
			debug_level = DBG_HIGH;
			cbd_log("%s, %d\n", str, debug_level);
			goto exit;
		default:
			cbd_log("%s, debug level undefined\n", str);
			break;
		}
	}

	cmd = "sec_debug.level=";
	memset(cmdline, 0x00, CMDLINE_BUF_SIZE);
	str = get_cmdline_str(cmdline, CMDLINE_BUF_SIZE, cmd);
	if (str) {
		if ( *(str + strlen(cmd)) - '0' != 0) {
			debug_level = DBG_MID;
			cbd_log("%s, %d\n", str, debug_level);
		}
		goto exit;
	}

	cmd = "sec_debug.enable=";
	memset(cmdline, 0x00, CMDLINE_BUF_SIZE);
	str = get_cmdline_str(cmdline, CMDLINE_BUF_SIZE, cmd);
	if (str) {
		if ( *(str + strlen(cmd)) - '0' != 0) {
			debug_level = DBG_MID;
			cbd_log("%s, %d\n", str, debug_level);
		}
		goto exit;
	}
exit:
	cbd_log("debug level=%d, cp_debug=%d\n", debug_level, debug_cp_opt);
}
#endif

/* "root" of each FS == mount point */
char log_root[MAX_PATH_LEN] = CPDUMP_ROOT;

/* -- prevent defect !! //23578
void set_log_root(char *path)
{
	strncpy(log_root, path, MAX_PATH_LEN);
}*/

char *get_log_root(void)
{
	return log_root;
}

static int create_directory(char *path)
{
	struct stat ldir_st;
	int err;

	err = stat(path, &ldir_st);
	if (!err) {
		/* path exist */
		if (!S_ISDIR(ldir_st.st_mode)) {
			cbd_log("(%s) is not a directory\n", path);
			err = -EFAULT;
			goto exit;
		}
		err = 0;
		goto exit;
	}

	err = mkdir(path, 0775);
	if (err < 0 && errno != EEXIST) {
		cbd_log("log path create fail err=%d(%d,%s)\n", err, errno, ERR2STR);
		goto exit;
	}
	cbd_log("log path (%s) created\n", path);
	err = 0;
exit:
	return err;
}

#define LOG_LIMIT_SIZE_MB 500
static int check_log_directory(char *path)
{
	struct statfs buf;
	int err;
	long fsize_mb;

	err = statfs(path, &buf);
	if (err < 0) {
		cbd_log("statvfs failed\n");
		return err;
	}
	fsize_mb = (buf.f_bfree / 1024) * (buf.f_bsize / 1024);
	cbd_log("Block size: %lu, Free Block: %" PRIu64 ", Free Size: %ld MB\n",
		buf.f_bsize, buf.f_bfree, fsize_mb);

	cbd_log("%s: freespace %ld / %d MB\n", path, fsize_mb, LOG_LIMIT_SIZE_MB);

	if (fsize_mb < LOG_LIMIT_SIZE_MB) {
		cbd_log("%s: under freespace %dMB\n", path, LOG_LIMIT_SIZE_MB);
		return -ENOSPC;
	}

	return 0;
}

/* full "pathname" of log directory */
char log_path[MAX_PATH_LEN] = FACTORY_CPDUMP_PATH;

int create_log_directory(char *path)
{
	int err = 0;
	int retrycnt_cd = 0;
	int retrycnt_cld = 0;

retry:
	if (debug_level != DBG_LOW) {
		/* check log path */
		err = create_directory(path);
		if (err) {
			if (retrycnt_cd++ < 5) {
				sleep(1);
				goto retry;
			}
			cbd_log("default path create fail %s\n", path);
			goto exit;
		}

		err = check_log_directory(path);
		if (err < 0) {
			if (retrycnt_cld++ < 5) {
				sleep(1);
				goto retry;
			}
			cbd_log("check_log_directory err(%d)\n", err);
			goto exit;
		}
	}
	return 0;
exit:
	return err;
}
/* -- prevent defect !! //23577
void set_log_dir(char *path)
{
	strncpy(log_path, path, MAX_PATH_LEN);
}
*/
char *get_log_dir(void)
{
	return log_path;
}

#ifdef CHECK_FACTORY_LOG_PATH
static void check_factory_log_path(void)
{
	int n;
	int fd;
	char buf[16] = {0, };

	fd = open(SWITCH_PATH, O_RDONLY);
	if (fd < 0) {
		cbd_log("fail to open %s (%d)\n", SWITCH_PATH, fd);

		return;
	}

	n = read(fd, buf, 3);
	if (n < 0) {
		cbd_log("fail to read %s (%d)\n", SWITCH_PATH, n);

		goto exit;
	}

	if (!strncasecmp(buf, "jig", 3))
		strcpy(log_path, FACTORY_CPDUMP_PATH);

exit:
	cbd_log("log path - %s\n", log_path);

	close(fd);
}
#endif

static struct save_logs_arg log_args;

void *_save_logs(void *arg)
{
	time_t now;
	struct tm result;
	char log_file_str[256], log_surfix[25];
	struct save_logs_arg *args = (struct save_logs_arg *)arg;
	int type = args->type;
	char *prefix = args->prefix;

	if (create_log_directory(log_path))
		return NULL;

	cbd_log("save dmesg begin...\n");

	time(&now);
	localtime_r(&now, &result);
	strftime(log_surfix, 20, "%Y%m%d%H%M_%S", &result);

	if (type & LOGB_BOOTFAIL) {
		sprintf(log_file_str, "echo %s > %s/%s_last.log", log_surfix,
			log_path, prefix);
		system(log_file_str);
		sprintf(log_file_str, "%s/%s_last.log", log_path, prefix);
		dmesg_to_file(log_file_str);
		cbd_log("%s\n", log_file_str);
		return NULL;
	}
	if (type & LOGB_DUMPSTATE) {
		sprintf(log_file_str, "dumpstate >> %s/%s_%s.log", log_path,
			prefix,	log_surfix);
		system(log_file_str);
		cbd_log("%s\n", log_file_str);
		return NULL; /* dumpstate contain both dmesg and radio */
	}
	if (type & LOGB_DMESG) {
		sprintf(log_file_str, "%s/%s_%s.log", log_path, prefix,
			log_surfix);
		dmesg_to_file(log_file_str);
		cbd_log("%s\n", log_file_str);
	}
	sprintf(log_file_str, "echo >> %s/%s_%s.log", log_path, prefix,	log_surfix);
	system(log_file_str);

	return NULL;
}

void save_logs(int type, char *prefix)
{
	struct save_logs_arg args = {.type = type, .prefix = prefix};
	_save_logs((void *)&args);
}

void save_logs_thread(int type, char *prefix)
{
	pthread_t thr;
	int ret;

	if (debug_level == DBG_LOW)
		return;

	log_args.type = type;
	log_args.prefix = prefix;

	ret = pthread_create(&thr, NULL, _save_logs, (void *)&log_args);
	if (ret < 0)
		cbd_log("pthread_create fail\n");

	pthread_detach(thr);

	return;
}

static int set_kernel_panic(struct boot_args *args, char *reason)
{
	if (m_list[args->type].upload_modem == NULL)
		return -EINVAL;

	cbd_log("forced upload\n");

	sprintf(args->reason, "%s", reason);

	return m_list[args->type].upload_modem(args);
}

#define CRASH_DUMP_RETRY_COUNT	5
static int status_loop(struct boot_args *args)
{
	int fd = 0;
	int err, status;
	struct pollfd pfd = {
		.events = POLLHUP,
	};
	char deviceOff[PROPERTY_VALUE_MAX];
	char cpboot_log[PROPERTY_VALUE_MAX];
	char suffix[MAX_SUFFIX_LEN];

	int dump_retry = CRASH_DUMP_RETRY_COUNT;

	if (!args->daemon) {
		cbd_log("Cp boot Oneshot\n");
		return -EBUSY;
	}

	cbd_log("CP status_loop start (modem = %d)\n", args->type);

	fd = open(args->cpn->node_status, O_RDWR);
	if (fd < 0) {
		cbd_log("%s open fail, err %d\n", args->cpn->node_status, fd);

		/* if ipc0 node was not created, we can think cp was crass while
		* CP boot time, try get the dump and reset */
		if (debug_level == DBG_LOW)
			goto exit;

		if (debug_cp_opt == DBG_CP_NORMAL) {
			cbd_log("%s open fail, err = %d\n",
				args->cpn->node_status, fd);
			err = m_list[args->type].start_dump(args);
			if (err < 0)
				cbd_log("start_dump fail\n");
			goto exit;
		}
	}

	if (debug_level != DBG_LOW && debug_cp_opt == DBG_CP_NORMAL) {
		property_get(VPROP_CPBOOT_DONE, cpboot_log, "0");
		if (cpboot_log[0] == '1') {
			snprintf(suffix, MAX_SUFFIX_LEN, "cp_boot_done_%s",
					args->cpn->rat);
			/* cp was not first boot, save log*/
			save_logs_thread(LOGB_DMESG, suffix);
		}

		property_set(VPROP_CPRESET_DONE, "1");
	}

	/* Set the property for checking CP normal boot*/
	property_set(VPROP_CPBOOT_DONE, "1");

	pfd.fd = fd;

	if (debug_level != DBG_LOW && debug_cp_opt == DBG_CP_AUTORESET) {
		cbd_log("CP Silent reset repeat\n");
		err = poll(&pfd, 1, 50000);
		goto exit;
	}

	cbd_log("Wait event from modem %d\n", args->type);

	while (1) {
		pfd.revents = 0;
		err = poll(&pfd, 1, -1);
		if (!(pfd.revents & POLLHUP))
			continue;

		status = ioctl(fd, IOCTL_MODEM_STATUS);
		property_get(PROP_DEV_OFFREQ, deviceOff, "0");
		cbd_log("deviceOff = %s\n", deviceOff);
		if (deviceOff[0] == '1') {
			cbd_log("deviceOff = %s\n", deviceOff);
			if (m_list[args->type].shutdown != NULL) {
				cbd_log("shutdown\n");
				err = m_list[args->type].shutdown(args);
			}
			/* M0 SKT workaround request - 2012-04-10
			 * sometimes, rild can't get the PHONE_ACTIVE event
			 * while waiting cp off.
			 */
			property_set(VPROP_DEV_OFFRES, "1");
			while(1)
				sleep(0xff);
		}

		cbd_log("get event %d\n", status);

		switch (status) {
		case STATE_CRASH_RESET:
			if (debug_cp_opt == DBG_CP_FORCEPANIC) {
				err = set_kernel_panic(args, "Force a kernel panic");
				if (err)
					cbd_log("set_kernel_panic() error %d\n", err);
			}

			if (debug_cp_opt != DBG_CP_NORMAL)
				goto exit;

			cbd_log("STATE_CRASH_RESET, wait onrestart by rild\n");
			sleep(3);
			err = ioctl(fd, IOCTL_MODEM_OFF);
			if (err)
				cbd_log("cp off ioctl fail err=%d\n", err);

			cbd_log("RILD dosenot restart for 3sec, save logs\n\n");
			if (debug_level != DBG_LOW) {
				snprintf(suffix, MAX_SUFFIX_LEN, "cbd_only_%s",
						args->cpn->rat);
				save_logs_thread(LOGB_DMESG, suffix);
			}
			sleep(27);
			goto exit;
			break;

		case STATE_CRASH_WATCHDOG:
		case STATE_CRASH_EXIT:
			if (debug_cp_opt == DBG_CP_FORCEPANIC) {
				err = set_kernel_panic(args, "Force a kernel panic");
				if (err)
					cbd_log("set_kernel_panic() error %d\n", err);
			}

			if (debug_level == DBG_LOW) {
				/* In case of debug level low, cbd should wait for rild
				 * to kill current cbd process and start new cbd process */
				cbd_log("DBG_LOW, wait onrestart by rild\n");
				sleep(3);
				goto exit;
			}

			if (debug_cp_opt != DBG_CP_NORMAL)
				goto exit;

			cbd_log("CP status CRASH\n");

			/* save CP RAMDUMP */
			err = create_log_directory(log_path);
			if (err == -ENOSPC) {
				err = set_kernel_panic(args, "Not enough freespace");
				if (err)
					cbd_log("set_kernel_panic() error %d\n", err);
			} else if (err) {
				goto exit;
			}

			while (m_list[args->type].start_dump(args) < 0) {
				cbd_log("start_dump fail\n");
				if (dump_retry-- < 0)
					break;
				sleep(1);
			}

			cbd_log("CP status RESET\n");

			/* Restart boot daemon */
			goto exit;

		case STATE_NV_REBUILDING:
		default:
			cbd_log("unknown Modem status\n");
			continue;
		}
	};

exit:
	/*
	 * If this process was start for boot daemon, below code will be exit
	 * the process and will be restart by daemon service.
	 */
	if (fd > 0)
		close(fd);

	cbd_log("status loop exit\n");
	return 0;
}

static void help(char *name)
{
	printf("Usage: %s [OPTION]\n"
	       " -h Usage\n"
	       " -d Daemon mode\n"
	       " -t modem Type [cmc22x, xmm626x, etc.]\n"
	       " -b Boot link [d, h, s, u, p, c, m, l]\n"
	       " \t d(DPRAM) h(HSIC) s(SPI) u(UART) p(PLD) c(C2C) m(shared Memory) l(LLI)\n"
	       " -m Main link [d, h, s, p, c, m, l, e]\n"
	       " \t d(DPRAM) h(HSIC) s(SPI) p(PLD) c(C2C) m(shared Memory) l(LLI) e(PCIE)\n"
	       " -o Options [u, t, r]\n"
	       " \t u(Upload test) t(Tegra EHCI) r(run with root)\n"
	       " -p Partition# of CP binary\n"
	       " -B Boot device\n"
	       " -D Dump device\n",
	       name);
}

static int get_modem_state(struct boot_args *args)
{
	int status;
	int fd = open(args->cpn->node_boot, O_RDWR);

	if (fd < 0) {
		cbd_log("boot node open fail(%d)\n", fd);
		return -ENODEV;
	}
	status = ioctl(fd, IOCTL_MODEM_STATUS);
	close(fd);
	cbd_log("modem_status: %d\n", status);
	return status;
}

#if defined(CONFIG_THROUGHPUT_MONITOR)
void traffic_monitor()
{
	FILE *fp, *fp_qos;
	const char *iface = "rmnet0,rmnet1,rmnet2,rmnet3,rmnet4";
	char buffer[1024], name[32];
	unsigned long rx = 0, tx = 0, delta = 0, prev = 0, tmp_sum = 0;
	unsigned Mbps = 0;
	int fd;

	fd = open("/dev/network_throughput", O_RDWR);
	if (!fd) {
		cbd_log("Device doesn't support qos\n");
		goto error;
	}

	while (1) {
		fp = fopen("/proc/net/dev", "r");
		if (!fp) {
			cbd_log("Fail to open /proc/net/dev node with(%d)\n", errno);
			break;
		}

		/* Ignore unnecessary header data */
		fgets(buffer, sizeof(buffer), fp);
		fgets(buffer, sizeof(buffer), fp);
		tmp_sum = 0;

		while(fgets(buffer, sizeof(buffer), fp)) {
			buffer[strlen(buffer) - 1] = '\0';
			sscanf(buffer, "%30[^:]%*[:] %10lu %*s %*s %*s %*s %*s %*s %*s %10lu", name, &rx, &tx);

			if (!strstr(iface, name))
				continue;

			tmp_sum += tx;
			tmp_sum += rx;
		}

		delta = tmp_sum - prev;
		Mbps = (delta * 8) / (1000 * 1000);

		sprintf(buffer, "0x%x\n", Mbps);
		write(fd, buffer, strlen(buffer) + 1);
		prev = tmp_sum;

		if (fp)
			fclose(fp);

		sleep(1);
	}

error:
	if (fd)
		close(fd);

	pthread_exit(NULL);
}
#endif

int main(int argc, char *argv[])
{
	int opt;
	int err;
	char *cmd;
	char path[512], path2[512], node[32], node2[32];
	struct boot_args cbd_args = {
		.type = DEFAULT_MODEM,
		.lnk_boot = DEFAULT_BOOT_LINK,
		.lnk_main = DEFAULT_MAIN_LINK,
		.cpn = &m_list[DEFAULT_MODEM],
	};
	char deviceOff[PROPERTY_VALUE_MAX];

	int fd = 0;
	char uart_ctl_start[]= {'s','t','a','r','t','\0'};
	char uart_ctl_done[] = {'d','o','n','e','\0'};
#if defined(CONFIG_THROUGHPUT_MONITOR)
	pthread_t tm_thread;
#endif
	char suffix[MAX_SUFFIX_LEN];

	umask(2);
	kprintf_init();
	check_debug_level();
#ifdef CHECK_FACTORY_LOG_PATH
	check_factory_log_path();
#endif

	cbd_log("Start CP Boot Daemon (CBD)\n");

	while (1) {
		opt = getopt(argc, argv, "hdt:b:m:n:o:p:P:B:D:");
		if (opt == -1)
			break;

		switch (opt) {
		case 'h':
			help(argv[0]);
			return 0;

		case 'd':
			cbd_log("Daemon Mode\n");
			cbd_args.daemon = 1;
			break;

		case 't':
			cmd = optarg;

			if (!strncmp(cmd, "xmm626", 6)) {
				cbd_log("XMM626x modem\n");
				cbd_args.type = IMC_XMM626X;
				cbd_args.cpn = &m_list[IMC_XMM626X];
			} else if (!strncmp(cmd, "xmm716", 6)) {
				cbd_log("XMM7160 modem\n");
				cbd_args.type = IMC_XMM7160;
				cbd_args.cpn = &m_list[IMC_XMM7160];
			} else if (!strncmp(cmd, "cmc22", 5)) {
				cbd_log("CMC22x Modem boot\n");
				cbd_args.type = SEC_CMC22X;
				cbd_args.cpn = &m_list[SEC_CMC22X];
			} else if (!strncmp(cmd, "cbp72", 5)) {
				cbd_log("CBP7.2 Modem boot\n");
				cbd_args.type = VIA_CBP72;
				cbd_args.cpn = &m_list[VIA_CBP72];
			} else if (!strncmp(cmd, "esc6270", 7)) {
				cbd_log("ESC6270 Modem boot\n");
				cbd_args.type = QC_ESC6270;
				cbd_args.cpn = &m_list[QC_ESC6270];
			} else if (!strncmp(cmd, "shannon_hsic", 12)) {
				cbd_log("SHANNON HSIC Modem boot\n");
				cbd_args.type = SEC_SHANNON_HSIC;
				cbd_args.lnk_boot = LINKDEV_SPI;
				cbd_args.lnk_main = LINKDEV_HSIC;
				cbd_args.cpn = &m_list[SEC_SHANNON_HSIC];
			} else if (!strncmp(cmd, "ss222", 5)) {
				cbd_log("SS222 modem\n");
				cbd_args.type = SEC_SS222;
				cbd_args.lnk_boot = LINKDEV_SPI;
				cbd_args.lnk_main = LINKDEV_C2C;
				cbd_args.cpn = &m_list[SEC_SS222];
			} else if (!strncmp(cmd, "ss300", 5)) {
				cbd_log("SS300 modem\n");
				cbd_args.type = SEC_SS300;
				cbd_args.lnk_boot = LINKDEV_SPI;
				cbd_args.lnk_main = LINKDEV_LLI;
				cbd_args.cpn = &m_list[SEC_SS300];
			} else if (!strncmp(cmd, "ss333", 5)) {
				cbd_log("SS333 modem\n");
				cbd_args.type = SEC_SS333;
				cbd_args.lnk_boot = LINKDEV_SPI;
				cbd_args.lnk_main = LINKDEV_LLI;
				cbd_args.cpn = &m_list[SEC_SS333];
			} else if (!strncmp(cmd, "xmm72xx_lli", 11)) {
				cbd_log("XMM72xx_lli modem\n");
				cbd_args.type = IMC_XMM72XX_LLI;
				cbd_args.cpn = &m_list[IMC_XMM72XX_LLI];
			} else if (!strncmp(cmd, "xmm72xx", 7)) {
				cbd_log("XMM72xx modem\n");
				cbd_args.type = IMC_XMM72XX;
				cbd_args.cpn = &m_list[IMC_XMM72XX];
			} else if (!strncmp(cmd, "ss310", 5)) {
				cbd_log("SS310 modem\n");
				cbd_args.type = SEC_SS310;
				cbd_args.lnk_boot = LINKDEV_SHMEM;
				cbd_args.lnk_main = LINKDEV_SHMEM;
				cbd_args.cpn = &m_list[SEC_SS310];
			} else if (!strncmp(cmd, "modap_ap", 8)) {
				cbd_log("ModAP AP only\n");
				cbd_args.type = SEC_MODAP_AP;
				cbd_args.lnk_boot = LINKDEV_SHMEM;
				cbd_args.lnk_main = LINKDEV_SHMEM;
				cbd_args.cpn = &m_list[SEC_MODAP_AP];
			} else if (!strncmp(cmd, "s5100", 5)) {
				cbd_log("S5100 modem\n");
				cbd_args.type = SEC_S5100;
				cbd_args.lnk_boot = LINKDEV_SPI;
				cbd_args.lnk_main = LINKDEV_PCIE;
				cbd_args.cpn = &m_list[SEC_S5100];
			} else {
				cbd_log("Unknown modem\n");
				return -1;
			}
			cbd_args.cpn->path_bin = m_list[cbd_args.type].path_bin;
			break;

		case 'b':
			cmd = optarg;
			switch (cmd[0]) {
			case 'd':
				cbd_log("boot dpram link\n");
				cbd_args.lnk_boot = LINKDEV_DPRAM;
				break;
			case 'h':
				cbd_log("boot hsic link\n");
				cbd_args.lnk_boot = LINKDEV_HSIC;
				break;
			case 's':
				cbd_log("boot spi link\n");
				cbd_args.lnk_boot = LINKDEV_SPI;
				break;
			case 'u':
				cbd_log("boot uart link\n");
				cbd_args.lnk_boot = LINKDEV_UART;
				break;
			case 'p':
				cbd_log("boot pld link\n");
				cbd_args.lnk_boot = LINKDEV_PLD;
				break;
			case 'c':
				cbd_log("boot C2C link\n");
				cbd_args.lnk_boot = LINKDEV_C2C;
				break;
			case 'm':
				cbd_log("boot SHMEM link\n");
				cbd_args.lnk_boot = LINKDEV_SHMEM;
				break;
			case 'l':
				cbd_log("boot LLI link\n");
				cbd_args.lnk_boot = LINKDEV_LLI;
				break;
			default:
				cbd_log("ERR! invalid boot link %c\n", cmd[0]);
				return -EINVAL;
			}
			break;

		case 'm':
			cmd = optarg;
			switch (cmd[0]) {
			case 'd':
				cbd_log("boot dpram link\n");
				cbd_args.lnk_main = LINKDEV_DPRAM;
				break;
			case 'h':
				cbd_log("boot hsic link\n");
				cbd_args.lnk_main = LINKDEV_HSIC;
				break;
			case 's':
				cbd_log("boot spi link\n");
				cbd_args.lnk_main = LINKDEV_SPI;
				break;
			case 'p':
				cbd_log("boot pld link\n");
				cbd_args.lnk_main = LINKDEV_PLD;
				break;
			case 'c':
				cbd_log("main C2C link\n");
				cbd_args.lnk_main = LINKDEV_C2C;
				break;
			case 'm':
				cbd_log("main SHMEM link\n");
				cbd_args.lnk_main = LINKDEV_SHMEM;
				break;
			case 'l':
				cbd_log("main LLI link\n");
				cbd_args.lnk_main = LINKDEV_LLI;
				break;
			case 'e':
				cbd_log("main PCIE link\n");
				cbd_args.lnk_main = LINKDEV_PCIE;
				break;
			default:
				cbd_log("ERR! invalid main link %c\n", cmd[0]);
				return -EINVAL;
			}
			break;

		case 'n':
			cmd = optarg;
			cbd_log("set nv partition : %s\n", cmd);

			sprintf(path2, "%s/nv_data.bin", cmd);
			cbd_args.cpn->path_nv = path2;
			cbd_log("nv file path : %s\n", cbd_args.cpn->path_nv);
			break;

		case 'o':
			cmd = optarg;
			switch (cmd[0]) {
			case 'u':
				cbd_log("Upload Test\n");
				cbd_args.options |= BOPT_CPUPLOAD;
				break;
			case 't':
				cbd_log("EHCI Tegra\n");
				cbd_args.options |= BOPT_EHCI_TEGRA;
				break;
			case 'r':
				cbd_log("run with root\n");
				cbd_args.options |= BOPT_ROOT;
				break;
			default:
				cbd_log("ERR! invalid option %c\n", cmd[0]);
				break;
			}
			break;

		case 'p':
			cmd = optarg;
			cbd_log("partition number : %s\n", cmd);

			sprintf(path, "/dev/block/mmcblk0p%s", cmd);
			cbd_args.cpn->path_bin = path;
			cbd_log("partition path : %s\n", cbd_args.cpn->path_bin);
			break;

		case 'P':
			cmd = optarg;
			cbd_log("partition number : %s\n", cmd);

			sprintf(path, "/dev/block/%s", cmd);
			cbd_args.cpn->path_bin = path;
			cbd_log("partition path : %s\n", cbd_args.cpn->path_bin);
			break;

		case 'B':
			cmd = optarg;
			cbd_log("set boot_node : %s\n", cmd);

			sprintf(node, "/dev/%s", cmd);
			cbd_args.cpn->node_boot = node;
			cbd_args.cpn->node_status = node;
			cbd_log("boot_node : %s\n", cbd_args.cpn->node_boot);
			cbd_log("boot_node : %s\n", cbd_args.cpn->node_status);
			break;

		case 'D':
			cmd = optarg;
			cbd_log("set dump_node : %s\n", cmd);

			sprintf(node2, "/dev/%s", cmd);
			cbd_args.cpn->node_dump = node2;
			cbd_log("dump_node : %s\n", cbd_args.cpn->node_dump);
			break;

		default:
			cbd_log("WARNING! invalid option %c\n", opt);
			break;
		}
	}

	if (!cbd_args.type) {
		cbd_log("Invaild modem type %d\n", cbd_args.type);
		/* if boot daemon was started with init.rc service, below loop
		 * will not restart boot daemon
		 */
		while(1);
		goto exit;
	}

	if (cbd_args.options & BOPT_CPUPLOAD) {
		cbd_log("\n\n ---- upload ----\n\n");
		sleep(5);
		err = m_list[cbd_args.type].start_dump(&cbd_args);
		if (err < 0) {
			cbd_log("start boot fail\n");
			goto exit;
		}
		goto exit;
	}

	if (cbd_args.options & BOPT_ROOT)
		cbd_log("Start with root\n");
	else
		switch_user();

__cpboot_retry:

	property_get(PROP_DEV_OFFREQ, deviceOff, "0");
	if (deviceOff[0] == '1') {
		cbd_log("deviceOff = %s\n", deviceOff);
		if (m_list[cbd_args.type].shutdown != NULL)
			err = m_list[cbd_args.type].shutdown(&cbd_args);
		while(1)
			sleep(0xff);
	}

	/* CP debugging */
	if (debug_level != DBG_LOW) {
		char cpboot[PROPERTY_VALUE_MAX];
		switch (debug_cp_opt) {
		case DBG_CP_NOBOOT:
			cbd_log("CP_DEBUG NOBOOT\n");
			goto skip_cpboot;
			break;
		case DBG_CP_NORESET:
			property_get(VPROP_CPBOOT, cpboot, "");
			if (cpboot[0] == '1') {
				cbd_log("CP_DEBUG NORESET\n");
				goto skip_cpboot;
			} else {
				cbd_log("CP_DEBUG NORESET prop set\n");
				property_set(VPROP_CPBOOT, "1");
			}
			break;
		}

		/* save cp reset log once */
		property_get(VPROP_CPRESET_DONE, cpboot, "");
		if (cpboot[0] == '1') {
			snprintf(suffix, MAX_SUFFIX_LEN, "cp_boot_%s",
					cbd_args.cpn->rat);
			save_logs_thread(LOGB_DMESG, suffix);
			property_set(VPROP_CPRESET_DONE, "0");
		}
	}

	/* call start boot code */
	cbd_log("Call boot start\n");
	cbd_log("Modem type = %d\n", cbd_args.type);
	cbd_log("Boot link = %d\n", cbd_args.lnk_boot);
	cbd_log("Main link = %d\n", cbd_args.lnk_main);

	/* Prevent to unexpected uart-input to VIA */
	if (cbd_args.type == VIA_CBP72) {
		if (!fd)
			fd = open("/sys/class/sec/switch/check_cpboot", O_WRONLY);

		if(fd > 0) {
			cbd_log("Fix CP-UART path to CMC during CP Booting\n");
			write(fd, uart_ctl_start, 6);
		} else {
			cbd_log("Error opening sysfs file\n");
		}
	}

	err = m_list[cbd_args.type].start_boot(&cbd_args);
	if (err < 0) {
		cbd_log("start boot fail\n");
		if (debug_level != DBG_LOW && debug_cp_opt == DBG_CP_NORMAL) {
			if (get_modem_state(&cbd_args) == STATE_CRASH_EXIT ||
				get_modem_state(&cbd_args) == STATE_CRASH_WATCHDOG) {
				cbd_log("CP Crash when CP booting..\n");
				err = create_log_directory(log_path);
				if (err == -ENOSPC ) {
					if (m_list[cbd_args.type].upload_modem != NULL) {
						cbd_log("forced upload\n");
						sprintf(cbd_args.reason, "Not enough freespace");
						err = m_list[cbd_args.type].upload_modem(&cbd_args);
					}
				} else if (err){
					goto exit;
				}
				m_list[cbd_args.type].start_dump(&cbd_args);
			}
		}
		goto exit;
	}

skip_cpboot:
	cbd_log("Boot up process done!!!\n");

	if ((cbd_args.type == VIA_CBP72) && (fd > 0))
	{
		cbd_log("Reroute CP-UART path to VIA after CP Booting\n");
		write(fd, uart_ctl_done, 5);
		close(fd);
	}
#if defined(CONFIG_THROUGHPUT_MONITOR)
	err = pthread_create(&tm_thread, NULL, (void *)traffic_monitor, (void*)NULL);
	if (err != 0) {
		cbd_log("Fail to create traffic monitor thread!!!\n");
	} else {
		cbd_log("Success to create traffic monitor thread!!!\n");
	}
#endif

	if (status_loop(&cbd_args))
		goto deinit;

exit:
	sleep(1);
	goto __cpboot_retry;

deinit:
	kprintf_deinit();
	return 0;
}
