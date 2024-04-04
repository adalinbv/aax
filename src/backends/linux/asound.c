/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_ASSERT_H
#include <assert.h>
#endif
#include <stdio.h>
#include <errno.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
# include <string.h>		/* strstr, strncmp */
#endif
#include <stdarg.h>		/* va_start */

#include <xml.h>

#include <base/timer.h>		/* for msecSleep */
#include <base/dlsym.h>
#include <base/logging.h>
#include <base/memory.h>
#include <base/types.h>

#include <ringbuffer.h>
#include <arch.h>
#include <api.h>

#include <dsp/effects.h>
#include <software/renderer.h>
#include "device.h"
#include "audio.h"
#include "alsa.h"

#define TIMER_BASED		false
#define MAX_ID_STRLEN		64

#define DEFAULT_DEVNUM		0
#define DEFAULT_IFNUM		0
#define DEFAULT_PERIODS		2
#define DEFAULT_OUTPUT_RATE	48000
#define DEFAULT_DEVNAME_OLD     "front:"AAX_MKSTR(DEFAULT_DEVNUM) \
                                       ","AAX_MKSTR(DEFAULT_IFNUM)
#define DEFAULT_DEVNAME		"default"
#define DEFAULT_HWDEVNAME	"hw:0"
#define DEFAULT_RENDERER	"ALSA"

#define ALSA_TIE_FUNCTION(a)	if ((TIE_FUNCTION(a)) == 0) printf("%s\n", #a)

_aaxDriverDetect _aaxALSADriverDetect;
static _aaxDriverNewHandle _aaxALSADriverNewHandle;
static _aaxDriverFreeHandle _aaxALSADriverFreeHandle;
static _aaxDriverGetDevices _aaxALSADriverGetDevices;
static _aaxDriverGetInterfaces _aaxALSADriverGetInterfaces;
static _aaxDriverConnect _aaxALSADriverConnect;
static _aaxDriverDisconnect _aaxALSADriverDisconnect;
static _aaxDriverSetup _aaxALSADriverSetup;
static _aaxDriverPlaybackCallback _aaxALSADriverPlayback;
static _aaxDriverCaptureCallback _aaxALSADriverCapture;
static _aaxDriverSetName _aaxALSADriverSetName;
static _aaxDriverGetName _aaxALSADriverGetName;
static _aaxDriverRender _aaxALSADriverRender;
static _aaxDriverThread _aaxALSADriverThread;
static _aaxDriverState _aaxALSADriverState;
static _aaxDriverParam _aaxALSADriverParam;
static _aaxDriverLog _aaxALSADriverLog;

static char _alsa_id_str[MAX_ID_STRLEN+1] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxALSADriverBackend =
{
   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_alsa_id_str,

   (_aaxDriverRingBufferCreate *)&_aaxRingBufferCreate,
   (_aaxDriverRingBufferDestroy *)&_aaxRingBufferFree,

   (_aaxDriverDetect *)&_aaxALSADriverDetect,
   (_aaxDriverNewHandle *)&_aaxALSADriverNewHandle,
   (_aaxDriverFreeHandle *)&_aaxALSADriverFreeHandle,
   (_aaxDriverGetDevices *)&_aaxALSADriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxALSADriverGetInterfaces,

   (_aaxDriverSetName *)&_aaxALSADriverSetName,
   (_aaxDriverGetName *)&_aaxALSADriverGetName,
   (_aaxDriverRender *)&_aaxALSADriverRender,
   (_aaxDriverThread *)&_aaxALSADriverThread,

   (_aaxDriverConnect *)&_aaxALSADriverConnect,
   (_aaxDriverDisconnect *)&_aaxALSADriverDisconnect,
   (_aaxDriverSetup *)&_aaxALSADriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxALSADriverCapture,
   (_aaxDriverPlaybackCallback *)&_aaxALSADriverPlayback,
   NULL,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,
   NULL,

   ( _aaxDriverGetSetSources*)_aaxSoftwareDriverGetSetSources,

   (_aaxDriverState *)&_aaxALSADriverState,
   (_aaxDriverParam *)&_aaxALSADriverParam,
   (_aaxDriverLog *)&_aaxALSADriverLog
};

typedef struct
{
    void *handle;
    char *name;
    char *devname;
    char *default_name[2];
    int default_devnum;
    int devnum;

    _aaxDriverPlaybackCallback *play;
    _aaxRenderer *render;
    snd_mixer_t *mixer;
    snd_hctl_t *hctl;
    snd_pcm_t *pcm;

    float volumeCur, volumeInit;
    float volumeMin, volumeMax;
    float volumeStep, hwgain;
    char *outMixer;		/* mixer element name for output volume */

    float latency;
    float frequency_hz;
    float refresh_rate;

    float padding;		/* for sensor clock drift correction   */
    size_t threshold;		/* sensor buffer threshold for padding */

    unsigned int no_tracks;
    unsigned int no_periods;
    size_t period_frames;
    ssize_t period_frames_actual;
    size_t max_frames;
    size_t buf_len;
    int mode;

    char pause;
    char can_pause;
    char use_mmap;
    char bits_sample;
    char interleaved;
    char playing;
    char shared;
    char shared_volume;
    char use_timer;

    _batch_cvt_to_proc cvt_to;
    _batch_cvt_from_proc cvt_from;
    _batch_cvt_to_intl_proc cvt_to_intl;
    _batch_cvt_from_intl_proc cvt_from_intl;

    void **ptr;
    char **scratch;

    char *ifname[2];

    float PID[1];
    float target[3];

   /* capabilities */
   unsigned int min_frequency;
   unsigned int max_frequency;
   unsigned int min_periods;
   unsigned int max_periods;
   unsigned int min_tracks;
   unsigned int max_tracks;

} _driver_t;

DECL_FUNCTION(snd_pcm_open);
DECL_FUNCTION(snd_pcm_close);
DECL_FUNCTION(snd_pcm_wait);
DECL_FUNCTION(snd_pcm_nonblock);
DECL_FUNCTION(snd_pcm_prepare);
DECL_FUNCTION(snd_pcm_pause);
DECL_FUNCTION(snd_hctl_open);
DECL_FUNCTION(snd_hctl_close);
DECL_FUNCTION(snd_pcm_hw_params_can_pause);
DECL_FUNCTION(snd_asoundlib_version);
DECL_FUNCTION(snd_config_update);
DECL_FUNCTION(snd_config_update_free_global);
DECL_FUNCTION(snd_device_name_hint);
DECL_FUNCTION(snd_device_name_get_hint);
DECL_FUNCTION(snd_device_name_free_hint);
DECL_FUNCTION(snd_pcm_hw_params_malloc);
DECL_FUNCTION(snd_pcm_hw_params_free);
DECL_FUNCTION(snd_pcm_hw_params);
DECL_FUNCTION(snd_pcm_hw_params_any);
DECL_FUNCTION(snd_pcm_hw_params_set_access);
DECL_FUNCTION(snd_pcm_hw_params_set_format);
DECL_FUNCTION(snd_pcm_hw_params_set_rate_resample);
DECL_FUNCTION(snd_pcm_hw_params_set_rate_near);
DECL_FUNCTION(snd_pcm_hw_params_get_rate_min);
DECL_FUNCTION(snd_pcm_hw_params_get_rate_max);
DECL_FUNCTION(snd_pcm_hw_params_test_channels);
DECL_FUNCTION(snd_pcm_hw_params_set_channels);
DECL_FUNCTION(snd_pcm_hw_params_get_channels_min);
DECL_FUNCTION(snd_pcm_hw_params_get_channels_max);
DECL_FUNCTION(snd_pcm_hw_params_set_buffer_size_near);
DECL_FUNCTION(snd_pcm_hw_params_get_buffer_size_max);
DECL_FUNCTION(snd_pcm_hw_params_set_periods_near);
DECL_FUNCTION(snd_pcm_hw_params_set_period_size_near);
DECL_FUNCTION(snd_pcm_hw_params_get_periods_min);
DECL_FUNCTION(snd_pcm_hw_params_get_periods_max);
DECL_FUNCTION(snd_pcm_sw_params_malloc);
DECL_FUNCTION(snd_pcm_sw_params_free);
DECL_FUNCTION(snd_pcm_sw_params_current);
DECL_FUNCTION(snd_pcm_sw_params);
DECL_FUNCTION(snd_pcm_sw_params_set_avail_min);
DECL_FUNCTION(snd_pcm_sw_params_set_start_threshold);
DECL_FUNCTION(snd_pcm_sw_params_set_stop_threshold);
DECL_FUNCTION(snd_pcm_sw_params_set_silence_threshold);
DECL_FUNCTION(snd_pcm_sw_params_set_silence_size);
DECL_FUNCTION(snd_pcm_mmap_begin);
DECL_FUNCTION(snd_pcm_mmap_commit);
DECL_FUNCTION(snd_pcm_writen);
DECL_FUNCTION(snd_pcm_writei);
DECL_FUNCTION(snd_pcm_readi);
DECL_FUNCTION(snd_pcm_readn);
DECL_FUNCTION(snd_pcm_hw_params_can_mmap_sample_resolution);
DECL_FUNCTION(snd_pcm_hw_params_get_rate_numden);
DECL_FUNCTION(snd_pcm_avail_update);
DECL_FUNCTION(snd_pcm_avail);
DECL_FUNCTION(snd_pcm_state);
DECL_FUNCTION(snd_pcm_start);
DECL_FUNCTION(snd_pcm_delay);
DECL_FUNCTION(snd_strerror);
DECL_FUNCTION(snd_lib_error_set_handler);
DECL_FUNCTION(snd_pcm_recover);
DECL_FUNCTION(snd_pcm_stream);
DECL_FUNCTION(snd_mixer_open);
DECL_FUNCTION(snd_mixer_close);
DECL_FUNCTION(snd_mixer_attach_hctl);
DECL_FUNCTION(snd_mixer_detach_hctl);
DECL_FUNCTION(snd_mixer_load);
DECL_FUNCTION(snd_mixer_selem_id_malloc);
DECL_FUNCTION(snd_mixer_selem_id_free);
DECL_FUNCTION(snd_mixer_selem_id_set_index);
DECL_FUNCTION(snd_mixer_selem_id_set_name);
DECL_FUNCTION(snd_mixer_find_selem);
DECL_FUNCTION(snd_mixer_selem_register);
DECL_FUNCTION(snd_mixer_first_elem);
DECL_FUNCTION(snd_mixer_elem_next);
DECL_FUNCTION(snd_mixer_selem_has_playback_volume);
DECL_FUNCTION(snd_mixer_selem_get_playback_volume);
DECL_FUNCTION(snd_mixer_selem_get_playback_volume_range);
DECL_FUNCTION(snd_mixer_selem_get_playback_dB_range);
DECL_FUNCTION(snd_mixer_selem_ask_playback_dB_vol);
DECL_FUNCTION(snd_mixer_selem_set_playback_volume_all);
DECL_FUNCTION(snd_mixer_selem_has_capture_volume);
DECL_FUNCTION(snd_mixer_selem_get_capture_volume);
DECL_FUNCTION(snd_mixer_selem_get_capture_volume_range);
DECL_FUNCTION(snd_mixer_selem_get_capture_dB_range);
DECL_FUNCTION(snd_mixer_selem_ask_capture_dB_vol);
DECL_FUNCTION(snd_mixer_selem_set_capture_volume_all);
DECL_FUNCTION(snd_mixer_selem_get_id);
DECL_FUNCTION(snd_mixer_selem_id_get_name);
DECL_FUNCTION(snd_pcm_hw_params_set_period_wakeup);

DECL_FUNCTION(snd_pcm_info_malloc);
DECL_FUNCTION(snd_pcm_info_free);
DECL_FUNCTION(snd_ctl_open);
DECL_FUNCTION(snd_ctl_close);
DECL_FUNCTION(snd_ctl_pcm_info);
DECL_FUNCTION(snd_pcm_info_get_subdevices_count);
#ifndef NDEBUG
DECL_FUNCTION(snd_pcm_dump);
DECL_FUNCTION(snd_output_stdio_attach);
#endif


typedef struct {
   char bits;
   snd_pcm_format_t format;
} _alsa_formats_t;

#ifndef NDEBUG
#define xrun_recovery(a,b)     _xrun_recovery_debug(id,a,b, __LINE__)
static int _xrun_recovery_debug(const void*,snd_pcm_t *, int, int);
#else
#define xrun_recovery(a,b)	_xrun_recovery(id,a,b)
static int _xrun_recovery(const void*,snd_pcm_t *, int);
#endif

static unsigned int get_devices_avail(int);
static int detect_devnum(_driver_t *, int);
static char *detect_devname(_driver_t *, int);
static char *_aaxALSADriverLogVar(const void *, const char *, ...);
static char *_aaxALSADriverGetDefaultInterface(const void *, int);
static char *_aaxALSADriverGetInterfaceName(const void *);

static int _alsa_pcm_open(_driver_t*, int);
static int _alsa_pcm_close(_driver_t*);
static int _alsa_set_access(const void *, snd_pcm_hw_params_t *);
static void _alsa_error_handler(const char *, int, const char *, int, const char *,...);
static void _alsa_error_handler_none(const char *, int, const char *, int, const char *,...);
static int _alsa_get_volume_range(_driver_t*);
static float _alsa_set_volume(_driver_t*, _aaxRingBuffer*, ssize_t, snd_pcm_sframes_t, unsigned int, float);
static _aaxDriverPlaybackCallback _aaxALSADriverPlayback_mmap_ni;
static _aaxDriverPlaybackCallback _aaxALSADriverPlayback_mmap_il;
static _aaxDriverPlaybackCallback _aaxALSADriverPlayback_rw_ni;
static _aaxDriverPlaybackCallback _aaxALSADriverPlayback_rw_il;


#define MAX_FORMATS		6
#define FILL_FACTOR		1.65f
#define DEFAULT_REFRESH		25.0f
#define _AAX_DRVLOG(a)		_aaxALSADriverLog(id, __LINE__, 0, a)
#define STRCMP(a, b)		(((a)&&(b)) ? strncmp((a), (b), strlen(b)) : -1)

/* forward declarations */
static const char* _alsa_type[2];
static const snd_pcm_stream_t _alsa_mode[2];
static const char *_const_alsa_default_name[2];
static const _alsa_formats_t _alsa_formats[MAX_FORMATS];

#define MAX_PREFIX	7
static const char* ifname_prefix[MAX_PREFIX];
static void *audio = NULL;

int
_aaxALSADriverDetect(int mode)
{
   static int rv = false;
   char *error = 0;

   _AAX_LOG(LOG_DEBUG, __func__);

#if HAVE_PULSEAUDIO_H
# if RELEASE
   const char *env = getenv("AAX_SHOW_ALSA_DEVICES");
   if (!env || !_aax_getbool(env)) {
      return false;
   }
# endif
#endif

   if (TEST_FOR_FALSE(rv) && !audio) {
      audio = _aaxIsLibraryPresent("asound", "2");
      _aaxGetSymError(0);
   }

   if (audio)
   {
      TIE_FUNCTION(snd_pcm_open);					//
      if (psnd_pcm_open)	
      {
         TIE_FUNCTION(snd_pcm_close);					//
         TIE_FUNCTION(snd_pcm_wait);					//
         TIE_FUNCTION(snd_pcm_hw_params);				//
         TIE_FUNCTION(snd_pcm_hw_params_any);				//
         TIE_FUNCTION(snd_pcm_sw_params);				//
         TIE_FUNCTION(snd_pcm_mmap_begin);				//
         TIE_FUNCTION(snd_pcm_mmap_commit);				//
         TIE_FUNCTION(snd_pcm_writen);					//
         TIE_FUNCTION(snd_pcm_writei);					//
         TIE_FUNCTION(snd_pcm_readi);					//
         TIE_FUNCTION(snd_pcm_readn);					//
         TIE_FUNCTION(snd_pcm_avail_update);				//
         TIE_FUNCTION(snd_pcm_avail);					//
         TIE_FUNCTION(snd_pcm_recover);					//
         TIE_FUNCTION(snd_hctl_open);					//
         TIE_FUNCTION(snd_hctl_close);					//
         TIE_FUNCTION(snd_mixer_open);					//
         TIE_FUNCTION(snd_mixer_close);					//
         TIE_FUNCTION(snd_mixer_attach_hctl);				//
         TIE_FUNCTION(snd_mixer_detach_hctl);				//
         TIE_FUNCTION(snd_mixer_load);					//
         TIE_FUNCTION(snd_mixer_selem_id_malloc);			//
         TIE_FUNCTION(snd_mixer_selem_id_free);				//
         TIE_FUNCTION(snd_mixer_selem_id_set_index);			//
         TIE_FUNCTION(snd_mixer_selem_id_set_name);			//
         TIE_FUNCTION(snd_mixer_find_selem);				//
         TIE_FUNCTION(snd_mixer_selem_register);			//
         TIE_FUNCTION(snd_mixer_first_elem);				//
         TIE_FUNCTION(snd_mixer_elem_next);				//
         TIE_FUNCTION(snd_mixer_selem_has_playback_volume);		//
         TIE_FUNCTION(snd_mixer_selem_get_playback_volume);		//
         TIE_FUNCTION(snd_mixer_selem_get_playback_volume_range);	//
         TIE_FUNCTION(snd_mixer_selem_get_playback_dB_range);		//
         TIE_FUNCTION(snd_mixer_selem_ask_playback_dB_vol);		//
         TIE_FUNCTION(snd_mixer_selem_set_playback_volume_all);		//
         TIE_FUNCTION(snd_mixer_selem_has_capture_volume);		//
         TIE_FUNCTION(snd_mixer_selem_get_capture_volume);		//
         TIE_FUNCTION(snd_mixer_selem_get_capture_volume_range);	//
         TIE_FUNCTION(snd_mixer_selem_get_capture_dB_range);		//
         TIE_FUNCTION(snd_mixer_selem_ask_capture_dB_vol);		//
         TIE_FUNCTION(snd_mixer_selem_set_capture_volume_all);		//
         TIE_FUNCTION(snd_mixer_selem_get_id);				//
         TIE_FUNCTION(snd_mixer_selem_id_get_name);			//
         TIE_FUNCTION(snd_pcm_nonblock);				//
         TIE_FUNCTION(snd_pcm_prepare);					//
         TIE_FUNCTION(snd_pcm_pause);					//
         TIE_FUNCTION(snd_pcm_hw_params_can_pause);			//
         TIE_FUNCTION(snd_device_name_hint);				//
         TIE_FUNCTION(snd_device_name_get_hint);			//
         TIE_FUNCTION(snd_device_name_free_hint);			//
         TIE_FUNCTION(snd_pcm_hw_params_malloc);			//
         TIE_FUNCTION(snd_pcm_hw_params_free);				//
         TIE_FUNCTION(snd_pcm_hw_params_set_access);			//
         TIE_FUNCTION(snd_pcm_hw_params_set_format);			//
         TIE_FUNCTION(snd_pcm_hw_params_set_rate_resample);		//
         TIE_FUNCTION(snd_pcm_hw_params_set_rate_near);			//
         TIE_FUNCTION(snd_pcm_hw_params_get_rate_min);			//
         TIE_FUNCTION(snd_pcm_hw_params_get_rate_max);			//
         TIE_FUNCTION(snd_pcm_hw_params_test_channels);			//
         TIE_FUNCTION(snd_pcm_hw_params_set_channels);			//
         TIE_FUNCTION(snd_pcm_hw_params_get_channels_min);		//
         TIE_FUNCTION(snd_pcm_hw_params_get_channels_max);		//
         TIE_FUNCTION(snd_pcm_hw_params_set_periods_near);		//
         TIE_FUNCTION(snd_pcm_hw_params_set_period_size_near);		//
         TIE_FUNCTION(snd_pcm_hw_params_get_periods_min);		//
         TIE_FUNCTION(snd_pcm_hw_params_get_periods_max);		//
         TIE_FUNCTION(snd_pcm_hw_params_set_buffer_size_near);		//
         TIE_FUNCTION(snd_pcm_hw_params_get_buffer_size_max);		//
         TIE_FUNCTION(snd_pcm_sw_params_malloc);			//
         TIE_FUNCTION(snd_pcm_sw_params_free);				//
         TIE_FUNCTION(snd_pcm_sw_params_current);			//
         TIE_FUNCTION(snd_pcm_sw_params_set_avail_min);			//
         TIE_FUNCTION(snd_pcm_sw_params_set_start_threshold);		//
         TIE_FUNCTION(snd_pcm_sw_params_set_stop_threshold);		//
         TIE_FUNCTION(snd_pcm_sw_params_set_silence_threshold);		//
         TIE_FUNCTION(snd_pcm_sw_params_set_silence_size);		//
         TIE_FUNCTION(snd_pcm_hw_params_can_mmap_sample_resolution);	//
         TIE_FUNCTION(snd_pcm_hw_params_get_rate_numden);		//
         TIE_FUNCTION(snd_pcm_state);					//
         TIE_FUNCTION(snd_pcm_start);					//
         TIE_FUNCTION(snd_pcm_delay);					//
         TIE_FUNCTION(snd_pcm_stream);					//
         TIE_FUNCTION(snd_strerror);					//
         TIE_FUNCTION(snd_config_update);				//
         TIE_FUNCTION(snd_config_update_free_global);			//
         TIE_FUNCTION(snd_lib_error_set_handler);			//
         TIE_FUNCTION(snd_asoundlib_version);				//

         TIE_FUNCTION(snd_pcm_info_malloc);				//
         TIE_FUNCTION(snd_pcm_info_free);				//
         TIE_FUNCTION(snd_ctl_open);					//
         TIE_FUNCTION(snd_ctl_close);					//
         TIE_FUNCTION(snd_ctl_pcm_info);				//
         TIE_FUNCTION(snd_pcm_info_get_subdevices_count);		//
//       TIE_FUNCTION(snd_output_stdio_attach);
      }

      error = _aaxGetSymError(0);
      if (!error)
      {
         TIE_FUNCTION(snd_pcm_hw_params_set_period_wakeup);
         if (get_devices_avail(mode) != 0)
         {
            snprintf(_alsa_id_str, MAX_ID_STRLEN, "%s %s",
                             DEFAULT_RENDERER, psnd_asoundlib_version());

            psnd_lib_error_set_handler(_alsa_error_handler_none);
            psnd_config_update();
            rv = true;
         }
      }
   }
   else {
      rv = false;
   }

   return rv;
}

static void *
_aaxALSADriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (handle)
   {
      int m = (mode > 0) ? 1 : 0;
      handle->default_name[m] = (char*)_const_alsa_default_name[m];
      handle->play = _aaxALSADriverPlayback_rw_il;
      handle->pause = 0;
      handle->use_mmap = 1;
      handle->interleaved = 0;

      handle->frequency_hz = 48000.0f;
      handle->no_tracks = 2;
      handle->bits_sample = 16;
      handle->no_periods = DEFAULT_PERIODS;
      handle->period_frames = handle->frequency_hz/DEFAULT_REFRESH;
      handle->period_frames_actual = handle->period_frames;

      handle->mode = mode;
      if (handle->mode != AAX_MODE_READ) { // Always interupt based for capture
         handle->use_timer = TIMER_BASED;
      }

      handle->volumeInit = 1.0f;
      handle->volumeCur  = 1.0f;
      handle->volumeMin = 0.0f;
      handle->volumeMax = 1.0f;

      handle->target[0] = FILL_FACTOR;
      handle->target[1] = FILL_FACTOR;
      handle->target[2] = AAX_FPINFINITE;
   }

   return handle;
}

static int
_aaxALSADriverFreeHandle(UNUSED(void *id))
{
   _aaxCloseLibrary(audio);
   audio = NULL;

   return true;
}

static void *
_aaxALSADriverConnect(void *config, const void *id, xmlId *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;
   int rdr_aax_fmt;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      id = handle = _aaxALSADriverNewHandle(mode);
   }

   rdr_aax_fmt = (renderer && strstr(renderer, ": ")) ? 1 : 0;
   if (handle)
   {
      if (rdr_aax_fmt) {
         handle->name = _aax_strdup((char*)renderer);
      }
      else
      {
         handle->devname = _aax_strdup(renderer);
         renderer = handle->name = _aaxALSADriverGetDefaultInterface(handle, mode);
      }

      if (xid)
      {
         float f;
         char *s;
         int i;

         if (!handle->name)
         {
            s = xmlAttributeGetString(xid, "name");
            if (s)
            {
               if (strcasecmp(s, "default")) {
                  handle->name = _aax_strdup(s);
               } else {						/* 'default' */
                  handle->name = _aaxALSADriverGetDefaultInterface(handle,mode);
               }
               xmlFree(s);
            }
         }

         s = getenv("AAX_USE_TIMER");
         if (s) {
            handle->use_timer = _aax_getbool(s);
         } else if (xmlNodeTest(xid, "timed")) {
            handle->use_timer = xmlNodeGetBool(xid, "timed");
         }
         if (handle->mode == AAX_MODE_READ) {
            handle->use_timer = false;
         }

         if (xmlNodeTest(xid, "shared")) {
            handle->shared = xmlNodeGetBool(xid, "shared");
            handle->shared_volume = handle->shared;
         }

         f = (float)xmlNodeGetDouble(xid, "frequency-hz");
         if (f)
         {
            if (f < (float)_AAX_MIN_MIXER_FREQUENCY)
            {
               _AAX_DRVLOG("frequency too small.");
               f = (float)_AAX_MIN_MIXER_FREQUENCY;
            }
            else if (f > (float)_AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_DRVLOG("frequency too large.");
               f = (float)_AAX_MAX_MIXER_FREQUENCY;
            }
            handle->frequency_hz = f;
         }

         if (mode != AAX_MODE_READ && mode != AAX_MODE_WRITE_HRTF)
         {
            i = xmlNodeGetInt(xid, "channels");
            if (i)
            {
               if (i < 1)
               {
                  _AAX_DRVLOG("no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_DRVLOG("no. tracks too great.");
                  i = _AAX_MAX_SPEAKERS;
               }
               handle->no_tracks = i;
            }
         }

         i = xmlNodeGetInt(xid, "bits-per-sample");
         if (i)
         {
            if (i < 8 || i > 32)
            {
               _AAX_DRVLOG("unsupported bits-per-sample");
               i = 16;
            }
            handle->bits_sample = i;
         }

         i = xmlNodeGetInt(xid, "periods");
         if (i)
         {
            if (i < 1)
            {
               _AAX_DRVLOG("no periods too small.");
               i = 1;
            }
            else if (i > 16)
            {
               _AAX_DRVLOG("no. periods too great.");
               i = 16;
            }
            handle->no_periods = i;
         }
      }
   }
#if 0
 printf("\nrenderer: %s\n", handle->name);
 printf("frequency-hz: %f\n", handle->frequency_hz);
 printf("channels: %i\n", handle->no_tracks);
 printf("bits-per-sample: %i\n", handle->bits_sample);
 printf("periods: %i\n", handle->no_periods);
 printf("\n");
#endif

   if (handle)
   {
      char m = (handle->mode == AAX_MODE_READ) ? 0 : 1;
      int err;

      handle->handle = config;
      handle->devnum = detect_devnum(handle, m);
      if (rdr_aax_fmt) {
         handle->devname = detect_devname(handle, m);
      }

      err = _alsa_pcm_open(handle, m);
//    psnd_config_update_free_global();
      if (err < 0)
      {
         _AAX_DRVLOG(psnd_strerror(err));
         if (id == 0) free(handle);
         handle = 0;
      }
   }

   return handle;
}

static int
_aaxALSADriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = false;

   _AAX_LOG(LOG_DEBUG, __func__);

   if (handle)
   {
      unsigned char m = (handle->mode == AAX_MODE_READ) ? 0 : 1;

      if (handle->default_name[m] != _const_alsa_default_name[m]) {
         free(handle->default_name[m]);
      }

      if (handle->ifname[0]) free(handle->ifname[0]);
      if (handle->ifname[1]) free(handle->ifname[1]);
      if (handle->devname)
      {
         if (handle->devname != handle->default_name[m]) {
            free(handle->devname);
         }
         handle->devname = 0;
      }
      if (handle->name)
      {
         if (handle->name != handle->default_name[m]) {
            free(handle->name);
         }
         handle->name = 0;
      }

      if (handle->render)
      {
         handle->render->close(handle->render->id);
         free(handle->render);
      }

      _alsa_pcm_close(handle);
      if (handle->outMixer)
      {
         _aax_free(handle->outMixer);
         handle->outMixer = NULL;
      }

      if (handle->ptr) _aax_free(handle->ptr);
      handle->ptr = 0;
      handle->pcm = 0;
      free(handle);
      handle = 0;

      rv = true;
   }

   return rv;
}


#ifndef NDEBUG
# define TRUN(f, s)	if (err >= 0) { err = f; if (err < 0) { _AAX_DRVLOG(s); printf("ALSA error: %s (%i) at line %i\n", s, err, __LINE__); } }
#else
# define TRUN(f, s)	if (err >= 0) { err = f; if (err < 0) _AAX_DRVLOG(s); }
#endif

static int
_aaxALSADriverSetup(const void *id, float *refresh_rate, int *fmt,
                    unsigned int *channels, float *speed, UNUSED(int *bitrate),
                    int registered, float period_rate)
{
   _driver_t *handle = (_driver_t *)id;
   snd_pcm_uframes_t period_frames_actual;
   snd_pcm_uframes_t period_frames;
   snd_pcm_hw_params_t *hwparams;
   snd_pcm_sw_params_t *swparams;
   unsigned int tracks, rate;
   unsigned int bits, periods;
   int err, rv = 0;


   _AAX_LOG(LOG_DEBUG, __func__);

   assert(handle != 0);

   rate = (unsigned int)*speed;
   tracks = *channels;
   handle->refresh_rate = *refresh_rate;
   if (tracks > handle->no_tracks) {
      tracks = handle->no_tracks;
   }

   if (handle->no_tracks != tracks)
   {
      char m = (handle->mode == AAX_MODE_READ) ? 0 : 1;

      _alsa_pcm_close(handle);

      handle->no_tracks = tracks;
      handle->devnum = detect_devnum(handle, m);
      handle->devname = detect_devname(handle, m);

      err = _alsa_pcm_open(handle, m);
//    psnd_config_update_free_global();
      if (err < 0)
      {
         _AAX_DRVLOG("unsupported number of tracks");
         return false;
      }
   }
   /* TODO: for now */
   if (tracks > 2) {
      handle->use_timer = false;
   }

   periods = handle->no_periods;
   if (!registered) {
      period_frames = get_pow2((size_t)rintf(rate/(*refresh_rate*periods)));
   } else {
      period_frames = get_pow2((size_t)rintf((rate*periods)/period_rate));
   }
   period_frames_actual = period_frames;
   bits = aaxGetBitsPerSample(*fmt);

   handle->latency = 1.0f / *refresh_rate;
   if (handle->latency < 0.010f) {
      handle->use_timer = false;
   }

   psnd_pcm_hw_params_malloc(&hwparams);
   psnd_pcm_sw_params_malloc(&swparams);
   if (hwparams && swparams && handle->pcm)
   {
      snd_pcm_t *hid = handle->pcm;
      snd_pcm_format_t data_format;
      unsigned int val1, val2;
      unsigned int bits_pos;

      err = psnd_pcm_hw_params_any(hid, hwparams);
      TRUN( psnd_pcm_hw_params_set_rate_resample(hid, hwparams, 0),
            "unable to disable sample rate conversion" );

      if (handle->use_timer && psnd_pcm_hw_params_set_period_wakeup) {
         TRUN( psnd_pcm_hw_params_set_period_wakeup(hid, hwparams, 0),
            "unable to disable period wakeups" );
      }

      /* Set the prefered access method (rw/mmap interleaved/non-interleaved) */
      err = _alsa_set_access(handle, hwparams);

      /* supported sample formats*/
      handle->can_pause = psnd_pcm_hw_params_can_pause(hwparams);

      TRUN( psnd_pcm_hw_params_get_rate_min(hwparams, &val1, 0),
            "unable to get the minimum sample rate" );
      TRUN( psnd_pcm_hw_params_get_rate_max(hwparams, &val2, 0),
            "unable to get the maximum sample rate" );
      if (err >= 0)
      {
         rate = _MINMAX(rate, val1, val2);
         handle->min_frequency = val1;
         handle->max_frequency = val2;
      }

      bits_pos = 0;
      if (err >= 0)
      {
         do
         {
            data_format = _alsa_formats[bits_pos].format;
            err = psnd_pcm_hw_params_set_format(hid, hwparams, data_format);
         }
         while ((err < 0) && (_alsa_formats[++bits_pos].bits != 0));
         bits = _alsa_formats[bits_pos].bits;
      }

      TRUN( psnd_pcm_hw_params_get_channels_min(hwparams, &val1),
            "unable to get the minimum no. tracks" );
      TRUN( psnd_pcm_hw_params_get_channels_max(hwparams, &val2),
            "unable to get the maximum no. tracks" );
      if (err >= 0)
      {
         val1 = _MIN(val1, handle->min_tracks);
         val2 = _MAX(_MIN(val2, _AAX_MAX_SPEAKERS), handle->max_tracks);
         tracks = _MINMAX(tracks, val1, val2);
         handle->min_tracks = val1;
         handle->max_tracks = val2;
      }

      TRUN ( psnd_pcm_hw_params_get_periods_min(hwparams, &val1, 0),
             "unable to get the minimum no. periods" );
      TRUN ( psnd_pcm_hw_params_get_periods_max(hwparams, &val2, 0),
             "unable to get the maximum no. periods" );
      if (err >= 0)
      {
         if (handle->use_timer) {
            periods = val1;
         } else {
            periods = _MINMAX(periods, val1, val2);
         }
         handle->min_periods = val1;
         handle->max_periods = val2;
      }

      /* recalculate period_frames and latency */
      if (!registered) {
         period_frames = get_pow2((size_t)rintf(rate/(*refresh_rate*periods)));
      } else {
         period_frames = get_pow2((size_t)rintf((rate*periods)/period_rate));
      }
      period_frames *= periods;
      period_frames_actual = period_frames;

      /* test for supported sample formats */
      switch (bits_pos)
      {
      case 0:				/* SND_PCM_FORMAT_S24_LE */
         handle->cvt_to = (_batch_cvt_to_proc)_batch_cvt24_24;
         handle->cvt_from = (_batch_cvt_from_proc)_batch_cvt24_24;
         handle->cvt_to_intl = _batch_cvt24_intl_24;
         handle->cvt_from_intl = _batch_cvt24_24_intl;
         *fmt = AAX_PCM24S;
         break;
      case 1:				/* SND_PCM_FORMAT_S32_LE */
         handle->cvt_to = _batch_cvt32_24;
         handle->cvt_from = _batch_cvt24_32;
         handle->cvt_to_intl = _batch_cvt32_intl_24;
         handle->cvt_from_intl = _batch_cvt24_32_intl;
         *fmt = AAX_PCM32S;
         break;
      case 2:				/* SND_PCM_FORMAT_S16_LE */
         handle->cvt_to = _batch_cvt16_24;
         handle->cvt_from = _batch_cvt24_16;
         handle->cvt_to_intl = _batch_cvt16_intl_24;
         handle->cvt_from_intl = _batch_cvt24_16_intl;
         *fmt = AAX_PCM16S;
         break;
      case 3:				/* SND_PCM_FORMAT_S24_3LE */
         handle->cvt_to = _batch_cvt24_3_24;
         handle->cvt_from = _batch_cvt24_24_3;
         handle->cvt_to_intl = _batch_cvt24_3intl_24;
         handle->cvt_from_intl = _batch_cvt24_24_3intl;
         *fmt = AAX_PCM24S;
         break;
      case 4:				/* SND_PCM_FORMAT_U8 */
         handle->cvt_to = _batch_cvt8_24;
         handle->cvt_from = _batch_cvt24_8;
         handle->cvt_to_intl = _batch_cvt8_intl_24;
         handle->cvt_from_intl = _batch_cvt24_8_intl;
         *fmt = AAX_PCM8S;
         break;
      default:
         _AAX_DRVLOG("unsupported hardware format\n");
         err = -EINVAL;
         break;
      }

      TRUN( psnd_pcm_hw_params_set_channels(hid, hwparams, tracks),
            "unsupported no. tracks" );

      TRUN( psnd_pcm_hw_params_set_rate_near(hid, hwparams, &rate, 0),
            "unsupported sample rate" );

      /** set buffer and period sizes */
      if (handle->use_timer)
      {
         long unsigned int max_size = 0;
         TRUN( psnd_pcm_hw_params_get_buffer_size_max(hwparams, &max_size),
               "unable to fetch the maximum buffer size" );
         period_frames_actual = period_frames = _MIN(65536, max_size);
      }
      else
      {
         /* Set buffer size (in frames). The resulting latency is given by */
         /* latency = periodsize * periods / (rate * bytes_per_frame))     */
         period_frames *= periods;
         if (handle->mode == AAX_MODE_READ) {
            period_frames_actual = period_frames*2;
         } else {
            period_frames_actual = period_frames;
         }
      }
      TRUN( psnd_pcm_hw_params_set_buffer_size_near(hid, hwparams,
                                                    &period_frames_actual),
            "invalid buffer size" );
      handle->max_frames = period_frames_actual;

      /* Ugly hack: At least the SUNXI driver needs it this way. */
      do {
         err = psnd_pcm_hw_params_set_periods_near(hid, hwparams, &periods, 0);
      } while ((err < 0) && (++periods < handle->max_periods));


      TRUN( psnd_pcm_hw_params(hid, hwparams), "unable to configure hardware" );
      if (err >= 0)
      {
         if (handle->use_timer)
         {
            handle->no_periods = periods = 2;
            if (!registered) {
               period_frames = (size_t)rintf(rate/(*refresh_rate*periods));
            } else {
               period_frames = (size_t)rintf((rate*periods)/period_rate);
            }
         }
         else
         {
            if (handle->mode == AAX_MODE_READ) {
               period_frames = period_frames_actual/2;
            } else {
               period_frames = period_frames_actual;
            }
            period_frames /= periods;
            handle->latency = (float)(period_frames*periods)/(float)rate;
         }

         TRUN( psnd_pcm_sw_params_current(hid, swparams),
               "unable to get software config" );

         TRUN( psnd_pcm_sw_params_set_avail_min(hid, swparams, period_frames),
               "wakeup treshold unsupported" );

         TRUN( psnd_pcm_sw_params_set_silence_size(hid, swparams, 0),
               "failed to set the silence size" );
#if 0
         TRUN( psnd_pcm_sw_params_set_silence_threshold(hid, swparams, periods*period_frames_actual),
               "failed to set the silence threshold" );
#endif

         if (handle->mode == AAX_MODE_READ)
         {
            TRUN( psnd_pcm_sw_params_set_start_threshold(hid, swparams, 1),
                  "improper interrupt treshold" );
            TRUN( psnd_pcm_sw_params_set_stop_threshold(hid, swparams,
                                                  periods*period_frames_actual),
                  "set_stop_threshold unsupported" );
         }
         else
         {
            TRUN( psnd_pcm_sw_params_set_start_threshold(hid, swparams,
                                                     period_frames*(periods-1)),
                  "improper interrupt treshold" );
            TRUN( psnd_pcm_sw_params_set_stop_threshold(hid, swparams,
                                                        handle->max_frames),
                  "set_stop_threshold unsupported" );
         }
         handle->threshold = 5*period_frames/4;

         TRUN( psnd_pcm_sw_params(hid, swparams),
               "unable to set software parameters" );
         if (err >= 0)
         {
            val1 = val2 = 0;
            err = psnd_pcm_hw_params_get_rate_numden(hwparams, &val1, &val2);
            if (val1 && val2)
            {
               handle->frequency_hz = (float)val1/(float)val2;
               rate = (unsigned int)handle->frequency_hz;
            }

            handle->frequency_hz = (float)rate;
            handle->no_tracks = tracks;
            handle->bits_sample = bits;
            handle->period_frames = period_frames;
            handle->period_frames_actual = period_frames_actual;
            handle->no_periods = periods;

            *speed = rate;
            *channels = tracks;
            if (!registered) {
               *refresh_rate = rate/(float)period_frames;
            } else {
               *refresh_rate = period_rate;
            }
            handle->refresh_rate = *refresh_rate;

            if (!handle->use_timer && strcmp(handle->devname, "default"))
            {
               handle->latency = (float)(period_frames*periods)/(float)rate;
               if (handle->mode != AAX_MODE_READ) // && !handle->use_timer)
               {
                  char m = (handle->mode == AAX_MODE_READ) ? 0 : 1;
                  snd_pcm_sframes_t delay;
                  _aaxRingBuffer *rb;
                  unsigned int i;

                  rb = _aaxRingBufferCreate(0.0f, m);
                  if (rb)
                  {
                     rb->set_format(rb, AAX_PCM24S, true);
                     rb->set_parami(rb, RB_NO_TRACKS, handle->no_tracks);
                     rb->set_paramf(rb, RB_FREQUENCY, handle->frequency_hz);
                     rb->set_parami(rb, RB_NO_SAMPLES, handle->period_frames);
                     rb->init(rb, true);
                     rb->set_state(rb, RB_STARTED);

                     for (i=0; i<handle->no_periods; i++) {
                        handle->play(handle, rb, 1.0f, 0.0f, 0);
                     }
                     _aaxRingBufferFree(rb);
                  }

                  err = psnd_pcm_delay(hid, &delay);
                  if (err >= 0) {
                     handle->latency = (float)delay/(float)rate;
                  }
                  err = 0;
               }
            }
            else
            {
               handle->target[0] = ((float)period_frames/(float)rate);
               if (handle->target[0] > 0.02f) {
                  handle->target[0] += 0.01f; // add 10ms
               } else {
                  handle->target[0] *= FILL_FACTOR;
               }
               handle->target[1] = handle->target[0];
               handle->latency = handle->target[0];
            }

            handle->render = _aaxSoftwareInitRenderer(handle->latency,
                                                      handle->mode, registered);
            if (handle->render)
            {
               const char *rstr = handle->render->info(handle->render->id);
               snprintf(_alsa_id_str, MAX_ID_STRLEN, "%s %s %s",
                             DEFAULT_RENDERER, psnd_asoundlib_version(), rstr);
               rv = true;
            }
            else {
               _AAX_DRVLOG("unable to get the renderer");
            }
         }
         else {
            _AAX_DRVLOG("invalid software configuration");
         }
      }
      else {
         _AAX_DRVLOG("incompatible hardware configuration");
      }

      do
      {
         char str[255];

         _AAX_SYSLOG("alsa; driver settings:");

         if (handle->mode != AAX_MODE_READ) {
            snprintf(str,255,"  output renderer: '%s'", handle->name);
         } else {
            snprintf(str,255,"  input renderer: '%s'", handle->name);
         }
         _AAX_SYSLOG(str);
         snprintf(str,255, "  devname: '%s'", handle->devname);
         _AAX_SYSLOG(str);
         snprintf(str,255, "  playback rate: %u hz",  rate);
         _AAX_SYSLOG(str);
         snprintf(str,255, "  buffer size: %u bytes", (unsigned int)(handle->period_frames*tracks*bits)/8);
         _AAX_SYSLOG(str);
         snprintf(str,255, "  latency: %3.2f ms",  1e3*handle->latency);
         _AAX_SYSLOG(str);
         snprintf(str,255, "  no. periods: %i", handle->no_periods);
         _AAX_SYSLOG(str);
         snprintf(str,255,"  use mmap: %s", handle->use_mmap?"yes":"no");
         _AAX_SYSLOG(str);
         snprintf(str,255,"  interleaved: %s",handle->interleaved?"yes":"no");
         _AAX_SYSLOG(str);
         snprintf(str,255,"  timer based: %s",handle->use_timer?"yes":"no");
         _AAX_SYSLOG(str);
         snprintf(str,255,"  channels: %i, bits/sample: %i\n", tracks, handle->bits_sample);
         _AAX_SYSLOG(str);
#if 0
 printf("alsa; driver settings:\n");
 if (handle->mode != AAX_MODE_READ) {
    printf("  output renderer: '%s'\n", handle->name);
 } else {
    printf("  input renderer: '%s'\n", handle->name);
 }
 printf( "  devname: '%s'\n", handle->devname);
 printf( "  playback rate: %u hz\n",  rate);
 printf( "  buffer size: %u bytes\n", (unsigned int)(handle->period_frames*tracks*bits)/8);
 printf( "  latency: %3.2f ms\n",  1e3*handle->latency);
 printf( "  no. periods: %i\n", handle->no_periods);
 printf("  use mmap: %s\n", handle->use_mmap?"yes":"no");
 printf("  interleaved: %s\n",handle->interleaved?"yes":"no");
 printf("  timer based: %s\n",handle->use_timer?"yes":"no");
 printf("  channels: %i, bits/sample: %i\n", tracks, handle->bits_sample);
 printf("\tformat: %x\n", *fmt);
#endif
      } while (0);
   }

   if (swparams) psnd_pcm_sw_params_free(swparams);
   if (hwparams) psnd_pcm_hw_params_free(hwparams);

   psnd_lib_error_set_handler(_alsa_error_handler);

   return rv;
}
#undef TRUN

static ssize_t
_aaxALSADriverCapture(const void *id, void **data, ssize_t *offset, size_t *req_frames, void *scratch, size_t scratchlen, float gain, UNUSED(char batched))
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int tracks, frame_size;
   snd_pcm_sframes_t avail;
   snd_pcm_state_t state;
   ssize_t offs = *offset;
   ssize_t init_offs = offs;
   size_t period_frames;
   ssize_t rv = 0;
   int res;

   if ((handle->mode != AAX_MODE_READ) || (req_frames == 0) || (data == 0))
   {
      if (handle->mode == AAX_MODE_READ) {
         _AAX_DRVLOG("calling the record function with a playback handle");
      } else if (req_frames == 0) {
         _AAX_DRVLOG("record buffer size is zero bytes");
      } else {
         _AAX_DRVLOG("calling the record function with null pointer");
      }
      return rv;
   }

   state = psnd_pcm_state(handle->pcm);
   if (state != SND_PCM_STATE_RUNNING) {
      psnd_pcm_start(handle->pcm);
   }
   else if (state == SND_PCM_STATE_XRUN) {
//    _AAX_DRVLOG("alsa (record): state = SND_PCM_STATE_XRUN.");
      xrun_recovery(handle->pcm, -EPIPE);
   }

   tracks = handle->no_tracks;
   frame_size = (tracks * handle->bits_sample)/8;
   period_frames = *req_frames;
   handle->buf_len = period_frames * frame_size;

   /* synchronise capture and playback for registered sensors */
   avail = 0;
   res = psnd_pcm_delay(handle->pcm, &avail);
   if (res < 0)
   {
      if ((res = xrun_recovery(handle->pcm, res)) < 0)
      {
         _aaxALSADriverLogVar(id, "xrun: %s", psnd_strerror(res));
         avail = -1;
       }
   }

   *req_frames = 0;
   if (period_frames && avail)
   {
      unsigned int scratchsz = scratchlen*8/(handle->no_tracks*handle->bits_sample);
      size_t corr, fetch = period_frames;
      unsigned int chunk, try = 0;
      snd_pcm_uframes_t size;
      int32_t **sbuf;
      float diff;

      sbuf = (int32_t**)data;

      /* try to keep the buffer padding at the threshold level at all times */
      diff = (float)avail-(float)handle->threshold;
      handle->padding = (handle->padding + diff/(float)period_frames)/2;
      corr = _MINMAX(roundf(handle->padding), -1, 1);
      fetch += corr;
      offs -= corr;
      *offset = -corr;
#if 0
if (corr)
 printf("avail: %4i (%4i), fetch: %6i\r", avail, handle->threshold, fetch);
#endif
      /* try to keep the buffer padding at the threshold level at all times */
      chunk = 10;
      size = fetch; // period_frames;
      rv = true;
      do
      {
         /*
          * Note:
          * When recording from a device that is also opened for playback the
          * frame size will be different when output is opened using a different
          * number of channels than the 2 cannel recording!
          */
         /* See http://www.alsa-project.org/alsa-doc/alsa-lib/group___p_c_m___direct.html#g3e3d8bb878f70e94a746d17410e93273 for more information */
         if (handle->use_mmap)
         {
            const snd_pcm_channel_area_t *area;
            snd_pcm_uframes_t frames = size;
            snd_pcm_uframes_t mmap_offs = 0;

            psnd_pcm_avail_update(handle->pcm);
            res = psnd_pcm_mmap_begin(handle->pcm, &area, &mmap_offs, &frames);
            if (res < 0)
            {
               if ((res = xrun_recovery(handle->pcm, res)) < 0)
               {
                  _aaxALSADriverLogVar(id, "mmap begin: %s",psnd_strerror(res));
                  return 0;
               }
            }

            if (!frames) break;

            if (handle->interleaved)
            {
               do {
                  res = psnd_pcm_mmap_commit(handle->pcm, mmap_offs, frames);
               }
               while (res == -EAGAIN);

               if (res > 0)
               {
                  char *p = (char *)area->addr;
                  p += (area->first + area->step*mmap_offs) >> 3;
                  handle->cvt_from_intl(sbuf, p, offs, tracks, res);
               }
            }
            else
            {
               do {
                  res = psnd_pcm_mmap_commit(handle->pcm, mmap_offs, frames);
               }
               while (res == -EAGAIN);

               if (res > 0)
               {
                  unsigned int t;
                  for (t=0; t<tracks; t++)
                  {
                     unsigned char *s;
                     s = (((unsigned char *)area[t].addr) + (area[t].first/8));
                     s += (mmap_offs*handle->bits_sample)/8;
                     handle->cvt_from(sbuf[t]+offs, s, res);
                  }
               }
            }
         }
         else
         {
            if (handle->interleaved)
            {
               do {
                  res = psnd_pcm_readi(handle->pcm, scratch,
                                       _MIN(size, scratchsz));
               }
               while (res == -EAGAIN);

               if (res > 0) {
                  handle->cvt_from_intl(sbuf, scratch, offs, tracks, res);
               }
            }
            else
            {
               void *s[2];

               s[0] = scratch;
               s[1] = (void*)((char*)s[0] + fetch*frame_size);
               do {
                  res = psnd_pcm_readn(handle->pcm, s, _MIN(size, scratchsz));
               } while (res == -EAGAIN);

               if (res > 0)
               {
                  handle->cvt_from(sbuf[0]+offs, s[0], res);
                  if (tracks == 2) {
                     handle->cvt_from(sbuf[1]+offs, s[1], res);
                  }
               }
            }
         }

         if (res < 0)
         {
            if (xrun_recovery(handle->pcm, res) < 0)
            {
               _AAX_DRVLOG("unable to run xrun_recovery");
               rv = false;
               break;
            }
            if (try++ > 2)
            {
               _AAX_DRVLOG("unable to recover from pcm read error");
               rv = false;
               break;
            }
//          _AAX_DRVLOG("warning: pcm read error");
            continue;
         }

         size -= res;
         offs += res;
      }
      while((size > 0) && --chunk);
      if (!chunk) _AAX_DRVLOG("too many capture tries\n");
      *req_frames = offs;

      gain = _alsa_set_volume(handle, NULL, init_offs, offs, tracks, gain);
      if (gain > LEVEL_96DB && fabsf(gain-1.0f) > LEVEL_96DB)
      {
         unsigned int i;
         for (i=0; i<tracks; i++)
         {
            int32_t *ptr = (int32_t*)sbuf[i]+init_offs;
            _batch_imul_value(ptr, ptr, sizeof(int32_t), offs, gain);
         }
      }
   }
   else rv = true;

   return rv;
}

static int
_aaxALSADriverSetName(const void *id, int type, const char *name)
{
   _driver_t *handle = (_driver_t *)id;
   int ret = false;
   if (handle)
   {
      switch (type)
      {
      default:
         break;
      }
   }
   return ret;
}

static char *
_aaxALSADriverGetName(const void *id, int type)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   switch (type)
   {
   case AAX_MODE_READ:
   case AAX_MODE_WRITE_STEREO:
      if (handle)
      {
         if (handle->name) {
            ret = _aax_strdup(handle->name);
         } else if (handle->devname) {
            ret = _aax_strdup(handle->devname);
         }
      }
      else {
         ret = _aaxALSADriverGetDefaultInterface(id, type);
      }
      break;
   case AAX_RENDERER_STRING:
      ret = _aaxALSADriverGetInterfaceName(id);
      break;
   default:
      break;
   }

   return ret;
}

static int
_aaxALSADriverState(const void *id, enum _aaxDriverState state)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = false;

   switch(state)
   {
   case DRIVER_AVAILABLE:
      if (handle && psnd_pcm_state(handle->pcm) != SND_PCM_STATE_DISCONNECTED) {
         rv = true;
      }
      break;
   case DRIVER_PAUSE:
      if (handle)
      {
         if (psnd_pcm_state(handle->pcm) == SND_PCM_STATE_RUNNING &&
             !handle->pause)
         {
            if (handle->can_pause) {
               rv = psnd_pcm_pause(handle->pcm, 1);
            }
         }
         handle->pause = 1;
      }
      break;
   case DRIVER_RESUME:
      if (handle)
      {
         if (psnd_pcm_state(handle->pcm) == SND_PCM_STATE_PAUSED &&
             handle->pause)
         {
            if (handle->can_pause) {
               rv = psnd_pcm_pause(handle->pcm, 0);
            }
         }
         handle->pause = 0;
      }
      break;
   case DRIVER_SHARED_MIXER:
      rv = handle->shared;
      break;
   case DRIVER_SUPPORTS_PLAYBACK:
   case DRIVER_SUPPORTS_CAPTURE:
      rv = true;
      break;
   case DRIVER_NEED_REINIT:
   default:
      break;
   }
   return rv;
}

static float
_aaxALSADriverParam(const void *id, enum _aaxDriverParam param)
{
   _driver_t *handle = (_driver_t *)id;
   float rv = 0.0f;
   if (handle)
   {
      switch(param)
      {
		/* float */
      case DRIVER_LATENCY:
         rv = handle->latency;
         break;
      case DRIVER_MAX_VOLUME:
         rv = handle->volumeMax;
         break;
      case DRIVER_MIN_VOLUME:
         rv = handle->volumeMin;
         break;
      case DRIVER_VOLUME:
         rv = handle->volumeCur;
         break;
      case DRIVER_REFRESHRATE:
         rv = handle->refresh_rate;
         break;

		/* int */
      case DRIVER_MIN_FREQUENCY:
         rv = (float)handle->min_frequency;
         break;
      case DRIVER_MAX_FREQUENCY:
         rv = (float)handle->max_frequency;
         break;
      case DRIVER_MIN_TRACKS:
         rv = (float)handle->min_tracks;
         break;
      case DRIVER_MAX_TRACKS:
         rv = (float)handle->max_tracks;
         break;
      case DRIVER_MIN_PERIODS:
         rv = (float)handle->min_periods;
         break;
      case DRIVER_MAX_PERIODS:
         rv = (float)handle->max_periods;
         break;
      case DRIVER_MAX_SOURCES:
         rv = ((_handle_t*)(handle->handle))->backend.ptr->getset_sources(0, 0);
         break;
      case DRIVER_MAX_SAMPLES:
         rv = AAX_FPINFINITE;
         break;
      case DRIVER_SAMPLE_DELAY:
      {
         snd_pcm_sframes_t avail;
         int res = psnd_pcm_delay(handle->pcm, &avail);
         if (res >= 0) rv = (float)avail;
         break;
      }

		/* boolean */
      case DRIVER_SHARED_MODE:
      case DRIVER_TIMER_MODE:
         rv = (float)true;
         break;
      case DRIVER_BATCHED_MODE:
      default:
         break;
      }
   }
   return rv;
}

static char *
_aaxALSADriverGetDevices(UNUSED(const void *id), int mode)
{
   static char names[2][1024] = { DEFAULT_DEVNAME"\0\0", DEFAULT_DEVNAME"\0\0" };
   static time_t t_previous[2] = { 0, 0 };
   int m = (mode > 0) ? 1 : 0;
   time_t t_now;

   psnd_lib_error_set_handler(_alsa_error_handler_none);
   t_now = time(NULL);
   if (t_now > (t_previous[m]+5))
   {
      void **hints = NULL;
      int res;

      t_previous[m] = t_now;

      res = psnd_device_name_hint(-1, "pcm", &hints);
      if (!res && hints)
      {
         void **lst = hints;
         size_t len = 1024;
         char *ptr;

         ptr = (char *)&names[m];
         *ptr = 0; *(ptr+1) = 0;
         do
         {
            char *type = psnd_device_name_get_hint(*lst, "IOID");
            if (type && !strcmp(type, _alsa_type[m]))
            {
               _sys_free(type);
               type = NULL;
            }
            if (!type)
            {
               char *name = psnd_device_name_get_hint(*lst, "NAME");
               char *colon = name ? strchr(name, ':') : NULL;
               char *comma = colon ? strchr(colon+1, ',') : NULL;

               if (comma)
               {
                  if (!STRCMP(comma+1, "DEV=0") &&
                      (!STRCMP(name, "hw:") || !STRCMP(name, "hdmi:"))
                     )
                  {
                     char *desc, *iface;
                     size_t slen;

                     desc = psnd_device_name_get_hint(*lst, "DESC");
                     if (!desc) desc = name;

                     iface = strstr(desc, ", ");
                     if (iface) *iface = 0;

                     slen = strlen(desc)+1;	/* skip the trailing 0 */
                     if (slen < len)
                     {
                        snprintf(ptr, len, "%s", desc);
                        len -= slen;
                        ptr += slen;
                     }
                     if (desc && desc != name) _sys_free(desc);
                  }
               }
               if (name) _sys_free(name);
            }
            if (type) _sys_free(type);
         }
         while (*(++lst) != NULL);
         *ptr = 0;
      }
      if (hints) psnd_device_name_free_hint(hints);

       /* always end with "\0\0" no matter what */
       names[m][1022] = 0;
       names[m][1023] = 0;
   }
   psnd_lib_error_set_handler(_alsa_error_handler);

   return (char *)&names[m];
}

static char *
_aaxALSADriverGetInterfaces(const void *id, const char *devname, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   int m = (mode > 0) ? 1 : 0;
   char *rv = handle->ifname[m];

   psnd_lib_error_set_handler(_alsa_error_handler_none);
   if (!rv && devname)
   {
      char devlist[1024] = "\0\0";
      void **hints = NULL;
      size_t len = 1024;
      int res;

      res = psnd_device_name_hint(-1, "pcm", &hints);
      if (!res && hints)
      {
         void **lst = hints;
         char *ptr;

         ptr = devlist;
         do
         {
            char *type = psnd_device_name_get_hint(*lst, "IOID");
            if (type && !strcmp(type, _alsa_type[m]))
            {
               _sys_free(type);
               type = NULL;
            }
            if (!type)
            {
               char *name = psnd_device_name_get_hint(*lst, "NAME");
               if (name)
               {
                  char *p = strchr(name, ':');
                  int i = 0;

                  if (p) *(++p) = 0;
                  while (ifname_prefix[i] && strcmp(name, ifname_prefix[i])) {
                     i++;
                  }

                  if (ifname_prefix[i] && (m ||
                       (!strcmp(name, "pulse:") || !strcmp(name, "front:") ||
                        !strcmp(name, "iec958:") || !strcmp(name, "default:")))
                     )
                  {
                     char *desc, *iface;
                     size_t slen;

                     desc = psnd_device_name_get_hint(*lst, "DESC");
                     if (!desc) desc = name;

                     iface = strstr(desc, ", ");
                     if (iface) *iface = 0;

                     if (iface && !strcasecmp(devname, desc))
                     {
                        if (!m) // Input
                        {
                           if (iface != desc)
                           {
                              char *p;

                              iface += 2;
                              p = strchr(iface, '\n');
                              if (p) *p = 0;
                           }

                           slen = strlen(iface)+1;
                           if (slen > (len-1))
                           {
                              _sys_free(desc);
                              break;
                           }

                           snprintf(ptr, len, "%s", iface);
                        }
                        else
                        {
                           if (iface != desc) {
                              iface = strchr(iface+2, '\n')+1;
                           }

                           slen = strlen(iface)+1;
                           if (slen > (len-1))
                           {
                              _sys_free(desc);
                              break;
                           }

                           snprintf(ptr, len, "%s", iface);
                        }

                        len -= slen;
                        ptr += slen;
                     }
                     if (desc && desc != name) _sys_free(desc);
                  }
               }
               if (name) _sys_free(name);
            }
            if (type) _sys_free(type);
         }
         while (*(++lst) != NULL);

         if (ptr != devlist)
         {
            *ptr++ = '\0';
            if (handle->ifname[m]) {
               rv = realloc(handle->ifname[m], ptr-devlist);
            } else {
               rv = malloc(ptr-devlist);
            }
            if (rv)
            {
               handle->ifname[m] = rv;
               memcpy(handle->ifname[m], devlist, ptr-devlist);
            }
         }
      }
      if (hints) psnd_device_name_free_hint(hints);
   }
   psnd_lib_error_set_handler(_alsa_error_handler);

   return rv;
}

static char *
_aaxALSADriverGetDefaultInterface(const void *id, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   int m = (mode > 0) ? 1 : 0;
   char rv[1024]  = "default";
   void **hints = NULL;
   ssize_t len = 1024;
   int res;

   psnd_lib_error_set_handler(_alsa_error_handler_none);
   res = psnd_device_name_hint(-1, "pcm", &hints);
   if (!res && hints)
   {
      void **lst = hints;
      int found = 0;

      do
      {
         char *type = psnd_device_name_get_hint(*lst, "IOID");
         if (type && !strcmp(type, _alsa_type[m]))
         {
            _sys_free(type);
            type = NULL;
         }
         if (!type)
         {
            char *name = psnd_device_name_get_hint(*lst, "NAME");
            if (name)
            {
               char *desc = psnd_device_name_get_hint(*lst, "DESC");
               if (!desc) desc = name;

               if (handle && (!strcasecmp(handle->devname, name) ||
                              !strcasecmp(handle->devname, desc) ||
                              (!strcasecmp(handle->devname, "default")
                                && strstr(name, "default"))))
               {
                  char *iface;
                  if (handle && !strcmp(desc, handle->devname))
                  {
                     free(handle->devname);
                     handle->devname = _aax_strdup(name);
                     snprintf(rv, len, "%s", desc);
                     found = 1;
                  }

                  iface = strstr(desc, ", ");
                  if (iface && (len > (iface-desc)))
                  {
                     char *s = rv;

                     snprintf(s, (iface-desc)+1, "%s", desc);
                     len -= iface-desc;
                     s += (iface-desc);

                     if (m) iface = strchr(desc, '\n')+1;
                     if (!m || !iface) iface = strstr(desc, ", ")+2;

                     snprintf(s, len, ": %s", iface);

                     if (!m && iface)
                     {
                        char *end = strchr(iface+2, '\n');
                        if (end) *end = 0;
                     }
                     found = 1;
                  }
               }
               if (desc && desc != name) _sys_free(desc);
            }
            if (name) _sys_free(name);
         }
         if (type) _sys_free(type);
      }
      while (*(++lst) != NULL && !found);
   }
   if (hints) psnd_device_name_free_hint(hints);

   psnd_lib_error_set_handler(_alsa_error_handler);

   return _aax_strdup(rv);
}

static char *
_aaxALSADriverGetInterfaceName(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   char *rv = NULL;
   void **hints = NULL;
   ssize_t len = 1024;
   int res;

   psnd_lib_error_set_handler(_alsa_error_handler_none);
   if (handle)
   {
      res = psnd_device_name_hint(-1, "pcm", &hints);
      if (!res && hints)
      {
         void **lst = hints;
         int found = 0;

         do
         {
            char *name = psnd_device_name_get_hint(*lst, "NAME");
            if (name)
            {
               if (!strcmp(name, handle->devname))
               {
                  char rvname[1024]  = "default";
                  char *desc = psnd_device_name_get_hint(*lst, "DESC");
                  char *iface, *s = rvname;

                  if (!desc) desc = name;
                  iface = strstr(desc, ", ");

                  if (iface && (len > (iface-desc)))
                  {
                     snprintf(s, (iface-desc)+1, "%s", desc);
                     len -= iface-desc;
                     s += (iface-desc);

                     iface = strchr(desc, '\n')+1;
                     snprintf(s, len, ": %s", iface);
                  }
                  else {
                     snprintf(s, len, "%s", desc);
                  }
                  if (desc && desc != name) _sys_free(desc);

                  found = 1;
                  rv = _aax_strdup(rvname);
               }
            }
            if (name) _sys_free(name);
         }
         while (*(++lst) != NULL && !found);
      }
      if (hints) psnd_device_name_free_hint(hints);
   }
   psnd_lib_error_set_handler(_alsa_error_handler);

   return rv;
}


static char *
_aaxALSADriverLogVar(const void *id, const char *fmt, ...)
{
   char _errstr[1024];
   va_list ap;

   _errstr[0] = '\0';
   va_start(ap, fmt);
   vsnprintf(_errstr, 1024, fmt, ap);

   // Whatever happen in vsnprintf, what i'll do is just to null terminate it
   _errstr[1023] = '\0';
   va_end(ap);

   return _aaxALSADriverLog(id, 0, -1, _errstr);
}

static char *
_aaxALSADriverLog(const void *id, UNUSED(int prio), UNUSED(int type), const char *str)
{
   _driver_t *handle = id ? ((_driver_t *)id)->handle : NULL;
   static char _errstr[256];

   if (snprintf(_errstr, 255, "alsa: %s\n", str) < 0) {
      _errstr[255] = '\0';  /* always null terminated */
   }

   __aaxDriverErrorSet(handle, AAX_BACKEND_ERROR, (char*)&_errstr);
   _AAX_SYSLOG(_errstr);
#ifndef NDEBUG
   printf("%s", _errstr);
#endif

   return (char*)&_errstr;
}

/*-------------------------------------------------------------------------- */

static const char* ifname_prefix[MAX_PREFIX] = {
   "front:", "rear:", "center_lfe:", "side:", "iec958:", "hdmi:", NULL
};

static const _alsa_formats_t _alsa_formats[MAX_FORMATS] =
{
   {32, SND_PCM_FORMAT_S24_LE},
   {32, SND_PCM_FORMAT_S32_LE},
   {16, SND_PCM_FORMAT_S16_LE},
   {24, SND_PCM_FORMAT_S24_3LE},
   { 8, SND_PCM_FORMAT_U8},
   { 0, 0}
};

static const char* _const_alsa_default_name[2] = {
   DEFAULT_HWDEVNAME, DEFAULT_DEVNAME_OLD
};

static const char* _alsa_type[2] = {
   "Input", "Output"
};

static const snd_pcm_stream_t _alsa_mode[2] = {
   SND_PCM_STREAM_CAPTURE, SND_PCM_STREAM_PLAYBACK
};

static int
_alsa_pcm_open(_driver_t *handle, int m)
{
   char name[32];
   int err;

   snprintf(name, 32, "hw:%d", handle->devnum);

   /**
    * Test whether this device has hardware mixing,
    * if so set handle->shared_volume to true.
    *
    * This is after detecting the proper device name so it only
    * affects volume handling and not timing or which device to choose.
    */
   if (m && !handle->pcm)
   {
      snd_pcm_info_t *pcminfo;
      snd_ctl_t *ctl;
      int res;

      res = psnd_pcm_info_malloc(&pcminfo);
      if (res >= 0)
      {
         res = psnd_ctl_open(&ctl, name, _alsa_mode[m]);
//       psnd_config_update_free_global();
         if (res >= 0)
         {
            res = psnd_ctl_pcm_info(ctl, pcminfo);
            if (res >= 0)
            {
               res = psnd_pcm_info_get_subdevices_count(pcminfo);
               if (res > 1) handle->shared_volume = true;
            }
            psnd_ctl_close(ctl);
         }
         psnd_pcm_info_free(pcminfo);
      }
   }

   if (!strcmp(handle->devname, "default"))
   {
      free(handle->devname);
      handle->devname = _aax_strdup(name);
   }

   err = psnd_pcm_open(&handle->pcm, handle->devname, _alsa_mode[m],
                       SND_PCM_NONBLOCK);

// psnd_config_update_free_global();
   if (err >= 0)
   {
      err = psnd_pcm_nonblock(handle->pcm, 1);
      if (err >= 0)
      {
         err = psnd_mixer_open(&handle->mixer, 0);
//       psnd_config_update_free_global();
         if (err >= 0)
         {
            char name[8];
            snprintf(name, 8, "hw:%i", handle->devnum);
            err = psnd_hctl_open(&handle->hctl, name, 0);
//          psnd_config_update_free_global();
            if (err >= 0) {
               err = psnd_mixer_attach_hctl(handle->mixer, handle->hctl);
            }
         }
         if (err >= 0) {
            err = psnd_mixer_selem_register(handle->mixer, NULL, NULL);
         }
         if (err >= 0)
         {
            err = psnd_mixer_load(handle->mixer);
            _alsa_get_volume_range(handle);
         }
         if (err < 0)
         {
            psnd_mixer_detach_hctl(handle->mixer, handle->hctl);
            psnd_hctl_close(handle->hctl);
            psnd_mixer_close(handle->mixer);
            handle->hctl = NULL;
            handle->mixer = NULL;
         }
      }
      else
      {
         _alsa_pcm_close(handle);
         handle->pcm = NULL;
      }
   }
   else {
      handle->pcm = NULL;
   }

   return err;
}

static int
_alsa_pcm_close(_driver_t *handle)
{
   int err = 0;
   if (handle)
   {
      if (handle->mixer)
      {
         _alsa_set_volume(handle, NULL, 0, 0, 0, handle->volumeInit);
         psnd_mixer_detach_hctl(handle->mixer, handle->hctl);
         psnd_hctl_close(handle->hctl);
         psnd_mixer_close(handle->mixer);
         handle->hctl = NULL;
         handle->mixer = NULL;
      }
      if (handle->pcm) {
         err = psnd_pcm_close(handle->pcm);
      }
   }
   return err;
}

static int
_alsa_set_access(const void *id, snd_pcm_hw_params_t *hwparams)
{
   _driver_t *handle = (_driver_t*)id;
   snd_pcm_t *hid = handle->pcm;
   int err = 0;
   char *s;

   /* for testing purposes */
   s = getenv("AAX_USE_MMAP");
   if (s && (_aax_getbool(s) == false))
   {
      handle->use_mmap = 0;
      handle->interleaved = 1;
      handle->play = _aaxALSADriverPlayback_rw_il;
      err = psnd_pcm_hw_params_set_access(hid, hwparams,
                                          SND_PCM_ACCESS_RW_INTERLEAVED);
      if (err < 0) _AAX_DRVLOG("unable to set interleaved mode");
   }
   else if (err >= 0)                     /* playback */
   {
      handle->use_mmap = 1;
      handle->interleaved = 0;
      handle->play = _aaxALSADriverPlayback_mmap_ni;
      err = psnd_pcm_hw_params_set_access(hid, hwparams,
                                          SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
      if (err < 0)
      {
         handle->use_mmap = 0;
         handle->play = _aaxALSADriverPlayback_rw_ni;
         err = psnd_pcm_hw_params_set_access(hid, hwparams,
                                             SND_PCM_ACCESS_RW_NONINTERLEAVED);
      }

      if (err < 0)
      {
         handle->use_mmap = 1;
         handle->interleaved = 1;
         handle->play = _aaxALSADriverPlayback_mmap_il;
         err = psnd_pcm_hw_params_set_access(hid, hwparams,
                                             SND_PCM_ACCESS_MMAP_INTERLEAVED);
         if (err < 0)
         {
            handle->use_mmap = 0;
            handle->play = _aaxALSADriverPlayback_rw_il;
            err = psnd_pcm_hw_params_set_access(hid, hwparams,
                                                SND_PCM_ACCESS_RW_INTERLEAVED);
         }

         if (err < 0) _AAX_DRVLOG("unable to find a proper renderer");
      }
   }

   return err;
}

static void
_alsa_error_handler_none(UNUSED(const char *file), UNUSED(int line), UNUSED(const char *function), UNUSED(int err), UNUSED(const char *fmt), ...)
{
}

static void
_alsa_error_handler(const char *file, int line, const char *function, UNUSED(int err), const char *fmt, ...)
{
   const void *id = NULL;
   char s[1024];
   va_list ap;

#ifndef NDEBUG
   snprintf(s, 1024, "%s at line %i in function %s:", file, line, function);
   _AAX_LOG(LOG_ERR, s);
#endif

   va_start(ap, fmt);
// snprintf(s, 1024, fmt, va_arg(ap, char *));
   vsnprintf(s, 1024, fmt, ap);
   va_end(ap);

   _AAX_DRVLOG(s);
}


static char *
detect_devname(_driver_t *handle, int m)
{
   static const char* dev_prefix[] = {
         "hw:", "front:", "surround40:", "surround51:", "surround71:"
   };
   static char rvname[1025];
   unsigned int tracks = handle->no_tracks;
   char *devname = _aax_strdup(handle->name);
   char vmix = handle->shared;
   char *rv = (char*)handle->default_name[m];

   if (devname && (tracks < _AAX_MAX_SPEAKERS))
   {
      unsigned int tracks_2 = tracks/2;
      void **hints = NULL;
      int res;

      res = psnd_device_name_hint(-1, "pcm", &hints);
      if (!res && hints)
      {
         void **lst = hints;
         char *ifname;

         ifname = strstr(devname, ": ");
         if (ifname && (strlen(ifname) >= 2))
         {
             *ifname = 0;
            ifname += 2;
         }
         else {
            ifname = NULL;
         }

         do
         {
            char *type = psnd_device_name_get_hint(*lst, "IOID");
            if (type && !strcmp(type, _alsa_type[m]))
            {
               _sys_free(type);
               type = NULL;
            }
            if (!type)
            {
               char *name = psnd_device_name_get_hint(*lst, "NAME");
               if (name)
               {
                  int i = 0;
                  while (ifname_prefix[i] && STRCMP(name, ifname_prefix[i])) {
                     i++;
                  }

                  if (!strcmp(devname, "pulse"))
                  {
                     snprintf(rvname, 1024, "%s", devname);
                     rv = rvname;
                  }
                  else if (ifname_prefix[i] && !STRCMP(devname, name))
                  {
                     size_t dlen = strlen(name)+1;
                     if (vmix)
                     {
                         dlen += strlen("plug:''");
                         if (m) dlen += strlen("dmix:");
                         else dlen += strlen("dsnoop:");
                     }

                     rv = malloc(dlen);
                     if (rv)
                     {
                        if (vmix)
                        {
                            snprintf(rv, dlen, "plug:'%s%s'",
                                         m ? "dmix:" : "dsnoop:",
                                         name + strlen(ifname_prefix[i]));
                        }
                        else {
                            snprintf(rv, dlen, "%s", name);
                        }
                     }
                     if (name) _sys_free(name);
                     break;
                  }
                  else if (ifname_prefix[i] &&
                           ((tracks_2 <= (_AAX_MAX_SPEAKERS/2)) ||
                            (STRCMP(name, dev_prefix[m ? tracks_2 : 0]) == 0))
                          )
                  {
                     char *desc = psnd_device_name_get_hint(*lst, "DESC");
                     char *iface, *description = 0;

                     if (!desc) desc = name;
                     iface = strstr(desc, ", ");
                     if (iface)
                     {
                        *iface++ = 0;
                        description = strchr(++iface, '\n');
                        if (description) *description++ = 0;
                        else description = iface;
                     }

                     if (!strcasecmp(devname, desc))
                     {
                        if (ifname)
                        {
                           if ((iface && !strcasecmp(ifname, iface)) ||
                               (description && !strcasecmp(ifname, description)))
                           {
                              size_t dlen = strlen(name)+1;
                              dlen += strlen(dev_prefix[m ? tracks_2 : 0]);
                              if (vmix)
                              {
                                 dlen += strlen("plug:''");
                                 if (m) dlen += strlen("dmix:");
                                 else dlen += strlen("dsnoop:");
                              }

                              rv = malloc(dlen);
                              if (rv)
                              {
                                 if (vmix) {
                                    snprintf(rv, dlen, "plug:'%s%s'",
                                                 m ? "dmix:" : "dsnoop:",
                                                 name+strlen(ifname_prefix[i]));
                                 }
                                 else if (ifname_prefix[i]) {
                                    snprintf(rv, dlen, "%s%s",
                                                 (tracks_2 > 1)
                                                 ? dev_prefix[m ? tracks_2 : 0]
                                                 : ifname_prefix[i],
                                                 name+strlen(ifname_prefix[i]));
                                 }
                              }
                              if (desc && desc != name) _sys_free(desc);
                              if (name) _sys_free(name);
                              break;
                           }
                        }
                        else	// no interface specified, use sysdefault
                        {
                           if (desc && desc != name) _sys_free(desc);
                           rv = name;
                           break;
                        }
                     }
                     if (desc && desc != name) _sys_free(desc);
                  }
               }
               if (name) _sys_free(name);
            }
            if (type) _sys_free(type);
         }
         while (*(++lst) != NULL);
      }
      if (hints) psnd_device_name_free_hint(hints);
      _aax_free(devname);
   }

   return rv;
}


static int
detect_devnum(_driver_t *handle, int m)
{
   const char *devname = handle->name;
   int devnum = handle->default_devnum;

   if (devname)
   {
      void **hints;
      int res = psnd_device_name_hint(-1, "pcm", &hints);

      if (!res && hints)
      {
         char card[MAX_ID_STRLEN] = "";
         int found, ctr = -1;
         void **lst = hints;
         size_t len;
         char *ptr;

         ptr = strstr(devname, ": ");
         if (ptr) len = ptr-devname;
         else len = strlen(devname);

         found = 0;
         do
         {
            char *type = psnd_device_name_get_hint(*lst, "IOID");
            if (type && !strcmp(type, _alsa_type[m]))
            {
               _sys_free(type);
               type = NULL;
            }
            if (!type)
            {
               char *name = psnd_device_name_get_hint(*lst, "NAME");
               if (name)
               {
                  char *p2, *p1 = strstr(name, "CARD=");
                  if (p1)
                  {
                     int cardlen;
                     p1 += strlen("CARD=");

                     p2 = strchr(p1, ',');
                     if (p2) cardlen = _MIN(p2-p1, MAX_ID_STRLEN-1);
                     else cardlen = _MIN(strlen(p1), MAX_ID_STRLEN-1);

                     if (strncmp(p1, card, cardlen) != 0)
                     {
                        memcpy(card, p1, cardlen);
                        card[cardlen] = 0;
                        ctr++;
                     }
                  }

                  if (!strcasecmp(devname, name))
                  {
                     _sys_free(name);
                     devnum = ctr;
                     break;
                  }

                  if (!found && !STRCMP(name, "front:"))
                  {
                     _sys_free(name);
                     if (!strcasecmp(devname, "default"))
                     {
                        devnum = ctr;
                        found = 1;
                     }
                     else
                     {
                        char *desc = psnd_device_name_get_hint(*lst, "DESC");
                        char *iface;

                        if (!desc) continue;

                        iface = strstr(desc, ", ");
                        if (iface) *iface = 0;

                        if (!strncasecmp(devname, desc, len))
                        {
                           handle->min_tracks = 2;
                           devnum = ctr;
                           found = 1;
                        }
                        _sys_free(desc);
                     }
                  }
                  else if (!STRCMP(name, "surround"))
                  {
                     char *desc, *iface;

                     desc = psnd_device_name_get_hint(*lst, "DESC");
                     if (!desc) continue;

                     iface = strstr(desc, ", ");
                     if (iface) *iface = 0;

                     if (!strncasecmp(devname, desc, len))
                     {
                        int pos = strlen("surround");
                        handle->max_tracks = name[pos]-'0' + name[pos+1]-'0';
                     }
                     _sys_free(name);
                     _sys_free(desc);
                  }
                  else {
                      _sys_free(name);
                  }

               }
            }
         }
         while (*(++lst) != NULL);
      }

      res = psnd_device_name_free_hint(hints);
   }

   return devnum;
}

unsigned int
get_devices_avail(int mode)
{
   static unsigned int rv[2] = {0, 0};
   int m = (mode > 0) ? 1 : 0;
   void **hints = NULL;
   int res;

   if (rv[m] == 0)
   {
      res = psnd_device_name_hint(-1, "pcm", &hints);
      if (!res)
      {
         void **lst = hints;
         do
         {
            char *type = psnd_device_name_get_hint(*lst, "IOID");
            if (type && !strcmp(type, _alsa_type[m]))
            {
               char *name = psnd_device_name_get_hint(*lst, "NAME");
               if (name && (!STRCMP(name,"front:") || !STRCMP(name,"hw:") ||
                            strstr(name,"default:")))
               {
                  rv[m]++;
               }
               if (name) _sys_free(name);
            }
            if (type) _sys_free(type);
         }
         while (*(++lst) != NULL);
         res = psnd_device_name_free_hint(hints);
      }
   }

   return rv[m];
}

static int
_alsa_get_volume_range_element(_driver_t *handle, const char *elem_name1,
                                                  const char *elem_name2,
                                                  snd_mixer_selem_id_t *sid,
                                                  char m)
{
   snd_mixer_elem_t *elem;
   const char *mixer_name;
   int rv = 0;

   psnd_mixer_selem_id_set_index(sid, 0);

   mixer_name = elem_name1;
   psnd_mixer_selem_id_set_name(sid, elem_name1);
   elem = psnd_mixer_find_selem(handle->mixer, sid);
   if (!elem && elem_name2)
   {
      mixer_name = elem_name2;
      psnd_mixer_selem_id_set_name(sid, elem_name2);
      elem = psnd_mixer_find_selem(handle->mixer, sid);
   }

   if (elem)
   {
      long min, max, init;
      if (m)
      {
         if (handle->outMixer) free(handle->outMixer);
         handle->outMixer = _aax_strdup(mixer_name);

         rv = psnd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &init);
         psnd_mixer_selem_get_playback_dB_range(elem, &min, &max);
         psnd_mixer_selem_set_playback_volume_all(elem, init);
      }
      else
      {
         rv = psnd_mixer_selem_get_capture_volume(elem, SND_MIXER_SCHN_MONO, &init);
         psnd_mixer_selem_get_capture_dB_range(elem, &min, &max);
         psnd_mixer_selem_set_capture_volume_all(elem, init);
      }

      handle->volumeMin = _db2lin((float)min*0.01f);
      handle->volumeMax = _db2lin((float)max*0.01f);
      handle->volumeStep = 0.5f/((float)max-(float)min);

      handle->volumeInit = (float)init*0.01f;
      handle->volumeCur = _MAX(handle->volumeInit, 1.0f);
   }

   return rv;
}

static int
_alsa_get_volume_range(_driver_t *handle)
{
   snd_mixer_selem_id_t *sid;
   int rv = 0;
   char m;

   m = (handle->mode == AAX_MODE_READ) ? 0 : 1;

   psnd_mixer_selem_id_malloc(&sid);
   if (!m) {	/* AAX_MODE_READ */
      _alsa_get_volume_range_element(handle, "Capture", NULL, sid, m);
   }
   else 	/* AAX_MODE_WRITE_* */
   {
      _alsa_get_volume_range_element(handle, "Front", "Speaker", sid, m);

      if (handle->volumeMax == 0.0f) {
         _alsa_get_volume_range_element(handle, "PCM", NULL, sid, m);
      }

      if (handle->volumeMax == 0.0f) {
         _alsa_get_volume_range_element(handle, "Master", NULL, sid, m);
      }
   }
   psnd_mixer_selem_id_free(sid);

   return rv;
}

static float
_alsa_set_volume(_driver_t *handle, _aaxRingBuffer *rb, ssize_t offset, snd_pcm_sframes_t no_frames, UNUSED(unsigned int no_tracks), float volume)
{
   float gain = fabsf(volume);
   float hwgain = gain;
   float rv = 0;

   if (handle && handle->mixer && !handle->shared_volume && handle->volumeMax)
   {
      hwgain = _MINMAX(fabsf(gain), handle->volumeMin, handle->volumeMax);

      /*
       * Slowly adjust volume to dampen volume slider movement.
       * If the volume step is large, don't dampen it.
       * volume is negative for auto-gain mode.
       */
      if ((volume < 0.0f) && (handle->mode == AAX_MODE_READ))
      {
         float dt = no_frames/handle->frequency_hz;
         float rr = _MINMAX(dt/10.0f, 0.0f, 1.0f);	/* 10 sec average */

         /* Quickly adjust for a very large step in volume */
         if (fabsf(hwgain - handle->volumeCur) > 0.825f) rr = 0.9f;

         hwgain = (1.0f-rr)*handle->hwgain + (rr)*hwgain;
         handle->hwgain = hwgain;
      }

      if (fabsf(hwgain - handle->volumeCur) >= handle->volumeStep)
      {
         long volume = (long)(hwgain*100.0f);
         snd_mixer_selem_id_t *sid;
         snd_mixer_elem_t *elem;

         psnd_mixer_selem_id_malloc(&sid);
         psnd_mixer_selem_id_set_index(sid, 0);
         if (handle->mode == AAX_MODE_READ)
         {
            psnd_mixer_selem_id_set_name(sid, "Capture");
            elem = psnd_mixer_find_selem(handle->mixer, sid);
            if (elem && psnd_mixer_selem_has_capture_volume(elem)) {
               psnd_mixer_selem_set_capture_volume_all(elem, volume);
            }
         }
         else
         {
            psnd_mixer_selem_id_set_name(sid, "Center");
            elem = psnd_mixer_find_selem(handle->mixer, sid);
            if (elem && psnd_mixer_selem_has_playback_volume(elem)) {
               psnd_mixer_selem_set_playback_volume_all(elem, volume);
            }
            psnd_mixer_selem_id_set_name(sid, "Surround");
            elem = psnd_mixer_find_selem(handle->mixer, sid);
            if (elem && psnd_mixer_selem_has_playback_volume(elem)) {
               psnd_mixer_selem_set_playback_volume_all(elem, volume);
            }
            psnd_mixer_selem_id_set_name(sid, "LFE");
            elem = psnd_mixer_find_selem(handle->mixer, sid);
            if (elem && psnd_mixer_selem_has_playback_volume(elem)) {
               psnd_mixer_selem_set_playback_volume_all(elem, volume);
            }
            psnd_mixer_selem_id_set_name(sid, "Side");
            elem = psnd_mixer_find_selem(handle->mixer, sid);
            if (elem && psnd_mixer_selem_has_playback_volume(elem)) {
               psnd_mixer_selem_set_playback_volume_all(elem, volume);
            }
         }
         psnd_mixer_selem_id_free(sid);

         handle->volumeCur = hwgain;
      }

      if (hwgain) gain /= hwgain;
      else gain = 0.0f;
      rv = gain;
   }

   /* software volume fallback */
   if (rb && fabsf(gain - 1.0f) > LEVEL_32DB) {
      rb->data_multiply(rb, offset, no_frames, gain);
   }

   return rv;
}

static int
_xrun_recovery(const void *id, snd_pcm_t *handle, int err)
{
   int res = psnd_pcm_recover(handle, err, 1);
#if 0
   snd_output_t *output = NULL;

   psnd_output_stdio_attach(&output, stdout, 0);
   psnd_pcm_dump(handle, output);
#endif
   if (res != 0) {
      _AAX_DRVLOG("xrun recovery failure");
   }
   else if (res == -EPIPE)
   {
      if (psnd_pcm_stream(handle) == SND_PCM_STREAM_CAPTURE)
      {
         /* capturing requirs an explicit call to snd_pcm_start */
         res = psnd_pcm_start(handle);
         if (res != 0) {
            _AAX_DRVLOG("input stream restart failure");
         }
      }
      else
      {
         res = psnd_pcm_prepare(handle);
         if (res != 0) {
            _AAX_DRVLOG("output stream restart failure");
         }
      }
   }
   return res;
}

#ifndef NDEBUG
static int
_xrun_recovery_debug(const void *id, snd_pcm_t *handle, int err, int line)
{
    printf("Alsa xrun error at line: %i\n", line);
    return _xrun_recovery(id, handle, err);
}
#endif


static size_t
_aaxALSADriverPlayback_mmap_ni(const void *id, void *src, UNUSED(float pitch), float gain, UNUSED(char batched))
{
   _driver_t *handle = (_driver_t *)id;
   _aaxRingBuffer *rbs = (_aaxRingBuffer *)src;
   unsigned int no_tracks, t, chunk;
   snd_pcm_sframes_t period_frames;
   snd_pcm_sframes_t avail;
   snd_pcm_state_t state;
   snd_pcm_sframes_t res;
   const int32_t **sbuf;
   size_t offs, rv = 0;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(handle != 0);
   if (handle->pause) return 0;

   assert(rbs != 0);

   offs = rbs->get_parami(rbs, RB_OFFSET_SAMPLES);
   no_tracks = rbs->get_parami(rbs, RB_NO_TRACKS);
   period_frames = rbs->get_parami(rbs, RB_NO_SAMPLES) - offs;

   state = psnd_pcm_state(handle->pcm);
   if (state != SND_PCM_STATE_RUNNING) {
      psnd_pcm_start(handle->pcm);
   }

   do
   {
      avail = psnd_pcm_avail(handle->pcm);
      if (avail < 0)
      {
         res = xrun_recovery(handle->pcm, avail);
         if (res < 0)
         {
            char s[255];
            snprintf(s, 255, "avail: %s\n", psnd_strerror(res));
            _AAX_DRVLOG(s);
            return 0;
         }
      }
   }
   while (avail < 0);
   rv = avail;

   _alsa_set_volume(handle, rbs, offs, period_frames, no_tracks, gain);

   if (avail < period_frames) {
      usecSleep(500);
   }

   chunk = 10;
   sbuf = (const int32_t **)rbs->get_tracks_ptr(rbs, RB_READ);
   do
   {
      const snd_pcm_channel_area_t *area;
      snd_pcm_uframes_t frames = period_frames;
      snd_pcm_uframes_t mmap_offs;

      res = psnd_pcm_mmap_begin(handle->pcm, &area, &mmap_offs, &frames);
      if (res < 0)
      {
         res = xrun_recovery(handle->pcm, res);
         if (res < 0)
         {
            char s[255];
            snprintf(s, 255, "mmap begin: %s\n", psnd_strerror(res));
            _AAX_DRVLOG(s);
            rv = 0;
            break;
         }
      }

      for (t=0; t<no_tracks; t++)
      {
         unsigned char *p;
         p = (((unsigned char *)area[t].addr) + (area[t].first/8));
         p += (mmap_offs*handle->bits_sample)/8;
         handle->cvt_to(p, sbuf[t]+offs, frames);
      }

      res = psnd_pcm_mmap_commit(handle->pcm, mmap_offs, frames);
      if (res < 0 || res != (int)frames)
      {
         res = xrun_recovery(handle->pcm, res >= 0 ? -EPIPE : res);
         if (res < 0)
         {
            rv = 0;
            break;
         }
         res = 0;
      }

      offs += res;
      period_frames -= res;
   }
   while ((period_frames > 0) && --chunk);
   rbs->release_tracks_ptr(rbs);

   if (!chunk) _AAX_DRVLOG("too many playback tries");

   return rv;
}


static size_t
_aaxALSADriverPlayback_mmap_il(const void *id, void *src, UNUSED(float pitch), float gain, UNUSED(char batched))
{
   _driver_t *handle = (_driver_t *)id;
   _aaxRingBuffer *rbs = (_aaxRingBuffer *)src;
   unsigned int no_tracks, chunk;
   snd_pcm_sframes_t period_frames;
   snd_pcm_sframes_t avail;
   snd_pcm_state_t state;
   snd_pcm_sframes_t res;
   const int32_t **sbuf;
   size_t offs, rv = 0;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(handle != 0);
   if (handle->pause) return 0;

   assert(rbs != 0);

   offs = rbs->get_parami(rbs, RB_OFFSET_SAMPLES);
   period_frames = rbs->get_parami(rbs, RB_NO_SAMPLES) - offs;
   no_tracks = rbs->get_parami(rbs, RB_NO_TRACKS);

   state = psnd_pcm_state(handle->pcm);
   if (state != SND_PCM_STATE_RUNNING) {
      psnd_pcm_start(handle->pcm);
   }

   do
   {
      avail = psnd_pcm_avail(handle->pcm);
      if (avail < 0)
      {
         res = xrun_recovery(handle->pcm, avail);
         if (res < 0)
         {
            char s[255];
            snprintf(s, 255, "avail: %s\n", psnd_strerror(res));
            _AAX_DRVLOG(s);
            return 0;
         }
      }
   }
   while (avail < 0);
   rv = avail;

   _alsa_set_volume(handle, rbs, offs, period_frames, no_tracks, gain);

   if (avail < period_frames) {
      usecSleep(500);
   }

   chunk = 10;
   sbuf = (const int32_t **)rbs->get_tracks_ptr(rbs, RB_READ);
   do
   {
      const snd_pcm_channel_area_t *area;
      snd_pcm_uframes_t frames = period_frames;
      snd_pcm_uframes_t mmap_offs;
      char *p;

      res = psnd_pcm_mmap_begin(handle->pcm, &area, &mmap_offs, &frames);
      if (res < 0)
      {
         res = xrun_recovery(handle->pcm, res);
         if (res < 0)
         {
            char s[255];
            snprintf(s, 255, "mmap begin: %s\n",psnd_strerror(res));
            _AAX_DRVLOG(s);
            rv = 0;
            break;
         }
      }

      p = (char *)area->addr + ((area->first + area->step*mmap_offs) >> 3);
      handle->cvt_to_intl(p, sbuf, offs, no_tracks, frames);

      res = psnd_pcm_mmap_commit(handle->pcm, mmap_offs, frames);
      if (res < 0 || res != (int)frames)
      {
         res = xrun_recovery(handle->pcm, res >= 0 ? -EPIPE : res);
         if (res < 0)
         {
            rv = 0;
            break;
         }
         res = 0;
      }

      offs += res;
      period_frames -= res;
   }
   while ((period_frames > 0) && --chunk);
   rbs->release_tracks_ptr(rbs);

   if (!chunk) _AAX_DRVLOG("too many playback tries");

   return rv;
}


static size_t
_aaxALSADriverPlayback_rw_ni(const void *id, void *src, UNUSED(float pitch), float gain, UNUSED(char batched))
{
   _driver_t *handle = (_driver_t *)id;
   _aaxRingBuffer *rbs = (_aaxRingBuffer *)src;
   char **data[_AAX_MAX_SPEAKERS];
   unsigned int t, no_tracks;
   unsigned int hw_bits, chunk;
   snd_pcm_sframes_t period_frames;
   snd_pcm_sframes_t avail;
// snd_pcm_state_t state;
   snd_pcm_sframes_t res;
   const int32_t **sbuf;
   size_t outbuf_size;
   size_t offs, rv = 0;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(handle != 0);
   if (handle->pause) return 0;

   assert(rbs != 0);

   offs = rbs->get_parami(rbs, RB_OFFSET_SAMPLES);
   no_tracks = rbs->get_parami(rbs, RB_NO_TRACKS);
   period_frames = rbs->get_parami(rbs, RB_NO_SAMPLES);
   hw_bits = handle->bits_sample;

   outbuf_size = (no_tracks * period_frames*hw_bits)/8;
   if (handle->ptr == 0 || (handle->buf_len < outbuf_size))
   {
      size_t offs, size;
      char *p;

      if (handle->ptr) _aax_free(handle->ptr);
      handle->buf_len = outbuf_size;

      outbuf_size = SIZE_ALIGNED((period_frames*hw_bits)/8);

      offs = no_tracks * sizeof(void*);
      size = no_tracks * outbuf_size;
      handle->ptr = (void**)_aax_malloc(&p, offs, size);
      handle->scratch = (char**)p;

      for (t=0; t<no_tracks; t++)
      {
         handle->ptr[t] = p;
         p += outbuf_size;
      }
   }

#if 0
   state = psnd_pcm_state(handle->pcm);
   if (state != SND_PCM_STATE_RUNNING) {
      psnd_pcm_start(handle->pcm);
   }
#endif

   period_frames -= offs;
   do
   {
      avail = psnd_pcm_avail(handle->pcm);
      if (avail < 0)
      {
         res = xrun_recovery(handle->pcm, avail);
         if (res < 0)
         {
            char s[255];
            snprintf(s, 255, "avail: %s\n", psnd_strerror(res));
            _AAX_DRVLOG(s);
            return 0;
         }
      }
   }
   while (avail < 0);
   rv = avail;

   _alsa_set_volume(handle, rbs, offs, period_frames, no_tracks, gain);

   sbuf = (const int32_t**)rbs->get_tracks_ptr(rbs, RB_READ);
   for (t=0; t<no_tracks; t++)
   {
      data[t] = handle->ptr[t];
      handle->cvt_to(data[t], sbuf[t]+offs, period_frames);
   }
   rbs->release_tracks_ptr(rbs);

   if (avail < period_frames) {
      usecSleep(500);
   }

   chunk = 10;
   do
   {
      res = psnd_pcm_writen(handle->pcm, (void**)data, period_frames);
      if (res < 0)
      {
         res = xrun_recovery(handle->pcm, res);
         if (res < 0)
         {
            _AAX_DRVLOG("xrun_recovery failure");
            rv = 0;
            break;
         }
         res = 0;
      }

      for (t=0; t<no_tracks; t++) {
         data[t] += (res*hw_bits)/8;
      }
      period_frames -= res;
   }
   while ((period_frames > 0) && --chunk);
   if (!chunk) _AAX_DRVLOG("too many playback tries");

   return rv;
}


static size_t
_aaxALSADriverPlayback_rw_il(const void *id, void *src, UNUSED(float pitch), float gain, UNUSED(char batched))
{
   _driver_t *handle = (_driver_t *)id;
   _aaxRingBuffer *rbs = (_aaxRingBuffer *)src;
   unsigned int no_tracks, chunk, hw_bits;
   snd_pcm_sframes_t period_frames;
   snd_pcm_sframes_t avail;
// snd_pcm_state_t state;
   snd_pcm_sframes_t res;
   const int32_t **sbuf;
   size_t outbuf_size;
   size_t offs, rv = 0;
   char *data;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(handle != 0);
   if (handle->pause) return 0;

   assert(rbs != 0);

   offs = rbs->get_parami(rbs, RB_OFFSET_SAMPLES);
   no_tracks = rbs->get_parami(rbs, RB_NO_TRACKS);
   period_frames = rbs->get_parami(rbs, RB_NO_SAMPLES);
   hw_bits = handle->bits_sample;

   outbuf_size = (no_tracks * period_frames*hw_bits)/8;
   if (handle->ptr == 0 || (handle->buf_len < outbuf_size))
   {
      char *p;

      _aax_free(handle->ptr);
      handle->buf_len = outbuf_size;

      handle->ptr = (void**)_aax_malloc(&p, 0, outbuf_size);
      handle->scratch = (char**)p;
   }

#if 0
   state = psnd_pcm_state(handle->pcm);
   if (state != SND_PCM_STATE_RUNNING) {
      psnd_pcm_start(handle->pcm);
   }
#endif

   period_frames -= offs;
   do
   {
      avail = psnd_pcm_avail(handle->pcm);
      if (avail < 0)
      {
         res = xrun_recovery(handle->pcm, avail);
         if (res < 0)
         {
            char s[255];
            snprintf(s, 255, "avail: %s\n", psnd_strerror(res));
            _AAX_DRVLOG(s);
            return 0;
         }
      }
   }
   while (avail < 0);
   rv = avail;

   _alsa_set_volume(handle, rbs, offs, period_frames, no_tracks, gain);

   data = (char*)handle->scratch;
   sbuf = (const int32_t**)rbs->get_tracks_ptr(rbs, RB_READ);
   handle->cvt_to_intl(data, sbuf, offs, no_tracks, period_frames);
   rbs->release_tracks_ptr(rbs);

   if (avail < period_frames) {
      usecSleep(500);
   }

   chunk = 10;
   do
   {
      res = psnd_pcm_writei(handle->pcm, data, period_frames);
      if (res < 0)
      {
         res = xrun_recovery(handle->pcm, res);
         if (res < 0)
         {
            _AAX_DRVLOG("xrun_recovery failure");
            rv = 0;
            break;
         }
         res = 0;
      }

      data += (res * no_tracks*hw_bits)/8;
      period_frames -= res;
   }
   while ((period_frames > 0) && --chunk);
   if (!chunk) _AAX_DRVLOG("too many playback tries");

   return rv;
}

static size_t
_aaxALSADriverPlayback(const void *id, void *src, float pitch, float gain,
                       char batched)
{
   _driver_t *handle = (_driver_t *)id;
   size_t res;

   res = handle->play(id, src, pitch, gain, batched);

   return handle->max_frames - res;
}

_aaxRenderer*
_aaxALSADriverRender(const void* config)
{
   _driver_t *handle = (_driver_t *)config;
   return handle->render;
}

int
_aaxALSADriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
#if ENABLE_TIMING
   _aaxTimer *timer = _aaxTimerCreate();
#endif
   float delay_sec, wait_us; // no_samples
   _intBufferData *dptr_sensor;
   const _aaxDriverBackend *be;
   _aaxRingBuffer *dest_rb;
   _aaxAudioFrame *mixer;
   _driver_t *be_handle;
   const void *id;
   int stdby_time;
   char state;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return false;
   }

   delay_sec = 1.0f/handle->info->period_rate;

   be = handle->backend.ptr;
   id = handle->backend.handle;		// Required for _AAX_DRVLOG

   dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      _sensor_t* sensor = _intBufGetDataPtr(dptr_sensor);

      mixer = sensor->mixer;

      dest_rb = be->get_ringbuffer(MAX_EFFECTS_TIME, mixer->info->mode);
      if (dest_rb)
      {
         dest_rb->set_format(dest_rb, AAX_PCM24S, true);
         dest_rb->set_parami(dest_rb, RB_NO_TRACKS, mixer->info->no_tracks);
         dest_rb->set_paramf(dest_rb, RB_FREQUENCY, mixer->info->frequency);
         dest_rb->set_paramf(dest_rb, RB_DURATION_SEC, delay_sec);
         dest_rb->init(dest_rb, true);
         dest_rb->set_state(dest_rb, RB_STARTED);

         handle->ringbuffer = dest_rb;
      }
      _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

      if (!dest_rb) {
         return false;
      }
   }
   else {
      return false;
   }

   be->state(handle->backend.handle, DRIVER_PAUSE);
   state = AAX_SUSPENDED;

   be_handle = (_driver_t *)handle->backend.handle;
   if (be_handle->use_timer) {
      _aaxProcessSetPriority(-20);
   }

   wait_us = delay_sec*1000000.0f;
// no_samples = dest_rb->get_parami(dest_rb, RB_NO_SAMPLES);
   stdby_time = (int)(delay_sec*1000);
   _aaxMutexLock(handle->thread.signal.mutex);
   while TEST_FOR_TRUE(handle->thread.started)
   {
      int err;

      _aaxMutexUnLock(handle->thread.signal.mutex);

      if (_IS_PLAYING(handle))
      {
         // TIMER_BASED
         if (be_handle->use_timer) {
            usecSleep(wait_us);
         }
				/* timeout is in ms */
         else
         {
#if 0
            int avail = psnd_pcm_avail_update(be_handle->pcm);
            if (avail < be_handle->period_frames)
#endif
            {
               err = psnd_pcm_wait(be_handle->pcm, 2*stdby_time);
               if (err < 0)
               {
                  xrun_recovery(be_handle->pcm, err);
                  _AAX_DRVLOG("snd_pcm_wait polling error");
              }
           }
         }
      }
      else {
         msecSleep(stdby_time);
      }

      if (be->state(be_handle, DRIVER_AVAILABLE) == false) {
         _SET_PROCESSED(handle);
      }

      _aaxMutexLock(handle->thread.signal.mutex);
      if TEST_FOR_FALSE(handle->thread.started) {
         break;
      }

      if (state != handle->state)
      {
         if (_IS_PAUSED(handle) ||
             (!_IS_PLAYING(handle) && _IS_STANDBY(handle))) {
            be->state(handle->backend.handle, DRIVER_PAUSE);
         }
         else if (_IS_PLAYING(handle) || _IS_STANDBY(handle)) {
            be->state(handle->backend.handle, DRIVER_RESUME);
         }
         state = handle->state;
      }

#if ENABLE_TIMING
       _aaxTimerStart(timer);
#endif
      if (_IS_PLAYING(handle))
      {
         int res = _aaxSoftwareMixerThreadUpdate(handle, dest_rb);

         if (be_handle->use_timer)
         {
            float target, input, err, P, I; //, D;
            float freq = mixer->info->frequency;

            target = be_handle->target[1];
            input = (float)res/freq;
            err = input - target;

            /* present error */
            P = err;

            /*  accumulation of past errors */
            be_handle->PID[0] += err*delay_sec;
            I = be_handle->PID[0];

            /* prediction of future errors, based on current rate of change */
//          D = (be_handle->PID.err - err)/delay_sec;
//          be_handle->PID.err = err;

            err = 1.85f*P + 0.9f*I;
//          wait_us = _MAX((delay_sec + err)*1000000.0f, 1.0f);
            wait_us = _MAX((delay_sec + err), 1e-6f) * 1000000.0f;

            be_handle->target[2] += delay_sec*1000.0f;	// ms
            if (res < be_handle->target[0]*freq)
            {
               be_handle->target[1] += 0.001f/be_handle->target[2];
               be_handle->target[2] = 0.0f;
#if 0
 printf("increase, target: %5.1f (%i), new: %5.2f, wait: %5.3f (%5.3f) ms\n", be_handle->target[0]*freq, res, be_handle->target[1]*freq, wait_us/1000.0f, delay_sec*1000.0f);
#endif
            }
            else if (be_handle->target[2] >= 10.0f*1000.0f)	// 10 sec
            {
               be_handle->target[1] *= 0.995f;
               be_handle->target[2] = 0.0f;
#if 0
 printf("reduce, target: %5.1f (%i), new: %5.2f, wait: %5.3f (%5.3f) ms\n", be_handle->target[0]*freq, res, be_handle->target[1]*freq, wait_us/1000.0f, delay_sec*1000.0f);
#endif
            }
         }
      }
#if ENABLE_TIMING
{
float elapsed = _aaxTimerElapsed(timer);
if (elapsed > delay_sec)
 printf("elapsed: %f ms\n", elapsed*1000.0f);
}
#endif

#if 0
 printf("state: %i, paused: %i\n", state, _IS_PAUSED(handle));
 printf("playing: %i, standby: %i\n", _IS_PLAYING(handle), _IS_STANDBY(handle));
#endif

      if (handle->batch_finished) { // batched mode
         _aaxSemaphoreRelease(handle->batch_finished);
      }
   }

#if ENABLE_TIMING
   _aaxTimerDestroy(timer);
#endif
   handle->ringbuffer = NULL;
   be->destroy_ringbuffer(dest_rb);
   _aaxMutexUnLock(handle->thread.signal.mutex);

   return handle ? true : false;
}
