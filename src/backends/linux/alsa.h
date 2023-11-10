/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef _ASOUND_AUDIO_H
#define _ASOUND_AUDIO_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if 0
#ifndef RELEASE
# ifdef NDEBUG
#  undef NDEBUG
# endif
#endif
#endif

/* all taken from the ALSA project website */
typedef enum
{
    SND_PCM_STREAM_PLAYBACK = 0,
    SND_PCM_STREAM_CAPTURE,
    SND_PCM_STREAM_LAST = SND_PCM_STREAM_CAPTURE
} snd_pcm_stream_t;

typedef enum _snd_pcm_access {
    SND_PCM_ACCESS_MMAP_INTERLEAVED = 0,
    SND_PCM_ACCESS_MMAP_NONINTERLEAVED,
    SND_PCM_ACCESS_MMAP_COMPLEX,
    SND_PCM_ACCESS_RW_INTERLEAVED,
    SND_PCM_ACCESS_RW_NONINTERLEAVED,
    SND_PCM_ACCESS_LAST = SND_PCM_ACCESS_RW_NONINTERLEAVED
} snd_pcm_access_t;

typedef enum
{
    SND_PCM_FORMAT_UNKNOWN = -1,
    SND_PCM_FORMAT_S8 = 0,
    SND_PCM_FORMAT_U8,
    SND_PCM_FORMAT_S16_LE,
    SND_PCM_FORMAT_S16_BE,
    SND_PCM_FORMAT_U16_LE,
    SND_PCM_FORMAT_U16_BE,
    SND_PCM_FORMAT_S24_LE,
    SND_PCM_FORMAT_S24_BE,
    SND_PCM_FORMAT_U24_LE,
    SND_PCM_FORMAT_U24_BE,
    SND_PCM_FORMAT_S32_LE,
    SND_PCM_FORMAT_S32_BE,
    SND_PCM_FORMAT_U32_LE,
    SND_PCM_FORMAT_U32_BE,
    SND_PCM_FORMAT_FLOAT_LE,
    SND_PCM_FORMAT_FLOAT_BE,
    SND_PCM_FORMAT_FLOAT64_LE,
    SND_PCM_FORMAT_FLOAT64_BE,
    SND_PCM_FORMAT_IEC958_SUBFRAME_LE,
    SND_PCM_FORMAT_IEC958_SUBFRAME_BE,
    SND_PCM_FORMAT_MU_LAW,
    SND_PCM_FORMAT_A_LAW,
    SND_PCM_FORMAT_IMA_ADPCM,
    SND_PCM_FORMAT_MPEG,
    SND_PCM_FORMAT_GSM,
    SND_PCM_FORMAT_SPECIAL,
    SND_PCM_FORMAT_S24_3LE,
    SND_PCM_FORMAT_S24_3BE,
    SND_PCM_FORMAT_U24_3LE,
    SND_PCM_FORMAT_U24_3BE,
    SND_PCM_FORMAT_S20_3LE,
    SND_PCM_FORMAT_S20_3BE,
    SND_PCM_FORMAT_U20_3LE,
    SND_PCM_FORMAT_U20_3BE,
    SND_PCM_FORMAT_S18_3LE,
    SND_PCM_FORMAT_S18_3BE,
    SND_PCM_FORMAT_U18_3LE,
    SND_PCM_FORMAT_U18_3BE,
    SND_PCM_FORMAT_LAST = SND_PCM_FORMAT_U18_3BE,
#if __BYTE_ORDER == __LITTLE_ENDIAN
    SND_PCM_FORMAT_S16 = SND_PCM_FORMAT_S16_LE,
    SND_PCM_FORMAT_U16 = SND_PCM_FORMAT_U16_LE,
    SND_PCM_FORMAT_S24 = SND_PCM_FORMAT_S24_LE,
    SND_PCM_FORMAT_U24 = SND_PCM_FORMAT_U24_LE,
    SND_PCM_FORMAT_S32 = SND_PCM_FORMAT_S32_LE,
    SND_PCM_FORMAT_U32 = SND_PCM_FORMAT_U32_LE,
    SND_PCM_FORMAT_FLOAT = SND_PCM_FORMAT_FLOAT_LE,
    SND_PCM_FORMAT_FLOAT64 = SND_PCM_FORMAT_FLOAT64_LE,
#elif __BYTE_ORDER == __BIG_ENDIAN
    SND_PCM_FORMAT_S16 = SND_PCM_FORMAT_S16_BE,
    SND_PCM_FORMAT_U16 = SND_PCM_FORMAT_U16_BE,
    SND_PCM_FORMAT_S24 = SND_PCM_FORMAT_S24_BE,
    SND_PCM_FORMAT_U24 = SND_PCM_FORMAT_U24_BE,
    SND_PCM_FORMAT_S32 = SND_PCM_FORMAT_S32_BE,
    SND_PCM_FORMAT_U32 = SND_PCM_FORMAT_U32_BE,
    SND_PCM_FORMAT_FLOAT = SND_PCM_FORMAT_FLOAT_BE,
    SND_PCM_FORMAT_FLOAT64 = SND_PCM_FORMAT_FLOAT64_BE
#endif
} snd_pcm_format_t;

typedef enum _snd_pcm_state
{
    SND_PCM_STATE_OPEN = 0,
    SND_PCM_STATE_SETUP,
    SND_PCM_STATE_PREPARED,
    SND_PCM_STATE_RUNNING,
    SND_PCM_STATE_XRUN,
    SND_PCM_STATE_DRAINING,
    SND_PCM_STATE_PAUSED,
    SND_PCM_STATE_SUSPENDED,
    SND_PCM_STATE_DISCONNECTED
} snd_pcm_state_t;

typedef enum _snd_pcm_type 
{
    SND_PCM_TYPE_HW = 0,
    SND_PCM_TYPE_HOOKS,
    SND_PCM_TYPE_MULTI,
    SND_PCM_TYPE_FILE,
    SND_PCM_TYPE_NULL,
    SND_PCM_TYPE_SHM,
    SND_PCM_TYPE_INET,
    SND_PCM_TYPE_COPY,
    SND_PCM_TYPE_LINEAR,
    SND_PCM_TYPE_ALAW,
    SND_PCM_TYPE_MULAW,
    SND_PCM_TYPE_ADPCM,
    SND_PCM_TYPE_RATE,
    SND_PCM_TYPE_ROUTE,
    SND_PCM_TYPE_PLUG,
    SND_PCM_TYPE_SHARE,
    SND_PCM_TYPE_METER,
    SND_PCM_TYPE_MIX,
    SND_PCM_TYPE_DROUTE,
    SND_PCM_TYPE_LBSERVER,
    SND_PCM_TYPE_LINEAR_FLOAT,
    SND_PCM_TYPE_LADSPA,
    SND_PCM_TYPE_DMIX,
    SND_PCM_TYPE_JACK,
    SND_PCM_TYPE_DSNOOP,
    SND_PCM_TYPE_DSHARE,
    SND_PCM_TYPE_IEC958,
    SND_PCM_TYPE_SOFTVOL,
    SND_PCM_TYPE_IOPLUG,
    SND_PCM_TYPE_EXTPLUG,
    SND_PCM_TYPE_MMAP_EMUL
} snd_pcm_type_t;

enum {
   SND_PCM_NONBLOCK = 1,
   SND_PCM_ASYNC
};

enum {
   SND_CTL_NONBLOCK = 1,
   SND_CTL_ASYNC
};

enum snd_mixer_selem_regopt_abstract {
   SND_MIXER_SABSTRACT_NONE = 0,
   SND_MIXER_SABSTRACT_BASIC
};

typedef enum {
   SND_MIXER_SCHN_UNKNOWN = -1,
   SND_MIXER_SCHN_FRONT_LEFT = 0,
   SND_MIXER_SCHN_FRONT_RIGHT,
   SND_MIXER_SCHN_REAR_LEFT,
   SND_MIXER_SCHN_REAR_RIGHT,
   SND_MIXER_SCHN_FRONT_CENTER,
   SND_MIXER_SCHN_WOOFER,
   SND_MIXER_SCHN_SIDE_LEFT,
   SND_MIXER_SCHN_SIDE_RIGHT,
   SND_MIXER_SCHN_REAR_CENTER,
   SND_MIXER_SCHN_LAST = 31,
   SND_MIXER_SCHN_MONO = SND_MIXER_SCHN_FRONT_LEFT
} snd_mixer_selem_channel_id_t;

typedef struct _snd_pcm_channel_area
{
   void *addr;
   unsigned int	first;
   unsigned int	step;
} snd_pcm_channel_area_t;

typedef struct _snd_pcm snd_pcm_t;
typedef struct _snd_pcm_hw_params snd_pcm_hw_params_t;
typedef struct _snd_pcm_sw_params snd_pcm_sw_params_t;

typedef struct snd_mixer_selem_regopt
{
   int 	ver;
   enum snd_mixer_selem_regopt_abstract	abstract;
   const char *device;
   snd_pcm_t *playback_pcm;
   snd_pcm_t *capture_pcm;
} _snd_mixer_selem_regopt;

typedef struct _snd_mixer snd_mixer_t;
typedef struct _snd_mixer_elem_t snd_mixer_elem_t;
typedef struct _snd_mixer_selem_id_t snd_mixer_selem_id_t;
typedef struct _snd_mixer_class_t snd_mixer_class_t;

typedef long snd_pcm_sframes_t;
typedef unsigned long snd_pcm_uframes_t;

typedef void snd_hctl_t;
typedef void snd_ctl_t;
typedef void snd_pcm_info_t;
typedef void snd_hctl_elem_t;


typedef void (*snd_lib_error_handler_t)(const char *, int, const char *, int, const char *,...);

typedef int (*snd_pcm_open_proc)(snd_pcm_t **, const char *, snd_pcm_stream_t, int);
typedef const char* (*snd_pcm_name_proc)(snd_pcm_t *);
typedef int (*snd_pcm_wait_proc)(snd_pcm_t *, int);
typedef int (*snd_pcm_nonblock_proc)(snd_pcm_t *, int);
typedef int (*snd_pcm_prepare_proc)(snd_pcm_t *);
typedef int (*snd_pcm_resume_proc)(snd_pcm_t *);
typedef int (*snd_pcm_start_proc)(snd_pcm_t *);
typedef int (*snd_pcm_drain_proc)(snd_pcm_t *);
typedef int (*snd_pcm_close_proc)(snd_pcm_t *);
typedef int (*snd_pcm_pause_proc)(snd_pcm_t *, int);
typedef int (*snd_pcm_drop_proc)(snd_pcm_t *);

typedef int (*snd_pcm_info_malloc_proc)(snd_pcm_info_t **);
typedef void (*snd_pcm_info_free_proc)(snd_pcm_info_t *);
typedef int (*snd_pcm_info_proc)(snd_pcm_t *, snd_pcm_info_t *);
typedef int (*snd_ctl_open_proc)(snd_ctl_t **, const char *, int);
typedef int (*snd_ctl_close_proc)(snd_ctl_t *);
typedef int (*snd_ctl_pcm_info_proc)(snd_ctl_t *, snd_pcm_info_t *);
typedef const char *(*snd_pcm_info_get_subdevice_name_proc)(const snd_pcm_info_t *);
typedef unsigned int (*snd_pcm_info_get_subdevice_proc)(const snd_pcm_info_t *);
typedef unsigned int (*snd_pcm_info_get_subdevices_avail_proc)(const snd_pcm_info_t *);
typedef unsigned int (*snd_pcm_info_get_subdevices_count_proc)(const snd_pcm_info_t *);
typedef void (*snd_pcm_info_set_subdevice_proc)(snd_pcm_info_t *, unsigned int);
typedef void (*snd_pcm_info_set_device_proc)(snd_pcm_info_t *, unsigned int);
typedef void (*snd_pcm_info_set_stream_proc)(snd_pcm_info_t *, snd_pcm_stream_t);
typedef const char *(*snd_pcm_info_get_name_proc)(const snd_pcm_info_t *);
typedef const char *(*snd_asoundlib_version_proc)(void);

typedef int (*snd_config_update_proc)(void);
typedef int (*snd_config_update_free_global_proc)(void);

typedef int (*snd_card_get_name_proc)(int, char **);
typedef int (*snd_card_get_longname_proc)(int, char **);
typedef int (*snd_card_get_index_proc)(const char *);
typedef int (*snd_card_next_proc)(int *);
typedef char* (*snd_device_name_get_hint_proc)(const void*, const char*);
typedef int (*snd_device_name_hint_proc)(int, const char*, void***);
typedef int (*snd_device_name_free_hint_proc)(void**);

typedef int (*snd_pcm_hw_params_malloc_proc)(snd_pcm_hw_params_t **);
typedef void (*snd_pcm_hw_params_free_proc)(snd_pcm_hw_params_t *);
typedef int (*snd_pcm_hw_params_proc)(snd_pcm_t *, snd_pcm_hw_params_t *);
typedef int (*snd_pcm_hw_params_any_proc)(snd_pcm_t *, snd_pcm_hw_params_t *);
typedef int (*snd_pcm_hw_params_set_access_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_access_t);
typedef int (*snd_pcm_hw_params_set_format_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_format_t);
typedef int (*snd_pcm_hw_params_set_rate_resample_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int);
typedef int (*snd_pcm_hw_params_set_rate_near_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int *, int *);
typedef int (*snd_pcm_hw_params_set_channels_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int);
typedef int (*snd_pcm_hw_params_set_channels_minmax_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int*, unsigned int*);
typedef int (*snd_pcm_hw_params_test_channels_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int);
typedef int (*snd_pcm_hw_params_get_channels_proc)(snd_pcm_t *, unsigned int*);
typedef int (*snd_pcm_hw_params_get_channels_min_proc)(const snd_pcm_hw_params_t *, unsigned int*);
typedef int (*snd_pcm_hw_params_get_channels_max_proc)(const snd_pcm_hw_params_t *, unsigned int*);
typedef int (*snd_pcm_hw_params_set_period_size_near_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_uframes_t *, int*);
typedef int (*snd_pcm_hw_params_set_buffer_size_near_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_uframes_t *);
typedef int (*snd_pcm_hw_params_get_buffer_size_max_proc)(const snd_pcm_hw_params_t *, snd_pcm_uframes_t *);
typedef int (*snd_pcm_hw_params_set_buffer_time_near_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int *, int *);
typedef int (*snd_pcm_hw_params_set_periods_near_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int *, int *);
typedef int (*snd_pcm_hw_params_set_periods_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int, int);
typedef int (*snd_pcm_hw_params_get_period_size_max_proc)(snd_pcm_hw_params_t *, snd_pcm_uframes_t *, int *);
typedef int (*snd_pcm_hw_params_can_pause_proc)(const snd_pcm_hw_params_t *);
typedef int (*snd_pcm_hw_params_can_resume_proc)(const snd_pcm_hw_params_t *);
typedef int (*snd_pcm_hw_params_can_drain_proc)(const snd_pcm_hw_params_t *);

typedef int (*snd_pcm_sw_params_malloc_proc)(snd_pcm_sw_params_t **);
typedef void (*snd_pcm_sw_params_free_proc)(snd_pcm_sw_params_t *);
typedef int (*snd_pcm_sw_params_current_proc)(snd_pcm_t *, snd_pcm_sw_params_t *);
typedef int (*snd_pcm_sw_params_proc)(snd_pcm_t *, snd_pcm_sw_params_t *);
typedef int (*snd_pcm_sw_params_set_avail_min_proc)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_uframes_t);
typedef int (*snd_pcm_sw_params_set_start_threshold_proc)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_uframes_t);
typedef int (*snd_pcm_sw_params_set_stop_threshold_proc)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_uframes_t);
typedef int (*snd_pcm_sw_params_set_silence_size_proc)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_uframes_t);
typedef int (*snd_pcm_sw_params_set_silence_threshold_proc)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_uframes_t);

typedef snd_pcm_sframes_t(*snd_pcm_writei_proc)(snd_pcm_t *, const void *, snd_pcm_uframes_t);
typedef snd_pcm_sframes_t(*snd_pcm_mmap_writei_proc)(snd_pcm_t *, const void *, snd_pcm_uframes_t);
typedef snd_pcm_sframes_t(*snd_pcm_writen_proc)(snd_pcm_t *, void **, snd_pcm_uframes_t);
typedef snd_pcm_sframes_t(*snd_pcm_mmap_writen_proc)(snd_pcm_t *, void **, snd_pcm_uframes_t);
typedef int (*snd_pcm_mmap_begin_proc)(snd_pcm_t *, const snd_pcm_channel_area_t **, snd_pcm_uframes_t *, snd_pcm_uframes_t *);
typedef snd_pcm_sframes_t(*snd_pcm_mmap_commit_proc)(snd_pcm_t *, snd_pcm_uframes_t, snd_pcm_uframes_t);
typedef snd_pcm_sframes_t(*snd_pcm_readi_proc)(snd_pcm_t *, void *, snd_pcm_uframes_t);
typedef snd_pcm_sframes_t(*snd_pcm_readn_proc)(snd_pcm_t *, void **, snd_pcm_uframes_t);
typedef snd_pcm_sframes_t(*snd_pcm_mmap_readi_proc)(snd_pcm_t *, void *, snd_pcm_uframes_t);
typedef snd_pcm_sframes_t(*snd_pcm_mmap_readn_proc)(snd_pcm_t *, void **, snd_pcm_uframes_t);

typedef int (*snd_pcm_hw_params_get_sbits_proc)(const snd_pcm_hw_params_t *);
typedef int (*snd_pcm_hw_params_get_format_proc)(const snd_pcm_hw_params_t *, snd_pcm_format_t *);
typedef int (*snd_pcm_hw_params_test_buffer_size_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_uframes_t);

typedef int (*snd_pcm_format_width_proc)(snd_pcm_format_t);
typedef int (*snd_pcm_hw_params_can_mmap_sample_resolution_proc)(const snd_pcm_hw_params_t *);
typedef int (*snd_pcm_hw_params_get_rate_numden_proc)(const snd_pcm_hw_params_t *, unsigned int *, unsigned int *);
typedef int (*snd_pcm_hw_params_get_rate_min_proc)(const snd_pcm_hw_params_t *, unsigned int *, int *);
typedef int (*snd_pcm_hw_params_get_rate_max_proc)(const snd_pcm_hw_params_t *, unsigned int *, int *);
typedef int (*snd_pcm_hw_params_get_buffer_size_proc)(const snd_pcm_hw_params_t *, snd_pcm_uframes_t *);
typedef int (*snd_pcm_hw_params_get_periods_proc)(const snd_pcm_hw_params_t *, unsigned int *, int *);
typedef int (*snd_pcm_hw_params_get_periods_min_proc)(const snd_pcm_hw_params_t *, unsigned int *, int *);
typedef int (*snd_pcm_hw_params_get_periods_max_proc)(const snd_pcm_hw_params_t *, unsigned int *, int *);
typedef int (*snd_pcm_hw_params_get_period_size_proc)(const snd_pcm_hw_params_t *,  	snd_pcm_uframes_t *, int*);
typedef snd_pcm_sframes_t (*snd_pcm_avail_update_proc)(snd_pcm_t *);
typedef snd_pcm_sframes_t (*snd_pcm_avail_proc)(snd_pcm_t *);
typedef snd_pcm_state_t (*snd_pcm_state_proc)(snd_pcm_t *);
typedef int (*snd_pcm_hwsync_proc)(snd_pcm_t *);
typedef int (*snd_pcm_delay_proc)(snd_pcm_t *, snd_pcm_sframes_t *);

typedef const char *(*snd_strerror_proc)(int);
typedef int (*snd_lib_error_set_handler_proc)(snd_lib_error_handler_t);

typedef int (*snd_hctl_open_proc)(snd_hctl_t **, const char *, int);
typedef int (*snd_hctl_close_proc)(snd_hctl_t *);
typedef int (*snd_hctl_wait_proc)(snd_hctl_t *, int);
typedef int (*snd_hctl_load_proc)(snd_hctl_t *);
typedef snd_hctl_elem_t *(*snd_hctl_last_elem_proc)(snd_hctl_t *);


typedef ssize_t (*snd_pcm_frames_to_bytes_proc)(snd_pcm_t *, snd_pcm_sframes_t);
typedef snd_pcm_type_t (*snd_pcm_type_proc)(snd_pcm_t *);
typedef int (*snd_pcm_recover_proc)(snd_pcm_t *, int, int);
typedef snd_pcm_stream_t (*snd_pcm_stream_proc)(snd_pcm_t *);
typedef snd_pcm_sframes_t (*snd_pcm_rewind_proc)(snd_pcm_t *, snd_pcm_uframes_t);
typedef snd_pcm_sframes_t (*snd_pcm_forward_proc)(snd_pcm_t *, snd_pcm_uframes_t);
typedef void snd_output_t;
typedef int (*snd_pcm_dump_proc)(snd_pcm_t *, snd_output_t *);
typedef int (*snd_output_stdio_attach_proc)(snd_output_t **, FILE*, int);


typedef int (*snd_mixer_open_proc)(snd_mixer_t **, int);
typedef int (*snd_mixer_close_proc)(snd_mixer_t *);
typedef int (*snd_mixer_load_proc)(snd_mixer_t *);
typedef int (*snd_mixer_attach_hctl_proc)(snd_mixer_t *mixer, snd_hctl_t*);
typedef int (*snd_mixer_detach_hctl_proc)(snd_mixer_t *mixer, snd_hctl_t*);
typedef snd_mixer_elem_t* (*snd_mixer_first_elem_proc)(snd_mixer_t*);
typedef snd_mixer_elem_t* (*snd_mixer_elem_next_proc)(snd_mixer_elem_t *);
typedef int (*snd_mixer_selem_id_malloc_proc)(snd_mixer_selem_id_t **);
typedef void (*snd_mixer_selem_id_free_proc)(snd_mixer_selem_id_t *);
typedef void (*snd_mixer_selem_id_set_index_proc)(snd_mixer_selem_id_t *, unsigned int);
typedef snd_mixer_elem_t* (*snd_mixer_find_selem_proc)(snd_mixer_t *, const snd_mixer_selem_id_t *);
typedef void (*snd_mixer_selem_id_set_name_proc)(snd_mixer_selem_id_t *, const char *);
typedef int (*snd_mixer_selem_register_proc)(snd_mixer_t*, struct snd_mixer_selem_regopt*, snd_mixer_class_t**);
typedef int (*snd_mixer_selem_has_playback_volume_proc)(snd_mixer_elem_t*);
typedef int (*snd_mixer_selem_get_playback_volume_proc)(snd_mixer_elem_t*,  	snd_mixer_selem_channel_id_t, long*);
typedef int (*snd_mixer_selem_get_playback_dB_proc)(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, long*);
typedef int (*snd_mixer_selem_set_playback_volume_all_proc)(snd_mixer_elem_t*, long);
typedef int (*snd_mixer_selem_get_playback_volume_range_proc)(snd_mixer_elem_t*, long*, long*);
typedef int (*snd_mixer_selem_get_playback_dB_range_proc)(snd_mixer_elem_t*, long*, long*);
typedef int (*snd_mixer_selem_ask_playback_vol_dB_proc)(snd_mixer_elem_t*, long, long*);
typedef int (*snd_mixer_selem_ask_playback_dB_vol_proc)(snd_mixer_elem_t *, long, int, long*);
typedef int (*snd_mixer_selem_has_capture_volume_proc)(snd_mixer_elem_t *);
typedef int (*snd_mixer_selem_get_capture_volume_proc)(snd_mixer_elem_t*, snd_mixer_selem_channel_id_t, long*);
typedef int (*snd_mixer_selem_get_capture_dB_proc)(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, long *);
typedef int (*snd_mixer_selem_set_capture_volume_proc)(snd_mixer_elem_t*, snd_mixer_selem_channel_id_t, long);
typedef int (*snd_mixer_selem_set_capture_volume_all_proc)(snd_mixer_elem_t*, long);
typedef int (*snd_mixer_selem_get_capture_volume_range_proc)(snd_mixer_elem_t*, long*, long*);
typedef int (*snd_mixer_selem_get_capture_dB_range_proc)(snd_mixer_elem_t*, long*, long*);
typedef int (*snd_mixer_selem_ask_capture_vol_dB_proc)(snd_mixer_elem_t*, long, long*);
typedef int (*snd_mixer_selem_ask_capture_dB_vol_proc)(snd_mixer_elem_t *, long, int, long*);
typedef void (*snd_mixer_selem_get_id_proc)(snd_mixer_elem_t*, snd_mixer_selem_id_t*);
typedef const char*(*snd_mixer_selem_id_get_name_proc)(const snd_mixer_selem_id_t*);


typedef int (*snd_mixer_callback_t)(snd_mixer_t *, unsigned int, snd_mixer_elem_t *);
typedef void (*snd_mixer_set_callback_proc)(snd_mixer_t *, snd_mixer_callback_t);
typedef void (*snd_mixer_set_callback_private_proc)(snd_mixer_t *, void *);
typedef void* (*snd_mixer_get_callback_private_proc)(const snd_mixer_t *);
typedef int (*snd_mixer_handle_events_proc)(snd_mixer_t * );	

typedef int (*snd_pcm_hw_params_set_period_wakeup_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* _ASOUND_AUDIO_H */

