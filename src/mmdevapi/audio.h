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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "wintypes.h"

#ifndef LPUNKNOWN
typedef void* LPUNKNOWN;
#endif

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


typedef VOID  IMMDevice;
typedef VOID  IAudioClient;
typedef VOID  IAudioRenderClient;
typedef VOID  IMMDeviceEnumerator;


EXTERN_C CLSID CLSID_MMDeviceEnumerator;
EXTERN_C IID IID_IMMDeviceEnumerator;
EXTERN_C IID IID_IAudioClient;
EXTERN_C IID IID_IAudioRenderClient;

typedef HRESULT (*CoCreateInstance_proc)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
typedef HRESULT (*IMMDeviceEnumerator_Release_proc)(IMMDeviceEnumerator*);
typedef HRESULT (*IMMDeviceEnumerator_GetDefaultEndPoint_proc)(EDataFlow, ERole, IMMDevice**);
typedef HRESULT (*IMMDeviceActivate_proc)(REFIID, DWORD, PROPVARIANT*, void**);

typedef HRESULT (*IAudioClient_Start_proc)(void);
typedef HRESULT (*IAudioClient_Stop_proc)(void);
typedef HRESULT (*IAudioClient_GetService_proc)(REFIID, void**);
typedef HRESULT (*IAudioClient_Initialize_proc)(AUDCLNT_SHAREMODE, DWORD, REFERENCE_TIME, REFERENCE_TIME, const WAVEFORMATEX, LPCGUID);
typedef HRESULT (*IAudioClient_GetMixFormat_proc)(WAVEFORMATEX**);
typedef HRESULT (*IAudioClient_GetBufefrSize_proc)(UINT32*);
typedef HRESULT (*IAudioClient_GetCurrentPadding_proc)(UINT32*);

typedef HRESULT (*IAudioRenderClient_GetBuffer_proc)(UINT32, BYTE**);
typedef HRESULT (*IAudioRenderClient_ReleaseBuffer_proc)(UINT32, DWORD);

#endif /* __MMDEVAPI_AUDIO_H */

