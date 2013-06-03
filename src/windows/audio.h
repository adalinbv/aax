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

#ifndef __MMDEVAPI_AUDIO_H
#define __MMDEVAPI_AUDIO_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

enum _wasapi_log
{
   WASAPI_VARLOG = -1,
   WASAPI_NO_ERROR = 0,

   WASAPI_UNSUPPORTED_NO_PERIODS,
   WASAPI_UNSUPPORTED_BUFFER_SIZE,
   WASAPI_UNSUPPORTED_FREQUENCY,
   WASAPI_UNSUPPORTED_NO_TRACKS,
   WASAPI_UNSUPPORTED_FORMAT,
   WASAPI_UNSUPPORTED_BPS,

   WASAPI_EXCLUSIVE_MODE_FAILED,
   WASAPI_EVENT_MODE_FAILED,
   WASAPI_EVENT_CREATION_FAILED,
   WASAPI_EVENT_SETUP_FAILED,
   WASAPI_TIMER_CREATION_FAILED,
   WASAPI_EVENT_TIMEOUT,
   WASAPI_WAIT_EVENT_FAILED,
   WASAPI_EVENT_NOTIFICATION_FAILED,

   WASAPI_CONNECTION_FAILED,
   WASAPI_INITIALIZATION_FAILED,
   WASAPI_STOP_FAILED,
   WASAPI_RESUME_FAILED,

   WASAPI_SET_BUFFER_SIZE_FAILED,
   WASAPI_GET_AUDIO_FORMAT_FAILED,
   WASAPI_GET_DEVICE_FORMAT_FAILED,
   WASAPI_GET_MIXER_FORMAT_FAILED,
   WASAPI_GET_PERIOD_SIZE_FAILED,
   WASAPI_GET_BUFFER_FAILED,
   WASAPI_RELEASE_BUFFER_FAILED,

   WASAPI_CREATE_CAPTURE_THREAD_FAILED,
   WASAPI_GET_AUDIO_SERVICE_FAILED,
   WASAPI_DATA_DISCONTINUITY,
   WASAPI_BUFFER_UNDERRUN,
   WASAPI_BUFFER_OVERRUN,

   WASAPI_UNCAUGHT_ERROR,
   WASAPI_MAX_ERROR

};

static char *_wasapi_errors[WASAPI_MAX_ERROR] =
{
   "No Error",
   "Unsupported no. periods",
   "Unsupported buffer size",
   "Invalid sample frequency",
   "Unsupported no. tracks",
   "Unsupported audio format",
   "Unsupported bits per sample",

   "Exclusive mode failed",
   "Event mode failed",
   "Event creation failed",
   "Event Setup failed",
   "Timer creation field",
   "Event timeout",
   "Wait event failed",
   "Unable to setup event notification handler",

   "Connection failed",
   "Initialization failed",
   "Failed to stop",
   "Failed to resume",

   "Unable to set the buffer size",
   "Unable to get the audio format",
   "Unable to get the device format",
   "Unable to get the mixer format",
   "Unable to get the period size",
   "Unable to get the audio buffer",
   "Unable to release the audio buffer",

   "Capture thread creation failed",
   "Failed to get the audio service",
   "Data discontinuity",
   "Buffer underrun",
   "Buffer overrun",

   "uncaught error"
};

# define WIN32_LEAN_AND_MEAN
# include <windows.h>

# if (defined(_MSC_VER) && (_MSC_VER >= 1400)) || defined(__MINGW32__)
#  define COBJMACROS
# endif


# ifndef NTDDI_VERSION
#  undef WINVER
#  undef _WIN32_WINNT
#  define WINVER       0x0600 // VISTA
#  define _WIN32_WINNT WINVER

#  include <basetyps.h> // << for IID/CLSID
#  include <rpcsal.h>
#  include <sal.h>

#  ifndef PROPERTYKEY_DEFINED
#   define PROPERTYKEY_DEFINED
typedef struct _tagpropertykey
{
    GUID fmtid;
    DWORD pid;
} PROPERTYKEY;
#  endif

#  ifdef __midl_proxy
#   define __MIDL_CONST
#  else
#   define __MIDL_CONST const
#  endif

#  ifdef WIN64
#   include <wtypes.h>
typedef LONG NTSTATUS;
#   define FASTCALL
#   include <oleidl.h>
#   include <objidl.h>
#  else
typedef struct _BYTE_BLOB
{
    unsigned long clSize;
    unsigned char abData[ 1 ];
} BYTE_BLOB;
typedef LONGLONG REFERENCE_TIME;
#  endif

#  ifndef WAVE_FORMAT_IEEE_FLOAT
#   define WAVE_FORMAT_IEEE_FLOAT 0x0003 // 32-bit floating-point
#  endif

#  ifndef __MINGW_EXTENSION
#   if defined(__GNUC__) || defined(__GNUG__)
#    define __MINGW_EXTENSION __extension__
#   else
#    define __MINGW_EXTENSION
#   endif
#  endif

#  include <sdkddkver.h>
#  include <propkeydef.h>
#  define COBJMACROS
#  define INITGUID
#  define CONST_VTABLE
#  include <audioclient.h>
#  include <audiopolicy.h>
#  include <mmdeviceapi.h>
#  include <Endpointvolume.h>
#  include <functiondiscoverykeys.h>
#  include <unknwn.h>
#  undef INITGUID

# ifndef InterlockedAnd
#  define InterlockedAnd InterlockedAnd_Inline
LONG InterlockedAnd_Inline(LONG volatile *, LONG);
# endif
# ifndef InterlockedOr
#  define InterlockedOr InterlockedOr_Inline
LONG InterlockedOr_Inline(LONG volatile *, LONG);
# endif

# endif // NTDDI_VERSION

# ifndef GUID_SECT
#  define GUID_SECT
# endif


#define AAX_DEFINE_CLSID(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) static const CLSID n GUID_SECT = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

 AAX_DEFINE_CLSID(aax_CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e,0x3d, 0xc4,0x57,0x92,0x91,0x69,0x2e);

#define AAX_DEFINE_IID(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) static const IID n GUID_SECT = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

 AAX_DEFINE_IID(aax_IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
 AAX_DEFINE_IID(aax_IID_IAudioEndpointVolume, 0x5CDF2C82, 0x841E, 0x4546, 0x97,0x22,0x0c,0xf7,0x40,0x78,0x22,0x9a);
 AAX_DEFINE_IID(aax_IID_IMMDeviceEnumerator, 0xa95664d2, 0x9614, 0x4f35, 0xa7,0x46, 0xde,0x8d,0xb6,0x36,0x17,0xe6);
 AAX_DEFINE_IID(aax_IID_IAudioClient, 0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1,0x78, 0xc2,0xf5,0x68,0xa7,0x03,0xb2);
 AAX_DEFINE_IID(aax_IID_IAudioRenderClient, 0xf294acfc, 0x3146, 0x4483, 0xa7,0xbf, 0xad,0xdc,0xa7,0xc2,0x60,0xe2);
 AAX_DEFINE_IID(aax_IID_IAudioCaptureClient, 0xc8adbd64, 0xe71e, 0x48a0, 0xa4,0xde, 0x18,0x5c,0x39,0x5c,0xd3,0x17);
 AAX_DEFINE_IID(aax_IID_IAudioSessionEvents, 0x24918acc, 0x64b3, 0x37c1, 0x8c,0xa9, 0x74,0xa6,0x6e,0x99,0x57,0xa8);
 AAX_DEFINE_IID(aax_IID_IAudioSessionControl, 0xf4b1a599, 0x7266, 0x4319, 0xa8,0xca, 0xe7,0x0a,0xcb,0x11,0xe8,0xcd);



#define AAX_DEFINE_GUID(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) static const GUID n GUID_SECT = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

 AAX_DEFINE_GUID(aax_KSDATAFORMAT_SUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
 AAX_DEFINE_GUID(aax_KSDATAFORMAT_SUBTYPE_ADPCM, 0x00000002, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
 AAX_DEFINE_GUID(aax_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);


#endif /*__MMDEVAPI_AUDIO_H */

