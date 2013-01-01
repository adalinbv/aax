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
#include <stdarg.h>		/* va_start */
#include <math.h>		/* roundf    */

#include <aax/aax.h>
#include <xml.h>

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>
#include <base/dlsym.h>
#include <base/timer.h>
#include <base/logging.h>
#include <base/threads.h>

#include "audio.h"
#include "wasapi.h"

#ifdef _MSC_VER
	/* disable the warning about _snprintf being depreciated */
# pragma warning(disable : 4995)
#endif

#define ENABLE_TIMING		AAX_FALSE
#define USE_EVENT_THREAD	AAX_TRUE
#define USE_CAPTURE_THREAD	AAX_FALSE
#define EXCLUSIVE_MODE		AAX_TRUE
#define USE_GETID		AAX_FALSE

#define DRIVER_INIT_MASK	0x0001
#define CAPTURE_INIT_MASK	0x0002
#define DRIVER_PAUSE_MASK	0x0004
#define EXCLUSIVE_MODE_MASK	0x0008
#define EVENT_DRIVEN_MASK	0x0010
#define CO_INIT_MASK		0x0080

// Testing purposes only!
#ifdef NDEBUG
# undef NDEBUG
#endif
#define PLAYBACK_PERIODS	2
#define CAPTURE_PERIODS		4
#define MAX_ID_STRLEN		32
#define DEFAULT_RENDERER	"WASAPI"
#define DEFAULT_DEVNAME		NULL
#define CBSIZE		sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX)

# define _AAX_DRVLOG(p, a)		_aaxWASAPIDriverLogPrio(p, a)
# define _AAX_DRVLOG_VAR(p, args...)	_aaxWASAPIDriverLogVar(p, args);


static _aaxDriverDetect _aaxWASAPIDriverDetect;
static _aaxDriverNewHandle _aaxWASAPIDriverNewHandle;
static _aaxDriverGetDevices _aaxWASAPIDriverGetDevices;
static _aaxDriverGetInterfaces _aaxWASAPIDriverGetInterfaces;
static _aaxDriverConnect _aaxWASAPIDriverConnect;
static _aaxDriverDisconnect _aaxWASAPIDriverDisconnect;
static _aaxDriverSetup _aaxWASAPIDriverSetup;
static _aaxDriverState _aaxWASAPIDriverPause;
static _aaxDriverState _aaxWASAPIDriverResume;
static _aaxDriverCaptureCallback _aaxWASAPIDriverCapture;
static _aaxDriverCallback _aaxWASAPIDriverPlayback;
static _aaxDriverGetName _aaxWASAPIDriverGetName;
static _aaxDriverThread _aaxWASAPIDriverThread;
static _aaxDriverState _aaxWASAPIDriverIsReachable;
static _aaxDriverState _aaxWASAPIDriverAvailable;
static _aaxDriver3dMixerCB _aaxWASAPIDriver3dMixer;
static _aaxDriverParam _aaxWASAPIDriverGetLatency;
static _aaxDriverLog _aaxWASAPIDriverLog;

static char _wasapi_default_renderer[100] = DEFAULT_RENDERER;
static const EDataFlow _mode[] = { eCapture, eRender };

const _aaxDriverBackend _aaxWASAPIDriverBackend =
{
   1.0,
   AAX_PCM16S,
   48000,
   2,

   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char*)&_wasapi_default_renderer,

   (_aaxCodec **)&_oalRingBufferCodecs,

   (_aaxDriverDetect *)&_aaxWASAPIDriverDetect,
   (_aaxDriverNewHandle *)&_aaxWASAPIDriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxWASAPIDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxWASAPIDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxWASAPIDriverGetName,
   (_aaxDriverThread *)&_aaxWASAPIDriverThread,

   (_aaxDriverConnect *)&_aaxWASAPIDriverConnect,
   (_aaxDriverDisconnect *)&_aaxWASAPIDriverDisconnect,
   (_aaxDriverSetup *)&_aaxWASAPIDriverSetup,
   (_aaxDriverState *)&_aaxWASAPIDriverPause,
   (_aaxDriverState *)&_aaxWASAPIDriverResume,
   (_aaxDriverCaptureCallback *)&_aaxWASAPIDriverCapture,
   (_aaxDriverCallback *)&_aaxWASAPIDriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxFileDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxWASAPIDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxFileDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)&_aaxWASAPIDriverAvailable,
   (_aaxDriverState *)&_aaxWASAPIDriverAvailable,
   (_aaxDriverState *)&_aaxWASAPIDriverIsReachable,

   (_aaxDriverParam *)&_aaxWASAPIDriverGetLatency
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

   WAVEFORMATEXTENSIBLE Fmt;
   EDataFlow Mode;

   REFERENCE_TIME hnsPeriod;
   REFERENCE_TIME hnsLatency;
   UINT32 buffer_frames;

   int status;
   char sse_level;

   char *ifname[2];
   _oalRingBufferMix1NFunc *mix_mono3d;
   enum aaxRenderMode setup;

   _batch_cvt_to_intl_proc cvt_to_intl;
   _batch_cvt_from_intl_proc cvt_from_intl;

   /* capture related */
   HANDLE thread;
   HANDLE shutdown_event;
   CRITICAL_SECTION mutex;

   char *scratch;
   void *scratch_ptr;
   unsigned int scratch_offs;	/* current offset in the scratch buffer */
   unsigned int threshold;	/* sensor buffer threshold for padding  */
   unsigned int packet_sz;

   float avail_padding;		/* calculated available no. frames      */
   float avail_avg;		/* avg. no. frames per capture call     */
   float padding;		/* for sensor clock drift correction    */

} _driver_t;
 
const char* _wasapi_default_name = DEFAULT_DEVNAME;

# define pCLSID_MMDeviceEnumerator &aax_CLSID_MMDeviceEnumerator
# define pIID_IMMDeviceEnumerator &aax_IID_IMMDeviceEnumerator
# define pIID_IAudioRenderClient &aax_IID_IAudioRenderClient
# define pIID_IAudioCaptureClient &aax_IID_IAudioCaptureClient
# define pIID_IAudioClient &aax_IID_IAudioClient
# define pPKEY_Device_DeviceDesc &PKEY_Device_DeviceDesc
# define pPKEY_Device_FriendlyName &PKEY_Device_FriendlyName
# define pPKEY_DeviceInterface_FriendlyName &PKEY_DeviceInterface_FriendlyName
# define pPKEY_Device_FriendlyName &PKEY_Device_FriendlyName
# define pKSDATAFORMAT_SUBTYPE_IEEE_FLOAT &aax_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
# define pKSDATAFORMAT_SUBTYPE_ADPCM &aax_KSDATAFORMAT_SUBTYPE_ADPCM
# define pKSDATAFORMAT_SUBTYPE_PCM &aax_KSDATAFORMAT_SUBTYPE_PCM

# define pIAudioClient_SetEventHandle IAudioClient_SetEventHandle

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
# define pIMMDevice_GetState IMMDevice_GetState
# define pIMMDevice_Release IMMDevice_Release
# define pIMMDevice_GetId IMMDevice_GetId
# define pIAudioClient_Start IAudioClient_Start
# define pIAudioClient_Stop IAudioClient_Stop
# define pIAudioClient_Reset IAudioClient_Reset
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
# define pIAudioCaptureClient_Release IAudioCaptureClient_Release
# define pIAudioRenderClient_Release IAudioRenderClient_Release
# define pIMMDevice_OpenPropertyStore IMMDevice_OpenPropertyStore
# define pIPropertyStore_GetValue IPropertyStore_GetValue
# define pIPropertyStore_Release IPropertyStore_Release
# define pIAudioCaptureClient_GetBuffer IAudioCaptureClient_GetBuffer
# define pIAudioCaptureClient_ReleaseBuffer IAudioCaptureClient_ReleaseBuffer
# define pIAudioCaptureClient_GetNextPacketSize IAudioCaptureClient_GetNextPacketSize


static const char* aaxNametoMMDevciceName(const char*);
static char* _aaxMMDeviceNameToName(char *);
static char *_aaxWASAPIDriverLogPrio(int, const char*);
static char *_aaxWASAPIDriverLogVar(int, const char *fmt, ...);

static LPWSTR name_to_id(const WCHAR*, unsigned char);
static char* detect_devname(IMMDevice*);
static char* wcharToChar(char*, int*, const WCHAR*);
static WCHAR* charToWChar(const char*);
static DWORD getChannelMask(WORD, enum aaxRenderMode);
static int copyFmtEx(WAVEFORMATEX*, WAVEFORMATEX*);
static int copyFmtExtensible(WAVEFORMATEXTENSIBLE*, WAVEFORMATEXTENSIBLE*);
static int exToExtensible(WAVEFORMATEXTENSIBLE*, WAVEFORMATEX*, enum aaxRenderMode);

static int _aaxWASAPIDriverCaptureFromHardware(_driver_t*);
static DWORD _aaxWASAPIDriverCaptureThread(LPVOID);

#ifndef UINT64_MAX
# define UINT64_MAX		(18446744073709551615ULL)
#endif

#if 0
static void displayError(LPTSTR);
#endif

static int
_aaxWASAPIDriverDetect(int mode)
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

      snprintf(_wasapi_default_renderer, MAX_ID_STRLEN, "%s %s",
               DEFAULT_RENDERER, hwstr);
      rv = AAX_TRUE;
   }

   return rv;
}

static void *
_aaxWASAPIDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (handle)
   {
      handle->Mode = _mode[(mode > 0) ? 1 : 0];
      handle->sse_level = _aaxGetSSELevel();
      handle->mix_mono3d = _oalRingBufferMixMonoGetRenderer(mode);
      handle->status = DRIVER_INIT_MASK | CAPTURE_INIT_MASK;
      handle->status |= DRIVER_PAUSE_MASK;
#if EXCLUSIVE_MODE
      handle->status |= EXCLUSIVE_MODE_MASK;
#endif
#if USE_EVENT_THREAD
      /* event threads are not supported for capturing prior to Win7 SP1 */
      /* nor for registered sensors                                      */
      if (handle->Mode == eRender) {
         handle->status |= EVENT_DRIVEN_MASK;
      }
#endif
   }

   return handle;
}

static void *
_aaxWASAPIDriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;
   WAVEFORMATEXTENSIBLE fmt;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      handle = _aaxWASAPIDriverNewHandle(mode);
   }

   if (handle)
   {
      handle->setup = mode;

      fmt.Format.nSamplesPerSec = _aaxWASAPIDriverBackend.rate;
      fmt.Format.nChannels = _aaxWASAPIDriverBackend.tracks;
      fmt.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
      fmt.Format.wBitsPerSample = 16;
      fmt.Format.cbSize = CBSIZE;

      fmt.Samples.wValidBitsPerSample = fmt.Format.wBitsPerSample;
      fmt.dwChannelMask = getChannelMask(fmt.Format.nChannels, mode);
      fmt.SubFormat = aax_KSDATAFORMAT_SUBTYPE_PCM;

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
               _AAX_DRVLOG(0, "wasapi; frequency too small.");
               i = _AAX_MIN_MIXER_FREQUENCY;
            }
            else if (i > _AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_DRVLOG(0, "wasapi; frequency too large.");
               i = _AAX_MAX_MIXER_FREQUENCY;
            }
            fmt.Format.nSamplesPerSec = i;
         }

         if (mode != AAX_MODE_READ)
         {
            i = xmlNodeGetInt(xid, "channels");
            if (i)
            {
               if (i < 1)
               {
                  _AAX_DRVLOG(0, "wasapi; no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_DRVLOG(0, "wasapi; no. tracks too great.");
                  i = _AAX_MAX_SPEAKERS;
               }
               fmt.Format.nChannels = i;
            }
         }

         i = xmlNodeGetInt(xid, "bits-per-sample");
         if (i)
         {
            if (i != 16)
            {
               _AAX_DRVLOG(0, "wasapi; unsopported bits-per-sample");
               i = 16;
            }
         }

         if (xmlNodeGetBool(xid, "virtual-mixer")) {
            handle->status &= ~EXCLUSIVE_MODE_MASK;
         }
      }
   }


   if (handle)
   {
      int m = (mode > 0) ? 1 : 0;
      HRESULT hr;

      /*
       * A thread enters an STA by specifying COINIT_APARTMENTTHREADED when it
       * calls CoInitializeEx(), or by simply calling CoInitialize() (calling
       * CoInitialize() will actually invoke CoInitializeEx() with
       * COINIT_APARTMENTTHREADED). A thread which has entered an STA is also
       * said to have created that apartment (after all, there are no other            * threads inside that apartment to first create it).
       *
       * If COM is already initialized CoInitialize will either return
       * FALSE, or RPC_E_CHANGED_MODE if it was initialised in a different
       * threading mode. In either case we shouldn't consider it an error
       * but we need to be careful to not call CoUninitialize() if
       * RPC_E_CHANGED_MODE was returned.
       */
      hr = pCoInitialize(NULL);
      if (hr != RPC_E_CHANGED_MODE) {
         handle->status |= CO_INIT_MASK;
      }

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

         if (!handle->devname)
         {
            hr = pIMMDeviceEnumerator_GetDefaultAudioEndpoint(
                                                 handle->pEnumerator, _mode[m],
                                                 eMultimedia, &handle->pDevice);
         }
         else {
            hr = pIMMDeviceEnumerator_GetDevice(handle->pEnumerator,
                                                handle->devid,
                                                &handle->pDevice);
         }

         if (hr == S_OK)
         {
            hr = pIMMDevice_Activate(handle->pDevice, pIID_IAudioClient,
                                     CLSCTX_INPROC_SERVER, NULL,
                                     (void**)&handle->pAudioClient);
            if (hr == S_OK)
            {
               WAVEFORMATEX *wfmt = (WAVEFORMATEX*)&fmt;
               hr = pIAudioClient_GetMixFormat(handle->pAudioClient, &wfmt);
               if (hr == S_OK) {
                  exToExtensible(&handle->Fmt, wfmt, handle->setup);
               }
            }
         }

         if (hr != S_OK)
         {
            _AAX_DRVLOG(10, "wasapi; failed to connect");
            pIMMDeviceEnumerator_Release(handle->pEnumerator);
            if (handle->status & CO_INIT_MASK) {
               pCoUninitialize();
            }
            free(handle);
            handle = 0;
         }
      }
   }

   return (void *)handle;
}

static int
_aaxWASAPIDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;

   if (handle)
   {
      HRESULT hr;

      _aaxWASAPIDriverPause(handle);

      if (handle->Event) {
         CloseHandle(handle->Event);
      }

      if (handle->pAudioClient != NULL)
      {
         hr = pIAudioClient_Stop(handle->pAudioClient);
         if (FAILED(hr)) {
            _AAX_DRVLOG(5, "wasapi; unable to stop the audio client");
         } else {
            pIAudioClient_Reset(handle->pAudioClient);
            handle->status |= CAPTURE_INIT_MASK;
         }
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
      if (handle->Mode == eRender)
      {
         if (handle->uType.pRender != NULL) 
         {
            pIAudioRenderClient_Release(handle->uType.pRender);
            handle->uType.pRender = NULL;
         }
      }
      else
      {
         if (handle->uType.pCapture != NULL)
         {
            pIAudioCaptureClient_Release(handle->uType.pCapture);
            handle->uType.pCapture = NULL;
         }
      }

      free(handle->ifname[0]);
      free(handle->ifname[1]);
      if (handle->devname)
      {
         free(handle->devname);
         handle->devname = 0;
         pCoTaskMemFree(handle->devid);
         handle->devid = 0;
      }

      _aax_free(handle->scratch_ptr);
      handle->scratch_ptr = 0;
      handle->scratch = 0;

      if (handle->status & CO_INIT_MASK) {
         pCoUninitialize();
      }

      free(handle);
 
      return AAX_TRUE;
   }
   return AAX_FALSE;
}

static int
_aaxWASAPIDriverSetup(const void *id, size_t *frames, int *format,
                   unsigned int *tracks, float *speed)
{
   _driver_t *handle = (_driver_t *)id;
   REFERENCE_TIME hnsBufferDuration;
   REFERENCE_TIME hnsPeriodicity;
   unsigned int samples = 1024;
   WAVEFORMATEXTENSIBLE fmt;
   AUDCLNT_SHAREMODE mode;
   int co_init, frame_sz;
   int channels, bps;
   DWORD stream;
   HRESULT hr;
   float freq;
   int rv = AAX_FALSE;

   assert(handle);

   freq = (float)*speed;
   if (frames && *frames) {
      samples = *frames;
   }

   /*
    * Adjust the number of samples to let the refresh rate be an
    * exact and EVEN number of miliseconds (rounded upwards).
    */
   samples = 2 + ((1000*samples/(unsigned int)freq) & 0xFFFFFFFE);
   samples *= (unsigned int)freq/1000;
   /* refresh rate adjustement */

   channels = *tracks;
   if (channels > handle->Fmt.Format.nChannels) {
      channels = handle->Fmt.Format.nChannels;
   }
   if (channels > 2) // TODO: for now
   {
      _AAX_DRVLOG_VAR(5, "wasapi; Unable to output to %i speakers in "
                      "this setup (2 is the maximum)", *tracks);
      return AAX_FALSE;
   }

   bps = aaxGetBytesPerSample(*format);
   frame_sz = channels * bps;

   co_init = AAX_FALSE;
   hr = pCoInitialize(NULL);
   if (hr != RPC_E_CHANGED_MODE) {
      co_init = AAX_TRUE;
   }

   do
   {
      WAVEFORMATEX *wfx = (WAVEFORMATEX*)&handle->Fmt.Format;
      const WAVEFORMATEX *pfmt = &fmt.Format;
      WAVEFORMATEX **cfmt = NULL;
      float pitch;

      /*
       * For shared mode set wfx to point to a valid, non-NULL pointer variable.
       * For exclusive mode, set wfx to NULL. The method allocates the storage
       * for the structure. The caller is responsible for freeing the storage,
       * when it is no longer needed, by calling the CoTaskMemFree function. If
       * the IsFormatSupported call fails and wfx is non-NULL, the method sets
       * *wfx to NULL.
       */
      if (handle->status & EXCLUSIVE_MODE_MASK) {
         mode = AUDCLNT_SHAREMODE_EXCLUSIVE;
      }
      else
      {
         mode = AUDCLNT_SHAREMODE_SHARED;
         cfmt = &wfx;
      }
 
      fmt.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
      fmt.Format.nSamplesPerSec = (unsigned int)freq;
      fmt.Format.nChannels = channels;
      fmt.Format.wBitsPerSample = bps*8;
      fmt.Format.nBlockAlign = frame_sz;
      fmt.Format.nAvgBytesPerSec = fmt.Format.nSamplesPerSec
                                   * fmt.Format.nBlockAlign;
      fmt.Format.cbSize = CBSIZE;

      fmt.Samples.wValidBitsPerSample = fmt.Format.wBitsPerSample;
      fmt.dwChannelMask = getChannelMask(fmt.Format.nChannels,handle->setup);
      fmt.SubFormat = aax_KSDATAFORMAT_SUBTYPE_PCM;
 
      hr = pIAudioClient_IsFormatSupported(handle->pAudioClient,mode,pfmt,cfmt);
      if (hr != S_OK)
      {
         /*
          * Succeeded but the specified format is not supported in exclusive
          * mode, try mixer format
          */
         if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT)
         {
            _AAX_DRVLOG(5, "wasapi; no device format found, trying mixer format");
            hr = pIAudioClient_GetMixFormat(handle->pAudioClient, &wfx);
            if (hr == S_OK)
            {
               hr = pIAudioClient_IsFormatSupported(handle->pAudioClient, mode,
                                                    &fmt.Format, &wfx);
            }
         }

         /*
          * eCapture mode must always be exclusive to prevent buffer
          * underflows in registered-sensor mode
          */
         if ((hr != S_OK) && (handle->Mode == eRender)
             && (handle->status & EXCLUSIVE_MODE_MASK))
         {
            _AAX_DRVLOG(8, "wasapi; failed in exclusive mode, trying shared");
            handle->status &= ~EXCLUSIVE_MODE_MASK;

            wfx = &handle->Fmt.Format;
            mode = AUDCLNT_SHAREMODE_SHARED;
            hr = pIAudioClient_IsFormatSupported(handle->pAudioClient, mode,
                                                 &fmt.Format, &wfx);
            if (hr == S_FALSE) hr = S_OK;
         }
         else
         {
            _AAX_DRVLOG(10, "wasapi; unable to get a proper audio  format");
            goto ExitSetup;
         }

         if (hr != S_OK) 
         {
            _AAX_DRVLOG(5, "wasapi; no device format found, trying mixer format");
            hr = pIAudioClient_GetMixFormat(handle->pAudioClient, &wfx);
         }

         if (hr != S_OK)
         {
            _AAX_DRVLOG(10, "wasapi; unable to get a proper mixer format");
            goto ExitSetup;
         }
      }


      if ((handle->status & EXCLUSIVE_MODE_MASK) && wfx) {
         exToExtensible(&handle->Fmt, wfx, handle->setup);
      }
      else if (((handle->status & EXCLUSIVE_MODE_MASK) == 0) || !wfx)
      {
         if (wfx) {
            exToExtensible(&handle->Fmt, wfx, handle->setup);
         }
         else {
            copyFmtExtensible(&handle->Fmt, &fmt);
         }
      }
      else {
         _AAX_DRVLOG(10, "wasapi; uncaught format error");
      }

      pitch = handle->Fmt.Format.nSamplesPerSec / freq;
      freq = (float)handle->Fmt.Format.nSamplesPerSec;
      *speed = (float)handle->Fmt.Format.nSamplesPerSec;
      *tracks = handle->Fmt.Format.nChannels;

      switch (handle->Fmt.Format.wBitsPerSample)
      {
      case 16:
         handle->cvt_to_intl = _batch_cvt16_intl_24;
         handle->cvt_from_intl = _batch_cvt24_16_intl;
         break;
      case 32:
         if (IsEqualGUID(&handle->Fmt.SubFormat,
                         pKSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
         {
            handle->cvt_to_intl = _batch_cvtps_intl_24;
            handle->cvt_from_intl = _batch_cvt24_ps_intl;
         }
         else if (handle->Fmt.Samples.wValidBitsPerSample == 24)
         {
            handle->cvt_to_intl = _batch_cvt24_intl_24;
            handle->cvt_from_intl = _batch_cvt24_24_intl;
         }
         else
         {
            handle->cvt_to_intl = _batch_cvt32_intl_24;
            handle->cvt_from_intl = _batch_cvt24_32_intl;
         }
         break;
      case 24:
         handle->cvt_to_intl = _batch_cvt24_3intl_24;
         handle->cvt_from_intl = _batch_cvt24_24_3intl;
         break;
      case 8:
         handle->cvt_to_intl = _batch_cvt8_intl_24;
         handle->cvt_from_intl = _batch_cvt24_8_intl;
         break;
      default:
         _AAX_DRVLOG(10, "wasapi; error: hardware format mismatch!\n");
         break;
      }

      /*
       * The buffer capacity as a time value. This parameter is of type
       * REFERENCE_TIME and is expressed in 100-nanosecond units. This
       * parametercontains the buffer size that the caller requests for the
       * buffer that the audio application will share with the audio engine
       * (in shared mode) or with the endpoint device (in exclusive mode). If
       * the call succeeds, the method allocates a buffer that is a least this
       * large.
       *
       * For a shared-mode stream that uses event-driven buffering, the caller
       * must set both hnsPeriodicity and hnsBufferDuration to 0. The
       * Initialize method determines how large a buffer to allocate based
       * on the method determines how large a buffer to allocate based on the
       * scheduling period of the audio engine.
       */
      samples = (int)ceilf(samples*pitch);
      if (samples & 0xF)
      {
         samples |= 0xF;
         samples++;
      }
  
      stream = 0;
      freq = (float)handle->Fmt.Format.nSamplesPerSec;

      hnsBufferDuration = (REFERENCE_TIME)rintf(10000000.0f*samples/freq);
      hnsPeriodicity = hnsBufferDuration;

#if 0
# if USE_CAPTURE_THREAD
# else
      if (handle->Mode == eCapture)
      {			/* use the minimum EVEN period size for capturing */
         hr = pIAudioClient_GetDevicePeriod(handle->pAudioClient, NULL,
                                            &hnsPeriodicity);
//       hnsPeriodicity = (((hnsPeriodicity/10000) & 0xFFFFFFFE) + 2) * 10000;
      }
# endif
#endif

      if ((freq > 44000 && freq < 44200) || (freq > 21000 && freq < 22000)) {
         hnsBufferDuration = 3*hnsBufferDuration/2;
      }

      if (handle->status & EVENT_DRIVEN_MASK)
      {
         stream = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
         if ((handle->status & EXCLUSIVE_MODE_MASK) == 0) /* shared mode */
         {
            hnsBufferDuration = 0;
            hnsPeriodicity = 0;
         }
         else {
            hnsBufferDuration = hnsPeriodicity;
         }
      }
      else /* timer driver, w. 1ms accuracy */
      {
         if (handle->Mode == eRender)
         {
            hnsBufferDuration /= 10000;	/* whole ms periods */
            hnsBufferDuration *= 10000*PLAYBACK_PERIODS;
         }
         else {
            hnsBufferDuration *= CAPTURE_PERIODS;
         }

         if ((handle->status & EXCLUSIVE_MODE_MASK) == 0) /* shared mode */
         {
            hnsPeriodicity = 0;
         }
      }
 
      /*
       * Note: In Windows 8, the first use of IAudioClient to access the audio
       * device should be on the STA thread. Calls from an MTA thread may
       * result in undefined behavior.
       */
      hr = pIAudioClient_Initialize(handle->pAudioClient, mode, stream,
                                    hnsBufferDuration, hnsPeriodicity,
                                    &handle->Fmt.Format, NULL);
      /*
       * Some drivers don't suport exclusive mode and return E_INVALIDARG
       * for pIAudioClient_Initialize.
       * In these cases it is necessary to switch to shared mode.
       *
       * Other drivers don't support the event callback method and return
       * E_INVALIDARG for pIAudioClient_Initialize.
       * In these cases it is necessary to switch to timer driven.
       *
       * If you do get E_INVALIDARG from Initialize(), it is important to
       * remember that you MUST create a new instance of IAudioClient before
       * trying to Initialize() again or you will have unpredictable results.
       */
      if (hr == E_INVALIDARG)
      {
         if (handle->status & EXCLUSIVE_MODE_MASK)
         {
            handle->status &= ~EXCLUSIVE_MODE_MASK;
            _AAX_DRVLOG(8, "wasapi: exclusive mode failed, trying shared mode");
         }
         else if (handle->status & EVENT_DRIVEN_MASK)
         {
            handle->status &= ~EVENT_DRIVEN_MASK;
            _AAX_DRVLOG(9, "wasapi: event driven unsupported, trying timer mode");
         }
         else
         {
            _AAX_DRVLOG(9, "wasapi: Init returned invalid argument");
            break;
         }
      }
      else if (hr == AUDCLNT_E_DEVICE_IN_USE)
      {
         handle->status &= ~EXCLUSIVE_MODE_MASK;
         _AAX_DRVLOG(9, "wasapi: audio device in use, use shared mode");
      } 
      else if (hr == S_OK) {
         break;
      }

      /* init failed, close and re-open the device and try the new settings */
      pIAudioClient_Release(handle->pAudioClient);
      handle->pAudioClient = NULL;

      hr = pIMMDevice_Activate(handle->pDevice, pIID_IAudioClient,
                                     CLSCTX_INPROC_SERVER, NULL,
                                     (void**)&handle->pAudioClient);
      if (hr == S_OK)
      {
         hr = pIAudioClient_GetMixFormat(handle->pAudioClient, &wfx);
         if (hr == S_OK) {
            exToExtensible(&handle->Fmt, wfx, handle->setup);
         }
      }
   }
   while (hr != S_OK);

   if (hr == S_OK)
   {
      UINT32 bufferFrameCnt, periodFrameCnt;
      REFERENCE_TIME minPeriod = 0;
      REFERENCE_TIME defPeriod = 0;
      REFERENCE_TIME latency;
 
      /*
       * The GetDevicePeriod method retrieves the length of the periodic
       * interval separating successive processing passes by the audio engine
       * on the data in the endpoint buffer.
       *
       * + defPeriod is the default scheduling period for shared-mode
       * + minPeriod is the minimum scheduling period for exclusive-mode
       * Note: shared-mode stream periods are fixed!
       */
      hr = pIAudioClient_GetDevicePeriod(handle->pAudioClient, &defPeriod,
                                                               &minPeriod);
      periodFrameCnt = samples;
      if (hr == S_OK)
      {
         if ((handle->status & EXCLUSIVE_MODE_MASK) == 0) {
            periodFrameCnt = (UINT32)((defPeriod*freq + 10000000-1)/10000000);
         }
      }
      else {
         _AAX_DRVLOG(5, "wasapi; unable to get period size");
      }

      /* get the actual buffer size */
      hr = pIAudioClient_GetBufferSize(handle->pAudioClient, &bufferFrameCnt);
      if (hr == S_OK)
      {
         int periods = rintf((float)bufferFrameCnt/(float)periodFrameCnt);
         if (periods < 1)
         {
            _AAX_DRVLOG(0, "wasapi; too small no. periods returned");
            periodFrameCnt = bufferFrameCnt;
         }

         handle->buffer_frames = bufferFrameCnt;

         if (handle->Mode == eRender)
         {
            *frames = periodFrameCnt;
            hr = pIAudioClient_GetService(handle->pAudioClient,
                                          pIID_IAudioRenderClient,
                                          (void**)&handle->uType.pRender);
         }
         else /* handle->Mode == eCapture */
         {
            *frames = samples;
            handle->threshold = 5*samples/4;	// same as the ALSA backend
            handle->hnsPeriod = hnsPeriodicity;
            handle->packet_sz = (hnsPeriodicity*freq + 10000000-1)/10000000;
            handle->avail_avg = (float)samples;

            hr = pIAudioClient_GetService(handle->pAudioClient,
                                          pIID_IAudioCaptureClient,
                                          (void**)&handle->uType.pCapture);
            handle->scratch = (char*)0;
            handle->scratch_ptr = _aax_malloc(&handle->scratch,
                                              handle->buffer_frames*frame_sz);
         }

         if (hr == S_OK)
         {
            if ((handle->Mode == eCapture) && (hr == S_OK)) {
               _aaxWASAPIDriverResume(handle);
            }
            rv = AAX_TRUE;
         }
         else {
            _AAX_DRVLOG(10, "wasapi; faild to allocate the audio service");
         }
      }
      else {
         _AAX_DRVLOG(10, "wasapi; unable to set device buffer size");
      }

      hr = pIAudioClient_GetStreamLatency(handle->pAudioClient, &latency);
      if (hr == S_OK) {
         handle->hnsLatency = latency;
      }
#if 1
 printf("Format for %s\n", (handle->Mode == eRender) ? "Playback" : "Capture");
 printf("- event driven: %i,", handle->status & EVENT_DRIVEN_MASK);
 printf(" exclusive: %i\n", handle->status & EXCLUSIVE_MODE_MASK);
 printf("- frequency: %i\n", (int)handle->Fmt.Format.nSamplesPerSec);
 printf("- bits/sample: %i\n", handle->Fmt.Format.wBitsPerSample);
 printf("- no. channels: %i\n", handle->Fmt.Format.nChannels);
 printf("- block size: %i\n", handle->Fmt.Format.nBlockAlign);
 printf("- cb size: %i\n",  handle->Fmt.Format.cbSize);
 printf("- valid bits/sample: %i\n", handle->Fmt.Samples.wValidBitsPerSample);
 printf("- speaker mask: %x\n", (int)handle->Fmt.dwChannelMask);
 printf("- subformat: float: %x - pcm: %x\n",
          IsEqualGUID(&handle->Fmt.SubFormat, pKSDATAFORMAT_SUBTYPE_IEEE_FLOAT),
          IsEqualGUID(&handle->Fmt.SubFormat, pKSDATAFORMAT_SUBTYPE_PCM));
 printf("- latency: %f ms\n", handle->hnsLatency/10000.0f);

 printf("- periods: default: %f ms, minimum: %f ms\n", defPeriod/10000.0f, minPeriod/10000.0f);
 printf("- period: %i, buffer : %i , periods: %i\n", periodFrameCnt, bufferFrameCnt, (int)(0.5f+((float)bufferFrameCnt/(float)periodFrameCnt)));
#endif
   }
   else {
      _AAX_DRVLOG(10, "wasapi; failed to initialize");
   }

ExitSetup:
   if (co_init) {
      pCoUninitialize();
   }

   return rv;
}

static int
_aaxWASAPIDriverPause(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;
   
   if (handle)
   {
      if ((handle->status & DRIVER_PAUSE_MASK) == 0)
      {
         HRESULT hr;

         if (handle->Mode == eCapture)
         {
            if (handle->shutdown_event) {
               SetEvent(handle->shutdown_event);
            }
         }

         hr = pIAudioClient_Stop(handle->pAudioClient);
         if (hr == S_OK)
         {
            hr = pIAudioClient_Reset(handle->pAudioClient);
            handle->status |= (CAPTURE_INIT_MASK | DRIVER_PAUSE_MASK);
            rv = AAX_TRUE;
         }

         if (handle->Mode == eCapture)
         {
            if (handle->thread)
            {
               WaitForSingleObject(handle->thread, INFINITE);

               DeleteCriticalSection(&handle->mutex);
               CloseHandle(handle->thread);
               handle->thread = NULL;
            }
         }
      }
   }
   return rv;
}

static int
_aaxWASAPIDriverResume(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;
   if (handle)
   {
      if (handle->status & DRIVER_PAUSE_MASK)
      {
         HRESULT hr = S_OK;

#if USE_CAPTURE_THREAD
         if (handle->Mode == eCapture)
         {
            LPTHREAD_START_ROUTINE cb;

            InitializeCriticalSection(&handle->mutex);
            handle->shutdown_event = CreateEvent(NULL, FALSE, FALSE, NULL);

            cb = (LPTHREAD_START_ROUTINE)&_aaxWASAPIDriverCaptureThread;
            handle->thread = CreateThread(NULL, 0, cb, (LPVOID)handle, 0, NULL);
            if (handle->thread == 0) {
               _AAX_DRVLOG(10, "wasapi; unable to create a capture thread");
            }
         }
#endif

         hr = pIAudioClient_Start(handle->pAudioClient);
         if (hr == S_OK)
         {
            handle->status &= ~DRIVER_INIT_MASK;
            rv = AAX_TRUE;
         }
         else {
            _AAX_DRVLOG(10, "wasapi; failed to resume playback");
         }
      } else {
         rv = AAX_TRUE;
      }
      handle->status &= ~DRIVER_PAUSE_MASK;
   }
   return rv;
}

static int
_aaxWASAPIDriverAvailable(const void *id)
{
   return AAX_TRUE;
}

static int
_aaxWASAPIDriverIsReachable(const void *id)
{
    _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;

   if (handle && handle->pDevice)
   {
      DWORD state;
      HRESULT hr;

      hr = pIMMDevice_GetState(handle->pDevice, &state);
      if (hr == S_OK) {
         rv = (state == DEVICE_STATE_ACTIVE) ? AAX_TRUE : AAX_FALSE;
      }
   }

   return rv;
}

int
_aaxWASAPIDriver3dMixer(const void *id, void *d, void *s, void *p, void *m, int n, unsigned char ctr, unsigned int nbuf)
{
   _driver_t *handle = (_driver_t *)id;
   float gain;
   int ret;

   assert(s);
   assert(d);
   assert(p);

   gain = _aaxWASAPIDriverBackend.gain;
   ret = handle->mix_mono3d(d, s, p, m, gain, n, ctr, nbuf);

   return ret;
}

static int
_aaxWASAPIDriverCapture(const void *id, void **data, int offs, size_t *req_frames, void *scratch, size_t scratchlen)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int no_frames;
   int rv = AAX_FALSE;

   if ((req_frames == 0) || (data == 0)) {
      return AAX_FALSE;
   }

   no_frames = *req_frames;
   if (no_frames == 0) {
      return AAX_TRUE;
   } 

   if (handle->status & DRIVER_INIT_MASK) {
      return _aaxWASAPIDriverResume(handle);
   }

   if (data && handle->scratch_ptr)
   {
      unsigned int frame_sz = handle->Fmt.Format.nBlockAlign;
      unsigned int fetch = no_frames;
      float diff;

      /* try to keep the buffer padding at the threshold level at all times */
      diff = handle->avail_avg - (float)no_frames;
      handle->avail_padding = (handle->avail_padding + diff/(float)no_frames)/2;
      fetch += roundf(handle->avail_padding);

      diff = (float)handle->scratch_offs - (float)handle->threshold;
      handle->padding = (handle->padding + diff/(float)no_frames)/2;
      fetch += roundf(handle->padding);
      if (fetch > no_frames) offs += no_frames - fetch;
#if 0
if (roundf(handle->padding))
printf("%4.2f (%3.2f), avail: %4i (th: %3i, d: %4.0f), avg: %5.2f, fetch: %3i (no: %3i)\n", handle->padding, handle->avail_padding, handle->scratch_offs, handle->threshold, diff, handle->avail_avg, fetch, no_frames);
#endif
      /* try to keep the buffer padding at the threshold level at all times */

      if (handle->thread) 			/* Lock the mutex */
      {
         EnterCriticalSection(&handle->mutex);

         if (handle->status & CAPTURE_INIT_MASK)
         {
            unsigned int keep = no_frames + handle->threshold;
            if (handle->scratch_offs > keep)
            {
               size_t offset = handle->scratch_offs - keep;

               memmove(handle->scratch, handle->scratch+offset, keep*frame_sz);
               handle->scratch_offs -= offset;
               handle->padding = 0;
            }
            handle->status &= ~CAPTURE_INIT_MASK;
         }
      }

      /* copy data from the buffer if available */
      if (handle->scratch_offs)
      {
         unsigned int cvt_frames = _MIN(handle->scratch_offs, fetch);
         int32_t **ptr = (int32_t**)data;

         
         handle->cvt_from_intl(ptr, handle->scratch, offs, 2, cvt_frames);
         handle->scratch_offs -= cvt_frames;
         fetch -= cvt_frames;
         offs += cvt_frames;

         if (handle->scratch_offs)
         {
            memmove(handle->scratch, handle->scratch + cvt_frames*frame_sz,
                    handle->scratch_offs*frame_sz);
         }
      }

      if (handle->thread) {			/* Unlock the mutex */
         LeaveCriticalSection(&handle->mutex);
      }
      else
      {			/* if there's room for other packets, fetch them */
         _aaxWASAPIDriverCaptureFromHardware(handle);
         if (handle->status & CAPTURE_INIT_MASK)
         {
            unsigned int keep = no_frames + handle->threshold;
            if (handle->scratch_offs > keep)
            {
               size_t offset = handle->scratch_offs - keep;

               memmove(handle->scratch, handle->scratch+offset, keep*frame_sz);
               handle->scratch_offs -= offset;
            }
            handle->status &= ~CAPTURE_INIT_MASK;
         }

         /* copy (remaining) data from the buffer if required  */
         if (fetch && handle->scratch_offs)
         {
            unsigned int avail = _MIN(handle->scratch_offs, fetch);
            int32_t **ptr = (int32_t**)data;

            handle->cvt_from_intl(ptr, handle->scratch, offs, 2, avail);
            handle->scratch_offs -= avail;
            fetch -= avail;

            if (handle->scratch_offs)
            {
               memmove(handle->scratch, handle->scratch + avail*frame_sz,
                       handle->scratch_offs*frame_sz);
            }
         }
      }

      if (fetch)
      {
         _AAX_DRVLOG(5, "wasapi; not enough data available for capture");
if (fetch > 1) {
printf("fetch: %i, avail: %f, padding: %f, avail_padding: %f\n", fetch, handle->avail_avg, handle->padding,  handle->avail_padding);
}
// exit(-1);
      }

      *req_frames -= fetch;
      rv = AAX_TRUE;
   }
   else {
     *req_frames = 0;
   }

   return rv;
}


static int
_aaxWASAPIDriverPlayback(const void *id, void *src, float pitch, float volume)
{
   _driver_t *handle = (_driver_t *)id;
   _oalRingBuffer *rbs = (_oalRingBuffer *)src;
   unsigned int no_tracks, offs;
   _oalRingBufferSample *rbsd;
   size_t no_frames;
   HRESULT hr;

   assert(handle != 0);
   if (handle->status & DRIVER_PAUSE_MASK) return 0;

   assert(rbs != 0);
   assert(rbs->sample != 0);

   rbsd = rbs->sample;
   offs = _oalRingBufferGetOffsetSamples(rbs);
   no_frames = _oalRingBufferGetNoSamples(rbs) - offs;
   no_tracks = _oalRingBufferGetNoTracks(rbs);

   assert(_oalRingBufferGetNoSamples(rbs) >= offs);

   if (handle->status & DRIVER_INIT_MASK) {
      _aaxWASAPIDriverResume(handle);
   }

   do
   {
      unsigned int frames = no_frames;

      /*
       * For an exclusive-mode rendering or capture stream that was initialized
       * with the AUDCLNT_STREAMFLAGS_EVENTCALLBACK flag, the client typically
       * has no use for the padding value reported by GetCurrentPadding. 
       * Instead, the client accesses an entire buffer during each processing
       * pass. 
       */
      if ((handle->status & (EXCLUSIVE_MODE_MASK | EVENT_DRIVEN_MASK)) == 0)
      {
         UINT32 padding;

         frames = 0;
         hr = pIAudioClient_GetCurrentPadding(handle->pAudioClient, &padding);
         if ((hr == S_OK) && (padding < no_frames)) {
            frames = _MIN(handle->buffer_frames - padding, no_frames);
         }
      }

      assert((handle->status & DRIVER_INITMASK) == 0);
      assert(frames <= no_frames);

      if (frames >= no_frames)
      {
         IAudioRenderClient *pRender = handle->uType.pRender;
         const int32_t **sbuf = (const int32_t**)rbsd->track;
         BYTE *data = NULL;

         hr = pIAudioRenderClient_GetBuffer(pRender, frames, &data);
         if (hr == S_OK)
         {
            handle->cvt_to_intl(data, sbuf, offs, no_tracks, no_frames);

            assert(no_frames >= frames);
            no_frames -= frames;

            hr = pIAudioRenderClient_ReleaseBuffer(handle->uType.pRender,
                                                frames, 0);
            if (hr != S_OK) {
               _AAX_DRVLOG(8, "wasapi; failed to release the buffer");
            }
         }
         else {
            _AAX_DRVLOG(8, "wasapi; failed to get the buffer");
         }
      }
   }
   while (0);

   return 0;
}

static char *
_aaxWASAPIDriverGetName(const void *id, int playback)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   if (handle && handle->pDevice) {
      ret = _aaxMMDeviceNameToName(detect_devname(handle->pDevice));
   }

   return ret;
}

static float
_aaxWASAPIDriverGetLatency(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   return handle ? handle->hnsLatency*100e-9f : 0.0f;
}


static char *
_aaxWASAPIDriverGetDevices(const void *id, int mode)
{
   static char names[2][1024] = { "\0\0", "\0\0" };
   IMMDeviceEnumerator *enumerator = NULL;
   int co_init = AAX_FALSE;
   HRESULT hr;

   hr = pCoInitialize(NULL);
   if (hr != RPC_E_CHANGED_MODE) {
      co_init = AAX_TRUE;
   }

   hr = pCoCreateInstance(pCLSID_MMDeviceEnumerator, NULL,
                          CLSCTX_INPROC_SERVER, pIID_IMMDeviceEnumerator,
                          (void**)&enumerator);
   if (hr == S_OK)
   {
      IMMDeviceCollection *collection = NULL;
      IPropertyStore *props = NULL;
      IMMDevice *device = NULL;
#if USE_GETID
      LPWSTR pwszID = NULL;
#endif
      UINT i, count;
      int m, len;
      char *ptr;

      len = 1023;
      m = mode > 0 ? 1 : 0;
      ptr = (char *)&names[m];
      hr = pIMMDeviceEnumerator_EnumAudioEndpoints(enumerator, _mode[m],
                                     DEVICE_STATE_ACTIVE|DEVICE_STATE_UNPLUGGED,
                                                   &collection);
      if (FAILED(hr)) goto ExitGetDevices;

      hr = pIMMDeviceCollection_GetCount(collection, &count);
      if (FAILED(hr)) goto ExitGetDevices;

      for(i=0; i<count; i++)
      {
         PROPVARIANT name;
         char *devname;
         int slen;

         hr = pIMMDeviceCollection_Item(collection, i, &device);
         if (hr != S_OK) goto NextGetDevices;

#if USE_GETID
         hr = pIMMDevice_GetId(device, &pwszID);
         if (hr != S_OK) goto NextGetDevices;
#endif

         hr = pIMMDevice_OpenPropertyStore(device, STGM_READ, &props);
         if (hr != S_OK) goto NextGetDevices;

         pPropVariantInit(&name);
         hr = pIPropertyStore_GetValue(props,
                         (const PROPERTYKEY*)pPKEY_DeviceInterface_FriendlyName,
                               &name);
         if (!SUCCEEDED(hr)) goto NextGetDevices;

         slen = len;
         devname = wcharToChar(ptr, &slen, name.pwszVal);
         /* namedoesn't match  with previous device */
         if (devname && strcmp(devname, ptr))
         {
            slen++;
            len -= slen;
            ptr += slen;
            if (len <= 0) break;
         }

NextGetDevices:
#if USE_GETID
         pCoTaskMemFree(pwszID);
         pwszID = NULL;
#endif

         pPropVariantClear(&name);
         pIPropertyStore_Release(props);
         pIMMDevice_Release(device);
      }
      pIMMDeviceEnumerator_Release(enumerator);
      pIMMDeviceCollection_Release(collection);
      if (co_init) {
         pCoUninitialize();
      }

      return (char *)&names[mode];

ExitGetDevices:
#if USE_GETID
      pCoTaskMemFree(pwszID);
#endif
      if (enumerator) pIMMDeviceEnumerator_Release(enumerator);
      if (collection) pIMMDeviceCollection_Release(collection);
      if (device) pIMMDevice_Release(device);
      if (props) pIPropertyStore_Release(props);
      if (co_init) pCoUninitialize();
   }

   return (char *)&names[mode];
}

static char *
_aaxWASAPIDriverGetInterfaces(const void *id, const char *devname, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   int m = (mode > 0) ? 1 : 0;
   char *rv = 0;

   if (handle && !rv)
   {
      IMMDeviceEnumerator *enumerator = NULL;
      int co_init = AAX_FALSE;
      char interfaces[1024];
      int len = 1024;
      HRESULT hr;

      memset(interfaces, '\0', 1024);

      hr = pCoInitialize(NULL);
      if (hr != RPC_E_CHANGED_MODE) {
         co_init = AAX_TRUE;
      }

      hr = pCoCreateInstance(pCLSID_MMDeviceEnumerator, NULL,
                             CLSCTX_INPROC_SERVER, pIID_IMMDeviceEnumerator,
                             (void**)&enumerator);
      if (hr == S_OK)
      {
         IMMDeviceCollection *collection = NULL;
         IPropertyStore *props = NULL;
         IMMDevice *device = NULL;
#if USE_GETID
         LPWSTR pwszID = NULL;
#endif
         UINT i, count;
         char *ptr;

         ptr = interfaces;
         hr = pIMMDeviceEnumerator_EnumAudioEndpoints(enumerator, _mode[m],
                                     DEVICE_STATE_ACTIVE|DEVICE_STATE_UNPLUGGED,
                                                      &collection);
         if (hr != S_OK) goto ExitGetInterfaces;

         hr = pIMMDeviceCollection_GetCount(collection, &count);
         if (FAILED(hr)) goto ExitGetInterfaces;

         for (i=0; i<count; i++)
         {
            PROPVARIANT name;
            char *device_name;

            hr = pIMMDeviceCollection_Item(collection, i, &device);
            if (hr != S_OK) goto NextGetInterfaces;

#if USE_GETID
            hr = pIMMDevice_GetId(device, &pwszID);
            if (hr != S_OK) goto NextGetInterfaces;
#endif

            hr = pIMMDevice_OpenPropertyStore(device, STGM_READ, &props);
            if (hr != S_OK) goto NextGetInterfaces;

            pPropVariantInit(&name);
            hr = pIPropertyStore_GetValue(props,
                         (const PROPERTYKEY*)pPKEY_DeviceInterface_FriendlyName,
                               &name);
            if (FAILED(hr)) goto NextGetInterfaces;

            device_name = wcharToChar(NULL, 0, name.pwszVal);
            if (device_name && !strcasecmp(device_name, devname)) /* found */
            {
               PROPVARIANT iface;

               hr = pIPropertyStore_GetValue(props,
                                    (const PROPERTYKEY*)pPKEY_Device_DeviceDesc,
                                    &iface);
               if (SUCCEEDED(hr))
               {
                  int slen = len;
                  char *if_name = wcharToChar(ptr, &slen, iface.pwszVal);
                  if (if_name)
                  {
                     slen++;		/* skip trailing '\0' */
                     len -= slen;
                     ptr += slen;
                     if (len <= 0) break;
                  }
                  pPropVariantClear(&iface);
               }
            }
            free(device_name);

NextGetInterfaces:
#if USE_GETID
            pCoTaskMemFree(pwszID);
            pwszID = NULL;
#endif

            pPropVariantClear(&name);
            pIPropertyStore_Release(props);
            pIMMDevice_Release(device);
         }

         pIMMDeviceEnumerator_Release(enumerator);
         pIMMDeviceCollection_Release(collection);
         if (co_init) {
            pCoUninitialize();
         }

         /* always end with "\0\0" no matter what */
         interfaces[1022] = 0;
         interfaces[1023] = 0;
         if (ptr != interfaces)		/* there was at least one added */
         {
            if (len > 0) *ptr++ = '\0';
            else ptr -= -len;

            rv = handle->ifname[m] = malloc(ptr-interfaces+1);
            if (rv) {
               memcpy(handle->ifname[m], interfaces, ptr-interfaces);
            }
         }

         return rv;
ExitGetInterfaces:
#if USE_GETID
         pCoTaskMemFree(pwszID);
#endif
         if (enumerator) pIMMDeviceEnumerator_Release(enumerator);
         if (collection) pIMMDeviceCollection_Release(collection);
         if (device) pIMMDevice_Release(device);
         if (props) pIPropertyStore_Release(props);
         if (co_init) pCoUninitialize();
      }
   }

   return rv;
}


static char *
_aaxWASAPIDriverLogVar(int prio, const char *fmt, ...)
{
   char _errstr[1024];
   va_list ap;

   _errstr[0] = '\0';
   va_start(ap, fmt);
   vsnprintf(_errstr, 1024, fmt, ap);

   // Whatever happen in vsnprintf, what i'll do is just to null terminate it
   _errstr[1023] = '\0';
   va_end(ap);

   return _aaxWASAPIDriverLogPrio(prio, _errstr);
}

static char *
_aaxWASAPIDriverLogPrio(int prio, const char *str)
{
   static int curr_prio = 0;
   char *rv = NULL;
   if (prio >= curr_prio)
   {
      rv = _aaxWASAPIDriverLog(str);
      curr_prio = prio;
   }
   return rv;
}

static char *
_aaxWASAPIDriverLog(const char *str)
{
   static char _errstr[256];
   int len = _MIN(strlen(str)+1, 256);

   memcpy(_errstr, str, len);
   _errstr[255] = '\0';  /* always null terminated */

   __aaxErrorSet(AAX_BACKEND_ERROR, (char*)&_errstr);
   OutputDebugString(_errstr);
   _AAX_SYSLOG(_errstr);
   printf("%s\n", _errstr);

   return (char*)&_errstr;
}

/* -------------------------------------------------------------------------- */

static DWORD
_aaxWASAPIDriverCaptureThread(LPVOID id)
{
   _driver_t *handle = (_driver_t*)id;
   unsigned int stdby_time;
   int active = AAX_TRUE;
   HRESULT hr;

   assert(handle);

   hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
   if (FAILED(hr)) return -1;

   stdby_time = handle->hnsPeriod/(2*10000);
   while (active)
   {
      DWORD res = WaitForSingleObject(handle->shutdown_event, stdby_time);
      switch (res)
      {
         case WAIT_OBJECT_0:
            active = AAX_FALSE;
            break;
         case WAIT_TIMEOUT:
            _aaxWASAPIDriverCaptureFromHardware(handle);
            break;
      }
   }

   CoUninitialize();
   return 0;
}

static int
_aaxWASAPIDriverCaptureFromHardware(_driver_t *handle)
{
   unsigned int total_avail = 0;
   unsigned int packet_cnt = 0;
   unsigned int packet_sz = 0;
   unsigned int cnt;		// max. 3 times which equals to 3ms
   HRESULT hr;

#if 0
   if (handle->thread) cnt = 1;
   else cnt = 3;
#endif

   /*
    * During each GetBuffer call, the caller must either obtain the
    * entire packet or none of it.
    *
    * return values (both are SUCCESS(hr)):
    * S_OK:
    *   The call succeeded and 'avail' is nonzero,
    *   indicating that a packet is ready to be read.
    * AUDCLNT_S_BUFFER_EMPTY:
    *   The call succeeded and 'avail' is 0, indicating
    *   that no capture data is available to be read.
    */
   do
   {
      UINT32 avail = 0;
      BYTE* buf = NULL;
      DWORD flags = 0;

      packet_sz = avail = 0;
      hr = pIAudioCaptureClient_GetBuffer(handle->uType.pCapture, &buf,
                                          &avail, &flags, NULL, NULL);
      if (SUCCEEDED(hr))
      {                                /* there is data available */
         HRESULT res;

         packet_sz = avail;
         handle->scratch_offs += avail;
         total_avail += avail;
         if ((hr == S_OK) && (handle->scratch_offs <= handle->buffer_frames))
         {
            unsigned int frame_sz = handle->Fmt.Format.nBlockAlign;
            size_t offset = handle->scratch_offs - avail;

            if (handle->thread) {		/* lock the mutex */
               EnterCriticalSection(&handle->mutex);
            }

            if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0)
            {
               _aax_memcpy(handle->scratch + offset*frame_sz,
                           buf, avail*frame_sz);
            }
            else {
               memset(handle->scratch + offset*frame_sz, 0, avail*frame_sz);
            }

            if (handle->thread) {		/* unlock the mutex */
               LeaveCriticalSection(&handle->mutex);
            }

            packet_cnt += (avail/handle->packet_sz);
         }
         else
         {
            packet_sz = 0;
            if (handle->scratch_offs > handle->buffer_frames)
            {
               total_avail -= avail;
               handle->scratch_offs -= avail;
               _AAX_DRVLOG(6, "wasapi; capture buffer exhausted");
            }
            else if (hr != AUDCLNT_S_BUFFER_EMPTY) {
               _AAX_DRVLOG(9, "wasapi; error getting the buffer");
            }
         }

         if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {
            _AAX_DRVLOG(3, "wasapi; data discontinuity");
         }

         /* release the original packet size or 0 */
         res = pIAudioCaptureClient_ReleaseBuffer(handle->uType.pCapture,
                                                  packet_sz);
         if (FAILED(res)) {
            _AAX_DRVLOG(5, "wasapi; error releasing the buffer");
         }
      }
      else if (hr != AUDCLNT_S_BUFFER_EMPTY) {
         _AAX_DRVLOG(9, "wasapi; error getting the buffer");
      }
      else if (cnt--)	/* hr == AUDCLNT_S_BUFFER_EMPTY */
      {
         hr = S_OK;
         msecSleep(1);
      }
      else break;
   }
   while (packet_sz && (hr == S_OK));

   handle->avail_avg = (0.98f*handle->avail_avg + 0.02f*total_avail);

   return hr;
}



#if 0
static void
displayError(LPTSTR lpszFunction) 
{ 
   LPVOID lpMsgBuf;
   LPVOID lpDisplayBuf;
   DWORD dw = GetLastError(); 

   FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

   lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR)); 
   StringCchPrintf((LPTSTR)lpDisplayBuf, 
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"), 
        lpszFunction, dw, lpMsgBuf); 
   MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK); 

   LocalFree(lpMsgBuf);
   LocalFree(lpDisplayBuf);
}
#endif

static const char *
aaxNametoMMDevciceName(const char *devname)
{
   static char rv[1024] ;

   if (devname)
   {
      char *ptr, dev[1024];

      snprintf(dev, 1024, "%s", devname);
      ptr =  strstr(dev, ": ");
      if (ptr)
      {
         *ptr++ = 0;
         snprintf(rv, 1024, "%s (%s)", ++ptr, dev);
      }
   }
   return (const char*)&rv;
}

static char*
_aaxMMDeviceNameToName(char *devname)
{
   char *rv = devname;

   if (rv)
   {
      char *ptr, dev[1024];

      snprintf(dev, 1024, "%s", devname);
      ptr =  strstr(dev, " (");
      if (ptr && (strlen(ptr) > 3))
      {
         unsigned int len;

         *ptr = 0;
         ptr += 2;

         len = strlen(ptr);
         *(ptr+len-1) = 0;

         len = strlen(devname);
         snprintf(devname, len, "%s: %s", ptr, dev);
      }
   }

   return rv;
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
      hr = pIPropertyStore_GetValue(props,
                                  (const PROPERTYKEY*)pPKEY_Device_FriendlyName,
                                  &name);
      if (SUCCEEDED(hr)) {
         rv = wcharToChar(NULL, 0, name.pwszVal);
      }
      pPropVariantClear(&name);
      pIPropertyStore_Release(props);
   }

   return rv;
}

static char*
wcharToChar(char *dst, int *dlen, const WCHAR* wstr)
{
   char *rv = dst;
   int alen, wlen;

   wlen = lstrlenW(wstr);
   alen = pWideCharToMultiByte(CP_ACP, 0, wstr, wlen, 0, 0, NULL, NULL);
   if (alen > 0)
   {
      if (!rv) {
         rv = (char *)malloc(alen+1);
      } else if (alen > *dlen) {
         assert(dlen);
         alen = *dlen;
      }
      if (rv)
      {
         if (dlen) *dlen = alen;
         pWideCharToMultiByte(CP_ACP, 0, wstr, wlen, rv, alen, NULL, NULL);
         rv[alen] = 0;
      }
   }
   return rv;
}

static WCHAR*
charToWChar(const char* str)
{
   WCHAR *rv = NULL;
   int alen, wlen;

   alen = lstrlenA(str);
   wlen = pMultiByteToWideChar(CP_ACP, 0, str, alen, NULL, 0);
   if (wlen > 0)
   {
      rv = (WCHAR *)malloc(sizeof(WCHAR)*(wlen+1));
      pMultiByteToWideChar(CP_ACP, 0, str, alen, rv, wlen);
      rv[wlen] = 0;
   }
   return rv;
}

static LPWSTR
name_to_id(const WCHAR* dname, unsigned char m)
{
   IMMDeviceCollection *collection = NULL;
   IMMDeviceEnumerator *enumerator = NULL;
   int co_init = AAX_FALSE;
   LPWSTR rv = 0;
   HRESULT hr;

   hr = pCoInitialize(NULL);
   if (hr != RPC_E_CHANGED_MODE) {
      co_init = AAX_TRUE;
   }

   hr = pCoCreateInstance(pCLSID_MMDeviceEnumerator, NULL,
                          CLSCTX_INPROC_SERVER, pIID_IMMDeviceEnumerator,
                          (void**)&enumerator);
   if (hr == S_OK) {
      hr = pIMMDeviceEnumerator_EnumAudioEndpoints(enumerator, _mode[m],
                                                   DEVICE_STATE_ACTIVE,
                                                   &collection);
   }

   if (hr == S_OK)
   {
      IPropertyStore *props = NULL;
      IMMDevice *device = NULL;
      UINT i, count;

      hr = pIMMDeviceCollection_GetCount(collection, &count);
      if (FAILED(hr)) goto ExitNameId;

      for(i=0; i<count; i++)
      {
         LPWSTR pwszID = NULL;
         PROPVARIANT name;

         hr = pIMMDeviceCollection_Item(collection, i, &device);
         if (hr != S_OK) goto NextNameId;

         hr = pIMMDevice_GetId(device, &pwszID);
         if (hr != S_OK) goto NextNameId;

         hr = pIMMDevice_OpenPropertyStore(device, STGM_READ, &props);
         if (hr != S_OK) goto NextNameId;

         pPropVariantInit(&name);
         hr = pIPropertyStore_GetValue(props,
                               (const PROPERTYKEY*)pPKEY_Device_FriendlyName,
                               &name);
         if (!SUCCEEDED(hr)) goto NextNameId;

         if (!wcsncmp(name.pwszVal, dname, wcslen(dname)))
         {
            rv = pwszID;
            break;
         }
NextNameId:
         pCoTaskMemFree(pwszID);
         pwszID = NULL;

         pPropVariantClear(&name);
         pIPropertyStore_Release(props);
         pIMMDevice_Release(device);
      }
ExitNameId:
      pIMMDeviceEnumerator_Release(enumerator);
      pIMMDeviceCollection_Release(collection);
   }

   if(co_init) {
      pCoUninitialize();
   }

   return rv;
}

void *
_aaxWASAPIDriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
#if ENABLE_TIMING
   _aaxTimer *timer = _aaxTimerCreate();
#endif
   _intBufferData *dptr_sensor;
   const _aaxDriverBackend *be;
   _oalRingBuffer *dest_rb;
   int stdby_time, state;
   _aaxAudioFrame *mixer;
   _driver_t *be_handle;
   float delay_sec;
   DWORD hr;

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
      }
      _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

      if (!dest_rb) {
         return NULL;
      }
   }
   else {
      return NULL;
   }

   be->pause(handle->backend.handle);
   state = AAX_SUSPENDED;

   /* get real duration, it might have been altered for better performance */
   delay_sec = _oalRingBufferGetDuration(dest_rb);

   _aaxMutexLock(handle->thread.mutex);
   stdby_time = (int)(4*delay_sec*1000);

   hr = S_OK;
   be_handle = (_driver_t *)handle->backend.handle;
   if (be_handle->status & EVENT_DRIVEN_MASK)
   {
      be_handle->Event = CreateEvent(NULL, FALSE, FALSE, NULL);
      if (be_handle->Event) {
         hr = IAudioClient_SetEventHandle(be_handle->pAudioClient,
                                          be_handle->Event);
      }
      else {
         _AAX_DRVLOG(10, "wasapi; unable to create audio event");
      }
   }
   else				/* timer driven, creat a periodic timer */
   {
      be_handle->Event = CreateWaitableTimer(NULL, FALSE, NULL);
      if (be_handle->Event)
      {
         LARGE_INTEGER liDueTime;
         LONG lPeriod;

         lPeriod = (LONG)((delay_sec*1000)+0.5f);
         liDueTime.QuadPart = -(LONGLONG)((delay_sec*1000*10000)+0.5);
         hr = SetWaitableTimer(be_handle->Event, &liDueTime, lPeriod,
                               NULL, NULL, FALSE);
         if (hr) hr = S_OK;
         else hr = S_FALSE;
      }
      else {
         _AAX_DRVLOG(10, "wasapi; unable to create event timer");
      }
   }
   if (!be_handle->Event || FAILED(hr)) {
      _AAX_DRVLOG(8, "wasapi; unable to set up the event handler");
   }

   /* playback loop */
   while ((hr == S_OK) && TEST_FOR_TRUE(handle->thread.started))
   {
      _aaxMutexUnLock(handle->thread.mutex);

      if (_IS_PLAYING(handle) && be->is_available(be_handle))
      {
         hr = WaitForSingleObject(be_handle->Event, stdby_time);
         switch (hr)
         {
         case WAIT_OBJECT_0:	/* event was triggered */
            break;
         case WAIT_TIMEOUT:	/* wait timed out      */
            if ((be_handle->status & DRIVER_INIT_MASK) == 0) {
               _AAX_DRVLOG(5, "wasapi; event timeout");
            }
            break;
         case WAIT_ABANDONED:
         case WAIT_FAILED:
         default:
            _AAX_DRVLOG(7, "wasapi; wait for even failed");
            break;
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
         if (_IS_PAUSED(handle)
             || (!_IS_PLAYING(handle) && _IS_STANDBY(handle)))
         {
            be->pause(handle->backend.handle);
         }
         else if (_IS_PLAYING(handle) || _IS_STANDBY(handle)) {
            be->resume(handle->backend.handle);
         }
         state = handle->state;
      }

#if ENABLE_TIMING
   _aaxTimerStart(timer);
#endif
      /* do all the mixing */
      if (_IS_PLAYING(handle) && be->is_available(be_handle)) {
         _aaxSoftwareMixerThreadUpdate(handle, dest_rb);
      }
#if ENABLE_TIMING
//printf("elapsed: %f ms\n", _aaxTimerElapsed(timer)*1000.0f);
#endif

      hr = S_OK;
   }

   if ((be_handle->status & EVENT_DRIVEN_MASK) == 0) {
      CancelWaitableTimer(be_handle->Event);
   }
   _aaxMutexUnLock(handle->thread.mutex);

   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      _oalRingBufferStop(handle->ringbuffer);
      _oalRingBufferDelete(handle->ringbuffer);
   }
   return handle;
}

static int
copyFmtEx(WAVEFORMATEX *out, WAVEFORMATEX *in)
{
   assert(in != 0);
   assert(out != 0);

#if 1
   memcpy(out, in, sizeof(WAVEFORMATEX));
#else
   out->wFormatTag = in->wFormatTag;
   out->nChannels = in->nChannels;
   out->nSamplesPerSec = in->nSamplesPerSec;
   out->nAvgBytesPerSec = in->nAvgBytesPerSec;
   out->nBlockAlign = in->nBlockAlign;
   out->wBitsPerSample = in->wBitsPerSample;
   out->cbSize = in->cbSize;
#endif

   return AAX_TRUE;
}

static int
copyFmtExtensible(WAVEFORMATEXTENSIBLE *out, WAVEFORMATEXTENSIBLE *in)
{
   assert(in != 0);
   assert(out != 0);

#if 1
   memcpy(out, in, sizeof(WAVEFORMATEXTENSIBLE));
#else
   copyFmtEx(&out->Format, &in->Format);

   out->Samples.wValidBitsPerSample = in->Samples.wValidBitsPerSample;
   out->dwChannelMask = in->dwChannelMask;
   out->SubFormat = in->SubFormat;
#endif

   return AAX_TRUE;
}

static int
exToExtensible(WAVEFORMATEXTENSIBLE *out, WAVEFORMATEX *in, enum aaxRenderMode setup)
{
   int rv = AAX_TRUE;

   assert(in);

   if (in->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
      copyFmtExtensible(out, (WAVEFORMATEXTENSIBLE*)in);
   }
   else if (in->wFormatTag == WAVE_FORMAT_PCM ||
            in->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
   {
      copyFmtEx(&out->Format, in);
      if (in->nChannels > 2 || in->wBitsPerSample > 16)
      {
         /* correct for extensible format */
         out->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
         out->Format.cbSize = CBSIZE;

         /* all formats match */
         out->Samples.wValidBitsPerSample = in->wBitsPerSample;

         out->dwChannelMask = getChannelMask(in->nChannels, setup);
         if (in->wFormatTag == WAVE_FORMAT_PCM) {
            out->SubFormat = aax_KSDATAFORMAT_SUBTYPE_PCM;
         } else {
            out->SubFormat = aax_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
         }
      }
   }
   else
   {
      _AAX_DRVLOG(4, "wasapi; usupported format requested");
      rv = AAX_FALSE;
   }

   return rv;
}

static DWORD
getChannelMask(WORD nChannels, enum aaxRenderMode mode)
{
   DWORD rv = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;

   /* for now without mode */
   switch (nChannels)
   {
   case 1:
      rv = SPEAKER_FRONT_CENTER;
      break;
   case 8:
      rv |= SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
   case 6:
      rv |= SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY;
   case 4:
      rv |= SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
   case 2:
      break;
   default:
      _AAX_DRVLOG(4, "wasapi; usupported no. tracks requested");
      break;
   }
   return rv;
}

