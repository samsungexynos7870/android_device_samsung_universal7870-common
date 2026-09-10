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

#define LOG_TAG "audio_hw_amplifier_tfa"
#define LOG_NDEBUG 0

#include <cutils/log.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <tinyalsa/asoundlib.h>

#include "tfa.h"

/*
 * Dummy write thread to provide I2S clock while amplifier is enabled.
 */
static void *write_dummy_data(void *param) {
    tfa_device_t *t = (tfa_device_t *) param;
    struct pcm *pcm = NULL;
    uint8_t *buffer = NULL;
    size_t buffer_size = 1024 * 8;
    bool first_write = true;

    struct pcm_config config = {
        .channels = 2,
        .rate = 48000,
        .period_size = 256,
        .period_count = 2,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = config.period_size * config.period_count - 1,
        .stop_threshold = UINT_MAX,
        .silence_threshold = 0,
        .avail_min = 1,
    };

    buffer = calloc(1, buffer_size);
    if (!buffer) {
        ALOGE("%s: failed to allocate buffer", __func__);
        goto exit;
    }

    while (t->initializing) {
        if (!pcm) {
            pcm = pcm_open(0, 0, PCM_OUT | PCM_MONOTONIC, &config);
            if (!pcm || !pcm_is_ready(pcm)) {
                if (pcm) {
                    ALOGE("%s: pcm_open failed: %s", __func__, pcm_get_error(pcm));
                    pcm_close(pcm);
                    pcm = NULL;
                } else {
                    ALOGE("%s: pcm_open failed (out of memory)", __func__);
                }
                usleep(50000); // 50 ms
                continue;
            }
            ALOGI("%s: PCM opened for dummy clock", __func__);
        }

        if (pcm_write(pcm, buffer, buffer_size) != 0) {
            ALOGE("%s: pcm_write failed, closing PCM", __func__);
            pcm_close(pcm);
            pcm = NULL;
            usleep(50000);
            continue;
        }

        if (first_write) {
            pthread_mutex_lock(&t->mutex);
            t->writing = true;
            pthread_cond_signal(&t->cond);
            pthread_mutex_unlock(&t->mutex);
            first_write = false;
        }
    }

    if (pcm) {
        pcm_close(pcm);
    }
    free(buffer);

exit:
    // Ensure we signal even if we never succeeded
    pthread_mutex_lock(&t->mutex);
    t->writing = true;
    pthread_cond_signal(&t->cond);
    pthread_mutex_unlock(&t->mutex);

    return NULL;
}

/*
 * Turn on the I2S clock by starting the dummy write thread.
 */
static int tfa_clock_on(tfa_device_t *tfa_dev)
{
    if (tfa_dev->clock_enabled) {
        ALOGW("%s: clocks already on", __func__);
        return 0;
    }

    tfa_dev->initializing = true;
    tfa_dev->writing = false;

    if (pthread_create(&tfa_dev->write_thread, NULL, write_dummy_data, tfa_dev) != 0) {
        ALOGE("%s: failed to create write thread", __func__);
        tfa_dev->initializing = false;
        return -1;
    }

    pthread_mutex_lock(&tfa_dev->mutex);
    while (!tfa_dev->writing) {
        pthread_cond_wait(&tfa_dev->cond, &tfa_dev->mutex);
    }
    pthread_mutex_unlock(&tfa_dev->mutex);

    if (!tfa_dev->initializing) {
        // Thread exited prematurely
        ALOGE("%s: write thread exited prematurely", __func__);
        return -1;
    }

    tfa_dev->clock_enabled = true;
    ALOGI("%s: clocks enabled", __func__);
    return 0;
}

/*
 * Turn off the I2S clock by stopping the dummy write thread.
 */
static int tfa_clock_off(tfa_device_t *tfa_dev)
{
    if (!tfa_dev->clock_enabled) {
        ALOGW("%s: clocks already off", __func__);
        return 0;
    }

    tfa_dev->initializing = false;
    pthread_join(tfa_dev->write_thread, NULL);
    tfa_dev->clock_enabled = false;
    tfa_dev->writing = false;

    ALOGI("%s: clocks disabled", __func__);
    return 0;
}

/*
 * Enables/disables the amplifier IC.
 */
int tfa_power(tfa_device_t *tfa_dev, bool on) {
    int rc = 0;

    ALOGV("%s: %s amplifier device", __func__, on ? "Enabling" : "Disabling");
    pthread_mutex_lock(&tfa_dev->tfa_lock);

    if (on) {
        // Optional: reset some handle field if needed
        if (tfa_dev->tfa_handle->a1 != 0) {
            tfa_dev->tfa_handle->a1 = 0;
        }

        rc = tfa_clock_on(tfa_dev);
        if (rc != 0) {
            ALOGE("%s: Failed to start clock", __func__);
            pthread_mutex_unlock(&tfa_dev->tfa_lock);
            return rc;
        }

        rc = tfa_enable(tfa_dev->tfa_handle, 1);
        if (rc != 0) {
            ALOGE("%s: Failed to enable amplifier", __func__);
            tfa_clock_off(tfa_dev);
        }
    } else {
        rc = tfa_enable(tfa_dev->tfa_handle, 0);
        if (rc != 0) {
            ALOGE("%s: Failed to disable amplifier", __func__);
        }
        tfa_clock_off(tfa_dev);
    }

    pthread_mutex_unlock(&tfa_dev->tfa_lock);
    return rc;
}

/*
 * Initializes the amplifier device.
 */
tfa_device_t * tfa_dev_open() {
    tfa_device_t *tfa_dev;
    int rc;

    ALOGV("Opening amplifier device");

    tfa_dev = (tfa_device_t *) malloc(sizeof(tfa_device_t));
    if (tfa_dev == NULL) {
        ALOGE("%s: Not enough memory for device struct", __func__);
        return NULL;
    }

    tfa_dev->tfa_handle = malloc(sizeof(tfa_handle_t));
    if (tfa_dev->tfa_handle == NULL) {
        ALOGE("%s: Not enough memory for tfa handle", __func__);
        free(tfa_dev);
        return NULL;
    }

    // Initialize synchronization primitives
    pthread_mutex_init(&tfa_dev->tfa_lock, NULL);
    pthread_mutex_init(&tfa_dev->mutex, NULL);
    pthread_cond_init(&tfa_dev->cond, NULL);
    tfa_dev->writing = false;
    tfa_dev->clock_enabled = false;

    // Call vendor open function directly
    rc = tfa_device_open(tfa_dev->tfa_handle, 0);
    if (rc < 0) {
        ALOGE("%s: Failed to open amplifier device", __func__);
        goto err;
    }

    // Perform initial power cycle to ensure known state
    rc = tfa_power(tfa_dev, false);
    if (rc < 0) {
        ALOGE("%s: Failed to do initial amplifier powerdown", __func__);
        goto err;
    }

    rc = tfa_power(tfa_dev, true);
    if (rc < 0) {
        ALOGE("%s: Failed to do initial amplifier powerup", __func__);
        goto err;
    }

    rc = tfa_power(tfa_dev, false);
    if (rc < 0) {
        ALOGE("%s: Failed to do final amplifier powerdown", __func__);
        goto err;
    }

    ALOGI("%s: TFA amplifier initialized successfully", __func__);
    return tfa_dev;

err:
    if (tfa_dev->tfa_handle) free(tfa_dev->tfa_handle);
    pthread_mutex_destroy(&tfa_dev->tfa_lock);
    pthread_mutex_destroy(&tfa_dev->mutex);
    pthread_cond_destroy(&tfa_dev->cond);
    free(tfa_dev);
    return NULL;
}

/*
 * De-initializes the amplifier device.
 */
void tfa_device_close(tfa_device_t *tfa_dev) {
    if (!tfa_dev) return;

    ALOGV("%s: Closing amplifier device", __func__);
    tfa_power(tfa_dev, false);

    pthread_mutex_destroy(&tfa_dev->tfa_lock);
    pthread_mutex_destroy(&tfa_dev->mutex);
    pthread_cond_destroy(&tfa_dev->cond);

    free(tfa_dev->tfa_handle);
    free(tfa_dev);
    ALOGI("%s: TFA amplifier closed", __func__);
}