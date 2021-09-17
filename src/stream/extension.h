/*
 * Copyright 2012-2020 by Erik Hofman.
 * Copyright 2012-2020 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
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
#include <base/memory.h>

#include "audio.h"

#define BSWAP(a)			is_bigendian() ? _aax_bswap32(a) : (a)
#define BSWAPH(a)			is_bigendian() ? _aax_bswap32h(a) : (a)

typedef enum {
   _EXT_NONE = 0,
   _EXT_WAV,
   _EXT_OGG,
   _EXT_OPUS,
   _EXT_SPEEX,
   _EXT_PATCH,

   /* raw format extensions */
   _EXT_PCM,
   _EXT_MP3,
   _EXT_FLAC,

   _EXT_MAX

} _ext_type_t;

struct _ext_st;

// Detect whether a particular extension type is supported.
//
// handle must be created using _ext_create(type);
// mode == 0 (AAX_MODE_READ) means reading, writing otherwise.
//
// The function may probe for special conditions required to support the
// requested extension in the requested mode. For example  whether a
// particular library is installed.
//
// Returns AAX_TRUE if the extension-type is supported, AAX_FALSE otherwise.
typedef int (_ext_detect_fn)(struct _ext_st *handle, int mode);

// Set up the extension in the requested format.
//
// handle must have been created using _ext_create(type);
// mode == 0 (AAX_MODE_READ) means reading, writing otherwise.
// *bufsize: returns the required buffer-size to hold the header data
//           for this extension
// rate: the requested sample-rate
// channels: the requested number of audio channels
// format: the requested audio format (enum aaxFormat)
// no_samples: the number of samples of one rendering period
// bitrate: the requested bit-rate of the audio stream
// 
// Returns AAX_TRUE on success, AAX_FALSE otherwise.
typedef int (_ext_new_handle_fn)(struct _ext_st *handle, int mode, size_t *bufsize, int rate, int channels, int format, size_t no_samples, int bitrate);

// Open the extension for business. This is where the stream is accessed
// for the first time.
//
// handle must have been created using _ext_create(type);
// buf: the buffer holding the stream-data to be processed
// *bufsize: size of the buffer of the stream-data to be processed
//           returns the actual number of bytes which where processed
// fsize: the file-size in bytes as reported by the operating system
//
// Returns a pointer to the buffer holding any data to be written to the steam.
// Returns a NULL pointer and sets *bufsize > 0 when the function was succesful
// and no data needs to be written.
//  In case of end-of-file a non-NULL pointer is returned but *bufsize is 0
//  In case of a failure NULL is returned and *bufsize is set to 0
typedef void* (_ext_open_fn)(struct _ext_st *handle, void_ptr buf, ssize_t *bufsize, size_t fsize);

// Close the stream and clear all it's resources.
//
// handle must have been created using _ext_create(type);
//
// Returns AAX_TRUE
typedef int (_ext_close_fn)(struct _ext_st *handle);

// Updates the extension (and format) internals before converting it to PCM
// Usually this means decoding to an internal buffer
//
// handle must have been created using _ext_create(type);
// *offset: returns the number of bytes which are unprocessed, or 0
// *size: returns the header size in bytes, or 0
// close: AAX_TRUE of this is the final call when closing the stream,
//        AAX_FLASE otherwise
//
// Returns a non-NULL pointer to a buffer containg the header if the header
// needs updating
typedef void* (_ext_update_fn)(struct _ext_st *handle, size_t *offset, ssize_t *size, char close);

// Returns a string from the extension and/or format
//
// handle must have been created using _ext_create(type);
// param: the string type to return
typedef char* (_ext_get_name_fn)(struct _ext_st *handle, enum _aaxStreamParam param);

// Returns the extensions supported file extensions in plain text for 
// the requested extension-type in the requested mode. Extensions are
// separated with a single space-character which is directly usable
// for GUI applications.
typedef char* (_ext_default_fname_fn)(int, int);

// Returns whether a file extension string is supported or not.
//
// Extensions may support multiple file extensions through formats
// The RAW extension for example supports .mp3 and .pcm formats
typedef int (_ext_extension_fn)(char*);

// Returns the value of one single parameter from the extension
//
// handle must have been created using _ext_create(type);
// param: the requested parameter (enum _aaxStreamParam)
typedef off_t (_ext_get_param_fn)(struct _ext_st *handle, int param);

// Set the value of one single parameter from the extension
//
// param: the requested parameter (enum _aaxStreamParam)
// value: the value to set the parameter to
typedef off_t (_ext_set_param_fn)(struct _ext_st *handle, int param, off_t value);

// Fill the extension and/or formats internal buffer with new data
//
// handle must have been created using _ext_create(type);
// buffer: a pointer to the buffer containing new raw data
// *bytes: The number of bytes available in the buffer
//         returns the number of bytes processed by the function
//
// Returns the number of processed bytes
//         __F_NEED_MORE if more data is required before processing can start
//         0 in case of an error
typedef size_t (_ext_fill_fn)(struct _ext_st *handle, void_ptr buffer, ssize_t *bytes);

// Convert data into a memory buffer
//
// handle must have been created using _ext_create(type);
// buffer: buffer where to put the converted data
// offset: offset from the start of the buffer where to put the new data
// *num: requested number of samples to convert
//       outputs the actual number of sampled which where converted
//
// Returns the number of processed bytes
//         __F_NEED_MORE if more data is required before processing can start
//         0 in case of an error
typedef size_t (_ext_copy_fn)(struct _ext_st *handle, int32_ptr buffer, size_t offset, size_t *num);

// Convert data from a possible compressed r interleaved stream into separate
// buffers for every track
//
// handle must have been created using _ext_create(type);
// buffer[]: array of audio channel buffers where to put the converted data
// offset: offset from the start of the buffer where to put the new data
// *num: requested number of samples to convert
//       outputs the actual number of sampled which where converted
//
// Returns the number of processed bytes
//         __F_NEED_MORE if more data is required before processing can start
//         0 in case of an error
typedef size_t (_ext_cvt_from_intl_fn)(struct _ext_st *handle, int32_ptrptr buf, size_t offset, size_t *num);

// Covert interleaved PCM data to an extension native format
//
// handle must have been created using _ext_create(type);
// buffer: buffer where to put the converted data
// channels: array of audio channel buffers where the source data is stored
// offset: offset from the start of the buffer where to get the new data
// *num: requested number of samples to convert
//       outputs the actual number of samples which where converted
// scratch: a pre-allocated scratch buffer for general use
// size: size in bytes of the scratch buffer
//
// Returns the number of processed bytes
//         __F_NEED_MORE if more data is required before processing can start
//         0 in case of an error
typedef size_t (_ext_cvt_to_intl_fn)(struct _ext_st *handle, void_ptr buffer, const_int32_ptrptr channels, size_t offset, size_t *num, void_ptr scratch, size_t size);


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
void* _wav_open(_ext_t*, void*, ssize_t*, size_t);
int _wav_close(_ext_t*);
void* _wav_update(_ext_t*, size_t*, ssize_t*, char);
char* _wav_name(_ext_t*, enum _aaxStreamParam);

char* _wav_interfaces(int, int);
int _wav_extension(char*);
off_t _wav_get(_ext_t*, int);
off_t _wav_set(_ext_t*, int, off_t);

size_t _wav_copy(_ext_t*, int32_ptr, size_t, size_t*);
size_t _wav_fill(_ext_t*, void_ptr, ssize_t*);
size_t _wav_cvt_from_intl(_ext_t*, int32_ptrptr, size_t, size_t*);
size_t _wav_cvt_to_intl(_ext_t*, void_ptr, const_int32_ptrptr, size_t, size_t*, void_ptr, size_t);

/* OGG, OPUS */
int _ogg_detect(_ext_t*, int);
int _ogg_setup(_ext_t*, int, size_t*, int, int, int, size_t, int);
void* _ogg_open(_ext_t*, void*, ssize_t*, size_t);
int _ogg_close(_ext_t*);
void* _ogg_update(_ext_t*, size_t*, ssize_t*, char);
char* _ogg_name(_ext_t*, enum _aaxStreamParam);

char* _ogg_interfaces(int, int);
int _ogg_extension(char*);
off_t _ogg_get(_ext_t*, int);
off_t _ogg_set(_ext_t*, int, off_t);

size_t _ogg_copy(_ext_t*, int32_ptr, size_t, size_t*);
size_t _ogg_fill(_ext_t*, void_ptr, ssize_t*);
size_t _ogg_cvt_from_intl(_ext_t*, int32_ptrptr, size_t, size_t*);
size_t _ogg_cvt_to_intl(_ext_t*, void_ptr, const_int32_ptrptr, size_t, size_t*, void_ptr, size_t);

/* PAT */
int _pat_detect(_ext_t*, int);
int _pat_setup(_ext_t*, int, size_t*, int, int, int, size_t, int);
void* _pat_open(_ext_t*, void*, ssize_t*, size_t);
int _pat_close(_ext_t*);
void* _pat_update(_ext_t*, size_t*, ssize_t*, char);
char* _pat_name(_ext_t*, enum _aaxStreamParam);

char* _pat_interfaces(int, int);
int _pat_extension(char*);
off_t _pat_get(_ext_t*, int);
off_t _pat_set(_ext_t*, int, off_t);

size_t _pat_copy(_ext_t*, int32_ptr, size_t, size_t*);
size_t _pat_fill(_ext_t*, void_ptr, ssize_t*);
size_t _pat_cvt_from_intl(_ext_t*, int32_ptrptr, size_t, size_t*);
size_t _pat_cvt_to_intl(_ext_t*, void_ptr, const_int32_ptrptr, size_t, size_t*, void_ptr, size_t);

/* RAW, MP3, FLAC */
int _raw_detect(_ext_t*, int);
int _raw_setup(_ext_t*, int, size_t*, int, int, int, size_t, int);
void* _raw_open(_ext_t*, void*, ssize_t*, size_t);
int _raw_close(_ext_t*);
void* _raw_update(_ext_t*, size_t*, ssize_t*, char);
char* _raw_name(_ext_t*, enum _aaxStreamParam);

char* _raw_interfaces(int, int);
int _raw_extension(char*);
off_t _raw_get(_ext_t*, int);
off_t _raw_set(_ext_t*, int, off_t);

size_t _raw_copy(_ext_t*, int32_ptr, size_t, size_t*);
size_t _raw_fill(_ext_t*, void_ptr, ssize_t*);
size_t _raw_cvt_from_intl(_ext_t*, int32_ptrptr, size_t, size_t*);
size_t _raw_cvt_to_intl(_ext_t*, void_ptr, const_int32_ptrptr, size_t, size_t*, void_ptr, size_t);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_EXTENSION_H */

