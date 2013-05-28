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


/***
 * Windows behaviour is influenced at differen levels in the code:
 * 
 * 1 round buffer size in ms up/down (5.33ms becomes 4ms/6ms)
 * 2 thread priority (in ../api.h and ../aax_mixer.c)
 * 3 threshold value (3*bufsz/2 or 5*bufsz/4)
 *   (these are set at _aaxThreadStart, or by our own)
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
#include <driver.h>
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

#define USE_CAPTURE_THREAD	AAX_FALSE
#define CAPTURE_USE_MIN_PERIOD	AAX_TRUE
#define EXCLUSIVE_MODE		AAX_TRUE
#define EVENT_DRIVEN		AAX_TRUE
#define ENABLE_TIMING		AAX_FALSE
#define LOG_TO_FILE		AAX_TRUE
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

#define _AAX_DRVLOG(p)			_aaxWASAPIDriverLog(id, 0, p, __FUNCTION__)
#define _AAX_DRVLOG_VAR(args...)	_aaxWASAPIDriverLogVar(id, args);
#define HW_VOLUME_SUPPORT(a)		(a->pEndpointVolume && (a->volumeMax))


static _aaxDriverDetect _aaxWASAPIDriverDetect;
static _aaxDriverNewHandle _aaxWASAPIDriverNewHandle;
static _aaxDriverGetDevices _aaxWASAPIDriverGetDevices;
static _aaxDriverGetInterfaces _aaxWASAPIDriverGetInterfaces;
static _aaxDriverConnect _aaxWASAPIDriverConnect;
static _aaxDriverDisconnect _aaxWASAPIDriverDisconnect;
static _aaxDriverSetup _aaxWASAPIDriverSetup;
static _aaxDriverCaptureCallback _aaxWASAPIDriverCapture;
static _aaxDriverCallback _aaxWASAPIDriverPlayback;
static _aaxDriverGetName _aaxWASAPIDriverGetName;
static _aaxDriverThread _aaxWASAPIDriverThread;
static _aaxDriver3dMixerCB _aaxWASAPIDriver3dMixer;
static _aaxDriverState _aaxWASAPIDriverState;
static _aaxDriverParam _aaxWASAPIDriverParam;
static _aaxDriverLog _aaxWASAPIDriverLog;

static char _wasapi_default_renderer[100] = DEFAULT_RENDERER;
static const EDataFlow _mode[] = { eCapture, eRender };

const _aaxDriverBackend _aaxWASAPIDriverBackend =
{
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
   (_aaxDriverCaptureCallback *)&_aaxWASAPIDriverCapture,
   (_aaxDriverCallback *)&_aaxWASAPIDriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxFileDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxWASAPIDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxFileDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)&_aaxWASAPIDriverState,
   (_aaxDriverParam *)&_aaxWASAPIDriverParam
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
   UINT32 hw_buffer_frames;

   int status;
   char sse_level;

   char *ifname[2];
   _oalRingBufferMix1NFunc *mix_mono3d;
   enum aaxRenderMode setup;

   _batch_cvt_to_intl_proc cvt_to_intl;
   _batch_cvt_from_intl_proc cvt_from_intl;

   /* enpoint volume control */
   IAudioEndpointVolume *pEndpointVolume;
   float volumeHW;
   float volumeCur, volumeInit;
   float volumeMin, volumeMax;
   float volumeStep, hwgain;

   /* capture related */
   char cthread_init;
   HANDLE task;
   HANDLE cthread;
   CRITICAL_SECTION mutex;

   char *scratch;
   void *scratch_ptr;
   unsigned int scratch_offs;	/* current offset in the scratch buffer  */
   unsigned int threshold;	/* sensor buffer threshold for padding   */
   unsigned int packet_sz;	/* number of audio frames per time-frame */
   float padding;		/* for sensor clock drift correction     */

   /* Audio session events */
   IAudioSessionControl *pControl;
   struct IAudioSessionEvents events;
   LONG refs;

   /* error handling */
   void *log_file;
   int prev_type;
   unsigned int err_ctr;
   unsigned int type_ctr;
   char errstr[256];

} _driver_t;
 
const char* _wasapi_default_name = DEFAULT_DEVNAME;
static const struct IAudioSessionEventsVtbl _aaxAudioSessionEvents;

# define pIID_IUnknown &aax_IID_IUnknown
# define pIID_IAudioSessionEvents &aax_IID_IAudioSessionEvents
# define pIID_IMMDeviceEnumerator &aax_IID_IMMDeviceEnumerator
# define pIID_IAudioRenderClient &aax_IID_IAudioRenderClient
# define pIID_IAudioCaptureClient &aax_IID_IAudioCaptureClient
# define pIID_IAudioClient &aax_IID_IAudioClient
# define pIID_IAudioEndpointVolume &aax_IID_IAudioEndpointVolume
# define pIID_IAudioSessionControl &aax_IID_IAudioSessionControl
# define pPKEY_Device_DeviceDesc &PKEY_Device_DeviceDesc
# define pPKEY_Device_FriendlyName &PKEY_Device_FriendlyName
# define pPKEY_DeviceInterface_FriendlyName &PKEY_DeviceInterface_FriendlyName
# define pPKEY_Device_FriendlyName &PKEY_Device_FriendlyName
# define pKSDATAFORMAT_SUBTYPE_IEEE_FLOAT &aax_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
# define pKSDATAFORMAT_SUBTYPE_ADPCM &aax_KSDATAFORMAT_SUBTYPE_ADPCM
# define pKSDATAFORMAT_SUBTYPE_PCM &aax_KSDATAFORMAT_SUBTYPE_PCM

# define pCLSID_MMDeviceEnumerator &aax_CLSID_MMDeviceEnumerator
# define pIAudioClient_SetEventHandle IAudioClient_SetEventHandle

# define pPropVariantInit PropVariantInit
# define pPropVariantClear PropVariantClear
# define pWideCharToMultiByte WideCharToMultiByte
# define pMultiByteToWideChar MultiByteToWideChar
# define pCoInitialize CoInitialize
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

static const char* _aaxNametoMMDevciceName(const char*);
static char* _aaxMMDeviceNameToName(char *);
static char *_aaxWASAPIDriverLogVar(const void *, const char *fmt, ...);
static HRESULT _aaxCaptureThreadStart(_driver_t*);
static HRESULT _aaxCaptureThreadStop(_driver_t*);
static int _aaxWASAPIDriverCaptureFromHardware(_driver_t*);
#if USE_CAPTURE_THREAD
static DWORD _aaxWASAPIDriverCaptureThread(LPVOID);
#endif

static LPWSTR name_to_id(const WCHAR*, unsigned char);
static char* detect_devname(IMMDevice*);
static char* wcharToChar(char*, int*, const WCHAR*);
static WCHAR* charToWChar(const char*);
static DWORD getChannelMask(WORD, enum aaxRenderMode);
static int copyFmtEx(WAVEFORMATEX*, WAVEFORMATEX*);
static int copyFmtExtensible(WAVEFORMATEXTENSIBLE*, WAVEFORMATEXTENSIBLE*);
static int exToExtensible(WAVEFORMATEXTENSIBLE*, WAVEFORMATEX*, enum aaxRenderMode);

static int _wasapi_get_volume_range(_driver_t *);
static int _wasapi_set_volume(_driver_t*, const int32_t**, int, unsigned int, unsigned int, float);

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
#else
      if (handle->Mode == eCapture) {	/* capture is always exclusive */
          handle->status |= EXCLUSIVE_MODE_MASK;
      }
#endif
#if EVENT_DRIVEN
# if USE_CAPTURE_THREAD
      handle->status |= EVENT_DRIVEN_MASK;
# else
      /* event threads are not supported for capturing prior to Vista SP1 */
      /* nor for registered sensors                                       */
      if (handle->Mode == eRender) {
         handle->status |= EVENT_DRIVEN_MASK;
      }
# endif
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

      fmt.Format.nSamplesPerSec = 48000;
      fmt.Format.nChannels = 2;
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
                  handle->devname = charToWChar(_aaxNametoMMDevciceName(s));
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
               _AAX_DRVLOG(WASAPI_UNSUPPORTED_FREQUENCY);
               i = _AAX_MIN_MIXER_FREQUENCY;
            }
            else if (i > _AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_DRVLOG(WASAPI_UNSUPPORTED_FREQUENCY);
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
                  _AAX_DRVLOG(WASAPI_UNSUPPORTED_NO_TRACKS);
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_DRVLOG(WASAPI_UNSUPPORTED_NO_TRACKS);
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
               _AAX_DRVLOG(WASAPI_UNSUPPORTED_BPS);
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
      hr = pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
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
            handle->devname = charToWChar(_aaxNametoMMDevciceName(renderer));
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

         if (hr == S_OK)
         {
            hr = IAudioClient_GetService(handle->pAudioClient,
                                         pIID_IAudioSessionControl,
                                         (void**)&handle->pControl);
            if ((hr == S_OK) && (handle->pControl != NULL))
            {
               handle->events.lpVtbl = &_aaxAudioSessionEvents;
               IAudioSessionControl_RegisterAudioSessionNotification(
                                                               handle->pControl,
                                                               &handle->events);
            }
            else {
               _AAX_DRVLOG(WASAPI_EVENT_NOTIFICATION_FAILED);
            }
         }
         else // hr != S_OK
         {
            _AAX_DRVLOG(WASAPI_CONNECTION_FAILED);
            pIMMDeviceEnumerator_Release(handle->pEnumerator);
            if (handle->status & CO_INIT_MASK) {
               pCoUninitialize();
            }
            free(handle);
            handle = 0;
         }
      }
   }

   if (handle) {
      _wasapi_get_volume_range(handle);
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

      if (handle->log_file) {
         fclose(handle->log_file);
      }

      if (handle->pControl != NULL)
      {
         IAudioSessionControl_UnregisterAudioSessionNotification(
                                                              handle->pControl,
                                                              &handle->events);
         IAudioSessionControl_Release(handle->pControl);
      }

      if (handle->pEndpointVolume != NULL)
      {
         _wasapi_set_volume(handle, NULL, 0, 0, 0, handle->volumeInit);
         IAudioEndpointVolume_Release(handle->pEndpointVolume);
         handle->pEndpointVolume = NULL;
      }

      _aaxWASAPIDriverState(handle, DRIVER_PAUSE);

      if (handle->Event) {
         CloseHandle(handle->Event);
      }

      if (handle->pAudioClient != NULL)
      {
         hr = pIAudioClient_Stop(handle->pAudioClient);
         if (FAILED(hr)) {
            _AAX_DRVLOG(WASAPI_STOP_FAILED);
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
   unsigned int sample_frames = 1024;
   _driver_t *handle = (_driver_t *)id;
   REFERENCE_TIME hnsBufferDuration;
   REFERENCE_TIME hnsPeriodicity;
   WAVEFORMATEXTENSIBLE fmt;
   AUDCLNT_SHAREMODE mode;
   int co_init, frame_sz;
   int channels, bps;
   WAVEFORMATEX *wfx;
   HRESULT hr, init;
   DWORD stream;
   float freq;
   int rv = AAX_FALSE;

   assert(handle);

   freq = (float)*speed;
   if (frames && *frames) {
      sample_frames = *frames;
   }

   channels = *tracks;
   if (channels > handle->Fmt.Format.nChannels) {
      channels = handle->Fmt.Format.nChannels;
   }

   bps = aaxGetBytesPerSample(*format);
   frame_sz = channels * bps;

   co_init = AAX_FALSE;
   hr = pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
   if (hr != RPC_E_CHANGED_MODE) {
      co_init = AAX_TRUE;
   }

   init = S_OK;
   do
   {
      const WAVEFORMATEX *pfmt;
      WAVEFORMATEX **cfmt;

      /*
       * For shared mode set wfx to point to a valid, non-NULL pointer variable.
       * For exclusive mode, set wfx to NULL. The method allocates the storage
       * for the structure. The caller is responsible for freeing the storage,
       * when it is no longer needed, by calling the CoTaskMemFree function. If
       * the IsFormatSupported call fails and wfx is non-NULL, the method sets
       * *wfx to NULL.
       */
      
      if (handle->status & EXCLUSIVE_MODE_MASK)
      {
         wfx = NULL;
         cfmt = NULL;
         mode = AUDCLNT_SHAREMODE_EXCLUSIVE;
      }
      else
      {
         wfx = (WAVEFORMATEX*)&handle->Fmt.Format;
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

      pfmt = &fmt.Format;
      hr = pIAudioClient_IsFormatSupported(handle->pAudioClient,mode,pfmt,cfmt);
      if (hr == S_FALSE)
      {
         exToExtensible(&fmt, wfx, handle->setup);
         hr = S_OK;
      }
      else if (hr != S_OK)
      {
         /*
          * Succeeded but the specified format is not supported in exclusive
          * mode, try mixer format
          */
         if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT)
         {
            _AAX_DRVLOG(WASAPI_GET_DEVICE_FORMAT_FAILED);
            hr = pIAudioClient_GetMixFormat(handle->pAudioClient, &wfx);
            if (hr == S_OK)
            {
               hr = pIAudioClient_IsFormatSupported(handle->pAudioClient, mode,
                                                    &fmt.Format, &wfx);
            }
         }

#if USE_CAPTURE_THREAD
         if ((hr != S_OK) && (handle->status & EXCLUSIVE_MODE_MASK))
#else
         /*
          * eCapture mode must always be exclusive to prevent buffer
          * underflows in registered-sensor mode
          */
         if ((hr != S_OK) && (handle->Mode == eRender) &&
             (handle->status & EXCLUSIVE_MODE_MASK))
#endif
         {
            _AAX_DRVLOG(WASAPI_EXCLUSIVE_MODE_FAILED);

            handle->status &= ~EXCLUSIVE_MODE_MASK;

            wfx = &handle->Fmt.Format;
            mode = AUDCLNT_SHAREMODE_SHARED;
            hr = pIAudioClient_IsFormatSupported(handle->pAudioClient, mode,
                                                 &fmt.Format, &wfx);
            if (hr == S_FALSE)
            {
               exToExtensible(&fmt, wfx, handle->setup);
               hr = S_OK;
            }
         }
         if (hr != S_OK)
         {
            _AAX_DRVLOG(WASAPI_GET_AUDIO_FORMAT_FAILED);
            goto ExitSetup;
         }

         if (hr != S_OK) 
         {
            _AAX_DRVLOG(WASAPI_GET_DEVICE_FORMAT_FAILED);
            hr = pIAudioClient_GetMixFormat(handle->pAudioClient, &wfx);
         }

         if (hr != S_OK)
         {
            _AAX_DRVLOG(WASAPI_GET_MIXER_FORMAT_FAILED);
            goto ExitSetup;
         }
      }

      if (cfmt && *cfmt && (handle->status & EXCLUSIVE_MODE_MASK)) {
         CoTaskMemFree(*cfmt);
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
         _AAX_DRVLOG(WASAPI_UNCAUGHT_ERROR);
      }

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
         _AAX_DRVLOG(WASAPI_UNSUPPORTED_FORMAT);
         break;
      }

      /*
       * The buffer capacity as a time value. This parameter is of type
       * REFERENCE_TIME and is expressed in 100-nanosecond units. This
       * parameter contains the buffer size that the caller requests for the
       * buffer that the audio application will share with the audio engine
       * (in shared mode) or with the endpoint device (in exclusive mode). If
       * the call succeeds, the method allocates a buffer that is a least this
       * large.
       *
       * For a shared-mode stream that uses event-driven buffering, the caller
       * must set both hnsPeriodicity and hnsBufferDuration to 0. The
       * Initialize method determines how large a buffer to allocate based
       * on the scheduling period of the audio engine.
       */
      stream = 0;
      hnsBufferDuration = (REFERENCE_TIME)rintf(10000000.0f/freq*sample_frames);
      hnsPeriodicity = hnsBufferDuration;

#if USE_CAPTURE_THREAD 
# if CAPTURE_USE_MIN_PERIOD
      if (handle->Mode == eCapture)
      {                         /* use the minimum period size for capturing */
         REFERENCE_TIME hnsMinPeriodicity;
         hr = pIAudioClient_GetDevicePeriod(handle->pAudioClient, NULL,
                                            &hnsMinPeriodicity);
         if (hr == S_OK && init == S_OK) {
            hnsPeriodicity = hnsMinPeriodicity;
         }
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
      else /* timer driven */
      {
         /* make periods exact ms to match the lowest timer resolution */
         hnsBufferDuration = (hnsBufferDuration/10000) & 0xFFFFFFFE;
         hnsBufferDuration = (2 + hnsBufferDuration)*10000;

         hnsPeriodicity = (hnsPeriodicity/10000) & 0xFFFFFFFE;
         hnsPeriodicity = (2 + hnsPeriodicity)*10000;

         if (handle->Mode == eRender) {
            hnsBufferDuration *= PLAYBACK_PERIODS;
         } else {
            hnsBufferDuration *= CAPTURE_PERIODS;
         }

         if ((handle->status & EXCLUSIVE_MODE_MASK) == 0) { /* shared mode */
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
      if (hr == S_OK) break;

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
         {				/* try shared, event-driven mode */
            handle->status &= ~EXCLUSIVE_MODE_MASK;
            _AAX_DRVLOG(WASAPI_EXCLUSIVE_MODE_FAILED);
         }
         else if ((handle->status & EVENT_DRIVEN_MASK) &&
                  !(handle->status & EXCLUSIVE_MODE_MASK))
         {				/* try exclusive, timer-driven mode */
            handle->status &= ~EVENT_DRIVEN_MASK;
            handle->status |= EXCLUSIVE_MODE_MASK;
            _AAX_DRVLOG(WASAPI_EVENT_MODE_FAILED);
         }
         else if (!(handle->status & EVENT_DRIVEN_MASK) &&
                  (handle->status & EXCLUSIVE_MODE_MASK))
         {				/* try shared, timer-driven mode */
            handle->status &= ~EXCLUSIVE_MODE_MASK;
         }
         else
         {
            _AAX_DRVLOG(WASAPI_INITIALIZATION_FAILED);
            break;
         }
      }
      else if (hr == AUDCLNT_E_DEVICE_IN_USE)
      {
         if ((handle->status & EXCLUSIVE_MODE_MASK) == 0) break;
         handle->status &= ~EXCLUSIVE_MODE_MASK;
         _AAX_DRVLOG(WASAPI_EXCLUSIVE_MODE_FAILED);
      } 
      else if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED && init == S_OK)
      {
         init = hr;
         hr = pIAudioClient_GetBufferSize(handle->pAudioClient, &sample_frames);
         _AAX_DRVLOG(WASAPI_UNSUPPORTED_BUFFER_SIZE);
      }
      else {
         _AAX_DRVLOG_VAR("Uncaught initialization error: %x", hr);
         break;
      }

      /* init failed, close and re-open the device and try the new settings */
      pIAudioClient_Release(handle->pAudioClient);
      handle->pAudioClient = NULL;

      hr = pIMMDevice_Activate(handle->pDevice, pIID_IAudioClient,
                                     CLSCTX_INPROC_SERVER, NULL,
                                     (void**)&handle->pAudioClient);
      hr = S_FALSE;
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
      if (hr == S_OK && (handle->status & EXCLUSIVE_MODE_MASK) == 0) {
         hnsPeriodicity = defPeriod;
      }

      /* get the actual buffer size */
      periodFrameCnt = (UINT32)((hnsPeriodicity*freq + 10000000-1)/10000000);
      hr = pIAudioClient_GetBufferSize(handle->pAudioClient, &bufferFrameCnt);
      if (hr == S_OK)
      {
         int periods = rintf((float)bufferFrameCnt/(float)periodFrameCnt);
         if (periods < 1)
         {
            _AAX_DRVLOG(WASAPI_UNSUPPORTED_NO_PERIODS);
            periodFrameCnt = bufferFrameCnt;
         }

         handle->hw_buffer_frames = bufferFrameCnt;
         handle->buffer_frames = 4*bufferFrameCnt;
         handle->hnsPeriod = hnsPeriodicity;
         if (handle->Mode == eRender)
         {
            if (frames) *frames = periodFrameCnt;
            hr = pIAudioClient_GetService(handle->pAudioClient,
                                          pIID_IAudioRenderClient,
                                          (void**)&handle->uType.pRender);
         }
         else /* handle->Mode == eCapture */
         {
//          if (frames) *frames = periodFrameCnt; /* accept what's requested */
            handle->threshold = *frames*5/4;		/* same as ALSA */
            handle->packet_sz = (hnsPeriodicity*freq + 10000000-1)/10000000;

            hr = pIAudioClient_GetService(handle->pAudioClient,
                                          pIID_IAudioCaptureClient,
                                          (void**)&handle->uType.pCapture);
            handle->scratch_offs = 0;
            handle->scratch = (char*)0;
            handle->scratch_ptr = _aax_malloc(&handle->scratch,
                                              handle->buffer_frames*frame_sz);
            if (!handle->scratch_ptr) hr = S_FALSE;
         }

         if (hr == S_OK) {
            rv = AAX_TRUE;
         }
         else {
            _AAX_DRVLOG(WASAPI_GET_AUDIO_SERVICE_FAILED);
         }
      }
      else {
         _AAX_DRVLOG(WASAPI_SET_BUFFER_SIZE_FAILED);
      }

      hr = pIAudioClient_GetStreamLatency(handle->pAudioClient, &latency);
      if (hr == S_OK) {
         handle->hnsLatency = latency;
      }
#if 1
 _AAX_DRVLOG_VAR("AeonWave version %i.%i.%i-%i", AAX_MAJOR_VERSION, AAX_MINOR_VERSION, AAX_MICRO_VERSION, AAX_PATCH_LEVEL);
 _AAX_DRVLOG_VAR("Device: %s", _aaxMMDeviceNameToName(detect_devname(handle->pDevice)));
 _AAX_DRVLOG_VAR("Format for %s", (handle->Mode == eRender) ? "Playback" : "Capture");
 _AAX_DRVLOG_VAR("- event driven: %s, exclusive: %s", (handle->status & EVENT_DRIVEN_MASK)?"true":"false", (handle->status & EXCLUSIVE_MODE_MASK)?"true":"false");
 _AAX_DRVLOG_VAR("- frequency: %i", (int)handle->Fmt.Format.nSamplesPerSec);
 _AAX_DRVLOG_VAR("- no. tracks: %i", handle->Fmt.Format.nChannels);
 _AAX_DRVLOG_VAR("- block size: %i (cb: %i)", handle->Fmt.Format.nBlockAlign, handle->Fmt.Format.cbSize);
 _AAX_DRVLOG_VAR("- bits/sample: %i", handle->Fmt.Format.wBitsPerSample);
 _AAX_DRVLOG_VAR("- valid bits/sample: %i", handle->Fmt.Samples.wValidBitsPerSample);
 _AAX_DRVLOG_VAR("- subformat: float: %x - pcm: %x",
          IsEqualGUID(&handle->Fmt.SubFormat, pKSDATAFORMAT_SUBTYPE_IEEE_FLOAT),
          IsEqualGUID(&handle->Fmt.SubFormat, pKSDATAFORMAT_SUBTYPE_PCM));
 _AAX_DRVLOG_VAR("- periods: default: %4.2f ms, minimum: %4.2f ms", defPeriod/10000.0f, minPeriod/10000.0f);
 _AAX_DRVLOG_VAR("- latency: %5.3f ms", handle->hnsLatency/10000.0f);
 _AAX_DRVLOG_VAR("- buffers: %i frames, total: %i frames, periods: %i", periodFrameCnt, bufferFrameCnt, rintf((float)bufferFrameCnt/(float)periodFrameCnt));
 _AAX_DRVLOG_VAR("- speaker mask: 0x%x", (int)handle->Fmt.dwChannelMask);
 _AAX_DRVLOG_VAR("- volume: %5.4f (%3.1f dB), min: %5.4f (%3.1f dB), max: %5.4f (%3.1f dB)", handle->volumeCur, _lin2db(handle->volumeCur), handle->volumeMin, _lin2db(handle->volumeMin), handle->volumeMax, _lin2db(handle->volumeMax));
#endif
   }
   else {
      _AAX_DRVLOG(WASAPI_INITIALIZATION_FAILED);
   }

ExitSetup:
   if (co_init) {
      pCoUninitialize();
   }

   return rv;
}

int
_aaxWASAPIDriver3dMixer(const void *id, void *d, void *s, void *p, void *m, int n, unsigned char ctr, unsigned int nbuf)
{
   _driver_t *handle = (_driver_t *)id;
   float gain = 1.0f;
   int ret;

   assert(s);
   assert(d);
   assert(p);

   ret = handle->mix_mono3d(d, s, p, m, gain, n, ctr, nbuf);

   return ret;
}

static int
_aaxWASAPIDriverCapture(const void *id, void **data, int offs, size_t *req_frames, void *scratch, size_t scratchlen, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int no_frames;
   int rv = AAX_FALSE;

   if (!handle || !handle->scratch_ptr || !req_frames || !data) {
      return AAX_FALSE;
   }

   no_frames = *req_frames;
   if (no_frames == 0) {
      return AAX_TRUE;
   } 

   if (handle->status & DRIVER_INIT_MASK) {
      return _aaxWASAPIDriverState(handle, DRIVER_RESUME);
   }

   if (data && handle->scratch_ptr)
   {
      unsigned int frame_sz = handle->Fmt.Format.nBlockAlign;
      unsigned int tracks = handle->Fmt.Format.nChannels;
      unsigned int corr, fetch = no_frames;
      float diff;

      if (handle->status & CAPTURE_INIT_MASK)
      {
         if ((handle->status & EVENT_DRIVEN_MASK) == 0)
         {
            if (!handle->cthread) {
               _aaxWASAPIDriverCaptureFromHardware(handle);
            }
         }
         handle->scratch_offs = 0;
         handle->status &= ~CAPTURE_INIT_MASK;
         return AAX_TRUE;
      }

      if (handle->cthread) {				/* Lock the mutex */
         EnterCriticalSection(&handle->mutex);
      }

      /* try to keep the buffer padding at the threshold level at all times */
      diff = (float)handle->scratch_offs - (float)handle->threshold;
      handle->padding = (handle->padding + diff/(float)no_frames)/2;
      corr = _MINMAX(roundf(handle->padding), -1, 1);
      fetch += corr;
      offs -= corr;

      if (!handle->cthread) { 		/* fetch any new data packates */
         _aaxWASAPIDriverCaptureFromHardware(handle);
      }

      /* copy data from the buffer */
      *req_frames = fetch;
      if (fetch)
      {
         unsigned int avail = _MIN(handle->scratch_offs, fetch);
         int32_t **ptr = (int32_t**)data;

         if (avail) {
            handle->cvt_from_intl(ptr, handle->scratch, offs, tracks, avail);
         }

         _wasapi_set_volume(handle, (const int32_t**)ptr,
                            offs, avail, tracks, gain);

         if (avail < fetch)
         {
            unsigned int i;
            for (i=0; i<tracks; i++) {
               memset(ptr[i]+offs+avail, 0, (fetch-avail)*sizeof(int32_t));
            }
         }

         handle->scratch_offs -= avail;
         fetch -= avail;

         if (handle->scratch_offs)
         {
            memmove(handle->scratch, handle->scratch + avail*frame_sz,
                    handle->scratch_offs*frame_sz);
         }
      }

      if (handle->cthread) {				/* Unlock the mutex */
         LeaveCriticalSection(&handle->mutex);
      }

      if (fetch) {
         _AAX_DRVLOG(WASAPI_BUFFER_UNDERRUN);
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
_aaxWASAPIDriverPlayback(const void *id, void *src, float pitch, float gain)
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
   offs = _oalRingBufferGetParami(rbs, RB_OFFSET_SAMPLES);
   no_frames = _oalRingBufferGetParami(rbs, RB_NO_SAMPLES) - offs;
   no_tracks = _oalRingBufferGetParami(rbs, RB_NO_TRACKS);

   assert(_oalRingBufferGetNoSamples(rbs) >= offs);

   if (handle->status & DRIVER_INIT_MASK) {
      return _aaxWASAPIDriverState(handle, DRIVER_RESUME);
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

      assert((handle->status & DRIVER_INIT_MASK) == 0);
      assert(frames <= no_frames);

      if (frames >= no_frames)
      {
         IAudioRenderClient *pRender = handle->uType.pRender;
         const int32_t **sbuf = (const int32_t**)rbsd->track;
         BYTE *data = NULL;

         _wasapi_set_volume(handle, sbuf, offs, no_frames, no_tracks, gain);

         hr = pIAudioRenderClient_GetBuffer(pRender, frames, &data);
         if (hr == S_OK)
         {
            handle->cvt_to_intl(data, sbuf, offs, no_tracks, no_frames);

            hr = pIAudioRenderClient_ReleaseBuffer(handle->uType.pRender,
                                                frames, 0);
            if (hr != S_OK) {
               _AAX_DRVLOG(WASAPI_RELEASE_BUFFER_FAILED);
            }

            assert(no_frames >= frames);
            no_frames -= frames;
         }
         else {
            _AAX_DRVLOG(WASAPI_GET_BUFFER_FAILED);
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

static int
_aaxWASAPIDriverState(const void *id, enum _aaxDriverState state)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;
   DWORD st;
   HRESULT hr;

   switch(state)
   {
   case DRIVER_AVAILABLE:
      if (handle)
      {
         assert(handle->pDevice);

         hr = pIMMDevice_GetState(handle->pDevice, &st);
         if (hr == S_OK) {
            rv = (st == DEVICE_STATE_ACTIVE) ? AAX_TRUE : AAX_FALSE;
         }
      }
      break;
   case DRIVER_PAUSE:
      if (handle && ((handle->status & DRIVER_PAUSE_MASK) == 0))
      {
#if USE_CAPTURE_THREAD
         if (handle->Mode == eCapture) {
            handle->cthread_init = -1;
         }
#endif

         hr = pIAudioClient_Stop(handle->pAudioClient);
         if (hr == S_OK)
         {
            hr = pIAudioClient_Reset(handle->pAudioClient);
            handle->status |= (CAPTURE_INIT_MASK | DRIVER_PAUSE_MASK);
            rv = AAX_TRUE;
         }

         _aaxCaptureThreadStop(handle);
      }
      break;
   case DRIVER_RESUME:
      if (handle && (handle->status & DRIVER_PAUSE_MASK))
      {
         hr = S_OK;

         _aaxCaptureThreadStart(handle);

         hr = pIAudioClient_Start(handle->pAudioClient);
         if (hr == S_OK)
         {
            handle->status &= ~DRIVER_INIT_MASK;
            rv = AAX_TRUE;
         }
         else {
            _AAX_DRVLOG(WASAPI_RESUME_FAILED);
         }
      } else {
         rv = AAX_TRUE;
      }
      handle->status &= ~DRIVER_PAUSE_MASK;      break;
   case DRIVER_SHARED_MIXER:
      rv = (handle->status & EXCLUSIVE_MODE_MASK) ? AAX_FALSE : AAX_TRUE;
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
_aaxWASAPIDriverParam(const void *id, enum _aaxDriverParam param)
{
   _driver_t *handle = (_driver_t *)id;
   float rv = 0.0f;
   if (handle)
   {
      switch(param)
      {
      case DRIVER_LATENCY:
         rv = handle->hnsLatency*100e-9f;
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
      default:
         break;
      }
   }
   return rv;
}


static char *
_aaxWASAPIDriverGetDevices(const void *id, int mode)
{
   static char names[2][1024] = { "\0\0", "\0\0" };
   static time_t t_previous[2] = { 0, 0 };
   int m = (mode > 0) ? 1 : 0;
   time_t t_now;

   t_now = time(NULL);
   if (t_now > (t_previous[m]+5))
   {
      IMMDeviceEnumerator *enumerator = NULL;
      int co_init = AAX_FALSE;
      HRESULT hr;

      t_previous[m] = t_now;

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
         char *ptr, *prev;
         UINT i, count;
         int len;

         len = 1023;
         ptr = (char *)&names[m];
         memset(ptr, 0, 1024);

         hr = pIMMDeviceEnumerator_EnumAudioEndpoints(enumerator, _mode[m],
                                     DEVICE_STATE_ACTIVE|DEVICE_STATE_UNPLUGGED,
                                     &collection);
         if (FAILED(hr)) goto ExitGetDevices;

         hr = pIMMDeviceCollection_GetCount(collection, &count);
         if (FAILED(hr)) goto ExitGetDevices;

         prev = "";
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
            if (devname && strcmp(devname, prev))
            {		// devname doesn't match the previous device
               slen++;
               len -= slen;
               prev = ptr;
               ptr += slen;
               if (len > 0) {
                  *(ptr+1) = 0;
               } else {
                  break;
               }
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

         /* always end with "\0\0" no matter what */
         names[m][1022] = 0;
         names[m][1023] = 0;

         return (char *)&names[m];

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

      memset(interfaces, 0, 1024);

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
                     if (len > 0) {
                        *(ptr+1) = 0;
                     } else {
                        break;
                     }
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
_aaxWASAPIDriverLogVar(const void *id, const char *fmt, ...)
{
   char _errstr[1024];
   va_list ap;

   _errstr[0] = '\0';
   va_start(ap, fmt);
   vsnprintf(_errstr, 1024, fmt, ap);

   /* always add a null-termination */
   _errstr[1023] = '\0';
   va_end(ap);

   return _aaxWASAPIDriverLog(id, 0, WASAPI_VARLOG, _errstr);
}

static char *
_aaxWASAPIDriverLog(const void *id, int prio, int type, const char *fn)
{
   _driver_t *handle = (_driver_t*)id;
   int prev_type = WASAPI_NO_ERROR;
   unsigned int type_ctr = 0;
   static char _errstr[256];
   char *perr = _errstr;

   if (handle)
   {
      type_ctr = handle->type_ctr;
      prev_type = handle->prev_type;
      perr = handle->errstr;
   }

#if LOG_TO_FILE
   if (handle && (handle->log_file == NULL))
   {
      char *tmp = getenv("TEMP");
      if (!tmp) tmp = getenv("TMP");
      if (tmp)
      {  
         char *pfname = detect_devname(handle->pDevice);
         char fname[1024];

         if (!pfname) pfname = "AAXDriverLog";
         snprintf(fname, 1024, "%s\\%s.log", tmp, pfname);
         handle->log_file = fopen(fname, "w");
      }
   }
#endif

   if (type == WASAPI_VARLOG)
   {
      __aaxErrorSet(AAX_BACKEND_ERROR, (char*)&fn);
      OutputDebugString(fn);
      _AAX_SYSLOG(fn);
#if LOG_TO_FILE
      if (handle && handle->log_file) {
         fprintf(handle->log_file, "%s\n", fn);
      }
#endif
      prev_type = type;
      type_ctr = 0;
   }
   else if ((type > WASAPI_NO_ERROR) && (type < WASAPI_MAX_ERROR) &&
       (type != prev_type) && (type_ctr < 100))
   {
      if (type_ctr > 0)
      {
         snprintf(perr, 255, "error repeated %i times", type_ctr+1);
         __aaxErrorSet(AAX_BACKEND_ERROR, perr);
         OutputDebugString(perr);
         _AAX_SYSLOG(perr);
#if LOG_TO_FILE
         if (handle && handle->log_file) {
            fprintf(handle->log_file, "%s\n", perr);
         }
#endif
         type_ctr = 0;
      }

      snprintf(perr, 255, "%s: %s", fn, _wasapi_errors[type]);
      __aaxErrorSet(AAX_BACKEND_ERROR, perr);
      OutputDebugString(perr);
      _AAX_SYSLOG(perr);
#if LOG_TO_FILE
      if (handle && handle->log_file)
      {
         fprintf(handle->log_file, "%s\n", perr);
         if (handle->err_ctr++ == 25)
         {
            fseek(handle->log_file, 0L, SEEK_CUR);	// sync.
            handle->err_ctr = 0;
         }
      }
#endif
      prev_type = type;
   }
   else type_ctr++;

   if (handle)
   {
      handle->type_ctr = type_ctr;
      handle->prev_type = prev_type;
   }

   return perr;
}

/* -------------------------------------------------------------------------- */

static HRESULT
_aaxCaptureThreadStart(_driver_t *id)
{
   HRESULT hr = S_FALSE;

#if USE_CAPTURE_THREAD
   _driver_t *handle = id;

   if (handle->Mode == eCapture)
   {
      LPTHREAD_START_ROUTINE cb;

      InitializeCriticalSection(&handle->mutex);
      handle->cthread_init = 0;

      cb = (LPTHREAD_START_ROUTINE)&_aaxWASAPIDriverCaptureThread;
      handle->cthread = CreateThread(NULL, 0, cb, (LPVOID)handle, 0, NULL);
      if (handle->cthread == 0) {
         _AAX_DRVLOG(WASAPI_CREATE_CAPTURE_THREAD_FAILED);
      }
      else
      {
         while (handle->cthread_init == 0) {
            msecSleep(10);
         }
         hr = S_OK;
      }
   }
#endif
   return hr;
}

static HRESULT
_aaxCaptureThreadStop(_driver_t *id)
{
   HRESULT hr = S_FALSE;

#if USE_CAPTURE_THREAD
   _driver_t *handle = id;

   if (handle->Mode == eCapture)
   {
      if (handle->cthread)
      {
         WaitForSingleObject(handle->cthread, INFINITE);

         DeleteCriticalSection(&handle->mutex);
         CloseHandle(handle->cthread);
         handle->cthread = NULL;
         hr = S_OK;
      }
   }
#endif
   return hr;
}

#if USE_CAPTURE_THREAD
static DWORD
_aaxWASAPIDriverCaptureThread(LPVOID id)
{
   _driver_t *handle = (_driver_t*)id;
   unsigned int stdby_time;
   int co_init;
   HRESULT hr;

   assert(handle);

   co_init = AAX_FALSE;
   hr = pCoInitializeEx(NULL, COINIT_MULTITHREADED);
   if (hr != RPC_E_CHANGED_MODE) {
      co_init = AAX_TRUE;
   }
   if (FAILED(hr)) return -1;

   if (pAvSetMmThreadCharacteristicsA)
   {
      DWORD tIdx = 0;
      handle->task = pAvSetMmThreadCharacteristicsA("Pro Audio", &tIdx);
   }

   hr = S_OK;
   if (handle->status & EVENT_DRIVEN_MASK)
   {
      handle->Event = CreateEvent(NULL, FALSE, FALSE, NULL);
      if (handle->Event) {
         hr = pIAudioClient_SetEventHandle(handle->pAudioClient,
                                          handle->Event);
      }
      else {
         _AAX_DRVLOG(WASAPI_EVENT_CREATION_FAILED);
      }
   }
   else				/* timer driven, creat a periodic timer */
   {
      handle->Event = CreateWaitableTimer(NULL, FALSE, NULL);
      if (handle->Event)
      {
         LARGE_INTEGER liDueTime;
         LONG lPeriod;

         /*
          * In Timer Driven mode, we want to wait for half the desired latency
          * in milliseconds.
          *
          * That way we'll wake up half way through the processing period to
          * pull the next set of samples from the engine.
          */
         lPeriod = (LONG)rintf(handle->hnsPeriod/(2*10*1000));
         liDueTime.QuadPart = -(LONGLONG)rintf(handle->hnsPeriod/2);
         hr = SetWaitableTimer(handle->Event, &liDueTime, lPeriod,
                               NULL, NULL, FALSE);
         if (hr) hr = S_OK;
         else hr = S_FALSE;
      }
      else {
         _AAX_DRVLOG(WASAPI_TIMER_CREATION_FAILED);
      }
   }

   if (handle->Event && SUCCEEDED(hr))
   {
      handle->cthread_init = 1;

      /*
       * the standby timeout is ten times the period length to not interfere 
       * with the more accurate event drivers.
       */
      stdby_time = handle->hnsPeriod/1000;
      do
      {
         hr = WaitForSingleObject(handle->Event, stdby_time);
         if (handle->cthread_init < 0) break;

         switch (hr)
         {
         case WAIT_TIMEOUT:
            _AAX_DRVLOG(WASAPI_EVENT_TIMEOUT);
            /* break not needed */
         case WAIT_OBJECT_0:
            _aaxWASAPIDriverCaptureFromHardware(handle);
            break;
         case WAIT_ABANDONED:
         case WAIT_FAILED:
         default:
            _AAX_DRVLOG(WASAPI_WAIT_EVENT_FAILED);
            hr = S_FALSE;
            break;
         }
      }
      while(hr != S_FALSE);
   }
   else {
      _AAX_DRVLOG(WASAPI_EVENT_SETUP_FAILED);
   }

   if (handle->Event)
   {
      if ((handle->status & EVENT_DRIVEN_MASK) == 0) {
         CancelWaitableTimer(handle->Event);
      }
      else {
         CloseHandle(handle->Event);
      }
      handle->Event = NULL;
   }

   if (pAvRevertMmThreadCharacteristics) {
      pAvRevertMmThreadCharacteristics(handle->task);
   }
   handle->task = 0;

   if (co_init) {
      CoUninitialize();
   }
   return 0;
}
#endif

static int
_aaxWASAPIDriverCaptureFromHardware(_driver_t *id)
{
   _driver_t *handle = id;
   HRESULT hr = S_OK;
   UINT32 avail = -1;

   hr = pIAudioClient_GetCurrentPadding(handle->pAudioClient, &avail);
   while ((hr == S_OK) && avail)
   {
      unsigned int frame_sz = handle->Fmt.Format.nBlockAlign;
      size_t offs_bytes = handle->scratch_offs * frame_sz;
      unsigned int new_scratch_offs;
      BYTE* buf = NULL;
      DWORD flags = 0;
      HRESULT res;

      /*
       * lock the mutex, needs to be before GetBuffer to keep the time
       * between GetBuffer and ReleaseBuffer as short as humanly possible
       */
      if (handle->cthread) {
         EnterCriticalSection(&handle->mutex);
      }

      hr = pIAudioCaptureClient_GetBuffer(handle->uType.pCapture, &buf, &avail,
                                          &flags, NULL, NULL);
      if (SUCCEEDED(hr))
      {              /* no error, but maybe there is no data available */
         new_scratch_offs = handle->scratch_offs + avail;
         if (new_scratch_offs <= handle->buffer_frames)
         {
            if (hr == S_OK)			/* there is data available */
            {
               if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0) {
                  _aax_memcpy(handle->scratch+offs_bytes, buf, avail*frame_sz);
               } else {
                  memset(handle->scratch + offs_bytes, 0, avail*frame_sz);
               }
               handle->scratch_offs = new_scratch_offs;
            }
//          else _AAX_DRVLOG_VAR("AUDCLNT_S_BUFFER_EMPTY");
         }
         else if (avail >= handle->hw_buffer_frames)
         {
            avail = 0;
            hr = S_FALSE;
            _AAX_DRVLOG(WASAPI_BUFFER_OVERRUN);
         }

         /* release the original packet size or 0 */
         res = pIAudioCaptureClient_ReleaseBuffer(handle->uType.pCapture,
                                                     avail);
         if (FAILED(res)) {
            _AAX_DRVLOG(WASAPI_RELEASE_BUFFER_FAILED);
         }

         if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {
            _AAX_DRVLOG(WASAPI_DATA_DISCONTINUITY);
         }
      }
      else {
         _AAX_DRVLOG(WASAPI_GET_BUFFER_FAILED);
      }

      if (handle->cthread) {				/* unlock the mutex */
         LeaveCriticalSection(&handle->mutex);
      }
   } /* while (hr == S_OK) */

   return hr;
}

/* turn "<device>: <interface>" back into "<interface> (<device>)" */
static const char *
_aaxNametoMMDevciceName(const char *devname)
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

/* turn "<interface> (<device>)" into "<device>: <interface>" */
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
         ptr += 2;			/* skip " ("         */

         len = strlen(ptr);
         *(ptr+len-1) = 0;		/* don't include ')' */

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
                                     DEVICE_STATE_ACTIVE|DEVICE_STATE_UNPLUGGED,
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
   const void *id;
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
         _oalRingBufferSetParami(dest_rb, RB_NO_TRACKS, mixer->info->no_tracks);
         _oalRingBufferSetParamf(dest_rb, RB_FREQUENCY, mixer->info->frequency);
         _oalRingBufferSetParamf(dest_rb, RB_DURATION_SEC, delay_sec);
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

   id = be_handle = (_driver_t *)handle->backend.handle;
   be->state(be_handle, DRIVER_PAUSE);
   state = AAX_SUSPENDED;

   /* get real duration, it might have been altered for better performance */
   delay_sec = _oalRingBufferGetParamf(dest_rb, RB_DURATION_SEC);

   _aaxMutexLock(handle->thread.mutex);
   stdby_time = (int)(4*delay_sec*1000);

   hr = S_OK;
   be_handle = (_driver_t *)handle->backend.handle;
   if (be_handle->status & EVENT_DRIVEN_MASK)
   {
      be_handle->Event = CreateEvent(NULL, FALSE, FALSE, NULL);
      if (be_handle->Event) {
         hr = pIAudioClient_SetEventHandle(be_handle->pAudioClient,
                                          be_handle->Event);
      }
      else {
         _AAX_DRVLOG(WASAPI_EVENT_CREATION_FAILED);
      }
   }
   else				/* timer driven, creat a periodic timer */
   {
      setTimerResolution(1);
      be_handle->Event = CreateWaitableTimer(NULL, FALSE, NULL);
      if (be_handle->Event)
      {
         LARGE_INTEGER liDueTime;
         LONG lPeriod;

         lPeriod = (LONG)rintf(delay_sec*1000);
         liDueTime.QuadPart = -(LONGLONG)rintf(delay_sec*1000*1000*10);
         hr = SetWaitableTimer(be_handle->Event, &liDueTime, lPeriod,
                               NULL, NULL, FALSE);
         if (hr) hr = S_OK;
         else hr = S_FALSE;
      }
      else {
         _AAX_DRVLOG(WASAPI_TIMER_CREATION_FAILED);
      }
   }
   if (!be_handle->Event || FAILED(hr)) {
      _AAX_DRVLOG(WASAPI_EVENT_SETUP_FAILED);
   }

   /* playback loop */
   while ((hr == S_OK) && TEST_FOR_TRUE(handle->thread.started))
   {
      _aaxMutexUnLock(handle->thread.mutex);

      if ((_IS_PLAYING(handle) && be->state(be_handle, DRIVER_AVAILABLE)) ||
          ((be_handle->status & EVENT_DRIVEN_MASK) == 0))
      {
         hr = WaitForSingleObject(be_handle->Event, stdby_time);
         switch (hr)
         {
         case WAIT_OBJECT_0:	/* event was triggered */
            break;
         case WAIT_TIMEOUT:	/* wait timed out      */
            if ((be_handle->status & DRIVER_INIT_MASK) == 0) {
               _AAX_DRVLOG(WASAPI_EVENT_TIMEOUT);
            }
            break;
         case WAIT_ABANDONED:
         case WAIT_FAILED:
         default:
            _AAX_DRVLOG(WASAPI_WAIT_EVENT_FAILED);
            hr = S_FALSE;
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
      /* do all the mixing */
      if (_IS_PLAYING(handle) && be->state(be_handle, DRIVER_AVAILABLE)) {
         _aaxSoftwareMixerThreadUpdate(handle, dest_rb);
      }
#if ENABLE_TIMING
{
float elapsed = _aaxTimerElapsed(timer);
if (elapsed > delay_sec)
_AAX_DRVLOG_VAR("elapsed: %f ms (%f)\n", elapsed*1000.0f, delay_sec*1000.0f);
}
#endif

      hr = S_OK;
   }

   if ((be_handle->status & EVENT_DRIVEN_MASK) == 0)
   {
      CancelWaitableTimer(be_handle->Event);
      resetTimerResolution(1);
   }
   else {
      CloseHandle(be_handle->Event);
   }
   be_handle->Event = NULL;
   
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
      _aaxWASAPIDriverLog(NULL, 0, WASAPI_UNSUPPORTED_FORMAT, __FUNCTION__);
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
      _aaxWASAPIDriverLog(NULL, 0, WASAPI_UNSUPPORTED_NO_TRACKS, __FUNCTION__);
      break;
   }
   return rv;
}

/**
 * http://msdn.microsoft.com/en-us/library/windows/desktop/dd370839%28v=vs.85%29.aspx 
 *
 * Applications that manage exclusive-mode streams can control the volume levels
 * of those streams through the IAudioEndpointVolume interface. This interface
 * controls the volume level of the audio endpoint device. It uses the hardware
 * volume control for the endpoint device if the audio hardware implements such
 * a control. Otherwise, the IAudioEndpointVolume interface implements the
 * volume control in software.
 *
 * http://blogs.msdn.com/b/larryosterman/default.aspx?PageIndex=1&PostSortBy=MostViewed
 */
static int
_wasapi_set_volume(_driver_t *handle, const int32_t **sbuf, int offset, unsigned int no_frames, unsigned int no_tracks, float volume)
{
   float gain = fabsf(volume);
   float hwgain = gain;
   int rv = 0;

   if (handle && HW_VOLUME_SUPPORT(handle) &&
       (handle->status & EXCLUSIVE_MODE_MASK))
   {
      hwgain = _MINMAX(gain, handle->volumeMin, handle->volumeMax);

      /*
       * Slowly adjust volume to dampen volume slider movement.
       * If the volume step is large, don't dampen it.
       * volume is negative for auto-gain mode.
       */
      if ((volume < 0.0f) && (handle->Mode == eCapture))
      {
         float dt = GMATH_E1*no_frames/handle->Fmt.Format.nSamplesPerSec;
         float rr = _MINMAX(dt/5.0f, 0.0f, 1.0f);	/* 10 sec average */

         /* Quickly adjust for a very large step in volume */
         if (fabsf(hwgain - handle->volumeCur) > 0.825f) rr = 0.9f;

         hwgain = (1.0f-rr)*handle->hwgain + (rr)*hwgain;
         handle->hwgain = hwgain;
      }

      if (fabsf(hwgain - handle->volumeCur) > 0.05f)
      {
         float cur;
         rv = IAudioEndpointVolume_GetMasterVolumeLevel(handle->pEndpointVolume,
                                                        &cur);
         if (rv == S_OK) {
            handle->volumeHW = _db2lin(cur);
         }

         rv = IAudioEndpointVolume_SetMasterVolumeLevel(handle->pEndpointVolume,
                                                        _lin2db(hwgain), NULL);
          handle->volumeCur = hwgain;
      }

      if (hwgain) gain /= hwgain;
      else gain = 0.0f;
   }

   /* software volume fallback */
   if (fabsf(gain - 1.0f) > 0.05f)
   {
      int t;
      for (t=0; t<no_tracks; t++) {
         _batch_mul_value((void*)(sbuf[t]+offset), sizeof(int32_t),
                          no_frames, gain);
      }
   }

   return rv;
}

static int
_wasapi_get_volume_range(_driver_t *handle)
{
   int rv;

   rv = pIMMDevice_Activate(handle->pDevice, pIID_IAudioEndpointVolume,
                            CLSCTX_INPROC_SERVER, NULL,
                            (void**)&handle->pEndpointVolume);
   if (rv == S_OK)
   {
      DWORD mask = 0;
      rv = IAudioEndpointVolume_QueryHardwareSupport(handle->pEndpointVolume,
                                                     &mask);
      if (rv == S_OK && (mask & ENDPOINT_HARDWARE_SUPPORT_VOLUME))
      {
         float cur, min, max, step;

         rv = IAudioEndpointVolume_GetVolumeRange(handle->pEndpointVolume,
                                                  &min, &max, &step);
         if (rv == S_OK)
         {
            handle->volumeMin = _db2lin(min);
            handle->volumeMax = _db2lin(max);
         }
         else
         {
            handle->volumeMin = 0.0f;
            handle->volumeMax = 0.0f;
         }

         rv = IAudioEndpointVolume_GetMasterVolumeLevel(handle->pEndpointVolume,
                                                        &cur);
         if (rv == S_OK) {
            handle->volumeInit = _db2lin(cur);
         } else {
            handle->volumeInit = 1.0f;
         }
         handle->volumeCur = handle->volumeInit;
      }
   }

   return rv;
}

/** Audio Session Events */
static _driver_t *
_aaxAudioSessionEventsUserData(IAudioSessionEvents *event)
{
   return (void *)(((char *)event) - offsetof(_driver_t, events));
}

static STDMETHODIMP
_aaxAudioSessionEvents_QueryInterface(IAudioSessionEvents *event, REFIID riid,
                                      void **ppvInterface)
{
#if 1
   if (IsEqualIID(riid, pIID_IUnknown)
     || IsEqualIID(riid, pIID_IAudioSessionEvents))
    {
        *ppvInterface = event;
        IUnknown_AddRef(event);
        return S_OK;
    }
    else
    {
       *ppvInterface = NULL;
        return E_NOINTERFACE;
    }
#else
   if (IsEqualIID(riid, pIID_IUnknown))
   {
      IUnknown_AddRef(event);
      *ppvInterface = (IUnknown*)event;
   }
   else if (IsEqualIID(riid, pIID_IAudioSessionEvents))
   {
      IUnknown_AddRef(event);
      *ppvInterface = event;
   }
   else
   {
      *ppvInterface = NULL;
      return E_NOINTERFACE;
   }
#endif
   return S_OK;
}

static STDMETHODIMP_(ULONG)
_aaxAudioSessionEvents_AddRef(IAudioSessionEvents *event)
{
   _driver_t *handle = _aaxAudioSessionEventsUserData(event);
   return InterlockedIncrement(&handle->refs);
}

static STDMETHODIMP_(ULONG)
_aaxAudioSessionEvents_Release(IAudioSessionEvents *event)
{
   _driver_t *handle = _aaxAudioSessionEventsUserData(event);
   return InterlockedDecrement(&handle->refs);
}

static STDMETHODIMP
_aaxAudioSessionEvents_OnDisplayNameChanged(IAudioSessionEvents *event,
                                            LPCWSTR NewDisplayName,
                                            LPCGUID EventContext)
{
   return S_OK;
}

static STDMETHODIMP
_aaxAudioSessionEvents_OnIconPathChanged(IAudioSessionEvents *event,
                                         LPCWSTR NewIconPath,
                                         LPCGUID EventContext)
{
   return S_OK;
}

static STDMETHODIMP
_aaxAudioSessionEvents_OnSimpleVolumeChanged(IAudioSessionEvents *event,
                                             float NewVolume,
                                              BOOL NewMute,
                                              LPCGUID EventContext)
{
   if (NewMute)
   {
      printf("MUTE\n");
   }
   else
   {
      printf("Volume = %d percent\n", (UINT32)(100*NewVolume + 0.5));
   }
   return S_OK;
}

static STDMETHODIMP
_aaxAudioSessionEvents_OnChannelVolumeChanged(IAudioSessionEvents *event,
                                              DWORD ChannelCount,
                                              float NewChannelVolumeArray[],
                                              DWORD ChangedChannel,
                                              LPCGUID EventContext)
{
   return S_OK;
}

static STDMETHODIMP
_aaxAudioSessionEvents_OnGroupingParamChanged(IAudioSessionEvents *event,
                                              LPCGUID NewGroupingParam,
                                              LPCGUID EventContext)

{
   return S_OK;
}

static STDMETHODIMP
_aaxAudioSessionEvents_OnStateChanged(IAudioSessionEvents *event,
                                      AudioSessionState NewState)
{
   char *pszState = "?????";

   switch (NewState)
   {
   case AudioSessionStateActive:
      pszState = "active";
      break;
   case AudioSessionStateInactive:
      pszState = "inactive";
      break;
   case AudioSessionStateExpired:
      pszState = "expired";
      break;
   }
   printf("New session state = %s\n", pszState);
   return S_OK;
}

static STDMETHODIMP
_aaxAudioSessionEvents_OnSessionDisconnected(IAudioSessionEvents *event,
                                  AudioSessionDisconnectReason DisconnectReason)
{
// _driver_t *handle = _aaxAudioSessionEventsUserData(event);
   char *pszReason = "?????";

   switch (DisconnectReason)
   {
   case DisconnectReasonDeviceRemoval:
      pszReason = "device removed";
      break;
   case DisconnectReasonServerShutdown:
      pszReason = "server shut down";
      break;
   case DisconnectReasonFormatChanged:
      pszReason = "format changed";
      break;
   case DisconnectReasonSessionLogoff:
      pszReason = "user logged off";
      break;
   case DisconnectReasonSessionDisconnected:
      pszReason = "session disconnected";
      break;
   case DisconnectReasonExclusiveModeOverride:
      pszReason = "exclusive-mode override";
      break;
   }
   printf("Audio session disconnected (reason: %s)\n",
          pszReason);

   return S_OK;
}

static const struct IAudioSessionEventsVtbl _aaxAudioSessionEvents =
{
   _aaxAudioSessionEvents_QueryInterface,
   _aaxAudioSessionEvents_AddRef,
   _aaxAudioSessionEvents_Release,

   _aaxAudioSessionEvents_OnDisplayNameChanged,
   _aaxAudioSessionEvents_OnIconPathChanged,
   _aaxAudioSessionEvents_OnSimpleVolumeChanged,
   _aaxAudioSessionEvents_OnChannelVolumeChanged,
   _aaxAudioSessionEvents_OnGroupingParamChanged,
   _aaxAudioSessionEvents_OnStateChanged,
   _aaxAudioSessionEvents_OnSessionDisconnected,
};

