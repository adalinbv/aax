/*
 * Copyright 2005-2016 by Erik Hofman.
 * Copyright 2009-2016 by Adalin B.V.
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

#include <string.h>	// memcpy

#include <base/types.h>

#include <software/audio.h>
#include <devices.h>
#include <arch.h>

#include "audio.h"
#include "format.h"


#define MSBLOCKSIZE_TO_SMP(b, t)	(((b)-4*(t))*2)/(t)
#define SMP_TO_MSBLOCKSIZE(s, t)	(((s)*(t)/2)+4*(t))

static size_t _batch_cvt24_adpcm_intl(_fmt_t*, int32_ptrptr, size_t, char_ptr, size_t, unsigned int, size_t*);
static void _batch_cvt24_alaw_intl(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
static void _batch_cvt24_mulaw_intl(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);

typedef struct
{
   _batch_cvt_proc cvt_to_signed;
   _batch_cvt_proc cvt_from_signed;
   _batch_cvt_proc cvt_endianness;
   _batch_cvt_to_intl_proc cvt_to_intl;
   _batch_cvt_from_intl_proc cvt_from_intl;

   size_t blocksize;
   int format;

   union
   {
      struct
      {
         size_t blockbufpos;
         int16_t predictor[_AAX_MAX_SPEAKERS];
         uint8_t index[_AAX_MAX_SPEAKERS];
      } read;
   } io;

} _driver_t;

int
_pcm_setup(_fmt_t *fmt, _fmt_type_t pcm_fmt, enum aaxFormat aax_fmt)
{
   _driver_t *handle = fmt->id;
   int rv = AAX_FALSE;

   fmt->id = calloc(1, sizeof(_driver_t));
   handle = fmt->id;

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
         rv = AAX_TRUE;
         break;
      default:
         _AAX_FILEDRVLOG("WAV: Unsupported format");
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

void
_pcm_close(_fmt_t *fmt)
{
   free(fmt->id);
}

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

size_t
_pcm_cvt_to_intl(_fmt_t *fmt, void_ptr dptr, const_int32_ptrptr sptr, size_t offset, unsigned int tracks, size_t num, void *scratch, size_t scratchlen)
{
   _driver_t *handle = fmt->id;
   if (handle->cvt_to_intl) {
      handle->cvt_to_intl(dptr, sptr, offset, tracks, num);
   }
   return num;
}

size_t
_pcm_process(_fmt_t *fmt, char_ptr dptr, void_ptr sptr, size_t offset, size_t num, size_t bytes)
{
   _driver_t *handle = fmt->id;

   if (handle->cvt_endianness) {
      handle->cvt_endianness(sptr, num);
   }
   if (handle->cvt_to_signed) {
      handle->cvt_to_signed(sptr, num);
   }

   memcpy(dptr+offset, sptr, bytes);

   return bytes;
}

size_t
_pcm_cvt_from_intl(_fmt_t *fmt, int32_ptrptr dptr, size_t dptr_offs, char_ptr buf, size_t buf_offs, unsigned int tracks, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t rv = *num;

   if (handle->format == AAX_IMA4_ADPCM)
   {
      rv = _batch_cvt24_adpcm_intl(fmt, dptr, dptr_offs, buf, buf_offs,
                                        tracks, num);
   }
   else
   {
      if (handle->cvt_from_intl) {
         handle->cvt_from_intl(dptr, buf, dptr_offs, tracks, rv);
      }
   }

   return rv;
}

size_t
_pcm_copy(_fmt_t *fmt, int32_ptr dptr, const_void_ptr sptr, size_t offset, size_t bytes)
{
   memcpy((char*)dptr+offset, sptr, bytes);
   return bytes;
}

int
_pcm_set(_fmt_t *fmt, int ptype, off_t param)
{
   _driver_t *handle = fmt->id;
   int rv = AAX_FALSE;
   switch(ptype)
   {
   case __F_BLOCK:
      handle->blocksize = param;
      break;
   case __F_POSITION:
      handle->io.read.blockbufpos = param;
      break;
   default:
      break;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

static size_t
_aaxWavMSADPCMBlockDecode(_driver_t *handle, int32_t **dptr, char_ptr src, size_t smp_offs, size_t num, size_t offset, unsigned int tracks)

{
   size_t offs_smp, rv = 0;
   unsigned t;

   for (t=0; t<tracks; t++)
   {
      int16_t predictor = handle->io.read.predictor[t];
      uint8_t index = handle->io.read.index[t];
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

      handle->io.read.predictor[t] = predictor;
      handle->io.read.index[t] = index;

      rv = num-l;
   }

   return rv;
}

static size_t
_batch_cvt24_adpcm_intl(_fmt_t *fmt, int32_ptrptr dptr, size_t offs, char_ptr src, size_t pos, unsigned int tracks, size_t *n)
{
   _driver_t *handle = fmt->id;
   size_t num = *n;
   size_t rv = 0;

   if (num && pos >= handle->blocksize)
   {
      size_t block_smp, decode_smp, offs_smp;
      size_t r;

      offs_smp = handle->io.read.blockbufpos;
      block_smp = MSBLOCKSIZE_TO_SMP(handle->blocksize, tracks);
      decode_smp = _MIN(block_smp-offs_smp, num);

      r = _aaxWavMSADPCMBlockDecode(handle, dptr, src, offs_smp, decode_smp, offs, tracks);
      handle->io.read.blockbufpos += r;
      *n = r;

      if (handle->io.read.blockbufpos >= block_smp)
      {
         handle->io.read.blockbufpos = 0;
         rv = handle->blocksize;
      }
   }
   else {
      *n = rv = 0;
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

