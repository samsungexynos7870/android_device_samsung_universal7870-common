/* implement Scribo protocol */

#include <stdio.h>
#include <stdint.h>
#if (defined(WIN32) || defined(_X64))
#include <windows.h>
#include "lxScriboWinsockUdp.h"
/* Winsock can not use read/write from a socket */
#define read(fd, ptr, size) recv(fd, (char*)ptr, size, 0)
#define write(fd, ptr, size) send(fd, (char*)ptr, size, 0)
#define udp_read(fd, inbuf, len, waitsec) winsock_udp_read(fd, (char*)inbuf, len, waitsec)
#define udp_write(fd, outbuf, len) winsock_udp_write(fd, (char*)outbuf, len)
#else
#include <unistd.h>
#define Sleep(x) usleep((x)*1000)
#include "udp_sockets.h"
#endif
#include "dbgprint.h"
#include "NXP_I2C.h"
#include "lxScribo.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern int lxScribo_verbose; /* lxSribo.c */
#define VERBOSE if (lxScribo_verbose)

/*
 * Command headers for communication
 *
 * note: commands headers are little endian
 */
static const uint16_t cmdVersion   = 'v' ;              /* Version */
static const uint16_t cmdRead      = 'r' ;              /* I2C read transaction  */
static const uint16_t cmdWrite     = 'w' ;              /* I2C write transaction */
static const uint16_t cmdWriteRead = ('w' << 8 | 'r');  /* I2C write+read transaction */
static const uint16_t cmdPinSet    = ('p' << 8 | 's') ; /* set pin */
static const uint16_t cmdPinRead   = ('p' << 8 | 'r') ; /* get pin */

static const uint8_t terminator    = 0x02; /* terminator for commands and answers */

static void hexdump(char *str, const unsigned char * data, int num_write_bytes) //TODO cleanup/consolidate all hexdumps
{
	int i;

	PRINT("%s", str);
	for(i=0;i<num_write_bytes;i++)
	{
		PRINT("0x%02x ", data[i]);
	}
	PRINT("\n");
	fflush(stdout);
}

/*
 * set pin
 */
int lxScriboUdpSetPin(int fd, int pin, int value)
{
	char cmd[6];
	char response[7];
	uint16_t error;
	int length;

	VERBOSE PRINT("SetPin[%d]<%d\n", pin, value);

	cmd[0] = (uint8_t)(cmdPinSet & 0xff);
	cmd[1] = (uint8_t)(cmdPinSet >> 8);
	cmd[2] = (uint8_t)pin;
	cmd[3] = (uint8_t)value;
	cmd[4] = (uint8_t)(value >> 8);
	cmd[5] = terminator;

	// write command
	VERBOSE hexdump("cmd:", (uint8_t *)cmd, sizeof(cmd));
	length = udp_write(fd, cmd, sizeof(cmd));
	assert(length == sizeof(cmd));

	// read response
	// response (lsb/msb)= echo cmd[0]='p' cmd[1]='s' ,status [0] [1], length [0]=0 [1]=0,terminator[0]=2
	length = udp_read(fd, response, sizeof(response), 0);

	VERBOSE hexdump("rsp:", (uint8_t *)response, sizeof(response));
	// check
	assert(length == sizeof(response)); 			// total packet size
	assert(response[1]=='p' && response[0]=='s'); 	// check return cmd
	assert((response[4] | response[5]<<8) == 0); 	// check datalength==0 bytes
	assert(response[6] == 2); 						// check terminator

	error = response[2] | response[3]<<8;
	if (error) {
		PRINT_ERROR("pin read error: %d\n", error);
		return error;
	}

	return 0;  // no  error
}

/*
 * get pin state
 */
int lxScriboUdpGetPin(int fd, int pin)
{
	char cmd[4];
	// response (lsb/msb)= echo cmd[0]='p' cmd[1]='r' , status [0] [1], length [0]=2 [1]=0, data[0] data[1],terminator[0]=2
	char response[9];
	uint16_t value, error;
	int length;
	int ret;

	cmd[0] = (uint8_t)(cmdPinRead & 0xff);
	cmd[1] = (uint8_t)(cmdPinRead >> 8);
	cmd[2] = (uint8_t)pin;
	cmd[3] = terminator;

	// write command
	VERBOSE hexdump("cmd:", (uint8_t *)cmd, sizeof(cmd));
	ret = udp_write(fd, cmd, sizeof(cmd));
	assert(ret == sizeof(cmd));

	// read response
	length = udp_read(fd, response, sizeof(response), 0);
	VERBOSE hexdump("rsp:", (uint8_t *)response, sizeof(response));
	assert(length == sizeof(response)); 		// total packet size
	assert(response[1]=='p' && response[0]=='r'); // check return cmd
	assert((response[4]|response[5]<<8) == 2); 	//check datalength=2bytes
	assert(response[8] == 2); 					//check terminator

	error = response[2] | response[3]<<8;
	if (error) {
		PRINT_ERROR("pin read error: %d\n", error);
		return 0;
	}
	value = response[6] << 8 | response[7];
	VERBOSE PRINT("GetPin[%d]:%d\n", pin, value);

	return value;
}

/*
 * Close an opened device
 * Return success (0) or fail (-1)
 */
int lxScriboUdpClose(int fd)
{
	(void)fd; /* Remove unreferenced for parameter warning */
	return 0;
}

/*
 * retrieve the version string from the device
 */
int lxScriboUdpVersion(char *buffer, int fd)
{
	char cmd[3];
	char response[NXP_I2C_MAX_SIZE]; // TODO fix buffer sizing
	int length;
	int ret;
	int rlength;
	uint16_t error;

	cmd[0] = (uint8_t)(cmdVersion & 0xff);
	cmd[1] = (uint8_t)(cmdVersion >> 8);
	cmd[2] = terminator;

	// write command
	VERBOSE hexdump("cmd:", (uint8_t *)cmd, sizeof(cmd));
	ret = udp_write(fd, cmd, sizeof(cmd));
	assert(ret == sizeof(cmd));


	// read response
	length = udp_read(fd, response, sizeof(response), 0);
	VERBOSE hexdump("rsp:", (uint8_t *)response, length);
	assert(length != 0 ); 		//
	assert(response[1]==0 && response[0]=='v'); // check return cmd

	rlength = response[4] | response[5]<<8;
	assert(response[6+rlength] == 2); 					//check terminator

	error = response[2] | response[3]<<8;
	if (error) {
		PRINT_ERROR("version read error: %d\n", error);
		return 0;
	}
	memcpy(buffer, response+6, rlength);

	return length;
}

static int lxScriboUdpWrite(int fd, int size, const uint8_t *buffer, uint32_t *pError)
{
	uint8_t cmd[1024], slave; //TODO use max buf from hal?
	//uint8_t term;
	int status, total=0;
	//int rlength;

	*pError = NXP_I2C_Ok;
	// slave is the 1st byte in wbuffer
	slave = buffer[0] >> 1;

	size -= 1;
	buffer++;

	cmd[0] = (uint8_t)cmdWrite;      // lsb
	cmd[1] = (uint8_t)(cmdWrite>>8); // msb
	cmd[2] = slave; // 1st byte is the i2c slave address
	cmd[3] = (uint8_t)(size & 0xff); // lsb
	cmd[4] = (uint8_t)(size >> 8);   // msb

	// write header
	// write payload
	memcpy(&cmd[5], buffer, size);
	// write terminator
	cmd[5+size] = terminator;

	VERBOSE hexdump("cmd:", (uint8_t *)cmd, 5+size+1);
#if (defined(WIN32) || defined(_X64))
	status = winsock_udp_write(fd, (char*) cmd, 5+size+1 );
#else
	status = udp_write(fd, (char*)cmd, 5+size+1 );
#endif
	if(status>0)
		total+=status;
	else
	{
		*pError = NXP_I2C_NoAck;
		return status;
	}

#if (defined(WIN32) || defined(_X64))
	status = winsock_udp_read(fd, (char*) cmd, 7, 0);
#else
	status = udp_read(fd, (char*)cmd, 7, 0);
#endif
	VERBOSE hexdump("rsp:", (uint8_t *)cmd, 7);

	if (status < 0)
	{
		if (*pError == NXP_I2C_Ok) *pError = NXP_I2C_NoAck;
		return status;
	}

	return total;
}

int lxScriboUdpWriteRead(int fd, int wsize, const uint8_t *wbuffer, int rsize,
		uint8_t *rbuffer, uint32_t *pError)
{
	uint8_t cmd[1024] = {0}, rcnt[2], slave, *rptr;
	// uint8_t term;
	int length = 0;
	int status, total = 0;
	//int rlength;

	if ((rsize == 0) || (rbuffer == NULL)) {
		return lxScriboUdpWrite(fd, wsize, wbuffer, pError);
	}

	*pError = NXP_I2C_Ok;
	// slave is the 1st byte in wbuffer
	slave = wbuffer[0] >> 1;

	wsize -= 1;
	wbuffer++;

	if ((slave<<1) + 1 == rbuffer[0]) // write & read to same target
			{
		//Format = 'wr'(16) + sla(8) + w_cnt(16) + data(8 * w_cnt) + r_cnt(16) + 0x02
		cmd[0] = (uint8_t)(cmdWriteRead & 0xFF);
		cmd[1] = (uint8_t)(cmdWriteRead >> 8);
		cmd[2] = slave;
		cmd[3] = (uint8_t)(wsize & 0xff); // lsb
		cmd[4] = (uint8_t)(wsize >> 8);   // msb

		memcpy(&cmd[5], wbuffer, wsize);

		//  readcount
		rsize -= 1;
		rcnt[0] = (uint8_t)(rsize & 0xff); // lsb
		rcnt[1] = (uint8_t)(rsize >> 8);   // msb
		//VERBOSE hexdump("rdcount:",rcnt, 2);
		cmd[5+wsize] = rcnt[0];
		cmd[6+wsize] = rcnt[1];

		//  terminator
		cmd[6+wsize+2-1] = terminator;

		VERBOSE hexdump("cmd:", (uint8_t *)cmd, 6+wsize+2);
		// now write at once
#if (defined(WIN32) || defined(_X64))
		status = winsock_udp_write(fd, (char*) cmd, 6+wsize+2);
#else
		status = udp_write(fd, (char*) cmd, 6+wsize+2);
#endif
		if (status > 0)
			total += status;
		else
		{
			*pError = NXP_I2C_NoAck;
			return status;
		}

		// slave is the 1st byte in rbuffer, remove here
		rptr = rbuffer+1;

		// response (lsb/msb)= echo cmd[0] cmd[1] , status [0] [1], length [0] [1] ...data[...]....terminator[0]
#if (defined(WIN32) || defined(_X64))
		total = winsock_udp_read(fd, (char*) cmd, sizeof(cmd), 0);
#else
		total = udp_read(fd, (char*)cmd, sizeof(cmd), 0);
#endif
		VERBOSE hexdump("rsp:", (uint8_t *)cmd, total);

		length = total - 6 - 1; //==rsize

		if(length < 1) {
			*pError = NXP_I2C_UnsupportedValue;
			return NXP_I2C_UnsupportedValue;
		} else {
			memcpy(rptr, &cmd[6], length);
		}

	}

	return length>0 ? (length + 1) : 0; // we need 1 more for the length because of the slave address
}

#if !(defined(WIN32) || defined(_X64))
int udp_socket_init(char *server, int port);
int lxScribo_udp_init(char *server)
{
	char *portnr;

	int port;

	if ( server==0 ) {
		fprintf (stderr, "%s:called for server, exiting for now...", __FUNCTION__);
		return -1;
	}

	portnr = strrchr(server , '@');
	if ( portnr == NULL )
	{
		fprintf (stderr, "%s: %s is not a valid servername, use host@port\n",__FUNCTION__, server);
		return -1;
	}

	*portnr++ ='\0'; //terminate
	port=atoi(portnr);

	return udp_socket_init(server, port);

}

const struct nxp_i2c_device lxScriboSocket_udp_device = {
		lxScribo_udp_init,
		lxScriboUdpWriteRead,
		lxScriboUdpVersion,
		lxScriboUdpSetPin,
		lxScriboUdpGetPin,
        lxScriboUdpClose
};
#endif
