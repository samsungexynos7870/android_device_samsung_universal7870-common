/*
 * tfa98xxCalibration.h
 *
 *  Created on: Feb 5, 2013
 *      Author: wim
 */

#ifndef TFA98XXCALIBRATION_H_
#define TFA98XXCALIBRATION_H_

#include "Tfa98API.h" /* legacy API */

/*
 * run the calibration sequence
 *
 * @param device handles
 * @param device index
 * @param once=1 or always=0
 * @return Tfa98xx Errorcode
 *
 */
Tfa98xx_Error_t tfa98xxCalibration(Tfa98xx_handle_t *handlesIn, int idx, int once, int profile);

/*
 * Check MtpEx to get calibration state
 * @param device handle
 *
 */
int tfa98xxCalCheckMTPEX(Tfa98xx_handle_t handle);
/**
 * reset mtpEx bit in MTP
 */
Tfa98xx_Error_t tfa98xxCalResetMTPEX(Tfa98xx_handle_t handle);
/*
 * return current tCoef , set new value if arg!=0
 */
void tfa98xxCaltCoefToSpeaker(Tfa98xx_SpeakerParameters_t speakerBytes, float tCoef);

Tfa98xx_Error_t tfa98xxCalSetCalibrateOnce(Tfa98xx_handle_t handle);
Tfa98xx_Error_t tfa98xxCalSetCalibrationAlways(Tfa98xx_handle_t handle);
int tfa98xxCalDspSupporttCoef(Tfa98xx_handle_t handle);

/* set calibration impedance for selected idx */
Tfa98xx_Error_t tfa98xxSetCalibrationImpedance(float re0, Tfa98xx_handle_t *handlesIn);

/* Set the calibration impedance.
 * DSP reset is done in this function so that the new re0
 * value to take effect, */
enum Tfa98xx_Error tfa98xx_dsp_set_calibration_impedance(Tfa98xx_handle_t
					        handle, const unsigned char *pBytes);

/* Get the calibration result */
enum Tfa98xx_Error tfa98xx_dsp_get_calibration_impedance(Tfa98xx_handle_t handle,
                                                                float *p_re25);

#endif /* TFA98XXCALIBRATION_H_ */
