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

#ifndef _AAX_FILETYPE_H
#define _AAX_FILETYPE_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

typedef int (_detect_fn)(int);
typedef void* (_new_hanle_fn)(int, int, int, int, int, int);
typedef int (_open_fn)(void*, const char*);
typedef int (_close_fn)(void*);
typedef int (_update_fn)(void*, void*, unsigned int);

typedef char* (_default_fname_fn)(int);
typedef int (_extension_fn)(char*);
typedef unsigned int (_get_param_fn)(void *);

typedef struct
{
   void *id;
   _detect_fn *detect;
   _new_hanle_fn *setup;

   _open_fn *open;
   _close_fn *close;
   _update_fn *update;

   _extension_fn *supported;
   _default_fname_fn *interfaces;

   _get_param_fn *get_format;
   _get_param_fn *get_no_tracks;
   _get_param_fn *get_frequency;
   _get_param_fn *get_bits_per_sample;

} _aaxFileHandle;


typedef _aaxFileHandle* (_aaxExtensionDetect)(void);

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

