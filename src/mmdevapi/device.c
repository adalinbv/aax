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

#include <assert.h>

#include <aax.h>
#include <xml.h>


#if 0
#include <driver.h>
#include <devices.h>
#endif
#include <api.h>
#include <arch.h>
#include <ringbuffer.h>
#include <base/dlsym.h>
#include <base/logging.h>
#include <base/types.h>

#include "audio.h"
#include "device.h"

#define MAX_ID_STRLEN		32
#define DEFAULT_RENDERER	"MMDevice"
#define DEFAULT_DEVNAME		"default"
#define MMDEV_ID_STRING		"MM Device API"

static _aaxDriverDetect _aaxMMDevDriverDetect;
static _aaxDriverGetDevices _aaxMMDevDriverGetDevices;
static _aaxDriverGetInterfaces _aaxMMDevDriverGetInterfaces;
static _aaxDriverConnect _aaxMMDevDriverConnect;
static _aaxDriverDisconnect _aaxMMDevDriverDisconnect;
static _aaxDriverSetup _aaxMMDevDriverSetup;
static _aaxDriverState _aaxMMDevDriverPause;
static _aaxDriverState _aaxMMDevDriverResume;
static _aaxDriverCaptureCallback _aaxMMDevDriverCapture;
static _aaxDriverCallback _aaxMMDevDriverPlayback;
static _aaxDriverGetName _aaxMMDevDriverGetName;
static _aaxDriverState _aaxMMDevDriverIsAvailable;
static _aaxDriverState _aaxMMDevDriverAvailable;

static char _mmdev_id_str[MAX_ID_STRLEN+1] = DEFAULT_RENDERER;
static char _mmdev_default_renderer[100] = DEFAULT_RENDERER;

const _aaxDriverBackend _aaxMMDevDriverBackend =
{
   1.0,
   AAX_PCM16S,
   48000,
   2,

   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_mmdev_default_renderer,

   (_aaxCodec **)&_oalRingBufferCodecs,

   (_aaxDriverDetect *)&_aaxMMDevDriverDetect,
   (_aaxDriverGetDevices *)&_aaxMMDevDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxMMDevDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxMMDevDriverGetName,
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,

   (_aaxDriverConnect *)&_aaxMMDevDriverConnect,
   (_aaxDriverDisconnect *)&_aaxMMDevDriverDisconnect,
   (_aaxDriverSetup *)&_aaxMMDevDriverSetup,
   (_aaxDriverState *)&_aaxMMDevDriverPause,
   (_aaxDriverState *)&_aaxMMDevDriverResume,
   (_aaxDriverCaptureCallback *)&_aaxMMDevDriverCapture,
   (_aaxDriverCallback *)&_aaxMMDevDriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxSoftwareDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxSoftwareDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)*_aaxMMDevDriverAvailable,
   (_aaxDriverState *)*_aaxMMDevDriverAvailable,
   (_aaxDriverState *)*_aaxMMDevDriverIsAvailable
};

typedef struct
{
   char *name;

   IMMDeviceEnumerator *pEnumerator;
   IMMDevice *pDevice;

   IAudioClient *pAudioClient;
   IAudioRenderClient *pRenderClient;

   float frequency_hz;
   unsigned int format;
   unsigned int no_tracks;
   unsigned int buffer_size;

   EDataFlow mode;
   int exclusive;
   char sse_level;
   char bytes_sample;

   int16_t *ptr, *scratch;
#ifndef NDEBUG
   unsigned int buf_len;
#endif

} _driver_t;


DECL_FUNCTION(CoCreateInstance);
DECL_FUNCTION(IMMDeviceEnumerator_Release);
DECL_FUNCTION(IMMDeviceEnumerator_GetDefaultEndPoint);
DECL_FUNCTION(IMMDeviceActivate);
DECL_FUNCTION(IAudioClient_Start);
DECL_FUNCTION(IAudioClient_Stop);
DECL_FUNCTION(IAudioClient_GetService);
DECL_FUNCTION(IAudioClient_Initialize);
DECL_FUNCTION(IAudioClient_GetMixFormat);
DECL_FUNCTION(IAudioClient_GetBufefrSize);
DECL_FUNCTION(IAudioClient_GetCurrentPadding);
DECL_FUNCTION(IAudioRenderClient_GetBuffer);
DECL_FUNCTION(IAudioRenderClient_ReleaseBuffer);

static IAudioClient* detect_audioclient(const char *);


char *_mmdev_default_name = DEFAULT_DEVNAME;


static int
_aaxMMDevDriverDetect(int mode)
{
   static int rv = AAX_FALSE;
   void *audio = NULL;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   if TEST_FOR_FALSE(rv) {
     audio = _oalIsLibraryPresent("mmdevapi", 0);
   }

   if (audio)
   {
      const char *hwstr = _aaxGetSIMDSupportString();
      char *error;

      snprintf(_mmdev_id_str, MAX_ID_STRLEN, "%s %s %s",
               DEFAULT_RENDERER, MMDEV_ID_STRING, hwstr);

      _oalGetSymError(0);

      TIE_FUNCTION(CoCreateInstance);
      if (pCoCreateInstance)
      {
         TIE_FUNCTION(IMMDeviceEnumerator_Release);
         TIE_FUNCTION(IMMDeviceEnumerator_GetDefaultEndPoint);
         TIE_FUNCTION(IMMDeviceActivate);
         TIE_FUNCTION(IAudioClient_Start);
         TIE_FUNCTION(IAudioClient_Stop);
         TIE_FUNCTION(IAudioClient_GetService);
         TIE_FUNCTION(IAudioClient_Initialize);
         TIE_FUNCTION(IAudioClient_GetMixFormat);
         TIE_FUNCTION(IAudioClient_GetBufefrSize);
         TIE_FUNCTION(IAudioClient_GetCurrentPadding);
         TIE_FUNCTION(IAudioRenderClient_GetBuffer);
         TIE_FUNCTION(IAudioRenderClient_ReleaseBuffer);

         error = _oalGetSymError(0);
         if (!error) {
            rv = AAX_TRUE;
         }
      }
#if 0
      IMMDeviceEnumerator *pEnumerator;
      HRESULT hr;
     
      hr = pCoCreateInstance(&CLSID_MMDeviceEnumerator, NULL,
                             CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator,
                             (void**)&pEnumerator);
      if (!FAILED(hr))
      {
         pIMMDeviceEnumerator_Release(pEnumerator);
         pEnumerator = NULL;
         rv = AAX_TRUE;
      }
#endif
   }

   return rv;
}

static void *
_aaxMMDevDriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle)
   {
      handle = (_driver_t *)calloc(1, sizeof(_driver_t));
      if (!handle) return 0;

      handle->sse_level = _aaxGetSSELevel();
      handle->frequency_hz = _aaxMMDevDriverBackend.rate;
      handle->no_tracks = _aaxMMDevDriverBackend.tracks;

      if (xid)
      {
         char *s;
         int i;

         if (!handle->name)
         {
            s = xmlNodeGetString(xid, "renderer");
            if (s) handle->pAudioClient = detect_audioclient(s);
         }

         i = xmlNodeGetInt(xid, "frequency-hz");
         if (i)
         {
            if (i < _AAX_MIN_MIXER_FREQUENCY)
            {
               _AAX_SYSLOG("mmdev; frequency too small.");
               i = _AAX_MIN_MIXER_FREQUENCY;
            }
            else if (i > _AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_SYSLOG("mmdev; frequency too large.");
               i = _AAX_MAX_MIXER_FREQUENCY;
            }
            handle->frequency_hz = i;
         }

         if (mode != AAX_MODE_READ)
         {
            i = xmlNodeGetInt(xid, "channels");
            if (i)
            {
               if (i < 1)
               {
                  _AAX_SYSLOG("mmdev; no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_SYSLOG("mmdev; no. tracks too great.");
                  i = _AAX_MAX_SPEAKERS;
               }
               handle->no_tracks = i;
            }
         }

         i = xmlNodeGetInt(xid, "bits-per-sample");
         if (i)
         {
            if (i != 16)
            {
               _AAX_SYSLOG("mmdev; unsopported bits-per-sample");
               i = 16;
            }
         }
      }

      if (renderer)
      {
         handle->pAudioClient = detect_audioclient(renderer);
      }
#if 0
printf("frequency-hz: %f\n", handle->frequency_hz);
printf("channels: %i\n", handle->no_tracks);
#endif
   }


   if (handle)
   {
      const EDataFlow _mode[] = { eCapture, eRender };
      int m;

      m = (mode > 0) ? 1 : 0;
      handle->mode = _mode[m];





   }

   _oalRingBufferMixMonoSetRenderer(mode);

   return (void *)handle;
}

static int
_aaxMMDevDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;

   if (handle)
   {
      if (handle->name)
      {
         if (handle->name != _mmdev_default_name) {
            free(handle->name);
         }
         handle->name = 0;
      }

      free(handle->ptr);
      free(handle);

      return AAX_TRUE;
   }
   return AAX_FALSE;
}

static int
_aaxMMDevDriverSetup(const void *id, size_t *bufsize, int fmt,
                   unsigned int *tracks, float *speed)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int channels, format, freq;
   int frag, bufsz = 4096;
   int err;

   if (*bufsize) {
      bufsz = *bufsize;
   }

   assert(handle);

   if (handle->no_tracks > *tracks) {
      handle->no_tracks = *tracks;
   }
   bufsz *= handle->no_tracks;
   bufsz /= *tracks;
   *tracks = handle->no_tracks;

   if (*tracks > 2)
   {
      char str[255];
      snprintf((char *)&str, 255, "mmdev; Unable to output to %i speakers in "
                "this setup (2 is the maximum)", *tracks);
      _AAX_SYSLOG(str);
      return AAX_FALSE;
   }

   freq = (unsigned int)*speed;
   channels = *tracks; // handle->no_tracks;

   switch(fmt)
   {
   case AAX_PCM8S:	
//    format = AFMT_S8;
      handle->bytes_sample = 1;
      break;
   case AAX_PCM16S:
//    format = AFMT_S16_LE;
      handle->bytes_sample = 2;
      break;
   default:
      _AAX_SYSLOG("mmdev; unsupported audio format.");
      return AAX_FALSE;
   }

   frag = log2i(bufsz);
// frag = log2i(channels*freq); // 1 second buffer
   if (frag < 4) {
      frag = 4;
   }




   err = 0;
   if (err >= 0)
   {
      handle->format = fmt; // format;
      handle->no_tracks = channels;
      handle->frequency_hz = (float)freq;
      handle->buffer_size = bufsz;
      if (bufsize) *bufsize = bufsz;
   }

   return (err >= 0) ? AAX_TRUE : AAX_FALSE;
}

static int
_aaxMMDevDriverPause(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;
   
   if (handle)
   {
      rv = AAX_TRUE;
   }
   return rv;
}

static int
_aaxMMDevDriverResume(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;
   if (handle)
   {
      rv = AAX_TRUE;
   }
   return rv;
}

static int
_aaxMMDevDriverAvailable(const void *id)
{
   return AAX_TRUE;
}

static int
_aaxMMDevDriverIsAvailable(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;

   if (handle)
   {
      rv = AAX_TRUE;
   }

   return rv;
}

static int
_aaxMMDevDriverCapture(const void *id, void **data, size_t *frames, void *scratch)
{
// _driver_t *handle = (_driver_t *)id;

// if (handle->mode != O_RDONLY || (frames == 0) || (data == 0))
//    return AAX_FALSE;

   if (*frames == 0)
      return AAX_TRUE;

   return AAX_FALSE;
}

static int
_aaxMMDevDriverPlayback(const void *id, void *d, void *s, float pitch, float volume)
{
// outbuf_size = info.fragstotal*info.fragsize - outbuf_size;
// return (info.bytes-outbuf_size)/(no_tracks*no_samples);
   return 0;
}

static char *
_aaxMMDevDriverGetName(const void *id, int playback)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = (char *)_mmdev_default_name;

   if (handle && handle->name)
      ret = handle->name;

   return ret;
}

static char *
_aaxMMDevDriverGetDevices(const void *id, int mode)
{
   static char names[2][256] = { "\0\0", "\0\0" };

   return (char *)&names[mode];
}

static char *
_aaxMMDevDriverGetInterfaces(const void *id, const char *devname, int mode)
{
   static const char *rd[2] = {
    "\0\0",
    "\0\0"
   };
   return (char *)rd[mode];

}


/* -------------------------------------------------------------------------- */

#if 0
static char *
detect_devname(int pAudioClient, unsigned int tracks, char mode)
{
   char *rv = (char*)_mmdev_default_name;

   if (pAudioClient > 0)
   {
      int len = strlen(rv)+4;
      char *name = malloc(len);
      if (name)
      {
         snprintf(name, len, "/dev/dsp%i", pAudioClient);
         rv = name;
      }
   }
   else {
      rv = _aax_strdup("/dev/dsp");
   }

   return rv;
}
#endif

static IAudioClient *
detect_audioclient(const char *devname)
{
   IAudioClient *pAudioClient = NULL;

   return pAudioClient;
}

