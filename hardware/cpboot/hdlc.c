/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                  D O W N L O A D  P A C K E T   P R O C E S S I N G

GENERAL DESCRIPTION
  This module handles the DMSS Async Download Protocol to download
  code using simple generic UART services.  This consists of an
  infinite loop of receiving a command packet through the UART,
  acting on it, and returning a response packet.

EXTERNALIZED FUNCTIONS
  process_packets
    Runs the packet protocol (forever).

INITIALIZATION AND SEQUENCING REQUIREMENTS
  All necessary initialization for normal CPU operation must have
  been performed, and the UART must have been initialized, before
  entering this module.

Copyright (c) 1998-2005 by QUALCOMM Incorporated.
All Rights Reserved.
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/
//#define _DBL_TEST_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include "hdlc.h"
#include "boot.h"

#ifdef _DBL_TEST_
#include "dbl_binary.h"
#endif

#define DEBUG 1

#if 0

struct boot_data_list boot_data[MAX_MODEM_TYPE];

//int fd_uart0      = 0;
#else

struct boot_data_list boot_data[MAX_MODEM_TYPE];
struct incoming_packet packet_data;
dload_hdlc_state_type  hdlc_curr_state;
uint32          hdlc_escape_state;

/* Define the data structure in ZI section so we don't overflow the stack */
pkt_buffer_type params_buf;

unsigned int dbl_size;

dload_buffer_type tx_pkt;

int g_dload_state = DLOAD_ACK_STATE;
int g_ack_cnt     = 0;
int g_retry_cnt   = 0;
int g_exit        = 0;
int fd_uart0      = 0;
int g_uart_read_cnt		= 0;
int g_DBL_START_ADDR	= 0;
int g_use_uart			= 0;
#endif

#if defined(MODEM_ESC6270)
int UART_Read(int type, uint8 *ch)
{
	/* cbd_log("UART_Read : enter\n"); */

	struct timeval tv;
	fd_set rfds;
	int n;
	int ret;

	tv.tv_sec = 5;
	tv.tv_usec = 0;

	if (boot_data[type].fd_uart0 < 0)
	{
		cbd_log("[UART_Read] open failed : fd_uart0 is %d \n", boot_data[type].fd_uart0);
		//prv_exit();
		return 0;
	}

	FD_ZERO(&rfds);
	FD_SET(boot_data[type].fd_uart0, &rfds);

	/* cbd_log("UART_Read : select -start !\n"); */

	n = select(boot_data[type].fd_uart0 + 1, &rfds, NULL, NULL, &tv);

	if (n == 0)  /* timeout */
	{
		cbd_log("UART_Read : select - timeout occur!\n");
		/* Reset the modem */
		//prv_exit();
		return 0;
	}

	if (FD_ISSET(boot_data[type].fd_uart0, &rfds)) {

		/* cbd_log("read the data\n"); */
		ret = read(boot_data[type].fd_uart0, ch, sizeof(uint8));

		if(DEBUG)
		{
			cbd_log("[onedram: %s] %02X\n", __func__, *ch);
		}

		if(ret < 0)
		{
			/* perror("uart read failed!"); */

			if(errno == EAGAIN)
			{
				/* cbd_log("EAGAIN cnt is %d\n",g_uart_read_cnt); */
				usleep(50);
				boot_data[type].uart_read_cnt++;

				if(boot_data[type].uart_read_cnt > 500) {
					cbd_log("[*] UART_Read fail !! Restart GSM-RIL\n");
					//prv_exit();
					return 0;
				}
				return 0;
			}
			else
			{
				cbd_log("uart read failed! errno is (%d)\n",errno);
				boot_data[type].uart_read_cnt = 0;
				//prv_exit();
				return 0;
			}
		}
		else if(ret > 0)
		{
			cbd_log("[%s] uart read the data : 0x%02X\n", __func__, *ch);
			boot_data[type].uart_read_cnt = 0;
		}
	}
	else
		cbd_log("not setted fd\n");

	return 1;

}

void UART_Write(int type, uint8 ch)
{
	int ret;

	if(DEBUG)
		printf("[onedram: %s] %02X\n", __func__, ch);

	ret = write(boot_data[type].fd_uart0, &ch, sizeof(uint8));

	if(ret < 0)
	{
		cbd_log("uart1 write failed! errno is (%d)\n",errno);
		return;
	}

}
#else
int UART_Read(int type, uint8 *ch)
{
	int ret;

	ret = read(boot_data[type].fd_uart0, ch, sizeof(uint8));

	if (DEBUG)
		cbd_log("[onedram: %s] %02X\n", __func__, *ch);

	if(ret < 0)
	{
		cbd_log("uart0 read failed!\n");
		return 0;
	}

	return 1;
}

void UART_Write(int type, uint8 ch)
{
	if (DEBUG)
		cbd_log("[onedram: %s] %02X\n", __func__, ch);

	write(boot_data[type].fd_uart0, &ch, sizeof(uint8));
}
#endif

#if defined(MODEM_MDM6600)
int USB_Read(int type, uint8 *ch)
{
	int ret;

	ret = read(boot_data[type].fd_uart0, ch, sizeof(uint8));

	if (DEBUG)
		cbd_log("[DLOAD] %s ret=%d, %02X\n", __func__, ret, *ch);

	if(ret < 0)
	{
		cbd_log("USB read failed!\n");
		return 0;
	}

	return 1;
}

int USB_Read_bytes(int type, uint8 *ch, uint32 length)
{
	int ret;

	if (DEBUG)
		cbd_log("[DLOAD] %s: length: %lu\n", __func__, length);

	ret = read(boot_data[type].fd_uart0, ch, length);

	if (DEBUG)
		cbd_log("[DLOAD] %s: 0x%02X, ret=%d\n", __func__, *ch, ret);
	if(ret < 0)
	{
		cbd_log("USB read failed!\n");
		return 0;
	}

	return ret;
}
#endif

/*===========================================================================

MACRO START_BUILDING_PACKET

DESCRIPTION
  This macro initializes the process of dynamically building a packet.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.

DEPENDENCIES
  None.

RETURN VALUE
  None.

SIDE EFFECTS
  pkt is evaluated twice within this macro.
  This macro is not an expression, nor is it a single statement.  It
  must be called with a trailing semicolon.

===========================================================================*/

#define  START_BUILDING_PACKET(pkt)          \
               start_building_packet(&pkt)

/*===========================================================================

MACRO ADD_BYTE_TO_PACKET

DESCRIPTION
  This macro adds a single byte to a packet being built dynamically.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.
  val     The byte to be added to the packet.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

#define  ADD_BYTE_TO_PACKET(pkt,val)         \
               add_byte_to_packet(&pkt, val)

/*===========================================================================

MACRO ADD_WORD_TO_PACKET

DESCRIPTION
  This macro adds a word (most significant byte first) to a packet
  being built dynamically.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.
  val     The word to be added to the packet.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  Each argument is evaluated twice within this macro.
  This macro is not an expression, not is it a single statement.  It
  must be called with a trailing semicolon.

===========================================================================*/

#define  ADD_WORD_TO_PACKET(pkt,val)         \
  /*lint -e778 val or (val >> 8) may evaluate to zero. */                    \
  add_byte_to_packet(&pkt, (byte)((val >> 8) & 0xFF)); /* high byte */ \
  add_byte_to_packet(&pkt, (byte)(val & 0xFF))         /* low  byte */ \
  /*lint +e778 */

/*===========================================================================

MACRO ADD_DWORD_TO_PACKET

DESCRIPTION
  This macro adds a dword (most significant byte first) to a packet
  being built dynamically.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.
  val     The word to be added to the packet.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  Each argument is evaluated twice within this macro.
  This macro is not an expression, not is it a single statement.  It
  must be called with a trailing semicolon.

===========================================================================*/

#define  ADD_DWORD_TO_PACKET(pkt,val)         \
  /*lint -e778 val or (val >> 8) may evaluate to zero. */                  \
  add_byte_to_packet(&pkt, (byte)((val >> 24) & 0xFF)); /* byte 3 */ \
  add_byte_to_packet(&pkt, (byte)((val >> 16) & 0xFF)); /* byte 2 */ \
  add_byte_to_packet(&pkt, (byte)((val >> 8) & 0xFF));  /* byte 1 */ \
  add_byte_to_packet(&pkt, (byte)(val & 0xFF))          /* byte 0 */ \
  /*lint +e778 */

/*===========================================================================

MACRO ADD_TARGET_DWORD_TO_PACKET

DESCRIPTION
  This macro adds a dword (least significant byte first) to a packet
  being built dynamically.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.
  val     The word to be added to the packet.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  Each argument is evaluated twice within this macro.
  This macro is not an expression, not is it a single statement.  It
  must be called with a trailing semicolon.

===========================================================================*/

#define  ADD_TARGET_DWORD_TO_PACKET(pkt,val)         \
  /*lint -e778 val or (val >> 8) may evaluate to zero. */                  \
  add_byte_to_packet(&pkt, (byte)(val & 0xFF));         /* byte 0 */ \
  add_byte_to_packet(&pkt, (byte)((val >> 8) & 0xFF));  /* byte 1 */ \
  add_byte_to_packet(&pkt, (byte)((val >> 16) & 0xFF)); /* byte 2 */ \
  add_byte_to_packet(&pkt, (byte)((val >> 24) & 0xFF))  /* byte 3 */ \
  /*lint +e778 */


/*===========================================================================

MACRO ADD_CRC_TO_PACKET

DESCRIPTION
  This macro adds a word (LEAST significant byte first) to a packet
  being built dynamically.  This should only be used for the CRC,
  since other words are supposed to be sent most significant byte
  first.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.
  val     The word to be added to the packet.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  Each argument is evaluated twice within this macro.
  This macro is not an expression, not is it a single statement.  It
  must be called with a trailing semicolon.

===========================================================================*/

#define  ADD_CRC_TO_PACKET(pkt,val)         \
  add_byte_to_packet(&pkt, (byte)(val & 0xFF));        /* low  byte */ \
  add_byte_to_packet(&pkt, (byte)((val >> 8) & 0xFF))  /* high byte */

/*===========================================================================

MACRO ADD_STUFF_TO_PACKET

DESCRIPTION
  This macro adds an arbitrary buffer of bytes to a packet being built
  dynamically.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.
  data    A pointer to byte (or array of bytes) containing the data to be added
  len     The number of bytes to be added to the packet.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

#define  ADD_STUFF_TO_PACKET(pkt,data,len)   \
               /*lint -e717 Yes, this is a do...while(0). */\
               do {                                         \
               uint32   stuff_i;                            \
                                                            \
               for (stuff_i=0; stuff_i < len; stuff_i++)    \
                 {                                          \
                 add_byte_to_packet(&pkt, data[stuff_i]);   \
                 }                                          \
               } while (0)                                  \
               /*lint +e717 */

/*===========================================================================

MACRO ADD_STRING_TO_PACKET

DESCRIPTION
  This macro adds a text string (null terminated) to a packet being built
  dynamically.  If the string is not null terminated or is longer than 20
  characters, only the first 20 characters are added to the packet.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.
  str     The string to be added to the packet.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

#define  ADD_STRING_TO_PACKET(pkt,str)       \
		/*lint -e717 Yes, this is a do...while(0). */ \
		do {                                          \
		byte count = 0;                               \
		byte stopping_point;                          \
		                                         \
		stopping_point = dload_str_len(str);          \
		while (count < stopping_point)                \
		add_byte_to_packet(&pkt, str[count++]);     \
		} while (0)                                   \
               /*lint -e717 */

/*===========================================================================

MACRO FINISH_BUILDING_PACKET

DESCRIPTION
  This macro completes the process of building a packet dynamically.
  It just calls a function to do the work.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet has been built.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

#define  FINISH_BUILDING_PACKET(pkt)         \
               finish_building_packet(&pkt)


/*-------------------------------------------------------------------------*/

/* Mask for CRC-16 polynomial:
**
**      x^16 + x^12 + x^5 + 1
**
** This is more commonly referred to as CCITT-16.
** Note:  the x^16 tap is left off, it's implicit.
*/
#define CRC_16_L_POLYNOMIAL     0x8408

/* Seed value for CRC register.  The all ones seed is part of CCITT-16, as
** well as allows detection of an entire data stream of zeroes.
*/
#define CRC_16_L_SEED           0xFFFF

/* Residual CRC value to compare against return value of a CRC_16_L_STEP().
** Use CRC_16_L_STEP() to calculate a 16 bit CRC, complement the result,
** and append it to the buffer.  When CRC_16_L_STEP() is applied to the
** unchanged entire buffer, and complemented, it returns CRC_16_L_OK.
** That is, it returns CRC_16_L_OK_NEG.
*/
#define CRC_16_L_OK             0x0F47
#define CRC_16_L_OK_NEG         0xF0B8


/*===========================================================================

MACRO CRC_16_L_STEP

DESCRIPTION
  This macro calculates one byte step of an LSB-first 16-bit CRC.
  It can be used to produce a CRC and to check a CRC.

PARAMETERS
  xx_crc  Current value of the CRC calculation, 16-bits
  xx_c    New byte to figure into the CRC, 8-bits

DEPENDENCIES
  None

RETURN VALUE
  The new CRC value, 16-bits.  If this macro is being used to check a
  CRC, and is run over a range of bytes, the return value will be equal
  to CRC_16_L_OK_NEG if the CRC checks correctly according to the DMSS
  Async Download Protocol Spec.

SIDE EFFECTS
  xx_crc is evaluated twice within this macro.

===========================================================================*/
#define DLOAD_CRC_TAB_SIZE    256         /* 2^CRC_TAB_BITS      */
/* CRC table for 16 bit CRC, with generator polynomial 0x8408,
** calculated 8 bits at a time, LSB first.  This table is used
** from a macro in sio.c.
*/
const uint32 crc_16_l_table[ DLOAD_CRC_TAB_SIZE ] = {
  0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
  0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
  0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
  0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
  0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
  0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
  0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
  0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
  0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
  0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
  0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
  0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
  0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
  0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
  0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
  0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
  0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
  0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
  0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
  0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
  0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
  0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
  0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
  0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
  0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
  0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
  0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
  0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
  0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
  0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
  0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
  0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

#define CRC_16_L_STEP(xx_crc,xx_c) \
  (((xx_crc) >> 8) ^ crc_16_l_table[((xx_crc) ^ (xx_c)) & 0x00ff])

/*===========================================================================

FUNCTION add_byte_to_packet

DESCRIPTION
  This function adds a single byte to a packet that is being built
  dynamically.  It takes care of byte stuffing and checks for buffer
  overflow.

  This function is a helper function for the packet building macros
  and should not be called directly.

DEPENDENCIES
  The START_BUILDING_PACKET() macro should have been called on the
  packet buffer before calling this function.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

void add_byte_to_packet
(
  pkt_buffer_type  *pkt,
    /* Structure containing the packet being built */

  const byte        val
    /* The byte to be added to the packet */
)
{
	if (pkt->broken != FALSE)   /* If the packet is broken already, */
	{
		return;                  /* Don't do anything. */
	}

	/* Check if the byte needs escaping for transparency. */
	if (val == ASYNC_HDLC_FLAG || val == ASYNC_HDLC_ESC)
	{
		/* Check for an impending overflow. */
		if (pkt->length+2+ROOM_FOR_CRC+ROOM_FOR_FLAG >= MAX_REQUEST_LEN)
		{
			pkt->broken = TRUE;     /* Overflow.  Mark this packet broken. */
			return;
		}

		/* No overflow.  Escape the byte into the buffer. */
		pkt->buf[pkt->length++] = ASYNC_HDLC_ESC;
		pkt->buf[pkt->length++] = val ^ (byte)ASYNC_HDLC_ESC_MASK;
	}
	else     /* Byte doesn't need escaping. */
	{
		/* Check for an impending overflow. */
		if (pkt->length+1+ROOM_FOR_CRC+ROOM_FOR_FLAG >= MAX_REQUEST_LEN)
		{
			pkt->broken = TRUE;     /* Overflow.  Mark this packet broken. */
			return;
		}

		/* No overflow.  Place the byte into the buffer. */
		pkt->buf[pkt->length++] = val;
	}
}/* add_byte_to_packet() */

/*===========================================================================

FUNCTION finish_building_packet

DESCRIPTION
  This function completes the process of building a packet dynamically.
  If all is well, it adds the CRC and a trailing flag to the buffer.
  If an error has been encountered in building the packet, it substitutes
  a NAK packet for whatever has been built.

  This function is a helper function for the packet building macros
  and should not be called directly.

DEPENDENCIES
  The START_BUILDING_PACKET() macro should have been called on the
  packet buffer before calling this function.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

void finish_building_packet
(
  pkt_buffer_type  *pkt
    /* Structure containing the packet being built */
)
{
	uint32  crc;
	/* Cyclic Redundancy Check for the packet we've built. */

	uint32  i;
	/* Index for scanning through the packet, computing the CRC. */


	if (pkt->broken == FALSE)
	{
		/* Compute the CRC for all the bytes in the packet. */
		crc = CRC_16_L_SEED;
		for (i=0; i < pkt->length; i++)
		{
			/* According to the DMSS Download Protocol ICD, the CRC should only
			* be run over the raw data, not the escaped data, so since we
			* escaped the data as we built it, we have to back out any escapes
			* and uncomplement the escaped value back to its original value */
			if (pkt->buf[i] != ASYNC_HDLC_ESC)
			{
				crc = CRC_16_L_STEP(crc, (word) pkt->buf[i]);
			}
			else
			{
				i++;
				crc = CRC_16_L_STEP(crc,
					(word)(pkt->buf[i] ^ (byte)ASYNC_HDLC_ESC_MASK));
			}
		}
		crc ^= CRC_16_L_SEED;
		ADD_CRC_TO_PACKET(*pkt,crc);             /* Add the CRC to the packet */

		pkt->buf[pkt->length] = ASYNC_HDLC_FLAG;  /* Add a flag to the packet.*/
		                                      /* This can't use the regular
		                                         add_byte_to_packet() function
		                                         because it's a flag. */
	}
	else
	{
		(void) memcpy((void*)pkt->buf, (void *)rsp_nak_invalid_len,
		     sizeof(rsp_nak_invalid_len));    /* Substitute a NAK */
	}

}/* finish_building_packet() */

/*===========================================================================

FUNCTtION start_building_packet

DESCRIPTION
  This function completes the process of building a packet dynamically.
  If all is well, it adds the CRC and a trailing flag to the buffer.
  If an error has been encountered in building the packet, it substitutes
  a NAK packet for whatever has been built.

  This function is a helper function for the packet building macros
  and should not be called directly.

DEPENDENCIES
  The START_BUILDING_PACKET() macro should have been called on the
  packet buffer before calling this function.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/
void start_building_packet
(
  pkt_buffer_type  *pkt
    /* Structure containing the packet being built */
)
{
	pkt->length = 0;
	pkt->broken = FALSE;
}

/*===========================================================================

FUNCTION dload_uart_transmit_byte

DESCRIPTION
  This function transmits a single byte through the UART.

DEPENDENCIES
  The UART transmitter must be initialized and enabled, or this routine
  will wait forever.

RETURN VALUE
  None.

SIDE EFFECTS
  The watchdog may be reset.

===========================================================================*/

void dload_uart_transmit_byte
(
  int type,
  byte  chr
    /* Character to be transmitted */
)
{
	UART_Write(type, chr);
} /* dload_uart_transmit_byte() */


/*===========================================================================

FUNCTION transmit_byte

DESCRIPTION
  This function transmits a byte through the UART/USB interface.

DEPENDENCIES
  None.

RETURN VALUE
  None.

SIDE EFFECTS
  The watchdog may be reset.

===========================================================================*/
void
transmit_byte(
  int type,
  byte data
)
{
	dload_uart_transmit_byte(type, data);
}

/*===========================================================================

FUNCTION transmit_response

DESCRIPTION
  This function transmits a packet response through the UART/USB.
  This function supplies the entire packet, based on the type
  code passed in.

DEPENDENCIES
  Uses the response_table[] to find the packet to transmit.  This
  packet must be terminated by a flag.

RETURN VALUE
  None.

SIDE EFFECTS
  The watchdog may be reset.

===========================================================================*/
void transmit_response
(
  int type,
  response_code_type rsp
    /* Type of response to transmit */
)

{
#if !defined(MODEM_MDM6600)
	/* Pointer into the packet being transmitted */
	const byte *pkt;
	pkt = response_table[rsp];             /* Find the packet to transmit */

	transmit_byte(type, ASYNC_HDLC_FLAG);      /* Supply the leading flag  */

	do
	{
		transmit_byte(type, *pkt);         /* Transmit bytes from the buffer */
	}
	while (*pkt++ != ASYNC_HDLC_FLAG);  /* Until we've transmitted a flag */
#else
	/* Pointer into the packet being transmitted */
	const byte *pkt;
	int length = 0, i = 0;
	int padding;
	char *buffer = NULL;

	pkt = response_table[rsp];             /* Find the packet to transmit */

	while (pkt[length] != ASYNC_HDLC_FLAG)
		length++;

	length += 2; // ASYNC_HDLC_FLAG

	padding = (length % 4);
	padding = 4 - padding;

	buffer = (char *)calloc(length+padding, sizeof(char));
	if (buffer == NULL) {
		//LOGE("memory allocation error %s", strerror(errno));
		return;
	}

	buffer[0] = ASYNC_HDLC_FLAG;
	memcpy(buffer+1, pkt, length-1);
	length += padding;

	while (length > 0) {
		if (length > 16) {
			write(boot_data[type].fd_uart0, buffer + i, 16);
			length -= 16;
		}
		else {
			write(boot_data[type].fd_uart0, buffer + i, length);
			length = 0;
		}
		i += 16;
	}
	/* Transmit bytes from the buffer */
	usleep(300);

	free(buffer);
#endif

} /* transmit_response() */

/*===========================================================================

FUNCTION dload_uart_receive_byte

DESCRIPTION
  This function receives a single byte from the UART by polling.

DEPENDENCIES
  The UART must be initialized and enabled, or else this routine will
  wait forever.

RETURN VALUE
  If a character is received without error, returns the value of the
  character received.  If an error occurs, returns UART_RX_ERR.  If a
  timeout occurs, returns UART_TIMEOUT.

SIDE EFFECTS

===========================================================================*/
void dload_uart_receive_byte
(
	int type,
	byte   *buf,
	uint32 *length
)
{
	int  len;
	byte chr;

	len = UART_Read(type, (uint8 *)&chr);

	if(len == 1)
	{
		buf[0] = chr;	 /* Add byte to buffer */
		*length = 1;
	}
	else
	{
		*length = 0;
	}

	return;
}

int dload_uart_receive_bytes(int type, uint8 *ch, uint32 length)
{
	int ret = 0;
	int i = 0;

	ret = read(boot_data[type].fd_uart0, ch, length);

	if(ret < 0)
	{
		cbd_log("UART read failed!\n");
		usleep(50);
		boot_data[type].uart_read_cnt++;
		if(boot_data[type].uart_read_cnt > 100000) {	/* 50 us * 100000 = 5 sec */
			cbd_log("[%s] UART_Read fail !! Restart GSM-RIL\n", __func__);
			boot_data[type].uart_read_cnt = 0;
			//prv_exit();
			return 0;
		}
		return 0;
	}

	cbd_log("[DLOAD] %s: read %d bytes, retry count %d\n", __func__, ret, boot_data[type].uart_read_cnt);

	if (DEBUG) {
		for (i=0; i<ret; i++)
			cbd_log("[DLOAD] %s: 0x%02X\n", __func__, *(ch+i));
	}

	boot_data[type].uart_read_cnt = 0;

	return ret;
}

#if defined(MODEM_MDM6600)
void dload_usb_receive_byte
(
	int type,
	byte   *buf,
	uint32 *length
)
{
	int  len;
	byte chr;

	len = USB_Read(type, (uint8 *)&chr);

	if(len == 1)
	{
		buf[0] = chr;	 /* Add byte to buffer */
		*length = 1;
	}
	else
	{
		*length = 0;
	}

	return;
}


void dload_usb_receive_bytes
(
	int type,
	byte   *buf,
	uint32 *length
)
{
	int  len;

	len = USB_Read_bytes(type, (uint8 *)buf, *length);

	*length = len;

	return;
}
#endif

/*===========================================================================

FUNCTION transmit_packet

DESCRIPTION
  This function transmits a packet response through the UART/USB.

DEPENDENCIES
  The packet must end with a flag.

RETURN VALUE
  None.

SIDE EFFECTS
  The watchdog may be reset.

===========================================================================*/
void transmit_packet
(
  int type,
  pkt_buffer_type  *pkt
    /* The packet to be transmitted. */
)
{
	/* Pointer into the packet being transmitted */
	const byte *data = (byte*) pkt->buf;
	transmit_byte(type, ASYNC_HDLC_FLAG);      /* Supply the leading flag  */
	do
	{
		transmit_byte(type, *data);        /* Transmit bytes from the buffer */
	}
	while (*data++ != ASYNC_HDLC_FLAG); /* Until we've transmitted a flag */

} /* transmit_packet() */

#if defined(MODEM_MDM6600) || defined(MODEM_ESC6270)
void transmit_packets
(
  int type,
  pkt_buffer_type  *pkt
    /* The packet to be transmitted. */
)
{
	int length = 0, i = 0;;
	int padding;
	byte *buffer = NULL;

	while (pkt->buf[length] != ASYNC_HDLC_FLAG)
		length++;

	length += 2; // ASYNC_HDLC_FLAG

	if (DEBUG)
		cbd_log("%s : length = %d\n", __func__, length);

	padding = (length % 4);
	padding = 4 - padding;

	if (DEBUG)
		cbd_log("%s : padding = %d\n", __func__, padding);

	buffer = (byte *)calloc(length+padding, sizeof(byte));
	if (buffer == NULL) {
//		LOGE("memory allocation error %s", strerror(errno));
		return;
	}

	buffer[0] = ASYNC_HDLC_FLAG;
	memcpy(buffer+1, pkt->buf, length-1);
	length += padding;

	if (DEBUG)
		cbd_log("%s : now length is = %d\n", __func__, length);

	while (length > 0) {
		if (length > 16) {
			write(boot_data[type].fd_uart0, buffer + i, 16);
			length -= 16;
		}
		else {
			write(boot_data[type].fd_uart0, buffer + i, length);
			length = 0;
		}
		i += 16;
	}
	/* Transmit bytes from the buffer */
	usleep(300);

	free(buffer);
} /* transmit_packet() */
#endif

/*===========================================================================

FUNCTION dload_start_timer

DESCRIPTION
  None.

DEPENDENCIES
  None.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/
void dload_start_timer(uint32 value)
{
#ifdef __TODO__
  if(NU_Create_Timer(&dload_timer, "dload timer", dload_time_out, 0, value, value, NU_ENABLE_TIMER) != NU_SUCCESS)
  {
	cbd_log("[DLOAD] NU_Create_Timer failed\n");
	cbd_log("[DLOAD] SpinForever...\n");
  	while(1){}
  }
#endif
}

/*===========================================================================

FUNCTION dload_stop_timer

DESCRIPTION
  None.

DEPENDENCIES
  None.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/
void dload_stop_timer(void)
{
#ifdef __TODO__
  if(NU_Control_Timer(&dload_timer, NU_DISABLE_TIMER) != NU_SUCCESS)
  {
	cbd_log("\n[DLOAD] NU_Control_Timer failed\n");
	cbd_log("\n[DLOAD] SpinForever...\n");
	while(1){}
  }

  if(NU_Delete_Timer(&dload_timer) != NU_SUCCESS)
  {
	cbd_log("\n[DLOAD] NU_Delete_Timer failed\n");
	cbd_log("\n[DLOAD] SpinForever...\n");
	while(1){}
  }
#endif
}

/*===========================================================================

FUNCTION nop_req

DESCRIPTION

DEPENDENCIES
  None.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

void nop_req(int type)
{
//	cbd_log("[DLOAD] NOP_REQ\n");

	START_BUILDING_PACKET(boot_data[type].params_buf);
	ADD_BYTE_TO_PACKET(boot_data[type].params_buf, CMD_NOP);
	FINISH_BUILDING_PACKET(boot_data[type].params_buf);

#if !defined(MODEM_MDM6600)
	transmit_packet(&params_buf);
#else
	transmit_packets(type, &boot_data[type].params_buf);
#endif
} /* nop_req() */

/*===========================================================================

FUNCTION param_req

DESCRIPTION

DEPENDENCIES
  None.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

void param_req(int type)
{
//	cbd_log("[DLOAD] CMD_PREQ\n");

	START_BUILDING_PACKET(boot_data[type].params_buf);
	ADD_BYTE_TO_PACKET(boot_data[type].params_buf, CMD_PREQ);
	FINISH_BUILDING_PACKET(boot_data[type].params_buf);

#if !defined(MODEM_MDM6600)
	transmit_packet(&params_buf);
#else
	transmit_packets(type, &boot_data[type].params_buf);
#endif
} /* param_req() */

/*===========================================================================

FUNCTION write_32bit_req

DESCRIPTION

DEPENDENCIES
  None.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/
void write_32bit_req(int type, uint32 addr, byte *buf, uint32 len)
{
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

	START_BUILDING_PACKET(boot_data[type].params_buf);
	ADD_BYTE_TO_PACKET(boot_data[type].params_buf, CMD_WRITE_32BIT);
	ADD_DWORD_TO_PACKET(boot_data[type].params_buf, addr);
	ADD_WORD_TO_PACKET(boot_data[type].params_buf, len);
	ADD_STUFF_TO_PACKET(boot_data[type].params_buf, buf, len);
	FINISH_BUILDING_PACKET(boot_data[type].params_buf);

#if !defined(MODEM_MDM6600)
	transmit_packet(&params_buf);
#else
	transmit_packets(type, &boot_data[type].params_buf);
#endif
} /* write_32bit_req() */

/*===========================================================================

FUNCTION go_req

DESCRIPTION

DEPENDENCIES
  None.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

void go_req(int type)
{
	word segment, offset;

#if 1 // !defined(MODEM_MDM6600)
	segment = boot_data[type].dbl_start_addr >> 16; //0x7801;
	offset  = boot_data[type].dbl_start_addr & 0xFFFF;
#else
	segment = 0x2001;//0x20012028 >> 16; //boot_data[type].DBL_START_ADDR >> 16; //0x7801;
	offset  = 0x2000;//0x0000;
#endif

	if (DEBUG)
		cbd_log("[DLOAD] CMD_GO seg 0x%x, offset 0x%x\n\n", segment, offset);

	START_BUILDING_PACKET( boot_data[type].params_buf);
	ADD_BYTE_TO_PACKET( boot_data[type].params_buf, CMD_GO);
	ADD_WORD_TO_PACKET( boot_data[type].params_buf, segment);
	ADD_WORD_TO_PACKET( boot_data[type].params_buf, offset);
	FINISH_BUILDING_PACKET( boot_data[type].params_buf);

#if !defined(MODEM_MDM6600)
	transmit_packet(&params_buf);
#else
	transmit_packets(type, & boot_data[type].params_buf);

	if (DEBUG)
		cbd_log("[DLOAD] transmit_packets done !\n");
#endif

} /* go_req() */

/*===========================================================================

FUNCTION nak_cmd

DESCRIPTION

DEPENDENCIES
  None.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

void nak_cmd
(
  int type,
  byte	 *cmd_buf,
	/* Pointer to the received command packet */

  uint32 cmd_len
	/* Number of bytes received in the command packet */
)
{
	word status;
	uint32 len = 0;


	cbd_log("[DLOAD] NAK_CMD : state %d\n", boot_data[type].dload_state);

	B_PTR(status)[1] = cmd_buf[1];
	B_PTR(status)[0] = cmd_buf[2];

	cbd_log("[DLOAD] NAK_CMD : fail reason %x, Maybe You need to download phoneimage.\n", status);

	switch(boot_data[type].dload_state)
	{
		case DLOAD_ACK_STATE :
		{
            if(++boot_data[type].retry_cnt > MAX_RETRY_COUNT)
            {
                cbd_log("[DLOAD] Retry(%d) failed\n", boot_data[type].retry_cnt);
                cbd_log("[DLOAD] SpinForever...\n");
                while(1){}
            }
            else
            {
                nop_req(type);
            }
		}
		break;

        case DLOAD_PARA_STATE :
        {
            if(++boot_data[type].retry_cnt > MAX_RETRY_COUNT)
            {
                cbd_log("[DLOAD] Retry(%d) failed\n", boot_data[type].retry_cnt);
                cbd_log("[DLOAD] SpinForever...\n");
                while(1){}
            }
            else
            {
                param_req(type);
            }
        }
        break;

		case DLOAD_WRITE_STATE :
		{
			len = ((boot_data[type].tx_pkt.length >= MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : boot_data[type].tx_pkt.length) ;

			++boot_data[type].retry_cnt;

			cbd_log("[DLOAD] Retry(%d)\n", boot_data[type].retry_cnt);

			write_32bit_req(type, boot_data[type].tx_pkt.addr, boot_data[type].tx_pkt.ptr, len);
		}
		break;

		case DLOAD_GO_STATE :
		{
			cbd_log("[DLOAD] SpinForever...\n");
			while(1){}
	}
		//break;

		default :
			cbd_log("[DLOAD] Unknown State %d\n", boot_data[type].dload_state);
			cbd_log("[DLOAD] SpinForever...\n");
			while(1){}
			//break;
	}

} /* nak_cmd() */


/*===========================================================================

FUNCTION pres_cmd

DESCRIPTION

DEPENDENCIES
  None.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

void pres_cmd
(
  int type,
  byte	 *cmd_buf,
	/* Pointer to the received command packet */

  uint32 cmd_len
	/* Number of bytes received in the command packet */
)

{
	byte				version;
	byte				min_version;
	word				max_write_size;
	byte				model;
	byte				device_size;
	byte				device_type;
	uint32				len;


//	cbd_log("[DLOAD] CMD_PARAMS\n");

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#ifndef _DBL_TEST_
#if 0
	int nRead;

//	char *filename = "/data/ces_CUBIC37_OPEN.bin";
//	char *filename = "/data/dbl.mbn";
	char *filename = "/data/CAPELA6246_20090120.bin";

	fd_dbl = fopen(filename, "r");

	if(fd_dbl == NULL)
	{
		fcbd_log(stderr, "phone DBL file(%s) doesn't exist.\n", filename);
		exit(1);
	}

	nRead = fread(tx_pkt.buf, 1, 20*1024, fd_dbl);
//	cbd_log("fread read = %d\n", nRead);

	fclose(fd_dbl);
#endif
#endif

	if (cmd_len < WRITE_SIZ)       /* Make sure at least the header arrived  */
	{
		transmit_response(type, NAK_EARLY_END);        /* Nope, packet ended early   */
		return;
	}

	B_PTR(version)[0] = cmd_buf[1];
	B_PTR(min_version)[0] = cmd_buf[2];
	B_PTR(max_write_size)[1] = cmd_buf[3];
	B_PTR(max_write_size)[0] = cmd_buf[4];
	B_PTR(model)[0] = cmd_buf[5];
	B_PTR(device_size)[0] = cmd_buf[6];
	B_PTR(device_type)[0] = cmd_buf[7];

	if (DEBUG)
		cbd_log("[DLOAD] PRES_CMD : 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			version, min_version, max_write_size, model, device_size, device_type);

	boot_data[type].dload_state = DLOAD_WRITE_STATE;

	boot_data[type].tx_pkt.addr = boot_data[type].dbl_start_addr;

	boot_data[type].tx_pkt.length = boot_data[type].dbl_size;
#ifdef _DBL_TEST_
	memcpy(boot_data[type].tx_pkt.buf, dbl, boot_data[type].dbl_size);
#endif

	boot_data[type].tx_pkt.ptr = boot_data[type].tx_pkt.buf;

	len = ((boot_data[type].tx_pkt.length >= MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : boot_data[type].tx_pkt.length) ;

	write_32bit_req(type, boot_data[type].tx_pkt.addr, boot_data[type].tx_pkt.ptr, len);

} /* pres_cmd() */

/*===========================================================================

FUNCTION ack_cmd

DESCRIPTION

DEPENDENCIES
  None.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

void ack_cmd(int type)
{
	uint32 len;


	/* retry init */
	boot_data[type].retry_cnt = 0;

	switch(boot_data[type].dload_state)
	{
		case DLOAD_ACK_STATE :
		{
			boot_data[type].ack_cnt++;
//			cbd_log("[DLOAD] g_ack_cnt %d\n", g_ack_cnt);
			if(boot_data[type].ack_cnt == MAX_ACK_COUNT)
			{
				boot_data[type].ack_cnt = 0;
				boot_data[type].dload_state = DLOAD_PARA_STATE;
				param_req(type);
			}
			else
			{
				nop_req(type);
			}
		}
		break;

		case DLOAD_WRITE_STATE :
		{
			// previous by ryu 08-09-26
			len  = ((boot_data[type].tx_pkt.length >= MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : boot_data[type].tx_pkt.length);

			/* update info for next packet */
			boot_data[type].tx_pkt.addr	+= len ;
			boot_data[type].tx_pkt.ptr	+= len ;
			boot_data[type].tx_pkt.length -= len ;

			if(boot_data[type].tx_pkt.length)
			{
				// current by ryu in 08-09-26
				len = ((boot_data[type].tx_pkt.length >= MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : boot_data[type].tx_pkt.length);
				write_32bit_req(type, boot_data[type].tx_pkt.addr, boot_data[type].tx_pkt.ptr, len);
			}
			else
			{
			    #if !defined(MODEM_MDM6600)
				g_dload_state = DLOAD_GO_STATE;
                #else
				boot_data[type].dload_state = DLOAD_ACK_STATE;
				boot_data[type].exit = 1;
                #endif
				go_req(type);
			}
		}
		break;

		case DLOAD_GO_STATE :
		{
			cbd_log("[DLOAD] Download Completed !!!\n");
			boot_data[type].dload_state = DLOAD_ACK_STATE;
			boot_data[type].exit = 1;
		}
		break;

		default :
			cbd_log("[DLOAD] Unknown State %d\n", boot_data[type].dload_state);
			cbd_log("[DLOAD] SpinForever...\n");
			while(1){}
			//break;
	}
} /* ack_cmd() */

/*===========================================================================

FUNCTION dload_time_out

DESCRIPTION

DEPENDENCIES
  None.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/
void dload_time_out(int type, uint32 dload_timer_id)
{
  uint32 len = 0;

  cbd_log("[DLOAD] TimeOut !!!\n");

  /* timer off */
  dload_stop_timer();

  switch(boot_data[type].dload_state)
  {
    case DLOAD_ACK_STATE :
	{
	  if(++boot_data[type].retry_cnt > MAX_RETRY_COUNT)
	  {
		cbd_log("[DLOAD] Retry(%d) failed\n", boot_data[type].retry_cnt);
		cbd_log("[DLOAD] SpinForever...\n");
		while(1){}
	  }
	  else
	  {
		nop_req(type);
	  }
	}
	break;

    case DLOAD_PARA_STATE :
	{
	  if(++boot_data[type].retry_cnt > MAX_RETRY_COUNT)
	  {
		cbd_log("[DLOAD] Retry(%d) failed\n", boot_data[type].retry_cnt);
		cbd_log("[DLOAD] SpinForever...\n");
		while(1){}
	  }
	  else
	  {
		param_req(type);
	  }
	}
	break;

    case DLOAD_WRITE_STATE :
	{
		len = ((boot_data[type].tx_pkt.length >= MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : boot_data[type].tx_pkt.length) ;

		cbd_log("[DLOAD] DLOAD_WRITE_STATE Retry(%d)\n", boot_data[type].retry_cnt);
		++boot_data[type].retry_cnt;

		cbd_log("[DLOAD] Retry(%d)\n", boot_data[type].retry_cnt);

		write_32bit_req(type, boot_data[type].tx_pkt.addr, boot_data[type].tx_pkt.ptr, len);
	}
	break;

    case DLOAD_GO_STATE :
	{
	   if(++boot_data[type].retry_cnt > MAX_RETRY_COUNT)
	  {
		cbd_log("[DLOAD] DLOAD_GO_STATE Retry(%d) failed\n", boot_data[type].retry_cnt);
		cbd_log("[DLOAD] SpinForever...\n");
		while(1){}
	}
#if !defined(MODEM_MDM6600)
	  else
	  {
		cbd_log("[DLOAD] DLOAD_GO_STATE Retry Again (%d)\n", g_retry_cnt);

		go_req();
	  }
#endif
    }
	break;

	default :
		cbd_log("[DLOAD] Unknown State %d\n", boot_data[type].dload_state);
		cbd_log("[DLOAD] SpinForever...\n");
		while(1){}
	  //break;
  }

} /* dload_time_out() */

/*-------------------------------------------------------------------------
 * New Method to get data from the underlying driver.
 *-------------------------------------------------------------------------*/

/*===========================================================================

FUNCTION dload_packet_init

DESCRIPTION
  This function Initialize the packet variables.

DEPENDENCIES
  None.

RETURN VALUE
  Does not return.

SIDE EFFECTS
  None.

===========================================================================*/
void dload_packet_init (int type)
{
	boot_data[type].packet_data.length         = 0;
	boot_data[type].packet_data.end_location   = 0;
	boot_data[type].packet_data.start_location = 0;
	boot_data[type].packet_data.step_crc       = CRC_16_L_SEED;
	boot_data[type].packet_data.packet_ready   = FALSE;
}

/*===========================================================================

FUNCTION dload_enable_transmission

DESCRIPTION
  This function enables transmission of packet.

DEPENDENCIES
  None.

RETURN VALUE
  Does not return.

SIDE EFFECTS
  None.

===========================================================================*/
void dload_enable_transmission(int type)
{
	boot_data[type].packet_data.packet_ready = TRUE;
	boot_data[type].packet_data.end_location--;
}

/*===========================================================================

FUNCTION dload_enable_transmission

DESCRIPTION
  This function enables transmission of packet.

DEPENDENCIES
  None.

RETURN VALUE
  Does not return.

SIDE EFFECTS
  None.

===========================================================================*/
void dload_packet_handle_incoming_data(int type, boolean escape_char, byte ch)
{

	if (escape_char == FALSE)
	{
		if (boot_data[type].packet_data.length < MAX_PACKET_LEN)
		{
			boot_data[type].packet_data.buffer[boot_data[type].packet_data.end_location] = ch;
			boot_data[type].packet_data.step_crc= CRC_16_L_STEP(boot_data[type].packet_data.step_crc, (word)ch);
			boot_data[type].packet_data.end_location++;
		}
	}
	/* if the count becomes greater than the size of the packet it
	will be discarded.
	*/
	boot_data[type].packet_data.length++;
}


/*===========================================================================

FUNCTION dload_hdlc_init

DESCRIPTION
  Initialize HDLC parameters

DEPENDENCIES
  None.

RETURN VALUE
  Does not return.

SIDE EFFECTS
  None.

===========================================================================*/
#if defined(MODEM_MDM6600) || defined(MODEM_ESC6270)
void dload_hdlc_init (int type, int size, int dbl_addr, int use_uart)
{
	boot_data[type].hdlc_curr_state   = DLOAD_HDLC_IDLE_STATE;
	boot_data[type].hdlc_escape_state = 0;

	boot_data[type].dbl_size = size;
	boot_data[type].dbl_start_addr = dbl_addr;
	boot_data[type].use_uart = use_uart;
}

#else
void dload_hdlc_init (void)
{
	hdlc_curr_state   = DLOAD_HDLC_IDLE_STATE;
	hdlc_escape_state = 0;
}

#endif

/*===========================================================================

FUNCTION dload_hdlc_handle_data

DESCRIPTION
  This function stores a character received over the serial link in the
  receive buffer. It handles the escape character appropriately.

DEPENDENCIES
  None.

RETURN VALUE
  Does not return.

SIDE EFFECTS
  None.

===========================================================================*/
void dload_hdlc_handle_data (int type, byte ch)
{

	switch(ch)
	{
		case ASYNC_HDLC_FLAG:
			/* Found the end flag */
			dload_enable_transmission(type);
			boot_data[type].hdlc_curr_state   = DLOAD_HDLC_IDLE_STATE;
			boot_data[type].hdlc_escape_state = 0;
			break;

		case ASYNC_HDLC_ESC:
			boot_data[type].hdlc_escape_state = 1;
			dload_packet_handle_incoming_data(type, TRUE, ch);
			break;

		default:
			if(boot_data[type].hdlc_escape_state == 1)
			{
			ch ^= ASYNC_HDLC_ESC_MASK;
			boot_data[type].hdlc_escape_state = 0;
			}
			//send this character over to message handler...
			dload_packet_handle_incoming_data(type, FALSE, ch);
	}

}

/*===========================================================================

FUNCTION dload_hdlc_handle_incoming_byte

DESCRIPTION
 This function stores a character received over the serial link in the
 receive buffer. It handles the escape character appropriately.

DEPENDENCIES
  None.

RETURN VALUE
  Does not return.

SIDE EFFECTS
  None.

===========================================================================*/
void dload_hdlc_handle_incoming_byte (int type, byte ch)
{

	switch(boot_data[type].hdlc_curr_state)
	{
		case DLOAD_HDLC_IDLE_STATE:
			/* Once in this state Hunt for ASYNC_HDLC_FLAG */
			if (ch == ASYNC_HDLC_FLAG)
			{
				boot_data[type].hdlc_curr_state = DLOAD_HDLC_GOT_FLAG;
				boot_data[type].packet_data.length++;
			}
			break;

		case DLOAD_HDLC_GOT_FLAG:
			if (ch == ASYNC_HDLC_FLAG)
			{
				boot_data[type].packet_data.length++;
				break;
			}
			else
			{
				boot_data[type].packet_data.start_location  = boot_data[type].packet_data.length;
				boot_data[type].packet_data.end_location    = boot_data[type].packet_data.start_location;
				boot_data[type].hdlc_curr_state = DLOAD_HDLC_MSG_BODY_STATE;
			}

		case DLOAD_HDLC_MSG_BODY_STATE:
			dload_hdlc_handle_data(type, ch);
			break;

		default:
			/* Should never come here */
			break;
	}
}

/*===========================================================================

FUNCTION dload_get_data_from_device

DESCRIPTION
 This function stores a character received over the serial link in the
 receive buffer. It handles the escape character appropriately.

DEPENDENCIES
  None.

RETURN VALUE
  Return length of received bytes

SIDE EFFECTS
  None.

===========================================================================*/
uint32 dload_get_data_from_device(int type)
{
#if !defined(MODEM_MDM6600)
	uint32 len;
	/* pointer to buffer for receiving a packet */
	byte   *pkt_buf;

	pkt_buf = (byte *)&boot_data[type].packet_data.buffer[boot_data[type].packet_data.length];

	dload_uart_receive_byte(type, pkt_buf, &len);
#else
	uint32 len = 512;
	/* pointer to buffer for receiving a packet */
	byte   *pkt_buf;

	if (DEBUG)
		cbd_log("dload_get_data_from_device()+\n");

	pkt_buf = (byte *)&boot_data[type].packet_data.buffer[boot_data[type].packet_data.length];

	if (boot_data[type].use_uart)
		len = dload_uart_receive_bytes(type, pkt_buf, len);
	else
	len = USB_Read_bytes(type, pkt_buf, len);

#endif
	return len;
}

/*===========================================================================

FUNCTION dload_process_incoming_packets

DESCRIPTION
 This function stores a character received over the serial link in the
 receive buffer. It handles the escape character appropriately.

DEPENDENCIES
  None.

RETURN VALUE
  Does not return.

SIDE EFFECTS
  None.

===========================================================================*/
void dload_process_incoming_packets(int type)
{
	/* pointer to buffer for receiving a packet */
	byte  *pkt_buf;

	/* packet length */
	uint32 len;

	if (DEBUG)
		cbd_log("%s \n", __func__);

	if (boot_data[type].packet_data.packet_ready == FALSE)
		return;

	/* set pointer to base of buffer area */
	pkt_buf = (byte *)&boot_data[type].packet_data.buffer[boot_data[type].packet_data.start_location];
	len     = (boot_data[type].packet_data.end_location - boot_data[type].packet_data.start_location) + 1;

	if (len < MIN_PACKET_LEN)       /* Reject any too-short packets  */
	{
		transmit_response(type, NAK_EARLY_END);            /* Send NAK       */
	}
	else if (boot_data[type].packet_data.step_crc != CRC_16_L_OK_NEG)      /* Reject any with bad CRC */
	{
		transmit_response(type, NAK_INVALID_FCS);          /* Send NAK       */
	}
	else /* Yay, it's a good packet! */
	{
		/* Process the packet according to its command code */
		switch(pkt_buf[0])
		{
			case CMD_ACK:
				ack_cmd(type);
				break;

			case CMD_NAK:
				nak_cmd(type, pkt_buf, len);
				break;

			case CMD_PARAMS:
				pres_cmd(type, pkt_buf, len);
				break;

			default:
				transmit_response(type, NAK_INVALID_CMD);
				break;
		}/* switch on packet command code */
	}
	memset( (uint8*)&boot_data[type].packet_data.buffer[0], 0x0, boot_data[type].packet_data.length+1);
	dload_packet_init(type);

}/* dload_process_incoming_packets() */

