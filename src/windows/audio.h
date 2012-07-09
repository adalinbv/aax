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


#ifdef _MSC_VER
# define HAVE_GUIDDEF_H 1
# if defined(HAVE_GUIDDEF_H) || defined(HAVE_INITGUID_H)
#  define INITGUID
#  include <windows.h>
#  ifdef HAVE_GUIDDEF_H
#   include <guiddef.h>
#  else
#   include <initguid.h>
#  endif


   DEFINE_GUID(KSDATAFORMAT_SUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
   DEFINE_GUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);


   DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e,0x3d, 0xc4,0x57,0x92,0x91,0x69,0x2e);
   DEFINE_GUID(IID_IMMDeviceEnumerator,  0xa95664d2, 0x9614, 0x4f35, 0xa7,0x46, 0xde,0x8d,0xb6,0x36,0x17,0xe6);
   DEFINE_GUID(IID_IAudioClient,         0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1,0x78, 0xc2,0xf5,0x68,0xa7,0x03,0xb2);
   DEFINE_GUID(IID_IAudioRenderClient,   0xf294acfc, 0x3146, 0x4483, 0xa7,0xbf, 0xad,0xdc,0xa7,0xc2,0x60,0xe2);
   DEFINE_GUID(IID_IAudioCaptureClient,  0xc8adbd64, 0xe71e, 0x48a0, 0xa4,0xde, 0x18,0x5c,0x39,0x5c,0xd3,0x17);

#  undef DEFINE_PROPERTYKEY
#  define DEFINE_PROPERTYKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const PROPERTYKEY DECLSPEC_SELECTANY name = { { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }, pid }
   DEFINE_PROPERTYKEY(PKEY_Device_DeviceDesc, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 2);
   DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);
   DEFINE_PROPERTYKEY(PKEY_DeviceInterface_FriendlyName,  0x026e516e, 0xb814, 0x414b, 0x83, 0xcd, 0x85, 0x6d, 0x6f, 0xef, 0x48, 0x22, 2);

#  include <devpropdef.h>
   DEFINE_DEVPROPKEY(DEVPKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd, 0x80,0x20, 0x67,0xd1,0x46,0xa8,0x50,0xe0, 14);
# endif


# define COBJMACROS 1
# include <mmdeviceapi.h>
# include <audioclient.h>
# include <Objbase.h>

#else
# include "wintypes.h"

typedef void* LPUNKNOWN;

typedef enum tagCLSCTX {
  CLSCTX_INPROC_SERVER		= 0x1,
  CLSCTX_INPROC_HANDLER		= 0x2,
  CLSCTX_LOCAL_SERVER		= 0x4,
  CLSCTX_INPROC_SERVER16	= 0x8,
  CLSCTX_REMOTE_SERVER		= 0x10,
  CLSCTX_INPROC_HANDLER16	= 0x20,
  CLSCTX_NO_CODE_DOWNLOAD	= 0x400,
  CLSCTX_NO_CUSTOM_MARSHAL	= 0x1000,
  CLSCTX_ENABLE_CODE_DOWNLOAD	= 0x2000,
  CLSCTX_NO_FAILURE_LOG		= 0x4000,
  CLSCTX_DISABLE_AAA		= 0x8000,
  CLSCTX_ENABLE_AAA		= 0x10000,
  CLSCTX_FROM_DEFAULT_CONTEXT	= 0x20000,
  CLSCTX_ACTIVATE_32_BIT_SERVER	= 0x40000,
  CLSCTX_ACTIVATE_64_BIT_SERVER	= 0x80000,
  CLSCTX_ENABLE_CLOAKING	= 0x100000,
  CLSCTX_PS_DLL			= 0x80000000 
} CLSCTX;

typedef enum  {
  eRender,
  eCapture,
  eAll,
  EDataFlow_enum_count 
} EDataFlow;

typedef enum  {
  eConsole,
  eMultimedia,
  eCommunications,
  ERole_enum_count 
} ERole;

typedef enum _AUDCLNT_SHAREMODE {
  AUDCLNT_SHAREMODE_SHARED,
  AUDCLNT_SHAREMODE_EXCLUSIVE 
} AUDCLNT_SHAREMODE;

enum _AUDCLNT_BUFFERFLAGS {
  AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY = 0x1,
  AUDCLNT_BUFFERFLAGS_SILENT = 0x2,
  AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR = 0x4
} AUDCLNT_BUFFERFLAGS;


typedef VOID  IMMDevice;
typedef VOID  IAudioClient;
typedef VOID  IAudioRenderClient;
typedef VOID  IAudioCaptureClient;
typedef VOID  IMMDeviceEnumerator;
typedef VOID  IMMDeviceCollection;
typedef VOID  IPropertyStore;
#endif

typedef HRESULT (*CoCreateInstance_proc)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
typedef HRESULT (*CoTaskMemFree_proc)(LPVOID);

typedef HRESULT (*IMMDeviceEnumerator_Release_proc)(void*);
typedef HRESULT (*IMMDeviceEnumerator_GetDefaultEndPoint_proc)(EDataFlow, ERole, IMMDevice**);
typedef HRESULT (*IMMDeviceEnumerator_GetDevice_proc)(void*, LPCWSTR, IMMDevice**);
typedef HRESULT (*IMMDeviceEnumerator_EnumAudioEndpoints_proc)(void*, EDataFlow, DWORD, void**);

typedef HRESULT (*IMMDeviceCollection_GetCount_proc)(void*, UINT*);
typedef HRESULT (*IMMDeviceCollection_Release_proc)(void*);
typedef HRESULT (*IMMDeviceCollection_Item_proc)(void*, UINT, void**);

typedef HRESULT (*IMMDevice_OpenPropertyStore_proc)(void*, DWORD, void**);
typedef HRESULT (*IMMDevice_Activate_proc)(void*, REFIID, DWORD, PROPVARIANT*, void**);
typedef HRESULT (*IMMDevice_Release_proc)(void*);
typedef HRESULT (*IMMDevice_GetId_proc)(void*, LPWSTR*);

typedef HRESULT (*IAudioClient_Start_proc)(void*);
typedef HRESULT (*IAudioClient_Stop_proc)(void*);
typedef HRESULT (*IAudioClient_GetService_proc)(void*, REFIID, void**);
typedef HRESULT (*IAudioClient_Initialize_proc)(void*, AUDCLNT_SHAREMODE, DWORD, REFERENCE_TIME, REFERENCE_TIME, const WAVEFORMATEX*, LPCGUID);
typedef HRESULT (*IAudioClient_GetMixFormat_proc)(void*, WAVEFORMATEX**);
typedef HRESULT (*IAudioClient_GetBufferSize_proc)(void*, UINT32*);
typedef HRESULT (*IAudioClient_GetCurrentPadding_proc)(void*, UINT32*);
typedef HRESULT (*IAudioClient_Release_proc)(void*);

typedef HRESULT (*ICaptureClient_GetBuffer_proc)(void*, BYTE**, UINT32*, DWORD*, UINT64*, UINT64*);
typedef HRESULT (*ICaptureClient_ReleaseBuffer_proc)(void*, UINT32);
typedef HRESULT (*ICaptureClient_GetNextPacketSize_proc)(void*, UINT32*);

typedef HRESULT (*IAudioRenderClient_GetBuffer_proc)(void*, UINT32, BYTE**);
typedef HRESULT (*IAudioRenderClient_ReleaseBuffer_proc)(void*, UINT32, DWORD);
typedef HRESULT (*IAudioRenderClient_Release_proc)(void*);

typedef HRESULT (*PropVariantInit_proc)(void*);
typedef HRESULT (*PropVariantClear_proc)(void*);

typedef HRESULT (*IPropertyStore_GetValue_proc)(void*, REFPROPERTYKEY, void*);
typedef HRESULT (*IPropertyStore_Release_proc)(void*);


typedef int (*WideCharToMultiByte_proc)(UINT, DWORD, LPCWSTR, int, LPSTR, int, LPCSTR, LPBOOL);
typedef int (*MultiByteToWideChar_proc)(UINT, DWORD, LPCSTR, int, LPCWSTR, int);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* __MMDEVAPI_AUDIO_H */

