/*
 * Copyright 2011-2012 by Erik Hofman.
 * Copyright 2011-2012 by Adalin B.V.
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

#define NO_PERIODS		2
#define MAX_ID_STRLEN		32
#define DEFAULT_RENDERER	"MMDevice"
#define DEFAULT_DEVNAME		NULL

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
   WCHAR *devname;
   LPWSTR devid;

   IMMDeviceEnumerator *pEnumerator;
   IMMDevice *pDevice;

   IAudioClient *pAudioClient;
   union
   {
      IAudioRenderClient *pRender;
      IAudioCaptureClient *pCapture;
   } client;

   WAVEFORMATEX *format;
   EDataFlow mode;

   unsigned int no_frames;
   int exclusive;
   char sse_level;

   int16_t *ptr, *scratch;
#ifndef NDEBUG
   unsigned int buf_len;
#endif

} _driver_t;

const char* _mmdev_default_name = DEFAULT_DEVNAME;

#if 0
DECL_VARIABLE(IID_IAudioClient);
DECL_VARIABLE(IID_IAudioRenderClient);
DECL_VARIABLE(CLSID_MMDeviceEnumerator);
DECL_VARIABLE(IID_IMMDeviceEnumerator);
// DECL_VARIABLE(PKEY_Device_FriendlyName);
DECL_VARIABLE(DEVPKEY_Device_FriendlyName);

DECL_FUNCTION(PropVariantInit);
DECL_FUNCTION(PropVariantClear);
DECL_FUNCTION(WideCharToMultiByte);
DECL_FUNCTION(MultiByteToWideChar);
DECL_FUNCTION(CoCreateInstance);
DECL_FUNCTION(CoTaskMemFree);
DECL_FUNCTION(IMMDeviceEnumerator_Release);
DECL_FUNCTION(IMMDeviceEnumerator_GetDevice);
DECL_FUNCTION(IMMDeviceEnumerator_GetDefaultAudioEndpoint);
DECL_FUNCTION(IMMDeviceEnumerator_EnumAudioEndpoints);
DECL_FUNCTION(IMMDeviceCollection_GetCount);
DECL_FUNCTION(IMMDeviceCollection_Release);
DECL_FUNCTION(IMMDeviceCollection_Item);
DECL_FUNCTION(IMMDevice_Activate);
DECL_FUNCTION(IMMDevice_Release);
DECL_FUNCTION(IMMDevice_GetId);
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
DECL_FUNCTION(IAudioCaptureClient_GetBuffer);
DECL_FUNCTION(IAudioCaptureClient_ReleaseBuffer);
DECL_FUNCTION(IAudioCaptureClient_GetNextPacketSize);
#else
# define pCLSID_MMDeviceEnumerator &CLSID_MMDeviceEnumerator
# define pIID_IMMDeviceEnumerator &IID_IMMDeviceEnumerator
# define pIID_IAudioRenderClient &IID_IAudioRenderClient
# define pIID_IAudioClient &IID_IAudioClient
# define pPKEY_Device_FriendlyName &PKEY_Device_FriendlyName
# define pDEVPKEY_Device_FriendlyName &DEVPKEY_Device_FriendlyName

# define pPropVariantInit PropVariantInit
# define pPropVariantClear PropVariantClear
# define pWideCharToMultiByte WideCharToMultiByte
# define pMultiByteToWideChar MultiByteToWideChar
# define pCoInitialize CoInitialize
# define pCoUninitialize CoUninitialize
# define pCoCreateInstance CoCreateInstance
# define pCoTaskMemFree CoTaskMemFree
# define pIMMDeviceEnumerator_Release IMMDeviceEnumerator_Release
# define pIMMDeviceEnumerator_GetDevice IMMDeviceEnumerator_GetDevice
# define pIMMDeviceEnumerator_GetDefaultAudioEndpoint IMMDeviceEnumerator_GetDefaultAudioEndpoint
# define pIMMDeviceEnumerator_EnumAudioEndpoints IMMDeviceEnumerator_EnumAudioEndpoints
# define pIMMDeviceCollection_GetCount IMMDeviceCollection_GetCount
# define pIMMDeviceCollection_Release IMMDeviceCollection_Release
# define pIMMDeviceCollection_Item IMMDeviceCollection_Item
# define pIMMDevice_Activate IMMDevice_Activate
# define pIMMDevice_Release IMMDevice_Release
# define pIMMDevice_GetId IMMDevice_GetId
# define pIAudioClient_Start IAudioClient_Start
# define pIAudioClient_Stop IAudioClient_Stop
# define pIAudioClient_GetService IAudioClient_GetService
# define pIAudioClient_Initialize IAudioClient_Initialize
# define pIAudioClient_GetMixFormat IAudioClient_GetMixFormat
# define pIAudioClient_GetBufferSize IAudioClient_GetBufferSize
# define pIAudioClient_GetCurrentPadding IAudioClient_GetCurrentPadding
# define pIAudioClient_Release IAudioClient_Release
# define pIAudioRenderClient_GetBuffer IAudioRenderClient_GetBuffer
# define pIAudioRenderClient_ReleaseBuffer IAudioRenderClient_ReleaseBuffer
# define pIAudioRenderClient_Release IAudioRenderClient_Release
# define pIMMDevice_OpenPropertyStore IMMDevice_OpenPropertyStore
# define pIPropertyStore_GetValue IPropertyStore_GetValue
# define pIPropertyStore_Release IPropertyStore_Release
# define pIAudioCaptureClient_GetBuffer IAudioCaptureClient_GetBuffer
# define pIAudioCaptureClient_ReleaseBuffer IAudioCaptureClient_ReleaseBuffer
# define pIAudioCaptureClient_GetNextPacketSize IAudioCaptureClient_GetNextPacketSize
#endif

static LPWSTR name_to_id(const WCHAR*, char);
static char* detect_devname(IMMDevice*);
static char* wcharToChar(const WCHAR*);
static WCHAR* charToWChar(const char*);

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

      snprintf(_mmdev_default_renderer, MAX_ID_STRLEN, "%s %s",
               DEFAULT_RENDERER, hwstr);

#if 0
      _oalGetSymError(0);

      TIE_FUNCTION(CoCreateInstance);
      if (pCoCreateInstance)
      {
         TIE_FUNCTION(CoTaskMemFree);
         TIE_VARIABLE(IID_IAudioClient);
         TIE_VARIABLE(IID_IAudioRenderClient);CKENDS
         TIE_VARIABLE(CLSID_MMDeviceEnumerator);
         TIE_VARIABLE(IID_IMMDeviceEnumerator);
//          TIE_VARIABLE(PKEY_Device_FriendlyName);
         TIE_VARIABLE(DEVPKEY_Device_FriendlyName);

         TIE_FUNCTION(PropVariantInit);
         TIE_FUNCTION(PropVariantClear);
         TIE_FUNCTION(WideCharToMultiByte);
         TIE_FUNCTION(MultiByteToWideChar);
         TIE_FUNCTION(IMMDeviceEnumerator_Release);
         TIE_FUNCTION(IMMDeviceEnumerator_GetDevice);
         TIE_FUNCTION(IMMDeviceEnumerator_GetDefaultAudioEndpoint);
         TIE_FUNCTION(IMMDeviceEnumerator_EnumAudioEndpoints);
         TIE_FUNCTION(IMMDeviceCollection_GetCount);
         TIE_FUNCTION(IMMDeviceCollection_Release);
         TIE_FUNCTION(IMMDeviceCollection_Item);
         TIE_FUNCTION(IMMDevice_Activate);
         TIE_FUNCTION(IMMDevice_Release);
         TIE_FUNCTION(IMMDevice_GetId);
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
         TIE_FUNCTION(IAudioCaptureClient_GetBuffer);
         TIE_FUNCTION(IAudioCaptureClient_ReleaseBuffer);
         TIE_FUNCTION(IAudioCaptureClient_GetNextPacketSize);

         error = _oalGetSymError(0);
         if (!error) {
            rv = AAX_TRUE;
         }
      }
#else
      rv = AAX_TRUE;
#endif
   }

   return rv;
}

static void *
_aaxMMDevDriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;
   WAVEFORMATEX fmt;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle)
   {
      handle = (_driver_t *)calloc(1, sizeof(_driver_t));
      if (!handle) return 0;

      handle->sse_level = _aaxGetSSELevel();
      fmt.nSamplesPerSec = _aaxMMDevDriverBackend.rate;
      fmt.nChannels = _aaxMMDevDriverBackend.tracks;

      if (xid)
      {
         char *s;
         int i;

         if (!handle->devname)
         {
            s = xmlNodeGetString(xid, "renderer");
            if (s)
            {
               if (strcmp(s, "default")) 
               {
                  handle->devname = charToWChar(s);
                  handle->devid = name_to_id(handle->devname, mode ? 1 : 0);
               }
               else handle->devname = NULL;
            }
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
            fmt.nSamplesPerSec = i;
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
               fmt.nChannels = i;
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

      hr = pCoInitialize(NULL);
      if (SUCCEEDED(hr))
      {
         hr = pCoCreateInstance(pCLSID_MMDeviceEnumerator, NULL,
                                CLSCTX_INPROC_SERVER, pIID_IMMDeviceEnumerator,
                          (void**)&handle->pEnumerator);

         if (SUCCEEDED(hr))
         {
            if (renderer && strcmp(renderer, "default")) {
               handle->devname = charToWChar(renderer);
               handle->devid = name_to_id(handle->devname, m);
            }

            if (!handle->devname) {
               hr = pIMMDeviceEnumerator_GetDefaultAudioEndpoint(
                                                 handle->pEnumerator, _mode[m],
                                                 eMultimedia, &handle->pDevice);
            } else {
                hr = pIMMDeviceEnumerator_GetDevice(handle->pEnumerator,
                                                    handle->devid,
                                                    &handle->pDevice);
            }

            hr = pIMMDevice_Activate(handle->pDevice, pIID_IAudioClient,
                                     CLSCTX_INPROC_SERVER, NULL,
                                     &handle->pAudioClient);

            hr = pIAudioClient_GetMixFormat(handle->pAudioClient,
                                            &handle->format);

#if 0
printf("\nDevice specific settings:\n");
printf("\twFormatTag: %i\n",  handle->format->wFormatTag);
printf("\tnSamplesPerSec: %i\n", handle->format->nSamplesPerSec);
printf("\tnChannels: %i\n",  handle->format->nChannels);
printf("\twBitsPerSample: %i\n", handle->format->wBitsPerSample);
printf("\tnBlockAlign: %i\n", handle->format->nBlockAlign);
printf("\tnAvgBytesPerSec: %i\n", handle->format->nAvgBytesPerSec);
printf("\tcbSize: %i\n", handle->format->cbSize);
#endif

//          handle->format->nSamplesPerSec = fmt.nSamplesPerSec;
            handle->format->nChannels = fmt.nChannels;
#if 0
printf("\nAdjusting for XNL settings:\n");
printf("\tnSamplesPerSec: %i\n", handle->format->nSamplesPerSec);
printf("\tnChannels: %i\n",  handle->format->nChannels);
#endif

         }
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
      HRESULT hr = pIAudioClient_Stop(handle->pAudioClient);
      if (FAILED(hr)) {
         _AAX_SYSLOG("mmdev; unable to stop the audio client");
      }

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
      if (handle->client.pRender != NULL) 
      {
         pIAudioRenderClient_Release(handle->client.pRender);
         handle->client.pRender = NULL;
      }

      pCoTaskMemFree(handle->format);

      if (handle->devname)
      {
         free(handle->devname);
         handle->devname = 0;
         pCoTaskMemFree(handle->devid);
         handle->devid = 0;
      }

      pCoUninitialize();

      free(handle->ptr);
      free(handle);
 
      return AAX_TRUE;
   }
   return AAX_FALSE;
}

static int
_aaxMMDevDriverSetup(const void *id, size_t *frames, int fmt,
                   unsigned int *tracks, float *speed)
{
   _driver_t *handle = (_driver_t *)id;
   REFERENCE_TIME hnsBufferDuration;
   REFERENCE_TIME hnsPeriodicity;
   unsigned int channels, freq;
   int bufsz = 4096;
   HRESULT hr;

   assert(handle);

   if (*frames) {
      bufsz = *frames;
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
   case  AAX_PCM32S:
      handle->format->wBitsPerSample = 32;
      break;
   default:
      _AAX_SYSLOG("mmdev; unsupported audio format.");
      return AAX_FALSE;
   }

   handle->format->cbSize = 0;
   handle->format->wFormatTag = WAVE_FORMAT_PCM;
// handle->format->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
// handle->format->cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
// handle->format->nSamplesPerSec = freq;
   *speed = (float)handle->format->nSamplesPerSec;

   handle->format->nChannels = channels;
   handle->format->nBlockAlign = (channels * handle->format->wBitsPerSample)/8;
   handle->format->nAvgBytesPerSec = handle->format->nSamplesPerSec *
                                     handle->format->nBlockAlign;

   handle->no_frames = bufsz;
   if (frames) *frames = bufsz;


   /*
    * The buffer capacity as a time value. This parameter is of type
    * REFERENCE_TIME and is expressed in 100-nanosecond units. This parameter
    * contains the buffer size that the caller requests for the buffer that the
    * audio application will share with the audio engine (in shared mode) or 
    * with the endpoint device (in exclusive mode). If the call succeeds, the 
    * method allocates a buffer that is a least this large
    */
   hnsBufferDuration = (REFERENCE_TIME)(1000.0f*(float)bufsz/(float)freq);

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
   if (hr == 0x800401f0)
   {
      hr = pCoInitialize(NULL);
      hr = pIAudioClient_Initialize(handle->pAudioClient,
                                    AUDCLNT_SHAREMODE_SHARED,
                                    0, hnsBufferDuration, hnsPeriodicity,
                                    handle->format, NULL);
   }
#if 0
printf("\nIAudioClient_Initialize:\n");
printf("\tformat:\n");
printf("\twFormatTag: %i\n",  handle->format->wFormatTag);
printf("\tnSamplesPerSec: %i\n", handle->format->nSamplesPerSec);
printf("\tnChannels: %i\n",  handle->format->nChannels);
printf("\twBitsPerSample: %i\n", handle->format->wBitsPerSample);
printf("\tnBlockAlign: %i\n", handle->format->nBlockAlign);
printf("\tnAvgBytesPerSec: %i\n", handle->format->nAvgBytesPerSec);
printf("\tcbSize: %i\n", handle->format->cbSize);
#endif
   if (SUCCEEDED(hr))
   {
      UINT32 bufFrameCnt;

      /* get the actual buffer size */
      hr = pIAudioClient_GetBufferSize(handle->pAudioClient, &bufFrameCnt);
      if (SUCCEEDED(hr))
      {
         handle->no_frames = bufFrameCnt;
         *frames = bufFrameCnt;

         hr = pIAudioClient_GetService(handle->pAudioClient,
                                       pIID_IAudioRenderClient,
                                       (void**)&handle->client.pRender);
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
   _driver_t *handle = (_driver_t *)id;

   if ((frames == 0) || (data == 0)) // handle->mode != O_RDONLY
      return AAX_FALSE;

   if (*frames == 0)
      return AAX_TRUE;

   *frames = 0;
   if (data)
   {
      UINT32 packet_sz;
      HRESULT hr;

      hr = pIAudioCaptureClient_GetNextPacketSize(handle->client.pCapture,
                                                  &packet_sz);
      if (SUCCEEDED(hr))
      {
         UINT32 frames_avail;
         BYTE* buf;
         DWORD fl;

         while (packet_sz != 0)
         {
            hr = pIAudioCaptureClient_GetBuffer(handle->client.pCapture, &buf,
                                                &frames_avail, &fl, NULL, NULL);
            if (SUCCEEDED(hr))
            {
               if ((fl & AUDCLNT_BUFFERFLAGS_SILENT) != 0) {
                  _batch_cvt24_16_intl((int32_t**)data, buf, 0,2, frames_avail);
               }
               *frames += frames_avail;

               hr = pIAudioCaptureClient_ReleaseBuffer(handle->client.pCapture,
                                                       frames_avail);
               if (SUCCEEDED(hr))
               {
                  hr=pIAudioCaptureClient_GetNextPacketSize(handle->client.pCapture,
                                                       &packet_sz);
                  if (FAILED(hr))
                  {
                     _AAX_SYSLOG("mmdev; unable to retrieve packet size");
                     packet_sz = 0;
                  }
               }
               else {
                  _AAX_SYSLOG("mmdev; unable to release buffer");
               }
            }
            else {
               _AAX_SYSLOG("mmdev; failed to get next buffer");
            }
         }
      }
      else {
         _AAX_SYSLOG("mmdev; unable to retrieve packet size");
      }

      return AAX_TRUE;
   }

   return AAX_FALSE;
}


static int
_aaxMMDevDriverPlayback(const void *id, void *d, void *s, float pitch, float volume)
{
   static char first_call = 1;
   _oalRingBuffer *rb = (_oalRingBuffer *)s;
   _driver_t *handle = (_driver_t *)id;
   unsigned int no_tracks, no_samples;
   unsigned int offs, outbuf_size;
   _oalRingBufferSample *rbd;
   UINT32 frames_avail;
   UINT32 frames = 0;
   HRESULT hr;
   BYTE *data;

   if ((frames == 0) || (d == 0)) // handle->mode != O_RDONLY
      return 0;

   assert(rb);
   assert(rb->sample);
   assert(id != 0);

   rbd = rb->sample;
   offs = _oalRingBufferGetOffsetSamples(rb);
   no_tracks = _oalRingBufferGetNoTracks(rb);
   no_samples = _oalRingBufferGetNoSamples(rb) - offs;

   outbuf_size = no_tracks * no_samples*sizeof(int16_t);
   if (handle->ptr == 0)
   {
      char *p = 0;
      handle->ptr = (int16_t *)_aax_malloc(&p, outbuf_size);
      handle->scratch = (int16_t*)p;
#ifndef NDEBUG
      handle->buf_len = outbuf_size;
#endif
   }
   data = (BYTE *)handle->scratch;
   assert(outbuf_size <= handle->buf_len);

   hr = pIAudioClient_GetCurrentPadding(handle->pAudioClient, &frames_avail);
   if (SUCCEEDED(hr))
   {
      frames = handle->no_frames - frames_avail;
      if (frames > no_samples) frames = no_samples;

      hr = pIAudioRenderClient_GetBuffer(handle->client.pRender, frames, &data);
      if (SUCCEEDED(hr))
      {
         _batch_cvt16_intl_24(data, (const int32_t**)rbd->track,
                              offs, no_tracks, frames);

         if (is_bigendian()) {
            _batch_endianswap16((uint16_t*)data, no_tracks*frames);
         }

         hr=pIAudioRenderClient_ReleaseBuffer(handle->client.pRender, frames,0);
      }

      if (SUCCEEDED(hr))
      {
         if (first_call)
         {
            first_call = 0;
            hr = pIAudioClient_Start(handle->pAudioClient);
            if (FAILED(hr)) {
               _AAX_SYSLOG("mmdev; unable to start the audio client");
            }
         }
      }
      else {
         _AAX_SYSLOG("mmdev; failed to get buffer space");
      }
   }
   else {
      _AAX_SYSLOG("mmdev; unable to detect free space");
   }

   return frames - outbuf_size;
}

static char *
_aaxMMDevDriverGetName(const void *id, int playback)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = (char *)_mmdev_default_name;

   if (handle && handle->pDevice) {
      ret = detect_devname(handle->pDevice);
   }

   return ret;
}

static char *
_aaxMMDevDriverGetDevices(const void *id, int mode)
{
   static char names[2][256] = { "\0\0", "\0\0" };
   IMMDeviceEnumerator *enumerator = NULL;
   HRESULT hr;

   hr = pCoInitialize(NULL);
   hr = pCoCreateInstance(pCLSID_MMDeviceEnumerator, NULL,
                          CLSCTX_INPROC_SERVER, pIID_IMMDeviceEnumerator,
                          (void**)&enumerator);
   if (SUCCEEDED(hr))
   {
      const EDataFlow _mode[] = { eCapture, eRender };
      IMMDeviceCollection *collection = NULL;
      IPropertyStore *props = NULL;
      IMMDevice *device = NULL;
      LPWSTR wstr = NULL;
      UINT i, count;
      int m, len;
      char *ptr;

      len = 255;
      m = mode > 0 ? 1 : 0;
      ptr = (char *)&names[m];
      hr = pIMMDeviceEnumerator_EnumAudioEndpoints(enumerator, _mode[m],
                                                   DEVICE_STATE_ACTIVE,
                                                   &collection);
      if (FAILED(hr)) goto Exit;

      hr = pIMMDeviceCollection_GetCount(collection, &count);
      for(i=0; SUCCEEDED(hr) && (i<count); i++)
      {
         PROPVARIANT name;
         char *devname;

         hr = pIMMDeviceCollection_Item(collection, i, &device);
         if (FAILED(hr)) break;

         hr = pIMMDevice_GetId(device, &wstr);
         if (FAILED(hr)) break;

         hr = pIMMDevice_OpenPropertyStore(device, STGM_READ, &props);
         if (FAILED(hr)) break;

         pPropVariantInit(&name);
         hr = pIPropertyStore_GetValue(props,
                               (const PROPERTYKEY*)pDEVPKEY_Device_FriendlyName,
                               &name);
         if (FAILED(hr)) break;

         devname = wcharToChar(name.pwszVal);
         if (devname)
         {
            int slen;

            snprintf(ptr, len, "%s", devname);
            free(devname);

            slen = strlen(ptr)+1;
            len -= slen;
            ptr += slen;
         }

         pCoTaskMemFree(wstr);
         wstr = NULL;

         pPropVariantClear(&name);
         pIPropertyStore_Release(props);
         pIMMDevice_Release(device);
      }
      pIMMDeviceEnumerator_Release(enumerator);
      pIMMDeviceCollection_Release(collection);
      pCoUninitialize();

      return (char *)&names[mode];

Exit:
      pCoUninitialize();
      pCoTaskMemFree(wstr);
      if (enumerator) pIMMDeviceEnumerator_Release(enumerator);
      if (collection) pIMMDeviceCollection_Release(collection);
      if (device) pIMMDevice_Release(device);
      if (props) pIPropertyStore_Release(props);
   }

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
   IPropertyStore *props;
   HRESULT hr;

   hr = pIMMDevice_OpenPropertyStore(device, STGM_READ, &props);
   if (SUCCEEDED(hr))
   {
      PROPVARIANT name;

      pPropVariantInit(&name);
      hr = pIPropertyStore_GetValue(props, (const PROPERTYKEY*)pDEVPKEY_Device_FriendlyName, &name);
      if (SUCCEEDED(hr)) {
         rv = wcharToChar(name.pwszVal);
      }
      pPropVariantClear(&name);
      pIPropertyStore_Release(props);
   }

   return rv;
}

static char*
wcharToChar(const WCHAR* str)
{
   char *rv = NULL;
   int len;

   len = pWideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
   if (len > 0)
   {
      rv = calloc(1, len);
      pWideCharToMultiByte(CP_ACP, 0, str, -1, rv, len, NULL, NULL);
   }
   return rv;
}

static WCHAR*
charToWChar(const char* str)
{
   WCHAR *rv = NULL;
   int len;

   len = pMultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
   if (len > 0)
   {
      rv = calloc(sizeof(WCHAR), len);
      pMultiByteToWideChar(CP_ACP, 0, str, -1, rv, len);
   }
   return rv;
}

static LPWSTR
name_to_id(const WCHAR* dname, char m)
{
   const EDataFlow _mode[] = { eCapture, eRender };
   IMMDeviceCollection *collection = NULL;
   IMMDeviceEnumerator *enumerator = NULL;
   LPWSTR rv = 0;
   HRESULT hr;

   hr = pCoInitialize(NULL);
   hr = pCoCreateInstance(pCLSID_MMDeviceEnumerator, NULL,
                          CLSCTX_INPROC_SERVER, pIID_IMMDeviceEnumerator,
                          (void**)&enumerator);
   hr = pIMMDeviceEnumerator_EnumAudioEndpoints(enumerator, _mode[m],
                                                   DEVICE_STATE_ACTIVE,
                                                   &collection);
   if (SUCCEEDED(hr))
   {
      IPropertyStore *props = NULL;
      IMMDevice *device = NULL;
      UINT i, count;

      hr = pIMMDeviceCollection_GetCount(collection, &count);
      for(i=0; SUCCEEDED(hr) && (i<count); i++)
      {
         LPWSTR wstr = NULL;
         PROPVARIANT name;

         hr = pIMMDeviceCollection_Item(collection, i, &device);
         if (FAILED(hr)) break;

         hr = pIMMDevice_GetId(device, &wstr);
         if (FAILED(hr)) break;

         hr = pIMMDevice_OpenPropertyStore(device, STGM_READ, &props);
         if (FAILED(hr)) break;

         pPropVariantInit(&name);
         hr = pIPropertyStore_GetValue(props,
                               (const PROPERTYKEY*)pDEVPKEY_Device_FriendlyName,
                               &name);
         if (FAILED(hr)) break;

         if (!wcsncmp(name.pwszVal, dname, wcslen(dname)))
         {
            rv = wstr;
            break;
         }

         pCoTaskMemFree(wstr);
         wstr = NULL;

         pPropVariantClear(&name);
         pIPropertyStore_Release(props);
         pIMMDevice_Release(device);
      }
      pIMMDeviceEnumerator_Release(enumerator);
      pIMMDeviceCollection_Release(collection);
      pCoUninitialize();
   }
   pCoUninitialize();

   return rv;
}
