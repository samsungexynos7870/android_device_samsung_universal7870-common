#include <stdio.h>	/* printf */
#include <string.h>	/* strlen */
#include <stdlib.h>	/* malloc */
#include <string.h>
#include <fcntl.h>	/* O_RDWR,O_NOCTTY,O_NONBLOCK */
#include <termios.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>	/* scandir */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/klog.h>
#include <sys/statfs.h>
#include <sys/vfs.h>
#include <dirent.h>

#include "util.h"
#include "boot.h"

#if 1
/*============================================================================*\
	Definitions for UART
\*============================================================================*/
#endif

static struct termios serial_old;

/* Set hardware flow control. */
static void m_sethwf(int fd, int on)
{
	struct termios tty;
	tcgetattr(fd, &tty);

	if (on)
		tty.c_cflag |= CRTSCTS;
	else
		tty.c_cflag &= ~CRTSCTS;

	tcsetattr(fd, TCSANOW, &tty);
}

/* Set RTS line. Sometimes dropped. Linux specific? */
static void m_setrts(int fd)
{
	int mcs;

	if (0 == ioctl(fd, TIOCMODG, &mcs)) {
		mcs |= TIOCM_RTS;
		ioctl(fd, TIOCMODS, &mcs);
	} else {
	/* TODO: add exception handler ..
		mcs |= TIOCM_RTS;
		ioctl(fd, TIOCMODS, &mcs);
	*/
	}
}

static int m_setparms(int fd, char *baudr, char *par, char *bits, char *stop, int hwf, int swf)
{
	int spd = -1;
	int newbaud;
	int bit = bits[0];
	int stop_bit = stop[0];

	struct termios tty;

	if (tcgetattr(fd, &tty) < 0) {
		perror("tcgetattr");
		return -1;
	}
	if (tcgetattr(fd, &serial_old) < 0) {
		perror("tcgetattr");
	}

//	fprintf(stderr, "[%s:%s%s%s]\n", baudr, bits, par, stop);
//	fprintf(stderr, "[S/W flow control : %s]\n", swf ? "on":"off");
//	fprintf(stderr, "[H/W flow control : %s]\n", hwf ? "on":"off");

	fflush(stdout);

	/* We generate mark and space parity ourself. */
	if (bit == '7' && (par[0] == 'M' || par[0] == 'S'))
		bit = '8';

	/* Check if 'baudr' is really a number */
	if ((newbaud = (atol(baudr) / 100)) == 0 && baudr[0] != '0')
		newbaud = -1;

	switch (newbaud) {
		case 0: spd = 0; break;
		case 3: spd = B300; break;
		case 6: spd = B600; break;
		case 12: spd = B1200; break;
		case 24: spd = B2400; break;
		case 48: spd = B4800; break;
		case 96: spd = B9600; break;
		case 192: spd = B19200; break;
		case 384: spd = B38400; break;
		case 576: spd = B57600; break;
		case 1152: spd = B115200; break;
		case 4608: spd = B460800; break;
		case 9216: spd = B921600; break;
		case 30000: spd = B3000000; break;
		case 40000: spd = B4000000; break;
	}

	if (spd != -1) {
		cfsetospeed(&tty, (speed_t) spd);
		cfsetispeed(&tty, (speed_t) spd);
	}

	switch (bit) {
		case '5': tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS5; break;
		case '6': tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS6; break;
		case '7': tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS7; break;
		case '8':
		default: tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; break;
	}

	switch (stop_bit) {
		case '1': tty.c_cflag &= ~CSTOPB; break;
		case '2':
		default : tty.c_cflag |= CSTOPB; break;
	}

	/* Set into raw, no echo mode */
	tty.c_iflag = IGNBRK;
	tty.c_lflag = 0;
	tty.c_oflag = 0;
	tty.c_cflag |= CLOCAL | CREAD;
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 1;

	if (swf)
		tty.c_iflag |= IXON | IXOFF;
	else
		tty.c_iflag &= ~(IXON | IXOFF | IXANY);

	tty.c_cflag &= ~(PARENB | PARODD);

	if (par[0] == 'E')
		tty.c_cflag |= PARENB;
	else if (par[0] == 'O')
		tty.c_cflag |= (PARENB | PARODD);

	if (tcsetattr(fd, TCSANOW, &tty) < 0) {
		perror("tcsetattr");
		return -1;
	}

	m_setrts(fd);

	m_sethwf(fd, hwf);

	return 0;
}

int open_serial(const char *dev, char *baudr, char *par, char *bits,
		char *stop, int hwf, int swf)
{
	int fp = open(dev, O_RDWR);

	if (fp < 0) {
		perror(dev);
		return -1;
	}
	if (m_setparms(fp, baudr, par, bits, stop, hwf, swf) < 0) {
		close(fp);
		return -1;
	}
	return fp;
}

int open_serial_nonblockingmode(const char *dev, char *baudr, char *par, char *bits,
		char *stop, int hwf, int swf)
{
	int fp = open(dev, O_NONBLOCK | O_RDWR);

	if (fp < 0) {
		perror(dev);
		return -1;
	}
	if (m_setparms(fp, baudr, par, bits, stop, hwf, swf) < 0) {
		close(fp);
		return -1;
	}
	return fp;
}

void close_serial(int fp)
{
    if (fp < 0)
        return;

    if (tcsetattr(fp, TCSAFLUSH, &serial_old) < 0)
        perror("tcsetattr");

    close(fp);
}

#if 1
/*============================================================================*\
	Definitions for MIF Logger
\*============================================================================*/
#endif

static inline int dump2hex(char *buf, const char *data, size_t len)
{
	static const char *hex = "0123456789abcdef";
	char *dest = buf;
	size_t i;

	for (i = 0; i < len; i++) {
		*dest++ = hex[(data[i] >> 4) & 0xf];
		*dest++ = hex[data[i] & 0xf];
		*dest++ = ' ';
	}
	if (len > 0)
		dest--; /* last space will be overwrited with null */

	*dest = '\0';
	return dest - buf;
}

static void save_ipc_log(FILE *fp, struct mif_ipc_block *block)
{
	char *prefix[] = {"[RL2AP]", "[AP2CP]", "[CP2AP]", "[AP2RL]", "[FLAGS]"};
	char hex_buf[(block->len > MAX_IPC_LOG_SIZE) ? (MAX_IPC_LOG_SIZE * 3) : (block->len * 3 + 1)];

	memset(hex_buf, 0, sizeof(hex_buf));
	dump2hex(hex_buf, block->buff,
		(block->len > MAX_IPC_LOG_SIZE) ? MAX_IPC_LOG_SIZE : block->len);

	fprintf(fp, "[%5lu.%06lu] %s [%lu] %s\n",
		(unsigned long)(block->time / 1000000000u),
		(unsigned long)((block->time % 1000000000u) / 1000),
		prefix[block->id - 1], block->len, hex_buf);
}

static void save_irq_log(FILE *fp, struct mif_irq_block *block)
{
	fprintf(fp, "[%5lu.%06lu] [M:0x%X|A:%u] F[TI:%u TO:%u | RI:%u RO:%u] "
		"R[TI:%u TO:%u | RI:%u RO:%u] CP2AP[0x%X] %s\n",
		(unsigned long)(block->time / 1000000000u),
		(unsigned long)((block->time % 1000000000u) / 1000),
		block->map.magic, block->map.access,
		block->map.fmt_tx_in, block->map.fmt_tx_out,
		block->map.fmt_rx_in, block->map.fmt_rx_out,
		block->map.raw_tx_in, block->map.raw_tx_out,
		block->map.raw_rx_in, block->map.raw_rx_out,
		block->map.cp2ap,
		(strnlen(block->buff, sizeof(block->buff)) ? block->buff : "None"));
}

static void save_common_log(FILE *fp, struct mif_common_block *block)
{
	fprintf(fp, "[%5lu.%06lu] %s",
		(unsigned long)(block->time / 1000000000u),
		(unsigned long)((block->time % 1000000000u) / 1000),
		block->buff);
}

static void save_time_log(FILE *fp, struct mif_time_block *block)
{
	struct tm date;

	localtime_r(&block->epoch.tv_sec, &date);
	fprintf(fp, "[%5lu.%06lu] [UTC : %02d-%02d %02d:%02d:%02d.%03lu] %s\n",
		(unsigned long)(block->time / 1000000000u),
		(unsigned long)((block->time % 1000000000u) / 1000),
		(1 + date.tm_mon), date.tm_mday, date.tm_hour,
		date.tm_min, date.tm_sec,
		(block->epoch.tv_nsec > 0 ? block->epoch.tv_nsec / 100000 : 0),
		(strnlen(block->buff, sizeof(block->buff)) ? block->buff : "None"));
}

static int save_mif_dump(char *name)
{
	char dev_path[MAX_PATH_SIZE], file_path[MAX_PATH_SIZE];
	char buf[MAX_BUF_SIZE], temp[MAX_PATH_SIZE];
	int fd, err = 0, read_size = 0;
	char *cpy_buf;
	unsigned long arg;
	fd_set reads, temps;
	struct timeval now, tv;
	struct tm date;
	FILE *fp = NULL;
	enum mif_log_id id;

	memset(dev_path, 0, sizeof(dev_path));
	strncat(dev_path, DIR_PATH, rest_len(dev_path));
	strncat(dev_path, name, rest_len(dev_path));

	fd = open(dev_path, O_RDWR);
	if (fd == -1) {
		cbd_log("[MIF] Device node open failed!!\n");
		return -EINVAL;
	}

	gettimeofday(&now, NULL);
	localtime_r(&now.tv_sec, &date);

	memset(file_path, 0, sizeof(file_path));
	strncat(file_path, FILE_PATH, rest_len(file_path));
	strncat(file_path, "mif", rest_len(file_path));

	snprintf(temp, MAX_PATH_SIZE, "_%s_%02d%02d%02d_%02d%02d%02d.log",
		name, (1900 + date.tm_year), (1 + date.tm_mon), date.tm_mday,
		date.tm_hour, date.tm_min, date.tm_sec);
	strncat(file_path, temp, rest_len(file_path));

	cbd_log("[MIF] file name is %s\n", file_path);

	fp = fopen(file_path, "w");
	if (!fp) {
		cbd_log("[MIF] %s File open failed!\n", file_path);
		close(fd);
		return -EFAULT;
	}

	err = ioctl(fd, IOCTL_MIF_LOG_DUMP, &arg);
	if (err < 0) {
		cbd_log("[MIF] MIF_DUMP ioctl failed!\n");
		close(fd);
		fclose(fp);
		return -EFAULT;
	} else {
		cbd_log("[MIF] Total size = %ld\n", arg);
	}

	FD_ZERO(&reads);
	FD_SET(fd, &reads);

	while (arg > 0) {
		temps = reads;
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		err = select(fd + 1, &temps, NULL, NULL, &tv);
		if (err == -1) {
			cbd_log("[MIF] 1. select failed!\n");
			close(fd);
			fclose(fp);
			return -EFAULT;
		}

		read_size = read(fd, buf, MAX_BUF_SIZE);
		cpy_buf = buf;
		arg -= read_size;

		while (read_size > 0) {
			id = ((struct mif_ipc_block *)cpy_buf)->id;

			switch (id) {
				case MIF_IPC_RL2AP:
				case MIF_IPC_AP2CP:
				case MIF_IPC_CP2AP:
				case MIF_IPC_AP2RL:
				case MIF_IPC_FLAG:
					save_ipc_log(fp, (struct mif_ipc_block *)cpy_buf);
					break;
				case MIF_IRQ:
					save_irq_log(fp, (struct mif_irq_block *)cpy_buf);
					break;
				case MIF_COM:
					save_common_log(fp, (struct mif_common_block *)cpy_buf);
					break;
				case MIF_TIME:
					save_time_log(fp, (struct mif_time_block *)cpy_buf);
					break;
				default:
					/* To do something */
					break;
			}

			cpy_buf += MAX_LOG_SIZE;
			read_size -= MAX_LOG_SIZE;
		}
	}

	cbd_log("[MIF] MIF[%s] dump done!\n", name);
	fclose(fp);
	close(fd);
	return 0;
}

static int myfilter(const struct dirent *d)
{
	return strcmp(d->d_name + (strlen(d->d_name) - 4), "_log") ? 0 : 1;
}

int exec_mif_logger()
{
	int ret, loop = 0;
	struct dirent **list;

	ret = scandir(DIR_PATH, &list, *myfilter, NULL);
	if (ret <= 0) {
		cbd_log("[MIF] Device didn't have log node\n");
		free(list);
		return -EINVAL;
	}

	for (loop = 0; loop < ret; loop++) {
		ret = save_mif_dump(list[loop]->d_name);
		if (ret < 0)
			cbd_log("[MIF] Failed to save %s_%s's log\n",
				"mif", list[loop]->d_name);
	}
	free(list);
	return 0;
}

#if 1
/*============================================================================*\
	Definitions for Error Check (Detection and/or Correction)
\*============================================================================*/
#endif

static const unsigned long crc32_table[256] =
{
    0x00000000L,    0x77073096L,    0xEE0E612CL,    0x990951BAL,    0x076DC419L,
    0x706AF48FL,    0xE963A535L,    0x9E6495A3L,    0x0EDB8832L,    0x79DCB8A4L,
    0xE0D5E91EL,    0x97D2D988L,    0x09B64C2BL,    0x7EB17CBDL,    0xE7B82D07L,
    0x90BF1D91L,    0x1DB71064L,    0x6AB020F2L,    0xF3B97148L,    0x84BE41DEL,
    0x1ADAD47DL,    0x6DDDE4EBL,    0xF4D4B551L,    0x83D385C7L,    0x136C9856L,
    0x646BA8C0L,    0xFD62F97AL,    0x8A65C9ECL,    0x14015C4FL,    0x63066CD9L,
    0xFA0F3D63L,    0x8D080DF5L,    0x3B6E20C8L,    0x4C69105EL,    0xD56041E4L,
    0xA2677172L,    0x3C03E4D1L,    0x4B04D447L,    0xD20D85FDL,    0xA50AB56BL,
    0x35B5A8FAL,    0x42B2986CL,    0xDBBBC9D6L,    0xACBCF940L,    0x32D86CE3L,
    0x45DF5C75L,    0xDCD60DCFL,    0xABD13D59L,    0x26D930ACL,    0x51DE003AL,
    0xC8D75180L,    0xBFD06116L,    0x21B4F4B5L,    0x56B3C423L,    0xCFBA9599L,
    0xB8BDA50FL,    0x2802B89EL,    0x5F058808L,    0xC60CD9B2L,    0xB10BE924L,
    0x2F6F7C87L,    0x58684C11L,    0xC1611DABL,    0xB6662D3DL,    0x76DC4190L,
    0x01DB7106L,    0x98D220BCL,    0xEFD5102AL,    0x71B18589L,    0x06B6B51FL,
    0x9FBFE4A5L,    0xE8B8D433L,    0x7807C9A2L,    0x0F00F934L,    0x9609A88EL,
    0xE10E9818L,    0x7F6A0DBBL,    0x086D3D2DL,    0x91646C97L,    0xE6635C01L,
    0x6B6B51F4L,    0x1C6C6162L,    0x856530D8L,    0xF262004EL,    0x6C0695EDL,
    0x1B01A57BL,    0x8208F4C1L,    0xF50FC457L,    0x65B0D9C6L,    0x12B7E950L,
    0x8BBEB8EAL,    0xFCB9887CL,    0x62DD1DDFL,    0x15DA2D49L,    0x8CD37CF3L,
    0xFBD44C65L,    0x4DB26158L,    0x3AB551CEL,    0xA3BC0074L,    0xD4BB30E2L,
    0x4ADFA541L,    0x3DD895D7L,    0xA4D1C46DL,    0xD3D6F4FBL,    0x4369E96AL,
    0x346ED9FCL,    0xAD678846L,    0xDA60B8D0L,    0x44042D73L,    0x33031DE5L,
    0xAA0A4C5FL,    0xDD0D7CC9L,    0x5005713CL,    0x270241AAL,    0xBE0B1010L,
    0xC90C2086L,    0x5768B525L,    0x206F85B3L,    0xB966D409L,    0xCE61E49FL,
    0x5EDEF90EL,    0x29D9C998L,    0xB0D09822L,    0xC7D7A8B4L,    0x59B33D17L,
    0x2EB40D81L,    0xB7BD5C3BL,    0xC0BA6CADL,    0xEDB88320L,    0x9ABFB3B6L,
    0x03B6E20CL,    0x74B1D29AL,    0xEAD54739L,    0x9DD277AFL,    0x04DB2615L,
    0x73DC1683L,    0xE3630B12L,    0x94643B84L,    0x0D6D6A3EL,    0x7A6A5AA8L,
    0xE40ECF0BL,    0x9309FF9DL,    0x0A00AE27L,    0x7D079EB1L,    0xF00F9344L,
    0x8708A3D2L,    0x1E01F268L,    0x6906C2FEL,    0xF762575DL,    0x806567CBL,
    0x196C3671L,    0x6E6B06E7L,    0xFED41B76L,    0x89D32BE0L,    0x10DA7A5AL,
    0x67DD4ACCL,    0xF9B9DF6FL,    0x8EBEEFF9L,    0x17B7BE43L,    0x60B08ED5L,
    0xD6D6A3E8L,    0xA1D1937EL,    0x38D8C2C4L,    0x4FDFF252L,    0xD1BB67F1L,
    0xA6BC5767L,    0x3FB506DDL,    0x48B2364BL,    0xD80D2BDAL,    0xAF0A1B4CL,
    0x36034AF6L,    0x41047A60L,    0xDF60EFC3L,    0xA867DF55L,    0x316E8EEFL,
    0x4669BE79L,    0xCB61B38CL,    0xBC66831AL,    0x256FD2A0L,    0x5268E236L,
    0xCC0C7795L,    0xBB0B4703L,    0x220216B9L,    0x5505262FL,    0xC5BA3BBEL,
    0xB2BD0B28L,    0x2BB45A92L,    0x5CB36A04L,    0xC2D7FFA7L,    0xB5D0CF31L,
    0x2CD99E8BL,    0x5BDEAE1DL,    0x9B64C2B0L,    0xEC63F226L,    0x756AA39CL,
    0x026D930AL,    0x9C0906A9L,    0xEB0E363FL,    0x72076785L,    0x05005713L,
    0x95BF4A82L,    0xE2B87A14L,    0x7BB12BAEL,    0x0CB61B38L,    0x92D28E9BL,
    0xE5D5BE0DL,    0x7CDCEFB7L,    0x0BDBDF21L,    0x86D3D2D4L,    0xF1D4E242L,
    0x68DDB3F8L,    0x1FDA836EL,    0x81BE16CDL,    0xF6B9265BL,    0x6FB077E1L,
    0x18B74777L,    0x88085AE6L,    0xFF0F6A70L,    0x66063BCAL,    0x11010B5CL,
    0x8F659EFFL,    0xF862AE69L,    0x616BFFD3L,    0x166CCF45L,    0xA00AE278L,
    0xD70DD2EEL,    0x4E048354L,    0x3903B3C2L,    0xA7672661L,    0xD06016F7L,
    0x4969474DL,    0x3E6E77DBL,    0xAED16A4AL,    0xD9D65ADCL,    0x40DF0B66L,
    0x37D83BF0L,    0xA9BCAE53L,    0xDEBB9EC5L,    0x47B2CF7FL,    0x30B5FFE9L,
    0xBDBDF21CL,    0xCABAC28AL,    0x53B39330L,    0x24B4A3A6L,    0xBAD03605L,
    0xCDD70693L,    0x54DE5729L,    0x23D967BFL,    0xB3667A2EL,    0xC4614AB8L,
    0x5D681B02L,    0x2A6F2B94L,    0xB40BBE37L,    0xC30C8EA1L,    0x5A05DF1BL,
    0x2D02EF8DL
};

unsigned long update_crc32(unsigned long crc, unsigned char *buff, unsigned long len)
{
	if (buff && len) {
		while (len--)
			crc = crc32_table[(crc ^ *buff++) & 0xFF] ^ (crc >> 8);

		return crc ^ 0xFFFFFFFFL;
	}

	return 0L;
}

#if 1
/*============================================================================*\
	Definitions for NV data file management
\*============================================================================*/
#endif

int create_empty_nv(char *path, size_t size)
{
	int ret = 0;
	int saved = 0;
	int nv_fd;
	char *nv_data = NULL;

	cbd_log("create NV(%s)\n", path);

	nv_fd = open(path, O_RDWR | O_CREAT | O_SYNC, S_IRUSR | S_IWUSR);
	if (nv_fd < 0) {
		cbd_log("ERR! NV(%s) open fail\n", path);
		ret = -EFAULT;
		goto exit;
	}

	if (!size) {
		cbd_log("ERR! wrong size(%ld)\n", size);
		ret = -EFAULT;
		goto exit;
	}

	nv_data = (char *)malloc(size);
	if (!nv_data) {
		cbd_log("ERR! malloc(%ld) fail\n", size);
		ret = -ENOMEM;
		goto exit;
	}
	memset(nv_data, 0xFF, size);

	saved = write(nv_fd, nv_data, size);
	if (saved < 0) {
		cbd_log("NV(%s) write fail\n", path);
		ret = -EFAULT;
		goto exit;
	}

	if (fsync(nv_fd)) {
		cbd_log("ERR! fsync(nv_fd) fail : %s\n", ERR2STR);
		goto exit;
	}

	cbd_log("NV(%s) saved %d bytes\n", path, saved);

exit:
	if (nv_fd >= 0)
		close(nv_fd);

	if (nv_data)
		free(nv_data);

	return ret;
}

#if 1
/*============================================================================*\
	Definitions for debug log
\*============================================================================*/
#endif

#define KLOG_BUF_SHIFT	19	/* CONFIG_LOG_BUF_SHIFT from our kernel */
#define KLOG_BUF_LEN	(1 << KLOG_BUF_SHIFT)
char buffer[KLOG_BUF_LEN + 1];

int dmesg_to_file(char *of)
{
	char *p = buffer;
	ssize_t ret;
	int n;
	int fd;

	fd = open(of, O_WRONLY | O_CREAT | O_APPEND, 0664);
	if (fd < 0) {
		cbd_log("file open fail(%s)\n", of);
		return fd;
	}
	fchmod(fd, 0664);

	ret = lseek(fd, 0, SEEK_END);
	if (ret < 0) {
		cbd_log("lseek fail (%s)\n", of);
		goto exit;
	}

	n = klogctl(KLOG_READ_ALL, buffer, KLOG_BUF_LEN);
	if (n < 0) {
		cbd_log("klogctl fail\n");
		close(fd);
		return -1;
	}
	buffer[n] = '\0';

	while ((ret = write(fd, p, n))) {
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			cbd_log("write fail\n");
			goto exit;
		}
		p += ret;
		n -= ret;
	}
exit:
	close(fd);
	return 0;
}

static const char *cmd_name[] = {
	[LOG_DMESG]	= "dmesg",
	[LOG_DUMPSTATE]	= "dumpstate",
};

void remove_logs(int type, char *log_dir, char *reason)
{
	char command[MAX_PATH_LEN] = {0, };

	if (type > LOG_DUMPSTATE)
		return;

	if (check_directory(log_dir) < 0)
		return;

	snprintf(command, MAX_PATH_LEN, "rm -f %s/%s_%s_*.log",
		log_dir, cmd_name[type], reason);
	cbd_log("%s\n", command);

	system(command);
}

#if 1
/*============================================================================*\
	Functions for file system (directory & file) management
\*============================================================================*/
#endif

int check_fs_free_space(char *root)
{
	int err = 0;
	int fsize_mb = 0;
	int free_blks = 0;
	int blk_size = 0;
	struct statfs stat;

	memset(&stat, 0, sizeof(struct statfs));
	err = statfs(root, &stat);
	if (err < 0) {
		err = -errno;
		cbd_log("statfs(%s) fail (err %d)\n", root, err);
		return err;
	}

	blk_size = (int)stat.f_bsize;
	free_blks = (int)stat.f_bfree;
	fsize_mb = (blk_size >> 10) * (free_blks >> 10);

	cbd_log("%s: %d (block size) * %d (free blocks) = %d MB (free space)\n",
		root, blk_size, free_blks, fsize_mb);

	if (fsize_mb < MIN_FS_FREE_SPACE) {
		cbd_log("ERR! %s: free space < %d MB\n", root, MIN_FS_FREE_SPACE);
		return -ENOSPC;
	}

	return 0;
}

/* Check a directiry @path, then returns 0 if it is a valid directory. */
int check_directory(char *path)
{
	int err;
	struct stat dir_st;

	err = stat(path, &dir_st);
	if (!err) {
		if (S_ISDIR(dir_st.st_mode)) {
			/* @path is a directory. */
			return 0;
		} else {
			/* @path is NOT a directory. */
			return -ENOTDIR;
		}
	} else {
		cbd_log("stat(%s) fail\n", path);
		return -errno;
	}
}

int set_file_value(char* file, char* strval)
{
	int fd = 0;
	int ret;

	fd = open(file, O_RDWR);
        if (fd < 0) {
                cbd_log("Error opening %s\n", file);
		return -ENODEV;
        }

	ret = write(fd, strval, strlen(strval));
	if (ret < 0) {
		cbd_log("failed to write the value to %s(%d)\n", file, ret);
		close(fd);
		return ret;
	}

	if (fd)
		close(fd);

	return 0;
}

int get_file_value(char* file, char* ch, int len)
{
	int fd = 0;

	fd = open(file, O_RDONLY);
        if (fd < 0) {
                cbd_log("Error opening %s\n", file);
		return -ENODEV;
        }

	if (read(fd, ch, len) < 0) {
		cbd_log("failed to read the value from %s\n", file);
		close(fd);
		return -1;
	}

	if (fd)
		close(fd);

	return 0;
}

int wait_file_value(char* file, char *ch, int cnt)
{
	int ret, len;
	char fileval[11];
	len = strlen(ch);

	while (cnt--) {
		ret = get_file_value(file, fileval, len);
		if (ret == -ENODEV) {
			usleep(500000);
			continue;
		}
		if (ret < 0)
			return -1;
        if (!strncmp(fileval, ch, len))
			break;
         usleep(500000);
	}

	if (cnt < 0) {
		cbd_log("%s = %s (expected value = %s)\n", file, fileval, ch);
		return -1;
	}

	return 0;
}

/* get a full path of sysfs using platform driver name */
int get_device_path(const char *drv_name, const char *key, char *fpath)
{
	DIR *pdir;
	struct dirent *de;
	int ret = 0;

	sprintf(fpath, "%s/%s/", ROOT_PATH, drv_name);

	pdir = opendir(fpath);
	if (!pdir) {
		cbd_log("plat_drv DIR doesn't exist %s\n", fpath);
		return -ENOENT;
	}

	while ((de = readdir(pdir)))
		if (strstr(de->d_name, drv_name) != NULL ||
			strstr(de->d_name, key) != NULL)
			break;

	if (!de) {
		cbd_log("%s DIR not found using %s, %s\n", drv_name, key, fpath);
		ret = -ENOENT;
		goto exit;
	}

	strcat(fpath, de->d_name);
	strcat(fpath, "/");
exit:
	closedir(pdir);
	return ret;
}

int set_gpio_value(char *drv_name, char *key, char *gpio, char *strval)
{
	char full_path[256];

	key = (key) ?: drv_name;
	if (get_device_path(drv_name, key, full_path))
		return -ENOENT;

	strcat(full_path, gpio);
	return set_file_value(full_path, strval);
}

int wait_gpio_value(char *drv_name, char *key, char* gpio, char *val, int cnt)
{
	char full_path[256];
	int ret, len;
	char fileval[2] = "";
	len = strlen(val);

	key = (key) ?: drv_name;
	if (get_device_path(drv_name, key, full_path))
		return -ENOENT;

	strcat(full_path, gpio);
	while (cnt--) {
		ret = get_file_value(full_path, fileval, len);
		if (ret == -ENODEV) {
			usleep(500000);
			continue;
		}
		if (ret < 0)
			return -1;
		if (!strncmp(fileval, val, len))
			break;
		usleep(500000);
	}

	if (cnt < 0) {
		cbd_log("%s = %s (expected value = %s)\n", full_path, fileval, val);
		return -1;
	}

	return 0;
}
