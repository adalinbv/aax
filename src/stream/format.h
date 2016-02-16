/*
 * Copyright 2012-2015 by Erik Hofman.
 * Copyright 2012-2015 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_FORMAT_H
#define _AAX_FORMAT_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <base/types.h>

/*
 * Calling sequence:
 * -----------------
 * 1.    _aaxWavDetect
 * 2.    _aaxWavSetup
 * 3.    _aaxWavOpen	// reads the file header
 *
 * (n-1) _aaxWavUpdate
 * (n)   _aaxWavClose
 */

enum _aaxFormatParam
{
   __F_PROCESS = -2,		/* get */
   __F_EOF = -1,
   __F_FMT = 0,
   __F_TRACKS,
   __F_FREQ,
   __F_BITS,
   __F_BLOCK,
   __F_SAMPLES,
   __F_NO_BYTES,

   __F_RATE,
   __F_PORT,
   __F_TIMEOUT,
   __F_FLAGS,
   __F_MODE,

   __F_NAME_CHANGED = 0x0400, /* set if the name changed since the last get */
   __F_IMAGE = 0x0800,		/* get info name strings */
   __F_ARTIST,
   __F_GENRE,
   __F_TITLE,
   __F_TRACKNO,
   __F_ALBUM,
   __F_DATE,
   __F_COPYRIGHT,
   __F_COMMENT, 
   __F_COMPOSER,
   __F_ORIGINAL,
   __F_WEBSITE,

   __F_IS_STREAM = 0x1000,	/* set */
   __F_POSITION
};

typedef int (_fmt_detect_fn)(void*, int);
typedef void* (_fmt_new_handle_fn)(int, size_t*, int, int, int, size_t, int);
typedef void* (_fmt_open_fn)(void*, void*, size_t*, size_t);
typedef int (_fmt_close_fn)(void*);
typedef void* (_fmt_update_fn)(void*, size_t*, size_t*, char);
typedef char* (_fmt_get_name_fn)(void*, enum _aaxFormatParam);

typedef void (_fmt_cvt_fn)(void*, void_ptr, size_t);
typedef size_t (_fmt_cvt_from_fn)(void*, int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
typedef size_t (_fmt_cvt_to_fn)(void*, void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t, void*, size_t);

typedef char* (_fmt_default_fname_fn)(int);
typedef int (_fmt_extension_fn)(char*);
typedef off_t (_fmt_get_param_fn)(void *, int);
typedef off_t (_fmt_set_param_fn)(void *, int, off_t);

typedef struct
{
   void *id;
   _fmt_detect_fn *detect;
   _fmt_new_handle_fn *setup;

   _fmt_open_fn *open;
   _fmt_close_fn *close;
   _fmt_update_fn *update;
   _fmt_get_name_fn *name;

   _fmt_cvt_fn *cvt_to_signed;
   _fmt_cvt_fn *cvt_from_signed;
   _fmt_cvt_fn *cvt_endianness;
   _fmt_cvt_to_fn *cvt_to_intl;			// convert to file format
   _fmt_cvt_from_fn *cvt_from_intl;		// convert to mixer format
   _fmt_cvt_from_fn *copy;			// copy raw sound data

   _fmt_extension_fn *supported;
   _fmt_default_fname_fn *interfaces;

   _fmt_get_param_fn *get_param;
   _fmt_set_param_fn *set_param;

} _aaxFmtHandle;


typedef _aaxFmtHandle* (_aaxExtensionDetect)(void);

extern _aaxExtensionDetect* _aaxFormatTypes[];

_aaxExtensionDetect _aaxDetectWavFormat;
_aaxExtensionDetect _aaxDetectMP3Format;
_aaxExtensionDetect _aaxDetectFLACFormat;
#if 0
_aaxExtensionDetect _aaxDetectAiffFormat;
_aaxExtensionDetect _aaxDetectVorbisFormat;
#endif


/* WAV format */
enum wavFormat
{
   UNSUPPORTED = 0,
   PCM_WAVE_FILE = 1,
   MSADPCM_WAVE_FILE = 2,
   FLOAT_WAVE_FILE = 3,
   ALAW_WAVE_FILE = 6,
   MULAW_WAVE_FILE = 7,
   IMA4_ADPCM_WAVE_FILE = 17,
   MP3_WAVE_FILE = 85,

   EXTENSIBLE_WAVE_FORMAT = 0xFFFE
};
enum aaxFormat getFormatFromWAVFormat(unsigned int, int);


#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_FORMAT_H */

