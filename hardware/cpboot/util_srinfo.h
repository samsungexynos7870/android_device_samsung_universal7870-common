#ifndef __UTIL_SRINFO_H__
#define __UTIL_SRINFO_H__

#define SRINFO_PATH "/data/vendor/log/cbd"
#define SRINFO_LAST_PATH "/data/vendor/log/cbd/err"
#define SRINFO_FILE "sr_info"
#define SRINFO_LAST "last_sr_info"
#define SRINFO_MAX_SIZE 0x10000 /* 64KB */
#define SRINFO_READ_SIZE 0x1000 /* 4KB*/

struct shmem_srinfo {
	unsigned size;
	char buf[0];
};

void store_srinfo(char *surfix, int fd);
void restore_srinfo(char *surfix, int fd);

#endif
