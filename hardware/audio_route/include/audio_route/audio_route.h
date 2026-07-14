/*
 * Copyright (C) 2013 The Android Open Source Project
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
 *
 * ============================================================================
 * RETROFITTED: added audio_route_force_reset_and_update_path() and
 *              extra utility functions from libaudior7870 binary.
 * ============================================================================
 */

#ifndef AUDIO_ROUTE_H
#define AUDIO_ROUTE_H

#include <stdio.h>
#include <tinyalsa/asoundlib.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* Opaque handle types */
struct audio_route;

/* ================================================================== */
/*  PCM DAI link identifiers                                           */
/* ================================================================== */

enum pcm_dai_link {
    PLAYBACK_LINK,
    PLAYBACK_LOW_LINK,
    PLAYBACK_DEEP_LINK,
    PLAYBACK_OFFLOAD_LINK,
    PLAYBACK_AUX_DIGITAL_LINK,
    PLAYBACK_DIRECT_LINK,
    PLAYBACK_INCALL_MUSIC_LINK,
    CAPTURE_LINK,
    BASEBAND_LINK,
    BASEBAND_CAPTURE_LINK,
    BLUETOOTH_LINK,
    BLUETOOTH_CAPTURE_LINK,
    VTS_CAPTURE_LINK,
    VTS_SEAMLESS_CAPTURE_LINK,
    CALL_REC_CAPTURE_LINK,
    FMRADIO_LINK,
    CAPTURE_CALLMIC_LINK,
    NUM_DAI_LINK,
};

/* ================================================================== */
/*  Core audio route API                                               */
/* ================================================================== */

/* Initialize and free the audio routes */
struct audio_route *audio_route_init(unsigned int card, const char *xml_path);
void audio_route_free(struct audio_route *ar);

/* Apply an audio route path by name */
int audio_route_apply_path(struct audio_route *ar, const char *name);

/* Apply and update mixer with audio route path by name */
int audio_route_apply_and_update_path(struct audio_route *ar,
                                      const char *name);

/* Reset an audio route path by name */
int audio_route_reset_path(struct audio_route *ar, const char *name);

/* Reset and update mixer with audio route path by name */
int audio_route_reset_and_update_path(struct audio_route *ar,
                                      const char *name);

/* Reset and update mixer with audio route path by name, forcibly */
int audio_route_force_reset_and_update_path(struct audio_route *ar,
                                            const char *name);

/* Reset the audio routes back to the initial state */
void audio_route_reset(struct audio_route *ar);

/* Update the mixer with any changed values */
int audio_route_update_mixer(struct audio_route *ar);

/* ================================================================== */
/*  Extra utility functions (from libaudior7870)                       */
/* ================================================================== */

/*
 * audio_values_apply_path - Apply parameter values from a caller-supplied
 * array to the given path. Used for direct DSP parameter writes.
 */
int audio_values_apply_path(struct audio_route *ar, const char *name,
                            int *values);

/*
 * direct_mixer_set_value - Directly set a single mixer control value
 * by name, using the global audio route handle.
 */
int direct_mixer_set_value(const char *name, int value);

/*
 * direct_mixer_set_array - Directly set a mixer control byte array
 * by name, using the global audio route handle.
 */
int direct_mixer_set_array(const char *name, char *data, size_t len);

/*
 * audio_route_get_mixer - Get the underlying mixer handle from a route.
 */
struct mixer *audio_route_get_mixer(struct audio_route *ar);

/*
 * audio_route_get_mixer_ctl - Get a mixer control by name from a route.
 */
struct mixer_ctl *audio_route_get_mixer_ctl(struct audio_route *ar,
                                             const char *name);

/*
 * get_dai_link - Retrieve the PCM device node for the given DAI link.
 * Returns the mixer card/index value parsed from the XML <pcmdai> tag,
 * or -1 if the audio_route handle is NULL.
 */
int get_dai_link(struct audio_route *ar, enum pcm_dai_link dai_link);

/*
 * get_audio_route - Return the global audio route handle, or NULL.
 */
struct audio_route *get_audio_route(void);

/*
 * audio_route_get_dsp_ctl - Get a DSP mixer control with retry logic
 * (handles DSP firmware reload).
 */
struct mixer_ctl *audio_route_get_dsp_ctl(struct audio_route *ar,
                                           const char *name);

/*
 * audio_route_missing_ctl - Return count of controls that were not found
 * during XML parsing.
 */
unsigned int audio_route_missing_ctl(struct audio_route *ar);

/*
 * path_update_mixer_state_reset - Reset a path's saved mixer state
 * from the path's setting values.
 */
int path_update_mixer_state_reset(struct audio_route *ar, const char *name);

/*
 * process_merge_bin_file - Merge two files into a DSP firmware binary.
 * out_path: optional alternate output path (NULL to skip).
 * in1, in2: input CSV/raw file paths.
 */
int process_merge_bin_file(const char *out_path, char *in1, char *in2);

/*
 * process_file - Read a text/CSV file and write as a binary chunk
 * to the given output stream.
 */
int process_file(char *filename, FILE *out, unsigned short type);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* AUDIO_ROUTE_H */
