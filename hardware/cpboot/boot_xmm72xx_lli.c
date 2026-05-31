/*
 * XMM626X boot process
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
#include <cutils/android_reboot.h>

#include "boot.h"
#include "xmm72xx.h"
#include "util.h"

#if 0
#ifdef CONFIG_BOOT_BINARY_VERIFY
#include "verify_sign.h"
#include "secure_key.h"
#include "sha.h"
#endif
#endif

/* CP Image contents table, it was fixed total 512byte.
 * Default value was filled settings of XMM6260(U1) for old version
 * conpatibility.
 * If you want to boot with xmm6260, you can use default tabel without table
 * update from modem.bin
 */
static struct cp_imgmap default_table[IMG_MAX_IDX] = {
	[IMG_PSI] = {
		.name = "PSIRAM",
		.bin_offset = 0x0,
		.size = 0xE000,
	},
	[IMG_EBL] = {
		.name = "EBL",
		.bin_offset = 0xF000,
		.mem_offset = 0x60000000,
		.size = 0x19000,
	},
	[IMG_MAIN0] = {
		.name = "MAIN",
		.bin_offset = 0x28000,
		.mem_offset = 0x60300000,
		.size = 0x9D7800,
	},
	[IMG_SECPACK] = {
		.name = "SECPACK",
		.bin_offset = 0x9FF800,
		.size = 0x800,
	},
	[IMG_NV] = {
		.name = "NV",
		.bin_offset = 0xa00000,
		.mem_offset = 0x60e80000,
		.size = 0x200000,
	},
};
static char img_table[IMG_TABLE_MAX_SIZE];
static char g_rx_buf[4096] = {0,};
static unsigned enable_crc;

/* Safe write and Safe read */
static ssize_t read_timeout(int fd, void *buf, size_t len, int timeout_sec)
{
	struct timeval tv;
	fd_set readfds;
	int err;

	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	tv.tv_sec = timeout_sec;
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
		break;
	}
	return err;
}
#define s_read(f, b, l) read_timeout(f, b, l, 5)
#define d_read(f, b, l) read_timeout(f, b, l, 20)

static ssize_t s_write(int fd, void *buf, size_t len)
{
	int err, block_size, rest_size, send_size = 0;
	rest_size = len;

	while (rest_size) {
		block_size = rest_size > BOOT_4KB ? BOOT_4KB : rest_size;
		err = write(fd, (char *)buf + send_size, block_size);
		if (err < 0) {
			cbd_log("Write failed: %d\n", err);
			return -EIO;
		}
		rest_size -= block_size;
		send_size += block_size;
	}

	return 0;

	/* TODO: check the write size and if return was less than requset,
	 * repeat write the reset buf data*/
	//return write(fd, buf, len);
}

static int get_index_by_name(struct imc_args *args, char *name)
{
	int index = IMG_MAX_IDX -1;
	struct cp_imgmap *imgmap;

	while (index >= 0) {
		imgmap = (struct cp_imgmap *)&args->img_tab[index];
		cbd_log("img_name=%s, search_name=%s\n", imgmap->name, name);
		if (!strcmp(imgmap->name, name))
			return index;
		else
			index--;
	}
	return -EINVAL;
}

#if 0

#ifdef CONFIG_BOOT_BINARY_VERIFY
static unsigned get_modem_binary_size(struct cp_imgmap *map)
{
	int i;
	unsigned size;

	for (i = 0; i < IMG_MAX_IDX; i++) {
		cbd_log(" map[%d].name[0] = 0x%x, %s, offset = 0x%x \n",
			i, map[i].name[0], map[i].name, map[i].bin_offset);
		if (map[i].name[0] == 0 && !map[i].bin_offset) {/* end of toc */
			size = map[i - 1].bin_offset + map[i - 1].size;
			cbd_log("size = 0x%x, offset(0x%x), size(%x)\n", size,
				map[i - 1].bin_offset, map[i - 1].size);
			return size;
		}
	}
	return 0;
}
#define SIGN_BUF_SIZE	2048
static void verify_binary_signature(struct imc_args *args)
{
	int ret;
	unsigned len, rest;
	char buf[SIGN_BUF_SIZE];
	uint8_t d[SHA_DIGEST_SIZE];
	uint8_t *sha;
	SHA_CTX ctx;

	rest = get_modem_binary_size(args->img_tab);
	if (!rest) {
		cbd_log("get modem binary fail\n");
		goto boot_stop;
	}

	SHA_init(&ctx);
	/* reset the cp binary fd ofsset */
	ret = lseek(args->bin_fd, 0, SEEK_SET);
	if (ret < 0) {
		cbd_log("CP binary partition lseek fail\n");
		goto boot_stop;
	}
	while (rest) {
		/* load modem binary */
		len = (SIGN_BUF_SIZE < rest) ? SIGN_BUF_SIZE : rest;
		ret = read(args->bin_fd, buf, len);
		if (ret < 0) {
			cbd_log("image read fail, ret=%d, len=%d\n", ret, len);
			goto boot_stop;
		}
		if (ret != (ssize_t)len)
			cbd_log("image read fail, ret=%d, len=%d\n", ret, len);
		SHA_update(&ctx, buf, len);
		rest -= len;
	}
	sha = SHA_final(&ctx);
	memcpy(d, (void *)sha, SHA_DIGEST_SIZE);

	/* load signature, fd offset is end of binary*/
	ret = read(args->bin_fd, buf, SIGNATURE_SIZE);
	if (ret < 0) {
		cbd_log("signature read fail(%d)\n", ret);
		goto boot_stop;
	}
	ret = is_signature_okay(d, buf, &PUBKEY_SECURE_BOOT);
	if (ret == 1) {
		/* signature verify ok */
		cbd_log("Modem binary signature is OK\n");
		return;
	}
	cbd_log("Modem binary signature fail(%d) stop.\n", ret);

boot_stop:
	while (1)
		sleep(0xff);
}
#else
#define verify_binary_signature(a) do {} while(0);
#endif
#endif

/* CP binary map update */
static int update_cp_imgmap(struct imc_args *args)
{
	int err;
	char *img_tab = img_table; /* table buf */
	struct cp_imgmap *psimap;

	if (args->bin_fd < 0) {
		cbd_log("invalid cp binary fd = %d\n", args->bin_fd);
		err = -EINVAL;
		goto exit;
	}
	/* backward compatibility for u1 cp binary, if it check teh PSIRAMN
	 * img name else skip update img table
	 */
	err = read(args->bin_fd, img_tab, IMG_TABLE_MAX_SIZE);
	if (err < 0) {
		cbd_log("cp img table read fail err=%d\n", err);
		goto exit;
	}

	psimap = (struct cp_imgmap *)img_tab;
	if (!strcmp(psimap->name, "PSIRAM")) {
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
	img->buf = (unsigned char *)malloc(img->length);
	if (!img->buf) {
		cbd_log("malloc fail\n");
		err = -ENOMEM;
		goto exit;
	}
	err = lseek(fd, cp_bin->bin_offset, SEEK_SET);
	if (err < 0) {
		cbd_log("lseek fail %d\n", err);
		free(img->buf);
		img->buf = NULL;
		goto exit;
	}
	err = read(fd, img->buf, img->length);
	if (err < (int)img->length) {
		cbd_log("file read fail err=%d\n", err);
		free(img->buf);
		img->buf = NULL;
		err = -EFAULT;
		goto exit;
	}
	err = 0;
	cbd_log("Load %s img offset=0x%x size=0x%x\n", cp_bin->name,
					cp_bin->bin_offset, img->length);
	print_data((char *)img->buf, 16);
exit:
	return err;
}

static int make_crc(unsigned char *buf, unsigned int size)
{
	int crc = 0;
	int i;

	for (i = 0; i < (int)size; i++)
		crc ^= *buf++;

	return crc;
}

/* Check unsigned ack value */
static int check_ack_val(int fd, unsigned check_val, unsigned size)
{
	unsigned val = 0;
	int err, i = 40;

	/* Get ACK */
	cbd_log("Try to read ack %dbyte, ack=0x%x\n", size, check_val);
	while (i--) {
		err = s_read(fd, &val, size);
		if (err < 0) {
			cbd_log("Read failed: %d\n", err);
			return err;
		}

		if (val == check_val) {
			cbd_log("ack read done: 0x%x\n\n", val);
			return 0;
		} else
			cbd_log("ack read fail: 0x%x\n", val);
	}
	return -1;
}

static int check_psi_done_ack(int fd)
{
	char val = 0, prev = 0;
	int err, i = 40;

	/* Get ACK */
	while (i--) {
		err = s_read(fd, &val, sizeof(val));
		if (err < 0) {
			cbd_log("Read failed: %d\n", err);
			return err;
		}
		/* XMM6360 secure boot, success ack was changed with 0x10 */
		if (prev == 0x01 && (val == 0x01 || val == 0x10)) {
			cbd_log("PSI ack 0x%02x-%02x\n", prev, val);
			return 0;
		}
		cbd_log("PSI read ack: 0x%02x-%02x\n", prev, val);
		prev = val;
	}
	return -1;
}

static int hsic_make_cmd_crc(unsigned char *buf)
{
	struct hsic_cmd_header *header = (struct hsic_cmd_header *)buf;
	int crc = 0;
	unsigned i;

	crc += header->type;
	crc += header->length;

	buf += sizeof(struct hsic_cmd_header);
	for (i = 0; i < header->length; i++)
		crc += *buf++;

	return crc;
}

static unsigned char *hsic_make_cmd(enum package_type type,
		unsigned int cmd_size, unsigned short data_size, void *data)
{
	struct hsic_cmd_header *cmd;

	cmd = (struct hsic_cmd_header *)malloc(cmd_size);
	if (!cmd) {
		cbd_log("Malloc failed\n");
		return NULL;
	}
	memset(cmd, 0, cmd_size);
	cmd->crc = 0;
	cmd->type = type;
	cmd->length = data_size;
	memcpy(cmd->payload, data, data_size);
	cmd->crc = hsic_make_cmd_crc((unsigned char *)cmd);

	return (unsigned char *)cmd;
}

static int hsic_read_ack(int fd, enum package_type type, int disable_log)
{
	struct hsic_cmd_header header;
	int payload_size = HSIC_PAYLOAD_SIZE;
	int apkt_size = 0;
	char *buf;
	int err;

	if (type == ReqSetProtConf)
		payload_size = 2048;

	err = s_read(fd, &header, sizeof(struct hsic_cmd_header));
	if (err < 0) {
		cbd_log("Read failed: %d\n", err);
		return err;
	}

	cbd_log("crc: %x type: %x len: %d\n",
		header.crc, header.type, header.length);

	if (header.type != type) {
		cbd_log("Command type is differ\n");
		return -1;
	}

	buf = (char *)malloc(payload_size);
	if (!buf) {
		cbd_log("Malloc failed\n");
		return -1;
	}

	while (apkt_size < payload_size) {
		err = s_read(fd, buf, payload_size);
		if (err < 0) {
			cbd_log("Read failed: %d\n", err);
			free(buf);
			return err;
		}
		apkt_size += err;
	}
	free(buf);

	return 0;
}

static int hsic_execute_cmd(int fd, enum package_type type,
			    unsigned int data_size, void *data)
{
	unsigned char *cmd;
	unsigned int cmd_size;
	int disable_log = 0;
	int err;

	if (type == ReqFlashWriteBlock)
		disable_log = 1;

	/* make command */
	if (type == ReqSetProtConf)
		cmd_size = sizeof(struct hsic_cmd_header) + 2048;
	else
		cmd_size = sizeof(struct hsic_cmd_header) + HSIC_PAYLOAD_SIZE;

	cmd = hsic_make_cmd(type, cmd_size, data_size, data);
	if (!cmd) {
		cbd_log("Make cmd failed\n");
		return -1;
	}

	/* Send command */
	if (!disable_log)
		cbd_log("Send cmd %dbytes\n", cmd_size);

	err = s_write(fd, cmd, cmd_size);
	if (err < 0) {
		cbd_log("Write failed: %d\n", err);
		free(cmd);
		return err;
	}
	free(cmd);

	/* Get ack */
	switch (type) {
	case ReqFlashWriteBlock:
	case ReqForceHwReset:
		return 0;
	default:
		break;
	}
	err = hsic_read_ack(fd, type, disable_log);
	if (err < 0) {
		cbd_log("Command read failed\n");
		return err;
	}
	if (!disable_log)
		cbd_log("\n");

	return 0;
}

char cmd_code_tx_ebl_lli[8] =
	{0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00};

static int hsic_usb_host_control(struct imc_args *args, int value);

static int hsic_xmit_psi(struct imc_args *args)
{
	struct image_buf img = {.length = 0, .buf = NULL};
	union psi_header h_psi;
	int err, i, fd = args->boot_fd;
	char crc = 0, val = 0;
	char chip_id;
	struct boot_args *cbd_arg = args->cbd_args;

	memset(&img, 0x00, sizeof(struct image_buf));
	err = s_write(fd, "AT", 2);
	if (err < 0) {
		cbd_log("send AT fail err=%d\n", err);
		goto exit;
	}
	cbd_log("Sent AT %d byte\n", err);

	err = s_write(fd, "AT", 2);
	if (err < 0) {
		cbd_log("send AT fail err=%d\n", err);
		goto exit;
	}
	cbd_log("Sent AT %d byte\n", err);
	/* Read ack */
	err = s_read(fd, g_rx_buf, 1);
	if (err < 0) {
		cbd_log("ack read fail: 0x%02x\n", g_rx_buf[0]);
		err = -EFAULT;
		goto exit;
	}

	switch(g_rx_buf[0]) {
	case IMC_CHIPID_XMM6360:
		enable_crc = (cbd_arg->type == IMC_XMM626X) ? 1 : 0;
	case IMC_CHIPID_XMM626X:
		cbd_log("ack read done: 0x%02x\n", g_rx_buf[0]);
		break;
	default:
		cbd_log("ack read fail: 0x%02x\n", g_rx_buf[0]);
		err = -EFAULT;
		goto exit;
	}

	err = s_read(fd, &chip_id, 1);
	if (err < 0)
		goto exit;
	cbd_log("Chip ID: 0x%02x\n", chip_id);
	args->chip_id = chip_id;

	for (i = 0; i < PSI_BOOTINFO_BYTE; i++) {
		err = s_read(fd, &val, sizeof(char));
		if (err < 0) {
			cbd_log("Read bootinfo failed: %d\n", err);
			if (err == -ETIMEDOUT)
				goto exit;
		}
		printf("%02x", val);
	}

	/* Load PSI bin */
	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_PSI]);
	if (err < 0) {
		cbd_log("PSI load fail err=%d\n", err);
		goto exit;
	}

	switch (chip_id) {
	case 0x1a: /* XMM7260 Chip ID*/
	case 0x1c: /* XMM6360 Chip ID*/
		h_psi.psi63.indication = 0x30;
		h_psi.psi63.length = img.length;
		break;
	default:
		h_psi.psi62.indication = 0x30;
		h_psi.psi62.length = (unsigned short)img.length;
		h_psi.psi62.dummy = 0xff;
		break;
	}
	/* send header */
	err = s_write(fd, &h_psi, sizeof(union psi_header));
	if (err < 0) {
		cbd_log("send psi header fail err=%d\n", err);
		goto exit;
	}
	cbd_log("Sent psi head size = %d\n", err);
	print_data((char *)&h_psi, sizeof(union psi_header));
	/* send PSI body */
	err = s_write(fd, (char *)img.buf, img.length);
	if (err < 0) {
		cbd_log("psi body sent fail err=%d\n", err);
		goto exit;
	}
	cbd_log("Sent psi body size = %d\n", err);
	print_data((char *)img.buf, 16);
	/* send PSI CRC */
	crc = make_crc(img.buf, img.length);
	err = s_write(fd, &crc, sizeof(char));
	if (err < 0) {
		cbd_log("psi CRC sent fail err=%d\n", err);
		goto exit;
	}

	err = check_psi_done_ack(fd);
	if (err < 0)
		goto exit;

	/* PSI_ACK_ENHANCED */
	err = check_ack_val(fd, 0xdd01, 2);
	if (err < 0) {
		cbd_log("ack 0x01dd fail err=%d\n", err);
		goto exit;
	}

	cbd_log("PSI sending is done\n\n");

exit:
	if (img.buf)
		free(img.buf);
	return err;
}

static int hsic_xmit_ebl(struct imc_args *args)
{
	struct image_buf img = {.length = 0, .buf = NULL};
	int fd = args->boot_fd;
	int err;
	struct bootloader_info boot_info;
	unsigned char crc;
	unsigned int ebl_crc_ack;
	struct boot_args *cbd_arg = args->cbd_args;

	/* Load EBL binary */
	memset(&img, 0x00, sizeof(struct image_buf));
	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_EBL]);
	if (err < 0) {
		cbd_log("EBL load fail err=%d\n", err);
		goto exit;
	}
	/* Send EBL size */
	cbd_log("Send EBL length: 0x%x bytes\n", img.length);
	err = s_write(fd, &img.length, sizeof(unsigned int));
	if (err < 0) {
		cbd_log("EBL head Write failed: %d\n", err);
		goto exit;
	}

	cbd_log("Read EBL length ack\n");
	err = check_ack_val(fd, 0xcccc, 2);
	if (err < 0)
		goto exit;

	/* Data bytes of EBL */
	cbd_log("Send EBL Body\n");
	err = s_write(fd, (char *)img.buf, img.length);
	if (err < 0) {
		cbd_log("EBL body write failed: %d\n", err);
		goto exit;
	}
	cbd_log("Send EBL BODY done: 0x%xbytes\n", img.length);

	/* CRC of EBL */
	crc = make_crc(img.buf, img.length);
	cbd_log("Send EBL CRC: 0x%x\n", crc);
	err = s_write(fd, &crc, sizeof(unsigned char));
	if (err < 0) {
		cbd_log("EBL CRC Write failed: %d\n", err);
		goto exit;
	}

	switch (cbd_arg->type) {
	case IMC_XMM7160: /* xmm7160 ChipID */
		ebl_crc_ack = 0xa554;
		break;
	default:
		if (args->chip_id == 0x1c) /* xmm6360 ChipID */
			ebl_crc_ack = 0xa552;
		else if (args->chip_id == 0x1a)
			ebl_crc_ack = 0xa558;
		else
			ebl_crc_ack = 0xa551;
		break;
	}

	cbd_log("Read EBL CRC ack(0x%x), chip_id(0x%x)\n", ebl_crc_ack,
			args->chip_id);
	err = check_ack_val(fd, ebl_crc_ack, 2);
	if (err < 0)
		goto exit;

	cbd_log("EBL sending is done\n\n");

	/* Get bootloader info */
	cbd_log("Read Bootloader info struct from EBL\n");
	err = s_read(fd, &boot_info, sizeof(struct bootloader_info));
	if (err < 0) {
		cbd_log("EBL Bootloader info Read failed: %d\n", err);
		goto exit;
	}
	cbd_log("bootloader info: name: %s\n\n", boot_info.name);

	/* SetProtConf command */
	cbd_log("Execute SetPortConf Command\n");
	err = hsic_execute_cmd(fd, ReqSetProtConf,
		sizeof(struct bootloader_info), &boot_info);
	if (err < 0)
		goto exit;

exit:
	if (img.buf)
		free(img.buf);

	return err;
}

static int hsic_xmit_reqflashwriteblock(struct imc_args *args,
	struct image_buf *img)
{
	unsigned char *buf = img->buf;
	unsigned rest = img->length, size;
	int fd = args->boot_fd;
	int err = 0;

	cbd_log("send binary size = %d\n", img->length);

	while (rest) {
		size = (rest < HSIC_PAYLOAD_SIZE) ? rest : HSIC_PAYLOAD_SIZE;
		err = hsic_execute_cmd(fd, ReqFlashWriteBlock, size, buf);
		if (err < 0) {
			cbd_log("ReqFlashWriteBlock cmd send fail err=%d\n",
				err);
			goto exit;
		}
		rest -= size;
		buf += size;
	}
	cbd_log("sent img done\n");
exit:
	return err;

}

/*	check_nv_file
 *
 *	check the nv files and if not exist, create default nv from CP bin
 */
static int check_nv_file(struct imc_args *args)
{
	struct image_buf img = {.length = 0, .buf = NULL};
	int fd, err = 0;
	char *nvfile = NV_PATH;
	int spin = 5;
	char prop_buf[PROPERTY_VALUE_MAX] = {0, };
	int nv_idx = get_index_by_name(args, "NV");

	cbd_log("check nv\n");

	memset(&img, 0, sizeof(struct image_buf));
	/* wait for RILD NV Validity done*/
	while (spin--) {
		property_get(VPROP_RFS_CHECKDONE, prop_buf, "0");
		if (prop_buf[0] == '1')
			break;
		cbd_log("wait restore nvfile\n");
		usleep(500000);
	}
	cbd_log("rild check nv %s\n", spin ? "done" : "timeout");

	fd = open(nvfile, O_RDWR | O_NDELAY);
	if (fd < 0) {
		memset(&img, 0x00, sizeof(struct image_buf));
		cbd_log("nv not exist path = %s create default NV\n", nvfile);
		fd = open(nvfile, O_RDWR | O_CREAT | O_TRUNC | O_SYNC,
			FILE_MODE);
		if (fd < 0) {
			err = fd;
			cbd_log("default nv file create fail\n");
			cbd_log("EFS filesystem error - stop cpboot\n");
			while (1)
				sleep(0xff);
			goto exit;
		}
		err = prepare_image(args->bin_fd, &img, &args->img_tab[nv_idx]);
		if (err < 0) {
			cbd_log("NV load fail err = %d\n", err);
			goto exit;
		}
		err = write(fd, img.buf, img.length);
		if (err < (int)img.length) {
			cbd_log("nv file write fail err = %d\n", err);
			goto exit;
		}
		fsync(fd);
		/* TODO: create md5 checksum */
	} else {
/*		cbd_log("check nv validity....\n");
		load_md5_state();
		check_nv_data_validity();*/
	}
exit:
	if (fd >= 0)
		close(fd);
	if (img.buf)
		free(img.buf);
	return err;
}

static int hsic_xmit_modem(struct imc_args *args)
{
	int fd = args->boot_fd, nvfd = -1;
	unsigned char sec_start[2048];
	unsigned short sec_end = 0x0000;
	struct imc_secpack_bin *secpack = (struct imc_secpack_bin *)sec_start;
	unsigned force_hw_reset = 0x00111001;
	unsigned mem_addr;
	int err;
	struct image_buf img = {.length = 0, .buf = NULL};
	struct cp_imgmap nvfile = {
			.name = "nvdata.bin",
			.bin_offset = 0x0,
		};
	int nv_idx = get_index_by_name(args, "NV");
	char secure_reboot[PROPERTY_VALUE_MAX];

	/*
	 * Send SECURE package
	 */
	cbd_log("Send Secure package\n");
	memset(&img, 0x00, sizeof(struct image_buf));
	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_SECPACK]);
	if (err < 0) {
			cbd_log("load Secure package fail err = %d\n", err);
			goto exit;
	}
	if (img.length != 2048) {
		cbd_log("Secure image length is wrong\n");
		goto exit;
	}
	memcpy(sec_start, img.buf, 2048);
	free(img.buf);
	img.buf = NULL;
	/* ReqSecStart command */
	cbd_log("Execute ReqSecStart Command\n");
	err = hsic_execute_cmd(fd, ReqSecStart, 2048, &sec_start);
	if (err < 0) {
		property_get(PROP_SALES_CODE, secure_reboot, "");
		if (strcmp(secure_reboot, "TMB") == 0) {
			cbd_log("secure err: Invalid Main image\n");
			android_reboot(ANDROID_RB_RESTART2, 0, "secure");
		}
		goto exit;
	}

	/*
	 * Send Main binary
	 */
	cbd_log("Send Main0 binary\n");
	memset(&img, 0x00, sizeof(struct image_buf));
	/* check the main0 binary image map size */
	cbd_log("MAIN0 imgmap size=0x%x, secpack size=0x%x\n",
		args->img_tab[IMG_MAIN0].size, secpack->len);
	if (!args->img_tab[IMG_MAIN0].size)
		args->img_tab[IMG_MAIN0].size = secpack->len;
	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_MAIN0]);
	if (err < 0) {
		cbd_log("Load main0 binary fail\n");
		return -1;
	}
	mem_addr = args->img_tab[IMG_MAIN0].mem_offset;
	cbd_log("Execute ReqFlashSetAddress cmd addr = 0x%x\n", mem_addr);
	err = hsic_execute_cmd(fd, ReqFlashSetAddress, sizeof(unsigned),
		&mem_addr);
	if (err < 0) {
		cbd_log("Excute ReqFlashSetAddr cmd fail err=%d\n", err);
		goto exit;
	}

	err = hsic_xmit_reqflashwriteblock(args, &img);
	if (err < 0) {
		cbd_log("cp Main0 binary send fail err=%d\n", err);
		goto exit;
	}
	free(img.buf);
	img.buf = NULL;

	cbd_log("Send Main1 binary\n");
	memset(&img, 0x00, sizeof(struct image_buf));
	/* check the main1 binary image map size */
	cbd_log("MAIN1 imgmap size=0x%x, secpack size=0x%x\n",
		args->img_tab[IMG_MAIN1].size, secpack->len2);
	if (!args->img_tab[IMG_MAIN1].size)
		args->img_tab[IMG_MAIN1].size = secpack->len2;
	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_MAIN1]);
	if (err < 0) {
		cbd_log("Load main1 binary fail\n");
		return -1;
	}
	mem_addr = args->img_tab[IMG_MAIN1].mem_offset;
	cbd_log("Execute ReqFlashSetAddress cmd addr = 0x%x\n", mem_addr);
	err = hsic_execute_cmd(fd, ReqFlashSetAddress, sizeof(unsigned),
		&mem_addr);
	if (err < 0) {
		cbd_log("Excute ReqFlashSetAddr cmd fail err=%d\n", err);
		goto exit;
	}

	err = hsic_xmit_reqflashwriteblock(args, &img);
	if (err < 0) {
		cbd_log("cp Main1 binary send fail err=%d\n", err);
		goto exit;
	}
	free(img.buf);
	img.buf = NULL;

	cbd_log("Send Main2 binary\n");
	memset(&img, 0x00, sizeof(struct image_buf));
	/* check the main2 binary image map size */
	cbd_log("MAIN2 imgmap size=0x%x, secpack size=0x%x\n",
		args->img_tab[IMG_MAIN2].size, secpack->len3);
	if (!args->img_tab[IMG_MAIN2].size)
		args->img_tab[IMG_MAIN2].size = secpack->len3;
	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_MAIN2]);
	if (err < 0) {
		cbd_log("Load main2 binary fail\n");
		return -1;
	}
	mem_addr = args->img_tab[IMG_MAIN2].mem_offset;
	cbd_log("Execute ReqFlashSetAddress cmd addr = 0x%x\n", mem_addr);
	err = hsic_execute_cmd(fd, ReqFlashSetAddress, sizeof(unsigned),
		&mem_addr);
	if (err < 0) {
		cbd_log("Excute ReqFlashSetAddr cmd fail err=%d\n", err);
		goto exit;
	}

	err = hsic_xmit_reqflashwriteblock(args, &img);
	if (err < 0) {
		cbd_log("cp Main2 binary send fail err=%d\n", err);
		goto exit;
	}
	free(img.buf);
	img.buf = NULL;

	/*
	 * Send NV data
	 */
	cbd_log("Send NV data\n");
	err = check_nv_file(args);
	if (err < 0) {
		cbd_log("NV not validate!!\n");
		goto exit;
	}
	nvfd = open(NV_PATH, O_RDONLY | O_NDELAY);
	if (nvfd < 0) {
		cbd_log("nv file(%s) open fail err=%d\n", NV_PATH, nvfd);
		goto exit;
	}
	nvfile.size = args->img_tab[nv_idx].size;
	nvfile.mem_offset = args->img_tab[nv_idx].mem_offset;

	memset(&img, 0x00, sizeof(struct image_buf));
	err = prepare_image(nvfd, &img, &nvfile);
	if (err < 0) {
		cbd_log("nv file load fail\n");
		goto exit;
	}
	mem_addr = args->img_tab[nv_idx].mem_offset;
	cbd_log("Execute ReqFlashSetAddress cmd addr = 0x%x\n", mem_addr);
	err = hsic_execute_cmd(fd, ReqFlashSetAddress, sizeof(unsigned),
		&mem_addr);
	if (err < 0) {
		cbd_log("Excute ReqFlashSetAddr cmd fail err=%d\n", err);
		goto exit;
	}

	err = hsic_xmit_reqflashwriteblock(args, &img);
	if (err < 0) {
		cbd_log("cp NV file send fail err=%d\n", err);
		goto exit;
	}
	free(img.buf);
	img.buf = NULL;

	/* ReqSecEnd command */
	cbd_log("Execute ReqSecEnd Command\n");
	err = hsic_execute_cmd(fd, ReqSecEnd, sizeof(unsigned short),
			   &sec_end);
	if (err < 0) {
		property_get(PROP_SALES_CODE, secure_reboot, "");
		if (strcmp(secure_reboot, "TMB") == 0) {
			cbd_log("secure err: Invalid Main image\n");
			android_reboot(ANDROID_RB_RESTART2, 0, "secure");
		}
		goto exit;
	}

	/*
	 * Send LTE SECURE package
	 */
	cbd_log("[xmm7260] Send LTE Secure package\n");
	memset(&img, 0x00, sizeof(struct image_buf));
	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_LTESECPACK]);
	if (err < 0) {
			cbd_log("load LTE Secure package fail err = %d\n", err);
			goto exit;
	}
	if (img.length != 2048) {
		cbd_log("LTE Secure image length is wrong\n");
		goto exit;
	}
	memcpy(sec_start, img.buf, 2048);
	free(img.buf);
	img.buf = NULL;
	/* ReqSecStart command */
	cbd_log("Execute ReqSecStart Command [LTE]\n");
	err = hsic_execute_cmd(fd, ReqSecStart, 2048, &sec_start);
	if (err < 0)
		goto exit;

	/*
	 * LTE
	 */
	cbd_log("Send LTE binary\n");
	memset(&img, 0x00, sizeof(struct image_buf));
	/* check the lte binary image map size */
	cbd_log("LTE imgmap size=0x%x, secpack size=0x%x\n",
		args->img_tab[IMG_LTE].size, secpack->len);
	if (!args->img_tab[IMG_LTE].size)
		args->img_tab[IMG_LTE].size = secpack->len;
	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_LTE]);
	if (err < 0) {
		cbd_log("Load lte binary fail\n");
		goto exit;
	}
	mem_addr = args->img_tab[IMG_LTE].mem_offset;
	cbd_log("Execute ReqFlashSetAddress cmd addr = 0x%x\n", mem_addr);
	err = hsic_execute_cmd(fd, ReqFlashSetAddress, sizeof(unsigned),
		&mem_addr);
	if (err < 0) {
		cbd_log("Excute ReqFlashSetAddr cmd fail err=%d\n", err);
		goto exit;
	}

	err = hsic_xmit_reqflashwriteblock(args, &img);
	if (err < 0) {
		cbd_log("cp lte binary send fail err=%d\n", err);
		goto exit;
	}
	free(img.buf);
	img.buf = NULL;

	/* ReqSecEnd command */
	cbd_log("Execute ReqSecEnd Command\n");
	err = hsic_execute_cmd(fd, ReqSecEnd, sizeof(unsigned short),
			   &sec_end);
	if (err < 0)
		goto exit;

	/*
	 * Send USPC SECURE package
	 */
	cbd_log("[xmm7260] Send USPC Secure package\n");
	memset(&img, 0x00, sizeof(struct image_buf));
	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_USPCSECPACK]);
	if (err < 0) {
			cbd_log("load UPSC Secure package fail err = %d\n", err);
			goto exit;
	}
	if (img.length != 2048) {
		cbd_log("USPC Secure image length is wrong\n");
		goto exit;
	}
	memcpy(sec_start, img.buf, 2048);
	free(img.buf);
	img.buf = NULL;
	/* ReqSecStart command */
	cbd_log("Execute ReqSecStart Command [USPC]\n");
	err = hsic_execute_cmd(fd, ReqSecStart, 2048, &sec_start);
	if (err < 0)
		goto exit;

	/*
	 * USPC
	 */
	cbd_log("Send USPC binary\n");
	memset(&img, 0x00, sizeof(struct image_buf));
	/* check the uspc binary image map size */
	cbd_log("USPC imgmap size=0x%x, secpack size=0x%x\n",
		args->img_tab[IMG_USPC].size, secpack->len);
	if (!args->img_tab[IMG_USPC].size)
		args->img_tab[IMG_USPC].size = secpack->len;
	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_USPC]);
	if (err < 0) {
		cbd_log("Load uspc binary fail\n");
		goto exit;
	}
	mem_addr = args->img_tab[IMG_USPC].mem_offset;
	cbd_log("Execute ReqFlashSetAddress cmd addr = 0x%x\n", mem_addr);
	err = hsic_execute_cmd(fd, ReqFlashSetAddress, sizeof(unsigned),
		&mem_addr);
	if (err < 0) {
		cbd_log("Excute ReqFlashSetAddr cmd fail err=%d\n", err);
		goto exit;
	}

	err = hsic_xmit_reqflashwriteblock(args, &img);
	if (err < 0) {
		cbd_log("cp USPC binary send fail err=%d\n", err);
		goto exit;
	}
	free(img.buf);
	img.buf = NULL;

	/* ReqSecEnd command */
	cbd_log("Execute ReqSecEnd Command\n");
	err = hsic_execute_cmd(fd, ReqSecEnd, sizeof(unsigned short),
			   &sec_end);
	if (err < 0)
		goto exit;

	/*
	 * Send FW SECURE package
	 */
	cbd_log("[xmm7260] Send FW Secure package\n");
	memset(&img, 0x00, sizeof(struct image_buf));
	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_FWSECPACK]);
	if (err < 0) {
			cbd_log("load FW Secure package fail err = %d\n", err);
			goto exit;
	}
	if (img.length != 2048) {
		cbd_log("FW Secure image length is wrong\n");
		goto exit;
	}
	memcpy(sec_start, img.buf, 2048);
	free(img.buf);
	img.buf = NULL;
	/* ReqSecStart command */
	cbd_log("Execute ReqSecStart Command [FW]\n");
	err = hsic_execute_cmd(fd, ReqSecStart, 2048, &sec_start);
	if (err < 0)
		goto exit;

	/*
	 * FW
	 */
	cbd_log("Send FW binary\n");
	memset(&img, 0x00, sizeof(struct image_buf));
	/* check the fw binary image map size */
	cbd_log("FW imgmap size=0x%x, secpack size=0x%x\n",
		args->img_tab[IMG_USPC].size, secpack->len);
	if (!args->img_tab[IMG_FW].size)
		args->img_tab[IMG_FW].size = secpack->len;
	err = prepare_image(args->bin_fd, &img, &args->img_tab[IMG_FW]);
	if (err < 0) {
		cbd_log("Load fw binary fail\n");
		goto exit;
	}
	mem_addr = args->img_tab[IMG_FW].mem_offset;
	cbd_log("Execute ReqFlashSetAddress cmd addr = 0x%x\n", mem_addr);
	err = hsic_execute_cmd(fd, ReqFlashSetAddress, sizeof(unsigned),
		&mem_addr);
	if (err < 0) {
		cbd_log("Excute ReqFlashSetAddr cmd fail err=%d\n", err);
		goto exit;
	}

	err = hsic_xmit_reqflashwriteblock(args, &img);
	if (err < 0) {
		cbd_log("cp fw binary send fail err=%d\n", err);
		goto exit;
	}
	free(img.buf);
	img.buf = NULL;

	/* ReqSecEnd command */
	cbd_log("Execute ReqSecEnd Command\n");
	err = hsic_execute_cmd(fd, ReqSecEnd, sizeof(unsigned short),
			   &sec_end);
	if (err < 0)
		goto exit;

	/* ReqForceHwReset command, never read ack */
	cbd_log("Execute ReqForceHwReset Command\n");
	err = hsic_execute_cmd(fd, ReqForceHwReset, sizeof(unsigned int),
			   &force_hw_reset);
exit:
	if (nvfd >= 0)
		close(nvfd);
	if(img.buf)
		free(img.buf);

	return err;
}

#ifdef CONFIG_PORT_POWER
/* force disconnect usb 1-2 */
static int remove_usb_link(void)
{
	int fd, err = -1;
	char *usb_remove = USB_REMOVE;

	/* if usb 1-2 device was enumerated, force disconnect*/
	fd = open(usb_remove, O_WRONLY);
	if (fd > 0) {
		cbd_log("usb 1-2 remove and port 2 power off\n");
		err = write(fd, "0", strlen("0"));
		if (err < 0) {
			cbd_log("write value=%s failed: %d\n", "0", err);
			goto exit;
		}
		cbd_log("usb 1-2 remove\n");
	} else {
		cbd_log("usb 1-2 not enumerated fd=%d\n", fd);
		return fd;
	}
exit:
	if (fd >= 0)
		close(fd);
	return err;
}

static int ehci_port_power(int value)
{
	int fd, err = 0;
	char *port_power = EXYNOS_PORT_POWER;

	fd = open(port_power, O_RDWR);
	if (fd < 0) {
		cbd_log("%s open fail err=%d\n", port_power, fd);
		/* first boot time, usb 1-2 was not created*/
		return fd;
	}
	cbd_log("port power = %d\n", value);
	err = write(fd, value ? "1 2" : "0 2", strlen("1 2"));
	if (err < 0) {
		cbd_log("write value=%d failed: %d\n", value, err);
		goto exit;
	}
	cbd_log("port power = %d\n", value);
exit:
	if (fd >= 0)
		close(fd);
	return err;
}

/* port power off - on*/
static int hsic_usb_host_port_power(struct imc_args *args, int value)
{
	int err;


	if (!value) {
		err = remove_usb_link();
		if (err < 0)
			goto exit;
	}

	err = ehci_port_power(value);
	if (err < 0)
		goto exit;
exit:
	return err;
}

/* USB Host reset */
static int hsic_usb_host_ehci_power(struct imc_args *args, int value)
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

static int hsic_usb_host_control(struct imc_args *args, int value)
{
	char boot_done[PROPERTY_VALUE_MAX] = {0, '\n'};

	property_get(VPROP_CPBOOT_DONE, boot_done, "0");
	cbd_log("%s, %s\n", VPROP_CPBOOT_DONE, boot_done);
	if (boot_done[0] == '1')
		/* port power off - on*/
		return hsic_usb_host_port_power(args, value);
	else
		/* ehchi off -on */
		return hsic_usb_host_ehci_power(args, value);
}

#else
/* USB Host reset */
static int hsic_usb_host_control(struct imc_args *args, int value)
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
#endif

/* MIPI-LLI boot */
static int lli_xmit_boot(struct imc_args *args)
{
	int err;
	int fd = args->boot_fd;

	err = hsic_usb_host_control(args, 0);
	if (err < 0)
		goto exit;

	/* boot_on ioctl */
	err = ioctl(fd, IOCTL_MODEM_BOOT_ON, NULL);
	if (err < 0)
		goto exit;

	/*
	 * cp on or reset
	 */
	if (args->cbd_args->options & BOPT_CPUPLOAD)
		err = ioctl(fd, IOCTL_MODEM_RESET);
	else
		err = ioctl(fd, IOCTL_MODEM_ON);
	if (err) {
		cbd_log("IOCTL_CP_ON  failed: %d\n", err);
		goto exit;
	}

	err = hsic_usb_host_control(args, 1);
	if (err < 0)
		goto exit;

	/*
	 * wait HSIC enumeration
	 */
	if(!wait_file_value(LINK_IDVENDOR, ID_VENDOR, HSIC_ENUM_COUNTER) &&
		!wait_file_value(LINK_IDPRODUCT, ID_PRODUCT, HSIC_ENUM_COUNTER)) {
		cbd_log("check [BOOTROM] link connected\n");
	} else {
		err = -ENODEV;
		cbd_log("re-enumeration fail\n");
		goto exit;
	}
	usleep(500000);

	/*
	 * Send AT and PSI binary to xmm626x through HSIC
	 */
	cbd_log("PSI send ----\n");
	err = hsic_xmit_psi(args);
	if (err < 0) {
		cbd_log("PSI xmit fail err=%d\n", err);
		goto exit;
	}

	cbd_log("CP boot binary send done.\n");
exit:
	return err;
}


static int lli_xmit_main(struct imc_args *args)
{
	int err;
	struct boot_args *cbd_arg = args->cbd_args;
	int fd = args->boot_fd;

	/* RPSI_CMD_CODE_TX_EBL */
	cbd_log("send RPSI_CMD_CODE-TX_EBL\n");
	err = s_write(fd, cmd_code_tx_ebl_lli, 8);
	if (err < 0) {
		cbd_log("Send code_tx_ebl fail err=%d\n", err);
		goto exit;
	}

	err = check_ack_val(fd, 0xaa00, 2);
	if (err < 0) {
		cbd_log("ack 0xaa00 fail err=%d\n", err);
		goto exit;
	}

	/* Send EBL */
	cbd_log("Send EBL ----\n");
	err = hsic_xmit_ebl(args);
	if (err < 0) {
		cbd_log("EBL send fail\n");
		goto exit;
	}

	/* Send Secpack, Main, NV */
	cbd_log("Send Main ----, 0x%x\n", cbd_arg->type);
	err = hsic_xmit_modem(args);
	if (err < 0) {
		cbd_log("Main send fail\n");
		goto exit;
	}

	cbd_log("setting lli control vallue to 1\n");

	set_file_value(MIPI_LLI_CONTROL, "1");
	usleep(4000000);
	hsic_usb_host_control(args, 0);

exit:
	return err;
}

int shutdown_xmm72xx_lli_modem(struct boot_args *args)
{
	int fd, err = 0;
	struct imc_args imc_boot;

	fd = open(args->cpn->node_boot, O_RDWR);
	if (fd < 0) {
		cbd_log("%s open fail err=%d\n", args->cpn->node_boot, fd);
		return fd;
	}

	switch (args->lnk_boot) {
		case LINKDEV_LLI:
			err = ioctl(fd, IOCTL_MODEM_BOOT_ON, NULL);
			if (err < 0) {
				cbd_log("IOCTL_MODEM_BOOT_ON  failed: %d\n", err);
				goto exit;
			}
			break;
		default:
			break;
	}

	err = ioctl(fd, IOCTL_MODEM_OFF);
	if (err) {
		cbd_log("IOCTL_CP_OFF  failed: %d\n", err);
		goto exit;
	}

	/* off EHCI ... */
	memset(&imc_boot, 0x00, sizeof(struct imc_args));
	imc_boot.cbd_args = args;
	cbd_log("CP off -> EHCI off\n");
	err = hsic_usb_host_control(&imc_boot, 0);
	if (err < 0)
		goto exit;

exit:
	if (fd >= 0)
		close(fd);
	return err;
}

static void boot_wake_lock(int lock)
{
	char *path = lock ? "/sys/power/wake_lock" : "/sys/power/wake_unlock";
	char *name = "xmm72xx";
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

int start_xmm72xx_lli_boot(struct boot_args *args)
{
	int err = 0, status;
	struct imc_args imc_boot;

	cbd_log("start\n");
	boot_wake_lock(1);

	memset(&imc_boot, 0x00, sizeof(struct imc_args));
	imc_boot.cbd_args = args;
	imc_boot.boot_fd = open(args->cpn->node_boot, O_RDWR);
	if (imc_boot.boot_fd < 0) {
		cbd_log("%s open fail err=%d\n", args->cpn->node_boot,
			imc_boot.boot_fd);
		err = imc_boot.boot_fd;
		goto exit;
	}

	/* off modem for change OFFLINE if rild reset case */
	status = ioctl(imc_boot.boot_fd, IOCTL_MODEM_STATUS);
	if (status == STATE_ONLINE) {
		cbd_log("xmm626x boot restart but status Online, cp off\n");
		err = ioctl(imc_boot.boot_fd, IOCTL_MODEM_OFF);
		if (err)
			cbd_log("cp off ioctl fail err=%d\n", err);
	}

	imc_boot.bin_fd = open(args->cpn->path_bin, O_RDONLY);
	if (imc_boot.bin_fd < 0) {
		cbd_log("%s open fail err=%d\n", args->cpn->path_bin,
			imc_boot.bin_fd);
		err = imc_boot.bin_fd;
		goto exit;
	}

	err = update_cp_imgmap(&imc_boot);
	if (err < 0) {
		cbd_log("cp img map update fail err=%d\n", err);
		goto exit;
	}
#if 0
	verify_binary_signature(&imc_boot);
#endif

	switch (args->lnk_boot) {
	case LINKDEV_LLI:
		err = lli_xmit_boot(&imc_boot);
		if (err < 0) {
			cbd_log("lli boot fail err=%d\n", err);
			goto exit;
		}
		break;
	default:
		break;
	}

	switch (args->lnk_main) {
	case LINKDEV_LLI:
		err = lli_xmit_main(&imc_boot);
		if (err < 0) {
			cbd_log("lli_xmit_main fail (err=%d)\n", err);
			goto exit;
		}
		break;
	default:
		break;
	}
exit:
	boot_wake_lock(0);
	if (imc_boot.boot_fd >= 0)
		close(imc_boot.boot_fd);
	if (imc_boot.bin_fd >= 0)
		close(imc_boot.bin_fd);
	return err;
}

static const unsigned long crcTable[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

/*!
\brief Function which initializes the CRC tables. Generator polynomial used is 0xEDB88320L.
\return  void
*/

/*!
\brief Function which computes 32 bit CRC checksum for the data.
\param ptr Pointer to data for which crc is to be computed.
\param length Length of data.
\param write_address Address at which the computed checksum  is to be written.
\return  void
*/
static void compute_crc32(unsigned char * ptr, unsigned int length, unsigned char * write_address)
{
	unsigned long crc;
	int c;

	crc = 0xFFFFFFFFL;
	while( length-- )
	  {
	c=*ptr++;
		crc = ((crc>>8) & 0x00FFFFFF) ^ crcTable[ (crc^c) & 0xFF ];
	}
	crc = crc^0xFFFFFFFF;
	write_address[0] = crc&0x000000ff;
	write_address[1] = (crc>>8)&0x000000ff;
	write_address[2] = (crc>>16)&0x000000ff;
	write_address[3] = (crc>>24)&0x000000ff;
	return;
}

#define DUMP_CRC_SIZE	sizeof(unsigned long)

static int check_dump_crc(unsigned long read_crc, char* buf)
{
	unsigned long crc = 0;

	compute_crc32((unsigned char *)buf, READ_BUF_SIZE, (unsigned char *)&crc);

	if (crc != read_crc) {
		cbd_log("read_crc : 0x%lx, dump_crc : 0x%lx\n", crc, read_crc);
		return -1;
	}
	else
		return 1;
}

#define RAMDUMP_USE_ACK_SEQ
static char error_info[ERROR_INFO_SIZE] = {0,};

int start_xmm72xx_lli_dump(struct boot_args *args)
{
	struct imc_args imc_boot = {.bin_fd = -1};
	time_t now;
	struct tm result;
	char log_file_str[256], log_surfix[25], *read_buf = NULL;
	char log_prefix[MAX_PREFIX_LEN];
	int aplog_fd = 0,  fd = 0, log_fd = 0, err_log_fd = 0;
	unsigned magic_key;
	int i = 0, err = -EFAULT, ret;
	int dpkt_size = 0;
	unsigned long dump_crc = 0;

	snprintf(log_prefix, MAX_PREFIX_LEN, "cpcrash");
	time(&now);
	localtime_r(&now, &result);
	strftime(log_surfix, 20, "%y%m%d-%H%M", &result);
	cbd_log("xmm6260_boot_hsic CP upload start %s\n", log_surfix);

	cbd_log("[MIF] <%s> IPC Logger Start!!\n", __func__);
	ret = exec_mif_logger();
	if (ret < 0)
		cbd_log("[MIF] <%s> mif logger failed!\n", __func__);
	else
		cbd_log("[MIF] <%s> mif logger success!!\n", __func__);

	if (args->lnk_boot == LINKDEV_LLI) {
		sprintf(log_file_str,
			"cat /sys/kernel/debug/svnet/mem_dump > %s/mem_dump_%s.log",
			get_log_dir(), log_surfix);
		cbd_log("%s\n", log_file_str);
		system(log_file_str);
	}

	sprintf(log_file_str, "%s/dmesg_%s.log", get_log_dir(), log_surfix);
	cbd_log("%s\n", log_file_str);
	dmesg_to_file(log_file_str);

	/* for cpdump routine debugging */
	cbd_log("open log file - %s\n", log_file_str);
	aplog_fd = open(log_file_str, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (aplog_fd < 0) {
		cbd_log("ap log file open fail err=%d\n", aplog_fd);
		err = aplog_fd;
		goto exit;
	}
	err = lseek(aplog_fd, 0, SEEK_END);
	if (err < 0) {
		cbd_log("aplog file lseek fail\n");
		goto exit;
	}
	dprintf(aplog_fd, "Log file opened\n");

	read_buf = malloc(READ_BUF_SIZE);
	if (!read_buf) {
		cbd_log("log rx buf alloc fail\n");
		dprintf(aplog_fd, "Rx buf alloc fail\n");
		err = -ENOMEM;
		goto exit;
	}

	/*
	 * CP reset and send PSI
	 */
	args->options |= BOPT_CPUPLOAD;
	imc_boot.cbd_args = args;
	imc_boot.boot_fd = open(args->cpn->node_boot, O_RDWR);
	if (imc_boot.boot_fd < 0) {
		cbd_log("%s open fail err=%d\n", args->cpn->node_boot,
			imc_boot.boot_fd);
		dprintf(aplog_fd, "%s open fail err=%d\n", args->cpn->node_boot,
			imc_boot.boot_fd);
		err = imc_boot.boot_fd;
		goto exit;
	}
	fd = imc_boot.boot_fd;

	imc_boot.bin_fd = open(args->cpn->path_bin, O_RDONLY);
	if (imc_boot.bin_fd < 0) {
		cbd_log("%s open fail err=%d\n", args->cpn->path_bin,
			imc_boot.bin_fd);
		dprintf(aplog_fd, "%s open fail err=%d\n", args->cpn->path_bin,
			imc_boot.bin_fd);
		err = imc_boot.bin_fd;
		goto exit;
	}

	err = update_cp_imgmap(&imc_boot);
	if (err < 0) {
		cbd_log("cp img map update fail err=%d\n", err);
		dprintf(aplog_fd, "cp img map update fail err=%d\n", err);
		goto exit;
	}

	/* avoid EHCI off while root hub resumming */
	sleep(2);

	switch (args->lnk_boot) {
	case LINKDEV_LLI:
		err = lli_xmit_boot(&imc_boot);
		break;
	default:
		break;
	}

	if (err < 0) {
		cbd_log("hsic boot fail err=%d\n", err);
		dprintf(aplog_fd, "hsic boot fail err=%d\n", err);
		goto exit;
	}

	/* RPSI_CMD_CODE_TX_EBL */
	cbd_log("send RPSI_CMD_CODE-TX_EBL\n");
	err = s_write(fd, cmd_code_tx_ebl_lli, 8);
	if (err < 0) {
		cbd_log("Send code_tx_ebl fail err=%d\n", err);
		goto exit;
	}

	err = check_ack_val(fd, 0xaa00, 2);
	if (err < 0) {
		cbd_log("ack 0xaa00 fail err=%d\n", err);
		goto exit;
	}

	sprintf(log_file_str, "%s/%s_dump_%s_%s.log", get_log_dir(),
			log_prefix, args->cpn->name, log_surfix);
	cbd_log("%s\n", log_file_str);
	log_fd = open(log_file_str, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (log_fd < 0)
		goto exit;
	sprintf(log_file_str, "%s/%s_info_%s_%s.log", get_log_dir(),
			log_prefix, args->cpn->name, log_surfix);
	cbd_log("%s\n", log_file_str);
	err_log_fd = open(log_file_str, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (err_log_fd < 0)
		goto exit;

	/*
	 * send upload key to cp
	 */
	magic_key = MAGIC_PSI_UPLOAD;
	cbd_log("Send Magic (%x)\n", magic_key);
	err = s_write(fd, &magic_key, sizeof(unsigned));
	if (err < 0) {
		cbd_log("Send Magic (%x) fail err=%d\n", magic_key, err);
		dprintf(aplog_fd, "Send Magic (%x) fail err=%d\n", magic_key,
			err);
		goto exit;
	}
	cbd_log("Send Magic (%x) success\n", magic_key);

	/*
	 * Get CP error summary
	 */
	dprintf(aplog_fd, "get cp error summary\n");
	memset(error_info, 0x00, sizeof(error_info));
	cbd_log("READ Error Message 150Byte\n");
	err = d_read(fd, error_info, ERROR_INFO_SIZE);
	if (err < 0)
		goto exit;
	cbd_log("Read error info size = %d\n", err);
	dprintf(aplog_fd, "Read error info size = %d\n", err);
	err = write(err_log_fd, error_info, ERROR_INFO_SIZE);
	if (err < 0)
		goto exit;
	fsync(err_log_fd);

	cbd_log("READ ERR_MSG success : send PASS ack to CP\n");
	err = write(fd, "PASS", 4 * sizeof(unsigned char));
	if (err < 0) {
		cbd_log("Write failed: %d\n", err);
		goto exit;
	}

	cbd_log("START read/write CPdump info\n");
	dprintf(aplog_fd, "START read/write cpdump info\n");
	memset(read_buf, 0x00, READ_BUF_SIZE);

	while (err) {
		err = enable_crc ? d_read(fd, &dump_crc, sizeof(dump_crc)) : 0;
		if (err < 0) {
			cbd_log("CRC read err\n");
			if (err == -ETIMEDOUT) {
				cbd_log("read 0, end of dump?\n");
				dprintf(aplog_fd, "read 0, end of dump?\n");
				break;
			}

			magic_key = MAGIC_DUMP_NACK;
			cbd_log("Send Magic (%x)\n", magic_key);
			dprintf(aplog_fd, "Send Magic (%x)\n", magic_key);
			err = write(fd, &magic_key, sizeof(unsigned));
			if (err < 0) {
				cbd_log("Send Magic (%x) fail err=%d\n",
					magic_key, err);
				break;
			}
			continue;
		}
		dpkt_size = 0;

		while (dpkt_size < READ_BUF_SIZE) {
			err= d_read(fd, (char *)(read_buf + dpkt_size), READ_BUF_SIZE - dpkt_size);
			if (err < 0) {
				cbd_log("data read err\n");
				if (err == -ETIMEDOUT) {
					cbd_log("read 0, end of dump?\n");
					dprintf(aplog_fd, "read 0, end of dump?\n");
					break;
				}

				magic_key = MAGIC_DUMP_NACK;
				cbd_log("Send Magic (%x)\n", magic_key);
				dprintf(aplog_fd, "Send Magic (%x)\n", magic_key);
				err = write(fd, &magic_key, sizeof(unsigned));
				if (err < 0) {
					cbd_log("Send Magic (%x) fail err=%d\n",
						magic_key, err);
				}
				break;
			}
			dpkt_size += err;
		}

		if (err == -ETIMEDOUT)
			break;
		else if (err < 0)
			continue;

		err = enable_crc ? check_dump_crc(dump_crc, read_buf) : 0;
		if (err < 0) {
			cbd_log("crc mismatch err\n");
			magic_key = MAGIC_DUMP_NACK;
			cbd_log("Send Magic (%x)\n", magic_key);
			dprintf(aplog_fd, "Send Magic (%x)\n", magic_key);
			err = write(fd, &magic_key, sizeof(unsigned));
			if (err < 0) {
				cbd_log("Send Magic (%x) fail err=%d\n",
					magic_key, err);
				break;
			}
			continue;
		}

		i++;
		magic_key = MAGIC_DUMP_ACK;
		err = write(fd, &magic_key, sizeof(unsigned));
		if (err < 0) {
			cbd_log("Send Magic (%x) fail err=%d\n",
				magic_key, err);
			break;
		}
		err = write(log_fd, read_buf, dpkt_size);
		if (err < 0)
			goto exit;
	}

	cbd_log("Written CPdump info count = %d\n", i);
	dprintf(aplog_fd, "Written CPdump info count = %d\n", i);
	fsync(log_fd);
	fsync(aplog_fd);

	cbd_log("Create CPdump success\n");

	sleep(3);
	cbd_log("Go to Upload mode\n");
	err = ioctl(fd, IOCTL_MODEM_CP_UPLOAD, error_info);
	if (err < 0) {
		cbd_log("Kernel upload enter fail\n");
		goto exit;
	}

exit:
	if (aplog_fd >= 0) {
		fsync(aplog_fd);
		close(aplog_fd);
	}
	if (imc_boot.bin_fd >= 0)
		close(imc_boot.bin_fd);
	if (log_fd >= 0)
		close(log_fd);
	if (err_log_fd >= 0)
		close(err_log_fd);
	close(fd);
	if (read_buf)
		free(read_buf);
	return err;
}
