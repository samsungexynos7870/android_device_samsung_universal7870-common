//
// lxScribo main entry
//
// 	 This is the initiator that sets up the connection to either a serial/RS232 device or a socket.
//
// 	 input args:
// 	 	 none: 		connect to the default device, /dev/Scribo
// 	 	 string: 	assume this is a special device name
// 	 	 number:	use this to connect to a socket with this number
//
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#if !(defined(WIN32) || defined(_X64))
#include <unistd.h>
#endif
#include <stdint.h>
#include "dbgprint.h"
#include "NXP_I2C.h"
#include "lxScribo.h"
#include <assert.h>

int lxScribo_verbose = 0;
#define VERBOSE if (lxScribo_verbose)
static char dev[FILENAME_MAX]; /* device name */
static int lxScriboFd=-1;

/* device usage reference counter */
static int refcnt = 0;

/**
 * register the low level I2C HAL interface
 *  @param target device name; if NULL the default will be registered
 *  @return file descriptor of target device if successful
 */
int lxScriboRegister(char *target)
{
	if (target ){
		strncpy(dev, target, FILENAME_MAX-1);
	} else {
		strncpy(dev, TFA_I2CDEVICE, FILENAME_MAX-1);
	}

	if ( lxScribo_verbose ) {
			PRINT("%s:target device=%s\n", __FUNCTION__, dev);
	}

	/* tell the HAL which interface functions : */

	/////////////// dummy //////////////////////////////
	if ( strncmp (dev, "dummy",  5 ) == 0 ) {// if dummy act dummy
		lxScriboFd = NXP_I2C_Interface(dev, &lxDummy_device);
	}
#ifdef HAL_WIN_SIDE_CHANNEL
	/////////////// hid //////////////////////////////
	else if ( strncmp( dev , "wsc", 3 ) == 0)	{ // windows side channel
		lxScriboFd = NXP_I2C_Interface(dev, &lxWinSideChannel_device);
	}
#endif
#ifdef	HAL_HID
	/////////////// hid //////////////////////////////
	else if ( strncmp( dev , "hid", 3 ) == 0)	{ // hid
		lxScriboFd = NXP_I2C_Interface(dev, &lxHid_device);
	}
#endif
#ifdef HAL_SYSFS
	/////////////// sysfs //////////////////////////////
	else if ( strncmp( dev , "sysfs", 5 ) == 0)	{
		lxScriboFd = NXP_I2C_Interface(dev, &lxSysfs_device);
	}
#endif
#if !( defined(WIN32) || defined(_X64) ) // posix/linux
	/////////////// network //////////////////////////////
	else if ( strchr( dev , ':' ) != 0)	{ // if : in name > it's a socket
		lxScriboFd = NXP_I2C_Interface(dev, &lxScriboSocket_device); //TCP
	}
	else if ( strchr( dev , '@' ) != 0)	{ // if @ in name > it's a UDP socket
		lxScriboFd = NXP_I2C_Interface(dev, &lxScriboSocket_udp_device);
	}
	/////////////// i2c //////////////////////////////
	else if ( strncmp (dev, "/dev/i2c",  8 ) == 0 ) { // if /dev/i2c... direct i2c device
		lxScriboFd = NXP_I2C_Interface(dev, &lxI2c_device);
		VERBOSE PRINT("%s: i2c\n", __FUNCTION__);
	}
	/////////////// serial/USB //////////////////////////////
	else if ( strncmp (dev, "/dev/tty",  8 ) == 0 ) { // if /dev/ it must be a serial device
		lxScriboFd = NXP_I2C_Interface(dev, &lxScriboSerial_device);
	}

#else /////////////// Scribo server //////////////////////////////
	else if ( strncmp (dev, "scribo",  5 ) == 0 ) // Scribo server dll interface
		lxScriboFd = NXP_I2C_Interface(dev, &lxWindows_device);
	/////////////// Windows network //////////////////////////////
	else if ( strchr( dev , ':' ) != 0)	// if : in name > it's a socket
		lxScriboFd = NXP_I2C_Interface(dev, &lxScriboWinsock_device);
	else if ( strchr( dev , '@' ) != 0)	// if @ in name > it's a socket
		lxScriboFd = NXP_I2C_Interface(dev, &lxScriboWinsock_udp_device);
#endif
	else {
		ERRORMSG("%s: devicename %s is not a valid target\n", __FUNCTION__, dev); // anything else is a file
		lxScriboFd = -1;
	}

	if (lxScriboFd != -1) {
		/* add one device user*/
		refcnt++;
	}

	return lxScriboFd;
}

/**
 *  unregister the low level I2C HAL interface
 *  @return success (0) or fail (-1)
 */
int lxScriboUnRegister(void)
{
    /* one device user less */
	return --refcnt;

}

int lxScriboGetFd(void)
{
	if ( lxScriboFd < 0 )
		ERRORMSG("Warning: the target is not open\n");

	return lxScriboFd;
}

char * lxScriboGetName(void)
{
	return dev;
}

void  lxScriboVerbose(int level)
{
	lxScribo_verbose = level;
}

int lxScriboNrHidDevices(void)
{
#ifdef HAL_HID
	return lxHidNrDevices();
#else
	ERRORMSG("Warning: HID support is missing\n");
	return 0;
#endif
}

int lxScriboWaitOnPinEvent(int pin, int value, unsigned int timeout)
{
#ifdef HAL_HID
	return lxHidWaitOnPinEvent(pin, value, timeout);
#else
	ERRORMSG("Warning: HID support is missing\n");
        (void)pin;
        (void)value;
        (void)timeout;
	return 0;
#endif
}

