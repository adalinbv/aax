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
#include <assert.h>		/* assert */
#include <errno.h>

#include <base/dlsym.h>

#include <arch.h>
#include <devices.h>
#include <ringbuffer.h>

#include "filetype.h"
#include "flacext.h"
#include "audio.h"

#define IOBUFFER_SIZE	16384

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
DECL_FUNCTION(FLAC__stream_decoder_new);
DECL_FUNCTION(FLAC__stream_decoder_set_metadata_respond);
DECL_FUNCTION(FLAC__stream_decoder_set_metadata_ignore);
DECL_FUNCTION(FLAC__stream_decoder_init_stream);
DECL_FUNCTION(FLAC__stream_decoder_process_single);
DECL_FUNCTION(FLAC__stream_decoder_finish);
DECL_FUNCTION(FLAC__stream_decoder_delete);
DECL_FUNCTION(FLAC__stream_decoder_flush);
DECL_FUNCTION(FLAC__stream_decoder_get_state);

static _file_detect_fn _aaxFLACDetect;
static _file_new_handle_fn _aaxFLACSetup;
static _file_open_fn _aaxFLACOpen;
static _file_close_fn _aaxFLACClose;
static _file_get_name_fn _aaxFLACGetName;
static _file_default_fname_fn _aaxFLACInterfaces;
static _file_extension_fn _aaxFLACExtension;
static _file_get_param_fn _aaxFLACGetParam;
static _file_set_param_fn _aaxFLACSetParam;
static _file_cvt_to_fn _aaxFLACCvtToIntl;
static _file_cvt_from_fn _aaxFLACCvtFromIntl;

/*
 * http://flac.sourceforge.net/api/group__flac__stream__decoder.html
 * http://flac.cvs.sourceforge.net/viewvc/flac/flac/examples/c/decode/file/main.c?content-type=text%2Fplain
 * http://alure.sourcearchive.com/documentation/1.2-1/codec__flac_8cpp_source.html
 *
 * http://ffmpeg.org/doxygen/trunk/libavcodec_2flacdec_8c_source.html
 */

_aaxFmtHandle*
_aaxDetectFLACFile()
{
   _aaxFmtHandle* rv = calloc(1, sizeof(_aaxFmtHandle));
   if (rv)
   {
      rv->detect = _aaxFLACDetect;
      rv->setup = _aaxFLACSetup;
      rv->open = _aaxFLACOpen;
      rv->close = _aaxFLACClose;
      rv->update = NULL;
      rv->name = _aaxFLACGetName;

      rv->cvt_from_intl = _aaxFLACCvtFromIntl;
      rv->cvt_to_intl = _aaxFLACCvtToIntl;
      rv->cvt_endianness = NULL;
      rv->cvt_from_signed = NULL;
      rv->cvt_to_signed = NULL;

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
   void *picture;
   void *id;

   int capturing;

   int no_tracks;
   int bits_sample;
   int frequency;
   enum aaxFormat format;
   size_t blocksize;
   size_t max_samples;

   uint32_t flacBufSize;
   uint32_t flacBufferPos;
   uint8_t flacBuffer[IOBUFFER_SIZE];
   int32_t **ptr;

} _driver_t;

static FLAC__StreamDecoderReadStatus _flac_read_cb(const void*, uint8_t[], size_t*, void*);
static FLAC__StreamDecoderWriteStatus _flac_write_cb(const void*, const FLAC__Frame*, const int32_t *const buffer[], void*);
static int _flac_eof_cb(const void*, void*);
static void _flac_metadata_cb(const void*, const FLAC__StreamMetadata*, void*);
static void _flac_error_cb(const void*, FLAC__StreamDecoderErrorStatus, void*);

static int
_aaxFLACDetect(void *fmt, int mode)
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

      TIE_FUNCTION(FLAC__stream_decoder_new);
      if (pFLAC__stream_decoder_new)
      {
         TIE_FUNCTION(FLAC__stream_decoder_set_metadata_respond);
         TIE_FUNCTION(FLAC__stream_decoder_set_metadata_ignore);
         TIE_FUNCTION(FLAC__stream_decoder_init_stream);
         TIE_FUNCTION(FLAC__stream_decoder_process_single);
         TIE_FUNCTION(FLAC__stream_decoder_finish);
         TIE_FUNCTION(FLAC__stream_decoder_delete);
         TIE_FUNCTION(FLAC__stream_decoder_flush);
         TIE_FUNCTION(FLAC__stream_decoder_get_state);

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

   assert(*bufsize == IOBUFFER_SIZE);

   if (handle)
   {
      handle->id = pFLAC__stream_decoder_new();
      if (handle->id)
      {
         FLAC__StreamDecoderInitStatus res;
         res = pFLAC__stream_decoder_init_stream(handle->id,
                                                 _flac_read_cb,
                                                 NULL, NULL, NULL,
                                                 _flac_eof_cb,
                                                 _flac_write_cb,
                                                 _flac_metadata_cb,
                                                 _flac_error_cb,
                                                 handle);
         if (res == FLAC__STREAM_DECODER_INIT_STATUS_OK)
         {
            if (handle->capturing) {
               memcpy(handle->flacBuffer, buf, *bufsize);
            }

            pFLAC__stream_decoder_process_single(handle->id);

            if (!handle->capturing)
            {
               *bufsize = handle->flacBufSize;
               memcpy(buf, handle->flacBuffer, *bufsize);
            }
            rv = buf;
         }
      }
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
      free(handle->artist);
      free(handle->title);
      free(handle->album);
      free(handle->trackno);
      free(handle->date);
      free(handle->genre);
      free(handle->copyright);
      free(handle->comments);
      free(handle->picture);

      pFLAC__stream_decoder_finish(handle->id);
      pFLAC__stream_decoder_delete(handle->id);
      free(handle);
   }

   return res;
}

static void*
_aaxFLACSetup(int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
{
   int bits_sample = aaxGetBitsPerSample(format);
   _driver_t *handle = NULL;

   if (bits_sample)
   {
      handle = calloc(1, sizeof(_driver_t));
      if (handle)
      {
         handle->capturing = (mode > 0) ? 0 : 1;
         *bufsize = IOBUFFER_SIZE;
      }
      else {
         _AAX_FILEDRVLOG("FLACFile: Insufficient memory");
      }
   }
   else {
      _AAX_FILEDRVLOG("FLACFile: Unsupported format");
   }

   return (void*)handle;
}

static size_t
_aaxFLACCvtFromIntl(void *id, int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   _driver_t *handle = (_driver_t *)id;
   int bits, ret, rv = __F_EOF;
   size_t bytes;

   bits = handle->bits_sample;
   bytes = _MIN(num*tracks*bits/8, IOBUFFER_SIZE);

   handle->ptr = dptr;
   handle->flacBufferPos = 0;
   handle->flacBufSize = bytes;
   memcpy(handle->flacBuffer, sptr, bytes);
   ret = pFLAC__stream_decoder_process_single(handle->id);
   if (!ret)
   {
      FLAC__StreamDecoderState state;
      state = pFLAC__stream_decoder_get_state(handle->id);
      switch (state)
      {
      case FLAC__STREAM_DECODER_ABORTED:
         // aborted by the read callback
         // this means the decoder needs more data
         rv = __F_PROCESS;
         break;
      case FLAC__STREAM_DECODER_SEEK_ERROR:
         pFLAC__stream_decoder_flush(handle->id);
         rv = __F_PROCESS;
         break;
      case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
         _AAX_FILEDRVLOG("FLACFile: unable to allocate memory.");
         // break not needed
      case FLAC__STREAM_DECODER_END_OF_STREAM:
         rv = __F_EOF;
         break;
      default:
         break;
      }
   }
   else {
      rv = handle->flacBufferPos;
   }

   return rv;
}

static size_t
_aaxFLACCvtToIntl(void *id, void_ptr dptr, const_int32_ptrptr sptr, size_t offset, unsigned int tracks, size_t num, void *scratch, size_t scratchlen)
{
   _driver_t *handle = (_driver_t *)id;
   size_t rv = 0;

   pFLAC__stream_decoder_process_single(handle->id);

   if (!handle->capturing)
   {
//    memcpy(buf, handle->flacBuffer, *bufsize);
   }

   return rv;
}

static int
_aaxFLACExtension(char *ext) {
   return (ext && !strcasecmp(ext, "flac")) ? 1 : 0;
}

static char*
_aaxFLACInterfaces(int mode)
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


/*
 * This function will be called when the decoder has decoded a metadata block.
 * In a valid FLAC file there will always be one STREAMINFO block, followed by
 * zero or more other metadata blocks. These will be supplied by the decoder in
 * the same order as they appear in the stream and always before the first audio
 * frame (i.e. write callback). The metadata block that is passed in must not
 * be modified, and it doesn't live beyond the callback, so you should make a
 * copy of it with FLAC__metadata_object_clone() if you will need it elsewhere.
 */
void
_flac_metadata_cb(const void *id, const FLAC__StreamMetadata *metadata, void *d)
{
   _driver_t *handle = (_driver_t *)d;

   if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
   {
      FLAC__StreamMetadata_StreamInfo *data;

      data = (FLAC__StreamMetadata_StreamInfo*)&metadata->data;
      handle->no_tracks = data->channels;
      handle->bits_sample = data->bits_per_sample;
      handle->frequency = data->sample_rate;
//    handle->blocksize = 
      handle->max_samples = data->total_samples;
   }
}

/*
 * This function will be called when the decoder needs more input data. The
 * address of the buffer to be filled is supplied, along with the number of
 * bytes the buffer can hold. The callback may choose to supply less data and
 * modify the byte count but must be careful not to overflow the buffer.
 */
static FLAC__StreamDecoderReadStatus
_flac_read_cb(const void *id, uint8_t buffer[], size_t *bytes, void *d)
{
// _driver_t *handle = (_driver_t *)d;
   FLAC__StreamDecoderReadStatus rv;

      rv = FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
// rv = FLAC__STREAM_DECODER_READ_STATUS_ABORT  // need more data

   return rv;
}

/*
 * This function will be called when the decoder has decoded a single audio
 * frame. The decoder will pass the frame metadata as well as an array of
 * pointers (one for each channel) pointing to the decoded audio.
 */
static FLAC__StreamDecoderWriteStatus
_flac_write_cb(const void *id, const FLAC__Frame *frame, const int32_t *const buffer[], void *d)
{
// _driver_t *handle = (_driver_t *)d;
   FLAC__StreamDecoderWriteStatus rv;

// handle->flacBufferPos = 0;
// handle->flacBufSize = bytes;
// handle->flacBuffer // source
// handle->ptr  // destenation int32_t**

      rv = FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
// rv = FLAC__STREAM_DECODER_WRITE_STATUS_ABORT  // need more data

   return rv;
}

static int
_flac_eof_cb(const void *id, void *d)
{
   return 0; // or 1 if at the end of the file
}

static void
_flac_error_cb(const void *id, FLAC__StreamDecoderErrorStatus status, void *d)
{
   _AAX_FILEDRVLOG(FLAC__StreamDecoderErrorStatusString[status]);
}

