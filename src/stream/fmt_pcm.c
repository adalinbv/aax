/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif


#include <base/types.h>

#include <software/audio.h>
#include <devices.h>
#include <arch.h>
#include <api.h>

#include "audio.h"
#include "device.h"


typedef struct
{
   int mode;

   char capturing;
   char copy_to_buffer;

   uint8_t no_tracks;
   uint8_t bits_sample;
   int frequency;
   int bitrate;
   unsigned int blocksize;
   int blocksmp;
   enum aaxFormat format;
   size_t no_samples;
   size_t max_samples;

   _data_t *pcmBuffer;

   /* data conversion */
   _batch_cvt_proc cvt_to_signed;
   _batch_cvt_proc cvt_from_signed;
   _batch_cvt_proc cvt_endianness;
   _batch_cvt_to_intl_proc cvt_to_intl;
   _batch_cvt_from_intl_proc cvt_from_intl;

   /* IMA */
   int16_t predictor[_AAX_MAX_SPEAKERS];
   uint8_t index[_AAX_MAX_SPEAKERS];
// _data_t *blockbuf;
   size_t blockbufpos;

} _driver_t;

static void _batch_cvt24_alaw_intl(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
static void _batch_cvt24_mulaw_intl(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
static size_t _batch_cvt24_adpcm_intl(_driver_t*, int32_t**, const_char_ptr, size_t, size_t, size_t, unsigned int);


int
_pcm_detect(_fmt_t *fmt, int mode)
{
   return AAX_TRUE;
}

void*
_pcm_open(_fmt_t *fmt, int mode, void *buf, UNUSED(size_t *bufsize), UNUSED(size_t fsize))
{
   _driver_t *handle = fmt->id;
   void *rv = NULL;

   if (!handle)
   {
      handle = fmt->id = calloc(1, sizeof(_driver_t));
      if (fmt->id)
      {
         handle->mode = mode;
         handle->capturing = (mode == 0) ? 1 : 0;
         handle->blocksmp = 1;
      }
      else {
         _AAX_FILEDRVLOG("PCM: Unable to create a handle");
      }
   }

   if (handle && buf && bufsize)
   {
      if (!handle->pcmBuffer) {
         handle->pcmBuffer = _aaxDataCreate(16384, 1);
      }

      if (handle->pcmBuffer) {
      }
      else {
         _AAX_FILEDRVLOG("PCM: Unable to allocate the audio buffer");
         rv = buf;   // try again
      }
   }

   return rv;
}

void
_pcm_close(_fmt_t *fmt)
{
   _driver_t *handle = fmt->id;

   if (handle)
   {
      _aaxDataDestroy(handle->pcmBuffer);
      free(handle);
   }
}

int
_pcm_setup(_fmt_t *fmt, _fmt_type_t pcm_fmt, enum aaxFormat aax_fmt)
{
   _driver_t *handle = fmt->id;
   int rv = AAX_FALSE;

   switch(pcm_fmt)
   {
   case _FMT_PCM:
   {
      int need_endian_swap = AAX_FALSE;
      int need_sign_swap = AAX_FALSE;

      handle->format = aax_fmt;
      if ( ((handle->format & AAX_FORMAT_LE) && is_bigendian()) ||
           ((handle->format & AAX_FORMAT_BE) && !is_bigendian()) )
      {
         need_endian_swap = AAX_TRUE;
      }

      if (handle->format & AAX_FORMAT_UNSIGNED) {
         need_sign_swap = AAX_TRUE;
      }

      switch (handle->format & AAX_FORMAT_NATIVE)
      {
      case AAX_PCM8S:
         handle->cvt_to_intl = _batch_cvt8_intl_24;
         handle->cvt_from_intl = _batch_cvt24_8_intl;
         if (need_sign_swap)
         {
            handle->cvt_to_signed = _batch_cvt8u_8s;
            handle->cvt_from_signed = _batch_cvt8s_8u;
         }
         rv = AAX_TRUE;
         break;
      case AAX_PCM16S:
         handle->cvt_to_intl = _batch_cvt16_intl_24;
         handle->cvt_from_intl = _batch_cvt24_16_intl;
         if (need_endian_swap) {
            handle->cvt_endianness = _batch_endianswap16;
         }
         if (need_sign_swap)
         {
            handle->cvt_to_signed = _batch_cvt16u_16s;
            handle->cvt_from_signed = _batch_cvt16s_16u;
         }
         rv = AAX_TRUE;
         break;
      case AAX_PCM24S:
         handle->cvt_to_intl = _batch_cvt24_3intl_24;
         handle->cvt_from_intl = _batch_cvt24_24_3intl;
         if (need_endian_swap) {
            handle->cvt_endianness = _batch_endianswap32;
         }
         if (need_sign_swap)
         {
            handle->cvt_to_signed = _batch_cvt32u_32s;
            handle->cvt_from_signed = _batch_cvt32s_32u;
         }
         rv = AAX_TRUE;
         break;
      case AAX_PCM32S:
         handle->cvt_to_intl = _batch_cvt32_intl_24;
         handle->cvt_from_intl = _batch_cvt24_32_intl;
         if (need_sign_swap)
         {
            handle->cvt_to_signed = _batch_cvt32u_32s;
            handle->cvt_from_signed = _batch_cvt32s_32u;
         }
         if (need_endian_swap) {
            handle->cvt_endianness = _batch_endianswap32;
         }
         rv = AAX_TRUE;
         break;
      case AAX_FLOAT:
         handle->cvt_to_intl = _batch_cvtps_intl_24;
         handle->cvt_from_intl = _batch_cvt24_ps_intl;
         if (need_endian_swap) {
            handle->cvt_endianness = _batch_endianswap32;
         }
         rv = AAX_TRUE;
         break;
      case AAX_DOUBLE:
         handle->cvt_to_intl = _batch_cvtpd_intl_24;
         handle->cvt_from_intl = _batch_cvt24_pd_intl;
         if (need_endian_swap) {
            handle->cvt_endianness = _batch_endianswap64;
         }
         rv = AAX_TRUE;
         break;
      case AAX_ALAW:
         handle->cvt_from_intl = _batch_cvt24_alaw_intl;
         rv = AAX_TRUE;
         break;
      case AAX_MULAW:
         handle->cvt_from_intl = _batch_cvt24_mulaw_intl;
         rv = AAX_TRUE;
         break;
      case AAX_IMA4_ADPCM:
//       if (handle->copy_to_buffer && handle->no_tracks == 1) {
            rv = AAX_TRUE;
//       }
         break;
      default:
            _AAX_FILEDRVLOG("PCM: Unsupported format");
         break;
      }
      break;
   }
   case _FMT_MP3:
   case _FMT_VORBIS:
   case _FMT_FLAC:
   default:
      break;
   }

   return rv;
}

size_t
_pcm_fill(_fmt_t *fmt, void_ptr sptr, size_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t rv = __F_PROCESS;

   if (_aaxDataAdd(handle->pcmBuffer, sptr, *bytes) == 0) {
      *bytes = 0;
   }

   return rv;
}

size_t
_pcm_copy(_fmt_t *fmt, int32_ptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bufsize, rv = __F_NEED_MORE;

   bufsize = handle->pcmBuffer->avail;
   if (bufsize)
   {
      unsigned int bufsize = handle->pcmBuffer->avail;
      unsigned int blocksize = handle->blocksize;
      unsigned int blocksmp = handle->blocksmp;
      size_t offs, bytes, n;

      if ((*num + handle->no_samples) > handle->max_samples) {
         *num = handle->max_samples - handle->no_samples;
      }

      if (*num)
      {
         n = *num/blocksmp;
         bytes = n*blocksize;
         if (bytes > bufsize)
         {
            n = (bufsize/blocksize);
            bytes = n*blocksize;
         }
         *num = n*blocksmp;

         offs = (dptr_offs/blocksmp)*blocksize;
         rv = _aaxDataMove(handle->pcmBuffer, (char*)dptr+offs, bytes);
         handle->no_samples += *num;
      }
   }
   else {
      *num = 0;
   }

   return rv;
}

size_t
_pcm_cvt_from_intl(_fmt_t *fmt, int32_ptrptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bufsize, rv = __F_NEED_MORE;

   bufsize = handle->pcmBuffer->avail;
   if (bufsize)
   {
      char *buf = (char*)handle->pcmBuffer->data;
      unsigned int blocksize = handle->blocksize;
      unsigned int tracks = handle->no_tracks;

      if ((*num + handle->no_samples) > handle->max_samples) {
         *num = handle->max_samples - handle->no_samples;
      }

      if (*num)
      {
         if (handle->format == AAX_IMA4_ADPCM)
         {
            if (handle->blocksize <= bufsize)
            {
               size_t block_smp = handle->blocksmp;
               size_t offs_smp = handle->blockbufpos;
               size_t decode_smp;

               decode_smp = _MIN(block_smp-offs_smp, *num);
               rv = _batch_cvt24_adpcm_intl(handle, dptr, buf, offs_smp,
                                       decode_smp, dptr_offs, tracks);
               handle->blockbufpos += rv;
               handle->no_samples += rv;
               *num = rv;

               if (handle->blockbufpos >= block_smp)
               {
                  handle->blockbufpos = 0;
                  _aaxDataMove(handle->pcmBuffer, NULL, handle->blocksize);
               }
            }
            else {
               *num = rv = 0;
            }
         }
         else
         {
            size_t bytes = *num*blocksize;
            if (bytes > bufsize) {
               bytes = bufsize;
            }

            *num = bytes/blocksize;

            if (handle->cvt_endianness) {
               handle->cvt_endianness(buf, *num);
            }
            if (handle->cvt_to_signed) {
               handle->cvt_to_signed(buf, *num);
            }

            if (handle->cvt_from_intl) {
               handle->cvt_from_intl(dptr, buf, dptr_offs, tracks, *num);
            }

            /* skip processed data */
            rv = _aaxDataMove(handle->pcmBuffer, NULL, bytes);
            handle->no_samples += *num;
         }
      }
   }
   else {
      *num = 0;
   }

   return rv;
}

size_t
_pcm_cvt_to_intl(_fmt_t *fmt, void_ptr dptr, const_int32_ptrptr sptr, size_t offset, size_t *num, UNUSED(void *scratch), UNUSED(size_t scratchlen))
{
   _driver_t *handle = fmt->id;
   int tracks = handle->no_tracks;

   handle->no_samples += *num;
   if (handle->cvt_to_intl) {
      handle->cvt_to_intl(dptr, sptr, offset, tracks, *num);
   }

   if (handle->cvt_to_signed) {
      handle->cvt_to_signed(dptr, *num * tracks);
   }

   if (handle->cvt_endianness) {
      handle->cvt_endianness(dptr, *num * tracks);
   }

   return *num*handle->blocksize;
}

char*
_pcm_name(UNUSED(_fmt_t *fmt), UNUSED(enum _aaxStreamParam param))
{
   return NULL;
}

off_t
_pcm_get(_fmt_t *fmt, int type)
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
      break;
   }
   return rv;
}

off_t
_pcm_set(_fmt_t *fmt, int type, off_t value)
{
   _driver_t *handle = fmt->id;
   off_t rv = 0;

   switch(type)
   {
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
      handle->max_samples = value;
      break;
   case __F_BITS_PER_SAMPLE:
      handle->bits_sample = value;
      break;
   case __F_BLOCK_SIZE:
      handle->blocksize = value;
      break;
   case __F_BLOCK_SAMPLES:
      handle->blocksmp = value;
      break;
   case __F_POSITION:
      handle->blockbufpos = value;
      break;
   case __F_COPY_DATA:
      handle->copy_to_buffer = value;
      break;
   default:
      break;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

void
_pcm_cvt_to_signed(_fmt_t *fmt, void_ptr dptr, size_t num)
{
   _driver_t *handle = fmt->id;
   if (dptr && handle->cvt_to_signed) {
      handle->cvt_to_signed(dptr, num);
   }
}

void
_pcm_cvt_from_signed(_fmt_t *fmt, void_ptr dptr, size_t num)
{
   _driver_t *handle = fmt->id;
   if (dptr && handle->cvt_from_signed) {
      handle->cvt_from_signed(dptr, num);
   }
}

void
_pcm_cvt_endianness(_fmt_t *fmt, void_ptr dptr, size_t num)
{
   _driver_t *handle = fmt->id;
   if (dptr && handle->cvt_endianness) {
      handle->cvt_endianness(dptr, num);
   }
}

void _pcm_cvt_lin_to_ima4_block(uint8_t* ndata, int32_t* data,
                                unsigned block_smp, int16_t* sample,
                                uint8_t* index, short step)
{
   unsigned int i;
   int16_t header;
   uint8_t nibble;

   header = *sample;
   *ndata++ = header & 0xFF;
   *ndata++ = header >> 8;
   *ndata++ = *index;
   *ndata++ = 0;

   for (i=0; i<block_smp; i += 2)
   {
      int16_t nsample;

      nsample = *data >> 8;
      _linear2adpcm(sample, nsample, &nibble, index);
      data += step;
      *ndata = nibble;

      nsample = *data >> 8;
      _linear2adpcm(sample, nsample, &nibble, index);
      data += step;
      *ndata++ |= nibble << 4;
   }
}

static size_t
_batch_cvt24_adpcm_intl(_driver_t *handle, int32_t **dptr, const_char_ptr src, size_t smp_offs, size_t num, size_t offset, unsigned int tracks)

{
   size_t offs_smp, rv = 0;
   unsigned t;

   for (t=0; t<tracks; t++)
   {
      int16_t predictor = handle->predictor[t];
      uint8_t index = handle->index[t];
      int32_t *d = dptr[t]+offset;
      int32_t *s = (int32_t*)src+t;
      size_t l, ctr = 4;
      uint8_t *sptr;

      l = num;
      offs_smp = smp_offs;
      if (!offs_smp)
      {
         sptr = (uint8_t*)s;                 /* read the block header */
         predictor = *sptr++;
         predictor |= *sptr++ << 8;
         index = *sptr;

         s += tracks;                        /* skip the header      */
         sptr = (uint8_t*)s;
      }
      else
      {
         /* 8 samples per chunk of 4 bytes (int32_t) */
         size_t offs_chunks = offs_smp/8;
         size_t offs_bytes;

         s += tracks;                        /* skip the header      */
         s += tracks*offs_chunks;            /* skip the data chunks */
         sptr = (uint8_t*)s;

         offs_smp -= offs_chunks*8;
         offs_bytes = offs_smp/2;            /* two samples per byte */

         sptr += offs_bytes;                 /* add remaining offset */
         ctr -= offs_bytes;

         offs_smp -= offs_bytes*2;           /* skip two-samples (bytes) */
         offs_bytes = offs_smp/2;

         sptr += offs_bytes;
         if (offs_smp)                       /* offset is an odd number */
         {
            uint8_t nibble = *sptr++;
            *d++ = _adpcm2linear(nibble >> 4, &predictor, &index) << 8;
            if (--ctr == 0)
            {
               ctr = 4;
               s += tracks;
               sptr = (uint8_t*)s;
            }
         }
      }

      if (l >= 2)                    /* decode the (rest of the) blocks  */
      {
         do
         {
            uint8_t nibble = *sptr++;
            *d++ = _adpcm2linear(nibble & 0xF, &predictor, &index) << 8;
            *d++ = _adpcm2linear(nibble >> 4, &predictor, &index) << 8;
            if (--ctr == 0)
            {
               ctr = 4;
               s += tracks;
               sptr = (uint8_t*)s;
            }
            l -= 2;
         }
         while (l >= 2);
      }

      if (l)                         /* no. samples was an odd number */
      {
         uint8_t nibble = *sptr;
         *d++ = _adpcm2linear(nibble & 0xF, &predictor, &index) << 8;
         l--;
      }

      handle->predictor[t] = predictor;
      handle->index[t] = index;

      rv = num-l;
   }

   return rv;
}

static void
_batch_cvt24_mulaw_intl(int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      size_t t;
      for (t=0; t<tracks; t++)
      {
         int8_t *s = (int8_t *)sptr + t;
         int32_t *d = dptr[t] + offset;
         size_t i = num;

         do
         {
            *d++ = _mulaw2linear(*s) << 8;
            s += tracks;
         }
         while (--i);
      }
   }
}

static void
_batch_cvt24_alaw_intl(int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      size_t t;
      for (t=0; t<tracks; t++)
      {
         int8_t *s = (int8_t *)sptr + t;
         int32_t *d = dptr[t] + offset;
         size_t i = num;

         do
         {
            *d++ = _alaw2linear(*s) << 8;
            s += tracks;
         }
         while (--i);
      }
   }
}

