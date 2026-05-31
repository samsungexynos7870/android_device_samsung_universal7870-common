/*
 * cbp72 boot process
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
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <termios.h>
#include <stdarg.h>
#include <errno.h>
#include <cutils/properties.h>

#include "boot.h"
#include "cbp72.h"
#include "util.h"

static struct cp_imgmap default_table[IMG_MAX_IDX] = {
	[IMG_CBL] = {
		.name = "CBL",
		.bin_offset = 0x0,
		.size = 0x5000,
	},
	[IMG_MAIN] = {
		.name = "MAIN",
		.bin_offset = 0x5000,
		.size = 0x400000,
	},
	[IMG_FSM] = {
		.name = "FSM",
		.bin_offset = 0x00400000,
		.size = 0x4BAF,
	},
};

static char img_table[MAX_TOC_SIZE];
static char g_rx_buf[4096] = {0,};

/* Safe write and Safe read */
static ssize_t s_read(int fd, void *buf, size_t len)
{
	struct timeval tv;
	fd_set readfds;
	int err;

	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	memset(g_rx_buf, 0x00, 4096);

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
		break;
	}
	return err;
}

static ssize_t s_write(int fd, void *buf, size_t len)
{
	/* TODO: check the write size and if return was less than requset,
	 * repeat write the reset buf data*/
	return write(fd, buf, len);
}

static int dpram_send_wait_cmd(struct dpram_boot_frame *bf, int boot_fd,
	unsigned req, unsigned res)
{
	int err;

	cbd_log("send 0x%x, wait 0x%x\n", req, res);
	memset(bf, 0, sizeof(struct dpram_boot_frame));
	bf->req = req;
	bf->res = res;
	bf->len = 0;
	err = write(boot_fd, bf, sizeof(struct dpram_boot_frame));
	if (err < 0) {
		cbd_log("boot frame write fail (err %d)\n", err);
		cbd_log("frame debug req=%08x, res=%08x, len=%ld\n", req, res, bf->len);
		goto exit;
	}
exit:
	return err;
}

/* CP binary map update */
static int update_cp_imgmap(struct via_args *args)
{
	int err;
	char *img_tab = img_table; /* table buf */
	struct cp_imgmap *tocmap;

	if (args->bin_fd < 0) {
		cbd_log("invalid cp binary fd = %d\n", args->bin_fd);
		err = -EINVAL;
		goto exit;
	}
	/* backward compatibility for u1 cp binary, if it check teh PSIRAMN
	 * img name else skip update img table
	 */
	err = read(args->bin_fd, img_tab, MAX_TOC_SIZE);
	if (err < 0) {
		cbd_log("cp img table read fail (err %d)\n", err);
		goto exit;
	}

	tocmap = (struct cp_imgmap *)img_tab;
	if (!strcmp(tocmap->name, "BOOT")) {
		args->img_tab = (struct cp_imgmap *)img_tab;
		cbd_log("CP img map from cp binary\n");
	} else {
	args->img_tab = default_table;
	cbd_log("CP img map from default table\n");
	}

	print_data(img_tab, 16);
exit:
	return err;
}

/* Load cp binaries */
static int prepare_image(int fd, struct image_buf *img,
	struct cp_imgmap *cp_bin)
{
	int err;

	img->length = cp_bin->size;
	img->buf = (unsigned char *)malloc(cp_bin->size);
	if (!img->buf) {
		cbd_log("malloc fail\n");
		err = -ENOMEM;
		goto exit;
	}

	err = lseek(fd, cp_bin->bin_offset, SEEK_SET);
	if (err < 0) {
		cbd_log("lseek fail %d\n", err);
		free(img->buf);
		goto exit;
	}

	err = read(fd, img->buf, img->length);
	if (err < (int)img->length) {
		cbd_log("file read fail (err %d)\n", err);
		free(img->buf);
		err = -EFAULT;
		goto exit;
	}

	err = 0;
	cbd_log("Load %s img size=%d\n", cp_bin->name, img->length);
	print_data((char *)img->buf, 16);

exit:
	return err;
}

/* dpram send boot */
static int dpram_xmit_boot(struct via_args *args)
{

	/*TODO: copy the CP binary to CP MOR*/
	return 0;
}

static int create_default_fsm_data(char *filename, struct image_buf *img)
{
	int fsm_fd;
	int ret = 0;

	cbd_log("start!!!\n");

	fsm_fd = open(filename, O_RDWR|O_CREAT|O_TRUNC|O_SYNC, FSM_FILE_MODE);
	if (fsm_fd < 0) {
		cbd_log("create %s failed\n", filename);
		return -1;
	}
	cbd_log("=> create new fsm_data file(%s).\n", filename);

	ret = write(fsm_fd, img->buf, img->length);
	cbd_log("wrote %d byte to %s\n", ret, filename);

	fsync(fsm_fd);
	close(fsm_fd);

	return 0;
}
static int dpram_xmit_bin(struct dpram_boot_frame *bf,
		struct via_args *args, const char *buf, int size)
{
	struct dpram_dl_header header;
	int boot_fd = args->boot_fd;
	int rest;
	int err = 0;
	int blk_size, send_size = 0;
	unsigned short nframes;
	unsigned short curframe = 1;

	bf->len = 0; /*for cmd packet */
	bf->offset = 0;
	rest = size;
	nframes = (rest / MAX_PAYLOAD_SIZE) + ((rest % MAX_PAYLOAD_SIZE) ? 1 : 0);
	header.start_index = START_INDEX;
	header.nframes = nframes;

	while (rest) {
		char *dest;

		memset(bf, 0x00, sizeof(struct dpram_boot_frame));
		blk_size = rest > MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : rest;

		header.curframe = curframe;
		header.len = blk_size;

		memcpy(bf->data, &header, sizeof(header));
		memcpy(bf->data + sizeof(header), buf + send_size, blk_size);
		dest = bf->data + sizeof(header) + blk_size + 2;
		*dest = (char)0x7F;

		bf->len = blk_size + sizeof(header) + 3;
		bf->req = CBP72_IMG_DL_REQ;
		bf->res = CBP72_IMG_DL_RESP;

		//print_data(bf->data, 16);
		err = write(boot_fd, bf, sizeof(struct dpram_boot_frame));
		if (err < 0) {
			cbd_log("boot frame write fail err = %d\n", err);
		/*	cbd_log("frame debug req=%x, res=%x, len=%ld\n",
			bf->req, bf->res. bf->len);*/
			goto exit;
		}
		rest -= blk_size;
		send_size += blk_size;
		curframe++;
	}

	cbd_log("rest = %d, send_size = %d\n", rest, send_size);
exit:
	return err;

}
static int dpram_xmit_fsm(struct dpram_boot_frame *bf,
		struct via_args *args)
{
	struct image_buf img;
	struct stat file_info;

	int boot_fd = args->boot_fd;
	int fsm_fd = -1;
	char *fsm_data = NULL;
	int err;
	int spin = 5;
	char prop_buf[PROPERTY_VALUE_MAX] = {0, };

	cbd_log("check fsm\n");
	/* wait for RILD FSM Validity done*/
	while (spin--) {
		property_get(VPROP_RFS_CHECKDONE, prop_buf, "0");
		if (prop_buf[0] == '1')
			break;
		cbd_log("wait restore fsm file\n");
		usleep(500000);
	}
	cbd_log("rild check fsm %s\n", spin < 0  ? "timeout" : "done");

	/* Load fsm binary */
	memset(&img, 0x00, sizeof(struct image_buf));
	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_FSM]);
	if (err < 0) {
		cbd_log("FSM load fail (err %d)\n", err);
		goto exit;
	}
	/* open fsm data file */
	fsm_fd = open(FSM_DATA_PATH, O_RDWR | O_NDELAY);
	if (fsm_fd < 0) {
		cbd_log("FSM(%s) open fail (err %d)\n", FSM_DATA_PATH, fsm_fd);
		create_default_fsm_data(FSM_DATA_PATH, &img);
		fsm_fd = open(FSM_DATA_PATH, O_RDWR | O_NDELAY);
		if (fsm_fd < 0) {
			cbd_log("Default FSM fail (err %d)\n", fsm_fd);
			goto exit;
		}
	}
	/* start fsm binary - write magic code */
	cbd_log("Send FSM Body\n");
	/* send fsm binary */
	err = fstat(fsm_fd, &file_info);
	if (err < 0) {
		cbd_log("Can not get file info\n");
		goto exit;
	}
	fsm_data = (char *)malloc(MAX_FSMDATA_SIZE);
	err = read(fsm_fd, fsm_data, file_info.st_size);
	if (err < 0) {
		cbd_log("load cp bin fail (err %d)\n", err);
		goto exit;
	}
	print_data(fsm_data, 16);
	err = dpram_xmit_bin(bf, args, fsm_data, file_info.st_size);
	if (err < 0) {
		cbd_log("cp img xmit bit fail (err %d)\n", err);
		goto exit;
	}

	err = dpram_send_wait_cmd(bf, boot_fd, CBP72_IMG_DL_DONE,
			CBP72_IMG_DL_DONE_RESP);

	cbd_log("Send FSM BODY done: %dbytes\n", img.length);
exit:
	if (img.buf)
		free(img.buf);
	if (fsm_fd >= 0)
		close(fsm_fd);

	free(fsm_data);

	return err;
}

static int dpram_xmit_modem(struct dpram_boot_frame *bf, struct via_args *args)
{
	struct image_buf img;
	int boot_fd = args->boot_fd;
	int err;

	/* Load MAIN binary */
	memset(&img, 0x00, sizeof(struct image_buf));
	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_MAIN]);
	if (err < 0) {
		cbd_log("MAIN load fail (err %d)\n", err);
		goto exit;
	}

	/* Data bytes of EBL */
	/* start main binary - write magic code */
	cbd_log("Send MAIN Body\n");

	err = dpram_xmit_bin(bf, args, (char *)img.buf, img.length);
	if (err < 0) {
		cbd_log("cp img xmit bit fail (err %d)\n", err);
		goto exit;
	}

	err = dpram_send_wait_cmd(bf, boot_fd, CBP72_IMG_DL_DONE,
			CBP72_IMG_DL_DONE_RESP);
	if (err < 0) {
		cbd_log("send cmd fail (err %d)\n", err);
		goto exit;
	}

	cbd_log("Send MAIN BODY done: %dbytes\n", img.length);
exit:
	if (img.buf)
		free(img.buf);

	return err;
}

/*dpram send main */
static int dpram_xmit_main(struct via_args *args)
{
	int err;
	struct dpram_boot_frame *bf = NULL;
	int boot_fd = args->boot_fd;

	cbd_log("start sending cp main binary\n");

	err = ioctl(boot_fd, IOCTL_MODEM_DL_START);
	if (err < 0)
		cbd_log("IOCTL_MODEM_DL_START fail (err %d)\n", err);

	sleep(1);

	/* Alloc boot frame */
	bf = (struct dpram_boot_frame *)malloc(sizeof(struct dpram_boot_frame));
	if (!bf) {
		cbd_log("Binary buf alloc fail size = %ld\n",
			sizeof(struct dpram_boot_frame));
		err = -ENOMEM;
		goto exit;
	}

	err = dpram_xmit_modem(bf, args);
	if (err < 0) {
		cbd_log("MAIN send fail\n");
		goto exit;
	}

	/* Send  FSM */
	cbd_log("Send FSM ----\n");
	err = dpram_xmit_fsm(bf, args);
	if (err < 0) {
		cbd_log("FSM send fail\n");
		goto exit;
	}
exit:
	if(bf)
		free(bf);
	return err;
}

static int uart_xmit_loader(struct via_args *args)
{
	struct image_buf img;
	struct Msg_header msg_h;
	struct Header_Msg h_msg;
	struct Checksum_Msg c_msg;
	int err;
	static int fd = 0;
	unsigned int i;
	int block_size, rest_size, send_size = 0, Total_Checksum = 0, Verify_Checksum = 0;
	char data[258];

	/* intialize image_buf */
	memset(&img, 0, sizeof(struct image_buf));

	/* Uart Chennel open */
	if (!fd){
		fd = open_serial("/dev/ttySAC3", "115200", "N", "8", "1", 0, 0);
		if (fd < 0) {
			cbd_log( "Serial open failed (err %d)\n", fd);
			return fd;
		}
	} else
		cbd_log("using opened Serial\n");

	/* Handshake Msg, AP->BootROM */
	msg_h.Sync = 0xFE;
	msg_h.PacketLen = 0x1;
	msg_h.MsgId = BOOT_UARTMSG_HANDSHAKE;

	//sleep(1);
	/* send Handshake Msg */
	err = s_write(fd, &msg_h, sizeof(struct Msg_header));
	if (err < 0) {
		cbd_log("send msg header fail (err %d)\n", err);
		goto exit;
	}
	usleep(10000);
	print_data((char *)&msg_h, sizeof(struct Msg_header));

	/* Read ack */
	err = s_read(fd, g_rx_buf, 3);
	if (err < 0 || g_rx_buf[0] != 0xFE ||
			g_rx_buf[2] != BOOT_UARTMSG_ACK) {
		cbd_log("Handshake ack read fail: err = %d, 0x%02x, "
			"0x%02x, 0x%02x\n", err, g_rx_buf[0], g_rx_buf[1], g_rx_buf[2]);
		err = -EFAULT;
		goto exit;
	}
	usleep(10000);

	cbd_log("Success Handshake\n");
	/* Header Msg, PC->BootROM */
	h_msg.Header.Sync = 0xFE;
	h_msg.Header.PacketLen = 9;
	h_msg.Header.MsgId = BOOT_UARTMSG_HEADER;
	h_msg.LoadAddr = 0x00000000;
	h_msg.ExecuteAddr = 0x00000020;

	/* send Header Msg */
	err = s_write(fd, &h_msg, sizeof(struct Header_Msg));
	if (err < 0) {
		cbd_log("send header msg fail (err %d)\n", err);
		goto exit;
	}
	usleep(10000);
	print_data((char *)&h_msg, sizeof(struct Header_Msg));
	/* Read ack */
	err = s_read(fd, g_rx_buf, 3);
	if (err < 0 || g_rx_buf[0] != 0xFE ||
			g_rx_buf[2] != BOOT_UARTMSG_ACK) {
		cbd_log("Handshake ack read fail: err = %d, 0x%02x, 0x%02x, 0x%02x\n",
			err, g_rx_buf[0], g_rx_buf[1], g_rx_buf[2]);
		err = -EFAULT;
		goto exit;
	}
	usleep(10000);

	cbd_log("Success sending Header Msg\n");
	/* Load bootloader bin */
	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_CBL]);
	if (err < 0) {
		cbd_log("CBL load fail (err %d)\n", err);
		goto exit;
	}
	/*Data Msg, PC->BootROM */
	cbd_log("Send CBL Body\n");
	rest_size = img.length;

	while(rest_size) {
		block_size = rest_size > 254 ? 254 : rest_size;
		msg_h.Sync = 0xFE;
		msg_h.PacketLen = block_size + 1;
		msg_h.MsgId = BOOT_UARTMSG_DATA;

		/* send header + img data */
		memcpy(data, &msg_h, sizeof(struct Msg_header));
		memcpy(data + sizeof(struct Msg_header), (char *)img.buf + send_size, block_size);

		err = s_write(fd, data, sizeof(struct Msg_header) + block_size);
		if (err < 0) {
			cbd_log("send data  msg fail (err %d)\n", err);
			goto exit;
		}
		usleep(10000);
		/* Read ack */
		err = s_read(fd, g_rx_buf, 3);
		if (err < 0 || g_rx_buf[0] != 0xFE || g_rx_buf[2] != BOOT_UARTMSG_ACK) {
			cbd_log("Handshake ack read fail: err = %d, 0x%02x, 0x%02x, 0x%02x\n",
				err, g_rx_buf[0], g_rx_buf[1], g_rx_buf[2]);
			err = -EFAULT;
			goto exit;
		}
		usleep(10000);
		rest_size -= block_size;
		send_size += block_size;
	}
	cbd_log("rest_size = %x, send_size = %d\n", rest_size, send_size);
	cbd_log("Send CBL done: %dbytes\n", img.length);

	/* Checksum Msg, PC->BootROM */
	for ( i = 0; i< img.length; i++){
		Total_Checksum += *((char *)img.buf + i);
	};
	for ( i = 0; i< 10; i++){
		Verify_Checksum += *((char *)img.buf + i);
	}
	cbd_log("Total_Checksum = %x, Verify_Checksum = %x\n", Total_Checksum, Verify_Checksum);
	c_msg.Header.Sync = 0xFE;
	c_msg.Header.PacketLen = 5;
	c_msg.Header.MsgId = BOOT_UARTMSG_CHECKSUM;
	c_msg.Checksum = Total_Checksum;
	/* send Checksum  Msg */
	err = s_write(fd, &c_msg, sizeof(struct Checksum_Msg));
	if (err < 0) {
		cbd_log("send checksum msg fail (err %d)\n", err);
		goto exit;
	}
	usleep(10000);
	cbd_log("Sent checksum msg size = %d\n", err);
	print_data((char *)&c_msg, sizeof(struct Checksum_Msg));

	/* Read ack */
	err = s_read(fd, g_rx_buf, 3);
	if (err < 0 || g_rx_buf[0] != 0xFE || g_rx_buf[2] != BOOT_UARTMSG_ACK) {
		cbd_log("Handshake ack read fail: err = %d, 0x%02x, 0x%02x, 0x%02x\n",
			err, g_rx_buf[0], g_rx_buf[1], g_rx_buf[2]);
		err = -EFAULT;
		goto exit;
	}
	usleep(10000);
exit:
	if (img.buf){
		free(img.buf);
		cbd_log("free img buffer\n");
	}
	if (fd && err >= 0){
		close(fd);
		cbd_log("Close fd\n");
	}
	return err;
}

/* Uart boot */
static int uart_xmit_boot(struct via_args *args)
{
	int err;
	int fd = args->boot_fd;
	int retry = 3;

	while(retry --){
		/* cp on or reset*/
		cbd_log("start\n");

		err = ioctl(fd, IOCTL_MODEM_ON, NULL);
		cbd_log("fd =%x err: %d\n",fd, err);
		if(err) {
			cbd_log("IOCTL_MODEM_ON fail (err %d)\n", err);
			goto exit;
		}
		sleep(2);

		/* Send bootloader binary to cbp72 through UART*/
		cbd_log("boot send ----\n");
		err = uart_xmit_loader(args);
		if(err < 0) {
			cbd_log("CBL xmit fail (err %d, retry %d)\n", err, retry);
		}
		else break;

		if(retry == 0){
			cbd_log("uart xmit boot fail\n");
			goto exit;
		}
	}
	cbd_log("CP boot binary send done.\n");
exit:
	return err;
}

int start_cbp72_boot(struct boot_args *args)
{
	int err = 0;
	int fd;
	struct via_args via_boot;

	memset(&via_boot, 0x00, sizeof(struct via_args));

	cbd_log("+++\n");

	via_boot.cbd_args = args;

	fd = open(args->cpn->node_boot, O_RDWR);
	if (fd < 0) {
		cbd_log("%s open fail (err %d)\n", args->cpn->node_boot, fd);
		err = -EIO;
		goto exit;
	}
	via_boot.boot_fd = fd;

	via_boot.bin_fd = open(args->cpn->path_bin, O_RDWR);
	if(via_boot.bin_fd < 0) {
		cbd_log("%s open fail (err %d)\n", args->cpn->path_bin,
			via_boot.bin_fd);
		err = via_boot.bin_fd;
		goto exit;
	}

	err = update_cp_imgmap(&via_boot);
	if (err < 0) {
		cbd_log("cp via map update fail (err %d)\n", err);
		goto exit;
	}
	cbd_log("lnk_boot = %x, lnk_main = %x\n", args->lnk_boot, args->lnk_main);

	switch (args->lnk_boot) {
	case LINKDEV_C2C:
		/* DO nothing*/
		break;
	case LINKDEV_UART:
		err = uart_xmit_boot(&via_boot);
		if (err < 0) {
			cbd_log("uart boot fail (err %d)\n", err);
			goto exit;
		}
		break;
	case LINKDEV_DPRAM:
	default:
		err = dpram_xmit_boot(&via_boot);
		if (err < 0) {
			cbd_log("via boot fail (err %d)\n", err);
			goto exit;
		}
		break;
	}

	switch (args->lnk_main) {
	case LINKDEV_DPRAM:
	default:
		err = dpram_xmit_main(&via_boot);
		if (err < 0) {
			cbd_log("via_xmit_main fail (err %d)\n", err);
			goto exit;
		}
		break;
	}

	err = ioctl(via_boot.boot_fd, IOCTL_MODEM_BOOT_OFF, NULL);
	if (err < 0) {
		cbd_log("IOCTL_MODEM_BOOT_OFF fail (err %d)\n", err);
		goto exit;
	}


exit:
	if (via_boot.boot_fd >= 0)
		close(via_boot.boot_fd);

	if (via_boot.bin_fd >= 0)
		close(via_boot.bin_fd);

	return err;
}

int start_cbp72_dump(struct boot_args *args)
{
	int err = -EFAULT;
	int boot_fd;
	int bin_fd = 0;
	int log_fd = 0;
	int dump_fd = 0;
	int info_fd = 0;
	time_t now = 0;
	struct tm result;
	int rcvd_size = 0;
	int recv_size = 0;
	struct dpram_dump_arg dump_arg;
	char log_file_str[256], log_surfix[32];
	char cpinfo_buf[512] = "CDMA: ";
	char log_prefix[MAX_PREFIX_LEN];

	memset(&dump_arg, 0, sizeof(dump_arg));
	memset(log_file_str, 0, sizeof(log_file_str));
	memset(log_surfix, 0, sizeof(log_surfix));
	memset(log_prefix, 0, sizeof(log_prefix));

	cbd_log("start\n");

	/* Open the boot device */
	boot_fd = open(args->cpn->node_boot, O_RDWR);
	if (boot_fd < 0) {
		err = boot_fd;
		cbd_log("%s open fail (err %d)\n", args->cpn->node_boot, err);
		goto exit;
	}
	cbd_log("open %s (fd = %d)\n", args->cpn->node_boot, boot_fd);

	/* Open the CP binary */
	bin_fd = open(args->cpn->path_bin, O_RDONLY);
	if (bin_fd < 0) {
		err = bin_fd;
		cbd_log("%s open fail (err %d)\n", args->cpn->path_bin, err);
		goto exit;
	}
	cbd_log("open %s (fd = %d)\n", args->cpn->path_bin, bin_fd);

	time(&now);
	localtime_r(&now, &result);
	strftime(log_surfix, 20, "%Y%m%d_%H%M%S", &result);
	snprintf(log_prefix, MAX_PREFIX_LEN, "cpcrash");

	/* Create a CP crash log file */
	sprintf(log_file_str, "%s/%s_cdma_crash_log_%s.log", get_log_dir(),
			log_prefix, log_surfix);
	log_fd = open(log_file_str, O_WRONLY | O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (log_fd < 0) {
		err = log_fd;
		cbd_log("%s open fail (err %d)\n", log_file_str, err);
		goto exit;
	}
	cbd_log("open %s (fd = %d)\n", log_file_str, log_fd);

	/* Open (create) a CP crash info file */
	sprintf(log_file_str, "%s/%s_info_%s_%s.log", get_log_dir(),
			log_prefix, args->cpn->name, log_surfix);
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

	/* Open (create) a CP dump file */
	sprintf(log_file_str, "%s/%s_dump_%s_%s.log", get_log_dir(),
			log_prefix, args->cpn->name, log_surfix);
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
	dump_arg.buff = malloc(CBP72_DUMP_BUFF_SIZE);
	if (!dump_arg.buff) {
		cbd_log("dump buffer malloc fail\n");
		dprintf(log_fd, "%s: dump buffer malloc fail\n", __func__);
		err = -ENOMEM;
		goto exit;
	}

	memset(dump_arg.buff, 0, CBP72_DUMP_BUFF_SIZE);
	dump_arg.req = CBP72_IMG_UL_START_REQ;
	dump_arg.resp = CBP72_IMG_UL_START_RESP;
	dump_arg.cmd = 1;

	err = ioctl(boot_fd, IOCTL_MODEM_DUMP_UPDATE, &dump_arg);
	if (err < 0 || err != (int)dump_arg.resp) {
		cbd_log("CBP72_IMG_UL_START_REQ fail (err %d)\n", err);
		dprintf(log_fd, "%s: CBP72_IMG_UL_START_REQ fail (err %d)\n", __func__, err);
		goto exit;
	}

	dump_arg.req = CBP72_IMG_UL_READY;
	dump_arg.resp = CBP72_IMG_UL_SEND_REQ;
	dump_arg.cmd = 0;

	err = ioctl(boot_fd, IOCTL_MODEM_DUMP_UPDATE, &dump_arg);
	if (err < 0 || err != (int)dump_arg.resp) {
		cbd_log("CBP72_IMG_UL_READY fail (err %d)\n", err);
		dprintf(log_fd, "%s: CBP72_IMG_UL_READY fail (err %d)\n", __func__, err);
		goto exit;
	}

	strncpy(cpinfo_buf + strlen(cpinfo_buf), dump_arg.buff, dump_arg.buff_size);
	err = write(info_fd, cpinfo_buf, dump_arg.buff_size);
	if (err < 0) {
		cbd_log("INFO write fail (err %d)\n", err);
		dprintf(log_fd, "%s: INFO write fail (err %d)\n", __func__, err);
		goto exit;
	}

	/* Receive dump data from CP */
	while (1) {
		memset(dump_arg.buff, 0, CBP72_DUMP_BUFF_SIZE);
		dump_arg.req = CBP72_IMG_UL_SEND_RESP;
		dump_arg.resp = CBP72_IMG_UL_SEND_REQ;
		dump_arg.cmd = 0;

		err = ioctl(boot_fd, IOCTL_MODEM_DUMP_UPDATE, &dump_arg);
		if (err == CBP72_IMG_UL_DONE_REQ) {
			dump_arg.req = CBP72_IMG_UL_DONE_RESP;
			dump_arg.resp = 0;
			dump_arg.cmd = 1;

			err = ioctl(boot_fd, IOCTL_MODEM_DUMP_UPDATE, &dump_arg);
			if (err < 0) {
				cbd_log("CBP72_IMG_UL_DONE_RESP fail (err %d)\n", err);
				dprintf(log_fd, "%s: CBP72_IMG_UL_DONE_RESP fail (err %d)\n", __func__, err);
				break;
			}

			cbd_log("DUMP complete (size %d)!!!\n", rcvd_size);
			dprintf(log_fd, "%s: DUMP complete (size %d)!!!\n", __func__, rcvd_size);
			break;
		} else if (err < 0 || err != (int)dump_arg.resp) {
			cbd_log("CBP72_IMG_UL_SEND_RESP fail (err %d)\n", err);
			dprintf(log_fd, "%s: CBP72_IMG_UL_SEND_RESP fail (err %d)\n", __func__, err);
			break;
		}

		recv_size = dump_arg.buff_size;
		err = write(dump_fd, dump_arg.buff, recv_size);
		if (err < 0) {
			cbd_log("DUMP write fail (err %d)\n", err);
			dprintf(log_fd, "%s: DUMP write fail (err %d)\n", __func__, err);
			break;
		}

		rcvd_size += recv_size;
		dprintf(log_fd, "%s: %d bytes received\n", __func__, rcvd_size);
	}

	fsync(log_fd);
	fsync(info_fd);
	fsync(dump_fd);

	cbd_log("VIA Dump done, Waiting for CMC dump.\n");
	sleep(60);

	cbd_log("Go to Upload mode\n");
	err = ioctl(boot_fd, IOCTL_MODEM_CP_UPLOAD, cpinfo_buf);
	if (err < 0) {
		cbd_log("IOCTL_MODEM_CP_UPLOAD fail (err %d)\n", err);
		goto exit;
	}

exit:
	if (dump_arg.buff)
		free(dump_arg.buff);

	if (bin_fd >= 0)
		close(bin_fd);

	if (boot_fd >= 0)
		close(boot_fd);

	if (log_fd >= 0)
		close(log_fd);

	if (info_fd >= 0)
		close(info_fd);

	if (dump_fd >= 0)
		close(dump_fd);

	return err;
}

int start_cbp72_shutdown(struct boot_args *args)
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

