/*
 * Copyright 2005-2012 by Erik Hofman.
 * Copyright 2009-2012 by Adalin B.V.
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
#if HAVE_TIME_H
#include <time.h>
#endif
#include <stdio.h>
#include <errno.h>
#include <strings.h>
#include <stdlib.h>	/* nanosleep */
#define _GNU_SOURCE
#ifndef NDEBUG
#include <stdarg.h>	/* va_start */
#endif

#include <xml.h>

#include <api.h>
#include <arch.h>
#include <devices.h>
#include <ringbuffer.h>
#include <base/types.h>
#include <base/dlsym.h>
#include <base/logging.h>
#include <base/threads.h>

#include "alsa/audio.h"
#include "device.h"

#define MAX_ID_STRLEN		32

#define DEFAULT_DEVNUM		0
#define DEFAULT_OUTPUT_RATE	48000
#define DEFAULT_DEVNAME		"hw:"AAX_MKSTR(DEFAULT_DEVNUM)
#define DEFAULT_RENDERER	"ALSA"

static _aaxDriverDetect _aaxALSASoftDriverDetect;
static _aaxDriverGetDevices _aaxALSASoftDriverGetDevices;
static _aaxDriverGetInterfaces _aaxALSASoftDriverGetInterfaces;
static _aaxDriverConnect _aaxALSASoftDriverConnect;
static _aaxDriverDisconnect _aaxALSASoftDriverDisconnect;
static _aaxDriverSetup _aaxALSASoftDriverSetup;
static _aaxDriverCallback _aaxALSASoftDriverPlayback_mmap_ni;
static _aaxDriverCallback _aaxALSASoftDriverPlayback_mmap_il;
static _aaxDriverCallback _aaxALSASoftDriverPlayback_rw_ni;
static _aaxDriverCallback _aaxALSASoftDriverPlayback_rw_il;
static _aaxDriverState _aaxALSASoftDriverPause;
static _aaxDriverState _aaxALSASoftDriverResume;
static _aaxDriverRecordCallback _aaxALSASoftDriverRecord;
static _aaxDriverGetName _aaxALSASoftDriverGetName;
static _aaxDriverThread _aaxALSASoftDriverThread;
static _aaxDriverState _aaxALSASoftDriverIsAvailable;
static _aaxDriverState _aaxALSASoftDriverAvailable;

static const snd_pcm_stream_t __mode[2] = {
       SND_PCM_STREAM_CAPTURE, SND_PCM_STREAM_PLAYBACK
   };
static char _alsa_id_str[MAX_ID_STRLEN+1] = DEFAULT_RENDERER;
_aaxDriverBackend _aaxALSASoftDriverBackend =
{
   1.0,
   AAX_PCM16S,
   DEFAULT_OUTPUT_RATE,
   2,

   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_alsa_id_str,

   (_aaxCodec **)&_oalRingBufferCodecs,

   (_aaxDriverDetect *)&_aaxALSASoftDriverDetect,
   (_aaxDriverGetDevices *)&_aaxALSASoftDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxALSASoftDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxALSASoftDriverGetName,
   (_aaxDriverThread *)&_aaxALSASoftDriverThread,

   (_aaxDriverConnect *)&_aaxALSASoftDriverConnect,
   (_aaxDriverDisconnect *)&_aaxALSASoftDriverDisconnect,
   (_aaxDriverSetup *)&_aaxALSASoftDriverSetup,
   (_aaxDriverState *)&_aaxALSASoftDriverPause,
   (_aaxDriverState *)&_aaxALSASoftDriverResume,
   (_aaxDriverRecordCallback *)&_aaxALSASoftDriverRecord,
   (_aaxDriverCallback *)&_aaxALSASoftDriverPlayback_mmap_ni,

   (_aaxDriver2dMixerCB *)&_aaxSoftwareDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxSoftwareDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)&_aaxALSASoftDriverAvailable,
   (_aaxDriverState *)&_aaxALSASoftDriverAvailable,
   (_aaxDriverState *)&_aaxALSASoftDriverIsAvailable
};

typedef struct
{
    char *name;
    int devnum;

    snd_pcm_t *id;

    float latency;
    float frequency_hz;
    unsigned int no_channels;
    unsigned int no_periods;

    int mode;
    char pause;
    char can_pause;
    char use_mmap;
    char bytes_sample;
    char interleaved;
    char hw_channels;
    char playing;
    char sse_level;

#ifndef NDEBUG
   unsigned int buf_len;
#endif

    void **scratch;
    int16_t **data;

} _driver_t;


DECL_FUNCTION(snd_pcm_open);
DECL_FUNCTION(snd_pcm_close);
DECL_FUNCTION(snd_pcm_wait);
DECL_FUNCTION(snd_pcm_nonblock);
DECL_FUNCTION(snd_pcm_prepare);
DECL_FUNCTION(snd_pcm_resume);
DECL_FUNCTION(snd_pcm_drain);
DECL_FUNCTION(snd_pcm_drop);
DECL_FUNCTION(snd_pcm_pause);
DECL_FUNCTION(snd_pcm_hw_params_can_pause);
DECL_FUNCTION(snd_pcm_hw_params_can_resume);
DECL_FUNCTION(snd_pcm_info_malloc);
DECL_FUNCTION(snd_pcm_info_sizeof);
DECL_FUNCTION(snd_asoundlib_version);
DECL_FUNCTION(snd_pcm_info_get_name);
DECL_FUNCTION(snd_pcm_info_get_subdevices_count);
DECL_FUNCTION(snd_pcm_info_set_subdevice);
DECL_FUNCTION(snd_pcm_info_set_device);
DECL_FUNCTION(snd_pcm_info_set_stream);
DECL_FUNCTION(snd_pcm_info_free);
DECL_FUNCTION(snd_card_next);
DECL_FUNCTION(snd_card_get_name);
DECL_FUNCTION(snd_ctl_open);
DECL_FUNCTION(snd_ctl_close);
DECL_FUNCTION(snd_ctl_pcm_info);
DECL_FUNCTION(snd_ctl_pcm_next_device);
DECL_FUNCTION(snd_pcm_hw_params_sizeof);
DECL_FUNCTION(snd_pcm_hw_params);
DECL_FUNCTION(snd_pcm_hw_params_any);
DECL_FUNCTION(snd_pcm_hw_params_set_access);
DECL_FUNCTION(snd_pcm_hw_params_set_format);
DECL_FUNCTION(snd_pcm_hw_params_set_rate_resample);
DECL_FUNCTION(snd_pcm_hw_params_set_rate_near);
DECL_FUNCTION(snd_pcm_hw_params_test_channels);
DECL_FUNCTION(snd_pcm_hw_params_set_channels);
DECL_FUNCTION(snd_pcm_hw_params_set_buffer_size_near);
DECL_FUNCTION(snd_pcm_hw_params_set_periods_near);
DECL_FUNCTION(snd_pcm_sw_params_sizeof);
DECL_FUNCTION(snd_pcm_sw_params_current);
DECL_FUNCTION(snd_pcm_sw_params);
DECL_FUNCTION(snd_pcm_sw_params_set_avail_min);
DECL_FUNCTION(snd_pcm_sw_params_set_start_threshold);
DECL_FUNCTION(snd_pcm_mmap_begin);
DECL_FUNCTION(snd_pcm_mmap_commit);
DECL_FUNCTION(snd_pcm_writen);
DECL_FUNCTION(snd_pcm_writei);
DECL_FUNCTION(snd_pcm_readi);
DECL_FUNCTION(snd_pcm_format_width);
DECL_FUNCTION(snd_pcm_hw_params_can_mmap_sample_resolution);
DECL_FUNCTION(snd_pcm_hw_params_get_rate_numden);
DECL_FUNCTION(snd_pcm_hw_params_get_buffer_size);
DECL_FUNCTION(snd_pcm_hw_params_get_periods);
DECL_FUNCTION(snd_pcm_hw_params_get_period_size);
DECL_FUNCTION(snd_pcm_avail_update);
DECL_FUNCTION(snd_pcm_avail);
DECL_FUNCTION(snd_pcm_state);
DECL_FUNCTION(snd_pcm_start);
DECL_FUNCTION(snd_pcm_delay);
DECL_FUNCTION(snd_strerror);
DECL_FUNCTION(snd_lib_error_set_handler);

static const snd_pcm_format_t _alsa_formats[];
static const char *_const_default_name = DEFAULT_DEVNAME;

static char *_default_name = NULL;
static int _default_devnum = DEFAULT_DEVNUM;

static int xrun_recovery(snd_pcm_t *, int);

static unsigned int get_devices_avail(int);
static char *detect_devname(const char*, int, unsigned int, int);
static int detect_devnum(const char *, int);
static int detect_ifnum(char *, const char *, int);

static void _alsa_error_handler(const char *, int, const char *, int, const char *,...);

static int
_aaxALSASoftDriverDetect()
{
   static int rv = AAX_FALSE;
   void *audio = NULL;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   if (_default_name == NULL) {
      _default_name = (char*)_const_default_name;
   }

   if TEST_FOR_FALSE(rv) {
      audio = _oalIsLibraryPresent("libasound.so.2");
   }

   if (audio)
   {
      const char *hwstr = _aaxGetSIMDSupportString();
      char *error;

      snprintf(_alsa_id_str, MAX_ID_STRLEN, "%s %s",
                                  DEFAULT_RENDERER, hwstr);

      _oalGetSymError(0);

      TIE_FUNCTION(snd_pcm_open);
      if (psnd_pcm_open)
      {
         TIE_FUNCTION(snd_pcm_close);
         TIE_FUNCTION(snd_pcm_wait);
         TIE_FUNCTION(snd_pcm_nonblock);
         TIE_FUNCTION(snd_pcm_prepare);
         TIE_FUNCTION(snd_pcm_resume);
         TIE_FUNCTION(snd_pcm_drain);
         TIE_FUNCTION(snd_pcm_drop);
         TIE_FUNCTION(snd_pcm_pause);
         TIE_FUNCTION(snd_pcm_hw_params_can_pause);
         TIE_FUNCTION(snd_pcm_hw_params_can_resume);
         TIE_FUNCTION(snd_pcm_info_malloc);
         TIE_FUNCTION(snd_pcm_info_sizeof);
         TIE_FUNCTION(snd_asoundlib_version);
         TIE_FUNCTION(snd_pcm_info_get_name);
         TIE_FUNCTION(snd_pcm_info_get_subdevices_count);
         TIE_FUNCTION(snd_pcm_info_set_subdevice);
         TIE_FUNCTION(snd_pcm_info_set_device);
         TIE_FUNCTION(snd_pcm_info_set_stream);
         TIE_FUNCTION(snd_pcm_info_free);
         TIE_FUNCTION(snd_card_next);
         TIE_FUNCTION(snd_card_get_name);
         TIE_FUNCTION(snd_ctl_open);
         TIE_FUNCTION(snd_ctl_close);
         TIE_FUNCTION(snd_ctl_pcm_info);
         TIE_FUNCTION(snd_ctl_pcm_next_device);
         TIE_FUNCTION(snd_pcm_hw_params_sizeof);
         TIE_FUNCTION(snd_pcm_hw_params);
         TIE_FUNCTION(snd_pcm_hw_params_any);
         TIE_FUNCTION(snd_pcm_hw_params_set_access);
         TIE_FUNCTION(snd_pcm_hw_params_set_format);
         TIE_FUNCTION(snd_pcm_hw_params_set_rate_resample);
         TIE_FUNCTION(snd_pcm_hw_params_set_rate_near);
         TIE_FUNCTION(snd_pcm_hw_params_test_channels);
         TIE_FUNCTION(snd_pcm_hw_params_set_channels);
         TIE_FUNCTION(snd_pcm_hw_params_set_periods_near);
         TIE_FUNCTION(snd_pcm_hw_params_set_buffer_size_near);
         TIE_FUNCTION(snd_pcm_sw_params_sizeof);
         TIE_FUNCTION(snd_pcm_sw_params_current);
         TIE_FUNCTION(snd_pcm_sw_params);
         TIE_FUNCTION(snd_pcm_sw_params_set_avail_min);
         TIE_FUNCTION(snd_pcm_sw_params_set_start_threshold);
         TIE_FUNCTION(snd_pcm_mmap_begin);
         TIE_FUNCTION(snd_pcm_mmap_commit);
         TIE_FUNCTION(snd_pcm_writen);
         TIE_FUNCTION(snd_pcm_writei);
         TIE_FUNCTION(snd_pcm_readi);
         TIE_FUNCTION(snd_pcm_format_width);
         TIE_FUNCTION(snd_pcm_hw_params_can_mmap_sample_resolution);
         TIE_FUNCTION(snd_pcm_hw_params_get_rate_numden);
         TIE_FUNCTION(snd_pcm_hw_params_get_buffer_size);
         TIE_FUNCTION(snd_pcm_hw_params_get_periods);
         TIE_FUNCTION(snd_pcm_hw_params_get_period_size);
         TIE_FUNCTION(snd_pcm_avail_update);
         TIE_FUNCTION(snd_pcm_avail);
         TIE_FUNCTION(snd_pcm_state);
         TIE_FUNCTION(snd_pcm_start);
         TIE_FUNCTION(snd_pcm_delay);
         TIE_FUNCTION(snd_strerror);
         TIE_FUNCTION(snd_lib_error_set_handler);
      }

      error = _oalGetSymError(0);
      if (!error)
      {
         if (get_devices_avail(1) != 0) {
            rv = AAX_TRUE;
         }
      }
   }

   return rv;
}

static void *
_aaxALSASoftDriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle)
   {
      const char *hwstr = _aaxGetSIMDSupportString();

      handle = (_driver_t *)calloc(1, sizeof(_driver_t));
      if (!handle) return 0;

      snprintf(_alsa_id_str, MAX_ID_STRLEN, "%s %s %s",
                             DEFAULT_RENDERER, psnd_asoundlib_version(), hwstr);

      handle->sse_level = _aaxGetSSELevel();
      handle->name = (char*)renderer;
      handle->pause = 0;
      handle->use_mmap = 1;
      handle->interleaved = 0;
      handle->hw_channels = 2;

      handle->frequency_hz = _aaxALSASoftDriverBackend.rate;
      handle->no_channels = _aaxALSASoftDriverBackend.tracks;
      handle->bytes_sample = 2;
      handle->no_periods = 2;

      if (xid)
      {
         char *s;
         int i;

         if (!handle->name)
         {
            s = xmlNodeGetString(xid, "renderer");
            if (s && strcmp(s, "default")) handle->name = s;
            else
            {
               free(s); /* 'default' */
               handle->name = (char *)_default_name;
            }
         }

         i = xmlNodeGetDouble(xid, "frequency-hz");
         if (i)
         {
            if (i < _AAX_MIN_MIXER_FREQUENCY)
            {
               _AAX_SYSLOG("alsa; frequency too small.");
               i = _AAX_MIN_MIXER_FREQUENCY;
            }
            else if (i > _AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_SYSLOG("alsa; frequency too large.");
               i = _AAX_MAX_MIXER_FREQUENCY;
            }
            handle->frequency_hz = i;
         }

         if (mode != AAX_MODE_READ && mode != AAX_MODE_WRITE_HRTF)
         {
            i = xmlNodeGetInt(xid, "channels");
            if (i)
            {
               if (i < 1)
               {
                  _AAX_SYSLOG("alsa; no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_SYSLOG("alsa; no. tracks too great.");
                  i = _AAX_MAX_SPEAKERS;
               }
               handle->no_channels = i;
            }
         }

         i = xmlNodeGetInt(xid, "bits-per-sample");
         if (i)
         {
            if (i != 16)
            {
               _AAX_SYSLOG("alsa; unsupported bits-per-sample");
               i = 16;
            }
            handle->bytes_sample = i/8;
         }

         i = xmlNodeGetInt(xid, "periods");
         if (i)
         {
            if (i < 1)
            {
               _AAX_SYSLOG("alsa; no periods too small.");
               i = 1;
            }
            else if (i > 16)
            {
               _AAX_SYSLOG("alsa; no. tracks too great.");
               i = 16;
            }
            handle->no_periods = i;
         }
      }
   }
#if 0
 printf("\nrenderer: %s\n", handle->name);
 printf("frequency-hz: %f\n", handle->frequency_hz);
 printf("channels: %i\n", handle->no_channels);
 printf("bytes-per-sample: %i\n", handle->bytes_sample);
 printf("periods: %i\n", handle->no_periods);
 printf("\n");
#endif

   if (handle)
   {
      int err, m;

      psnd_lib_error_set_handler(_alsa_error_handler);

      m = (mode > 0) ? 1 : 0;
      handle->mode = m;
      handle->devnum = detect_devnum(handle->name, m);

      handle->name = detect_devname(handle->name, handle->devnum, handle->no_channels, m);
      err = psnd_pcm_open(&handle->id, handle->name, __mode[m], SND_PCM_NONBLOCK);
      if (err >= 0)
      {
         err = psnd_pcm_nonblock(handle->id, 0);
         if (err < 0) psnd_pcm_close(handle->id);
      }

      if (err < 0)
      {
         if (id == 0) free(handle);
         handle = 0;
      }
      
   }

   _oalRingBufferMixMonoSetRenderer(mode);

   return handle;
}

static int
_aaxALSASoftDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   if (handle)
   {
      if (handle->name)
      {
         if (handle->name != _default_name) {
            free(handle->name);
         }
         handle->name = 0;
      }

      psnd_pcm_close(handle->id);
      free(handle->scratch);
      free(handle);

      if (_default_name != _const_default_name)
      {
         free(_default_name);
         _default_name = (char*)_const_default_name;
         _default_devnum = DEFAULT_DEVNUM;
      }

      return AAX_TRUE;
   }

   return AAX_FALSE;
}


#define TRUN(f, s)	if (err >= 0) { err = f; if (err < 0) _AAX_SYSLOG("alsa; "s); }

static int
_aaxALSASoftDriverSetup(const void *id, size_t *bufsize, int fmt,
                        unsigned int *tracks, float *speed)
{
   _driver_t *handle = (_driver_t *)id;
   snd_pcm_hw_params_t *hwparams;
   snd_pcm_sw_params_t *swparams;
   unsigned int rate = *speed;
   unsigned int channels;
   int err = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(handle != 0);

   channels = *tracks;
   if (channels > handle->no_channels) {
      channels = handle->no_channels;
   } else if (handle->no_channels > channels) {
      channels = handle->no_channels;
   }

   hwparams = calloc(1, psnd_pcm_hw_params_sizeof());
   swparams = calloc(1, psnd_pcm_sw_params_sizeof());
   if (hwparams && swparams)
   {
      snd_pcm_t *hid = handle->id;
      snd_pcm_format_t data_format;
      snd_pcm_uframes_t size;
      unsigned int bps = handle->bytes_sample;
      unsigned int periods = handle->no_periods;
      unsigned int val1, val2;

      /* Set buffer size (in frames). The resulting latency is given by */
      /* latency = size * periods / (rate * tracks * bps))     */
      if (bufsize && (*bufsize > 0)) {
         size = *bufsize; // * *tracks;
      } else {
         size = rate/periods;
      }
      if (size < (128*channels*bps)) size = 128*channels*bps;

      err = psnd_pcm_hw_params_any(hid, hwparams);
      TRUN( psnd_pcm_hw_params_set_rate_resample(hid, hwparams, 0),
            "unable to disable sample rate conversion" );

      /* TODO: opt for 32-bit (bps = 4) if possible */
      bps = 2;
      data_format = _alsa_formats[bps];

      TRUN( psnd_pcm_hw_params_set_format(hid, hwparams, data_format),
            "unsupported audio fromat" );

      if (err >= 0  && handle->mode == 0)	/* capture */
      {
         handle->interleaved = 1;
         handle->use_mmap = 0;
         err = psnd_pcm_hw_params_set_access(hid, hwparams,
                                         SND_PCM_ACCESS_RW_INTERLEAVED);
         if (err < 0) _AAX_SYSLOG("alsa; unable to set interleaved mode");
      }
      else if (err >= 0)			/* playback */
      {
         handle->use_mmap = 1;
         handle->interleaved = 0;
         err = psnd_pcm_hw_params_set_access(hid, hwparams,
                                         SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
         if (err < 0)
         {
            handle->use_mmap = 0;
            err = psnd_pcm_hw_params_set_access(hid, hwparams,
                                           SND_PCM_ACCESS_RW_NONINTERLEAVED);
         }

         if (err < 0)
         {

            handle->use_mmap = 1;
            handle->interleaved = 1;
            err = psnd_pcm_hw_params_set_access(hid, hwparams,
                                           SND_PCM_ACCESS_MMAP_INTERLEAVED);
            if (err < 0)
            {
               handle->use_mmap = 0;
               err = psnd_pcm_hw_params_set_access(hid, hwparams,
                                              SND_PCM_ACCESS_RW_INTERLEAVED);
            }

            if (err < 0) _AAX_SYSLOG("alsa; unable to find a proper renderer");
         }

         do
         {
            data_format = _alsa_formats[bps];
            err = psnd_pcm_hw_params_set_format(hid, hwparams, data_format);
         }
         while ((err < 0) && (bps-- != 0));
         handle->bytes_sample = bps;

#if 0
         TRUN( psnd_pcm_hw_params_can_mmap_sample_resolution(params) );
         handle->use_mmap = (err == 1);
#endif

         handle->latency = (float)size*(float)periods;
         handle->latency /= (float)rate*(float)*tracks*(float)bps;
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
         snprintf(str,255, "  playback rate: %u hz",  rate);
         _AAX_SYSLOG(str);
         snprintf(str,255, "  buffer size: %u bytes", (unsigned int)size);
         _AAX_SYSLOG(str);
         snprintf(str,255, "  latency: %3.2f ms",  1e3*handle->latency);
         _AAX_SYSLOG(str);
         snprintf(str,255, "  no. periods: %i", handle->no_periods);
         _AAX_SYSLOG(str);
         snprintf(str,255,"  use mmap: %s", handle->use_mmap?"yes":"no");
         _AAX_SYSLOG(str);
         snprintf(str,255,"  interleaved: %s",handle->interleaved?"yes":"no");
         _AAX_SYSLOG(str);
         snprintf(str,255,"  channels: %i, bytes/sample: %i\n", channels, handle->bytes_sample);
         _AAX_SYSLOG(str);
#if 0
// printf("\tformat: %X", fmt);
#endif
      } while (0);

      do
      {
         int err = psnd_pcm_hw_params_test_channels(hid, hwparams, channels);
         int tracks = channels;
         if (err < 0)
         {
            do
            {
               channels -= 2;
               if ((int)channels < 2) break;

               err = psnd_pcm_hw_params_test_channels(hid, hwparams, channels);
            }
            while (err < 0);
         }

         if (channels != tracks)
         {
            char str[255];
            snprintf((char *)&str, 255, "Unable to output to %i speakers"
                                   " (%i is the maximum)", tracks, channels);
            _AAX_SYSLOG(str);
         }
      }
      while (0);

      TRUN( psnd_pcm_hw_params_set_channels(hid, hwparams, channels),
            "unsupported no. channels" );
      if (channels > handle->no_channels) handle->no_channels = channels;
      *tracks = channels;

      TRUN( psnd_pcm_hw_params_set_rate_near(hid, hwparams, &rate, 0),
            "unsupported sample rate" );
      TRUN( psnd_pcm_hw_params_set_buffer_size_near(hid, hwparams, &size),
            "unvalid buffer size" );
      TRUN( psnd_pcm_hw_params_set_periods_near(hid, hwparams, &periods, 0),
            "unsupported no. periods" ); 
      TRUN( psnd_pcm_hw_params(hid, hwparams),
            "incompatible hardware parameters"  );
      *bufsize = size;

      val1 = val2 = 0;
      err = psnd_pcm_hw_params_get_rate_numden(hwparams, &val1, &val2);
      if (val1 && val2)
      {
         handle->frequency_hz = (float)val1/(float)val2;
         rate = (unsigned)handle->frequency_hz;
      }
      handle->frequency_hz = rate;
      *speed = rate;

      err = psnd_pcm_hw_params_get_buffer_size(hwparams, &size);
      err = psnd_pcm_hw_params_get_periods(hwparams, &periods, 0);
      handle->no_periods = periods;

      handle->can_pause = psnd_pcm_hw_params_can_pause(hwparams);
      handle->can_pause &= psnd_pcm_hw_params_can_resume(hwparams);

      if (handle->use_mmap && !handle->interleaved)
      {
         _aaxALSASoftDriverBackend.play = _aaxALSASoftDriverPlayback_mmap_ni;
      }
      else if (handle->use_mmap && handle->interleaved)
      {
         _aaxALSASoftDriverBackend.play = _aaxALSASoftDriverPlayback_mmap_il;
      }
      else if (!handle->use_mmap && !handle->interleaved)
      {
         _aaxALSASoftDriverBackend.play = _aaxALSASoftDriverPlayback_rw_ni;
      }
      else
      {
         _aaxALSASoftDriverBackend.play = _aaxALSASoftDriverPlayback_rw_il;
      }
      TRUN( psnd_pcm_hw_params(hid, hwparams), "unabel to configure hardware" );

      TRUN( psnd_pcm_sw_params_current(hid, swparams), 
            "unable to set software config" );
      if (handle->mode == 0)	/* record */
      {
         snd_pcm_uframes_t period_size;
         int dir;

         TRUN( psnd_pcm_hw_params_get_period_size(hwparams, &period_size, &dir),
               "unable to get period size" );
#if 0
         TRUN( psnd_pcm_sw_params_set_start_threshold(hid, swparams, 0U),
               "improper interrupt treshold" );
#endif
         TRUN( psnd_pcm_sw_params_set_avail_min(hid, swparams, period_size),
               "wakeup treshold unsupported" );
      }
      else			/* playback */
      {
         snd_pcm_uframes_t period_size;
         int dir;

         TRUN( psnd_pcm_hw_params_get_period_size(hwparams, &period_size, &dir),
               "unable to get period size" );

         TRUN( psnd_pcm_sw_params_set_start_threshold(hid, swparams,
                                                (size/period_size)*period_size),
               "improper interrupt treshold" );
         TRUN( psnd_pcm_sw_params_set_avail_min(hid, swparams, period_size),
               "wakeup treshold unsupported" );
      }
      TRUN( psnd_pcm_sw_params(hid, swparams),
            "unable to configure software" );

      TRUN( psnd_pcm_prepare(hid), "failed preparation" );
   }

   if (swparams) free(swparams);
   if (hwparams) free(hwparams);

   psnd_lib_error_set_handler(_alsa_error_handler);

   return (err >= 0) ? AAX_TRUE : AAX_FALSE;
}
#undef TRUN

static int
_aaxALSASoftDriverPause(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int err = -ENOSYS;

   assert(id);

   if (psnd_pcm_state(handle->id) == SND_PCM_STATE_RUNNING && !handle->pause)
   {
      handle->pause = 1;
      if (handle->can_pause) {
         err = psnd_pcm_pause(handle->id, 1);
      }
#if 0
      else {
         err = psnd_pcm_drop(handle->id);
      }
#endif
   }

   return err;
}

static int
_aaxALSASoftDriverResume(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int err = 0;

   assert(id);

   if (psnd_pcm_state(handle->id) == SND_PCM_STATE_PAUSED && handle->pause)
   {
      handle->pause = 0;
      if (handle->can_pause) {
         err = psnd_pcm_pause(handle->id, 0);
      }
#if 0
      else {
         err = psnd_pcm_prepare(handle->id);
      }
#endif
   }

   return err;
}

static int
_aaxALSASoftDriverAvailable(const void *id)
{
   return AAX_TRUE;
}

static int
_aaxALSASoftDriverIsAvailable(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;

   assert(id);

   if (psnd_pcm_state(handle->id) != SND_PCM_STATE_DISCONNECTED) {
      rv = AAX_TRUE;
   }

   return rv;
}

static int
_aaxALSASoftDriverRecord(const void *id, void *data, size_t *size, float pitch, float volume)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int frames, frame_size, avail;
   snd_pcm_state_t state;
   int res, rv = AAX_FALSE;

   if ((handle->mode != 0) || (size == 0) || (data == 0))
   {
      if (handle->mode == 0) {
         _AAX_SYSLOG("alsa; calling the record function with a playback handle");
      } else if (size == 0) {
         _AAX_SYSLOG("alsa; record buffer size is zero bytes");
      } else {
         _AAX_SYSLOG("alsa; calling the record function with null pointer");
      }
      return rv;
   }

   state = psnd_pcm_state(handle->id);
   if (state != SND_PCM_STATE_RUNNING)
   {
      if (state <= SND_PCM_STATE_PREPARED)
      {
         if (handle->playing++ < 1) {
            psnd_pcm_prepare(handle->id);
         } else {
            psnd_pcm_start(handle->id);
         }
      }
      else if (state == SND_PCM_STATE_XRUN) {
         _AAX_SYSLOG("alsa (record): state = SND_PCM_STATE_XRUN.");
         xrun_recovery(handle->id, -EPIPE);
      }
   }

   frame_size = handle->no_channels * handle->bytes_sample;
   frames = *size / frame_size;
#ifndef NDEBUG
   handle->buf_len = frames * frame_size;
#endif


   *size = 0;
   avail = psnd_pcm_avail(handle->id);
   if (avail < 0)
   {
      if ((res = xrun_recovery(handle->id, avail)) < 0)
      {
         char s[255];
         snprintf(s, 255, "PCM avail error: %s\n", psnd_strerror(res));
         _AAX_SYSLOG(s);
         avail = -1;
      }
   }

   if (frames && (avail >= frames))
   {
      int try = 0;

      do
      {
         do {
            res = psnd_pcm_readi(handle->id, data, frames);
         }
         while (res == -EAGAIN);

         if (res < 0)
         {
            if (xrun_recovery(handle->id, res) < 0)
            {
               _AAX_SYSLOG("alsa; unable to run xrun_recovery");
               rv = AAX_FALSE;
               break;
            }
            if (try++ > 2)
            {
               _AAX_SYSLOG("alsa; unable to recover from pcm read error");
               rv = AAX_FALSE;
               break;
            }
            _AAX_SYSLOG("alsa; warning: pcm read error");
            continue;
         }

         *size += (res * frame_size);
         frames -= res;
         rv = AAX_TRUE;
      }
      while ((res > 0) && frames);
   }
   else rv = AAX_TRUE;

   return rv;
}

static char *
_aaxALSASoftDriverGetName(const void *id, int playback)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = (char *)_default_name;

   /* TODO: distinguish between playback and record */
   if (handle && handle->name) {
      ret = handle->name;
   }

   return ret;
}

static char *
_aaxALSASoftDriverGetDevices(const void *id, int mode)
{
   static char names[2][1024] = { "\0\0", "\0\0" };
   int card_idx;
   char *ptr;
   int m, len;

   len = 1024;
   m = (mode > 0) ? 1 : 0;
   ptr = (char *)&names[m];
   card_idx = -1;
   while ((psnd_card_next(&card_idx) == 0) && (card_idx >= 0))
   {
      static char _found = 0;
      char *cardname, *devname;
      int slen, err, devnum;
      snd_pcm_t *id;

      psnd_card_get_name(card_idx, &cardname);
      devnum = detect_devnum(cardname, m);
      devname = detect_devname(cardname, devnum, 2, __mode[m]);
      err = psnd_pcm_open(&id, devname, __mode[m], SND_PCM_NONBLOCK);
      if (err == 0)
      {
         psnd_pcm_close(id);
         snprintf(ptr, len, "%s", cardname);

         slen = strlen(ptr)+1;
         if (slen > (len-1)) break;

         len -= slen;
         ptr += slen;

         if (!_found)
         {
            _default_name = devname;
            _default_devnum = devnum;
            _found = 1;
         }
         else {
            free(devname);
         }
      }
      else {
         free(devname);
      }
   }
   *ptr = 0;

   return (char *)&names[mode];
}

static char *
_aaxALSASoftDriverGetInterfaces(const void *id, const char *name, int mode)
{
   static char names[2][256] = { "\0\0", "\0\0" };
   char devname[64] = "hw:0";
   int m, card_idx;
   snd_ctl_t *ctl;

   m = (mode > 0) ? 1 : 0;
   card_idx = detect_devnum(name, m);
   snprintf(devname, 64, "hw:%i", card_idx);

   if (psnd_ctl_open(&ctl, devname, 0) >= 0)
   {
      snd_pcm_info_t *info;
      int len, dev_idx;
      char *ptr;

      info = (snd_pcm_info_t *) calloc(1, psnd_pcm_info_sizeof());

      len = 256;
      ptr = (char *)&names[m];
      dev_idx = -1;
      while ((psnd_ctl_pcm_next_device(ctl, &dev_idx) == 0) && (dev_idx >= 0))
      {
         psnd_pcm_info_set_device(info, dev_idx);
         psnd_pcm_info_set_subdevice(info, 0);
         psnd_pcm_info_set_stream(info, __mode[m]);

         if (psnd_ctl_pcm_info(ctl, info) >= 0)
         {
            char *ifname = (char *)psnd_pcm_info_get_name(info);
//          int subdev_ct = psnd_pcm_info_get_subdevices_count(info);
            int slen;

            snprintf(ptr, len, "%s (hw:%i,%i)", ifname, card_idx, dev_idx);

            slen = strlen(ptr)+1;
            if (slen > (len-1)) break;

            len -= slen;
            ptr += slen;
         }
      }

      *ptr = 0;
      free(info);
      psnd_ctl_close(ctl);
   }

   return (char *)&names[mode];
}

/*-------------------------------------------------------------------------- */

static const snd_pcm_format_t _alsa_formats[] =
{
   SND_PCM_FORMAT_UNKNOWN,
   SND_PCM_FORMAT_U8,
   SND_PCM_FORMAT_S16,
   SND_PCM_FORMAT_S24,
   SND_PCM_FORMAT_S32
};

static void
_alsa_error_handler(const char *file, int line, const char *function, int err,
                    const char *fmt, ...)
{
#ifndef NDEBUG
   char s[1024];
   va_list ap;

   snprintf((char *)&s, 1024, "%s at line %i in function %s:", file, line, function);
   _AAX_LOG(LOG_ERR, s);

   va_start(ap, fmt);
   snprintf((char *)&s, 1024, fmt, va_arg(ap, char *));
   va_end(ap);

   _AAX_LOG(LOG_ERR, s);
#endif
}

static char *
detect_devname(const char *devname, int devnum, unsigned int tracks, int m)
{
   static const char* dev_prefix[] = {
         "plughw:", "hw:", "surround40:", "surround51:", "surround71:"
   };
   char *rv = (char*)_default_name;

   if (tracks <= _AAX_MAX_SPEAKERS)
   {
      unsigned int len;
      char *name;

      tracks /= 2;
      len = strlen(dev_prefix[tracks])+6;

      name = malloc(len);
      if (name)
      {
         char *ptr;

         snprintf(name, len, "%s%i", dev_prefix[tracks], devnum);
         rv = name;

         if (devname && (ptr = strstr(devname, ": ")) != NULL)
         {
            int ifnum = 0;

            ptr += 2;
            while (*ptr == ' ' && *ptr != '\0') ptr++;

            ifnum = detect_ifnum(name, ptr, m);
            snprintf(name, len, "%s%i,%i", dev_prefix[tracks], devnum, ifnum);
         }
      }
   }
   return rv;
}

static int
detect_devnum(const char *name, int m)
{
   int devnum = _default_devnum;
   char *ptr = NULL;

   if ( !name ) {
      devnum = _default_devnum;
   }
   else if ( !strcmp(name, "hw:") || (ptr = strstr(name, "(hw:")) != NULL )
   {
      if (!ptr) ptr = (char *)name;
      else ptr++;

      devnum = atoi(ptr+3);
   }
   else if ( !strcmp(name, "surround") )
   {
      char *c = strchr(name, ':');
      if (c) {
         devnum = atoi(c+1);
      } else {
         devnum = 0;
      }
   }
   else if ( !strcasecmp(name, "default") )
   {
      int card_idx;

      card_idx = -1;
      while ((psnd_card_next(&card_idx) == 0) && (card_idx >= 0))
      {
         char *cardname, *devname;
         snd_pcm_t *id;
         int err;

         psnd_card_get_name(card_idx, &cardname);
         devnum = detect_devnum(cardname, m);
         devname = detect_devname(cardname, devnum, 2, __mode[m]);
         err = psnd_pcm_open(&id, devname, __mode[m], SND_PCM_NONBLOCK);
         free(devname);
         if (err >= 0)
         {
            psnd_pcm_close(id);
            devnum = card_idx;
            break;
         }
      }
   }
   else
   {
      char *ptr = strstr(name, ": ");
      int len, card_idx;

      if (ptr) {
         len = ptr - name;
      } else {
         len = strlen(name);
      }

      card_idx = -1;
      while ((psnd_card_next(&card_idx) == 0) && (card_idx >= 0))
      {
         char *cardname;

         psnd_card_get_name(card_idx, &cardname);
         if ( !strncasecmp(name, cardname, len) )
         {
            devnum = card_idx;
            break;
         }
      }
   }

   return devnum;
}

static int
detect_ifnum(char *devname, const char *name, int m)
{
   snd_ctl_t *ctl;
   int rv = 0;
   char *ptr;

   ptr = strstr(name, "(hw:");
   if (ptr)
   {
      ptr = strchr(ptr+4, ',');
      if (ptr) {
         rv = atoi(ptr+1);
      }
   }
   else if (psnd_ctl_open(&ctl, devname, 0) >= 0)
   {
      snd_pcm_info_t *info;
      int dev_idx;

      info = (snd_pcm_info_t *) calloc(1, psnd_pcm_info_sizeof());

      dev_idx = -1;
      while ((psnd_ctl_pcm_next_device(ctl, &dev_idx) == 0) && (dev_idx >= 0))
      {
         psnd_pcm_info_set_device(info, dev_idx);
         psnd_pcm_info_set_stream(info, __mode[m]);

         psnd_pcm_info_set_subdevice(info, 0);
         if (psnd_ctl_pcm_info(ctl, info) >= 0)
         {
            char *ifname = (char *)psnd_pcm_info_get_name(info);
            if (!strcasecmp(ifname, name))
            {
               rv = dev_idx;
               break;
            }
         }
      }
      psnd_ctl_close(ctl);
   }
   return rv;
}

unsigned int
get_devices_avail(int m)
{
   unsigned int rv = 0;
   int card_idx;

   card_idx = -1;
   while ((psnd_card_next(&card_idx) == 0) && (card_idx >= 0))
   {
      char *cardname, *devname;
      int err, devnum;
      snd_pcm_t *id;

      psnd_card_get_name(card_idx, &cardname);

      devnum = detect_devnum(cardname, m);
      devname = detect_devname(cardname, devnum, 2, __mode[m]);
      err = psnd_pcm_open(&id, devname, __mode[m], SND_PCM_NONBLOCK);
      free(devname);
      if (err >= 0)
      {
         psnd_pcm_close(id);
         rv++;
         break;
      }
   }

   return rv;
}

static int
xrun_recovery(snd_pcm_t *handle, int err)
{
   if (err == -EPIPE)   /* under-run */
   {
      _AAX_SYSLOG("alsa; buffer underrun.");

      err = psnd_pcm_prepare(handle);
      if (err < 0) {
         _AAX_SYSLOG("alsa; unable to recover from underrun");
      }

      return 0;
   }
   else if (err == -ESTRPIPE)
   {
      static const struct timespec sleept = {0, 1000};

      /* wait until the suspend flag is released */
      while ((err = psnd_pcm_resume(handle)) == -EAGAIN) {
         nanosleep(&sleept, 0); /* wait until the suspend flag is released */
      }
      if (err < 0)
      {
         err = psnd_pcm_prepare(handle);
         if (err < 0) {
            _AAX_SYSLOG("alsa; unable to recover from suspend");
         }
      }
      return 0;
   }

   /*
    *  Make sure PCM is in the right state:
    *    SND_PCM_STATE_PREPARED or SND_PCM_STATE_RUNNING
    */
   assert(err != -EBADFD);

   return err;
}


static int
_aaxALSASoftDriverPlayback_mmap_ni(const void *id, void *dst, void *src, float pitch, float volume)
{
   _driver_t *handle = (_driver_t *)id;
   _oalRingBuffer *rbs = (_oalRingBuffer *)src;
   unsigned int no_tracks, offs, t;
   _oalRingBufferSample *rbsd;
   snd_pcm_sframes_t no_frames;
   snd_pcm_sframes_t avail;
   snd_pcm_state_t state;
   unsigned int start = 0;
   int err;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(handle != 0);
   if (handle->pause) return 0;

   assert(rbs != 0);
   assert(rbs->sample != 0);

   rbsd = rbs->sample;
   offs = _oalRingBufferGetOffsetSamples(rbs);
   no_frames = _oalRingBufferGetNoSamples(rbs) - offs;
   no_tracks = _oalRingBufferGetNoTracks(rbs);

   state = psnd_pcm_state(handle->id);
   if (state != SND_PCM_STATE_RUNNING)
   {
      if (state == SND_PCM_STATE_PREPARED)
      {
         if (handle->playing++ < 1) {
            psnd_pcm_prepare(handle->id);
         } else {
            psnd_pcm_start(handle->id);
         }
      }
      else if (state == SND_PCM_STATE_XRUN) {
         _AAX_SYSLOG("alsa (mmap_ni): state = SND_PCM_STATE_XRUN.");
         xrun_recovery(handle->id, -EPIPE);
      }
   }

   avail = psnd_pcm_avail_update(handle->id);
   if (avail < 0)
   {
      int err;
      if ((err = xrun_recovery(handle->id, avail)) < 0)
      {
         char s[255];
         snprintf(s, 255, "PCM avail error: %s\n", psnd_strerror(err));
         _AAX_SYSLOG(s);
         return 0;
      }
   }

   if (avail < no_frames) avail = 0;
   else avail = no_frames;

   while(avail > 0)
   {
      const snd_pcm_channel_area_t *areas;
      snd_pcm_uframes_t frames = avail;
      snd_pcm_uframes_t offset;
      snd_pcm_sframes_t result;
      int16_t *ptr;

      err = psnd_pcm_mmap_begin(handle->id, &areas, &offset, &frames);
      if (err < 0)
      {
         if ((err = xrun_recovery(handle->id, err)) < 0)
         {
            char s[255];
            snprintf(s, 255, "MMAP begin avail error: %s\n",psnd_strerror(err));
            _AAX_SYSLOG(s);
            return 0;
         }
      }

      for (t=0; t<no_tracks; t++)
      {
         char *p = (char *)areas[t].addr + handle->bytes_sample*offset;
         ptr = (int16_t *)p;
         _batch_cvt24_16(ptr, (const int32_t *)rbsd->track[t]+offs, frames);
#if 0
         for (s=0; s<frames; s++)
         {
            int16_t smp = rbsd->get_sample(rbsd, t, s);
            *ptr++ = smp;
         }
#endif
      }

      result = psnd_pcm_mmap_commit(handle->id, offset, frames);
      if (result < 0 || (snd_pcm_uframes_t)result != frames)
      {
         if (xrun_recovery(handle->id, result >= 0 ? -EPIPE : result) < 0) {
            return 0;
         }
      }
      start += result;
      avail -= result;
   }

   return 0;
}


static int
_aaxALSASoftDriverPlayback_mmap_il(const void *id, void *dst, void *src, float pitch, float volume)
{
   _driver_t *handle = (_driver_t *)id;
   _oalRingBuffer *rbs = (_oalRingBuffer *)src;
   unsigned int no_tracks, offs;
   _oalRingBufferSample *rbsd;
   snd_pcm_sframes_t no_frames;
   snd_pcm_sframes_t avail;
   snd_pcm_state_t state;
   unsigned int start = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(handle != 0);

   assert(rbs != 0);
   assert(rbs->sample != 0);

   rbsd = rbs->sample;
   offs = _oalRingBufferGetOffsetSamples(rbs);
   no_frames = _oalRingBufferGetNoSamples(rbs) - offs;
   no_tracks = _oalRingBufferGetNoTracks(rbs);

   state = psnd_pcm_state(handle->id);
   if (state != SND_PCM_STATE_RUNNING)
   {
      if (state == SND_PCM_STATE_PREPARED)
      {
         if (handle->playing++ < 1) {
            psnd_pcm_prepare(handle->id);
         } else {
            psnd_pcm_start(handle->id);
         }
      }
      else if (state == SND_PCM_STATE_XRUN) {
         _AAX_SYSLOG("alsa (mmap_il): state = SND_PCM_STATE_XRUN.");
         xrun_recovery(handle->id, -EPIPE);
      }
   }

   avail = psnd_pcm_avail_update(handle->id);
   if (avail < 0)
   {
      int err;
      if ((err = xrun_recovery(handle->id, avail)) < 0) 
      {
         char s[255];
         snprintf(s, 255, "PCM avail error: %s\n", psnd_strerror(err));
         _AAX_SYSLOG(s);
         return 0;
      }
   }
   
   if (avail < no_frames) avail = 0;
   else avail = no_frames;

   while(avail > 0)
   {
      const snd_pcm_channel_area_t *area;
      snd_pcm_uframes_t frames = avail;
      snd_pcm_uframes_t offset;
      snd_pcm_sframes_t result;
      int16_t *ptr;
      char *tmp;
      int err;

      err = psnd_pcm_mmap_begin(handle->id, &area, &offset, &frames);
      if (err < 0)
      {
         if ((err = xrun_recovery(handle->id, err)) < 0)
         {
            char s[255];
            snprintf(s, 255, "MMAP begin avail error: %s\n",psnd_strerror(err));
            _AAX_SYSLOG(s);
            return 0;
         }
      }

      tmp = (char *)area->addr + ((area->first + area->step*offset) >> 3);
      ptr = (int16_t *)tmp;
      _batch_cvt24_16_intl(ptr, (const int32_t**)rbsd->track, start, no_tracks, frames);

      result = psnd_pcm_mmap_commit(handle->id, offset, frames);
      if (result < 0 || (snd_pcm_uframes_t)result != frames)
      {
         if (xrun_recovery(handle->id, result >= 0 ? -EPIPE : result) < 0) {
            return 0;
         }
      }
      start += result;
      avail -= result;
   }

   return 0;
}


static int
_aaxALSASoftDriverPlayback_rw_ni(const void *id, void *dst, void *src, float pitch, float volume)
{
   _driver_t *handle = (_driver_t *)id;
   _oalRingBuffer *rbs = (_oalRingBuffer *)src;
   unsigned int no_samples, no_tracks, offs, t;
   _oalRingBufferSample *rbsd;
   int16_t **data;
   int err;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(handle != 0);
   if (handle->pause) return 0;

   assert(rbs != 0);
   assert(rbs->sample != 0);

   rbsd = rbs->sample;
   offs = _oalRingBufferGetOffsetSamples(rbs);
   no_samples = _oalRingBufferGetNoSamples(rbs) - offs;
   no_tracks = _oalRingBufferGetNoTracks(rbs);

   if (handle->scratch == 0)
   {
      unsigned int samples, outbuf_size, size;
      int16_t *ptr;
      char *p;

      samples = _oalRingBufferGetNoSamples(rbs);
      outbuf_size = samples * sizeof(int16_t);
      if (outbuf_size & 0xF)
      {
         outbuf_size|= 0xF;
         outbuf_size++;
      }

      size = no_tracks * (2*sizeof(void*));
      if (handle->bytes_sample == 2) {
         size += no_tracks * outbuf_size;
      }

      p = (char *)(no_tracks * 2*sizeof(void*));
      handle->scratch = (void**)_aax_malloc(&p, size);
      handle->data = (int16_t **)handle->scratch + no_tracks;

      ptr = (int16_t*)p;
      for (t=0; t<no_tracks; t++)
      {
         handle->scratch[t] = ptr;
         ptr += samples;
      }
#ifndef NDEBUG
      handle->buf_len = outbuf_size;
#endif
   }
  
   data = handle->data;
#ifndef NDEBUG
   assert(no_samples*sizeof(int16_t) <= handle->buf_len);
#endif

   if (handle->bytes_sample == 4)
   {
      for (t=0; t<no_tracks; t++)
         data[t] = (int16_t*)rbsd->track[t] + offs * 4;
   }
   else if (handle->bytes_sample == 2)
   {
      for (t=0; t<no_tracks; t++) {
         data[t] = handle->scratch[t];
         _batch_cvt24_16(data[t], (const int32_t*)rbsd->track[t]+offs, no_samples);
      }
   }
   else return 0;

   while (no_samples > 0)
   {
      do {
         err = psnd_pcm_writen(handle->id, (void**)data, no_samples);
      }
      while (err == -EAGAIN);

      if (err < 0)
      {
         if (xrun_recovery(handle->id, err) < 0) {
            return 0;
         }
         break; /* skip one period */
      }

      for (t=0; t<no_tracks; t++) {
         data[t] += err;
      }
      no_samples -= err;
   }

   return 0;
}


static int
_aaxALSASoftDriverPlayback_rw_il(const void *id, void *dst, void *src, float pitch, float volume
)
{
   _driver_t *handle = (_driver_t *)id;
   _oalRingBuffer *rbs = (_oalRingBuffer *)src;
   unsigned int no_samples, no_tracks, offs;
   unsigned int outbuf_size;
   _oalRingBufferSample *rbsd;
   int16_t *data;
   int err;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(handle != 0);
   if (handle->pause) return 0;

   assert(rbs != 0);
   assert(rbs->sample != 0);

   rbsd = rbs->sample;
   offs = _oalRingBufferGetOffsetSamples(rbs);
   no_samples = _oalRingBufferGetNoSamples(rbs) - offs;
   no_tracks = _oalRingBufferGetNoTracks(rbs);

   outbuf_size = no_tracks * no_samples*sizeof(int16_t);
   if (handle->scratch == 0)
   {
      char *p = 0;
      handle->scratch = (void**)_aax_malloc(&p, outbuf_size);
      handle->data = (int16_t**)p;
#ifndef NDEBUG
      handle->buf_len = outbuf_size;
#endif
   }
   data = (int16_t*)handle->data;
#if 0
   assert(outbuf_size <= handle->buf_len);
#endif

   _batch_cvt24_16_intl(data, (const int32_t**)rbsd->track, offs, no_tracks, no_samples);

   while (no_samples > 0)
   {
      int try = 0;

      do {
         err = psnd_pcm_writei(handle->id, data, no_samples);
      }
      while (err == -EAGAIN);

      if (err < 0)
      {
         if (xrun_recovery(handle->id, err) < 0)
         {
            _AAX_SYSLOG("alsa; unable to run xrun_recovery");
            return 0;
         }
         if (try++ > 2) 
         {
            _AAX_SYSLOG("alsa; unable to recover from pcm write error");
            break;
         }
         _AAX_SYSLOG("alsa; warning: pcm write error");
         continue;
      }

      data += err * no_tracks;
      no_samples -= err;
   }

   return 0;
}


void *
_aaxALSASoftDriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   _intBufferData *dptr_sensor;
   const _aaxDriverBackend *be;
   _oalRingBuffer *dest_rb;
   _aaxAudioFrame *mixer;
   float delay_sec;
   int stdby_time;
   char state;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return NULL;
   }

   /*
    * We're actually abusing the Ringbuffer container a bit since
    * the internal data format is interleaved instead of non-
    * interleaved. This shouldn't cause any problems though since
    * it won't be used internally but instead is just a container
    * to move the data form that driver to the client application.
    */
   dest_rb = _oalRingBufferCreate(AAX_TRUE);
   if (!dest_rb) {
      return NULL;
   }

   delay_sec = 1.0/handle->info->refresh_rate;

   be = handle->backend.ptr;
   dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      _sensor_t* sensor = _intBufGetDataPtr(dptr_sensor);
      mixer = sensor->mixer;

      _oalRingBufferSetFormat(dest_rb, be->codecs, AAX_PCM24S);
      _oalRingBufferSetNoTracks(dest_rb, mixer->info->no_tracks);
      _oalRingBufferSetFrequency(dest_rb, mixer->info->frequency);
      _oalRingBufferSetDuration(dest_rb, delay_sec);
      _oalRingBufferInit(dest_rb, AAX_TRUE);
      _oalRingBufferStart(dest_rb);

      mixer->ringbuffer = dest_rb;
      _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
   }
   else
   {
      _oalRingBufferDelete(dest_rb);
      return NULL;
   }

   be->pause(handle->backend.handle);
   state = AAX_SUSPENDED;

   _aaxMutexLock(handle->thread.mutex);
   stdby_time = 2*(int)(delay_sec*1e3);
   while TEST_FOR_TRUE(handle->thread.started)
   {
      _driver_t *be_handle = (_driver_t *)handle->backend.handle;
      int err;

      _aaxMutexUnLock(handle->thread.mutex);

//    if (!_IS_STANDBY(handle) && be->is_available(be_handle))
      if (_IS_PLAYING(handle) && be->is_available(be_handle))
      {
				/* timeout is in ms */
         if ((err = psnd_pcm_wait(be_handle->id, stdby_time)) < 0)
         {
            _AAX_SYSLOG("alsa; snd_pcm_wait polling error");
            _aaxMutexLock(handle->thread.mutex);
            break;
         }
      }
      else
      {
         static struct timespec sleept = {0, 1000};
         sleept.tv_nsec = (long)(delay_sec*1e9);
         nanosleep(&sleept, 0);
      }
      _aaxMutexLock(handle->thread.mutex);

#if 1
      /* Ultimately this line should go */
      if TEST_FOR_FALSE(handle->thread.started) {
         break;
      }
#endif

#if 0
 printf("state: %i, paused: %i\n", state, _IS_PAUSED(handle));
 printf("playing: %i, standby: %i\n", _IS_PLAYING(handle), _IS_STANDBY(handle));
#endif
      if (state != handle->state)
      {
         if (_IS_PAUSED(handle) ||
             (!_IS_PLAYING(handle) && _IS_STANDBY(handle))) {
            be->pause(handle->backend.handle);
         }
         else if (_IS_PLAYING(handle) || _IS_STANDBY(handle)) {
            be->resume(handle->backend.handle);
         }
         state = handle->state;
      }

      /* do all the mixing */
      _aaxSoftwareMixerThreadUpdate(handle, dest_rb);
   }
   _aaxMutexUnLock(handle->thread.mutex);

   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      _oalRingBufferStop(mixer->ringbuffer);
      _oalRingBufferDelete(mixer->ringbuffer);
   }
   return handle;
}
