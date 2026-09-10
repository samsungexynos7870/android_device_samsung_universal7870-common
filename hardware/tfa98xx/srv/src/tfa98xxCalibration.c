/*
 * tfa98xxCalibration.c
 *
 *  Created on: Feb 5, 2013
 *      Author: wim
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "dbgprint.h"
#include "Tfa98API.h"
#include "tfaContUtil.h"
#include "tfa98xxCalibration.h"

#include "../../tfa/inc/tfa98xx_genregs_N1C.h"
#include "tfa_container.h"
#include "tfaOsal.h"
#include "tfa98xx_tfafieldnames.h"
#include "tfa_dsp_fw.h"

/* MTP bits */
/* one time calibration */
#define TFA_MTPOTC_POS          TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS /**/
/* calibration done executed */
#define TFA_MTPEX_POS           TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS /**/


/* for verbosity */
int tfa98xx_cal_verbose;

int tfa98xxCalDspSupporttCoef(Tfa98xx_handle_t handle)
{
	Tfa98xx_Error_t err;
	int bSupporttCoef;

	err = Tfa98xx_DspSupporttCoef(handle, &bSupporttCoef);
	assert(err == Tfa98xx_Error_Ok);

	return bSupporttCoef;
}

float tfa98xxCaltCoefFromSpeaker(Tfa98xx_SpeakerParameters_t speakerBytes)
{
	int iCoef;

	/* tCoef(A) is the last parameter of the speaker */
	iCoef = (speakerBytes[TFA1_SPEAKERPARAMETER_LENGTH-3]<<16) +
		(speakerBytes[TFA1_SPEAKERPARAMETER_LENGTH-2]<<8) +
		speakerBytes[TFA1_SPEAKERPARAMETER_LENGTH-1];

	return (float)iCoef/(1<<23);
}

/*
 *  calculate a new tCoefA and put the result into the loaded Speaker params
 */
Tfa98xx_Error_t tfa98xxCalComputeSpeakertCoefA(Tfa98xx_handle_t handle,
		Tfa98xx_SpeakerParameters_t loadedSpeaker,
		float tCoef)
{
	Tfa98xx_Error_t err;
	float tCoefA;
	float re25;
	int Tcal; /* temperature at which the calibration happened */
	int T0;
	int calibrateDone = 0;
	int nxpTfaCurrentProfile = tfa_get_swprof(handle);

	/* make sure there is no valid calibration still present */
	err = tfa98xx_dsp_get_calibration_impedance(handle, &re25);
	PRINT_ASSERT(err);
	assert(fabs(re25) < 0.1);
	PRINT(" re25 = %2.2f\n", re25);

	/* use dummy tCoefA, also eases the calculations, because tCoefB=re25 */
	tfa98xxCaltCoefToSpeaker(loadedSpeaker, 0.0f);

	// write all the files from the device list (typically spk and config)
	err = tfaContWriteFiles(handle);
	if (err) return err;

	/* write all the files from the profile list (typically preset) */
	err = tfaContWriteFilesProf(handle, nxpTfaCurrentProfile, 0); /* use volumestep 0 */
	if (err) return err;

	/* start calibration and wait for result */
	err = -TFA_SET_BF_VOLATILE(handle, SBSL, 1);
	if (err != Tfa98xx_Error_Ok) {
		return err;
	}
	if (tfa98xx_cal_verbose)
		PRINT(" ----- Configured (for tCoefA) -----\n");

	tfaRunWaitCalibration(handle, &calibrateDone);
	if (calibrateDone) {
		err = tfa98xx_dsp_get_calibration_impedance(handle, &re25);
	} else {
		re25 = 0;
	}

	err = tfa98xx_dsp_read_mem(handle, 232, 1, &Tcal);
	if (err != Tfa98xx_Error_Ok) {
		return err;
	}
	PRINT("Calibration value is %2.2f ohm @ %d degrees\n", re25, Tcal);

	/* calculate the tCoefA */
	T0 = 25; /* definition of temperature for Re0 */
	tCoefA = tCoef * re25 / (tCoef * (Tcal - T0)+1); /* TODO: need Rapp influence */
	PRINT(" Final tCoefA %1.5f\n", tCoefA);

	/* update the speaker model */
	tfa98xxCaltCoefToSpeaker(loadedSpeaker, tCoefA);

	/* !!! the host needs to save this loadedSpeaker as it is needed after the next cold boot !!! */

	return err;
}

void tfa98xxCaltCoefToSpeaker(Tfa98xx_SpeakerParameters_t speakerBytes, float tCoef)
{
	int iCoef;

	iCoef =(int)(tCoef*(1<<23));

	speakerBytes[TFA1_SPEAKERPARAMETER_LENGTH-3] = (iCoef>>16)&0xFF;
	speakerBytes[TFA1_SPEAKERPARAMETER_LENGTH-2] = (iCoef>>8)&0xFF;
	speakerBytes[TFA1_SPEAKERPARAMETER_LENGTH-1] = (iCoef)&0xFF;
}

/*
 * set OTC
 */
Tfa98xx_Error_t tfa98xxCalSetCalibrateOnce(Tfa98xx_handle_t handle)
{
	Tfa98xx_Error_t err;

	err = tfa98xx_set_mtp(handle, 1<<TFA_MTPOTC_POS, 1<<TFA_MTPOTC_POS); /* val mask */

	return err;
}
/*
 * clear OTC
 */
Tfa98xx_Error_t tfa98xxCalSetCalibrationAlways(Tfa98xx_handle_t handle){
	Tfa98xx_Error_t err;

	err = tfa98xx_set_mtp(handle, 0, (1<<TFA_MTPEX_POS) | (1<<TFA_MTPOTC_POS));/* val mask */
	if (err)
		PRINT("MTP write failed\n");

	return err;
}

/*
 *
 */
Tfa98xx_Error_t tfa98xxCalibration(Tfa98xx_handle_t *handlesIn, int idx, int once, int profile)
{
	uint8_t *speakerbuffer = NULL;
	Tfa98xx_Error_t err = Tfa98xx_Error_Ok;
	float tCoef = 0, imp[2] = { 0 };
	int spkr_count = 0, cal_profile = -1;

	PRINT("calibrate %s\n", once ? "once" : "always" );

	/* Search if there is a calibration profile
	* Only if the user did not give a specific profile and coldstart
	*/
	if (profile == 0) {
		cal_profile = tfaContGetCalProfile(idx);
		if (cal_profile >= 0)
			profile = cal_profile;
		else
			profile = 0;
	}

	err = tfa98xx_supported_speakers(handlesIn[idx], &spkr_count);
	if (err) {
		PRINT("Getting the number of supported speakers failed");
		return err;
	}

	/* Do a full startup */
	if (tfaRunStartup(handlesIn[idx], profile))
		return err;

	/* force cold boot to set ACS to 1 */
	if (tfaRunColdboot(handlesIn[idx], 1))
		return err;

	if (once) {
		/* Set MTPOTC */
		tfa98xxCalSetCalibrateOnce(handlesIn[idx]);
	} else {
		/* Clear MTPOTC */
		tfa98xxCalSetCalibrationAlways(handlesIn[idx]);
	}

	/* Only for tfa1 (tfa9887 specific) */
	if ((tfa98xxCalCheckMTPEX(handlesIn[idx]) == 0) && (!tfa98xxCalDspSupporttCoef(handlesIn[idx]))) {
		nxpTfaSpeakerFile_t *spkFile;

		/* ensure no audio during special calibration */
		err = Tfa98xx_SetMute(handlesIn[idx], Tfa98xx_Mute_Digital);
		assert(err == Tfa98xx_Error_Ok);

		PRINT(" 2 step calibration\n");

		spkFile = (nxpTfaSpeakerFile_t *)tfacont_getfiledata(idx, 0, speakerHdr);

		if (spkFile == NULL) {
			PRINT("No speaker data found\n");
			return Tfa98xx_Error_Bad_Parameter;
		}
		speakerbuffer = spkFile->data;

		tCoef = tfa98xxCaltCoefFromSpeaker(speakerbuffer);
		PRINT(" tCoef = %1.5f\n", tCoef);

		err = tfa98xxCalComputeSpeakertCoefA(handlesIn[idx], speakerbuffer, tCoef);
		assert(err == Tfa98xx_Error_Ok);

		/* if we were in one-time calibration (OTC) mode, clear the calibration results
		from MTP so next time 2nd calibartion step can start. */
		tfa98xxCalResetMTPEX(handlesIn[idx]);

		/* force recalibration now with correct tCoefA */
		Tfa98xx_SetMute(handlesIn[idx], Tfa98xx_Mute_Off);
		tfaRunColdStartup(handlesIn[idx], profile);
	}

	/* if CF is in bypass then return here */
	if (TFA_GET_BF(handlesIn[idx], CFE) == 0)
		return err;

	/* Load patch and delay tables */
	if (tfaRunStartDSP(handlesIn[idx]))
		return err;

	/* DSP is running now */
	/* write all the files from the device list (speaker, vstep and all messages) */
	if (tfaContWriteFiles(handlesIn[idx]))
		return err;

	/* write all the files from the profile list (typically preset) */
	if (tfaContWriteFilesProf(handlesIn[idx], profile, 0)) /* use volumestep 0 */
		return err;

	/* Check if CF is not in bypass */
	if (TFA_GET_BF(handlesIn[idx], CFE) == 0) {
		PRINT("It is not possible to calibrate with CF in bypass! \n");
		return Tfa98xx_Error_DSP_not_running;
	}

	if (tfa98xxCalCheckMTPEX(handlesIn[idx]) == 1) {
		PRINT("DSP already calibrated.\n Calibration skipped, previous results loaded.\n");
		err = tfa_dsp_get_calibration_impedance(handlesIn[idx]);
#ifdef __KERNEL__ /* Necessary otherwise we are thrown out of operating mode in kernel (because of internal clock) */
	    if((strstr(tfaContProfileName(handle, profile), ".cal") == NULL) && (tfa98xx_dev_family(handle) == 2))
        {
            TFA_SET_BF_VOLATILE(handle, SBSL, 1);
        }
        else if (tfa98xx_dev_family(handle) != 2)
#endif
            TFA_SET_BF_VOLATILE(handlesIn[idx], SBSL, 1);
	} else {
		/* Save the current profile */
		tfa_set_swprof(handlesIn[idx], (unsigned short)profile);
		/* calibrate */
		err = tfaRunSpeakerCalibration(handlesIn[idx], profile);
		if (err) return err;
	}

	imp[0] = (float)tfa_get_calibration_info(handlesIn[idx], 0);
	imp[1] = (float)tfa_get_calibration_info(handlesIn[idx], 1);
	imp[0] = imp[0]/1000;
	imp[1] = imp[1]/1000;

	if (spkr_count == 1)
		PRINT("Calibration value is: %2.2f ohm\n", imp[0]);
	else
		PRINT("Calibration value is: %2.2f %2.2f ohm\n", imp[0], imp[1]);

	/* Check speaker range */
	err = tfa98xx_verify_speaker_range(idx, imp, spkr_count);

	/* Unmute after calibration */
	Tfa98xx_SetMute(handlesIn[idx], Tfa98xx_Mute_Off);

	if(tCoef != 0) {
		if (!tfa98xxCalDspSupporttCoef(handlesIn[idx]))
			tfa98xxCaltCoefToSpeaker(speakerbuffer, tCoef);
	}

	/* Save the current profile */
	tfa_set_swprof(handlesIn[idx], (unsigned short)profile);

	/* After loading calibration profile we need to load acoustic shock (safe) profile */
	if (cal_profile >= 0) {
		profile = 0;
		PRINT("Loading %s profile! \n", tfaContProfileName(idx, profile));
		err = tfaContWriteProfile(idx, profile, 0);
	}

	/* Always search and apply filters after a startup */
	err = tfa_set_filters(idx, profile);

	/* Save the current profile */
	tfa_set_swprof(handlesIn[idx], (unsigned short)profile);

	return err;
}
/*
 *	MTPEX is 1, calibration is done
 *
 */
int tfa98xxCalCheckMTPEX(Tfa98xx_handle_t handle)
{
	/* MTPEX is 1, calibration is done */
	return TFA_GET_BF(handle, MTPEX) == 1 ? 1 : 0;
}
/*
 *
 */
Tfa98xx_Error_t tfa98xxCalResetMTPEX(Tfa98xx_handle_t handle)
{
	Tfa98xx_Error_t err;

	err = tfa98xx_set_mtp(handle, 0, 1<<TFA_MTPEX_POS);

	return err;
}

/*
 * Set the re0 in the MTP. The value must be in range [4-10] ohms
 * Note that this only operates on the selected device
 */
Tfa98xx_Error_t tfa98xxSetCalibrationImpedance( float re0, Tfa98xx_handle_t *handlesIn )
{
	Tfa98xx_Error_t err = Tfa98xx_Error_Not_Supported;
	int re0_xmem = 0;

	unsigned char bytes[3];
	if ( re0 > 10.0 || re0 < 4.0 ) {
		err = Tfa98xx_Error_Bad_Parameter;
	} else {
		/* multiply with 2^14 */
		re0_xmem = (int)(re0 * (1<<14));
		/* convert to bytes */
		tfa98xx_convert_data2bytes(1, (int *) &re0_xmem, bytes);
		/* call the TFA layer, which is also expected to reset the DSP */
		err = tfa98xx_dsp_set_calibration_impedance(handlesIn[0], bytes);
	}

	return err;
}

enum Tfa98xx_Error
tfa98xx_dsp_set_calibration_impedance(Tfa98xx_handle_t handle, const unsigned char *bytes)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Not_Supported;

	/* Set new Re0. */
	error = tfa_dsp_cmd_id_write(handle, MODULE_SETRE,
				SB_PARAM_SET_RE0, 3, bytes);

	if (error == Tfa98xx_Error_Ok) {
		/* reset the DSP to take the newly set re0*/
		error = tfa98xx_dsp_reset(handle, 1);
		if (error == Tfa98xx_Error_Ok)
			error = tfa98xx_dsp_reset(handle, 0);
	}
	return error;
}

enum Tfa98xx_Error
tfa98xx_dsp_get_calibration_impedance(Tfa98xx_handle_t handle, float *p_re25)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int spkr_count, i;

	/* Get number of speakers */
	error = tfa98xx_supported_speakers(handle, &spkr_count);
	if (error == Tfa98xx_Error_Ok) {
		/* Get calibration values (from MTP or Speakerboost) */
		error = tfa_dsp_get_calibration_impedance(handle);
		for(i=0; i<spkr_count; i++) {
			p_re25[i] = (float)tfa_get_calibration_info(handle, i) / 1000;
		}
	}

	return error;
}
