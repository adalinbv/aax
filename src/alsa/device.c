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
#include <stdio.h>
#include <errno.h>
#if HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdarg.h>	/* va_start */

#include <xml.h>

#include <api.h>
#include <arch.h>
#include <devices.h>
#include <ringbuffer.h>
#include <base/timer.h>		/* for msecSleep */
#include <base/dlsym.h>
#include <base/logging.h>
#include <base/threads.h>

#include "alsa/audio.h"
#include "device.h"
#include "driver.h"

#define ENABLE_TIMING		AAX_FALSE
#define MAX_ID_STRLEN		32

#define DEFAULT_DEVNUM		0
#define DEFAULT_IFNUM		0
#define DEFAULT_OUTPUT_RATE	48000
#define DEFAULT_DEVNAME_OLD     "front:"AAX_MKSTR(DEFAULT_DEVNUM) \
                                       ","AAX_MKSTR(DEFAULT_IFNUM)
#define DEFAULT_DEVNAME		"default"
#define DEFAULT_HWDEVNAME	"hw:0"
#define DEFAULT_RENDERER	"ALSA"

#define ALSA_TIE_FUNCTION(a)	if ((TIE_FUNCTION(a)) == 0) printf("%s\n", #a);

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
static _aaxDriverThread _aaxALSADriverThread;
static _aaxDriver3dMixerCB _aaxALSADriver3dMixer;
static _aaxDriverState _aaxALSADriverState;
static _aaxDriverParam _aaxALSADriverParam;
static _aaxDriverLog _aaxALSADriverLog;

static char _alsa_id_str[MAX_ID_STRLEN+1] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxALSADriverBackend =
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

   (_aaxDriverDetect *)&_aaxALSADriverDetect,
   (_aaxDriverNewHandle *)&_aaxALSADriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxALSADriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxALSADriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxALSADriverGetName,
   (_aaxDriverThread *)&_aaxALSADriverThread,

   (_aaxDriverConnect *)&_aaxALSADriverConnect,
   (_aaxDriverDisconnect *)&_aaxALSADriverDisconnect,
   (_aaxDriverSetup *)&_aaxALSADriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxALSADriverCapture,
   (_aaxDriverCallback *)&_aaxALSADriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxFileDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxALSADriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxFileDriver3dPrepare,
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
    int devnum;

    _aaxDriverCallback *play;
    snd_mixer_t *mixer;
    snd_pcm_t *pcm;

    float latency;
    float pitch;       /* difference between requested freq and returned freq */
    float frequency_hz;
    float volume;

    float padding;		/* for sensor clock drift correction   */
    unsigned int threshold;	/* sensor buffer threshold for padding */

    long initVolume, minVolume, maxVolume;
    unsigned int no_channels;
    unsigned int no_periods;
    unsigned int period_frames;
    int mode;

    char pause;
    char can_pause;
    char use_mmap;
    char bytes_sample;
    char interleaved;
    char hw_channels;
    char playing;
    char shared;
    char sse_level;

#ifndef NDEBUG
   unsigned int buf_len;
#endif

    _batch_cvt_to_proc cvt_to;
    _batch_cvt_from_proc cvt_from;
    _batch_cvt_to_intl_proc cvt_to_intl;
    _batch_cvt_from_intl_proc cvt_from_intl;

    void **scratch;
    char **data;

    char *ifname[2];
    _oalRingBufferMix1NFunc *mix_mono3d;

} _driver_t;


DECL_FUNCTION(snd_pcm_open);
DECL_FUNCTION(snd_pcm_close);
DECL_FUNCTION(snd_pcm_wait);
DECL_FUNCTION(snd_pcm_nonblock);
DECL_FUNCTION(snd_pcm_prepare);
DECL_FUNCTION(snd_pcm_pause);
DECL_FUNCTION(snd_pcm_hw_params_can_pause);
DECL_FUNCTION(snd_pcm_hw_params_can_resume);
DECL_FUNCTION(snd_asoundlib_version);
DECL_FUNCTION(snd_device_name_hint);
DECL_FUNCTION(snd_device_name_get_hint);
DECL_FUNCTION(snd_device_name_free_hint);
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
DECL_FUNCTION(snd_pcm_hw_params_set_period_size_near);
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
DECL_FUNCTION(snd_pcm_dump);
DECL_FUNCTION(snd_output_stdio_attach);
DECL_FUNCTION(snd_mixer_open);
DECL_FUNCTION(snd_mixer_close);
DECL_FUNCTION(snd_mixer_attach);
DECL_FUNCTION(snd_mixer_load);
DECL_FUNCTION(snd_mixer_selem_register);
DECL_FUNCTION(snd_mixer_first_elem);
DECL_FUNCTION(snd_mixer_elem_next);
DECL_FUNCTION(snd_mixer_selem_has_playback_volume);
DECL_FUNCTION(snd_mixer_selem_get_playback_volume);
DECL_FUNCTION(snd_mixer_selem_get_playback_volume_range);
DECL_FUNCTION(snd_mixer_selem_set_playback_volume_range);
DECL_FUNCTION(snd_mixer_selem_set_playback_volume_all);
DECL_FUNCTION(snd_mixer_selem_has_capture_volume);
DECL_FUNCTION(snd_mixer_selem_get_capture_volume);
DECL_FUNCTION(snd_mixer_selem_get_capture_volume_range);
DECL_FUNCTION(snd_mixer_selem_set_capture_volume_range);
DECL_FUNCTION(snd_mixer_selem_set_capture_volume_all);
DECL_FUNCTION(snd_mixer_selem_get_id);
DECL_FUNCTION(snd_mixer_selem_id_get_name);
#if 0
// UNUSED
DECL_FUNCTION(snd_pcm_resume);
DECL_FUNCTION(snd_pcm_drain);
DECL_FUNCTION(snd_pcm_drop); 
DECL_FUNCTION(snd_pcm_info_malloc);
DECL_FUNCTION(snd_pcm_info_sizeof);
DECL_FUNCTION(snd_pcm_info_get_name);
DECL_FUNCTION(snd_pcm_info_get_subdevices_count);
DECL_FUNCTION(snd_pcm_info_set_subdevice);
DECL_FUNCTION(snd_pcm_info_set_device);
DECL_FUNCTION(snd_pcm_info_set_stream);
DECL_FUNCTION(snd_pcm_info_free);
DECL_FUNCTION(snd_pcm_hw_params_set_period_time_near);
DECL_FUNCTION(snd_pcm_hw_params_set_buffer_time_near);
DECL_FUNCTION(snd_pcm_hw_params_get_channels);
DECL_FUNCTION(snd_pcm_mmap_readi);
DECL_FUNCTION(snd_pcm_mmap_readn);
DECL_FUNCTION(snd_pcm_format_width);
DECL_FUNCTION(snd_pcm_avail);
DECL_FUNCTION(snd_pcm_hw_params_get_buffer_size);
DECL_FUNCTION(snd_pcm_hw_params_get_periods);
DECL_FUNCTION(snd_pcm_hw_params_get_period_size);
DECL_FUNCTION(snd_pcm_frames_to_bytes);
DECL_FUNCTION(snd_pcm_type);
DECL_FUNCTION(snd_pcm_rewind);
DECL_FUNCTION(snd_pcm_forward);
#endif

typedef struct {
   char bps;
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
static int detect_devnum(const char *, int);
static char *detect_devname(const char*, int, unsigned int, int, char);
static char *_aaxALSADriverLogVar(const char *, ...);
static void _alsa_error_handler(const char *, int, const char *, int, const char *,...);
static int _alsa_get_volume(_driver_t*);
static int _alsa_set_volume(_driver_t*, const int32_t**, int, snd_pcm_sframes_t, unsigned int, float);
static _aaxDriverCallback _aaxALSADriverPlayback_mmap_ni;
static _aaxDriverCallback _aaxALSADriverPlayback_mmap_il;
static _aaxDriverCallback _aaxALSADriverPlayback_rw_ni;
static _aaxDriverCallback _aaxALSADriverPlayback_rw_il;


#define MAX_FORMATS		6
#define _AAX_DRVLOG(a)		_aaxALSADriverLog(a)

static const char* _alsa_type[2];
static const snd_pcm_stream_t _alsa_mode[2];
static const char *_const_alsa_default_name[2];
static const _alsa_formats_t _alsa_formats[MAX_FORMATS];

char *_alsa_default_name[2] = { NULL, NULL };
int _default_devnum = DEFAULT_DEVNUM;

static int
_aaxALSADriverDetect(int mode)
{
   int m = (mode > 0) ? 1 : 0;
   static void *audio = NULL;
   static int rv = AAX_FALSE;
   char *error = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   if (_alsa_default_name[m] == NULL) {
      _alsa_default_name[m] = (char*)_const_alsa_default_name[m];
   }

   if TEST_FOR_FALSE(rv) {
#ifndef USE_SALSA
      audio = _oalIsLibraryPresent("asound", "2");
#else
      audio = _oalIsLibraryPresent("salse", "0");
#endif
   }

   if (audio)
   {
      const char *hwstr = _aaxGetSIMDSupportString();

      snprintf(_alsa_id_str, MAX_ID_STRLEN, "%s %s", DEFAULT_RENDERER, hwstr);
      _oalGetSymError(0);

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
         TIE_FUNCTION(snd_pcm_recover);					//
//       TIE_FUNCTION(snd_pcm_dump);
         TIE_FUNCTION(snd_mixer_open);					//
         TIE_FUNCTION(snd_mixer_close);					//
         TIE_FUNCTION(snd_mixer_attach);				//
         TIE_FUNCTION(snd_mixer_load);					//
         TIE_FUNCTION(snd_mixer_selem_register);			//
         TIE_FUNCTION(snd_mixer_first_elem);				//
         TIE_FUNCTION(snd_mixer_elem_next);				//
         TIE_FUNCTION(snd_mixer_selem_has_playback_volume);		//
         TIE_FUNCTION(snd_mixer_selem_get_playback_volume);		//
         TIE_FUNCTION(snd_mixer_selem_get_playback_volume_range);	//
         TIE_FUNCTION(snd_mixer_selem_set_playback_volume_range);	//
         TIE_FUNCTION(snd_mixer_selem_set_playback_volume_all);		//
         TIE_FUNCTION(snd_mixer_selem_has_capture_volume);		//
         TIE_FUNCTION(snd_mixer_selem_get_capture_volume);		//
         TIE_FUNCTION(snd_mixer_selem_get_capture_volume_range);	//
         TIE_FUNCTION(snd_mixer_selem_set_capture_volume_range);	//
         TIE_FUNCTION(snd_mixer_selem_set_capture_volume_all);		//
         TIE_FUNCTION(snd_mixer_selem_get_id);				//
         TIE_FUNCTION(snd_mixer_selem_id_get_name);			//
#ifndef USE_SALSA
         TIE_FUNCTION(snd_pcm_nonblock);				//
         TIE_FUNCTION(snd_pcm_prepare);					//
         TIE_FUNCTION(snd_pcm_pause);					//
         TIE_FUNCTION(snd_pcm_hw_params_can_pause);			//
         TIE_FUNCTION(snd_pcm_hw_params_can_resume);			//
         TIE_FUNCTION(snd_device_name_hint);				//
         TIE_FUNCTION(snd_device_name_get_hint);			//
         TIE_FUNCTION(snd_device_name_free_hint);			//
         TIE_FUNCTION(snd_pcm_hw_params_sizeof);			//
         TIE_FUNCTION(snd_pcm_hw_params_set_access);			//
         TIE_FUNCTION(snd_pcm_hw_params_set_format);			//
         TIE_FUNCTION(snd_pcm_hw_params_set_rate_resample);		//
         TIE_FUNCTION(snd_pcm_hw_params_set_rate_near);			//
         TIE_FUNCTION(snd_pcm_hw_params_test_channels);			//
         TIE_FUNCTION(snd_pcm_hw_params_set_channels);			//
         TIE_FUNCTION(snd_pcm_hw_params_set_periods_near);		//
         TIE_FUNCTION(snd_pcm_hw_params_set_period_size_near);		//
         TIE_FUNCTION(snd_pcm_hw_params_set_buffer_size_near);		//
         TIE_FUNCTION(snd_pcm_sw_params_sizeof);			//
         TIE_FUNCTION(snd_pcm_sw_params_current);			//
         TIE_FUNCTION(snd_pcm_sw_params_set_avail_min);			//
         TIE_FUNCTION(snd_pcm_sw_params_set_start_threshold);		//
         TIE_FUNCTION(snd_pcm_hw_params_can_mmap_sample_resolution);	//
         TIE_FUNCTION(snd_pcm_hw_params_get_rate_numden);		//
         TIE_FUNCTION(snd_pcm_state);					//
         TIE_FUNCTION(snd_pcm_start);					//
         TIE_FUNCTION(snd_pcm_delay);					//
         TIE_FUNCTION(snd_pcm_stream);					//
         TIE_FUNCTION(snd_strerror);					//
         TIE_FUNCTION(snd_lib_error_set_handler);			//
         TIE_FUNCTION(snd_asoundlib_version);				//
//       TIE_FUNCTION(snd_output_stdio_attach);
#if 0
// UNUSED?
         TIE_FUNCTION(snd_pcm_resume);
         TIE_FUNCTION(snd_pcm_drain);
         TIE_FUNCTION(snd_pcm_drop); 
         TIE_FUNCTION(snd_pcm_info_malloc);
         TIE_FUNCTION(snd_pcm_info_sizeof);
         TIE_FUNCTION(snd_pcm_info_get_name);
         TIE_FUNCTION(snd_pcm_info_get_subdevices_count);
         TIE_FUNCTION(snd_pcm_info_set_subdevice);
         TIE_FUNCTION(snd_pcm_info_set_device);
         TIE_FUNCTION(snd_pcm_info_set_stream);
         TIE_FUNCTION(snd_pcm_info_free);
         TIE_FUNCTION(snd_pcm_hw_params_set_period_time_near);
         TIE_FUNCTION(snd_pcm_hw_params_set_buffer_time_near);
         TIE_FUNCTION(snd_pcm_hw_params_get_channels);
         TIE_FUNCTION(snd_pcm_mmap_readi);
         TIE_FUNCTION(snd_pcm_mmap_readn);
         TIE_FUNCTION(snd_pcm_format_width);
         TIE_FUNCTION(snd_pcm_avail);
         TIE_FUNCTION(snd_pcm_hw_params_get_buffer_size);
         TIE_FUNCTION(snd_pcm_hw_params_get_periods);
         TIE_FUNCTION(snd_pcm_hw_params_get_period_size);
         TIE_FUNCTION(snd_pcm_frames_to_bytes);
         TIE_FUNCTION(snd_pcm_type);
         TIE_FUNCTION(snd_pcm_rewind);
         TIE_FUNCTION(snd_pcm_forward);
#endif
#endif
      }

      error = _oalGetSymError(0);
      if (!error)
      {
         if (get_devices_avail(mode) != 0) {
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
      handle->play = _aaxALSADriverPlayback_rw_il;
      handle->sse_level = _aaxGetSSELevel();
      handle->pause = 0;
      handle->use_mmap = 1;
      handle->interleaved = 0;
      handle->hw_channels = 2;

      handle->frequency_hz = (float)_aaxALSADriverBackend.rate;
      handle->no_channels = _aaxALSADriverBackend.tracks;
      handle->bytes_sample = 2;
      handle->no_periods = (mode) ? PLAYBACK_PERIODS : CAPTURE_PERIODS;

      handle->mode = (mode > 0) ? 1 : 0;
      handle->mix_mono3d = _oalRingBufferMixMonoGetRenderer(mode);
   }

   return handle;
}

static void *
_aaxALSADriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      handle = _aaxALSADriverNewHandle(mode);
   }

   if (handle)
   {
      const char *hwstr = _aaxGetSIMDSupportString();
      snprintf(_alsa_id_str, MAX_ID_STRLEN, "%s %s %s",
                             DEFAULT_RENDERER, psnd_asoundlib_version(), hwstr);

      handle->name = _aax_strdup((char*)renderer);

      if (xid)
      {
         float f;
         char *s;
         int i;

         if (!handle->name)
         {
            s = xmlNodeGetString(xid, "renderer");
            if (s && strcmp(s, "default")) {
               handle->name = _aax_strdup(s);
            }
            else
            {
               free(s); /* 'default' */
               handle->name = (char *)_alsa_default_name[mode > 0 ? 1 : 0];
            }
         }

         if (xmlNodeGetBool(xid, "virtual-mixer") ||
             xmlNodeGetBool(xid, "shared")) {
            handle->shared = AAX_TRUE;
         }

         f = (float)xmlNodeGetDouble(xid, "frequency-hz");
         if (f)
         {
            if (f < (float)_AAX_MIN_MIXER_FREQUENCY)
            {
               _AAX_DRVLOG("alsa; frequency too small.");
               f = (float)_AAX_MIN_MIXER_FREQUENCY;
            }
            else if (f > (float)_AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_DRVLOG("alsa; frequency too large.");
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
                  _AAX_DRVLOG("alsa; no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_DRVLOG("alsa; no. tracks too great.");
                  i = _AAX_MAX_SPEAKERS;
               }
               handle->no_channels = i;
            }
         }

         i = xmlNodeGetInt(xid, "bits-per-sample");
         if (i)
         {
            i /= 8;
            if (i < 2 || i > 4)
            {
               _AAX_DRVLOG("alsa; unsupported bits-per-sample");
               i = 2;
            }
            handle->bytes_sample = i;
         }

         i = xmlNodeGetInt(xid, "periods");
         if (i)
         {
            if (i < 1)
            {
               _AAX_DRVLOG("alsa; no periods too small.");
               i = 1;
            }
            else if (i > 16)
            {
               _AAX_DRVLOG("alsa; no. tracks too great.");
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
      handle->devnum = detect_devnum(handle->name, m);

      handle->devname = detect_devname(handle->name, handle->devnum,
                                       handle->no_channels, m, handle->shared);
      err = psnd_pcm_open(&handle->pcm, handle->devname, _alsa_mode[m],
                          SND_PCM_NONBLOCK);
      if (err >= 0)
      {
         err = psnd_pcm_nonblock(handle->pcm, 1);
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
            if (err >= 0) {
               err = psnd_mixer_load(handle->mixer);
            }
            if (err >= 0) {
               _alsa_get_volume(handle);
            }
            else
            {
               psnd_mixer_close(handle->mixer);
               handle->mixer = NULL;
            }
         }
         else {
            psnd_pcm_close(handle->pcm);
            handle->pcm = NULL;
         }
      }

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

      free(handle->ifname[0]);
      free(handle->ifname[1]);
      if (handle->devname)
      {
         if (handle->devname != _alsa_default_name[m]) {
            free(handle->devname);
         }
         handle->devname = 0;
      }
      if (handle->name)
      {
         if (handle->name != _alsa_default_name[m]) {
            free(handle->name);
         }
         handle->name = 0;
      }

      if (handle->mixer)
      {
         _alsa_set_volume(handle, NULL, 0, 0, 0, handle->initVolume);
         psnd_mixer_close(handle->mixer);
      }
      if (handle->pcm) {
         psnd_pcm_close(handle->pcm);
      }
      free(handle->scratch);
      handle->scratch = 0;
      handle->pcm = 0;
      free(handle);
      handle = 0;

      if (_alsa_default_name[m] != _const_alsa_default_name[m])
      {
         free(_alsa_default_name[m]);
         _alsa_default_name[m] = (char*)_const_alsa_default_name[m];
         _default_devnum = DEFAULT_DEVNUM;
      }

      return AAX_TRUE;
   }

   return AAX_FALSE;
}


#ifndef NDEBUG
# define TRUN(f, s)	if (err >= 0) { err = f; if (err < 0) { _AAX_DRVLOG("alsa; "s); printf("ALSA error: %s (%i) at line %i\n", s, err, __LINE__); } }
#else
# define TRUN(f, s)	if (err >= 0) { err = f; if (err < 0) _AAX_DRVLOG("alsa; "s); }
#endif

static int
_aaxALSADriverSetup(const void *id, size_t *frames, int *fmt,
                        unsigned int *tracks, float *speed)
{
   _driver_t *handle = (_driver_t *)id;
   snd_pcm_hw_params_t *hwparams;
   snd_pcm_sw_params_t *swparams;
   unsigned int channels, rate;
   int err = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(handle != 0);

   rate = (unsigned int)*speed;

   channels = *tracks;
   if (channels > handle->no_channels) {
      channels = handle->no_channels;
   }
 
   if (handle->no_channels != channels)
   {
      int m = handle->mode;

      err = psnd_pcm_nonblock(handle->pcm, 0);
      err = psnd_pcm_close(handle->pcm);

      handle->no_channels = channels;
      handle->devnum = detect_devnum(handle->name, m);
      handle->devname = detect_devname(handle->name, handle->devnum,
                                    handle->no_channels, m, handle->shared);
      err = psnd_pcm_open(&handle->pcm, handle->devname, _alsa_mode[m],
                          SND_PCM_NONBLOCK);
      if (err >= 0) {
         err = psnd_pcm_nonblock(handle->pcm, 1);
      }
   }

   hwparams = calloc(1, psnd_pcm_hw_params_sizeof());
   swparams = calloc(1, psnd_pcm_sw_params_sizeof());
   if (hwparams && swparams)
   {
      unsigned int bps = handle->bytes_sample;
      unsigned int periods = handle->no_periods;
      unsigned int val1, val2, period_fact;
      snd_pcm_t *hid = handle->pcm;
      snd_pcm_format_t data_format;
      snd_pcm_uframes_t no_frames;
      unsigned int bytes;

      err = psnd_pcm_hw_params_any(hid, hwparams);
      TRUN( psnd_pcm_hw_params_set_rate_resample(hid, hwparams, 0),
            "unable to disable sample rate conversion" );

#if 0
      /* for testing purposes */
      if (err >= 0)
      {
         handle->interleaved = 1;
         handle->use_mmap = 0;
         err = psnd_pcm_hw_params_set_access(hid, hwparams,
                                         SND_PCM_ACCESS_RW_INTERLEAVED);
         if (err < 0) _AAX_DRVLOG("alsa; unable to set interleaved mode");
      } else
#endif
      if (err >= 0)			/* playback */
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

            if (err < 0) _AAX_DRVLOG("alsa; unable to find a proper renderer");
         }
#if 0
         TRUN( psnd_pcm_hw_params_can_mmap_sample_resolution(hwparams),
               "unable to determine if mmap is supported" );
         handle->use_mmap = (err == 1);
#endif
      }

      /* test for supported sample formats*/
      bps = 0;
      if (err >= 0)
      {
         do
         {
            data_format = _alsa_formats[bps].format;
            err = psnd_pcm_hw_params_set_format(hid, hwparams, data_format);
         }
         while ((err < 0) && (_alsa_formats[++bps].bps != 0));

         if ((err >= 0) && (_alsa_formats[bps].bps > 0))
         {
            switch (bps)
            {
            case 0:				/* SND_PCM_FORMAT_S16_LE */
               handle->cvt_to = _batch_cvt16_24;
               handle->cvt_from = _batch_cvt24_16;
               handle->cvt_to_intl = _batch_cvt16_intl_24;
               handle->cvt_from_intl = _batch_cvt24_16_intl;
               break;
            case 1:				/* SND_PCM_FORMAT_S32_LE */
               handle->cvt_to = _batch_cvt32_24;
               handle->cvt_from = _batch_cvt24_32;
               handle->cvt_to_intl = _batch_cvt32_intl_24;
               handle->cvt_from_intl = _batch_cvt24_32_intl;
               break;
            case 2:				/* SND_PCM_FORMAT_S24_LE */
               handle->cvt_to = (_batch_cvt_to_proc)_batch_cvt24_24;
               handle->cvt_from = (_batch_cvt_from_proc)_batch_cvt24_24;
               handle->cvt_to_intl = _batch_cvt24_intl_24;
               handle->cvt_from_intl = _batch_cvt24_24_intl;
               break;
            case 3:				/* SND_PCM_FORMAT_S24_3LE */
               handle->cvt_to = _batch_cvt24_3_24;
               handle->cvt_from = _batch_cvt24_24_3;
               handle->cvt_to_intl = _batch_cvt24_3intl_24;
               handle->cvt_from_intl = _batch_cvt24_24_3intl;
               break;
            case 4:				/* SND_PCM_FORMAT_U8 */
               handle->cvt_to = _batch_cvt8_24;
               handle->cvt_from = _batch_cvt24_8;
               handle->cvt_to_intl = _batch_cvt8_intl_24;
               handle->cvt_from_intl = _batch_cvt24_8_intl;
               break;
            default:
               _AAX_DRVLOG("alsa; error: hardware format mismatch!\n");
               err = -EINVAL;
               break;
            }
            handle->bytes_sample = _alsa_formats[bps].bps;
            bps = handle->bytes_sample;
         }
         else {
            _AAX_DRVLOG("alsa; unable to match hardware format");
         }
      }

      if (err >= 0)
      {
         int err = psnd_pcm_hw_params_test_channels(hid, hwparams, channels);
         int tracks = channels;
         if (err < 0)
         {
            do
            {
               if (channels > 2) {
                  channels -= 2;
               } else {
                  channels--;
               }
               if (channels == 0) break;

               err = psnd_pcm_hw_params_test_channels(hid, hwparams, channels);
            }
            while (err < 0);
         }

         if (channels != tracks)
         {
            _aaxALSADriverLogVar("Unable to output to %i speakers"
                                 " (%i is the maximum)", tracks, channels);
         }
      }

      TRUN( psnd_pcm_hw_params_set_channels(hid, hwparams, channels),
            "unsupported no. channels" );
      if (channels > handle->no_channels) handle->no_channels = channels;
      handle->hw_channels = channels;
      *tracks = channels;

      handle->pitch = (float)rate;
      TRUN( psnd_pcm_hw_params_set_rate_near(hid, hwparams, &rate, 0),
            "unsupported sample rate" );
      handle->pitch = rate/handle->pitch;

      TRUN( psnd_pcm_hw_params_set_periods_near(hid, hwparams, &periods, 0),
            "unsupported no. periods" );
      period_fact = handle->no_periods/periods;
      if (err >= 0) {
         handle->no_periods = periods;
      }

      if (frames && (*frames > 0))
      {
         no_frames = *frames;
         if (!handle->mode) no_frames *= period_fact;
      } else {
         no_frames = rate/25;
      }

      bytes = (unsigned int)ceilf(no_frames*channels*bps*handle->pitch);
      if (bytes & 0xF)
      {
         bytes |= 0xF;
         bytes++;
      }
      if (bytes < 16) bytes = 16;
      no_frames = bytes/(channels*bps);

      /* Set buffer size (in frames). The resulting latency is given by */
      /* latency = periodsize * periods / (rate * bytes_per_frame))     */
      no_frames *= periods;
      TRUN( psnd_pcm_hw_params_set_buffer_size_near(hid, hwparams, &no_frames),
            "invalid buffer size" );

      no_frames /= periods;
      TRUN( psnd_pcm_hw_params_set_period_size_near(hid, hwparams, &no_frames, 0), "invalid period size" );

      if (!handle->mode) no_frames = (no_frames/period_fact);
      handle->period_frames = no_frames;
      *frames = no_frames;

      val1 = val2 = 0;
      err = psnd_pcm_hw_params_get_rate_numden(hwparams, &val1, &val2);
      if (val1 && val2)
      {
         handle->frequency_hz = (float)val1/(float)val2;
         rate = (unsigned int)handle->frequency_hz;
      }
      *speed = handle->frequency_hz = (float)rate;
//    handle->latency = (float)no_frames*(float)periods/(float)rate;
      handle->latency = (float)no_frames/(float)rate;

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
         snprintf(str,255, "  buffer size: %u bytes", (unsigned int)*frames*channels*bps);
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
// printf("\tformat: %X", *fmt);
#endif
      } while (0);


      handle->can_pause = psnd_pcm_hw_params_can_pause(hwparams);
      handle->can_pause &= psnd_pcm_hw_params_can_resume(hwparams);

      if (handle->use_mmap && !handle->interleaved) {
         handle->play = _aaxALSADriverPlayback_mmap_ni;
      } else if (handle->use_mmap && handle->interleaved) {
         handle->play = _aaxALSADriverPlayback_mmap_il;
      } else if (!handle->use_mmap && !handle->interleaved) {
         handle->play = _aaxALSADriverPlayback_rw_ni;
      } else {
         handle->play = _aaxALSADriverPlayback_rw_il;
      }
      TRUN( psnd_pcm_hw_params(hid, hwparams), "unable to configure hardware" );
      if (err == -EBUSY) {
         _AAX_DRVLOG("alsa; device busy\n");
      }

      TRUN( psnd_pcm_sw_params_current(hid, swparams), 
            "unable to set software config" );
      if (handle->mode == 0)	/* record */
      {
         snd_pcm_uframes_t period_frames = handle->period_frames;
//       int dir;
//       TRUN( psnd_pcm_hw_params_get_period_size(hwparams, &period_frames, &dir),
//             "unable to get period size" );
//       handle->period_frames = period_frames;

         TRUN( psnd_pcm_sw_params_set_start_threshold(hid, swparams,0x7fffffff),
               "improper interrupt treshold" );
         TRUN( psnd_pcm_sw_params_set_avail_min(hid, swparams, period_frames),
               "wakeup treshold unsupported" );
      }
      else			/* playback */
      {
         snd_pcm_uframes_t period_frames = handle->period_frames;
//       int dir;
//       TRUN( psnd_pcm_hw_params_get_period_size(hwparams, &period_frames, &dir),
//             "unable to get period size" );
//       handle->period_frames = period_frames;

         TRUN( psnd_pcm_sw_params_set_start_threshold(hid, swparams,
                                       (no_frames/period_frames)*period_frames),
               "improper interrupt treshold" );
         TRUN( psnd_pcm_sw_params_set_avail_min(hid, swparams, period_frames),
               "wakeup treshold unsupported" );
      }
      handle->threshold = 5*handle->period_frames/4;

      TRUN( psnd_pcm_sw_params(hid, swparams),
            "unable to configure software" );
   }

   if (swparams) free(swparams);
   if (hwparams) free(hwparams);

   psnd_lib_error_set_handler(_alsa_error_handler);

   return (err >= 0) ? AAX_TRUE : AAX_FALSE;
}
#undef TRUN

int
_aaxALSADriver3dMixer(const void *id, void *d, void *s, void *p, void *m, int n, unsigned char ctr, unsigned int nbuf)
{
   _driver_t *handle = (_driver_t *)id;
   float gain;
   int ret;

   assert(s);
   assert(d);
   assert(p);

   gain = _aaxALSADriverBackend.gain;
   ret = handle->mix_mono3d(d, s, p, m, gain, n, ctr, nbuf);

   return ret;
}

static int
_aaxALSADriverCapture(const void *id, void **data, int offs, size_t *req_frames, void *scratch, size_t scratchlen, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int tracks, no_frames;
   unsigned int frame_size;
   snd_pcm_sframes_t avail;
   snd_pcm_state_t state;
   int res, rv = AAX_FALSE;

   if ((handle->mode != 0) || (req_frames == 0) || (data == 0))
   {
      if (handle->mode == 0) {
         _AAX_DRVLOG("alsa; calling the record function with a playback handle");
      } else if (req_frames == 0) {
         _AAX_DRVLOG("alsa; record buffer size is zero bytes");
      } else {
         _AAX_DRVLOG("alsa; calling the record function with null pointer");
      }
      return rv;
   }

   state = psnd_pcm_state(handle->pcm);
   if (state != SND_PCM_STATE_RUNNING)
   {
      if (state <= SND_PCM_STATE_PREPARED)
      {
         if (handle->playing == 0)
         {
            psnd_pcm_prepare(handle->pcm);
            psnd_pcm_start(handle->pcm);
         }
      }
      else if (state == SND_PCM_STATE_XRUN) {
//       _AAX_DRVLOG("alsa (record): state = SND_PCM_STATE_XRUN.");
         xrun_recovery(handle->pcm, -EPIPE);
      }
   }

   tracks = handle->hw_channels;
   frame_size = tracks * handle->bytes_sample;
   no_frames = *req_frames;
#ifndef NDEBUG
   handle->buf_len = no_frames * frame_size;
#endif

   /* synchronise capture and playback for registered sensors */
   avail = 0;
   res = psnd_pcm_delay(handle->pcm, &avail);
   if (res < 0)
   {
      if ((res = xrun_recovery(handle->pcm, res)) < 0)
      {
         _aaxALSADriverLogVar("PCM avail error: %s\n", psnd_strerror(res));
         avail = -1;
       }
   }

   *req_frames = 0;
   if (no_frames && avail)
   {
      unsigned int corr, fetch = no_frames;
      unsigned int chunk, try = 0;
      snd_pcm_uframes_t size;
      float diff;

      /* try to keep the buffer padding at the threshold level at all times */
      diff = (float)avail-(float)handle->threshold;
      handle->padding = (handle->padding + diff/(float)no_frames)/2;
      corr = _MINMAX(roundf(handle->padding), -1, 1);
      fetch += corr;
      offs -= corr;
#if 0
if (corr)
printf("avail: %4i (%4i), fetch: %6i\r", avail, handle->threshold, fetch);
#endif
      /* try to keep the buffer padding at the threshold level at all times */

      chunk = 10;
      size = fetch; // no_frames;
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
                  _aaxALSADriverLogVar("MMAP begin error: %s\n",
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
                  handle->cvt_from_intl((int32_t**)data, p, offs, tracks, res);
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
                  unsigned char *s;
                  s = (((unsigned char *)area[0].addr) + (area[0].first/8));
                  s += mmap_offs*handle->bytes_sample;
                  handle->cvt_from((int32_t*)data[0]+offs, s, res);
                  if (tracks == 2)
                  {
                     s = (((unsigned char *)area[1].addr) + (area[1].first/8));
                     s += mmap_offs*handle->bytes_sample;
                     handle->cvt_from((int32_t*)data[1]+offs, s, res);
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
                  handle->cvt_from_intl((int32_t**)data, scratch, offs, tracks, res);
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
                  handle->cvt_from((int32_t*)data[0]+offs, s[0], res);
                  if (tracks == 2) {
                     handle->cvt_from((int32_t*)data[1]+offs, s[1], res);
                  }
               }
            }
         }

         if (gain < 0.99f || gain > 1.01f) {
            _alsa_set_volume(handle, (const int32_t**)data, offs, res, tracks, gain);
         }

         if (tracks == 1) {	// copy the left channel to the right channel
            _aax_memcpy((int32_t*)data[1]+offs, (int32_t*)data[0]+offs,
                        res*sizeof(int32_t));
         }

         if (res < 0)
         {
            if (xrun_recovery(handle->pcm, res) < 0)
            {
               _AAX_DRVLOG("alsa; unable to run xrun_recovery");
               rv = AAX_FALSE;
               break;
            }
            if (try++ > 2)
            {
               _AAX_DRVLOG("alsa; unable to recover from pcm read error");
               rv = AAX_FALSE;
               break;
            }
//          _AAX_DRVLOG("alsa; warning: pcm read error");
            continue;
         }

         size -= res;
         offs += res;
      }
      while((size > 0) && --chunk);
      if (!chunk) _AAX_DRVLOG("alsa; too many capture tries\n");
      *req_frames = offs;
   }
   else rv = AAX_TRUE;

   return rv;
}

static char *
_aaxALSADriverGetName(const void *id, int playback)
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
      rv = -ENOSYS;
      if (handle && psnd_pcm_state(handle->pcm) == SND_PCM_STATE_RUNNING &&
          !handle->pause)
      {
         handle->pause = 1;
         if (handle->can_pause) {
            rv = psnd_pcm_pause(handle->pcm, 1);
         }
      }
      break;
   case DRIVER_RESUME:
      if (handle && psnd_pcm_state(handle->pcm) == SND_PCM_STATE_PAUSED &&
          handle->pause)
      {
         handle->pause = 0;
         if (handle->can_pause) {
            rv = psnd_pcm_pause(handle->pcm, 0);
         }
      }
      break;
   case DRIVER_SHARED_MIXER:
      rv = handle->shared;
      break;
   case DRIVER_SUPPORTS_PLAYBACK:
   case DRIVER_SUPPORTS_CAPTURE:
      rv = AAX_TRUE;
      break;
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
      case DRIVER_LATENCY:
         rv = handle->latency;
         break;
      case DRIVER_MIN_VOLUME:
         rv = (float)handle->minVolume/(float)handle->maxVolume;
         break;
      case DRIVER_MAX_VOLUME:
         rv = 1.0f;
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
      void **hints;
      int res;

      t_previous[m] = t_now;

      res = psnd_device_name_hint(-1, "pcm", &hints);
      if (!res && hints)
      {
         void **lst = hints;
         int len = 1024;
         char *ptr;

         ptr = (char *)&names[m];
         do
         {
            char *type = psnd_device_name_get_hint(*lst, "IOID");
            if (!type || (type && !strcmp(type, _alsa_type[m])))
            {
               char *name = psnd_device_name_get_hint(*lst, "NAME");
               if (name && !strncmp(name, "front:", strlen("front:")))
               {
                  snd_pcm_t *id;
                  if (!psnd_pcm_open(&id, name, _alsa_mode[m], SND_PCM_NONBLOCK))
                  {
                     char *desc = psnd_device_name_get_hint(*lst, "DESC");
                     char *iface;
                     int slen;

                     psnd_pcm_close(id);
                     if (!desc) desc = name;

                     iface = strstr(desc, ", ");
                     if (iface) *iface = 0;

                     snprintf(ptr, len, "%s", desc);
                     slen = strlen(ptr)+1;	/* skip the trailing 0 */
                     if (slen > (len-1)) break;

                     len -= slen;
                     ptr += slen;

                     if (desc != name) _aax_free(desc);
                  }
               }
               _aax_free(name);
            }
            _aax_free(type);
            ++lst;
         }
         while (*lst != NULL);
         *ptr = 0;
      }

      res = psnd_device_name_free_hint(hints);
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
      char name[1024] = "\0\0";
      int len = 1024;
      void **hints;
      int res;

      res = psnd_device_name_hint(-1, "pcm", &hints);
      if (!res && hints)
      {
         void **lst = hints;
         char *ptr;

         ptr = name;
         do
         {
            char *type = psnd_device_name_get_hint(*lst, "IOID");
            if (!type || (type && !strcmp(type, _alsa_type[m])))
            {
               char *name = psnd_device_name_get_hint(*lst, "NAME");
               if (name && (!strncmp(name, "front:", strlen("front:"))
                         || !strncmp(name, "center_lfe:", strlen("center_lfe:"))
                         || !strncmp(name, "rear:", strlen("rear:"))
                         || !strncmp(name, "side:", strlen("side:"))
                         || !strncmp(name, "iec958:", strlen("iec958:"))))
               {
                  if (m || (!strncmp(name, "front:", strlen("front:")) ||
                            !strncmp(name, "iec958:", strlen("iec958:"))))
                  {
                     char *desc = psnd_device_name_get_hint(*lst, "DESC");
                     char *iface;

                     if (!desc) desc = name;
                     iface = strstr(desc, ", ");

                     if (iface) *iface = 0;
                     if (iface && !strcmp(devname, desc))
                     {
                        snd_pcm_t *id;
                        if (!psnd_pcm_open(&id, name, _alsa_mode[m],
                                           SND_PCM_NONBLOCK))
                        {
                           int slen;

                           psnd_pcm_close(id);
                           if (m || strncmp(name, "front:", strlen("front:")))
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
                     }
                     if (desc != name) _aax_free(desc);
                  }
               }
               _aax_free(name);
            }
            _aax_free(type);
            ++lst;
         }
         while (*lst != NULL);

         if (ptr != name)
         {
            *ptr++ = '\0';
            rv = handle->ifname[m] = malloc(ptr-name);
            if (rv) {
               memcpy(handle->ifname[m], name, ptr-name);
            }
         }
      }
      res = psnd_device_name_free_hint(hints);
   }

   return rv;
}

static char *
_aaxALSADriverLogVar(const char *fmt, ...)
{
   char _errstr[1024];
   va_list ap;

   _errstr[0] = '\0';
   va_start(ap, fmt);
   vsnprintf(_errstr, 1024, fmt, ap);

   // Whatever happen in vsnprintf, what i'll do is just to null terminate it
   _errstr[1023] = '\0';
   va_end(ap);

   return _aaxALSADriverLog(_errstr);
}

static char *
_aaxALSADriverLog(const char *str)
{
   static char _errstr[256];
   int len = _MIN(strlen(str)+1, 256);

   memcpy(_errstr, str, len);
   _errstr[255] = '\0';  /* always null terminated */

   __aaxErrorSet(AAX_BACKEND_ERROR, (char*)&_errstr);
   _AAX_SYSLOG(_errstr);

   return (char*)&_errstr;
}

/*-------------------------------------------------------------------------- */

static const _alsa_formats_t _alsa_formats[MAX_FORMATS] =
{
   {2, SND_PCM_FORMAT_S16_LE},
   {4, SND_PCM_FORMAT_S32_LE},
   {4, SND_PCM_FORMAT_S24_LE},
   {3, SND_PCM_FORMAT_S24_3LE},
   {1, SND_PCM_FORMAT_U8},
   {0, 0}
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

static void
_alsa_error_handler(const char *file, int line, const char *function, int err,
                    const char *fmt, ...)
{
   char s[1024];
   va_list ap;

#ifndef NDEBUG
   snprintf((char *)&s, 1024, "%s at line %i in function %s:", file, line, function);
   _AAX_LOG(LOG_ERR, s);
#endif

   va_start(ap, fmt);
   snprintf((char *)&s, 1024, fmt, va_arg(ap, char *));
   va_end(ap);

   _AAX_DRVLOG(s);
}

static char *
detect_devname(const char *devname, int devnum, unsigned int tracks, int m, char vmix)
{
   static const char* dev_prefix[] = {
         "hw:", "front:", "surround40:", "surround51:", "surround71:"
   };
   char *rv = (char*)_alsa_default_name[m];

   if (devname && (tracks <= _AAX_MAX_SPEAKERS))
   {
      void **hints;
      int res;

      tracks /= 2;
      res = psnd_device_name_hint(-1, "pcm", &hints);
      if (!res && hints)
      {
         void **lst = hints;
         char *ptr;
         int len;

         ptr = strstr(devname, ": ");
         if (ptr) len = ptr-devname;
         else len = strlen(devname);

         do
         {
            char *type = psnd_device_name_get_hint(*lst, "IOID");
            if (!type || (type && !strcmp(type, _alsa_type[m])))
            {
               char *name = psnd_device_name_get_hint(*lst, "NAME");
               if (name && !strncmp(name, "front:", strlen("front:")))
               {
                  if (!strcmp(devname, name))
                  {
                     int dlen = strlen(name)+1;
                     if (vmix)
                     {
                         dlen -= strlen("front:");
                         dlen += strlen("plug:''");
                         if (m) dlen += strlen("dmix:");
                         else dlen += strlen("dsnoop:");
                     }
                     rv = malloc(dlen);
                     if (rv)
                     {
                        if (vmix)
                        {
                            char *ptr =  name+strlen("front:");
                            snprintf(rv, dlen, "plug:'%s%s'",
                                         m ? "dmix:" : "dsnoop:", ptr);
                        }
                        else {
                            snprintf(rv, dlen, "%s", name);
                        }
                     } else {
                        rv = _aax_strdup(name);
                     }
                     break;
                  }
                  else if ((tracks <= 4)
                            || (strncmp(name, dev_prefix[m ? tracks : 0],
                                    strlen(dev_prefix[m ? tracks : 0])) == 0))
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

                     if (!strncmp(devname, desc, len))
                     {
                        char *devptr = strstr(devname, ": ");
                        if (devptr)
                        {
                           devptr += 2;
                           if (!strcmp(devptr, iface) ||
                               (description && !strcmp(devptr, description)))
                           {
                              int dlen = strlen(name)-strlen("front:")+1;
                              if (vmix)
                              {
                                 dlen += strlen("plug:''");
                                 if (m) dlen += strlen("dmix:");
                                 else dlen += strlen("dsnoop:");
                              }
                              else dlen += strlen(dev_prefix[m ? tracks : 0]);
                              rv = malloc(dlen);
                              if (rv)
                              {
                                 char *ptr = name+strlen("front:");
                                 if (vmix)
                                 {
                                    snprintf(rv, dlen, "plug:'%s%s'",
                                                 m ? "dmix:" : "dsnoop:", ptr);
                                 }
                                 else if (ptr)
                                 {
                                    snprintf(rv, dlen, "%s%s",
                                               dev_prefix[m ? tracks : 0], ptr);
                                 } else {
                                    snprintf(rv, dlen, "%s", name);
                                 }
                              } else {
                                 rv = _aax_strdup(name);
                              }
                              break;
                           }
                        }
                        else
                        {
                           int dlen = strlen(name)-strlen("front:")+1;
                           if (vmix)
                           {
                              dlen += strlen("plug:''");
                              if (m) dlen += strlen("dmix:");
                              else dlen += strlen("dsnoop:");
                           }
                           else dlen += strlen(dev_prefix[m ? tracks : 0]);
                           rv = malloc(dlen);
                           if (rv)
                           {
                              char *ptr = name+strlen("front:");

                              if (vmix)
                              {
                                 snprintf(rv, dlen, "plug:'%s%s'",
                                                 m ? "dmix:" : "dsnoop:", ptr);
                              }
                              else if (ptr)
                              {
                                    snprintf(rv, dlen, "%s%s",
                                               dev_prefix[m ? tracks : 0], ptr);
                              } else {
                                 snprintf(rv, dlen, "%s", name);
                              }
                           } else {
                              rv = _aax_strdup(name);
                           }
                           break;
                        }
                     }
                     if (desc != name) _aax_free(desc);
                  }
                  _aax_free(name);
               }
            }
            _aax_free(type);
            ++lst;
         }
         while (*lst != NULL);
      }

      res = psnd_device_name_free_hint(hints);
   }

   return rv;
}


static int
detect_devnum(const char *devname, int m)
{
   int devnum = _default_devnum;

   if (devname)
   {
      void **hints;
      int res = psnd_device_name_hint(-1, "pcm", &hints);

      if (!res && hints)
      {
         void **lst = hints;
         int len, ctr = 0;
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
                  if (!strcmp(devname, name))
                  {
                     _aax_free(name);
                     devnum = ctr;
                     break;
                  }

                  if (!strncmp(name, "front:", strlen("front:")))
                  {
                     if (!strcmp(devname, "default"))
                     {
                        _aax_free(name);
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
   
                        if (!strncmp(devname, desc, len))
                        {
                           _aax_free(desc);
                           _aax_free(name);
                           devnum = ctr;
                           break;
                        }
                        ctr++;
                     }
                  }
                  _aax_free(name);
               }
            }
            _aax_free(type);
            ++lst;
         }
         while (*lst != NULL);
      }

      res = psnd_device_name_free_hint(hints);
   }

   return devnum;
}

unsigned int
get_devices_avail(int m)
{
   unsigned int rv = 0;
   void **hints;
   int res;

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
            if (name && !strncmp(name, "front:", strlen("front:"))) rv++;
         }
         _aax_free(type);
         ++lst;
      }
      while (*lst != NULL);

      res = psnd_device_name_free_hint(hints);
   }

   return rv;
}

static int
_alsa_get_volume(_driver_t *handle)
{
   int rv = 0;
   if (handle && handle->mixer)
   {
      snd_mixer_selem_id_t *sid = calloc(1,4096);
      snd_mixer_elem_t *elem;

      for (elem = psnd_mixer_first_elem(handle->mixer); elem;
           elem = psnd_mixer_elem_next(elem))
      {
         const char *name;

         psnd_mixer_selem_get_id(elem, sid);
         name = psnd_mixer_selem_id_get_name(sid);

         if (handle->mode == AAX_MODE_READ)
         {
            if (!strcmp(name, "Capture"))
            {
               if (psnd_mixer_selem_has_capture_volume(elem))
               {
                  psnd_mixer_selem_get_capture_volume_range(elem,
                                                            &handle->minVolume,
                                                            &handle->maxVolume);
                  rv = psnd_mixer_selem_get_capture_volume(elem,
                                                          SND_MIXER_SCHN_MONO,
                                                          &handle->initVolume);
               }
            }
         }
         else
         {
            if (!strcmp(name, "Front"))
            {
               if (psnd_mixer_selem_has_playback_volume(elem))
               {
                  psnd_mixer_selem_get_playback_volume_range(elem, 
                                                            &handle->minVolume,                                                             &handle->maxVolume);
                  rv = psnd_mixer_selem_get_playback_volume(elem,
                                                           SND_MIXER_SCHN_MONO,
                                                           &handle->initVolume);
               }
            }
         }
      }
      free(sid);
   }
   return rv;
}

static int
_alsa_set_volume(_driver_t *handle, const int32_t **sbuf, int offset, snd_pcm_sframes_t no_frames, unsigned int no_tracks, float gain)
{
   int rv = 0;
   if (handle && handle->mixer && handle->shared &&
       ((gain <= 1.0f) && (fabs(handle->volume - gain) > 4e-3f))
      )
   {
      snd_mixer_selem_id_t *sid = calloc(1,4096);
      snd_mixer_elem_t *elem;

      handle->volume = gain;
      for (elem = psnd_mixer_first_elem(handle->mixer); elem;
           elem = psnd_mixer_elem_next(elem))
      {
         const char *name;

         psnd_mixer_selem_get_id(elem, sid);
         name = psnd_mixer_selem_id_get_name(sid);

         if (handle->mode == AAX_MODE_READ)
         {
            if (!strcmp(name, "Capture"))
            {
               if (psnd_mixer_selem_has_capture_volume(elem)) {
                  psnd_mixer_selem_set_capture_volume_all(elem,
                                                        handle->maxVolume*gain);
               }
            }
         }
         else
         {
            if (!strcmp(name, "Front") || !strcmp(name, "Surround") ||
                !strcmp(name, "Center") || !strcmp(name, "LFE") ||
                !strcmp(name, "Side") || !strcmp(name, "Rear"))
            {
               if (psnd_mixer_selem_has_playback_volume(elem)) {
                  psnd_mixer_selem_set_playback_volume_all(elem,
                                                        handle->maxVolume*gain);
               }
            }
         }	             
      }
      free(sid);
   }
   else if (sbuf && no_frames && no_tracks)
   {
      if (gain < 0.99f || gain > 1.01f)
      {
         int t;
         for (t=0; t<no_tracks; t++) {
            _batch_mul_value((void*)(sbuf[t]+offset), sizeof(int32_t),
                             no_frames, gain);
         }
      }
   }

   return rv;
}

static int
_xrun_recovery(snd_pcm_t *handle, int err)
{
   int res = psnd_pcm_recover(handle, err, 1);
#if 0
   snd_output_t *output = NULL;

   psnd_output_stdio_attach(&output, stdout, 0);
   psnd_pcm_dump(handle, output);
#endif
   if (res != 0) {
      _AAX_DRVLOG("alsa; Unable to recover from xrun situation");
   }
   else if (err == -EPIPE)
   {
      if (psnd_pcm_stream(handle) == SND_PCM_STREAM_CAPTURE)
      {
         /* capturing requirs an explicit call to snd_pcm_start */
         res = psnd_pcm_start(handle);
         if (res != 0) {
            _AAX_DRVLOG("alsa; unable to restart input stream");
         }
      }
      else
      {
         res = psnd_pcm_prepare(handle);
         if (res != 0) {
            _AAX_DRVLOG("alsa; unable to restart output stream");
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


static int
_aaxALSADriverPlayback_mmap_ni(const void *id, void *src, float pitch, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   _oalRingBuffer *rbs = (_oalRingBuffer *)src;
   unsigned int no_tracks, offs, t, chunk;
   _oalRingBufferSample *rbsd;
   snd_pcm_sframes_t no_frames;
   snd_pcm_sframes_t avail;
   snd_pcm_state_t state;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(handle != 0);
   if (handle->pause) return 0;

   assert(rbs != 0);
   assert(rbs->sample != 0);

   rbsd = rbs->sample;
   offs = _oalRingBufferGetOffsetSamples(rbs);
   no_frames = _oalRingBufferGetNoSamples(rbs) - offs;
   no_tracks = _oalRingBufferGetNoTracks(rbs);

   state = psnd_pcm_state(handle->pcm);
   if (state != SND_PCM_STATE_RUNNING)
   {
      if (state == SND_PCM_STATE_PREPARED)
      {
         if (handle->playing++ < 1) {
            psnd_pcm_prepare(handle->pcm);
         } else {
            psnd_pcm_start(handle->pcm);
         }
      }
      else if (state == SND_PCM_STATE_XRUN) {
         _AAX_DRVLOG("alsa (mmap_ni): state = SND_PCM_STATE_XRUN.");
         xrun_recovery(handle->pcm, -EPIPE);
      }
   }

   avail = psnd_pcm_avail_update(handle->pcm);
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

   if (avail < no_frames) avail = 0;
   else avail = no_frames;

   chunk = 10;
   do
   {
      const snd_pcm_channel_area_t *area;
      snd_pcm_uframes_t frames = avail;
      snd_pcm_uframes_t mmap_offs;
      snd_pcm_sframes_t res;
      const int32_t **sbuf;
      int err;

      err = psnd_pcm_mmap_begin(handle->pcm, &area, &mmap_offs, &frames);
      if (err < 0)
      {
         if ((err = xrun_recovery(handle->pcm, err)) < 0)
         {
            char s[255];
            snprintf(s, 255, "MMAP begin avail error: %s\n",psnd_strerror(err));
            _AAX_DRVLOG(s);
            return 0;
         }
      }

      sbuf = (const int32_t**)rbsd->track;
      if (gain < 0.99f) {
         _alsa_set_volume(handle, sbuf, offs, frames, no_tracks, gain);
      }

      for (t=0; t<no_tracks; t++)
      {
         unsigned char *p;
         p = (((unsigned char *)area[t].addr) + (area[t].first/8));
         p += mmap_offs*handle->bytes_sample;
         handle->cvt_to(p, sbuf[t]+offs, frames);
      }

      res = psnd_pcm_mmap_commit(handle->pcm, mmap_offs, frames);
      if (res < 0 || (snd_pcm_uframes_t)res != frames)
      {
         if (xrun_recovery(handle->pcm, res >= 0 ? -EPIPE : res) < 0) {
            return 0;
         }
      }
      offs += res;
      avail -= res;
   }
   while ((avail > 0) && --chunk);
   if (!chunk) _AAX_DRVLOG("alsa; too many playback tries\n");

   return 0;
}


static int
_aaxALSADriverPlayback_mmap_il(const void *id, void *src, float pitch, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   _oalRingBuffer *rbs = (_oalRingBuffer *)src;
   unsigned int no_tracks, offs, chunk;
   _oalRingBufferSample *rbsd;
   snd_pcm_sframes_t no_frames;
   snd_pcm_sframes_t avail;
   snd_pcm_state_t state;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(handle != 0);

   assert(rbs != 0);
   assert(rbs->sample != 0);

   rbsd = rbs->sample;
   offs = _oalRingBufferGetOffsetSamples(rbs);
   no_frames = _oalRingBufferGetNoSamples(rbs) - offs;
   no_tracks = _oalRingBufferGetNoTracks(rbs);

   state = psnd_pcm_state(handle->pcm);
   if (state != SND_PCM_STATE_RUNNING)
   {
      if (state == SND_PCM_STATE_PREPARED)
      {
         if (handle->playing++ < 1) {
            psnd_pcm_prepare(handle->pcm);
         } else {
            psnd_pcm_start(handle->pcm);
         }
      }
      else if (state == SND_PCM_STATE_XRUN) {
         _AAX_DRVLOG("alsa (mmap_il): state = SND_PCM_STATE_XRUN.");
         xrun_recovery(handle->pcm, -EPIPE);
      }
   }

   avail = psnd_pcm_avail_update(handle->pcm);
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
   
   if (avail < no_frames) avail = 0;
   else avail = no_frames;

   chunk = 10;
   do
   {
      const snd_pcm_channel_area_t *area;
      snd_pcm_uframes_t frames = avail;
      snd_pcm_uframes_t mmap_offs;
      snd_pcm_sframes_t res;
      const int32_t **sbuf;
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
            return 0;
         }
      }

      sbuf = (const int32_t**)rbsd->track;
      if (gain < 0.99f) {
         _alsa_set_volume(handle, sbuf, offs, frames, no_tracks, gain);
      }

      p = (char *)area->addr + ((area->first + area->step*mmap_offs) >> 3);
      handle->cvt_to_intl(p, sbuf, offs, no_tracks, frames);

      res = psnd_pcm_mmap_commit(handle->pcm, mmap_offs, frames);
      if (res < 0 || (snd_pcm_uframes_t)res != frames)
      {
         if (xrun_recovery(handle->pcm, res >= 0 ? -EPIPE : res) < 0) {
            return 0;
         }
      }
      offs += res;
      avail -= res;
   }
   while ((avail > 0) && --chunk);
   if (!chunk) _AAX_DRVLOG("alsa; too many playback tries\n");

   return 0;
}


static int
_aaxALSADriverPlayback_rw_ni(const void *id, void *src, float pitch, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   _oalRingBuffer *rbs = (_oalRingBuffer *)src;
   unsigned int no_samples, no_tracks, chunk;
   unsigned int offs, t, hw_bps;
   _oalRingBufferSample *rbsd;
   const int32_t **sbuf;
   char **data;
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
   hw_bps = handle->bytes_sample;

   if (handle->scratch == 0)
   {
      unsigned int samples, outbuf_size, size;
      int16_t *ptr;
      char *p;

      samples = _oalRingBufferGetNoSamples(rbs);
      outbuf_size = samples * hw_bps;
      if (outbuf_size & 0xF)
      {
         outbuf_size|= 0xF;
         outbuf_size++;
      }

      size = no_tracks * (2*sizeof(void*));
      size += no_tracks * outbuf_size;

      p = (char *)(no_tracks * 2*sizeof(void*));
      handle->scratch = (void**)_aax_malloc(&p, size);
      handle->data = (char**)handle->scratch + no_tracks;

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

#ifndef NDEBUG
      assert(no_samples*hw_bps <= handle->buf_len);
#endif

   sbuf = (const int32_t**)rbsd->track;
   if (gain < 0.99f) {
      _alsa_set_volume(handle, sbuf, offs, no_samples, no_tracks, gain);
   }
  
   data = handle->data;
   for (t=0; t<no_tracks; t++)
   {
      data[t] = handle->scratch[t];
      handle->cvt_to(data[t], sbuf[t]+offs, no_samples);
   }

   chunk = 10;
   do
   {
      do {
         err = psnd_pcm_writen(handle->pcm, (void**)data, no_samples);
      }
      while (err == -EAGAIN);

      if (err < 0)
      {
         if (xrun_recovery(handle->pcm, err) < 0) {
            return 0;
         }
         break; /* skip one period */
      }

      for (t=0; t<no_tracks; t++) {
         data[t] += err;
      }
      no_samples -= err;
   }
   while ((no_samples > 0) && --chunk);
   if (!chunk) _AAX_DRVLOG("alsa; too many playback tries\n");

   return 0;
}


static int
_aaxALSADriverPlayback(const void *id, void *src, float pitch, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   return handle->play(id, src, pitch, gain);
}

static int
_aaxALSADriverPlayback_rw_il(const void *id, void *src, float pitch, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   _oalRingBuffer *rbs = (_oalRingBuffer *)src;
   unsigned int no_samples, no_tracks, offs, hw_bps;
   unsigned int outbuf_size, chunk;
   _oalRingBufferSample *rbsd;
   const int32_t **sbuf;
   char *data;
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
   hw_bps = handle->bytes_sample;

   outbuf_size = no_tracks * no_samples*hw_bps;
   if (handle->scratch == 0)
   {
      char *p = 0;
      handle->scratch = (void**)_aax_malloc(&p, outbuf_size);
      handle->data = (char**)p;
#ifndef NDEBUG
      handle->buf_len = outbuf_size;
#endif
   }

#if 0
   assert(outbuf_size <= handle->buf_len);
#endif

   sbuf = (const int32_t**)rbsd->track;
   if (gain < 0.99f) {
      _alsa_set_volume(handle, sbuf, offs, no_samples, no_tracks, gain);
   }

   data = (char*)handle->data;
   handle->cvt_to_intl(data, sbuf, offs, no_tracks, no_samples);

   chunk = 10;
   do
   {
      int try = 0;

      do {
         err = psnd_pcm_writei(handle->pcm, data, no_samples);
      }
      while (err == -EAGAIN);

      if (err < 0)
      {
         if (xrun_recovery(handle->pcm, err) < 0)
         {
            _AAX_DRVLOG("alsa; unable to run xrun_recovery");
            return 0;
         }
         if (try++ > 2) 
         {
            _AAX_DRVLOG("alsa; unable to recover from pcm write error");
            break;
         }
//       _AAX_DRVLOG("alsa; warning: pcm write error");
         continue;
      }

      data += err * no_tracks;
      no_samples -= err;
   }
   while ((no_samples > 0) && --chunk);
   if (!chunk) _AAX_DRVLOG("alsa; too many playback tries\n");

   return 0;
}


void *
_aaxALSADriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
#if ENABLE_TIMING
   _aaxTimer *timer = _aaxTimerCreate();
#endif
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

   delay_sec = 1.0f/handle->info->refresh_rate;

   be = handle->backend.ptr;
   dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      _sensor_t* sensor = _intBufGetDataPtr(dptr_sensor);
//    _oalRingBuffer *nrb;

      mixer = sensor->mixer;

      dest_rb = _oalRingBufferCreate(REVERB_EFFECTS_TIME);
      if (dest_rb)
      {
         _oalRingBufferSetFormat(dest_rb, be->codecs, AAX_PCM24S);
         _oalRingBufferSetNoTracks(dest_rb, mixer->info->no_tracks);
         _oalRingBufferSetFrequency(dest_rb, mixer->info->frequency);
         _oalRingBufferSetDuration(dest_rb, delay_sec);
         _oalRingBufferInit(dest_rb, AAX_TRUE);
         _oalRingBufferStart(dest_rb);

         handle->ringbuffer = dest_rb;
//       nrb = _oalRingBufferDuplicate(dest_rb, AAX_FALSE, AAX_FALSE);
//       _intBufAddData(mixer->ringbuffers, _AAX_RINGBUFFER, nrb);
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

   stdby_time = 2*(int)(delay_sec*1000);
   _aaxMutexLock(handle->thread.mutex);
   while TEST_FOR_TRUE(handle->thread.started)
   {
      _driver_t *be_handle = (_driver_t *)handle->backend.handle;
      int err;

      _aaxMutexUnLock(handle->thread.mutex);

      if (_IS_PLAYING(handle) && be->state(be_handle, DRIVER_AVAILABLE))
      {
				/* timeout is in ms */
         if ((err = psnd_pcm_wait(be_handle->pcm, stdby_time)) < 0)
         {
            _AAX_DRVLOG("alsa; snd_pcm_wait polling error");
//          _aaxMutexLock(handle->thread.mutex);
//          continue;
         }
      }
      else {
         msecSleep((unsigned int)(delay_sec*1000));
      }

      _aaxMutexLock(handle->thread.mutex);
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
      if (_IS_PLAYING(handle) && be->state(be_handle, DRIVER_AVAILABLE)) {
         _aaxSoftwareMixerThreadUpdate(handle, dest_rb);
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
   _aaxMutexUnLock(handle->thread.mutex);

   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      _oalRingBufferStop(handle->ringbuffer);
      _oalRingBufferDelete(handle->ringbuffer);
      handle->ringbuffer = NULL;
   }
   return handle;
}
