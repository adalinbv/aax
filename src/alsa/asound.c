/*
 * Copyright 2005-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_ASSERT_H
#include <assert.h>
#endif
#include <stdio.h>
#include <errno.h>
#if HAVE_STRINGS_H
# include <strings.h>
#endif
#include <string.h>		/* strstr */
#include <stdarg.h>		/* va_start */

#include <xml.h>

#include <base/timer.h>		/* for msecSleep */
#include <base/dlsym.h>
#include <base/logging.h>
#include <base/threads.h>
#include <base/types.h>

#include <arch.h>
#include <api.h>
#include <driver.h>
#include <devices.h>
#include <ringbuffer.h>

#include <software/renderer.h>
#include "device.h"
#include "audio.h"

#define TIMER_BASED		AAX_FALSE
#define MAX_ID_STRLEN		64

#define DEFAULT_DEVNUM		0
#define DEFAULT_IFNUM		0
#define DEFAULT_OUTPUT_RATE	48000
#define DEFAULT_DEVNAME_OLD     "front:"AAX_MKSTR(DEFAULT_DEVNUM) \
                                       ","AAX_MKSTR(DEFAULT_IFNUM)
#define DEFAULT_DEVNAME		"default"
#define DEFAULT_HWDEVNAME	"hw:0"
#define DEFAULT_RENDERER	"ALSA"

#define ALSA_TIE_FUNCTION(a)	if ((TIE_FUNCTION(a)) == 0) printf("%s\n", #a)

static _aaxDriverDetect _aaxALSADriverDetect;
static _aaxDriverNewHandle _aaxALSADriverNewHandle;
static _aaxDriverGetDevices _aaxALSADriverGetDevices;
static _aaxDriverGetInterfaces _aaxALSADriverGetInterfaces;
static _aaxDriverConnect _aaxALSADriverConnect;
static _aaxDriverDisconnect _aaxALSADriverDisconnect;
static _aaxDriverSetup _aaxALSADriverSetup;
static _aaxDriverCallback _aaxALSADriverPlayback;
static _aaxDriverCaptureCallback _aaxALSADriverCapture;
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
   (_aaxDriverGetDevices *)&_aaxALSADriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxALSADriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxALSADriverGetName,
   (_aaxDriverRender *)&_aaxALSADriverRender,
   (_aaxDriverThread *)&_aaxALSADriverThread,

   (_aaxDriverConnect *)&_aaxALSADriverConnect,
   (_aaxDriverDisconnect *)&_aaxALSADriverDisconnect,
   (_aaxDriverSetup *)&_aaxALSADriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxALSADriverCapture,
   (_aaxDriverCallback *)&_aaxALSADriverPlayback,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)&_aaxALSADriverState,
   (_aaxDriverParam *)&_aaxALSADriverParam,
   (_aaxDriverLog *)&_aaxALSADriverLog
};

typedef struct
{
    char *name;
    char *devname;
    char *default_name[2];
    int default_devnum;
    int devnum;

    _aaxDriverCallback *play;
    _aaxRenderer *render;
    snd_mixer_t *mixer;
    snd_pcm_t *pcm;

    float volumeHW;
    float volumeCur, volumeInit;
    float volumeMin, volumeMax;
    float volumeStep, hwgain;
    char *outMixer;		/* mixer element name for output volume */

    float latency;
    float frequency_hz;

    float padding;		/* for sensor clock drift correction   */
    size_t threshold;		/* sensor buffer threshold for padding */

    unsigned int no_tracks;
    unsigned int no_periods;
    size_t period_frames;
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
DECL_FUNCTION(snd_pcm_hw_params_can_pause);
DECL_FUNCTION(snd_asoundlib_version);
DECL_FUNCTION(snd_config_update);
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
DECL_FUNCTION(snd_mixer_attach);
DECL_FUNCTION(snd_mixer_load);
DECL_FUNCTION(snd_mixer_selem_id_malloc);
DECL_FUNCTION(snd_mixer_selem_id_free);
DECL_FUNCTION(snd_mixer_selem_register);
DECL_FUNCTION(snd_mixer_first_elem);
DECL_FUNCTION(snd_mixer_elem_next);
DECL_FUNCTION(snd_mixer_selem_has_playback_volume);
DECL_FUNCTION(snd_mixer_selem_get_playback_dB);
DECL_FUNCTION(snd_mixer_selem_get_playback_volume_range);
DECL_FUNCTION(snd_mixer_selem_get_playback_dB_range);
DECL_FUNCTION(snd_mixer_selem_ask_playback_dB_vol);
DECL_FUNCTION(snd_mixer_selem_set_playback_volume_all);
DECL_FUNCTION(snd_mixer_selem_has_capture_volume);
DECL_FUNCTION(snd_mixer_selem_get_capture_dB);
DECL_FUNCTION(snd_mixer_selem_get_capture_volume_range);
DECL_FUNCTION(snd_mixer_selem_get_capture_dB_range);
DECL_FUNCTION(snd_mixer_selem_ask_capture_dB_vol);
DECL_FUNCTION(snd_mixer_selem_set_capture_volume_all);
DECL_FUNCTION(snd_mixer_selem_get_id);
DECL_FUNCTION(snd_mixer_selem_id_get_name);
#ifndef NDEBUG
DECL_FUNCTION(snd_pcm_dump);
DECL_FUNCTION(snd_output_stdio_attach);
#endif


typedef struct {
   char bits;
   snd_pcm_format_t format;
} _alsa_formats_t;

#ifndef NDEBUG
#define xrun_recovery(a,b)     _xrun_recovery_debug(a,b, __LINE__)
static int _xrun_recovery_debug(snd_pcm_t *, int, int);
#else
#define xrun_recovery(a,b)	_xrun_recovery(a,b)
static int _xrun_recovery(snd_pcm_t *, int);
#endif

static unsigned int get_devices_avail(int);
static int detect_devnum(_driver_t *, int);
static char *detect_devname(_driver_t *, int);
static char *_aaxALSADriverLogVar(const void *, const char *, ...);
static char *_aaxALSADriverGetDefaultInterface(const void *, int);

static int _alsa_pcm_open(_driver_t*, int);
static int _alsa_pcm_close(_driver_t*);
static int _alsa_set_access(const void *, snd_pcm_hw_params_t *, snd_pcm_sw_params_t *);
static void _alsa_error_handler(const char *, int, const char *, int, const char *,...);
static void _alsa_error_handler_none(const char *, int, const char *, int, const char *,...);
static int _alsa_get_volume_range(_driver_t*);
static float _alsa_set_volume(_driver_t*, _aaxRingBuffer*, ssize_t, snd_pcm_sframes_t, unsigned int, float);
static _aaxDriverCallback _aaxALSADriverPlayback_mmap_ni;
static _aaxDriverCallback _aaxALSADriverPlayback_mmap_il;
static _aaxDriverCallback _aaxALSADriverPlayback_rw_ni;
static _aaxDriverCallback _aaxALSADriverPlayback_rw_il;


#define MAX_FORMATS		6
#define FILL_FACTOR		1.65f
#define _AAX_DRVLOG(a)		_aaxALSADriverLog(id, __LINE__, 0, a)
#define STRCMP(a, b)		strncmp((a), (b), strlen(b))

/* forward declarations */
static const char* _alsa_type[2];
static const snd_pcm_stream_t _alsa_mode[2];
static const char *_const_alsa_default_name[2];
static const _alsa_formats_t _alsa_formats[MAX_FORMATS];
static const char* ifname_prefix[];

static int
_aaxALSADriverDetect(int mode)
{
   static void *audio = NULL;
   static int rv = AAX_FALSE;
   char *error = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   if TEST_FOR_FALSE(rv) {
      audio = _aaxIsLibraryPresent("asound", "2");
   }

   if (audio)
   {
      _aaxGetSymError(0);

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
         TIE_FUNCTION(snd_mixer_open);					//
         TIE_FUNCTION(snd_mixer_close);					//
         TIE_FUNCTION(snd_mixer_attach);				//
         TIE_FUNCTION(snd_mixer_load);					//
         TIE_FUNCTION(snd_mixer_selem_id_malloc);			//
         TIE_FUNCTION(snd_mixer_selem_id_free);				//
         TIE_FUNCTION(snd_mixer_selem_register);			//
         TIE_FUNCTION(snd_mixer_first_elem);				//
         TIE_FUNCTION(snd_mixer_elem_next);				//
         TIE_FUNCTION(snd_mixer_selem_has_playback_volume);		//
         TIE_FUNCTION(snd_mixer_selem_get_playback_dB);			//
         TIE_FUNCTION(snd_mixer_selem_get_playback_volume_range);	//
         TIE_FUNCTION(snd_mixer_selem_get_playback_dB_range);		//
         TIE_FUNCTION(snd_mixer_selem_ask_playback_dB_vol);		//
         TIE_FUNCTION(snd_mixer_selem_set_playback_volume_all);		//
         TIE_FUNCTION(snd_mixer_selem_has_capture_volume);		//
         TIE_FUNCTION(snd_mixer_selem_get_capture_dB);			//
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
         TIE_FUNCTION(snd_pcm_hw_params_can_mmap_sample_resolution);	//
         TIE_FUNCTION(snd_pcm_hw_params_get_rate_numden);		//
         TIE_FUNCTION(snd_pcm_state);					//
         TIE_FUNCTION(snd_pcm_start);					//
         TIE_FUNCTION(snd_pcm_delay);					//
         TIE_FUNCTION(snd_pcm_stream);					//
         TIE_FUNCTION(snd_strerror);					//
         TIE_FUNCTION(snd_config_update);				//
         TIE_FUNCTION(snd_lib_error_set_handler);			//
         TIE_FUNCTION(snd_asoundlib_version);				//
//       TIE_FUNCTION(snd_output_stdio_attach);
      }

      error = _aaxGetSymError(0);
      if (!error)
      {
         if (get_devices_avail(mode) != 0)
         {
            snprintf(_alsa_id_str, MAX_ID_STRLEN, "%s %s",
                             DEFAULT_RENDERER, psnd_asoundlib_version());

            psnd_lib_error_set_handler(_alsa_error_handler_none);
            psnd_config_update();
            rv = AAX_TRUE;
         }
      }
   }

   return rv;
}

static void *
_aaxALSADriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (handle)
   {
      int m = (mode > 0) ? 1 : 0;
      handle->default_name[m] = (char*)_const_alsa_default_name[m];
      handle->play = _aaxALSADriverPlayback_rw_il;
      handle->pause = 0;
      handle->use_mmap = 1;
      handle->interleaved = 0;
      handle->no_tracks = 2;

      handle->frequency_hz = 48000.0f; 
      handle->no_tracks = 2;
      handle->bits_sample = 16;
      handle->no_periods = (mode) ? PLAYBACK_PERIODS : CAPTURE_PERIODS;

      handle->mode = (mode > 0) ? 1 : 0;
      if (handle->mode != AAX_MODE_READ) { // Always interupt based for capture
         handle->use_timer = TIMER_BASED;
      }

      handle->target[0] = FILL_FACTOR;
      handle->target[1] = FILL_FACTOR;
      handle->target[2] = AAX_FPINFINITE;
   }

   return handle;
}

static void *
_aaxALSADriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;
   int rdr_aax_fmt;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      handle = _aaxALSADriverNewHandle(mode);
   }

   rdr_aax_fmt = (renderer && strstr(renderer, ": ")) ? 1 : 0;
   if (handle)
   {
      if (rdr_aax_fmt) {
         handle->name = _aax_strdup((char*)renderer);
      }
      else {
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
            handle->use_timer = AAX_FALSE;
         }

         if (xmlNodeTest(xid, "shared")) {
            handle->shared = xmlNodeGetBool(xid, "shared");
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
      int err, m;

      m = (handle->mode > 0) ? 1 : 0;
      handle->devnum = detect_devnum(handle, m);
      if (rdr_aax_fmt) {
         handle->devname = detect_devname(handle, m);
      } else {
         handle->devname = _aax_strdup(renderer ? renderer : "default");
      }
    
      err = _alsa_pcm_open(handle, m);
      if (err < 0)
      {
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

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   if (handle)
   {
      int m = (handle->mode > 0) ? 1 : 0;

      if (handle->default_name[m] != _const_alsa_default_name[m]) {
         free(handle->default_name[m]);
      }

      free(handle->ifname[0]);
      free(handle->ifname[1]);
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

      if (handle->mixer)
      {
         _alsa_set_volume(handle, NULL, 0, 0, 0, handle->volumeInit);
         psnd_mixer_close(handle->mixer);
      }
      _aax_free(handle->outMixer);
      handle->outMixer = NULL;

      if (handle->render) {
         handle->render->close(handle->render->id);
      }

      if (handle->pcm) {
         psnd_pcm_close(handle->pcm);
      }
      _aax_free(handle->ptr);
      handle->ptr = 0;
      handle->pcm = 0;
      free(handle);
      handle = 0;

      return AAX_TRUE;
   }

   return AAX_FALSE;
}


#ifndef NDEBUG
# define TRUN(f, s)	if (err >= 0) { err = f; if (err < 0) { _AAX_DRVLOG(s); printf("ALSA error: %s (%i) at line %i\n", s, err, __LINE__); } }
#else
# define TRUN(f, s)	if (err >= 0) { err = f; if (err < 0) _AAX_DRVLOG(s); }
#endif

static int
_aaxALSADriverSetup(const void *id, size_t *frames, int *fmt,
                        unsigned int *channels, float *speed, int *bitrate)
{
   _driver_t *handle = (_driver_t *)id;
   snd_pcm_hw_params_t *hwparams;
   snd_pcm_sw_params_t *swparams;
   unsigned int tracks, rate;
   int err, rv = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(handle != 0);

   rate = (unsigned int)*speed;
   tracks = *channels;
   if (tracks > handle->no_tracks) {
      tracks = handle->no_tracks;
   }
 
   if (handle->no_tracks != tracks)
   {
      int m = handle->mode;

      _alsa_pcm_close(handle);

      handle->no_tracks = tracks;
      handle->devnum = detect_devnum(handle, m);
      handle->devname = detect_devname(handle, m);

      err = _alsa_pcm_open(handle, m);
      if (err <= 0)
      {
         _AAX_DRVLOG("unsupported number of tracks");
         return AAX_FALSE;
      }
   }

   psnd_pcm_hw_params_malloc(&hwparams);
   psnd_pcm_sw_params_malloc(&swparams);
   if (hwparams && swparams)
   {
      unsigned int bits = handle->bits_sample;
      unsigned int periods = handle->no_periods;
      unsigned int val1, val2, period_fact;
      snd_pcm_uframes_t period_frames;
      snd_pcm_format_t data_format;
      snd_pcm_t *hid = handle->pcm;
      snd_pcm_sframes_t delay;
//    unsigned int bytes;

      err = psnd_pcm_hw_params_any(hid, hwparams);
      TRUN( psnd_pcm_hw_params_set_rate_resample(hid, hwparams, 0),
            "unable to disable sample rate conversion" );

#if 0
      if (handle->use_timer) {
         TRUN( psnd_pcm_hw_params_set_period_wakeup(hid, hwparams, 0),
            "unable to disable period wakeups" );
      }
#endif

      /* Set the prefered access method (rw/mmap interleaved/non-interleaved) */
      err = _alsa_set_access(handle, hwparams, swparams);

      /* supported sample formats*/
      if (err >= 0)
      {
         handle->can_pause = psnd_pcm_hw_params_can_pause(hwparams);

         TRUN( psnd_pcm_hw_params_get_rate_min(hwparams, &val1, 0),
               "unable to het the minimum sample reate" );
         TRUN( psnd_pcm_hw_params_get_rate_max(hwparams, &val2, 0),
               "unable to het the maximum sample reate" );
         if (err >= 0)
         {
            rate = _MINMAX(rate, val1, val2);
            handle->min_frequency = val1;
            handle->max_frequency = val2;
         }

         TRUN( psnd_pcm_hw_params_get_channels_min(hwparams, &val1),
               "unable to get the minimum no. tracks" );
         TRUN( psnd_pcm_hw_params_get_channels_max(hwparams, &val2),
               "unable to get the maximum no. tracks" );
         if (err >= 0)
         {
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
            periods = _MINMAX(periods, val1, val2);
            handle->min_periods = val1;
            handle->max_periods = val2;
         }

         /* recalculate period_frames and latency */
         period_frames = (*frames * rate)/(*speed * periods);
         handle->latency = (float)(period_frames*periods)/(float)rate;
      }

      /* test for supported sample formats*/
      if (err >= 0)
      {
         unsigned int pos = 0;
         do
         {
            data_format = _alsa_formats[pos].format;
            err = psnd_pcm_hw_params_set_format(hid, hwparams, data_format);
         }
         while ((err < 0) && (_alsa_formats[++pos].bits != 0));
         bits = _alsa_formats[pos].bits;

         if ((err >= 0) && (bits > 0))
         {
            switch (pos)
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
               _AAX_DRVLOG("error: hardware format mismatch!\n");
               err = -EINVAL;
               break;
            }
            handle->bits_sample = _alsa_formats[pos].bits;
         }
         else {
            _AAX_DRVLOG("unable to match hardware format");
         }
      }

      TRUN( psnd_pcm_hw_params_set_channels(hid, hwparams, tracks),
            "unsupported no. tracks" );
      if (tracks > handle->no_tracks) handle->no_tracks = tracks;

      TRUN( psnd_pcm_hw_params_set_rate_near(hid, hwparams, &rate, 0),
            "unsupported sample rate" );

      val1 = val2 = 0;
      err = psnd_pcm_hw_params_get_rate_numden(hwparams, &val1, &val2);
      if (val1 && val2)
      {
         handle->frequency_hz = (float)val1/(float)val2;
         rate = (unsigned int)handle->frequency_hz;
      }
      handle->frequency_hz = (float)rate;

      if (frames && (*frames > 0))
      {
         period_frames = (*frames * rate)/(*speed);
         if (handle->mode) period_frames /= 2;
      }
      else {
         period_frames = rate/25;
      }

      // Always use interupts for low latency.
      handle->latency = (float)period_frames/(float)rate;

      TRUN ( psnd_pcm_hw_params_get_periods_min(hwparams, &val1, 0),
             "unable to get the minimum no. periods" );

      // TIMER_BASED
      if (handle->use_timer) {
         periods = val1;
      }
      else
      {
         TRUN ( psnd_pcm_hw_params_get_periods_max(hwparams, &val2, 0),
                "unable to get the maximum no. periods" );
         periods = _MINMAX(periods, val1, val2);
      }

      TRUN( psnd_pcm_hw_params_set_periods_near(hid, hwparams, &periods, 0),
            "unsupported no. periods" );
      if (periods == 0) periods = 1;

      period_fact = handle->no_periods/periods;
      if (err >= 0) {
         handle->no_periods = periods;
      }
      handle->no_tracks = tracks;

      if (!handle->mode) {
         period_frames *= period_fact;
      }

      /* Set buffer size (in frames). The resulting latency is given by */
      /* latency = periodsize * periods / (rate * bytes_per_frame))     */
      period_frames *= periods;

      // TIMER_BASED
      if (handle->use_timer) {
         TRUN( psnd_pcm_hw_params_get_buffer_size_max(hwparams, &period_frames),
               "unable to fetch the mx., buffer size" );
      }
      handle->max_frames = period_frames;
      TRUN( psnd_pcm_hw_params_set_buffer_size_near(hid, hwparams, &period_frames),
            "invalid buffer size" );

      // TIMER_BASED
      if (handle->use_timer)
      {
         period_frames = *frames;
         handle->no_periods = periods = 2;
      }

      period_frames /= periods;
      if (!handle->mode) {
         period_frames = (period_frames/period_fact);
      }
      handle->period_frames = period_frames;

      *speed = rate;
      *channels = tracks;
      *frames = period_frames;

      handle->target[0] = ((float)period_frames/(float)rate);
      if (handle->target[0] > 0.02f) {
         handle->target[0] += 0.01f; // add 10ms
      } else {
         handle->target[0] *= FILL_FACTOR;
      }
      handle->target[1] = handle->target[0];

      if (handle->use_timer) {
         handle->latency = handle->target[0];
      }
      else {
          handle->latency = (float)(period_frames*periods)/(float)rate;
      }

      if (handle->latency < 0.010f) {
         handle->use_timer = AAX_FALSE;
      }

      TRUN( psnd_pcm_hw_params(hid, hwparams), "unable to configure hardware" );
      if (err == -EBUSY) {
         _AAX_DRVLOG("device busy\n");
      }

      TRUN( psnd_pcm_sw_params_current(hid, swparams), 
            "unable to set software config" );
      TRUN( psnd_pcm_sw_params_set_start_threshold(hid, swparams, 0x7fffffff),
            "improper interrupt treshold" );

      // TIMER_BASED
      if (handle->use_timer)
      {
         TRUN( psnd_pcm_sw_params_set_avail_min(hid, swparams,
                                                     handle->max_frames),
               "wakeup treshold unsupported" );
         TRUN( psnd_pcm_sw_params_set_stop_threshold(hid, swparams, -1),
               "set_stop_threshold unsupported" );
      } else {
         TRUN( psnd_pcm_sw_params_set_avail_min(hid, swparams,
                                                     handle->period_frames),
               "wakeup treshold unsupported" );
      }
      handle->threshold = 5*handle->period_frames/4;

      TRUN( psnd_pcm_sw_params(hid, swparams),
            "unable to configure software" );

      handle->render = _aaxSoftwareInitRenderer(handle->latency, handle->mode);
      if (handle->render)
      {
         const char *rstr = handle->render->info(handle->render->id);
         snprintf(_alsa_id_str, MAX_ID_STRLEN, "%s %s %s",
                       DEFAULT_RENDERER, psnd_asoundlib_version(), rstr);
      }

      TRUN( psnd_pcm_prepare(hid),
            "unable to prepare" );

      // Now fill the playback buffer with handle->no_periods periods of
      // silence for lowest latency.
      // TIMER_BASED
      if (handle->mode) // && !handle->use_timer)
      {
         _aaxRingBuffer *rb;
         int i;

         rb = _aaxRingBufferCreate(0.0f, handle->mode);
         if (rb)
         {
            rb->set_format(rb, AAX_PCM24S, AAX_TRUE);
            rb->set_parami(rb, RB_NO_TRACKS, handle->no_tracks);
            rb->set_paramf(rb, RB_FREQUENCY, handle->frequency_hz);
            rb->set_parami(rb, RB_NO_SAMPLES, handle->period_frames);
            rb->init(rb, AAX_TRUE);
            rb->set_state(rb, RB_STARTED);

            for (i=0; i<handle->no_periods; i++) {
               handle->play(handle, rb, 1.0f, 0.0f);
            }
            _aaxRingBufferFree(rb);
         }

         err = psnd_pcm_delay(hid, &delay);
         if (err >= 0) {
            handle->latency = (float)delay/(float)rate;
         }
         err = 0;

         rv = AAX_TRUE;
      }

      do
      {
         char str[255];

         _AAX_SYSLOG("alsa; driver settings:");

         if (handle->mode != 0) {
            snprintf(str,255,"  output renderer: '%s'", handle->name);
         } else {
            snprintf(str,255,"  input renderer: '%s'", handle->name);
         }
         _AAX_SYSLOG(str);
         snprintf(str,255, "  devname: '%s'", handle->devname);
         _AAX_SYSLOG(str);
         snprintf(str,255, "  playback rate: %u hz",  rate);
         _AAX_SYSLOG(str);
         snprintf(str,255, "  buffer size: %u bytes", (unsigned int)(*frames*tracks*bits)/8);
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
 if (handle->mode != 0) {
    printf("  output renderer: '%s'\n", handle->name);
 } else {
    printf("  input renderer: '%s'\n", handle->name);
 }
 printf( "  devname: '%s'\n", handle->devname);
 printf( "  playback rate: %u hz\n",  rate);
 printf( "  buffer size: %u bytes\n", (unsigned int)(*frames*tracks*bits)/8);
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

static size_t
_aaxALSADriverCapture(const void *id, void **data, ssize_t *offset, size_t *req_frames, void *scratch, size_t scratchlen, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int tracks, frame_size;
   snd_pcm_sframes_t avail;
   snd_pcm_state_t state;
   ssize_t offs = *offset;
   ssize_t init_offs = offs;
   size_t period_frames;
   size_t rv = 0;
   int res;

   if ((handle->mode != 0) || (req_frames == 0) || (data == 0))
   {
      if (handle->mode == 0) {
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
         _aaxALSADriverLogVar(id, "PCM avail error: %s\n", psnd_strerror(res));
         avail = -1;
       }
   }

   *req_frames = 0;
   if (period_frames && avail)
   {
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
      rv = AAX_TRUE;
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
                  _aaxALSADriverLogVar(id, "MMAP begin error: %s\n",
                                       psnd_strerror(res));
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
                  res = psnd_pcm_readi(handle->pcm, scratch, size);
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
                  res = psnd_pcm_readn(handle->pcm, s, size);
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
               rv = AAX_FALSE;
               break;
            }
            if (try++ > 2)
            {
               _AAX_DRVLOG("unable to recover from pcm read error");
               rv = AAX_FALSE;
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
      if (gain > 0)
      {
         unsigned int i;
         for (i=0; i<tracks; i++) {
            _batch_imul_value(sbuf[i]+init_offs, sizeof(int32_t), offs, gain);
         }
      }
   }
   else rv = AAX_TRUE;

   return rv;
}

static char *
_aaxALSADriverGetName(const void *id, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   if (handle)
   {
      if (handle->name) {
         ret = _aax_strdup(handle->name);
      } else if (handle->devname) {
         ret = _aax_strdup(handle->devname);
      }
   }
   else {
      ret = _aaxALSADriverGetDefaultInterface(id, mode);
   }

   return ret;
}

static int
_aaxALSADriverState(const void *id, enum _aaxDriverState state)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;

   switch(state)
   {
   case DRIVER_AVAILABLE:
      if (handle && psnd_pcm_state(handle->pcm) != SND_PCM_STATE_DISCONNECTED) {
         rv = AAX_TRUE;
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
      rv = AAX_TRUE;
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
         rv = handle->volumeHW;
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

		/* boolean */
      case DRIVER_SHARED_MODE:
      case DRIVER_TIMER_MODE:
         rv = (float)AAX_TRUE;
         break;
      default:
         break;
      }
   }
   return rv;
}

static char *
_aaxALSADriverGetDevices(const void *id, int mode)
{
   static char names[2][1024] = { "\0\0", "\0\0" };
   static time_t t_previous[2] = { 0, 0 };
   int m = (mode > 0) ? 1 : 0;
   time_t t_now;

   t_now = time(NULL);
   if (t_now > (t_previous[m]+5))
   {
      char *sysdefault = NULL;
      void **hints;
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
            if (!type || (type && !strcmp(type, _alsa_type[m])))
            {
               char *name = psnd_device_name_get_hint(*lst, "NAME");
               char *colon = strchr(name, ':');

               if (sysdefault && !STRCMP(name, "sysdefault:"))
               {
                  free(sysdefault);
                  sysdefault = NULL;
               }

               if (name && !(sysdefault && !STRCMP(colon, sysdefault)))
               {
                  if ((!colon &&
                          (strcmp(name, "null") && !strstr(name, "default")))
                      || (!STRCMP(name, "hw:") && strstr(name, ",DEV=0"))
                      || !STRCMP(name, "sysdefault:")
                     )
                  {
                     char *desc = psnd_device_name_get_hint(*lst, "DESC");
                     char *iface;
                     size_t slen;

                     if (!sysdefault && !STRCMP(name, "sysdefault:")) {
                        sysdefault = strdup(colon);
                     }
                     if (!desc) desc = name;

                     iface = strstr(desc, ", ");
                     if (iface) *iface = 0;

//                   if (!strchr(name, ':') && (!desc || strcmp(name, desc))) {
//                      snprintf(ptr, len, "%s: %s", name, desc);
//                   } else {
                        snprintf(ptr, len, "%s", desc);
//                   }
                     slen = strlen(ptr)+1;	/* skip the trailing 0 */
                     if (slen > (len-1)) break;

                     len -= slen;
                     ptr += slen;

                     if (desc != name) _sys_free(desc);
                  }
                  _sys_free(name);
               }
            }
            _sys_free(type);
            ++lst;
         }
         while (*lst != NULL);
         *ptr = 0;
         free(sysdefault);
      }

      res = psnd_device_name_free_hint(hints);

       /* always end with "\0\0" no matter what */
       names[m][1022] = 0;
       names[m][1023] = 0;
   }

   return (char *)&names[m];
}

static char *
_aaxALSADriverGetInterfaces(const void *id, const char *devname, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   int m = (mode > 0) ? 1 : 0;
   char *rv = handle->ifname[m];

   if (!rv)
   {
      char devlist[1024] = "\0\0";
      size_t len = 1024;
      void **hints;
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
            if (!type || (type && !strcmp(type, _alsa_type[m])))
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
                     char *desc = psnd_device_name_get_hint(*lst, "DESC");
                     char *iface;

                     if (!desc) desc = name;
                     iface = strstr(desc, ", ");

                     if (iface) *iface = 0;
                     if (iface && !strcasecmp(devname, desc))
                     {
                        size_t slen;

                        if (m || strcmp(name, "front:"))
                        {
                           if (iface != desc) {
                              iface = strchr(iface+2, '\n')+1;
                           }
                           snprintf(ptr, len, "%s", iface);
                        }
                        else
                        {
                           if (iface != desc) iface += 2;
                           snprintf(ptr, len, "%s", iface);
                           iface = strchr(ptr, '\n');
                           if (iface) *iface = 0;
                        }
                        slen = strlen(ptr)+1; /* skip the trailing 0 */
                        if (slen > (len-1)) break;

                        len -= slen;
                        ptr += slen;
                     }
                     if (desc != name) _sys_free(desc);
                  }
               }
               _sys_free(name);
            }
            _sys_free(type);
            ++lst;
         }
         while (*lst != NULL);

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
      res = psnd_device_name_free_hint(hints);
   }

   return rv;
}

static char *
_aaxALSADriverGetDefaultInterface(const void *id, int mode)
{
// _driver_t *handle = (_driver_t *)id;
   int m = (mode > 0) ? 1 : 0;
   char rv[1024]  = "default";
   size_t len = 1024;
   void **hints;
   int res;

   res = psnd_device_name_hint(-1, "pcm", &hints);
   if (!res && hints)
   {
      void **lst = hints;
      int found = 0;

      do
      {
         char *type = psnd_device_name_get_hint(*lst, "IOID");
         if (!type || (type && !strcmp(type, _alsa_type[m])))
         {
            char *name = psnd_device_name_get_hint(*lst, "NAME");
            if (name)
            {
               if ((!m && (!strcmp(name, "default:") ||
                          !strcmp(name, "sysdefault:")))
                    || !strcmp(name, "front:")
                  )
               {
                  char *desc = psnd_device_name_get_hint(*lst, "DESC");
                  char *iface;
 
                  if (!desc) desc = name;
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

                     if (!m)
                     {
                        iface = strchr(iface+2, '\n');
                        *iface = 0;
                     }
                     found = 1;
                  }
                  if (desc != name) _sys_free(desc);
               }
               _sys_free(name);
            }
         }
         _sys_free(type);
         ++lst;
       }
      while (!found && (*lst != NULL));
      res = psnd_device_name_free_hint(hints);
   }

   return _aax_strdup(rv);
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
_aaxALSADriverLog(const void *id, int prio, int type, const char *str)
{
   static char _errstr[256];

   snprintf(_errstr, 256, "asound: %s\n", str);
   _errstr[255] = '\0';  /* always null terminated */

   __aaxErrorSet(AAX_BACKEND_ERROR, (char*)&_errstr);
   _AAX_SYSLOG(_errstr);
#ifndef NDEBUG
   printf("%s", _errstr);
#endif

   return (char*)&_errstr;
}

/*-------------------------------------------------------------------------- */

static const char* ifname_prefix[] = {
   "front:", "rear:", "center_lfe:", "side:", "iec958:", NULL
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
   int err;

   err = psnd_pcm_open(&handle->pcm, handle->devname, _alsa_mode[m],
                       SND_PCM_NONBLOCK);
   if (err >= 0) {
      err = psnd_pcm_nonblock(handle->pcm, 1);
   }

   if (err >= 0)
   {
      err = psnd_mixer_open(&handle->mixer, 0);
      if (err >= 0)
      {
         char name[8];
         snprintf(name, 8, "hw:%i", handle->devnum);
         err = psnd_mixer_attach(handle->mixer, name);
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
         psnd_mixer_close(handle->mixer);
         handle->mixer = NULL;
      }
   }
   else
   {
      psnd_pcm_close(handle->pcm);
      handle->pcm = NULL;
   }

   return err;
}

static int
_alsa_pcm_close(_driver_t *handle)
{
   int err = 0;
   if (handle->mixer)
   {
      _alsa_set_volume(handle, NULL, 0, 0, 0, handle->volumeInit);
      psnd_mixer_close(handle->mixer);
   }
   if (handle->pcm) {
      err = psnd_pcm_close(handle->pcm);
   }
   return err;
}

static int
_alsa_set_access(const void *id, snd_pcm_hw_params_t *hwparams, snd_pcm_sw_params_t *swparams)
{
   _driver_t *handle = (_driver_t*)id;
   snd_pcm_t *hid = handle->pcm;
   int err = 0;

#if 0
   /* for testing purposes */
   if (err >= 0)
   {
      handle->use_mmap = 0;
      handle->interleaved = 1;
      handle->play = _aaxALSADriverPlayback_rw_il;
      err = psnd_pcm_hw_params_set_access(hid, hwparams,
                                          SND_PCM_ACCESS_RW_INTERLEAVED);
      if (err < 0) _AAX_DRVLOG("unable to set interleaved mode");
   } else
#endif
   if (err >= 0)                     /* playback */
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
_alsa_error_handler_none(const char *file, int line, const char *function,
                         int err, const char *fmt, ...)
{
}

static void
_alsa_error_handler(const char *file, int line, const char *function, int err,
                    const char *fmt, ...)
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
   unsigned int tracks = handle->no_tracks;
   const char *devname = handle->name;
   char vmix = handle->shared;
   char *rv = (char*)handle->default_name[m];

   if (devname && (tracks < _AAX_MAX_SPEAKERS))
   {
      unsigned int tracks_2 = tracks/2;
      void **hints;
      int res;

      res = psnd_device_name_hint(-1, "pcm", &hints);
      if (!res && hints)
      {
         void **lst = hints;
         char *ifname;

         ifname = strstr(devname, ": ");
         if (ifname)
         {
             *ifname = 0;
            ifname += 2;
         }

         do
         {
            char *type = psnd_device_name_get_hint(*lst, "IOID");
            if (!type || (type && !strcmp(type, _alsa_type[m])))
            {
               char *name = psnd_device_name_get_hint(*lst, "NAME");
               if (name)
               {
                  int i = 0;
                  while (ifname_prefix[i] && STRCMP(name, ifname_prefix[i])) {
                     i++;
                  }

                  if (!strcmp(devname, "pulse")) {
                     rv = strdup(devname);
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
                           if (!strcasecmp(ifname, iface) ||
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
                              break;
                           }
                        }
                        else	// no interface specified, use sysdefault
                        {
                           rv = strdup(name);
                           break;
                        }
                     }
                     if (desc != name) _sys_free(desc);
                  }
                  _sys_free(name);
               }
            }
            _sys_free(type);
            ++lst;
         }
         while (*lst != NULL);

         if (ifname)
         {
            ifname -= 2;
            *ifname = ':';
         }
      }

      res = psnd_device_name_free_hint(hints);
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
         void **lst = hints;
         size_t len;
         int ctr = 0;
         char *ptr;

         ptr = strstr(devname, ": ");
         if (ptr) len = ptr-devname;
         else len = strlen(devname);

         do
         {
            char *type = psnd_device_name_get_hint(*lst, "IOID");
            if (!type || (type && !strcmp(type, _alsa_type[m])))
            {
               char *name = psnd_device_name_get_hint(*lst, "NAME");
               if (name)
               {
                  if (!strcasecmp(devname, name))
                  {
                     _sys_free(name);
                     devnum = ctr;
                     break;
                  }

                  if (!STRCMP(name, "front:"))
                  {
                     if (!strcasecmp(devname, "default"))
                     {
                        _sys_free(name);
                        devnum = ctr;
                        break;
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
                           _sys_free(desc);
                           _sys_free(name);
                           devnum = ctr;
                           break;
                        }
                        ctr++;
                     }
                  }
                  _sys_free(name);
               }
            }
            _sys_free(type);
            ++lst;
         }
         while (*lst != NULL);
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
   void **hints;
   int res;

   if (rv[m] == 0)
   {
      res = psnd_device_name_hint(-1, "pcm", &hints);
      if (!res && hints)
      {
         void **lst = hints;

         do
         {
            char *type = psnd_device_name_get_hint(*lst, "IOID");
            if (!type || (type && !strcmp(type, _alsa_type[m])))
            {
               char *name = psnd_device_name_get_hint(*lst, "NAME");
               if (name && (!STRCMP(name,"front:") || strstr(name,"default:")))
               {
                  rv[m]++;
               }
            }
            _sys_free(type);
            ++lst;
         }
         while (*lst != NULL);

         res = psnd_device_name_free_hint(hints);
      }
   }

   return rv[m];
}

static int
_alsa_get_volume_range(_driver_t *handle)
{
   snd_mixer_selem_id_t *sid;
   int rv = 0;

   psnd_mixer_selem_id_malloc(&sid);
   if (handle->mode == AAX_MODE_READ)
   {
      snd_mixer_elem_t *elem;
      for (elem = psnd_mixer_first_elem(handle->mixer); elem;
           elem = psnd_mixer_elem_next(elem))
      {
         const char *name;

         psnd_mixer_selem_get_id(elem, sid);
         name = psnd_mixer_selem_id_get_name(sid);

         if (!strcasecmp(name, "Capture"))
         {
            if (psnd_mixer_selem_has_capture_volume(elem))
            {
               long min, max;

               psnd_mixer_selem_get_capture_dB_range(elem, &min, &max);
               handle->volumeMin = _db2lin((float)min*0.01f);
               handle->volumeMax = _db2lin((float)max*0.01f);

               rv = psnd_mixer_selem_get_capture_dB(elem,
                                                    SND_MIXER_SCHN_MONO, &max);
               handle->volumeInit = _db2lin((float)max*0.01f);
               handle->volumeCur = handle->volumeInit;
               handle->volumeHW = handle->volumeInit;

               psnd_mixer_selem_get_capture_volume_range(elem, &min, &max);
               handle->volumeStep = 0.5f/((float)max-(float)min);
            }
            break;
         }
      }
   }
   else
   {
      snd_mixer_elem_t *elem;
      for (elem = psnd_mixer_first_elem(handle->mixer); elem;
           elem = psnd_mixer_elem_next(elem))
      {
         const char *name;

         psnd_mixer_selem_get_id(elem, sid);
         name = psnd_mixer_selem_id_get_name(sid);

         if (!strcasecmp(name, "Front") || !strcasecmp(name, "Speaker"))
         {
            handle->outMixer = _aax_strdup(name);
            if (psnd_mixer_selem_has_playback_volume(elem))
            {
               long min, max;

               psnd_mixer_selem_get_playback_dB_range(elem, &min, &max);
               handle->volumeMin = _db2lin((float)min*0.01f);
               handle->volumeMax = _db2lin((float)max*0.01f);

               rv = psnd_mixer_selem_get_playback_dB(elem,
                                                     SND_MIXER_SCHN_MONO, &min);
               handle->volumeInit = _db2lin((float)min*0.01f);
               handle->volumeCur = handle->volumeInit;
               handle->volumeHW = handle->volumeInit;
               
               psnd_mixer_selem_get_playback_volume_range(elem, &min, &max);
               handle->volumeStep = 0.5f/((float)max-(float)min);
            }
            break;
         }
      }

      if (handle->volumeMax == 0.0f)
      {
         for (elem = psnd_mixer_first_elem(handle->mixer); elem;
              elem = psnd_mixer_elem_next(elem))
         {
            const char *name;

            psnd_mixer_selem_get_id(elem, sid);
            name = psnd_mixer_selem_id_get_name(sid);

            if (!strcasecmp(name, "PCM"))
            {
               handle->outMixer = _aax_strdup(name);
               if (psnd_mixer_selem_has_playback_volume(elem))
               {
                  long min, max;

                  psnd_mixer_selem_get_playback_dB_range(elem, &min, &max);
                  handle->volumeMin = _db2lin((float)min*0.01f);
                  handle->volumeMax = _db2lin((float)max*0.01f);

                  rv = psnd_mixer_selem_get_playback_dB(elem,
                                                     SND_MIXER_SCHN_MONO, &min);
                  handle->volumeInit = _db2lin((float)min*0.01f);
                  handle->volumeCur = handle->volumeInit;
                  handle->volumeHW = handle->volumeInit;
               
                  psnd_mixer_selem_get_playback_volume_range(elem, &min, &max);
                  handle->volumeStep = 0.5f/((float)max-(float)min);
               }
               break;
            }
         }
      }

      if (handle->volumeMax == 0.0f)
      {
         for (elem = psnd_mixer_first_elem(handle->mixer); elem;
              elem = psnd_mixer_elem_next(elem))
         {
            const char *name;

            psnd_mixer_selem_get_id(elem, sid);
            name = psnd_mixer_selem_id_get_name(sid);

            if (!strcasecmp(name, "Master"))
            {
               handle->outMixer = _aax_strdup(name);
               if (psnd_mixer_selem_has_playback_volume(elem))
               {
                  long min, max;

                  psnd_mixer_selem_get_playback_dB_range(elem, &min, &max);
                  handle->volumeMin = _db2lin((float)min*0.01f);
                  handle->volumeMax = _db2lin((float)max*0.01f);

                  rv = psnd_mixer_selem_get_playback_dB(elem,
                                                     SND_MIXER_SCHN_MONO, &min);
                  handle->volumeInit = _db2lin((float)min*0.01f);
                  handle->volumeCur = handle->volumeInit;
                  handle->volumeHW = handle->volumeInit;
               
                  psnd_mixer_selem_get_playback_volume_range(elem, &min, &max);
                  handle->volumeStep = 0.5f/((float)max-(float)min);
               }
               break;
            }
         }
      }

   }
   psnd_mixer_selem_id_free(sid);

   return rv;
}

static float
_alsa_set_volume(_driver_t *handle, _aaxRingBuffer *rb, ssize_t offset, snd_pcm_sframes_t no_frames, unsigned int no_tracks, float volume)
{
   float gain = fabsf(volume);
   float hwgain = gain;
   float rv = 0;

   if (handle && handle->mixer && !handle->shared && handle->volumeMax)
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

      if (1) // fabsf(hwgain - handle->volumeCur) >= handle->volumeStep)
      {
         snd_mixer_selem_id_t *sid;
         snd_mixer_elem_t *elem;

         psnd_mixer_selem_id_malloc(&sid);
         for (elem = psnd_mixer_first_elem(handle->mixer); elem;
              elem = psnd_mixer_elem_next(elem))
         {
            const char *name;
            long volume;

            psnd_mixer_selem_get_id(elem, sid);

            if ((handle->mode == AAX_MODE_READ) &&
                 psnd_mixer_selem_has_capture_volume(elem))
            {
               name = psnd_mixer_selem_id_get_name(sid);
               if (!strcasecmp(name, "Capture"))
               {
                  long volumeDB;
#if 0
                  psnd_mixer_selem_get_capture_dB(elem, SND_MIXER_SCHN_MONO,
                                                  &volumeDB);
                  handle->volumeHW = _db2lin(volumeDB*0.01f);
#endif
                  if (fabsf(hwgain - handle->volumeCur) >= handle->volumeStep)
                  {
                     int dir = 0;

                     volumeDB = (long)(_lin2db(hwgain)*100.0f);
                     if (volumeDB > 0) dir = -1;
                     else if (volumeDB < 0) dir = 1;
                     psnd_mixer_selem_ask_capture_dB_vol(elem, volumeDB, dir,
                                                         &volume);
                     psnd_mixer_selem_set_capture_volume_all(elem, volume);
                  }
                  break;
               }
            }
            else if ((handle->mode != AAX_MODE_READ) &&
                      psnd_mixer_selem_has_playback_volume(elem))
            {
               long volumeDB;

               name = psnd_mixer_selem_id_get_name(sid);
               if ((fabsf(hwgain - handle->volumeCur) >= handle->volumeStep) &&
                   (!strcasecmp(name, handle->outMixer) || !strcasecmp(name, "Center")
                    || !strcasecmp(name, "Surround") || !strcasecmp(name, "LFE")
                    || !strcasecmp(name, "Side")))
               {
                  int dir = 0;

                  volumeDB = (long)(_lin2db(hwgain)*100.0f);
                  if (volumeDB > 0) dir = -1;
                  else if (volumeDB < 0) dir = 1;
                  psnd_mixer_selem_ask_playback_dB_vol(elem, volumeDB, dir,
                                                       &volume);
                  psnd_mixer_selem_set_playback_volume_all(elem, volume);
               }
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
   if (rb && fabsf(gain - 1.0f) > 0.05f) {
      rb->data_multiply(rb, offset, no_frames, gain);
   }

   return rv;
}

static int
_xrun_recovery(snd_pcm_t *handle, int err)
{
   const void *id = NULL;
   int res = psnd_pcm_recover(handle, err, 1);
#if 0
   snd_output_t *output = NULL;

   psnd_output_stdio_attach(&output, stdout, 0);
   psnd_pcm_dump(handle, output);
#endif
   if (res != 0) {
      _AAX_DRVLOG("Unable to recover from xrun situation");
   }
   else if (res == -EPIPE)
   {
      if (psnd_pcm_stream(handle) == SND_PCM_STREAM_CAPTURE)
      {
         /* capturing requirs an explicit call to snd_pcm_start */
         res = psnd_pcm_start(handle);
         if (res != 0) {
            _AAX_DRVLOG("unable to restart input stream");
         }
      }
      else
      {
         res = psnd_pcm_prepare(handle);
         if (res != 0) {
            _AAX_DRVLOG("unable to restart output stream");
         }
      }
   }
   return res;
}

#ifndef NDEBUG
static int
_xrun_recovery_debug(snd_pcm_t *handle, int err, int line)
{
    printf("Alsa xrun error at line: %i\n", line);
    return _xrun_recovery(handle, err);
}
#endif


static size_t
_aaxALSADriverPlayback_mmap_ni(const void *id, void *src, float pitch, float gain)
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

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(handle != 0);
   if (handle->pause) return 0;

   assert(rbs != 0);

   offs = rbs->get_parami(rbs, RB_OFFSET_SAMPLES);
   no_tracks = rbs->get_parami(rbs, RB_NO_TRACKS);
   period_frames = rbs->get_parami(rbs, RB_NO_SAMPLES) - offs;

   res = avail = psnd_pcm_avail_update(handle->pcm);
   if (avail < 0)
   {
      int err;
      if ((err = xrun_recovery(handle->pcm, avail)) < 0)
      {
         char s[255];
         snprintf(s, 255, "PCM avail error: %s\n", psnd_strerror(err));
         _AAX_DRVLOG(s);
         return 0;
      }
   }

   state = psnd_pcm_state(handle->pcm);
   if ((state != SND_PCM_STATE_RUNNING) &&
       (avail > 0) && (avail <= handle->threshold))
   {
      psnd_pcm_start(handle->pcm);
   }

   if (avail < period_frames) avail = 0;
   else avail = period_frames;

   _alsa_set_volume(handle, rbs, offs, period_frames, no_tracks, gain);

   chunk = 10;
   sbuf = (const int32_t **)rbs->get_tracks_ptr(rbs, RB_READ);
   do
   {
      const snd_pcm_channel_area_t *area;
      snd_pcm_uframes_t frames = avail;
      snd_pcm_uframes_t mmap_offs;
      int err;

      err = psnd_pcm_mmap_begin(handle->pcm, &area, &mmap_offs, &frames);
      if (err < 0)
      {
         if ((err = xrun_recovery(handle->pcm, err)) < 0)
         {
            char s[255];
            snprintf(s, 255, "MMAP begin error: %s\n",psnd_strerror(err));
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
      if (res < 0 || (snd_pcm_uframes_t)res != frames)
      {
         if (xrun_recovery(handle->pcm, res >= 0 ? -EPIPE : res) < 0)
         {
            rv = 0;
            break;
         }
      }
      offs += res;
      avail -= res;
      rv += res;
   }
   while ((avail > 0) && --chunk);
   rbs->release_tracks_ptr(rbs);

   if (!chunk) _AAX_DRVLOG("too many playback tries\n");

   return rv;
}


static size_t
_aaxALSADriverPlayback_mmap_il(const void *id, void *src, float pitch, float gain)
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

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(handle != 0);
   if (handle->pause) return 0;

   assert(rbs != 0);

   offs = rbs->get_parami(rbs, RB_OFFSET_SAMPLES);
   period_frames = rbs->get_parami(rbs, RB_NO_SAMPLES) - offs;
   no_tracks = rbs->get_parami(rbs, RB_NO_TRACKS);

   res = avail = psnd_pcm_avail_update(handle->pcm);
   if (avail < 0)
   {
      int err;
      if ((err = xrun_recovery(handle->pcm, avail)) < 0)
      {
         char s[255];
         snprintf(s, 255, "PCM avail error: %s\n", psnd_strerror(err));
         _AAX_DRVLOG(s);
         return 0;
      }
   }

   state = psnd_pcm_state(handle->pcm);
   if ((state != SND_PCM_STATE_RUNNING) &&
       (avail > 0) && (avail <= handle->threshold))  
   {
      psnd_pcm_start(handle->pcm);
   }

   if (avail < period_frames) avail = 0;
   else avail = period_frames;

   _alsa_set_volume(handle, rbs, offs, period_frames, no_tracks, gain);

   chunk = 10;
   sbuf = (const int32_t **)rbs->get_tracks_ptr(rbs, RB_READ);
   do
   {
      const snd_pcm_channel_area_t *area;
      snd_pcm_uframes_t frames = avail;
      snd_pcm_uframes_t mmap_offs;
      char *p;
      int err;

      err = psnd_pcm_mmap_begin(handle->pcm, &area, &mmap_offs, &frames);
      if (err < 0)
      {
         if ((err = xrun_recovery(handle->pcm, err)) < 0)
         {
            char s[255];
            snprintf(s, 255, "MMAP begin error: %s\n",psnd_strerror(err));
            _AAX_DRVLOG(s);
            rv = 0;
            break;
         }
      }

      p = (char *)area->addr + ((area->first + area->step*mmap_offs) >> 3);
      handle->cvt_to_intl(p, sbuf, offs, no_tracks, frames);

      res = psnd_pcm_mmap_commit(handle->pcm, mmap_offs, frames);
      if (res < 0 || (snd_pcm_uframes_t)res != frames)
      {
         if (xrun_recovery(handle->pcm, res >= 0 ? -EPIPE : res) < 0)
         {
            rv = 0;
            break;
         }
      }
      offs += res;
      avail -= res;
      rv += res;
   }
   while ((avail > 0) && --chunk);
   rbs->release_tracks_ptr(rbs);

   if (!chunk) _AAX_DRVLOG("too many playback tries\n");

   return rv;
}


static size_t
_aaxALSADriverPlayback_rw_ni(const void *id, void *src, float pitch, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   _aaxRingBuffer *rbs = (_aaxRingBuffer *)src;
   char **data[_AAX_MAX_SPEAKERS];
   unsigned int t, no_tracks;
   unsigned int hw_bits, chunk;
   snd_pcm_sframes_t period_frames;
   snd_pcm_sframes_t avail;
   snd_pcm_state_t state;
   snd_pcm_sframes_t res;
   const int32_t **sbuf;
   ssize_t outbuf_size;
   size_t offs, rv = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(handle != 0);
   if (handle->pause) return 0;

   assert(rbs != 0);

   period_frames = rbs->get_parami(rbs, RB_NO_SAMPLES);
   no_tracks = rbs->get_parami(rbs, RB_NO_TRACKS);
   hw_bits = handle->bits_sample;

   outbuf_size = no_tracks * (period_frames*hw_bits)/8;
   if (handle->ptr == 0 || (handle->buf_len < outbuf_size))
   {
      size_t size;
      char *p;

      _aax_free(handle->ptr);
      handle->buf_len = outbuf_size;
      
      outbuf_size = SIZETO16((period_frames*hw_bits)/8);

      size = no_tracks * sizeof(void*);
      p = (char *)size;

      size += no_tracks * outbuf_size;
      handle->ptr = (void**)_aax_malloc(&p, size);
      handle->scratch = (char**)p;

      for (t=0; t<no_tracks; t++)
      {
         handle->ptr[t] = p;
         p += outbuf_size;
      }
   }

   offs = rbs->get_parami(rbs, RB_OFFSET_SAMPLES);
   period_frames -= offs;

   res = avail = psnd_pcm_avail_update(handle->pcm);
   if (avail < 0)
   {
      int err;
      if ((err = xrun_recovery(handle->pcm, avail)) < 0)
      {
         char s[255];
         snprintf(s, 255, "PCM avail error: %s\n", psnd_strerror(err));
         _AAX_DRVLOG(s);
         return 0;
      }
   }

   state = psnd_pcm_state(handle->pcm);
   if ((state != SND_PCM_STATE_RUNNING) &&
       (avail > 0) && (avail <= handle->threshold))
   {
      psnd_pcm_start(handle->pcm);
   }

   if (avail < period_frames) avail = 0;
   else avail = period_frames;

   _alsa_set_volume(handle, rbs, offs, period_frames, no_tracks, gain);

   sbuf = (const int32_t**)rbs->get_tracks_ptr(rbs, RB_READ);
   for (t=0; t<no_tracks; t++)
   {
      data[t] = handle->ptr[t];
      handle->cvt_to(data[t], sbuf[t]+offs, period_frames);
   }
   rbs->release_tracks_ptr(rbs);

   chunk = 10;
   do
   {
      int try = 0;

      do {
         res = psnd_pcm_writen(handle->pcm, (void**)data, period_frames);
      }
      while (res == -EAGAIN);

      if (res < 0)
      {
         if (xrun_recovery(handle->pcm, res) < 0)
         {
            _AAX_DRVLOG("unable to run xrun_recovery");
            rv = 0;
            break;
         }
         if (try++ > 2)
         {
            _AAX_DRVLOG("unable to recover from pcm write error");
            rv = 0;
            break;
         }
         continue;
      }

      for (t=0; t<no_tracks; t++) {
         data[t] += (res*hw_bits)/8;
      }
      offs += res;
      avail -= res;
      rv += res;
   }
   while ((avail > 0) && --chunk);
   if (!chunk) _AAX_DRVLOG("too many playback tries\n");

   return rv;
}


static size_t
_aaxALSADriverPlayback_rw_il(const void *id, void *src, float pitch, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   _aaxRingBuffer *rbs = (_aaxRingBuffer *)src;
   unsigned int no_tracks, chunk, hw_bits;
   snd_pcm_sframes_t period_frames;
   snd_pcm_sframes_t avail;
   snd_pcm_state_t state;
   snd_pcm_sframes_t res;
   const int32_t **sbuf;
   size_t outbuf_size;
   size_t offs, rv = 0;
   char *data;
    
   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(handle != 0);
   if (handle->pause) return 0;

   assert(rbs != 0);

   period_frames = rbs->get_parami(rbs, RB_NO_SAMPLES);
   no_tracks = rbs->get_parami(rbs, RB_NO_TRACKS);
   hw_bits = handle->bits_sample;

   outbuf_size = (no_tracks * period_frames*hw_bits)/8;
   if (handle->ptr == 0 || (handle->buf_len < outbuf_size))
   {
      char *p = 0;

      _aax_free(handle->ptr);
      handle->buf_len = outbuf_size;

      handle->ptr = (void**)_aax_malloc(&p, outbuf_size);
      handle->scratch = (char**)p;
   }

   offs = rbs->get_parami(rbs, RB_OFFSET_SAMPLES);
   period_frames -= offs;

   res = avail = psnd_pcm_avail_update(handle->pcm);
   if (avail < 0)
   {
      int err;
      if ((err = xrun_recovery(handle->pcm, avail)) < 0)
      {
         char s[255];
         snprintf(s, 255, "PCM avail error: %s\n", psnd_strerror(err));
         _AAX_DRVLOG(s);
         return 0;
      }
   }

   state = psnd_pcm_state(handle->pcm);
   if ((state != SND_PCM_STATE_RUNNING) &&
       (avail > 0) && (avail <= handle->threshold))
   {
      psnd_pcm_start(handle->pcm);
   }

   if (avail < period_frames) avail = 0;
   else avail = period_frames;

   _alsa_set_volume(handle, rbs, offs, period_frames, no_tracks, gain);

   data = (char*)handle->scratch;
   sbuf = (const int32_t**)rbs->get_tracks_ptr(rbs, RB_READ);
   handle->cvt_to_intl(data, sbuf, offs, no_tracks, period_frames);
   rbs->release_tracks_ptr(rbs);

   chunk = 10;
   do
   {
      int try = 0;

      do {
         res = psnd_pcm_writei(handle->pcm, data, period_frames);
      }
      while (res == -EAGAIN);

      if (res < 0)
      {
         if (xrun_recovery(handle->pcm, res) < 0)
         {
            _AAX_DRVLOG("unable to run xrun_recovery");
            rv = 0;
            break;
         }
         if (try++ > 2) 
         {
            _AAX_DRVLOG("unable to recover from pcm write error");
            rv = 0;
            break;
         }
//       _AAX_DRVLOG("warning: pcm write error");
         continue;
      }

      data += (res * no_tracks*hw_bits)/8;
      offs += res;
      avail -= res;
      rv += res;
   }
   while ((avail > 0) && --chunk);
   if (!chunk) _AAX_DRVLOG("too many playback tries\n");

   return rv;
}

static size_t
_aaxALSADriverPlayback(const void *id, void *src, float pitch, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   snd_pcm_sframes_t avail;
   size_t res;

   res = handle->play(id, src, pitch, gain);
   if (psnd_pcm_state(handle->pcm) == SND_PCM_STATE_PREPARED) {
      psnd_pcm_start(handle->pcm);
   }

   // return the current buffer fill level
// avail = psnd_pcm_avail(handle->pcm);
   psnd_pcm_delay(handle->pcm, &avail);
   if (avail < 0)
   {
      xrun_recovery(handle->pcm, avail);
      avail = _MAX(psnd_pcm_avail(handle->pcm), 0);
   }
   res = avail; // handle->max_frames - avail;

   return res;
}

_aaxRenderer*
_aaxALSADriverRender(const void* config)
{
   _driver_t *handle = (_driver_t *)config;
   return handle->render;
}

void *
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
   const void *id;
   int stdby_time;
   char state;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return NULL;
   }

   delay_sec = 1.0f/handle->info->refresh_rate;

   be = handle->backend.ptr;
   id = handle->backend.handle;		// Required for _AAX_DRVLOG

   dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      _sensor_t* sensor = _intBufGetDataPtr(dptr_sensor);

      mixer = sensor->mixer;

      dest_rb = be->get_ringbuffer(REVERB_EFFECTS_TIME, mixer->info->mode);
      if (dest_rb)
      {
         dest_rb->set_format(dest_rb, AAX_PCM24S, AAX_TRUE);
         dest_rb->set_parami(dest_rb, RB_NO_TRACKS, mixer->info->no_tracks);
         dest_rb->set_paramf(dest_rb, RB_FREQUENCY, mixer->info->frequency);
         dest_rb->set_paramf(dest_rb, RB_DURATION_SEC, delay_sec);
         dest_rb->init(dest_rb, AAX_TRUE);
         dest_rb->set_state(dest_rb, RB_STARTED);

         handle->ringbuffer = dest_rb;
      }
      _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

      if (!dest_rb) {
         return NULL;
      }
   }
   else {
      return NULL;
   }

   be->state(handle->backend.handle, DRIVER_PAUSE);
   state = AAX_SUSPENDED;

   wait_us = delay_sec*1000000.0f;
// no_samples = dest_rb->get_parami(dest_rb, RB_NO_SAMPLES);
   stdby_time = (int)(delay_sec*1000);
   _aaxMutexLock(handle->thread.signal.mutex);
   while TEST_FOR_TRUE(handle->thread.started)
   {
      _driver_t *be_handle = (_driver_t *)handle->backend.handle;
      int err;

      _aaxMutexUnLock(handle->thread.signal.mutex);

      if (_IS_PLAYING(handle))
      {
         // TIMER_BASED
         if (be_handle->use_timer) {
            usecSleep(wait_us);
         }
				/* timeout is in ms */
         else if ((err = psnd_pcm_wait(be_handle->pcm, 2*stdby_time)) < 0)
         {
            xrun_recovery(be_handle->pcm, err);
            _AAX_DRVLOG("snd_pcm_wait polling error");
         }
      }
      else {
         msecSleep(stdby_time);
      }

      if (be->state(be_handle, DRIVER_AVAILABLE) == AAX_FALSE) {
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
            float diff, target, input, err, P, I;
            float freq = mixer->info->frequency;

            target = be_handle->target[1];
            input = (float)res/freq;
            err = input - target;

            P = err;
            I = err*delay_sec;

            be_handle->PID[0] += I;
            I = be_handle->PID[0];

            diff = 1.85f*P + 0.9f*I;
            wait_us = _MAX((delay_sec + diff)*1000000.0f, 1.0f);

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
   }

#if ENABLE_TIMING
   _aaxTimerDestroy(timer);
#endif
   handle->ringbuffer = NULL;
   be->destroy_ringbuffer(dest_rb);
   _aaxMutexUnLock(handle->thread.signal.mutex);

   return handle;
}
