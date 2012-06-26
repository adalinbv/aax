/*
 * Copyright 2012 by Erik Hofman.
 * Copyright 2012 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef __FILE_EXT_AUDIO_H
#define __FILE_EXT_AUDIO_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif


/** libmpg123 */
typedef struct mpg123_handle_struct     mpg123_handle;

enum    mpg123_parms {
  MPG123_VERBOSE = 0, MPG123_FLAGS,
  MPG123_ADD_FLAGS, MPG123_FORCE_RATE,
  MPG123_DOWN_SAMPLE, MPG123_RVA,
  MPG123_DOWNSPEED, MPG123_UPSPEED,
  MPG123_START_FRAME, MPG123_DECODE_FRAMES,
  MPG123_ICY_INTERVAL, MPG123_OUTSCALE,
  MPG123_TIMEOUT, MPG123_REMOVE_FLAGS,
  MPG123_RESYNC_LIMIT, MPG123_INDEX_SIZE,
  MPG123_PREFRAMES, MPG123_FEEDPOOL,
  MPG123_FEEDBUFFER
};

enum    mpg123_param_flags {
  MPG123_FORCE_MONO = 0x7, MPG123_MONO_LEFT = 0x1,
  MPG123_MONO_RIGHT = 0x2, MPG123_MONO_MIX = 0x4,
  MPG123_FORCE_STEREO = 0x8, MPG123_FORCE_8BIT = 0x10,
  MPG123_QUIET = 0x20, MPG123_GAPLESS = 0x40,
  MPG123_NO_RESYNC = 0x80, MPG123_SEEKBUFFER = 0x100,
  MPG123_FUZZY = 0x200, MPG123_FORCE_FLOAT = 0x400,
  MPG123_PLAIN_ID3TEXT = 0x800, MPG123_IGNORE_STREAMLENGTH = 0x1000,
  MPG123_SKIP_ID3V2 = 0x2000, MPG123_IGNORE_INFOFRAME = 0x4000,
  MPG123_AUTO_RESAMPLE = 0x8000
};

enum    mpg123_param_rva {
  MPG123_RVA_OFF = 0, MPG123_RVA_MIX = 1,
  MPG123_RVA_ALBUM = 2, MPG123_RVA_MAX = MPG123_RVA_ALBUM
};

enum    mpg123_feature_set {
  MPG123_FEATURE_ABI_UTF8OPEN = 0, MPG123_FEATURE_OUTPUT_8BIT,
  MPG123_FEATURE_OUTPUT_16BIT, MPG123_FEATURE_OUTPUT_32BIT,
  MPG123_FEATURE_INDEX, MPG123_FEATURE_PARSE_ID3V2,
  MPG123_FEATURE_DECODE_LAYER1, MPG123_FEATURE_DECODE_LAYER2,
  MPG123_FEATURE_DECODE_LAYER3, MPG123_FEATURE_DECODE_ACCURATE,
  MPG123_FEATURE_DECODE_DOWNSAMPLE, MPG123_FEATURE_DECODE_NTOM,
  MPG123_FEATURE_PARSE_ICY, MPG123_FEATURE_TIMEOUT_READ
};

enum mpg123_errors
{
  MPG123_DONE=-12,
  MPG123_NEW_FORMAT=-11,
  MPG123_NEED_MORE=-10,
  MPG123_ERR=-1,
  MPG123_OK=0,
  MPG123_BAD_OUTFORMAT,
  MPG123_BAD_CHANNEL,
  MPG123_BAD_RATE,
  MPG123_ERR_16TO8TABLE,
  MPG123_BAD_PARAM,
  MPG123_BAD_BUFFER,
  MPG123_OUT_OF_MEM,
  MPG123_NOT_INITIALIZED,
  MPG123_BAD_DECODER,
  MPG123_BAD_HANDLE,
  MPG123_NO_BUFFERS,
  MPG123_BAD_RVA,
  MPG123_NO_GAPLESS,
  MPG123_NO_SPACE,
  MPG123_BAD_TYPES,
  MPG123_BAD_BAND,
  MPG123_ERR_NULL,
  MPG123_ERR_READER,
  MPG123_NO_SEEK_FROM_END,
  MPG123_BAD_WHENCE,
  MPG123_NO_TIMEOUT,
  MPG123_BAD_FILE,
  MPG123_NO_SEEK,
  MPG123_NO_READER,
  MPG123_BAD_PARS,
  MPG123_BAD_INDEX_PAR,
  MPG123_OUT_OF_SYNC,
  MPG123_RESYNC_FAIL,
  MPG123_NO_8BIT,
  MPG123_BAD_ALIGN,
  MPG123_NULL_BUFFER,
  MPG123_NO_RELSEEK,
  MPG123_NULL_POINTER,
  MPG123_BAD_KEY,
  MPG123_NO_INDEX,
  MPG123_INDEX_FAIL,
  MPG123_BAD_DECODER_SETUP,
  MPG123_MISSING_FEATURE,
  MPG123_BAD_VALUE,
  MPG123_LSEEK_FAILED,
  MPG123_BAD_CUSTOM_IO,
  MPG123_LFS_OVERFLOW,
  MPG123_INT_OVERFLOW
};

typedef int (*mpg123_init_proc)(void);
typedef void (*mpg123_exit_proc)(void);
typedef mpg123_handle* (*mpg123_new_proc)(const char*, int*);
typedef void (*mpg123_delete_proc)(mpg123_handle*);
typedef int (*mpg123_open_feed_proc)(mpg123_handle*);
typedef int (*mpg123_decode_proc)(mpg123_handle*,const unsigned char*, size_t, unsigned char*, size_t, size_t*);
typedef int (*mpg123_param_proc)(mpg123_handle*, enum mpg123_parms, long, double);
typedef int (*mpg123_getparam_proc)(mpg123_handle*, enum mpg123_parms, long*, double*);
typedef int (*mpg123_feature_proc)(const enum mpg123_feature_set);
typedef int (*mpg123_getformat_proc)(mpg123_handle*, long*, int*, int*);

/* libmpg123 */



#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* __FILE_EXT_AUDIO_H */


