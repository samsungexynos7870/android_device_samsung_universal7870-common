/*
 * Shannon boot process
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <pthread.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>

#include <termios.h>

#include <stdarg.h>
#include <errno.h>
#include <cutils/properties.h>

#include "boot.h"
#include "shannon_hsic.h"
#include "util.h"

#define MODEM_LINK	"/dev/link_pm"
#define MAX_RETRY_CNT	10

#define MAIN_CONNECT	1
#define LOADER_CONNECT	2

static inline void msleep(int msec)
{
	usleep(msec * 1000);
}

/* Safe write and Safe read */
static ssize_t s_read(int fd, void *buf, size_t len)
{
	struct timeval tv;
	fd_set readfds;
	int err;
	int retry_cnt = MAX_RETRY_CNT;

retry:
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	tv.tv_sec = 5;
	tv.tv_usec = 0;

	err = select(fd+1, &readfds, (fd_set *)NULL, (fd_set *)NULL, &tv);
	switch (err) {
	case -1:
		cbd_log("select %s\n", "ERROR");
		err = -EINVAL;
		break;
	case 0:
		cbd_log("select %s\n", "TIMEOUT");
		err = -ETIMEDOUT;
		break;
	default:
		err = read(fd, buf, len);
		if (err < 0)
			cbd_log("read fail(%d)\n", err);
		break;
	}

	if (err == 0) {
		if (retry_cnt-- <= 0) {
			cbd_log("retry.. but read len is zero\n");
			return -EINVAL;
		}
		msleep(100);
		goto retry;
	}

	return err;
}

static inline ssize_t s_write(int fd, void *buf, size_t len)
{
	/* TODO: check the write size and if return was less than requset,
	 * repeat write the reset buf data*/
	return write(fd, buf, len);
}

static int send_boot_fw_bin(int boot_fd, int bin_fd, size_t size)
{
	int ret;
	int len;
	size_t rest = size;
	char buf[DATA_PAYLOAD_SIZE] = {0, };

	do {
		len = rest > DATA_PAYLOAD_SIZE ? DATA_PAYLOAD_SIZE : rest;
		ret = read(bin_fd, buf, len);
		if (ret < 0) {
			cbd_log("read() failed(%d)\n", ret);
			goto exit;
		}

		ret = s_write(boot_fd, buf, len);
		if (ret != len) {
			cbd_log("s_write() failed(%d)\n", ret);
			goto exit;
		}
		rest -= len;
	} while (rest > 0);

exit:
	return ret;
}

static int link_device_reset(void)
{
	int fd;
	int ret = -ENODEV;

	fd = open(MODEM_LINK, O_RDWR);
	if (fd < 0)
		goto exit;

	ret = ioctl(fd, IOCTL_LINK_DEVICE_RESET);

	close(fd);
exit:
	return ret;

}

static int check_dev_connect(int status)
{
	int fd;
	int spin = 20;
	int ret = -ENODEV;

	fd = open(MODEM_LINK, O_RDWR);
	if (fd < 0)
		goto exit;

	do {
		ret = ioctl(fd, IOCTL_LINK_CONNECTED, NULL);
		if (ret == status)
			break;
		msleep(500);
	} while (spin--);

	if (ret <= 0)
		ret = -ENODEV;

	close(fd);
exit:
	return ret;
}

static int hsic_usb_host_control(struct shannon_args *args, int value)
{
	int ehci_fd = -1, ohci_fd = -1, err;
	int tegra = !!(args->cbd_args->options & BOPT_EHCI_TEGRA);
	char *ehci_node = tegra ? TEGRA_EHCI : EXYNOS_EHCI;
	char *ohci_node = tegra ? NULL : EXYNOS_OHCI;

	/* OPEN EHCI */
	ehci_fd = open(ehci_node, O_RDWR);
	if (ehci_fd < 0) {
		cbd_log("%s open fail err=%d\n", ehci_node, ehci_fd);
		err = ehci_fd;
		goto exit;
	}
	/* OPEN OHCI */
	if (ohci_node) {
		ohci_fd = open(ohci_node, O_RDWR);
		if (ohci_fd < 0) {
			cbd_log("%s open fail err=%d\n", ohci_node, ohci_fd);
			err = ohci_fd;
			/* goto exit; */
			ohci_node = NULL;
		}
	}

	if (value) {
		err = write(ehci_fd, "1", strlen("1"));
		if (err < 0) {
			cbd_log("ehci write value=%s failed: %d\n", "1", err);
			goto exit;
		}
		if (ohci_node) {
			err = write(ohci_fd, "1", strlen("1"));
			if (err < 0) {
				cbd_log("ohci write value=%s failed: %d\n", "1", err);
				goto exit;
			}
		}
	} else {
		if (ohci_node) {
			err = write(ohci_fd, "0", strlen("0"));
			if (err < 0) {
				cbd_log("ohci write value=%s failed: %d\n", "0", err);
				goto exit;
			}
			usleep(50000);
		}
		err = write(ehci_fd, "0", strlen("0"));
		if (err < 0) {
			cbd_log("ehci write value=%s failed: %d\n", "0", err);
			goto exit;
		}
		usleep(50000);
	}
exit:
	if (ohci_node && ohci_fd >= 0)
		close(ohci_fd);
	if (ehci_fd >= 0)
		close(ehci_fd);
	return err;
}

static int xmit_boot_fw(struct shannon_args *args)
{
	int ret;
	int bin_fd = args->bin_fd;
	int boot_fd = args->boot_fd;

	struct image_map *toc;

	toc = &args->toc[IMAGE_TYPE_BOOT];
	ret = lseek(bin_fd, toc->bin_offset, SEEK_SET);
	if (ret < 0) {
		cbd_log("lseek failed(%d)\n", ret);
		goto exit;
	}

	cbd_log("%s send start\n", toc->name);
	ret = send_boot_fw_bin(boot_fd, bin_fd, toc->size);
	if (ret < 0) {
		cbd_log("send_boot_fw_bin failed(%d)\n", ret);
		goto exit;
	}

	cbd_log("%s(%d) send complete\n", toc->name, toc->size);

	if (wait_file_value(GPIO_CP2AP_STATUS, "1", 10) < 0) {
		ret = -ETIMEDOUT;
		goto exit;
	}

	if (args->cbd_args->lnk_main == LINKDEV_HSIC) {
		toc = &args->toc[IMAGE_TYPE_LOADER];
		ret = lseek(bin_fd, toc->bin_offset, SEEK_SET);
		if (ret < 0)
			goto exit;

		cbd_log("%s send start\n", toc->name);

		/* notify the loader bin's size to modem */
		ret = s_write(boot_fd, &toc->size, sizeof(size_t));
		if (ret < 0)
			goto exit;

		ret = send_boot_fw_bin(boot_fd, bin_fd, toc->size);
		if (ret < 0)
			goto exit;

		cbd_log("%s(%d) send complete\n", toc->name, toc->size);

		if (wait_file_value(GPIO_CP2AP_STATUS, "0", 10) < 0) {
			ret = -ETIMEDOUT;
			goto exit;
		}
	}
exit:
	return ret;
}

static inline int send_cmd(int boot_fd, int step, u32 cmd)
{
	u32 dat = CMD_DIR_AP2CP | step << 8 | cmd;

	cbd_log("cmd: %x\n", dat);

	return s_write(boot_fd, &dat, sizeof(u32));
}

static inline int read_res(int boot_fd, int step, u32 res)
{
	int ret;
	u32 dat;

	ret = s_read(boot_fd, &dat, sizeof(u32));
	if (ret < 0)
		return ret;

	cbd_log("res: %x\n", dat);

	if (dat != (CMD_DIR_CP2AP | step << 8 | res))
		return -EINVAL;

	return 0;
}

static int transact_cmd(int boot_fd, int step, u32 cmd, u32 res)
{
	int ret;

	ret = send_cmd(boot_fd, step, cmd);
	if (ret < 0) {
		cbd_log("send_cmd fail(%d)\n", ret);
		goto exit;
	}

	ret = read_res(boot_fd, step, res);
	if (ret < 0) {
		cbd_log("read_res fail(%d)\n", ret);
		return ret;
	}
exit:
	return 0;
}

static int send_bin(int boot_fd, int bin_fd,
		enum bootstrap_step step, u32 size)
{
	int ret;
	int len;
	u32 res;
	u32 rest = size;

	struct data_frame frame;
	frame.cmd = CMD_DIR_AP2CP | step << 8 | CMD_SEND_DATA;
	frame.num_frame = size % DATA_PAYLOAD_SIZE ?
			size / DATA_PAYLOAD_SIZE + 1 :
			size / DATA_PAYLOAD_SIZE;
	frame.curr_frame = 1;

	cbd_log("size: %d, frame: %d\n", size, frame.num_frame);

	do {
		len = rest > DATA_PAYLOAD_SIZE ? DATA_PAYLOAD_SIZE : rest;
		frame.len = len;

		ret = read(bin_fd, frame.data, len);
		if (ret < 0) {
			cbd_log("read() failed(%d)\n", ret);
			goto exit;
		}

		ret = s_write(boot_fd, &frame, sizeof(struct data_frame));
		if (ret < 0) {
			cbd_log("s_write() failed(%d)\n", ret);
			goto exit;
		}

		frame.curr_frame++;
		rest -= len;
	} while (rest > 0);

	res = CMD_DIR_CP2AP | step << 8 | CMD_SEND_DATA;
	ret = read_res(boot_fd, step, res);
	if (ret < 0)
		goto exit;
exit:
	return ret;
}

static int verify_crc(int boot_fd, enum bootstrap_step step, u32 crc)
{
	int ret;
	struct crc_frame frame;

	frame.cmd = CMD_DIR_AP2CP | step << 8 | CMD_SEND_CRC;
	frame.crc = crc;

	ret = s_write(boot_fd, &frame, sizeof(struct crc_frame));
	if (ret < 0)
		goto exit;

	ret = read_res(boot_fd, step, CMD_SEND_CRC);
	if (ret < 0)
		cbd_log("invalid crc\n");
exit:
	return ret;
}

static int xmit_main_fw(struct shannon_args *args)
{
	int i;
	int ret;

	int bin_fd = args->bin_fd;
	int boot_fd = args->boot_fd;

	struct image_map *toc;

	for (i = 0; i < STEP_MAX_IDX; i++) {
		switch (i) {
		case STEP_LOADER:
			cbd_log("STEP LOADER\n");
			ret = transact_cmd(boot_fd, i, CMD_STEP_COMPLETE,
					CMD_STEP_COMPLETE);
			if (ret < 0)
				goto exit;
			break;
		case STEP_TOC:
			cbd_log("STEP TOC\n");
			toc = &args->toc[IMAGE_TYPE_TOC];
			ret = lseek(bin_fd, toc->bin_offset, SEEK_SET);
			if (ret < 0)
				goto exit;
			goto xmit;
		case STEP_MAIN:
			cbd_log("STEP MAIN\n");
			toc = &args->toc[IMAGE_TYPE_MAIN];
			ret = lseek(bin_fd, toc->bin_offset, SEEK_SET);
			if (ret < 0)
				goto exit;
			goto xmit;
		case STEP_NV:
			cbd_log("STEP NV\n");
			toc = &args->toc[IMAGE_TYPE_NV];
			bin_fd = args->nv_fd;
xmit:
			ret = transact_cmd(boot_fd, i, CMD_STEP_START,
					CMD_STEP_START);
			if (ret < 0)
				goto exit;

			ret = send_bin(boot_fd, bin_fd, i, toc->size);
			if (ret < 0)
				goto exit;

			if (i == STEP_MAIN) {
				ret = verify_crc(boot_fd, i, toc->crc);
				if (ret < 0)
					goto exit;
			}

			ret = transact_cmd(boot_fd, i, CMD_STEP_COMPLETE,
					CMD_STEP_COMPLETE);
			if (ret < 0)
				goto exit;
			break;
		case STEP_FIN:
			cbd_log("STEP FIN\n");
			ret = transact_cmd(boot_fd, 0xF, CMD_BOOT_COMPLETE,
					CMD_BOOT_COMPLETE);
			if (ret < 0)
				goto exit;
			break;
		case STEP_BOOT:
		default:
			break;
		}
	}

exit:
	return ret;
}

static int enumerate_link_main_dev(struct shannon_args *args)
{
	int ret;

	cbd_log("\n");

//	hsic_usb_host_control(args, 0);
	set_file_value(GPIO_AP2CP_STATUS, "0");

	ret = wait_file_value(GPIO_HOST_WAKEUP, "1", 60);
	if (ret < 0)
		return ret;

//	hsic_usb_host_control(args, 1);
	set_file_value(GPIO_AP2CP_STATUS, "1");

	ret = wait_file_value(GPIO_HOST_WAKEUP, "0", 10);
	if (ret < 0)
		return ret;

	usleep(1500000);
	link_device_reset();

	return 0;
}

static int bootstrap(struct shannon_args *args)
{
	int ret;
	int ehci_on = 1;

	int fd = args->boot_fd;
	enum modem_link link = LINKDEV_SPI;

	args->state = STATE_OFFLINE;

	/* modem off */
	ret = ioctl(fd, IOCTL_MODEM_OFF, NULL);
	if (ret < 0)
		goto exit;

	if (args->cbd_args->lnk_main == LINKDEV_HSIC) {
		hsic_usb_host_control(args, 0);
		ehci_on = 0;
	}

	/* set tx_link to spi */
	ret = ioctl(fd, IOCTL_MODEM_SET_TX_LINK, &link);
	if (ret < 0)
		goto exit;

	/* boot_on ioctl */
	ret = ioctl(fd, IOCTL_MODEM_BOOT_ON, NULL);
	if (ret < 0)
		goto exit;

	/* modem on */
	ret = ioctl(fd, IOCTL_MODEM_ON, NULL);
	if (ret < 0)
		goto exit;

	/* send boot/loader via spi */
	ret = xmit_boot_fw(args);
	if (ret < 0)
		goto exit;

	args->state = STATE_LOADER_DONE;

	if (args->cbd_args->lnk_main == LINKDEV_HSIC) {
		hsic_usb_host_control(args, 1);
		ehci_on = 1;
	}

	/* set tx_link to hsic */
	link = LINKDEV_HSIC;
	ret = ioctl(fd, IOCTL_MODEM_SET_TX_LINK, &link);
	if (ret < 0)
		goto exit;

	ret = check_dev_connect(LOADER_CONNECT);
	if (ret < 0) {
		cbd_log("check_dev_connect fail(%d)\n", ret);
		cbd_log("select %s\n", "TIMOEOUT");
		goto exit;
	}

	/* send toc/main/nv/fin via hsic */
	ret = xmit_main_fw(args);
	if (ret < 0)
		goto exit;

	if (args->cbd_args->lnk_main == LINKDEV_HSIC) {
		ret = enumerate_link_main_dev(args);
		if (ret < 0)
			goto exit;
	}

	/* boot_done ioctl */
	ret = ioctl(fd, IOCTL_MODEM_BOOT_DONE, NULL);
	if (ret < 0)
		goto exit;

	args->state = STATE_ONLINE;

exit:
	if (!ehci_on)
		hsic_usb_host_control(args, 1);

	return ret;
}

static int recv_dump_info(struct shannon_args *args,
		struct dump_info *info, char *cause)
{
	int ret;

	int boot_fd = args->boot_fd;

	char buf[DATA_PAYLOAD_SIZE];
	char *ptr = buf;
	unsigned int res;
	struct dump_info tmp;

	ret = send_cmd(boot_fd, STEP_DUMP, CMD_STEP_START);
	if (ret < 0) {
		cbd_log("send_cmd fail(%d)\n", ret);
		return ret;
	}

	msleep(100);

	ret = s_read(boot_fd, buf, DATA_PAYLOAD_SIZE);
	if (ret < 0) {
		cbd_log("s_read fail(%d)\n", ret);
		return ret;
	}

	res = *(u32 *)ptr;
	if (res != (CMD_DIR_CP2AP | STEP_DUMP << 8 | CMD_STEP_START)) {
		cbd_log("invalid res(0x%04x)\n", res);
		return -EINVAL;
	}
	ptr += sizeof(u32);

	tmp = *(struct dump_info *)ptr;
	ptr += sizeof(struct dump_info);
	memcpy(info, &tmp, sizeof(struct dump_info));

	cbd_log("size = %d, total step = %d, reason len =%d\n",
		info->size, info->step_num, info->cause_len);

	memcpy(cause, ptr, info->cause_len);
	ret = s_write(args->info_fd, cause, info->cause_len);
	if (ret < 0) {
		cbd_log("s_write fail(%d)\n", ret);
		return ret;
	}

	ret = fsync(args->info_fd);
	if (ret < 0) {
		cbd_log("fsync fail(%d)\n", ret);
		return ret;
	}

	cbd_log("cp crashed due to %s\n", cause);
	cbd_log("cp crash info successfully stored\n");

	return 0;
}

static int verify_dump_data(struct data_frame *data,
		unsigned int step, unsigned int data_seq)
{
	if (data->cmd != (CMD_DIR_CP2AP | STEP_DUMP << 8 | step))
		return -EINVAL;

	if (data->curr_frame != data_seq)
		return -EINVAL;

	return 0;
}

static int recv_dump_data(struct shannon_args *args, struct dump_info info)
{
	int ret;
	unsigned int i;

	int boot_fd = args->boot_fd;
	int dump_fd = args->dump_fd;

	struct data_frame dump;
	unsigned int dump_seq = 0;

	unsigned int size = info.size;
	unsigned int step = info.step_num;

	unsigned int recv_len = 0;
	unsigned int total_recv_len = 0;

	cbd_log("start - size = %d, total step = %d\n", size, step);

	for (i = 1; i <= step; i++) {
		dump_seq = 0;
		recv_len = 0;
		ret = send_cmd(boot_fd, STEP_DUMP, i);
		if (ret < 0) {
			cbd_log("[step%d] send_cmd fail(%d)\n", i, ret);
			goto exit;
		}

		msleep(100);

		do {
			dump_seq++;

			ret = s_read(boot_fd, &dump, sizeof(struct data_frame));
			if (ret < 0) {
				cbd_log("[step%d] s_read fail(%d)\n", i, ret);
				goto exit;
			}

			ret = verify_dump_data(&dump, i, dump_seq);
			if (ret < 0) {
				cbd_log("[step%d] verify fail(%d) - %d/%d\n",
					i, ret, dump_seq, dump.num_frame);
				goto exit;
			}

			if (dump.curr_frame == 1)
				cbd_log("[step%d] total frame: %d, cmd: %04x\n",
					i, dump.num_frame, dump.cmd);

			ret = s_write(dump_fd, dump.data, dump.len);
			if (ret < 0) {
				cbd_log("[step%d] seq#%d write fail(%d)\n",
					i, dump.curr_frame, ret);
				goto exit;
			}
			recv_len += dump.len;
		} while (dump_seq < dump.num_frame);

		cbd_log("[step%d] received length: %d, total seq: %d\n",
			i, recv_len, dump.num_frame);

		total_recv_len += recv_len;

		sleep(1);
	}

	if (size != total_recv_len) {
		cbd_log("invalid dump size(%d/%d)\n",
			total_recv_len, size);
		ret = -EINVAL;
		goto exit;
	}

	ret = fsync(dump_fd);
	if (ret < 0) {
		cbd_log("fsync fail(%d)", ret);
		goto exit;
	}

	cbd_log("dump saved\n");

	ret = send_cmd(boot_fd, STEP_DUMP, CMD_STEP_COMPLETE);
	if (ret < 0) {
		cbd_log("send_cmd(CMD_STEP_COMPLETE) fail\n");
		goto exit;
	}

	cbd_log("dump success\n");

	return 0;

exit:
	if (ret < 0) {
		cbd_log("dump fail(%d) - step: %d, seq: %d, recv len: %d\n",
			ret, i, dump_seq, recv_len);
	}
	sleep(1);

	return ret;
}

static int dumpstrap(struct shannon_args *args)
{
	int ret;
	int fd = args->boot_fd;
	enum modem_link link = LINKDEV_SPI;

	struct dump_info info = {
		.size = 0,
		.step_num = 0,
		.cause_len = 0,
	};
	char cause[DUMP_CAUSE_MAX_LEN] = {0, };

	if (args->cbd_args->lnk_main == LINKDEV_HSIC)
		hsic_usb_host_control(args, 0);

	/* set tx_link to spi */
	ret = ioctl(fd, IOCTL_MODEM_SET_TX_LINK, &link);
	if (ret < 0) {
		cbd_log("IOCTL_MODEM_SET_TX_LINK(%d) fail(%d)\n",
			link, ret);
		goto exit;
	}

	ret = ioctl(fd, IOCTL_MODEM_DUMP_RESET, NULL);
	if (ret < 0) {
		cbd_log("IOCTL_MODEM_DUMP_RESET fail(%d)\n", ret);
		goto exit;
	}

	/* send boot/loader via spi */
	ret = xmit_boot_fw(args);
	if (ret < 0) {
		cbd_log("xmit_boot_fw fail(%d)\n", ret);
		goto exit;
	}

	/* set tx_link to hsic */
	link = LINKDEV_HSIC;
	ret = ioctl(fd, IOCTL_MODEM_SET_TX_LINK, &link);
	if (ret < 0) {
		cbd_log("IOCTL_MODEM_SET_TX_LINK(%d) fail(%d)\n",
			link, ret);
		goto exit;
	}

	if (args->cbd_args->lnk_main == LINKDEV_HSIC) {
		hsic_usb_host_control(args, 1);
		msleep(50);
	}

	ret = check_dev_connect(LOADER_CONNECT);
	if (ret < 0) {
		cbd_log("check_dev_connect fail(%d)\n", ret);
		goto exit;
	}

	ret = recv_dump_info(args, &info, cause);
	if (ret < 0) {
		cbd_log("recv_dump_info fail(%d)\n", ret);
		goto exit;
	}

	/* Dump data is consist of 6 parts */
	/* RAM, ITCM, DTCM, DSP_DTCM, DSP_SHIM, DBG_INFO */
	ret = recv_dump_data(args, info);
	if (ret < 0) {
		cbd_log("recv_dump_data fail(%d)\n", ret);
		goto exit;
	}

	sleep(30);

	ret = ioctl(fd, IOCTL_MODEM_CP_UPLOAD, cause);
	if (ret < 0) {
		cbd_log("IOCTL_MODEM_CP_UPLOAD fail(%d)\n", ret);
		goto exit;
	}

	cbd_log("dump done\n");

	return 0;
exit:
	return ret;
}

static int check_nv_file(struct shannon_args *args)
{
	int fd;
	int ret = 0;

	char prop[PROPERTY_VALUE_MAX] = {0, };
	int spin = 5;

	char *path_nv = args->cbd_args->cpn->path_nv;
	int size = args->toc[IMAGE_TYPE_NV].size;

	while (spin--) {
		property_get(VPROP_RFS_CHECKDONE, prop, "0");
		if (prop[0] == '1')
			break;
		msleep(500);
	}
	cbd_log("rild check nv %s\n", spin ? "done" : "timeout");

	fd = open(path_nv, O_RDONLY);
	if (fd < 0) {
		ret = create_empty_nv(path_nv, size);
		if (ret < 0) {
			cbd_log("create_empty_nv() failed(%d)\n", ret);

			return ret;
		}

		fd = open(path_nv, O_RDONLY);
		if (fd < 0) {
			cbd_log("open() failed(%d)\n", fd);

			return ret;
		}
	}

	args->nv_fd = fd;

	cbd_log("success\n");

	return ret;
}

static int make_image_map(struct shannon_args *args)
{
	int i;
	int ret = 0;
	int size = sizeof(struct image_map) * IMAGE_TYPE_MAX_IDX;

	ret = read(args->bin_fd, args->toc, size);
	if (ret != size) {
		cbd_log("toc read failed(%d)\n", ret);
		return -EINVAL;
	}

	for (i = 0; i < IMAGE_TYPE_MAX_IDX; i++) {
		if (strncmp(args->toc[i].name, bin_name[i],
				(size_t)strlen(bin_name[i]))) {
			cbd_log("invalid cp image\n");
			return -EINVAL;
		} else
			cbd_log("toc[%d].name = %s(%d)\n", i,
				args->toc[i].name,
				args->toc[i].size);
	}

	if (args->state != STATE_CRASH_EXIT) {
		ret = check_nv_file(args);
		if (ret < 0) {
			cbd_log("check_nv_file() failed(%d)\n", ret);
			return -EINVAL;
		}
	}

	return 0;
}

static int check_args(const struct boot_args *args)
{
	if (args->lnk_boot != LINKDEV_SPI) {
		cbd_log("boot opt error - %d is not supported\n",
			args->lnk_boot);
		return -ENODEV;
	}

	switch (args->lnk_main) {
	case LINKDEV_HSIC:
	case LINKDEV_C2C:
		break;
	default:
		cbd_log("main opt error - %d is not supported\n",
			args->lnk_boot);
		return -ENODEV;
	}

	return 0;
}

static void boot_wake_lock(int lock)
{
	char *path = lock ? "/sys/power/wake_lock" : "/sys/power/wake_unlock";
	char *name = "shannon";
	int fd, ret;

	fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0x664);
	if (fd < 0) {
		cbd_log("user wake_%s open fail(%d)\n",
			lock ? "lock" : "unlock", fd);
		return;
	}
	ret = write(fd, name, strlen(name));
	if (ret < 0) {
		cbd_log("write fail - %s (%d)\n", name, ret);
		goto exit;

	}
	printf("%s/%s\n", path, name);
exit:
	close(fd);
	return;
}

static void close_boot_fds(struct shannon_args *boot)
{
	if (boot->boot_fd)
		close(boot->boot_fd);

	if (boot->bin_fd)
		close(boot->bin_fd);

	if (boot->nv_fd)
		close(boot->nv_fd);
}

static void close_dump_fds(struct shannon_args *boot)
{
	if (boot->info_fd)
		close(boot->info_fd);

	if (boot->dump_fd)
		close(boot->dump_fd);
}

static int open_boot_fds(struct boot_args *args, struct shannon_args *boot)
{
	int ret;

	ret = open(args->cpn->node_boot, O_RDWR);
	if (ret < 0) {
		cbd_log("%s open fail err=%d\n", args->cpn->node_boot, ret);
		goto exit;
	}
	boot->boot_fd = ret;

	ret = open(args->cpn->path_bin, O_RDONLY);
	if (ret < 0) {
		cbd_log("%s open fail err=%d\n", args->cpn->path_bin, ret);
		goto exit;
	}
	boot->bin_fd = ret;

	return 0;
exit:
	close_boot_fds(boot);

	return ret;
}

static int open_dump_fds(struct boot_args *args, struct shannon_args *boot,
		char *suffix)
{
	int ret;

	char *log_path = get_log_dir();
	char path[MAX_PATH_LEN] = {0, };

	sprintf(path, "%s/info_%s.log", log_path, suffix);
	ret = open(path, O_WRONLY | O_CREAT, 0x644);
	if (ret < 0) {
		cbd_log("%s open failed(%d)\n", path, ret);
		goto exit;
	}
	boot->info_fd = ret;

	sprintf(path, "%s/dump_%s.log", log_path, suffix);
	ret = open(path, O_WRONLY | O_CREAT, 0x644);
	if (ret < 0) {
		cbd_log("%s open failed(%d)\n", path, ret);
		goto exit;
	}
	boot->dump_fd = ret;

	cbd_log("dump fd open success\n");

	return 0;
exit:
	close_dump_fds(boot);

	return ret;
}

static void exec_kernel_dmesg(struct boot_args *args, char *suffix)
{
	save_logs(LOGB_DMESG, "dmesg");

	cbd_log("%s\n", __func__);
}

int start_shannon_hsic_boot(struct boot_args *args)
{
	int ret = 0;
	struct shannon_args boot = {.boot_fd = 0, .bin_fd = 0};

	boot_wake_lock(1);

	cbd_log("start\n");

	ret = check_args(args);
	if (ret < 0)
		goto exit;

	memset(&boot, 0x00, sizeof(struct shannon_args));
	boot.cbd_args = args;

	ret = open_boot_fds(args, &boot);
	if (ret < 0)
		goto exit;

	ret = make_image_map(&boot);
	if (ret < 0)
		goto exit;

	ret = bootstrap(&boot);
	if (ret < 0)
		goto exit;

exit:
	close_boot_fds(&boot);

	boot_wake_lock(0);

	return ret;
}

int start_shannon_hsic_dump(struct boot_args *args)
{
	int ret = 0;

	char suffix[MAX_SUFFIX_LEN] = {0, };
	time_t now;
	struct tm result;

	struct shannon_args boot = {.boot_fd = 0, .bin_fd = 0};

	boot_wake_lock(1);

	cbd_log("start\n");

	ret = check_args(args);
	if (ret < 0)
		goto exit;

	memset(&boot, 0x00, sizeof(struct shannon_args));
	boot.cbd_args = args;
	boot.state = STATE_CRASH_EXIT;

	time(&now);
	localtime_r(&now, &result);
	strftime(suffix, MAX_SUFFIX_LEN, "%Y%m%d_%H%M%S", &result);

	ret = open_boot_fds(args, &boot);
	if (ret < 0)
		goto exit;

	ret = open_dump_fds(args, &boot, suffix);
	if (ret < 0)
		goto exit;

	exec_kernel_dmesg(args, suffix);

	ret = make_image_map(&boot);
	if (ret < 0)
		goto exit;

	ret = dumpstrap(&boot);
	if (ret < 0)
		goto exit;

exit:
	cbd_log("end\n");

	close_boot_fds(&boot);

	boot_wake_lock(0);

	return ret;
}

int shutdown_shannon_hsic(struct boot_args *args)
{
	int ret;
	int fd = -1;

	ret = open(args->cpn->node_boot, O_RDWR);
	if (ret < 0) {
		cbd_log("%s open fail(%d)\n", args->cpn->node_boot, ret);
		goto exit;
	}
	fd = ret;

	ret = ioctl(fd, IOCTL_MODEM_OFF, NULL);
	if (ret < 0) {
		cbd_log("IOCTL_MODEM_OFF fail(%d)\n", ret);
	}

	if (fd)
		close(fd);

exit:
	return ret;
}
