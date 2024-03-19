struct mixer {
    int fd;
    struct snd_ctl_card_info card_info;
    struct snd_ctl_elem_info *elem_info;
    struct mixer_ctl *ctl;
    unsigned int count;
};

struct audio_route {
    struct mixer *mixer;
    unsigned int num_mixer_ctls;
    struct mixer_state *mixer_state;

    unsigned int mixer_path_size;
    unsigned int num_mixer_paths;
    struct mixer_path *mixer_path;
    int missing;
};


extern "C" {

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
    VTS_SEAMLESS_CATURE_LINK,
    CALL_REC_CAPTURE_LINK,
    FMRADIO_LINK,
    CAPTURE_CALLMIC_LINK,
    NUM_DAI_LINK,
};

/* Get pcm-dai information */
int get_dai_link(struct audio_route *ar, enum pcm_dai_link dai_link);

/* return number of missing control */
int audio_route_missing_ctl(struct audio_route *ar);

/* audio mixer getter (from decompiled audio route)*/
// int audio_route_get_mixer(int *param_1);

/* this needs to be fixed*/
// int audio_values_apply_path(int a1, int str, int a3);
// int mixer_get_ctl_by_name(struct mixer *pMixer, const char *ctl);

struct mixer* audio_route_get_mixer(struct audio_route* pAudioRoute);

// int audio_route_get_mixer_ctl(int *param_1);

int audio_values_apply_path(struct audio_route *ar, const char *name,int values);

int * mixer_read_event(struct mixer *mixer, uint param_2);

}  /* extern "C" */

