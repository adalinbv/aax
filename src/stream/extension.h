/*
 * Copyright 2012-2017 by Erik Hofman.
 * Copyright 2012-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _AAX_EXTENSION_H
#define _AAX_EXTENSION_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <base/types.h>
#include <devices.h>

#include "audio.h"

#define BSWAP(a)			is_bigendian() ? _aax_bswap32(a) : (a)
#define BSWAPH(a)			is_bigendian() ? _aax_bswap32h(a) : (a)

typedef enum {
   _EXT_NONE = 0,
   _EXT_WAV,
   _EXT_OGG,
   _EXT_OPUS,
// _EXT_SPEEX,

   /* raw format extensions */
   _EXT_PCM,
   _EXT_MP3,
   _EXT_FLAC,

   _EXT_MAX

} _ext_type_t;

struct _ext_st;

typedef int (_ext_detect_fn)(struct _ext_st*, int);
typedef int (_ext_new_handle_fn)(struct _ext_st*, int, size_t*, int, int, int, size_t, int);
typedef void* (_ext_open_fn)(struct _ext_st*, void_ptr, size_t*, size_t);
typedef int (_ext_close_fn)(struct _ext_st*);
typedef void* (_ext_update_fn)(struct _ext_st*, size_t*, size_t*, char);
typedef char* (_ext_get_name_fn)(struct _ext_st*, enum _aaxStreamParam);

typedef char* (_ext_default_fname_fn)(int, int);
typedef int (_ext_extension_fn)(char*);
typedef off_t (_ext_get_param_fn)(struct _ext_st*, int);
typedef off_t (_ext_set_param_fn)(struct _ext_st*, int, off_t);

typedef size_t (_ext_fill_fn)(struct _ext_st*, void_ptr, size_t*);
typedef size_t (_ext_copy_fn)(struct _ext_st*, int32_ptr, size_t, size_t*);
typedef size_t (_ext_cvt_from_intl_fn)(struct _ext_st*, int32_ptrptr, size_t, size_t*);
typedef size_t (_ext_cvt_to_intl_fn)(struct _ext_st*, void_ptr, const_int32_ptrptr , size_t, size_t*, void_ptr, size_t);


struct _ext_st
{
   void *id;
   _ext_detect_fn *detect;
   _ext_new_handle_fn *setup;

   _ext_open_fn *open;
   _ext_close_fn *close;
   _ext_update_fn *update;
   _ext_get_name_fn *name;

   _ext_extension_fn *supported;
   _ext_default_fname_fn *interfaces;

   _ext_get_param_fn *get_param;
   _ext_set_param_fn *set_param;

   _ext_copy_fn *copy;
   _ext_fill_fn *fill;
   _ext_cvt_from_intl_fn *cvt_from_intl;
   _ext_cvt_to_intl_fn *cvt_to_intl;
};
typedef struct _ext_st _ext_t;

_ext_t* _ext_create(_ext_type_t);
void* _ext_free(_ext_t*);

/* WAV */
int _wav_detect(_ext_t*, int);
int _wav_setup(_ext_t*, int, size_t*, int, int, int, size_t, int);
void* _wav_open(_ext_t*, void*, size_t*, size_t);
int _wav_close(_ext_t*);
void* _wav_update(_ext_t*, size_t*, size_t*, char);
char* _wav_name(_ext_t*, enum _aaxStreamParam);

char* _wav_interfaces(int, int);
int _wav_extension(char*);
off_t _wav_get(_ext_t*, int);
off_t _wav_set(_ext_t*, int, off_t);

size_t _wav_copy(_ext_t*, int32_ptr, size_t, size_t*);
size_t _wav_fill(_ext_t*, void_ptr, size_t*);
size_t _wav_cvt_from_intl(_ext_t*, int32_ptrptr, size_t, size_t*);
size_t _wav_cvt_to_intl(_ext_t*, void_ptr, const_int32_ptrptr, size_t, size_t*, void_ptr, size_t);

/* OGG, OPUS */
int _ogg_detect(_ext_t*, int);
int _ogg_setup(_ext_t*, int, size_t*, int, int, int, size_t, int);
void* _ogg_open(_ext_t*, void*, size_t*, size_t);
int _ogg_close(_ext_t*);
void* _ogg_update(_ext_t*, size_t*, size_t*, char);
char* _ogg_name(_ext_t*, enum _aaxStreamParam);

char* _ogg_interfaces(int, int);
int _ogg_extension(char*);
off_t _ogg_get(_ext_t*, int);
off_t _ogg_set(_ext_t*, int, off_t);

size_t _ogg_copy(_ext_t*, int32_ptr, size_t, size_t*);
size_t _ogg_fill(_ext_t*, void_ptr, size_t*);
size_t _ogg_cvt_from_intl(_ext_t*, int32_ptrptr, size_t, size_t*);
size_t _ogg_cvt_to_intl(_ext_t*, void_ptr, const_int32_ptrptr, size_t, size_t*, void_ptr, size_t);

/* RAW, MP3, FLAC */
int _raw_detect(_ext_t*, int);
int _raw_setup(_ext_t*, int, size_t*, int, int, int, size_t, int);
void* _raw_open(_ext_t*, void*, size_t*, size_t);
int _raw_close(_ext_t*);
void* _raw_update(_ext_t*, size_t*, size_t*, char);
char* _raw_name(_ext_t*, enum _aaxStreamParam);

char* _raw_interfaces(int, int);
int _raw_extension(char*);
off_t _raw_get(_ext_t*, int);
off_t _raw_set(_ext_t*, int, off_t);

size_t _raw_copy(_ext_t*, int32_ptr, size_t, size_t*);
size_t _raw_fill(_ext_t*, void_ptr, size_t*);
size_t _raw_cvt_from_intl(_ext_t*, int32_ptrptr, size_t, size_t*);
size_t _raw_cvt_to_intl(_ext_t*, void_ptr, const_int32_ptrptr, size_t, size_t*, void_ptr, size_t);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_EXTENSION_H */

