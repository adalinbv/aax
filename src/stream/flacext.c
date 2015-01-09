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

#if HAVE_CONFIG_H
#include "config.h"
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
#include <fcntl.h>		/* SEEK_*, O_* */
#include <assert.h>		/* assert */
#include <errno.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif

#include <aax/aax.h>

#include <ringbuffer.h>
#include <base/dlsym.h>
#include <arch.h>

#include "filetype.h"
#include "audio.h"

// https://xiph.org/flac/api/index.html
// https://xiph.org/flac/api/group__flac__stream__decoder.html
/*
 * The basic usage of this decoder is as follows:
 *
 *   The program creates an instance of a decoder using
 *       FLAC__stream_decoder_new().
 *
 *   The program overrides the default settings using
 *       FLAC__stream_decoder_set_*() functions.
 *
 *   The program initializes the instance to validate the settings and prepare
 *   for decoding using
 *       FLAC__stream_decoder_init_stream() or FLAC__stream_decoder_init_FILE()
 *       or FLAC__stream_decoder_init_file() for native FLAC,
 *       FLAC__stream_decoder_init_ogg_stream() or
 *       FLAC__stream_decoder_init_ogg_FILE() or 
 *       FLAC__stream_decoder_init_ogg_file() for Ogg FLAC
 *
 *   The program calls the FLAC__stream_decoder_process_*() functions to
 *   decode data, which subsequently calls the callbacks.
 *   The program finishes the decoding with FLAC__stream_decoder_finish(),
 *   which flushes the input and output and resets the decoder to the
 *   uninitialized state.
 *
 *   The instance may be used again or deleted with
 *    FLAC__stream_decoder_delete().
 */


DECL_FUNCTION(FLAC__stream_decoder_init_stream);
DECL_FUNCTION(FLAC__stream_decoder_new);

static _detect_fn _aaxFLACFileDetect;

static _file_new_handle_fn _aaxFLACSetup;
static _file_open_fn _aaxFLACOpen;
static _file_close_fn _aaxFLACClose;
static _file_update_fn _aaxFLACUpdate;
static _file_get_name_fn _aaxFLACGetName;

static _file_cvt_to_fn _aaxFLACCvtToIntl;
static _file_cvt_from_fn _aaxFLACCvtFromIntl;
static _file_cvt_fn _aaxFLACCvtEndianness;
static _file_cvt_fn _aaxFLACCvtToSigned;
static _file_cvt_fn _aaxFLACCvtFromSigned;

static _file_default_fname_fn _aaxFLACInterfaces;
static _file_extension_fn _aaxFLACExtension;

static _file_get_param_fn _aaxFLACGetParam;
static _file_set_param_fn _aaxFLACSetParam;

/*
 * http://flac.sourceforge.net/api/group__flac__stream__decoder.html
 * http://flac.cvs.sourceforge.net/viewvc/flac/flac/examples/c/decode/file/main.c?content-type=text%2Fplain
 * http://alure.sourcearchive.com/documentation/1.2-1/codec__flac_8cpp_source.html
 *
 * http://ffmpeg.org/doxygen/trunk/libavcodec_2flacdec_8c_source.html
 */

_aaxFileHandle*
_aaxDetectFLACFile()
{
   _aaxFmtHandle* rv = calloc(1, sizeof(_aaxFmtHandle));
   if (rv)
   {
      rv->detect = _aaxFLACDetect;
      rv->setup = _aaxFLACSetup;
      rv->open = _aaxFLACOpen;
      rv->close = _aaxFLACClose;
      rv->update = _aaxFLACUpdate;
      rv->name = _aaxFLACGetName;

      rv->cvt_from_intl = _aaxFLACCvtFromIntl;
      rv->cvt_to_intl = _aaxFLACCvtToIntl;
      rv->cvt_endianness = _aaxFLACCvtEndianness;
      rv->cvt_from_signed = _aaxFLACCvtFromSigned;
      rv->cvt_to_signed = _aaxFLACCvtToSigned;

      rv->supported = _aaxFLACExtension;
      rv->interfaces = _aaxFLACInterfaces;

      rv->get_param = _aaxFLACGetParam;
      rv->set_param = _aaxFLACSetParam;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

typedef struct
{
   char *artist;
   char *title;
   char *album;
   char *trackno;
   char *date;
   char *genre;
   char *copyright;
   char *comments;
   void *id;

   int capturing;

   int no_tracks;
   int bits_sample;
   int frequency;
   enum aaxFormat format;
   size_t blocksize;
   size_t max_samples;

   int mode;

   unsigned char *blockbuf;
   unsigned int offset_samples;

} _handle_t;

static int
_aaxFLACFileDetect(int mode)
{
   static void *audio = NULL;
   int rv = AAX_FALSE;

   if (!audio) {
      audio = _aaxIsLibraryPresent("FLAC", "8");
   }
   if (!audio) {
      audio = _aaxIsLibraryPresent("libFLAC", "0");
   }

   if (audio)
   {
      char *error;
      _aaxGetSymError(0);

      TIE_FUNCTION(FLAC__stream_decoder_init_stream);
      if (pFLAC__stream_decoder_init_stream)
      {
         TIE_FUNCTION(FLAC__stream_decoder_new);

         error = _aaxGetSymError(0);
         if (!error) {
            rv = AAX_TRUE;
         }
      }
   }
   return rv;
}

static void*
_aaxFLACOpen(void *id, void *buf, size_t *bufsize, size_t fsize)
{
   _driver_t *handle = (_driver_t *)id;
   void *rv = NULL;

   assert(bufsize);

   if (handle)
   {
   }
   else
   {
      _AAX_FILEDRVLOG("WAVFile: Internal error: handle id equals 0");
   }

   return rv;
}

static int
_aaxFLACClose(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int res = AAX_TRUE;

   if (handle)
   {
   }

   return res;
}

static void*
_aaxFLACSetup(int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
{
   int bits_sample = aaxGetBitsPerSample(format);
   _driver_t *handle = NULL;

   return (void*)handle;
}

static void*
_aaxFLACUpdate(void *id, size_t *offs, size_t *size, char close)
{
   _driver_t *handle = (_driver_t *)id;
   void *rv = NULL;

   return rv;
}

static size_t
_aaxFLACCvtFromIntl(void *id, int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   _driver_t *handle = (_driver_t *)id;
   size_t rv  = 0;

   return rv;
}

static size_t
_aaxFLACCvtToIntl(void *id, void_ptr dptr, const_int32_ptrptr sptr, size_t offset, unsigned int tracks, size_t num, void *scratch, size_t scratchlen)
{
   _driver_t *handle = (_driver_t *)id;
   size_t rv = 0;

   return rv;
}

static int
_aaxFLACFileExtension(char *ext) {
   return (ext && !strcasecmp(ext, "flac")) ? 1 : 0;
}

static char*
_aaxFLACFileInterfaces(int mode)
{
   static const char *rd[2] = { "*.flac\0", "*.flac\0" };
   return (char *)rd[mode];
}

static char*
_aaxFLACGetName(void *id, enum _aaxFileParam param)
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
   default:
      break;
   }
   return rv;
}

static off_t
_aaxFLACGetParam(void *id, int type)
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
      break;
   }
   return rv;
}

static  off_t
_aaxFLACSetParam(void *id, int type, off_t value)
{
// _driver_t *handle = (_driver_t *)id;
   off_t rv = 0;

   switch(type)
   {
   case __F_POSITION:
      break;
   default:
      break;
   }
   return rv;
}

