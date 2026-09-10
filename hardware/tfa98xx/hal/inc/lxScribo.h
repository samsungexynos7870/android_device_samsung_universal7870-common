/*
 * lxScribo.h
 *
 *  Created on: Mar 14, 2012
 *      Author: nlv02095
 */

#ifndef LXSCRIBO_H_
#define LXSCRIBO_H_
#include"tfa98xx_cust.h"
#define LXSCRIBO_VERSION 	1.0

/** the default target device */
/*#define TFA_I2CDEVICE "hid"*/

/*
 * devkit pin assignments
 */

enum devkit_pinset {
	pinset_gpo_en_ana,
	pinset_gpo_en_i2s1,
	pinset_gpo_en_spdif,
	pinset_gpo_en_lpc,
	pinset_power_1v8, // 4
	pinset_led1,
	pinset_led2,
	pinset_led3,
	pinset_power_1v8_also, //8
	pinset_tfa_rst_l, // 9
	pinset_tfa_rst_r
};
enum devkit_pinget {
	pinget_gpi_det_i2s1,
	pinget_gpi_det_spdif,
	pinget_gpi_det_ana,
	pinget_gpi_det_i2s2,
	pinget_tfa_int_l, //4
	pinget_tfa_int_r
};

/* NXP_I2C interface plugin */
struct nxp_i2c_device {
	int (*init)(char *dev);
	int (*write_read)(int fd, int wsize, const unsigned char *wbuffer,
			int rsize, unsigned char *rbuffer, unsigned int *pError);
	int (*version_str)(char *buffer, int fd);
	int (*set_pin)(int fd, int pin, int value);
	int (*get_pin)(int fd, int pin);
	int (*close)(int fd);
};
int  NXP_I2C_Interface(char *target, const struct nxp_i2c_device *dev);

/* lxScribo.c */
void  lxScriboVerbose(int level); // set verbose level.
int lxScriboRegister(char *dev);  // register target and return opened file desc.
int lxScriboUnRegister(void);
int lxScriboGetFd(void);          // return active file desc.
char * lxScriboGetName(void);     // target name

int lxScriboNrHidDevices(void);
int lxScriboWaitOnPinEvent(int pin, int value, unsigned int timeout);

/* lxScriboSerial.c */
int lxScriboSerialInit(char *dev);

/* lxScriboSocket.c */
int lxScriboSocketInit(char *dev);
void lxScriboSocketExit(int status);

/* lxScriboProtocol.c */
int lxScriboWriteRead(int fd, int wsize, const unsigned char *wbuffer,
			int rsize, unsigned char *rbuffer, unsigned int *pError);
int lxScriboVersion(char *buffer, int fd);
int lxScriboSetPin(int fd, int pin, int value);
int lxScriboGetPin(int fd, int pin);
int lxScriboClose(int fd);

/* lxScriboProtocolUdp.c */
#if (defined(WIN32) || defined(_X64))
int lxScriboUdpWriteRead(int fd, int wsize, const unsigned char *wbuffer, int rsize, unsigned char *rbuffer, unsigned int *pError);
#endif
int lxScriboUdpVersion(char *buffer, int fd);
int lxScriboUdpGetPin(int fd, int pin);
int lxScriboUdpSetPin(int fd, int pin, int value);

#ifdef HAL_WIN_SIDE_CHANNEL
extern const struct nxp_i2c_device lxWinSideChannel_device;
#endif

#ifdef HAL_HID
extern const struct nxp_i2c_device lxHid_device;
int lxHidNrDevices(void);
int lxHidWaitOnPinEvent(int pin, int value, unsigned int timeout);
#endif

#ifdef HAL_SYSFS
extern const struct nxp_i2c_device lxSysfs_device;
#endif

extern const struct nxp_i2c_device lxDummy_device;

#if defined(WIN32) || defined(_X64)
extern const struct nxp_i2c_device lxWindows_device;
extern const struct nxp_i2c_device lxScriboWinsock_device;
extern const struct nxp_i2c_device lxScriboWinsock_udp_device;
#else  // posix/linux
extern const struct nxp_i2c_device lxI2c_device;
extern const struct nxp_i2c_device lxScriboSocket_device;
extern const struct nxp_i2c_device lxScriboSocket_udp_device;
extern const struct nxp_i2c_device lxScriboSerial_device;
#endif

extern int i2c_trace;

// for dummy
#define rprintf printf

//from gpio.h
#define I2CBUS 0

//from gui.h
/* IDs of menu items */
typedef enum
{
  ITEM_ID_VOLUME = 100,
  ITEM_ID_PRESET,
  ITEM_ID_EQUALIZER,

  ITEM_ID_STANDALONE,

  ITEM_ID_ENABLESCREENDUMP,
} menuElement_t;

#endif /* LXSCRIBO_H_ */
