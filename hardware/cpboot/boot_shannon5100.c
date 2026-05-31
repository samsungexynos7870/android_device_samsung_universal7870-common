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
	int i;
	struct std_dload_control *tmp;

	memset(dl_ctrl, 0, sizeof(struct std_dload_control) * MAX_DLOAD_STAGE);

	for (i = 0; i < (int)std_args->num_stages; i++) {
		/* End of TOC */
		if (strcmp(toc[i].name, "OFFSET") == 0)
			break;

		/* TOC */
		if (strcmp(toc[i].name, "TOC") == 0) {
			dl_ctrl[TOC_STAGE].stage = TOC_STAGE;
			dl_ctrl[TOC_STAGE].start = 1;
			dl_ctrl[TOC_STAGE].download = 1;
			dl_ctrl[TOC_STAGE].validate = 0;
			dl_ctrl[TOC_STAGE].finish = 1;
			dl_ctrl[TOC_STAGE].b_fd = bin_fd;
			dl_ctrl[TOC_STAGE].b_offset = toc[i].b_offset;
			/* CP receive TOC for only size 0x100 */
			dl_ctrl[TOC_STAGE].b_size = toc[IMG_TOC].size;
			dl_ctrl[TOC_STAGE].crc = toc[i].crc;
			goto next;
		}

		/*
		 * During CP download with S5100 PCIE interface,
		 * AP should download CP boot binary via SPI and
		 * the other binaries via PCIE.
		 * So CP boot should be separated with others
		 */
		if (strcmp(toc[i].name, "BOOT") == 0) {
			dl_ctrl[BOOT_STAGE].stage = BOOT_STAGE;
			dl_ctrl[BOOT_STAGE].start = 0;
			dl_ctrl[BOOT_STAGE].download = 0;
			dl_ctrl[BOOT_STAGE].validate = 1;
			dl_ctrl[BOOT_STAGE].finish = 1;
			dl_ctrl[BOOT_STAGE].b_fd = bin_fd;
			dl_ctrl[BOOT_STAGE].b_offset = toc[IMG_BOOT].b_offset;
			dl_ctrl[BOOT_STAGE].b_size = toc[IMG_BOOT].size;
			dl_ctrl[TOC_STAGE].crc = toc[i].crc;
			goto next;
		}

		/* MAIN */
		if (strcmp(toc[i].name, "MAIN") == 0)
			dl_ctrl[i].validate = 1;
		else
			dl_ctrl[i].validate = 0;

		if (strcmp(toc[i].name, "NV") == 0)
			dl_ctrl[i].b_fd = nv_fd;
		else
			dl_ctrl[i].b_fd = bin_fd;

		dl_ctrl[i].stage = i;
		dl_ctrl[i].b_offset = toc[i].b_offset;
		dl_ctrl[i].b_size = toc[i].size;
		dl_ctrl[i].crc = toc[i].crc;
		dl_ctrl[i].start = 1;
		dl_ctrl[i].download = 1;
		dl_ctrl[i].finish = 1;

next:
		tmp = &dl_ctrl[i];
		cbd_log("stage=%u, name:%s b_off=0x%08x, m_offset=0x%08x b_size=0x%08x\n",
			tmp->stage, toc[i].name, tmp->b_offset, tmp->m_offset,
			tmp->b_size);
	}

	dl_ctrl[i].stage = STD_UDL_FIN_STAGE;
	dl_ctrl[i].start = 1;
}

static int std_register_pcie(struct std_boot_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;

	ret = ioctl(dev_fd, IOCTL_REGISTER_PCIE, NULL);
	if (ret < 0) {
		cbd_log("ERR! IOCTL_MODEM_DL_START fail\n");
		goto exit;
	}

	return 0;

exit:
	return ret;
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
	int max_stage;
	u32 nv_size = 0;
	int i;

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
	toc_size = sizeof(struct std_toc_element) * MAX_TOC_INDEX;

	ret = read(bin_fd, toc, toc_size);
	if (ret < 0) {
		cbd_log("ERR! TOC read fail (%s)\n", ERR2STR);
		goto exit;
	}

	if (strcmp(toc[IMG_TOC].name, "TOC")) {
		cbd_log("ERR! invalid TOC: No TOC\n");
		goto exit;
	}

	if (toc[IMG_TOC].toc_count > MAX_TOC_INDEX) {
		cbd_log("ERR! invalid TOC: Total TOC count is %d\n",
			toc[IMG_TOC].toc_count);
		goto exit;
	}

	if (toc[IMG_TOC].toc_count == 1)
		max_stage = SHANNON_MAX_DL_STAGE;
	else
		max_stage = toc[IMG_TOC].toc_count;

	std_args->num_stages = max_stage;
	cbd_log("max_stage: %d\n", max_stage);

	for (i = 0; i < max_stage; i++) {
		if (strcmp(toc[i].name, "NV") == 0)
			nv_size = toc[i].size;

		cbd_log("TOC[%d].name = %s, b_off=0x%08x, m_off=0x%08x, size=0x%08x crc=0x%08x\n",
			i, toc[i].name, toc[i].b_offset, toc[i].m_offset,
			toc[i].size, toc[i].crc);
	}

	if ((mode != CP_BOOT_MODE_DUMP) && (nv_size == 0) && cpn->path_nv[0]) {
		cbd_log("ERR! invalid TOC : There is no NV\n");
		goto exit;
	}

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

			ret = create_empty_nv(cpn->path_nv, nv_size);
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

static int vss_full_dump(struct std_dump_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;
	int log_fd = args->log_fd;
	unsigned long vss_size, copied = 0;
	char buf[PAGE_SIZE];
	int dump_fd = -1;
	char path[MAX_PATH_LEN];
	char suffix[MAX_SUFFIX_LEN];
	time_t now;
	struct tm result;

	/* Get vss memory size and Trigger vss dump */
	ret = ioctl(dev_fd, IOCTL_VSS_FULL_DUMP, &vss_size);
	if (ret < 0) {
		cbd_log("ERR! ioctl fail (%s)\n", ERR2STR);
		dprintf(log_fd, "ERR! ioctl fail (%s)\n", ERR2STR);
		return -EINVAL;
	}

	cbd_log("vss_size:%lu\n", vss_size);
	dprintf(log_fd, "%s: vss_size:%lu\n", __func__, vss_size);

	/* Open (create) a vss memory dump file */
	time(&now);
	localtime_r(&now, &result);
	strftime(suffix, MAX_SUFFIX_LEN, "%Y%m%d-%H%M", &result);
	sprintf(path, "%s/cpcrash_vss_dump_%s_%s.log", get_log_dir(),
			args->cbd_args->cpn->rat, suffix);

	dump_fd = open(path, O_WRONLY | O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (dump_fd < 0) {
		cbd_log("ERR! %s open fail (%s)\n", path, ERR2STR);
		dprintf(log_fd, "%s: ERR! %s open fail (%s)\n", __func__, path, ERR2STR);
		return -EINVAL;
	}

	cbd_log("%s opened (fd %d)\n", path, dump_fd);
	dprintf(log_fd, "%s: %s opened (fd %d)\n", __func__, path, dump_fd);

	/* Read & Save vss memory dump */
	while (copied < vss_size) {
		ret = std_ul_rx_frame(args, buf, sizeof(buf));
		if (ret < 0) {
			cbd_log("ERR! vss, std_ul_rx_frame fail (ret %d)\n", ret);
			dprintf(log_fd, "%s: ERR! vss, std_ul_rx_frame fail (ret %d)\n", __func__, ret);
			goto exit;
		}

		/* not verified */
		copied += ret;

		ret = write(dump_fd, buf, ret);
		if (ret < 0) {
			cbd_log("ERR! vss, write fail (%s)\n", ERR2STR);
			dprintf(log_fd, "%s: ERR! vss, write fail (%s)\n", __func__, ERR2STR);
			goto exit;
		}
	}

	cbd_log("Complete! (%lu bytes)\n", copied);
	dprintf(log_fd, "%s: %s Complete! (%lu bytes)\n", __func__, path, copied);
exit:
	if (dump_fd >= 0)
		close(dump_fd);

	return ret;
}

static int acpm_full_dump(struct std_dump_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;
	int log_fd = args->log_fd;
	unsigned long acpm_size, copied = 0;
	char buf[PAGE_SIZE];
	int dump_fd = -1;
	char path[MAX_PATH_LEN];
	char suffix[MAX_SUFFIX_LEN];
	time_t now;
	struct tm result;

	/* Get acpm memory size and Trigger acpm dump */
	ret = ioctl(dev_fd, IOCTL_ACPM_FULL_DUMP, &acpm_size);
	if (ret < 0) {
		cbd_kernel("ERR! ioctl fail (%s)\n", ERR2STR);
		dprintf(log_fd, "ERR! ioctl fail (%s)\n", ERR2STR);
		return -EINVAL;
	}

	cbd_log("acpm_size:%lu\n", acpm_size);
	dprintf(log_fd, "%s: acpm_size:%lu\n", __func__, acpm_size);

	/* Open (create) a acpm memory dump file */
	time(&now);
	localtime_r(&now, &result);
	strftime(suffix, MAX_SUFFIX_LEN, "%Y%m%d-%H%M", &result);
	sprintf(path, "%s/cpcrash_acpm_dump_%s_%s.log", get_log_dir(),
			args->cbd_args->cpn->rat, suffix);

	dump_fd = open(path, O_WRONLY | O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (dump_fd < 0) {
		cbd_log("ERR! %s open fail (%s)\n", path, ERR2STR);
		dprintf(log_fd, "%s: ERR! %s open fail (%s)\n", __func__, path, ERR2STR);
		return -EINVAL;
	}

	cbd_log("%s opened (fd %d)\n", path, dump_fd);
	dprintf(log_fd, "%s: %s opened (fd %d)\n", __func__, path, dump_fd);

	/* Read & Save acpm memory dump */
	while (copied < acpm_size) {
		ret = std_ul_rx_frame(args, buf, sizeof(buf));
		if (ret < 0) {
			cbd_log("ERR! acpm, std_ul_rx_frame fail (ret %d)\n", ret);
			dprintf(log_fd, "%s: ERR! acpm, std_ul_rx_frame fail (ret %d)\n", __func__, ret);
			goto exit;
		}

		/* not verified */
		copied += ret;

		ret = write(dump_fd, buf, ret);
		if (ret < 0) {
			cbd_log("ERR! acpm, write fail (%s)\n", ERR2STR);
			dprintf(log_fd, "%s: ERR! acpm, write fail (%s)\n", __func__, ERR2STR);
			goto exit;
		}
	}

	cbd_log("Complete! (%lu bytes)\n", copied);
	dprintf(log_fd, "%s: %s Complete! (%lu bytes)\n", __func__, path, copied);
exit:
	if (dump_fd >= 0)
		close(dump_fd);

	return ret;
}

static int shmem_full_dump(struct std_dump_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;
	int log_fd = args->log_fd;
	unsigned long shmem_size, copied = 0;
	char buf[PAGE_SIZE];
	int dump_fd = -1;
	char path[MAX_PATH_LEN];
	char suffix[MAX_SUFFIX_LEN];
	time_t now;
	struct tm result;

	/* Get shared memory size and Trigger shmem dump */
	ret = ioctl(dev_fd, IOCTL_SHMEM_FULL_DUMP, &shmem_size);
	if (ret < 0) {
		cbd_log("ERR! ioctl fail (%s)\n", ERR2STR);
		dprintf(log_fd, "ERR! ioctl fail (%s)\n", ERR2STR);
		return -EINVAL;
	}

	cbd_log("shmem_size:%lu\n", shmem_size);
	dprintf(log_fd, "%s: shmem_size:%lu\n", __func__, shmem_size);

	/* Open (create) a shared memory dump file */
	time(&now);
	localtime_r(&now, &result);
	strftime(suffix, MAX_SUFFIX_LEN, "%Y%m%d-%H%M", &result);
	sprintf(path, "%s/cpcrash_shmem_dump_%s_%s.log", get_log_dir(),
			args->cbd_args->cpn->rat, suffix);

	dump_fd = open(path, O_WRONLY | O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (dump_fd < 0) {
		cbd_log("ERR! %s open fail (%s)\n", path, ERR2STR);
		dprintf(log_fd, "%s: ERR! %s open fail (%s)\n", __func__, path, ERR2STR);
		return -EINVAL;
	}

	cbd_log("%s opened (fd %d)\n", path, dump_fd);
	dprintf(log_fd, "%s: %s opened (fd %d)\n", __func__, path, dump_fd);

	/* Read & Save shared memory dump */
	while (copied < shmem_size) {
		ret = std_ul_rx_frame(args, buf, sizeof(buf));
		if (ret < 0) {
			cbd_log("ERR! shmem, std_ul_rx_frame fail (ret %d)\n", ret);
			dprintf(log_fd, "%s: ERR! shmem, std_ul_rx_frame fail (ret %d)\n", __func__, ret);
			goto exit;
		}

		/* not verified */
		copied += ret;

		ret = write(dump_fd, buf, ret);
		if (ret < 0) {
			cbd_log("ERR! shmem, write fail (%s)\n", ERR2STR);
			dprintf(log_fd, "%s: ERR! shmem, write fail (%s)\n", __func__, ERR2STR);
			goto exit;
		}
	}

	cbd_log("Complete! (%lu bytes)\n", copied);
	dprintf(log_fd, "%s: %s Complete! (%lu bytes)\n", __func__, path, copied);
exit:
	if (dump_fd >= 0)
		close(dump_fd);

	return ret;
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
	if (ret) {
		cbd_log("ERR! IOCTL_MODEM_XMIT_BOOT fail (%d)\n", ret);
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

	if (cbd_args->lnk_main == LINKDEV_PCIE) {
		/* Register PCIe Device - Initialization & LINK */
		ret = std_register_pcie(std_args);
		if (ret < 0) {
			cbd_log("ERR! std_register_pcie fail\n");
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

	if (cbd_args->lnk_main == LINKDEV_PCIE) {
		/* Register PCIe Device - Initialization & LINK */
		ret = std_register_pcie(std_args);
		if (ret < 0) {
			cbd_log("ERR! std_register_pcie fail\n");
			goto exit;
		}
	}

exit:
	return ret;
}

int start_shannon5100_boot(struct boot_args *cbd_args)
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

	case LINKDEV_PCIE:
		cbd_log("MAIN link PCIE\n");
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

int start_shannon5100_dump(struct boot_args *cbd_args)
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

	ret = shmem_full_dump(ul_args);
	if (ret < 0) {
		cbd_info("ERR! shmem_full_dump fail\n");
		cbd_kernel("ERR! shmem_full_dump fail\n");
	}

	ret = vss_full_dump(ul_args);
	if (ret < 0) {
		cbd_info("ERR! vss_full_dump fail\n");
		cbd_kernel("ERR! vss_full_dump fail\n");
	}

	ret = acpm_full_dump(ul_args);
	if (ret < 0) {
		cbd_info("ERR! acpm_full_dump fail\n");
		cbd_kernel("ERR! acmp_full_dump fail\n");
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

int shutdown_shannon5100_modem(struct boot_args *cbd_args)
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

int upload_shannon5100_modem(struct boot_args *cbd_args)
{
	int ret = -1;
	char *node_boot = cbd_args->cpn->node_boot;
	int fd;

	fd = open(node_boot, O_RDWR | O_NDELAY);
	if (fd < 0) {
		cbd_log("%s open fail\n", node_boot);
		ret = -errno;
		goto exit;
	}

	cbd_log("Go to UPLOAD mode\n");

	ret = ioctl(fd, IOCTL_MODEM_CP_UPLOAD, cbd_args->reason);
	if (ret < 0) {
		cbd_log("ERR! IOCTL_MODEM_CP_UPLOAD fail (%s)\n", ERR2STR);
		goto exit;
	}

	return 0;

exit:
	return ret;
}