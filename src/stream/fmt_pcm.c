/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif

#include <base/databuffer.h>
#include <base/types.h>
#include <base/memory.h>

#include <software/audio.h>
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
   int block_size;
   int block_samps;
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

} _driver_t;

static void _batch_cvt24_alaw_intl(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
static void _batch_cvt24_mulaw_intl(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
static size_t _batch_cvt24_adpcm_intl(_driver_t*, int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);



int
_pcm_detect(UNUSED(_fmt_t *fmt), UNUSED(int mode))
{
   return true;
}

void*
_pcm_open(_fmt_t *fmt, int mode, void *buf, ssize_t *bufsize, UNUSED(size_t fsize))
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
         handle->block_samps = 1;
      }
      else {
         _AAX_FILEDRVLOG("PCM: Unable to create a handle");
      }
   }

   if (handle && buf && bufsize)
   {
      if (!handle->pcmBuffer) {
         handle->pcmBuffer = _aaxDataCreate(1, 16384, 1);
      }

      if (!handle->pcmBuffer)
      {
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
   int rv = false;

   switch(pcm_fmt)
   {
   case _FMT_PCM:
   {
      int need_endian_swap = false;
      int need_sign_swap = false;

      handle->format = aax_fmt;
      if ( ((handle->format & AAX_FORMAT_LE) && is_bigendian()) ||
           ((handle->format & AAX_FORMAT_BE) && !is_bigendian()) )
      {
         need_endian_swap = true;
      }

      if (handle->format & AAX_FORMAT_UNSIGNED) {
         need_sign_swap = true;
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
         rv = true;
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
         rv = true;
         break;
      case AAX_PCM24S_PACKED:
         handle->cvt_to_intl = _batch_cvt24_3intl_24;
         handle->cvt_from_intl = _batch_cvt24_24_3intl;
         if (need_endian_swap) {
            handle->cvt_endianness = _batch_endianswap24;
         }
         /* for the extension we act as 24-bit in 32-bit */
         if (!handle->copy_to_buffer)
         {
            handle->format = AAX_PCM24S;
            handle->bits_sample = 32;
            handle->block_size = handle->no_tracks*handle->bits_sample/8;
         }
         rv = true;
         break;
      case AAX_PCM24S:
         handle->cvt_to_intl = _batch_cvt24_intl_24;
         handle->cvt_from_intl = _batch_cvt24_24_intl;
         if (need_sign_swap)
         {
            handle->cvt_to_signed = _batch_cvt24u_24s;
            handle->cvt_from_signed = _batch_cvt24s_24u;
         }
         if (need_endian_swap) {
            handle->cvt_endianness = _batch_endianswap32;
         }
         rv = true;
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
         rv = true;
         break;
      case AAX_FLOAT:
         handle->cvt_to_intl = _batch_cvtps_intl_24;
         handle->cvt_from_intl = _batch_cvt24_ps_intl;
         if (need_endian_swap) {
            handle->cvt_endianness = _batch_endianswap32;
         }
         rv = true;
         break;
      case AAX_DOUBLE:
         handle->cvt_to_intl = _batch_cvtpd_intl_24;
         handle->cvt_from_intl = _batch_cvt24_pd_intl;
         if (need_endian_swap) {
            handle->cvt_endianness = _batch_endianswap64;
         }
         rv = true;
         break;
      case AAX_ALAW:
         handle->cvt_from_intl = _batch_cvt24_alaw_intl;
         rv = true;
         break;
      case AAX_MULAW:
         handle->cvt_from_intl = _batch_cvt24_mulaw_intl;
         rv = true;
         break;
      case AAX_IMA4_ADPCM:
//       if (handle->copy_to_buffer && handle->no_tracks == 1) {
            rv = true;
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
_pcm_fill(_fmt_t *fmt, void_ptr sptr, ssize_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t rv = __F_PROCESS;

   if (_aaxDataAdd(handle->pcmBuffer, 0, sptr, *bytes) == 0) {
      *bytes = 0;
   }

   return rv;
}

size_t
_pcm_copy(_fmt_t *fmt, int32_ptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bufsize, rv = __F_NEED_MORE;

   bufsize = _aaxDataGetDataAvail(handle->pcmBuffer, 0);
   if (dptr_offs == -1)
   {
      rv = _aaxDataMove(handle->pcmBuffer, 0, (char*)dptr, *num);
      *num = bufsize;
   }
   else if (bufsize)
   {
      int block_size = handle->block_size;
      int block_samps = handle->block_samps;
      size_t offs, bytes, no_blocks;

      if ((*num + handle->no_samples) > handle->max_samples) {
         *num = handle->max_samples - handle->no_samples;
      }

      if (*num)
      {
         no_blocks = *num/block_samps;
//       if (*num % block_samps) no_blocks++;

         bytes = no_blocks*block_size;
         if (bytes > bufsize)
         {
            no_blocks = (bufsize/block_size);
            bytes = no_blocks*block_size;
         }
         *num = no_blocks*block_samps;

         if (bytes)
         {
            offs = (dptr_offs/block_samps)*block_size;
            rv = _aaxDataMove(handle->pcmBuffer, 0, (char*)dptr+offs, bytes);
            handle->no_samples += *num;

            if (handle->no_samples >= handle->max_samples) {
               rv = __F_EOF;
            }
         }
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

   bufsize = _aaxDataGetDataAvail(handle->pcmBuffer, 0);
   if (bufsize)
   {
      char *buf = (char*)_aaxDataGetData(handle->pcmBuffer, 0);
//    size_t bufsize = _aaxDataGetDataAvail(handle->pcmBuffer, 0);
      int block_size = handle->block_size;
      int block_samps = handle->block_samps;
      int tracks = handle->no_tracks;

      if ((*num + handle->no_samples) > handle->max_samples) {
         *num = handle->max_samples - handle->no_samples;
      }

      if (*num)
      {
         if (handle->format == AAX_IMA4_ADPCM)
         {
            rv = _batch_cvt24_adpcm_intl(handle, dptr, buf, dptr_offs, tracks,
                                         *num);
            if (rv) {
               rv = _aaxDataMove(handle->pcmBuffer, 0, NULL, rv);
            }
            handle->no_samples += *num;
         }
         else
         {
            size_t bytes, no_blocks;

            no_blocks = *num/block_samps;
//          if (*num % block_samps) no_blocks++;

            bytes = no_blocks*block_size;
            if (bytes > bufsize)
            {
               no_blocks = (bufsize/block_size);
               bytes = no_blocks*block_size;
            }
            *num = no_blocks*block_samps;

            if (bytes)
            {
               if (handle->cvt_endianness) {
                  handle->cvt_endianness(buf, *num);
               }
               if (handle->cvt_from_signed) {
                  handle->cvt_from_signed(buf, *num);
               }
               if (handle->cvt_from_intl) {
                  handle->cvt_from_intl(dptr, buf, dptr_offs, tracks, *num);
               }

               /* skip processed data */
               rv = _aaxDataMove(handle->pcmBuffer, 0, NULL, bytes);
               handle->no_samples += *num;
            }
            else {
               *num = rv = 0;
            }
         }

         if (handle->no_samples >= handle->max_samples) {
            rv = __F_EOF;
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

   return *num*handle->block_size;
}

int
_pcm_set_name(_fmt_t *fmt, enum _aaxStreamParam param, const char *desc)
{
   return false;
}

char*
_pcm_name(UNUSED(_fmt_t *fmt), UNUSED(enum _aaxStreamParam param))
{
   return NULL;
}

float
_pcm_get(_fmt_t *fmt, int type)
{
   _driver_t *handle = fmt->id;
   float rv = 0.0f;

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
      rv = handle->block_size;
      break;
   case __F_NO_SAMPLES:
      rv = handle->max_samples;
      break;
   default:
      break;
   }
   return rv;
}

float
_pcm_set(_fmt_t *fmt, int type, float value)
{
   _driver_t *handle = fmt->id;
   float rv = 0.0f;

   switch(type)
   {
   case __F_FREQUENCY:
      handle->frequency = rv = value;
      break;
   case __F_BITRATE:
      handle->bitrate = rv = value;
      break;
   case __F_TRACKS:
      handle->no_tracks = rv = value;
      break;
   case __F_NO_SAMPLES:
      handle->max_samples = rv = value;
      break;
   case __F_BITS_PER_SAMPLE:
      handle->bits_sample = rv = value;
      break;
   case __F_BLOCK_SIZE:
      handle->block_size = rv = value;
      break;
   case __F_BLOCK_SAMPLES:
      handle->block_samps = rv = value;
      break;
   case __F_POSITION:
      handle->no_samples = rv = value;
      break;
   case __F_COPY_DATA:
      handle->copy_to_buffer = rv = value;
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

/*
 * convert one track of adpcm data
 * 8 samples per chunk of 4 bytes (int32_t) interleaved per track
 * https://wiki.multimedia.cx/index.php?title=Microsoft_IMA_ADPCM
 */
static size_t
_batch_cvt24_adpcm_track(_driver_t *handle, int32_ptrptr dptr, const_int32_ptr sptr, size_t offset, unsigned int t, unsigned int tracks, size_t num)
{
   int block_size = handle->block_size;
   int block_samps = handle->block_samps;
   int block_offs = handle->no_samples % block_samps;
   int32_t *d = dptr[t]+offset;
   const int32_t *src = sptr+t;
   const uint8_t *s = (const uint8_t*)src;
   size_t i = num;
   size_t rv = 0;

   if (block_offs)				/* start inside a block  */
   {						/* finish this block     */
      int16_t predictor = handle->predictor[t];
      uint8_t index = handle->index[t];
      int chunk_ctr = 4;			/* 4 bytes per chunk     */
      int chunk_offs;				/* offset within a chunk */
      int bytes_offs;

      chunk_offs = block_offs % 8;		/* 8 samples per chunk   */
      bytes_offs = chunk_offs/2;		/* two samples per byte  */
      chunk_ctr -= bytes_offs;

      src += tracks;				/* skip the block header */
      src += tracks*block_offs/8;		/* skip the data chunks  */
      s = (uint8_t*)src;
      s += bytes_offs;

      if (chunk_offs % 2)			/* offset is an odd number */
      {
         uint8_t nibble;

         nibble = *s++;
         *d++ = _adpcm2linear(nibble >> 4, &predictor, &index) << 8;
         if (--chunk_ctr == 0)
         {
            chunk_ctr = 4;
            src += tracks;
            s = (uint8_t*)src;
         }

         block_offs++;
         i--;
      }

      /* the rest of the samples in the block start at a byte boundary */
      while (i >= 2 && block_offs < block_samps)
      {
         uint8_t nibble = *s++;
         *d++ = _adpcm2linear(nibble & 0xF, &predictor, &index) << 8;
         *d++ = _adpcm2linear(nibble >> 4, &predictor, &index) << 8;

         if (--chunk_ctr == 0)
         {
            chunk_ctr = 4;
            src += tracks;
            s = (uint8_t*)src;
         }

         block_offs += 2;
         i -= 2;
      }

      if (block_offs == block_samps)
      {
         block_offs = 0;
         rv += block_size;
      }
      handle->predictor[t] = predictor;
      handle->index[t] = index;
   }

   /* decode remaining block(s) */
   while (i >= 2)
   {
      size_t chunk_ctr = 4;
      int16_t predictor;
      uint8_t index;

      predictor = *s++;
      predictor |= *s++ << 8;
      index = *s;

      src += tracks;				/* skip the header       */
      s = (uint8_t*)src;

      while (i >= 2 && block_offs < block_samps)
      {
         uint8_t nibble = *s++;
         *d++ = _adpcm2linear(nibble & 0xF, &predictor, &index) << 8;
         *d++ = _adpcm2linear(nibble >> 4, &predictor, &index) << 8;

         if (--chunk_ctr == 0)
         {
            chunk_ctr = 4;
            src += tracks;
            s = (uint8_t*)src;
         }

         block_offs += 2;
         i -= 2;
      }

      if (block_offs == block_samps)
      {
         block_offs = 0;
         rv += block_size;
      }
      handle->predictor[t] = predictor;
      handle->index[t] = index;
   }

   if (i)				/* no. samples was an odd number */
   {
      int16_t predictor = handle->predictor[t];
      uint8_t index = handle->index[t];
      uint8_t nibble = *s;

      *d++ = _adpcm2linear(nibble & 0xF, &predictor, &index) << 8;
      block_offs++;
      i--;

      if (block_offs == block_samps) {
         rv += block_size;
      }
      handle->predictor[t] = predictor;
      handle->index[t] = index;
   }

   return rv;
}

static size_t
_batch_cvt24_adpcm_intl(_driver_t *handle, int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   size_t rv = 0;
   unsigned t;

   for (t=0; t<tracks; t++) {
      rv +=_batch_cvt24_adpcm_track(handle, dptr, sptr, offset, t, tracks, num);
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

