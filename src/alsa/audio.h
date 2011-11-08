/*
 * Copyright 2005-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _ALSA_AUDIO_H
#define _ALSA_AUDIO_H 1

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
    PLACEHOLDER1,
    PLACEHOLDER2,
    SND_PCM_FORMAT_MU_LAW,
    SND_PCM_FORMAT_A_LAW,
    SND_PCM_FORMAT_IMA_ADPCM,
    SND_PCM_FORMAT_MPEG,
    SND_PCM_FORMAT_LAST = SND_PCM_FORMAT_MPEG,
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

enum _snd_ctl_elem_iface
{
    SND_CTL_ELEM_IFACE_CARD = 0,
    SND_CTL_ELEM_IFACE_HWDEP,
    SND_CTL_ELEM_IFACE_MIXER,
    SND_CTL_ELEM_IFACE_PCM,
    SND_CTL_ELEM_IFACE_RAWMIDI,
    SND_CTL_ELEM_IFACE_TIMER,
    SND_CTL_ELEM_IFACE_SEQUENCER
};

enum {
   SND_PCM_NONBLOCK = 1,
   SND_PCM_ASYNC
};

enum {
   SND_CTL_NONBLOCK = 1,
   SND_CTL_ASYNC
};

typedef struct _snd_pcm_channel_area
{
   void *addr;
   unsigned int	first;
   unsigned int	step;
} snd_pcm_channel_area_t;

typedef struct _snd_ctl snd_ctl_t;
typedef struct _snd_pcm snd_pcm_t;
typedef struct _snd_pcm_hw_params snd_pcm_hw_params_t;
typedef struct _snd_pcm_sw_params snd_pcm_sw_params_t;
typedef long snd_pcm_sframes_t;
typedef unsigned long snd_pcm_uframes_t;

typedef void snd_hctl_t;
typedef void snd_pcm_info_t;
typedef void snd_hctl_elem_t;
typedef void snd_ctl_elem_value_t;
typedef void snd_ctl_elem_list_t;
typedef void snd_ctl_elem_id_t;
typedef void snd_ctl_elem_info_t;
typedef enum _snd_ctl_elem_iface snd_ctl_elem_iface_t;


typedef void (*snd_lib_error_handler_t)(const char *, int, const char *, int, const char *,...);

typedef int (*snd_pcm_open_proc)(snd_pcm_t **, const char *, snd_pcm_stream_t, int);
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
typedef size_t(*snd_pcm_info_sizeof_proc)(void);
typedef int (*snd_pcm_info_proc)(snd_pcm_t *, snd_pcm_info_t *);
typedef const char *(*snd_pcm_info_get_subdevice_name_proc)(const snd_pcm_info_t *);
typedef unsigned int (*snd_pcm_info_get_subdevice_proc)(const snd_pcm_info_t *);
typedef unsigned int (*snd_pcm_info_get_subdevices_avail_proc)(const snd_pcm_info_t *);
typedef unsigned int (*snd_pcm_info_get_subdevices_count_proc)(const snd_pcm_info_t *);
typedef void (*snd_pcm_info_set_subdevice_proc)(snd_pcm_info_t *, unsigned int);
typedef void (*snd_pcm_info_set_device_proc)(snd_pcm_info_t *, unsigned int);
typedef void (*snd_pcm_info_set_stream_proc)(snd_pcm_info_t *, snd_pcm_stream_t);
typedef const char *(*snd_pcm_info_get_name_proc)(const snd_pcm_info_t *);
typedef const char *(*snd_asoundlib_version_proc)(void);

typedef int (*snd_card_get_name_proc)(int, char **);
typedef int (*snd_card_get_longname_proc)(int, char **);
typedef int (*snd_card_get_index_proc)(const char *);

typedef int (*snd_card_next_proc)(int *);
typedef int (*snd_pcm_hw_params_malloc_proc)(snd_pcm_hw_params_t **);
typedef void (*snd_pcm_hw_params_free_proc)(snd_pcm_hw_params_t *);
typedef size_t (*snd_pcm_hw_params_sizeof_proc)(void);
typedef int (*snd_pcm_hw_params_proc)(snd_pcm_t *, snd_pcm_hw_params_t *);
typedef int (*snd_pcm_hw_params_any_proc)(snd_pcm_t *, snd_pcm_hw_params_t *);
typedef int (*snd_pcm_hw_params_set_access_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_access_t);
typedef int (*snd_pcm_hw_params_set_format_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_format_t);
typedef int (*snd_pcm_hw_params_set_rate_resample_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int);
typedef int (*snd_pcm_hw_params_set_rate_near_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int *, int *);
typedef int (*snd_pcm_hw_params_set_channels_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int);
typedef int (*snd_pcm_hw_params_test_channels_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int);
typedef int (*snd_pcm_hw_params_get_channels_proc)(snd_pcm_t *, unsigned int*);
typedef int (*snd_pcm_hw_params_set_buffer_size_near_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_uframes_t *);
typedef int (*snd_pcm_hw_params_set_periods_near_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int *, int *);
typedef int (*snd_pcm_hw_params_set_periods_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int, int);
typedef int (*snd_pcm_hw_params_can_pause_proc)(const snd_pcm_hw_params_t *);
typedef int (*snd_pcm_hw_params_can_resume_proc)(const snd_pcm_hw_params_t *);
typedef int (*snd_pcm_hw_params_can_drain_proc)(const snd_pcm_hw_params_t *);

typedef size_t (*snd_pcm_sw_params_sizeof_proc)(void);
typedef int (*snd_pcm_sw_params_current_proc)(snd_pcm_t *, snd_pcm_sw_params_t *);
typedef int (*snd_pcm_sw_params_proc)(snd_pcm_t *, snd_pcm_sw_params_t *);
typedef int (*snd_pcm_sw_params_set_avail_min_proc)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_uframes_t);
typedef int (*snd_pcm_sw_params_set_start_threshold_proc)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_uframes_t);

typedef snd_pcm_sframes_t(*snd_pcm_writei_proc)(snd_pcm_t *, const void *, snd_pcm_uframes_t);
typedef snd_pcm_sframes_t(*snd_pcm_mmap_writei_proc)(snd_pcm_t *, const void *, snd_pcm_uframes_t);
typedef snd_pcm_sframes_t(*snd_pcm_writen_proc)(snd_pcm_t *, void **, snd_pcm_uframes_t);
typedef snd_pcm_sframes_t(*snd_pcm_mmap_writen_proc)(snd_pcm_t *, void **, snd_pcm_uframes_t);
typedef int (*snd_pcm_mmap_begin_proc)(snd_pcm_t *, const snd_pcm_channel_area_t **, snd_pcm_uframes_t *, snd_pcm_uframes_t *);
typedef snd_pcm_sframes_t(*snd_pcm_mmap_commit_proc)(snd_pcm_t *, snd_pcm_uframes_t, snd_pcm_uframes_t);
typedef snd_pcm_sframes_t(*snd_pcm_readi_proc)(snd_pcm_t *, void *, snd_pcm_uframes_t);

typedef int (*snd_pcm_hw_params_get_sbits_proc)(const snd_pcm_hw_params_t *);
typedef int (*snd_pcm_hw_params_get_format_proc)(const snd_pcm_hw_params_t *, snd_pcm_format_t *);
typedef int (*snd_pcm_hw_params_test_buffer_size_proc)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_uframes_t);

typedef int (*snd_pcm_format_width_proc)(snd_pcm_format_t);
typedef int (*snd_pcm_hw_params_can_mmap_sample_resolution_proc)(const snd_pcm_hw_params_t *);
typedef int (*snd_pcm_hw_params_get_rate_numden_proc)(const snd_pcm_hw_params_t *, unsigned int *, unsigned int *);
typedef int (*snd_pcm_hw_params_get_buffer_size_proc)(const snd_pcm_hw_params_t *, snd_pcm_uframes_t *);
typedef int (*snd_pcm_hw_params_get_periods_proc)(const snd_pcm_hw_params_t *, unsigned int *, int *);
typedef int (*snd_pcm_hw_params_get_period_size_proc)(const snd_pcm_hw_params_t *,  	snd_pcm_uframes_t *, int*);
typedef snd_pcm_sframes_t (*snd_pcm_avail_update_proc)(snd_pcm_t *);
typedef snd_pcm_state_t (*snd_pcm_state_proc)(snd_pcm_t *);
typedef int (*snd_pcm_hwsync_proc)(snd_pcm_t *);
typedef int (*snd_pcm_delay_proc)(snd_pcm_t *, snd_pcm_sframes_t *);

typedef const char *(*snd_strerror_proc)(int);
typedef int (*snd_lib_error_set_handler_proc)(snd_lib_error_handler_t);

typedef int (*snd_hctl_open_proc)(snd_hctl_t **, const char *, int);
typedef int (*snd_hctl_close_proc)(snd_hctl_t *);
typedef int (*snd_hctl_wait_proc)(snd_hctl_t *, int);
typedef int (*snd_hctl_load_proc)(snd_hctl_t *);
typedef int (*snd_hctl_elem_write_proc)(snd_hctl_t *, snd_ctl_elem_value_t *);
typedef int (*snd_hctl_elem_read_proc)(snd_hctl_t *, snd_ctl_elem_value_t *);
typedef snd_hctl_elem_t *(*snd_hctl_last_elem_proc)(snd_hctl_t *);
typedef snd_hctl_elem_t *(*snd_hctl_find_elem_proc)(snd_hctl_t *, const snd_ctl_elem_id_t *);
typedef int (*snd_hctl_elem_info_proc)(snd_hctl_elem_t *, snd_ctl_elem_info_t *);

typedef int (*snd_ctl_open_proc)(snd_ctl_t **, const char *, int);
typedef int (*snd_ctl_close_proc)(snd_ctl_t *);
typedef int (*snd_ctl_pcm_info_proc)(snd_ctl_t *, snd_pcm_info_t *);
typedef void (*snd_ctl_elem_info_clear_proc)(snd_ctl_elem_info_t *);
typedef size_t (*snd_ctl_elem_info_sizeof_proc)(void);
typedef size_t (*snd_ctl_elem_value_sizeof_proc)(void);
typedef unsigned int (*snd_ctl_elem_info_get_count_proc)(const snd_ctl_elem_info_t *);
typedef int (*snd_ctl_pcm_next_device_proc)(snd_ctl_t *, int *);
typedef int (*snd_ctl_elem_list_malloc_proc)(snd_ctl_elem_list_t **);
typedef void (*snd_ctl_elem_value_clear_proc)(snd_ctl_elem_list_t *);
typedef void (*snd_ctl_elem_id_clear_proc)(snd_ctl_elem_id_t *);
typedef void (*snd_ctl_elem_value_set_interface_proc)(snd_ctl_elem_value_t *, snd_ctl_elem_iface_t);
typedef void (*snd_ctl_elem_value_set_name_proc)(snd_ctl_elem_value_t *, const char *);
typedef void (*snd_ctl_elem_value_set_index_proc)(snd_ctl_elem_value_t *, unsigned int);
typedef void (*snd_ctl_elem_value_set_integer_proc)(snd_ctl_elem_value_t *, unsigned int, long);
typedef int (*snd_ctl_elem_value_get_integer_proc)(const snd_ctl_elem_value_t *, unsigned int);
typedef size_t (*snd_ctl_elem_id_sizeof_proc)(void);
typedef void (*snd_ctl_elem_id_set_name_proc)(snd_ctl_elem_id_t *, const char *);
typedef void (*snd_ctl_elem_id_set_interface_proc)(snd_ctl_elem_id_t *, snd_ctl_elem_iface_t);


#endif /* _ALSA_AUDIO_H */

