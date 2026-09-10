/*
 * Copyright (C) 2017 Christopher N. Hesse <raymanfx@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TFA_H
#define TFA_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

/*
 * Amplifier audio modes for different usecases.
 */
typedef enum {
    Audio_Mode_None = -1,
    Audio_Mode_Music_Normal,
    Audio_Mode_Voice,
    Audio_Mode_Max
} tfa_mode_t;

/*
 * It doesn't really matter what this is, apparently we just need a continuous
 * chunk of memory...
 */
typedef struct {
    volatile int a1;
    volatile unsigned char a2[500];
} __attribute__((packed)) tfa_handle_t;

/*
 * Vendor functions – directly linked.
 */
#ifdef __cplusplus
extern "C" {
#endif

int tfa_device_open(tfa_handle_t *handle, int something);
int tfa_enable(tfa_handle_t *handle, int enable);

#ifdef __cplusplus
}
#endif

/*
 * TFA Amplifier device abstraction.
 */
typedef struct {
    tfa_handle_t* tfa_handle;
    pthread_mutex_t tfa_lock;
    tfa_mode_t mode;

    // for clock init
    atomic_bool initializing;
    bool clock_enabled;
    bool writing;
    pthread_t write_thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} tfa_device_t;

/*
 * Public API
 */
int tfa_power(tfa_device_t *tfa_dev, bool on);
tfa_device_t * tfa_dev_open(void);      // avoid conflict with vendor function
void tfa_device_close(tfa_device_t *tfa_dev);

#endif // TFA_H

