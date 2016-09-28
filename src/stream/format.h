/*
 * Copyright 2012-2016 by Erik Hofman.
 * Copyright 2012-2016 by Adalin B.V.
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

#include <aax/aax.h>

#include <base/types.h>

#include "audio.h"

typedef enum
{
   _FMT_PCM = 0,
   _FMT_MP3,
   _FMT_VORBIS,
   _FMT_FLAC,
   _FMT_SPEEX,

   _FMT_MAX

} _fmt_type_t;

struct _fmt_st;

typedef int (_fmt_setup_fn)(struct _fmt_st*, _fmt_type_t, enum aaxFormat);
typedef void* (_fmt_open_fn)(struct _fmt_st*, void*, size_t*, size_t);
typedef void (_fmt_close_fn)(struct _fmt_st*);
typedef char* (_fmt_name_fn)(struct _fmt_st*, enum _aaxStreamParam);
typedef void (_fmt_cvt_fn)(struct _fmt_st*, void_ptr, size_t);
typedef size_t (_fmt_cvt_from_fn)(struct _fmt_st*, int32_ptrptr, size_t, size_t*);
typedef size_t (_fmt_cvt_to_fn)(struct _fmt_st*, void_ptr, const_int32_ptrptr, size_t, size_t*, void_ptr, size_t);
typedef size_t (_fmt_fill_fn)(struct _fmt_st*, void_ptr, size_t*);
typedef size_t (_fmt_copy_fn)(struct _fmt_st*, int32_ptr, size_t, size_t*);
typedef off_t (_fmt_set_fn)(struct _fmt_st*, int, off_t);
typedef off_t (_fmt_get_fn)(struct _fmt_st*, int);

struct _fmt_st
{
   void *id;
   _fmt_setup_fn *setup;
   _fmt_open_fn *open;
   _fmt_close_fn *close;
   _fmt_name_fn *name;

   _fmt_cvt_fn *cvt_to_signed;
   _fmt_cvt_fn *cvt_from_signed;
   _fmt_cvt_fn *cvt_endianness;
   _fmt_cvt_to_fn *cvt_to_intl;			// convert to file format
   _fmt_cvt_from_fn *cvt_from_intl;		// convert to mixer format
   _fmt_fill_fn *fill;
   _fmt_copy_fn *copy;				// copy raw sound data

   _fmt_set_fn *set;
   _fmt_get_fn *get;
};
typedef struct _fmt_st _fmt_t;

_fmt_t* _fmt_create(_fmt_type_t, int);
void* _fmt_free(_fmt_t*);

/* PCM */
int _pcm_detect(_fmt_t*, int);
int _pcm_setup(_fmt_t*, _fmt_type_t, enum aaxFormat);
void* _pcm_open(_fmt_t*, void*, size_t*, size_t);
void _pcm_close(_fmt_t*);
void _pcm_cvt_to_signed(_fmt_t*, void_ptr, size_t);
void _pcm_cvt_from_signed(_fmt_t*, void_ptr, size_t);
void _pcm_cvt_endianness(_fmt_t*, void_ptr, size_t);
size_t _pcm_cvt_to_intl(_fmt_t*, void_ptr, const_int32_ptrptr, size_t, size_t*, void_ptr, size_t);
size_t _pcm_cvt_from_intl(_fmt_t*, int32_ptrptr, size_t, size_t*);
size_t _pcm_fill(_fmt_t*, void_ptr, size_t*);
size_t _pcm_copy(_fmt_t*, int32_ptr, size_t, size_t*);
char* _pcm_name(_fmt_t*, enum _aaxStreamParam);
off_t _pcm_set(_fmt_t*, int, off_t);
off_t _pcm_get(_fmt_t*, int);

/* MP3 - mpg123 & lame */
int _mpg123_detect(_fmt_t*, int);
int _mpg123_setup(_fmt_t*, _fmt_type_t, enum aaxFormat);
void* _mpg123_open(_fmt_t*, void*, size_t*, size_t);
void _mpg123_close(_fmt_t*);
size_t _mpg123_cvt_to_intl(_fmt_t*, void_ptr, const_int32_ptrptr, size_t, size_t*, void_ptr, size_t);
size_t _mpg123_cvt_from_intl(_fmt_t*, int32_ptrptr, size_t, size_t*);
size_t _mpg123_fill(_fmt_t*, void_ptr, size_t*);
size_t _mpg123_copy(_fmt_t*, int32_ptr, size_t, size_t*);
char* _mpg123_name(_fmt_t*, enum _aaxStreamParam);
off_t _mpg123_set(_fmt_t*, int, off_t);
off_t _mpg123_get(_fmt_t*, int);

// void* _mpg123_update(_fmt_t*, size_t*, size_t*, char);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_FORMAT_H */

