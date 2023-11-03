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

#ifndef __EXYNOS_VOICE_DEF_H__
#define __EXYNOS_VOICE_DEF_H__

// Voice

static struct pcm_config pcm_config_voicecall = {
    .channels = 2,
    .rate = 8000,
    .period_size = CAPTURE_PERIOD_SIZE_LOW_LATENCY,
    .period_count = CAPTURE_PERIOD_COUNT_LOW_LATENCY,
    .format = PCM_FORMAT_S16_LE,
};

static struct pcm_config pcm_config_voicecall_wideband = {
    .channels = 2,
    .rate = 16000,
    .period_size = CAPTURE_PERIOD_SIZE_LOW_LATENCY,
    .period_count = CAPTURE_PERIOD_COUNT_LOW_LATENCY,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_voice_sco = {
    .channels = 1,
    .rate = SCO_DEFAULT_SAMPLING_RATE,
    .period_size = SCO_PERIOD_SIZE,
    .period_count = SCO_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_voice_sco_wb = {
    .channels = 1,
    .rate = SCO_WB_SAMPLING_RATE,
    .period_size = SCO_PERIOD_SIZE,
    .period_count = SCO_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

#endif  // __EXYNOS_AUDIOHAL_DEF_H__
