#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <ctype.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <cutils/properties.h>
#include <sched.h>

#include "boot.h"
#include "std_boot.h"
#include "shannon.h"
#include "util.h"
#ifdef CONFIG_SEC_CP_SECURE_BOOT
#include "cp_protect.h"
#endif

#define WAIT_POLL_TIME		30000	/* 30secs */

static struct std_boot_args std_boot;
static struct std_dump_args std_dump;
static char path[MAX_PATH_LEN];

#if 1
/* Common static functions for both STD_BOOT and STD_DUMP */
#endif

static int std_reboot_system(char *str)
{
	int ret;
	char reboot[PROPERTY_VALUE_MAX] = "reboot,";

	strcat(reboot, str);
	ret = property_set(PROP_SYS_POWERCTL, reboot);
	if (ret < 0)
		cbd_log("ERR! failed to setprop [sys.powerctl](%s)\n",
				reboot);
	return ret;
}

static unsigned char check_csc_sales_code()
{
	char secure_reboot[PROPERTY_VALUE_MAX];

	property_get(PROP_SALES_CODE, secure_reboot, "");

	cbd_log("sales_code:%s\n", secure_reboot);

	if (strcmp(secure_reboot, "TMB") == 0)
		return 1;
	else if (strcmp(secure_reboot, "VZW") == 0)
		return 1;

	return 0;
}

static int std_udl_poll(int fd, short events, long timeout)
{
	int ret;
	struct pollfd pfd;

	pfd.fd = fd;
	pfd.events = events;
	while (1) {
		pfd.revents = 0;

		/* Wait "events" up to "timeout" msec */
		ret = poll(&pfd, 1, timeout);
		if (pfd.revents & events)
			break;

		if (ret > 0)
			cbd_log("ERR! poll fail (events 0x%X != revents 0x%X)\n", events, pfd.revents);
		else if (ret == 0)
			cbd_log("ERR! poll fail (events 0x%X, TIMEOUT)\n", events);
		else
			cbd_log("ERR! poll fail (events 0x%X, %s)\n", events, ERR2STR);

		ret = -EIO;
		goto exit;
	}

	return 0;

exit:
	return ret;
}

static int std_udl_req_resp(int fd, u32 req, u32 exp)
{
	int ret = 0;
	u32 resp;

	/* Send a request to CP if exists */
	if (req) {
		cbd_log("request:0x%08X\n", req);
		ret = write(fd, &req, sizeof(u32));
		if (ret < 0) {
			cbd_log("ERR! write fail (%s)\n", ERR2STR);
			goto exit;
		}
	}

	/* Receive and verify a response from CP if expected */
	if (exp) {
		cbd_log("expected:0x%08X\n", exp);
		/* Wait for a response from CP up to WAIT_POLL_TIME ms */
		ret = std_udl_poll(fd, POLLIN, WAIT_POLL_TIME);
		if (ret < 0) {
			cbd_log("ERR! std_udl_poll fail\n");
			goto exit;
		}

		/* Receive a response from CP */
		ret = read(fd, &resp, sizeof(u32));
		if (ret < 0) {
			cbd_log("ERR! read fail (%s)\n", ERR2STR);
			goto exit;
		}

		/* Verify the response */
		if (resp != exp) {
			cbd_log("ERR! resp 0x%X != exp 0x%X\n", resp, exp);
			ret = -EFAULT;
			goto exit;
		}
		else
			cbd_log("OK!! resp 0x%X == exp\n", resp);
	}

	return 0;

exit:
	return ret;
}

static int std_udl_stage_start(int fd, u32 stage)
{
	int ret;
	u32 req = STD_UDL_AP2CP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_STAGE_START;
	u32 exp = STD_UDL_CP2AP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_STAGE_START;

	/* Send a request to CP, then receive and check a response from CP */
	ret = std_udl_req_resp(fd, req, exp);
	if (ret < 0) {
		cbd_log("ERR! [stage %d] START fail (req:0x%X exp:0x%X)\n",
			stage, req, exp);
	}

	return ret;
}

static int std_udl_stage_done(int fd, u32 stage)
{
	int ret;
	u32 req = STD_UDL_AP2CP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_STAGE_DONE;
	u32 exp = STD_UDL_CP2AP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_STAGE_DONE;

	/* Send a request to CP, then receive and check a response from CP */
	ret = std_udl_req_resp(fd, req, exp);
	if (ret < 0) {
		cbd_log("ERR! [stage %d] DONE fail (req:0x%X exp:0x%X)\n",
			stage, req, exp);
	}

	return ret;
}

#if 1
/* Static functions for STD_BOOT */
#endif

static int std_dl_tx_frame(int fd, int b_fd, struct std_udl_frame *frm)
{
	int ret = 0;
	u32 frm_len = STD_UDL_HDR_LEN + frm->len;
	u32 rcvd;
	u32 sent;

	memset(frm->data, 0, STD_UDL_MSS);

	/* Read a segment of a CP binary */
	rcvd = ret = read(b_fd, frm->data, frm->len);
	if (ret < 0) {
		cbd_log("ERR! read fail (%s)\n", ERR2STR);
		goto exit;
	}

	if (rcvd != frm->len) {
		cbd_log("ERR! rcvd %d != frm->len %d\n", rcvd, frm->len);
		ret = -EFAULT;
		goto exit;
	}

	/* Send the CP binary segment */
	sent = ret = write(fd, frm, frm_len);
	if (ret < 0) {
		cbd_log("ERR! write fail (%s)\n", ERR2STR);
		goto exit;
	}

	if (sent != frm_len) {
		cbd_log("ERR! sent %d != frm_len %d\n", sent, frm_len);
		ret = -EFAULT;
		goto exit;
	}

	return 0;

exit:
	return ret;
}

static int std_dl_send_bin(struct std_boot_args *args, u32 stage, int b_fd,
			u32 b_offset, u32 size)
{
	int ret;
	int dev_fd = args->dev_fd;
	u32 rest = size;
	u32 exp = STD_UDL_CP2AP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_SEND;
	struct std_udl_frame *frm = &args->frame_buff;
	struct std_dload_info dl_info;

	memset(frm, 0, sizeof(struct std_udl_frame));

	/* Set a file pointer for a CP binary (MAIN, NV, etc.) */
	ret = lseek(b_fd, b_offset, SEEK_SET);
	if (ret < 0) {
		cbd_log("ERR! lseek fail (%s)\n", ERR2STR);
		goto exit;
	}

	/* Set DLOAD command */
	frm->cmd = STD_UDL_AP2CP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_SEND;
	cbd_log("cmd = 0x%x\n", frm->cmd);

	/* Calculate the number of frames to be trsnamitted to CP */
	frm->num_frames = (size / STD_UDL_MSS);
	if (size > (STD_UDL_MSS * frm->num_frames))
		frm->num_frames++;

	/* Print DLOAD information at each stage */
	cbd_log("stage = %d\n", stage);
	cbd_log("size = %d (0x%X)\n", size, size);
	cbd_log("mtu = %d\n", STD_UDL_MTU);
	cbd_log("mss = %d\n", STD_UDL_MSS);
	cbd_log("frames = %d\n", frm->num_frames);

	/* Set DLOAD information for works in kernel */
	dl_info.size = size;
	dl_info.mtu = STD_UDL_MTU;
	dl_info.num_frames = frm->num_frames;
	ioctl(dev_fd, IOCTL_MODEM_FW_UPDATE, &dl_info);

	/* Read and send the CP binary */
	while (rest > 0) {
		frm->curr_frame++;
		frm->len = (rest < STD_UDL_MSS) ? rest : STD_UDL_MSS;
#if 0
		cbd_log("curr_frame:%d len:%d\n", frm->curr_frame, frm->len);
#endif

		ret = std_dl_tx_frame(dev_fd, b_fd, frm);
		if (ret < 0) {
			cbd_log("ERR! std_dl_tx_frame fail\n");
			goto exit;
		}

		rest -= frm->len;
	}

	/* Receive and check a response from CP */
	ret = std_udl_req_resp(dev_fd, 0, exp);
	if (ret < 0) {
		cbd_log("ERR! std_udl_req_resp fail\n");
		goto exit;
	}

	return 0;

exit:
	return ret;
}

static int std_dl_send_crc(struct std_boot_args *args, u32 stage, u32 crc)
{
	int ret;
	int dev_fd = args->dev_fd;
	u32 exp = STD_UDL_CP2AP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_CRC;
	struct std_udl_crc_frame crc_frm;

	crc_frm.cmd = STD_UDL_AP2CP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_CRC;
	crc_frm.crc = crc;
	cbd_log("cmd = 0x%x, crc = 0x%x\n", crc_frm.cmd, crc_frm.crc);

	/* Send a CRC data */
	ret = write(dev_fd, &crc_frm, sizeof(struct std_udl_crc_frame));
	if (ret < 0) {
		cbd_log("ERR! write fail (%s)\n", ERR2STR);
		goto exit;
	}

	/* Receive and check a response from CP */
	ret = std_udl_req_resp(dev_fd, 0, exp);
	if (ret < 0) {
		cbd_log("ERR! std_udl_req_resp fail\n");
		goto exit;
	}

	return 0;

exit:
	return ret;
}

#if 1
/* Static functions for STD_DUMP */
#endif

int std_ul_rx_frame(struct std_dump_args *args, void *buff, u32 size)
{
	int ret;
	int dev_fd = args->dev_fd;
	int log_fd = args->log_fd;

	/* Wait for a DUMP frame from CP up to WAIT_POLL_TIME ms */
	ret = std_udl_poll(dev_fd, POLLIN, WAIT_POLL_TIME);
	if (ret < 0) {
		cbd_log("ERR! DUMP std_udl_poll fail\n");
		dprintf(log_fd, "%s: ERR! DUMP std_udl_poll fail (%s)\n",
			__func__, ERR2STR);
		goto exit;
	}

	/* Receive a DUMP frame from CP */
	ret = read(dev_fd, buff, size);
	if (ret < 0) {
		cbd_log("ERR! DUMP read fail (%s)\n", ERR2STR);
		dprintf(log_fd, "%s: ERR! DUMP read fail (%s)\n", __func__, ERR2STR);
		goto exit;
	}

exit:
	return ret;
}

static int std_ul_recv_info(struct std_dump_args *args)
{
	int ret;
	int log_fd = args->log_fd;
	int info_fd = args->info_fd;
	struct std_uload_info *ul_info = &args->info;
	char *buff;
	int status;
	struct crash_reason reason;
	char string_crash_reason[][5] = { "RIL", "USER", "MIF", "CP" };

	/* Receive DUMP information */
	ret = std_ul_rx_frame(args, ul_info, sizeof(struct std_uload_info));
	if (ret < 0) {
		cbd_log("ERR! INFO std_ul_rx_frame fail\n");
		dprintf(log_fd, "%s: ERR! INFO std_ul_rx_frame fail\n", __func__);
		goto exit;
	}

	/* Print DUMP information */
	cbd_log("dump_size = %d\n", ul_info->dump_size);
	cbd_log("num_steps = %d\n", ul_info->num_steps);
	cbd_log("reason_len = %d\n", ul_info->reason_len);

	/* Receive CP CRASH reason  */
	buff = args->reason + strlen(args->reason);
	ret = std_ul_rx_frame(args, buff, ul_info->reason_len);
	if (ret < 0) {
		cbd_log("ERR! REASON std_ul_rx_frame fail\n");
		dprintf(log_fd, "%s: ERR! REASON std_ul_rx_frame fail\n", __func__);
		goto exit;
	}

	/* In watchdog case, reason str should be set by AP */
	status = ioctl(args->dev_fd, IOCTL_MODEM_STATUS);
	if (status == STATE_CRASH_WATCHDOG)
		strcpy(buff, STD_WDT_RESET_STR);

	/* Get crash reason for crash_by_AP or crash_by_RIL */
	ret = ioctl(args->dev_fd, IOCTL_MODEM_CRASH_REASON, &reason);
	if (ret < 0) {
		cbd_log("ERR! IOCTL_MODEM_CRASH_REASON fail (%s)\n", ERR2STR);
	}
	else {
		if (reason.owner) {
			if (reason.owner <= CRASH_REASON_RIL_RSV_MAX) {
				ret = 0;
			} else if (reason.owner == CRASH_REASON_USER) {
				ret = 1;
			} else if (reason.owner <= CRASH_REASON_MIF_RSV_MAX) {
				ret = 2;
			} else {
				ret = 3;
			}
			sprintf(path, ": Crash by %s - ", string_crash_reason[ret]);
			strcpy(args->reason, path);
			buff = args->reason + strlen(args->reason);
			strcpy(buff, reason.string);
		}
	}

	/* Store the CP CRASH reason */
	ret = write(info_fd, args->reason, STD_CRASH_REASON_SIZE);
	if (ret < 0) {
		cbd_log("ERR! REASON write fail (%s)\n", ERR2STR);
		dprintf(log_fd, "%s: ERR! REASON write fail (%s)\n", __func__, ERR2STR);
		goto exit;
	}
	cbd_log("CP CRASH %s\n", args->reason);

	if (fsync(info_fd)) {
		cbd_log("ERR! fsync(info_fd) fail (%s)\n", ERR2STR);
		dprintf(log_fd, "%s: ERR! fsync(info_fd) fail (%s)\n",
			__func__, ERR2STR);
		goto exit;
	}
	cbd_log("DUMP INFO saved\n");
	return 0;

exit:
	return ret;
}

static int std_ul_recv_data(struct std_dump_args *args, u32 step)
{
	int ret;
	int saved;
	int dev_fd = args->dev_fd;
	int log_fd = args->log_fd;
	int dump_fd = args->dump_fd;
	struct std_udl_frame *frm = &args->frame_buff;
	u32 req = STD_UDL_AP2CP | (STD_UDL_DUMP_STAGE << STD_UDL_STAGE_SHIFT) | step;
	u32 exp = STD_UDL_CP2AP | (STD_UDL_DUMP_STAGE << STD_UDL_STAGE_SHIFT) | step;
	u32 seqn;

	memset(frm, 0, sizeof(struct std_udl_frame));

	/*
	** Send "START of each step" command to CP
	*/
	ret = std_udl_req_resp(dev_fd, req, 0);
	if (ret < 0) {
		cbd_log("ERR! [step %d] start fail (ret %d)\n",
			step, ret);
		dprintf(log_fd, "%s: ERR! [step %d] start fail (ret %d)\n",
			__func__, step, ret);
		goto exit;
	}

	/*
	** Receive DUMP frames of each step from CP and store them
	*/
	seqn = 1;
	saved = 0;
	do {
		/* Receive a DUMP frame from CP */
		ret = std_ul_rx_frame(args, frm, sizeof(struct std_udl_frame));
		if (ret < 0) {
			cbd_log("ERR! [step %d] std_ul_rx_frame fail (ret %d)\n",
				step, ret);
			dprintf(log_fd, "%s: ERR! [step %d] std_ul_rx_frame fail (ret %d)\n",
				__func__, step, ret);
			goto exit;
		}

		/* Verify the command in the frame */
		if (frm->cmd != exp) {
			cbd_log("ERR! [step %d] cmd 0x%X != exp 0x%X\n",
				step, frm->cmd, exp);
			dprintf(log_fd, "%s: ERR! [step %d] cmd 0x%X != exp 0x%X\n",
				__func__, step, frm->cmd, exp);
			ret = -EFAULT;
			goto exit;
		}

		/* Verify the sequence number in the frame */
		if (frm->curr_frame != seqn) {
			cbd_log("ERR! [step %d] curr_frame %d != seqn %d\n",
				step, frm->curr_frame, seqn);
			dprintf(log_fd, "%s: ERR! [step %d] curr_frame %d != seqn %d\n",
				__func__, step, frm->curr_frame, seqn);
			goto exit;
		}
		seqn++;

		/* Record the information of each step at the start of the step */
		if (frm->curr_frame == 1) {
			cbd_log("[step %d] num_frames = %d\n", step, frm->num_frames);
			cbd_log("[step %d] command = 0x%X\n", step, frm->cmd);
			dprintf(log_fd, "%s: [step %d] num_frames = %d\n",
				__func__, step, frm->num_frames);
			dprintf(log_fd, "%s: [step %d] command = 0x%X\n",
				__func__, step, frm->cmd);
		}

		/* Store the DUMP data in the frame */
		ret = write(dump_fd, frm->data, frm->len);
		if (ret < 0) {
			cbd_log("ERR! [step %d] seq# %d write fail (%s)\n",
				step, frm->curr_frame, ERR2STR);
			dprintf(log_fd, "%s: ERR! [step %d] seq# %d write fail (%s)\n",
				__func__, step, frm->curr_frame, ERR2STR);
			goto exit;
		}

		/* Update "saved" variable */
		saved += frm->len;
	} while (frm->curr_frame < frm->num_frames);

	cbd_log("[step %d] saved = %d\n", step, saved);
	return saved;

exit:
	return ret;
}

#if 1
/* Functions for STD_BOOT */
#endif

struct std_boot_args *std_boot_prepare_args(struct boot_args *cbd_args, u32 num_stages)
{
	int dev_fd;
	struct modem_comp *cpn = cbd_args->cpn;
	struct std_boot_args *args = &std_boot;
#ifdef CONFIG_SEC_CP_SECURE_BOOT
	int ret;
#endif

	memset(args, 0, sizeof(struct std_boot_args));

	/* Open the boot device */
	dev_fd = open(cpn->node_boot, O_RDWR);
	if (dev_fd < 0) {
		cbd_log("ERR! DEV(%s) open fail (%s)\n", cpn->node_boot, ERR2STR);
		goto exit;
	}
	cbd_log("DEV(%s) opened (fd %d)\n", cpn->node_boot, dev_fd);

	args->cbd_args = cbd_args;
	args->dev_fd = dev_fd;
	args->num_stages = num_stages;

#ifdef CONFIG_SEC_CP_SECURE_BOOT
	ret = sec_cp_init();
	if (ret) {
		cbd_log("ERR! sec_cp_init fail (err %d)\n", ret);
		goto exit;
	}
	cbd_log("SECURE_BOOT initialized\n");
#endif

	return args;

exit:
	return NULL;
}

void std_boot_close_args(struct std_boot_args *args)
{
	if (args) {
		if (args->dev_fd >= 0)
			close(args->dev_fd);

		memset(args, 0, sizeof(struct std_boot_args));
	}
}

int std_boot_load_loader(struct std_boot_args *args, u32 stage)
{
	int ret = 0;
	int dev_fd = args->dev_fd;
	struct std_dload_control *dlc = &args->dl_ctrl[stage];
	struct modem_firmware img;

	cbd_log("size = %d\n", dlc->b_size);

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

#ifdef CONFIG_SEC_CP_SECURE_BOOT
	if (dlc->validate) {
		struct shdmem_info mem_info;

		ioctl(dev_fd, IOCTL_MODEM_GET_SHMEM_INFO, &mem_info);
		cbd_log("SHMEM base:0x%08x size:%d\n", mem_info.base, mem_info.size);

		/* Validate CP bootloader */
		ret = sec_cp_validate_boot(mem_info.base, mem_info.size, mem_info.base, dlc->b_size);
		if (ret) {
			cbd_log("ERR! BOOT validation fail (err %d)\n", ret);
			goto exit;
		}
		cbd_log("BOOT validated\n");
	}
#endif

	cbd_log("xmit bootloader complete!\n");
exit:
	if (img.binary)
		free(img.binary);

	return ret;
}

int std_boot_modem_on(struct std_boot_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;

	ret = ioctl(dev_fd, IOCTL_MODEM_ON, NULL);
	if (ret < 0) {
		cbd_log("ERR! IOCTL_MODEM_ON fail\n");
		goto exit;
	}

	return 0;

exit:
	return ret;
}

int std_boot_modem_off(struct std_boot_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;

	ret = ioctl(dev_fd, IOCTL_MODEM_OFF, NULL);
	if (ret < 0) {
		cbd_log("ERR! IOCTL_MODEM_OFF fail\n");
		goto exit;
	}

	return 0;

exit:
	return ret;
}

int std_boot_modem_reset(struct std_boot_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;

	ret = ioctl(dev_fd, IOCTL_MODEM_RESET, NULL);
	if (ret < 0) {
		cbd_log("ERR! IOCTL_MODEM_RESET fail\n");
		goto exit;
	}

	return 0;

exit:
	return ret;
}

int std_boot_xmit(struct std_boot_args *args, enum cp_boot_mode mode)
{
	int ret = 0;
	u32 stage;

	for ( stage = 0; stage < args->num_stages; stage++ ) {
		ret = std_boot_xmit_bin(args, stage, mode);
		if(ret < 0) {
			cbd_log("ERR! std_boot_xmit_bin stage[%u] fail\n", stage);
			return ret;
		}
	}
	return 0;
}

int std_boot_xmit_bin(struct std_boot_args *args, u32 stage, enum cp_boot_mode mode)
{
	int ret = 0;
	int last = 0;
	int dev_fd = args->dev_fd;
	struct std_dload_control *dlc = &args->dl_ctrl[stage];
	struct modem_firmware img;
	unsigned total = 0;

	/* Prepare an image buffer */
	img.binary = malloc(EXYNOS_PAYLOAD_LEN);
	if (!img.binary) {
		cbd_log("ERR! malloc(%d) fail\n", dlc->b_size);
		ret = -ENOMEM;
		goto exit;
	}

	img.size = dlc->b_size;
	img.m_offset = dlc->m_offset;
	img.b_offset = dlc->b_offset;
	img.mode = mode;
	img.len = EXYNOS_PAYLOAD_LEN;

	cbd_log("stage=%u(%u), b_off=%u, m_off=%u, b_size=%u, mode=%u\n",
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

		ret = read(dlc->b_fd, img.binary, img.len);
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
		if (ret < 0) {
			cbd_log("ERR! IOCTL_MODEM_XMIT_BOOT fail (%u)\n", stage);
			goto exit;
		}

		if(last == 1)
			break;

		total += img.len;
		img.m_offset += img.len;
	}
	cbd_log("%u stage complelte\n", stage);
exit:
	if (img.binary)
		free(img.binary);

	return ret;
}

int std_boot_finish_handshake(struct std_boot_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;

	/* BOOT_DONE command */
	ret = std_udl_stage_done(dev_fd, STD_UDL_STAGE_START);
	if (ret < 0) {
		cbd_log("ERR! std_udl_stage_done fail\n");
		goto exit;
	}

	/* FIN command */
	ret = std_udl_stage_start(dev_fd, STD_UDL_FIN_STAGE);
	if (ret < 0) {
		cbd_log("ERR! std_udl_stage_done fail\n");
		goto exit;
	}

	return 0;
exit:
	return ret;
}

int std_boot_dload_on(struct std_boot_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;

	ret = ioctl(dev_fd, IOCTL_MODEM_BOOT_ON, NULL);
	if (ret < 0) {
		cbd_log("ERR! IOCTL_MODEM_BOOT_ON fail\n");
		goto exit;
	}

	return 0;

exit:
	return ret;
}

int std_boot_dload_start(struct std_boot_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;

	ret = ioctl(dev_fd, IOCTL_MODEM_DL_START, NULL);
	if (ret < 0) {
		cbd_log("ERR! IOCTL_MODEM_DL_START fail\n");
		goto exit;
	}

	return 0;

exit:
	return ret;
}

int std_boot_dload(struct std_boot_args *args)
{
	int ret = 0;
	int max_stages = args->num_stages;
	int i;
	int start_stage;

	if (args->cbd_args->lnk_boot == LINKDEV_SPI &&
			args->cbd_args->lnk_main == LINKDEV_PCIE)
		/* BOOT was already downloaded by SPI link */
		start_stage = TOC_STAGE;
	else
		start_stage = BOOT_STAGE;

	/* {BOOT(?), TOC, MAIN, NV, ... , FIN} stages */
	for (i = start_stage; i < max_stages; i++) {
		int dev_fd = args->dev_fd;
		struct std_dload_control *dlc = &args->dl_ctrl[i];
		u32 stage = dlc->stage;

		if (dlc->start) {
			ret = std_udl_stage_start(dev_fd, stage);

			if (ret < 0) {
				cbd_log("ERR! [%d] std_udl_stage_start fail\n", stage);
				goto exit;
			}
		}

		if (dlc->download) {
			ret = std_dl_send_bin(args, stage, dlc->b_fd, dlc->b_offset, dlc->b_size);
			if (ret < 0) {
				cbd_log("ERR! [%d] std_dl_send_bin fail\n", stage);
				goto exit;
			}
		}

		if (dlc->download && dlc->validate) {
#ifdef CONFIG_SEC_CP_SECURE_BOOT
			if (args->cbd_args->lnk_boot == LINKDEV_SHMEM) {
				struct shdmem_info mem_info;
				u32 magic_base;
				u32 dl_base;
				u32 dl_size;

				ioctl(dev_fd, IOCTL_MODEM_GET_SHMEM_INFO, &mem_info);
				magic_base = mem_info.base + CP_FIRM_MAGIC_OFFSET;
				dl_base = mem_info.base + CP_FIRM_DL_OFFSET;
				dl_size = dlc->b_size;
				cbd_log("magic@0x%08x dload@0x%08x size:%d\n",
					magic_base, dl_base, dl_size);

				/* Validate CP MAIN binary */
				ret = sec_cp_validate_main(magic_base, dl_base, dl_size);
				if (ret) {
					cbd_log("ERR! [%d] binary validation fail\n",
						stage);
					goto exit;
				}
				cbd_log("[%d] binary validated\n", stage);
			}
#else
			ret = std_dl_send_crc(args, stage, dlc->crc);
			if (ret < 0) {
				if (check_csc_sales_code()) {
					cbd_log("secure err: Invalid Main image\n");
					std_reboot_system("secure");
				} else {
					cbd_log("ERR! [%d] std_dl_send_crc fail\n", stage);
				}

				goto exit;
			}
#endif
		}

		if (dlc->finish) {
			ret = std_udl_stage_done(dev_fd, stage);
			if (ret < 0) {
				cbd_log("ERR! [%d] std_udl_stage_done fail\n", stage);
				goto exit;
			}
		}
	}

	return 0;

exit:
	return ret;
}

int std_boot_dload_off(struct std_boot_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;

	ret = ioctl(dev_fd, IOCTL_MODEM_BOOT_OFF, NULL);
	if (ret < 0) {
		cbd_log("ERR! IOCTL_MODEM_BOOT_OFF fail\n");
		ioctl(dev_fd, IOCTL_MODEM_GET_CP_BOOTLOG, NULL);
		goto exit;
	}

	ioctl(dev_fd, IOCTL_MODEM_CLR_CP_BOOTLOG, NULL);

	return 0;

exit:
	return ret;
}

int std_boot_finalize(struct std_boot_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;

	ret = ioctl(dev_fd, IOCTL_MODEM_BOOT_DONE, NULL);
	if (ret < 0) {
		cbd_log("ERR! IOCTL_MODEM_BOOT_DONE fail\n");
		goto exit;
	}

	return 0;

exit:
	return ret;
}

#if 1
/* Functions for STD_DUMP */
#endif

struct std_dump_args *std_dump_prepare_args(struct boot_args *cbd_args)
{
	int i;
	int len;
	char *log_root = get_log_root();
	char *log_dir = get_log_dir();
	struct modem_comp *cpn = cbd_args->cpn;
	struct std_dump_args *args = &std_dump;
	int dev_fd = -1;
	int log_fd = -1;
	int info_fd = -1;
	int dump_fd = -1;
	char prefix[MAX_PREFIX_LEN];
	char suffix[MAX_SUFFIX_LEN];
	time_t now;
	struct tm result;

	if (create_log_directory(log_dir) < 0)
		goto exit;

	if (check_fs_free_space(log_root) < 0) {
		remove_logs(LOG_DMESG, log_dir, cpn->rat);
		if (check_fs_free_space(log_root) < 0)
			goto exit;
	}

	memset(args, 0, sizeof(struct std_dump_args));

	len = strlen(cpn->rat);
	for (i = 0; i < len; i++)
		args->reason[i] = toupper(cpn->rat[i]);
	strcat(args->reason, ": ");

	/* Open the DUMP device */
	dev_fd = open(cpn->node_dump, O_RDWR);
	if (dev_fd < 0) {
		cbd_log("ERR! %s open fail (%s)\n", cpn->node_dump, ERR2STR);
		goto exit;
	}
	cbd_log("%s opened (fd %d)\n", cpn->node_dump, dev_fd);
	args->dev_fd = dev_fd;

	/* Set prefix and suffix for DUMP file paths */
	snprintf(prefix, MAX_PREFIX_LEN, "cpcrash_%s", cpn->rat);
	time(&now);
	localtime_r(&now, &result);
	strftime(suffix, MAX_SUFFIX_LEN, "%Y%m%d-%H%M", &result);

	/* Open (create) a CP crash log file */
	sprintf(path, "%s/%s_log_%s.log", log_dir, prefix, suffix);

	log_fd = open(path, O_WRONLY | O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (log_fd < 0) {
		cbd_log("ERR! %s open fail (%s)\n", path, ERR2STR);
		goto exit;
	}
	fchmod(log_fd, 0664);
	cbd_log("%s opened (fd %d)\n", path, log_fd);
	dprintf(log_fd, "%s: %s opened (fd %d)\n", __func__, path, log_fd);
	args->log_fd = log_fd;

	/* Open (create) a CP crash info file */
	sprintf(path, "%s/%s_info_%s_%s.log", log_dir, prefix, cpn->name, suffix);
	info_fd = open(path, O_WRONLY | O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (info_fd < 0) {
		cbd_log("ERR! %s open fail (%s)\n", path, ERR2STR);
		dprintf(log_fd, "%s: ERR! %s open fail (%s)\n", __func__, path, ERR2STR);
		goto exit;
	}
	fchmod(info_fd, 0664);
	cbd_log("%s opened (fd %d)\n", path, info_fd);
	dprintf(log_fd, "%s: %s opened (fd %d)\n", __func__, path, info_fd);
	args->info_fd = info_fd;

	/* Open (create) a CP crash dump file */
	sprintf(path, "%s/%s_dump_%s.log", log_dir, prefix, suffix);
	dump_fd = open(path, O_WRONLY | O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (dump_fd < 0) {
		cbd_log("ERR! %s open fail (%s)\n", path, ERR2STR);
		dprintf(log_fd, "%s: ERR! %s open fail (%s)\n", __func__, path, ERR2STR);
		goto exit;
	}
	fchmod(dump_fd, 0664);
	cbd_log("%s opened (fd %d)\n", path, dump_fd);
	dprintf(log_fd, "%s: %s opened (fd %d)\n", __func__, path, dump_fd);
	args->dump_fd = dump_fd;

	/* Set standard DUMP arguments */
	args->cbd_args = cbd_args;
	return args;

exit:
	return NULL;
}

void std_dump_close_args(struct std_dump_args *args)
{
	if (args) {
		if (args->dev_fd >= 0)
			close(args->dev_fd);

		if (args->info_fd >= 0)
			close(args->info_fd);

		if (args->dump_fd >= 0)
			close(args->dump_fd);

		if (args->log_fd >= 0)
			close(args->log_fd);

		memset(args, 0, sizeof(struct std_dump_args));
	}
}

int std_dump_modem_reset(struct std_boot_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;

	/* Reset CP as DUMP mode */
	ret = ioctl(dev_fd, IOCTL_MODEM_DUMP_RESET, NULL);
	if (ret < 0) {
		cbd_log("ERR! IOCTL_MODEM_DUMP_RESET fail\n");
		goto exit;
	}

	return 0;

exit:
	return ret;
}

int std_dump_uload_start(struct std_boot_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;

	/* Set DUMP (upload) magic key */
	ret = ioctl(dev_fd, IOCTL_MODEM_DUMP_START, NULL);
	if (ret < 0) {
		cbd_log("IOCTL_MODEM_DUMP_START fail\n");
		goto exit;
	}

	return 0;

exit:
	return ret;
}

int std_dump_uload(struct std_dump_args *args)
{
	int ret;
	int dev_fd = args->dev_fd;
	int log_fd = args->log_fd;
	int dump_fd = args->dump_fd;
	struct std_uload_info *ul_info;
	u32 saved = 0;
	u32 step;

	/* Send DUMP START request and wait for the response from CP */
	ret = std_udl_stage_start(dev_fd, STD_UDL_DUMP_STAGE);
	if (ret < 0) {
		cbd_log("ERR! std_udl_stage_start fail\n");
		dprintf(log_fd, "%s: ERR! std_udl_stage_start fail\n", __func__);
		goto exit;
	}

	/* Receive DUMP information and the CP CRASH reason */
	ret = std_ul_recv_info(args);
	if (ret < 0) {
		cbd_log("ERR! std_ul_recv_info fail\n");
		dprintf(log_fd, "%s: ERR! std_ul_recv_info fail\n", __func__);
		goto exit;
	}

	/* Receive DUMP data at every DUMP step */
	ul_info = &args->info;
	for (step = 1; step <= ul_info->num_steps; step++) {
		ret = std_ul_recv_data(args, step);
		if (ret < 0) {
			cbd_log("ERR! std_ul_recv_data fail\n");
			dprintf(log_fd, "%s: ERR! std_ul_recv_data fail\n", __func__);
			goto exit;
		}
		saved += ret;
	}

	/* Verify the size of total DUMP data */
	if (saved != ul_info->dump_size) {
		cbd_log("ERR! saved %d != dump_size %d\n", saved, ul_info->dump_size);
		dprintf(log_fd, "%s: ERR! saved %d != dump_size %d\n",
			__func__, saved, ul_info->dump_size);
		ret = -EFAULT;
		goto exit;
	}

	if (fsync(dump_fd)) {
		cbd_log("ERR! fsync(dump_fd) fail (%s)\n", ERR2STR);
		dprintf(log_fd, "%s: ERR! fsync(dump_fd) fail (%s)\n",
			__func__, ERR2STR);
		ret = errno;
		goto exit;
	}
	cbd_log("DUMP DATA saved\n");

	/* Send DUMP DONE request and wait for the response from CP */
	ret = std_udl_stage_done(dev_fd, STD_UDL_DUMP_STAGE);
	if (ret < 0) {
		cbd_log("ERR! DUMP std_udl_stage_done fail\n");
		dprintf(log_fd, "%s: ERR! DUMP std_udl_stage_done fail\n", __func__);
		goto exit;
	}

	return 0;

exit:
	return ret;
}

int std_dump_finalize(struct std_dump_args *args)
{
	int ret = -1;
	int dev_fd;
	int log_fd;

	if (!args)
		goto exit;

	/* close files */
	if (args->dump_fd >= 0) {
		fsync(args->dump_fd);
		close(args->dump_fd);
		args->dump_fd = -1;
	}

	if (args->info_fd >= 0) {
		fsync(args->info_fd);
		close(args->info_fd);
		args->info_fd = -1;
	}

	dev_fd = args->dev_fd;
	log_fd = args->log_fd;

	cbd_log("Go to UPLOAD mode\n");
	dprintf(log_fd, "%s: Go to UPLOAD mode\n", __func__);

	ret = ioctl(dev_fd, IOCTL_MODEM_CP_UPLOAD, args->reason);
	if (ret < 0) {
		cbd_log("ERR! IOCTL_MODEM_CP_UPLOAD fail (%s)\n", ERR2STR);
		dprintf(log_fd, "%s: ERR! IOCTL_MODEM_CP_UPLOAD fail (%s)\n",
			__func__, ERR2STR);
		goto exit;
	}

	if (fsync(log_fd)) {
		cbd_log("ERR! fsync(log_fd) fail (%s)\n", ERR2STR);
		dprintf(log_fd, "%s: ERR! fsync(log_fd) fail (%s)\n",
			__func__, ERR2STR);
		return -1;
	}
	return 0;

exit:
	return ret;
}

static int std_check_cp_secure_fail(u32 value)
{
	/*
	 * Only for ModAP model.
	 * CP Secure fail err code.
	 */
	u32 err_code[] = {
		0xFEED02,	/* Exynos3475 CP Boot */
		0xFEED04,	/* Exynos3475 CP Main */
		0xFEED0002,	/* Exynos7580, 8890 (EL3) */
		0x50E00,	/* 0xFEED0002 -> 0x50E00(from J-series) */
	};

	int count = sizeof(err_code) / sizeof(u32);

	while (count--) {
		if (value == err_code[count])
			return 1;
	}

	return 0;
}

int std_security_req(struct std_boot_args *args, u32 mode, u32 p2, u32 p3)
{
	int ret;
	int dev_fd = args->dev_fd;
	struct modem_sec_req msr;

	msr.mode = mode;
	msr.param2 = p2;
	msr.param3 = p3;
	msr.param4 = 0;

	cbd_log("security_req: %x:%x:%x:%x\n", 
		msr.mode, msr.param2, msr.param3, msr.param4);

	cpu_set_t cpu_set;
	sched_getaffinity(0, sizeof(cpu_set), &cpu_set);
	cbd_log("orig cpu_set[0]=0x%08lx\n", cpu_set.__bits[0]);
	CPU_CLR(0, &cpu_set);
	sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
	sched_getaffinity(0, sizeof(cpu_set), &cpu_set);
	cbd_log("new cpu_set[0]=0x%08lx\n", cpu_set.__bits[0]);

	ret = ioctl(dev_fd, IOCTL_SECURITY_REQ, &msr);
	if (ret != 0) {
		cbd_log("ERR! IOCTL_CHECK_SECURITY fail (%d)\n", ret);

		/* 11 (CP not working) is an expected when mode == CP_BOOT_RE_INIT */
		if (mode == CP_BOOT_RE_INIT && ret == 11) {
			ret = 0;
		} else {
			if (std_check_cp_secure_fail(ret)) {
				cbd_log("secure err: Invalid Main image\n");
				if (check_csc_sales_code())
					std_reboot_system("secure");
			}
			ret = -1;
		}
	}

	CPU_SET(0, &cpu_set);
	sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
	sched_getaffinity(0, sizeof(cpu_set), &cpu_set);
	cbd_log("restore cpu_set[0]=0x%08lx\n", cpu_set.__bits[0]);

	return ret;
}
