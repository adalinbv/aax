/*
 * Copyright 2005-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
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

#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>   /* strcasecmp */
# endif
#endif
#include <assert.h>		/* assert */
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_IO_H
# include <io.h>
#endif

#include <xml.h>
#include <aax/aax.h>

#include <base/dlsym.h>

#include <arch.h>
#include <ringbuffer.h>

#include "extension.h"
#include "audio.h"

#define	MAX_ID3V1_GENRES	192
#define __DUP(a, b)	if ((b) != NULL && (b)->fill) a = strdup((b)->p);
#define __COPY(a, b)	do { int s = sizeof(b); \
      a = calloc(1, s+1); if (a) memcpy(a,b,s); \
   } while(0);

static _ext_detect_fn _aaxMP3Detect;
static _ext_new_handle_fn _aaxMP3Setup;
static _ext_get_name_fn _aaxMP3GetName;
static _ext_default_fname_fn _aaxMP3Interfaces;
static _ext_extension_fn _aaxMP3Extension;
static _ext_get_param_fn _aaxMP3GetParam;

static _ext_open_fn *_aaxMP3Open;
static _ext_close_fn *_aaxMP3Close;
static _ext_cvt_to_fn *_aaxMP3CvtToIntl;
static _ext_cvt_from_fn *_aaxMP3CvtFromIntl;
static _ext_set_param_fn *_aaxMP3SetParam;

_aaxFmtHandle*
_aaxDetectMP3Format()
{
   _aaxFmtHandle* rv = NULL;

   rv = calloc(1, sizeof(_aaxFmtHandle));
   if (rv)
   {
      rv->detect = _aaxMP3Detect;
      rv->setup = _aaxMP3Setup;
      rv->open = _aaxMP3Open;
      rv->close = _aaxMP3Close;
      rv->name = _aaxMP3GetName;
      rv->update = NULL;

      rv->copy = _aaxMP3CvtFromIntl;
      rv->cvt_from_intl = _aaxMP3CvtFromIntl;
      rv->cvt_to_intl = _aaxMP3CvtToIntl;
      rv->cvt_endianness = NULL;
      rv->cvt_from_signed = NULL;
      rv->cvt_to_signed = NULL;

      rv->supported = _aaxMP3Extension;
      rv->interfaces = _aaxMP3Interfaces;

      rv->get_param = _aaxMP3GetParam;
      rv->set_param = _aaxMP3SetParam;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

typedef struct
{
   void *id;
   char *artist;
   char *original;
   char *title;
   char *album;
   char *trackno;
   char *date;
   char *genre;
   char *composer;
   char *comments;
   char *copyright;
   char *website;
   char *image;

   char capturing;
   char id3_found;
   char streaming;

   uint8_t no_tracks;
   uint8_t bits_sample;
   int frequency;
   int bitrate;
   int blocksize;
   enum aaxFormat format;
   size_t no_samples;
   size_t max_samples;

   size_t mp3BufSize;
   void *mp3Buffer;
   void *mp3ptr;

   void *xid;	// XML genre names

#ifdef WINXP
   HACMSTREAM acmStream;
   ACMSTREAMHEADER acmStreamHeader;

   LPBYTE pcmBuffer;
   unsigned long pcmBufPos;
   unsigned long pcmBufMax;
#endif

} _driver_t;

#include "ext_mp3_mpg123.c"
#include "ext_mp3_msacm.c"

static int
_aaxMP3Detect(void *fmt, int mode)
{
   static void *_audio[2] = { NULL, NULL };
   int m = (mode > 0) ? 1 : 0;
   int rv = AAX_FALSE;

#ifdef WINXP
   rv = _aaxMSACMDetect(fmt, m, _audio);
#endif
   /* if not found, try mpg123  with lame */
   if (rv == AAX_FALSE) {
      rv = _aaxMPG123Detect(fmt, m, _audio);
   }

   return rv;
}

static void*
_aaxMP3Setup(int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
{
   _driver_t *handle = NULL;

   *bufsize = 0;
   if (1) // mode == 0)
   {
      handle = calloc(1, sizeof(_driver_t));
      if (handle)
      {
         handle->capturing = (mode == 0) ? 1 : 0;
         handle->blocksize = 4096;
         handle->frequency = freq;
         handle->no_tracks = tracks;
         handle->bitrate = bitrate;
         handle->format = format;
         handle->no_samples = no_samples;
         handle->max_samples = 0;
         handle->bits_sample = aaxGetBitsPerSample(handle->format);

         if (mode == 0) {
            *bufsize = (no_samples*tracks*handle->bits_sample)/8;
         }
      }
      else {
         _AAX_FILEDRVLOG("MP3: Insufficient memory");
      }
   }
   else {
      _AAX_FILEDRVLOG("MP3: playback is not supported");
   }

   return (void*)handle;
}

static int
_aaxMP3Extension(char *ext) {
   return (ext && !strcasecmp(ext, "mp3")) ? 1 : 0;
}

static char*
_aaxMP3Interfaces(int mode)
{
   static const char *rd[2] = { "*.mp3\0", "*.mp3\0" };
   return (char *)rd[mode];
}

static char*
_aaxMP3GetName(void *id, enum _aaxStreamParam param)
{
   _driver_t *handle = (_driver_t *)id;
   char *rv = NULL;

   switch(param)
   {
   case __F_ARTIST:
      rv = handle->artist;
      break;
   case __F_TITLE:
      rv = handle->title;
      break;
   case __F_COMPOSER:
      rv = handle->composer;
      break;
   case __F_GENRE:
      rv = handle->genre;
      break;
   case __F_TRACKNO:
      rv = handle->trackno;
      break;
   case __F_ALBUM:
      rv = handle->album;
      break;
   case __F_DATE:
      rv = handle->date;
      break;
   case __F_COMMENT:
      rv = handle->comments;
      break;
   case __F_COPYRIGHT:
      rv = handle->copyright;
      break;
   case __F_ORIGINAL:
      rv = handle->original;
      break;
   case __F_WEBSITE:
      rv = handle->website;
      break;
   case __F_IMAGE:
      rv = handle->image;
      break;
   default:
      break;
   }
   return rv;
}

static off_t
_aaxMP3GetParam(void *id, int type)
{
   _driver_t *handle = (_driver_t *)id;
   off_t rv = 0;

   switch(type)
   {
   case __F_FMT:
      rv = handle->format;
      break;
   case __F_TRACKS:
      rv = handle->no_tracks;
      break;
   case __F_FREQ:
      rv = handle->frequency;
      break;
   case __F_BITS:
      rv = handle->bits_sample;
      break;
   case __F_BLOCK:
      rv = handle->blocksize;
      break;
   case __F_SAMPLES:
      rv = handle->max_samples;
      break;
   default:
      if (type & __F_NAME_CHANGED)
      {
         switch (type & ~__F_NAME_CHANGED)
         {
         default:
            break;
         }
      }
      break;
   }
   return rv;
}

