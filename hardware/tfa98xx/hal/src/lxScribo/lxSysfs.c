/*
 *Copyright 2014,2015 NXP Semiconductors
 *
 *Licensed under the Apache License, Version 2.0 (the "License");
 *you may not use this file except in compliance with the License.
 *You may obtain a copy of the License at
 *
 *http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing, software
 *distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *See the License for the specific language governing permissions and
 *limitations under the License.
 */

/*
	access to the tfa98xx Linux driver sysfs i2c interface
	------------------------------------------------------

	It is assumed that the driver is named tfa98xx, if this
	is changed please adapt the DRIVER_NAME define below.

	For each tfa98xx I2C device there are 2 additional
        sysfs entries created:

	root@bbbgert:~# ls -al /sys/bus/i2c/devices/2-0036/
	total 0
	drwxr-xr-x  3 root root    0 May 15 02:25 .
	drwxr-xr-x 11 root root    0 May 15 02:25 ..
	lrwxrwxrwx  1 root root    0 May 15 02:54 driver -> ../../../../../bus/i2c/drivers/tfa98xx
	-r--r--r--  1 root root 4096 May 15 02:54 modalias
	-r--r--r--  1 root root 4096 May 15 02:25 name
	drwxr-xr-x  2 root root    0 May 15 02:54 power
	--w-------  1 root root    0 May 15 02:50 reg
	-rw-------  1 root root    0 May 15 02:50 rw
	lrwxrwxrwx  1 root root    0 May 15 02:54 subsystem -> ../../../../../bus/i2c
	-rw-r--r--  1 root root 4096 May 15 02:54 uevent
	root@bbbgert:~#

	- "reg" is a write only entry which must be used to set the
	   register / sub address.
	- "rw" is a write/read entry which can be used to read/write
          I2C data at the sub address set with the "reg" entry

	When writing/reading I2C data using the "rw" entry, then
        the write/read syscall will return the total number of
        bytes write/read including the register / sub address byte
*/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <dirent.h>

#include "dbgprint.h"
#include "lxScribo.h"
#include "NXP_I2C.h" /* for the error codes */

#define MAX_DEVICES 2
#define MAX_PATH_SIZE 50
#define MAX_DEVICE_NAME 10

/* this should match the driver name in the file /sys/bus/i2c/devices/{bus}-{address}/name */
#define DRIVER_NAME "tfa98xx"

static const char sysfs[] = "/sys/bus/i2c/devices";

struct sysfs_device {
	uint8_t bus;
	uint8_t addr;
	int fd_reg;
	int fd_rw;
};

static struct sysfs_device sysfs_devices[MAX_DEVICES];

extern int i2c_trace;

static void hexdump(int num_write_bytes, const unsigned char * data)
{
	int i;

	for(i=0;i<num_write_bytes;i++) {
		PRINT("0x%02x ", data[i]);
	}
}

static int lxSysfsSlave(int fd, int slave)
{
	int i;
    (void)fd; /* Avoid warning */

	for (i=0; i<MAX_DEVICES; i++) {
		if (sysfs_devices[i].addr == slave) {
			break;
		}
	}
	if (i == MAX_DEVICES) {
		ERRORMSG("Can't set i2c slave 0x%x\n", slave);
		_exit(1);
	}

	if (i2c_trace) PRINT("I2C slave=0x%02x\n", slave);

	return i;
}

static int lxSysfsWriteRead(int fd, int NrOfWriteBytes, const uint8_t * WriteData,
		int NrOfReadBytes, uint8_t * ReadData, unsigned int *pError) {
	int ln = -1;
	int index = lxSysfsSlave(fd, WriteData[0]>>1);

	*pError = NXP_I2C_Ok;

	if (NrOfWriteBytes && i2c_trace) {
		PRINT("W %d:", NrOfWriteBytes);
		hexdump (NrOfWriteBytes, WriteData);
		PRINT("\n");
	}

	if (NrOfWriteBytes > 2) {
		ln = write(sysfs_devices[index].fd_reg, &WriteData[1], 1); /* sub address */
		if (ln > 0) {
			ln = write(sysfs_devices[index].fd_rw, &WriteData[2], NrOfWriteBytes-2);
		}
		if (ln <= 0) {
			*pError = NXP_I2C_NoAck; /* treat all errors as nack */
		}
	}

	if ((*pError == NXP_I2C_Ok) && (NrOfReadBytes)) {
		ln = write(sysfs_devices[index].fd_reg, &WriteData[1], 1); /* sub address */
		if (ln > 0) {
			ln = read(sysfs_devices[index].fd_rw, &ReadData[1], NrOfReadBytes-1);
		}
		if (ln <= 0) {
			*pError = NXP_I2C_NoAck; /* treat all errors as nack */
		}
	}

	if (NrOfReadBytes && i2c_trace) {
		PRINT("R %d:", NrOfReadBytes);
		hexdump (NrOfReadBytes, ReadData);
		PRINT("\n");
	}

	if (*pError != NXP_I2C_Ok) {
		perror("i2c slave error");
		return 0;
	}

	return ln+1;
}

static int lxSysfsInit(char *devname)
{
	DIR *dir;
	struct dirent *entry;
	FILE *fp;
	char path[MAX_PATH_SIZE];
	char device_name[MAX_DEVICE_NAME];
	int i;
	int index = 0;

    (void)devname; /* Avoid warning */

	for (i=0; i<MAX_DEVICES; i++) {
		sysfs_devices[i].fd_reg = -1;
		sysfs_devices[i].fd_rw = -1;
	}

	dir = opendir(sysfs);
	if (dir != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			snprintf(path, MAX_PATH_SIZE, "%s/%s/name", sysfs, entry->d_name);
			fp = fopen(path, "r");
			if (fp == NULL)
				continue;
			fgets(device_name, MAX_DEVICE_NAME, fp);
			fclose(fp);
			/* compare to driver name, except for last 2 chars */
			if (strncmp(device_name, DRIVER_NAME, sizeof(DRIVER_NAME)-2) == 0) {
				int bus, addr;
				sscanf(entry->d_name, "%d-%x", &bus, &addr);
				//TODO verbose only PRINT("found tfa98xx: bus=%d addr=0x%x (%s/%s)\n", bus, addr, sysfs, entry->d_name);

				sysfs_devices[index].bus = bus;
				sysfs_devices[index].addr = addr;

				snprintf(path, MAX_PATH_SIZE, "%s/%s/reg", sysfs, entry->d_name);
				sysfs_devices[index].fd_reg = open(path, O_WRONLY);
				if (sysfs_devices[index].fd_reg == -1) {
					ERRORMSG("Can't open %s\n", path);
					_exit(1);
				}

				snprintf(path, MAX_PATH_SIZE, "%s/%s/rw", sysfs, entry->d_name);
				sysfs_devices[index].fd_rw = open(path, O_RDWR);
				if (sysfs_devices[index].fd_rw == -1) {
					ERRORMSG("Can't open %s\n", path);
					_exit(1);
				}

				index++;
			}
		}
		closedir(dir);
	}

	return 1;
}

/*
 * Close an opened device
 * Return success (0) or fail (-1)
 */
static int lxSysfsClose(int fd)
{
    (void)fd; /* Avoid warning */
	return 0;
}

static int lxSysfsVersion(char *buffer, int fd)
{
    (void)fd; /* Avoid warning */
    (void)buffer; /* Avoid warning */
    PRINT("Not implemented! \n");
	return 0;
}

static int lxSysfsSetPin(int fd, int pin, int value)
{
    (void)fd; /* Avoid warning */
    (void)pin; /* Avoid warning */
    (void)value; /* Avoid warning */
    PRINT("Not implemented! \n");
	return 0;
}

static int lxSysfsGetPin(int fd, int pin)
{
    (void)fd; /* Avoid warning */
    (void)pin; /* Avoid warning */
    PRINT("Not implemented! \n");
	return 0;
}

const struct nxp_i2c_device lxSysfs_device = {
	lxSysfsInit,
	lxSysfsWriteRead,
	lxSysfsVersion,
	lxSysfsSetPin,
	lxSysfsGetPin,
	lxSysfsClose
};
