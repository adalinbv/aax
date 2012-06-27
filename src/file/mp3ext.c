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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>		/* for malloc */
#include <string.h>		/* for strdup */
#include <fcntl.h>		/* SEEK_*, O_* */
#include <assert.h>		/* assert */
#include <errno.h>
#if HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif

#include <ringbuffer.h>
#include <base/dlsym.h>

#include "filetype.h"
#include "audio.h"

DECL_FUNCTION(mpg123_init);
DECL_FUNCTION(mpg123_exit);
DECL_FUNCTION(mpg123_new);
DECL_FUNCTION(mpg123_open_feed);
DECL_FUNCTION(mpg123_decode);
DECL_FUNCTION(mpg123_delete);
DECL_FUNCTION(mpg123_param);
DECL_FUNCTION(mpg123_getparam);
DECL_FUNCTION(mpg123_feature);
DECL_FUNCTION(mpg123_getformat);

static _detect_fn _aaxMPG123FileDetect;

static _new_hanle_fn _aaxMPG123FileSetup;
static _open_fn _aaxMPG123FileOpen;
static _close_fn _aaxMPG123FileClose;
static _update_fn _aaxMPG123FileReadWrite;

static _default_fname_fn _aaxMPG123FileInterfaces;
static _extension_fn _aaxMPG123FileExtension;

static _get_param_fn _aaxMPG123FileGetFormat;
static _get_param_fn _aaxMPG123FileGetNoTracks;
static _get_param_fn _aaxMPG123FileGetFrequency;
static _get_param_fn _aaxMPG123FileGetBitsPerSample;

_aaxFileHandle*
_aaxDetectMP3File()
{
   _aaxFileHandle* rv = NULL;

   rv = calloc(1, sizeof(_aaxFileHandle));
   if (rv)
   {
      rv->detect = (_detect_fn*)&_aaxMPG123FileDetect;
      rv->setup = (_new_hanle_fn*)&_aaxMPG123FileSetup;
      rv->open = (_open_fn*)&_aaxMPG123FileOpen;
      rv->close = (_close_fn*)&_aaxMPG123FileClose;
      rv->update = (_update_fn*)&_aaxMPG123FileReadWrite;

      rv->supported = (_extension_fn*)&_aaxMPG123FileExtension;
      rv->interfaces = (_default_fname_fn*)&_aaxMPG123FileInterfaces;

      rv->get_format = (_get_param_fn*)&_aaxMPG123FileGetFormat;
      rv->get_no_tracks = (_get_param_fn*)&_aaxMPG123FileGetNoTracks;
      rv->get_frequency = (_get_param_fn*)&_aaxMPG123FileGetFrequency;
      rv->get_bits_per_sample = (_get_param_fn*)&_aaxMPG123FileGetBitsPerSample;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

typedef struct
{
   char *name;

   int fd;
   int mode;
   int capturing;

   int frequency;
   enum aaxFormat format;
// uint16_t blocksize;
   uint8_t no_tracks;
   uint8_t bits_sample;

   void *blockbuf;
   unsigned int blockstart_smp;

} _handle_t;

static int
_aaxMPG123FileDetect(int mode)
{
   static void *audio = NULL;
   int rv = AAX_FALSE;

   if (!audio) {
      audio = _oalIsLibraryPresent("mpg123", "0");
   }

   if (audio)
   {
      char *error;
      _oalGetSymError(0);

      TIE_FUNCTION(mpg123_init);
      if (pmpg123_init)
      {
         TIE_FUNCTION(mpg123_exit);
         TIE_FUNCTION(mpg123_new);
         TIE_FUNCTION(mpg123_open_feed);
         TIE_FUNCTION(mpg123_decode);
         TIE_FUNCTION(mpg123_delete);
         TIE_FUNCTION(mpg123_param);
         TIE_FUNCTION(mpg123_getparam);
         TIE_FUNCTION(mpg123_feature);
         TIE_FUNCTION(mpg123_getformat);

         error = _oalGetSymError(0);
         if (!error) {
            rv = AAX_TRUE;
         }
      }
   }
   return rv;
}

static int
_aaxMPG123FileOpen(void *id, const char* fname)
{
   _handle_t *handle = (_handle_t *)id;
   int res = AAX_TRUE;
   return (res >= 0) ? AAX_TRUE : AAX_FALSE;
}

static int
_aaxMPG123FileClose(void *id)
{
   _handle_t *handle = (_handle_t *)id;
   int ret = AAX_TRUE;
   return ret;
}

static void*
_aaxMPG123FileSetup(int mode, int freq, int tracks, int format)
{
   _handle_t *handle = NULL;
   return (void*)handle;
}

static int
_aaxMPG123FileReadWrite(void *id, void *data, unsigned int no_frames)
{
   _handle_t *handle = (_handle_t *)id;
   int rv = AAX_FALSE;

   if (handle->capturing)
   {
   }

   return rv;
}

static int
_aaxMPG123FileExtension(char *ext) {
   return !strcasecmp(ext, "mp3");
}

static char*
_aaxMPG123FileInterfaces(int mode)
{
   static const char *rd[2] = {
    "*.mp3\0",
    "\0"
   };
   return (char *)rd[mode];
}

static unsigned int
_aaxMPG123FileGetFormat(void *id)
{
   _handle_t *handle = (_handle_t *)id;
   return handle->format;
}

static unsigned int
_aaxMPG123FileGetNoTracks(void *id)
{
   _handle_t *handle = (_handle_t *)id;
   return handle->no_tracks;
}

static unsigned int
_aaxMPG123FileGetFrequency(void *id)
{
   _handle_t *handle = (_handle_t *)id;
   return handle->frequency;
}

static unsigned int
_aaxMPG123FileGetBitsPerSample(void *id)
{
   _handle_t *handle = (_handle_t *)id;
   return handle->bits_sample;
}

