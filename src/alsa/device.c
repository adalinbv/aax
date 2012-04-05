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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_ASSERT_H
#include <assert.h>
#endif
#if HAVE_ALLOCA_H
# include <alloca.h>
#endif

#if HAVE_TIME_H
#include <time.h>
#endif
#include <stdio.h>
#include <errno.h>
#include <strings.h>
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

#include "audio.h"
#include "device.h"

#define MIN_HW_CHANNELS		8
#define HW_SEQUENCER		0
#define MAX_ALLOWED_SOURCES	512
#define DEFAULT_OUTPUT_RATE	48000
#define DEFAULT_NAME		"hw:0"	/* hardware device name */
#define DEFAULT_RENDERER	"ALSA Hardware"
#define MULTI_PLAYBACK_VOLUME	"OpenAL Hardware Volume"
#define MULTI_PLAYBACK_SWITCH	"OpenAL Hardware Switch"


static _aaxDriverDetect _aaxALSADriverDetect;
static _aaxDriverGetDevices _aaxALSADriverGetDevices;
static _aaxDriverGetInterfaces _aaxALSADriverGetInterfaces;
static _aaxDriverConnect _aaxALSADriverConnect;
static _aaxDriverDisconnect _aaxALSADriverDisconnect;
static _aaxDriverSetup _aaxALSADriverSetup;
static _aaxDriverCallback _aaxALSADriverPlayback;
static _aaxDriverState _aaxALSADriverPause;
static _aaxDriverState _aaxALSADriverResume;
static _aaxDriverGetName _aaxALSADriverGetName;
static _aaxDriver3dMixerCB _aaxALSADriver3dMixer;
static _aaxDriverState _aaxALSADriverAvailable;

char _hw_default_renderer[100] = DEFAULT_RENDERER;
_aaxDriverBackend _aaxALSADriverBackend =
{
   1.0,
   AAX_PCM16S,
   DEFAULT_OUTPUT_RATE,
   2,

   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_hw_default_renderer,

   (_aaxCodec **)&_oalRingBufferCodecs,

   (_aaxDriverDetect *)&_aaxALSADriverDetect,
   (_aaxDriverGetDevices *)&_aaxALSADriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxALSADriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxALSADriverGetName,
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,

   (_aaxDriverConnect *)&_aaxALSADriverConnect,
   (_aaxDriverDisconnect *)&_aaxALSADriverDisconnect,
   (_aaxDriverSetup *)&_aaxALSADriverSetup,
   (_aaxDriverState *)&_aaxALSADriverPause,
   (_aaxDriverState *)&_aaxALSADriverResume,
   NULL,
   (_aaxDriverCallback *)&_aaxALSADriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxSoftwareDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxALSADriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)&_aaxALSADriverAvailable,
   (_aaxDriverState *)&_aaxALSADriverAvailable,
   (_aaxDriverState *)&_aaxALSADriverAvailable
};

typedef struct
{
   char *name;
   snd_pcm_t *id;
   uint16_t subdev;
   uint16_t rate_hz;
   char periods;
   char bytes_sample;

   char playing;
   char can_pause;
   uint16_t volume[2];

} _alsa_hw_channel;

typedef struct
{
    char *detected_device_name;
    char *name;
    char *interface;

    char pause;
    char use_mmap;
    char interleaved;
    char sse_level;
    int mode;

    snd_hctl_t *hctl;
    snd_hctl_elem_t *elem;
    _alsa_hw_channel **channel;

    uint16_t *hw_channels_list;
    uint16_t *sources_list;
    uint16_t no_hw_channels;
    uint16_t detected_hw_channels;
    uint16_t no_output_channels;
    uint16_t cur_channel;

} _driver_t;


DECL_FUNCTION(snd_pcm_open);
DECL_FUNCTION(snd_pcm_close);
DECL_FUNCTION(snd_pcm_nonblock);
DECL_FUNCTION(snd_pcm_prepare);
DECL_FUNCTION(snd_pcm_resume);
DECL_FUNCTION(snd_pcm_drop);
DECL_FUNCTION(snd_pcm_pause);
DECL_FUNCTION(snd_pcm_stream);
DECL_FUNCTION(snd_pcm_recover);
DECL_FUNCTION(snd_pcm_hw_params_can_pause);
DECL_FUNCTION(snd_pcm_info);
DECL_FUNCTION(snd_pcm_info_sizeof);
DECL_FUNCTION(snd_pcm_info_get_subdevice_name);
DECL_FUNCTION(snd_pcm_info_get_subdevice);
DECL_FUNCTION(snd_pcm_info_get_name);
DECL_FUNCTION(snd_pcm_info_get_subdevices_avail);
DECL_FUNCTION(snd_pcm_info_get_subdevices_count);
DECL_FUNCTION(snd_pcm_info_set_subdevice);
DECL_FUNCTION(snd_pcm_info_set_device);
DECL_FUNCTION(snd_pcm_info_set_stream);
DECL_FUNCTION(snd_card_next);
DECL_FUNCTION(snd_pcm_hw_params_malloc);
DECL_FUNCTION(snd_pcm_hw_params_free);
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
DECL_FUNCTION(snd_pcm_mmap_begin);
DECL_FUNCTION(snd_pcm_mmap_commit);
DECL_FUNCTION(snd_pcm_writen);
DECL_FUNCTION(snd_pcm_writei);
DECL_FUNCTION(snd_pcm_readi);
DECL_FUNCTION(snd_pcm_hw_params_can_mmap_sample_resolution);
DECL_FUNCTION(snd_pcm_hw_params_get_rate_numden);
DECL_FUNCTION(snd_pcm_hw_params_get_buffer_size);
DECL_FUNCTION(snd_pcm_hw_params_get_periods);
DECL_FUNCTION(snd_pcm_hw_params_get_period_size);
DECL_FUNCTION(snd_pcm_avail_update);
DECL_FUNCTION(snd_pcm_state);
DECL_FUNCTION(snd_pcm_start);
DECL_FUNCTION(snd_pcm_delay);
DECL_FUNCTION(snd_card_get_name);
DECL_FUNCTION(snd_strerror);
DECL_FUNCTION(snd_lib_error_set_handler);

DECL_FUNCTION(snd_ctl_open);
DECL_FUNCTION(snd_ctl_close);
DECL_FUNCTION(snd_ctl_pcm_info);
DECL_FUNCTION(snd_ctl_pcm_next_device);
DECL_FUNCTION(snd_ctl_elem_value_clear);
DECL_FUNCTION(snd_ctl_elem_id_clear);
DECL_FUNCTION(snd_ctl_elem_info_clear);
DECL_FUNCTION(snd_ctl_elem_list_malloc);
DECL_FUNCTION(snd_ctl_elem_id_sizeof);
DECL_FUNCTION(snd_ctl_elem_id_set_name);
DECL_FUNCTION(snd_ctl_elem_id_set_interface);
DECL_FUNCTION(snd_ctl_elem_info_get_count);
DECL_FUNCTION(snd_ctl_elem_value_sizeof);
DECL_FUNCTION(snd_ctl_elem_info_sizeof);
DECL_FUNCTION(snd_ctl_elem_value_set_interface);
DECL_FUNCTION(snd_ctl_elem_value_set_name);
DECL_FUNCTION(snd_ctl_elem_value_set_index);
DECL_FUNCTION(snd_ctl_elem_value_set_integer);
DECL_FUNCTION(snd_ctl_elem_value_get_integer);
DECL_FUNCTION(snd_hctl_elem_write);
DECL_FUNCTION(snd_hctl_elem_read);

DECL_FUNCTION(snd_hctl_open);
DECL_FUNCTION(snd_hctl_close);
DECL_FUNCTION(snd_hctl_wait);
DECL_FUNCTION(snd_hctl_load);
DECL_FUNCTION(snd_hctl_find_elem);
DECL_FUNCTION(snd_hctl_last_elem);
DECL_FUNCTION(snd_hctl_elem_info);


static _aaxDriverCallback _aaxALSADriverPlayback_rw;
static _aaxDriverCallback _aaxALSADriverPlayback_mmap;
static _aaxDriverCallback *_alsa_playback_fn = _aaxALSADriverPlayback_rw;

static int xrun_recovery(snd_pcm_t *, int);
static char *detect_default_interface(char *);
static char *detect_default_device();
static char *detect_device(const char *);
static unsigned int _detect_no_hw_channels(char *);
static char _setup_channel(_alsa_hw_channel *, unsigned int, unsigned int, char);
static void _alsa_error_handler(const char *, int, const char *, int, const char*,...);


static const snd_pcm_format_t _alsa_formats[];
const char *_hw_default_name = DEFAULT_NAME;

static int
_aaxALSADriverDetect(int mode)
{
   static void *audio = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   audio = _oalIsLibraryPresent("libasound.so.2");
   if (audio)
   {
      char *error;

      _oalGetSymError(0);

      TIE_FUNCTION(snd_pcm_open);
      if (psnd_pcm_open)
      {
         TIE_FUNCTION(snd_pcm_close);
         TIE_FUNCTION(snd_pcm_nonblock);
         TIE_FUNCTION(snd_pcm_prepare);
         TIE_FUNCTION(snd_pcm_resume);
         TIE_FUNCTION(snd_pcm_drop);
         TIE_FUNCTION(snd_pcm_pause);
         TIE_FUNCTION(snd_pcm_stream);
         TIE_FUNCTION(snd_pcm_recover);
         TIE_FUNCTION(snd_pcm_hw_params_can_pause);
         TIE_FUNCTION(snd_pcm_state);
         TIE_FUNCTION(snd_pcm_start);
         TIE_FUNCTION(snd_pcm_delay);
         TIE_FUNCTION(snd_pcm_mmap_begin);
         TIE_FUNCTION(snd_pcm_mmap_commit);
         TIE_FUNCTION(snd_pcm_writen);
         TIE_FUNCTION(snd_pcm_writei);
         TIE_FUNCTION(snd_pcm_readi);
         TIE_FUNCTION(snd_pcm_avail_update);
         TIE_FUNCTION(snd_pcm_info);
         TIE_FUNCTION(snd_pcm_info_sizeof);
         TIE_FUNCTION(snd_pcm_info_get_subdevice_name);
         TIE_FUNCTION(snd_pcm_info_get_subdevice);
         TIE_FUNCTION(snd_pcm_info_get_name);
         TIE_FUNCTION(snd_pcm_info_get_subdevices_avail);
         TIE_FUNCTION(snd_pcm_info_get_subdevices_count);
         TIE_FUNCTION(snd_pcm_info_set_subdevice);
         TIE_FUNCTION(snd_pcm_info_set_device);
         TIE_FUNCTION(snd_pcm_info_set_stream);
         TIE_FUNCTION(snd_pcm_hw_params_malloc);
         TIE_FUNCTION(snd_pcm_hw_params_free);
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
         TIE_FUNCTION(snd_pcm_hw_params_can_mmap_sample_resolution);
         TIE_FUNCTION(snd_pcm_hw_params_get_rate_numden);
         TIE_FUNCTION(snd_pcm_hw_params_get_buffer_size);
         TIE_FUNCTION(snd_pcm_hw_params_get_periods);
         TIE_FUNCTION(snd_pcm_hw_params_get_period_size);
         TIE_FUNCTION(snd_card_next);
         TIE_FUNCTION(snd_card_get_name);
         TIE_FUNCTION(snd_lib_error_set_handler);
         TIE_FUNCTION(snd_strerror);
         TIE_FUNCTION(snd_ctl_open);
         TIE_FUNCTION(snd_ctl_close);
         TIE_FUNCTION(snd_ctl_pcm_info);
         TIE_FUNCTION(snd_ctl_pcm_next_device);
         TIE_FUNCTION(snd_ctl_elem_id_sizeof);
         TIE_FUNCTION(snd_ctl_elem_value_sizeof);
         TIE_FUNCTION(snd_ctl_elem_info_sizeof);
         TIE_FUNCTION(snd_ctl_elem_list_malloc);
         TIE_FUNCTION(snd_ctl_elem_value_clear);
         TIE_FUNCTION(snd_ctl_elem_id_clear);
         TIE_FUNCTION(snd_ctl_elem_id_set_name);
         TIE_FUNCTION(snd_ctl_elem_id_set_interface);
         TIE_FUNCTION(snd_ctl_elem_info_clear);
         TIE_FUNCTION(snd_ctl_elem_value_set_interface);
         TIE_FUNCTION(snd_ctl_elem_info_get_count);
         TIE_FUNCTION(snd_ctl_elem_value_set_name);
         TIE_FUNCTION(snd_ctl_elem_value_set_index);
         TIE_FUNCTION(snd_ctl_elem_value_set_integer);
         TIE_FUNCTION(snd_ctl_elem_value_get_integer);
         TIE_FUNCTION(snd_hctl_elem_write);
         TIE_FUNCTION(snd_hctl_elem_read);
         TIE_FUNCTION(snd_hctl_open);
         TIE_FUNCTION(snd_hctl_close);
         TIE_FUNCTION(snd_hctl_wait);
         TIE_FUNCTION(snd_hctl_load);
         TIE_FUNCTION(snd_hctl_find_elem);
         TIE_FUNCTION(snd_hctl_last_elem);
         TIE_FUNCTION(snd_hctl_elem_info);
      }

      error = _oalGetSymError(0);
      if (error)
      {
         /* fprintf(stderr, "Error: %s\n", error); */
         return AAX_FALSE;
      }
   }
   else
      return AAX_FALSE;

   return AAX_TRUE;
}

static void *
_aaxALSADriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   static snd_pcm_stream_t __mode[] =
                           { SND_PCM_STREAM_CAPTURE, SND_PCM_STREAM_PLAYBACK };
   _driver_t *handle = (_driver_t *)id;
   char periods = 4, bytes_sample = 2;
   unsigned int rate_hz = 44100;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle)
   {
      handle = (_driver_t *)calloc(1, sizeof(_driver_t));
      if (!handle) return 0;

      {
         const char *hwstr = _aaxGetSIMDSupportString();
         snprintf(_hw_default_renderer, 99, "%s %s", DEFAULT_RENDERER, hwstr);
         handle->sse_level = _aaxGetSSELevel();
      }

      if (!renderer)
      {
         if (!handle->detected_device_name)
            handle->detected_device_name = detect_default_device();
         handle->name = handle->detected_device_name;
      }
      else
         handle->name = detect_device(renderer);

      handle->interface = detect_default_interface(handle->name);

      if (xid)
      {
         char *s;
         int i;

         if (!handle->name)
         {
            s = xmlNodeGetString(xid, "renderer");
            if (s && strcmp(s, "default")) handle->name = s;
         }

         i = xmlNodeGetInt(xid, "frequency-hz");
         if (i) rate_hz = i;

         i = xmlNodeGetInt(xid, "channels");
         if (i) handle->no_output_channels = i;

         i = xmlNodeGetInt(xid, "bits-per-sample");
         if (i) bytes_sample = i/8;

         i = xmlNodeGetInt(xid, "periods");
         if (i) periods = i;
      }
      else
      {
         rate_hz = _aaxALSADriverBackend.rate;
         handle->no_output_channels = _aaxALSADriverBackend.tracks;
         bytes_sample = 2;
         periods = 2;
      }
   }

   if (handle)
   {
      unsigned int size, no_hw_channels;
      _alsa_hw_channel **ch;
      snd_hctl_t *hctl;
      unsigned int i;
      int m, err;
      char *ptr;

      m = (mode > 0) ? 1 : 0;
      if (m) no_hw_channels = _detect_no_hw_channels(handle->name)/2;
      else no_hw_channels = 1;

      if (no_hw_channels == 0)
      {
         free(handle);
         return 0;
      }

      handle->detected_hw_channels = no_hw_channels;
      handle->no_hw_channels = no_hw_channels;

      size = sizeof(_alsa_hw_channel) + sizeof(_alsa_hw_channel *)
             + sizeof(uint16_t);

      ptr = calloc(MAX_ALLOWED_SOURCES, sizeof(uint16_t));
      handle->sources_list = (uint16_t *)ptr;

      ptr = calloc(no_hw_channels, size);
      handle->hw_channels_list = (uint16_t *)ptr;

      ptr += no_hw_channels * sizeof(uint16_t);
      handle->channel = (_alsa_hw_channel **)ptr;
      ch = (_alsa_hw_channel **)ptr;

      ptr += no_hw_channels * sizeof(_alsa_hw_channel*);

      if ((err = psnd_hctl_open(&hctl, handle->name, 0)) >= 0)
      {
         uint16_t pos = 0;
#if HW_SEQUENCER
         snd_ctl_elem_value_t *vol_ctl = 0;
         snd_ctl_elem_info_t *elem_info = 0;
         snd_ctl_elem_id_t *ctl_id = 0;
         snd_hctl_elem_t *elem = 0;
         int vcount = 0;
         int volume[2];
         size_t size;

         ctl_id = calloc(1, psnd_ctl_elem_id_sizeof() );
         vol_ctl = calloc(1, psnd_ctl_elem_value_sizeof() );
         elem_info = calloc(1, psnd_ctl_elem_info_sizeof() );

         if (!ctl_id || !vol_ctl || !elem_info)
         {
            if (ctl_id) free(ctl_id);
            if (vol_ctl) free(vol_ctl);
            if (elem_info) free(elem_info);
            return 0;
         }

         err = 0;
         psnd_hctl_load(hctl);
         psnd_ctl_elem_id_set_interface(ctl_id, SND_CTL_ELEM_IFACE_MIXER);
         psnd_ctl_elem_id_set_name(ctl_id, "Master Playback Volume");
         elem = psnd_hctl_find_elem(hctl, ctl_id);
         if (elem)
         {
            err = psnd_hctl_elem_info(elem, elem_info);
            if (err >= 0)
            {
               vcount = psnd_ctl_elem_info_get_count(elem_info);
               err = psnd_hctl_elem_read(elem, vol_ctl);
               if (err >= 0)
               {
                  volume[0] = psnd_ctl_elem_value_get_integer(vol_ctl, 0);
                  if (vcount == 1)
                     volume[1] = psnd_ctl_elem_value_get_integer(vol_ctl, 0);
                  else if (vcount == 2)
                     volume[1] = psnd_ctl_elem_value_get_integer(vol_ctl, 1);
                  else err = -1;
               }
            }
         }
         else err = -1;
         if (err < 0)
         {
            free(elem_info);
            free(vol_ctl);
            free(ctl_id);
            return 0;
         }

         handle->hctl = hctl;
         handle->elem = elem;
#endif
         err = 0;
         for (i=0; i<no_hw_channels; i++)
         {
            snd_pcm_stream_t _mode = __mode[m];

            ch[i] = (_alsa_hw_channel *)ptr;
            ptr += sizeof(_alsa_hw_channel);

            ch[i]->rate_hz = rate_hz;
            ch[i]->bytes_sample = bytes_sample;
            ch[i]->periods = periods;
#if HW_SEQUENCER
            ch[i]->volume[0] = volume[0];
            ch[i]->volume[1] = volume[1];
#endif

            err = psnd_pcm_open(&ch[i]->id, handle->interface, _mode,
                                                      SND_PCM_NONBLOCK);
            if (err >= 0)
            {
               err = psnd_pcm_nonblock(ch[i]->id, 0);
               if (err < 0)
               {
                  psnd_pcm_close(ch[i]->id);
                  continue;
               }

               handle->hw_channels_list[pos++] = i;
            }
            else handle->no_hw_channels--;
         }
#if 0
printf("\n\nHardware settings:\n");
printf("renderer: %s\n", handle->name);
printf("\tchannels: %u", handle->no_output_channels);
printf("\trate: %u hz\n", rate_hz);
printf("\tbytes/sample: %u\n", bytes_sample);
printf("\tno periods: %i\n", periods);
printf("\tuse mmap: %i", handle->use_mmap);
printf("\tinterleaved: %i\n", handle->interleaved);
printf("successful: %i\n", (err >= 0));
#endif

#if HW_SEQUENCER
         free(elem_info);
         free(vol_ctl);
         free(ctl_id);
#endif
      }
      else
        printf("Unable to open ctl interface for '%s'\n", handle->name);

      if ((err < 0) || handle->no_hw_channels < 2)
      {
         if (id == 0) free(handle);
         handle = 0;
      }
   }

   _oalRingBufferMixMonoSetRenderer(mode);

   return handle;
}

static int
_aaxALSADriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   if (handle)
   {
      int i;

      if (handle->name)
      {
         if (handle->name != _hw_default_name)
         {
            free(handle->name);
         }
         if (handle->detected_device_name &&
             handle->detected_device_name != _hw_default_name)
         {
            free(handle->detected_device_name);
         }
         handle->name = 0;
      }

      for (i=0; i<handle->detected_hw_channels; i++)
         psnd_pcm_close(handle->channel[i]->id);

#if HW_SEQUENCER
      psnd_hctl_close(handle->hctl);
#endif
      
      free(handle->hw_channels_list);
      free(handle->sources_list);
      free(handle);

      return AAX_TRUE;
   }

   return AAX_FALSE;
}


static int
_aaxALSADriverSetup(const void *id, size_t *bufsize, int fmt,
                              unsigned int *tracks, float *speed)
{
   _driver_t *handle = (_driver_t *)id;
   int err = -1;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(handle != 0);

   if (fmt == AAX_PCM16S)
   {
      _alsa_hw_channel **ch;
      unsigned int i;

      ch = handle->channel;
      handle->no_output_channels = *tracks;
      for (i=0; i<handle->no_hw_channels; i++)
      {
         uint16_t rate_hz = (uint16_t)*speed;
         err = _setup_channel(ch[i], rate_hz, *bufsize, *tracks);
         if (err < 0) break;
         ch[i]->rate_hz = rate_hz;
      }

      if (err > 0)
      {
         handle->use_mmap = err-1;
         if (handle->use_mmap)
         {
            _alsa_playback_fn = _aaxALSADriverPlayback_mmap;
            _aaxALSADriverBackend.mix3d = _aaxALSADriver3dMixer;
         }
         else
         {
            _alsa_playback_fn = _aaxALSADriverPlayback_rw;
            _aaxALSADriverBackend.mix3d = _aaxALSADriver3dMixer;
         }
      }
   }
   else return AAX_FALSE;

   return (err >= 0) ? AAX_TRUE : AAX_FALSE;
}

static int
_aaxALSADriverPause(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int err = -ENOSYS;

   assert(id);

   if (!handle->pause)
   {
      unsigned i;

      handle->pause = 1;
      for (i=0; i<handle->detected_hw_channels; i++)
      {
         _alsa_hw_channel **ch = handle->channel;

         if (psnd_pcm_state(ch[i]->id) != SND_PCM_STATE_RUNNING) continue;

         if (ch[i]->can_pause) {
            err = psnd_pcm_pause(ch[i]->id, 1);
         }
         else {
            err = psnd_pcm_drop(ch[i]->id);
         }
      }
   }

   return err;
}

static int
_aaxALSADriverResume(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int err = 0;

   assert(id);

   if (handle->pause)
   {
      unsigned i;

      handle->pause = 0;
      for (i=0; i<handle->detected_hw_channels; i++)
      {
         _alsa_hw_channel **ch = handle->channel;

         if (psnd_pcm_state(ch[i]->id) != SND_PCM_STATE_PAUSED) continue;

         if (ch[i]->can_pause) {
            err = psnd_pcm_pause(ch[i]->id, 0);
         }
         else {
            err = psnd_pcm_prepare(ch[i]->id);
         }
      }
   }

   return err;
}

static int
_aaxALSADriverAvailable(const void *id)
{
   return AAX_TRUE;
}

static int
_aaxALSADriver3dMixer(const void *id, void *d, void *s, void *p, void *m, int n)
{
   static _oalRingBuffer *hwbuf = 0;
   _driver_t *handle = (_driver_t *)id;
   int ret = 0;
   int pos;

   assert(handle);
   assert(s);
   assert(d);

   if (!handle->pause)
   {
      float gain = _aaxALSADriverBackend.gain;

      if (hwbuf == 0)
         hwbuf = _oalRingBufferDuplicate(d, AAX_TRUE, AAX_FALSE);

      pos = handle->sources_list[n];
      if (pos == 0)
      {
         if (handle->no_hw_channels > 0)
         {
            pos = handle->hw_channels_list[--handle->no_hw_channels];
            handle->sources_list[n] = pos;
         } else {
            /* TODO: last parameter should be emitter->track */
            ret =_oalRingBufferMixMono16(d, s, p, m, gain, 0);
         }
      }

      if (pos)
      {
         _oalRingBufferClear(hwbuf);
         /* TODO: last parameter should be emitter->track */
         ret = _oalRingBufferMixMono16(hwbuf, s, p, m, gain, 0);
         _alsa_playback_fn(id, handle->channel[pos], hwbuf, 1.0, 1.0);
         if (ret)
         {
            handle->sources_list[n] = 0;
            handle->hw_channels_list[handle->no_hw_channels++] = pos;
         }
      }

      handle->cur_channel++;
   }

   return ret;
}

static char *
_aaxALSADriverGetName(const void *id, int playback)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = (char *)_hw_default_name;

   /* TODO: distinguish between playback and record */
   if (handle && handle->name)
      ret = handle->name;

   return ret;
}

static char *
_aaxALSADriverGetDevices(const void *id, int mode)
{
   static char names[2][256] = { "\0\0", "\0\0" };
   int card_idx;
   char *ptr;
   int len;

   len = 256;
   ptr = (char *)&names[mode];
   card_idx = -1;
   while ((psnd_card_next(&card_idx) == 0) && (card_idx >= 0))
   {
      char alsaname[64];
      int no_hw_channels;

      snprintf(alsaname, 64, "hw:%d", card_idx);
      no_hw_channels = _detect_no_hw_channels(alsaname)/2;
      if (no_hw_channels > MIN_HW_CHANNELS)
      {
         char *cardname;
         int slen;

         psnd_card_get_name(card_idx, &cardname);
         snprintf(ptr, len, "%s", cardname);
         slen = strlen(ptr)+1;
         len -= slen;
         ptr += slen;
      }
   }
   *ptr = 0;

   return (char *)&names[mode];
}

static char *
_aaxALSADriverGetInterfaces(const void *id, const char *devname, int mode)
{
   static char names[2][256] = { "\0\0", "\0\0" };
   snd_pcm_info_t *info;
   snd_ctl_t *ctl;
   int dev_idx;
   char *ptr;
   int len;

   len = 256;
   ptr = (char *)&names[mode];
   dev_idx = -1;

   info = (snd_pcm_info_t *) alloca(psnd_pcm_info_sizeof());
   memset(info, 0, psnd_pcm_info_sizeof());

   psnd_ctl_open(&ctl, devname, 0);
   while ((psnd_ctl_pcm_next_device(ctl, &dev_idx) == 0) && (dev_idx >=0))
   {
      int subdev_idx, slen;
      int subdev_cnt;

      psnd_pcm_info_set_device(info, dev_idx);
      psnd_pcm_info_set_stream(info, SND_PCM_STREAM_PLAYBACK);

      subdev_idx = 0;
      subdev_cnt = 0;
      psnd_pcm_info_set_subdevice(info, subdev_idx);
      if (psnd_ctl_pcm_info(ctl, info) >= 0) {
            subdev_cnt = psnd_pcm_info_get_subdevices_count(info);
      }

      if (subdev_cnt > MIN_HW_CHANNELS)
      {
         char *ifname = (char *)psnd_pcm_info_get_name(info);
         snprintf(ptr, len, "%s", ifname);
         slen = strlen(ptr)+1;
         len -= slen;
         ptr += slen;
      }
   }
   *ptr = 0;

   return (char *)&names[mode];
}

static int
_aaxALSADriverPlayback(const void *id, void *dst, void *src, float pitch, float volume)
{
   _driver_t *handle = (_driver_t *)id;

   assert(handle);
   if (handle->pause) return 0;

   assert(handle->channel);
   assert(handle->channel[0]);

   return _alsa_playback_fn(id, handle->channel[0], src, pitch, volume);
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

static char*
detect_default_interface(char *device)
{
   unsigned int slen = strlen(device)+4;	/* "<name>,999"*/
   char *name;
   snd_ctl_t *ctl;
   int idx = -1;

   assert(device != 0);

   name = malloc(slen+1);
   if (name)
   {
      psnd_ctl_open(&ctl, device, 0);
      if ((psnd_ctl_pcm_next_device(ctl, &idx) == 0) && (idx >= 0))
      {
         snprintf(name, slen, "%s,%d", device, idx);
      }
      else
      {
         free(name);
         name = (char *)_hw_default_name;
      }
      psnd_ctl_close(ctl);
   }

   return name;
}

static char*
detect_default_device()
{
   char *name = (char *)_hw_default_name;
   int cidx = -1;

   if ((psnd_card_next(&cidx) == 0) && (cidx >= 0))
   {
      unsigned int slen = 10;		/* "hw:999,999" */
      snd_ctl_t *ctl;
      int didx = -1;

      name = malloc(slen+1);
      if (name)
      {
         snprintf(name, slen, "hw:%d", cidx);
         psnd_ctl_open(&ctl, name, 0);

         if ((psnd_ctl_pcm_next_device(ctl, &didx) == 0) && (didx >= 0))
         {
            snprintf(name, slen, "hw:%d,%d", cidx, didx);
         }
         else
         {
            free(name);
            name = (char *)_hw_default_name;
         }
         psnd_ctl_close(ctl);
      }
   }

   return name;
}

static char *
detect_device(const char *name)
{
   char *device_name = (char *)_hw_default_name;

   if (!strncmp(name, "default", 7) || strstr(name, "hw:"))
   {
      unsigned int slen = strlen(name);
      device_name = malloc(slen+1);
      if (device_name)
      {
         strcpy(device_name, name);
         *(device_name+slen) = '\0';
      }
   }
   else
   {
      int card_idx;

      card_idx = -1;
      while ((psnd_card_next(&card_idx) == 0) && (card_idx >= 0))
      {
         char *cardname;

         psnd_card_get_name(card_idx, &cardname);
         if (!strncasecmp(name, cardname, strlen(cardname)))
         {
            unsigned int slen = 6;		/* "hw:999" */
            device_name = calloc(1, slen);
            if (device_name)
            {
               snprintf(device_name, slen, "hw:%d", card_idx);
            }
            break;
         }
      }
   }

   return device_name;
}

static int
xrun_recovery(snd_pcm_t *handle, int err)
{
   int res = psnd_pcm_recover(handle, err, 1);
   if (res != 0) {
      _AAX_SYSLOG("alsa; Unable to recover from xrun situation");
   }
   else if ((err == -EPIPE) &&
            (psnd_pcm_stream(handle) == SND_PCM_STREAM_CAPTURE))
   {
      /* capturing requirs an explicit call to snd_pcm_start */
      res = psnd_pcm_start(handle);
      if (res != 0) {
         _AAX_SYSLOG("alsa; unable to restart input stream");
      }
   }
   return res;
}


static unsigned int
_detect_no_hw_channels(char *alsaname)
{
   unsigned int ret = 0;
   snd_ctl_t *ctl;
   int dev_idx;

   dev_idx = -1;
   psnd_ctl_open(&ctl, alsaname, 0);
   if ((psnd_ctl_pcm_next_device(ctl, &dev_idx) == 0)
          && (dev_idx >=0))
   {
      snd_pcm_info_t *info;
      info = calloc(1, psnd_pcm_info_sizeof());
      if (info)
      {
         int subdev_idx;
         psnd_pcm_info_set_device(info, dev_idx);
         psnd_pcm_info_set_stream(info, SND_PCM_STREAM_PLAYBACK);

         subdev_idx = 0;
         psnd_pcm_info_set_subdevice(info, subdev_idx);
         if (psnd_ctl_pcm_info(ctl, info) >= 0) {
            ret = psnd_pcm_info_get_subdevices_avail(info);
         }
         free(info);
      }
   }

   return ret;
}

#define TRUN(f)	if (err >= 0) { err = f; }
static char 
_setup_channel(_alsa_hw_channel *ch, unsigned int rate, unsigned int bufsize, char channels)
{
   snd_pcm_hw_params_t *params;
   int use_mmap = 0, err = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   err = psnd_pcm_hw_params_malloc(&params);
   if ((err >= 0) && params)
   {
      snd_pcm_t *hid = ch->id;
      snd_pcm_format_t data_format;
      snd_pcm_uframes_t size;
      unsigned int bps = ch->bytes_sample;
      unsigned int periods = ch->periods;
      unsigned int val1, val2;

      if (bufsize && (bufsize > 0)) size = bufsize * 2;
      else size = rate/10;

      /* buffer durations smaller than 20000us will give problems */
      if ((rate/size) > 50) return AAX_FALSE;

      err = psnd_pcm_hw_params_any(hid, params);
      TRUN( psnd_pcm_hw_params_set_rate_resample(hid, params, 0) );

      bps = 2;
      data_format = _alsa_formats[bps];

      TRUN( psnd_pcm_hw_params_set_format(hid, params, data_format) );
      if (err >= 0)
      {
         use_mmap = 2;
         err = psnd_pcm_hw_params_set_access(hid, params,
                                            SND_PCM_ACCESS_MMAP_INTERLEAVED);

         if (err < 0)
         {
            use_mmap = 1;
            err = psnd_pcm_hw_params_set_access(hid, params,
                                               SND_PCM_ACCESS_RW_INTERLEAVED);
         }

         do
         {
            data_format = _alsa_formats[bps];
            err = psnd_pcm_hw_params_set_format(hid, params, data_format);
         }
         while ((err < 0) && (bps-- != 0));
         ch->bytes_sample = bps;
      }

      TRUN( psnd_pcm_hw_params_set_channels(hid, params, channels) );
      TRUN( psnd_pcm_hw_params_set_rate_near(hid, params, &rate, 0) );
      TRUN( psnd_pcm_hw_params_set_buffer_size_near(hid, params, &size) );
      TRUN( psnd_pcm_hw_params_set_periods_near(hid, params, &periods, 0) );
      TRUN( psnd_pcm_hw_params(hid, params) );

      err = psnd_pcm_hw_params_get_rate_numden(params, &val1, &val2);
      if (val1 && val2)
      {
         ch->rate_hz = val1/val2;
      }

      err = psnd_pcm_hw_params_get_buffer_size(params, &size);
      err = psnd_pcm_hw_params_get_periods(params, &periods, 0);
      ch->periods = periods;
      ch->can_pause = psnd_pcm_hw_params_can_pause(params);

      TRUN( psnd_pcm_hw_params(hid, params) );
      TRUN( psnd_pcm_prepare(hid) );

      psnd_pcm_hw_params_free(params);
   }

   psnd_lib_error_set_handler(_alsa_error_handler);

   return (err >= 0) ? use_mmap : 0;
}
#undef TRUN

static int
_aaxALSADriverPlayback_rw(const void *hid, void *cid, void *src, float pitch, float volume)
{
   _driver_t *handle = (_driver_t *)hid;
   _alsa_hw_channel *ch = (_alsa_hw_channel *)cid;
   _oalRingBuffer *rbs = (_oalRingBuffer *)src;
   unsigned int no_frames, no_tracks, offs;
   _oalRingBufferSample *rbsd;
   int16_t *data;
   void *ptr = 0;
   char *p;
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

   if (ch->bytes_sample == 4)
   {
      ptr = _oalRingBufferGetDataInterleavedMalloc(rbs, 1.0f);
      data = (int16_t *)ptr;
      data += no_tracks * offs * ch->bytes_sample/sizeof(int16_t);
   }
   else if (ch->bytes_sample == 2)
   {
      ptr = _aax_malloc(&p, no_tracks * no_frames*2);
      if (ptr == 0) return 0;

      data = (int16_t *)p;
      _batch_cvt16_intl_24(data, (const int32_t**)rbsd->track, 0, no_tracks, no_frames);
   }
   else return 0;

   while (no_frames > 0)
   {
      err = psnd_pcm_writei(ch->id, data, no_frames);

      if (err == -EAGAIN)
         continue;

      if (err < 0)
      {
         if (xrun_recovery(ch->id, err) < 0)
         {
            free(ptr);
            return 0;
         }
         break; /* skip one period */
      }

      data += err * no_tracks;
      no_frames -= err;
   }
   free(ptr);

   return 0;
}

static int
_aaxALSADriverPlayback_mmap(const void *hid, void *cid, void *src, float pitch, float volume)
{
   _driver_t *handle = (_driver_t *)hid;
   _alsa_hw_channel *ch = (_alsa_hw_channel *)cid;
   _oalRingBuffer *rbs = (_oalRingBuffer *)src;
   unsigned int no_tracks, offs;
#if HW_SEQUENCER
   snd_ctl_elem_value_t *vol_ctl = 0;
#endif
   _oalRingBufferSample *rbsd;
   snd_pcm_sframes_t no_frames;
   snd_pcm_sframes_t avail;
   snd_pcm_state_t state;
   unsigned int start = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(handle != 0);
   if (handle->pause) return 0;

   assert(rbs != 0);
   assert(rbs->sample != 0);

   rbsd = rbs->sample;
   offs = _oalRingBufferGetOffsetSamples(rbs);
   no_frames = _oalRingBufferGetNoSamples(rbs) - offs;
   no_tracks = _oalRingBufferGetNoTracks(rbs);

   state = psnd_pcm_state(ch->id);
   if (state != SND_PCM_STATE_RUNNING)
   {
      psnd_pcm_prepare(ch->id);
   }

   avail = psnd_pcm_avail_update(ch->id);
   if (avail < 0)
   {
      int err;
      if ((err = xrun_recovery(ch->id, avail)) < 0)
      {
         char s[255];
         snprintf(s, 255, "PCM avail error: %s\n", psnd_strerror(err));
         _AAX_SYSLOG(s);
         return 0;
      }
   }

#if HW_SEQUENCER
   vol_ctl = calloc(1, psnd_ctl_elem_value_sizeof() );
   if (vol_ctl)
   {
      psnd_hctl_elem_read(handle->elem, vol_ctl);
      psnd_ctl_elem_value_set_integer(vol_ctl, 0, ch->volume[0]);
      psnd_ctl_elem_value_set_integer(vol_ctl, 1, ch->volume[1]);
      psnd_hctl_elem_write(handle->elem, vol_ctl);
      free(vol_ctl);
   }
   else
      _AAX_LOG(LOG_ERR, "Unable to allocate memory for snd_ctl_elem_value");
#endif

   if (avail < no_frames) avail = 0;
   else avail = no_frames;

   while(avail > 0)
   {
      const snd_pcm_channel_area_t *area;
      snd_pcm_uframes_t frames = avail;
      snd_pcm_uframes_t offset;
      snd_pcm_sframes_t result;
      int16_t *ptr;
      char *p;
      int err;

      err = psnd_pcm_mmap_begin(ch->id, &area, &offset, &frames);
      if (err < 0)
      {
         if ((err = xrun_recovery(ch->id, err)) < 0)
         {
            char s[255];
            snprintf(s, 255, "MMAP begin avail error: %s\n",psnd_strerror(err));
            _AAX_SYSLOG(s);
            return 0;
         }
      }

      p = (char *)area->addr + ((area->first + area->step*offset) >> 3);
      ptr = (int16_t *)p;
      _batch_cvt16_intl_24(ptr, (const int32_t**)rbsd->track, start, no_tracks, frames);

      result = psnd_pcm_mmap_commit(ch->id, offset, frames);
      if (result < 0 || (snd_pcm_uframes_t)result != frames)
      {
         if (xrun_recovery(ch->id, result >= 0 ? -EPIPE : result) < 0) {
            return 0;
         }
      }
      start += result;
      avail -= result;
   }

   /* start playing when 2 frames have been filled */
   if (state != SND_PCM_STATE_RUNNING)
   {
      if (ch->playing == 0)
         ch->playing = 1;
      else
         psnd_pcm_start(ch->id);
   }

   return 0;
}

#if 0
static char *
_aaxALSASoftDriverGetHardwareDevices(const void *id, int mode)
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
      devname = detect_hardware_devname(cardname, devnum, 2, __mode[m]);
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
            _hw_default_name = devname;
            _hw_default_devnum = devnum;
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
_aaxALSASoftDriverGetHardwareInterfaces(const void *id, const char *name, int mode)
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

static int detect_hardware_ifnum(char *, const char *, int);

static char *
detect_harware_devname(const char *devname, int devnum, unsigned int tracks, int m)
{
   static const char* dev_prefix[] = {
         "plughw:", "hw:", "surround40:", "surround51:", "surround71:"
   };
   char *rv = (char*)_hw_default_name;

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

            ifnum = detect_hardware_ifnum(name, ptr, m);
            snprintf(name, len, "%s%i,%i", dev_prefix[tracks], devnum, ifnum);
         }
      }
   }
   return rv;
}

static int
detect_hardware_devnum(const char *name, int m)
{
   int devnum = _hw_default_devnum;
   char *ptr = NULL;

   if ( !name ) {
      devnum = _hw_default_devnum;
   }
   else if ( !strncmp(name, "hw:", strlen("hw:"))
             || (ptr = strstr(name, "(hw:")) != NULL )
   {
      if (!ptr) ptr = (char *)name;
      else ptr++;

      devnum = atoi(ptr+3);
   }
   else if ( !strncmp(name, "surround:", strlen("surround:")) )
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
         devname = detect_hardware_devname(cardname, devnum, 2, __mode[m]);
         err = psnd_pcm_open(&id, devname, __mode[m], SND_PCM_NONBLOCK);
         free(devname);
         if (err >= 0)
         {
            psnd_pcm_close(id);
            devnum = card_idx;
            break;
         }
      }
   }   else
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
detect_hardware_ifnum(char *devname, const char *name, int m)
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
get_hardware_devices_avail(int m)
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

      devnum = detect_hardware_devnum(cardname, m);
      devname = detect_hardware_devname(cardname, devnum, 2, __mode[m]);
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
#endif
