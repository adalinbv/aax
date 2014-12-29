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
   __F_PROCESS = -2,		/* get */
   __F_EOF = -1,
   __F_FMT = 0,
   __F_TRACKS,
   __F_FREQ,
   __F_BITS,
   __F_BLOCK,
   __F_SAMPLES,

   __F_ARTIST = 0x0800,		/* get name strings */
   __F_TITLE,

   __F_POSITION = 0x1000	/* set */
};

typedef int (_file_detect_fn)(int);
typedef void* (_file_new_handle_fn)(int, size_t*, int, int, int, size_t, int);
typedef void* (_file_open_fn)(void*, void*, size_t*, size_t);
typedef int (_file_close_fn)(void*);
typedef void* (_file_update_fn)(void*, size_t*, size_t*, char);
typedef char* (_file_get_name_fn)(void*, enum _aaxFileParam);

typedef void (_file_cvt_fn)(void*, void_ptr, size_t);
typedef size_t (_file_cvt_from_fn)(void*, int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
typedef size_t (_file_cvt_to_fn)(void*, void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t, void*, size_t);

typedef char* (_file_default_fname_fn)(int);
typedef int (_file_extension_fn)(char*);
typedef off_t (_file_get_param_fn)(void *, int);
typedef off_t (_file_set_param_fn)(void *, int, off_t);

typedef struct
{
   void *id;
   _file_detect_fn *detect;
   _file_new_handle_fn *setup;

   _file_open_fn *open;
   _file_close_fn *close;
   _file_update_fn *update;
   _file_get_name_fn *name;

   _file_cvt_fn *cvt_to_signed;
   _file_cvt_fn *cvt_from_signed;
   _file_cvt_fn *cvt_endianness;
   _file_cvt_to_fn *cvt_to_intl;
   _file_cvt_from_fn *cvt_from_intl;

   _file_extension_fn *supported;
   _file_default_fname_fn *interfaces;

   _file_get_param_fn *get_param;
   _file_set_param_fn *set_param;

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

