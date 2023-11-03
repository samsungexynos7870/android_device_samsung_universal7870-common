/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef __EXYNOS_AUDIOHAL_DEF_H__
#define __EXYNOS_AUDIOHAL_DEF_H__

#include <system/audio.h>

// audio hw

/* TODO: the following PCM device profiles could be read from a config file */
static struct pcm_device_profile pcm_device_playback = {
    .config = {
        .channels = PLAYBACK_DEFAULT_CHANNEL_COUNT,
        .rate = PLAYBACK_DEFAULT_SAMPLING_RATE,
        .period_size = PLAYBACK_PERIOD_SIZE,
        .period_count = PLAYBACK_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = PLAYBACK_START_THRESHOLD(PLAYBACK_PERIOD_SIZE, PLAYBACK_PERIOD_COUNT),
        .stop_threshold = PLAYBACK_STOP_THRESHOLD(PLAYBACK_PERIOD_SIZE, PLAYBACK_PERIOD_COUNT),
        .silence_threshold = 0,
        .silence_size = UINT_MAX,
        .avail_min = PLAYBACK_AVAILABLE_MIN,
    },
    .card = SOUND_CARD,
    .id = SOUND_PLAYBACK_DEVICE,
    .type = PCM_PLAYBACK,
    .devices = AUDIO_DEVICE_OUT_WIRED_HEADSET|AUDIO_DEVICE_OUT_WIRED_HEADPHONE|
               AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_EARPIECE|AUDIO_DEVICE_OUT_ALL_SCO,
};

static struct pcm_device_profile pcm_device_deep_buffer = {
    .config = {
        .channels = PLAYBACK_DEFAULT_CHANNEL_COUNT,
        .rate = DEEP_BUFFER_OUTPUT_SAMPLING_RATE,
        .period_size = DEEP_BUFFER_OUTPUT_PERIOD_SIZE,
        .period_count = DEEP_BUFFER_OUTPUT_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = DEEP_BUFFER_OUTPUT_PERIOD_SIZE / 4,
        .stop_threshold = INT_MAX,
        .avail_min = DEEP_BUFFER_OUTPUT_PERIOD_SIZE / 4,
    },
    .card = SOUND_CARD,
    .id = SOUND_DEEP_BUFFER_DEVICE,
    .type = PCM_PLAYBACK,
    .devices = AUDIO_DEVICE_OUT_WIRED_HEADSET|AUDIO_DEVICE_OUT_WIRED_HEADPHONE|
               AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_EARPIECE|AUDIO_DEVICE_OUT_ALL_SCO,
};

static struct pcm_device_profile pcm_device_capture = {
    .config = {
        .channels = CAPTURE_DEFAULT_CHANNEL_COUNT,
        .rate = CAPTURE_DEFAULT_SAMPLING_RATE,
        .period_size = CAPTURE_PERIOD_SIZE,
        .period_count = CAPTURE_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = CAPTURE_START_THRESHOLD,
        .stop_threshold = 0,
        .silence_threshold = 0,
        .avail_min = 0,
    },
    .card = SOUND_CARD,
    .id = SOUND_CAPTURE_DEVICE,
    .type = PCM_CAPTURE,
    .devices = AUDIO_DEVICE_IN_BUILTIN_MIC|AUDIO_DEVICE_IN_WIRED_HEADSET|AUDIO_DEVICE_IN_BACK_MIC|AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET,
};

static struct pcm_device_profile pcm_device_capture_low_latency = {
    .config = {
        .channels = CAPTURE_DEFAULT_CHANNEL_COUNT,
        .rate = CAPTURE_DEFAULT_SAMPLING_RATE,
        .period_size = CAPTURE_PERIOD_SIZE_LOW_LATENCY,
        .period_count = CAPTURE_PERIOD_COUNT_LOW_LATENCY,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = CAPTURE_START_THRESHOLD,
        .stop_threshold = 0,
        .silence_threshold = 0,
        .avail_min = 0,
    },
    .card = SOUND_CARD,
    .id = SOUND_CAPTURE_DEVICE,
    .type = PCM_CAPTURE_LOW_LATENCY,
    .devices = AUDIO_DEVICE_IN_BUILTIN_MIC|AUDIO_DEVICE_IN_WIRED_HEADSET|AUDIO_DEVICE_IN_BACK_MIC|AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET,
};

static struct pcm_device_profile * const pcm_devices[] = {
    &pcm_device_playback,
    &pcm_device_capture,
    &pcm_device_capture_low_latency,
    NULL,
};

static const char * const use_case_table[AUDIO_USECASE_MAX] = {
    [USECASE_AUDIO_PLAYBACK] = "playback",
    [USECASE_AUDIO_PLAYBACK_MULTI_CH] = "playback multi-channel",
    [USECASE_AUDIO_PLAYBACK_OFFLOAD] = "compress-offload-playback",
    [USECASE_AUDIO_PLAYBACK_DEEP_BUFFER] = "playback deep-buffer",
    [USECASE_AUDIO_CAPTURE] = "capture",
    [USECASE_VOICE_CALL] = "voice-call",
};

#define STRING_TO_ENUM(string) { #string, string }

static unsigned int audio_device_ref_count;

struct string_to_enum {
    const char *name;
    uint32_t value;
};

static const struct string_to_enum out_channels_name_to_enum_table[] = {
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_STEREO),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_5POINT1),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_7POINT1),
};

static struct audio_device *adev = NULL;

#endif  // __EXYNOS_AUDIOHAL_DEF_H__
