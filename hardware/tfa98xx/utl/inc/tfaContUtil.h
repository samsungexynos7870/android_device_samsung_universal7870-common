/*
 * tfaContUtil.h
 *
 *  interface to the Tfa98xx API
 *
 *  Created on: Apr 6, 2012
 *      Author: wim
 */

#ifndef TFACONTUTIL_H_
#define TFACONTUTIL_H_

#ifdef __ANDROID__
#include <utils/Log.h>
#else
#define LOGV if (0/*tfa98xx_verbose*/) printf //TODO improve logging
#endif

#include "tfa_dsp_fw.h"
#include "tfa98xx_parameters.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(SVN_BUILD_REV) || (SVN_BUILD_REV+1 == 1)
  #undef SVN_BUILD_REV
  #define SVN_BUILD_REV 0
#endif

#define NXPTFA_APP_REV_MAJOR		2               // major API rev
#define NXPTFA_APP_REV_MINOR		10               // minor
#define NXPTFA_APP_REV_REVISION		1               //SVN_BUILD_REV   // revision

#define NXPTFA_MAXLINE			(256)       // maximum string length

/** the default target slave address */
#define TFA_I2CSLAVEBASE		(0x36)

extern unsigned char tfa98xxI2cSlave; // global for i2c access

/*
 * buffer types for setting parameters
 */
typedef enum nxpTfa98xxParamsType {
        tfa_patch_params,
        tfa_speaker_params,
        tfa_preset_params,
        tfa_config_params,
        tfa_equalizer_params,
        tfa_drc_params,
        tfa_vstep_params,
        tfa_cnt_params,
        tfa_msg_params,
        tfa_no_params,
	tfa_info_params
} nxpTfa98xxParamsType_t;

enum Tfa98xx_Error tfaVersions(Tfa98xx_handle_t *handlesIn, char *strings, int maxlength); // return all version
int tfa98xxSaveFile(Tfa98xx_handle_t handle, char *filename, nxpTfa98xxParamsType_t params);
int tfa98xxLoadFile(Tfa98xx_handle_t *handlesIn, char *filename, nxpTfa98xxParamsType_t params);
enum Tfa98xx_Error tfaGetDspFWAPIVersion(int devidx, char *strings, int maxlength);

/*
 * save dedicated device files. Depends on the file extension
 */
int tfa98xxSaveFileWrapper(Tfa98xx_handle_t handle, char *filename);

enum Tfa98xx_Error tfa98xx_verify_speaker_range(int idx, float imp[2], int spkr_count);

void tfa98xxI2cSetSlave(unsigned char);
/* hex dump of cnt */
void tfa_cnt_hexdump(void);
void tfa_cnt_dump(void);
enum Tfa98xx_Error tfa_probe_all(nxpTfaContainer_t* p_cnt);
void tfa_soft_probe_all(nxpTfaContainer_t* p_cnt);

#if defined(__cplusplus)
}  /* extern "C" */
#endif
#endif /* TFACONTUTIL_H_ */
