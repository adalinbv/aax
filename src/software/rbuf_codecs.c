/*
 * Copyright 2005-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
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

#include <assert.h>
#include <stdlib.h>	/* free */

#include <base/types.h>
#include <ringbuffer.h>
#include <devices.h>
#include <arch.h>

#include "audio.h"
#include "arch_simd.h"

static _aaxCodec _sw_bufcpy_8s;
static _aaxCodec _sw_bufcpy_16s;
static _aaxCodec _sw_bufcpy_24s;
#if 0
static _aaxCodec _sw_bufcpy_32s;
#endif
static _aaxCodec _sw_bufcpy_mulaw;
static _aaxCodec _sw_bufcpy_alaw;

_aaxCodec *_oalRingBufferCodecs[AAX_FORMAT_MAX] =
{
   &_sw_bufcpy_8s,
   &_sw_bufcpy_16s,
   &_sw_bufcpy_24s,
   &_sw_bufcpy_24s,	/* 32-bit gets converted to 24-bit */
   &_sw_bufcpy_24s,	/* floats get converted to 24-bit  */
   &_sw_bufcpy_24s,	/* doubles get converted to 24-bit */
   &_sw_bufcpy_mulaw,
   &_sw_bufcpy_alaw,
#ifdef USE_IMA_ADPCM_CODEC
   &_sw_bufcpy_ima_adpcm
#else
   &_sw_bufcpy_16s	/* IMA4-ADPCM gets converted to 16-bit */
#endif
};

/*
 * This function transforms a buffer into a signed, 32-bit buffer ready
 * for the mixer using the provided 'codecfn' function.
 *
 * The funtion takes care of looping of the source buffer, making the
 * destination buffer a seamles and continuous chunk of data.
 *
 * The destination buffer must have the following layout:
 *     +----+------------------+----+
 *     | ddesamps | track data      |
 *     +----+------------------+----+
 *
 * @codecfn the function that transforms the data to singed, 16-bit.
 * @s pointers referencing the ss that contain the track data.
 * @spos starting sample position within the source track.
 * @sdesamps length (in samples) of the delay effects of the source buffer.
 * @loop_start sample position to start from when looping.
 * @sno_samples total length (in samples) of the source buffer.
 *              sno_samples gets adjusted for loop end point
 * @ddesamps length (in samples) of the delay effects of the destination.
 * @dno_samples total length (in samples) of the destination buffer.
 * @bps bytes per sample of the source s.
 * @no_tracks number of no_tracks in the source s.
 * @src_loops boolean, 0 = no srource looping, otherwise the source loops.
 */
void
_aaxProcessCodec(int32_t* d, void *s, _aaxCodec *codecfn, unsigned int src_pos,
                              unsigned int loop_start, unsigned int sno_samples,
                              unsigned int ddesamps, unsigned int dno_samples,
                              unsigned char sbps, char src_loops)
{
   static const int dbps = sizeof(int32_t);
   const unsigned int sbuflen = sno_samples - loop_start;
   const unsigned int dbuf_len = dno_samples;
   unsigned int dbuflen, new_len;
   int32_t *dptr = d;
   char *sptr;

   /*
    * calculate the actual starting position in the src and dst buffer
    * and fill dde samples of the source buffer
    */
   dbuflen = dbuf_len + ddesamps;
   if (src_loops)
   {
      if(src_pos > loop_start) 
      {
         /* if src_pos is large enough there is no need to fold back into
          * the source buffer 
          */
         if (src_pos >= (ddesamps+loop_start)) {
             src_pos -= ddesamps;
         }
         else
         {
            new_len = (ddesamps - (src_pos - loop_start)) % sbuflen;
            src_pos = loop_start + (sbuflen - new_len);
         }
         dbuflen += ddesamps;
         dptr -= ddesamps;
      }
   }

   /*
    * convert the number of samples that is still available in the
    * source buffer at the current source position
    */
   new_len = (dbuflen>(sno_samples-src_pos)) ? (sno_samples-src_pos) : dbuflen;
   if (src_pos < sno_samples)
   {
      sptr = (char *)s + src_pos*sbps;
      codecfn(dptr, sptr, sbps, new_len);
      dbuflen -= new_len;
      dptr += new_len;
   }

   if (dbuflen && src_loops)
   {
      /*
       * convert the remaining samples for the destination buffer or one
       * complete source buffer starting at the start of the source buffer
       */
      new_len = (dbuflen > sbuflen) ? sbuflen : dbuflen;
      sptr = (char *)s + loop_start*sbps;
      codecfn(dptr, sptr, sbps, new_len);

      dbuflen -= new_len;
      if (dbuflen)
      {
         int32_t *start_dptr = dptr;
         dptr += new_len;
         if (dbuflen > sbuflen)
         {
            /* copy the source buffer multiple times if needed */
            new_len = dbuflen;
            do
            {
               _aax_memcpy(dptr, start_dptr, sbuflen*dbps);
               new_len -= sbuflen;
               dptr += sbuflen;
            }
            while (new_len > sbuflen);

            /* and copy the remaining samples */
            if (new_len) {
               _aax_memcpy(dptr, start_dptr, new_len*dbps);
            }
         }
         else
         {
            sptr = (char *)s + loop_start*sbps;
            codecfn(dptr, sptr, sbps, dbuflen);
         }
      }
   }
   else if (dbuflen) {
      memset(dptr, 0, dbuflen*dbps);
   }
}

/* -------------------------------------------------------------------------- */

static void
_sw_bufcpy_8s(void *dst, const void *src, unsigned char sbps, unsigned int l)
{
   assert(sbps == 1);
   _batch_cvt24_8(dst, src, l);
}

static void
_sw_bufcpy_16s(void *dst, const void *src, unsigned char sbps, unsigned int l)
{
   assert(sbps == 2);
   _batch_cvt24_16(dst, src, l);
}

static void
_sw_bufcpy_24s(void *dst, const void *src, unsigned char sbps, unsigned int l)
{
   assert (sbps == 4);
   _batch_cvt24_24(dst, src, l);
}

#if 0
static void
_sw_bufcpy_32s(void *dst, const void *src, unsigned char sbps, unsigned int l)
{
   assert (sbps == 4);
   _batch_cvt24_32(dst, src, l);
}
#endif

static void
_sw_bufcpy_mulaw(void *dst, const void *src, unsigned char sbps, unsigned int l)
{
   if (sbps == 1)
   {
      int32_t *d = (int32_t *)dst;
      uint8_t *s = (uint8_t *)src;
      unsigned int i = l;
      do {
         *d++ = _mulaw2linear_table[*s++] << 8;
      }     
      while (--i);
   }
}

static void
_sw_bufcpy_alaw(void *dst, const void *src, unsigned char sbps, unsigned int l)
{
   if (sbps == 1)
   {
      int32_t *d = (int32_t *)dst;
      uint8_t *s = (uint8_t *)src;
      unsigned int i = l;
      do {
         *d++ = _alaw2linear_table[*s++] << 8;
      }
      while (--i);
   }
}

/*
 * decode one block of IMA4 nibbles, single channel only
 * only valid when src points to the start of an IMA block
 * l is the block length in samples;
 */
void
_sw_bufcpy_ima_adpcm(void *dst, const void *src, unsigned char sbps, unsigned int l)
{
   uint8_t *s = (uint8_t *)src;
   int16_t *d = (int16_t *)dst;
   int16_t predictor;
   uint8_t index;
   unsigned int i;

   predictor = *s++;
   predictor |= *s++ << 8;
   index = *s++;
   s++;

   i = l;
   do
   {
      uint8_t nibble = *s++;
      *d++ = ima2linear(nibble & 0xF, &predictor, &index);
      *d++ = ima2linear(nibble >> 4, &predictor, &index);
   }
   while (--i);
}


/* single sample convert */
#define _BIAS		0x80
#define _CLIP		32635
uint8_t
linear2mulaw(int16_t sample)
{
   int sign, exponent, mantissa;
   int rv;

   sign = (sample >> 8) & _BIAS;
   if (sign) {
      sample = (int16_t)-sample;
   }
   if (sample > _CLIP) {
      sample = _CLIP;
   }
   sample = (int16_t)(sample + 0x84);

   exponent = (int)_linear2mulaw_table[(sample>>7) & 0xFF];
   mantissa = (sample >> (exponent+3)) & 0x0F;
   rv = ~ (sign | (exponent << 4) | mantissa);

   return (uint8_t)rv;
}

/* single sample convert */
uint8_t
linear2alaw(int16_t sample)
{
     int sign, exponent, mantissa;
     uint8_t rv;

     sign = ((~sample) >> 8) & _BIAS;
     if (!sign) {
        sample = (int16_t)-sample;
     }
     if (sample > _CLIP) {
        sample = _CLIP;
     }
     if (sample >= 256)
     {
          exponent = (int)_linear2alaw_table[(sample >> 8) & 0x7F];
          mantissa = (sample >> (exponent + 3) ) & 0x0F;
          rv = ((exponent << 4) | mantissa);
     }
     else
     {
          rv = (uint8_t)(sample >> 4);
     }
     rv ^= (sign ^ 0x55);

     return rv;
}

/* single sample convert */
int16_t
ima2linear (uint8_t nibble, int16_t *val, uint8_t *idx)
{
  int32_t predictor;
  int16_t diff, step;
  int8_t delta, sign;
  int8_t index;

  index = _MINMAX(*idx, 0, 88);

  predictor = *val;
  step = _ima4_step_table[index];

  sign = nibble & 0x8;
  delta = nibble & 0x7;

  diff = 0;
  if (delta & 4) diff += step;
  if (delta & 2) diff += (step >> 1);
  if (delta & 1) diff += (step >> 2);
  diff += (step >> 3);

  if (sign) predictor -= diff;
  else predictor += diff;

  index += _ima4_index_table[nibble];
  *idx = _MINMAX(index, 0, 88);
  *val = _MINMAX(predictor, -32768, 32767);

  return *val;
}


/* single sample convert */
void
linear2ima(int16_t *val, int16_t nval, uint8_t *nbbl, uint8_t *idx)
{
   int16_t diff, ndiff, mask, step;
   uint8_t nibble, index = *idx;
   int32_t value = *val;

   nibble = 0;
   diff = nval - value;
   step = _ima4_step_table[index];
   ndiff = step >> 3;

   if(diff < 0)
   {
      nibble = 8;
      diff = -diff;
   }

   mask = 4;
   while (mask)
   {
      if (diff >= step)
      {
         nibble |= mask;
         ndiff += step;
         diff -= step;
      }
      step >>= 1;
      mask >>= 1;
   }

   if (nibble & 8) {
      value -= ndiff;
   } else {
      value += ndiff;
   }

   index += _ima4_index_table[nibble];
   *val = _MINMAX(value, -32767, 32767);
   *idx = _MINMAX(index, 0, 88);
   *nbbl = nibble;
}

static void
_aaxMuLaw2Linear(int32_t*ndata, uint8_t* data, unsigned int i)
{
   do {
      *ndata++ = _mulaw2linear_table[*data++] << 8;
   } while (--i);
}

static void
_aaxALaw2Linear(int32_t*ndata, uint8_t* data, unsigned int i)
{
   do {
      *ndata++ = _alaw2linear_table[*data++] << 8;
   } while (--i);
}

void
bufConvertDataToPCM24S(void *ndata, void *data, unsigned int samples, enum aaxFormat format)
{
   if (ndata)
   {
      switch(format)
      {
      case AAX_PCM8S:
         _batch_cvt24_8(ndata, data, samples);
         break;
      case AAX_IMA4_ADPCM:
         /* the ringbuffer uses AAX_PCM16S internally for AAX_IMA4_ADPCM */
      case AAX_PCM16S:
         _batch_cvt24_16(ndata, data, samples);
         break;
      case AAX_PCM32S:
         _batch_cvt24_32(ndata, data, samples);
         break;
      case AAX_FLOAT:
        _batch_cvt24_ps(ndata, data, samples);
         break;
      case AAX_DOUBLE:
         _batch_cvt24_pd(ndata, data, samples);
         break;
      case AAX_MULAW:
         _aaxMuLaw2Linear(ndata, data, samples);
         break;
      case AAX_ALAW:
         _aaxALaw2Linear(ndata, data, samples);
         break;
      default:
         break;
      }
   } /* ndata */
}

static void
_aaxLinear2MuLaw(uint8_t* ndata, int32_t* data, unsigned int i)
{
   do {
      *ndata++ = linear2mulaw(*data++ >> 8);
   } while (--i);
}

static void
_aaxLinear2ALaw(uint8_t* ndata, int32_t* data, unsigned int i)
{
   do {
      *ndata++ = linear2alaw(*data++ >> 8);
   } while (--i);
}

static void
_aaxLinear2IMABlock(uint8_t* ndata, int32_t* data, unsigned block_smp,
                   int16_t* sample, uint8_t* index, short step)
{
   unsigned int i;
   int16_t header;
   uint8_t nibble;

   header = *sample;
   *ndata++ = header & 0xff;
   *ndata++ = header >> 8;
   *ndata++ = *index;
   *ndata++ = 0;

   for (i=0; i<block_smp; i += 2)
   {
      int16_t nsample;

      nsample = *data >> 8;
      linear2ima(sample, nsample, &nibble, index);
      data += step;
      *ndata = nibble;

      nsample = *data >> 8;
      linear2ima(sample, nsample, &nibble, index);
      data += step;
      *ndata++ |= nibble << 4;
   }
}

/*
 * Incompatible with MS-IMA which specifies a different way of interleaving.
 */
static void
_aaxLinear2IMA4(uint8_t* ndata, int32_t* data, unsigned int samples, unsigned block_smp, unsigned tracks)
{
   unsigned int i, no_blocks, blocksize;
   int16_t sample = 0;
   uint8_t index = 0;

   no_blocks = samples/block_smp;
   blocksize = IMA4_SMP_TO_BLOCKSIZE(block_smp);

   for(i=0; i<no_blocks; i++)
   {
      _aaxLinear2IMABlock(ndata, data, block_smp, &sample, &index, tracks);
      ndata += blocksize*tracks;
      data += block_smp*tracks;
   }

   if (no_blocks*block_smp < samples)
   {
      unsigned int rest = (no_blocks+1)*block_smp - samples;

      samples = block_smp - rest;
      _aaxLinear2IMABlock(ndata, data, samples, &sample, &index, tracks);

      ndata += IMA4_SMP_TO_BLOCKSIZE(samples);
      memset(ndata, 0, rest/2);
   }
}

void
bufConvertDataFromPCM24S(void *ndata, void *data, unsigned int tracks, unsigned int no_samples, enum aaxFormat format, unsigned int blocksize)
{
   if (ndata)
   {
      unsigned int samples = tracks*no_samples;
      switch(format)
      {
      case AAX_PCM8S:
         _batch_cvt8_24(ndata, data, samples);
         break;
      case AAX_PCM16S:
         _batch_cvt16_24(ndata, data, samples);
         break;
      case AAX_PCM32S:
         _batch_cvt32_24(ndata, data, samples);
         break;
      case AAX_FLOAT:
         _batch_cvtps_24(ndata, data, samples);
         break;
      case AAX_DOUBLE:
         _batch_cvtpd_24(ndata, data, samples);
         break;
      case AAX_MULAW:
         _aaxLinear2MuLaw(ndata, data, samples);
         break;
      case AAX_ALAW:
         _aaxLinear2ALaw(ndata, data, samples);
         break;
      case AAX_IMA4_ADPCM:
      {
         int block_smp = BLOCKSIZE_TO_SMP(blocksize);
         unsigned t;
         for (t=0; t<tracks; t++)
         {
            uint8_t *dst = (uint8_t *)ndata + t*blocksize;
            int32_t *src = (int32_t *)data + t;
            _aaxLinear2IMA4(dst, src, no_samples, block_smp, tracks);
         }
         break;
      }
      default:
         break;
      }
   } /* ndata */
}
