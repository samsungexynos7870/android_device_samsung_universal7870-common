/*
 * Copyright (C) 2021 The LineageOS Project
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

#include <errno.h>
#include <unistd.h>
#include <sound/asound.h>
#include <audio_route.h>
#include <tinyalsa/asoundlib.h>
#include <log/log.h>
#include <utils/Errors.h>
#include "libaudiopoxy_shim.h"

// extern "C" int get_dai_link(int /*param_1*/,int /*param_2*/) {
/*if (param_1 != 0) {
  ALOGV(2,"audio_route","requested PCM for %d",param_2);
}*/
//  return -1;
//}

/* Get pcm-dai information */
extern "C" int get_dai_link(struct audio_route *ar, enum pcm_dai_link dai_link)
{
  if (ar)
    ALOGV("requested PCM for %d", dai_link);

  return -1;
}

/**
 * Return whether an audio route is missing a mixer control
 *
 * @param ar the audio route to check
 *
 * @return 1 if the audio route is missing a mixer control, 0 otherwise
 */
extern "C" int audio_route_missing_ctl(struct audio_route *ar)
{
  /* Returns 1 if the audio route is missing a mixer control,
   * 0 otherwise.
   */
  if (!ar)
  {
    ALOGV("invalid audio_route");
    return 0;
  }

  return ar->missing;
}

extern "C"
{
int audio_route_get_mixer(struct audio_route *ar) 
{
  return ar->mixer;
}
}
// struct mixer_ctl

// call example mixer_ctl = audio_route_get_mixer_ctl(*(_DWORD *)(a1 + 220), "VSP data");
extern "C"
{

  // Assuming this function is defined in some other part of the audio library
  // int mixer_get_ctl_by_name(struct mixer *pMixer, const char *ctl);

  // struct mixer_ctl *mixer_get_ctl_by_name(struct mixer *mixer, const char *name);

  struct mixer_ctl *audio_route_get_mixer_ctl(struct audio_route *ar, const char *name)
{
  return mixer_get_ctl_by_name(ar->mixer, name);
}
}

// examples function calls
// mixer_ctl = audio_route_get_mixer_ctl(*(_DWORD *)(a1 + 220), "VSP data");
// v6 = audio_route_get_mixer_ctl(*(_DWORD *)(a1 + 220), "LRSM data");

// audio_values_apply_path(v6, v7, &voice_volume_index);


// audio_route: Apply path: media-speaker
extern "C" int audio_values_apply_path(struct audio_route *ar, const char *name, int *values) {

  unsigned int i;
  unsigned int length;
  struct mixer_path *path;
  
  path = path_get_by_name(ar, name);
  if (!path) {
    return -1; 
  }

  length = path->length;

  for (i = 0; i < length; i++) {
    struct mixer_ctl *ctl = index_to_ctl(ar, path->setting[i].ctl_index);

    if (path->setting[i].type == MIXER_CTL_TYPE_BYTE) {
      mixer_ctl_set_value(ctl, 0, values[i]);
    } else if (path->setting[i].type == MIXER_CTL_TYPE_ENUM) {
      mixer_ctl_set_value(ctl, 0, values[i]); 
    } else {
      mixer_ctl_set_array(ctl, &values[i], path->setting[i].num_values);
    }
  }
  // return 0;
  return -1;
}

/*extern "C" int audio_values_apply_path(struct audio_route *ar, const char *name,int values)
{
  // param_2 = name;
  int ctl;
  // undefined4 uVar2;
  struct mixer_path *path;
  // struct mixer_ctl *ctl;

  int iVar3;
  char *current_name;
  int iVar5;
  char *name;
  int *piVar7;
  uint type;
  char *pcVar9;
  uint uVar10;
  char *pcVar11;

  current_name = name;
  ALOGV(4,"audio_route","> %s : \"%s\"","audio_values_apply_path",name);
  if (param_1 == (undefined4 *)0x0) {
    ALOGV(6,"audio_route","invalid audio_route");
  }
  else {
    type = param_1[4];
    if (type != 0) {
      uVar10 = 0;
      piVar7 = (int *)(param_1[5] + 0xc);
      do {
        ctl = strcmp((char *)piVar7[-3],param_2);
        if (ctl == NULL) {
          if (piVar7 != (int *)0xc) {
            if (piVar7[-1] != 0) {
              pcVar11 = (char *)0x0;
              do {
                ctl = *piVar7;
                if (*(int *)(ctl + (int)pcVar11 * 0x1c + 0x10) == 1) {
                  pcVar9 = (char *)0x0;
                  do {
                    ctl = mixer_get_ctl_by_name
                                      (*param_1,*(undefined4 *)(ctl + (int)pcVar11 * 0x1c + 0x14))
                    ;
                    if (ctl != NULL) {
                      //Skip unsupported types
                      type = mixer_ctl_get_type(ctl);
                      if (3 < type) {
                        pcVar9 = (char *)mixer_ctl_get_name(ctl); //maybe path ?
                        uVar2 = 6;
                        name = "param : \"%s\" skip: not supported type";
                        goto LAB_00013afc; // goto error
                      }
                      iVar3 = mixer_ctl_get_num_values(ctl);
                      iVar5 = *(int *)(*piVar7 + (int)pcVar11 * 0x1c + 0x18);
                      if ((iVar5 == -1) || (iVar3 <= iVar5)) {
                        pcVar9 = (char *)mixer_ctl_get_name(ctl);
                        uVar2 = 6;
                        name = "param : \"%s\" skip: mixer index %d is out of range";
                        current_name = *(char **)(*piVar7 + (int)pcVar11 * 0x1c + 0x18);
                        goto LAB_00013afc;
                      }
                      pcVar9 = (char *)mixer_ctl_set_value(ctl,iVar5,
                                                           *(undefined4 *)
                                                            (param_3 + (int)pcVar11 * 4));
                      if (pcVar9 == (char *)0x0) {
                        if (DAT_00017000 != '\0') goto LAB_00013b00;
                        pcVar9 = (char *)mixer_ctl_get_name(ctl);
                        uVar2 = 2;
                        name = "param : \"%s\" id(%d) value %d";
                        current_name = *(char **)(*piVar7 + (int)pcVar11 * 0x1c + 0x18);
                      }
                      else {
                        current_name = (char *)mixer_ctl_get_name(ctl);
                        uVar2 = 6;
                        name = "param : Fail to set (%d) : \"%s\" id(%d) value %d";
                      }
                      goto LAB_00013afc;
                    }
                    current_name = pcVar9;
                    ALOGV(6,"audio_route",
                                        "param : \"%s\" doesn\'t exist, should be reloaded (%d)",
                                        *(undefined4 *)(*piVar7 + (int)pcVar11 * 0x1c + 0x14),pcVar9
                                       );
                    ctl = update_mixer_state(param_1);
                    if (ctl != NULL) {
                      usleep(10000);
                    }
                    ctl = *piVar7;
                    pcVar9 = pcVar9 + 1;
                  } while (pcVar9 < (char *)0x14);
                  pcVar9 = *(char **)(ctl + (int)pcVar11 * 0x1c + 0x14);
                  uVar2 = 6;
                  name = "param : \"%s\" couldn\'t be set";
                }
                else {
                  uVar2 = 6;
                  name = "%s : [%d] skip : support only param tag";
                  pcVar9 = "apply_param_id_values";
                  current_name = pcVar11;
                }
LAB_00013afc:
                ALOGV(uVar2,"audio_route",name,pcVar9,current_name);
LAB_00013b00:
                pcVar11 = pcVar11 + 1;
              } while (pcVar11 < (char *)piVar7[-1]);
            }
            return 0;
          }
          break;
        }
        uVar10 = uVar10 + 1;
        piVar7 = piVar7 + 4;
      } while (uVar10 < type);
    }

    // param 2 is path ???
    ALOGV("audio_route","unable to find path \'%s\'",param_2,current_name);
    ALOGE("unable to find path '%s'", name);
  }
  return -1;
}*/

/**
 * Exported function to set mixer control value
 *
 * @param param_1 mixer control name string
 * @param param_2 mixer control value
 *
 * This function is exported and called from JNI layer to set
 * mixer control value based on mixer control name string.
 *
 * This function first gets a mixer control pointer from
 * mixer control name string by calling mixer_get_ctl_by_name()
 * using the global mixer control list pointer DAT_00017004.
 * If DAT_00017004 is NULL (initialization failed), returns
 * 0xffffffff. Otherwise, it sets the mixer control value using
 * mixer_ctl_set_value() and returns the result of
 * mixer_ctl_set_value().
 */
// extern "C" int direct_mixer_set_value(const char *name, int value)
//{
//   int value; // local variable to store mixer control pointer or return value
//
//   // set local variable to input mixer control value
//   ALOGV(3,"audio_route","direct_mixer_set_value: %s, value: %d",name,value);
//   /* Check if mixer control list pointer is not NULL */
//   if (DAT_00017004 != (undefined4 *)0x0) {
//     /* Get mixer control pointer from mixer control name string */
//     value = mixer_get_ctl_by_name(*DAT_00017004,param_1);
//     /* Set mixer control value using mixer control pointer */
//     value = __ThumbV7PILongThunk_mixer_ctl_set_value(value,0,value);
//     /* Return result of mixer_ctl_set_value() */
//     return value;
//   }
//   /* If mixer control list is NULL, print error message and return 0xffffffff */
//   ALOGV(3,"audio_route","%s: No ar","direct_mixer_set_value",name);
//   return -1;
// } /* direct_mixer_set_value */

/**
 * Set an array of values to a mixer control
 *
 * @param param_1 Name of the mixer control
 * @param param_2 Value to set, a zero-terminated string
 * @param param_3 Length of param_2, or 0 to use strlen
 *
 * @return 0 on success, <0 on error
 */
// int direct_mixer_set_array(struct mixer *mixer, const char *name, size_t array_length)
//{
//   //int ctl;
//   //int iVar2;
//   // undefined4 uVar3;
//   //struct mixer_ctl *ctl;
//   //size_t sVar4;
//
//   //struct mixer_ctl *ctl;
//   //int status;
//   //const char *ctl_name;
//   //size_t name_length;
//
//   /* Log set value if debug is enabled */
//   if (DAT_00017000 != '\0') {
//     name_length = strlen(value_array);
//     ALOGV(3,"audio_route","%s: %s, value %s len %d","direct_mixer_set_array",mixer, value_array, name_length);
//   }
//   /* If there is no mixer, return error */
//   if (DAT_00017004 == (undefined4 *)0x0) {
//     ALOGV(3,"audio_route","%s: No ar","direct_mixer_set_array");
//     ctl = -1;
//   }
//   else {
//     /* Get control by name */
//     ctl = mixer_get_ctl_by_name(*DAT_00017004,param_1);
//     /* Return error if control not found */
//     if (ctl == NULL) {
//       ALOGV(6,"audio_route","%s: Unable to find mixer \"%s\"","direct_mixer_set_array"
//                           ,param_1);
//     }
//     /* If control found, set value */
//     else {
//       /* If length is not given, use strlen */
//       if (param_3 == 0) {
//         param_3 = strlen(param_2);
//       }
//       iVar2 = mixer_ctl_set_array(ctl,param_2,param_3);
//       /* If error, return error code */
//       if (iVar2 != 0) {
//         uVar3 = mixer_ctl_get_name(ctl);
//         ALOGV(6,"audio_route","param : Fail to set (%d) : \"%s\"",iVar2,uVar3);
//         return iVar2;
//       }
//     }
//     /* Return success */
//     ctl = 0;
//   }
//   return ctl;
// }

/*char * g22[67] = {
    "media",
    "camcorder",
    "recording",
    "interview",
    "meeting",
    "incall_nb",
    "incall_nb_extra_vol",
    "incall_wb",
    "incall_wb_extra_vol",
    "video_call",
    "volte_cp_nb",
    "volte_cp_nb_extra_vol",
    "volte_cp_wb",
    "volte_cp_wb_extra_vol",
    "volte_vt_cp_nb",
    "volte_vt_cp_wb",
    "volte_cp_evs",
    "volte_cp_evs_extra_vol",
    "volte_vt_cp_evs",
    "communication",
    "wificall_nb",
    "wificall_nb_extra_vol",
    "wificall_wb",
    "wificall_wb_extra_vol",
    "wificall_evs",
    "wificall_evs_extra_vol",
    "recognition",
    "bargein_samsung_engine",
    "bargein_external_engine",
    "dualmic_samsung_engine",
    "dualmic_external_engine",
    "loopback_packet",
    "loopback",
    "realtimeloopback",
    "loopback_codec",
    "tty_mode",
    "ap_tty_mode",
    "fm_radio",
    "call_forwarding_master",
    "incall-rec-uplink",
    "incall-normal-rec-uplink",
    "incall-rec-downlink",
    "incall-rec-uplink-and-downlink",
    "incall-rec-callmemo",
    "incall_default",
    "-none",
    "-handset",
    "-speaker",
    "-headset",
    "-headphone",
    "-speaker-headset",
    "-bt-sco-headset",
    "-speaker-bt-sco-headset",
    (char *)&g290,
    (char *)&g290,
    "-mic",
    "-2nd-mic",
    "-headset-mic",
    "-bt-sco-headset-in",
    "-handset-mic",
    "-speaker-mic",
    "-headphone-mic",
    "-dualmic",
    "-full-mic",
    "-hco-mic",
    "-vco-mic",
    "-fm-recording"
}; // 0x16048
char * g24[22] = {
    "-none",
    "-handset",
    "-speaker",
    "-headset",
    "-headphone",
    "-speaker-headset",
    "-bt-sco-headset",
    "-speaker-bt-sco-headset",
    (char *)&g290,
    (char *)&g290,
    "-mic",
    "-2nd-mic",
    "-headset-mic",
    "-bt-sco-headset-in",
    "-handset-mic",
    "-speaker-mic",
    "-headphone-mic",
    "-dualmic",
    "-full-mic",
    "-hco-mic",
    "-vco-mic",
    "-fm-recording"
}; // 0x160fc
int32_t * g128 = &g41; // 0x2f9f*/

/**
 * Read an event from a mixer control
 *
 * @param mixer Mixer control to read from
 * @param param_2 Mask of the events to look for (SND_CTL_EVENT_MASK_* constants)
 *
 * @return Pointer to the event read, or NULL on error or no event found
 */
extern "C" int *mixer_read_event(struct mixer *mixer, uint param_2)
{
  struct snd_ctl_event *ev = NULL; /* Event buffer */
  ssize_t count;                   /* Return value of read(2) */

  /* Check if mixer is valid and has a valid file descriptor */
  if (mixer && mixer->fd >= 0)
  {
    ev = (struct snd_ctl_event *)calloc(1, sizeof(*ev));

    if (ev)
    {
      /* Read event from the mixer file descriptor */
      count = read(mixer->fd, ev, sizeof(*ev));

      /* Loop until there are no more events */
      while (0 < count)
      {
        /* Check if the event is of the right type and has the right mask */
        if (ev->type == 0 && (ev->data.elem.mask & param_2) != 0)
        {
          /* Return the event */
          return (int *)ev;
        }

        /* Read next event */
        count = read(mixer->fd, ev, sizeof(*ev));
      }

      /* Free the event buffer */
      free(ev);
    }
  }

  /* Return NULL if no event was found or there was an error */
  return NULL;
}
