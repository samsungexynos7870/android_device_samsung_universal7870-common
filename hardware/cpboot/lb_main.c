#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <linux/capability.h>
#include <linux/prctl.h>
#include <cutils/android_filesystem_config.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cutils/properties.h>
#include <log/log.h>
#include <linux/rtc.h>

#define DEV_LOOPBACK_NODE	"/dev/ipc_loopback0"
#define IOCTL_MODEM_STATUS		_IO('o', 0x27)
#define IOCTL_MODEM_FORCE_CRASH_EXIT	_IO('o', 0x34)

#define DEFAULT_PKT_SIZE 438
#define DEFAULT_INT_SEC 5
#define DEFAULT_DUR_SEC (1*60)

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define seq_diff(a, b) (((a) >= (b)) ? ((a) - (b)) : ((a) + (0xFF - (b))))

#define SYS_WAKE_LOCK "/sys/power/wake_lock"
#define SYS_WAKE_UNLOCK "/sys/power/wake_unlock"

#define CLD_READ_LOCK "cld-read-lock"
#define CLD_WRITE_LOCK "cld-write-lock"

enum rw_lock {
	READ_LOCK,
	WRITE_LOCK,
};

enum modem_state {
	STATE_OFFLINE,
	STATE_CRASH_RESET,
	STATE_CRASH_EXIT,
	STATE_BOOTING,
	STATE_ONLINE,
	STATE_NV_REBUILDING,
	STATE_LOADER_DONE,
};

/*
 * kprintf - kernel printf
 *
 * Printout message to kmsg for syncing with radio log
 * if not defined BOOT_KERNEL_MSG, dprintf(kmsg_fd, fmt ...) will be printout to
 * STDOUT
 */
int kmsg_fd = STDOUT_FILENO;
#define kprintf(fmt) dprintf(kmsg_fd, fmt)
static void kprintf_init(void)
{
	char *name = "dev/__cld_msg_";
	int ret;

	if (mknod(name, S_IFCHR | 0600, (1 << 8) | 11) == 0) {
		kmsg_fd = open(name, O_RDWR);
		ret = fcntl(kmsg_fd, F_SETFD, FD_CLOEXEC);
		if (ret < 0)
			fprintf(stderr, "fcntl err : %s\n", strerror(errno));
		unlink(name);
	}
}
static void kprintf_deinit(void)
{
	if (kmsg_fd != STDOUT_FILENO)
		close(kmsg_fd);
}

#define cld_info(s, args...) \
	__android_log_buf_print(LOG_ID_RADIO, ANDROID_LOG_DEBUG, "loopback", \
	"<4>cld: %s: " s, __func__, ##args)

#define cld_kernel(s, args...) dprintf(kmsg_fd, "<3>cld: %s: " s, __func__, \
	##args)

#define cld_log(s, args...) \
	do { \
		cld_info(s, ##args); \
		cld_kernel(s, ##args); \
	} while (0)

enum {
	AP_INIT_LB = 1,
	CP_INIT_LB
};

struct sipc_main_hdr {
	unsigned short len;
	unsigned char msg_seq;
	unsigned char ack_seq;
	unsigned char main_cmd;
	unsigned char sub_cmd;
	unsigned char cmd_type;
} __packed;

struct lb_packet {
	struct sipc_main_hdr hdr;
	unsigned int pkt_size;
	long interval;
} __packed;

struct lb_args {
	int fd;
	unsigned int mode;

	pthread_t cld_rx_thread;
	pthread_t cld_tx_thread;

	unsigned char tx_seq;
	unsigned char rx_seq;

	long tx_interval;
	unsigned long tx_packet;

	long rx_interval;
	unsigned long rx_packet;

	long duration;
	int forever;

	int tx_range;
	int rx_range;

	int alarm_fd;
};

static void cld_wake_lock(int val, enum rw_lock lock)
{
	char *path = val ? SYS_WAKE_LOCK : SYS_WAKE_UNLOCK;
	char *name = (lock == READ_LOCK) ? CLD_READ_LOCK : CLD_WRITE_LOCK;
	int fd, ret;

	fd = open(path,  O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (fd < 0) {
		cld_log("wake_%s open fail(%d)\n", lock ? "lock" : "unlock", fd);
		return;
	}

	ret = write(fd, name, strlen(name));
	if (ret < 0)
		cld_log("write fail - %s (%d)\n", name, ret);

	close(fd);
	return;
}

int set_alarm(void *param)
{
	struct lb_args *lb_arg = (struct lb_args *) param;
	int ret;
	struct rtc_time rtc;
	static struct rtc_time rtc_temp;

	memset(&rtc, 0, sizeof(rtc));

	if (lb_arg->alarm_fd < 0) {
		lb_arg->alarm_fd = open("/dev/rtc0", O_RDWR);

		/* convert tx_interval into rtc_time format */
		memset(&rtc_temp, 0, sizeof(rtc_temp));
		rtc_temp.tm_sec = lb_arg->tx_interval;
		if (rtc_temp.tm_sec >= 60) {
			rtc_temp.tm_min += rtc_temp.tm_sec/60 ;
			rtc_temp.tm_sec %= 60;
		}
		if (rtc_temp.tm_min >= 60) {
			rtc_temp.tm_hour += rtc_temp.tm_min/60;
			rtc_temp.tm_min %= 60;
		}
		if (rtc_temp.tm_hour >= 24) {
			rtc_temp.tm_hour %= 24;
		}
	}

	if (lb_arg->alarm_fd < 0) {
		cld_log("open /dev/rtc0 failed(%s)", strerror(errno));
		return lb_arg->alarm_fd;
	}

	/* read current rtc time */
again1:
	ret = ioctl(lb_arg->alarm_fd, RTC_RD_TIME, &rtc);
	if (ret < 0) {
		if (errno == EINTR) {
			cld_log("RTC_RD_TIME. EINTR(%s), repeating\n", strerror(errno));
			goto again1;
		} else {
			cld_log("read rtc time failed(%s)\n", strerror(errno));
			close(lb_arg->alarm_fd);
			lb_arg->alarm_fd = -1;
			return ret;
		}
	}

	if (lb_arg->tx_range) {
		/* convert tx_interval into rtc_time format */
		memset(&rtc_temp, 0, sizeof(rtc_temp));
		rtc_temp.tm_sec = lb_arg->tx_interval;
		if (rtc_temp.tm_sec >= 60) {
			rtc_temp.tm_min += rtc_temp.tm_sec/60 ;
			rtc_temp.tm_sec %= 60;
		}
		if (rtc_temp.tm_min >= 60) {
			rtc_temp.tm_hour += rtc_temp.tm_min/60;
			rtc_temp.tm_min %= 60;
		}
		if (rtc_temp.tm_hour >= 24) {
			/* max 1 day only */
			rtc_temp.tm_hour %= 24;
		}
	}

	cld_log("curr=%d:%d:%d\n", rtc.tm_hour, rtc.tm_min, rtc.tm_sec);

	rtc.tm_sec += rtc_temp.tm_sec;
	rtc.tm_min += rtc_temp.tm_min;
	rtc.tm_hour += rtc_temp.tm_hour;

	if (rtc.tm_sec >= 60) {
		rtc.tm_sec %= 60;
		rtc.tm_min++;
	}
	if (rtc.tm_min >= 60) {
		rtc.tm_min %= 60;
		rtc.tm_hour++;
	}
	if (rtc.tm_hour == 24)
		rtc.tm_hour = 0;

	cld_log("next=%d:%d:%d\n", rtc.tm_hour, rtc.tm_min, rtc.tm_sec);

	/* set alarm for interval seconds in future */
again2:
	ret = ioctl(lb_arg->alarm_fd, RTC_ALM_SET, &rtc);
	if (ret < 0) {
		if (errno == EINTR) {
			cld_log("RTC_ALM_SET. EINTR(%s), repeating\n", strerror(errno));
			goto again2;
		} else {
			cld_log("read rtc time failed(%s)\n", strerror(errno));
			close(lb_arg->alarm_fd);
			lb_arg->alarm_fd = -1;
			return ret;
		}
	}

	/* Enable alarm interrupts */
again3:
	ret = ioctl(lb_arg->alarm_fd, RTC_AIE_ON, 0);
	if (ret < 0) {
		if (errno == EINTR) {
			cld_log("RTC_AIE_ON. EINTR(%s), repeating\n", strerror(errno));
			goto again3;
		} else {
			cld_log("read rtc time failed(%s)\n", strerror(errno));
			close(lb_arg->alarm_fd);
			lb_arg->alarm_fd = -1;
			return ret;
		}
	}

	return ret;
}

void unset_alarm(void *param)
{
	int ret;
	struct lb_args *lb_arg = (struct lb_args *) param;

	cld_log("+\n");

	if (lb_arg->alarm_fd >= 0) {
again:
		ret = ioctl(lb_arg->alarm_fd, RTC_AIE_OFF, 0);
		if (ret < 0) {
			if (errno == EINTR) {
				cld_log("RTC_AIE_OFF. EINTR(%s), repeating\n", strerror(errno));
				goto again;
			} else {
				cld_log("disabling RTC Alarm Interrupt failed: (%s)\n", strerror(errno));
			}
		}

		close(lb_arg->alarm_fd);
		lb_arg->alarm_fd = -1;
	}

	cld_log("-\n");
}

int wait_for_alarm(void *param)
{
	int result = 0;
	unsigned long data;
	struct lb_args *lb_arg = (struct lb_args *) param;

	cld_log("+\n");

	/* Wait until alarm timout causes an interrupt */
again:
	cld_wake_lock(0, WRITE_LOCK);
	result = read(lb_arg->alarm_fd, &data, sizeof(unsigned long));
	cld_wake_lock(1, WRITE_LOCK);
	if (result < 0) {
		if (errno == EINTR) {
			cld_log("EINTR(%s), repeating\n", strerror(errno));
			goto again;
		} else {
			cld_log("Unable to wait on alarm: %s\n", strerror(errno));
		}
	}

	cld_log("-\n");
	return result;
}

void set_and_wait_for_alarm(void *param)
{
	struct lb_args *lb_arg = (struct lb_args *) param;

	if (!(set_alarm(param) >= 0 && wait_for_alarm(param) >= 0)) {
		cld_log("FAILED to use RTC Alarm, exit the Sender loop\n");
		lb_arg->duration = 0;
	}
}

static void help(char *name)
{
	cld_log("Usage: %s\n\n", name);
	cld_log(" [OPTION] [DESCRIPTION]\n");
	cld_log("-h	Help\n\n");
	cld_log("-s	Enable AP loopback mode(default mode is AP)\n");
	cld_log("-r	Enable CP loopback mode\n\n");

	cld_log("-t	Time interval in secs\n");
	cld_log("	negative val to specify range [1, val]\n");
	cld_log("	default is 5 secs\n");
	cld_log("	cld -s -t -20 :rand interval in the range [1, 20]\n\n");

	cld_log("-d	Duration of test-run in secs\n");
	cld_log("	-1 is for forever\n");
	cld_log("	default is 60\n");
	cld_log("	cld -s -d -1 :test will run forever\n\n");

	cld_log("-p	Loopback packet size in bytes\n");
	cld_log("	default is 438 bytes\n\n");

	cld_log("example1: cld\n");
	cld_log("example1: cld -s -t 10 -d 60\n");
	cld_log("example2: cld -r -t 10 -d 60\n");
	cld_log("example3: cld -s -r -t 10 -d 60\n");

	cld_log("\n\n");
}

static void *cld_reader_loop(void *param)
{
	struct lb_args *lb_arg = (struct lb_args *)param;
	fd_set rfds, tmp_fds;
	char *buff;
	struct sipc_main_hdr *hdr;
	struct lb_packet init_pkt;
	int ret;
	struct timeval tv;
	long timeout_sec = 2;
	unsigned long alloc_size = max(lb_arg->tx_packet, lb_arg->rx_packet);

	cld_log("cld readerLoop Start\n");

	buff = malloc(alloc_size);
	if (!buff) {
		cld_log("memory alloc failed, size=%lu\n", alloc_size);
		goto exit;
	}

	FD_ZERO(&rfds);
	FD_SET(lb_arg->fd, &rfds);

	if (lb_arg->mode & AP_INIT_LB) {
		if (timeout_sec < lb_arg->tx_interval)
			timeout_sec = lb_arg->tx_interval;
	}

	if (lb_arg->mode & CP_INIT_LB) {
		if (timeout_sec < labs(lb_arg->rx_interval))
			timeout_sec = labs(lb_arg->rx_interval);
	}
	cld_log("select() timeout val=%ld secs\n", timeout_sec);

	while (lb_arg->forever || lb_arg->duration > 0) {
		unsigned recved_size = 0;
retry:
		tmp_fds = rfds;

		/* timeout after twice the interval */
		tv.tv_sec = 2 * timeout_sec;
		tv.tv_usec = 0;

		cld_wake_lock(0, READ_LOCK);
		ret = select(lb_arg->fd + 1, &tmp_fds, NULL, NULL, &tv);
		cld_wake_lock(1, READ_LOCK);

		if (ret < 0) {
			cld_log("select ERROR! err=%d\n", ret);
			goto exit;
		} else if (ret == 0) {
			cld_log("select TIMEOUT!\n");
			continue;
		}

		ret = ioctl(lb_arg->fd, IOCTL_MODEM_STATUS);
		if (ret < 0) {
			cld_log("fail to get modem state, err=%d\n", ret);
			goto exit;
		}

		if (ret != STATE_ONLINE) {
			cld_log("modem state=%d, not ONLINE\n", ret);
			goto exit;
		}

		ret = read(lb_arg->fd, buff + recved_size, alloc_size);
		if (ret < 0) {
			cld_log("read fail, err=%d\n", ret);
			goto exit;
		}

		recved_size += ret;
		hdr = (struct sipc_main_hdr *)buff;
		if (recved_size != hdr->len) {
			cld_log("pkt with msg seq=%u hdr->len=%u, recvd=%u\n",
					hdr->msg_seq, hdr->len, recved_size);
			goto retry;
		}

		if (hdr->main_cmd == 0x90) {
			cld_log("pkt with msg seq=%u hdr->len=%u, recvd=%u\n",
					hdr->msg_seq, hdr->len, recved_size);
			ret = write(lb_arg->fd, (void *)buff, hdr->len);
			if (ret < 0) {
				cld_log("fail to send loopback data, err=%d\n", ret);
				goto exit;
			}

			/* if AP2CP lb is also enabled, SenderLoop tracks the duration */
			if (!lb_arg->forever && !(lb_arg->mode & AP_INIT_LB)) {
				if (lb_arg->rx_range)
					lb_arg->rx_interval = (rand() % lb_arg->rx_range) + 1;

				lb_arg->duration -= lb_arg->rx_interval;
			}
		} else if (hdr->main_cmd == 0x91) {
			cld_log("pkt wth ack seq=%u recvd\n", hdr->msg_seq);
			lb_arg->rx_seq = hdr->msg_seq;
		} else {
			cld_log("Error!, main_cmd=%u unknown\n", hdr->main_cmd);
			goto exit;
		}
	} /* end of while */
	cld_log("duration=%ld\n", lb_arg->duration);

exit:
	lb_arg->duration = 0;

	if (lb_arg->mode & CP_INIT_LB) {
		/* Make stop packet for CP initiated loopback */
		init_pkt.hdr.len = sizeof(struct lb_packet);
		init_pkt.hdr.main_cmd = 'x';
		init_pkt.pkt_size = 0;
		init_pkt.interval = 0;

		ret = write(lb_arg->fd, &init_pkt, sizeof(init_pkt));
		if (ret < 0)
			cld_log("lb_stop packet send failed, err=%d\n", ret);
		else
			cld_log("lb_stop packet sent\n");
	}

	if (buff)
		free(buff);

	cld_log("ReaderLoop Terminated\n");
	return NULL;
}

static void *cld_sender_loop(void *param)
{
	struct lb_args *lb_arg = (struct lb_args *)param;
	char *buff = NULL;
	struct lb_packet init_pkt;
	struct sipc_main_hdr *hdr;
	int ret = 0;
	int status = 0;

	if (lb_arg->mode & CP_INIT_LB) {
		cld_log("starting CP2AP loopback mode\n");

		/* Make start packet for CP initiated loopback */
		init_pkt.hdr.len = sizeof(struct lb_packet);
		init_pkt.hdr.main_cmd = 's';
		init_pkt.pkt_size = lb_arg->rx_packet;
		init_pkt.interval = lb_arg->rx_interval;

		if (lb_arg->rx_interval < 0)
			lb_arg->rx_range = lb_arg->rx_interval = labs(lb_arg->rx_interval);

		ret = write(lb_arg->fd, &init_pkt, sizeof(init_pkt));
		if (ret < 0) {
			cld_log("fail to send lb_start packet\n");
			goto exit;
		}

		cld_log("lb_start packet sent\n");
	}

	if (lb_arg->mode & AP_INIT_LB) {
		cld_log("starting AP2CP loopback mode\n");

		/* Make lb packet for AP initiated loopback */
		buff = malloc(lb_arg->tx_packet);
		if (!buff) {
			cld_log("memory alloc failed\n");
			ret = -1;
			goto exit;
		}

		hdr = (struct sipc_main_hdr *)buff;
		hdr->len = lb_arg->tx_packet;
		hdr->main_cmd = 0x91;

		lb_arg->alarm_fd = -1;

		while (lb_arg->forever || lb_arg->duration > 0) {
			if (seq_diff(lb_arg->tx_seq, lb_arg->rx_seq) > 2) {
				cld_log("seq err!, tx_seq=%u, rx_seq=%u",
					lb_arg->tx_seq, lb_arg->rx_seq);

				ret = ioctl(lb_arg->fd, IOCTL_MODEM_FORCE_CRASH_EXIT);
				if (ret < 0) {
					cld_log("fail to crash exit. err=%d\n", ret);
					goto exit;
				}

				/* wait for CP upload mode */
				while (1)
					sleep(1000);
			}

			ret = ioctl(lb_arg->fd, IOCTL_MODEM_STATUS);
			if (ret < 0) {
				cld_log("fail to get modem state err=%d\n", ret);
				goto exit;
			}

			if (ret != STATE_ONLINE) {
				cld_log("modem state=%d, not ONLINE\n", ret);
				ret = -1;
				goto exit;
			}

			/* set tx sequence number */
			hdr->msg_seq = lb_arg->tx_seq;

			ret = write(lb_arg->fd, buff, lb_arg->tx_packet);
			if (ret < 0) {
				cld_log("fail to send lb_pkt, err=%d\n", ret);
				goto exit;
			}

			cld_log("pkt with msg seq=%u sent\n", hdr->msg_seq);

			lb_arg->tx_seq = (lb_arg->tx_seq + 1) % 0x100;

			if (lb_arg->tx_range) {
				lb_arg->tx_interval = (rand() % lb_arg->tx_range) + 1;
				cld_log("next interval=%ld\n", lb_arg->tx_interval);
			}

			set_and_wait_for_alarm(lb_arg);

			if (!lb_arg->forever)
				lb_arg->duration -= lb_arg->tx_interval;
		} /* end of while */
		cld_log("lb_arg->duration=%ld\n", lb_arg->duration);
	}

exit:
	if (ret < 0) {
		cld_log("SenderLoop terminating due to err=%d\n", ret);
		lb_arg->duration = 0;
	}

	if (buff)
		free(buff);

	if (lb_arg->mode & AP_INIT_LB) {
		unset_alarm(lb_arg);

		/* wait until rx thread termiantes */
		pthread_join(lb_arg->cld_rx_thread, (void **)&status);
	}

	cld_log("SenderLoop Terminated\n");
	return NULL;
}

int get_cld_args(int argc, char *argv[], struct lb_args *arg)
{
	int opt;
	int mode = 0;
	char *opt_arg, *endptr;
	unsigned long p_val;

	while (1) {
		opt = getopt(argc, argv, "hrst:p:d:e");
		if (opt == -1)
			break;

		opt_arg = optarg;

		switch (opt) {
		case 'h':
			help(argv[0]);
			return -1;

		case 'r':
			arg->mode |= CP_INIT_LB;
			mode = CP_INIT_LB;
			arg->rx_packet = DEFAULT_PKT_SIZE;
			arg->rx_interval = DEFAULT_INT_SEC;
			arg->duration = DEFAULT_DUR_SEC;
			cld_log("CP Loopback enabled\n");
			break;

		case 's':
			arg->mode |= AP_INIT_LB;
			mode = AP_INIT_LB;
			arg->tx_packet = DEFAULT_PKT_SIZE;
			arg->tx_interval = DEFAULT_INT_SEC;
			arg->duration = DEFAULT_DUR_SEC;
			cld_log("AP Loopback enabled\n");
			break;

		case 't':
			if (opt_arg != NULL) {
				errno = 0;
				p_val = strtoul(opt_arg, &endptr, 10);
				if (*endptr || errno || !p_val ||
					(p_val == ULONG_MAX
					 && errno == ERANGE)) {
					cld_log("[-t] arg is wrong! default\n");
					p_val = DEFAULT_INT_SEC;
				}
			} else {
				cld_log("[-t] arg is wrong!, default\n");
				p_val = DEFAULT_INT_SEC;
			}

			if (mode == AP_INIT_LB) {
				arg->tx_interval = p_val;
			} else if (mode == CP_INIT_LB) {
				arg->rx_interval = p_val;
			} else {
				cld_log("[-t] arg list is wrong!\n");
				return -EINVAL;
			}
			break;

		case 'p':
			if (opt_arg != NULL) {
				errno = 0;
				p_val = strtoul(opt_arg, &endptr, 10);
				if (*endptr || errno || !p_val ||
					(p_val == ULONG_MAX
					 && errno == ERANGE)) {
					cld_log("[-p] arg is wrong! default\n");
					p_val = DEFAULT_PKT_SIZE;
				}
			} else {
				cld_log("[-p] arg is wrong!, default\n");
				p_val = DEFAULT_PKT_SIZE;
			}

			if (mode == AP_INIT_LB) {
				arg->tx_packet = p_val;
			} else if (mode == CP_INIT_LB) {
				arg->rx_packet = p_val;
			} else {
				cld_log("[-p] arg list is wrong!\n");
				return -EINVAL;
			}
			break;

		case 'd':
			if (opt_arg != NULL) {
				errno = 0;
				p_val = strtoul(opt_arg, &endptr, 10);
				if (*endptr || errno || !p_val ||
					(p_val == ULONG_MAX && errno == ERANGE)) {
					cld_log("[-d] arg is wrong! default\n");
					p_val = DEFAULT_DUR_SEC;
				}
			} else {
				cld_log("[-d] arg is wrong!, default\n");
				p_val = DEFAULT_DUR_SEC;
			}

			cld_log("[-d] arg is %ld secs\n", p_val);
			arg->duration =	 p_val;
			if (arg->duration < 0)
				arg->forever = 1;
			break;

		case 'e':
			cld_log("Loopback disable\n");
			/* To do: Something */
			break;
		}
	}

	if (!mode) {
		cld_log("Argument list is wrong! continue with default settings\n");
		arg->mode = AP_INIT_LB;
		arg->tx_packet = DEFAULT_PKT_SIZE;
		arg->tx_interval = DEFAULT_INT_SEC;
		arg->duration = DEFAULT_DUR_SEC;
	}

	if (arg->mode & AP_INIT_LB)
		cld_log("[AP_LB] pkt = %lu bytes, int = %ld secs\n",
			arg->tx_packet, arg->tx_interval);

	if (arg->mode & CP_INIT_LB)
		cld_log("[CP_LB] pkt = %lu bytes, int = %ld secs",
			arg->rx_packet, arg->rx_interval);

	cld_log("test-run duration=%ld secs\n", arg->duration);

	if (arg->tx_interval < 0)
		arg->tx_range = arg->tx_interval = labs(arg->tx_interval);

	return 0;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int status = 0;
	struct lb_args lb_arg;
	struct lb_packet lb_pkt;

	kprintf_init();

	cld_log("Start CP Loopback Daemon!!\n");

	memset(&lb_arg, 0, sizeof(struct lb_args));

	/* get cld_argument from command line */
	ret = get_cld_args(argc, argv, &lb_arg);
	if (ret < 0) {
		kprintf_deinit();
		return 0;
	}

	/* Open device node file */
	lb_arg.fd = open(DEV_LOOPBACK_NODE, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (lb_arg.fd < 0) {
		cld_log("open %s failed, err=%d\n", DEV_LOOPBACK_NODE, lb_arg.fd);
		kprintf_deinit();
		return 0;
	}

	cld_wake_lock(1, READ_LOCK);
	cld_wake_lock(1, WRITE_LOCK);

	/* Create rx_thread */
	if (pthread_create(&lb_arg.cld_rx_thread, NULL,
			cld_reader_loop, (void *)&lb_arg) != 0) {
		cld_log("cannot create rx_thread\n");
		goto exit;
	}

	/* Create tx_thread */
	if (pthread_create(&lb_arg.cld_tx_thread, NULL,
			cld_sender_loop, (void *)&lb_arg) != 0) {
		cld_log("cannot create tx_thread\n");
		cld_wake_lock(0, WRITE_LOCK);
		goto exit;
	}

	/* Wait until both thread finish */
	pthread_join(lb_arg.cld_tx_thread, (void **)&status);
	cld_wake_lock(0, WRITE_LOCK);

	pthread_join(lb_arg.cld_rx_thread, (void **)&status);

exit:
	if (lb_arg.fd >= 0)
		close(lb_arg.fd);

	cld_log("CLD Terminated\n");

	cld_wake_lock(0, READ_LOCK);

	kprintf_deinit();
	return 0;
}
