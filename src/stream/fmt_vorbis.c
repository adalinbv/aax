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
#include "fmt_vorbis.h"

DECL_FUNCTION(vorbis_info_init);

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

   vorbis_comment   vc;
   vorbis_dsp_state vd;
   vorbis_block     vb;

} _driver_t;


int
_vorbis_detect(_fmt_t *fmt, int mode)
{
   void *audio = NULL;
   int rv = AAX_FALSE;

   audio = _aaxIsLibraryPresent("vorbis", "0");
   if (!audio) {
      audio = _aaxIsLibraryPresent("libvorbis", "0");
   }

   if (audio)
   {
      char *error;

      _aaxGetSymError(0);

      TIE_FUNCTION(vorbis_info_init)
      if (pvorbis_info_init)
      {
         TIE_FUNCTION();

         error = _aaxGetSymError(0);
         if (!error)
         {
            /* not required but useful */
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
               _AAX_FILEDRVLOG("VORBIS: Insufficient memory");
            }
         }
      }
   }

   return rv;
}

void*
_vorbis_open(_fmt_t *fmt, void *buf, size_t *bufsize, size_t fsize)
{
   _driver_t *handle = fmt->id;
   void *rv = NULL;

   assert(bufsize);

   if (handle)
   {
      if (!handle->id)
      {
         int err;
         handle->id = popus_decoder_create(handle->frequency, handle->no_tracks, &err);
         if (err > 0)
         {
            char *ptr = 0;

            handle->opusBufPos = 0;
            handle->opusBufSize = MAX_PACKET_SIZE;
            handle->opusptr = _aax_malloc(&ptr, handle->opusBufSize);
            handle->opusBuffer = (unsigned char*)ptr;

            handle->pcm = _aax_aligned_alloc16(MAX_FRAME_SIZE*handle->no_tracks);

            rv = buf;
         }
         else {
            _AAX_FILEDRVLOG("VORBIS: Unable to create a handle");
         }
      }
   }
   else
   {
      _AAX_FILEDRVLOG("VORBIS: Internal error: handle id equals 0");
   }

   return rv;
}

void
_vorbis_close(_fmt_t *fmt)
{
   _driver_t *handle = fmt->id;

   if (handle)
   {
      popus_encoder_destroy(handle->id);
      handle->id = NULL;

      _aax_aligned_free(handle->pcm);
      free(handle->opusptr);
      free(handle);
   }
}

int
_vorbis_setup(_fmt_t *fmt, _fmt_type_t pcm_fmt, enum aaxFormat aax_fmt)
{
   return AAX_TRUE;
}

size_t
_vorbis_fill(_fmt_t *fmt, void_ptr sptr, size_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t bufpos, bufsize;
   size_t rv = 0;

   bufpos = handle->opusBufPos;
   bufsize = handle->opusBufSize;
   if ((*bytes+bufpos) <= bufsize)
   {
      unsigned char *buf = handle->opusBuffer + bufpos;

      memcpy(buf, sptr, *bytes);
      handle->opusBufPos += *bytes;
      rv = *bytes;
   }

   return rv;
}

size_t
_vorbis_copy(_fmt_t *fmt, int32_ptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bytes, bufsize, size = 0;
   unsigned int bits, tracks;
   int ret, decode_fec = 0;
   size_t rv = __F_EOF;
   unsigned char *buf;

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
   ret = popus_decode(handle->id, buf, bytes, (int16_t*)dptr, *num, decode_fec);
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
_vorbis_cvt_from_intl(_fmt_t *fmt, int32_ptrptr dptr, size_t offset, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bytes, bufsize, size = 0;
   unsigned int bits, tracks;
   int ret, decode_fec = 0;
   size_t rv = __F_EOF;
   unsigned char *buf;

   tracks = handle->no_tracks;
   bits = handle->bits_sample;
   bytes = *num*tracks*bits/8;

   buf = handle->opusBuffer;
   bufsize = handle->opusBufSize;

   if (bytes > bufsize) {
      bytes = bufsize;
   }

   /* see the comments in _vorbis_copy() for *num */
   ret = popus_decode(handle->id, buf, bytes, handle->pcm, *num, decode_fec);
   if (ret > 0)
   {
      unsigned int framesize = tracks*bits/8;

      *num = size/framesize;
      _batch_cvt24_16_intl(dptr, handle->pcm, offset, tracks, *num);

      handle->no_samples += *num;
      rv = size;
   }
   return rv;
}

size_t
_vorbis_cvt_to_intl(_fmt_t *fmt, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
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
_vorbis_name(_fmt_t *fmt, enum _aaxStreamParam param)
{
   return NULL;
}

off_t
_vorbis_get(_fmt_t *fmt, int type)
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
_vorbis_set(_fmt_t *fmt, int type, off_t value)
{
   _driver_t *handle = fmt->id;
   off_t rv = 0;

   switch(type)
   {
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

