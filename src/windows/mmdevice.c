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

#include <aax/aax.h>
#include <xml.h>

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>
#include <base/dlsym.h>
#include <base/types.h>
#include <base/logging.h>
#include <base/threads.h>

#include "audio.h"
#include "mmdevice.h"

#define MAX_ID_STRLEN		32
#define DEFAULT_RENDERER	"MMDevice"
#define DEFAULT_DEVNAME		NULL
#define USE_EVENT_THREAD	1

static _aaxDriverDetect _aaxMMDevDriverDetect;
static _aaxDriverNewHandle _aaxMMDevDriverNewHandle;
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
static _aaxDriverThread _aaxMMDevDriverThread;
static _aaxDriverState _aaxMMDevDriverIsReachable;
static _aaxDriverState _aaxMMDevDriverAvailable;
static _aaxDriver3dMixerCB _aaxMMDevDriver3dMixer;

static char _mmdev_default_renderer[100] = DEFAULT_RENDERER;
static const EDataFlow _mode[] = { eCapture, eRender };

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
   (_aaxDriverNewHandle *)&_aaxMMDevDriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxMMDevDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxMMDevDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxMMDevDriverGetName,
#if USE_EVENT_THREAD
   (_aaxDriverThread *)&_aaxMMDevDriverThread,
#else
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,
#endif

   (_aaxDriverConnect *)&_aaxMMDevDriverConnect,
   (_aaxDriverDisconnect *)&_aaxMMDevDriverDisconnect,
   (_aaxDriverSetup *)&_aaxMMDevDriverSetup,
   (_aaxDriverState *)&_aaxMMDevDriverPause,
   (_aaxDriverState *)&_aaxMMDevDriverResume,
   (_aaxDriverCaptureCallback *)&_aaxMMDevDriverCapture,
   (_aaxDriverCallback *)&_aaxMMDevDriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxFileDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxMMDevDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxFileDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)*_aaxMMDevDriverAvailable,
   (_aaxDriverState *)*_aaxMMDevDriverAvailable,
   (_aaxDriverState *)*_aaxMMDevDriverIsReachable
};

typedef struct
{
   WCHAR *devname;
   LPWSTR devid;

   IMMDeviceEnumerator *pEnumerator;
   IMMDevice *pDevice;
   HANDLE Event;

   IMMNotificationClient *pNotify;
   IAudioClient *pAudioClient;
   union
   {
      IAudioRenderClient *pRender;
      IAudioCaptureClient *pCapture;
   } uType;

   WAVEFORMATEX *pFormat;
   EDataFlow Mode;

   REFERENCE_TIME hnsPeriod;
   REFERENCE_TIME hnsLatency;

   char paused;
   char initializing;
   char exclusive;
   char sse_level;

   char *ifname[2];
   _oalRingBufferMix1NFunc *mix_mono3d;

} _driver_t;

const char* _mmdev_default_name = DEFAULT_DEVNAME;

# define pCLSID_MMDeviceEnumerator &CLSID_MMDeviceEnumerator
# define pIID_IMMDeviceEnumerator &IID_IMMDeviceEnumerator
# define pIID_IAudioRenderClient &IID_IAudioRenderClient
# define pIID_IAudioClient &IID_IAudioClient
# define pPKEY_Device_DeviceDesc &PKEY_Device_DeviceDesc
# define pPKEY_Device_FriendlyName &PKEY_Device_FriendlyName
# define pPKEY_DeviceInterface_FriendlyName &PKEY_DeviceInterface_FriendlyName
# define pPKEY_Device_FriendlyName &PKEY_Device_FriendlyName
# define pDEVPKEY_Device_FriendlyName &DEVPKEY_Device_FriendlyName

# define pPropVariantInit PropVariantInit
# define pPropVariantClear PropVariantClear
# define pWideCharToMultiByte WideCharToMultiByte
# define pMultiByteToWideChar MultiByteToWideChar
# define pCoInitializeEx CoInitializeEx
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
# define pIMMDevice_GetState IMMDevice_GetState
# define pIMMDevice_Release IMMDevice_Release
# define pIMMDevice_GetId IMMDevice_GetId
# define pIAudioClient_Start IAudioClient_Start
# define pIAudioClient_Stop IAudioClient_Stop
# define pIAudioClient_GetService IAudioClient_GetService
# define pIAudioClient_Initialize IAudioClient_Initialize
# define pIAudioClient_GetMixFormat IAudioClient_GetMixFormat
# define pIAudioClient_GetDevicePeriod IAudioClient_GetDevicePeriod
# define pIAudioClient_GetBufferSize IAudioClient_GetBufferSize
# define pIAudioClient_GetStreamLatency IAudioClient_GetStreamLatency
# define pIAudioClient_GetCurrentPadding IAudioClient_GetCurrentPadding
# define pIAudioClient_Release IAudioClient_Release
# define pIAudioClient_IsFormatSupported IAudioClient_IsFormatSupported
# define pIAudioRenderClient_GetBuffer IAudioRenderClient_GetBuffer
# define pIAudioRenderClient_ReleaseBuffer IAudioRenderClient_ReleaseBuffer
# define pIAudioRenderClient_Release IAudioRenderClient_Release
# define pIMMDevice_OpenPropertyStore IMMDevice_OpenPropertyStore
# define pIPropertyStore_GetValue IPropertyStore_GetValue
# define pIPropertyStore_Release IPropertyStore_Release
# define pIAudioCaptureClient_GetBuffer IAudioCaptureClient_GetBuffer
# define pIAudioCaptureClient_ReleaseBuffer IAudioCaptureClient_ReleaseBuffer
# define pIAudioCaptureClient_GetNextPacketSize IAudioCaptureClient_GetNextPacketSize

static LPWSTR name_to_id(const WCHAR*, char);
static char* detect_devname(IMMDevice*);
static char* wcharToChar(const WCHAR*);
static WCHAR* charToWChar(const char*);
static const char* aaxNametoMMDevciceName(const char*);

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
      rv = AAX_TRUE;
   }

   return rv;
}

static void *
_aaxMMDevDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (handle)
   {
      handle->Mode = _mode[(mode > 0) ? 1 : 0];
      handle->initializing = 0;
      handle->paused = AAX_FALSE;
      handle->exclusive = AAX_TRUE;
      handle->sse_level = _aaxGetSSELevel();
      handle->mix_mono3d = _oalRingBufferMixMonoGetRenderer(mode);
   }

   return handle;
}

static void *
_aaxMMDevDriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;
   WAVEFORMATEX fmt;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      handle = _aaxMMDevDriverNewHandle(mode);
   }

   if (handle)
   {
      fmt.nSamplesPerSec = _aaxMMDevDriverBackend.rate;
      fmt.nChannels = _aaxMMDevDriverBackend.tracks;
      fmt.wFormatTag = WAVE_FORMAT_PCM;
      fmt.wBitsPerSample = 16;
      fmt.cbSize = 0;

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
                  handle->devname = charToWChar(aaxNametoMMDevciceName(s));
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

         // TODO: fix non exclusive mode
#if 0
         if (xmlNodeGetBool(xid, "virtual-mixer")) {
            handle->exclusive = AAX_FALSE;
         }
#endif
      }
   }


   if (handle)
   {
      HRESULT hr;
      int m;

      m = (mode > 0) ? 1 : 0;
      pCoInitializeEx(NULL, 0);
      hr = pCoCreateInstance(pCLSID_MMDeviceEnumerator, NULL,
                             CLSCTX_INPROC_SERVER, pIID_IMMDeviceEnumerator,
                             (void**)&handle->pEnumerator);
      if (SUCCEEDED(hr))
      {
         if (renderer && strcmp(renderer, "default"))
         {
            handle->devname = charToWChar(aaxNametoMMDevciceName(renderer));
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

         if (SUCCEEDED(hr))
         {
            hr = pIMMDevice_Activate(handle->pDevice, pIID_IAudioClient,
                                     CLSCTX_INPROC_SERVER, NULL,
                                     &handle->pAudioClient);
            if (SUCCEEDED(hr))
            {
               WAVEFORMATEX *wfmt = &fmt;
               hr = pIAudioClient_GetMixFormat(handle->pAudioClient, &wfmt);
               if (SUCCEEDED(hr)) {
                  handle->pFormat = wfmt;
               }
            }
         }

         if (FAILED(hr))
         {
            pIMMDeviceEnumerator_Release(handle->pEnumerator);
            pCoUninitialize();
            free(handle);
            handle = 0;
         }
      }
   }

   return (void *)handle;
}

static int
_aaxMMDevDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;

   if (handle)
   {
      HRESULT hr;

      CloseHandle(handle->Event);

      hr = pIAudioClient_Stop(handle->pAudioClient);
      if (FAILED(hr)) {
         _AAX_SYSLOG("mmdev; unable to stop the audio uType");
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
      if (handle->uType.pRender != NULL) 
      {
         pIAudioRenderClient_Release(handle->uType.pRender);
         handle->uType.pRender = NULL;
      }

      pCoTaskMemFree(handle->pFormat);

      free(handle->ifname[0]);
      free(handle->ifname[1]);
      if (handle->devname)
      {
         free(handle->devname);
         handle->devname = 0;
         pCoTaskMemFree(handle->devid);
         handle->devid = 0;
      }

      pCoUninitialize();
      free(handle);
 
      return AAX_TRUE;
   }
   return AAX_FALSE;
}

static int
_aaxMMDevDriverSetup(const void *id, size_t *frames, int *format,
                   unsigned int *tracks, float *speed)
{
   _driver_t *handle = (_driver_t *)id;
   REFERENCE_TIME hnsBufferDuration;
   REFERENCE_TIME hnsPeriodicity;
   unsigned int channels, freq;
   int bps, no_samples = 1024;
   WAVEFORMATEX *wfx, fmt;
   AUDCLNT_SHAREMODE mode;
   int frame_sz;
   DWORD stream;
   HRESULT hr;

   assert(handle);

   freq = (unsigned int)*speed;

   if (frames && *frames) {
      no_samples = *frames;
   }

   channels = *tracks;
   if (channels > handle->pFormat->nChannels) {
      channels = handle->pFormat->nChannels;
   }
   if (channels > 2) // TODO: for now
   {
      char str[255];
      snprintf((char *)&str, 255, "mmdev; Unable to output to %i speakers in "
                "this setup (2 is the maximum)", *tracks);
      _AAX_SYSLOG(str);
      return AAX_FALSE;
   }

   bps = aaxGetBytesPerSample(*format);
   frame_sz = channels * bps;

   /*
    * For shared mode set wfx to point to a valid, non-NULL pointer variable.
    * For exclusive mode, set wfx to NULL. The method allocates the storage for
    * the structure. The caller is responsible for freeing the storage, when it
    * is no longer needed, by calling the CoTaskMemFree function. If the
    * IsFormatSupported call fails and wfx is non-NULL, the method sets *wfx
    * to NULL.
    */
   if (handle->exclusive)
   {
      wfx = NULL;
      mode = AUDCLNT_SHAREMODE_EXCLUSIVE;
   }
   else
   {
      wfx = handle->pFormat;
      mode = AUDCLNT_SHAREMODE_SHARED;
   }
   
   fmt.nSamplesPerSec = freq;
   fmt.wFormatTag = WAVE_FORMAT_PCM;
   fmt.wBitsPerSample = bps*8;
   fmt.nChannels = channels;
   fmt.nBlockAlign = frame_sz;
   fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
   fmt.cbSize = 0;
 
   hr = pIAudioClient_IsFormatSupported(handle->pAudioClient, mode, &fmt, &wfx);
   if (FAILED(hr) && !handle->exclusive)
   {
      WAVEFORMATEX *wfmt = &fmt;
      _AAX_SYSLOG("mmdev; no device format found, trying mixer format");
      hr = pIAudioClient_GetMixFormat(handle->pAudioClient, &wfmt);
      if (SUCCEEDED(hr)) {
         wfx = wfmt;
      }
   }

   if (FAILED(hr))
   {
      // TODO: fix non exclusive mode
#if 0
      if (handle->exclusive)
      {
         _AAX_SYSLOG("mmdev; failed in exclusive mode, trying shared");
         handle->exclusive = AAX_FALSE;

         wfx = handle->pFormat;
         mode = AUDCLNT_SHAREMODE_SHARED;

         hr = pIAudioClient_IsFormatSupported(handle->pAudioClient,
                                              mode, &fmt, &wfx);
         if (FAILED(hr)) {
            _AAX_SYSLOG("mmdev; unable to get a proper format");
         }
      }
#endif
      if (FAILED(hr)) {
         return AAX_FALSE;
      }
   }

   if (handle->exclusive && wfx)
   {
      pCoTaskMemFree(handle->pFormat);
      handle->pFormat = wfx;
   }
   else if (!handle->exclusive || !wfx)
   {
      if (!wfx) wfx = &fmt;
      handle->pFormat->nSamplesPerSec = wfx->nSamplesPerSec;
      handle->pFormat->wFormatTag = WAVE_FORMAT_PCM;
      handle->pFormat->wBitsPerSample = wfx->wBitsPerSample;
      handle->pFormat->nChannels = wfx->nChannels;
      handle->pFormat->nBlockAlign = wfx->nBlockAlign;
      handle->pFormat->nAvgBytesPerSec = wfx->nAvgBytesPerSec;
      handle->pFormat->cbSize = 0;
   }
   else {
      _AAX_SYSLOG("mmdev; uncaught format error");
   }
   *speed = (float)handle->pFormat->nSamplesPerSec;
   *tracks = handle->pFormat->nChannels;

   /*
    * The buffer capacity as a time value. This parameter is of type
    * REFERENCE_TIME and is expressed in 100-nanosecond units. This parameter
    * contains the buffer size that the caller requests for the buffer that the
    * audio application will share with the audio engine (in shared mode) or 
    * with the endpoint device (in exclusive mode). If the call succeeds, the 
    * method allocates a buffer that is a least this large
    */
   stream = 0;
   freq = (unsigned int)handle->pFormat->nSamplesPerSec;
   hnsBufferDuration = (REFERENCE_TIME)(0.5f+10000000.0f*no_samples/freq);
   if (handle->exclusive)
   {
#if USE_EVENT_THREAD
      stream = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
#endif
      hnsPeriodicity = hnsBufferDuration;
   }
   else
   {
      stream =  AUDCLNT_STREAMFLAGS_RATEADJUST;
#if USE_EVENT_THREAD
      stream |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
#endif
      hnsPeriodicity = 0;
   }

   pCoInitializeEx(NULL, 0);
   hr = pIAudioClient_Initialize(handle->pAudioClient, mode, stream,
                                 hnsBufferDuration, hnsPeriodicity,
                                 handle->pFormat, NULL);
   if (SUCCEEDED(hr))
   {
      REFERENCE_TIME latency;
      UINT32 bufFrameCnt;
      BYTE *d = NULL;

      /* get the actual buffer size */
      hr = pIAudioClient_GetBufferSize(handle->pAudioClient, &bufFrameCnt);
      if (SUCCEEDED(hr))
      {
         if (frames) *frames = bufFrameCnt;

         hr = pIAudioClient_GetService(handle->pAudioClient,
                                       pIID_IAudioRenderClient,
                                       (void**)&handle->uType.pRender);
         if (FAILED(hr)) {
            _AAX_SYSLOG("mmdev; faild to get audio servcie");
         }
      }
      else {
         _AAX_SYSLOG("mmdev; unable to het device buffer size");
      }

      hr = pIAudioClient_GetStreamLatency(handle->pAudioClient, &latency);
      if (SUCCEEDED(hr)) {
         handle->hnsLatency = latency;
      }
   }
   else {
      _AAX_SYSLOG("mmdev; failed to initialize");
   }
   pCoUninitialize();

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
      handle->paused = AAX_TRUE;
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
#if USE_EVENT_THREAD
      if (handle->initializing == 1)
      {
         handle->Event = CreateEvent(NULL, FALSE, FALSE, NULL);
         if (handle->Event)
         {
            HRESULT hr = IAudioClient_SetEventHandle(handle->pAudioClient,
                                                     handle->Event);
            if (SUCCEEDED(hr)) {
               handle->initializing--;
            }
         } else {
            _AAX_SYSLOG("mmdev; unable to set up the event handler");
         }
      }
#endif

      if (!handle->initializing)
      {
         HRESULT hr = pIAudioClient_Start(handle->pAudioClient);
         rv = (SUCCEEDED(hr)) ? AAX_TRUE : AAX_FALSE;
      } else {
         rv = AAX_TRUE;
      }
      handle->paused = AAX_FALSE;
   }
   return rv;
}

static int
_aaxMMDevDriverAvailable(const void *id)
{
   return AAX_TRUE;
}

static int
_aaxMMDevDriverIsReachable(const void *id)
{
    _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;

   if (handle && handle->pDevice)
   {
      DWORD state;
      HRESULT hr;

      hr = pIMMDevice_GetState(handle->pDevice, &state);
      if (SUCCEEDED(hr)) {
         rv = (state == DEVICE_STATE_ACTIVE) ? AAX_TRUE : AAX_FALSE;
      }
   }

   return rv;
}

int
_aaxMMDevDriver3dMixer(const void *id, void *d, void *s, void *p, void *m, int n, unsigned char ctr)
{
   _driver_t *handle = (_driver_t *)id;
   float gain;
   int ret;

   assert(s);
   assert(d);
   assert(p);

   gain = _aaxMMDevDriverBackend.gain;
   ret = handle->mix_mono3d(d, s, p, m, gain, n, ctr);

   return ret;
}


static int
_aaxMMDevDriverCapture(const void *id, void **data, size_t *frames, void *scratch)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;

   if ((frames == 0) || (data == 0))
      return AAX_FALSE;

   if (*frames == 0)
      return AAX_TRUE;

   *frames = 0;
   if (data)
   {
      UINT32 packet_sz = *frames;
      HRESULT hr;

      if (handle->initializing)
      {
         if (handle->initializing == 1) {
            hr = pIAudioClient_Start(handle->pAudioClient);
            if (FAILED(hr)) {
               handle->initializing++;
            }
         }
         handle->initializing--;
      }

      if (!handle->exclusive)
      {
         hr = pIAudioCaptureClient_GetNextPacketSize(handle->uType.pCapture,
                                                     &packet_sz);
         if (FAILED(hr)) {
             packet_sz = 0;
         }
      }

      if (packet_sz)
      {
         UINT32 avail = 0;
         DWORD flags;
         BYTE* buf;

         do
         {
            hr = pIAudioCaptureClient_GetBuffer(handle->uType.pCapture, &buf,
                                                &avail, &flags, NULL, NULL);
            if (SUCCEEDED(hr))
            {
               if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0) {
                  _batch_cvt24_16_intl((int32_t**)data, buf, 0, 2, avail);
               }
               pIAudioCaptureClient_ReleaseBuffer(handle->uType.pCapture,avail);
               packet_sz -= avail;
               *frames += avail;
            }
            else {
               break;
            }
         }
         while (packet_sz);

         if (packet_sz) {
            _AAX_SYSLOG("mmdev; failed to get next packet");
         } else {
            rv = AAX_TRUE;
         }
      }
   }

   return rv;
}


static int
_aaxMMDevDriverPlayback(const void *id, void *d, void *s, float pitch, float volume)
{
   _oalRingBuffer *rb = (_oalRingBuffer *)s;
   _driver_t *handle = (_driver_t *)id;
   unsigned int offs, no_tracks, no_samples;
   UINT32 padding = 0, frames = 0; 
   _oalRingBufferSample *rbd;
   BYTE *data = NULL;
   HRESULT hr;

   assert(rb);
   assert(rb->sample);
   assert(id != 0);

   rbd = rb->sample;
   offs = _oalRingBufferGetOffsetSamples(rb);
   no_tracks = _oalRingBufferGetNoTracks(rb);
   no_samples = _oalRingBufferGetNoSamples(rb) - offs;

   if (handle->initializing)
   {
      if (handle->initializing == 1) {
         hr = pIAudioClient_Start(handle->pAudioClient);
         if (FAILED(hr)) {
            handle->initializing++;
         }
      }
      handle->initializing--;
   }

   if (!handle->exclusive)
   {
      hr = pIAudioClient_GetCurrentPadding(handle->pAudioClient, &padding);
      if (SUCCEEDED(hr))
      {
         if (no_samples > padding) {
            frames = no_samples - padding;
         } else {
            frames = no_samples;
         }
      } else {
         frames = 0;
      }
   } else {
      frames = no_samples;
   }

   if ((frames == no_samples) && (no_samples != padding))
   {
      hr = pIAudioRenderClient_GetBuffer(handle->uType.pRender, frames, &data);
      if (SUCCEEDED(hr))
      {
         if (handle->pFormat->wBitsPerSample == 16) {
            _batch_cvt16_intl_24(data, (const int32_t**)rbd->track, offs,
                                 no_tracks, frames);
         } else if (handle->pFormat->wBitsPerSample == 32) {
            _batch_cvt32_intl_24(data, (const int32_t**)rbd->track, offs,
                                 no_tracks, frames);
         } else {
            _batch_cvt8_intl_24(data, (const int32_t**)rbd->track, offs,
                                no_tracks, frames);
         }

         if (is_bigendian()) {
            _batch_endianswap16((uint16_t*)data, no_tracks*frames);
         }

         hr = pIAudioRenderClient_ReleaseBuffer(handle->uType.pRender,
                                             frames, 0);
         if (FAILED(hr)) {
            _AAX_SYSLOG("mmdev; failed to release the buffer");
         }
      }
      else {
         _AAX_SYSLOG("mmdev; failed to get the buffer");
      }
   }

   return 0;
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

   pCoInitializeEx(NULL, 0);
   hr = pCoCreateInstance(pCLSID_MMDeviceEnumerator, NULL,
                          CLSCTX_INPROC_SERVER, pIID_IMMDeviceEnumerator,
                          (void**)&enumerator);
   if (SUCCEEDED(hr))
   {
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
                         (const PROPERTYKEY*)pPKEY_DeviceInterface_FriendlyName,
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
      pCoTaskMemFree(wstr);
      if (enumerator) pIMMDeviceEnumerator_Release(enumerator);
      if (collection) pIMMDeviceCollection_Release(collection);
      if (device) pIMMDevice_Release(device);
      if (props) pIPropertyStore_Release(props);
      pCoUninitialize();
   }

   return (char *)&names[mode];
}

static char *
_aaxMMDevDriverGetInterfaces(const void *id, const char *devname, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   int m = (mode > 0) ? 1 : 0;
   char *rv = 0;

   if (handle && !rv)
   {
      IMMDeviceEnumerator *enumerator = NULL;
      char name[1024] = "\0\0";
      int len = 1024;
      HRESULT hr;

      pCoInitializeEx(NULL, 0);
      hr = pCoCreateInstance(pCLSID_MMDeviceEnumerator, NULL,
                             CLSCTX_INPROC_SERVER, pIID_IMMDeviceEnumerator,
                             (void**)&enumerator);
      if (SUCCEEDED(hr))
      {
         IMMDeviceCollection *collection = NULL;
         IPropertyStore *props = NULL;
         IMMDevice *device = NULL;
         LPWSTR wstr = NULL;
         UINT i, count;
         char *ptr;

         ptr = name;
         hr = pIMMDeviceEnumerator_EnumAudioEndpoints(enumerator, _mode[m],
                                                      DEVICE_STATE_ACTIVE,
                                                      &collection);
         if (FAILED(hr)) goto Exit;

         hr = pIMMDeviceCollection_GetCount(collection, &count);
         for(i=0; SUCCEEDED(hr) && (i<count); i++)
         {
            PROPVARIANT name;
            char *device_name;

            hr = pIMMDeviceCollection_Item(collection, i, &device);
            if (FAILED(hr)) break;

            hr = pIMMDevice_GetId(device, &wstr);
            if (FAILED(hr)) break;

            hr = pIMMDevice_OpenPropertyStore(device, STGM_READ, &props);
            if (FAILED(hr)) break;

            pPropVariantInit(&name);
            hr = pIPropertyStore_GetValue(props,
                         (const PROPERTYKEY*)pPKEY_DeviceInterface_FriendlyName,
                               &name);
            if (FAILED(hr)) break;

            device_name = wcharToChar(name.pwszVal);
            if (device_name && !strcasecmp(device_name, devname))
            {
               PROPVARIANT iface;

               hr = pIPropertyStore_GetValue(props,
                                    (const PROPERTYKEY*)pPKEY_Device_DeviceDesc,
                                    &iface);
               if (SUCCEEDED(hr))
               {
                  char *if_name = wcharToChar(iface.pwszVal);
                  if (if_name)
                  {
                     int slen;

                     snprintf(ptr, len, "%s", if_name);
                     free(if_name);

                     slen = strlen(ptr)+1;
                     len -= slen;
                     ptr += slen;
                  }
                  pPropVariantClear(&iface);
               }
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

         *ptr++ = 0;
         rv = handle->ifname[m] = malloc(ptr-name);
         memcpy(handle->ifname[m], name, ptr-name);

         return rv;
Exit:
         pCoTaskMemFree(wstr);
         if (enumerator) pIMMDeviceEnumerator_Release(enumerator);
         if (collection) pIMMDeviceCollection_Release(collection);
         if (device) pIMMDevice_Release(device);
         if (props) pIPropertyStore_Release(props);
         pCoUninitialize();
      }
   }

   return rv;
}


/* -------------------------------------------------------------------------- */

const char *
aaxNametoMMDevciceName(const char *devname)
{
   static char name[256];
   *name = 0;

   if (devname)
   {
      char *ptr, dev[256];

      snprintf(dev, 256, "%s", devname);
      ptr =  strstr(dev, ": ");
      if (ptr)
      {
         *ptr++ = 0;
         snprintf(name, 256, "%s (%s)", ++ptr, dev);
      }
   }
   return (const char*)&name;
}

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
      hr = pIPropertyStore_GetValue(props, (const PROPERTYKEY*)pPKEY_Device_FriendlyName, &name);
      if (SUCCEEDED(hr)) {
         rv = wcharToChar(name.pwszVal);
      }
      pPropVariantClear(&name);
      pIPropertyStore_Release(props);
   }

   return rv;
}

static char*
wcharToChar(const WCHAR* wstr)
{
   char *rv = NULL;
   int len;

   len = pWideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
   if (len > 0)
   {
      rv = calloc(1, len+1);
      pWideCharToMultiByte(CP_ACP, 0, wstr, -1, rv, len, NULL, NULL);
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
      rv = calloc(sizeof(WCHAR), len+1);
      pMultiByteToWideChar(CP_ACP, 0, str, -1, rv, len);
   }
   return rv;
}

static LPWSTR
name_to_id(const WCHAR* dname, char m)
{
   IMMDeviceCollection *collection = NULL;
   IMMDeviceEnumerator *enumerator = NULL;
   LPWSTR rv = 0;
   HRESULT hr;

   pCoInitializeEx(NULL, 0);
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
                               (const PROPERTYKEY*)pPKEY_Device_FriendlyName,
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
   }
   pCoUninitialize();

   return rv;
}

void *
_aaxMMDevDriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   _intBufferData *dptr_sensor;
   const _aaxDriverBackend *be;
   _oalRingBuffer *dest_rb;
   _aaxAudioFrame *mixer;
   _driver_t *be_handle;
   unsigned int nsamps;
   float delay_sec;
   int stdby_time;
   char state;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return NULL;
   }

   dest_rb = _oalRingBufferCreate(AAX_TRUE);
   if (!dest_rb) {
      return NULL;
   }

   delay_sec = 1.0f/handle->info->refresh_rate;

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

   nsamps = 0;
   stdby_time = 2*(int)(delay_sec*1000);
   be->pause(handle->backend.handle);
   state = AAX_SUSPENDED;

   be_handle = (_driver_t *)handle->backend.handle;
#if USE_EVENT_THREAD
   be_handle->Event = CreateEvent(NULL, FALSE, FALSE, NULL);
   if (be_handle->Event) {
      IAudioClient_SetEventHandle(be_handle->pAudioClient, be_handle->Event);
   } else {
      _AAX_SYSLOG("mmdev; unable to set up the event handler");
   }
#endif


   _aaxMutexLock(handle->thread.mutex);
   do
   {
      if TEST_FOR_FALSE(handle->thread.started) {
         break;
      }

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

      _aaxMutexUnLock(handle->thread.mutex);
      if (_IS_PLAYING(handle) && be->is_available(be_handle))
      {
                                /* timeout is in miliseconds */
         DWORD r = WaitForSingleObjectEx(be_handle->Event, stdby_time, FALSE);
         switch (r)
         {
         case WAIT_OBJECT_0:
            break;
         case WAIT_TIMEOUT:
            if (!be_handle->initializing) {
               _AAX_SYSLOG("mmdev; snd_pcm_wait polling error");
            }
            break;
         case WAIT_ABANDONED:
         case WAIT_FAILED:
         default:
            _AAX_SYSLOG("mmdev; snd_pcm_wait polling failed");
            break;
         }
      }
      else {
         msecSleep(delay_sec*1000);
      }
      _aaxMutexLock(handle->thread.mutex);
   }
   while TEST_FOR_TRUE(handle->thread.started);
   _aaxMutexUnLock(handle->thread.mutex);

   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      _oalRingBufferStop(mixer->ringbuffer);
      _oalRingBufferDelete(mixer->ringbuffer);
   }
   return handle;
}
