/*
 * Copyright 2012-2014 by Erik Hofman.
 * Copyright 2012-2014 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_FILETYPE_H
#define _AAX_FILETYPE_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <base/types.h>

enum _aaxFileParam
{
   __F_PROCESS = -2,
   __F_EOF = -1,
   __F_FMT = 0,
   __F_TRACKS,
   __F_FREQ,
   __F_BITS,
   __F_BLOCK
};

typedef int (_detect_fn)(int);
typedef void* (_new_handle_fn)(int, size_t*, int, int, int, size_t, int);
typedef void* (_open_fn)(void*, void*, size_t*);
typedef int (_close_fn)(void*);
typedef void* (_update_fn)(void*, size_t*, size_t*, char);

typedef void (_cvt_fn)(void*, void_ptr, size_t);
typedef size_t (_cvt_from_fn)(void*, int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
typedef size_t (_cvt_to_fn)(void*, void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t, void*, size_t);

typedef char* (_default_fname_fn)(int);
typedef int (_extension_fn)(char*);
typedef unsigned int (_get_param_fn)(void *, int);

typedef struct
{
   void *id;
   _detect_fn *detect;
   _new_handle_fn *setup;

   _open_fn *open;
   _close_fn *close;
   _update_fn *update;

   _cvt_fn *cvt_to_signed;
   _cvt_fn *cvt_from_signed;
   _cvt_fn *cvt_endianness;
   _cvt_to_fn *cvt_to_intl;
   _cvt_from_fn *cvt_from_intl;

   _extension_fn *supported;
   _default_fname_fn *interfaces;

   _get_param_fn *get_param;

} _aaxFmtHandle;


typedef _aaxFmtHandle* (_aaxExtensionDetect)(void);

extern _aaxExtensionDetect* _aaxFileTypes[];

_aaxExtensionDetect _aaxDetectWavFile;
_aaxExtensionDetect _aaxDetectMP3File;
#if 0
_aaxExtensionDetect _aaxDetectAiffFile;
_aaxExtensionDetect _aaxDetectFLACFile;
_aaxExtensionDetect _aaxDetectVorbisFile;
#endif


#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_FILETYPE_H */

