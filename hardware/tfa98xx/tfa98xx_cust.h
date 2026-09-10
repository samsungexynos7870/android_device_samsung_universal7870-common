/*
 * tfa98xx_cust.h
 *
 *
 *  Created on: Oct 8, 2014
 *  Author: Customer, according to the Platform
 */

#ifndef _TFA98XX_CUST_H_
#define _TFA98XX_CUST_H_

#define LOCATION_FILES "/vendor/etc/"

#if defined(TFA_MODEL_9890)
    #define CNT_FILENAME "Tfa9890.cnt"
#elif defined(TFA_MODEL_9896)
    #define CNT_FILENAME "Tfa9896.cnt"
#else
    #error "No TFA model defined – please set TARGET_BOARD_TFA_MODEL"
#endif

#define TFA_I2CDEVICE "/dev/i2c-20"

#endif /* _TFA98XX_CUST_H_ */
