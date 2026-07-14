/*
 * Copyright (C) 2013 The Android Open Source Project
 * Inspired by TinyHW, written by Mark Brown at Wolfson Micro
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
 * ENHANCEMENTS for Samsung Exynos7870 platform:
 *   - active_count reference tracking in mixer_state
 *   - DIRECTION_REVERSE_RESET (force reset) support
 *   - audio_route_force_reset_and_update_path() public API
 *   - audio_route_get_dsp_ctl(), audio_values_apply_path(),
 *   - vendor mixer_paths.xml support
 *     direct_mixer_set_value(), direct_mixer_set_array(),
 *     audio_route_get_mixer(), audio_route_get_mixer_ctl(),
 *     get_dai_link(), get_audio_route(), audio_route_missing_ctl(),
 *     update_mixer_state(), path_update_mixer_state_reset(),
 *     process_merge_bin_file(), process_file()
 * ============================================================================
 */

#define LOG_TAG "audio_route"
/*#define LOG_NDEBUG 0*/

#include <errno.h>
#include <expat.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <cutils/properties.h>
#include <log/log.h>

#include <tinyalsa/asoundlib.h>
#include "include/audio_route/audio_route.h"

//#include "audio_route.h"

#define BUF_SIZE 1024
#define MIXER_XML_PATH "/vendor/etc/mixer_paths.xml"
#define INITIAL_MIXER_PATH_SIZE 8

/* Retry limit when a mixer control is not found (DSP reload) */
#define CTL_RELOAD_RETRY_MAX 20
#define CTL_RELOAD_DELAY_US 10000

enum update_direction {
    DIRECTION_FORWARD,
    DIRECTION_REVERSE,
    DIRECTION_REVERSE_RESET
};

union ctl_values {
    int *enumerated;
    long *integer;
    void *ptr;
    unsigned char *bytes;
};

struct mixer_state {
    struct mixer_ctl *ctl;
    unsigned int num_values;
    union ctl_values old_value;
    union ctl_values new_value;
    union ctl_values reset_value;
    unsigned int active_count;
};

struct mixer_setting {
    unsigned int ctl_index;
    unsigned int num_values;
    unsigned int type;
    union ctl_values value;
};

struct mixer_value {
    unsigned int ctl_index;
    int index;
    long value;
};

struct mixer_path {
    char *name;
    unsigned int size;
    unsigned int length;
    struct mixer_setting *setting;
};

struct audio_route {
    struct mixer *mixer;
    unsigned int num_mixer_ctls;
    struct mixer_state *mixer_state;

    unsigned int mixer_path_size;
    unsigned int num_mixer_paths;
    struct mixer_path *mixer_path;

    unsigned int missing_ctl_count;

    /* PCM DAI link card/device values, populated from <pcmdai> XML tags */
    int dai_link[NUM_DAI_LINK];

    char *include_path;
};

struct config_parse_state {
    struct audio_route *ar;
    struct mixer_path *path;
    int level;
};

/* Global audio route handle, used by direct-mixer helpers */
static struct audio_route *g_ar = NULL;

/* Ship-mode flag: when set, suppresses verbose logging of mixer ctl values */
static bool g_ship_mode = false;

/* ------------------------------------------------------------------ */
/*  PCM DAI link name-to-enum mapping                                  */
/* ------------------------------------------------------------------ */

static const char *dai_link_name[NUM_DAI_LINK] = {
    [PLAYBACK_LINK]              = "playback_link",
    [PLAYBACK_LOW_LINK]          = "playback_low_link",
    [PLAYBACK_DEEP_LINK]         = "playback_deep_link",
    [PLAYBACK_OFFLOAD_LINK]      = "playback_offload_link",
    [PLAYBACK_AUX_DIGITAL_LINK]  = "playback_aux_digital_link",
    [PLAYBACK_DIRECT_LINK]       = "playback_direct_link",
    [PLAYBACK_INCALL_MUSIC_LINK] = "playback_incall_music_link",
    [CAPTURE_LINK]               = "capture_link",
    [BASEBAND_LINK]              = "baseband_link",
    [BASEBAND_CAPTURE_LINK]      = "baseband_capture_link",
    [BLUETOOTH_LINK]             = "bluetooth_link",
    [BLUETOOTH_CAPTURE_LINK]     = "bluetooth_capture_link",
    [VTS_CAPTURE_LINK]           = "vts_capture_link",
    [VTS_SEAMLESS_CAPTURE_LINK]  = "vts_seamless_capture_link",
    [CALL_REC_CAPTURE_LINK]      = "call_rec_capture_link",
    [FMRADIO_LINK]               = "fmradio_link",
    [CAPTURE_CALLMIC_LINK]       = "capture_callmic_link",
};

/* ------------------------------------------------------------------ */
/*  Forward declarations of internal helpers                          */
/* ------------------------------------------------------------------ */
static struct mixer_path *path_get_by_name(struct audio_route *ar,
                                           const char *name);
static int path_apply(struct audio_route *ar, struct mixer_path *path);
static int path_reset(struct audio_route *ar, struct mixer_path *path);
static int audio_route_update_path(struct audio_route *ar,
                                   const char *name, int direction);
static int update_mixer_state(struct audio_route *ar);

/* ------------------------------------------------------------------ */
/*  Type helpers                                                       */
/* ------------------------------------------------------------------ */

static bool is_supported_ctl_type(enum mixer_ctl_type type)
{
    switch (type) {
    case MIXER_CTL_TYPE_BOOL:
    case MIXER_CTL_TYPE_INT:
    case MIXER_CTL_TYPE_ENUM:
    case MIXER_CTL_TYPE_BYTE:
        return true;
    default:
        return false;
    }
}

/* as they match in alsa */
static size_t sizeof_ctl_type(enum mixer_ctl_type type)
{
    switch (type) {
    case MIXER_CTL_TYPE_BOOL:
    case MIXER_CTL_TYPE_INT:
        return sizeof(long);
    case MIXER_CTL_TYPE_ENUM:
        return sizeof(int);
    case MIXER_CTL_TYPE_BYTE:
        return sizeof(unsigned char);
    case MIXER_CTL_TYPE_INT64:
    case MIXER_CTL_TYPE_IEC958:
    case MIXER_CTL_TYPE_UNKNOWN:
    default:
        LOG_ALWAYS_FATAL("Unsupported mixer ctl type: %d, check type before calling",
                         (int)type);
        return 0;
    }
}

static inline struct mixer_ctl *index_to_ctl(struct audio_route *ar,
                                             unsigned int ctl_index)
{
    return ar->mixer_state[ctl_index].ctl;
}

/* ------------------------------------------------------------------ */
/*  Path management                                                    */
/* ------------------------------------------------------------------ */

static void path_free(struct audio_route *ar)
{
    unsigned int i;

    for (i = 0; i < ar->num_mixer_paths; i++) {
        free(ar->mixer_path[i].name);
        if (ar->mixer_path[i].setting) {
            size_t j;
            for (j = 0; j < ar->mixer_path[i].length; j++) {
                free(ar->mixer_path[i].setting[j].value.ptr);
            }
            free(ar->mixer_path[i].setting);
            ar->mixer_path[i].size = 0;
            ar->mixer_path[i].length = 0;
            ar->mixer_path[i].setting = NULL;
        }
    }
    free(ar->mixer_path);
    ar->mixer_path = NULL;
    ar->mixer_path_size = 0;
    ar->num_mixer_paths = 0;
}

static struct mixer_path *path_get_by_name(struct audio_route *ar,
                                           const char *name)
{
    unsigned int i;

    for (i = 0; i < ar->num_mixer_paths; i++)
        if (strcmp(ar->mixer_path[i].name, name) == 0)
            return &ar->mixer_path[i];

    return NULL;
}

static struct mixer_path *path_create(struct audio_route *ar, const char *name)
{
    struct mixer_path *new_mixer_path = NULL;

    if (path_get_by_name(ar, name)) {
        ALOGE("Path name '%s' already exists", name);
        return NULL;
    }

    /* check if we need to allocate more space for mixer paths */
    if (ar->mixer_path_size <= ar->num_mixer_paths) {
        if (ar->mixer_path_size == 0)
            ar->mixer_path_size = INITIAL_MIXER_PATH_SIZE;
        else
            ar->mixer_path_size *= 2;

        new_mixer_path = realloc(ar->mixer_path,
                                 ar->mixer_path_size * sizeof(struct mixer_path));
        if (new_mixer_path == NULL) {
            ALOGE("Unable to allocate more paths");
            return NULL;
        } else {
            ar->mixer_path = new_mixer_path;
        }
    }

    /* initialise the new mixer path */
    ar->mixer_path[ar->num_mixer_paths].name = strdup(name);
    ar->mixer_path[ar->num_mixer_paths].size = 0;
    ar->mixer_path[ar->num_mixer_paths].length = 0;
    ar->mixer_path[ar->num_mixer_paths].setting = NULL;

    /* return the mixer path just added, then increment number of them */
    return &ar->mixer_path[ar->num_mixer_paths++];
}

static int find_ctl_index_in_path(struct mixer_path *path,
                                  unsigned int ctl_index)
{
    unsigned int i;

    for (i = 0; i < path->length; i++)
        if (path->setting[i].ctl_index == ctl_index)
            return i;

    return -1;
}

static int alloc_path_setting(struct mixer_path *path)
{
    struct mixer_setting *new_path_setting;
    int path_index;

    /* check if we need to allocate more space for path settings */
    if (path->size <= path->length) {
        if (path->size == 0)
            path->size = INITIAL_MIXER_PATH_SIZE;
        else
            path->size *= 2;

        new_path_setting = realloc(path->setting,
                                   path->size * sizeof(struct mixer_setting));
        if (new_path_setting == NULL) {
            ALOGE("Unable to allocate more path settings");
            return -1;
        } else {
            path->setting = new_path_setting;
        }
    }

    path_index = path->length;
    path->length++;

    return path_index;
}

static int path_add_setting(struct audio_route *ar, struct mixer_path *path,
                            struct mixer_setting *setting)
{
    int path_index;

    if (find_ctl_index_in_path(path, setting->ctl_index) != -1) {
        struct mixer_ctl *ctl = index_to_ctl(ar, setting->ctl_index);

        ALOGE("Control '%s' already exists in path '%s'",
              mixer_ctl_get_name(ctl), path->name);
        return -1;
    }

    if (!is_supported_ctl_type(setting->type)) {
        ALOGE("unsupported type %d", (int)setting->type);
        return -1;
    }

    path_index = alloc_path_setting(path);
    if (path_index < 0)
        return -1;

    path->setting[path_index].ctl_index = setting->ctl_index;
    path->setting[path_index].type = setting->type;
    path->setting[path_index].num_values = setting->num_values;

    size_t value_sz = sizeof_ctl_type(setting->type);

    path->setting[path_index].value.ptr = calloc(setting->num_values, value_sz);
    /* copy all values */
    memcpy(path->setting[path_index].value.ptr, setting->value.ptr,
           setting->num_values * value_sz);

    return 0;
}

static int path_add_value(struct audio_route *ar, struct mixer_path *path,
                          struct mixer_value *mixer_value)
{
    unsigned int i;
    int path_index;
    unsigned int num_values;
    struct mixer_ctl *ctl;

    /* Check that mixer value index is within range */
    ctl = index_to_ctl(ar, mixer_value->ctl_index);
    num_values = mixer_ctl_get_num_values(ctl);
    if (mixer_value->index >= (int)num_values) {
        ALOGE("mixer index %d is out of range for '%s'", mixer_value->index,
              mixer_ctl_get_name(ctl));
        return -1;
    }

    path_index = find_ctl_index_in_path(path, mixer_value->ctl_index);
    if (path_index < 0) {
        /* New path */
        enum mixer_ctl_type type = mixer_ctl_get_type(ctl);
        if (!is_supported_ctl_type(type)) {
            ALOGE("unsupported type %d", (int)type);
            return -1;
        }
        path_index = alloc_path_setting(path);
        if (path_index < 0)
            return -1;

        /* initialise the new path setting */
        path->setting[path_index].ctl_index = mixer_value->ctl_index;
        path->setting[path_index].num_values = num_values;
        path->setting[path_index].type = type;

        size_t value_sz = sizeof_ctl_type(type);
        path->setting[path_index].value.ptr = calloc(num_values, value_sz);
        if (path->setting[path_index].type == MIXER_CTL_TYPE_BYTE)
            path->setting[path_index].value.bytes[0] = mixer_value->value;
        else if (path->setting[path_index].type == MIXER_CTL_TYPE_ENUM)
            path->setting[path_index].value.enumerated[0] = mixer_value->value;
        else
            path->setting[path_index].value.integer[0] = mixer_value->value;
    }

    if (mixer_value->index == -1) {
        /* set all values the same */
        if (path->setting[path_index].type == MIXER_CTL_TYPE_BYTE) {
            for (i = 0; i < num_values; i++)
                path->setting[path_index].value.bytes[i] = mixer_value->value;
        } else if (path->setting[path_index].type == MIXER_CTL_TYPE_ENUM) {
            for (i = 0; i < num_values; i++)
                path->setting[path_index].value.enumerated[i] = mixer_value->value;
        } else {
            for (i = 0; i < num_values; i++)
                path->setting[path_index].value.integer[i] = mixer_value->value;
        }
    } else {
        /* set only one value */
        if (path->setting[path_index].type == MIXER_CTL_TYPE_BYTE)
            path->setting[path_index].value.bytes[mixer_value->index] = mixer_value->value;
        else if (path->setting[path_index].type == MIXER_CTL_TYPE_ENUM)
            path->setting[path_index].value.enumerated[mixer_value->index] = mixer_value->value;
        else
            path->setting[path_index].value.integer[mixer_value->index] = mixer_value->value;
    }

    return 0;
}

static int path_add_path(struct audio_route *ar, struct mixer_path *path,
                         struct mixer_path *sub_path)
{
    unsigned int i;

    for (i = 0; i < sub_path->length; i++)
        if (path_add_setting(ar, path, &sub_path->setting[i]) < 0)
            return -1;

    return 0;
}

static int path_apply(struct audio_route *ar, struct mixer_path *path)
{
    unsigned int i;
    unsigned int ctl_index;
    struct mixer_ctl *ctl;
    enum mixer_ctl_type type;

    ALOGD("Apply path: %s", path->name != NULL ? path->name : "none");
    for (i = 0; i < path->length; i++) {
        ctl_index = path->setting[i].ctl_index;
        ctl = index_to_ctl(ar, ctl_index);
        type = mixer_ctl_get_type(ctl);
        if (!is_supported_ctl_type(type))
            continue;
        size_t value_sz = sizeof_ctl_type(type);
        memcpy(ar->mixer_state[ctl_index].new_value.ptr,
               path->setting[i].value.ptr,
               path->setting[i].num_values * value_sz);
    }

    return 0;
}

static int path_reset(struct audio_route *ar, struct mixer_path *path)
{
    unsigned int i;
    unsigned int ctl_index;
    struct mixer_ctl *ctl;
    enum mixer_ctl_type type;

    ALOGV("Reset path: %s", path->name != NULL ? path->name : "none");
    for (i = 0; i < path->length; i++) {
        ctl_index = path->setting[i].ctl_index;
        ctl = index_to_ctl(ar, ctl_index);
        type = mixer_ctl_get_type(ctl);
        if (!is_supported_ctl_type(type))
            continue;
        size_t value_sz = sizeof_ctl_type(type);
        /* reset the value(s) */
        memcpy(ar->mixer_state[ctl_index].new_value.ptr,
               ar->mixer_state[ctl_index].reset_value.ptr,
               ar->mixer_state[ctl_index].num_values * value_sz);
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Mixer helper                                                       */
/* ------------------------------------------------------------------ */

static int mixer_enum_string_to_value(struct mixer_ctl *ctl, const char *string)
{
    unsigned int i;
    unsigned int num_values = mixer_ctl_get_num_enums(ctl);

    if (string == NULL) {
        ALOGE("NULL enum value string passed to mixer_enum_string_to_value() for ctl %s",
              mixer_ctl_get_name(ctl));
        return 0;
    }

    /* Search the enum strings for a particular one */
    for (i = 0; i < num_values; i++) {
        if (strcmp(mixer_ctl_get_enum_string(ctl, i), string) == 0)
            break;
    }
    if (i == num_values) {
        ALOGE("unknown enum value string %s for ctl %s",
              string, mixer_ctl_get_name(ctl));
        return 0;
    }
    return i;
}

/* ------------------------------------------------------------------ */
/*  Update mixer state when ctl addresses change (DSP reload)         */
/* ------------------------------------------------------------------ */

static int update_mixer_state(struct audio_route *ar)
{
    struct mixer_ctl *ctl;
    struct mixer_ctl *check_ctl;
    unsigned int i;

    ctl = mixer_get_ctl(ar->mixer, 0);
    if (!ctl) {
        ALOGE("update_mixer_state: mixer_get_ctl error.");
        return -1;
    }

    check_ctl = mixer_get_ctl(ar->mixer, 0);
    if (ctl != check_ctl) {
        ALOGD("Change mixer_ctl address, update mixer_state list");
        if (ar->num_mixer_ctls) {
            for (i = 0; i < ar->num_mixer_ctls; i++)
                ar->mixer_state[i].ctl = mixer_get_ctl(ar->mixer, i);
        }
    }

    return mixer_update(ar->mixer);
}

/* ------------------------------------------------------------------ */
/*  XML parser callbacks                                               */
/* ------------------------------------------------------------------ */

static void start_tag(void *data, const XML_Char *tag_name,
                      const XML_Char **attr)
{
    const XML_Char *attr_name = NULL;
    const XML_Char *attr_id = NULL;
    const XML_Char *attr_value = NULL;
    struct config_parse_state *state = data;
    struct audio_route *ar = state->ar;
    unsigned int i;
    unsigned int ctl_index;
    struct mixer_ctl *ctl;
    long value;
    unsigned int id;
    struct mixer_value mixer_value;
    enum mixer_ctl_type type;

    /* Get name, id and value attributes (these may be empty) */
    for (i = 0; attr[i]; i += 2) {
        if (strcmp(attr[i], "name") == 0)
            attr_name = attr[i + 1];
        if (strcmp(attr[i], "id") == 0)
            attr_id = attr[i + 1];
        else if (strcmp(attr[i], "value") == 0)
            attr_value = attr[i + 1];
    }

    /* Look at tags */
    if (strcmp(tag_name, "path") == 0) {
        if (attr_name == NULL) {
            ALOGE("Unnamed path!");
        } else {
            if (state->level == 1) {
                /* top level path: create and stash the path */
                state->path = path_create(ar, (char *)attr_name);
                if (state->path == NULL)
                    ALOGE("path created failed, please check the path if existed");
            } else {
                /* nested path */
                struct mixer_path *sub_path = path_get_by_name(ar, attr_name);
                if (!sub_path) {
                    ALOGE("unable to find sub path '%s'", attr_name);
                } else if (state->path != NULL) {
                    path_add_path(ar, state->path, sub_path);
                }
            }
        }
    }

    else if (strcmp(tag_name, "ctl") == 0) {
        /* Obtain the mixer ctl and value */
        ctl = mixer_get_ctl_by_name(ar->mixer, attr_name);
        if (ctl == NULL) {
            ALOGE("Control '%s' doesn't exist - skipping", attr_name);
            ar->missing_ctl_count++;
            goto done;
        }

        switch (mixer_ctl_get_type(ctl)) {
        case MIXER_CTL_TYPE_BOOL:
        case MIXER_CTL_TYPE_INT:
            value = strtol((char *)attr_value, NULL, 0);
            break;
        case MIXER_CTL_TYPE_BYTE:
            value = (unsigned char) strtol((char *)attr_value, NULL, 16);
            break;
        case MIXER_CTL_TYPE_ENUM:
            value = mixer_enum_string_to_value(ctl, (char *)attr_value);
            break;
        default:
            value = 0;
            break;
        }

        /* locate the mixer ctl in the list */
        for (ctl_index = 0; ctl_index < ar->num_mixer_ctls; ctl_index++) {
            if (ar->mixer_state[ctl_index].ctl == ctl)
                break;
        }

        if (state->level == 1) {
            /* top level ctl (initial setting) */

            type = mixer_ctl_get_type(ctl);
            if (is_supported_ctl_type(type)) {
                /* apply the new value */
                if (attr_id) {
                    /* set only one value */
                    id = atoi((char *)attr_id);
                    if (id < ar->mixer_state[ctl_index].num_values) {
                        if (type == MIXER_CTL_TYPE_BYTE)
                            ar->mixer_state[ctl_index].new_value.bytes[id] = value;
                        else if (type == MIXER_CTL_TYPE_ENUM)
                            ar->mixer_state[ctl_index].new_value.enumerated[id] = value;
                        else
                            ar->mixer_state[ctl_index].new_value.integer[id] = value;
                    } else {
                        ALOGE("value id out of range for mixer ctl '%s'",
                              mixer_ctl_get_name(ctl));
                    }
                } else {
                    /* set all values the same */
                    for (i = 0; i < ar->mixer_state[ctl_index].num_values; i++) {
                        if (type == MIXER_CTL_TYPE_BYTE)
                            ar->mixer_state[ctl_index].new_value.bytes[i] = value;
                        else if (type == MIXER_CTL_TYPE_ENUM)
                            ar->mixer_state[ctl_index].new_value.enumerated[i] = value;
                        else
                            ar->mixer_state[ctl_index].new_value.integer[i] = value;
                    }
                }
            }
        } else {
            /* nested ctl (within a path) */
            mixer_value.ctl_index = ctl_index;
            mixer_value.value = value;
            if (attr_id)
                mixer_value.index = atoi((char *)attr_id);
            else
                mixer_value.index = -1;
            if (state->path != NULL)
                path_add_value(ar, state->path, &mixer_value);
        }
    }

    else if (strcmp(tag_name, "pcmdai") == 0) {
        /*
         * Parse <pcmdai playback_link="7" capture_link="0" ... />
         * The attribute name IS the link identifier, the value is
         * the card/device number.
         */
        int i;
        for (i = 0; attr[i]; i += 2) {
            const char *link_name = (const char *)attr[i];
            const char *link_val  = (const char *)attr[i + 1];
            int j;
            for (j = 0; j < NUM_DAI_LINK; j++) {
                if (strcmp(link_name, dai_link_name[j]) == 0) {
                    ar->dai_link[j] = atoi(link_val);
                    ALOGD("pcmdai: %s -> %d", dai_link_name[j], ar->dai_link[j]);
                    break;
                }
            }
            if (j == NUM_DAI_LINK)
                ALOGW("pcmdai: unknown link \"%s\"", link_name);
        }
    }

    else if (strcmp(tag_name, "include") == 0) {
        if (attr_name) {
            if (!ar->include_path)
                ar->include_path = strdup((const char *)attr_name);
        } else {
            ALOGE("Unnamed include!");
        }
    }

done:
    state->level++;
}

static void end_tag(void *data, const XML_Char *tag_name)
{
    struct config_parse_state *state = data;
    (void)tag_name;

    state->level--;
}

/* ------------------------------------------------------------------ */
/*  Mixer state allocation / free                                     */
/* ------------------------------------------------------------------ */

static int alloc_mixer_state(struct audio_route *ar)
{
    unsigned int i;
    unsigned int num_values;
    struct mixer_ctl *ctl;
    enum mixer_ctl_type type;

    ar->num_mixer_ctls = mixer_get_num_ctls(ar->mixer);
    ar->mixer_state = calloc(ar->num_mixer_ctls, sizeof(struct mixer_state));
    if (!ar->mixer_state)
        return -1;

    for (i = 0; i < ar->num_mixer_ctls; i++) {
        ctl = mixer_get_ctl(ar->mixer, i);
        num_values = mixer_ctl_get_num_values(ctl);

        ar->mixer_state[i].ctl = ctl;
        ar->mixer_state[i].num_values = num_values;
        ar->mixer_state[i].active_count = 0;

        /* Skip unsupported types that are not supported yet in XML */
        type = mixer_ctl_get_type(ctl);

        if (!is_supported_ctl_type(type))
            continue;

        size_t value_sz = sizeof_ctl_type(type);
        ar->mixer_state[i].old_value.ptr = calloc(num_values, value_sz);
        ar->mixer_state[i].new_value.ptr = calloc(num_values, value_sz);
        ar->mixer_state[i].reset_value.ptr = calloc(num_values, value_sz);

        if (type == MIXER_CTL_TYPE_ENUM)
            ar->mixer_state[i].old_value.enumerated[0] = mixer_ctl_get_value(ctl, 0);
        else
            mixer_ctl_get_array(ctl, ar->mixer_state[i].old_value.ptr, num_values);

        memcpy(ar->mixer_state[i].new_value.ptr, ar->mixer_state[i].old_value.ptr,
               num_values * value_sz);
    }

    return 0;
}

static void free_mixer_state(struct audio_route *ar)
{
    unsigned int i;
    enum mixer_ctl_type type;

    for (i = 0; i < ar->num_mixer_ctls; i++) {
        type = mixer_ctl_get_type(ar->mixer_state[i].ctl);
        if (!is_supported_ctl_type(type))
            continue;

        free(ar->mixer_state[i].old_value.ptr);
        free(ar->mixer_state[i].new_value.ptr);
        free(ar->mixer_state[i].reset_value.ptr);
    }

    free(ar->mixer_state);
    ar->mixer_state = NULL;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

/* saves the current state of the mixer, for resetting all controls */
static void save_mixer_state(struct audio_route *ar)
{
    unsigned int i;
    enum mixer_ctl_type type;

    for (i = 0; i < ar->num_mixer_ctls; i++) {
        type = mixer_ctl_get_type(ar->mixer_state[i].ctl);
        if (!is_supported_ctl_type(type))
            continue;

        size_t value_sz = sizeof_ctl_type(type);
        memcpy(ar->mixer_state[i].reset_value.ptr, ar->mixer_state[i].new_value.ptr,
               ar->mixer_state[i].num_values * value_sz);
    }
}

/* Update the mixer with any changed values */
int audio_route_update_mixer(struct audio_route *ar)
{
    unsigned int i;
    unsigned int j;
    struct mixer_ctl *ctl;
    unsigned int changed_count = 0;

    ALOGD("> %s : +", __func__);

    for (i = 0; i < ar->num_mixer_ctls; i++) {
        unsigned int num_values = ar->mixer_state[i].num_values;
        enum mixer_ctl_type type;

        ctl = ar->mixer_state[i].ctl;

        /* Skip unsupported types */
        type = mixer_ctl_get_type(ctl);
        if (!is_supported_ctl_type(type))
            continue;

        /* if the value has changed, update the mixer */
        bool changed = false;
        if (type == MIXER_CTL_TYPE_BYTE) {
            for (j = 0; j < num_values; j++) {
                if (ar->mixer_state[i].old_value.bytes[j]
                        != ar->mixer_state[i].new_value.bytes[j]) {
                    changed = true;
                    break;
                }
            }
        } else if (type == MIXER_CTL_TYPE_ENUM) {
            for (j = 0; j < num_values; j++) {
                if (ar->mixer_state[i].old_value.enumerated[j]
                        != ar->mixer_state[i].new_value.enumerated[j]) {
                    changed = true;
                    break;
                }
            }
        } else {
            for (j = 0; j < num_values; j++) {
                if (ar->mixer_state[i].old_value.integer[j]
                        != ar->mixer_state[i].new_value.integer[j]) {
                    changed = true;
                    break;
                }
            }
        }

        if (changed) {
            if (type == MIXER_CTL_TYPE_ENUM) {
                int ret = mixer_ctl_set_value(ctl, 0,
                            ar->mixer_state[i].new_value.enumerated[0]);
                if (ret) {
                    ALOGE("ctl   : Fail to set (%d) : \"%s\" value \"%s\"",
                          ret,
                          mixer_ctl_get_name(ctl),
                          mixer_ctl_get_enum_string(ctl,
                              ar->mixer_state[i].new_value.enumerated[0]));
                } else {
                    if (!g_ship_mode) {
                        ALOGV("ctl   : \"%s\" value \"%s\"",
                              mixer_ctl_get_name(ctl),
                              mixer_ctl_get_enum_string(ctl,
                                  ar->mixer_state[i].new_value.enumerated[0]));
                    }
                }
            } else {
                int ret = mixer_ctl_set_array(ctl,
                            ar->mixer_state[i].new_value.ptr, num_values);
                if (ret) {
                    if (type == MIXER_CTL_TYPE_BYTE)
                        ALOGE("ctl   : Fail to set (%d) : \"%s\" value %d ...",
                              ret, mixer_ctl_get_name(ctl),
                              ar->mixer_state[i].new_value.bytes[0]);
                    else
                        ALOGE("ctl   : Fail to set (%d) : \"%s\" value %ld",
                              ret, mixer_ctl_get_name(ctl),
                              ar->mixer_state[i].new_value.integer[0]);
                } else {
                    if (!g_ship_mode) {
                        if (type == MIXER_CTL_TYPE_BYTE)
                            ALOGV("ctl   : \"%s\" value %d ...",
                                  mixer_ctl_get_name(ctl),
                                  ar->mixer_state[i].new_value.bytes[0]);
                        else
                            ALOGV("ctl   : \"%s\" value %ld",
                                  mixer_ctl_get_name(ctl),
                                  ar->mixer_state[i].new_value.integer[0]);
                    }
                }
            }

            size_t value_sz = sizeof_ctl_type(type);
            memcpy(ar->mixer_state[i].old_value.ptr,
                   ar->mixer_state[i].new_value.ptr,
                   num_values * value_sz);
            changed_count++;
        }
    }

    ALOGD("> %s : changed(%d) -", __func__, changed_count);
    return 0;
}

/* Reset the audio routes back to the initial state */
void audio_route_reset(struct audio_route *ar)
{
    unsigned int i;
    enum mixer_ctl_type type;

    ALOGD("> %s :", __func__);

    /* load all of the saved values */
    for (i = 0; i < ar->num_mixer_ctls; i++) {
        type = mixer_ctl_get_type(ar->mixer_state[i].ctl);
        if (!is_supported_ctl_type(type))
            continue;

        size_t value_sz = sizeof_ctl_type(type);
        memcpy(ar->mixer_state[i].new_value.ptr,
               ar->mixer_state[i].reset_value.ptr,
               ar->mixer_state[i].num_values * value_sz);
    }
}

/* Apply an audio route path by name */
int audio_route_apply_path(struct audio_route *ar, const char *name)
{
    struct mixer_path *path;

    if (!ar) {
        ALOGE("invalid audio_route");
        return -1;
    }

    path = path_get_by_name(ar, name);
    if (!path) {
        ALOGE("unable to find path '%s'", name);
        return -1;
    }

    path_apply(ar, path);

    return 0;
}

/* Reset an audio route path by name */
int audio_route_reset_path(struct audio_route *ar, const char *name)
{
    struct mixer_path *path;

    if (!ar) {
        ALOGE("invalid audio_route");
        return -1;
    }

    path = path_get_by_name(ar, name);
    if (!path) {
        ALOGE("unable to find path '%s'", name);
        return -1;
    }

    path_reset(ar, path);

    return 0;
}

/*
 * Operates on the specified path .. controls will be updated in the
 * order listed in the XML file
 */
static int audio_route_update_path(struct audio_route *ar,
                                   const char *name, int direction)
{
    struct mixer_path *path;
    unsigned int j;
    bool reverse = direction != DIRECTION_FORWARD;
    bool force_reset = direction == DIRECTION_REVERSE_RESET;

    ALOGD("> %s : \"%s\" reverse(%d)", __func__, name, direction);

    if (!ar) {
        ALOGE("invalid audio_route");
        return -1;
    }

    path = path_get_by_name(ar, name);
    if (!path) {
        ALOGE("unable to find path '%s'", name);
        return -1;
    }

    for (size_t i = 0; i < path->length; ++i) {
        unsigned int ctl_index;

        ctl_index = path->setting[reverse ? path->length - 1 - i : i].ctl_index;

        struct mixer_state *ms = &ar->mixer_state[ctl_index];

        enum mixer_ctl_type type = mixer_ctl_get_type(ms->ctl);
        if (!is_supported_ctl_type(type)) {
            continue;
        }

        if (reverse && ms->active_count > 0) {
            if (force_reset)
                ms->active_count = 0;
            else
                ms->active_count--;
        } else if (!reverse) {
            ms->active_count++;
        }

        size_t value_sz = sizeof_ctl_type(type);
        /* if any value has changed, update the mixer */
        for (j = 0; j < ms->num_values; j++) {
            if (type == MIXER_CTL_TYPE_BYTE) {
                if (ms->old_value.bytes[j] != ms->new_value.bytes[j]) {
                    if (reverse && ms->active_count > 0) {
                        ALOGD("%s: skip to reset mixer control '%s' in path '%s' "
                              "because it is still needed by other paths",
                              __func__, mixer_ctl_get_name(ms->ctl), name);
                        memcpy(ms->new_value.bytes, ms->old_value.bytes,
                               ms->num_values * value_sz);
                        break;
                    }
                    mixer_ctl_set_array(ms->ctl, ms->new_value.bytes,
                                        ms->num_values);
                    memcpy(ms->old_value.bytes, ms->new_value.bytes,
                           ms->num_values * value_sz);
                    break;
                }
            } else if (type == MIXER_CTL_TYPE_ENUM) {
                if (ms->old_value.enumerated[j] != ms->new_value.enumerated[j]) {
                    if (reverse && ms->active_count > 0) {
                        ALOGD("%s: skip to reset mixer control '%s' in path '%s' "
                              "because it is still needed by other paths",
                              __func__, mixer_ctl_get_name(ms->ctl), name);
                        memcpy(ms->new_value.enumerated, ms->old_value.enumerated,
                               ms->num_values * value_sz);
                        break;
                    }
                    mixer_ctl_set_value(ms->ctl, 0,
                                        ms->new_value.enumerated[0]);
                    memcpy(ms->old_value.enumerated, ms->new_value.enumerated,
                           ms->num_values * value_sz);
                    break;
                }
            } else if (ms->old_value.integer[j] != ms->new_value.integer[j]) {
                if (reverse && ms->active_count > 0) {
                    ALOGD("%s: skip to reset mixer control '%s' in path '%s' "
                          "because it is still needed by other paths",
                          __func__, mixer_ctl_get_name(ms->ctl), name);
                    memcpy(ms->new_value.integer, ms->old_value.integer,
                           ms->num_values * value_sz);
                    break;
                }
                mixer_ctl_set_array(ms->ctl, ms->new_value.integer,
                                    ms->num_values);
                memcpy(ms->old_value.integer, ms->new_value.integer,
                       ms->num_values * value_sz);
                break;
            }
        }
    }
    return 0;
}

int audio_route_apply_and_update_path(struct audio_route *ar, const char *name)
{
    if (audio_route_apply_path(ar, name) < 0)
        return -1;

    return audio_route_update_path(ar, name, DIRECTION_FORWARD);
}

int audio_route_reset_and_update_path(struct audio_route *ar, const char *name)
{
    if (audio_route_reset_path(ar, name) < 0)
        return -1;

    return audio_route_update_path(ar, name, DIRECTION_REVERSE);
}

/* Retrofit enhancement: force reset with DIRECTION_REVERSE_RESET */
int audio_route_force_reset_and_update_path(struct audio_route *ar,
                                            const char *name)
{
    if (audio_route_reset_path(ar, name) < 0)
        return -1;

    return audio_route_update_path(ar, name, DIRECTION_REVERSE_RESET);
}

/* ================================================================== */
/*  EXTRA FUNCTIONS from decompiled binary                             */
/* ================================================================== */

/*
 * audio_values_apply_path - For each mixer control in the given path, set
 * its value from the caller-supplied values array. Each element in values[]
 * is written to the corresponding path setting's ctl at the setting's index.
 */
int audio_values_apply_path(struct audio_route *ar, const char *name,
                            int *values)
{
    struct mixer_path *path;
    struct mixer_ctl *ctl;
    unsigned int j;
    int ret;
    int retry;

    ALOGD("> %s : \"%s\"", __func__, name);

    if (!ar) {
        ALOGE("invalid audio_route");
        return -1;
    }

    path = path_get_by_name(ar, name);
    if (!path) {
        ALOGE("unable to find path '%s'", name);
        return -1;
    }

    for (j = 0; j < path->length; j++) {
        unsigned int ctl_index = path->setting[j].ctl_index;
        enum mixer_ctl_type type = (enum mixer_ctl_type)path->setting[j].type;

        /* Retry loop for DSP reload */
        ctl = NULL;
        for (retry = 0; retry < CTL_RELOAD_RETRY_MAX; retry++) {
            ctl = index_to_ctl(ar, ctl_index);
            if (ctl && mixer_ctl_get_name(ctl))
                break;
            ALOGE("param : ctl_index %u doesn't exist, reload (%d)",
                  ctl_index, retry);
            if (update_mixer_state(ar))
                usleep(CTL_RELOAD_DELAY_US);
        }
        if (!ctl) {
            ALOGE("param : ctl_index %u couldn't be set", ctl_index);
            continue;
        }

        if (!is_supported_ctl_type(type)) {
            ALOGE("param : \"%s\" skip: unsupported type %d",
                  mixer_ctl_get_name(ctl), type);
            continue;
        }

        unsigned int num_values = mixer_ctl_get_num_values(ctl);
        if (j >= num_values) {
            ALOGE("param : \"%s\" skip: values index %u out of range (%u)",
                  mixer_ctl_get_name(ctl), j, num_values);
            continue;
        }

        ret = mixer_ctl_set_value(ctl, j, values[j]);
        if (ret) {
            ALOGE("param : Fail to set (%d) : \"%s\" id(%u) value %d",
                  ret, mixer_ctl_get_name(ctl), j, values[j]);
        } else if (!g_ship_mode) {
            ALOGV("param : \"%s\" id(%u) value %d",
                  mixer_ctl_get_name(ctl), j, values[j]);
        }
    }

    return 0;
}

/*
 * direct_mixer_set_value - Directly set a mixer control by name.
 * Uses the global audio_route handle.
 */
int direct_mixer_set_value(const char *name, int value)
{
    struct mixer_ctl *ctl;

    ALOGD("direct_mixer_set_value: %s, value: %d", name, value);
    if (!g_ar) {
        ALOGI("%s: No ar", __func__);
        return -1;
    }

    ctl = mixer_get_ctl_by_name(g_ar->mixer, name);
    return mixer_ctl_set_value(ctl, 0, value);
}

/*
 * direct_mixer_set_array - Directly set a mixer control byte array by name.
 * Uses the global audio_route handle.
 */
int direct_mixer_set_array(const char *name, char *data, size_t len)
{
    struct mixer_ctl *ctl;
    int ret;

    if (!g_ship_mode)
        ALOGD("%s: %s, value %s len %zu", __func__, name, data, len);

    if (!g_ar) {
        ALOGI("%s: No ar", __func__);
        return -1;
    }

    ctl = mixer_get_ctl_by_name(g_ar->mixer, name);
    if (!ctl) {
        ALOGE("%s: Unable to find mixer \"%s\"", __func__, name);
        return 0;
    }

    if (!len)
        len = strlen(data);

    ret = mixer_ctl_set_array(ctl, data, len);
    if (ret) {
        ALOGE("param : Fail to set (%d) : \"%s\"", ret,
              mixer_ctl_get_name(ctl));
    }
    return ret;
}

/*
 * audio_route_get_mixer - Return the underlying mixer handle.
 */
struct mixer *audio_route_get_mixer(struct audio_route *ar)
{
    return ar->mixer;
}

/*
 * audio_route_get_mixer_ctl - Get a mixer control by name from the route.
 */
struct mixer_ctl *audio_route_get_mixer_ctl(struct audio_route *ar,
                                            const char *name)
{
    return mixer_get_ctl_by_name(ar->mixer, name);
}

/*
 * get_dai_link - Retrieve the PCM device node for the given DAI link.
 * Values are parsed from the XML <pcmdai> tags during init.
 */
int get_dai_link(struct audio_route *ar, enum pcm_dai_link dai_link)
{
    if (!ar) {
        ALOGE("ar is NULL");
        return -1;
    }

    if (dai_link < 0 || dai_link >= NUM_DAI_LINK) {
        ALOGE("dai_link %d out of range (max %d)", dai_link,
              NUM_DAI_LINK - 1);
        return -1;
    }

    ALOGV("requested PCM for %d, card=%d", dai_link,
          ar->dai_link[dai_link]);

    return ar->dai_link[dai_link];
}

/*
 * get_audio_route - Return the global audio route handle.
 */
struct audio_route *get_audio_route(void)
{
    if (!g_ar)
        ALOGI("%s: No ar", __func__);
    return g_ar;
}

/*
 * audio_route_get_dsp_ctl - Get a DSP mixer control by name with retry.
 * Waits for the control to become available (e.g. after DSP firmware load).
 */
struct mixer_ctl *audio_route_get_dsp_ctl(struct audio_route *ar,
                                          const char *name)
{
    struct mixer_ctl *ctl;
    int retry;

    for (retry = 0; retry < CTL_RELOAD_RETRY_MAX; retry++) {
        ctl = mixer_get_ctl_by_name(ar->mixer, name);
        if (ctl)
            return ctl;

        ALOGE("param : \"%s\" doesn't exist, should be reloaded (%d)",
              name, retry);
        if (update_mixer_state(ar))
            usleep(CTL_RELOAD_DELAY_US);
    }

    ALOGE("param : \"%s\" couldn't be set", name);
    return NULL;
}

/*
 * audio_route_missing_ctl - Return the count of controls that were not found
 * during XML parsing.
 */
unsigned int audio_route_missing_ctl(struct audio_route *ar)
{
    if (!ar) {
        ALOGE("invalid audio_route");
        return 0;
    }
    return ar->missing_ctl_count;
}

/*
 * path_update_mixer_state_reset - Reset a path's new_value to the saved
 * setting values (not the reset values), effectively undoing the last apply.
 */
int path_update_mixer_state_reset(struct audio_route *ar, const char *name)
{
    struct mixer_path *path;
    unsigned int i;

    ALOGD("%s : \"%s\"", __func__, name);

    if (!ar) {
        ALOGE("Invalid audio_route");
        return -1;
    }

    path = path_get_by_name(ar, name);
    if (!path) {
        ALOGE("unable to find path '%s'", name);
        return -1;
    }

    for (i = 0; i < path->length; i++) {
        unsigned int ctl_index = path->setting[i].ctl_index;
        struct mixer_ctl *ctl = index_to_ctl(ar, ctl_index);
        enum mixer_ctl_type type = mixer_ctl_get_type(ctl);

        if (!is_supported_ctl_type(type))
            continue;

        size_t value_sz = sizeof_ctl_type(type);
        memcpy(ar->mixer_state[ctl_index].reset_value.ptr,
               path->setting[i].value.ptr,
               path->setting[i].num_values * value_sz);
    }

    return 0;
}

/*
 * process_merge_bin_file - Merge two CSV/raw files into a binary firmware
 * file with a 16-byte header (MDR\0 magic). Used for ez2-control DSP.
 *
 * Format:
 *   offset 0: "MDR\0" magic
 *   offset 4: version (1)
 *   offset 8..15: reserved
 *   then subfile chunks with 20-byte headers
 */
int process_merge_bin_file(const char *out_path, char *in1, char *in2)
{
    FILE *fout = NULL;
    FILE *fout2 = NULL;
    int ret = -EINVAL;
    /* 16-byte header: "MDR\0" + version(4) + reserved(8) */
    unsigned char header[16] = {
        'M', 'D', 'R', '\0',
        0x01, 0x00, 0x00, 0x00,  /* version 1 LE */
        0x00, 0x01, 0x01, 0x01,  /* reserved */
        0x00, 0x00, 0x50, 0x02   /* reserved */
    };

    if (!in1 || !in2) {
        ALOGE("process_merge_bin_file: invalid params");
        return -EINVAL;
    }

    fout = fopen("/data/firmware/ez2-control", "wb");
    if (!fout) {
        ALOGE("Failed to open: output bin file(%s), error: %s",
              "/data/firmware/ez2-control", strerror(errno));
        if (errno == EACCES) {
            ALOGI("remove previous file(%s), error: %s",
                  "/data/firmware/ez2-control", strerror(EACCES));
            remove("/data/firmware/ez2-control");
            fout = fopen("/data/firmware/ez2-control", "wb");
            if (!fout) {
                ALOGE("Failed to open: output bin file(%s)",
                      "/data/firmware/ez2-control");
            }
        }
    }
    if (fout)
        ALOGD("Successfully opened: output bin file(%s)",
              "/data/firmware/ez2-control");

    /* Also write to alternate path if specified and different */
    if (out_path && strcmp("/data/firmware/ez2-control", out_path)) {
        fout2 = fopen(out_path, "wb");
        if (!fout2) {
            ALOGE("Failed to open: output bin file(%s)", out_path);
            if (errno == EACCES) {
                ALOGI("remove previous file(%s), error: %s",
                      out_path, strerror(EACCES));
                remove(out_path);
                fout2 = fopen(out_path, "wb");
                if (fout2)
                    ALOGD("Successfully opened: output bin file(%s)", out_path);
            }
        } else {
            ALOGD("Successfully opened: output bin file(%s)", out_path);
        }
    }

    /* Write header and process subfiles */
    if (fout) {
        fwrite(header, 1, 16, fout);
        process_file(in1, fout, 0);
        process_file(in2, fout, 0x0C00); /* 3072 */
        fclose(fout);
        ret = 0;
    }

    if (fout2) {
        fwrite(header, 1, 16, fout2);
        process_file(in1, fout2, 0);
        process_file(in2, fout2, 0x0C00); /* 3072 */
        fclose(fout2);
        ret = 0;
    }

    return ret;
}

/*
 * process_file - Read a CSV/raw text file and write it as a binary chunk
 * with a 20-byte sub-header.
 *
 * Sub-header format:
 *   offset 0:  address (2 bytes LE)
 *   offset 2:  reserved (2 bytes)
 *   offset 4:  block_size (4 bytes LE) = 36
 *   offset 8:  data_size (4 bytes LE) = 4096
 *   offset 12: reserved (4 bytes)
 *   offset 16: type (2 bytes)
 *   offset 18: data_length (2 bytes LE)
 *
 * Data is read as 3-byte RGB groups from the text file (one byte per
 * character value), and written as 4-byte groups (B,G,R,0).
 */
int process_file(char *filename, FILE *out, unsigned short type)
{
    FILE *fin;
    int c1, c2, c3;
    struct stat st;
    unsigned char subheader[20];

    fin = fopen(filename, "r");
    if (!fin) {
        ALOGE("Failed to open: %s", filename);
        return -1;
    }

    if (fstat(fileno(fin), &st) == -1)
        ALOGE("Failed fstat");

    memset(subheader, 0, sizeof(subheader));
    subheader[0] = type & 0xFF;
    subheader[1] = (type >> 8) & 0xFF;
    subheader[4] = 36;          /* block_size */
    subheader[8] = 0x00;        /* data_size low */
    subheader[9] = 0x10;        /* data_size = 4096 */
    /* data_length = blocksize / 3 * 4 */
    {
        unsigned short data_len = 4 * ((unsigned int)st.st_blksize / 3);
        subheader[18] = data_len & 0xFF;
        subheader[19] = (data_len >> 8) & 0xFF;
    }

    fwrite(subheader, 1, 20, out);

    c1 = fgetc(fin);
    while (c1 != EOF) {
        c2 = fgetc(fin);
        if (c2 == EOF) {
            c2 = 0;
            c3 = 0;
            c1 = EOF;
        } else {
            c3 = fgetc(fin);
            if (c3 == EOF) {
                c3 = 0;
                c1 = EOF;
            } else {
                c1 = fgetc(fin);
            }
        }
        /* Write B G R 0 */
        fputc(0, out);
        fputc(c3, out);
        fputc(c2, out);
        fputc(c1 == EOF ? 0 : c1, out);
    }

    fclose(fin);
    return 0;
}

/* ================================================================== */
/*  Init / Free                                                        */
/* ================================================================== */

struct audio_route *audio_route_init(unsigned int card, const char *xml_path)
{
    struct config_parse_state state;
    XML_Parser parser;
    FILE *file;
    int bytes_read;
    void *buf;
    struct audio_route *ar;

    ar = calloc(1, sizeof(struct audio_route));
    if (!ar)
        goto err_calloc;

    ar->mixer = mixer_open(card);
    if (!ar->mixer) {
        ALOGE("Unable to open the mixer, aborting.");
        goto err_mixer_open;
    }

    ar->mixer_path = NULL;
    ar->mixer_path_size = 0;
    ar->num_mixer_paths = 0;
    ar->missing_ctl_count = 0;

    /* Allocate space for and read current mixer settings */
    if (alloc_mixer_state(ar) < 0)
        goto err_mixer_state;

    /* Use the default XML path if none is provided */
    if (xml_path == NULL)
        xml_path = MIXER_XML_PATH;

    /* Check device-tree override for mixer path */
    {
        FILE *dt_file;
        char dt_path[256];
        char dt_buf[256];

        dt_file = fopen("/proc/device-tree/sound/mixer-paths", "r");
        if (dt_file) {
            int n = fread(dt_buf, 1, 256, dt_file);
            fclose(dt_file);
            if (n >= 1) {
                dt_buf[n] = '\0';
                /* Build path from directory of xml_path + dt filename */
                char tmp_path[256];
                strncpy(tmp_path, xml_path, sizeof(tmp_path) - 1);
                tmp_path[sizeof(tmp_path) - 1] = '\0';
                char *slash = strrchr(tmp_path, '/');
                if (slash) {
                    *slash = '\0';
                    snprintf(dt_path, sizeof(dt_path), "%s/%s", tmp_path, dt_buf);
                    ALOGD("init: dt path (%s)", dt_path);
                    file = fopen(dt_path, "r");
                    if (file) {
                        ALOGI("init: using device-tree mixer path (%s)", dt_path);
                        goto have_file;
                    }
                }
            }
        }
    }

    file = fopen(xml_path, "r");

have_file:
    if (!file) {
        ALOGE("Failed to open %s: %s", xml_path, strerror(errno));
        goto err_fopen;
    }

    parser = XML_ParserCreate(NULL);
    if (!parser) {
        ALOGE("Failed to create XML parser");
        goto err_parser_create;
    }

    memset(&state, 0, sizeof(state));
    state.ar = ar;
    XML_SetUserData(parser, &state);
    XML_SetElementHandler(parser, start_tag, end_tag);

    for (;;) {
        buf = XML_GetBuffer(parser, BUF_SIZE);
        if (buf == NULL)
            goto err_parse;

        bytes_read = fread(buf, 1, BUF_SIZE, file);
        if (bytes_read < 0)
            goto err_parse;

        if (XML_ParseBuffer(parser, bytes_read,
                            bytes_read == 0) == XML_STATUS_ERROR) {
            ALOGE("Error in mixer xml (%s)", MIXER_XML_PATH);
            goto err_parse;
        }

        if (bytes_read == 0)
            break;
    }

    /* Apply the initial mixer values, and save them so we can reset the
       mixer to the original values */
    audio_route_update_mixer(ar);
    save_mixer_state(ar);

    XML_ParserFree(parser);
    fclose(file);

    /* Set global handle */
    g_ar = ar;

    /* Check ship mode */
    {
        char ship_buf[96];
        memset(ship_buf, 0, sizeof(ship_buf));
        property_get("ro.vendor.product_ship", ship_buf, "");
        g_ship_mode = (strcmp(ship_buf, "true") == 0);
    }

    return ar;

err_parse:
    path_free(ar);
    XML_ParserFree(parser);
err_parser_create:
    fclose(file);
err_fopen:
    free_mixer_state(ar);
err_mixer_state:
    mixer_close(ar->mixer);
err_mixer_open:
    free(ar);
    ar = NULL;
err_calloc:
    return NULL;
}

void audio_route_free(struct audio_route *ar)
{
    free_mixer_state(ar);
    mixer_close(ar->mixer);
    path_free(ar);
    if (g_ar == ar)
        g_ar = NULL;
    free(ar->include_path);
    free(ar);
}
