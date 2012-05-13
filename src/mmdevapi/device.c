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

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>
#include <base/dlsym.h>
#include <base/logging.h>
#include <base/types.h>

#include "audio.h"
#include "device.h"

#define REFTIMES_PER_SEC	10000000
#define REFTIMES_PER_MILLISEC	
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
   (char*)&_mmdev_default_renderer,

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
// char* name;
   WCHAR *devname;

   IMMDeviceEnumerator *pEnumerator;
   IMMDevice *pDevice;

   IAudioClient *pAudioClient;
   IAudioRenderClient *pRenderClient;

   WAVEFORMATEX *format;
   EDataFlow mode;
   int exclusive;
   char sse_level;

   int16_t *ptr, *scratch;
#ifndef NDEBUG
   unsigned int buf_len;
#endif

} _driver_t;

const char* _mmdev_default_name = DEFAULT_DEVNAME;

DECL_VARIABLE(IID_IAudioClient);
DECL_VARIABLE(IID_IAudioRenderClient);
DECL_VARIABLE(CLSID_MMDeviceEnumerator);
DECL_VARIABLE(IID_IMMDeviceEnumerator);
DECL_VARIABLE(DEVPKEY_Device_FriendlyName);

DECL_FUNCTION(PropVariantInit);
DECL_FUNCTION(PropVariantClear);
DECL_FUNCTION(WideCharToMultiByte);
DECL_FUNCTION(CoCreateInstance);
DECL_FUNCTION(CoTaskMemFree);
DECL_FUNCTION(IMMDeviceEnumerator_Release);
DECL_FUNCTION(IMMDeviceEnumerator_GetDevice);
DECL_FUNCTION(IMMDeviceEnumerator_GetDefaultEndPoint);
DECL_FUNCTION(IMMDevice_Activate);
DECL_FUNCTION(IMMDevice_Release);
DECL_FUNCTION(IAudioClient_Start);
DECL_FUNCTION(IAudioClient_Stop);
DECL_FUNCTION(IAudioClient_GetService);
DECL_FUNCTION(IAudioClient_Initialize);
DECL_FUNCTION(IAudioClient_GetMixFormat);
DECL_FUNCTION(IAudioClient_GetBufferSize);
DECL_FUNCTION(IAudioClient_GetCurrentPadding);
DECL_FUNCTION(IAudioClient_Release);
DECL_FUNCTION(IAudioRenderClient_GetBuffer);
DECL_FUNCTION(IAudioRenderClient_ReleaseBuffer);
DECL_FUNCTION(IAudioRenderClient_Release);
DECL_FUNCTION(IMMDevice_OpenPropertyStore);
DECL_FUNCTION(IPropertyStore_GetValue);
DECL_FUNCTION(IPropertyStore_Release);

static WCHAR* detect_audioclient(const char*);
static char *detect_devname(IMMDevice *);


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
         TIE_FUNCTION(CoTaskMemFree);
         TIE_VARIABLE(IID_IAudioClient);
         TIE_VARIABLE(IID_IAudioRenderClient);
         TIE_VARIABLE(CLSID_MMDeviceEnumerator);
         TIE_VARIABLE(IID_IMMDeviceEnumerator);
         TIE_VARIABLE(DEVPKEY_Device_FriendlyName);

         TIE_FUNCTION(PropVariantInit);
         TIE_FUNCTION(PropVariantClear);
         TIE_FUNCTION(WideCharToMultiByte);
         TIE_FUNCTION(IMMDeviceEnumerator_Release);
         TIE_FUNCTION(IMMDeviceEnumerator_GetDevice);
         TIE_FUNCTION(IMMDeviceEnumerator_GetDefaultEndPoint);
         TIE_FUNCTION(IMMDevice_Activate);
         TIE_FUNCTION(IMMDevice_Release);
         TIE_FUNCTION(IAudioClient_Start);
         TIE_FUNCTION(IAudioClient_Stop);
         TIE_FUNCTION(IAudioClient_GetService);
         TIE_FUNCTION(IAudioClient_Initialize);
         TIE_FUNCTION(IAudioClient_GetMixFormat);
         TIE_FUNCTION(IAudioClient_GetBufferSize);
         TIE_FUNCTION(IAudioClient_GetCurrentPadding);
         TIE_FUNCTION(IAudioClient_Release);
         TIE_FUNCTION(IAudioRenderClient_GetBuffer);
         TIE_FUNCTION(IAudioRenderClient_ReleaseBuffer);
         TIE_FUNCTION(IAudioRenderClient_Release);
         TIE_FUNCTION(IMMDevice_OpenPropertyStore);
         TIE_FUNCTION(IPropertyStore_GetValue);
         TIE_FUNCTION(IPropertyStore_Release);

         error = _oalGetSymError(0);
         if (!error) {
            rv = AAX_TRUE;
         }
      }
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
      handle->format->nSamplesPerSec = _aaxMMDevDriverBackend.rate;
      handle->format->nChannels = _aaxMMDevDriverBackend.tracks;

      if (xid)
      {
         char *s;
         int i;

         if (!handle->devname)
         {
            s = xmlNodeGetString(xid, "renderer");
            if (s) handle->devname = detect_audioclient(s);
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
            handle->format->nSamplesPerSec = i;
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
               handle->format->nChannels = i;
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

#if 0
printf("frequency-hz: %f\n", handle->frequency_hz);
printf("channels: %i\n", handle->no_tracks);
#endif
   }


   if (handle)
   {
      const EDataFlow _mode[] = { eCapture, eRender };
      HRESULT hr;
      int m;

      m = (mode > 0) ? 1 : 0;
      handle->mode = _mode[m];

      hr = pCoCreateInstance(pCLSID_MMDeviceEnumerator, NULL,
                             CLSCTX_INPROC_SERVER, pIID_IMMDeviceEnumerator,
                             (void**)&handle->pEnumerator);
      if (SUCCEEDED(hr))
      {
         if (renderer) {
            handle->devname = detect_audioclient(renderer);
         }

         if (!handle->devname) {
            hr = pIMMDeviceEnumerator_GetDefaultEndPoint(_mode[m], eMultimedia,
                                                         &handle->pDevice);
         } else {
             hr = pIMMDeviceEnumerator_GetDevice(handle->pEnumerator,
                                                 handle->devname,
                                                 &handle->pDevice);
         }

         hr = pIMMDevice_Activate(handle->pDevice, pIID_IAudioClient,
                                  CLSCTX_INPROC_SERVER, NULL,
                                  handle->pAudioClient);

         hr = pIAudioClient_GetMixFormat(handle->pAudioClient, &handle->format);
      }
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
      if (handle->pEnumerator != NULL)
      {
         pIMMDeviceEnumerator_Release(handle->pEnumerator);
         handle->pEnumerator = NULL;
      }
      if (handle->pDevice != NULL) 
      {
         pIMMDevice_Release(handle->pDevice);
         handle->pDevice = NULL;
      }
      if (handle->pAudioClient != NULL) 
      {
         pIAudioClient_Release(handle->pAudioClient);
         handle->pAudioClient = NULL;
      }
      if (handle->pRenderClient != NULL) 
      {
         pIAudioRenderClient_Release(handle->pRenderClient);
         handle->pRenderClient = NULL;
      }

      pCoTaskMemFree(handle->format);

      if (handle->devname)
      {
         free(handle->devname);
         handle->devname = 0;
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
   REFERENCE_TIME hnsBufferDuration;
   REFERENCE_TIME hnsPeriodicity;
   unsigned int channels, freq;
   int bufsz = 4096;
   HRESULT hr;

   assert(handle);

   if (*bufsize) {
      bufsz = *bufsize;
   }
   
   if (handle->format->nChannels > *tracks) {
      handle->format->nChannels = *tracks;
   }

   bufsz *= handle->format->nChannels;
   bufsz /= *tracks;

   *tracks = handle->format->nChannels;
   if (*tracks > 2) // TODO: for now
   {
      char str[255];
      snprintf((char *)&str, 255, "mmdev; Unable to output to %i speakers in "
                "this setup (2 is the maximum)", *tracks);
      _AAX_SYSLOG(str);
      return AAX_FALSE;
   }

   freq = (unsigned int)*speed;
   channels = *tracks;

   switch(fmt)
   {
   case AAX_PCM8S:	
      handle->format->wBitsPerSample = 8;
      break;
   case AAX_PCM16S:
      handle->format->wBitsPerSample = 16;
      break;
   default:
      _AAX_SYSLOG("mmdev; unsupported audio format.");
      return AAX_FALSE;
   }

   handle->format->nSamplesPerSec = freq;
   handle->format->nChannels = channels;
   handle->format->cbSize = bufsz;
   if (bufsize) *bufsize = bufsz;


   /*
    * The buffer capacity as a time value. This parameter is of type
    * REFERENCE_TIME and is expressed in 100-nanosecond units. This parameter
    * contains the buffer size that the caller requests for the buffer that the
    * audio application will share with the audio engine (in shared mode) or 
    * with the endpoint device (in exclusive mode). If the call succeeds, the 
    * method allocates a buffer that is a least this large
    */
   hnsBufferDuration = (REFERENCE_TIME)((float)freq*1e7f/(float)bufsz);

   /*
    * The device period. This parameter can be nonzero only in exclusive mode.
    * In shared mode, always set this parameter to 0. In exclusive mode, this
    * parameter specifies the requested scheduling period for successive buffer
    * accesses by the audio endpoint device. If the requested device period
    * lies outside the range that is set by the device's minimum period and the
    * system's maximum period, then the method clamps the period to that range.
    * If this parameter is 0, the method sets the device period to its default
    * value.
    */
   hnsPeriodicity = 0;

   hr = pIAudioClient_Initialize(handle->pAudioClient, AUDCLNT_SHAREMODE_SHARED,
                                 0, hnsBufferDuration, hnsPeriodicity,
                                 handle->format, NULL);

   if (SUCCEEDED(hr))
   {
      UINT32 bufFrameCnt;

      hr = pIAudioClient_GetBufferSize(handle->pAudioClient, &bufFrameCnt);
      if (SUCCEEDED(hr))
      {
         handle->format->cbSize = bufFrameCnt;
         *bufsize = bufFrameCnt;

         hr = pIAudioClient_GetService(handle->pAudioClient,
                                       pIID_IAudioRenderClient,
                                       (void**)&handle->pRenderClient);
      }
   }

   return (SUCCEEDED(hr)) ? AAX_TRUE : AAX_FALSE;
}

static int
_aaxMMDevDriverPause(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;
   
   if (handle)
   {
      HRESULT hr = pIAudioClient_Stop(handle->pAudioClient);
      rv = (SUCCEEDED(hr)) ? AAX_TRUE : AAX_FALSE;
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
      HRESULT hr = pIAudioClient_Start(handle->pAudioClient);
      rv = (SUCCEEDED(hr)) ? AAX_TRUE : AAX_FALSE;
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

   // TODO:
// if (handle->mode != O_RDONLY || (frames == 0) || (data == 0))
//    return AAX_FALSE;

   if (*frames == 0)
      return AAX_TRUE;

   return AAX_FALSE;
}

static int
_aaxMMDevDriverPlayback(const void *id, void *d, void *s, float pitch, float volume)
{
   // TODO:

// outbuf_size = info.fragstotal*info.fragsize - outbuf_size;
// return (info.bytes-outbuf_size)/(no_tracks*no_samples);
   return 0;
}

static char *
_aaxMMDevDriverGetName(const void *id, int playback)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = (char *)_mmdev_default_name;

   if (handle && handle->devname) {
      ret = detect_devname(handle->devname);
   }

   return ret;
}

static char *
_aaxMMDevDriverGetDevices(const void *id, int mode)
{
   static char names[2][256] = { "\0\0", "\0\0" };

// TODO:

   return (char *)&names[mode];
}

static char *
_aaxMMDevDriverGetInterfaces(const void *id, const char *devname, int mode)
{
   static const char* rd[2] = {
    "\0\0",
    "\0\0"
   };

// TODO:

   return (char *)rd[mode];

}


/* -------------------------------------------------------------------------- */

char *
detect_devname(IMMDevice *device)
{
   char *rv = NULL;
   void *props;
   HRESULT hr;

   hr = pIMMDevice_OpenPropertyStore(device, STGM_READ, &props);
   if (SUCCEEDED(hr))
   {
      PROPVARIANT name;

      pPropVariantInit(&name);
      hr = pIPropertyStore_GetValue(props, pDEVPKEY_Device_FriendlyName, &name);
      if (SUCCEEDED(hr))
      {
         int len = pWideCharToMultiByte(CP_ACP, 0, name.pwszVal, -1,
                                        NULL, 0, NULL, NULL);
         if (len > 0)
         {
            rv = calloc(1, len);
            pWideCharToMultiByte(CP_ACP, 0, name.pwszVal, -1, rv, len,
                                 NULL, NULL);
         }
      }
      pPropVariantClear(&name);
      pIPropertyStore_Release(props);
   }

   return rv;
}

static WCHAR*
detect_audioclient(const char* devname)
{
   IAudioClient *pAudioClient = NULL;

// TODO:

   return pAudioClient;
}

