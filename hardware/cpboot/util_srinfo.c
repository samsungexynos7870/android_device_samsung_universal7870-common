#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/ioctl.h>
#include <sys/stat.h>

#include "boot.h"
#include "util.h"
#include "util_srinfo.h"

static char tochar(char x)
{
	return (x > 0x21 && x < 0x7E) ? x : '.';
}

static int create_srinfo_directory(char *path)
{
	struct stat ldir_st;
	int ret;

	/* create log directory - /data/log */
	ret = stat(path, &ldir_st);
	if (!ret) { /* path exist */
		if (!S_ISDIR(ldir_st.st_mode)) {
			cbd_log("(%s) is not a directory\n", path);
			ret = -EPERM;
			goto exit;
		}
	} else {
		ret = mkdir(path, 0775);
		if (ret) {
			cbd_log("log path create fail(%d/%s)\n", ret, path);
			goto exit;
		}
		cbd_log("log path (%s) created\n", path);
	}
exit:
	return ret;
}

static struct shmem_srinfo *alloc_srinfo(unsigned *len, int boot_fd)
{
	struct shmem_srinfo *args;
	unsigned *magic;
	int ret;

	args = (struct shmem_srinfo *)malloc(SRINFO_READ_SIZE);
	if (!args) {
		cbd_log("read buf alloc(%d) fail\n", SRINFO_READ_SIZE);
		return NULL;
	}
	memset(args, 0x00, SRINFO_READ_SIZE);
	args->size = SRINFO_READ_SIZE - sizeof(unsigned);

	ret = ioctl(boot_fd, IOCTL_MODEM_GET_SHMEM_SRINFO, args);
	if (ret < 0) {
		cbd_log("ioctl fail - Get srinfo(%d)\n", ret);
		goto exit;
	}
	*len = args->size;

	/* srinfo has "{JVER:" format from offset 0x10 */
	magic = (unsigned *)(args->buf + 0x12);
	if (*magic != 'REVJ') {
		cbd_log("Crash info string was invalid(%x/%x)\n", *magic, 'REVJ');
		goto exit;
	}

	return args;
exit:
	free(args);
	return NULL;
}

/* open the srinfo file to save the crash info and check the file size,
   if it was bigger than MAX_SIZE, rename to bak file and open new file */
static int open_srinfo_file(char *surfix)
{
	int ret, fd;
	char infofile[64], infobak[64];
	struct stat sb;

	sprintf(infofile, "%s/%s_%s", SRINFO_PATH, SRINFO_FILE, surfix);
	sprintf(infobak, "%s/.%s_%s.bak", SRINFO_PATH, SRINFO_FILE, surfix);
	cbd_log("open file - %s(%s)\n", infofile, infobak);
	ret = fd = open(infofile,
			O_WRONLY | O_APPEND | O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (ret < 0) {
		cbd_log("open fail - %s\n", infofile);
		goto exit;
	}

	/* check file size */
	memset(&sb, 0x00, sizeof(struct stat));
	ret = fstat(fd, &sb);
	if (ret) {
		cbd_log("stat fail - srinfo log\n");
		close(fd);
		goto exit;
	}
	ret = fd;

	cbd_log("srinfo log size : %lu(0x%lx/0x%x)\n",
			sb.st_size, sb.st_size, SRINFO_MAX_SIZE);

	/* file size limit */
	if (sb.st_size > SRINFO_MAX_SIZE) {
		close(fd);
		ret = unlink(infobak);
		if (ret < 0 && ret != -EPERM) {
			cbd_log("deleate (%s) file fail(%d)\n", infobak, ret);
			goto exit;
		}
		ret = rename(infofile, infobak);
		if (ret < 0) {
			cbd_log("rename (%s -> %s) file fail(%d)\n",
					infofile, infobak, ret);
			goto exit;
		}
		ret = fd = open(infofile,
				O_WRONLY | O_APPEND | O_CREAT,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if (fd < 0) {
			cbd_log("srinfo open fail(%d)\n", fd);
			goto exit;
		}
	}
exit:
	return ret;
}

static unsigned write_srinfo_file(int fd, char *buf, unsigned size)
{
	char timestr[MAX_SUFFIX_LEN], ascii[17];
	time_t now;
	struct tm result;
	unsigned i, linelen = 16;

	time(&now);
	localtime_r(&now, &result);
	strftime(timestr, MAX_SUFFIX_LEN, "%Y-%m-%d %H:%M:%S", &result);
	dprintf(fd, "[%s]\n", timestr);

	cbd_log("store to hex ascii text size=(0x%x)\n", (unsigned)size);
	/*
	   ret = write(fd, buf, size);
	   if (ret <= 0) {
	   cbd_log("raw data write fail\n");
	   goto exit_free;
	   }
	 */
	for (i = 0; i < size; i += linelen) {
		unsigned j;
		char *rp = buf + i;
		int *rpr = (int *)rp;


		for (j = 0; j < linelen; j++)
			ascii[j] = tochar(*(rp + j));
		ascii[16] = '\0';

		/*
		   cbd_log("line = 0x%04x: %s\n", i, ascii);
		 */

		dprintf(fd, "%04x:%08x %08x %08x %08x %s\n", i, *rpr,
				*(rpr + 1), *(rpr + 2), *(rpr + 3), ascii);
	}
	dprintf(fd, "\n");
	cbd_log("store done\n");

	return size;
}

/* store raw srinfo to file */
static int open_srinfo_last(char *surfix)
{
	int fd, ret = -1;
	char file[64];

	sprintf(file, "%s/%s_%s", SRINFO_LAST_PATH, SRINFO_LAST, surfix);
	cbd_log("open last_sr_info file - %s\n", file);
	fd = open(file, O_RDWR);
	if (fd < 0) {
		cbd_log("last_sr_info open fail(%d)\n", fd);
		goto exit;
	}

	ret = lseek(fd, 0, SEEK_SET);
	if (ret < 0) {
		cbd_log("fd_last lseek fail\n");
		close(fd);
		goto exit;
	}
	return fd;
exit:
	return ret;
}

/* store raw srinfo to file */
static int create_srinfo_last(char *surfix)
{
	int fd, ret = -1;
	char file[64];

	sprintf(file, "%s/%s_%s", SRINFO_LAST_PATH, SRINFO_LAST, surfix);
	cbd_log("create last_sr_info file - %s\n", file);
	fd = open(file,
		O_RDWR | O_CREAT,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd < 0) {
		cbd_log("last_sr_info create fail(%d)\n", fd);
		goto exit;
	}

	ret = lseek(fd, 0, SEEK_SET);
	if (ret < 0) {
		cbd_log("fd_last lseek fail\n");
		close(fd);
		goto exit;
	}
	return fd;
exit:
	return ret;
}

static int write_srinfo_last(int fd, char *buf, unsigned size)
{
	unsigned i;
	char *rdbuf = buf + 0x10;

	cbd_log("store to ascii text\n");

	for (i = 0; i < size; i++) {
		char *rp = rdbuf + i;

		if (*rp == '}')
			break;
	}
	return write(fd, rdbuf, i+1);
}

/* check srinfo file */
static int check_srinfo_last(char *surfix)
{
	struct stat ldir_st;
	char file[64];

	sprintf(file, "%s/%s_%s", SRINFO_LAST_PATH, SRINFO_LAST, surfix);
	cbd_log("%s: full_path: %s\n", __func__, file);

	return stat(file, &ldir_st);
}

void store_srinfo(char *surfix, int dev_fd)
{
	int ret, fd;
	unsigned size;
	struct shmem_srinfo *rdbuf;

	ret = check_srinfo_last(surfix);
	if (!ret) {
		cbd_log("srinfo_last file exist, skip!\n");
		return;
	}

	rdbuf = alloc_srinfo(&size, dev_fd);
	if (!rdbuf) {
		cbd_log("alloc srinfo fail\n");
		return;
	}

	ret = create_srinfo_directory(SRINFO_PATH);
	if (ret < 0)
		goto exit;

	ret = create_srinfo_directory(SRINFO_LAST_PATH);
	if (ret < 0)
		goto exit;

	fd = create_srinfo_last(surfix);
	if (fd < 0) {
		cbd_log("sr_info_last open fail(%d)\n", fd);
		goto exit;
	}
	ret = write_srinfo_last(fd, rdbuf->buf, size);
	if (ret < 0) {
		cbd_log("srinfo_last write fail(%d)\n", ret);
		close(fd);
		goto exit;
	}
	close(fd);

	fd = open_srinfo_file(surfix);
	if (fd < 0) {
		cbd_log("sr_info_last create fail(%d)\n", fd);
		goto exit;
	}
	ret = write_srinfo_file(fd, rdbuf->buf, size);
	if (ret < 0) {
		cbd_log("srinfo write fail(%d)\n", ret);
		close(fd);
		goto exit;
	}
	close(fd);

exit:
	free(rdbuf);
}

void restore_srinfo(char *surfix, int dev_fd)
{
	int ret, fd, size;
	char *rdbuf, filepath[64], filebak[64];
	struct shmem_srinfo *sr_args;

	fd = open_srinfo_last(surfix);
	if (fd < 0) {
		cbd_log("sr_info_last open fail(%d)\n", fd);
		return;
	}

	rdbuf = malloc(SRINFO_READ_SIZE);
	if (!rdbuf) {
		cbd_log("read buf alloc(%d) fail\n", SRINFO_READ_SIZE);
		goto exit;
	}
	memset(rdbuf, 0x00, SRINFO_READ_SIZE);

	sr_args = (struct shmem_srinfo *)rdbuf;
	size = read(fd, sr_args->buf, SRINFO_READ_SIZE - sizeof(unsigned));
	if (size <= 0) {
		cbd_log("last_sr_info file read fail(%d)\n", size);
		goto exit_free;
	}
	sr_args->size = size;

	ret = ioctl(dev_fd, IOCTL_MODEM_SET_SHMEM_SRINFO, sr_args);
	if (ret < 0) {
		cbd_log("ioctl fail - Set srinfo(%d)\n", ret);
		goto exit_free;
	}

#ifndef DEBUG
	sprintf(filepath, "%s/%s_%s", SRINFO_LAST_PATH, SRINFO_LAST, surfix);
	ret = unlink(filepath);
	if (ret < 0 && ret != -EPERM) {
		cbd_log("deleate (%s) file fail(%d)\n", filebak, ret);
		goto exit_free;
	}
#else
	sprintf(filepath, "%s/%s_%s", SRINFO_LAST_PATH, SRINFO_LAST, surfix);
	sprintf(filebak, "%s/%s_%s.bak", SRINFO_LAST_PATH, SRINFO_LAST, surfix);

	ret = unlink(filebak);
	if (ret < 0 && ret != -EPERM) {
		cbd_log("deleate (%s) file fail(%d)\n", filebak, ret);
		goto exit_free;
	}

	ret = rename(filepath, filebak);
	if (ret < 0) {
		cbd_log("rename (%s -> %s) file fail(%d)\n",
				filepath, filebak, ret);
		goto exit_free;
	}
#endif
	cbd_log("done = %s\n", rdbuf);

exit_free:
	free(rdbuf);
exit:
	close(fd);
}
