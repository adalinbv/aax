/*
 * Copyright 2005-2015 by Erik Hofman.
 * Copyright 2009-2015 by Adalin B.V.
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

// https://android.googlesource.com/platform/external/libopus/+/refs/heads/master-soong/doc/trivial_example.c
#define FRAME_SIZE 960
#define MAX_FRAME_SIZE 6*960
#define MAX_PACKET_SIZE (3*1276)

DECL_FUNCTION(opus_decoder_create);
DECL_FUNCTION(opus_decoder_destroy);
DECL_FUNCTION(opus_decode_float);

DECL_FUNCTION(opus_encoder_create);
DECL_FUNCTION(opus_encoder_destroy);
DECL_FUNCTION(opus_encoder_ctl);
DECL_FUNCTION(opus_encode);

DECL_FUNCTION(opus_strerror);
DECL_FUNCTION(opus_get_version_string);

typedef struct
{
   void *id;
   void *audio;

   int mode;

   char capturing;

   uint8_t no_tracks;
   uint8_t bits_sample;
   int frequency;
   int bitrate;
   int blocksize;
   enum aaxFormat format;
   size_t no_samples;
   size_t max_samples;

   ssize_t opusBufPos;
   size_t opusBufSize;
   unsigned char *opusBuffer;
   char *opusptr;

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
_opus_open(_fmt_t *fmt, void *buf, size_t *bufsize, size_t fsize)
{
   _driver_t *handle = fmt->id;
   void *rv = NULL;

   assert(bufsize);

   if (handle)
   {
      if (!handle->opusptr)
      {
         char *ptr = 0;
 
         handle->opusBufPos = 0;
         handle->opusBufSize = 100*1024; // ;MAX_PACKET_SIZE;
         handle->opusptr = _aax_malloc(&ptr, handle->opusBufSize);
         handle->opusBuffer = (unsigned char*)ptr;

         handle->out_pos = 0;
         handle->out_size = MAX_FRAME_SIZE*handle->no_tracks*sizeof(float);
         handle->outputs = _aax_aligned_alloc(handle->out_size);
      }

      if (handle->opusptr)
      {
         if (handle->capturing)
         {
            if (!handle->id)
            {
               int err, tracks = handle->no_tracks;
               int32_t freq = 48000;

               handle->id = popus_decoder_create(freq, tracks, &err);
               handle->frequency = freq;

               if (!handle->id && popus_strerror)
               {
                  char s[1025];
                  snprintf(s, 1024, "OPUS: Unable to create a handle: %s",
                                     popus_strerror(err));
                  s[1024] = 0;
                  _aaxStreamDriverLog(NULL, 0, 0, s);
               }
            }

            if (handle->id) {
               _opus_fill(fmt, buf, bufsize);
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

      _aax_aligned_free(handle->outputs);
      free(handle->opusptr);
      free(handle);
   }
}

int
_opus_setup(_fmt_t *fmt, _fmt_type_t pcm_fmt, enum aaxFormat aax_fmt)
{
   return AAX_TRUE;
}

size_t
_opus_fill(_fmt_t *fmt, void_ptr sptr, size_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t bufpos, bufsize, size;
   size_t rv = 0;

   size = *bytes;
   bufpos = handle->opusBufPos;
   bufsize = handle->opusBufSize;
   if ((size+bufpos) <= bufsize)
   {
      unsigned char *buf = handle->opusBuffer + bufpos;

      memcpy(buf, sptr, size);
      handle->opusBufPos += size;
      rv = size;
   }
   else {
      *bytes = 0;
   }

   return rv;
}

size_t
_opus_copy(_fmt_t *fmt, int32_ptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bytes, bufsize, size = 0;
   unsigned int bits, tracks;
   size_t rv = __F_EOF;
   unsigned char *buf;
   int ret;

   tracks = handle->no_tracks;
   bits = handle->bits_sample;
   bytes = *num*tracks*bits/8;

   buf = handle->opusBuffer;
   bufsize = handle->opusBufSize;

   if (bytes > bufsize) {
      bytes = bufsize;
   }

   /*
    * -- about *num --
    * Number of samples per channel of available space in pcm. If this is less
    * than the maximum packet duration (120ms; 5760 for 48kHz), this function
    * will not be capable of decoding some packets. In the case of PLC
    * (data==NULL) or FEC (decode_fec=1), then frame_size needs to be exactly
    * the duration of audio that is missing, otherwise the decoder will not be
    * in the optimal state to decode the next incoming packet. For the PLC and
    * FEC cases, frame_size must be a multiple of 2.5 ms.
    */
// ret = popus_decode(handle->id, buf, bytes, (int16_t*)dptr, *num, 0);
ret = 0;
   if (ret >= 0)
   {
      unsigned int framesize = tracks*bits/8;

      *num = size/framesize;
      handle->no_samples += *num;

      rv = size;
   }
   return rv;
}

size_t
_opus_cvt_from_intl(_fmt_t *fmt, int32_ptrptr dptr, size_t offset, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bufsize, rv = 0;
   unsigned char *buf;
   int tracks, req;

   req = *num;
   tracks = handle->no_tracks;

   buf = handle->opusBuffer;
   bufsize = handle->opusBufPos;

   /* there is still data left in the buffer from the previous run */
   if (handle->out_pos > 0)
   {
      unsigned int pos = handle->out_pos*tracks;
      unsigned int max = _MIN(req, handle->out_size - pos);

printf("1. converting: %i\n", max);
      _batch_cvt24_ps_intl(dptr, handle->outputs+pos, offset, tracks, max);
      offset += max;
      handle->out_pos += max;
      handle->no_samples += max;
      if (handle->out_pos == handle->out_size) {
         handle->out_pos = 0;
      }
      req -= max;
      rv = max;
   }

   if (req > 0)
   {
      unsigned int packet_sz = handle->blocksize;
      unsigned int cvt = 0;
      float *outputs;

      /*
       * -- about frame_size --
       * Number of samples per channel of available space in pcm. If this is
       * less than the maximum packet duration (120ms; 5760 for 48kHz), this
       * function will not be capable of decoding some packets. In the case of
       * PLC (data==NULL) or FEC (decode_fec=1), then frame_size needs to be
       * exactly the duration of audio that is missing, otherwise the decoder
       * will not be n the optimal state to decode the next incoming packet.
       * For the PLC and FEC cases, frame_size must be a multiple of 2.5 ms.
       */
      outputs = handle->outputs;
      do
      {
         int n = popus_decode_float(handle->id, buf, packet_sz, outputs,
                                    MAX_FRAME_SIZE, 0);
printf("opus_decode: %i, requested: %i\n", n, req);
         if (n > 0)
         {
            if (n > req)
            {
               handle->out_pos = req;
               cvt = req;
               req = 0;
            }
            else
            {
               assert(handle->out_pos == 0);

               buf += packet_sz;
               bufsize -= packet_sz;
               outputs += n*tracks;

               cvt += n;
               req -= n;
            }
         }
         else
         {
            if (n < 0 && popus_strerror)
            {
               char s[81];
               snprintf(s, 80, "OPUS: Decoding error: %s", popus_strerror(n));
               s[80] = 0;
               _aaxStreamDriverLog(NULL, 0, 0, s);
            }
            break;
         }
      }
      while ((req > 0) && (bufsize >= handle->blocksize));

      if (cvt > 0)
      {
printf("2. converting: %i\n", cvt);
         handle->no_samples += cvt;
         _batch_cvt24_ps_intl(dptr, handle->outputs, offset, tracks, cvt);
         req -= cvt;
         rv += cvt;
      }
   }

   *num -= req;

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
                                  handle->opusBuffer, handle->opusBufSize);
   _aax_memcpy(dptr, handle->opusBuffer, res);

   return res;
}

char*
_opus_name(_fmt_t *fmt, enum _aaxStreamParam param)
{
   return NULL;
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

off_t
_opus_set(_fmt_t *fmt, int type, off_t value)
{
   _driver_t *handle = fmt->id;
   off_t rv = 0;

   switch(type)
   {
   case __F_BLOCK:
      handle->blocksize = value;
      break;
   case __F_FREQ:
      handle->frequency = value;
      break;
   case __F_RATE:
      handle->bitrate = value;
      break;
   case __F_TRACKS:
      handle->no_tracks = value;
      break;
   case __F_SAMPLES:
      handle->no_samples = value;
      handle->max_samples = value;
      break;
   case __F_BITS:
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

