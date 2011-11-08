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


#include <math.h>	/* rinft */

#include "arch_simd.h"

void
_batch_fmadd_cpu(int32_ptr d, const int32_ptr sptr, unsigned int num, float f, float fstep)
{
   int32_t* s = (int32_t* )sptr;
   unsigned int i = num;
   int v = f*1024.0;

   do {
      *d++ += ((*s++ >> 2) * v) >> 8;
   }
   while (--i);
}

void
_batch_mul_value_cpu(void* data, unsigned bps, unsigned int num, float f)
{
   unsigned int i = num;
   switch (bps)
   {
   case 1:
   {
      int8_t* d = (int8_t*)data;
      do {
         *d++ *= f;
      }
      while (--i);
      break;
   }
   case 2:
   {
      int16_t* d = (int16_t*)data;
      do {
         *d++ *= f;
      }
      while (--i);
      break;
   }
   case 4:
   {
      int32_t* d = (int32_t*)data;
      do {
         *d++ *= f;
      }
      while (--i);
      break;
   }
   default:
      break;
   }
}

void
_batch_cvt32_24_cpu(int32_t*__restrict d, const void*__restrict sptr, unsigned int num)
{
   int32_t *s = (int32_t *)sptr;
   unsigned int i = num;

   do {
      *d++ = *s++ >> 8;
   }
   while (--i);
}

void
_batch_cvt24_32_cpu(void*__restrict dptr, const int32_t*__restrict sptr, unsigned int num)
{
   int32_t *d = (int32_t *)dptr;
   int32_t *s = (int32_t *)sptr;
   unsigned int i = num;

   do {
      *d++ = *s++ << 8;
   }
   while (--i);
}

void
_batch_cvtps_24_cpu(int32_t*__restrict d, const void*__restrict sptr, unsigned int num)
{
   static const float mul = (float)(1<<23);
   float* s = (float*)sptr;
   unsigned int i = num;

   do {
      *d++ = (int32_t)(*s++ * mul);
   } while (--i);
}

void
_batch_cvt24_ps_cpu(void*__restrict dst, const int32_t*__restrict sptr, unsigned int num)
{
   static const float mul = 1.0/(float)(1<<23);
   int32_t* s = (int32_t*)sptr;
   float* d = (float*)dst;
   unsigned int i = num;

   do {
      *d++ = (float)*s++ * mul;
   } while (--i);
}

void
_batch_cvtpd_24_cpu(int32_t*__restrict d, const void*__restrict sptr, unsigned int num)
{
   static const double mul = (double)(1<<23);
   double* s = (double*)sptr;
   unsigned int i = num;

   do {
      *d++ = (int32_t)(*s++ * mul);
   } while (--i);
}

void
_batch_cvt32_24_intl_cpu(int32_t**__restrict dptr, const void*__restrict sptr, unsigned int tracks, unsigned int num)
{
   unsigned int t;
   for (t=0; t<tracks; t++)
   {
      int32_t *s = (int32_t *)sptr + t;
      int32_t *d = dptr[t];
      unsigned int i = num;

      do {
         *d++ = *s >> 8;
         s += tracks;
      }
      while (--i);
   }
}

void
_batch_cvtps_24_intl_cpu(int32_t**__restrict dptr, const void*__restrict sptr, unsigned int tracks, unsigned int num)
{
   static const float mul = (float)(1<<23);
   unsigned int t;

   for (t=0; t<tracks; t++)
   {
      float *s = (float*)sptr + t;
      int32_t *d = dptr[t];
      unsigned int i = num;

      do {
         *d++ = (int32_t)(*s * mul);
         s += tracks;
      } while (--i);
   }
}

void
_batch_cvtpd_24_intl_cpu(int32_t**__restrict dptr, const void*__restrict sptr, unsigned int tracks, unsigned int num)
{
   static const double mul = (double)(1<<23);
   unsigned int t;
   for (t=0; t<tracks; t++)
   {
      double *s = (double*)sptr + t;
      int32_t *d = dptr[t];
      unsigned int i = num;

      do {
         *d++ = (int32_t)(*s * mul);
         s += tracks;
      } while (--i);
   }
}

void
_batch_cvt24_pd_cpu(void*__restrict dst, const int32_t*__restrict sptr, unsigned int num)
{
   static const double mul = 1.0/(double)(1<<23);
   int32_t* s = (int32_t*)sptr;
   double* d = (double*)dst;
   unsigned int i = num;
   do {
      *d++ = (double)*s++ * mul;
   } while (--i);
}

void
_batch_cvt8_24_cpu(int32_t*__restrict d, const void*__restrict sptr, unsigned int num)
{
   int8_t* s = (int8_t*)sptr;
   unsigned int i = num;

   do {
      *d++ = (int32_t)*s++ << 16;
   }
   while (--i);
}

void
_batch_cvt16_24_cpu(int32_t*__restrict d, const void*__restrict sptr, unsigned int num)
{
   int16_t* s = (int16_t*)sptr;
   unsigned int i = num;

   do {
      *d++ = (int32_t)*s++ << 8;
   }
   while (--i);
}

void
_batch_cvt24_8_cpu(void*__restrict dptr, const int32_t*__restrict sptr, unsigned int num)
{
   int32_t* s = (int32_t*)sptr;
   int8_t* d = (int8_t*)dptr;
   unsigned int i = num;

   do {
      *d++ = *s++ >> 16;
   }
   while (--i);
}

void
_batch_cvt24_16_cpu(void*__restrict dptr, const int32_t*__restrict sptr, unsigned int num)
{
   int32_t* s = (int32_t*)sptr;
   int16_t* d = (int16_t*)dptr;
   unsigned int i = num;

   do {
      *d++ = *s++ >> 8;
   }
   while (--i);
}

void
_batch_cvt24_16_intl_cpu(void*__restrict dptr, const int32_t**__restrict sptr, unsigned int offset, unsigned int tracks, unsigned int num)
{
   unsigned int t;

   for (t=0; t<tracks; t++)
   {
      int32_t *s = (int32_t *)sptr[t] + offset;
      int16_t *d = (int16_t *)dptr + t;
      unsigned int i = num;
       
      do
      {
         *d = *s++ >> 8;
         d += tracks;
      }
      while (--i);
   }
}

void
_batch_cvt24_ps_intl_cpu(void*__restrict d, const int32_t**__restrict s, unsigned int offset, unsigned int tracks, unsigned int num)
{
   static const float mul = 1.0/(float)(1<<23);
   unsigned int t;

   for (t=0; t<tracks; t++)
   {
      int32_t *sptr = (int32_t*)s[t] + offset;
      float *dptr = (float*)d + t;
      unsigned int i = num;

      do
      {
         *dptr = (float)*sptr++ * mul;
         dptr += tracks;
      }
      while (--i);
   }
}

void
_batch_cvt24_pd_intl_cpu(void*__restrict d, const int32_t**__restrict s, unsigned int offset, unsigned int tracks, unsigned int num)
{
   static const double mul = 1.0/(double)(1<<23);
   unsigned int t;

   for (t=0; t<tracks; t++)
   {
      int32_t *sptr = (int32_t *)s[t] + offset;
      double *dptr = (double*)d + t;
      unsigned int i = num;

      do 
      {
         *dptr = (double)*sptr++ * mul;
         dptr += tracks;
      }
      while (--i);
   }
}

void
_batch_saturate24_cpu(void *data, unsigned int no_samples)
{
   int32_t* p = (int32_t*)data;
   unsigned int i = no_samples;
   do
   {
      int32_t samp = _MINMAX(*p, -8388607, 8388607);
      *p++ = samp;
   }
   while(--i);
}

void
_batch_cvt8u_8s_cpu(void *data, unsigned int no_samples)
{
   int8_t* p = (int8_t*)data;
   unsigned int i = no_samples;

   do {
      *p++ -= (int8_t)127;
   } while (--i);
}

void
_batch_cvt8s_8u_cpu(void *data, unsigned int no_samples)
{
   uint8_t* p = (uint8_t*)data;
   unsigned int i = no_samples;

   do {
      *p++ += (uint8_t)127;
   } while (--i);
}

void
_batch_cvt16u_16s_cpu(void *data, unsigned int no_samples)
{
   int16_t* p = (int16_t*)data;
   unsigned int i = no_samples;

   do {
      *p++ -= (int16_t)32767;
   } while (--i);
}

void
_batch_cvt16s_16u_cpu(void *data, unsigned int no_samples)
{
   uint16_t* p = (uint16_t*)data;
   unsigned int i = no_samples;

   do {
      *p++ += (uint16_t)32767;
   } while (--i);
}

void
_batch_cvt24u_24s_cpu(void *data, unsigned int no_samples)
{
   int32_t* p = (int32_t*)data;
   unsigned int i = no_samples;

   do {
      *p++ -= (int32_t)8388607;
   } while (--i);
}

void
_batch_cvt24s_24u_cpu(void *data, unsigned int no_samples)
{
   uint32_t* p = (uint32_t*)data;
   unsigned int i = no_samples;

   do {
      *p++ += (uint32_t)8388607;
   } while (--i);
}

void
_batch_cvt32u_32s_cpu(void *data, unsigned int no_samples)
{
   int32_t* p = (int32_t*)data;
   unsigned int i = no_samples;

   do {
      *p++ -= (int32_t)2147483647;
   } while (--i);
}

void
_batch_cvt32s_32u_cpu(void *data, unsigned int no_samples)
{
   uint32_t* p = (uint32_t*)data;
   unsigned int i = no_samples;

   do {
      *p++ += (uint32_t)2147483647;
   } while (--i);
}

void
_batch_endianswap16_cpu(void* data, unsigned int num)
{
   int16_t* p = (int16_t*)data;
   unsigned int i = num;

   do
   {
      *p = _bswap16(*p);
      p++;
   }
   while (--i);
}

void
_batch_endianswap32_cpu(void* data, unsigned int num)
{
   int32_t* p = (int32_t*)data;
   unsigned int i = num;

   do
   {
      *p = _bswap32(*p);
      p++;
   }
   while (--i);
}

void
_batch_endianswap64_cpu(void* data, unsigned int num)
{
   int64_t* p = (int64_t*)data;
   unsigned int i = num;

   do
   {
      *p = _bswap64(*p);
      p++;
   }
   while (--i);
}

void
_batch_freqfilter_cpu(int32_ptr d, const int32_ptr sptr, unsigned int num, float *hist, float lfgain, float hfgain, float k, const float *cptr)
{
   int32_ptr s = (int32_ptr)sptr;
   float smp, nsmp, h0, h1;
   unsigned int i = num;

   h0 = hist[0];
   h1 = hist[1];
   do
   {
      smp = *s * k;
      smp = smp - h0 * cptr[0];
      nsmp = smp - h1 * cptr[1];
      smp = nsmp + h0 * cptr[2];
      smp = smp + h1 * cptr[3];

      h1 = h0;
      h0 = nsmp;
      *d++ = smp*lfgain + (*s-smp)*hfgain;
      s++;
   }
   while (--i);

   hist[0] = h0;
   hist[1] = h1;
}

/**
 * A mixer callback function mixes the audio from one mono source track and
 * the already existing audio in the mono destination track. The result is
 * presented in the destination buffer.
 *
 * @d 32-bit destination buffer
 * @s 32-bit source buffer
 * @dmin starting sample position in the dest buffer
 * @dmax last sample position in the dest buffer
 * @smin sample position where to return to in case of sample looping
 * @sdesamps starting sample position in the source buffer
 * @smu normalised starting position (between sdesamps and sdesamps+1)
 * @freq_factor stepsize in the src buffer for one step in the dest buffer.
 * @smax last sample position in the src buffer
 *
 * Note: smax is only used in the *Loop mixing functions
 */
void
_aaxBufResampleSkip_cpu(int32_ptr d, const int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   int32_ptr sptr = (int32_ptr)s;
   int32_ptr dptr = d;
   int32_t samp, dsamp;
   unsigned int i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor >= 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   sptr += sdesamps;
   dptr += dmin;

   samp = *sptr++;              // n+(step-1)
   dsamp = *sptr - samp;        // (n+1) - n

   i=dmax-dmin;
   do
   {
      int step;

      *dptr++ = samp + (dsamp * smu);

      smu += freq_factor;
      step = floorf(smu);

      smu -= step;
      sptr += step-1;
      samp = *sptr++;
      dsamp = *sptr - samp;
   }
   while (--i);
}

void
_aaxBufResampleNearest_cpu(int32_ptr d, const int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   if (freq_factor == 1.0f) {
      _aax_memcpy(d+dmin, s+sdesamps, (dmax-dmin)*sizeof(int32_t));
   }
   else
   {
      int32_ptr sptr = (int32_ptr)s;
      int32_ptr dptr = d;
      unsigned int i;

      assert(s != 0);
      assert(d != 0);
      assert(dmin < dmax);
      assert(0.95f <= freq_factor && freq_factor <= 1.05f);
      assert(0.0f <= smu && smu < 1.0f);

      sptr += sdesamps;
      dptr += dmin;

      i = dmax-dmin;
      do
      {
         *dptr++ = *sptr;

         smu += freq_factor;
         if (smu > 0.5f)
         {
            sptr++;
            smu -= 1.0f;
         }
      }
      while (--i);
   }
}

void
_aaxBufResampleLinear_cpu(int32_ptr d, const int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   int32_ptr sptr = (int32_ptr)s;
   int32_ptr dptr = d;
   int32_t samp, dsamp;
   unsigned int i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor < 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   sptr += sdesamps;
   dptr += dmin;

   samp = *sptr++;		// n
   dsamp = *sptr - samp;	// (n+1) - n

   i = dmax-dmin;
   do
   {
      *dptr++ = samp + (dsamp * smu);

      smu += freq_factor;
      if (smu >= 1.0f)
      {
         smu -= 1.0f;
         samp = *sptr++;
         dsamp = *sptr - samp;
      }
   }
   while (--i);

#if 0
printf("dptr: %x, d+dmax: %x, dptr-d: %i (%x)\n", dptr, d+dmax, dptr-d, samp);
for (i=0; i<dmax; i++)
   if (d[i] != 0x333300) printf("->d[%i] = %x\n", i, d[i]);
#endif
}

void
_aaxBufResampleCubic_cpu(int32_ptr d, const int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   float y0, y1, y2, y3, a0, a1, a2;
   int32_ptr sptr = (int32_ptr)s;
   int32_ptr dptr = d;
   unsigned int i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(0.0f <= smu && smu < 1.0f);
   assert(0.0f < freq_factor && freq_factor <= 1.0f);

   sptr += sdesamps;
   dptr += dmin;

   y0 = *sptr++;
   y1 = *sptr++;
   y2 = *sptr++;
   y3 = *sptr++;

   a0 = y3 - y2 - y0 + y1;
   a1 = y0 - y1 - a0;
   a2 = y2 - y0;

   i = dmax-dmin;
   do
   {
      float smu2, ftmp;

      smu2 = smu*smu;
      ftmp = (a0*smu*smu2 + a1*smu2 + a2*smu + y1);
      *dptr++ = (int32_t)ftmp;

      smu += freq_factor;
      if (smu >= 1.0f)
      {
         smu--;
#if 0
         /* original code */
         sptr -= 3;
         y0 = *sptr++;
         y1 = *sptr++;
         y2 = *sptr++;
         y3 = *sptr++;

         a0 = y3 - y2 - y0 + y1;
         a1 = y0 - y1 - a0;
         a2 = y2 - y0;
#else
         /* optimized code */
         a0 += y0;
         y0 = y1;
         y1 = y2;
         y2 = y3;
         y3 = *sptr++;
         a0 = -a0 + y3;			/* a0 = y3 - y2 - y0 + y1; */
         a1 = y0 - y1 - a0;
         a2 = y2 - y0;
#endif
      }
   }
   while (--i);
}

