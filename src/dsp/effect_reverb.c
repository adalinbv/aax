/*
 * Copyright 2007-2018 by Erik Hofman.
 * Copyright 2009-2018 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
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

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <malloc.h>
#endif

#include <aax/aax.h>

#include <base/types.h>		/*  for rintf */
#include <base/gmath.h>

#include <software/rbuf_int.h>
#include "effects.h"
#include "arch.h"
#include "dsp.h"
#include "api.h"

static void _reverb_destroy(void*);
static void _reverb_destroy_delays(_aaxRingBufferReverbData*);
static void _reverb_add_reflections(void*, float, unsigned int, float, int);
static void _reverb_add_reverb(void**, float, unsigned int, float, float);
static void _reflections_run(void*, MIX_PTR_T, CONST_MIX_PTR_T, size_t, size_t, unsigned int, float, const void*, _aaxMixerInfo*, unsigned char);
static void _reverb_run(void*, MIX_PTR_T, CONST_MIX_PTR_T, MIX_PTR_T, size_t, size_t, unsigned int, const void*, _aaxMixerInfo*, unsigned char);


static aaxEffect
_aaxReverbEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 2);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxSetDefaultEffect3d(eff->slot[0], eff->pos, 0);
      _aaxSetDefaultEffect3d(eff->slot[1], eff->pos, 1);
      eff->slot[0]->destroy = _reverb_destroy;
      eff->slot[0]->swap = swap;
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxReverbEffectDestroy(_effect_t* effect)
{
   if (effect->slot[0]->data)
   {
      effect->slot[0]->destroy(effect->slot[0]->data);
      effect->slot[0]->data = NULL;
   }
   free(effect);

   return AAX_TRUE;
}

static aaxEffect
_aaxReverbEffectSetState(_effect_t* effect, int state)
{
   void *handle = effect->handle;
   aaxEffect rv = AAX_FALSE;

   switch (state & ~AAX_INVERSE)
   {
   case AAX_CONSTANT_VALUE:
   {
      unsigned int tracks = effect->info->no_tracks;
      float lb_depth, lb_gain;
      float depth, fs = 48000.0f;

      if (effect->info) {
         fs = effect->info->frequency;
      }

      /* calculate initial and loopback samples                      */
      lb_depth = effect->slot[0]->param[AAX_DECAY_DEPTH]/0.7f;
      lb_gain = 0.01f+effect->slot[0]->param[AAX_DECAY_LEVEL]*0.99f;
      _reverb_add_reverb(&effect->slot[0]->data, fs, tracks, lb_depth, lb_gain);

      do
      {
         _aaxRingBufferReverbData *reverb = effect->slot[0]->data;
         _aaxRingBufferFreqFilterData *flt = reverb->freq_filter;

         depth = effect->slot[0]->param[AAX_DELAY_DEPTH]/0.07f;
         _reverb_add_reflections(&reverb->reflections, fs, tracks, depth, state);

         if (!flt)
         {
            flt = calloc(1, sizeof(_aaxRingBufferFreqFilterData));
            if (flt)
            {
               flt->freqfilter = _aax_aligned_alloc(sizeof(_aaxRingBufferFreqFilterHistoryData));
               if (flt->freqfilter) {
                  memset(flt->freqfilter, 0, sizeof(_aaxRingBufferFreqFilterHistoryData));
               }
               else
               {
                  free(flt);
                  flt = NULL;
               }
            }
         }

         if (flt)
         {
            float dfact;

            /* set up a frequency filter between 100Hz and 15000Hz
             * for the refelctions. The lower the cut-off frequency,
             * the more the low frequencies get exaggerated.
             *
             * low: 100Hz/1.75*gain .. 15000Hz/1.0*gain
             * high: 100Hz/0.0*gain .. 15000Hz/0.33*gain
             *
             * Q is set to 0.6 to overly dampen the frequency response to
             * provide a bit smoother frequency response  around the
             * cut-off frequency.
             */
            reverb->fc = effect->slot[0]->param[AAX_CUTOFF_FREQUENCY];

            flt->run = _freqfilter_run;
            flt->lfo = 0;
            flt->fs = fs;
            flt->Q = 0.6f;
            flt->low_gain = 0.0f;
            flt->no_stages = 1;

            dfact = powf(reverb->fc*0.00005f, 0.2f);
            flt->low_gain = _MIN(1.75f-0.75f*dfact, 1.15f);
            flt->high_gain = 1.0f - 0.33f*dfact;
            flt->k = flt->low_gain/flt->high_gain;

            _aax_butterworth_compute(reverb->fc, flt);
         }

         reverb->info = effect->info;
         reverb->freq_filter = flt;
         reverb->occlusion = _occlusion_create(reverb->occlusion, effect->slot[1], state, fs);
      }
      while(0);

      break;
   }
   case AAX_FALSE:
      if (effect->slot[0]->data)
      {
         effect->slot[0]->destroy(effect->slot[0]->data);
         effect->slot[0]->data = NULL;
      }
      break;
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
      break;
   }
   rv = effect;
   return rv;
}

_effect_t*
_aaxNewReverbEffectHandle(const aaxConfig config, enum aaxEffectType type, UNUSED(_aax2dProps* p2d), _aax3dProps* p3d)
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 2);

   if (rv)
   {
      unsigned int size = sizeof(_aaxEffectInfo);

      memcpy(rv->slot[0], &p2d->effect[rv->pos], size);
      memcpy(rv->slot[1], &p3d->effect[rv->pos], size);
      rv->slot[0]->destroy = _reverb_destroy;
      rv->slot[0]->swap = swap;
      rv->slot[0]->data = NULL;

      rv->state = p3d->effect[rv->pos].state;
   }
   return rv;
}

static float
_aaxReverbEffectSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{  
   float rv = val;
   return rv;
}
   
static float
_aaxReverbEffectGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{  
   float rv = val;
   return rv;
}

static float
_aaxReverbEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxReverbRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { {50.0f, 0.0f, 0.0f, 0.0f }, { 22000.0f,   0.07f,    1.0f, 0.7f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {  FLT_MAX, FLT_MAX, FLT_MAX, 1.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f,    0.0f,    0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f,    0.0f,    0.0f, 0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxReverbRange[slot].min[param],
                       _aaxReverbRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxReverbEffect =
{
   AAX_TRUE,
   "AAX_reverb_effect_1.1", 1.1f,
   (_aaxEffectCreate*)&_aaxReverbEffectCreate,
   (_aaxEffectDestroy*)&_aaxReverbEffectDestroy,
   (_aaxEffectSetState*)&_aaxReverbEffectSetState,
   NULL,
   (_aaxNewEffectHandle*)&_aaxNewReverbEffectHandle,
   (_aaxEffectConvert*)&_aaxReverbEffectSet,
   (_aaxEffectConvert*)&_aaxReverbEffectGet,
   (_aaxEffectConvert*)&_aaxReverbEffectMinMax
};

static void
_reverb_destroy(void *ptr)
{  
   _aaxRingBufferReverbData *reverb = (_aaxRingBufferReverbData*)ptr;
   if (reverb)
   {
      _occlusion_destroy(reverb->occlusion);
      _aax_aligned_free(reverb->freq_filter->freqfilter);
      _reverb_destroy_delays(reverb);
      free(ptr);
   }
}



static void
_reflections_run(void *rb, MIX_PTR_T dptr, CONST_MIX_PTR_T sptr,
            size_t no_samples, size_t ds, unsigned int track, float gain,
            const void *data, _aaxMixerInfo *info, unsigned char mono)
{
   float dst = info ? _MAX(info->speaker[track].v4[0]*info->frequency*track/343.0,0.0f) : 0;
   _aaxRingBufferSample *rbd = (_aaxRingBufferSample*)rb;
   const _aaxRingBufferReflectionData *reflections = data;
   unsigned int snum, tracks;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(dptr != 0);
   assert(sptr != 0);
   assert(track < _AAX_MAX_SPEAKERS);

   tracks = rbd->no_tracks;

   /* reverb (1st order reflections) */
   /* skip if the caller is mono  */
   snum = reflections->no_delays;
   if (!mono && snum > 0)
   {
      unsigned int q;

      memset(dptr, 0, no_samples*sizeof(MIX_T));
      for(q=track % snum; q<snum; q += tracks)
      {  
         float volume = gain*reflections->delay[q].gain;
         if ((volume > 0.001f) || (volume < -0.001f))
         {  
            ssize_t offs = reflections->delay[q].sample_offs[track] + dst;
            if (offs && offs < (ssize_t)ds) {
               rbd->add(dptr, sptr-offs, no_samples, volume, 0.0f);
            }
         }
      }
   }
   else if (gain > LEVEL_64DB)
   {
      _aax_memcpy(dptr, sptr, no_samples*sizeof(MIX_T));
      if (gain < (1.0f-LEVEL_64DB)) {
         rbd->multiply(dptr, sizeof(MIX_T), no_samples, gain);
      }
   }
}

static void
_reverb_prepare(_aaxEmitter *src, _aax3dProps *fp3d, void *data)
{
   _aaxRingBufferReverbData *reverb = data;
   _aaxRingBufferOcclusionData *occlusion;
   _aaxRingBufferFreqFilterData *filter;

   filter = reverb->freq_filter;
   occlusion = reverb->occlusion;
   if (occlusion)
   {
      float l;

      _occlusion_prepare(src, fp3d, occlusion);

      l = 1.0f - occlusion->level;
      reverb->fc = _MINMAX(l*22000.0f, 100.0f, reverb->fc);
      if (reverb->fc > 100.0f) {
          _aax_butterworth_compute(reverb->fc, filter);
      }
   }
}

static void
_reverb_run(void *rb, MIX_PTR_T dptr, CONST_MIX_PTR_T sptr, MIX_PTR_T scratch,
            size_t no_samples, size_t ds, unsigned int track,
            const void *data, _aaxMixerInfo *info, unsigned char mono)
{
   float dst = info ? _MAX(info->speaker[track].v4[0]*info->frequency*track/343.0,0.0f) : 0;
   _aaxRingBufferSample *rbd = (_aaxRingBufferSample*)rb;
   const _aaxRingBufferReverbData *reverb = data;
   _aaxRingBufferFreqFilterData *filter = reverb->freq_filter;
   _aaxRingBufferOcclusionData *occlusion;
   float gain = 1.0f;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(dptr != 0);
   assert(sptr != 0);
   assert(scratch != 0);
   assert(track < _AAX_MAX_SPEAKERS);

   occlusion = reverb->occlusion;

   if (gain > LEVEL_64DB)
   {
      int snum;

#if 1
// TODO: move tot _reverb_prepare
      if (occlusion)
      {
         float l, fc;

         l = 1.0f - occlusion->level;
         fc = _MINMAX(l*22000.0f, 100.0f, reverb->fc);
         if (fc > 100.0f) {
          _aax_butterworth_compute(fc, filter);
         }
      }
#endif

      _reflections_run(rb, scratch, sptr, no_samples, ds, track, gain,
                       &reverb->reflections, info, mono);

      /* loopback for reverb (2nd order reflections) */
      snum = reverb->no_loopbacks;
      if (snum > 0)
      {
         size_t bytes = ds*sizeof(MIX_T);
         int q;

         _aax_memcpy(scratch-ds, reverb->reverb->history[track], bytes);
         filter->run(rbd, scratch, scratch, 0, no_samples, 0, track, filter, NULL, 0);
         for(q=0; q<snum; ++q)
         {
            float volume = gain * reverb->loopback[q].gain / (snum+1);
            if ((volume > 0.001f) || (volume < -0.001f))
            {
               ssize_t offs = reverb->loopback[q].sample_offs[track] + dst;
               if (offs && offs < (ssize_t)ds) {
                  rbd->add(scratch, scratch-offs, no_samples, volume, 0.0f);
               }
            }
         }
         _aax_memcpy(reverb->reverb->history[track], scratch+no_samples-ds, bytes);
      }

      filter->run(rbd, dptr, scratch, 0, no_samples, 0, track, filter, NULL,0);
      if (occlusion) {
         occlusion->run(rbd, dptr, sptr, scratch, no_samples, track,occlusion);
      }
   }
}

// Calculate the 1st order reflections
static void
_reverb_add_reflections(void *ptr, float fs, unsigned int tracks, float depth, int state)
{
   _aaxRingBufferReflectionData *reflections = ptr;
   unsigned int num = 4;

   assert(ptr != 0);
   assert(num < _AAX_MAX_DELAYS);
   assert(reflections);

   if (reflections)
   {
      static const float max_depth = _MIN(REVERB_EFFECTS_TIME, 0.15f);
      float delays[8], gains[8];
      float idepth, igain, idepth_offs;
      size_t i;

      reflections->run = _reflections_run;

      /* initial delay in seconds (should be between 10ms en 70 ms) */
      /* initial gains, defnining a direct path is not necessary    */
      /* sound Attenuation coeff. in dB/m (α) = 4.343 µ (m-1)       */
// http://www.sae.edu/reference_material/pages/Coefficient%20Chart.htm
      igain = 0.50f;
      if (state & AAX_INVERSE)
      {
         gains[6] = igain*0.9484f;      // conrete/brick = 0.95
         gains[5] = igain*0.8935f;      // wood floor    = 0.90
         gains[4] = igain*0.8254f;      // carpet        = 0.853
         gains[3] = igain*0.8997f;
         gains[2] = igain*0.8346f;
         gains[1] = igain*0.7718f;
         gains[0] = igain*0.7946f;
      }
      else
      {
         gains[0] = igain*0.9484f;      // conrete/brick = 0.95
         gains[1] = igain*0.8935f;      // wood floor    = 0.90
         gains[2] = igain*0.8254f;      // carpet        = 0.853
         gains[3] = igain*0.8997f;
         gains[4] = igain*0.8346f;
         gains[5] = igain*0.7718f;
         gains[6] = igain*0.7946f;
      }

      idepth = 0.005f+0.045f*depth;
      idepth_offs = (max_depth-idepth)*depth;
      idepth_offs = _MINMAX(idepth_offs, 0.01f, max_depth-0.05f);
      assert(idepth_offs+idepth*0.9876543f <= REVERB_EFFECTS_TIME);

      if (state & AAX_INVERSE)
      {
         delays[0] = idepth_offs + idepth*0.9876543f;
         delays[2] = idepth_offs + idepth*0.5019726f;
         delays[1] = idepth_offs + idepth*0.3333333f;
         delays[6] = idepth_offs + idepth*0.1992736f;
         delays[4] = idepth_offs + idepth*0.1428571f;
         delays[5] = idepth_offs + idepth*0.0909091f;
         delays[3] = idepth_offs + idepth*0.0769231f;
      }
      else
      {
         delays[6] = idepth_offs + idepth*0.9876543f;
         delays[4] = idepth_offs + idepth*0.5019726f;
         delays[5] = idepth_offs + idepth*0.3333333f;
         delays[0] = idepth_offs + idepth*0.1992736f;
         delays[2] = idepth_offs + idepth*0.1428571f;
         delays[1] = idepth_offs + idepth*0.0909091f;
         delays[3] = idepth_offs + idepth*0.0769231f;
      }

      reflections->gain = igain;
      reflections->no_delays = num;
      for (i=0; i<num; ++i)
      {
         if ((gains[i] > 0.001f) || (gains[i] < -0.001f))
         {
            unsigned int track;
            for (track=0; track<tracks; ++track) {
               reflections->delay[i].sample_offs[track]=(ssize_t)(delays[i] * fs);
            }
            reflections->delay[i].gain = gains[i];
#if 0
 printf("delay[%zi]: %zi\n", i, reverb->delay[i].sample_offs[0]);
#endif
         }
         else {
            reflections->delay[i].gain = 0.0f;
         }
      }
   }
}

// Calculate the 2nd order reflections
static void
_reverb_add_reverb(void **data, float fs, unsigned int tracks, float lb_depth, float lb_gain)
{
   _aaxRingBufferReverbData **ptr = (_aaxRingBufferReverbData**)data;
   _aaxRingBufferReverbData *reverb;
   unsigned int num;

   assert(ptr != 0);

   if (*ptr == NULL) {
      *ptr = calloc(1, sizeof(_aaxRingBufferReverbData));
   }

   reverb = *ptr;
   if (reverb)
   {
      reverb->prepare = _reverb_prepare;
      reverb->run = _reverb_run;

      if (reverb->reverb == 0)
      {
         size_t samples = TIME_TO_SAMPLES(fs, REVERB_EFFECTS_TIME);
         _aaxRingBufferCreateHistoryBuffer(&reverb->reverb,
                                           samples, tracks);
      }

      // http://www.sae.edu/reference_material/pages/Coefficient%20Chart.htm
      // https://web.archive.org/web/20150416071915/http://www.sae.edu/reference_material/pages/Coefficient%20Chart.htm
      if ((lb_depth != 0) && (lb_gain != 0))
      {
         static const float max_depth = REVERB_EFFECTS_TIME*0.6877777f;
         unsigned int track;
         float dlb, dlbp;

         num = 5;
         reverb->loopback[0].gain = lb_gain*0.95015f;   // conrete/brick = 0.95
         reverb->loopback[1].gain = lb_gain*0.87075f;
         reverb->loopback[2].gain = lb_gain*0.91917f;
         reverb->loopback[3].gain = lb_gain*0.72317f;   // carpet     = 0.853
         reverb->loopback[4].gain = lb_gain*0.80317f;
         reverb->loopback[5].gain = lb_gain*0.73317f;
         reverb->loopback[6].gain = lb_gain*0.88317f;
#if 0
 for (int i=0; i<7; ++i)
 printf(" loopback[%i].gain: %f\n", i,  reverb->loopback[i].gain);
#endif

         dlb = 0.01f+lb_depth*max_depth;
         dlbp = (REVERB_EFFECTS_TIME-dlb)*lb_depth;
         dlbp = _MINMAX(dlbp, 0.01f, REVERB_EFFECTS_TIME-0.01f);
//       dlbp = 0;

         dlb *= fs;
         dlbp *= fs;
         reverb->no_loopbacks = num;
         for (track=0; track<tracks; ++track)
         {
            reverb->loopback[0].sample_offs[track] = (dlbp + dlb*0.9876543f);
            reverb->loopback[1].sample_offs[track] = (dlbp + dlb*0.4901861f);
            reverb->loopback[2].sample_offs[track] = (dlbp + dlb*0.3333333f);
            reverb->loopback[3].sample_offs[track] = (dlbp + dlb*0.2001743f);
            reverb->loopback[4].sample_offs[track] = (dlbp + dlb*0.1428571f);
            reverb->loopback[5].sample_offs[track] = (dlbp + dlb*0.0909091f);
            reverb->loopback[6].sample_offs[track] = (dlbp + dlb*0.0769231f);
         }
#if 0
 for (int i=0; i<7; ++i)
 printf(" loopback.offset[%i]: %zi\n", i, reverb->loopback[i].sample_offs[0]);
#endif
      }
      *data = reverb;
   }
}

static void
_reverb_destroy_delays(_aaxRingBufferReverbData *reverb)
{
   assert(reverb != 0);
   if (reverb)
   {
      reverb->reflections.no_delays = 0;
      reverb->reflections.delay[0].gain = 1.0f;
      reverb->no_loopbacks = 0;
#if BYTE_ALIGN
      _aax_free(reverb->reverb);
#else
      free(reverb->reverb);
#endif
      free(reverb->freq_filter);
      reverb->freq_filter = 0;
      reverb->reverb = 0;
   }
}
