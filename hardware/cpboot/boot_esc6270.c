/*
 * esc6270 boot process
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
#include "esc6270.h"
#include "util.h"
#include "hdlc.h"

static int g_type_esc;

/*
 *	UART helper functions
 */
static int open_uart(char *dev, char*baudr, char*par, char*bits,
                char*stop, int hwf, int swf)
{
	int fp = open_serial(dev, baudr, par, bits, stop, hwf, swf);
	cbd_log("%s: name: %s\n", __FUNCTION__, dev);

	if (fp < 0)
	{
		cbd_log("%s: [%s] open - fd: %d, errno: [0x%x,%s]\n", __FUNCTION__, dev, fp, errno, strerror(errno) );
		/*perror(dev);*/
		return -1;
	}

	cbd_log("open uart success\n");
	return fp;
}

void close_uart(int fp)
{
	cbd_log("close uart %d\n", fp);

	if (fp < 0)
		return;

#if 0	// block this. sometimes tcsetattr blocks long time.
    cbd_log("tcsetattr - start");
    if (tcsetattr(fp, TCSAFLUSH, &serial_old) < 0)
        cbd_log("tcsetattr error");
#endif

	cbd_log("close uart - start\n");
	close(fp);
	cbd_log("close uart success\n");
}

/* Write exactly len bytes (Signal safe)*/
static inline int write_n(int fd, char *buf, int len)
{
    register int t = 0, w;

    while (len > 0)
    {
        if ((w = write(fd, buf, len)) < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return -1;
        }

        if (!w)
            return 0;

        len -= w;
        buf += w;
        t += w;
    }

    return t;
}

static int create_default_nv_data(int fd_pimg, const char *filename)
{
    int fd_nv = 0;
    int ret = 0;

    char *nvdata = NULL;

    cbd_log("%s: \n",__FUNCTION__ );

    if ((fd_nv = open(filename, O_RDWR | O_CREAT | O_TRUNC | O_SYNC, FILE_MODE)) < 0) {
        fprintf(stderr, "create %s failed\n", filename);
        return -1;
    }
    cbd_log("=> create new nv_data file(%s).\n", filename);

    nvdata = (char *)malloc(MODEM_NV_SIZE);
    if (nvdata == NULL)
    {
        cbd_log("malloc fail in %s", __FUNCTION__);
        close(fd_nv);
        return -1;
    }

    memset(nvdata, 0xFF, MODEM_NV_SIZE);
    if (lseek(fd_pimg, MODEM_NV_OFFSET, SEEK_SET) <0 ) {
        cbd_log("lseek failed\n");
        close(fd_nv);
        free(nvdata);
        return -1;
    }

    ret = read(fd_pimg, nvdata, MODEM_NV_SIZE);
    if (ret != MODEM_NV_SIZE) {
        cbd_log("%s: error. read %d byte from modem partition\n", __FUNCTION__, ret);
    }

    ret = write_n(fd_nv, nvdata, MODEM_NV_SIZE);
    cbd_log("%s: write %d byte to %s\n", __FUNCTION__, ret, filename);

    fsync(fd_nv);
    close(fd_nv);

    if (nvdata) {
        free(nvdata);
        nvdata = NULL;
    }

#if 0	// do not enable
    /* create MD5 */
    if (md5_state == MD5_ON)
        refresh_md5_file(NV_BIN_PATH);

    WriteLogOnEFS(PATH_FOR_NV_LOG, "default NV restored");
#endif

    return 0;
}

static int read_bootloader(struct qc_args *qc_boot)
{
	int fd_pimg, ret;
	char *path = qc_boot->cbd_args->cpn->path_bin;

	fd_pimg = open(path, O_RDONLY);
	if (fd_pimg < 0)
	{
		cbd_log("%s: can't open a phone image from (%s). %s\n",
				__FUNCTION__, path, strerror(errno));
		return -1;
	}

	ret = read(fd_pimg, boot_data[g_type_esc].tx_pkt.buf, MAX_DBL_SIZE);

	close(fd_pimg);

	cbd_log("dbl read size= 0x%x\n", ret);
	return 0;
}

static int dload_uart_init(void)
{
	char *filename = UART_PATH;

	boot_data[g_type_esc].fd_uart0 = open_uart(filename, "115200", "N", "8", "1", 0, 0);

	if (boot_data[g_type_esc].fd_uart0 < 0)
	{
		cbd_log("%s open failed! %s.\n", filename, strerror(errno));
		return -1;
	}
	cbd_log("%s open success!\n", filename);
	return 0;
}

static int check_args_for_esc6270(struct boot_args *args)
{
	if (args->type != QC_ESC6270)
		return -1;

	if (args->lnk_boot != LINKDEV_UART)
		return -1;

	if (args->lnk_main != LINKDEV_DPRAM && args->lnk_main != LINKDEV_PLD)
		return -1;

	return 0;
}

static int xmit_bootloader(struct qc_args *qc_boot)
{
	unsigned long len;
	unsigned long i, len_inc;

	// init uart
	dload_uart_init();

	// init uart protocal
	dload_hdlc_init(g_type_esc, MAX_DBL_SIZE, DBL_START_ADDR, 1);
	dload_packet_init(g_type_esc);

	// send first packet.
	nop_req(g_type_esc);
	cbd_log("send CMD_NOP request success.\n");

	boot_data[g_type_esc].exit = 0;

	for (;;)
	{
		if (boot_data[g_type_esc].exit)
		{
			cbd_log("g_exit is true : break!\n");
			break;
		}

		len = dload_get_data_from_device (g_type_esc);

		for (i=0, len_inc = boot_data[g_type_esc].packet_data.length; i<len; i++, len_inc++)
		{
			dload_hdlc_handle_incoming_byte(g_type_esc, boot_data[g_type_esc].packet_data.buffer[len_inc]);
		}

		dload_process_incoming_packets(g_type_esc);
	}

	return boot_data[g_type_esc].exit;
}

static int xmit_nvdata(const char *nvfile, int fd, unsigned char *onedram)
{
	int len = 0, ret = 0;
	int nvfd = -1;

	cbd_log("%s:\n", __func__);

#if 0 /* ifdef NV_BACKUP_AND_INTEGRITY */
	load_md5_state();

	check_nv_data_validity();
#endif

	cbd_log("%s: loading %s...", __func__, nvfile);

	nvfd = open(nvfile, O_RDWR | O_NDELAY);
	if (nvfd < 0) {
		cbd_log("nvdata file open failed. %s.\n", strerror(errno));

		nvfd = create_default_nv_data(fd, nvfile);
		if (nvfd < 0) {
			cbd_log("%s: create default nv data failed. ret %d, %s.\n", __func__, nvfd, strerror(errno));
			return -1;
		}

		nvfd = open(nvfile, O_RDWR | O_NDELAY);
		if (nvfd < 0) {
			cbd_log("%s: open default nv data failed. ret %d, %s.\n", __func__, nvfd, strerror(errno));
			return -1;
		}
	}

	/* read NV data from /efs/nv_data.bin to onedram-mapped region */
	len = read(nvfd, onedram, MODEM_NV_SIZE);
	if (len != MODEM_NV_SIZE) {
		cbd_log("nvdata read failed, len %d. %s.\n", len, strerror(errno));
		ret = -1;
		goto close_fd;
	} else {
		ret = 0;
	}

	cbd_log("%s: %d bytes written to %p\n", __func__, len, onedram);

close_fd:
	close(nvfd);
	return ret;
}

static int xmit_modem(struct qc_args *qc_boot)
{
	int fd = -1;
	unsigned char *img = NULL;
	struct _param_nv pnv;
	int ret = 0;
	int i = 0;
	int dpram_buffer_size;

	if(qc_boot->cbd_args->lnk_boot == LINKDEV_DPRAM)	{
		cbd_log("LINKDEV_DPRAM\n");
		dpram_buffer_size = DPRAM_BUFFER_SIZE;
	}
	else if(qc_boot->cbd_args->lnk_boot == LINKDEV_PLD)	{
		cbd_log("LINKDEV_PLD\n");
		dpram_buffer_size = PLD_BUFFER_SIZE;
	}
	else	{
		cbd_log("Default LINKDEV_PLD\n");
		dpram_buffer_size = PLD_BUFFER_SIZE;
	}

	unsigned int quotient = MODEM_CODE_SIZE_FRAME/dpram_buffer_size;
	unsigned int remainder = ((MODEM_CODE_SIZE_FRAME%dpram_buffer_size) ? 1 : 0);
	unsigned int count = 1;

	cbd_log("%s() + dpram_buffer_size(%d), quotient(%d)\n", __func__, dpram_buffer_size, quotient);

	const char *img_file = qc_boot->cbd_args->cpn->path_bin;
	int onedram_fd = qc_boot->boot_fd;
	int len;

	cbd_log("[%s] fd %d, Open image_file : %s\n", __func__, onedram_fd, img_file);

	fd = open(img_file, O_RDONLY);
	if (fd == -1) {
		cbd_log("open image\n");
		return -1;
	}

	img = (unsigned char *)malloc(MODEM_CODE_SIZE);
	if (img == NULL) {
		cbd_log("failed code image memory\n");
		goto close;
	}

	cbd_log("%s() == STEP1 ==\n", __func__);
	memset(img, 0, MODEM_CODE_SIZE);

	len = read(fd, img, MODEM_CODE_SIZE);

	cbd_log("%s : Read esc_modem bianry. Len = 0x%x\n", __func__, len);
	if (len != MODEM_CODE_SIZE) {
		cbd_log("%s : image read failed. errno %d, %s\n", __func__, errno, strerror(errno));
		cbd_log("image read failed\n");
		goto close;
	}

	cbd_log("%s() == STEP2 ==\n", __func__);
	ret = ioctl(onedram_fd, IOCTL_DPRAM_PHONE_POWON, NULL);
	if (ret < 0) {
		cbd_log("%s: ioctl(DPRAM_PHONE_POWON) failed. %s.\n", __func__, strerror(errno));
		goto close;
	}

	for (i = 0; i < MODEM_CODE_SIZE_DIVIDER; i++) {
		cbd_log("%s() == STEP3 == i %d\n", __func__, i);
		pnv.addr = img+i*MODEM_CODE_SIZE_FRAME;
		pnv.size = MODEM_CODE_SIZE_FRAME;
		pnv.count = count;
		ret = ioctl(onedram_fd, IOCTL_DPRAM_PHONEIMG_LOAD, &pnv);
		if (ret < 0) {
			cbd_log("%s(%d): ioctl(DPRAM_PHONEIMG_LOAD) failed. %s.\n",
							__func__, i, strerror(errno));
			goto close;
		}
		count += (quotient+remainder);
		cbd_log("count: (%d), quotient: (%d), remainder: (%d)\n", count, quotient, remainder);
	}
	memset(img, 0, MODEM_NV_SIZE);

	ret = xmit_nvdata(NVDATA_FILE, fd, img);
	if (ret < 0) {
		cbd_log("GSM NV data load fail. ret %d\n", ret);
		goto close;
	}

	pnv.addr = img;
	pnv.size = MODEM_NV_SIZE;

	ret = ioctl(onedram_fd, IOCTL_DPRAM_NVDATA_LOAD, &pnv);
	if (ret < 0) {
		cbd_log("%s: ioctl(DPRAM_NVDATA_LOAD) failed. %s.\n", __func__, strerror(errno));
		goto close;
	}

	cbd_log("GSM NV data load done\n");

	ret = ioctl(onedram_fd, IOCTL_DPRAM_PHONE_BOOTSTART, NULL);
	if (ret < 0) {
		cbd_log("%s: ioctl(DPRAM_PHONE_BOOTSTART) failed. %s.\n", __func__, strerror(errno));
		goto close;
	}

	cbd_log("DPRAM_PHONE_BOOTSTART\n");

close:
	if (img != NULL)
		free(img);

	close(fd);
	return ret;
}

int start_esc6270_boot(struct boot_args *args)
{
	cbd_log("<%s> start. type %d\n", __FUNCTION__, args->type);

	return 0;
}

int esc6270_dload(struct boot_args *args)
{
	int err = -1;
	struct qc_args qc_boot = {.boot_fd = -1};

	cbd_log("<%s> start. type %d\n", __FUNCTION__, args->type);

	g_type_esc = args->type;

	// check args first
	if (check_args_for_esc6270(args) < 0)	{
		cbd_log("%s args err, type(%d)\n", __func__, args->type);
		goto exit;
	}

/*
	cbd_log("Sleep(3)!!!!!\n");
	sleep(10);
*/

	// open boot node. ex) "/dev/gsm_boot0"
	memset(&qc_boot, 0x00, sizeof(struct qc_args));
	qc_boot.cbd_args = args;
	qc_boot.boot_fd = open(args->cpn->node_boot, O_RDWR);

	if (qc_boot.boot_fd < 0) {
		cbd_log("%s open fail err=%d\n", args->cpn->node_boot,
			qc_boot.boot_fd);
		err = qc_boot.boot_fd;
		goto exit;
	}

	// read bootloader
	err = read_bootloader(&qc_boot);
	if (err < 0) {
		cbd_log("%s: read_bootloader failed.\n", __func__);
		goto exit;
	}

	// turn on the CP
	err = ioctl(qc_boot.boot_fd, IOCTL_MODEM_BOOT_ON, NULL);
	if (err < 0) {
		cbd_log("%s: IOCTL_MODEM_BOOT_ON failed.\n", __func__);
		goto exit;
	}

	// xmit bootloader(dbl)
	err = xmit_bootloader(&qc_boot);
	if (err < 0) {
		cbd_log("%s: xmit_bootloader failed.\n", __func__);
		goto exit;
	}

	//
	err = ioctl(qc_boot.boot_fd, IOCTL_MODEM_BOOT_OFF, NULL);
	if (err < 0) {
		cbd_log("%s: IOCTL_MODEM_BOOT_OFF failed.\n", __func__);
		goto exit;
	}

	// xmit modem(amss)
	err = xmit_modem(&qc_boot);
	if (err < 0) {
		cbd_log("%s: xmit_modem failed.\n", __func__);
		goto exit;
	}

exit:
	if (qc_boot.boot_fd > 0)
		close(qc_boot.boot_fd);
	if (qc_boot.bin_fd > 0)
		close(qc_boot.bin_fd);
	return err;
}

int cp_ramdump_esc6270_retry(struct boot_args *args)
{
    int err = 0;
    time_t now;
    struct tm result;
    char log_file[256];
    char read_buf[0x7C00]; /* 31KB = 31744byte = 0x7C00 */
    int log_fd = -1;
    unsigned int i = 0;
    char command[512];
    char logfile_dumpstate[256];
    char log_prefix[MAX_PREFIX_LEN];
    char log_suffix[MAX_SUFFIX_LEN];
    struct _param_nv pnv;

	int onedram_fd = 0;

	cbd_log("esc6270 CP upload start ..\n");

	struct qc_args qc_boot;

	cbd_log("<%s> start. type %d\n", __FUNCTION__, args->type);

	g_type_esc = args->type;

	// check args first
	if (check_args_for_esc6270(args) < 0)	{
		cbd_log("%s args err, type(%d)\n", __func__, args->type);
		goto end;
	}

/*
	cbd_log("Sleep(3)!!!!!\n");
	sleep(10);
*/

	// open boot node. ex) "/dev/gsm_boot0"
	memset(&qc_boot, 0x00, sizeof(struct qc_args));
	qc_boot.cbd_args = args;
	qc_boot.boot_fd = open(args->cpn->node_boot, O_RDWR);

	if (qc_boot.boot_fd < 0) {
		cbd_log("%s open fail err=%d\n", args->cpn->node_boot,
			qc_boot.boot_fd);
		err = qc_boot.boot_fd;
		goto end;
	}

	// read bootloader
	err = read_bootloader(&qc_boot);
	if (err < 0) {
		cbd_log("%s: read_bootloader failed.\n", __func__);
		goto end;
	}

	// turn on the CP
	err = ioctl(qc_boot.boot_fd, IOCTL_MODEM_BOOT_ON, NULL);
	if (err < 0) {
		cbd_log("%s: IOCTL_MODEM_BOOT_ON failed.\n", __func__);
		goto end;
	}

	// xmit bootloader(dbl)
	err = xmit_bootloader(&qc_boot);
	if (err < 0) {
		cbd_log("%s: xmit_bootloader failed.\n", __func__);
		goto end;
	}

	onedram_fd = qc_boot.boot_fd;

	if (onedram_fd< 0) {
		cbd_log("%d open fail err\n", qc_boot.boot_fd);
		goto end;
	}


    cbd_log("ESC BOOT Download Done\n");
    err = ioctl(onedram_fd, DPRAM_PHONE_UPLOAD_STEP1, NULL);
    if (err < 0) {
        cbd_log("phone uload step1  failed: %d, %s\n", err, strerror(errno));
        goto end;
    }
/*
    RilLog("Create DUMP file to /data/log/dumpstates.txt\n");
    system("dumpstate > /data/log/dumpstates.txt");
*/
    snprintf(log_prefix, MAX_PREFIX_LEN, "cpcrash");
    time(&now);
    localtime_r(&now, &result);
    strftime(log_suffix, MAX_SUFFIX_LEN, "%y%m%d%H%M", &result);
    sprintf(log_file, "/sdcard/log/%s_dump_%s_%s.log",
		    log_prefix, args->cpn->name, log_suffix);
    cbd_log("Create CP DUMP file to %s\n", log_file);
    log_fd = open(log_file, O_WRONLY|O_CREAT, 0644);
    if (log_fd < 0) {
        cbd_log("open for log_fd  failed: %d, %s\n", log_fd, strerror(errno));
        goto end;
    }

    i = 0;
    while (1) {
        memset(read_buf, 0, sizeof(read_buf));
        pnv.addr= (unsigned char *)read_buf;
        err = ioctl(onedram_fd, DPRAM_PHONE_UPLOAD_STEP2, &pnv);
        if (err < 0) {
            cbd_log("phone uload step2  failed: %d, %s\n", err, strerror(errno));
            goto end;
        }

        err = write(log_fd, read_buf, pnv.size);
        if (err < 0)    {
            cbd_log("write failed: %d, %s\n", err, strerror(errno));
            goto end;
        } else {
            i++;
            if (!(i % 500)) {
                    cbd_log("CPdump info write count= %d\n", i);
            }
            if (pnv.count != i) {
                cbd_log("Receive invalid count = %d, pnv.count = %d\n", i, pnv.count);
                goto end;
            }
            if (pnv.tag == 4) {
                cbd_log("Receive last tag = %d\n", pnv.tag);
                sync();
                break;
            }
        }
    }


    if (log_fd >= 0) {
        close(log_fd);
        log_fd = -1;
    }

    usleep(500000);

    memset(command, 0, sizeof(command));
    sprintf(command, "chmod 0777 %s", log_file);
    cbd_log("changemode ramdump file : [%s]\n", command);
    system(command);

    usleep(1000);

    memset(logfile_dumpstate, 0, sizeof(logfile_dumpstate));
    memset(command, 0, sizeof(command));
    localtime_r(&now, &result);
    strftime(logfile_dumpstate, 80, "/sdcard/log/cpdump_dumpState_%y%m%d%H%M.txt", &result);
    sprintf(command, "dumpstate > %s", logfile_dumpstate);
    cbd_log("dumpstate ramdump file : [%s]\n", command);
    system(command);

    usleep(3000);

    memset(command, 0, sizeof(command));
    sprintf(command, "chmod 0777 %s", logfile_dumpstate);
    cbd_log("changemode ramdump dumpstate file : [%s]\n", command);
    system(command);

    sleep(3);

    cbd_log("Go to Upload mode\n");

    err = ioctl(onedram_fd, IOCTL_MODEM_CP_UPLOAD, ": ESC6270");

    if (err < 0) {
        cbd_log("phone go to upload failed: %d, %s\n", err, strerror(errno));
        goto end;
    }

end:
    /* ehci_runtime_forbid(0); */

    cbd_log("EXIT : cp_ramdump_esc6270\n");
    if (log_fd >= 0) {
        close(log_fd);
    }

    close(onedram_fd);

    return err;
}

int start_esc6270_dump(struct boot_args *args)
{
    int cnt = 5;
    int n = -1;

    cbd_log("[SHAWN]%s() 1\n", __FUNCTION__);

    n = cp_ramdump_esc6270_retry(args);
    if (n >= 0) {
        return n;
    }
    cnt--;

    while (cnt--) {
        n = cp_ramdump_esc6270_retry(args);
        if (n < 0) {
            sleep(2);
            continue;
        } else {
            break;
        }
    }

    return n;
}

int shutdown_esc6270_modem(struct boot_args *args)
{
	return 0;
#if 0
	int fd = 0;
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
	close(fd);
	return err;
#endif
}
