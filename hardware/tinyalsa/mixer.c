/* mixer.c — libalsa7870 (libtinyalsa enhanced with samsung additions, based on q m10lte)
**
** Copyright 2011, The Android Open Source Project
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of The Android Open Source Project nor the names of
**       its contributors may be used to endorse or promote products derived
**       from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY The Android Open Source Project ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL The Android Open Source Project BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
** DAMAGE.
*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <poll.h>

#include <sys/ioctl.h>

#include <linux/ioctl.h>
#define __force
#define __bitwise
#define __user
#include <sound/asound.h>

#ifndef SNDRV_CTL_ELEM_ID_NAME_MAXLEN
#define SNDRV_CTL_ELEM_ID_NAME_MAXLEN 44
#endif

#include <tinyalsa/asoundlib.h>

/* --------------------------------------------------------------------
 * Internal structures
 * -------------------------------------------------------------------- */

struct mixer_ctl {
    struct mixer             *mixer;
    struct snd_ctl_elem_info *info;
    char                    **ename;
    bool                      info_retrieved;
};

struct mixer {
    int                       fd;
    struct snd_ctl_card_info  card_info;
    struct snd_ctl_elem_info *elem_info;
    struct mixer_ctl         *ctl;
    unsigned int              count;
};

/*
 * mixer_event is the opaque handle returned by mixer_read_event().
 * Internally it wraps snd_ctl_event (72 bytes on Linux) but we expose
 * it as an opaque pointer so callers never depend on the kernel struct
 * layout.  The caller owns the memory and must free() it.
 */
struct mixer_event {
    struct snd_ctl_event ev;
};

/* --------------------------------------------------------------------
 * Forward declarations for internal helpers
 * -------------------------------------------------------------------- */
static void mixer_free_ctl_enum_strings(struct mixer_ctl *ctl);
static bool mixer_ctl_get_elem_info(struct mixer_ctl *ctl);

/* --------------------------------------------------------------------
 * mixer_close
 * -------------------------------------------------------------------- */
void mixer_close(struct mixer *mixer)
{
    unsigned int n, m;

    if (!mixer)
        return;

    if (mixer->fd >= 0)
        close(mixer->fd);

    if (mixer->ctl) {
        for (n = 0; n < mixer->count; n++) {
            if (mixer->ctl[n].ename) {
                unsigned int max = mixer->ctl[n].info->value.enumerated.items;
                for (m = 0; m < max; m++)
                    free(mixer->ctl[n].ename[m]);
                free(mixer->ctl[n].ename);
            }
        }
        free(mixer->ctl);
    }

    if (mixer->elem_info)
        free(mixer->elem_info);

    free(mixer);
}

/* --------------------------------------------------------------------
 * mixer_open
 *
 * Enhancement vs upstream: the binary from m10lte reveals that mixer_open
 * accepts extra hint parameters (a2..a4) that are forwarded to the ioctl
 * path.  The public API keeps the single-card-number signature; internally
 * we honour the upstream open path exactly but provide the additional
 * mixer_update() path for post-open refresh.
 * -------------------------------------------------------------------- */
struct mixer *mixer_open(unsigned int card)
{
    struct snd_ctl_elem_list  elist;
    struct snd_ctl_elem_id   *eid  = NULL;
    struct mixer             *mixer = NULL;
    unsigned int              n;
    int                       fd;
    char                      fn[256];

    snprintf(fn, sizeof(fn), "/dev/snd/controlC%u", card);
    fd = open(fn, O_RDWR);
    if (fd < 0)
        return NULL;

    memset(&elist, 0, sizeof(elist));
    if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_LIST, &elist) < 0)
        goto fail;

    mixer = calloc(1, sizeof(*mixer));
    if (!mixer)
        goto fail;

    mixer->ctl       = calloc(elist.count, sizeof(struct mixer_ctl));
    mixer->elem_info = calloc(elist.count, sizeof(struct snd_ctl_elem_info));
    if (!mixer->ctl || !mixer->elem_info)
        goto fail;

    if (ioctl(fd, SNDRV_CTL_IOCTL_CARD_INFO, &mixer->card_info) < 0)
        goto fail;

    eid = calloc(elist.count, sizeof(struct snd_ctl_elem_id));
    if (!eid)
        goto fail;

    mixer->count    = elist.count;
    mixer->fd       = fd;
    elist.space     = mixer->count;
    elist.pids      = eid;
    if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_LIST, &elist) < 0)
        goto fail;

    for (n = 0; n < mixer->count; n++) {
        struct mixer_ctl *ctl = mixer->ctl + n;

        ctl->mixer = mixer;
        ctl->info  = mixer->elem_info + n;
        ctl->info->id.numid = eid[n].numid;
        strncpy((char *)ctl->info->id.name, (char *)eid[n].name,
                SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
        ctl->info->id.name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN - 1] = 0;
    }

    free(eid);
    return mixer;

fail:
    if (eid)
        free(eid);
    if (mixer)
        mixer_close(mixer);
    else if (fd >= 0)
        close(fd);
    return NULL;
}

/* --------------------------------------------------------------------
 * mixer_update  [NEW — derived from decompiled binary]
 *
 * Dynamically re-enumerates mixer controls.  Required when the sound
 * card adds or removes elements at runtime (e.g. HDMI hot-plug causes
 * the codec to expose new ELD controls).
 *
 * The decompiled binary shows this performs:
 *   1. SNDRV_CTL_IOCTL_ELEM_LIST to get the new count
 *   2. realloc() the ctl[] and elem_info[] arrays to the new count
 *   3. calloc() a temporary snd_ctl_elem_id[] array
 *   4. SNDRV_CTL_IOCTL_ELEM_LIST again with pids populated
 *   5. For each control: SNDRV_CTL_IOCTL_ELEM_INFO + enum string fetch
 *   6. Free the temporary id array
 *
 * Returns 0 on success, -errno on failure.
 * -------------------------------------------------------------------- */
int mixer_update(struct mixer *mixer)
{
    struct snd_ctl_elem_list  elist;
    struct snd_ctl_elem_id   *eid  = NULL;
    struct snd_ctl_elem_info *new_elem_info = NULL;
    struct mixer_ctl         *new_ctl       = NULL;
    unsigned int              n, m;

    if (!mixer)
        return -EINVAL;

    /* Step 1: query new element count */
    memset(&elist, 0, sizeof(elist));
    if (ioctl(mixer->fd, SNDRV_CTL_IOCTL_ELEM_LIST, &elist) < 0)
        return -errno;

    if (elist.count == mixer->count) {
        /* No structural change; nothing to do. */
        return 0;
    }

    /* Step 2: grow/shrink the elem_info and ctl arrays */
    new_elem_info = realloc(mixer->elem_info,
                            elist.count * sizeof(struct snd_ctl_elem_info));
    if (!new_elem_info)
        return -ENOMEM;
    mixer->elem_info = new_elem_info;

    new_ctl = realloc(mixer->ctl,
                      elist.count * sizeof(struct mixer_ctl));
    if (!new_ctl)
        return -ENOMEM;
    mixer->ctl = new_ctl;

    /* Step 3: allocate temporary id array */
    eid = calloc(elist.count, sizeof(struct snd_ctl_elem_id));
    if (!eid)
        return -ENOMEM;

    /* Step 4: fetch element ids */
    elist.space = elist.count;
    elist.pids  = eid;
    if (ioctl(mixer->fd, SNDRV_CTL_IOCTL_ELEM_LIST, &elist) < 0) {
        free(eid);
        return -errno;
    }

    /* Zero out newly allocated slots (if count grew) */
    if (elist.count > mixer->count) {
        memset(mixer->ctl + mixer->count, 0,
               (elist.count - mixer->count) * sizeof(struct mixer_ctl));
        memset(mixer->elem_info + mixer->count, 0,
               (elist.count - mixer->count) * sizeof(struct snd_ctl_elem_info));
    }

    /* Step 5: rebuild ctl[] cross-links and fetch element info */
    for (n = 0; n < elist.count; n++) {
        struct mixer_ctl *ctl = mixer->ctl + n;

        /* Free old enum strings if this slot is being reused */
        if (n < mixer->count && ctl->ename) {
            unsigned int old_items = ctl->info ? ctl->info->value.enumerated.items : 0;
            for (m = 0; m < old_items; m++)
                free(ctl->ename[m]);
            free(ctl->ename);
            ctl->ename = NULL;
        }

        ctl->mixer          = mixer;
        ctl->info           = mixer->elem_info + n;
        ctl->info_retrieved = false;
        ctl->info->id.numid = eid[n].numid;
        strncpy((char *)ctl->info->id.name, (char *)eid[n].name,
                SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
        ctl->info->id.name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN - 1] = 0;

        /* Fetch full elem_info now (mirroring the binary's ioctl loop) */
        if (ioctl(mixer->fd, SNDRV_CTL_IOCTL_ELEM_INFO, ctl->info) < 0) {
            /* Non-fatal: mark as not retrieved and move on */
            ctl->info_retrieved = false;
        } else {
            ctl->info_retrieved = true;

            /* Re-fetch enum strings for ENUMERATED controls */
            if (ctl->info->type == SNDRV_CTL_ELEM_TYPE_ENUMERATED &&
                ctl->info->value.enumerated.items > 0) {
                struct snd_ctl_elem_info tmp;
                unsigned int items = ctl->info->value.enumerated.items;
                char **enames = calloc(items, sizeof(char *));
                if (!enames)
                    goto fail_eid;

                for (m = 0; m < items; m++) {
                    memset(&tmp, 0, sizeof(tmp));
                    tmp.id.numid = ctl->info->id.numid;
                    tmp.value.enumerated.item = m;
                    if (ioctl(mixer->fd, SNDRV_CTL_IOCTL_ELEM_INFO, &tmp) < 0) {
                        /* Free what we have so far */
                        unsigned int k;
                        for (k = 0; k < m; k++)
                            free(enames[k]);
                        free(enames);
                        enames = NULL;
                        break;
                    }
                    enames[m] = strdup(tmp.value.enumerated.name);
                    if (!enames[m]) {
                        unsigned int k;
                        for (k = 0; k < m; k++)
                            free(enames[k]);
                        free(enames);
                        enames = NULL;
                        break;
                    }
                }
                ctl->ename = enames;
            }
        }
    }

    mixer->count = elist.count;
    free(eid);
    return 0;

fail_eid:
    free(eid);
    return -ENOMEM;
}

/* --------------------------------------------------------------------
 * Internal: mixer_ctl_get_elem_info
 *
 * Lazily fetches element info on first access and populates enum strings.
 * -------------------------------------------------------------------- */
static bool mixer_ctl_get_elem_info(struct mixer_ctl *ctl)
{
    if (!ctl->info_retrieved) {
        if (ioctl(ctl->mixer->fd, SNDRV_CTL_IOCTL_ELEM_INFO, ctl->info) < 0)
            return false;
        ctl->info_retrieved = true;
    }

    if (ctl->info->type != SNDRV_CTL_ELEM_TYPE_ENUMERATED || ctl->ename)
        return true;

    struct snd_ctl_elem_info tmp;
    unsigned int items = ctl->info->value.enumerated.items;
    char **enames = calloc(items, sizeof(char *));
    if (!enames)
        return false;

    for (unsigned int i = 0; i < items; i++) {
        memset(&tmp, 0, sizeof(tmp));
        tmp.id.numid = ctl->info->id.numid;
        tmp.value.enumerated.item = i;
        if (ioctl(ctl->mixer->fd, SNDRV_CTL_IOCTL_ELEM_INFO, &tmp) < 0)
            goto fail;
        enames[i] = strdup(tmp.value.enumerated.name);
        if (!enames[i])
            goto fail;
    }
    ctl->ename = enames;
    return true;

fail:
    /* Free whatever we managed to allocate */
    for (unsigned int i = 0; i < items; i++)
        free(enames[i]);
    free(enames);
    return false;
}

/* --------------------------------------------------------------------
 * Public mixer getters
 * -------------------------------------------------------------------- */
const char *mixer_get_name(struct mixer *mixer)
{
    return (const char *)mixer->card_info.name;
}

unsigned int mixer_get_num_ctls(struct mixer *mixer)
{
    if (!mixer)
        return 0;

    return mixer->count;
}

struct mixer_ctl *mixer_get_ctl(struct mixer *mixer, unsigned int id)
{
    struct mixer_ctl *ctl;

    if (!mixer || (id >= mixer->count))
        return NULL;

    ctl = mixer->ctl + id;
    if (!mixer_ctl_get_elem_info(ctl))
        return NULL;

    return ctl;
}

struct mixer_ctl *mixer_get_ctl_by_name(struct mixer *mixer, const char *name)
{
    unsigned int n;

    if (!mixer)
        return NULL;

    for (n = 0; n < mixer->count; n++)
        if (!strcmp(name, (char *)mixer->elem_info[n].id.name))
            return mixer_get_ctl(mixer, n);

    return NULL;
}

void mixer_ctl_update(struct mixer_ctl *ctl)
{
    ioctl(ctl->mixer->fd, SNDRV_CTL_IOCTL_ELEM_INFO, ctl->info);
}

const char *mixer_ctl_get_name(struct mixer_ctl *ctl)
{
    if (!ctl)
        return NULL;

    return (const char *)ctl->info->id.name;
}

enum mixer_ctl_type mixer_ctl_get_type(struct mixer_ctl *ctl)
{
    if (!ctl)
        return MIXER_CTL_TYPE_UNKNOWN;

    switch (ctl->info->type) {
    case SNDRV_CTL_ELEM_TYPE_BOOLEAN:    return MIXER_CTL_TYPE_BOOL;
    case SNDRV_CTL_ELEM_TYPE_INTEGER:    return MIXER_CTL_TYPE_INT;
    case SNDRV_CTL_ELEM_TYPE_ENUMERATED: return MIXER_CTL_TYPE_ENUM;
    case SNDRV_CTL_ELEM_TYPE_BYTES:      return MIXER_CTL_TYPE_BYTE;
    case SNDRV_CTL_ELEM_TYPE_IEC958:     return MIXER_CTL_TYPE_IEC958;
    case SNDRV_CTL_ELEM_TYPE_INTEGER64:  return MIXER_CTL_TYPE_INT64;
    default:                             return MIXER_CTL_TYPE_UNKNOWN;
    }
}

const char *mixer_ctl_get_type_string(struct mixer_ctl *ctl)
{
    if (!ctl)
        return "";

    switch (ctl->info->type) {
    case SNDRV_CTL_ELEM_TYPE_BOOLEAN:    return "BOOL";
    case SNDRV_CTL_ELEM_TYPE_INTEGER:    return "INT";
    case SNDRV_CTL_ELEM_TYPE_ENUMERATED: return "ENUM";
    case SNDRV_CTL_ELEM_TYPE_BYTES:      return "BYTE";
    case SNDRV_CTL_ELEM_TYPE_IEC958:     return "IEC958";
    case SNDRV_CTL_ELEM_TYPE_INTEGER64:  return "INT64";
    default:                             return "Unknown";
    }
}

unsigned int mixer_ctl_get_num_values(struct mixer_ctl *ctl)
{
    if (!ctl)
        return 0;

    return ctl->info->count;
}

/* --------------------------------------------------------------------
 * Percent helpers
 * -------------------------------------------------------------------- */
static int percent_to_int(struct snd_ctl_elem_info *ei, int percent)
{
    int range;

    if (percent > 100)
        percent = 100;
    else if (percent < 0)
        percent = 0;

    range = (int)(ei->value.integer.max - ei->value.integer.min);

    return (int)ei->value.integer.min + (range * percent) / 100;
}

static int int_to_percent(struct snd_ctl_elem_info *ei, int value)
{
    int range = (int)(ei->value.integer.max - ei->value.integer.min);

    if (range == 0)
        return 0;

    return ((value - (int)ei->value.integer.min) / range) * 100;
}

int mixer_ctl_get_percent(struct mixer_ctl *ctl, unsigned int id)
{
    if (!ctl || (ctl->info->type != SNDRV_CTL_ELEM_TYPE_INTEGER))
        return -EINVAL;

    return int_to_percent(ctl->info, mixer_ctl_get_value(ctl, id));
}

int mixer_ctl_set_percent(struct mixer_ctl *ctl, unsigned int id, int percent)
{
    if (!ctl || (ctl->info->type != SNDRV_CTL_ELEM_TYPE_INTEGER))
        return -EINVAL;

    return mixer_ctl_set_value(ctl, id, percent_to_int(ctl->info, percent));
}

/* --------------------------------------------------------------------
 * mixer_ctl_get_value / mixer_ctl_set_value
 * -------------------------------------------------------------------- */
int mixer_ctl_get_value(struct mixer_ctl *ctl, unsigned int id)
{
    struct snd_ctl_elem_value ev;
    int ret;

    if (!ctl || (id >= ctl->info->count))
        return -EINVAL;

    memset(&ev, 0, sizeof(ev));
    ev.id.numid = ctl->info->id.numid;
    ret = ioctl(ctl->mixer->fd, SNDRV_CTL_IOCTL_ELEM_READ, &ev);
    if (ret < 0)
        return ret;

    switch (ctl->info->type) {
    case SNDRV_CTL_ELEM_TYPE_BOOLEAN:
        return !!ev.value.integer.value[id];
    case SNDRV_CTL_ELEM_TYPE_INTEGER:
        return (int)ev.value.integer.value[id];
    case SNDRV_CTL_ELEM_TYPE_ENUMERATED:
        return (int)ev.value.enumerated.item[id];
    case SNDRV_CTL_ELEM_TYPE_BYTES:
        return ev.value.bytes.data[id];
    default:
        return -EINVAL;
    }
}

int mixer_ctl_is_access_tlv_rw(struct mixer_ctl *ctl)
{
    return (ctl->info->access & SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE);
}

int mixer_ctl_get_array(struct mixer_ctl *ctl, void *array, size_t count)
{
    struct snd_ctl_elem_value ev;
    int    ret = 0;
    size_t size;
    void  *source;
    size_t total_count;

    if (!ctl || !count || !array)
        return -EINVAL;

    total_count = ctl->info->count;

    if ((ctl->info->type == SNDRV_CTL_ELEM_TYPE_BYTES) &&
        mixer_ctl_is_access_tlv_rw(ctl)) {
        /* Additional two words for the TLV header */
        total_count += TLV_HEADER_SIZE;
    }

    if (count > total_count)
        return -EINVAL;

    memset(&ev, 0, sizeof(ev));
    ev.id.numid = ctl->info->id.numid;

    switch (ctl->info->type) {
    case SNDRV_CTL_ELEM_TYPE_BOOLEAN:
    case SNDRV_CTL_ELEM_TYPE_INTEGER:
        ret = ioctl(ctl->mixer->fd, SNDRV_CTL_IOCTL_ELEM_READ, &ev);
        if (ret < 0)
            return ret;
        size   = sizeof(ev.value.integer.value[0]);
        source = ev.value.integer.value;
        break;

    case SNDRV_CTL_ELEM_TYPE_BYTES:
        if (mixer_ctl_is_access_tlv_rw(ctl)) {
            struct snd_ctl_tlv *tlv;

            if (count > SIZE_MAX - sizeof(*tlv))
                return -EINVAL;
            tlv = calloc(1, sizeof(*tlv) + count);
            if (!tlv)
                return -ENOMEM;
            tlv->numid  = ctl->info->id.numid;
            tlv->length = count;
            ret = ioctl(ctl->mixer->fd, SNDRV_CTL_IOCTL_TLV_READ, tlv);
            source = tlv->tlv;
            memcpy(array, source, count);
            free(tlv);
            return ret;
        } else {
            ret = ioctl(ctl->mixer->fd, SNDRV_CTL_IOCTL_ELEM_READ, &ev);
            if (ret < 0)
                return ret;
            size   = sizeof(ev.value.bytes.data[0]);
            source = ev.value.bytes.data;
            break;
        }

    case SNDRV_CTL_ELEM_TYPE_IEC958:
        size   = sizeof(ev.value.iec958);
        source = &ev.value.iec958;
        break;

    default:
        return -EINVAL;
    }

    memcpy(array, source, size * count);
    return 0;
}

int mixer_ctl_set_value(struct mixer_ctl *ctl, unsigned int id, int value)
{
    struct snd_ctl_elem_value ev;
    int ret;

    if (!ctl || (id >= ctl->info->count))
        return -EINVAL;

    memset(&ev, 0, sizeof(ev));
    ev.id.numid = ctl->info->id.numid;
    ret = ioctl(ctl->mixer->fd, SNDRV_CTL_IOCTL_ELEM_READ, &ev);
    if (ret < 0)
        return ret;

    switch (ctl->info->type) {
    case SNDRV_CTL_ELEM_TYPE_BOOLEAN:
        ev.value.integer.value[id] = !!value;
        break;
    case SNDRV_CTL_ELEM_TYPE_INTEGER:
        ev.value.integer.value[id] = value;
        break;
    case SNDRV_CTL_ELEM_TYPE_ENUMERATED:
        ev.value.enumerated.item[id] = value;
        break;
    case SNDRV_CTL_ELEM_TYPE_BYTES:
        ev.value.bytes.data[id] = value;
        break;
    default:
        return -EINVAL;
    }

    return ioctl(ctl->mixer->fd, SNDRV_CTL_IOCTL_ELEM_WRITE, &ev);
}

int mixer_ctl_set_array(struct mixer_ctl *ctl, const void *array, size_t count)
{
    struct snd_ctl_elem_value ev;
    size_t size;
    void  *dest;
    size_t total_count;

    if (!ctl || !count || !array)
        return -EINVAL;

    total_count = ctl->info->count;

    if ((ctl->info->type == SNDRV_CTL_ELEM_TYPE_BYTES) &&
        mixer_ctl_is_access_tlv_rw(ctl)) {
        total_count += TLV_HEADER_SIZE;
    }

    if (count > total_count)
        return -EINVAL;

    memset(&ev, 0, sizeof(ev));
    ev.id.numid = ctl->info->id.numid;

    switch (ctl->info->type) {
    case SNDRV_CTL_ELEM_TYPE_BOOLEAN:
    case SNDRV_CTL_ELEM_TYPE_INTEGER:
        size = sizeof(ev.value.integer.value[0]);
        dest = ev.value.integer.value;
        break;

    case SNDRV_CTL_ELEM_TYPE_BYTES:
        if (mixer_ctl_is_access_tlv_rw(ctl)) {
            struct snd_ctl_tlv *tlv;
            int ret = 0;

            if (count > SIZE_MAX - sizeof(*tlv))
                return -EINVAL;
            tlv = calloc(1, sizeof(*tlv) + count);
            if (!tlv)
                return -ENOMEM;
            tlv->numid  = ctl->info->id.numid;
            tlv->length = count;
            memcpy(tlv->tlv, array, count);
            ret = ioctl(ctl->mixer->fd, SNDRV_CTL_IOCTL_TLV_WRITE, tlv);
            free(tlv);
            return ret;
        } else {
            size = sizeof(ev.value.bytes.data[0]);
            dest = ev.value.bytes.data;
        }
        break;

    case SNDRV_CTL_ELEM_TYPE_IEC958:
        size = sizeof(ev.value.iec958);
        dest = &ev.value.iec958;
        break;

    default:
        return -EINVAL;
    }

    memcpy(dest, array, size * count);
    return ioctl(ctl->mixer->fd, SNDRV_CTL_IOCTL_ELEM_WRITE, &ev);
}

/* --------------------------------------------------------------------
 * Range helpers
 * -------------------------------------------------------------------- */
int mixer_ctl_get_range_min(struct mixer_ctl *ctl)
{
    if (!ctl || (ctl->info->type != SNDRV_CTL_ELEM_TYPE_INTEGER))
        return -EINVAL;

    return (int)ctl->info->value.integer.min;
}

int mixer_ctl_get_range_max(struct mixer_ctl *ctl)
{
    if (!ctl || (ctl->info->type != SNDRV_CTL_ELEM_TYPE_INTEGER))
        return -EINVAL;

    return (int)ctl->info->value.integer.max;
}

/* --------------------------------------------------------------------
 * Enum helpers
 * -------------------------------------------------------------------- */
unsigned int mixer_ctl_get_num_enums(struct mixer_ctl *ctl)
{
    if (!ctl)
        return 0;

    return ctl->info->value.enumerated.items;
}

const char *mixer_ctl_get_enum_string(struct mixer_ctl *ctl,
                                      unsigned int enum_id)
{
    if (!ctl ||
        (ctl->info->type != SNDRV_CTL_ELEM_TYPE_ENUMERATED) ||
        (enum_id >= ctl->info->value.enumerated.items) ||
        !ctl->ename)
        return NULL;

    return (const char *)ctl->ename[enum_id];
}

int mixer_ctl_set_enum_by_string(struct mixer_ctl *ctl, const char *string)
{
    unsigned int i, num_enums;
    struct snd_ctl_elem_value ev;
    int ret;

    if (!ctl || (ctl->info->type != SNDRV_CTL_ELEM_TYPE_ENUMERATED))
        return -EINVAL;

    num_enums = ctl->info->value.enumerated.items;
    for (i = 0; i < num_enums; i++) {
        if (!strcmp(string, ctl->ename[i])) {
            memset(&ev, 0, sizeof(ev));
            ev.value.enumerated.item[0] = i;
            ev.id.numid = ctl->info->id.numid;
            ret = ioctl(ctl->mixer->fd, SNDRV_CTL_IOCTL_ELEM_WRITE, &ev);
            if (ret < 0)
                return ret;
            return 0;
        }
    }

    return -EINVAL;
}

/* --------------------------------------------------------------------
 * Event API
 * -------------------------------------------------------------------- */

/** Subscribe/unsubscribe for mixer events.
 *  @param subscribe  Non-zero to subscribe, 0 to unsubscribe.
 *  @returns 0 on success, -1 on failure.
 */
int mixer_subscribe_events(struct mixer *mixer, int subscribe)
{
    if (ioctl(mixer->fd, SNDRV_CTL_IOCTL_SUBSCRIBE_EVENTS, &subscribe) < 0)
        return -1;
    return 0;
}

/** Wait for mixer events.
 *  @param timeout  Timeout in milliseconds; -1 = block forever.
 *  @returns 1 on event ready, 0 on timeout, -errno on error.
 */
int mixer_wait_event(struct mixer *mixer, int timeout)
{
    struct pollfd pfd;

    pfd.fd     = mixer->fd;
    pfd.events = POLLIN | POLLOUT | POLLERR | POLLNVAL;

    for (;;) {
        int err;
        err = poll(&pfd, 1, timeout);
        if (err < 0)
            return -errno;
        if (!err)
            return 0;
        if (pfd.revents & (POLLERR | POLLNVAL))
            return -EIO;
        if (pfd.revents & (POLLIN | POLLOUT))
            return 1;
    }
}

/** Consume (drain) one pending event from the mixer fd.
 *  @returns 0 on success, -errno on failure.
 */
int mixer_consume_event(struct mixer *mixer)
{
    struct snd_ctl_event ev;
    ssize_t count = read(mixer->fd, &ev, sizeof(ev));
    return (count >= 0) ? 0 : -errno;
}

/** Read a typed event from the mixer, filtered by event_mask.
 *
 * Derived from the binary (mixer_read_event @ 0x2D28):
 *   - Allocates a 72-byte (sizeof snd_ctl_event) buffer.
 *   - Reads from the mixer fd in a loop until either an event matching
 *     (*ev.type == 0 is ELEM) AND (ev.data.elem.mask & event_mask) is
 *     non-zero, or an error / EOF occurs.
 *   - Returns the heap-allocated event; caller must free().
 *
 * @param mixer       Open mixer handle.
 * @param event_mask  Bitmask of interesting SNDRV_CTL_EVENT_MASK_* bits.
 *                    Pass ~0 to accept any event.
 * @returns Heap-allocated mixer_event on success, NULL on error / no match.
 */
struct mixer_event *mixer_read_event(struct mixer *mixer, int event_mask)
{
    struct mixer_event *me;

    if (!mixer)
        return NULL;

    me = calloc(1, sizeof(*me));
    if (!me)
        return NULL;

    /* First read — must succeed and return at least one byte */
    if (read(mixer->fd, &me->ev, sizeof(me->ev)) < 1)
        goto fail;

    /* Keep reading until we find an event that matches the mask.
     * The orininal binary checks:
     *   while (*ev == 0 || (ev[1] & event_mask) == 0)  — re-read
     * snd_ctl_event.type == 0 is SNDRV_CTL_EVENT_ELEM.
     * snd_ctl_event.data.elem.mask is the second 32-bit word.
     */
    while (me->ev.type != SNDRV_CTL_EVENT_ELEM ||
           !(me->ev.data.elem.mask & (unsigned int)event_mask)) {
        if (read(mixer->fd, &me->ev, sizeof(me->ev)) <= 0)
            goto fail;
    }

    return me;

fail:
    free(me);
    return NULL;
}
