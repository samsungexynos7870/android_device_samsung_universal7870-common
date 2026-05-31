#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/select.h>

#include <cutils/properties.h>

#include "boot.h"
#include "std_boot.h"
#include "util.h"
#include "shannon.h"
#include "boot_shannon.h"

static struct shannon_boot_args shannon_boot_arguments;

static void build_std_dload_control(struct std_boot_args *std_args,
				    struct shannon_boot_args *args)
{
	struct std_dload_control *dl_ctrl = std_args->dl_ctrl;
	struct std_toc_element *toc = std_args->toc;
	int bin_fd = args->bin_fd;
	int nv_fd = args->nv_fd;

	/* BOOT (loader) */
	dl_ctrl[BOOT_STAGE].stage = BOOT_STAGE;
	dl_ctrl[BOOT_STAGE].start = 0;
	dl_ctrl[BOOT_STAGE].download = 0;
	dl_ctrl[BOOT_STAGE].validate = 1;
	dl_ctrl[BOOT_STAGE].finish = 1;
	dl_ctrl[BOOT_STAGE].b_fd = bin_fd;
	dl_ctrl[BOOT_STAGE].b_offset = toc[IMG_BOOT].b_offset;
	dl_ctrl[BOOT_STAGE].b_size = toc[IMG_BOOT].size;
	dl_ctrl[BOOT_STAGE].crc = 0;

	/* TOC */
	dl_ctrl[TOC_STAGE].stage = TOC_STAGE;
	dl_ctrl[TOC_STAGE].start = 1;
	dl_ctrl[TOC_STAGE].download = 1;
	dl_ctrl[TOC_STAGE].validate = 0;
	dl_ctrl[TOC_STAGE].finish = 1;
	dl_ctrl[TOC_STAGE].b_fd = bin_fd;
	dl_ctrl[TOC_STAGE].b_offset = toc[IMG_TOC].b_offset;
	dl_ctrl[TOC_STAGE].b_size = toc[IMG_TOC].size;
	dl_ctrl[TOC_STAGE].crc = 0;

	/* MAIN */
	dl_ctrl[MAIN_STAGE].stage = MAIN_STAGE;
	dl_ctrl[MAIN_STAGE].start = 1;
	dl_ctrl[MAIN_STAGE].download = 1;
	dl_ctrl[MAIN_STAGE].validate = 1;
	dl_ctrl[MAIN_STAGE].finish = 1;
	dl_ctrl[MAIN_STAGE].b_fd = bin_fd;
	dl_ctrl[MAIN_STAGE].b_offset = toc[IMG_MAIN].b_offset;
	dl_ctrl[MAIN_STAGE].b_size = toc[IMG_MAIN].size;
	dl_ctrl[MAIN_STAGE].crc = toc[IMG_MAIN].crc;

	/* NV */
	dl_ctrl[NV_STAGE].stage = NV_STAGE;
	dl_ctrl[NV_STAGE].start = 1;
	dl_ctrl[NV_STAGE].download = 1;
	dl_ctrl[NV_STAGE].validate = 0;
	dl_ctrl[NV_STAGE].finish = 1;
	dl_ctrl[NV_STAGE].b_fd = nv_fd;
	dl_ctrl[NV_STAGE].b_offset = 0;
	dl_ctrl[NV_STAGE].b_size = toc[IMG_NV].size;
	dl_ctrl[NV_STAGE].crc = 0;

	/* FIN : "stage" field must have STD_UDL_FIN_STAGE value */
	dl_ctrl[FIN_STAGE].stage = STD_UDL_FIN_STAGE;
	dl_ctrl[FIN_STAGE].start = 1;
	dl_ctrl[FIN_STAGE].download = 0;
	dl_ctrl[FIN_STAGE].validate = 0;
	dl_ctrl[FIN_STAGE].finish = 0;
	dl_ctrl[FIN_STAGE].b_fd = 0;
	dl_ctrl[FIN_STAGE].b_offset = 0;
	dl_ctrl[FIN_STAGE].b_size = 0;
	dl_ctrl[FIN_STAGE].crc = 0;
}

static struct shannon_boot_args *prepare_boot_args(struct boot_args *cbd_args,
						   enum cp_boot_mode mode)
{
	int ret;
	int spi_fd = -1;
	int bin_fd = -1;
	int nv_fd = -1;
	struct modem_comp *cpn = cbd_args->cpn;
	struct shannon_boot_args *args = &shannon_boot_arguments;
	struct std_boot_args *std_args;
	struct std_toc_element *toc;
	size_t toc_size;

	memset(args, 0, sizeof(struct shannon_boot_args));

	/*
	** Prepare BOOT arguments which are common to all modems
	*/
	std_args = std_boot_prepare_args(cbd_args, SHANNON_MAX_DL_STAGE);
	if (!std_args) {
		cbd_log("ERR! std_boot_prepare_args fail\n");
		goto exit;
	}

	if (cbd_args->lnk_boot == LINKDEV_SPI) {
		/*
		** Open SPI device
		*/
		spi_fd = open(SPI_BOOT_DEV, O_RDWR);
		if (spi_fd < 0) {
			cbd_log("ERR! DEV(%s) open fail (%s)\n", SPI_BOOT_DEV, ERR2STR);
			goto exit;
		}
		cbd_log("DEV(%s) opened (fd %d)\n", SPI_BOOT_DEV, spi_fd);
	}

	/*
	** Open CP binary file
	*/
	bin_fd = open(cpn->path_bin, O_RDONLY);
	if(bin_fd < 0) {
		cbd_log("ERR! BIN(%s) open fail (%s)\n", cpn->path_bin, ERR2STR);
		goto exit;
	}
	cbd_log("BIN(%s) opened (fd %d)\n", cpn->path_bin, bin_fd);

	/*
	** Load and check TOC
	*/
	toc = std_args->toc;
	toc_size = sizeof(struct std_toc_element) * MAX_IMAGE_TYPE;

	ret = read(bin_fd, toc, toc_size);
	if (ret < 0) {
		cbd_log("ERR! TOC read fail (%s)\n", ERR2STR);
		goto exit;
	}

	if (strcmp(toc[IMG_TOC].name, "TOC")
	    || strcmp(toc[IMG_BOOT].name, "BOOT")
	    || strcmp(toc[IMG_MAIN].name, "MAIN")
	    || strcmp(toc[IMG_NV].name, "NV")) {
		cbd_log("ERR! invalid TOC\n");
		goto exit;
	}

	cbd_log("toc[%d].name = %s\n", IMG_TOC, toc[IMG_TOC].name);
	cbd_log("toc[%d].name = %s\n", IMG_BOOT, toc[IMG_BOOT].name);
	cbd_log("toc[%d].name = %s\n", IMG_MAIN, toc[IMG_MAIN].name);
	cbd_log("toc[%d].name = %s\n", IMG_NV, toc[IMG_NV].name);

	/*
	** Open NV data file
	*/
	if (mode == CP_BOOT_MODE_NORMAL) {
		/* Open NV data file */
		nv_fd = open(cpn->path_nv, O_RDONLY);
		if (nv_fd < 0) {
			if (errno != ENOENT) {
				cbd_log("ERR! NV(%s) open fail (%s)\n",
					cpn->path_nv, ERR2STR);
				goto exit;
			}

			cbd_log("ERR! no NV(%s) file\n", cpn->path_nv);

			ret = create_empty_nv(cpn->path_nv, toc[IMG_NV].size);
			if (ret < 0) {
				cbd_log("ERR! create_empty_nv(%s, %d) fail\n",
					cpn->path_nv, toc[IMG_NV].size);
				goto exit;
			}

			nv_fd = open(cpn->path_nv, O_RDONLY);
			if (nv_fd < 0) {
				cbd_log("ERR! NV(%s) open fail (%s)\n",
					cpn->path_nv, ERR2STR);
				goto exit;
			}
		}
		cbd_log("NV(%s) opened (fd %d)\n", cpn->path_nv, nv_fd);
	}

	/*
	** Assign SHANNON BOOT arguments
	*/
	args->cbd_args = cbd_args;
	args->std_args = std_args;
	args->load_fd = spi_fd;
	args->bin_fd = bin_fd;
	args->nv_fd = nv_fd;

	/*
	** Set standard DLOAD control parameters with SHANNON BOOT arguments
	*/
	build_std_dload_control(std_args, args);

	return args;

exit:
	if (std_args)
		std_boot_close_args(std_args);

	if (spi_fd >= 0)
		close(spi_fd);

	if (bin_fd >= 0)
		close(bin_fd);

	if (nv_fd >= 0)
		close(nv_fd);

	return NULL;
}

static void close_boot_args(struct shannon_boot_args *args)
{
	if (args) {
		if (args->std_args)
			std_boot_close_args(args->std_args);

		if (args->load_fd >= 0)
			close(args->load_fd);

		if (args->bin_fd >= 0)
			close(args->bin_fd);

		if (args->nv_fd >= 0)
			close(args->nv_fd);

		memset(args, 0, sizeof(struct shannon_boot_args));
	}
}

static int shannon_boot_xmit_loader(int dev_fd, struct std_boot_args *args, u32 stage)
{

	int ret = 0;
	struct std_dload_control *dlc = &args->dl_ctrl[stage];
	struct modem_firmware img;
	size_t rest = 0;

	rest = dlc->b_size;

	/* Prepare an image buffer */
	img.binary = malloc(dlc->b_size);
	if (!img.binary) {
		cbd_log("ERR! malloc(%d) fail\n", dlc->b_size);
		ret = -ENOMEM;
		goto exit;
	}
	img.size = dlc->b_size;

	/* Read BOOT loader */
	ret = lseek(dlc->b_fd, dlc->b_offset, SEEK_SET);
	if (ret < 0) {
		cbd_log("ERR! lseek fail (%s)\n", ERR2STR);
		goto exit;
	}

	ret = read(dlc->b_fd, img.binary, img.size);
	if (ret < 0) {
		cbd_log("ERR! read fail (%s)\n", ERR2STR);
		goto exit;
	}
	if ((u32)ret != img.size) {
		cbd_log("ERR! read %d != img.size %d\n", ret, img.size);
		ret = -EFAULT;
		goto exit;
	}

	/* Send BOOT loader */
	ret = ioctl(dev_fd, IOCTL_MODEM_XMIT_BOOT, &img);
	if (ret < 0) {
		cbd_log("ERR! IOCTL_MODEM_XMIT_BOOT fail (%s)\n", ERR2STR);
		goto exit;
	}

exit:
	if (img.binary)
		free(img.binary);

	return ret;
}

static int shannon_normal_boot(struct shannon_boot_args *args)
{
	int ret;
	struct std_boot_args *std_args = args->std_args;
	struct boot_args *cbd_args = args->cbd_args;

	if (cbd_args->lnk_boot == LINKDEV_SHMEM) {
		/* Load BOOT loader */
		ret = std_boot_load_loader(std_args, BOOT_STAGE);
		if (ret < 0) {
			cbd_log("ERR! std_boot_load_loader fail\n");
			goto exit;
		}
	}

	/* MODEM ON */
	ret = std_boot_modem_on(std_args);
	if (ret < 0) {
		cbd_log("ERR! std_boot_modem_on fail\n");
		goto exit;
	}

	/* BOOT ON */
	ret = std_boot_dload_on(std_args);
	if (ret < 0) {
		cbd_log("ERR! std_boot_dload_on fail\n");
		goto exit;
	}

	/* BOOT START */
	ret = std_boot_dload_start(std_args);
	if (ret < 0) {
		cbd_log("ERR! std_boot_dload_start fail\n");
		goto exit;
	}

	if (cbd_args->lnk_boot == LINKDEV_SPI) {
		/* Send BOOT loader */
		ret = shannon_boot_xmit_loader(args->load_fd, std_args, BOOT_STAGE);
		if (ret < 0) {
			cbd_log("ERR! shannon_boot_xmit_loader fail\n");
			goto exit;
		}
	}

	/* Send {BOOT_FIN, TOC, MAIN, NV, FIN} */
	ret = std_boot_dload(std_args);
	if (ret < 0) {
		cbd_log("ERR! std_boot_dload fail\n");
		goto exit;
	}

	/* BOOT OFF */
	ret = std_boot_dload_off(std_args);
	if (ret < 0) {
		cbd_log("ERR! std_boot_dload_off fail\n");
		goto exit;
	}

	/* BOOT DONE */
	ret = std_boot_finalize(std_args);
	if (ret < 0) {
		cbd_log("ERR! std_boot_finalize fail\n");
		goto exit;
	}

exit:
	return ret;
}

static int shannon_dump_boot(struct shannon_boot_args *args)
{
	int ret;
	struct std_boot_args *std_args = args->std_args;
	struct boot_args *cbd_args = args->cbd_args;

	/* Reset CP for CRASH DUMP */
	ret = std_dump_modem_reset(std_args);
	if (ret < 0) {
		cbd_log("ERR! std_dump_modem_reset fail\n");
		goto exit;
	}

	if (cbd_args->lnk_boot == LINKDEV_SHMEM) {
		/* Load BOOT loader */
		ret = std_boot_load_loader(std_args, BOOT_STAGE);
		if (ret < 0) {
			cbd_log("ERR! std_boot_load_loader fail\n");
			goto exit;
		}
	}

	/* DUMP START */
	ret = std_dump_uload_start(std_args);
	if (ret < 0) {
		cbd_log("ERR! std_dump_uload_start fail\n");
		goto exit;
	}

	if (cbd_args->lnk_boot == LINKDEV_SPI) {
		/* Send BOOT loader */
		ret = shannon_boot_xmit_loader(args->load_fd, std_args, BOOT_STAGE);
		if (ret < 0) {
			cbd_log("ERR! shannon_boot_xmit_loader fail\n");
			goto exit;
		}
	}

exit:
	return ret;
}

int start_shannon_boot(struct boot_args *cbd_args)
{
	int ret = 0;
	int spin = 50;
	char prop_buf[PROPERTY_VALUE_MAX] = {0, };
	struct shannon_boot_args *dl_args = NULL;

	cbd_log("CP boot device = %s\n", cbd_args->cpn->node_boot);

	cbd_log("CP binary file = %s\n", cbd_args->cpn->path_bin);

	cbd_log("CP NV file = %s\n", cbd_args->cpn->path_nv);

	switch (cbd_args->lnk_boot) {
	case LINKDEV_SPI:
		cbd_log("BOOT link SPI\n");
		break;

	case LINKDEV_SHMEM:
		cbd_log("BOOT link SHMEM\n");
		break;

	default:
		cbd_log("ERR! BOOT link# %d not supported\n", cbd_args->lnk_boot);
		ret = -ENODEV;
		goto exit;
	}

	switch (cbd_args->lnk_main) {
	case LINKDEV_C2C:
		cbd_log("MAIN link C2C\n");
		break;

	case LINKDEV_SHMEM:
		cbd_log("MAIN link SHMEM\n");
		break;

	case LINKDEV_LLI:
		cbd_log("MAIN link MIPI-LLI\n");
		break;

	default:
		cbd_log("ERR! MAIN link# %d not supported\n", cbd_args->lnk_main);
		ret = -ENODEV;
		goto exit;
	}

	/* Wait for completion of RILD's NV validity check */
	while (spin--) {
		property_get(VPROP_RFS_CHECKDONE, prop_buf, "0");
		if (prop_buf[0] == '1')
			break;
		usleep(100000);
	}
	cbd_log("NV validation %s\n", spin < 0 ? "TIMEOUT" : "done");

	/*
	** Start CP BOOT
	*/

	/* Prepare BOOT arguments which are specific to SHANNON */
	dl_args = prepare_boot_args(cbd_args, CP_BOOT_MODE_NORMAL);
	if (!dl_args) {
		cbd_log("ERR! prepare_boot_args fail\n");
		ret = -EFAULT;
		goto exit;
	}

	ret = shannon_normal_boot(dl_args);
	if (ret < 0) {
		cbd_log("ERR! shannon_normal_boot fail\n");
		goto exit;
	}

exit:
	if (dl_args)
		close_boot_args(dl_args);

	return ret;
}

int start_shannon_dump(struct boot_args *cbd_args)
{
	int ret;
	int log_fd = -1;
	struct std_dump_args *ul_args;
	struct shannon_boot_args *dl_args = NULL;
	char reason[MAX_PREFIX_LEN];

	/*
	** Prepare CP CRASH DUMP
	*/
	ul_args = std_dump_prepare_args(cbd_args);
	if (!ul_args) {
		cbd_log("ERR! std_dump_prepare_args fail\n");
		ret = -EFAULT;
		goto exit;
	}
	log_fd = ul_args->log_fd;

	/*
	** Start CP BOOT for CRASH DUMP
	*/
	dl_args = prepare_boot_args(cbd_args, CP_BOOT_MODE_DUMP);
	if (!dl_args) {
		cbd_log("ERR! prepare_boot_args fail\n");
		ret = -EFAULT;
		goto exit;
	}

	ret = shannon_dump_boot(dl_args);
	if (ret < 0) {
		cbd_log("ERR! shannon_dump_boot fail\n");
		dprintf(log_fd, "%s: ERR! shannon_dump_boot fail\n", __func__);
		goto exit;
	}

	/*
	** Receive CP CRASH DUMP
	*/
	ret = std_dump_uload(ul_args);
	if (ret < 0) {
		cbd_log("ERR! std_dump_uload fail\n");
		dprintf(log_fd, "%s: ERR! std_dump_uload fail\n", __func__);
		goto exit;
	}

	/*
	** Save kernel log
	*/
	snprintf(reason, MAX_PREFIX_LEN, "%s_crash", cbd_args->cpn->rat);
	save_logs(LOGB_DMESG, reason);

	/*
	** Finalize "CP Crash DUMP" and trigger kernel panic
	*/
	sleep(30);
	ret = std_dump_finalize(ul_args);
	if (ret < 0) {
		cbd_log("ERR! std_dump_finalize fail\n");
		dprintf(log_fd, "%s: ERR! std_dump_finalize fail\n", __func__);
		goto exit;
	}

exit:
	if (ret < 0) {
		snprintf(reason, MAX_PREFIX_LEN, "%s_dump_fail", cbd_args->cpn->rat);
		save_logs(LOGB_DMESG, reason);
	}

	if (ul_args)
		std_dump_close_args(ul_args);

	if (dl_args)
		close_boot_args(dl_args);

	return ret;
}

int shutdown_shannon_modem(struct boot_args *cbd_args)
{
	int ret = 0;
	char *node_boot = cbd_args->cpn->node_boot;
	int fd;

	fd = open(node_boot, O_RDWR | O_NDELAY);
	if (fd < 0) {
		cbd_log("%s open fail\n", node_boot);
		ret = -errno;
		goto exit;
	}

	ioctl(fd, IOCTL_MODEM_OFF, NULL);

	close(fd);

exit:
	return ret;
}
