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

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
#endif

#include <base/types.h>

#include <backends/driver.h>
#include <ringbuffer.h>
#include <arch.h>

#include "audio.h"
#include "cpu/arch2d_simd.h"

static void _sw_bufcpy_8s(void_ptr, const_void_ptr, size_t);
static void _sw_bufcpy_16s(void_ptr, const_void_ptr, size_t);
static void _sw_bufcpy_24s(void_ptr, const_void_ptr, size_t);
static void _sw_bufcpy_mulaw(void_ptr, const_void_ptr, size_t);
static void _sw_bufcpy_alaw(void_ptr, const_void_ptr, size_t);

_batch_codec_proc _aaxRingBufferCodecs[AAX_FORMAT_MAX] =
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
   &_sw_bufcpy_16s,	/* IMA4-ADPCM gets converted to 16-bit */
#endif
   &_sw_bufcpy_24s	/* 24-bit packed gets converted to 24-bit */
};

extern const int16_t _ima4_step_table[89];
extern const int16_t _ima4_index_table[16];

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
 * @src source data buffer
 * @codecfn the function that transforms the data to singed, 16-bit.
 * @src_pos starting sample position within the source track.
 * @loop_start sample position to start from when looping.
 * @sno_samples total length (in samples) of the source buffer.
 *              sno_samples gets adjusted for loop end point
 * @ddesamps length (in samples) of the delay effects of the destination.
 * @dno_samples total length (in samples) of the destination buffer.
 * @sbps bytes per sample of the source s.
 * @src_loops boolean, 0 = no srource looping, otherwise the source loops.
 */
void
_aaxRingBufferProcessCodec(int32_t *dst, void *src, _batch_codec_proc codecfn,
                 size_t src_pos, size_t loop_start, size_t sno_samples,
                 size_t ddesamps, size_t dno_samples, unsigned char sbps,
                 char src_loops)
{
   static const int dbps = sizeof(int32_t);
   unsigned int sdebytes = ddesamps*sbps;
   const size_t sbuflen = sno_samples - loop_start;
   size_t new_len, dbuflen = dno_samples;
   char *sptr, *s = (char*)src - sdebytes;
   int32_t *dptr = dst - ddesamps;

   assert(src_pos <= sno_samples);

   if (dbuflen >= (sno_samples-src_pos)) {
      new_len = sno_samples - src_pos;
   } else {
      new_len = dbuflen;
   }

   /*
    * convert the number of samples that is still available in the
    * source buffer at the current source position
    */
   sptr = s + src_pos*sbps;
   assert(sptr+(new_len+ddesamps)*sbps <= s+sno_samples*sbps);
   assert(dptr+new_len+ddesamps <= dst+dno_samples);

   codecfn(dptr, sptr, new_len+ddesamps);
   dbuflen -= new_len;

   if (dbuflen && src_loops)
   {
      dptr += (new_len + ddesamps);
      s += sdebytes;

      /*
       * convert the remaining samples for the destination buffer or one
       * complete source buffer starting at the start of the source buffer
       */
      new_len = (dbuflen > sbuflen) ? sbuflen : dbuflen;
      sptr = s + loop_start*sbps;
      assert(sptr+new_len*sbps <= s+sno_samples*sbps);
      assert(dptr+new_len <= dst+dno_samples);

      codecfn(dptr, sptr, new_len);

      dbuflen -= new_len;
      if (dbuflen)
      {
         int32_t *start_dptr = dptr;
         dptr += new_len;
         if (dbuflen > sbuflen)
         {
            /* copy the source buffer multiple times if required */
            new_len = dbuflen;
            do
            {
               assert(start_dptr+sbuflen <= dst+dno_samples);
               _aax_memcpy(dptr, start_dptr, sbuflen*dbps);
               new_len -= sbuflen;
               dptr += sbuflen;
            }
            while(new_len > sbuflen);

            /* and copy the remaining samples */
            if (new_len)
            {
               assert(start_dptr+new_len <= dst+dno_samples);
               _aax_memcpy(dptr, start_dptr, new_len*dbps);
            }
         }
         else
         {
            sptr = s + loop_start*sbps;
            assert(sptr+dbuflen*sbps <= s+sno_samples*sbps);
            assert(dptr+dbuflen <= dst+dno_samples);

            codecfn(dptr, sptr, dbuflen);
         }
      }
   }
}

/* -------------------------------------------------------------------------- */

static void
_sw_bufcpy_8s(void *dst, const void *src, size_t l)
{
   _batch_cvt24_8(dst, src, l);
}

static void
_sw_bufcpy_16s(void *dst, const void *src, size_t l)
{
   _batch_cvt24_16(dst, src, l);
}

static void
_sw_bufcpy_24s(void *dst, const void *src, size_t l)
{
   _batch_cvt24_24(dst, src, l);
}

/** http://docs.freeswitch.org/g711_8h_source.html */
#define ULAW_BIAS       0x84
int16_t
_mulaw2linear(uint8_t ulaw)
{
   int t;

   /* Complement to obtain normal u-law value. */
   ulaw = ~ulaw;

   /*
    * Extract and bias the quantization bits. Then
    * shift up by the segment number and subtract out the bias.
    */
   t = (((ulaw & 0x0F) << 3) + ULAW_BIAS) << (((int) ulaw & 0x70) >> 4);
   return (int16_t) ((ulaw & 0x80) ? (ULAW_BIAS - t) : (t - ULAW_BIAS));
}

static void
_sw_bufcpy_mulaw(void *dst, const void *src, size_t l)
{
   int32_t *d = (int32_t *)dst;
   uint8_t *s = (uint8_t *)src;
   size_t i = l;
   do {
      /*
       * Lookup tables for A-law and u-law look attractive, until you
       * consider the impact on the CPU cache. If it causes a substantial
       * area of your processor cache to get hit too often, cache sloshing
       * will severely slow things down.
       */
      *d++ = _mulaw2linear(*s++) << 8;
   }     
   while(--i);
}

#define ALAW_AMI_MASK   0x55
int16_t
_alaw2linear(uint8_t alaw)
{
   int i;
   int seg;

   alaw ^= ALAW_AMI_MASK;
   i = ((alaw & 0x0F) << 4);
   seg = (((int) alaw & 0x70) >> 4);
   if (seg)           
      i = (i + 0x108) << (seg - 1);
   else               
      i += 8;
   return (int16_t) ((alaw & 0x80) ? i : -i);
}

static void
_sw_bufcpy_alaw(void *dst, const void *src, size_t l)
{
   int32_t *d = (int32_t *)dst;
   uint8_t *s = (uint8_t *)src;
   size_t i = l;
   do {
      /*
       * Lookup tables for A-law and u-law look attractive, until you
       * consider the impact on the CPU cache. If it causes a substantial
       * area of your processor cache to get hit too often, cache sloshing
       * will severely slow things down.
       */
      *d++ = _alaw2linear(*s++) << 8;
   }
   while (--i);
}

/*
 * decode one block of IMA4 nibbles, single channel only
 * only valid when src points to the start of an IMA block
 * l is the block length in samples;
 */
void
_sw_bufcpy_ima_adpcm(void *dst, const void *src, size_t l)
{
   uint8_t *s = (uint8_t *)src;
   int16_t *d = (int16_t *)dst;
   int16_t predictor;
   uint8_t index;
   size_t i;

   predictor = *s++;
   predictor |= *s++ << 8;
   index = *s++;
   s++;

   i = l;
   do
   {
      uint8_t nibble = *s++;
      *d++ = _adpcm2linear(nibble & 0xF, &predictor, &index);
      *d++ = _adpcm2linear(nibble >> 4, &predictor, &index);
   }
   while (--i);
}


/* single sample convert */
/** http://docs.freeswitch.org/g711_8h_source.html */
int16_t
top_bit(uint16_t bits)
{
   int i = 0;

#if defined(__i386__)
   ASM ("movl $-1, %%edx;\n"		\
        "bsrl %%eax, %%edx;\n"		\
            : "=d" (i)			\
            : "a" (bits));
#elif defined(__x86_64__)
   ASM ("movq $-1, %%rdx;\n"		\
        "bsrq %%rax, %%rdx;\n"		\
            : "=d" (i)			\
            : "a" (bits));
#else
   if  (bits == 0)
      return -1;

   if  (bits & 0xFFFF0000)
   {
      bits &= 0xFFFF0000;
      i += 16;
   }
   if (bits & 0xFF00FF00)
   {
      bits &= 0xFF00FF00;
      i += 8;
   }
   if (bits & 0xF0F0F0F0)
   {
      bits &= 0xF0F0F0F0;
      i += 4;
   }
   if (bits & 0xCCCCCCCC)
   {
      bits &= 0xCCCCCCCC;
      i += 2;
   }
   if (bits & 0xAAAAAAAA)
   {
      bits &= 0xAAAAAAAA;
      i += 1;
   }
#endif
   return i;
}

/** http://docs.freeswitch.org/g711_8h_source.html */
uint8_t
_linear2mulaw(int16_t linear)
{
   uint8_t u_val;
   int mask;
   int seg;

   /* Get the sign and the magnitude of the value. */
   if (linear < 0)
   {
      linear = ULAW_BIAS - linear;
      mask = 0x7F;
   }
   else
   {
      linear = ULAW_BIAS + linear;
      mask = 0xFF;
   }

   seg = top_bit(linear | 0xFF) - 7;

   /*
    * Combine the sign, segment, quantization bits,
    * and complement the code word.
    */
   if (seg >= 8) {
      u_val = (uint8_t)(0x7F ^ mask);
   } else {
      u_val = (uint8_t)(((seg << 4) | ((linear >> (seg+3)) & 0xF))^mask);
   }

#ifdef ULAW_ZEROTRAP
   /* Optional ITU trap */
   if (u_val == 0) {
           u_val = 0x02;
   }
#endif

   return u_val;
}

/** http://docs.freeswitch.org/g711_8h_source.html */
#define ALAW_AMI_MASK		0x55
uint8_t
_linear2alaw(int16_t linear)
{
   int mask;
   int seg;

   if (linear >= 0)
   {
      /* Sign (bit 7) bit = 1 */
      mask = ALAW_AMI_MASK | 0x80;
   }
   else
   {
      /* Sign (bit 7) bit = 0 */
      mask = ALAW_AMI_MASK;
      linear = -linear - 8;
   }

   /* Convert the scaled magnitude to segment number. */
   seg = top_bit(linear | 0xFF) - 7;
   if (seg >= 8)
   {
      if (linear >= 0)
      {
         /* Out of range. Return maximum value. */
         return (uint8_t)(0x7F ^ mask);
      }
      /* We must be just a tiny step below zero */
      return (uint8_t)(0x00 ^ mask);
   }
   /* Combine the sign, segment, and quantization bits. */
   return (uint8_t)(((seg << 4) | ((linear >> ((seg) ? (seg+3) : 4)) & 0x0F))^ mask);
}

/* single sample convert */
int16_t
_adpcm2linear(uint8_t nibble, int16_t *val, uint8_t *idx)
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

  diff = (step >> 3);
  if (delta & 4) diff += step;
  if (delta & 2) diff += (step >> 1);
  if (delta & 1) diff += (step >> 2);

  if (sign) predictor -= diff;
  else predictor += diff;

  index += _ima4_index_table[nibble];
  *idx = _MINMAX(index, 0, 88);
  *val = _MINMAX(predictor, -32768, 32767);

  return *val;
}


/* single sample convert */
void
_linear2adpcm(int16_t *val, int16_t nval, uint8_t *nbbl, uint8_t *idx)
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
   while(mask)
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
   *idx = _MIN(index, 88);
   *nbbl = nibble;
}

