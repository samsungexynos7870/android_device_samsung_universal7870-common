/* implement Scribo protocol */

#include <stdio.h>
#include <stdint.h>
#if (defined(WIN32) || defined(_X64))
#include <windows.h>
/* Winsock can not use read/write from a socket */
#define read(fd, ptr, size) recv(fd, ptr, size, 0)
#define write(fd, ptr, size) send(fd, ptr, size, 0)
#else
#include <unistd.h>
#define Sleep(x) usleep((x)*1000)
#endif
#include "dbgprint.h"
#include "NXP_I2C.h"
#include "lxScribo.h"
#include <assert.h>

extern int lxScribo_verbose; /* lxSribo.c */
#define VERBOSE if (lxScribo_verbose)

/*
 * Command headers for communication
 *
 * note: commands headers are little endian
 */
const uint16_t cmdVersion   = 'v' ;              /* Version */
const uint16_t cmdRead      = 'r' ;              /* I2C read transaction  */
const uint16_t cmdWrite     = 'w' ;              /* I2C write transaction */
const uint16_t cmdWriteRead = ('w' << 8 | 'r');  /* I2C write+read transaction */
const uint16_t cmdPinSet    = ('p' << 8 | 's') ; /* set pin */
const uint16_t cmdPinRead   = ('p' << 8 | 'r') ; /* get pin */

const uint8_t terminator    = 0x02; /* terminator for commands and answers */

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

static int lxScriboGetResponseHeader(int fd, const uint16_t cmd, int* prlength)
{
	uint8_t response[6];
	uint16_t rcmd, rstatus;
	int length;

	length = read(fd, (char *)response, sizeof(response));

	VERBOSE hexdump("rsp:", response, sizeof(response));

	// response (lsb/msb)= echo cmd[0] cmd[1] , status [0] [1], length [0] [1] ...data[...]....terminator[0]
	if ( length==sizeof(response) )
	{
		rcmd    = response[0] | response[1]<<8;
		rstatus = response[2] | response[3]<<8;
		*prlength = response[4] | response[5]<<8;  /* extra bytes that need to be read */

		/* must get response to expected cmd */
		if  ( cmd != rcmd) {
			ERRORMSG("scribo protocol error: expected %d bytes , got %d bytes\n", cmd, rcmd);
		}
		if  (rstatus != 0) {
			ERRORMSG("scribo status error: 0x%02x\n", rstatus);
		}

		return (int)rstatus; /* iso length */
	}
	else {
		ERRORMSG("bad response length=%d\n", length);
		return -1;
	}
}

/*
 * set pin
 */
int lxScriboSetPin(int fd, int pin, int value)
{
	uint8_t cmd[6];
	int stat;
	int ret;
	uint8_t term;
	int rlength;

	VERBOSE PRINT("SetPin[%d]<%d\n", pin, value);

	cmd[0] = (uint8_t)(cmdPinSet & 0xff);
	cmd[1] = (uint8_t)(cmdPinSet >> 8);
	cmd[2] = (uint8_t)pin;
	cmd[3] = (uint8_t)value;
	cmd[4] = (uint8_t)(value >> 8);
	cmd[5] = terminator;

	// write header
	VERBOSE PRINT("cmd: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n",
			cmd[0], cmd[1],cmd[2],cmd[3],cmd[4], cmd[5]);
        ret = write(fd, (char *)cmd, sizeof(cmd));
	assert(ret == sizeof(cmd));

        stat = lxScriboGetResponseHeader( fd, cmdPinSet, &rlength);
	assert(stat == 0);
	assert(rlength == 0); // expect no return data
	ret = read(fd, (char *)&term, 1);
	assert(ret == 1);

	VERBOSE PRINT("term: 0x%02x\n", term);

	assert(term == terminator);

	return stat>=0;
}

/*
 * get pin state
 */
int lxScriboGetPin(int fd, int pin)
{
	uint8_t cmd[4];
	uint8_t value;
	int length;
	int ret;
	int rlength;

	cmd[0] = (uint8_t)(cmdPinRead & 0xff);
	cmd[1] = (uint8_t)(cmdPinRead >> 8);
	cmd[2] = (uint8_t)pin;
	cmd[3] = terminator;

	// write header
	VERBOSE PRINT("cmd:0x%02x 0x%02x 0x%02x 0x%02x\n",
			cmd[0], cmd[1],cmd[2],cmd[3]);
	ret = write(fd, (const char *)cmd, sizeof(cmd));
	assert(ret == sizeof(cmd));

	ret = lxScriboGetResponseHeader( fd, cmdPinRead, &rlength);
	assert(ret == 0);

	length = read(fd, (char *)&value, 1);
	assert(length == 1);

	VERBOSE PRINT("GetPin[%d]:%d\n", pin, value);

	return value;
}

/*
 * Close an opened device
 * Return success (0) or fail (-1)
 */
int lxScriboClose(int fd)
{
	(void)fd; /* Remove unreferenced parameter warning */
	VERBOSE PRINT("Function close not implemented for target scribo.");
	return 0;
}

/*
 * retrieve the version string from the device
 */
int lxScriboVersion(char *buffer, int fd)
{
	uint8_t cmd[3];
	int length;
	int ret;
	int rlength;

	cmd[0] = (uint8_t)(cmdVersion & 0xff);
	cmd[1] = (uint8_t)(cmdVersion >> 8);
	cmd[2] = terminator;

	ret = write(fd, (const char *)cmd, sizeof(cmd));
	assert(ret == sizeof(cmd));

	ret = lxScriboGetResponseHeader(fd, cmdVersion, &rlength);
	assert(ret == 0);
	assert(rlength > 0);

	length = read(fd, buffer, 256);
	/* no new line is added */

	return length;
}

static int lxScriboWrite(int fd, int size, const uint8_t *buffer, uint32_t *pError)
{
	uint8_t cmd[5], slave,term;
	int status, total=0;
	int rlength;

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
	VERBOSE PRINT("cmd: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x ",
			cmd[0], cmd[1],cmd[2],cmd[3],cmd[4]);
	status = write(fd, (char *)cmd, sizeof(cmd));
	if(status>0)
		total+=status;
	else
	{
		*pError = NXP_I2C_NoAck;
		return status;
	}
	// write payload
	VERBOSE hexdump("\t\twdata:", buffer, size);
	status = write(fd, (char *)buffer, size);
	if(status>0)
		total+=status;
	else
	{
		*pError = NXP_I2C_NoAck;
		return status;
	}
	// write terminator
	cmd[0] = terminator;
	VERBOSE PRINT("term: 0x%02x\n", cmd[0]);
	status = write(fd, (char *)cmd, 1);
	if(status>0)
		total+=status;
	else
	{
		*pError = NXP_I2C_NoAck;
		return status;
	}

	status = lxScriboGetResponseHeader(fd, cmdWrite, &rlength);
	if (status != 0)
	{
		*pError = status;
	}
	assert(rlength == 0); // expect no return data
	status = read(fd, (char *)&term, 1);
	if (status < 0)
	{
		if (*pError == NXP_I2C_Ok) *pError = NXP_I2C_NoAck;
		return status;
	}
	assert(term == terminator);
	VERBOSE PRINT("term: 0x%02x\n", term);
	return total;
}

int lxScriboWriteRead(int fd, int wsize, const uint8_t *wbuffer, int rsize,
		uint8_t *rbuffer, uint32_t *pError)
{
	uint8_t cmd[5], rcnt[2], slave, *rptr, term;
	int length = 0;
	int status, total = 0;
	int rlength;

	if ((rsize == 0) || (rbuffer == NULL)) {
		return lxScriboWrite(fd, wsize, wbuffer, pError);
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

		// write header
		VERBOSE hexdump("cmd:", cmd, sizeof(cmd));
		status = write(fd, (char *)cmd, sizeof(cmd));
		if (status > 0)
			total += status;
		else
		{
			*pError = NXP_I2C_NoAck;
			return status;
		}
		// write payload
		VERBOSE hexdump("\t\twdata:", wbuffer, wsize);
		status = write(fd, (char *)wbuffer, wsize);
		if (status > 0)
			total += status;
		else
		{
			*pError = NXP_I2C_NoAck;
			return status;
		}

		// write readcount
		rsize -= 1;
		rcnt[0] = (uint8_t)(rsize & 0xff); // lsb
		rcnt[1] = (uint8_t)(rsize >> 8);   // msb
		VERBOSE hexdump("rdcount:",rcnt, 2);
		status = write(fd, (char *)rcnt, 2);
		if (status > 0)
			total += status;
		else
		{
			*pError = NXP_I2C_NoAck;
			return status;
		}

		// write terminator
		cmd[0] = terminator;
		VERBOSE PRINT("term: 0x%02x\n", cmd[0]);
		status = write(fd, (char *)cmd, 1);
		if (status > 0)
			total += status;
		else
		{
			*pError = NXP_I2C_NoAck;
			return status;
		}

		//if( rcnt[1] | rcnt[0] >100)    // TODO check timing
		if (rsize > 100) // ?????????????????????????????????????
			Sleep(20);
		// slave is the 1st byte in rbuffer, remove here
		//rsize -= 1;
		rptr = rbuffer+1;
		// read back, blocking
		status = lxScriboGetResponseHeader(fd, cmdWriteRead, &rlength);
		if (status != 0)
		{
			*pError = status;
		}
		//assert(rlength == rsize);
		if  ( rlength != rsize) {
			ERRORMSG("scribo protocol error: expected %d bytes , got %d bytes\n", rsize, rlength);
		}

		//	VERBOSE PRINT("Reading %d bytes\n", rsize);
		length = read(fd, (char *)rptr, rsize);
		if (length < 0)
		{
			if (*pError == NXP_I2C_Ok) *pError = NXP_I2C_NoAck;
			return -1;
		}
		VERBOSE hexdump("\trdata:",rptr, rsize);
		// else something wrong, still read the terminator
		//	if(status>0) TODO handle error
		status = read(fd, (char *)&term, 1);
		if (length < 0)
		{
			if (*pError == NXP_I2C_Ok) *pError = NXP_I2C_NoAck;
			return -1;
		}
		assert(term == terminator);
		VERBOSE PRINT("rterm: 0x%02x\n", term);
	}
	else {
		PRINT("!!!! write slave != read slave !!! %s:%d\n", __FILE__, __LINE__);
		*pError = NXP_I2C_UnsupportedValue;
		status = -1;
		return status;
	}
	return length>0 ? (length + 1) : 0; // we need 1 more for the length because of the slave address
}
