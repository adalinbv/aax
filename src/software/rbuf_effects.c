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

#include <assert.h>
#include <math.h>	/* tan */

#include <base/logging.h>

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>


static void szxform(float *, float *, float *, float *, float *, float *,
                    float, float, float *, float *);

// *(sptr-4) = 0; *(sptr+4) = 0;
#define WRITE(a, b, dptr, ds, no_samples) \
	if (a) { static int ct = 0; \
	  if (++ct > (b)) { \
*(dptr-4) = 0; *(dptr+4) = 0; \
             WRITE_BUFFER_TO_FILE(dptr-ds, ds+no_samples); } }


/**
 * - dst and scratch point to the beginning of a buffer containing room for
 *   the delay effects prior to the pointer.
 * - start is the starting pointer
 * - end is the end pointer (end-start is the number of smaples)
 * - dmax does not include ds
 */
#define BUFSWAP(a, b) do { void* t = (a); (a) = (b); (b) = t; } while (0);
void
bufEffectsApply(int32_ptr dst, int32_ptr src, int32_ptr scratch,
          unsigned int start, unsigned int end, unsigned int ddesamps,
          unsigned int track, void *freq, void *delay, void *distort)
{
   static const unsigned int bps = sizeof(int32_t);
   unsigned int ds = delay ? ddesamps : 0;
   unsigned int no_samples = end - start;
   int32_t *psrc = src;
   int32_t *pdst = dst;

   if (freq)				/* Apply frequency filter first */
   {
      bufFilterFrequency(pdst, psrc, start, end, ds, track, freq);
      BUFSWAP(pdst, psrc);
   }

   if (distort)				/* distortion is second */
   {
      bufEffectDistort(pdst, psrc , start, end, ds, track, distort);
      BUFSWAP(pdst, psrc);
   }  

#if !ENABLE_LITE
   if (delay)
   {
      _oalRingBufferDelayEffectData* effect = delay;
      unsigned int offs = effect->delay.sample_offs[track];

      /* Apply delay effects */
      if (effect->history_ptr) {	/*    flanging     */
         bufEffectDelay(psrc, psrc, scratch, start, end, ds, delay, track);
      }
      else				/* phasing, chorus */
      {
         unsigned int ext_no_samples = no_samples + offs;

         _aax_memcpy(pdst+start-offs, psrc+start-offs, ext_no_samples*bps);
         bufEffectDelay(pdst, psrc, scratch, start, end, offs, delay, track);
         BUFSWAP(pdst, psrc);
      }
   }
#endif

   if (dst == pdst) {	/* copy the data back to the dst buffer */
      _aax_memcpy(dst, src, no_samples*bps);
   }
}

#if !ENABLE_LITE
void
bufEffectReflections(int32_t* d, const int32_ptr s,
                        unsigned int dmin, unsigned int dmax,
                        unsigned int track, const void *data)
{
   const _oalRingBufferReverbData *reverb = data;
   unsigned int snum;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(track < _AAX_MAX_SPEAKERS);

   /* reverb (1st order reflections) */
   snum = reverb->no_delays;
   if (snum > 1)
   {
      const _oalRingBufferDelayInfo *delay = reverb->delay;
      const int32_ptr sptr = s + dmin;
      int32_t *dptr = d + dmin;
      unsigned int q = snum;

      do
      {
         float volume = delay[q].gain / snum;

         --q;
         if ((volume > 0.001f) || (volume < -0.001f))
         {
            unsigned int samples = delay[q].sample_offs[track];
            assert(samples < dmin);

            _batch_fmadd(dptr, sptr-samples, dmax-dmin, volume, 0.0f);
         }
      }
      while (q);
   }
   else
   {
      float volume = reverb->delay[0].gain;
      if ((volume >= 0.001f) || (volume <= -0.001f)) {
         _batch_fmadd(d+dmin, s+dmin, dmax-dmin, volume, 0.0f);
      }
   }
}

void
bufEffectReverb(int32_t *s,
                   unsigned int dmin, unsigned int dmax, unsigned int ds,
                   unsigned int track, const void *data)
{
   const _oalRingBufferReverbData *reverb = data;
   unsigned int snum;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(s != 0);
   assert(dmin < dmax);
   assert(track < _AAX_MAX_SPEAKERS);

   /* loopback for reverb (2nd order reflections) */
   snum = reverb->no_loopbacks;
   if (snum > 0)
   {
      int32_t *sptr = s + dmin + ds;
      unsigned int q = snum;

      _aax_memcpy(s, reverb->reverb_history[track], ds*sizeof(int32_t));
      do
      {
         unsigned int samples = reverb->loopback[q].sample_offs[track];
         float volume = -reverb->loopback[q].gain / (snum+1);

         --q;
         assert(samples < ds);

         _batch_fmadd(sptr, sptr-samples, dmax-dmin, volume, 0.0f);
      }
      while (q);
      _aax_memcpy(reverb->reverb_history[track], s+dmin-1, ds*sizeof(int32_t));
   }
}

/**
 * - d and s point to a buffer containing the delay effects buffer prior to
 *   the pointer.
 * - start is the starting pointer
 * - end is the end pointer (end-start is the number of smaples)
 * - dmax does not include ds
 */
void
bufEffectDelay(int32_ptr d, const int32_ptr s, int32_ptr scratch,
               unsigned int start, unsigned int end,
               unsigned int ds, void *data, unsigned int track)
{
   _oalRingBufferDelayEffectData* effect = data;
   unsigned int no_samples = end-start;
   float volume;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(s != 0);
   assert(d != 0);
   assert(start < end);
   assert(data != NULL);

   volume =  effect->delay.gain;
   if (volume > 0.001f || volume < -0.001f)
   {
      const int32_ptr src = s + start;
      int32_t *sptr = (int32_t*)src;
      int32_t *dptr = d + start;
      unsigned int offs, noffs;

      offs = effect->delay.sample_offs[track];
      assert(offs <= ds);

      noffs = effect->lfo.get(&effect->lfo, track);
      effect->delay.sample_offs[track] = noffs;

      if (s == d)	/*  flanging */
      {
         static const unsigned int bps = sizeof(int32_t);
         unsigned int i, coffs, doffs, step;
         int32_t *ptr, *ddeptr;
//       int32_t v1, v2;
         int sign;

         ptr = dptr;
         ddeptr = d - ds;
         _aax_memcpy(ddeptr, effect->reverb_history[track], ds*bps);

         doffs = abs(noffs - offs);
         step = doffs ? no_samples/doffs : no_samples;
         if (step < 2) {
            step = no_samples;
         }

         coffs = offs;
         sign = (noffs < offs) ? -1 : 1;
         i = no_samples;
         do
         {
            _batch_fmadd(ptr, ptr-coffs, step, volume, 0.0f);
#if 0
            v1 = *(ptr-1); v2 = v1*0.5f + *ptr*0.5f;
            *(ptr-1) = *(ptr-2)*0.5f + v1*0.2f + v2*0.3f;
            *ptr = v1*0.3f + v2*0.2f + *(ptr+1)*0.5f;
#endif
            ptr += step;
            coffs += sign;
            i -= step;
         }
         while(i >= step);

         if (i)
         {
            _batch_fmadd(ptr, ptr-coffs, i, volume, 0.0f);
#if 0
            v1 = *(ptr-1); v2 = v1*0.5f + *ptr*0.5f;
            *(ptr-1) = *(ptr-2)*0.5f + v1*0.2f + v2*0.3f;
            *ptr = v1*0.3f + v2*0.2f + *(ptr+1)*0.5f;
#endif
         }
         _aax_memcpy(effect->reverb_history[track], ddeptr+end, ds*bps);
      }
      else	/* chorus, phasing */
      {
         _batch_resample_proc resamplefn = _aaxBufResampleNearest;
         int doffs = noffs - offs;
         float fact;

         fact = (float)(no_samples-doffs)/(float)no_samples;
         if (fact < 1.0f) {
            resamplefn = _aaxBufResampleLinear;
         }
         else if (fact > 1.0f) {
            resamplefn = _aaxBufResampleSkip;
         }

         resamplefn(scratch-ds, sptr-offs, 0, no_samples, 0, 0.0f, fact);
         _batch_fmadd(dptr, scratch-ds, no_samples, volume, 0.0f);
      }
   }
}

void
bufEffectDistort(int32_ptr d, const int32_ptr s,
                   unsigned int dmin, unsigned int dmax, unsigned int ds,
                   unsigned int track, void *data)
{
   float *distort = data;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(s != 0);
   assert(d != 0);
   assert(data != 0);
   assert(dmin < dmax);
   assert(data != NULL);
   assert(track < _AAX_MAX_SPEAKERS);

   do
   {
      const int32_ptr sptr = s - ds + dmin;
      int32_t *dptr = d - ds + dmin;
      unsigned int no_samples, bps;
      float clip, fact, mixfact;

      bps = sizeof(int32_t);
      no_samples = dmax+ds-dmin;
      _aax_memcpy(dptr, sptr, no_samples*bps);

      clip = distort[CLIPPING_FACTOR];
      mixfact = distort[AAX_MIX_FACTOR];
      fact = 128.0f*distort[AAX_DISTORTION_FACTOR];
      
      _batch_mul_value(dptr, bps, no_samples, (1.0f+fact)*mixfact);
       bufCompress(dptr, 0, no_samples, clip);
      _batch_fmadd(dptr, sptr, no_samples, 1.0f-mixfact, 0.0f);
   }
   while (0);
}

#endif /* !ENABLE_LITE */

void
bufFilterFrequency(int32_ptr d, const int32_ptr s,
                   unsigned int dmin, unsigned int dmax, unsigned int ds,
                   unsigned int track, void *data)
{
   _oalRingBufferFreqFilterInfo *filter = data;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(s != 0);
   assert(d != 0);
   assert(data != 0);
   assert(dmin < dmax);
   assert(data != NULL);
   assert(track < _AAX_MAX_SPEAKERS);

   if (filter->k)
   {
      const int32_ptr sptr = s - ds + dmin;
      int32_t *dptr = d - ds + dmin;
      float *hist = filter->freqfilter_history[track];
      float *cptr = filter->coeff;
      float lf = filter->lf_gain;
      float hf = filter->hf_gain;
      float k = filter->k;

      _batch_freqfilter(dptr, sptr, dmax+ds-dmin, hist, lf, hf, k, cptr);
   }
}

void
iir_compute_coefs(float fc, float fs, float *coef, float *gain)
{
   float k = 1.0f;
   float Q = 1.0f;

   float a0 = 1.0f;
   float a1 = 0.0f;
   float a2 = 0.0f;
   float b0 = 1.0f;
   float b1 = 1.4142f / Q;
   float b2 = 1.0f;

   szxform(&a0, &a1, &a2, &b0, &b1, &b2, fc, fs, &k, coef);

   *gain = k;
}

/* -------------------------------------------------------------------------- */

/*
 * From: http://www.gamedev.net/reference/articles/article845.asp
 *       http://www.gamedev.net/reference/articles/article846.asp
 */
static void
bilinear(float a0, float a1, float a2, float b0, float b1, float b2,
             float *k, float fs, float *coef)
{
   float ad, bd;

   a2 *= (4.0f * fs*fs);
   b2 *= (4.0f * fs*fs);
   a1 *= (2.0f * fs);
   b1 *= (2.0f * fs);

   ad = a2 + a1 + a0;
   bd = b2 + b1 + b0;

   *k *= ad/bd;

   *coef++ = (-2.0f*b2 + 2.0f*b0) / bd;
   *coef++ = (b2 - b1 + b0) / bd;
   *coef++ = (-2.0f*a2 + 2.0f*a0) / ad;
   *coef   = (a2 - a1 + a0) / ad;
}

static void
szxform(float *a0, float *a1, float *a2, float *b0, float *b1, float *b2,
        float fc, float fs, float *k, float *coef)
{
   float wp;

   wp = 2.0f*fs * tanf(GMATH_PI * fc/fs);
   *a2 /= wp*wp;
   *b2 /= wp*wp;
   *a1 /= wp;
   *b1 /= wp;

   bilinear(*a0, *a1, *a2, *b0, *b1, *b2, k, fs, coef);
}
