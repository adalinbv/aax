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

#ifndef _AAX_EXTENSION_H
#define _AAX_EXTENSION_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <base/types.h>

#include "audio.h"

typedef enum {
   _EXT_WAV = 0,
   _EXT_MP3,
   _EXT_OGG,
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

typedef char* (_ext_default_fname_fn)(int);
typedef int (_ext_extension_fn)(char*);
typedef off_t (_ext_get_param_fn)(struct _ext_st*, int);
typedef off_t (_ext_set_param_fn)(struct _ext_st*, int, off_t);

typedef size_t (_ext_process_fn)(struct _ext_st*, void_ptr, size_t*);
typedef size_t (_ext_copy_fn)(struct _ext_st*, int32_ptr, size_t, size_t*);
typedef size_t (_ext_cvt_from_intl_fn)(struct _ext_st*, int32_ptrptr, size_t, size_t*);
typedef size_t (_ext_cvt_to_intl_fn)(struct _ext_st*, void_ptr, const_int32_ptrptr , size_t, size_t, void_ptr, size_t);


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
   _ext_process_fn *process;
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

char* _wav_interfaces(int);
int _wav_extension(char*);
off_t _wav_get(_ext_t*, int);
off_t _wav_set(_ext_t*, int, off_t);

size_t _wav_copy(_ext_t*, int32_ptr, size_t, size_t*);
size_t _wav_process(_ext_t*, void_ptr, size_t*);
size_t _wav_cvt_from_intl(_ext_t*, int32_ptrptr, size_t, size_t*);
size_t _wav_cvt_to_intl(_ext_t*, void_ptr, const_int32_ptrptr, size_t, size_t, void_ptr, size_t);

/* MP3 */
int _mp3_detect(_ext_t*, int);
int _mp3_setup(_ext_t*, int, size_t*, int, int, int, size_t, int);
void* _mp3_open(_ext_t*, void*, size_t*, size_t);
int _mp3_close(_ext_t*);
void* _mp3_update(_ext_t*, size_t*, size_t*, char);
char* _mp3_name(_ext_t*, enum _aaxStreamParam);

char* _mp3_interfaces(int);
int _mp3_extension(char*);
off_t _mp3_get(_ext_t*, int);
off_t _mp3_set(_ext_t*, int, off_t);

size_t _mp3_copy(_ext_t*, int32_ptr, size_t, size_t*);
size_t _mp3_process(_ext_t*, void_ptr, size_t*);
size_t _mp3_cvt_from_intl(_ext_t*, int32_ptrptr, size_t, size_t*);
size_t _mp3_cvt_to_intl(_ext_t*, void_ptr, const_int32_ptrptr, size_t, size_t, void_ptr, size_t);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_EXTENSION_H */

