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

   ssize_t vorbisBufPos;
   size_t vorbisBufSize;
   unsigned char *vorbisBuffer;
   void *vorbisptr;

} _driver_t;


#define FRAME_SIZE		4096


int
_vorbis_detect(_fmt_t *fmt, int mode)
{
   void *audio = NULL;
   int rv = AAX_FALSE;

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
#if 0
      vorbis_encoder_destroy(handle->id);
      handle->id = NULL;

      _aax_aligned_free(handle->pcm);
      free(handle->vorbisptr);
      free(handle);
#endif
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

   bufpos = handle->vorbisBufPos;
   bufsize = handle->vorbisBufSize;
   if ((*bytes+bufpos) <= bufsize)
   {
      unsigned char *buf = handle->vorbisBuffer + bufpos;

      memcpy(buf, sptr, *bytes);
      handle->vorbisBufPos += *bytes;
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

   buf = handle->vorbisBuffer;
   bufsize = handle->vorbisBufSize;

   if (bytes > bufsize) {
      bytes = bufsize;
   }

#if 0
   ret = vorbis_decode(handle->id, buf, bytes, (int16_t*)dptr, *num, decode_fec);
   if (ret >= 0)
   {
      unsigned int framesize = tracks*bits/8;

      *num = size/framesize;
      handle->no_samples += *num;

      rv = size;
   }
#endif
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

   buf = handle->vorbisBuffer;
   bufsize = handle->vorbisBufSize;

   if (bytes > bufsize) {
      bytes = bufsize;
   }

#if 0
   /* see the comments in _vorbis_copy() for *num */
   ret = vorbis_decode(handle->id, buf, bytes, handle->pcm, *num, decode_fec);
   if (ret > 0)
   {
      unsigned int framesize = tracks*bits/8;

      *num = size/framesize;
      _batch_cvt24_16_intl(dptr, handle->pcm, offset, tracks, *num);

      handle->no_samples += *num;
      rv = size;
   }
#endif
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

#if 0
   res = vorbis_encode(handle->id, scratch, *num,
                                  handle->vorbisBuffer, handle->vorbisBufSize);
   _aax_memcpy(dptr, handle->vorbisBuffer, res);
#endif

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

