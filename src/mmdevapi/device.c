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
#include <base/logging.h>
#include <base/types.h>

#include "audio.h"
#include "device.h"

#define DEFAULT_RENDERER	"MMDevice"
#define DEFAULT_DEVNAME		"default"

static _aaxDriverDetect _aaxMMDEVAPIDriverDetect;
static _aaxDriverGetDevices _aaxMMDEVAPIDriverGetDevices;
static _aaxDriverGetInterfaces _aaxMMDEVAPIDriverGetInterfaces;
static _aaxDriverConnect _aaxMMDEVAPIDriverConnect;
static _aaxDriverDisconnect _aaxMMDEVAPIDriverDisconnect;
static _aaxDriverSetup _aaxMMDEVAPIDriverSetup;
static _aaxDriverState _aaxMMDEVAPIDriverPause;
static _aaxDriverState _aaxMMDEVAPIDriverResume;
static _aaxDriverCaptureCallback _aaxMMDEVAPIDriverCapture;
static _aaxDriverCallback _aaxMMDEVAPIDriverPlayback;
static _aaxDriverGetName _aaxMMDEVAPIDriverGetName;
static _aaxDriverState _aaxMMDEVAPIDriverIsAvailable;
static _aaxDriverState _aaxMMDEVAPIDriverAvailable;

char _mmdev_default_renderer[100] = DEFAULT_RENDERER;
_aaxDriverBackend _aaxMMDEVAPIDriverBackend =
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

   (_aaxDriverDetect *)&_aaxMMDEVAPIDriverDetect,
   (_aaxDriverGetDevices *)&_aaxMMDEVAPIDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxMMDEVAPIDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxMMDEVAPIDriverGetName,
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,

   (_aaxDriverConnect *)&_aaxMMDEVAPIDriverConnect,
   (_aaxDriverDisconnect *)&_aaxMMDEVAPIDriverDisconnect,
   (_aaxDriverSetup *)&_aaxMMDEVAPIDriverSetup,
   (_aaxDriverState *)&_aaxMMDEVAPIDriverPause,
   (_aaxDriverState *)&_aaxMMDEVAPIDriverResume,
   (_aaxDriverCaptureCallback *)&_aaxMMDEVAPIDriverCapture,
   (_aaxDriverCallback *)&_aaxMMDEVAPIDriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxSoftwareDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxSoftwareDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)*_aaxMMDEVAPIDriverAvailable,
   (_aaxDriverState *)*_aaxMMDEVAPIDriverAvailable,
   (_aaxDriverState *)*_aaxMMDEVAPIDriverIsAvailable
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


static IAudioClient* detect_audioclient(const char *);


char *_mmdev_default_name = DEFAULT_DEVNAME;


static int
_aaxMMDEVAPIDriverDetect(int mode)
{
   IMMDeviceEnumerator *pEnumerator;
   static int rv = AAX_FALSE;
   HRESULT hr;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);
     
   hr = pCoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER,
                         &IID_IMMDeviceEnumerator, (void**)&pEnumerator);
   if (!FAILED(hr))
   {
      pIMMDeviceEnumerator_Release(pEnumerator);
      pEnumerator = NULL;
      rv = AAX_TRUE;
   }

   return rv;
}

static void *
_aaxMMDEVAPIDriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle)
   {
      handle = (_driver_t *)calloc(1, sizeof(_driver_t));
      if (!handle) return 0;

      handle->sse_level = _aaxGetSSELevel();
      handle->frequency_hz = _aaxMMDEVAPIDriverBackend.rate;
      handle->no_tracks = _aaxMMDEVAPIDriverBackend.tracks;

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
_aaxMMDEVAPIDriverDisconnect(void *id)
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
_aaxMMDEVAPIDriverSetup(const void *id, size_t *bufsize, int fmt,
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





   if (err >= 0)
   {
      handle->format = format;
      handle->no_tracks = channels;
      handle->frequency_hz = (float)freq;
      handle->buffer_size = bufsz;
      if (bufsize) *bufsize = bufsz;
   }

   return (err >= 0) ? AAX_TRUE : AAX_FALSE;
}

static int
_aaxMMDEVAPIDriverPause(const void *id)
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
_aaxMMDEVAPIDriverResume(const void *id)
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
_aaxMMDEVAPIDriverAvailable(const void *id)
{
   return AAX_TRUE;
}

static int
_aaxMMDEVAPIDriverIsAvailable(const void *id)
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
_aaxMMDEVAPIDriverCapture(const void *id, void **data, size_t *frames, void *scratch)
{
// _driver_t *handle = (_driver_t *)id;

// if (handle->mode != O_RDONLY || (frames == 0) || (data == 0))
//    return AAX_FALSE;

   if (*frames == 0)
      return AAX_TRUE;

   return AAX_FALSE;
}

static int
_aaxMMDEVAPIDriverPlayback(const void *id, void *d, void *s, float pitch, float volume)
{
// outbuf_size = info.fragstotal*info.fragsize - outbuf_size;
// return (info.bytes-outbuf_size)/(no_tracks*no_samples);
   return 0;
}

static char *
_aaxMMDEVAPIDriverGetName(const void *id, int playback)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = (char *)_mmdev_default_name;

   if (handle && handle->name)
      ret = handle->name;

   return ret;
}

static char *
_aaxMMDEVAPIDriverGetDevices(const void *id, int mode)
{
   static char names[2][256] = { "\0\0", "\0\0" };

   return (char *)&names[mode];
}

static char *
_aaxMMDEVAPIDriverGetInterfaces(const void *id, const char *devname, int mode)
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

