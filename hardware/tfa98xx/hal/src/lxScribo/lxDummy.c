/*
 *Copyright 2014 NXP Semiconductors
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
 *  	The code is organized and ordered in the layering that matches the real devices:
 *  		- HAL interface called via Scribo registered functions
 *  		- I2C bus/slave handling code
 *  		- TFA I2C registers read/write
 *  		- CoolFlux subsystem, mainly dev[thisdev].xmem, read and write
 *  		- DSP RPC interaction response
 *  		- utility and helper functions
 *  		- static default parameter array defs
 */

/*
 * include files
 */
#include <stdio.h>
#if !(defined(WIN32) || defined(_X64))
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
/* TFA98xx API */
#include "dbgprint.h"
#include "Tfa98API.h"
#include "tfa_dsp_fw.h"
#include "tfa98xx_parameters.h"
#include "tfa9888n1b_genregs_por.h"
#include "lxScribo.h"
#include "NXP_I2C.h"
//#include "tfa98xx_genregs.h"
static char dummy_version_string[]="V1.0";

/******************************************************************************
 * globals
 */
typedef enum dummyType {
	tfa9888,
	tfa9887,
	tfa9890,
	tfa9890b,
	tfa9891,
	tfa9895,
	tfa9887b,
	tfa9897,
} dummyType_t;
static const char *typeNames[] = {
	"tfa9888",
	"tfa9887",
	"tfa9890",
	"tfa9890b",
	"tfa9891",
	"tfa9895",
	"tfa9887b",
	"tfa9897"
};

void convertBytes2Data24(int num_bytes, const unsigned char bytes[],
			       int data[]);
/* globals */
static int dummy_pins[100];
static int dummy_warm = 0; // can be set via "warm" argument
static int dummy_noclock = 0; // can be set via "noclk" argument
static int lxDummy_verbose = 0;
int lxDummyFailTest;		/* nr of the test to fail */

/******************************************************************************/
/* speakerboost defs TODO from fw include */

// this works because MODULE_FRAMEWORK==0
#define SB_ALL_ID           SB_PARAM_SET_ALGO_PARAMS //set all for sb1.0
#define SB_LAGW_ID          SB_PARAM_SET_LAGW
#define SB_RE0_ID           FW_PAR_ID_SETSENSESCAL
#define SB_LSMODEL_ID       SB_PARAM_SET_LSMODEL

#define ALGO_LENGTH (3*253)
#define LSMODEL_LENGTH (3*151)
/* for speaker parameters */
#define TFA2_SPEAKERPARAMETER_LENGTH		(3*151)	/* MAX2=450 MAX1=423 */
#define TFA1_SPEAKERPARAMETER_LENGTH		(3*141)	/* MAX2=450 MAX1=423 */
//#define TFA2_ALGOPARAMETER_LENGTH		(3*308)	/* N1B = (308) 309 is including the cmd-id     */
#define TFA1_ALGOPARAMETER_LENGTH		(3*253)
#define TFA98XX_FILTERCOEFSPARAMETER_LENGTH	(3*168)	/* 170 is including the cmd-id */

/**********/
/* printf higher level function activity like patch version, reset dsp params */
#if defined (WIN32) || defined(_X64)
#define FUNC_TRACE if (lxDummy_verbose & 1) printf
#else
#define FUNC_TRACE(va...) if (lxDummy_verbose & 1) PRINT("    dummy: " va)
#endif
static const char *fsname[]={"48k","44.1k","32k","24k","22.05k","16k","12k","11.0025k","8k"};

#define TFA_XMEM_PATCHVERSION 0x12bf
#define TFA9897_XMEM_PATCHVERSION 0x0d7f
#define TFA9891_XMEM_PATCHVERSION 0x13ff

/******************************************************************************
 * module globals
 */
#define MAX_DUMMIES 4
#define MAX_TRACELINE_LENGTH 256
/*
 * device structure that keeps the state for each individual target
 */
struct dummy_device{
	dummyType_t type;// = tfa9887;
	int config_length;
	uint8_t slave; //dev[thisdev].slave
	uint16_t Reg[256];
	uint8_t currentreg;		/* active register */
	int xmem_patch_version	; /* highest xmem address */
#define CF_PATCHMEM_START				(1024 * 16)
#define CF_PATCHMEM_LENGTH			(64*1024) //512
#define CF_XMEM_START						0
#define CF_XMEM_LENGTH					4800	/* 7008 */
#define CF_XMEMROM_START				8192
#define CF_XMEMROM_LENGTH			2048
#define CF_YMEM_START						0
#define CF_YMEM_LENGTH					512
#define CF_YMEMROM_START				2048
#define CF_YMEMROM_LENGTH			1536
#define CF_IO_PATCH_START					768
#define CF_IO_PATCH_LENGTH				40
#define CF_IO_CF_CONTROL_REG			0x8100
	uint8_t pmem[CF_PATCHMEM_LENGTH * 4];
	uint8_t ymem[CF_YMEM_LENGTH * 3];
	uint8_t iomem[CF_IO_PATCH_LENGTH * 3];
	uint8_t xmem[(CF_XMEMROM_START + CF_XMEMROM_LENGTH) * 3];	/* TODO treat xmemrom differently */
	int memIdx;		/* set via cf mad */
	uint16_t intack_sum; // bits for err and ack bits from TFA98XX_CF_STATUS
//	struct _intreg {
//		uint16_t out[3];
//		uint16_t in[3];
//		uint16_t ena[3];
//		uint16_t pol[3];
//	} intreg;
	uint8_t fracdelaytable[3*9];
	int irqpin;
	/* per device parameters */
	unsigned char lastConfig[TFA2_ALGOPARAMETER_LENGTH]; //tfa2 is largest
	unsigned char lastSpeaker[TFA2_SPEAKERPARAMETER_LENGTH];//tfa2 is largest
	unsigned char lastPreset[TFA98XX_PRESET_LENGTH]; //tfa1 (only) preset could be stored in lastConfig

#define BIQUAD_COEFF_LENGTH (BIQUAD_COEFF_SIZE*3)
	unsigned char filters[TFA98XX_FILTERCOEFSPARAMETER_LENGTH/BIQUAD_COEFF_LENGTH][BIQUAD_COEFF_LENGTH];
} ;
static struct dummy_device dev[MAX_DUMMIES]; // max
static int thisdev=0;
/*  */
 /* - I2C bus/slave handling code */
/*  */

static int i2cWrite(int length, const uint8_t *data);
static int i2cRead(int length, uint8_t *data);
/*  */
/* - TFA registers read/write */
/*  */
static void resetRegs(dummyType_t type);
static void setRomid(dummyType_t type);
static int tfaRead(uint8_t *data);
static int tfaWrite(const uint8_t *data);
/*  */
/* - CoolFlux subsystem, mainly dev[thisdev].xmem, read and write */
/*  */

static int lxDummyMemr(int type, uint8_t *data);
static int lxDummyMemw(int type, const uint8_t *data);
static int isClockOn(int thisdev); /* True if CF is ok and running */
/*  */
/* - DSP RPC interaction response */
/*  */
static int setgetDspParamsSpeakerboost(int param);	/* speakerboost */
static int setDspParamsFrameWork(int param);
static int setDspParamsEq(int param);
static int setDspParamsRe0(int param);
static int getStateInfo(void);
static void makeStateInfo(float agcGain, float limGain, float sMax,
			  int T, int statusFlag, float X1, float X2, float Re, int shortOnMips);
#if (defined( TFA9887B) || defined( TFA98XX_FULL ))
//static int getStateInfoDrc(void);
static void makeStateInfoDrc(float *);
static void makeStateInfoDrcInit(void); //fill
#endif
/* - function emulations */
static int updateInterrupt(void);

 /* - utility and helper functions */
static int dev2fam(struct dummy_device *this);
static int setInputFile(char *file);
static void hexdump(int num_write_bytes, const unsigned char *data);
static void convert24Data2Bytes(int num_data, unsigned char bytes[],
				int data[]);
static int dummy_get_bf(struct dummy_device *this, const uint16_t bf);
static int dummy_set_bf(struct dummy_device *this, const uint16_t bf, const uint16_t value);
/*  */

/*
 * redefine tfa1/2 family abstraction macros
 *  using the global device structures
 *   NOTE no need for dev_idx, it's always thisdev
 */
#define IS_FAM (dev2fam(&dev[thisdev]))
#undef TFA_FAM
#define TFA_FAM(fieldname) ((dev2fam(&dev[thisdev]) == 1) ? TFA1_BF_##fieldname :  TFA2_BF_##fieldname)
#undef TFA_FAM_FW
#define TFA_FAM_FW(fwname) ((dev2fam(&dev[thisdev])== 1) ? TFA1_FW_##fwname :  TFA2_FW_##fwname)

/* set/get bit fields to HW register*/
#undef TFA_SET_BF
#undef TFA_GET_BF
#define TFA_SET_BF(fieldname, value) dummy_set_bf(&dev[thisdev], TFA_FAM(fieldname), value)
#define TFA_GET_BF(fieldname) dummy_get_bf(&dev[thisdev], TFA_FAM(fieldname))

/* abstract family for register */
#define TFA98XX_CF_CONTROLS_REQ_MSK      0xff00
#define TFA98XX_CF_CONTROLS_RST_MSK      0x1
#define TFA98XX_CF_CONTROLS_CFINT_MSK    0x10
#define FAM_TFA98XX_CF_CONTROLS (TFA_FAM(RST) >>8) //use bitfield def to get register
#define FAM_TFA98XX_CF_MADD      (TFA_FAM(MADD)>>8)
#define FAM_TFA98XX_CF_MEM      (TFA_FAM(MEMA)>>8)
#define FAM_TFA98XX_CF_STATUS   (TFA_FAM(ACK)>>8)
#define FAM_TFA98XX_TDM_REGS	(TFA_FAM(TDMFSPOL)>>8) //regs base
// interrupt register have no common bitnames
#define FAM_TFA98XX_INT_REGS	(((dev2fam(&dev[thisdev]) == 1) ? 0x20 :  0x40)) // regsbase

/************************************ TFA1 reg defs ***************************************/
// some  legacy defs TODO cleanup
#define TFA1_AUDIO_CTR_VOL            (0xff<<8)
#define TFA1_AUDIO_CTR_VOL_POS        8
#define TFA1_AUDIO_CTR_VOL_LEN        8
#define TFA1_AUDIO_CTR_VOL_MAX        255
#define TFA1_AUDIO_CTR_VOL_MSK        0xff00
#define TFA98XX_I2SREG             0x04
#define TFA1_I2SREG_I2SSR             (0xf<<12)
#define TFA1_I2SREG_I2SSR_POS         12
#define TFA1_I2SREG_I2SSR_LEN         4
#define TFA1_I2SREG_I2SSR_MAX         15
#define TFA1_I2SREG_I2SSR_MSK         0xf000

#define TFA1_STATUSREG          0x00
#define TFA1_STATUSREG_PLLS           (0x1<<1)
#define TFA1_STATUSREG_CLKS           (0x1<<6)
#define TFA1_STATUSREG_AREFS          (0x1<<15)
/**  sys_ctrl Register ($09) *************************************************/

#define TFA98XX_SYS_CTRL           0x09
#define TFA98XX_SYS_CTRL_PWDN            (0x1<<0)
#define TFA98XX_SYS_CTRL_PWDN_POS        0
#define TFA98XX_SYS_CTRL_PWDN_LEN        1
#define TFA98XX_SYS_CTRL_PWDN_MAX        1
#define TFA98XX_SYS_CTRL_PWDN_MSK        0x1

#define TFA98XX_SYS_CTRL_I2CR            (0x1<<1)
#define TFA98XX_SYS_CTRL_I2CR_POS        1
#define TFA98XX_SYS_CTRL_I2CR_LEN        1
#define TFA98XX_SYS_CTRL_I2CR_MAX        1
#define TFA98XX_SYS_CTRL_I2CR_MSK        0x2

#define TFA98XX_SYS_CTRL_CFE             (0x1<<2)
#define TFA98XX_SYS_CTRL_CFE_POS         2
#define TFA98XX_SYS_CTRL_CFE_LEN         1
#define TFA98XX_SYS_CTRL_CFE_MAX         1
#define TFA98XX_SYS_CTRL_CFE_MSK         0x4

#define TFA98XX_SYS_CTRL_AMPE            (0x1<<3)
#define TFA98XX_SYS_CTRL_AMPE_POS        3
#define TFA98XX_SYS_CTRL_AMPE_LEN        1
#define TFA98XX_SYS_CTRL_AMPE_MAX        1
#define TFA98XX_SYS_CTRL_AMPE_MSK        0x8

#define TFA98XX_SYS_CTRL_DCA             (0x1<<4)
#define TFA98XX_SYS_CTRL_DCA_POS         4
#define TFA98XX_SYS_CTRL_DCA_LEN         1
#define TFA98XX_SYS_CTRL_DCA_MAX         1
#define TFA98XX_SYS_CTRL_DCA_MSK         0x10

#define TFA98XX_SYS_CTRL_SBSL            (0x1<<5)
#define TFA98XX_SYS_CTRL_SBSL_POS        5
#define TFA98XX_SYS_CTRL_SBSL_LEN        1
#define TFA98XX_SYS_CTRL_SBSL_MAX        1
#define TFA98XX_SYS_CTRL_SBSL_MSK        0x20

#define TFA98XX_SYS_CTRL_AMPC            (0x1<<6)
#define TFA98XX_SYS_CTRL_AMPC_POS        6
#define TFA98XX_SYS_CTRL_AMPC_LEN        1
#define TFA98XX_SYS_CTRL_AMPC_MAX        1
#define TFA98XX_SYS_CTRL_AMPC_MSK        0x40

#define TFA98XX_SYS_CTRL_DCDIS           (0x1<<7)
#define TFA98XX_SYS_CTRL_DCDIS_POS       7
#define TFA98XX_SYS_CTRL_DCDIS_LEN       1
#define TFA98XX_SYS_CTRL_DCDIS_MAX       1
#define TFA98XX_SYS_CTRL_DCDIS_MSK       0x80

#define TFA98XX_SYS_CTRL_PSDR            (0x1<<8)
#define TFA98XX_SYS_CTRL_PSDR_POS        8
#define TFA98XX_SYS_CTRL_PSDR_LEN        1
#define TFA98XX_SYS_CTRL_PSDR_MAX        1
#define TFA98XX_SYS_CTRL_PSDR_MSK        0x100

#define TFA98XX_SYS_CTRL_DCCV            (0x3<<9)
#define TFA98XX_SYS_CTRL_DCCV_POS        9
#define TFA98XX_SYS_CTRL_DCCV_LEN        2
#define TFA98XX_SYS_CTRL_DCCV_MAX        3
#define TFA98XX_SYS_CTRL_DCCV_MSK        0x600

#define TFA98XX_SYS_CTRL_CCFD            (0x1<<11)
#define TFA98XX_SYS_CTRL_CCFD_POS        11
#define TFA98XX_SYS_CTRL_CCFD_LEN        1
#define TFA98XX_SYS_CTRL_CCFD_MAX        1
#define TFA98XX_SYS_CTRL_CCFD_MSK        0x800

#define TFA98XX_SYS_CTRL_INTPAD          (0x3<<12)
#define TFA98XX_SYS_CTRL_INTPAD_POS      12
#define TFA98XX_SYS_CTRL_INTPAD_LEN      2
#define TFA98XX_SYS_CTRL_INTPAD_MAX      3
#define TFA98XX_SYS_CTRL_INTPAD_MSK      0x3000

#define TFA98XX_SYS_CTRL_IPLL            (0x1<<14)
#define TFA98XX_SYS_CTRL_IPLL_POS        14
#define TFA98XX_SYS_CTRL_IPLL_LEN        1
#define TFA98XX_SYS_CTRL_IPLL_MAX        1
#define TFA98XX_SYS_CTRL_IPLL_MSK        0x4000

/**  sys_ctrl Register ($09) *************************************************/

#define TFA9890_SYS_CTRL           0x09
#define TFA9890_SYS_CTRL_ISEL            (0x1<<13)
#define TFA9890_SYS_CTRL_ISEL_POS        13
#define TFA9890_SYS_CTRL_ISEL_LEN        1
#define TFA9890_SYS_CTRL_ISEL_MAX        1
#define TFA9890_SYS_CTRL_ISEL_MSK        0x2000

/***************************************************************************/
// max2
/***************************************************************************/

//#define TFA2_CF_CONTROLS               0x90
//#define TFA2_CF_MAD                    0x91
//#define TFA2_CF_MEM                    0x92
//#define TFA2_CF_STATUS                 0x93

/* max2 defs */
#define TFA98XX_STATUS_FLAGS0_PLLS                        (0x1<<1)
#define TFA98XX_STATUS_FLAGS0_AREFS                      (0x1<<13)
#define TFA98XX_STATUS_FLAGS0_CLKS                        (0x1<<5)

/*
 * (0x00)-sys_control0
 */

/*
 * powerdown
 */
#define TFA98XX_SYS_CONTROL0_PWDN                         (0x1<<0)
#define TFA98XX_SYS_CONTROL0_PWDN_POS                            0
#define TFA98XX_SYS_CONTROL0_PWDN_LEN                            1
#define TFA98XX_SYS_CONTROL0_PWDN_MAX                            1
#define TFA98XX_SYS_CONTROL0_PWDN_MSK                          0x1

/*
 * reset
 */
#define TFA98XX_SYS_CONTROL0_I2CR                         (0x1<<1)
#define TFA98XX_SYS_CONTROL0_I2CR_POS                            1
#define TFA98XX_SYS_CONTROL0_I2CR_LEN                            1
#define TFA98XX_SYS_CONTROL0_I2CR_MAX                            1
#define TFA98XX_SYS_CONTROL0_I2CR_MSK                          0x2

/*
 * enbl_coolflux
 */
#define TFA98XX_SYS_CONTROL0_CFE                          (0x1<<2)
#define TFA98XX_SYS_CONTROL0_CFE_POS                             2
#define TFA98XX_SYS_CONTROL0_CFE_LEN                             1
#define TFA98XX_SYS_CONTROL0_CFE_MAX                             1
#define TFA98XX_SYS_CONTROL0_CFE_MSK                           0x4

/*
 * enbl_amplifier
 */
#define TFA98XX_SYS_CONTROL0_AMPE                         (0x1<<3)
#define TFA98XX_SYS_CONTROL0_AMPE_POS                            3
#define TFA98XX_SYS_CONTROL0_AMPE_LEN                            1
#define TFA98XX_SYS_CONTROL0_AMPE_MAX                            1
#define TFA98XX_SYS_CONTROL0_AMPE_MSK                          0x8

/*
 * enbl_boost
 */
#define TFA98XX_SYS_CONTROL0_DCA                          (0x1<<4)
#define TFA98XX_SYS_CONTROL0_DCA_POS                             4
#define TFA98XX_SYS_CONTROL0_DCA_LEN                             1
#define TFA98XX_SYS_CONTROL0_DCA_MAX                             1
#define TFA98XX_SYS_CONTROL0_DCA_MSK                          0x10

/*
 * coolflux_configured
 */
#define TFA98XX_SYS_CONTROL0_SBSL                         (0x1<<5)
#define TFA98XX_SYS_CONTROL0_SBSL_POS                            5
#define TFA98XX_SYS_CONTROL0_SBSL_LEN                            1
#define TFA98XX_SYS_CONTROL0_SBSL_MAX                            1
#define TFA98XX_SYS_CONTROL0_SBSL_MSK                         0x20

/*
 * sel_enbl_amplifier
 */
#define TFA98XX_SYS_CONTROL0_AMPC                         (0x1<<6)
#define TFA98XX_SYS_CONTROL0_AMPC_POS                            6
#define TFA98XX_SYS_CONTROL0_AMPC_LEN                            1
#define TFA98XX_SYS_CONTROL0_AMPC_MAX                            1
#define TFA98XX_SYS_CONTROL0_AMPC_MSK                         0x40

/*
 * int_pad_io
 */
#define TFA98XX_SYS_CONTROL0_INTP                         (0x3<<7)
#define TFA98XX_SYS_CONTROL0_INTP_POS                            7
#define TFA98XX_SYS_CONTROL0_INTP_LEN                            2
#define TFA98XX_SYS_CONTROL0_INTP_MAX                            3
#define TFA98XX_SYS_CONTROL0_INTP_MSK                        0x180

/*
 * fs_pulse_sel
 */
#define TFA98XX_SYS_CONTROL0_FSSSEL                      (0x3<<10)
#define TFA98XX_SYS_CONTROL0_FSSSEL_POS                         10
#define TFA98XX_SYS_CONTROL0_FSSSEL_LEN                          2
#define TFA98XX_SYS_CONTROL0_FSSSEL_MAX                          3
#define TFA98XX_SYS_CONTROL0_FSSSEL_MSK                      0xc00


/************************************ BFFIELD_TRACE ***************************************/

#define TFA_DUMMY_BFFIELD_TRACE
//TODO get pointer to the table
#define SPKRBST_HEADROOM			7

#ifdef TFA_DUMMY_BFFIELD_TRACE
#include "tfa98xx_parameters.h"
#include "tfa98xx_tfafieldnames.h"
#include "tfa_service.h"

TFA2_NAMETABLE //TOTO remove defines here
TFA1_NAMETABLE
TFA9891_NAMETABLE
static char *tfa_fam_find_bitname( unsigned short bfEnum) {

	/* Remove the unused warning */
	bfEnum = bfEnum;
	return	0; //tfaContBitName(bfEnum, 0xdeadbeef, dev[thisdev].Reg[3]&0xff);

}

static int tfa_fam_trace_bitfields(int reg, uint16_t oldval, uint16_t newval) { //TODO fix or old max types
	uint16_t change = oldval ^ newval;
	uint16_t bfmask;
	int cnt = 0, bfname_idx, pos;
	int max_bfs=0;
	tfaBfName_t *bf_name,*DatasheetNames = {0};
	nxpTfaBfEnum_t *bf;
	char traceline[MAX_TRACELINE_LENGTH], thisname[MAX_TRACELINE_LENGTH];

	if (oldval == newval)
		return 0;

	if (IS_FAM == 1) {
		if (dev[thisdev].Reg[3]==0x92) {

		} else {
			DatasheetNames = Tfa9891DatasheetNames;
			max_bfs = (sizeof(Tfa9891DatasheetNames) / sizeof(tfaBfName_t) - 1);
		}
	}
	else {
		DatasheetNames = Tfa2DatasheetNames;
		max_bfs = (sizeof(Tfa2DatasheetNames) / sizeof(tfaBfName_t) - 1);
	}

	traceline[0]='\0';
	/* get through the bitfields aand look for target register */
	for (bfname_idx = 0; bfname_idx < max_bfs; bfname_idx++) { // datasheet names loop
		do { //register loop
			bf_name = &DatasheetNames[bfname_idx];
			bf = (nxpTfaBfEnum_t *) &bf_name->bfEnum;
			pos = bf->pos;
			bfmask = ((1 << (bf->len + 1)) - 1) << pos;
			if (bf->address != (unsigned int)reg)
				break;
			/* go to the first changed  bit and find the bitfield that describes it */
			if (change & bfmask) { //changed bit in this bitfield
				int new = (bfmask & newval) >> pos;
				int old = (bfmask & oldval) >> pos;
				if (bf->len > 0) // only print last value for longer fields
					sprintf(thisname, " %s (%s)=%d (%d)", bf_name->bfName,
							tfa_fam_find_bitname(bf_name->bfEnum), new, old);
				else
					sprintf(thisname," %s (%s)=%d", bf_name->bfName,
							tfa_fam_find_bitname(bf_name->bfEnum), new);
				cnt++;
				strncat(traceline, thisname,
						sizeof(traceline) - strlen(traceline) - strlen(thisname));
			}
			bfname_idx++; //bitfieldloop
		} while (bf->address == (unsigned int)reg); //register loop
	} // datasheet names loop

	if (cnt==0) /* none found, just print reg */
			sprintf(traceline, " 0x%02x=0x%04x (0x%04x)", reg, newval, oldval);

	if (lxDummy_verbose)// & 2)
		PRINT("\t%s\n",traceline);

	return cnt;
}

/*
 * print the list of changed bitfields in the named register
 */
int lxDummy_trace_bitfields(int reg, uint16_t oldval, uint16_t newval) {

	return tfa_fam_trace_bitfields(reg, oldval, newval);

//	if (IS_FAM==2)
//		return tfa2_trace_bitfields(reg, oldval, newval);
//	else if (IS_FAM==1)
//		return tfa1_trace_bitfields(reg, oldval, newval);
//
//	PRINT_ERROR("family type is wrong:%d\n", IS_FAM);
//	return -1;
}

#endif // TFA_DUMMY_BFFIELD_TRACE


/* the following replaces the former "extern regdef_t regdefs[];"
 *  TODO replace this by a something generated from the regmap
 */
/* *INDENT-OFF* */
static regdef_t regdefs[] = {
		{TFA98XX_SYS_CONTROL0, TFA98XX_SYS_CONTROL0_POR, 0, "TFA98XX_SYS_CONTROL0"},
		{TFA98XX_SYS_CONTROL1, TFA98XX_SYS_CONTROL1_POR, 0, "TFA98XX_SYS_CONTROL1_POR"},
		{TFA98XX_SYS_CONTROL2, TFA98XX_SYS_CONTROL2_POR, 0, "TFA98XX_SYS_CONTROL2_POR"},
		{TFA98XX_DEVICE_REVISION, TFA98XX_DEVICE_REVISION_POR, 0, "TFA98XX_DEVICE_REVISION_POR"},
		{TFA98XX_CLOCK_CONTROL, TFA98XX_CLOCK_CONTROL_POR, 0, "TFA98XX_CLOCK_CONTROL_POR"},
		{TFA98XX_CLOCK_GATING_CONTROL, TFA98XX_CLOCK_GATING_CONTROL_POR, 0, "TFA98XX_CLOCK_GATING_CONTROL_POR"},
		{TFA98XX_SIDE_TONE_CONFIG, TFA98XX_SIDE_TONE_CONFIG_POR, 0, "TFA98XX_SIDE_TONE_CONFIG_POR"},
		{TFA98XX_STATUS_FLAGS0, TFA98XX_STATUS_FLAGS0_POR, 0, "TFA98XX_STATUS_FLAGS0_POR"},
		{TFA98XX_STATUS_FLAGS1, TFA98XX_STATUS_FLAGS1_POR, 0, "TFA98XX_STATUS_FLAGS1_POR"},
		{TFA98XX_STATUS_FLAGS2, TFA98XX_STATUS_FLAGS2_POR, 0, "TFA98XX_STATUS_FLAGS2_POR"},
		{TFA98XX_STATUS_FLAGS3, TFA98XX_STATUS_FLAGS3_POR, 0, "TFA98XX_STATUS_FLAGS3_POR"},
		{TFA98XX_BATTERY_VOLTAGE, TFA98XX_BATTERY_VOLTAGE_POR, 0, "TFA98XX_BATTERY_VOLTAGE_POR"},
		{TFA98XX_TEMPERATURE, TFA98XX_TEMPERATURE_POR, 0, "TFA98XX_TEMPERATURE_POR"},
		{TFA98XX_TDM_CONFIG0, TFA98XX_TDM_CONFIG0_POR, 0, "TFA98XX_TDM_CONFIG0_POR"},
		{TFA98XX_TDM_CONFIG1, TFA98XX_TDM_CONFIG1_POR, 0, "TFA98XX_TDM_CONFIG1_POR"},
		{TFA98XX_TDM_CONFIG2, TFA98XX_TDM_CONFIG2_POR, 0, "TFA98XX_TDM_CONFIG2_POR"},
		{TFA98XX_TDM_CONFIG3, TFA98XX_TDM_CONFIG3_POR, 0, "TFA98XX_TDM_CONFIG3_POR"},
		{TFA98XX_TDM_CONFIG4, TFA98XX_TDM_CONFIG4_POR, 0, "TFA98XX_TDM_CONFIG4_POR"},
		{TFA98XX_TDM_CONFIG5, TFA98XX_TDM_CONFIG5_POR, 0, "TFA98XX_TDM_CONFIG5_POR"},
		{TFA98XX_TDM_CONFIG6, TFA98XX_TDM_CONFIG6_POR, 0, "TFA98XX_TDM_CONFIG6_POR"},
		{TFA98XX_TDM_CONFIG7, TFA98XX_TDM_CONFIG7_POR, 0, "TFA98XX_TDM_CONFIG7_POR"},
		{TFA98XX_TDM_CONFIG8, TFA98XX_TDM_CONFIG8_POR, 0, "TFA98XX_TDM_CONFIG8_POR"},
		{TFA98XX_TDM_CONFIG9, TFA98XX_TDM_CONFIG9_POR, 0, "TFA98XX_TDM_CONFIG9_POR"},
		{TFA98XX_PDM_CONFIG0, TFA98XX_PDM_CONFIG0_POR, 0, "TFA98XX_PDM_CONFIG0_POR"},
		{TFA98XX_PDM_CONFIG1, TFA98XX_PDM_CONFIG1_POR, 0, "TFA98XX_PDM_CONFIG1_POR"},
		{TFA98XX_HAPTIC_DRIVER_CONFIG, TFA98XX_HAPTIC_DRIVER_CONFIG_POR, 0, "TFA98XX_HAPTIC_DRIVER_CONFIG_POR"},
		{TFA98XX_GPIO_DATAIN_REG, TFA98XX_GPIO_DATAIN_REG_POR, 0, "TFA98XX_GPIO_DATAIN_REG_POR"},
		{TFA98XX_GPIO_CONFIG, TFA98XX_GPIO_CONFIG_POR, 0, "TFA98XX_GPIO_CONFIG_POR"},
		{TFA98XX_INTERRUPT_OUT_REG1, TFA98XX_INTERRUPT_OUT_REG1_POR, 0, "TFA98XX_INTERRUPT_OUT_REG1_POR"},
		{TFA98XX_INTERRUPT_OUT_REG2, TFA98XX_INTERRUPT_OUT_REG2_POR, 0, "TFA98XX_INTERRUPT_OUT_REG2_POR"},
		{TFA98XX_INTERRUPT_OUT_REG3, TFA98XX_INTERRUPT_OUT_REG3_POR, 0, "TFA98XX_INTERRUPT_OUT_REG3_POR"},
		{TFA98XX_INTERRUPT_IN_REG1, TFA98XX_INTERRUPT_IN_REG1_POR, 0, "TFA98XX_INTERRUPT_IN_REG1_POR"},
		{TFA98XX_INTERRUPT_IN_REG2, TFA98XX_INTERRUPT_IN_REG2_POR, 0, "TFA98XX_INTERRUPT_IN_REG2_POR"},
		{TFA98XX_INTERRUPT_IN_REG3, TFA98XX_INTERRUPT_IN_REG3_POR, 0, "TFA98XX_INTERRUPT_IN_REG3_POR"},
		{TFA98XX_INTERRUPT_ENABLE_REG1, TFA98XX_INTERRUPT_ENABLE_REG1_POR, 0, "TFA98XX_INTERRUPT_ENABLE_REG1_POR"},
		{TFA98XX_INTERRUPT_ENABLE_REG2, TFA98XX_INTERRUPT_ENABLE_REG2_POR, 0, "TFA98XX_INTERRUPT_ENABLE_REG2_POR"},
		{TFA98XX_INTERRUPT_ENABLE_REG3, TFA98XX_INTERRUPT_ENABLE_REG3_POR, 0, "TFA98XX_INTERRUPT_ENABLE_REG3_POR"},
		{TFA98XX_STATUS_POLARITY_REG1, TFA98XX_STATUS_POLARITY_REG1_POR, 0, "TFA98XX_STATUS_POLARITY_REG1_POR"},
		{TFA98XX_STATUS_POLARITY_REG2, TFA98XX_STATUS_POLARITY_REG2_POR, 0, "TFA98XX_STATUS_POLARITY_REG2_POR"},
		{TFA98XX_STATUS_POLARITY_REG3, TFA98XX_STATUS_POLARITY_REG3_POR, 0, "TFA98XX_STATUS_POLARITY_REG3_POR"},
		{TFA98XX_BAT_PROT_CONFIG, TFA98XX_BAT_PROT_CONFIG_POR, 0, "TFA98XX_BAT_PROT_CONFIG_POR"},
		{TFA98XX_AUDIO_CONTROL, TFA98XX_AUDIO_CONTROL_POR, 0, "TFA98XX_AUDIO_CONTROL_POR"},
		{TFA98XX_AMPLIFIER_CONFIG, TFA98XX_AMPLIFIER_CONFIG_POR, 0, "TFA98XX_AMPLIFIER_CONFIG_POR"},
		{TFA98XX_DCDC_CONTROL0, TFA98XX_DCDC_CONTROL0_POR, 0, "TFA98XX_DCDC_CONTROL0_POR"},
		{TFA98XX_CF_CONTROLS, TFA98XX_CF_CONTROLS_POR, 0, "TFA98XX_CF_CONTROLS_POR"},
		{TFA98XX_CF_MAD, TFA98XX_CF_MAD_POR, 0, "TFA98XX_CF_MAD_POR"},
		{TFA98XX_CF_MEM, TFA98XX_CF_MEM_POR, 0, "TFA98XX_CF_MEM_POR"},
		{TFA98XX_CF_STATUS, TFA98XX_CF_STATUS_POR, 0, "TFA98XX_CF_STATUS_POR"},
		{TFA98XX_MTPKEY2_REG, TFA98XX_MTPKEY2_REG_POR, 0, "TFA98XX_MTPKEY2_REG_POR"},
		{TFA98XX_KEY_PROTECTED_MTP_CONTROL, TFA98XX_KEY_PROTECTED_MTP_CONTROL_POR, 0, "TFA98XX_KEY_PROTECTED_MTP_CONTROL_POR"},
		{TFA98XX_TEMP_SENSOR_CONFIG, TFA98XX_TEMP_SENSOR_CONFIG_POR, 0, "TFA98XX_TEMP_SENSOR_CONFIG_POR"},
		{TFA98XX_KEY2_PROTECTED_MTP0, TFA98XX_KEY2_PROTECTED_MTP0_POR, 0, "TFA98XX_KEY2_PROTECTED_MTP0_POR"}
};
static regdef_t regdefs_mx1[] = {
        { 0x00, 0x081d, 0xfeff, "statusreg"}, //ignore MTP busy bit
        { 0x01, 0x0, 0x0, "batteryvoltage"},
        { 0x02, 0x0, 0x0, "temperature"},
        { 0x03, 0x0012, 0xffff, "revisionnumber"},
        { 0x04, 0x888b, 0xffff, "i2sreg"},
        { 0x05, 0x13aa, 0xffff, "bat_prot"},
        { 0x06, 0x001f, 0xffff, "audio_ctr"},
        { 0x07, 0x0fe6, 0xffff, "dcdcboost"},
        { 0x08, 0x0800, 0x3fff, "spkr_calibration"}, //NOTE: this is a software fix to 0xcoo
        { 0x09, 0x041d, 0xffff, "sys_ctrl"},
        { 0x0a, 0x3ec3, 0x7fff, "i2s_sel_reg"},
        { 0x40, 0x0, 0x00ff, "hide_unhide_key"},
        { 0x41, 0x0, 0x0, "pwm_control"},
        { 0x46, 0x0, 0x0, "currentsense1"},
        { 0x47, 0x0, 0x0, "currentsense2"},
        { 0x48, 0x0, 0x0, "currentsense3"},
        { 0x49, 0x0, 0x0, "currentsense4"},
        { 0x4c, 0x0, 0xffff, "abisttest"},
        { 0x62, 0x0, 0, "mtp_copy"},
        { 0x70, 0x0, 0xffff, "cf_controls"},
        { 0x71, 0x0, 0, "cf_mad"},
        { 0x72, 0x0, 0, "cf_mem"},
        { 0x73, 0x0000, 0xffff, "cf_status"},
        { 0x80, 0x0, 0, "mtp"},
        { 0x83, 0x0, 0, "mtp_re0"},
        { 0xff, 0,   0, NULL},
};
static regdef_t tdm_regdefs_mx1[] = {
		{ 0x10, 0x0220, 0, "tdm_config_reg0"},
		{ 0x11, 0xc1f1, 0, "tdm_config_reg1"},
		{ 0x12, 0x0020, 0, "tdm_config_reg2"},
		{ 0x13, 0x0000, 0, "tdm_config_reg3"},
		{ 0x14, 0x2000, 0, "tdm_config_reg4"},
		{ 0x15, 0x0000, 0, "tdm_status_reg"},
        { 0xff, 0,0, NULL}
};

static regdef_t int_regdefs_mx1[] = {
		{ 0x20, 0x0000, 0, "int_out0"},
		{ 0x21, 0x0000, 0, "int_out1"},
		{ 0x22, 0x0000, 0, "int_out2"},
		{ 0x23, 0x0000, 0, "int_in0"},
		{ 0x24, 0x0000, 0, "int_in1"},
		{ 0x25, 0x0000, 0, "int_in2"},
		{ 0x26, 0x0001, 0, "int_ena0"},
		{ 0x27, 0x0000, 0, "int_ena1"},
		{ 0x28, 0x0000, 0, "int_ena2"},
		{ 0x29, 0xf5e2, 0, "int_pol0"},
		{ 0x2a, 0xfc2f,  0, "int_pol1"},
		{ 0x2b, 0x0003, 0, "int_pol2"},
		{ 0xff, 0,0, NULL}
};

/* *INDENT-ON* */
/*
 * for debugging
 */
int dummy_trace = 0;
/******************************************************************************
 * macros
 */

#define DUMMYVERBOSE if (lxDummy_verbose > 3)

#define XMEM2INT(i) (dev[thisdev].xmem[i]<<16|dev[thisdev].xmem[i+1]<<8|dev[thisdev].xmem[i+2])

#if (defined(WIN32) || defined(_X64))
void bzero(void *s, size_t n)
{
	memset(s, 0, n);
}

float roundf(float x)
{
	return (float)(int)x;
}
#endif

/* endian swap */
#define BE2LEW(x)   (( ( (x) << 8 ) | ( (x) & 0xFF00 ) >> 8 )&0xFFFF)
#define BE2LEDW( x)  (\
	       ((x) << 24) \
	     | (( (x) & 0x0000FF00) << 8 ) \
	     | (( (x) & 0x00FF0000) >> 8 ) \
	     | ((x) >> 24) \
	     )

/******************************************************************************
 *  - static default parameter array defs
 */
#define STATE_SIZE             9  // in words
#define STATE_SIZE_DRC            (2* 9)
static unsigned char stateInfo[STATE_SIZE*3];
static unsigned char stateInfoDrc[STATE_SIZE_DRC*3];
/* life data models */
/* ls model (0x86) */
static unsigned char lsmodel[423];
/* ls model (0xc1) */
static unsigned char lsmodelw[423];

//This can be used to always get a "real" dumpmodel=x for the dummy. It is created from a real dumpmodel=x
static unsigned char static_x_model[423] =
{
  0xff, 0xff, 0x53, 0x00, 0x00, 0x42, 0xff, 0xff, 0x93, 0x00, 0x00, 0x1b,
  0xff, 0xff, 0x1d, 0xff, 0xff, 0xef, 0xff, 0xff, 0x55, 0xff, 0xff, 0xca,
  0xff, 0xff, 0xee, 0x00, 0x00, 0x39, 0xff, 0xff, 0x49, 0x00, 0x00, 0x51,
  0xff, 0xff, 0x13, 0xff, 0xff, 0x8a, 0xff, 0xfe, 0xd5, 0xff, 0xff, 0x93,
  0xff, 0xff, 0x09, 0xff, 0xff, 0x00, 0xff, 0xff, 0x7e, 0xff, 0xfe, 0xf3,
  0xff, 0xff, 0x01, 0xff, 0xfe, 0xaa, 0xff, 0xff, 0x0c, 0xff, 0xfe, 0x55,
  0xff, 0xfe, 0x80, 0xff, 0xfe, 0x24, 0xff, 0xfe, 0xad, 0xff, 0xfe, 0x60,
  0xff, 0xfe, 0x36, 0xff, 0xfe, 0xd4, 0xff, 0xfd, 0xee, 0xff, 0xfe, 0x5d,
  0xff, 0xfd, 0x75, 0xff, 0xfe, 0x45, 0xff, 0xfd, 0x2e, 0xff, 0xfd, 0xd3,
  0xff, 0xfd, 0x4c, 0xff, 0xfe, 0x26, 0xff, 0xfc, 0xc9, 0xff, 0xfd, 0x79,
  0xff, 0xfd, 0x07, 0xff, 0xfd, 0x06, 0xff, 0xfd, 0x91, 0xff, 0xfc, 0x6f,
  0xff, 0xfd, 0x26, 0xff, 0xfc, 0x96, 0xff, 0xfd, 0x02, 0xff, 0xfd, 0x18,
  0xff, 0xfd, 0x23, 0xff, 0xfd, 0x1a, 0xff, 0xfc, 0x86, 0xff, 0xfc, 0x9b,
  0xff, 0xfc, 0xb1, 0xff, 0xfc, 0x30, 0xff, 0xfd, 0x35, 0xff, 0xfc, 0x02,
  0xff, 0xfd, 0x40, 0xff, 0xfc, 0xca, 0xff, 0xfd, 0x47, 0xff, 0xfc, 0xe9,
  0xff, 0xfd, 0x23, 0xff, 0xfc, 0x64, 0xff, 0xfc, 0x6a, 0xff, 0xfc, 0xda,
  0xff, 0xfc, 0x78, 0xff, 0xfd, 0xda, 0xff, 0xfd, 0x7c, 0xff, 0xfe, 0x3a,
  0xff, 0xfd, 0x87, 0xff, 0xfd, 0xd7, 0xff, 0xfb, 0x7c, 0xff, 0xfd, 0xd1,
  0xff, 0xfc, 0x1b, 0xff, 0xfe, 0xa8, 0xff, 0xfe, 0x6c, 0xff, 0xfe, 0x7c,
  0xff, 0xff, 0x3a, 0xff, 0xfd, 0x21, 0xff, 0xfd, 0xcc, 0xff, 0xfb, 0x1e,
  0xff, 0xfd, 0x0e, 0xff, 0xfa, 0xd4, 0xff, 0xff, 0xc3, 0xff, 0xfe, 0x64,
  0x00, 0x02, 0x25, 0xff, 0xff, 0x1a, 0xff, 0xfe, 0x54, 0xff, 0xfb, 0x0f,
  0xff, 0xf6, 0xa3, 0xff, 0xf9, 0xa7, 0xff, 0xf7, 0x3f, 0x00, 0x03, 0x9b,
  0xff, 0xfd, 0xce, 0x00, 0x0f, 0x23, 0xff, 0xfa, 0x16, 0x00, 0x08, 0x02,
  0xff, 0xe4, 0x4c, 0xff, 0xf7, 0x42, 0xff, 0xd6, 0xdb, 0x00, 0x04, 0x90,
  0xff, 0xf4, 0xa6, 0x00, 0x31, 0x89, 0x00, 0x13, 0x72, 0x00, 0x2f, 0xe2,
  0xff, 0xe4, 0x6b, 0xff, 0xd8, 0x43, 0xff, 0x93, 0x85, 0xff, 0xad, 0xef,
  0xff, 0xd4, 0xeb, 0x00, 0x39, 0x16, 0x00, 0xa0, 0x8c, 0x00, 0xb0, 0xdf,
  0x00, 0x9f, 0xe0, 0xff, 0xaa, 0x10, 0xff, 0xb9, 0x67, 0x06, 0xfa, 0x73,
  0xff, 0xb4, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


/******************************************************************************
 * HAL interface called via Scribo registered functions
 */
int lxDummyWriteRead(int fd, int NrOfWriteBytes, const uint8_t *WriteData,
		     int NrOfReadBytes, uint8_t *ReadData, uint32_t *pError)
{
	int length;
        /* Remove unreferenced parameter warning C4100 */
	(void)fd;

	*pError = NXP_I2C_Ok;
	/* there's always a write */
	length = i2cWrite(NrOfWriteBytes, WriteData);
	/* and maybe a read */
	if ((NrOfReadBytes != 0) && (length != 0)) {
		length = i2cRead(NrOfReadBytes, ReadData);
	}
	if (length == 0) {
		PRINT_ERROR("lxDummy slave error\n");
		*pError = NXP_I2C_NoAck;
	}

	return length;

/* if (WriteData[0] !=  (tfa98xxI2cSlave<<1)) { */
/* PRINT("wrong slave 0x%02x iso 0x%02x\n", WriteData[0]>>1, tfa98xxI2cSlave); */
/* //      return 0; */
/* } */
}

int lxDummyWrite(int fd, int size, unsigned char *buffer, uint32_t *pError)
{
	return lxDummyWriteRead(fd, size, buffer, 0, NULL, pError);
}

/*
 * set calibration done
 */
static void set_caldone()
{
	int xm_caldone=0;

	switch(dev2fam(&dev[thisdev])) {
	case 1:
		xm_caldone = TFA1_FW_XMEM_CALIBRATION_DONE;
		break;
	case 2:
		xm_caldone = TFA2_FW_XMEM_CALIBRATION_DONE;
		break;
	default:
		PRINT_ERROR("wrong family selector!");
		break;
	}

	dev[thisdev].xmem[xm_caldone * 3 +2] = 1;	/* calibration done */
	dev[thisdev].xmem[xm_caldone * 3 +1] = 0;	/* calibration done */
	dev[thisdev].xmem[xm_caldone * 3 ] = 0;	/* calibration done */
	//dev[thisdev].Reg[TFA1_STATUSREG] &= ~(TFA98XX_STATUS_FLAGS0_ACS);	/* clear coldstart */
	TFA_SET_BF(ACS,0);/* clear coldstart */
	dev[thisdev].Reg[0x83] = 0x0710;	/* dummy re0 value */
}
/*
 * increment count boot
 */

static void inc_countboot()
{
	int xm_count_boot = 0;
	unsigned int val;

	switch(dev2fam(&dev[thisdev])) {
	case 1:
		xm_count_boot = TFA1_FW_XMEM_COUNT_BOOT;
		break;
	case 2:
		xm_count_boot = TFA2_FW_XMEM_COUNT_BOOT;
		break;
	default:
		PRINT_ERROR("wrong family selector!");
		break;
	}

	// reverse bytes
	val =  (dev[thisdev].xmem[(xm_count_boot*3)]&0xff <<16 ) |
					(dev[thisdev].xmem[(xm_count_boot*3)+1]&0xff <<8)|
					(dev[thisdev].xmem[(xm_count_boot*3)+2]&0xff);
	val++;
	dev[thisdev].xmem[(xm_count_boot*3)] = (uint8_t)(val>>16);// ((val>>16)&0xff) | (val&0x00ff00) |((val&0xff)<<16);
	dev[thisdev].xmem[(xm_count_boot*3)+1] = (uint8_t)(val>>8);
        dev[thisdev].xmem[(xm_count_boot*3)+2] = (uint8_t)val;
}

void  lxDummyVerbose(int level) {
	lxDummy_verbose = level;
	DUMMYVERBOSE PRINT("dummy verbose on\n");
}

/*
 * the input lxDummyArg is from the -d<lxDummyArg> global
 * the file is the last argument  option
 */
int lxDummyInit(char *file)
{
	int type;
	char *lxDummyArg;		/* extra command line arg for test settings */
	dev[0].slave = 0x34;
	dev[1].slave = 0x35;
	dev[2].slave = 0x36; //default
	dev[3].slave = 0x37;

	if (file) {
		lxDummyArg = strchr(file, ','); /* the extra arg is after the comma */

		if (lxDummyArg) {
			lxDummyArg++; /* skip the comma */ //TODO parse commalist
			if (strcmp(lxDummyArg, "warm") == 0)
				dummy_warm = 1;
			else if (strcmp(lxDummyArg, "noclk") == 0)
				dummy_noclock = 1;
			else if (!setInputFile(lxDummyArg)) /* if filename use it */
			{
				lxDummyFailTest = atoi(lxDummyArg);
			}
		}
		DUMMYVERBOSE
			PRINT("arg: %s\n", lxDummyArg);
	} else {
		PRINT("%s: called with NULL arg\n", __FUNCTION__);
	}

	for(thisdev=0;thisdev<MAX_DUMMIES;thisdev++ ){
		dev[thisdev].xmem_patch_version = TFA_XMEM_PATCHVERSION; /* all except 97 */
		if (sscanf(file, "dummy%x", &type)) {
			switch (type) {
			case 0x90:
				dev[thisdev].type = tfa9890;
				dev[thisdev].config_length = 165;
				break;
			case 0x90b:
				//PRINT("warning: 91 is tfa9890b; 92 is tfa9891\n");
				dev[thisdev].type = tfa9890b;
				dev[thisdev].config_length = 165;
				break;
			case 0x92:
			case 0x91:
				dev[thisdev].type = tfa9891;
				dev[thisdev].config_length = 201;
				dev[thisdev].xmem_patch_version = TFA9891_XMEM_PATCHVERSION;
				break;
			case 0x87:
				dev[thisdev].type = tfa9887;
				dev[thisdev].config_length = 165;
				break;
			case 0x95:
				dev[thisdev].type = tfa9895;
				dev[thisdev].config_length = 201;
				break;
			case 0x87b:
				dev[thisdev].type = tfa9887b;
				dev[thisdev].config_length = 165;
				break;
			case 0x97:
				dev[thisdev].type = tfa9897;
				dev[thisdev].config_length = 201;
				dev[thisdev].xmem_patch_version = TFA9897_XMEM_PATCHVERSION;
				break;
			case 0x88:
			default:
				dev[thisdev].type = tfa9888;
				dev[thisdev].config_length = 165;
				break;
			}
		}

		resetRegs(dev[thisdev].type);
		setRomid(dev[thisdev].type);
	}
	thisdev = 2; //default
	PRINT("%s: running DUMMY i2c, type=%s\n", __FUNCTION__,
	       typeNames[dev[thisdev].type]);

	///* dev[thisdev].Reg[0x00] =  0x091d ;    /* statusreg mtp bit is only on after real pwron */ */
	/* lsmodel default */

	memcpy(dev[thisdev].lastSpeaker, lsmodel, sizeof(dev[thisdev].lastSpeaker));


	return (int)dev[thisdev].type;
}

static int lxDummyVersion(char *buffer, int fd)
{
	int len = (int)strlen(dummy_version_string);
        //PRINT("dummy version: %s\n", DUMMY_VERSION);
	/* Remove unreferenced parameter warning C4100 */
	(void)fd;
	strcpy(buffer,dummy_version_string);

	return len;
}

static int lxDummyGetPin(int fd, int pin)
{
	/* Remove unreferenced parameter warning C4100 */
	(void)fd;

        if ((unsigned int)pin > sizeof(dummy_pins)/sizeof(int)) {
		return -1;
	}

	return dummy_pins[pin];

}

static int lxDummySetPin(int fd, int pin, int value)
{
	/* Remove unreferenced parameter warning C4100 */
	(void)fd;

        if ((unsigned int)pin > sizeof(dummy_pins)/sizeof(int)) {
		return -1;
	}
	PRINT("dummy: pin[%d]=%d\n", pin, value);

	dummy_pins[pin] = value;

	return 0;
}

static int lxDummyClose(int fd)
{
        /* Remove unreferenced parameter warning C4100 */
	(void)fd;
	PRINT("Function close not implemented for target dummy.");
        return 0;
}

const struct nxp_i2c_device lxDummy_device = {
	lxDummyInit,
	lxDummyWriteRead,
	lxDummyVersion,
	lxDummySetPin,
	lxDummyGetPin,
        lxDummyClose
};

/******************************************************************************
 * I2C bus/slave handling code
 */
static int lxDummySetSlaveIdx(uint8_t slave) {
	int i;
	for(i=0;i<MAX_DUMMIES;i++){
		if (dev[i].slave == slave) {
			thisdev=i;
			return i;
		}
	}
	return -1;
}
/*
 * read I2C
 */
static int i2cRead(int length, uint8_t *data)
{
	int idx;

	if (lxDummySetSlaveIdx(data[0] / 2) <0) {
		PRINT_ERROR("dummy: slave read NoAck\n");
		return 0;
	}
	DUMMYVERBOSE PRINT("dummy: slave[%d]=0x%02x\n", thisdev, dev[thisdev].slave);

/* ln =length - 1;  // without slaveaddress */
	idx = 1;
	while (idx < length) {
		idx += tfaRead(&data[idx]);	/* write current and return bytes consumed */
	};

	return length;
}

/*
 * write i2c
 */
static int i2cWrite(int length, const uint8_t *data)
{
	//uint8_t slave;		/*  */
	int idx;

	if (lxDummySetSlaveIdx(data[0] / 2) <0) {
		PRINT_ERROR("dummy: slave read NoAck\n");
		return 0;
	}
	DUMMYVERBOSE PRINT("slave[%d]=0x%02x\n", thisdev, dev[thisdev].slave);

	dev[thisdev].currentreg = data[1];	/* start subaddress */

	/* without slaveaddress and regaddr */
	idx = 2;
	while (idx < length) {
		idx += tfaWrite(&data[idx]);	/* write current and return bytes consumed */
	};

	return length;

}

/******************************************************************************
 * TFA I2C registers read/write
 */
/* reg73 */
/* cf_err[7:0]     8   [7 ..0] 0           cf error flags */
/* reg73 cf_ack[7:0]     8   [15..8] 0           acknowledge of requests (8 channels")" */

#define 	CTL_CF_RST_DSP	(0)
#define 	CTL_CF_DMEM	(1)
#define 	CTL_CF_AIF		(3)
#define    CTL_CF_INT		(4)
#define    CTL_CF_REQ		(5)
#define    STAT_CF_ERR		(0)
#define    STAT_CF_ACK		(8)
#define    CF_PMEM			(0)
#define    CF_XMEM			(1)
#define    CF_YMEM			(2)
#define    CF_IOMEM			(3)

/*
 * in the local register cache the values are stored as little endian,
 *  all processing is done in natural little endianess
 * The i2c data is big endian
 */
/*
 * i2c regs reset to default 9887
 */
static void resetRegs9887(void)
{
	dev[thisdev].Reg[0x00] = 0x081d;	/* statusreg */
	dev[thisdev].Reg[0x03] = 0x0012;	/* revisionnumber */
	dev[thisdev].Reg[0x04] = 0x888b;	/* i2sreg */
	dev[thisdev].Reg[0x05] = 0x13aa;	/* bat_prot */
	dev[thisdev].Reg[0x06] = 0x001f;	/* audio_ctr */
	dev[thisdev].Reg[0x07] = 0x0fe6;	/* dcdcboost */
	dev[thisdev].Reg[0x08] = 0x0800;	/* spkr_calibration */
	dev[thisdev].Reg[0x09] = 0x041d;	/* sys_ctrl */
	dev[thisdev].Reg[0x0a] = 0x3ec3;	/* i2s_sel_reg */
	dev[thisdev].Reg[0x40] = 0x0000;	/* hide_unhide_key */
	dev[thisdev].Reg[0x41] = 0x0000;	/* pwm_control */
	dev[thisdev].Reg[0x4c] = 0x0000;	/* abisttest */
	dev[thisdev].Reg[0x62] = 0x0000;
	dev[thisdev].Reg[0x70] = 0x0000;	/* cf_controls */
	dev[thisdev].Reg[0x71] = 0x0000;	/* cf_mad */
	dev[thisdev].Reg[0x72] = 0x0000;	/* cf_mem */
	dev[thisdev].Reg[0x73] = 0x0000;	/* cf_status */
	dev[thisdev].Reg[0x80] = 0x0000;	/* mpt */
	dev[thisdev].Reg[0x83] = 0x0000;	/* mpt_re0 */
}

/*
 *  i2c regs reset to default 9897
 */
static void resetRegs9897(void)
{
        /* i used in unsigned int comparison in for loops. */
	unsigned int i;

	dev[thisdev].Reg[0x00] = 0x081d;	/* statusreg */
	dev[thisdev].Reg[0x03] = 0x0b97;	/* revisionnumber */
	dev[thisdev].Reg[0x04] = 0x888b;	/* i2sreg */
	dev[thisdev].Reg[0x05] = 0x13aa;	/* bat_prot */
	dev[thisdev].Reg[0x06] = 0x001f;	/* audio_ctr */
	dev[thisdev].Reg[0x07] = 0x0fe6;	/* dcdcboost */
	dev[thisdev].Reg[0x08] = 0x0800;	/* spkr_calibration */
	dev[thisdev].Reg[0x09] = 0x041d;	/* sys_ctrl */
	dev[thisdev].Reg[0x0a] = 0x3ec3;	/* i2s_sel_reg */
	dev[thisdev].Reg[0x40] = 0x0000;	/* hide_unhide_key */
	dev[thisdev].Reg[0x41] = 0x0000;	/* pwm_control */
	dev[thisdev].Reg[0x4c] = 0x0000;	/* abisttest */
	dev[thisdev].Reg[0x62] = 0x0000;
	dev[thisdev].Reg[0x70] = 0x0000;	/* cf_controls */
	dev[thisdev].Reg[0x71] = 0x0000;	/* cf_mad */
	dev[thisdev].Reg[0x72] = 0x0000;	/* cf_mem */
	dev[thisdev].Reg[0x73] = 0x0000;	/* cf_status */
	dev[thisdev].Reg[0x80] = 0x0000;
	dev[thisdev].Reg[0x83] = 0x0000;	/* mtp_re0 */
	for(i=0;i<(sizeof(tdm_regdefs_mx1)/sizeof(regdef_t))-1;i++) {//TFA98XX_TDM_CONFIG_REG0=0x10
		dev[thisdev].Reg[i+FAM_TFA98XX_TDM_REGS] = tdm_regdefs_mx1[i].pwronDefault;
	}

	for(i=0;i<(sizeof(int_regdefs_mx1)/sizeof(regdef_t))-1;i++) {
		dev[thisdev].Reg[int_regdefs_mx1[i].offset] = int_regdefs_mx1[i].pwronDefault;
	}

}
/*
 *  i2c regs reset to default 9890
 */
static void resetRegs9890(void)
{
	dev[thisdev].Reg[0x00] = 0x0a5d;	/* statusreg */
	dev[thisdev].Reg[0x03] = 0x0080;	/* revisionnumber */
	dev[thisdev].Reg[0x04] = 0x888b;	/* i2sreg */
	dev[thisdev].Reg[0x05] = 0x93a2;	/* bat_prot */
	dev[thisdev].Reg[0x06] = 0x001f;	/* audio_ctr */
	dev[thisdev].Reg[0x07] = 0x8fe6;	/* dcdcboost */
	dev[thisdev].Reg[0x08] = 0x3800;	/* spkr_calibration */
	dev[thisdev].Reg[0x09] = 0x825d;	/* sys_ctrl */
	dev[thisdev].Reg[0x0a] = 0x3ec3;	/* i2s_sel_reg */
	dev[thisdev].Reg[0x40] = 0x0000;	/* hide_unhide_key */
	dev[thisdev].Reg[0x41] = 0x0308;	/* pwm_control */
	dev[thisdev].Reg[0x4c] = 0x0000;	/* abisttest */
	dev[thisdev].Reg[0x62] = 0x0000;	/* mtp_copy */
	dev[thisdev].Reg[0x70] = 0x0000;	/* cf_controls */
	dev[thisdev].Reg[0x71] = 0x0000;	/* cf_mad */
	dev[thisdev].Reg[0x72] = 0x0000;	/* cf_mem */
	dev[thisdev].Reg[0x73] = 0x0000;	/* cf_status */
	dev[thisdev].Reg[0x80] = 0x0000;	/* mtp */
	dev[thisdev].Reg[0x83] = 0x0000;	/* mtp_re0 */
	dev[thisdev].Reg[0x84] = 0x1234;	/* MTP for '90 startup system stable detection */
}

/*
 *  i2c regs reset to default 9890B/9891
 */
static void resetRegs9890b(void)
{
	dev[thisdev].Reg[0x00] = 0x0a5d;	/* statusreg */
	dev[thisdev].Reg[0x03] = 0x0091;	/* revisionnumber */
	dev[thisdev].Reg[0x04] = 0x888b;	/* i2sreg */
	dev[thisdev].Reg[0x05] = 0x93a2;	/* bat_prot */
	dev[thisdev].Reg[0x06] = 0x001f;	/* audio_ctr */
	dev[thisdev].Reg[0x07] = 0x8fe6;	/* dcdcboost */
	dev[thisdev].Reg[0x08] = 0x3800;	/* spkr_calibration */
	dev[thisdev].Reg[0x09] = 0x825d;	/* sys_ctrl */
	dev[thisdev].Reg[0x0a] = 0x3ec3;	/* i2s_sel_reg */
	dev[thisdev].Reg[0x40] = 0x0000;	/* hide_unhide_key */
	dev[thisdev].Reg[0x41] = 0x0308;	/* pwm_control */
	dev[thisdev].Reg[0x4c] = 0x0000;	/* abisttest */
	dev[thisdev].Reg[0x62] = 0x0000;	/* mtp_copy */
	dev[thisdev].Reg[0x70] = 0x0000;	/* cf_controls */
	dev[thisdev].Reg[0x71] = 0x0000;	/* cf_mad */
	dev[thisdev].Reg[0x72] = 0x0000;	/* cf_mem */
	dev[thisdev].Reg[0x73] = 0x0000;	/* cf_status */
	dev[thisdev].Reg[0x80] = 0x0000;	/* mtp */
	dev[thisdev].Reg[0x83] = 0x0000;	/* mtp_re0 */
	dev[thisdev].Reg[0x84] = 0x1234;	/* MTP for '90 startup system stable detection */
}
/*
 *  i2c regs reset to default 9891
 */
static void resetRegs9891(void)
{
	dev[thisdev].Reg[0x00] = 0x0a5d;	/* statusreg */
	dev[thisdev].Reg[0x03] = 0x0092;	/* revisionnumber */
	dev[thisdev].Reg[0x04] = 0x888b;	/* i2sreg */
	dev[thisdev].Reg[0x05] = 0x93a2;	/* bat_prot */
	dev[thisdev].Reg[0x06] = 0x000f;	/* audio_ctr */
	dev[thisdev].Reg[0x07] = 0x8fff;	/* dcdcboost */
	dev[thisdev].Reg[0x08] = 0x3800;	/* spkr_calibration */
	dev[thisdev].Reg[0x09] = 0x825d;	/* sys_ctrl */
	dev[thisdev].Reg[0x0a] = 0x3ec3;	/* i2s_sel_reg */
	dev[thisdev].Reg[0x0f] = 0x40;	/* irq_reg */

}
/*
 * i2c regs reset to default 9895
 */
static void resetRegs9895(void)
{
	dev[thisdev].Reg[0x00] = 0x081d;	/* statusreg */
	dev[thisdev].Reg[0x01] = 0x3ff;	/* battV clock off */
	dev[thisdev].Reg[0x02] = 0x100;	/* ictemp clock off */
	dev[thisdev].Reg[0x03] = 0x0012;	/* revisionnumber */
	dev[thisdev].Reg[0x04] = 0x888b;	/* i2sreg */
	dev[thisdev].Reg[0x05] = 0x13aa;	/* bat_prot */
	dev[thisdev].Reg[0x06] = 0x001f;	/* audio_ctr */
	dev[thisdev].Reg[0x07] = 0x0fe6;	/* dcdcboost */
	dev[thisdev].Reg[0x08] = 0x0c00;	/* spkr_calibration */
	dev[thisdev].Reg[0x09] = 0x041d;	/* sys_ctrl */
	dev[thisdev].Reg[0x0a] = 0x3ec3;	/* i2s_sel_reg */
	dev[thisdev].Reg[0x40] = 0x0000;	/* hide_unhide_key */
	dev[thisdev].Reg[0x41] = 0x0300;	/* pwm_control */
	dev[thisdev].Reg[0x4c] = 0x0000;	/* abisttest */
	dev[thisdev].Reg[0x62] = 0x5be1;
	dev[thisdev].Reg[0x70] = 0;		/* cf_controls */
	dev[thisdev].Reg[0x71] = 0;		/* cf_mad */
	dev[thisdev].Reg[0x72] = 0x0000;	/* cf_mem */
	dev[thisdev].Reg[0x73] = 0x0000;	/* cf_status */
	dev[thisdev].Reg[0x80] = 0x0000;
	dev[thisdev].Reg[0x83] = 0x0000;	/* mtp_re0 */
}


/*
 * i2c regs reset 88N1A12 to default
 */
static void resetRegs9888(void)
{
	/* i used in unsigned int comparison in for loops. */
	unsigned int i;

	if (dummy_warm) { //TODO reflect warm state if needed
		dev[thisdev].Reg[TFA98XX_SYS_CONTROL0] = TFA98XX_SYS_CONTROL0_POR;
		dev[thisdev].Reg[TFA98XX_SYS_CONTROL1] = TFA98XX_SYS_CONTROL1_POR;
		dev[thisdev].Reg[TFA98XX_SYS_CONTROL2] = TFA98XX_SYS_CONTROL2_POR;
		dev[thisdev].Reg[TFA98XX_DEVICE_REVISION]=  TFA98XX_DEVICE_REVISION_POR;
		dev[thisdev].Reg[TFA98XX_STATUS_FLAGS0]=  0x21c; //N1A12
		//dev[thisdev].Reg[TFA98XX_STATUS_POLARITY_REG1] = 0x27a0;
	} else {
		for(i=0;i<(sizeof(regdefs)/sizeof(regdef_t))-1;i++) {//TFA98XX_TDM_CONFIG_REG0=0x10
			dev[thisdev].Reg[regdefs[i].offset] = regdefs[i].pwronDefault;
		}
	}
	if (dummy_noclock)
		TFA_SET_BF(NOCLK,1);
}

/*
 * i2c regs reset to default
 */
static void resetRegs(dummyType_t type)
{
	uint16_t acs=0;
	/* this is a I2CR ot POR */

	if (dummy_warm) {
		/* preserve acs state */
		acs = (uint16_t)TFA_GET_BF(ACS);
	}
	TFA_SET_BF(BATS,0x3ff);//when clock is off
	switch (type) {
	case tfa9887:
		resetRegs9887();
		break;
	case tfa9890:
		resetRegs9890();
		break;
	case tfa9890b:
		resetRegs9890b();
		break;
	case tfa9891:
		resetRegs9891();
		break;
	case tfa9895:
	case tfa9887b:
		resetRegs9895();
		break;
	case tfa9897:
		resetRegs9897();
		break;
	case tfa9888:
		resetRegs9888();
		break;
	default:
		PRINT("dummy: %s, unknown type %d\n", __FUNCTION__, type);
		break;
	}
	if (dummy_warm) {
		TFA_SET_BF(ACS,acs);
		if(IS_FAM==1)
			dev[thisdev].Reg[0] = 0x805f;
		//dev[thisdev].Reg[TFA98XX_STATUS_POLARITY_REG1] = 0x27a0;
		else
			dev[thisdev].Reg[0x10] = 0x743e;
	}
}
/*
 * return the regname
 */
static const char *getRegname(int reg)
{
	int i, nr_of_elements = sizeof(regdefs)/sizeof(regdefs[0]);

	for(i = 0; i < nr_of_elements; i++) {
		if(reg == regdefs[i].offset)
			return regdefs[i].name;
	}
	return "unknown";
}

/*
 * emulation of tfa9887 register read
 */
static int tfaRead(uint8_t *data) {
	int reglen = 2; /* default */
	uint8_t reg = dev[thisdev].currentreg;
	uint16_t regval = 0xdead;
	static short temperature = 0;

	if (IS_FAM == 2) {
		switch (reg) {
		case TFA98XX_CF_STATUS /*0x73 */:
			regval = dev[thisdev].Reg[reg]; /* just return */
			reglen = 2;
			dev[thisdev].currentreg++; /* autoinc */
			break;
		case TFA98XX_CF_MEM /*0x72 */:
			if (isClockOn(thisdev))
				reglen = lxDummyMemr(
						(dev[thisdev].Reg[TFA98XX_CF_CONTROLS] >> CTL_CF_DMEM)
								& 0x03, data);
			break;
		case TFA98XX_CF_MAD /*0x71 */:
			regval = dev[thisdev].Reg[reg]; /* just return */
			if (lxDummyFailTest == 2)
				regval = 0xdead; /* fail test */
			reglen = 2;
			dev[thisdev].currentreg++; /* autoinc */
			break;
		default:
			regval = dev[thisdev].Reg[reg]; /* just return anyway */
			dev[thisdev].currentreg++; /* autoinc */
			break;
		}
		if (reg != 0x92 /* TFA98XX_CF_MEM*/) {
			DUMMYVERBOSE
				PRINT("0x%02x:0x%04x (%s)\n", reg, regval, getRegname(reg));

			*(uint16_t *) (data) = BE2LEW(regval); /* return in proper endian */
		}

	} else { //max1
		switch (reg) {
		default:
			regval = dev[thisdev].Reg[reg]; /* just return */
			reglen = 2;
			dev[thisdev].currentreg++; /* autoinc */
			break;
		case 0x84: /* MTP for '90 startup system stable detection */
			regval = dev[thisdev].Reg[reg]; /* just return */
			reglen = 2;
			dev[thisdev].currentreg++; /* autoinc */
			break;
		case 0x86: /* MTP */
#define FEATURE1_TCOEF              0x100	/* bit8 set means tCoefA expected */
#define FEATURE1_DRC                0x200	/* bit9 NOT set means DRC expected */
			regval = dev[thisdev].Reg[reg]; /* just return */
			reglen = 2;
			dev[thisdev].currentreg++; /* autoinc */
			break;
		case 2: //TFA98XX_TEMPERATURE /*0x02 */:
			if (temperature++ > 170)
				temperature = -40;
			dev[thisdev].Reg[TFA98XX_TEMPERATURE] = temperature;
			regval = dev[thisdev].Reg[reg]; /* just return */
			reglen = 2;
			dev[thisdev].currentreg++; /* autoinc */
			break;
		case 1: // TFA98XX_BATTERYVOLTAGE /*0x01 */:
			if (lxDummyFailTest == 10)
				dev[thisdev].Reg[1] = (uint16_t) (1 / (5.5 / 1024)); /* 1V */
			regval = dev[thisdev].Reg[reg]; /* just return */
			reglen = 2;
			dev[thisdev].currentreg++; /* autoinc */
			break;
		case 0x73: //TFA98XX_CF_STATUS /*0x73 */:
			regval = dev[thisdev].Reg[reg]; /* just return */
			reglen = 2;
			dev[thisdev].currentreg++; /* autoinc */
			break;
		case 0x72: //TFA98XX_CF_MEM /*0x72 */:
			if (isClockOn(thisdev))
				reglen = lxDummyMemr(
						(dev[thisdev].Reg[0x70] >> CTL_CF_DMEM) & 0x03, data);
			else
				PRINT("dummy: ! DSP mem read without clock!\n");
			break;
		case 0x71: //TFA98XX_CF_MAD /*0x71 */:
			regval = dev[thisdev].Reg[reg]; /* just return */
			if (lxDummyFailTest == 2)
				regval = 0xdead; /* fail test */
			reglen = 2;
			dev[thisdev].currentreg++; /* autoinc */
			break;

		}

		if (reg != 0x72) {
			DUMMYVERBOSE
				PRINT("0x%02x:0x%04x (%s)\n", reg, regval, getRegname(reg));

			*(uint16_t *) (data) = BE2LEW(regval); /* return in proper endian */
		}

	}

	return reglen;
}
/*
 * set dsp firmware ack
 */
static void cfAck(int thisdev, enum tfa_fw_event evt) {
	uint16_t oldack=dev[thisdev].Reg[TFA98XX_CF_STATUS];

	FUNC_TRACE("ack\n");
	dev[thisdev].Reg[FAM_TFA98XX_CF_STATUS] |= (1 << (evt + STAT_CF_ACK));

	if ( oldack != dev[thisdev].Reg[FAM_TFA98XX_CF_STATUS])
		updateInterrupt();
}

static void cfCountboot(int thisdev) {
	if (isClockOn(thisdev)) {
		inc_countboot();		// count_boot (lsb)
		cfAck(thisdev, tfa_fw_reset_start);
	}
}
/*
 * cf control reg (0x70)
 */
static int i2cCfControlReg(uint16_t val)
{
	unsigned char module_id, param_id;
	uint16_t xor, clearack, negedge, newval, oldval = dev[thisdev].Reg[TFA98XX_CF_CONTROLS];
	int ack=1;
	newval = val;

	/* REQ/ACK bits:
	 *  if REQ was high and goes low then clear ACK
	 * */
	xor = oldval^newval;
	if ( xor & TFA_GET_BF(REQ) ) {
		negedge = (oldval & ~newval);
		clearack = negedge;
		dev[thisdev].Reg[FAM_TFA98XX_CF_STATUS] &= ~(clearack & TFA98XX_CF_CONTROLS_REQ_MSK);
	}
	// reset transition 1->0 increment count_boot
	if ( dev[thisdev].Reg[FAM_TFA98XX_CF_CONTROLS] & TFA98XX_CF_CONTROLS_RST_MSK ) {// reset is on
			if ( (val  & TFA98XX_CF_CONTROLS_RST_MSK) ==0) { //clear it now
				cfCountboot(thisdev);
				FUNC_TRACE("DSP reset release\n");
			}
			else return val; // stay in reset
}
	if ( (val & TFA98XX_CF_CONTROLS_CFINT_MSK) &&
		 (val & TFA98XX_CF_CONTROLS_REQ_MSK) )	/* if cfirq and msg req*/
	{
		FUNC_TRACE("req: ");
		module_id = dev[thisdev].xmem[4];
		param_id = dev[thisdev].xmem[5];
		if ((module_id == MODULE_SPEAKERBOOST+0x80) ) {	/* MODULE_SPEAKERBOOST */
			FUNC_TRACE("MODULE_SPEAKERBOOST\n");
			setgetDspParamsSpeakerboost(param_id);
		} else if (module_id == MODULE_FRAMEWORK+0x80) {	/* MODULE_FRAMEWORK */
			FUNC_TRACE("MODULE_FRAMEWORK\n");
			setDspParamsFrameWork(param_id);
		} else if (module_id == MODULE_BIQUADFILTERBANK+0x80) { // EQ
			FUNC_TRACE("MODULE_BIQUADFILTERBANK\n");
			setDspParamsEq(param_id);
		} else if (module_id == MODULE_SETRE+0x80) { // set Re0
			FUNC_TRACE("MODULE_SETRE\n");
			setDspParamsRe0(param_id);
		} else  {
			PRINT("? UNKNOWN MODULE ! module_id: 0x%02x param_id:0x%02x\n",module_id, param_id);
			ack=0;
			_exit(Tfa98xx_Error_Bad_Parameter);
		}
		if(ack)
			cfAck(thisdev, tfa_fw_i2c_cmd_ack);
	}

	return val;
}
static int updateInterrupt(void) {
return 0;
}

/*
 * i2c interrupt registers
 *   abs addr: TFA98XX_INTERRUPT_OUT_REG1 + idx
 */
static uint16_t i2cInterruptReg(int idx, uint16_t wordvalue)
{
	DUMMYVERBOSE PRINT("0x%02x,intreg[%d] < 0x%04x\n", TFA98XX_INTERRUPT_OUT_REG1+idx, idx, wordvalue);
	dev[thisdev].Reg[TFA98XX_INTERRUPT_OUT_REG1+idx] = wordvalue;

	return wordvalue;
}
/*
 * i2c  control reg
 */
static uint16_t tfa1_i2cControlReg(uint16_t wordvalue)
{
	uint16_t isreset=0;

	if ((wordvalue & TFA98XX_SYS_CTRL_I2CR)) {
		FUNC_TRACE("I2CR reset\n");
		resetRegs(dev[thisdev].type);
		wordvalue = dev[thisdev].Reg[TFA98XX_SYS_CTRL]; //reset value
		isreset = 1;
	}
	/* if PLL input changed */
	if ((wordvalue ^ dev[thisdev].Reg[TFA98XX_SYS_CTRL])
			& TFA98XX_SYS_CTRL_IPLL_MSK) {
		int ipll = (wordvalue & TFA98XX_SYS_CTRL_IPLL_MSK)
				>> TFA98XX_SYS_CTRL_IPLL_POS;
		FUNC_TRACE("PLL in=%d (%s)\n", ipll, ipll ? "fs" : "bck");
	}

	if ((wordvalue & (1 << 0)) && !(wordvalue & (1 << 13))) { /* powerdown=1, i2s input 1 */
		FUNC_TRACE("power off\n");
		dev[thisdev].Reg[TFA1_STATUSREG] &= ~(TFA1_STATUSREG_PLLS
				| TFA1_STATUSREG_CLKS);
		TFA_SET_BF(BATS, 0);
	} else {
		FUNC_TRACE("power on\n");
		dev[thisdev].Reg[TFA1_STATUSREG] |= (TFA1_STATUSREG_PLLS
				| TFA1_STATUSREG_CLKS);
		dev[thisdev].Reg[TFA1_STATUSREG] |= (TFA1_STATUSREG_AREFS);
		//dev[thisdev].Reg[TFA98XX_BATTERYVOLTAGE] = ; //=5.08V R*5.5/1024
		TFA_SET_BF(BATS, 0x3b2); //=5.08V R*5.5/1024
	}
	if (wordvalue & TFA98XX_SYS_CTRL_SBSL) { /* configured */
		FUNC_TRACE("configured\n");
		set_caldone();
	}
	if ((wordvalue & TFA98XX_SYS_CTRL_AMPE)
			&& !(wordvalue & TFA98XX_SYS_CTRL_CFE)) { /* assume bypass if AMPE and not CFE */
		FUNC_TRACE("CF by-pass\n");
		dev[thisdev].Reg[0x73] = 0x00ff; // else irq test wil fail
	}

	/* reset with ACS set */
	if (TFA_GET_BF(ACS)) {
		FUNC_TRACE("reset with ACS set\n");
		dev[thisdev].xmem[231 * 3] = 0; /* clear calibration done */
		dev[thisdev].xmem[231 * 3 + 1] = 0; /* clear calibration done */
		dev[thisdev].xmem[231 * 3 + 2] = 0; /* clear calibration done */
	}

	return isreset;
}

/*
 * i2c  control reg
 */
static uint16_t tfa2_i2cControlReg(uint16_t wordvalue)
{
	uint16_t isreset=0;

	if ( (wordvalue & TFA98XX_SYS_CONTROL0_I2CR_MSK) ) {
		FUNC_TRACE("I2CR reset\n");
		resetRegs(dev[thisdev].type);
		wordvalue = dev[thisdev].Reg[TFA98XX_SYS_CONTROL0]; //reset value
		isreset=1;
	}
	if (wordvalue & TFA98XX_SYS_CONTROL0_PWDN_MSK)
	{
		//show only if changed
		if( (dev[thisdev].Reg[TFA98XX_SYS_CONTROL0] &  TFA98XX_SYS_CONTROL0_PWDN_MSK) == 0)
			FUNC_TRACE("PLL power off\n");
		dev[thisdev].Reg[TFA98XX_STATUS_FLAGS0] &=
		    ~(TFA98XX_STATUS_FLAGS0_PLLS | TFA98XX_STATUS_FLAGS0_CLKS);
		dev[thisdev].Reg[TFA98XX_STATUS_FLAGS0] |= (TFA98XX_STATUS_FLAGS0_AREFS);
		dev[thisdev].Reg[TFA98XX_BATTERY_VOLTAGE] = 0x3b2; //=5.08V R*5.5/1024
	} else if ( (dev[thisdev].Reg[TFA98XX_SYS_CONTROL0] &  TFA98XX_SYS_CONTROL0_PWDN_MSK) ) {//show only if changed
			FUNC_TRACE("PLL power on\n");
			dev[thisdev].Reg[TFA98XX_STATUS_FLAGS0] |=
			    (TFA98XX_STATUS_FLAGS0_PLLS | TFA98XX_STATUS_FLAGS0_CLKS);
	}
	if (wordvalue & TFA98XX_SYS_CONTROL0_SBSL_MSK) {	/* configured */
		if((dev[thisdev].Reg[TFA98XX_SYS_CONTROL0] &  TFA98XX_SYS_CONTROL0_SBSL_MSK) == 0 )
			FUNC_TRACE("configured\n");
		set_caldone();
	}
	if ( (wordvalue & TFA98XX_SYS_CONTROL0_AMPC) && !(wordvalue & TFA98XX_SYS_CONTROL0_CFE)) {	/* assume bypass if AMPE and not CFE */
		FUNC_TRACE("CF by-pass\n");
		//dev[thisdev].Reg[0x73] = 0x00ff;// else irq test wil fail
	}

	return isreset;
}
static uint16_t i2cControlReg(uint16_t wordvalue) {
	if (IS_FAM==2)
		return tfa2_i2cControlReg(wordvalue);
	else if (IS_FAM==1)
		return tfa1_i2cControlReg(wordvalue);

	PRINT_ERROR("family type is wrong:%d\n", IS_FAM);
	return 0;
}
/*
 * emulation of tfa9887 register write
 *
 *  write current register , autoincrement  and return bytes consumed
 *
 */
static int tfaWrite(const uint8_t *data) {
	int reglen = 2; /* default */
	uint8_t reg = dev[thisdev].currentreg;
	uint16_t wordvalue, oldvalue, newvalue;

	oldvalue = dev[thisdev].Reg[reg];
	newvalue = wordvalue = data[0] << 8 | data[1];

	if (IS_FAM == 2) {
		switch (reg) {
		case TFA98XX_SYS_CONTROL0:
			if (i2cControlReg(wordvalue)) {
				// i2c reset, fix printing wrong trace
				dev[thisdev].Reg[TFA98XX_SYS_CONTROL0] = wordvalue = newvalue =
						oldvalue;
				newvalue = wordvalue | TFA98XX_SYS_CONTROL0_I2CR_MSK; /* set i2cr  bit for proper trace */
			} else
				dev[thisdev].Reg[TFA98XX_SYS_CONTROL0] = wordvalue;
			break;
		case TFA98XX_CF_MAD /* */:
			dev[thisdev].Reg[reg] = wordvalue;
			dev[thisdev].memIdx = wordvalue * 3; /* set glbl mem idx */
			break;
		case TFA98XX_CF_CONTROLS /* */:
			dev[thisdev].Reg[reg] = (uint16_t) i2cCfControlReg(wordvalue);
			break;
		case TFA98XX_CF_MEM /* */:
			reglen = lxDummyMemw(
					(dev[thisdev].Reg[TFA98XX_CF_CONTROLS] >> CTL_CF_DMEM)
							& 0x03, data);
			break;
		default:
			if (reg >> 4 == 2) {
				wordvalue = i2cInterruptReg(reg & 0x0f, wordvalue);
			} else {
				DUMMYVERBOSE
					PRINT("dummy: undefined wr register: 0x%02x\n", reg);
			}
			dev[thisdev].Reg[reg] = wordvalue;
			break;
		}
		/* all but cf_mem autoinc and 2 bytes */
		if (reg != 0x92 /*TFA98XX_CF_MEM*/) {
			dev[thisdev].currentreg++; /* autoinc */
			reglen = 2;

			DUMMYVERBOSE
				PRINT("0x%02x<0x%04x (%s)\n", reg, wordvalue, getRegname(reg));
	#ifdef TFA_DUMMY_BFFIELD_TRACE
			lxDummy_trace_bitfields(reg, oldvalue, newvalue);
	#endif
		} else {
			; //TODO add dots with proper newline
//			if (lxDummy_verbose & 2)
//					PRINT(".");
		}
	} else { //max1
		switch (reg) {

		case 0x06: //TFA1_AUDIO_CTR /*0x06 */:
			/* if volume rate changed */
			if ((wordvalue ^ dev[thisdev].Reg[reg]) & TFA1_AUDIO_CTR_VOL_MSK) {
				int vol = (wordvalue & TFA1_AUDIO_CTR_VOL_MSK)
						>> TFA1_AUDIO_CTR_VOL_POS;
				FUNC_TRACE("volume=%d %s\n", vol,
						vol == 0xff ? "(softmute)" : "");
				if (vol == 0xff)
					cfAck(thisdev, tfa_fw_soft_mute_ready);
			}
			dev[thisdev].Reg[reg] = wordvalue;
			break;
		case 4 /*I2SREG 0x04 or Audio control reg for 97 */:
			/* if sample rate changed */
			if ((wordvalue ^ dev[thisdev].Reg[TFA98XX_I2SREG])
					& TFA1_I2SREG_I2SSR_MSK) {
				int fs = (wordvalue & TFA1_I2SREG_I2SSR_MSK)
						>> TFA1_I2SREG_I2SSR_POS;
				FUNC_TRACE("sample rate=%d (%s)\n", fs, fsname[8 - fs]);
			}
			dev[thisdev].Reg[reg] = wordvalue;
			break;
		case 0x08: //TFA98XX_SPKR_CALIBRATION /*0x08 PVP bit */:
			if (dev[thisdev].type == tfa9887)
				dev[thisdev].Reg[reg] = wordvalue; /* PVP bit is RW */
			else
				dev[thisdev].Reg[reg] = wordvalue | 0x0400; /* PVP bit is always 1 */
			break;
		case 0x71: //TFA98XX_CF_MAD /*0x71 */:
			dev[thisdev].Reg[reg] = wordvalue;
			dev[thisdev].memIdx = wordvalue * 3; /* set glbl mem idx */
			break;
		case 0x70: //TFA98XX_CF_CONTROLS /*0x70 */:
			dev[thisdev].Reg[reg] = (uint16_t) i2cCfControlReg(wordvalue);
			break;
		case 0x72: //TFA98XX_CF_MEM /*0x72 */:
			reglen = lxDummyMemw((dev[thisdev].Reg[0x70] >> CTL_CF_DMEM) & 0x03,
					data);
			break;
		case 0x09: //TFA98XX_SYS_CTRL /*0x09 */:
			if (i2cControlReg(wordvalue)) {
				// i2c reset, fix printing wrong trace
				newvalue = wordvalue | TFA98XX_SYS_CONTROL0_I2CR_MSK; /* set i2cr  bit for proper trace */
			} else
				dev[thisdev].Reg[reg] = wordvalue;
			break;
		default:
			if (reg >> 4 == 2) {
				wordvalue = i2cInterruptReg(reg & 0x0f, wordvalue);
			} else {
				DUMMYVERBOSE
					PRINT("dummy: undefined wr register: 0x%02x\n", reg);
			}
			dev[thisdev].Reg[reg] = wordvalue;
			break;
		}
		/* all but cf_mem autoinc and 2 bytes */
		if (reg != 0x72) {
			dev[thisdev].currentreg++; /* autoinc */
			reglen = 2;

			DUMMYVERBOSE
				PRINT("0x%02x<0x%04x (%s)\n", reg, wordvalue, getRegname(reg));
	#ifdef TFA_DUMMY_BFFIELD_TRACE
			lxDummy_trace_bitfields(reg, oldvalue, newvalue);
	#endif
		} else {
			; //TODO add dots with proper newline
//			if (lxDummy_verbose & 2)
//					PRINT(".");
		}
	}


	updateInterrupt();
	return reglen;
}

/******************************************************************************
 * CoolFlux subsystem, mainly dev[thisdev].xmem, read and write
 */

/*
 * set value returned for the patch load romid check
 */
static void setRomid(dummyType_t type)
{
	if (dummy_warm) {
		set_caldone();
	}
	switch (type) {
	case tfa9888:
		dev[thisdev].xmem[0x200c * 3] = 0x5c;	/* N1A12 0x200c=0x5c551e*/
		dev[thisdev].xmem[0x200c * 3 + 1] = 0x55;
		dev[thisdev].xmem[0x200c * 3 + 2] = 0x1e;
		break;
	case tfa9887:
		dev[thisdev].xmem[0x2210 * 3] = 0x73;	/* N1D2 */
		dev[thisdev].xmem[0x2210 * 3 + 1] = 0x33;
		dev[thisdev].xmem[0x2210 * 3 + 2] = 0x33;
		break;
	case tfa9890:
	case tfa9890b:
		dev[thisdev].xmem[0x20c6 * 3] = 0x00;	/* 90 N1C3 */
		dev[thisdev].xmem[0x20c6 * 3 + 1] = 0x00;
		dev[thisdev].xmem[0x20c6 * 3 + 2] = 0x31; // C2=0x31;
		break;
	case tfa9891:
		dev[thisdev].xmem[0x20f0 * 3] = 0x00;	/* 91 */
		dev[thisdev].xmem[0x20f0 * 3 + 1] = 0x00;
		dev[thisdev].xmem[0x20f0 * 3 + 2] = 0x37; //
		break;
	case tfa9895:
	case tfa9887b:
		dev[thisdev].xmem[0x21b4 * 3] = 0x00;	/* 95 */
		dev[thisdev].xmem[0x21b4 * 3 + 1] = 0x77;
		dev[thisdev].xmem[0x21b4 * 3 + 2] = 0x9a;
		break;
	case tfa9897:
		/* N1B: 0x22b0=0x000032*/
		dev[thisdev].xmem[0x22B0 * 3] = 0x00;	/* 97N1B */
		dev[thisdev].xmem[0x22B0 * 3 + 1] = 0x00;
		dev[thisdev].xmem[0x22B0 * 3 + 2] = 0x32;
		//n1a
//		dev[thisdev].xmem[0x2286 * 3] = 0x00;	/* 97N1A */
//		dev[thisdev].xmem[0x2286 * 3 + 1] = 0x00;
//		dev[thisdev].xmem[0x2286 * 3 + 2] = 0x33;
		break;
	default:
		PRINT("dummy: %s, unknown type %d\n", __FUNCTION__, type);
		break;
	}
}

/* modules */
#define MODULE_SPEAKERBOOST  1

/* RPC commands */
#define PARAM_SET_LSMODEL        0x06	/* Load a full model into SpeakerBoost. */
#define PARAM_SET_LSMODEL_SEL    0x07	/* Select one of the default models present in Tfa9887 ROM. */
#define PARAM_SET_EQ             0x0A	/* 2 Equaliser Filters. */
#define PARAM_SET_PRESET         0x0D	/* Load a preset */
#define PARAM_SET_CONFIG         0x0E	/* Load a config */

#define PARAM_GET_RE0            0x85	/* gets the speaker calibration impedance (@25 degrees celsius) */
#define PARAM_GET_LSMODEL        0x86	/* Gets current LoudSpeaker Model. */
#define PARAM_GET_LSMODELW       0xC1	/* Gets current LoudSpeaker excursion Model. */
#define PARAM_GET_ALL            0x80	/* read current config and preset. */
#define PARAM_GET_STATE          0xC0
#define PARAM_GET_TAG            0xFF

/* RPC Status results */
#define STATUS_OK                  0
#define STATUS_INVALID_MODULE_ID   2
#define STATUS_INVALID_PARAM_ID    3
#define STATUS_INVALID_INFO_ID     4

static char *cfmemName[] = { "dev[thisdev].pmem", "dev[thisdev].xmem", "dev[thisdev].ymem", "dev[thisdev].iomem" };

/* True if PLL is on */
static int isClockOn(int thisdev) {
	return (TFA_GET_BF(CLKS)); /* clks should be enough */
}

/*
 * write to CF memory space
 */
static int lxDummyMemw(int type, const uint8_t *data)
{
	uint8_t *memptr=(uint8_t *)data;
	int idx = dev[thisdev].memIdx;

	switch (type) {
	case CF_PMEM:
		/* dev[thisdev].pmem is 4 bytes */
		idx = (dev[thisdev].memIdx - (dev[thisdev].Reg[TFA98XX_CF_CONTROLS] * 3)) / 4;	/* this is the offset */
		idx += CF_PATCHMEM_START;
		DUMMYVERBOSE
		    PRINT("W %s[%02d]: 0x%02x 0x%02x 0x%02x 0x%02x\n",
			   cfmemName[type], idx, data[0], data[1], data[2],
			   data[3]);
		if ((CF_PATCHMEM_START <= idx)
&&(idx < (CF_PATCHMEM_START + CF_PATCHMEM_LENGTH))) {
			memptr = &dev[thisdev].pmem[(idx - CF_PATCHMEM_START) * 4];
			dev[thisdev].memIdx += 4;
			*memptr++ = *data++;
			*memptr++ = *data++;
			*memptr++ = *data++;
			*memptr++ = *data++;
			return 4;

		} else {
			//PRINT("dummy: dev[thisdev].pmem[%d] write is illegal!\n", idx);
			;//return 0;
		}

		break;
	case CF_XMEM:
		memptr = &dev[thisdev].xmem[idx];
		if(idx/3 ==dev[thisdev].xmem_patch_version)
			FUNC_TRACE("Patch version=%d.%d.%d\n", data[0], data[1], data[2]);
		break;
	case CF_YMEM:
		memptr = &dev[thisdev].ymem[idx];
		break;
	case CF_IOMEM:
		/* address is in TFA98XX_CF_MAD */
		if (dev[thisdev].Reg[FAM_TFA98XX_CF_MADD] == 0x8100) {	/* CF_CONTROL reg */
			if (data[2] & 1) {	/* set ACS */
//				dev[thisdev].Reg[TFA1_STATUSREG] |=
//				    (TFA98XX_STATUS_FLAGS0_ACS);
				TFA_SET_BF(ACS,1);
				dev[thisdev].memIdx += 3;
				return 3;	/* go back, writing is done */
			} else { 		/* clear ACS */
				//dev[thisdev].Reg[TFA1_STATUSREG] &=  ~(TFA98XX_STATUS_FLAGS0_ACS);
				TFA_SET_BF(ACS,0);
				dev[thisdev].memIdx += 3;
				return 3;	/* go back, writing is done */			}
		} else if ((CF_IO_PATCH_START <= dev[thisdev].Reg[FAM_TFA98XX_CF_MADD]) &&
			   (dev[thisdev].Reg[FAM_TFA98XX_CF_MADD] <
			    (CF_IO_PATCH_START + CF_IO_PATCH_LENGTH))) {
			memptr = &dev[thisdev].iomem[idx];
		} else {
			/* skip other io's */
			return 3;
		}
		memptr = &dev[thisdev].iomem[idx];
		break;
	}
	DUMMYVERBOSE
	    PRINT("W %s[%02d]: 0x%02x 0x%02x 0x%02x\n", cfmemName[type],
		   idx / 3, data[0], data[1], data[2]);
	*memptr++ = *data++;
	*memptr++ = *data++;
	*memptr++ = *data++;
	dev[thisdev].memIdx += 3;
	return 3;		/* TODO 3 */
}

/*
 * read from CF memory space
 */
static int lxDummyMemr(int type, uint8_t *data)
{
	uint8_t *memptr = data;
	int idx = dev[thisdev].memIdx;

	switch (type) {
	case CF_PMEM:
		memptr = &dev[thisdev].pmem[idx];
		break;
	case CF_XMEM:
		memptr = &dev[thisdev].xmem[idx];
		break;
	case CF_YMEM:
		memptr = &dev[thisdev].ymem[idx];
		break;
	case CF_IOMEM:
		memptr = &dev[thisdev].iomem[idx];
		break;
	}
	DUMMYVERBOSE
	    PRINT("R %s[%02d]: 0x%02x 0x%02x 0x%02x\n", cfmemName[type],
		   idx / 3, memptr[0], memptr[1], memptr[2]);

	*data++ = *memptr++;
	*data++ = *memptr++;
	*data++ = *memptr++;
/* *data++ =0; */
/* *data++ =0; */
/* *data++ =0; */

	dev[thisdev].memIdx += 3;

	return 3;		/* TODO 3 */
}

/******************************************************************************
 * DSP RPC interaction response
 */
static void setStateInfo(Tfa98xx_StateInfo_t *pInfo, unsigned char *bytes);
static void makeStateInfo(float agcGain, float limGain, float sMax,
			  int T, int statusFlag, float X1, float X2, float Re, int shortOnMips);

/* from Tfa9887_internals.h */
#define SPKRBST_HEADROOM			 7	/* Headroom applied to the main input signal */
#define SPKRBST_AGCGAIN_EXP			SPKRBST_HEADROOM	/* Exponent used for AGC Gain related variables */
#define SPKRBST_TEMPERATURE_EXP     9
#define SPKRBST_LIMGAIN_EXP			    4	/* Exponent used for Gain Corection related variables */
#define SPKRBST_TIMECTE_EXP         1

static void setStateInfo(Tfa98xx_StateInfo_t *pInfo, unsigned char *bytes)
{
	int data[STATE_SIZE];

	data[0] =
	    (int)roundf(pInfo->agcGain * (1 << (23 - SPKRBST_AGCGAIN_EXP)));
	data[1] =
	    (int)roundf(pInfo->limGain * (1 << (23 - SPKRBST_LIMGAIN_EXP)));
	data[2] = (int)roundf(pInfo->sMax * (1 << (23 - SPKRBST_HEADROOM)));
	data[3] = pInfo->T * (1 << (23 - SPKRBST_TEMPERATURE_EXP));
	data[4] = pInfo->statusFlag;
	data[5] = (int)roundf(pInfo->X1 * (1 << (23 - SPKRBST_HEADROOM)));
	// 97 has shorter stateinfo
	if ( dev[0].type != tfa9897) { // TODO allow mixed types ?
		data[6] = (int)roundf(pInfo->X2 * (1 << (23 - SPKRBST_HEADROOM)));
		data[7] = (int)roundf(pInfo->Re * (1 << (23 - SPKRBST_TEMPERATURE_EXP)));
		data[8]= pInfo->shortOnMips;
	} else {
		data[6] =  (int)roundf(pInfo->Re * (1 << (23 - SPKRBST_TEMPERATURE_EXP)));
		data[7]=pInfo->shortOnMips;
		data[8]=0 ; // not used
	}

	convert24Data2Bytes(STATE_SIZE, bytes, data);

}
#if (defined( TFA9887B) || defined( TFA98XX_FULL ))
static void setStateInfoDrc(Tfa98xx_DrcStateInfo_t *pInfo, unsigned char *bytes) {
	int data[STATE_SIZE_DRC], *pdata;

	pdata = data;
	*pdata++ = (int)roundf(pInfo->GRhighDrc1[0]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRhighDrc1[1]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRhighDrc2[0]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRhighDrc2[1]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRmidDrc1[0]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRmidDrc1[1]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRmidDrc2[0]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRmidDrc2[1]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRlowDrc1[0]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRlowDrc1[1]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRlowDrc2[0]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRlowDrc2[1]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRpostDrc1[0]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRpostDrc1[1]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRpostDrc2[0]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRpostDrc2[1]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata++ = (int)roundf(pInfo->GRblDrc[0]* (1 << (23 - SPKRBST_HEADROOM)));
	*pdata = (int)roundf(pInfo->GRblDrc[1]* (1 << (23 - SPKRBST_HEADROOM)));

	convert24Data2Bytes(STATE_SIZE_DRC, bytes, data);
}
#endif
/*
 * fill the StateInfo structure with the input data
 */
static void makeStateInfo(float agcGain, float limGain, float sMax,
			  int T, int statusFlag, float X1, float X2, float Re, int shortOnMips)
{
	Tfa98xx_StateInfo_t Info;

	Info.agcGain = agcGain;
	Info.limGain = limGain;
	Info.sMax = sMax;
	Info.T = T;
	Info.statusFlag = statusFlag;
	Info.X1 = X1;
	Info.X2 = X2; // skipped for 97
	Info.Re = Re;
	Info.shortOnMips = shortOnMips;
	setStateInfo(&Info, stateInfo);

}
#if (defined( TFA9887B) || defined( TFA98XX_FULL ))
/*
 * fill the StateInfo structure with a pattern
 */
static void makeStateInfoDrcInit()
{
	int i;
	float data[STATE_SIZE_DRC];

	for (i=0;i<STATE_SIZE_DRC;i++){
		data[i] = (float)(10+i); //value, 9 is previous (shortOnMips)
	}
	makeStateInfoDrc(data);
}

/*
 * fill the StateInfo structure with the input data
 */
static void makeStateInfoDrc(float *data)
{
	setStateInfoDrc((Tfa98xx_DrcStateInfo_t*)data, stateInfoDrc);
}
#endif
#define TFA9887_MAXTAG              (138)
/* the number of elements in Tfa98xx_SpeakerBoost_StateInfo */
#define FW_STATE_SIZE             9
#if (defined( TFA9887B) || defined( TFA98XX_FULL ))
#define FW_STATEDRC_SIZE        18	/* extra elements for DRC */
#define FW_STATE_MAX_SIZE      (FW_STATE_SIZE + FW_STATEDRC_SIZE)
#else
#define FW_STATE_MAX_SIZE       FW_STATE_SIZE
#endif

/*
 * fill dev[thisdev].xmem Speakerboost module RPC buffer with the return values
 */
static int setgetDspParamsSpeakerboost(int param)
{
	int i = 0;
	/* memory address to be accessed (0 : Status, 1 : ID, 2 : parameters) */

	switch (param) {
	case SB_ALL_ID:
		dev[thisdev].xmem[0] = 0;
		dev[thisdev].xmem[1] = 0;
		dev[thisdev].xmem[2] = 0;
		memcpy(dev[thisdev].lastConfig, &dev[thisdev].xmem[6], sizeof(dev[thisdev].lastConfig)); //copies a bit too much for tfa1
		FUNC_TRACE("loaded algo\n");
		break;
	case PARAM_SET_CONFIG:
		dev[thisdev].xmem[0] = 0;
		dev[thisdev].xmem[1] = 0;
		dev[thisdev].xmem[2] = 0;
		memcpy(dev[thisdev].lastConfig, &dev[thisdev].xmem[6], sizeof(dev[thisdev].lastConfig));
		FUNC_TRACE("loaded config\n");
		break;
	case PARAM_SET_PRESET:
		dev[thisdev].xmem[0] = 0;
		dev[thisdev].xmem[1] = 0;
		dev[thisdev].xmem[2] = 0;
		memcpy(dev[thisdev].lastPreset, &dev[thisdev].xmem[6], sizeof(dev[thisdev].lastPreset));
//		if (lxDummyFailTest == 8)
//			lastPreset[1] = ~lastPreset[1];
		FUNC_TRACE("loaded preset\n");
		break;
	case SB_LSMODEL_ID:
		dev[thisdev].xmem[0] = 0;
		dev[thisdev].xmem[1] = 0;
		dev[thisdev].xmem[2] = 0;
		memcpy(dev[thisdev].lastSpeaker, &dev[thisdev].xmem[6],
		       sizeof(dev[thisdev].lastSpeaker));
		FUNC_TRACE("loaded speaker\n");
		break;
	case SB_LAGW_ID:
		FUNC_TRACE("SB_LAGW_ID\n");
		dev[thisdev].xmem[0] = 0;
		dev[thisdev].xmem[1] = 0;
		dev[thisdev].xmem[2] = 0;
		break;
	case SB_LSMODEL_ID | 0x80:	/* for now just return the speakermodel */
	FUNC_TRACE("GetLsModel\n");
		dev[thisdev].xmem[0] = 0;
		dev[thisdev].xmem[1] = 0;
		dev[thisdev].xmem[2] = 0;
		//memcpy(&dev[thisdev].xmem[6], lsmodelw, sizeof(Tfa98xx_SpeakerParameters_t)); //Original
		memcpy(&dev[thisdev].xmem[6], dev[thisdev].lastSpeaker, sizeof(dev[thisdev].lastSpeaker)); //static_x_model is a static kopie of a real X-model
		break;
	case SB_ALL_ID| 0x80:
		dev[thisdev].xmem[0] = 0;
		dev[thisdev].xmem[1] = 0;
		dev[thisdev].xmem[2] = 0;

		memcpy(&dev[thisdev].xmem[6], dev[thisdev].lastConfig, sizeof(dev[thisdev].lastConfig));
		if (IS_FAM==2) {
			FUNC_TRACE("get algo\n");
		}
		else { //tfa1 append preset as well
			memcpy(&dev[thisdev].xmem[6 + dev[thisdev].config_length], dev[thisdev].lastPreset,
					sizeof(dev[thisdev].lastPreset));
			FUNC_TRACE("get all (config+preset)\n");
		}
		break;
	case PARAM_GET_RE0:		/* : */
		FUNC_TRACE("PARAM_GET_RE0\n");
		dev[thisdev].xmem[0] = 0;
		dev[thisdev].xmem[1] = 0;
		dev[thisdev].xmem[2] = 0;
		dev[thisdev].xmem[6] = 0x07;
		dev[thisdev].xmem[7] = 0x6f;
		dev[thisdev].xmem[8] = 0xcd;
		dev[thisdev].xmem[9] = 0x07;
		dev[thisdev].xmem[10] = 0x6e;
		dev[thisdev].xmem[11] = 0x5b;
		break;
	case SB_PARAM_SET_AGCINS:
		FUNC_TRACE("SB_PARAM_SET_AGCINS\n");
		dev[thisdev].xmem[0] = 0;
		dev[thisdev].xmem[1] = 0;
		dev[thisdev].xmem[2] = 0;
		break;
	case SB_PARAM_SET_DRC:
		FUNC_TRACE("set DRC (dummy tbd)\n");
		break;
	default:
		PRINT("%s: unknown RPC PARAM:0x%0x\n", __FUNCTION__, param);
		dev[thisdev].xmem[0] = 0;
		dev[thisdev].xmem[1] = 0;
		dev[thisdev].xmem[2] = 3;
		break;
	}

	return i - 1;
}
/*
 *
 */
#define BIQUAD_COEFF_SIZE 6
static int setDspParamsEq(int param)
{
	int index, size;

	dev[thisdev].xmem[0] = 0;
	dev[thisdev].xmem[1] = 0;
	dev[thisdev].xmem[2] = 0;

	DUMMYVERBOSE hexdump(18, &dev[thisdev].xmem[6]);

	switch (dev2fam(&dev[thisdev]))
	{
	case 1:
		index = param & 0x7f;
		if (index > 0x0a) {
			FUNC_TRACE("EQ: unsupported param: 0x%x\n", param);
			return 0;
		} else if (index == 0) {
			size = TFA98XX_FILTERCOEFSPARAMETER_LENGTH;
		} else {
			size = BIQUAD_COEFF_LENGTH;
		}
		break;
	case 2:
		switch (param & 0x7f) {
		case 0:
			index = 0;
			size = TFA98XX_FILTERCOEFSPARAMETER_LENGTH;
			break;
		case 1:
			index = 0;
			size = 10 * BIQUAD_COEFF_LENGTH;
			break;
		case 2:
			index = 10;
			size = 10 * BIQUAD_COEFF_LENGTH;
			break;
		case 3:
			index = 20;
			size = 2 * BIQUAD_COEFF_LENGTH;
			break;
		case 4:
			index = 22;
			size = 2 * BIQUAD_COEFF_LENGTH;
			break;
		case 5:
			index = 24;
			size = 2 * BIQUAD_COEFF_LENGTH;
			break;
		case 6:
			index = 26;
			size = 2 * BIQUAD_COEFF_LENGTH;
			break;
		default:
			FUNC_TRACE("EQ: unsupported param: 0x%x\n", param);
			return 0;
		}
		break;
	default:
		FUNC_TRACE("EQ: unsupported family: %d\n", dev2fam(&dev[thisdev]));
		return 0;
	}

	if ((param & BFB_PAR_ID_GET_COEFS) == BFB_PAR_ID_GET_COEFS) {
		FUNC_TRACE("EQ: GetCoefs %d\n", index);
		memcpy(&dev[thisdev].xmem[3], &dev[thisdev].filters[index], size);
	} else {
		FUNC_TRACE("EQ: SetCoefs %d\n", index);
		memcpy(&dev[thisdev].filters[index], &dev[thisdev].xmem[6], size);
	}

	return 0;
}

static int setDspParamsRe0(int param)
{
		dev[thisdev].xmem[0] = 0;
		dev[thisdev].xmem[1] = 0;
		dev[thisdev].xmem[2] = 0;
		PRINT("\nRe[%d] ", param);

		return 0;
}
/*
 * fill dev[thisdev].xmem Framework module RPC buffer with the return values
 */
static int setDspParamsFrameWork(int param)
{
	int len,i;
//	printf(">>>>>>>>>>%0x\n", param);
	//FUNC_TRACE("FrameWork");
	dev[thisdev].xmem[0] = 0;
	dev[thisdev].xmem[1] = 0;
	dev[thisdev].xmem[2] = 0;

	switch (param) {
	case FW_PAR_ID_SET_MEMORY://duplicate MSG ID, also FW_PAR_ID_SET_CURRENT_DELAY
		if (IS_FAM==2) {
			FUNC_TRACE("FW_PAR_ID_SET_MEMORY\n");
		}
		else {
			FUNC_TRACE("FW_PAR_ID_SET_CURRENT_DELAY (set firmware delay table):");
			for(i=0;i<9;i++)
				FUNC_TRACE(" %d", dev[thisdev].xmem[8+i*3]);
			FUNC_TRACE("\n");
		}
		break;
	case FW_PAR_ID_SET_SENSES_DELAY:
		FUNC_TRACE("FW_PAR_ID_SET_SENSES_DELAY\n");
		break;
	case FW_PAR_ID_SET_INPUT_SELECTOR: //duplicate MSG ID, also FW_PAR_ID_SET_CURFRAC_DELAY
		if (IS_FAM==2) {
			FUNC_TRACE("FW_PAR_ID_SET_INPUT_SELECTOR\n");
		}
		else {
			FUNC_TRACE("FW_PAR_ID_SET_CURFRAC_DELAY (set fractional delay table):");
			for(i=0;i<9;i++)
				FUNC_TRACE(" %d", dev[thisdev].xmem[8+i*3]);
			FUNC_TRACE("\n");
		}
		break;
	case 7:
		FUNC_TRACE("strange, this was FW_PAR_ID_SET_INPUT_SELECTOR_2:0x07, unspec-ed now\n");
		break;
	case FW_PAR_ID_SET_OUTPUT_SELECTOR:
		FUNC_TRACE("FW_PAR_ID_SET_OUTPUT_SELECTOR\n");
		break;
	case SB_PARAM_SET_LAGW:
		FUNC_TRACE("SB_PARAM_SET_LAGW\n");
		break;
	case FW_PAR_ID_SET_PROGRAM_CONFIG:
		FUNC_TRACE("FW_PAR_ID_SET_PROGRAM_CONFIG\n");
		break;
	case FW_PAR_ID_SET_GAINS:
		FUNC_TRACE("FW_PAR_ID_SET_GAINS\n");
		break;
	case FW_PAR_ID_SETSENSESCAL:
		FUNC_TRACE("FW_PAR_ID_SETSENSESCAL\n");
		break;
	case FW_PAR_ID_GET_FEATURE_INFO:
		FUNC_TRACE("FW_PAR_ID_GET_FEATURE_INFO\n");
		dev[thisdev].xmem[0] = 0;
		dev[thisdev].xmem[1] = 0;
		dev[thisdev].xmem[2] = (dev[thisdev].type == tfa9887) || (dev[thisdev].type == tfa9890) ? 3 : 0;	/* no feature bits */
		/* i2cExecuteRS W   2: 0x6c 0x72 */
		/* i2cExecuteRS R   4: 0x6d 0x00 0x00 0x03 */
		dev[thisdev].xmem[6] = 0x00;
		dev[thisdev].xmem[7] = (dev[thisdev].type == tfa9895 ||
				dev[thisdev].type == tfa9891 ) ? 0x00 : 0x02;	/* No DRC */
		dev[thisdev].xmem[8] = 0x00;
		break;
#define TFA1_DSP_revstring        "< Dec 21 2011 - 12:33:16 -  SpeakerBoostOnCF >"
#define TFA2_DSP_revstring		 "< Dec  8 2014 - 17:13:09 - App_Max2 >"
	case FW_PAR_ID_GET_TAG: {
		int i = 0, j;
		uint8_t *ptr;
		char *tag = IS_FAM==1 ? TFA1_DSP_revstring : TFA2_DSP_revstring;
		FUNC_TRACE("FW_PAR_ID_GET_TAG\n");
		/* tag */
		ptr = &dev[thisdev].xmem[6 + 2];
		for (i = 0, j = 0; i < TFA9887_MAXTAG; i++, j += 3) {
			ptr[j] = tag[i];	/* 24 bits, byte[2] */
		}
		if (lxDummyFailTest == 6)
			ptr[0] = '!';	/* fail */
		/* *pRpcStatus = (mem[0]<<16) | (mem[1]<<8) | mem[2]; */
		dev[thisdev].xmem[0] = 0;
		dev[thisdev].xmem[1] = 0;
		dev[thisdev].xmem[2] = 0;
	}
		break;

	case FW_PAR_ID_SET_MEMTRACK:
		len = XMEM2INT(2*3) ;//dev[thisdev].xmem[8];
		FUNC_TRACE("FW_PAR_ID_SET_MEMTRACK: len=%d words\n", len);
		if (len>20) {
			PRINT("Error memtrack list too long (max=20)!!!\n");
			return 1;
		}
		for (i=(3*3);i<(len+3)*3;i+=3) {
			PRINT(" %d 0x%04x", dev[thisdev].xmem[i], dev[thisdev].xmem[i+1]<<8|dev[thisdev].xmem[i+2]);
		}
		PRINT("\n");
		break;
	default:
		PRINT("Unknown M-ID\n");
	}

	return 0;
}

/******************************************************************************
 * utility and helper functions
 */
static FILE *infile;
/*
 * get state info from file and wrap around
 */
static int getStateInfo(void)
{
	int n, linenr, ShortOnMips;
	float agcGain, limitGain, limitClip, batteryVoltage,
	    boostExcursion, manualExcursion, speakerResistance;
	unsigned int icTemp, speakerTemp;
	unsigned int shortOnMips = 0;
	unsigned short statusFlags, statusRegister;
	char line[256];

	if (infile == 0)
		return -1;

	if (feof(infile)) {
		rewind(infile);
		fgets(line, sizeof(line), infile);	/* skip 1st line */
	}

	fgets(line, sizeof(line), infile);
	n = sscanf(line, "%d,%hx,0x%4hx,%f,%f,%f,%f,%u,%u,%f,%f,%f,%d",	/* 1 2 *///TODO update csv!!!!
		   &linenr,	/* 3 */
		   &statusRegister,	/* 4 */
		   &statusFlags,	/* 5 */
		   &agcGain,	/* 6 */
		   &limitGain,	/* 7 */
		   &limitClip,	/* 8 */
		   &batteryVoltage,	/* 9 */
		   &speakerTemp,	/* 10 */
		   &icTemp,	/* 11 */
		   &boostExcursion,	/* 12 */
		   &manualExcursion,	/* 13 */
		   &speakerResistance,	/* 14 */
		   &ShortOnMips);	/* 15 */
	/* PRINT("%x >%s\n",statusRegister,line); */

	if (13 == n) {

		makeStateInfo(agcGain, limitGain, limitClip, speakerTemp,
			      statusFlags, boostExcursion, manualExcursion,
			      speakerResistance,shortOnMips);
		dev[thisdev].Reg[0] = statusRegister;
		dev[thisdev].Reg[1] = (uint16_t)batteryVoltage;
		dev[thisdev].Reg[2] = (uint16_t)icTemp;
		return 0;
	}
	return 1;

}

/*
 * set the input file for state info input
 */
static int setInputFile(char *file)
{
	char line[256];

	infile = fopen(file, "r");

	if (infile == 0)
		return 0;

	fgets(line, sizeof(line), infile);	/* skip 1st line */

	return 1;
}

static void hexdump(int num_write_bytes, const unsigned char *data)
{
	int i;

	for (i = 0; i < num_write_bytes; i++) {
		PRINT("0x%02x ", data[i]);
	}

}

/* convert DSP memory bytes to signed 24 bit integers
   data contains "num_bytes/3" elements
   bytes contains "num_bytes" elements */
static void convert24Data2Bytes(int num_data, unsigned char bytes[], int data[])
{
	int i;			/* index for data */
	int k;			/* index for bytes */
/* int num_bytes = num_data * 3; */

	for (i = 0, k = 0; i < num_data; ++i, k += 3) {
		*bytes = 0xff & (data[i] >> 16);
		if (data[i] < 0)
			*bytes++ |= 0x80;	/* sign */
		else
			bytes++;
		*bytes++ = 0xff & (data[i] >> 8);
		*bytes++ = 0xff & (data[i]);
	}
}

/* convert DSP memory bytes to signed 24 bit integers
   data contains "num_bytes/3" elements
   bytes contains "num_bytes" elements */
void convertBytes2Data24(int num_bytes, const unsigned char bytes[],
			       int data[])
{
	int i;			/* index for data */
	int k;			/* index for bytes */
	int d;
	int num_data = num_bytes / 3;
	//_ASSERT((num_bytes % 3) == 0);
	for (i = 0, k = 0; i < num_data; ++i, k += 3) {
		d = (bytes[k] << 16) | (bytes[k + 1] << 8) | (bytes[k + 2]);
		//_ASSERT(d >= 0);
		//_ASSERT(d < (1 << 24));	/* max 24 bits in use */
		if (bytes[k] & 0x80)	/* sign bit was set */
			d = -((1 << 24) - d);

		data[i] = d;
	}
}

/*
 * return family type for this device
 *  copy of the tfa tfa98xx_dev2family(this->Reg[3]);
 *  NOTE: Dependency from HAL layer. Does not work on Windows..
 *  TODO: replace by TFA API when in single library
 */
static int dev2fam(struct dummy_device *this) {
	int dev_type = this->Reg[3];
	/* only look at the die ID part (lsb byte) */
	switch(dev_type & 0xff) {
	case 0x12:
	case 0x80:
	case 0x81:
	case 0x91:
	case 0x92:
	case 0x97:
		return 1;
	case 0x88:
		return 2;
	case 0x50:
		return 3;
	default:
		return 0;
	}
}

static int dummy_set_bf(struct dummy_device *this, const uint16_t bf, const uint16_t value)
{
	uint16_t regvalue, msk, oldvalue;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	regvalue = this->Reg[address];

	oldvalue = regvalue;
	msk = ((1 << (len + 1)) - 1) << pos;
	regvalue &= ~msk;
	regvalue |= value << pos;

	/* Only write when the current register value is not the same as the new value */
	if (oldvalue != regvalue) {

		this->Reg[address] = regvalue;
	}

	return 0;
}

static int dummy_get_bf(struct dummy_device *this, const uint16_t bf)
{
	uint16_t regvalue, msk;
	uint16_t value;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

    regvalue = this->Reg[address];

	msk = ((1<<(len+1))-1)<<pos;
	regvalue &= msk;
	value = regvalue>>pos;

	return value;
}
