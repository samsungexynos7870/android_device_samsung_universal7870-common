/*
 * cliCommands.c
 *
 *  Created on: Apr 3, 2012
 *      Author: nlv02095
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !(defined(WIN32) || defined(_X64))
#include <unistd.h>
#include <inttypes.h>	//TODO fix/converge types
#endif

#include "tfa.h"
#include "cmdline.h"
#include "climax.h"
#include "dbgprint.h"
#include "tfaContUtil.h"
#include "tfa98xxCalibration.h"
#include "tfa98xxDiagnostics.h"
#include "tfa98xxLiveData.h"
#include "tfa_container.h"
#include "tfa98xxCalculations.h"
#include "tfaContainerWrite.h"
#include <limits.h>
#include "NXP_I2C.h"


#if !(defined(WIN32) || defined(_X64))
/************************
 * time measurement TODO cleanup
 */
#include <sys/time.h>
#include <sys/resource.h>
typedef struct tag_time_measure
{
  struct timeval startTimeVal;
  struct timeval stopTimeVal;

  struct rusage startTimeUsage;
  struct rusage stopTimeUsage;
} time_measure;
time_measure * startTimeMeasuring();
void stopTimeMeasuring(time_measure * tu);
void printMeasuredTime(time_measure * tu);

extern int gNXP_i2c_writes, gNXP_i2c_reads; // declared in NXP i2c hal
extern int gTfaRun_timingVerbose; // in tfaRuntime
#endif

extern struct gengetopt_args_info gCmdLine; /* Globally visible command line args */
extern int nxpTfaCurrentProfile;
extern unsigned char gTfa98xxI2cSlave;
int gNXP_i2c_writes, gNXP_i2c_reads;

/*
 *
 */
static size_t cliFileToBuffer(char *filename, char *buffer, int max)
{
	FILE *f;
	size_t len;

	f = fopen(filename, "rb");;
	if (!f) {
		PRINT_ERROR("can't open %s\n", filename);
		return (size_t)-1;
	}

	len = fread(buffer, 1, max, f);
	if(cli_verbose) PRINT("%s: %i bytes from %s\n", __FUNCTION__, (int)len, filename);

	fclose(f);

	return len;
}

/*
 * execute the commands
 */
int cliCommands(char *xarg, Tfa98xx_handle_t *handlesIn, int profile)
{
	unsigned char buffer[6*1024] = {0};	//,byte TODO check size or use malloc
	char str[NXPTFA_MAXLINE];
	int i, j, k, writes;
	int length, maxdev;
	int loopcount=gCmdLine.loop_arg, printloop=1;
	float tCoef, imp[2];
	int spkr_count = 0;
	Tfa98xx_Error_t error=Tfa98xx_Error_Ok;

  do {
    if ( gCmdLine.reset_given ) {
		error = tfa_reset();
		if (error != Tfa98xx_Error_Ok) {	// error with profile number so exit
			PRINT("last status: %d (%s)\n", error, Tfa98xx_GetErrorString(error));
			return error;
		}
	}

    // the param file is a multi arg
    for (i = 0; i < (int)gCmdLine.params_given; ++i)
    {
	if( handlesIn[0] == -1) //TODO properly handle open/close dev
		PRINT_ASSERT( Tfa98xx_Open(gTfa98xxI2cSlave*2, &handlesIn[0] ));

	error = tfaContWriteFileByname(handlesIn[0], gCmdLine.params_arg[i], gCmdLine.volume_arg, gCmdLine.vstepmsg_arg);

	if (error != Tfa98xx_Error_Ok) {
		if (error == Tfa98xx_Error_DSP_not_running) {
			PRINT("DSP not running\n");
		}
	}
    }
//    /*
//     *  here the parameters are known :::in container now
//     */


    //
    // a write is only performed if there is a register AND a regwrite option
    // the nr of register args determine the total amount of transactions
    //
    writes = gCmdLine.regwrite_given;
    for (i = 0; i < (int)gCmdLine.register_given; ++i)
    {
	unsigned short value;

	maxdev=gCmdLine.slave_given ? 1 : tfa98xx_cnt_max_device();
	if ( maxdev <= 0 ) {
		PRINT("Please provide slave or container file.\n");
		return 0;
	} else {
		for (j=0; j < maxdev; j++ ) {
			if (gCmdLine.slave_given) {
				gTfa98xxI2cSlave = (unsigned char) gCmdLine.slave_arg;
				tfa98xxI2cSetSlave(gTfa98xxI2cSlave);
			} else {
				tfaContGetSlave(j, &gTfa98xxI2cSlave); /* get device I2C address */
			}
			if( handlesIn[j] == -1) /* TODO properly handle open/close dev */
				error = Tfa98xx_Open(gTfa98xxI2cSlave*2, &handlesIn[j]);

			if ( !writes ) {// read if no write arg
				error = Tfa98xx_ReadRegister16(handlesIn[j], (unsigned char)gCmdLine.register_arg[i], &value);
				PRINT("[0x%02x] 0x%02x : 0x%04x ", gTfa98xxI2cSlave, gCmdLine.register_arg[i], value);

				if (cli_verbose)
					tfaRunBitfieldDump((unsigned char)gCmdLine.register_arg[i], value, tfa98xx_dev_family(handlesIn[j]));
				else
					PRINT("\n");
			} else {
				/* Show what is going to be done */
				PRINT("[0x%02x] 0x%02x < 0x%04x \n", gTfa98xxI2cSlave, gCmdLine.register_arg[i], gCmdLine.regwrite_arg[i]);
				/* Write new value */
				value = (unsigned char) gCmdLine.regwrite_arg[i];
				error = Tfa98xx_WriteRegister16(handlesIn[j], (unsigned char)gCmdLine.register_arg[i], (unsigned short)gCmdLine.regwrite_arg[i]);
				/* Read and show new value */
				error = Tfa98xx_ReadRegister16(handlesIn[j], (unsigned char)gCmdLine.register_arg[i], &value);
				PRINT("[0x%02x] 0x%02x : 0x%04x \n", gTfa98xxI2cSlave, gCmdLine.register_arg[i], value);
			}

			if (error != Tfa98xx_Error_Ok) {	// error with profile number so exit
				PRINT("last status: %d (%s)\n", error, Tfa98xx_GetErrorString(error));
				return error;
			}
		}
	}
    }

    // read xmem
    for (i = 0; i < (int)gCmdLine.xmem_given; ++i)
    {
	enum Tfa98xx_DMEM memtype = Tfa98xx_DMEM_XMEM;
	unsigned char bytes[TFA2_SPEAKERPARAMETER_LENGTH];
	int value, count;
	unsigned int offset;

	maxdev=gCmdLine.slave_given ? 1 : tfa98xx_cnt_max_device();
	if ( maxdev <= 0 ) {
		PRINT("Please provide slave or container file.\n");
		return 0;
	} else {
		for (j=0; j < maxdev; j++ ) {
			if (gCmdLine.slave_given) {
				gTfa98xxI2cSlave = (unsigned char) gCmdLine.slave_arg;
				tfa98xxI2cSetSlave(gTfa98xxI2cSlave);
			} else {
				tfaContGetSlave(j, &gTfa98xxI2cSlave); /* get device I2C address */
			}
			if( handlesIn[j] == -1) /* TODO properly handle open/close dev */
				error = Tfa98xx_Open(gTfa98xxI2cSlave*2, &handlesIn[j]);

			count = gCmdLine.count_given ? gCmdLine.count_arg-1 : 0;
			offset= gCmdLine.xmem_arg[i] & 0xffff /*mask off DMEM*/;

			if ( !writes ) { // read if no write arg
				do {
					if(gCmdLine.rpc_given) {
						error = tfa98xx_dsp_get_memory(handlesIn[j], Tfa98xx_DMEM_XMEM, offset, 1, bytes);
						value = (bytes[0]<<16) + (bytes[1]<<8) + bytes[2];
					} else {
						error = tfa98xx_dsp_read_mem(handlesIn[j], (memtype<<16 | offset), 1, &value);
					}

					PRINT("[0x%02x] xmem[0x%04x] : 0x%06x \n", gTfa98xxI2cSlave, offset, abs(value));
					offset++;

					if (error != Tfa98xx_Error_Ok) {	// error with profile number so exit
						PRINT("last status: %d (%s)\n", error, Tfa98xx_GetErrorString(error));
						return error;
					}
				} while (count--);
			} else {
				PRINT("[0x%02x] 0x%02x < 0x%04x (memtype=%d)\n", gTfa98xxI2cSlave, offset, gCmdLine.regwrite_arg[i], memtype);

				/* Write new value */
				if(gCmdLine.rpc_given)
					error = tfa98xx_dsp_set_memory(handlesIn[j], Tfa98xx_DMEM_XMEM, offset, 1, gCmdLine.regwrite_arg[i]);
				else
					error = Tfa98xx_DspWriteMem(handlesIn[j], (unsigned short)offset, gCmdLine.regwrite_arg[i], memtype);

				/* Show new value */
				if(gCmdLine.rpc_given) {
					error = tfa98xx_dsp_get_memory(handlesIn[j], Tfa98xx_DMEM_XMEM, offset, 1, bytes);
					value = (bytes[0]<<16) + (bytes[1]<<8) + bytes[2];
				} else {
					error = tfa98xx_dsp_read_mem(handlesIn[j], (memtype<<16 | offset), 1, &value);
				}

				PRINT("[0x%02x] xmem[0x%04x] : 0x%06x \n", gTfa98xxI2cSlave, offset, abs(value));

				if (error != Tfa98xx_Error_Ok) {	// error with profile number so exit
					PRINT("last status: %d (%s)\n", error, Tfa98xx_GetErrorString(error));
					return error;
				}
			}
		}
	}
    }

     // read/write pmem
    for (i = 0; i < (int)gCmdLine.pmem_given; ++i)
    {
	    PRINT("Not implemented! \n");
    }

     // read/write ymem
    for (i = 0; i < (int)gCmdLine.ymem_given; ++i)
    {
	enum Tfa98xx_DMEM memtype = Tfa98xx_DMEM_YMEM;
	unsigned char bytes[TFA2_SPEAKERPARAMETER_LENGTH];
	int value, count;
	unsigned int offset;

	maxdev=gCmdLine.slave_given ? 1 : tfa98xx_cnt_max_device();
	if ( maxdev <= 0 ) {
		PRINT("Please provide slave or container file.\n");
		return 0;
	} else {
		for (j=0; j < maxdev; j++ ) {
			if (gCmdLine.slave_given) {
				gTfa98xxI2cSlave = (unsigned char) gCmdLine.slave_arg;
				tfa98xxI2cSetSlave(gTfa98xxI2cSlave);
			} else {
				tfaContGetSlave(j, &gTfa98xxI2cSlave); /* get device I2C address */
			}
			if( handlesIn[j] == -1) /* TODO properly handle open/close dev */
				error = Tfa98xx_Open(gTfa98xxI2cSlave*2, &handlesIn[j]);

			count = gCmdLine.count_given ? gCmdLine.count_arg-1 : 0;
			offset= gCmdLine.ymem_arg[i] & 0xffff /*mask off DMEM*/;

			if ( !writes ) { // read if no write arg
				do {
					if(gCmdLine.rpc_given) {
						error = tfa98xx_dsp_get_memory(handlesIn[j], Tfa98xx_DMEM_YMEM, offset, 1, bytes);
						value = (bytes[0]<<16) + (bytes[1]<<8) + bytes[2];
					} else {
						error = tfa98xx_dsp_read_mem(handlesIn[j], (memtype<<16 | offset), 1, &value);
					}

					PRINT("[0x%02x] ymem[0x%04x] : 0x%06x \n", gTfa98xxI2cSlave, offset, abs(value));
					offset++;

					if (error != Tfa98xx_Error_Ok) {	// error with profile number so exit
						PRINT("last status: %d (%s)\n", error, Tfa98xx_GetErrorString(error));
						return error;
					}
				} while (count--);
			} else {
				PRINT("[0x%02x] 0x%02x < 0x%04x (memtype=%d)\n", gTfa98xxI2cSlave, offset, gCmdLine.regwrite_arg[i], memtype);

				/* Write new value */
				if(gCmdLine.rpc_given)
					error = tfa98xx_dsp_set_memory(handlesIn[j], Tfa98xx_DMEM_YMEM, offset, 1, gCmdLine.regwrite_arg[i]);
				else
					error = Tfa98xx_DspWriteMem(handlesIn[j], (unsigned short)offset, gCmdLine.regwrite_arg[i], memtype);

				/* Show new value */
				if(gCmdLine.rpc_given) {
					error = tfa98xx_dsp_get_memory(handlesIn[j], Tfa98xx_DMEM_YMEM, offset, 1, bytes);
					value = (bytes[0]<<16) + (bytes[1]<<8) + bytes[2];
				} else {
					error = tfa98xx_dsp_read_mem(handlesIn[j], (memtype<<16 | offset), 1, &value);
				}

				PRINT("[0x%02x] ymem[0x%04x] : 0x%06x \n", gTfa98xxI2cSlave, offset, abs(value));

				if (error != Tfa98xx_Error_Ok) {	// error with profile number so exit
					PRINT("last status: %d (%s)\n", error, Tfa98xx_GetErrorString(error));
					return error;
				}
			}
		}
	}
    }

    // read/write iomem
    for (i = 0; i < (int)gCmdLine.iomem_given; ++i)
    {
	enum Tfa98xx_DMEM memtype = Tfa98xx_DMEM_IOMEM;
	unsigned char bytes[TFA2_SPEAKERPARAMETER_LENGTH];
	int value, count;
	unsigned int offset;

	maxdev=gCmdLine.slave_given ? 1 : tfa98xx_cnt_max_device();
	if ( maxdev <= 0 ) {
		PRINT("Please provide slave or container file.\n");
		return 0;
	} else {
		for (j=0; j < maxdev; j++ ) {
			if (gCmdLine.slave_given) {
				gTfa98xxI2cSlave = (unsigned char) gCmdLine.slave_arg;
				tfa98xxI2cSetSlave(gTfa98xxI2cSlave);
			} else {
				tfaContGetSlave(j, &gTfa98xxI2cSlave); /* get device I2C address */
			}
			if( handlesIn[j] == -1) /* TODO properly handle open/close dev */
				error = Tfa98xx_Open(gTfa98xxI2cSlave*2, &handlesIn[j]);

			count = gCmdLine.count_given ? gCmdLine.count_arg-1 : 0;
			offset= gCmdLine.iomem_arg[i] & 0xffff /*mask off DMEM*/;

			if ( !writes ) { // read if no write arg
				do {
					if(gCmdLine.rpc_given) {
						error = tfa98xx_dsp_get_memory(handlesIn[j], Tfa98xx_DMEM_IOMEM, offset, 1, bytes);
						value = (bytes[0]<<16) + (bytes[1]<<8) + bytes[2];
					} else {
						error = tfa98xx_dsp_read_mem(handlesIn[j], (memtype<<16 | offset), 1, &value);
					}

					PRINT("[0x%02x] iomem[0x%04x] : 0x%06x \n", gTfa98xxI2cSlave, offset, abs(value));
					offset++;

					if (error != Tfa98xx_Error_Ok) {	// error with profile number so exit
						PRINT("last status: %d (%s)\n", error, Tfa98xx_GetErrorString(error));
						return error;
					}
				} while (count--);
			} else {
				PRINT("[0x%02x] 0x%02x < 0x%04x (memtype=%d)\n", gTfa98xxI2cSlave, offset, gCmdLine.regwrite_arg[i], memtype);

				/* Write new value */
				if(gCmdLine.rpc_given)
					error = tfa98xx_dsp_set_memory(handlesIn[j], Tfa98xx_DMEM_IOMEM, offset, 1, gCmdLine.regwrite_arg[i]);
				else
					error = Tfa98xx_DspWriteMem(handlesIn[j], (unsigned short)offset, gCmdLine.regwrite_arg[i], memtype);

				/* Show new value */
				if(gCmdLine.rpc_given) {
					error = tfa98xx_dsp_get_memory(handlesIn[j], Tfa98xx_DMEM_IOMEM, offset, 1, bytes);
					value = (bytes[0]<<16) + (bytes[1]<<8) + bytes[2];
				} else {
					error = tfa98xx_dsp_read_mem(handlesIn[j], (memtype<<16 | offset), 1, &value);
				}

				PRINT("[0x%02x] iomem[0x%04x] : 0x%06x \n", gTfa98xxI2cSlave, offset, abs(value));

				if (error != Tfa98xx_Error_Ok) {	// error with profile number so exit
					PRINT("last status: %d (%s)\n", error, Tfa98xx_GetErrorString(error));
					return error;
				}
			}
		}
	}
    }

	/* re0 */
	if ( gCmdLine.re0_given ) {
		maxdev=gCmdLine.slave_given ? 1 : tfa98xx_cnt_max_device(); /* max dev only if no slave option */
		if ( maxdev <= 0 ) {
			PRINT("Please provide slave address or container file.\n");
			return 1;
		}

		for (i=0; i < maxdev; i++ ) {
			if (gCmdLine.slave_given) {
				gTfa98xxI2cSlave = (unsigned char) gCmdLine.slave_arg;
				tfa98xxI2cSetSlave(gTfa98xxI2cSlave);
			}
			else {
				tfaContGetSlave(i, &gTfa98xxI2cSlave); /* get device I2C address */
				PRINT("Found Device[%d]: %s at 0x%02x.\n", i, tfaContDeviceName(i), gTfa98xxI2cSlave);
			}

			if( handlesIn[i] == -1) //TODO properly handle open/close dev
				error = Tfa98xx_Open(gTfa98xxI2cSlave*2, &handlesIn[i]);

			if (error != Tfa98xx_Error_Ok) {	// error with profile number so exit
				PRINT("last status: %d (%s)\n", error, Tfa98xx_GetErrorString(error));
				return error;
			}

			error = tfa98xx_dsp_get_calibration_impedance(handlesIn[i], imp);
			if ( error==Tfa98xx_Error_Ok )
				PRINT("re0: %.2f %.2f\n", imp[0]/1000, imp[1]/1000);
			if ( gCmdLine.re0_arg ) {
				error = tfa98xxSetCalibrationImpedance( (gCmdLine.re0_arg), handlesIn);
				if ( error == Tfa98xx_Error_Ok ) {
					PRINT("New re0: %2.2f\n", gCmdLine.re0_arg);
				} else if ( error == Tfa98xx_Error_Bad_Parameter ) {
					PRINT("Unable to set re0: %2.2f. \n", gCmdLine.re0_arg);
					PRINT("The allowed range is 4.0 to 10.0 ohm.");
				} else {
					PRINT("Unable to set re0: %2.2f.\n", gCmdLine.re0_arg);
					PRINT("Check the patch or device for errors.\n");
				}
			}
		}
	}

	if ( gCmdLine.resetMtpEx_given ) {
		maxdev=gCmdLine.slave_given ? 1 : tfa98xx_cnt_max_device(); /* max dev only if no slave option */
		if ( maxdev <= 0 ) {
			PRINT("Please provide slave address or container file.\n");
			return 1;
		}
		for (i=0; i < maxdev; i++ ) {
			if (gCmdLine.slave_given) {
				gTfa98xxI2cSlave = (unsigned char) gCmdLine.slave_arg;
				tfa98xxI2cSetSlave(gTfa98xxI2cSlave);
			}
			else {
				tfaContGetSlave(i, &gTfa98xxI2cSlave); /* get device I2C address */
				PRINT("Reset MtpEx Device[%d]: %s at 0x%02x.\n", i, tfaContDeviceName(i), gTfa98xxI2cSlave);
			}
			if( handlesIn[i] == -1) //TODO properly handle open/close dev
				error = Tfa98xx_Open(gTfa98xxI2cSlave*2, &handlesIn[i]);

			if (error != Tfa98xx_Error_Ok) {	// error with profile number so exit
				PRINT("last status: %d (%s)\n", error, Tfa98xx_GetErrorString(error));
				return error;
			}

			error = tfa98xxCalResetMTPEX( handlesIn[i] );
			if ( error!=Tfa98xx_Error_Ok ) {
				PRINT_ERROR("Reset MTPEX failed\n");
				return error;
			}
		}
	}

	// must come after loading param files, it will need the loaded speakerfile
	if ( gCmdLine.calibrate_given ) {
		if ( tfa98xx_cnt_max_device() <= 0 ) {
			PRINT_ERROR("Please provide/load container file for calibration.\n");
			goto errorexit;
		}
		maxdev = tfa98xx_cnt_max_device();
		for (i=0; i < maxdev; i++ ) {
			if (gCmdLine.slave_given) {
				gTfa98xxI2cSlave=(unsigned char)gCmdLine.slave_arg;
				tfa98xxI2cSetSlave(gTfa98xxI2cSlave);
				PRINT("Warning: using parameters for device[0] from cnt file!\n");
			}
			else
				tfaContGetSlave(i, &gTfa98xxI2cSlave); /* get device I2C address */

			PRINT("Found Device[%d]: %s at 0x%02x.\n", i, tfaContDeviceName(i), gTfa98xxI2cSlave);

			if( handlesIn[i] == -1) //TODO properly handle open/close dev
				error = Tfa98xx_Open(gTfa98xxI2cSlave*2, &handlesIn[i]);

			if (error != Tfa98xx_Error_Ok) {	// error with profile number so exit
				PRINT("last status: %d (%s)\n", error, Tfa98xx_GetErrorString(error));
				return error;
			}

			if ( gCmdLine.profile_given ) {
				error = getProfileInfo(gCmdLine.profile_arg[0], &profile);
				if (error != Tfa98xx_Error_Ok) {
					PRINT("last status: %d (%s)\n", error, Tfa98xx_GetErrorString(error));
					return error;
				}
			}
			if(profile != -1) {
				//once if o , else always
				error = tfa98xxCalibration(handlesIn, i, gCmdLine.calibrate_arg[0]=='o', profile);
				if ( error!=Tfa98xx_Error_Ok )
					goto errorexit;
			}
		}
	}

	// shows the current  impedance
	if ( gCmdLine.calshow_given )
	{
		unsigned char calshow_buffer[TFA2_SPEAKERPARAMETER_LENGTH];

		maxdev=gCmdLine.slave_given ? 1 : tfa98xx_cnt_max_device(); /* max dev only if no slave option */
		if ( maxdev <= 0 ) {
			PRINT("Please provide slave address or container file.\n");
			return 1;
		}

		for (i=0; i < maxdev; i++ ) {
			if (gCmdLine.slave_given) {
				gTfa98xxI2cSlave = (unsigned char) gCmdLine.slave_arg;
				tfa98xxI2cSetSlave(gTfa98xxI2cSlave);
			}
			else {
				tfaContGetSlave(i, &gTfa98xxI2cSlave); /* get device I2C address */
				PRINT("Found Device[%d]: %s at 0x%02x.\n", i, tfaContDeviceName(i), gTfa98xxI2cSlave);
			}

			if( handlesIn[i] == -1) //TODO properly handle open/close dev
				error = Tfa98xx_Open(gTfa98xxI2cSlave*2, &handlesIn[i]);

			if (error != Tfa98xx_Error_Ok) {	// error with profile number so exit
				PRINT("last status: %d (%s)\n", error, Tfa98xx_GetErrorString(error));
				return error;
			}

			error = tfa_dsp_cmd_id_write_read(handlesIn[i],MODULE_SPEAKERBOOST,SB_PARAM_GET_LSMODEL, sizeof(calshow_buffer), calshow_buffer);
			calculate_tCoef_calibration(calshow_buffer, &tCoef);

			error = tfa_dsp_get_calibration_impedance(handlesIn[i]);
			imp[0] = (float)(tfa_get_calibration_info(handlesIn[i], 0));
			imp[1] = (float)tfa_get_calibration_info(handlesIn[i], 1);

			if (error != Tfa98xx_Error_Ok) {
				imp[0] = 0;
				imp[1] = 0;
				tCoef = 0;
			}

			error = tfa98xx_supported_speakers(handlesIn[i], &spkr_count);

			if (spkr_count == 1)
				PRINT("Current calibration impedance: %.4f\n", imp[0]/1000);
			else
				PRINT("Current calibration impedance: %.4f %.4f\n", imp[0]/1000, imp[1]/1000);

			PRINT("Current calibration tCoef: %f\n", tCoef);
		}
	}

    if ( gCmdLine.versions_given ) {
	maxdev=gCmdLine.slave_given ? 1 : tfa98xx_cnt_max_device();
	if ( maxdev <= 0 ) {
		PRINT("Please provide slave or container file.\n");
		return 0;
	} else {
		for (j=0; j < maxdev; j++ ) {
			if (gCmdLine.slave_given) {
				gTfa98xxI2cSlave = (unsigned char) gCmdLine.slave_arg;
				tfa98xxI2cSetSlave(gTfa98xxI2cSlave);
			} else {
				tfaContGetSlave(j, &gTfa98xxI2cSlave); /* get device I2C address */
				PRINT("Found Device[%d]: %s at 0x%02x.\n", j, tfaContDeviceName(j), gTfa98xxI2cSlave);
			}

			if( handlesIn[j] == -1) /* TODO properly handle open/close dev */
				error = Tfa98xx_Open(gTfa98xxI2cSlave*2, &handlesIn[j]);

			error = tfaVersions(handlesIn, (char*)buffer, sizeof(buffer));
			error = tfaGetDspFWAPIVersion(handlesIn[j], (char*)buffer, sizeof(buffer));
			error = tfa98xxDiagPrintFeatures(handlesIn[j], (char*)buffer, sizeof(buffer));
			sprintf(str, "Climax cli: %s\n", CMDLINE_PARSER_VERSION);
			if((strlen((char*)buffer)+sizeof(str)) < sizeof(buffer))
				strcat((char*)buffer, str);
			NXP_I2C_Version((char *)buffer);
			error = tfa98xxGetAudioSource((char*)buffer, sizeof(buffer));

			if (error == Tfa98xx_Error_Buffer_too_small)    // TODO: realocate more memory
				PRINT("Error: buffer size is too short! \n");
			else {
				puts((char*)buffer);
				fflush(stdout);
			}
		}
	}
    }

    /*
     * gCmdLine.diag_arg=0 means all tests
     */
    if ( gCmdLine.diag_given) {
	int maxdev,i,code,nr;

	maxdev=gCmdLine.slave_given ? 1 : tfa98xx_cnt_max_device(); /* max dev only if no slave option */
	if ( maxdev <= 0 ) {
		PRINT("Please provide slave address or container file.\n");
		return 0;
	}

	for (i=0; i < maxdev; i++ )
	{
		nr = gCmdLine.diag_arg;
			if (gCmdLine.slave_given) {
				gTfa98xxI2cSlave = (unsigned char) gCmdLine.slave_arg;
				tfa98xxI2cSetSlave(gTfa98xxI2cSlave);
			}
			else {
				tfaContGetSlave(i, &gTfa98xxI2cSlave); /* get device I2C address */
				PRINT("Diagnostics run on Device[%d]: %s at 0x%02x.\n", i, tfaContDeviceName(i), gTfa98xxI2cSlave);
			}

		if (nr>0)
			code = tfa98xxDiag ( gTfa98xxI2cSlave,  nr) ;
		else if (nr==-1) {
			code = tfa98xxDiag ( gTfa98xxI2cSlave,  0) ; /* only print descriptions */
			break;
		}
		else // all
			code = tfa98xxDiagGroup(gTfa98xxI2cSlave,  tfa98xxDiagGroupname(xarg));

		nr = tfa98xxDiagGetLatest();
		PRINT("test %d %s (code=%d) %s\n", nr, code ? "Failed" : "Passed", code,
				tfa98xxDiagGetLastErrorString());
		error = code ? Tfa98xx_Error_Fail : Tfa98xx_Error_Ok; // non-0 fail
		if(error)
			goto errorexit;
	}
    }
    /*
     * dump
     *   registers or container hex
     */
    if ( gCmdLine.dump_given) {
	if ( strcmp ("cnt", gCmdLine.dump_arg) == 0 ) {
		/* container pretty dump */
		tfa_cnt_dump();
	} else if (strcmp ("cnthex", gCmdLine.dump_arg) == 0) {
		/* container big endian hex dump */
		tfa_cnt_hexdump();
	} else {
		/* register dump */
		int maxdev;
		maxdev=gCmdLine.slave_given ? 1 : tfa98xx_cnt_max_device(); /* max dev only if no slave option */
		if ( maxdev <= 0 ) {
			PRINT("Please provide slave address or container file.\n");
			return 1;
		}
		for (i=0; i < maxdev; i++ ) {
			if (gCmdLine.slave_given) {
				gTfa98xxI2cSlave = (unsigned char) gCmdLine.slave_arg;
				tfa98xxI2cSetSlave(gTfa98xxI2cSlave);
			}
			else {
				tfaContGetSlave(i, &gTfa98xxI2cSlave); /* get device I2C address */
				PRINT("Dump Device[%d]: %s at 0x%02x.\n", i, tfaContDeviceName(i), gTfa98xxI2cSlave);
			}

			tfa98xxDiagRegisterDump(gTfa98xxI2cSlave);
			//TODO: Functions below are not implemented!
			//tfa98xxDiagRegisterDumpTdm(gTfa98xxI2cSlave);
			//tfa98xxDiagRegisterDumpInt(gTfa98xxI2cSlave);
		}
	}
    }

    if ( gCmdLine.dumpmodel_given) {
	FILE *file, *fpSbGains, *fpSbFilt, *fpCombGain;
	float dump_buf[64];
	s_subBandDynamics sbDynamics[MAX_NR_SUBBANDS] = {0};
	int model=gCmdLine.dumpmodel_arg[0]=='x';
	char *end;
	int index_subband=strtol(&gCmdLine.dumpmodel_arg[1], &end, 16), loop;
	int maxdev = gCmdLine.slave_given ? 1 : tfa98xx_cnt_max_device(); /* max dev only if no slave option */
	int count = gCmdLine.count_given? gCmdLine.count_arg : 0;
	int hz;
	float *SbGains_buf;
	float *SbFilt_buf;
	float *CombGain_buf;
	int dump_buffer_size = (MAX_NR_SUBBANDS*(FFT_SIZE_DRC))*sizeof(double)*sizeof(float);

	/* Allocate memory for the three buffers and clear them: */
	SbGains_buf = (float*) malloc(dump_buffer_size);
	SbFilt_buf = (float*) malloc(dump_buffer_size);
	CombGain_buf = (float*) malloc(dump_buffer_size);
	memset(SbGains_buf, 0, dump_buffer_size);
	memset(SbFilt_buf, 0, dump_buffer_size);
	memset(CombGain_buf, 0, dump_buffer_size);

	if(gCmdLine.dumpmodel_arg[0]!='x' && gCmdLine.dumpmodel_arg[0]!='z' && gCmdLine.dumpmodel_arg[0]!='d') {
		PRINT("Invalid argument given for dumpmodel.\n");
		error = Tfa98xx_Error_Bad_Parameter;
		goto dumpmodelexit;
	}

	if (gCmdLine.slave_given) {
		gTfa98xxI2cSlave = (unsigned char) gCmdLine.slave_arg;
		tfa98xxI2cSetSlave(gTfa98xxI2cSlave);

		if( handlesIn[i] == -1) /* TODO properly handle open/close dev */
			error = Tfa98xx_Open(gTfa98xxI2cSlave*2, &handlesIn[i]);

		if (error != Tfa98xx_Error_Ok) {	/* error with profile number so exit */
			PRINT("last status: %d (%s)\n", error, Tfa98xx_GetErrorString(error));
			goto dumpmodelexit;
		}
	} else {
		error = nxpTfa98xxOpenLiveDataSlaves(handlesIn, gTfa98xxI2cSlave, maxdev);
	}

	if (error == Tfa98xx_Error_Ok) {
		if(gCmdLine.dumpmodel_arg[0]=='x' || gCmdLine.dumpmodel_arg[0]=='z') {
			PRINT("dumping %s model\n", model?"excursion":"impedance");
			for (i=0; i < maxdev; i++ ) {
				/* If count is given print "table" */
				if(count) {
					PRINT("dumping %s model as table per frequency \n", model?"excursion":"impedance");
					for(loop=1; loop<=count; loop++) {
						error = tfa98xxPrintSpeakerTable(handlesIn, (char*)buffer, sizeof(buffer), model, i, loop);
						if(error == Tfa98xx_Error_Fail)
							PRINT("Error: buffer size is too short! \n");

						if (error != Tfa98xx_Error_Ok)
							goto dumpmodelexit;

						length = (int)(strlen((char*)buffer));
						*(buffer+length) = '\0';

						if ( gCmdLine.output_given) {
							if(loop==1)
								file = fopen(gCmdLine.output_arg, "w");
							else
								file = fopen(gCmdLine.output_arg, "a");

							fwrite(buffer, 1, length, file);
							fclose(file);
						} else {
							puts((char*)buffer);
							fflush(stdout);
						}
					}
				} else {
					error = tfa98xxPrintSpeakerModel(handlesIn, dump_buf, sizeof(dump_buf), model, i);
					if(error == Tfa98xx_Error_Fail)
						PRINT("Error: Buffer size is too short! \n");

					if (error != Tfa98xx_Error_Ok)
						goto dumpmodelexit;

					if(maxdev > 1) {
						tfaContGetSlave(i, &gTfa98xxI2cSlave);
					}

					PRINT("\nDevice[%d]: %s at 0x%02x:\n", i, tfaContDeviceName(i), gTfa98xxI2cSlave);
					PRINT("Hz, %s\n", model ? "xcursion mm/V" : "impedance Ohm");

					hz = model ? 62 : 40;
					j = model ? 1 : 0;
					k=0;
					for (; j < 64; j++) {
						PRINT("%d,%f\n", hz, dump_buf[k]);
						hz += 62;
						k++;
					}
				}
			}
		} else {
			PRINT("dumping MBDrc model \n");
			if(index_subband < 1 || index_subband > 63) {
				PRINT("Subband index 0x%x is not within range! \n", index_subband);
				error = Tfa98xx_Error_Bad_Parameter;
				goto dumpmodelexit;
			}

			for (i=0; i < maxdev; i++ ) {
				error = tfa98xxPrintMBDrcModel(handlesIn, i, index_subband, sbDynamics,
						SbGains_buf, SbFilt_buf, CombGain_buf, dump_buffer_size);
				if (error != Tfa98xx_Error_Ok)
					goto dumpmodelexit;

				fpSbGains = fopen("sbGains.txt", "w");
				fpSbFilt = fopen("sbFilt.txt", "w");
				fpCombGain = fopen("combGain.txt", "w");

				/* Be-aware: Calculating the length is dangerous this way
				 * They are linked to the for loop from the tfa98xxPrintMBDrcModel function
				 * If those change, these should change too!!
				 */
				length = (bitCount_subbands(index_subband) * (FFT_SIZE_DRC/2));
				for(j=0; j<length; j++)
					fprintf(fpSbGains, "%f\n", SbGains_buf[j]);

				length = (bitCount_subbands(index_subband) * (FFT_SIZE_DRC/2));
				for(j=0; j<length; j++)
					fprintf(fpSbFilt, "%f\n", SbFilt_buf[j]);

				length = FFT_SIZE_DRC/2;
				for(j=0; j<length; j++)
					fprintf(fpCombGain, "%f\n", CombGain_buf[j]);

				fclose(fpSbGains);
				fclose(fpSbFilt);
				fclose(fpCombGain);

				PRINT("Created files: sbGains.txt, sbFilt.txt, combGain.txt \n");
			}
		}
	}
dumpmodelexit:
	free(SbGains_buf);
	free(SbFilt_buf);
	free(CombGain_buf);
    }

errorexit:
    if ( gCmdLine.loop_given) {
	loopcount = ( gCmdLine.loop_arg == 0) ? 1 : loopcount-1 ;
	if (cli_verbose) PRINT("loop count=%d\n", printloop++); // display the count of the executed loops
	if (error)
		loopcount=0; //stop
    }

    if ( (error!=Tfa98xx_Error_Ok)  | cli_verbose ) {
	PRINT("last status :%d (%s)\n", error, Tfa98xx_GetErrorString(error));
    }
#if !(defined(WIN32) || defined(_X64))
    if (gTfaRun_timingVerbose)
	PRINT("i2c bytes transferred:%d (%d writes, %d reads)\n",
			gNXP_i2c_writes+gNXP_i2c_reads, gNXP_i2c_writes, gNXP_i2c_reads);

	gNXP_i2c_writes = gNXP_i2c_reads =0;
#endif

  } while (loopcount>0) ;

  if ( gCmdLine.save_given)
  {
	maxdev=gCmdLine.slave_given ? 1 : tfa98xx_cnt_max_device();
	if ( maxdev <= 0 ) {
		PRINT("Please provide slave or container file.\n");
		return 0;
	} else {
		for (j=0; j < maxdev; j++ ) {
			if (gCmdLine.slave_given) {
				gTfa98xxI2cSlave = (unsigned char) gCmdLine.slave_arg;
				tfa98xxI2cSetSlave(gTfa98xxI2cSlave);
			} else {
				tfaContGetSlave(j, &gTfa98xxI2cSlave); /* get device I2C address */
			}
			if( handlesIn[j] == -1) /* TODO properly handle open/close dev */
				error = Tfa98xx_Open(gTfa98xxI2cSlave*2, &handlesIn[j]);

			error = tfa98xxSaveFileWrapper(handlesIn[j], gCmdLine.save_arg);
			if ( error == 0 ) {
				PRINT("Save file %s\n", gCmdLine.save_arg);
			} else {
				PRINT("last status:%s\n", Tfa98xx_GetErrorString(Tfa98xx_Error_Bad_Parameter));
				return Tfa98xx_Error_Bad_Parameter;
			}
		}
	}
  }
  return error;
}

/*
 *
 */
int cliTargetDevice(char *devname)
{
	int fd;

	TRACEIN(__FUNCTION__);

	fd = NXP_I2C_Open(devname);

	if (fd < 0) {
		PRINT("Can't open %s\n", devname);
		exit(0);
	}

	return fd;
}


