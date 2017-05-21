/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#include <string.h>
#include <assert.h>
#include <xml.h>

#include <base/dlsym.h>

#include <api.h>
#include <arch.h>

#include "extension.h"
#include "format.h"
#include "fmt_opus.h"
#include "audio.h"

// https://android.googlesource.com/platform/external/libopus/+/refs/heads/master-soong/doc/trivial_example.c
#define FRAME_SIZE 960
#define MAX_FRAME_SIZE 6*960
#define MAX_PACKET_SIZE (3*1276)

DECL_FUNCTION(opus_decoder_create);
DECL_FUNCTION(opus_decoder_destroy);
DECL_FUNCTION(opus_decode_float);
DECL_FUNCTION(opus_decode);

DECL_FUNCTION(opus_encoder_create);
DECL_FUNCTION(opus_encoder_destroy);
DECL_FUNCTION(opus_encoder_ctl);
DECL_FUNCTION(opus_encode);

DECL_FUNCTION(opus_strerror);
DECL_FUNCTION(opus_get_version_string);

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

   void *audio;
   int mode;

   char capturing;

   uint8_t no_tracks;
   uint8_t bits_sample;
   unsigned int frequency;
   unsigned int bitrate;
   unsigned int blocksize;
   enum aaxFormat format;
   size_t no_samples;
   size_t max_samples;

   _data_t *opusBuffer;

   float *outputs;
   unsigned int out_pos;
   unsigned int out_size;

} _driver_t;


int
_opus_detect(_fmt_t *fmt, int mode)
{
   void *audio = NULL;
   int rv = AAX_FALSE;

   audio = _aaxIsLibraryPresent("opus", "0");
   if (!audio) {
      audio = _aaxIsLibraryPresent("libopus", "0");
   }

   if (audio)
   {
      char *error;

      _aaxGetSymError(0);

      TIE_FUNCTION(opus_decoder_create);
      if (popus_decoder_create)
      {
         TIE_FUNCTION(opus_decoder_destroy);
         TIE_FUNCTION(opus_decode_float);
         TIE_FUNCTION(opus_decode);

         TIE_FUNCTION(opus_encoder_create);
         TIE_FUNCTION(opus_encoder_destroy);
         TIE_FUNCTION(opus_encoder_ctl);
         TIE_FUNCTION(opus_encode);

         error = _aaxGetSymError(0);
         if (!error)
         {
            /* not required but useful */
            TIE_FUNCTION(opus_strerror);
            TIE_FUNCTION(opus_get_version_string);

            fmt->id = calloc(1, sizeof(_driver_t));
            if (fmt->id)
            {
               _driver_t *handle = fmt->id;

               handle->audio = audio;
               handle->mode = mode;
               handle->capturing = (mode == 0) ? 1 : 0;
               handle->blocksize = FRAME_SIZE;

               rv = AAX_TRUE;
            }             
            else {
               _AAX_FILEDRVLOG("OPUS: Insufficient memory");
            }
         }
      }
   }

   return rv;
}

void*
_opus_open(_fmt_t *fmt, void *buf, size_t *bufsize, VOID(size_t fsize))
{
   _driver_t *handle = fmt->id;
   void *rv = NULL;

   assert(bufsize);

   if (handle)
   {
      if (!handle->opusBuffer)
      {
         handle->opusBuffer = _aaxDataCreate(100*1024, 1);

         handle->out_pos = 0;
         handle->out_size = MAX_FRAME_SIZE*handle->no_tracks*sizeof(float);
         handle->outputs = _aax_aligned_alloc(handle->out_size);
      }

      if (handle->opusBuffer)
      {
         if (handle->capturing)
         {
            if (_opus_fill(fmt, buf, bufsize) > 0)
            {
               if (!handle->id)
               {
                  int err, tracks = handle->no_tracks;
                  int32_t freq = 48000;

                  handle->frequency = freq;
		  handle->blocksize = FRAME_SIZE;
		  handle->format = AAX_PCM24S;
		  handle->bits_sample = aaxGetBitsPerSample(handle->format);

		  handle->id = popus_decoder_create(freq, tracks, &err);
                  if (!handle->id && popus_strerror)
                  {
                     char s[1025];
                     snprintf(s, 1024, "OPUS: Unable to create a handle: %s",
                                        popus_strerror(err));
                     s[1024] = 0;
                     _aaxStreamDriverLog(NULL, 0, 0, s);
                  }
               }
            }
         }
      }
   }
   else
   {
      _AAX_FILEDRVLOG("OPUS: Internal error: handle id equals 0");
   }

   return rv;
}

void
_opus_close(_fmt_t *fmt)
{
   _driver_t *handle = fmt->id;

   if (handle)
   {
      popus_encoder_destroy(handle->id);
      handle->id = NULL;

      _aaxDataDestroy(handle->opusBuffer);
      _aax_aligned_free(handle->outputs);

      free(handle->trackno);
      free(handle->artist);
      free(handle->title);
      free(handle->album);
      free(handle->date);
      free(handle->genre);
      free(handle->comments);
      free(handle->composer);
      free(handle->copyright);
      free(handle->original);
      free(handle->website);
      free(handle->image);
      free(handle);
   }
}

int
_opus_setup(VOID(_fmt_t *fmt), VOID(_fmt_type_t pcm_fmt), VOID(enum aaxFormat aax_fmt))
{
   return AAX_TRUE;
}

size_t
_opus_fill(_fmt_t *fmt, void_ptr sptr, size_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t rv = __F_PROCESS;

   if (_aaxDataAdd(handle->opusBuffer, sptr, *bytes) == 0) {
      *bytes = 0;
   }

   return rv;
}

size_t
_opus_copy(_fmt_t *fmt, int32_ptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   unsigned int bits, tracks, framesize, packet_sz;
   size_t req, bufsize, rv = 0;
   unsigned char *buf;
   int ret, n;

   req = *num;
   packet_sz = handle->blocksize;
   tracks = handle->no_tracks;
   bits = handle->bits_sample;
   framesize = tracks*bits/8;
   *num = 0;

   buf = handle->opusBuffer->data;
   bufsize = handle->opusBuffer->avail;

   /* there is still data left in the buffer from the previous run */
   if (handle->out_pos > 0)
   {
      const_int32_ptrptr outputs = (const_int32_ptrptr)handle->outputs;
      unsigned char *ptr = (unsigned char*)dptr;
      unsigned int pos = handle->out_pos;
      unsigned int max = _MIN(req, handle->out_size - pos);

      ptr += dptr_offs*framesize;
      _batch_cvt24_intl_ps(ptr, outputs, pos, tracks, max);

      dptr_offs += max;
      handle->out_pos += max;
      handle->no_samples += max;
      if (handle->out_pos == handle->out_size) {
         handle->out_pos = 0;
      }
      req -= max;
      *num = max;
      if (req == 0) {
         rv = 1;
      }
   }

   while (req > 0)
   {
      do
      {
         ret = n = popus_decode_float(handle->id, buf, bufsize, 
                                      handle->outputs, packet_sz, 0);
         if (n > 0)
         {
            rv += _aaxDataMove(handle->opusBuffer, NULL, packet_sz);
            bufsize = handle->opusBuffer->avail;

            handle->out_pos += n;
         }
      }
      while (ret && n == 0);

      if (ret > 0)
      {
         const_int32_ptrptr outputs = (const_int32_ptrptr)handle->outputs;
         unsigned char *ptr = (unsigned char*)dptr;

         if (n > (int)req)
         {
            handle->out_size = n;
            handle->out_pos = req;
            n = req;
            req = 0;
         }
         else
         {
            assert(handle->out_pos == 0);
            req -= n;
         }

         *num += n;
         handle->no_samples += n;

         ptr += dptr_offs*framesize;
         _batch_cvt24_intl_ps(ptr, outputs, 0, tracks, n);
         dptr_offs += n;
      }
      else {
         break;
      }
   }
   handle->max_samples = handle->no_samples;

   return rv;
}

size_t
_opus_cvt_from_intl(_fmt_t *fmt, int32_ptrptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   unsigned int bits, tracks, framesize, packet_sz;
   size_t req, bufsize, rv = 0;
   unsigned char *buf;
   int ret, n;

   req = *num;
   tracks = handle->no_tracks;
   packet_sz = handle->blocksize;
   bits = handle->bits_sample;
   framesize = tracks*bits/8;
   *num = 0;

   buf = handle->opusBuffer->data;
   bufsize = handle->opusBuffer->avail;

   /* there is still data left in the buffer from the previous run */
   if (handle->out_pos > 0)
   {
      const_int32_ptrptr outputs = (const_int32_ptrptr)handle->outputs;
      unsigned int pos = handle->out_pos*framesize;
      unsigned int max = _MIN(req, handle->out_size - pos);

      _batch_cvt24_intl_ps(dptr, outputs+pos, dptr_offs, tracks, max);

      dptr_offs += max;
      handle->out_pos += max;
      handle->no_samples += max;
      if (handle->out_pos == handle->out_size) {
         handle->out_pos = 0;
      }
      req -= max;
      *num = max;
      if (req == 0) {
         rv = 1;
      }
   }

   while (req > 0)
   {
      ret = 0;
      do
      {
         ret = n = popus_decode_float(handle->id, buf, bufsize,
                                      handle->outputs, packet_sz, 0);
         if (ret > 0)
         {
            rv += _aaxDataMove(handle->opusBuffer, NULL, ret);
            bufsize = handle->opusBuffer->avail;
         }
      }
      while (ret && n == 0);

      if (ret > 0)
      {
         const_int32_ptrptr outputs = (const_int32_ptrptr)handle->outputs;

         if (n > (int)req)
         {
            handle->out_size = n;
            handle->out_pos = req;
            n = req;
            req = 0;
         }
         else
         {
            assert(handle->out_pos == 0);
            req -= n;
         }

         _batch_cvt24_intl_ps(dptr, outputs, dptr_offs, tracks, n);
         handle->no_samples += n;
         dptr_offs += n;
         *num += n;
      }
      else {
         break;
      }
   }

   return rv;
}

size_t
_opus_cvt_to_intl(_fmt_t *fmt, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = fmt->id;
   int res;

   assert(scratchlen >= *num*handle->no_tracks*sizeof(int32_t));

   handle->no_samples += *num;
   _batch_cvt16_intl_24(scratch, sptr, offs, handle->no_tracks, *num);

   /*
    * -- about *num --
    * Number of samples per channel in the input signal. This must be an Opus
    * frame size for the encoder's sampling rate. For example, at 48 kHz the
    * permitted values are 120, 240, 480, 960, 1920, and 2880. Passing in a
    * duration of less than 10 ms (480 samples at 48 kHz) will prevent the
    * encoder from using the LPC or hybrid modes. 
    */
   res = popus_encode(handle->id, scratch, *num,
                      handle->opusBuffer->data, handle->opusBuffer->avail);
   _aax_memcpy(dptr, handle->opusBuffer, res);

   return res;
}

char*
_opus_name(_fmt_t *fmt, enum _aaxStreamParam param)
{
   _driver_t *handle = fmt->id;
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

off_t
_opus_get(_fmt_t *fmt, int type)
{
   _driver_t *handle = fmt->id;
   off_t rv = 0;

   switch(type)
   {
   case __F_FMT:
      rv = handle->format;
      break;
   case __F_TRACKS:
      rv = handle->no_tracks;
      break;
   case __F_FREQUENCY:
      rv = handle->frequency;
      break;
   case __F_BITS_PER_SAMPLE:
      rv = handle->bits_sample;
      break;
   case __F_BLOCK_SIZE:
      rv = handle->blocksize;
      break;
   case __F_NO_SAMPLES:
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

off_t
_opus_set(_fmt_t *fmt, int type, off_t value)
{
   _driver_t *handle = fmt->id;
   off_t rv = 0;

   switch(type)
   {
   case __F_BLOCK_SIZE:
      handle->blocksize = value;
      break;
   case __F_FREQUENCY:
      handle->frequency = value;
      break;
   case __F_RATE:
      handle->bitrate = value;
      break;
   case __F_TRACKS:
      handle->no_tracks = value;
      break;
   case __F_NO_SAMPLES:
      handle->no_samples = value;
      handle->max_samples = value;
      break;
   case __F_BITS_PER_SAMPLE:
      handle->bits_sample = value;
      break;
   case __F_IS_STREAM:
      break;
   case __F_POSITION:
      break;
   default:
      break;
   }
   return rv;
}

