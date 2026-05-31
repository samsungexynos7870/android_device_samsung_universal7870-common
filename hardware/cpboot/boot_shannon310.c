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
#include "shannon310.h"

#include "util_srinfo.h"

static struct shannon_boot_args shannon_boot_arguments;

static void boot_wake_lock(int lock)
{
	char *path = lock ? "/sys/power/wake_lock" : "/sys/power/wake_unlock";
	char *name = "ss310";
	int fd, ret;

	fd = open(path,  O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (fd < 0) {
		cbd_log("user wake_%s open fail(%d)\n", lock ? "lock" : "unlock", fd);
		return;
	}
	ret = write(fd, name, strlen(name));
	if (ret < 0) {
		cbd_log("write fail - %s (%d)\n", name, ret);
		goto exit;

	}
	cbd_log("%s/%s\n", path, name);
exit:
	if (fd >= 0)
		close(fd);
	return;
}

static void build_std_dload_control(struct std_boot_args *std_args,
				    struct shannon_boot_args *args)
{
	struct std_dload_control *dl_ctrl = std_args->dl_ctrl;
	struct std_toc_element *toc = std_args->toc;
	int bin_fd = args->bin_fd;
	int nv_fd = args->nv_fd;
	struct std_dload_control *tmp;
	u32 i;

	for (i = std_args->start_stages; i < std_args->num_stages + 1; i++) {
		dl_ctrl[i].stage = i;
		dl_ctrl[i].start = 0;
		dl_ctrl[i].download = 0;
		dl_ctrl[i].validate = 0;
		dl_ctrl[i].finish = 0;

		if (strcmp(toc[i].name, "VSS") == 0)
			dl_ctrl[i].dl_once = 1;
		else
			dl_ctrl[i].dl_once = 0;

		if (strcmp(toc[i].name, "NV") == 0)
			dl_ctrl[i].b_fd = nv_fd;
		else
			dl_ctrl[i].b_fd = bin_fd;

		dl_ctrl[i].b_offset = toc[i].b_offset;
		dl_ctrl[i].m_offset = (toc[i].m_offset & CP_MEMORY_MASK);
		dl_ctrl[i].b_size = toc[i].size;
		dl_ctrl[i].crc = toc[i].crc;

		tmp = &dl_ctrl[i];
		cbd_log("stage=%u, name:%s b_off=0x%08x, m_offset=0x%08x b_size=0x%08x\n",
			tmp->stage, toc[i].name, tmp->b_offset, tmp->m_offset,
			tmp->b_size);
	}
}

static struct shannon_boot_args *prepare_boot_args(struct boot_args *cbd_args,
						   enum cp_boot_mode mode)
{
	int ret;
	int bin_fd = -1;
	int nv_fd = -1;
	struct modem_comp *cpn = cbd_args->cpn;
	struct shannon_boot_args *args = &shannon_boot_arguments;
	struct std_boot_args *std_args;
	struct std_toc_element *toc;
	size_t toc_size;
	u32 i;
	u32 nv_size = 0;

	memset(args, 0, sizeof(struct shannon_boot_args));

	/*
	** Prepare BOOT arguments which are common to all modems
	*/
	std_args = std_boot_prepare_args(cbd_args, SHANNON_MAX_DL_STAGE);
	if (!std_args) {
		cbd_log("ERR! std_boot_prepare_args fail\n");
		goto exit;
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


	/*
	** Set binary stage information
	*/
	std_args->start_stages = BOOT_STAGE;

	if (toc[IMG_TOC].toc_count == 1) {
		/* Support legacy TOC table */
		std_args->num_stages = SHANNON_LEGACY_MAX_DL_STAGE;
	} else {
#ifdef CONFIG_SEC_CP_VERIFYING_ALL
		/* Support verifying TOC/VSS signing : if TOC has m_offset, load it to cp dram */
		if (toc[IMG_TOC].m_offset != 0)
			std_args->start_stages = TOC_STAGE;
#endif
		std_args->num_stages = toc[IMG_TOC].toc_count - 1;
	}

	if (!cpn->path_nv[0]) {
		/* For wifi model */
		std_args->num_stages = MAIN_STAGE;
	}

	if (mode == CP_BOOT_MODE_DUMP) {
		/* For Dump mode */
		std_args->num_stages = BOOT_STAGE;
	}

	for (i = std_args->start_stages; i < std_args->num_stages + 1; i++) { /* Included 'start TOC' */
		if (strcmp(toc[i].name, "NV") == 0)
			nv_size = toc[i].size;

		cbd_log("toc[%d].name = %s, b_off=0x%08x, m_off=0x%08x, size=0x%08x\n",
			i, toc[i].name, toc[i].b_offset, toc[i].m_offset, toc[i].size);
	}

	if ((mode != CP_BOOT_MODE_DUMP) && (nv_size == 0) && cpn->path_nv[0]) {
		cbd_log("ERR! invalid TOC : There is no NV\n");
		goto exit;
	}

	/*
	** Open NV data file
	*/
	if (mode == CP_BOOT_MODE_NORMAL && cpn->path_nv[0]) {
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
					cpn->path_nv, nv_size);
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

static int cplog_full_dump(struct std_dump_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;
	int log_fd = args->log_fd;
	unsigned long cplog_size, copied = 0;
	char buf[PAGE_SIZE];
	int dump_fd = -1;
	char path[MAX_PATH_LEN];
	char suffix[MAX_SUFFIX_LEN];
	time_t now;
	struct tm result;

	/* Get cplog memory size and Trigger cplog dump */
	ret = ioctl(dev_fd, IOCTL_CPLOG_FULL_DUMP, &cplog_size);
	if (ret < 0) {
		cbd_log("ERR! ioctl fail (%s)\n", ERR2STR);
		dprintf(log_fd, "ERR! ioctl fail (%s)\n", ERR2STR);
		return -EINVAL;
	}

	cbd_log("cplog_size:%lu\n", cplog_size);
	dprintf(log_fd, "%s: cplog_size:%lu\n", __func__, cplog_size);

	/* Open (create) a cplog memory dump file */
	time(&now);
	localtime_r(&now, &result);
	strftime(suffix, MAX_SUFFIX_LEN, "%Y%m%d-%H%M", &result);
	sprintf(path, "%s/cpcrash_cplog_dump_%s_%s.BTL", get_log_dir(),
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

	/* Read & Save cplog memory dump */
	while (copied < cplog_size) {
		ret = std_ul_rx_frame(args, buf, sizeof(buf));
		if (ret < 0) {
			cbd_log("ERR! cplog, std_ul_rx_frame fail (ret %d)\n", ret);
			dprintf(log_fd, "%s: ERR! cplog, std_ul_rx_frame fail (ret %d)\n", __func__, ret);
			goto exit;
		}

		/* not verified */
		copied += ret;

		ret = write(dump_fd, buf, ret);
		if (ret < 0) {
			cbd_log("ERR! cplog, write fail (%s)\n", ERR2STR);
			dprintf(log_fd, "%s: ERR! cplog, write fail (%s)\n", __func__, ERR2STR);
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
		cbd_log("ERR! ioctl fail (%s)\n", ERR2STR);
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

static int databuf_full_dump(struct std_dump_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;
	int log_fd = args->log_fd;
	unsigned long dump_size, copied = 0;
	char buf[PAGE_SIZE];
	int dump_fd = -1;
	char path[MAX_PATH_LEN];
	char suffix[MAX_SUFFIX_LEN];
	time_t now;
	struct tm result;

#ifndef CONFIG_DUMP_DATABUF
	return 0;
#endif

	/* Get databuf memory size and Trigger dump */
	ret = ioctl(dev_fd, IOCTL_DATABUF_FULL_DUMP, &dump_size);
	if (ret < 0) {
		cbd_log("ERR! ioctl fail (%s)\n", ERR2STR);
		dprintf(log_fd, "ERR! ioctl fail (%s)\n", ERR2STR);
		return -EINVAL;
	}

	cbd_log("dump_size:%lu\n", dump_size);
	dprintf(log_fd, "%s: dump_size:%lu\n", __func__, dump_size);

	/* Open (create) a databuf dump file */
	time(&now);
	localtime_r(&now, &result);
	strftime(suffix, MAX_SUFFIX_LEN, "%Y%m%d-%H%M", &result);
	sprintf(path, "%s/cpcrash_databuf_dump_%s_%s.log", get_log_dir(),
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

	/* Read & Save databuf memory dump */
	while (copied < dump_size) {
		ret = std_ul_rx_frame(args, buf, sizeof(buf));
		if (ret < 0) {
			cbd_log("ERR! databuf, std_ul_rx_frame fail (ret %d)\n", ret);
			dprintf(log_fd, "%s: ERR! databuf, std_ul_rx_frame fail (ret %d)\n", __func__, ret);
			goto exit;
		}

		/* not verified */
		copied += ret;

		ret = write(dump_fd, buf, ret);
		if (ret < 0) {
			cbd_log("ERR! databuf, write fail (%s)\n", ERR2STR);
			dprintf(log_fd, "%s: ERR! databuf, write fail (%s)\n", __func__, ERR2STR);
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

static int get_modem_state(struct std_boot_args *args)
{
	int status;

	status = ioctl(args->dev_fd, IOCTL_MODEM_STATUS);
	cbd_log("modem_status: %d\n", status);
	return status;
}

int shannon310_boot_xmit_bin(struct std_boot_args *args, u32 stage, enum cp_boot_mode mode)
{
	int ret = 0;
	int last = 0;
	int dev_fd = args->dev_fd;
	struct std_dload_control *dlc = &args->dl_ctrl[stage];
	struct modem_img img;
	void *binary;
	unsigned total = 0;

	/* Prepare an image buffer */
	binary = malloc(EXYNOS_PAYLOAD_LEN);
	if (!binary) {
		cbd_log("ERR! malloc(%d) fail\n", dlc->b_size);
		ret = -ENOMEM;
		goto exit;
	}

	img.binary = (unsigned long)binary;
	img.size = dlc->b_size;
	img.m_offset = dlc->m_offset;
	img.b_offset = dlc->b_offset;
	img.mode = mode;
	img.len = EXYNOS_PAYLOAD_LEN;

	cbd_log("stage=%u(%u), b_off=0x%08x, m_off=0x%08x, b_size=0x%08x, mode=0x%08x\n",
		stage, dlc->stage, dlc->b_offset, dlc->m_offset, dlc->b_size, img.mode);

	ret = lseek(dlc->b_fd, img.b_offset, SEEK_SET);
	if (ret < 0) {
		cbd_log("ERR! lseek fail (%u)\n", stage);
		goto exit;
	}

	while(1) {
		if(img.size == img.b_offset)
			break;

		if((img.size - total) < EXYNOS_PAYLOAD_LEN) {
			img.len = img.size - total;
			last = 1;
		}

		ret = read(dlc->b_fd, (void *)img.binary, img.len);
		if (ret < 0) {
			cbd_log("ERR! read fail (%u)\n", stage);
			goto exit;
		}

		if ((u32)ret != img.len) {
			cbd_log("ERR! read %d != img.len %d\n", ret, img.len);
			ret = -EFAULT;
			goto exit;
		}

		ret = ioctl(dev_fd, IOCTL_MODEM_XMIT_BOOT, &img);
		if (ret) {
			cbd_log("ERR! IOCTL_MODEM_XMIT_BOOT fail (%u,%d)\n", stage, ret);
			goto exit;
		}

		if(last == 1)
			break;

		total += img.len;
		img.m_offset += img.len;
	}
	cbd_log("%u stage complelte\n", stage);
exit:
	if (binary)
		free(binary);

	return ret;
}

int shannon310_boot_xmit(struct std_boot_args *args, enum cp_boot_mode mode)
{
	int ret = 0;
	u32 stage;
	char prop_buf[PROPERTY_VALUE_MAX] = {0, };
	struct std_dload_control *dl_ctrl = args->dl_ctrl;

	property_get(VPROP_FIRST_XMIT_DONE, prop_buf, "0");

	for (stage = args->start_stages; stage < args->num_stages + 1; stage++) {
		if (dl_ctrl[stage].dl_once && prop_buf[0] == '1') {
			cbd_log("stage[%u] : stage is already xmit once\n", stage);
			continue;
		}
		ret = shannon310_boot_xmit_bin(args, stage, mode);
		if(ret < 0) {
			cbd_log("ERR! shannon310_boot_xmit_bin stage[%u] fail\n", stage);
			return ret;
		}
	}

	/* return xmit_boot state */
	ret = prop_buf[0] - '0';

	property_set(VPROP_FIRST_XMIT_DONE, "1");
	return ret;
}


#define DT_REVISION_PATH "/proc/device-tree/model_info-system_rev"
void check_board_revision(void)
{
	int fd, ret;
	char rev[256];

	fd = open(DT_REVISION_PATH, O_RDONLY);
	if (fd < 0) {
		cbd_log("%s open fail\n", DT_REVISION_PATH);
		return;
	}
	ret = read(fd, rev, 256);
	if (ret < 0) {
		cbd_log("%s read fail\n", DT_REVISION_PATH);
		close(fd);
		return;
	}
	cbd_log("model_info-system_rev: %s\n", rev);
	close(fd);
	property_set(VPROP_DT_REVISION, rev);
}

#define SIM_CONF_PATH		"/efs/factory.prop"
#define MAX_PROP_STRING_LEN	128
int get_factory_prop(void) 
{
	int fd, ret;
	char full_string[MAX_PROP_STRING_LEN];
	char *sim_value, *token = "=";

	fd = open(SIM_CONF_PATH, O_RDONLY);
	if (fd < 0) {
		cbd_log("%s open fail(not support)\n", SIM_CONF_PATH);
		return -EINVAL;
	}

	ret = read(fd, full_string, MAX_PROP_STRING_LEN);
	if (ret < 0) {
		cbd_log("%s read fail\n", SIM_CONF_PATH);
		goto exit;
	}

	sim_value = strstr(full_string, token);
	if (sim_value == NULL) {
		cbd_log("can't find token!\n");
		ret = -EINVAL;
		goto exit;
	} else {
		ret = (++sim_value)[0] - '0';
		cbd_log("sim_count: %d\n", ret);
	}

exit:
	close(fd);
	return ret;
}

#define SIM_CONF_KERN_PATH	"/sys/devices/platform/10000.mif_pdata/sim/ds_detect"
void set_sim_configuration(void)
{
	char cmd_string[128];
	int sim_count;

	sim_count = get_factory_prop();
	if (sim_count > 0) {
		sprintf(cmd_string, "echo %d > %s", sim_count, SIM_CONF_KERN_PATH);
		system(cmd_string);
	}
}

int start_shannon310_boot(struct boot_args *cbd_args)
{
	int ret = 0;
	int spin = 50;
	char prop_buf[PROPERTY_VALUE_MAX] = {0, };
	struct shannon_boot_args *dl_args = NULL;

	check_board_revision();

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

	boot_wake_lock(1);

	/* Prepare arguments, read TOC, check NV */
	cbd_log("Prepare arguments, read TOC, check NV\n");
	dl_args = prepare_boot_args(cbd_args, CP_BOOT_MODE_NORMAL);
	if (!dl_args) {
		cbd_log("ERR! prepare_boot_args fail\n");
		ret = -EFAULT;
		goto exit;
	}

	store_srinfo(cbd_args->cpn->rat, dl_args->std_args->dev_fd);

	/* Modem off */
	if (get_modem_state(dl_args->std_args) != STATE_OFFLINE) {
		ret = std_boot_modem_reset(dl_args->std_args);
		if (ret < 0) {
			cbd_log("ERR! std_boot_modem_reset fail\n");
			goto exit;
		}
	}

	/* Request Security : non-secure mode */
	cbd_log("Request Security : non-secure mode\n");
	ret = std_security_req(dl_args->std_args, CP_BOOT_RE_INIT, 0, 0);
	if (ret < 0) {
		cbd_log("ERR! security check fail\n");
		goto exit;
	}

	/* Send BIN */
	cbd_log("Send BIN\n");
	ret = shannon310_boot_xmit(dl_args->std_args, CP_BOOT_MODE_NORMAL);
	if (ret < 0) {
		cbd_log("ERR! BOOT_STAGE fail\n");
		goto exit;
	}

#ifdef CONFIG_SEC_CP_VERIFYING_ALL
	cbd_log("Request Security : verifying VSS, TOC\n");
	if (dl_args->std_args->start_stages == BOOT_STAGE) {
		cbd_log("ERR! TOC has no m_offset\n");
		ret = -EINVAL;
		goto exit;
	}

	if (ret) {
		/* if xmit_boot was already done once, skip vss verifying */
		cbd_log("skip VSS check\n");
	} else {
		ret = std_security_req(dl_args->std_args, CP_BOOT_MODE_MANUAL,
				       dl_args->std_args->dl_ctrl[IMG_VSS].m_offset,
				       dl_args->std_args->dl_ctrl[IMG_VSS].b_size);
		if (ret < 0) {
			cbd_log("ERR! security check fail for VSS\n");
			goto exit;
		}
	}

	ret = std_security_req(dl_args->std_args, CP_BOOT_MODE_MANUAL,
			       dl_args->std_args->dl_ctrl[IMG_TOC].m_offset,
			       dl_args->std_args->dl_ctrl[IMG_TOC].b_size);
	if (ret < 0) {
		cbd_log("ERR! security check fail for TOC\n");
		goto exit;
	}
#endif

	/* Request Security : secure mode */
	cbd_log("Request Security : secure mode\n");
	ret = std_security_req(dl_args->std_args, CP_BOOT_MODE_NORMAL,
			       dl_args->std_args->dl_ctrl[IMG_BOOT].b_size,
			       dl_args->std_args->dl_ctrl[IMG_MAIN].b_size);
	if (ret < 0) {
		cbd_log("ERR! security check fail\n");
		goto exit;
	}

	/* set SIM configuration using /efs/factory.prop */
	set_sim_configuration();

	/* MODEM ON : pda_active 1, cp power on */
	cbd_log("MODEM ON : pda_active 1, cp power on\n");
	ret = std_boot_modem_on(dl_args->std_args);
	if (ret < 0) {
		cbd_log("ERR! std_boot_modem_on fail\n");
		goto exit;
	}

	/* BOOT ON : change modem state, irq, wait cp state */
	cbd_log("BOOT ON : change modem state, irq, wait cp state\n");
	ret = std_boot_dload_on(dl_args->std_args);
	if (ret < 0) {
		cbd_log("ERR! std_boot_dload_on fail\n");
		goto exit;
	}

	/* BOOT START : reset ipcmap, set magic code */
	cbd_log("BOOT START : reset ipcmap, set magic code\n");
	ret = std_boot_dload_start(dl_args->std_args);
	if (ret < 0) {
		cbd_log("ERR! std_boot_dload_start fail\n");
		goto exit;
	}

	/* HANDSHAKE COMMAND : send & recv boot_done, fin message */
	cbd_log("HANDSHAKE COMMAND : send & recv boot_done, fin message\n");
	ret = std_boot_finish_handshake(dl_args->std_args);
	if (ret < 0) {
		cbd_log("ERR! std_boot_xmit_cmd fail\n");
		goto exit;
	}

	/* BOOT OFF : wait completion, irq */
	cbd_log("BOOT OFF : wait completion, irq\n");
	ret = std_boot_dload_off(dl_args->std_args);
	if (ret < 0) {
		cbd_log("ERR! std_boot_dload_off fail\n");
		goto exit;
	}
	restore_srinfo(cbd_args->cpn->rat, dl_args->std_args->dev_fd);

exit:
	if (dl_args)
		close_boot_args(dl_args);

	boot_wake_lock(0);

	return ret;
}

//#define CBD_CPCRASH_HISTORY_LOG
#ifdef CBD_CPCRASH_HISTORY_LOG
/* save the cp crash dump history to /data/cp_log/cpcrash_history.txt */
static int open_crash_history_file(void)
{
	struct stat ldir_st;
	int ret;

	ret = stat("/data/cp_log", &ldir_st);
	if (!ret) { /* path exist */
		if (!S_ISDIR(ldir_st.st_mode)) {
			cbd_log("(%s) is not a directory\n", "/data/cp_log");
			goto exit;
		}
	} else {
		ret = mkdir("/data/cp_log", 0755);
		if (ret) {
			cbd_log("log path create fail(%d)\n", ret);
			goto exit;
		}
		cbd_log("log path (%s) created\n", "/data/cp_log");
	}
	return open("/data/cp_log/cpcrash_history.txt",
		O_WRONLY | O_APPEND | O_CREAT,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
exit:
	return ret;
}
#else
static inline int open_crash_history_file(void)
{
	return -1;
}
#endif

int start_shannon310_dump(struct boot_args *cbd_args)
{
	int ret;
	int log_fd = -1, history_fd = -1;
	struct std_dump_args *ul_args;
	struct shannon_boot_args *dl_args = NULL;
	char reason[MAX_PREFIX_LEN];

	boot_wake_lock(1);

	/*
	** Save kernel log
	*/
	snprintf(reason, MAX_PREFIX_LEN, "%s_crash", cbd_args->cpn->rat);
	save_logs(LOGB_DMESG, reason);

	history_fd = open_crash_history_file();
	if (history_fd < 0) {
		cbd_log("cp dump history file open fail\n");
	} else {
		char timestr[MAX_SUFFIX_LEN];
		time_t now;
		struct tm result;
		char serial[PROPERTY_VALUE_MAX];

		property_get(PROP_SERIAL_NO, serial, 0);
		time(&now);
		localtime_r(&now, &result);
		strftime(timestr, MAX_SUFFIX_LEN, "%Y%m%d-%H%M", &result);
		dprintf(history_fd, "***** %s (%s)*****\n\r", timestr, serial);
	}

	/*
	** Prepare CP CRASH DUMP
	*/
	cbd_log("Prepare CP Crash DUMP\n");
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
	cbd_log("Start CP BOOT for CRASH DUMP\n");
	dl_args = prepare_boot_args(cbd_args, CP_BOOT_MODE_DUMP);
	if (!dl_args) {
		cbd_log("ERR! prepare_boot_args fail\n");
		ret = -EFAULT;
		goto exit;
	}

	/* Save srinfo from shmem to file */
	store_srinfo(cbd_args->cpn->rat, dl_args->std_args->dev_fd);

	cbd_log("Shared memory full dump\n");
	ret = shmem_full_dump(ul_args);
	if (ret < 0) {
		cbd_log("ERR! shmem_full_dump fail\n");
	}

	ret = databuf_full_dump(ul_args);
	if (ret < 0) {
		cbd_log("ERR! shmem_full_dump fail\n");
	}

	ret = vss_full_dump(ul_args);
	if (ret < 0) {
		cbd_log("ERR! vss_full_dump fail\n");
	}

	ret = acpm_full_dump(ul_args);
	if (ret < 0) {
		cbd_log("ERR! acpm_full_dump fail\n");
	}

	ret = cplog_full_dump(ul_args);
	if (ret < 0) {
		cbd_log("ERR! cplog_full_dump fail\n");
	}

	/* Reset CP for CRASH DUMP */
	cbd_log("Reset CP for CRASH DUMP\n");
	ret = std_boot_modem_reset(dl_args->std_args);
	if (ret < 0) {
		cbd_log("ERR! std_dump_modem_reset fail\n");
		goto exit;
	}

	/* Load BOOT loader */
	cbd_log("Load BOOT loader\n");
	ret = shannon310_boot_xmit(dl_args->std_args, CP_BOOT_MODE_DUMP);
	if (ret < 0) {
		cbd_log("ERR! shannon310_boot_xmit fail\n");
		goto exit;
	}

	/* Request Security : secure mode */
	cbd_log("Request Security : dump mode\n");
	ret = std_security_req(dl_args->std_args, CP_BOOT_MODE_DUMP,
			       dl_args->std_args->dl_ctrl[IMG_BOOT].b_size,
			       dl_args->std_args->dl_ctrl[IMG_MAIN].b_size);
	if (ret < 0) {
		cbd_log("ERR! security check fail\n");
		goto exit;
	}

	/* DUMP START */
	cbd_log("DUMP START\n");
	ret = std_dump_uload_start(dl_args->std_args);
	if (ret < 0) {
		cbd_log("ERR! std_dump_uload_start fail\n");
		goto exit;
	}

	/*
	** Receive CP CRASH DUMP
	*/
	cbd_log("Receive CP CRASH DUMP\n");
	ret = std_dump_uload(ul_args);
	if (ret < 0) {
		cbd_log("ERR! std_dump_uload fail\n");
		dprintf(log_fd, "%s: ERR! std_dump_uload fail\n", __func__);
		goto exit;
	}
	if (history_fd >= 0) {
		ret = write(history_fd, ul_args->reason, 64);
		if (ret)
			cbd_log("write crash info to history file\n");
		dprintf(history_fd, "--\n\r");
		fsync(history_fd);
		system("ls -al /sdcard/log/cpcrash_dump_* >> /data/cp_log/cpcrash_filelist.txt");
	}

	/*
	** Finalize "CP Crash DUMP" and trigger kernel panic
	*/
	sleep(30);
	ret = std_dump_finalize(ul_args);
	if (ret < 0) {
		cbd_log("ERR! std_dump_finalize fail\n");
		dprintf(log_fd, "%s: ERR! std_dump_finalize fail\n", __func__);
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

	if (history_fd >= 0) {
		dprintf(history_fd, "***** dump fail *****\n");
		close(history_fd);
	}

	boot_wake_lock(0);

	return ret;
}

int start_shannon310_dummy_dump(struct boot_args *cbd_args)
{
	/* Do nothing */
	return 0;
}

int shutdown_shannon310_modem(struct boot_args *cbd_args)
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

int upload_shannon310_modem(struct boot_args *cbd_args)
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
