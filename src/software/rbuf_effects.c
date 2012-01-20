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
          unsigned int start, unsigned int end, unsigned int no_samples,
          unsigned int ddesamps, unsigned int track,
          void *freq, void *delay, void *distort)
{
   static const unsigned int bps = sizeof(int32_t);
   _oalRingBufferDelayEffectData* effect = delay;
   unsigned int ds = effect ? ddesamps : 1;	/* 1 for frequency filtering */
   int32_t *psrc = src;
   int32_t *pdst = dst;

   if (effect && !effect->reverb && effect->history_ptr)	/* streaming */
   {
      DBG_MEMCLR(1, src-ds, ds+start, bps);
      _aax_memcpy(src+start-ds, effect->delay_history[track], ds*bps);

//    DBG_MEMCLR(1, effect->delay_history[track], ds, bps);
      _aax_memcpy(effect->delay_history[track], src+start+no_samples-ds,ds*bps);
   }

   if (freq)				/* Apply frequency filter first */
   {
      bufFilterFrequency(pdst, psrc, start, end, ds, track, freq);
      BUFSWAP(pdst, psrc);
   }

   if (distort)				/* distortion is second */
   {
      bufEffectDistort(pdst, psrc, start, end, ds, track, distort);
      BUFSWAP(pdst, psrc);
   }  

#if !ENABLE_LITE
   if (delay)
   {
      /* Apply delay effects */
      if (effect->reverb) {		/*    flanging     */
         bufEffectDelay(psrc, psrc, scratch, start, end, no_samples, ds, delay, track);
      }
      else				/* phasing, chorus */
      {
         unsigned int offs = effect->delay.sample_offs[track];
         unsigned int ext_no_samples = no_samples + offs;

         _aax_memcpy(pdst+start-offs, psrc+start-offs, ext_no_samples*bps);
         bufEffectDelay(pdst, psrc, scratch, start, end, no_samples, ds, delay, track);
         BUFSWAP(pdst, psrc);
      }
   }
#endif

   if (dst == pdst)	/* copy the data back to the dst buffer */
   {
      DBG_MEMCLR(1, dst-ds, ds+end, bps);
      _aax_memcpy(dst+start, src+start, no_samples*bps);
   }
// bufCompress(dptr, start, end, 0.2f);
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
 * - end is the end pointer (end-start is the number of samples)
 * - no_samples is the number of samples to process this run
 * - dmax does not include ds
 */
void
bufEffectDelay(int32_ptr d, const int32_ptr s, int32_ptr scratch,
               unsigned int start, unsigned int end, unsigned int no_samples,
               unsigned int ds, void *data, unsigned int track)
{
   static const unsigned int bps = sizeof(int32_t);
   _oalRingBufferDelayEffectData* effect = data;
   float volume;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(s != 0);
   assert(d != 0);
   assert(start < end);
   assert(data != NULL);

   volume =  effect->delay.gain;
   do
   {
      unsigned int offs, noffs;
      int32_t *sptr = s + start;
      int32_t *dptr = d + start;

      offs = effect->delay.sample_offs[track];
      assert(offs <= ds);

      if (start) {
         noffs = effect->curr_noffs;
      }
      else
      {
         noffs = effect->lfo.get(&effect->lfo, s, track, end);
         effect->delay.sample_offs[track] = noffs;
         effect->curr_noffs = noffs;
      }

      if (s == d)	/*  flanging */
      {
         unsigned int sign = (noffs < offs) ? -1 : 1;
         unsigned int doffs = abs(noffs - offs);
         unsigned int i = no_samples;
         unsigned int coffs = offs;
         unsigned int step = end;
         int32_t *ptr = dptr;

         if (start)
         {
            step = effect->curr_step;
            coffs = effect->curr_coffs;
         }
         else
         {
            if (doffs)
            {
               step = end/doffs;
               if (step < 2) step = end;
            }
         }
         effect->curr_step = step;

         DBG_MEMCLR(1, s-ds, ds+start, bps);
         _aax_memcpy(sptr-ds, effect->delay_history[track], ds*bps);
         if (i >= step)
         {
            do
            {
               _batch_fmadd(ptr, ptr-coffs, step, volume, 0.0f);
               ptr += step;
               coffs += sign;
               i -= step;
            }
            while(i >= step);
         }
         if (i) {
            _batch_fmadd(ptr, ptr-coffs, i, volume, 0.0f);
         }

//       DBG_MEMCLR(1, effect->delay_history[track], ds, bps);
         _aax_memcpy(effect->delay_history[track], dptr+no_samples-ds, ds*bps);
         effect->curr_coffs = coffs;
      }
      else	/* chorus, phasing */
      {
         _batch_resample_proc resamplefn = _aaxBufResampleNearest;
         int doffs = noffs - offs;
         float fact;

         fact = _MAX((float)((int)end-doffs)/(float)(end), 0.0f);
         if (fact < 1.0f) {
            resamplefn = _aaxBufResampleLinear;
         }
         else if (fact > 1.0f) {
            resamplefn = _aaxBufResampleSkip;
         }

         DBG_MEMCLR(1, scratch-ds, ds+end, bps);
         resamplefn(scratch-ds, sptr-offs, 0, no_samples, 0, 0.0f, fact);
         _batch_fmadd(dptr, scratch-ds, no_samples, volume, 0.0f);
      }
   }
   while (0);
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
      static const unsigned int bps = sizeof(int32_t);
      const int32_ptr sptr = s - ds + dmin;
      int32_t *dptr = d - ds + dmin;
      float clip, fact, mixfact;
      unsigned int no_samples;

      clip = distort[CLIPPING_FACTOR];
      mixfact = distort[AAX_MIX_FACTOR];
      fact = 192.0f*distort[AAX_DISTORTION_FACTOR];

      no_samples = dmax+ds-dmin;
      DBG_MEMCLR(1, d-ds, ds+dmax, bps);
      _aax_memcpy(dptr, sptr, no_samples*bps);
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

      if (filter->lfo)
      {
         float fc = filter->lfo->get(filter->lfo, s, track, dmax);
         float Q = filter->Q;

         k = 1.0f;
         iir_compute_coefs(fc, filter->fs, cptr, &k, Q);
      }

      _batch_freqfilter(dptr, sptr, dmax+ds-dmin, hist, lf, hf, k, cptr);
   }
}

void
iir_compute_coefs(float fc, float fs, float *coef, float *gain, float Q)
{
   float k = 1.0f;

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
