/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to thcd parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <math.h>	/* tan */

#include <base/logging.h>
#include <base/geometry.h>

#include <dsp/effects.h>

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>

#include "rbuf2d_effects.h"


/**
 * - dst and scratch point to the beginning of a buffer containing room for
 *   the delay effects prior to the pointer.
 * - start is the starting pointer
 * - end is the end pointer (end-start is the number of smaples)
 * - dmax does not include ds
 */
#define BUFSWAP(a, b) do { void* t = (a); (a) = (b); (b) = t; } while (0);

void
_aaxRingBufferEffectsApply(_aaxRingBufferSample *rbd,
          MIX_PTR_T dst, MIX_PTR_T src, MIX_PTR_T scratch,
          size_t start, size_t end, size_t no_samples,
          size_t ddesamps, unsigned int track, unsigned char ctr,
          void *freq, void *delay, void *distort, void *env)
{
   static const size_t bps = sizeof(MIX_T);
   _aaxRingBufferDelayEffectData* effect = delay;
   size_t ds = delay ? ddesamps : 0;	/* 0 for frequency filtering */
   MIX_T *psrc = src; /* might change further in the code */
   MIX_T *pdst = dst; /* might change further in the code */

   src += start;
   dst += start;

   /* streaming emitters with delay effects need the source history */
   if (effect && !effect->loopback && effect->history_ptr)
   {
      // copy the delay effects history to src
      DBG_MEMCLR(1, src-ds, ds, bps);
      _aax_memcpy(src-ds, effect->delay_history[track], ds*bps);

      // copy the new delay effects history back
      _aax_memcpy(effect->delay_history[track], src+no_samples-ds, ds*bps);
   }

   /* Apply frequency filter first */
   if (freq)
   {
      _aaxRingBufferFilterFrequency(rbd, pdst, psrc, start, end, ds, track,
                                    freq, env, ctr);
      BUFSWAP(pdst, psrc);
   }

   if (distort)
   {
      _aaxRingBufferEffectDistort(rbd, pdst, psrc, start, end, ds, track, distort, env);
      BUFSWAP(pdst, psrc);
   }

#if !ENABLE_LITE
   if (delay)
   {
      /* Apply delay effects */
      if (effect->loopback) {		/*    flanging     */
         _aaxRingBufferEffectDelay(rbd, psrc, psrc, scratch, start, end,
                                   no_samples, ds, delay, env, track);
      }
      else				/* phasing, chorus */
      {
         _aax_memcpy(pdst+start, psrc+start, no_samples*bps);
         _aaxRingBufferEffectDelay(rbd, pdst, psrc, scratch, start, end,
                                   no_samples, ds, delay, env, track);
         BUFSWAP(pdst, psrc);
      }
   }
#endif

   if (dst == pdst)	/* copy the data back to the dst buffer */
   {
      DBG_MEMCLR(1, dst-ds, ds+end, bps);
      _aax_memcpy(dst, src, no_samples*bps);
   }
}

#if !ENABLE_LITE
void
_aaxRingBufferEffectReflections(_aaxRingBufferSample *rbd,
                        MIX_PTR_T s, CONST_MIX_PTR_T sbuf, MIX_PTR_T sbuf2,
                        size_t dmin, size_t dmax, size_t ds,
                        unsigned int track, const void *data)
{
   const _aaxRingBufferReverbData *reverb = data;
   int snum;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(s != 0);
   assert(sbuf != 0);
   assert(sbuf2 != 0);
   assert(dmin < dmax);
   assert(track < _AAX_MAX_SPEAKERS);

   /* reverb (1st order reflections) */
   snum = reverb->no_delays;
   if (snum > 0)
   {
      _aaxRingBufferFreqFilterData* filter = reverb->freq_filter;
      MIX_T *scratch = (MIX_T*)sbuf + dmin;
      MIX_PTR_T sptr = s + dmin;
      int q;

      dmax -= dmin;
      _aax_memcpy(scratch, sptr, dmax*sizeof(MIX_T));
      for(q=0; q<snum; ++q)
      {
         float volume = reverb->delay[q].gain / (snum+1);
         if ((volume > 0.001f) || (volume < -0.001f))
         {
            size_t offs = reverb->delay[q].sample_offs[track];

            assert(offs < ds);
//          if (samples >= ds) samples = ds-1;

            rbd->add(scratch, sptr-offs, dmax, volume, 0.0f);
         }
      }

      _aaxRingBufferFilterFrequency(rbd, sbuf2, scratch, 0, dmax, 0, track, filter, NULL, 0);
      rbd->add(sptr, sbuf2, dmax, 0.5f, 0.0f);
   }
}

void
_aaxRingBufferEffectReverb(_aaxRingBufferSample *rbd, MIX_PTR_T s,
                   size_t dmin, size_t dmax, size_t ds,
                   unsigned int track, const void *data)
{
   const _aaxRingBufferReverbData *reverb = data;
   int snum;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(s != 0);
   assert(dmin < dmax);
   assert(track < _AAX_MAX_SPEAKERS);

   /* loopback for reverb (2nd order reflections) */
   snum = reverb->no_loopbacks;
   if (snum > 0)
   {
      size_t bytes = ds*sizeof(MIX_T);
      MIX_T *sptr = s + dmin;
      int q;

      _aax_memcpy(sptr-ds, reverb->reverb_history[track], bytes);
      for(q=0; q<snum; ++q)
      {
         size_t samples = reverb->loopback[q].sample_offs[track];
         float volume = reverb->loopback[q].gain / (snum+1);

         assert(samples < ds);
         if (samples >= ds) samples = ds-1;

         rbd->add(sptr, sptr-samples, dmax-dmin, volume, 0.0f);
      }
      _aax_memcpy(reverb->reverb_history[track], sptr+dmax-ds, bytes);
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
_aaxRingBufferEffectDelay(_aaxRingBufferSample *rbd,
               MIX_PTR_T d, CONST_MIX_PTR_T s, MIX_PTR_T scratch,
               size_t start, size_t end, size_t no_samples,
               size_t ds, void *data, void *env, unsigned int track)
{
   static const size_t bps = sizeof(MIX_T);
   _aaxRingBufferDelayEffectData* effect = data;
   float volume;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(s != 0);
   assert(d != 0);
   assert(start < end);
   assert(data != NULL);

   volume =  effect->delay.gain;
   do
   {
      ssize_t offs, noffs;
      MIX_T *sptr = (MIX_T*)s + start;
      MIX_T *dptr = d + start;

      offs = effect->delay.sample_offs[track];
      assert(start || (offs < ds));
      if (offs >= ds) offs = ds-1;

      if (start) {
         noffs = effect->curr_noffs[track];
      }
      else
      {
         noffs = (size_t)effect->lfo.get(&effect->lfo, env, s, track, end);
         effect->delay.sample_offs[track] = noffs;
         effect->curr_noffs[track] = noffs;
      }

      if (s == d)	/*  flanging */
      {
         size_t sign = (noffs < offs) ? -1 : 1;
         size_t doffs = labs(noffs - offs);
         size_t i = no_samples;
         size_t coffs = offs;
         size_t step = end;
         MIX_T *ptr = dptr;

         if (start)
         {
            step = effect->curr_step[track];
            coffs = effect->curr_coffs[track];
         }
         else
         {
            if (doffs)
            {
               step = end/doffs;
               if (step < 2) step = end;
            }
         }
         effect->curr_step[track] = step;

         DBG_MEMCLR(1, s-ds, ds+start, bps);
         _aax_memcpy(sptr-ds, effect->delay_history[track], ds*bps);
         if (i >= step)
         {
//          float diff;
            do
            {
               rbd->add(ptr, ptr-coffs, step, volume, 0.0f);
#if 0
               /* gradually fade to the new value */
               diff = (float)*ptr - (float)*(ptr-1);
               *(ptr-1) += (6.0f/7.0f)*diff;
               *(ptr-2) += (5.0f/7.0f)*diff;
               *(ptr-3) += (4.0f/7.0f)*diff;
               *(ptr-4) += (3.0f/7.0f)*diff;
               *(ptr-5) += (2.0f/7.0f)*diff;
               *(ptr-6) += (1.0f/7.0f)*diff;
#endif
               ptr += step;
               coffs += sign;
               i -= step;
            }
            while(i >= step);
         }
         if (i) {
            rbd->add(ptr, ptr-coffs, i, volume, 0.0f);
         }

//       DBG_MEMCLR(1, effect->delay_history[track], ds, bps);
         _aax_memcpy(effect->delay_history[track], dptr+no_samples-ds, ds*bps);
         effect->curr_coffs[track] = coffs;
      }
      else	/* chorus, phasing */
      {
         size_t doffs = noffs - offs;
         float fact;

         fact = _MAX(((float)end-(float)doffs)/(float)(end), 0.001f);
         if (fact == 1.0f) {
            rbd->add(dptr, sptr-offs, no_samples, volume, 0.0f);
         }
         else
         {
            DBG_MEMCLR(1, scratch-ds, ds+end, bps);
            rbd->resample(scratch-ds, sptr-offs, 0, no_samples, 0.0f, fact);
            rbd->add(dptr, scratch-ds, no_samples, volume, 0.0f);
         }
      }
   }
   while (0);
}

void
_aaxRingBufferEffectConvolution(_aaxRingBufferSample *rbd, MIX_PTR_T s,
                 size_t dmin, size_t dmax, unsigned int track, const void *data)
{
   _aaxRingBufferConvolutionData *convolution = data;
   MIX_T *hptr = convolution->history[track];
   MIX_T *cptr = convolution->sample;
   unsigned int q, dnum, cnum, cpos;
   MIX_T *sptr = s + dmin;
   MIX_T *dptr = sptr;
   float f;

   dnum = dmax-dmin;
   f = 50.0f/dnum;

   cnum = convolution->no_samples;
   cpos = convolution->history_start;
   hptr += cpos;

   q = cnum;
   do
   {
      float volume = *cptr++ * f;
      if (fabsf(volume) > convolution->silence_level) {
         rbd->add(hptr, sptr, dnum, volume, 0.0f);
      }
      hptr++;
   }
   while (--q);

   hptr = convolution->history[track] + cpos;
   rbd->add(dptr, hptr, dnum, convolution->gain, 0.0f);

   cpos += dnum;
   cnum = convolution->history_samples;
// if ((cpos + cnum) > convolution->history_max)
   {
      size_t bytes = (cnum-dnum)*sizeof(MIX_T);
      memmove(hptr, hptr+cpos, bytes);
      cpos = 0;
   }
   convolution->history_start = cpos;
}


void
_aaxRingBufferEffectDistort(_aaxRingBufferSample *rbd,
                   MIX_PTR_T d, CONST_MIX_PTR_T s,
                   size_t dmin, size_t dmax, size_t ds,
                   unsigned int track, void *data, void *env)
{
   _aaxFilterInfo *dist_effect = (_aaxFilterInfo*)data;
   _aaxRingBufferLFOData* lfo = dist_effect->data;
   float *params = dist_effect->param;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(s != 0);
   assert(d != 0);
   assert(data != 0);
   assert(dmin < dmax);
   assert(data != NULL);
   assert(track < _AAX_MAX_SPEAKERS);

   do
   {
      static const size_t bps = sizeof(MIX_T);
      CONST_MIX_PTR_T sptr = s - ds + dmin;
      MIX_T *dptr = d - ds + dmin;
      float clip, asym, fact, mix;
      size_t no_samples;
      float lfo_fact = 1.0;

      no_samples = dmax+ds-dmin;
      DBG_MEMCLR(1, d-ds, ds+dmax, bps);

      if (lfo) {
         lfo_fact = lfo->get(lfo, env, sptr, track, no_samples);
      }
      fact = params[AAX_DISTORTION_FACTOR]*lfo_fact;
      clip = params[AAX_CLIPPING_FACTOR];
      mix  = params[AAX_MIX_FACTOR]*lfo_fact;
      asym = params[AAX_ASYMMETRY];

      _aax_memcpy(dptr, sptr, no_samples*bps);
      if (mix > 0.01f)
      {
         float mix_factor;

         /* make dptr the wet signal */
         if (fact > 0.0013f) {
            rbd->multiply(dptr, bps, no_samples, 1.0f+64.0f*fact);
         }

         if ((fact > 0.01f) || (asym > 0.01f))
         {
            size_t average = 0;
            size_t peak = no_samples;
            _aaxRingBufferLimiter(dptr, &average, &peak, clip, 4*asym);
         }

         /* mix with the dry signal */
         mix_factor = mix/(0.5f+powf(fact, 0.25f));
         rbd->multiply(dptr, bps, no_samples, mix_factor);
         if (mix < 0.99f) {
            rbd->add(dptr, sptr, no_samples, 1.0f-mix, 0.0f);
         }
      }
   }
   while(0);
}

#endif /* !ENABLE_LITE */

void
_aaxRingBufferFilterFrequency(_aaxRingBufferSample *rbd,
                   MIX_PTR_T d, CONST_MIX_PTR_T s,
                   size_t dmin, size_t dmax, size_t ds,
                   unsigned int track, void *data, void *env, unsigned char ctr)
{
   _aaxRingBufferFreqFilterData *filter = data;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(s != 0);
   assert(d != 0);
   assert(data != 0);
   assert(dmin < dmax);
   assert(data != NULL);
   assert(track < _AAX_MAX_SPEAKERS);

   if (filter->k)
   {
      CONST_MIX_PTR_T sptr = s - ds + dmin;
      MIX_T *dptr = d - ds + dmin;
      int num;

      if (filter->lfo && !ctr)
      {
         float fc = _MAX(filter->lfo->get(filter->lfo, env, s, track, dmax), 1);

         if (filter->state) {
            _aax_bessel_compute(fc, filter);
         } else {
            _aax_butterworth_compute(fc, filter);
         }
      }

      num = dmax+ds-dmin;
      rbd->freqfilter(dptr, sptr, track, num, filter);
      if (filter->state && (filter->low_gain > LEVEL_128DB)) {
         rbd->add(dptr, sptr, num, filter->low_gain, 0.0f);
      }
   }
}



/* -------------------------------------------------------------------------- */

void
_aaxRingBufferDelaysAdd(void **data, float fs, unsigned int tracks, const float *delays, const float *gains, size_t num, float igain, float lb_depth, float lb_gain)
{
   _aaxRingBufferReverbData **ptr = (_aaxRingBufferReverbData**)data;
   _aaxRingBufferReverbData *reverb;

   assert(ptr != 0);
   assert(delays != 0);
   assert(gains != 0);

   if (*ptr == NULL) {
      *ptr = calloc(1, sizeof(_aaxRingBufferReverbData));
   }

   reverb = *ptr;
   if (reverb)
   {
      size_t j, snum = _AAX_MAX_SPEAKERS;

      if (reverb->history_ptr == 0)
      {
         size_t samples = TIME_TO_SAMPLES(fs, REVERB_EFFECTS_TIME);
         _aaxRingBufferCreateHistoryBuffer(&reverb->history_ptr,
                                           reverb->reverb_history,
                                           samples, tracks);
      }

      if (num < _AAX_MAX_DELAYS)
      {
         size_t i;

         reverb->gain = igain;
         reverb->no_delays = num;
         for (i=0; i<num; ++i)
         {
            if ((gains[i] > 0.001f) || (gains[i] < -0.001f))
            {
               for (j=0; j<snum; ++j) {
                  reverb->delay[i].sample_offs[j] = (ssize_t)(delays[i] * fs);
               }
               reverb->delay[i].gain = gains[i];
            }
            else {
               reverb->no_delays--;
            }
         }
      }

      // http://www.sae.edu/reference_material/pages/Coefficient%20Chart.htm
      if ((num > 0) && (lb_depth != 0) && (lb_gain != 0))
      {
         static const float max_depth = REVERB_EFFECTS_TIME*0.6877777f;
         float dlb, dlbp;

         num = 5;
         reverb->loopback[0].gain = lb_gain*0.95015f;	// conrete/brick = 0.95
         reverb->loopback[1].gain = lb_gain*0.87075f;
         reverb->loopback[2].gain = lb_gain*0.91917f;
         reverb->loopback[3].gain = lb_gain*0.72317f;	// carpet     = 0.853
         reverb->loopback[4].gain = lb_gain*0.80317f;
         reverb->loopback[5].gain = lb_gain*0.73317f;
         reverb->loopback[6].gain = lb_gain*0.88317f;

         dlb = 0.01f+lb_depth*max_depth;
         dlbp = (REVERB_EFFECTS_TIME-dlb)*lb_depth;
         dlbp = _MINMAX(dlbp, 0.01f, REVERB_EFFECTS_TIME-0.01f);
//       dlbp = 0;

         dlb *= fs;
         dlbp *= fs;
         reverb->no_loopbacks = num;
         for (j=0; j<num; ++j)
         {
            reverb->loopback[0].sample_offs[j] = (dlbp + dlb*0.9876543f);
            reverb->loopback[1].sample_offs[j] = (dlbp + dlb*0.4901861f);
            reverb->loopback[2].sample_offs[j] = (dlbp + dlb*0.3333333f);
            reverb->loopback[3].sample_offs[j] = (dlbp + dlb*0.2001743f);
            reverb->loopback[4].sample_offs[j] = (dlbp + dlb*0.1428571f);
            reverb->loopback[5].sample_offs[j] = (dlbp + dlb*0.0909091f);
            reverb->loopback[6].sample_offs[j] = (dlbp + dlb*0.0769231f);
         }
      }
      *data = reverb;
   }
}

void
_aaxRingBufferDelaysRemove(void **data)
{
   _aaxRingBufferReverbData *reverb;

   assert(data != 0);

   reverb = *data;
   if (reverb)
   {
      reverb->no_delays = 0;
      reverb->no_loopbacks = 0;
      reverb->delay[0].gain = 1.0f;
      free(reverb->history_ptr);
      free(reverb->freq_filter);
      reverb->freq_filter = 0;
      reverb->history_ptr = 0;
   }
}

#if 0
void
_aaxRingBufferDelayRemoveNum(_aaxRingBuffer *rb, size_t n)
{
   size_t size;

   assert(rb);

   if (rb->reverb)
   {
      assert(n < rb->reverb->no_delays);

      rb->reverb->no_delays--;
      size = rb->reverb->no_delays - n;
      size *= sizeof(_aaxRingBufferDelayInfo);
      memcpy(&rb->reverb->delay[n], &rb->reverb->delay[n+1], size);
   }
}
#endif

// size is the number of sampler for every track
size_t
_aaxRingBufferCreateHistoryBuffer(void **hptr, int32_t *history[_AAX_MAX_SPEAKERS], size_t size, int tracks)
{
   char *ptr, *p;
   int i;

   size *= sizeof(MIX_T);
#if BYTE_ALIGN
   if (size & MEMMASK)
   {
      size |= MEMMASK;
      size++;
   }
   p = 0;
   ptr = _aax_calloc(&p, tracks, size);
#else
   ptr = p = calloc(tracks, size);
#endif
   if (ptr)
   {
      *hptr = ptr;
      for (i=0; i<tracks; ++i)
      {
         history[i] = (int32_t*)p;
         p += size;
      }
   }
   return size;
}

