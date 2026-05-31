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
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <termios.h>
#include <stdarg.h>
#include <errno.h>
#include <cutils/properties.h>

#include "boot.h"
#include "cmc22x.h"
#include "util.h"

static struct cp_imgmap default_table[IMG_MAX_IDX] = {
	[IMG_BOOT] = {
		.name = "BOOT",
		.bin_offset = 0x0,
		.size = 0x1000,
	},
	[IMG_MAIN] = {
		.name = "MAIN",
		.bin_offset = 0x5000,
		.size = 0x1200000,
	},
};

static char img_table[MAX_TOC_SIZE];
static struct cp_imgmap *toc = (struct cp_imgmap *)img_table;

/* CP binary map update */
static int update_cp_imgmap(struct cmc_args *args)
{
	int err;
	char *img_tab = img_table; /* table buf */
	struct cp_imgmap *tocmap;

	if (args->bin_fd < 0) {
		cbd_log("invalid cp binary (fd %d)\n", args->bin_fd);
		err = -EINVAL;
		goto exit;
	}

	err = read(args->bin_fd, img_tab, MAX_TOC_SIZE);
	if (err < 0) {
		cbd_log("cp img table read fail (err %d)\n", err);
		goto exit;
	}

	tocmap = (struct cp_imgmap *)img_tab;

	cbd_log("tocmap->name = %s\n", tocmap->name);
	if (!strcmp(tocmap[IMG_BOOT].name, "BOOT")) {
		cbd_log("LTE img map from CP binary\n");
		args->toc_valid = 1;
		if (tocmap[IMG_BOOT].crc && tocmap[IMG_MAIN].crc) {
			args->crc_check = 1;
			cbd_log("LTE img map = TOC + CRC\n");
		} else {
			args->crc_check = 0;
		}
		args->img_tab = (struct cp_imgmap *)img_tab;
	} else {
		cbd_log("LTE img map from default table\n");
		args->toc_valid = 0;
		args->crc_check = 0;
		args->img_tab = default_table;
	}

	print_data(img_tab, 16);

exit:
	return err;
}

static int dpram_send_wait_cmd(struct dpram_boot_frame *bf, int boot_fd,
			unsigned req, unsigned res)
{
	int err;

	cbd_log("Req 0x%X, wait resp 0x%X\n", req, res);

	memset(bf, 0, sizeof(struct dpram_boot_frame));
	bf->req = req;
	bf->res = res;
	bf->len = 0;

	err = write(boot_fd, bf, sizeof(struct dpram_boot_frame));
	if (err < 0) {
		cbd_log("write fail (err %d)\n", err);
	}

	return err;
}

static int dpram_send_binary(struct dpram_boot_frame *bf, struct cmc_args *args,
			const char *buf, int size, unsigned flag)
{
	int cnt = 0, err;
	int rest = size;
	int blk_size = 0;
	int send_size = 0;

	cbd_log("send binary size = %d\n", size);

	bf->len = 0; /*for cmd packet*/

	while (rest) {
		memset(bf, flag, sizeof(struct dpram_boot_frame));
		blk_size = rest > MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : rest;
		if (!(cnt++ % 2)) {
			bf->req = CMC22x_1ST_BUFF_FULL;
			bf->res = CMC22x_2ND_BUFF_READY;
			bf->offset = 0;
		} else {
			bf->req = CMC22x_2ND_BUFF_FULL;
			bf->res = CMC22x_1ST_BUFF_READY;
			bf->offset = 0x2000;
		}

		memcpy(bf->data, buf + send_size, blk_size);
		bf->len = blk_size;
		err = write(args->boot_fd, bf, sizeof(struct dpram_boot_frame));
		if (err < 0) {
			cbd_log("boot frame write fail (err %d)\n", err);
			cbd_log("frame debug req=%x, res=%x, len=%ld\n",
			bf->req, bf->res, bf->len);
			goto exit;
		}

		rest -= blk_size;
		send_size += blk_size;
	}

	cbd_log("rest = %d, send_size = %d\n", rest, send_size);

	return 0;

exit:
	return err;
}

/* Load cp binaries */
static int prepare_image(int fd, struct dpram_boot_img *img, struct cp_imgmap *cp_bin)
{
	int err;

	cbd_log("CP binary offset = %x\n", cp_bin->bin_offset);
	err = lseek(fd, cp_bin->bin_offset, SEEK_SET);
	if (err < 0) {
		cbd_log("lseek fail %d\n", err);
		goto exit;
	}

	err = read(fd, img->addr, img->size);
	if (err < (int)img->size) {
		cbd_log("file read fail (err %d)\n", err);
		err = -EFAULT;
		goto exit;
	}

	err = 0;

	cbd_log("Load bin %s, img size %ld\n", cp_bin->name, img->size);
	print_data((char *)img->addr, 16);

exit:
	return err;
}

static int dpram_download_main(struct cmc_args *args)
{
	int err = 0;
	int nv_fd = -1;
	int boot_fd = args->boot_fd;
	struct modem_comp *cpn = args->cbd_args->cpn;
	struct dpram_boot_img img;
	struct dpram_boot_frame *bf;
	char *nv_data = NULL;
	struct stat file_info;
	char err_str[512];
	int spin = 5;
	char prop_buf[PROPERTY_VALUE_MAX] = {0, };

	/*Intialize dpram_boot_img */
	memset(&img, 0, sizeof(struct dpram_boot_img));
	memset(err_str, 0, sizeof(err_str));

	cbd_log("start sending LTE main binary\n");

	/* Alloc boot frame */
	bf = (struct dpram_boot_frame *)malloc(sizeof(struct dpram_boot_frame));
	if (!bf) {
		cbd_log("Binary buf alloc fail size = %ld\n",
			sizeof(struct dpram_boot_frame));
		err = -ENOMEM;
		goto exit;
	}

	/* load MAIN binary */
	img.size = args->img_tab[IMG_MAIN].size;
	img.addr = (unsigned char *)malloc(img.size);
	if (!img.addr) {
		cbd_log("Fail to malloc for MAIN img\n");
		err = -ENOMEM;
		goto exit;
	}

	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_MAIN]);
	if (err < 0) {
		cbd_log("MAIN load fail (err %d)\n", err);
		goto exit;
	}

	cbd_log("check nv\n");
	/* wait for RILD NV Validity done*/
	while (spin--) {
		property_get(VPROP_RFS_CHECKDONE, prop_buf, "0");
		if (prop_buf[0] == '1')
			break;
		cbd_log("wait restore nv file\n");
		usleep(500000);
	}
	cbd_log("rild check nv %s\n", spin < 0 ? "timeout" : "done");

	/* Open NV data file */
	nv_fd = open(cpn->path_nv, O_RDWR | O_NDELAY);
	if (nv_fd < 0) {
		cbd_log("NV(%s) open fail (err %d)\n", cpn->path_nv, nv_fd);

		err = create_empty_nv(cpn->path_nv, cpn->nv_size);
		if (err < 0) {
			cbd_log("NV(%s, %d bytes) create_empty_nv fail\n",
				cpn->path_nv, cpn->nv_size);
		}

		nv_fd = open(cpn->path_nv, O_RDWR | O_NDELAY);
		if (nv_fd < 0) {
			cbd_log("Default NV fail (err %d)\n", nv_fd);
			goto exit;
		}
	}

	/*
	** Download TOC
	*/
	if (args->toc_valid && args->crc_check) {
		unsigned long crc;

		cbd_log("Send TOC ...\n");

		cbd_log("TOC: name %s, size %d, CRC 0x%08X\n",
			toc[IMG_BOOT].name, toc[IMG_BOOT].size, toc[IMG_BOOT].crc);
		cbd_log("TOC: name %s, size %d, CRC 0x%08X\n",
			toc[IMG_MAIN].name, toc[IMG_MAIN].size, toc[IMG_MAIN].crc);

		crc = update_crc32(0xFFFFFFFFL, img.addr, img.size);
		cbd_log("TOC: IMG_MAIN calculated CRC 0x%08lX\n", crc);
		if (crc != toc[IMG_MAIN].crc) {
			cbd_log("TOC: CRC check fail!!!\n");

			/* Reload MAIN binary */
			memset(img.addr, 0, img.size);
			err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_MAIN]);
			if (err < 0) {
				cbd_log("MAIN load fail (err %d)\n", err);
				goto exit;
			}

			crc = update_crc32(0xFFFFFFFFL, img.addr, img.size);
			cbd_log("TOC: IMG_MAIN calculated CRC 0x%08lX\n", crc);
			if (crc != toc[IMG_MAIN].crc) {
				cbd_log("TOC: CRC check fail twice!!!\n");
				cbd_log("TOC: Go to Upload mode\n");
				sprintf(err_str, "%s", "... BAD CRC in AP");
				err = ioctl(boot_fd, IOCTL_MODEM_CP_UPLOAD, err_str);
				goto exit;
			}
		}

		/* Send CMC22x_HOST_DOWN_START and wait for CMC22x_1ST_BUFF_READY */
		err = dpram_send_wait_cmd(bf, boot_fd, CMC22x_HOST_DOWN_START, CMC22x_1ST_BUFF_READY);
		if (err < 0) {
			cbd_log("Wait 1ST_BUFF_READY fail (err %d)\n", err);
			goto exit;
		}

		/* Prepare TOC image */
		bf->req = CMC22x_1ST_BUFF_FULL;
		bf->res = 0;
		bf->len = MAX_TOC_SIZE;
		bf->offset = 0;
		memcpy(bf->data, img_table, MAX_TOC_SIZE);

		/* Send TOC image */
		err = write(boot_fd, bf, sizeof(struct dpram_boot_frame));
		if (err < 0) {
			cbd_log("Send TOC fail (err %d)\n", err);
			cbd_log("Send TOC fail (req %x, res %x, len %ld)\n",
				bf->req, bf->res, bf->len);
			goto exit;
		}

		/* Send CMC22x_HOST_DOWN_END */
		err = dpram_send_wait_cmd(bf, boot_fd, CMC22x_HOST_DOWN_END, 0);
		if (err < 0) {
			cbd_log("Send HOST_DOWN_END fail (err %d)\n", err);
			goto exit;
		}

		cbd_log("Send TOC done\n");
	}

	/*
	** Download CP main binary
	*/
	cbd_log("Send MAIN ...\n");

	/* Send CMC22x_HOST_DOWN_START and wait for CMC22x_1ST_BUFF_READY */
	err = dpram_send_wait_cmd(bf, boot_fd, CMC22x_HOST_DOWN_START, CMC22x_1ST_BUFF_READY);
	if (err < 0) {
		cbd_log("Wait 1ST_BUFF_READY fail (err %d)\n", err);
		goto exit;
	}

	/* Send CP main binary */
	err = dpram_send_binary(bf, args, (char *)img.addr, img.size, 0);
	if (err < 0) {
		cbd_log("Xmit MAIN fail (err %d)\n", err);
		goto exit;
	}

	/* Send CMC22x_HOST_DOWN_END */
	err = dpram_send_wait_cmd(bf, boot_fd, CMC22x_HOST_DOWN_END, 0);
	if (err < 0) {
		cbd_log("Wait CP_REQ_NV_DATA fail (err %d)\n", err);
		goto exit;
	}

	cbd_log("Send MAIN done\n");

	/*
	** Download REG NV data
	*/
	cbd_log("Send REG NV ...\n");

	/* Wait for CMC22x_CP_REQ_NV_DATA & CMC22x_1ST_BUFF_READY */
	err = dpram_send_wait_cmd(bf, boot_fd, 0, CMC22x_CP_REQ_NV_DATA);
	if (err < 0) {
		cbd_log("send cmd fail (err %d)\n", err);
		goto exit;
	}

	err = dpram_send_wait_cmd(bf, boot_fd, 0, CMC22x_1ST_BUFF_READY);
	if (err < 0) {
		cbd_log("Wait 1ST_BUFF_READY fail (err %d)\n", err);
		goto exit;
	}

	/* Send CMC22x_HOST_DOWN_START */
	err = dpram_send_wait_cmd(bf, boot_fd, CMC22x_HOST_DOWN_START, 0);
	if (err < 0) {
		cbd_log("Send HOST_DOWN_START fail (err %d)\n", err);
		goto exit;
	}

	/* Prepare  NV data */
	err = fstat(nv_fd, &file_info);
	if (err < 0) {
		cbd_log("Can not get file info\n");
		goto exit;
	}

	nv_data = (char *)malloc(MAX_NVDATA_SIZE);
	err = read(nv_fd, nv_data, file_info.st_size);
	if (err < 0) {
		cbd_log("load cp bin fail (err %d)\n", err);
		goto exit;
	}
	print_data(nv_data, 16);

	/* Send REG NV data */
	err = dpram_send_binary(bf, args, nv_data, CNV_UNIT_SIZE, 0xff);
	if (err < 0) {
		cbd_log("cp reg nv xmit  fail (err %d)\n", err);
		goto exit;
	}

	/* Send CMC22x_HOST_DOWN_END & wait for CMC22x_CP_RECV_NV_END */
	err = dpram_send_wait_cmd(bf, boot_fd, CMC22x_HOST_DOWN_END, CMC22x_CP_RECV_NV_END);
	if (err < 0) {
		cbd_log("send cmd fail (err %d)\n", err);
		goto exit;
	}

	/* Send CMC22x_REG_NV_DOWN_END */
	err = dpram_send_wait_cmd(bf, boot_fd, CMC22x_REG_NV_DOWN_END, 0);
	if (err < 0) {
		cbd_log("send cmd fail (err %d)\n", err);
		goto exit;
	}

	cbd_log("Send REG NV done\n");

	/*
	** Download CAL NV data
	*/
	cbd_log("Send CAL NV ...\n");

	/* Wait for CMC22x_CP_REQ_NV_DATA & CMC22x_1ST_BUFF_READY */
	err = dpram_send_wait_cmd(bf, boot_fd, 0, CMC22x_CP_REQ_NV_DATA);
	if (err < 0) {
		cbd_log("send cmd fail (err %d)\n", err);
		goto exit;
	}

	err = dpram_send_wait_cmd(bf, boot_fd, 0, CMC22x_1ST_BUFF_READY);
	if (err < 0) {
		cbd_log("send cmd fail (err %d)\n", err);
		goto exit;
	}

	/* Send CMC22x_HOST_DOWN_START */
	err = dpram_send_wait_cmd(bf, boot_fd, CMC22x_HOST_DOWN_START, 0);
	if (err < 0) {
		cbd_log("send cmd fail (err %d)\n", err);
		goto exit;
	}

	/* Send CAL NV data */
	err = dpram_send_binary(bf, args, nv_data + CNV_UNIT_SIZE, CNV_UNIT_SIZE, 0xff);
	if (err < 0) {
		cbd_log("cp cal nv xmit  fail (err %d)\n", err);
		goto exit;
	}

	/* Send CMC22x_HOST_DOWN_END & wait for CMC22x_CP_RECV_NV_END */
	err = dpram_send_wait_cmd(bf, boot_fd, CMC22x_HOST_DOWN_END, CMC22x_CP_RECV_NV_END);
	if (err < 0) {
		cbd_log("send cmd fail (err %d)\n", err);
		goto exit;
	}

	/* Send CMC22x_CAL_NV_DOWN_END */
	err = dpram_send_wait_cmd(bf, boot_fd, CMC22x_CAL_NV_DOWN_END, 0);
	if (err < 0) {
		cbd_log("send cmd fail (err %d)\n", err);
		goto exit;
	}

	cbd_log("Send CAL NV done\n");

	err = 0;

exit:
	if (bf)
		free(bf);

	if (img.addr)
		free(img.addr);

	if (nv_data)
		free(nv_data);

	if (nv_fd >= 0)
		close(nv_fd);

	return err;
}

static int dpram_download_boot(struct cmc_args *args, enum cp_boot_mode boot_mode)
{
	int err = 0;
	struct dpram_boot_img img;

	cbd_log("Send BOOT\n");

	memset(&img, 0x00, sizeof(struct dpram_boot_img));

	/* load BOOT binary */
	img.size = args->img_tab[IMG_BOOT].size;
	img.addr = (unsigned char *)malloc(img.size);
	if (!img.addr) {
		cbd_log("Fail to malloc for BOOT img\n");
		goto exit;
	}
	if (!img.size) {
		cbd_log("Fail to malloc(size: %ld)\n", img.size);
		goto exit;
	}

	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_BOOT]);
	if (err < 0) {
		cbd_log("BOOT load fail (err %d)\n", err);
		goto exit;
	}
	img.mode = boot_mode;

	if (args->toc_valid && args->crc_check) {
		cbd_log("TOC valid && CRC check\n");
		img.resp = CMC22x_CP_REQ_TOC;
	} else {
		img.resp = CMC22x_CP_REQ_MAIN_BIN;
	}
	img.req = CMC22x_AP_BOOT_DOWN_DONE;

	cbd_log("LTE boot_size = 0x%lx\n", img.size);
	print_data((char *)img.addr, 32);

	usleep(10000);

	/* Send DPRAM boot */
	cbd_log("write LTE boot binary to dpram\n");
	err = ioctl(args->boot_fd, IOCTL_MODEM_XMIT_BOOT, &img);
	if (err < 0) {
		cbd_log("send dpram ioctl fail (err %d)\n", err);
		goto exit;
	}

	err = 0;
	cbd_log("Send BOOT done\n");

exit:
	if (img.addr)
		free(img.addr);

	return err;
}

int start_cmc221_boot(struct boot_args *args)
{
#ifdef CONFIG_USBHUB_USB3503
	int connected = 0, spin = 0, status = 0, usberr = 0;
#endif
	int err = 0;
	int ipc_fd = 0;
	struct cmc_args cmc_boot;

	cbd_log("start\n");

	memset(&cmc_boot, 0, sizeof(struct cmc_args));
	cmc_boot.cbd_args = args;

	cmc_boot.boot_fd = open(args->cpn->node_boot, O_RDWR);
	if (cmc_boot.boot_fd < 0) {
		cbd_log("%s open fail (fd %d)\n", args->cpn->node_boot,
			cmc_boot.boot_fd);
		err = cmc_boot.boot_fd;
		goto exit;
	}

	cmc_boot.bin_fd = open(args->cpn->path_bin, O_RDWR);
	if(cmc_boot.bin_fd < 0) {
		cbd_log("%s open fail (fd %d)\n", args->cpn->path_bin,
			cmc_boot.bin_fd);
		err = cmc_boot.bin_fd;
		goto exit;
	}

	err = update_cp_imgmap(&cmc_boot);
	if (err < 0) {
		cbd_log("cp cmc map update fail (err %d)\n", err);
		goto exit;
	}

#ifdef CONFIG_USBHUB_USB3503
	cmc_boot.link_fd = open("/dev/link_pm", O_RDWR);
	if (cmc_boot.link_fd < 0) {
		cbd_log("%s open fail (err %d)\n", "/dev/link_pm",
			cmc_boot.link_fd);
		err = cmc_boot.link_fd;
		goto exit;
	}

	cbd_log("USB3503 off\n");
	err = ioctl(cmc_boot.link_fd, IOCTL_LINK_PORT_OFF, NULL);
	if (err < 0) {
		cbd_log("LINK_PORT_OFF ioctl fail (err %d)\n", err);
		goto exit;
	}
#endif

	/* CP power on andd release reset_n */
	cbd_log("LTE Power on\n");
	err = ioctl(cmc_boot.boot_fd, IOCTL_MODEM_ON, NULL);
	if (err < 0) {
		cbd_log("MODEM_ON ioctl fail (err %d)\n", err);
		goto exit;
	}

	err = ioctl(cmc_boot.boot_fd, IOCTL_MODEM_BOOT_ON, NULL);
	if (err < 0) {
		cbd_log("MODEM_BOOT_ON ioctl fail (err %d)\n", err);
		goto exit;
	}

	err = dpram_download_boot(&cmc_boot, CP_BOOT_MODE_NORMAL);
	if (err < 0) {
		cbd_log("dpram_download_boot fail (err %d)\n", err);
		goto exit;
	}

	err = dpram_download_main(&cmc_boot);
	if (err < 0) {
		cbd_log("dpram_download_main fail (err %d)\n", err);
		goto exit;
	}

	err = ioctl(cmc_boot.boot_fd, IOCTL_MODEM_BOOT_OFF, NULL);
	if (err < 0) {
		cbd_log("MODEM_BOOT_OFF ioctl fail (err %d)\n", err);
		goto exit;
	}

#ifdef CONFIG_USBHUB_USB3503
	/* wait DPRAM initailized - C8 */
	spin = 50;
	while (spin--) {
		status = ioctl(cmc_boot.boot_fd, IOCTL_MODEM_STATUS, NULL);
		cbd_log("wait dpram initialize = [%d]\n", status);
		if (status == STATE_ONLINE)
			break;
		sleep(1);
	}

	/* USB port on */
	spin = 10;
	usberr = -1;
	while (spin-- && usberr < 0) {
		cbd_log("USB3503 on\n");
		usberr = ioctl(cmc_boot.link_fd, IOCTL_LINK_PORT_ON, NULL);
		if (usberr < 0)
			cbd_log("LINK_PORT_ON ioctl fail (err %d)\n", usberr);
	}
	if (usberr < 0) /* check last error */
		goto exit;

	/* Check the HSIC connection */
	spin = 30;
	while (spin--) {
		connected = ioctl(cmc_boot.link_fd, IOCTL_LINK_CONNECTED, NULL);
		cbd_log("link connected = [%d]\n", connected);
		if (connected == 1)
			break;
		usleep(500000);
	}

	/* Start IPC */
	ipc_fd = open("/dev/lte_ipc0", O_RDWR);
	if (ipc_fd < 0) {
		cbd_log("%s open fail (err %d)\n", "/dev/lte_ipc0", ipc_fd);
		usberr = ipc_fd;
		goto exit;
	}
#endif

exit:
	err = ioctl(cmc_boot.boot_fd, IOCTL_MODEM_BOOT_DONE, NULL);
	if (err < 0)
		cbd_log("IOCTL_MODEM_BOOT_DONE ioctl fail (err %d)\n", err);

#ifdef CONFIG_USBHUB_USB3503
	/* when usb enumration failed, use dpram only.. turn off usb3503. */
	if (usberr < 0) {
		cbd_log("USB3503 off\n");
		usberr = ioctl(cmc_boot.link_fd, IOCTL_LINK_PORT_OFF, NULL);
		if (usberr < 0)
			cbd_log("LINK_PORT_OFF ioctl fail (err %d)\n", usberr);
	}
#endif

	if (ipc_fd >= 0)
		close(ipc_fd);

#ifdef CONFIG_USBHUB_USB3503
	if (cmc_boot.link_fd >= 0)
		close(cmc_boot.link_fd);
#endif

	if (cmc_boot.boot_fd >= 0)
		close(cmc_boot.boot_fd);

	if (cmc_boot.bin_fd >= 0)
		close(cmc_boot.bin_fd);

	return err;
}

int start_cmc221_dump(struct boot_args *args)
{
	int err = -EFAULT;
	struct modem_comp *modem = args->cpn;
	int log_fd = 0;
	int dump_fd = 0;
	int info_fd = 0;
	int rcvd_size = 0;
	time_t now = 0;
	struct tm result;
	struct cmc_args cmc_boot;
	struct dpram_dump_arg dump_arg;
	char prefix[16];
	char suffix[32];
	char log_file_str[256];
	char cpinfo_buf[512] = "LTE: ";

	memset(&cmc_boot, 0, sizeof(struct cmc_args));
	memset(&dump_arg, 0, sizeof(struct dpram_dump_arg));

	cbd_log("start\n");

	cmc_boot.cbd_args = args;

	/* Open the boot device */
	cmc_boot.boot_fd = open(args->cpn->node_boot, O_RDWR);
	if (cmc_boot.boot_fd < 0) {
		err = cmc_boot.boot_fd;
		cbd_log("%s open fail (err %d)\n", args->cpn->node_boot, err);
		goto exit;
	}
	cbd_log("open %s (fd = %d)\n", args->cpn->node_boot, cmc_boot.boot_fd);

	/* Open the CP binary */
	cmc_boot.bin_fd = open(args->cpn->path_bin, O_RDONLY);
	if (cmc_boot.bin_fd < 0) {
		err = cmc_boot.bin_fd;
		cbd_log("%s open fail (err %d)\n", args->cpn->path_bin, err);
		goto exit;
	}
	cbd_log("open %s (fd = %d)\n", args->cpn->path_bin, cmc_boot.bin_fd);

	err = update_cp_imgmap(&cmc_boot);
	if (err < 0) {
		cbd_log("update_cp_imgmap fail (err %d)\n", err);
		goto exit;
	}

	snprintf(prefix, sizeof(prefix), "%s_crash", modem->rat);

	time(&now);
	localtime_r(&now, &result);
	strftime(suffix, 20, "%Y%m%d_%H%M%S", &result);

	/* Create a CP crash log file */
	sprintf(log_file_str, "%s/%s_log_%s.log", get_log_dir(), prefix, suffix);
	log_fd = open(log_file_str, O_WRONLY | O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (log_fd < 0) {
		err = log_fd;
		cbd_log("%s open fail (err %d)\n", log_file_str, err);
		goto exit;
	}
	cbd_log("open %s (fd = %d)\n", log_file_str, log_fd);

	/* Open (create) a CP crash info file */
	sprintf(log_file_str, "%s/%s_info_%s_%s.log", get_log_dir(), prefix,
			args->cpn->name, suffix);
	info_fd = open(log_file_str, O_WRONLY | O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (info_fd < 0) {
		err = info_fd;
		cbd_log("%s open fail (err %d)\n", log_file_str, err);
		dprintf(log_fd, "%s: %s open fail (err %d)\n", __func__, log_file_str, err);
		goto exit;
	}
	cbd_log("open %s (fd = %d)\n", log_file_str, info_fd);
	dprintf(log_fd, "%s: open %s (fd = %d)\n", __func__, log_file_str, info_fd);

	/* Open (create) a CP crash dump file */
	sprintf(log_file_str, "%s/%s_dump_%s_%s.log", get_log_dir(), prefix,
			args->cpn->name, suffix);
	dump_fd = open(log_file_str, O_WRONLY | O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (dump_fd < 0) {
		err = dump_fd;
		cbd_log("%s open fail (err %d)\n", log_file_str, err);
		dprintf(log_fd, "%s: %s open fail (err %d)\n", __func__, log_file_str, err);
		goto exit;
	}
	cbd_log("open %s (fd = %d)\n", log_file_str, dump_fd);
	dprintf(log_fd, "%s: open %s (fd = %d)\n", __func__, log_file_str, dump_fd);

	/* Prepare CP dump argument */
	dump_arg.buff = malloc(CMC22x_DUMP_BUFF_SIZE);
	if (!dump_arg.buff) {
		err = -ENOMEM;
		cbd_log("dump buffer malloc fail\n");
		dprintf(log_fd, "%s: dump buffer malloc fail\n", __func__);
		goto exit;
	}
	dump_arg.buff_size = CMC22x_DUMP_BUFF_SIZE;

	/* Reset and boot up CP as dump mode */
	err = ioctl(cmc_boot.boot_fd, IOCTL_MODEM_DUMP_RESET, NULL);
	if (err < 0) {
		cbd_log("IOCTL_MODEM_DUMP_RESET fail (err %d)\n", err);
		dprintf(log_fd, "%s: IOCTL_MODEM_DUMP_RESET fail (err %d)\n", __func__, err);
		goto exit;
	}

	err = dpram_download_boot(&cmc_boot, CP_BOOT_MODE_DUMP);
	if (err < 0) {
		cbd_log("dpram_download_boot fail (err %d)\n", err);
		dprintf(log_fd, "%s: dpram_download_boot fail (err %d)\n", __func__, err);
		goto exit;
	}

	/* Send upload key to CP */
	err = ioctl(cmc_boot.boot_fd, IOCTL_MODEM_DUMP_START, NULL);
	if (err < 0) {
		cbd_log("IOCTL_MODEM_DUMP_START fail (err %d)\n", err);
		dprintf(log_fd, "%s: IOCTL_MODEM_DUMP_START fail (err %d)\n", __func__, err);
		goto exit;
	}

	/* Receive Cp Crash info from CP */
	memset(dump_arg.buff, 0, CMC22x_DUMP_BUFF_SIZE);

	err = ioctl(cmc_boot.boot_fd, IOCTL_MODEM_DUMP_UPDATE, &dump_arg);
	if (err < 0) {
		cbd_log("IOCTL_MODEM_DUMP_UPDATE fail (err %d)\n", err);
		dprintf(log_fd, "%s: IOCTL_MODEM_DUMP_UPDATE fail (err %d)\n", __func__, err);
		goto exit;
	}

	strncpy(cpinfo_buf + strlen(cpinfo_buf), dump_arg.buff, 511);

	err = write(info_fd, cpinfo_buf, 512);
	if (err < 0) {
		cbd_log("INFO write fail (err %d)\n", err);
		dprintf(log_fd, "%s: INFO write fail (err %d)\n", __func__, err);
		goto exit;
	}

	/* Receive dump data from CP */
	while (1) {
		memset(dump_arg.buff, 0, CMC22x_DUMP_BUFF_SIZE);

		err = ioctl(cmc_boot.boot_fd, IOCTL_MODEM_DUMP_UPDATE, &dump_arg);
		if (err < 0) {
			cbd_log("IOCTL_MODEM_DUMP_UPDATE fail (err %d)\n", err);
			dprintf(log_fd, "%s: IOCTL_MODEM_DUMP_UPDATE fail (err %d)\n", __func__, err);
			break;
		}

		if (err == 0) {
			cbd_log("DUMP complete!!! (size %d) \n", rcvd_size);
			dprintf(log_fd, "%s: DUMP complete!!! (size %d)\n", __func__, rcvd_size);
			break;
		}

		rcvd_size += err;
		dprintf(log_fd, "%s: %d bytes received\n", __func__, rcvd_size);

		err = write(dump_fd, dump_arg.buff, CMC22x_DUMP_BUFF_SIZE);
		if (err < 0) {
			cbd_log("DUMP write fail (err %d)\n", err);
			dprintf(log_fd, "%s: DUMP write fail (err %d)\n", __func__, err);
			break;
		}
	}

	fsync(log_fd);
	fsync(info_fd);
	fsync(dump_fd);

	sleep(30);

	cbd_log("Go to Upload mode\n");
	err = ioctl(cmc_boot.boot_fd, IOCTL_MODEM_CP_UPLOAD, cpinfo_buf);
	if (err < 0) {
		cbd_log("IOCTL_MODEM_CP_UPLOAD fail (err %d)\n", err);
		goto exit;
	}

exit:
	if (dump_arg.buff)
		free(dump_arg.buff);

	if (cmc_boot.bin_fd >= 0)
		close(cmc_boot.bin_fd);

	if (cmc_boot.boot_fd >= 0)
		close(cmc_boot.boot_fd);

	if (log_fd >= 0)
		close(log_fd);

	if (info_fd >= 0)
		close(info_fd);

	if (dump_fd >= 0)
		close(dump_fd);

	return err;
}

int start_cmc221_shutdown(struct boot_args *args)
{
	int fd;
	int err = 0;

	fd = open(args->cpn->node_boot, O_RDWR);
	if (fd < 0) {
		cbd_log("%s open fail (err %d)\n", args->cpn->node_boot, fd);
		err = fd;
		goto exit;
	}

	cbd_log("%s->ioctl(IOCTL_MODEM_OFF)\n", args->cpn->node_boot);
	err = ioctl(fd, IOCTL_MODEM_OFF);
	if (err) {
		cbd_log("IOCTL_MODEM_OFF fail (err %d)\n", err);
		goto exit;
	}

exit:
	if (fd >= 0)
		close(fd);
	return err;
}
