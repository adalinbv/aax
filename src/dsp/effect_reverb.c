/*
 * Copyright 2007-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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
#include "api.h"
#include "arch.h"

static void _reverb_destroy(void*);
static void _reverb_destory_delays(void**);
static void _reverb_add_delays(void**, float, unsigned int, const float*, const float*, size_t, float, float, float);
static void _reverb_run(void*, MIX_PTR_T, CONST_MIX_PTR_T, MIX_PTR_T, size_t, size_t, unsigned int, const void*, _aaxMixerInfo*);
void _freqfilter_run(void*, MIX_PTR_T, CONST_MIX_PTR_T, size_t, size_t, size_t, unsigned int, void*, void*, unsigned char);

static aaxEffect
_aaxReverbEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 2);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxSetDefaultEffect2d(eff->slot[0], eff->pos, 0);
      _aaxSetDefaultEffect2d(eff->slot[1], eff->pos, 1);
      eff->slot[0]->destroy = _reverb_destroy;
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxReverbEffectDestroy(_effect_t* effect)
{
   effect->slot[0]->destroy(effect->slot[0]->data);
   effect->slot[0]->data = NULL;
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
      /* i = initial, lb = loopback */
      /* max 100ms reverb, longer sounds like echo */
      static const float max_depth = _MIN(REVERB_EFFECTS_TIME, 0.15f);
      unsigned int tracks = effect->info->no_tracks;
      float delays[8], gains[8];
      float idepth, igain, idepth_offs, lb_depth, lb_gain;
      float depth, fs = 48000.0f;
      int num;

      if (effect->info) {
         fs = effect->info->frequency;
      }

      /* initial delay in seconds (should be between 10ms en 70 ms) */
      /* initial gains, defnining a direct path is not necessary    */
      /* sound Attenuation coeff. in dB/m (α) = 4.343 µ (m-1)       */
// http://www.sae.edu/reference_material/pages/Coefficient%20Chart.htm
      num = 3;
      igain = 0.50f;
      if (state & AAX_INVERSE)
      {
         gains[6] = igain*0.9484f;	// conrete/brick = 0.95
         gains[5] = igain*0.8935f;	// wood floor    = 0.90
         gains[4] = igain*0.8254f;	// carpet        = 0.853
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

      depth = effect->slot[0]->param[AAX_DELAY_DEPTH]/0.07f;
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

      /* calculate initial and loopback samples                      */
      lb_depth = effect->slot[0]->param[AAX_DECAY_DEPTH]/0.7f;
      lb_gain = 0.01f+effect->slot[0]->param[AAX_DECAY_LEVEL]*0.99f;
      _reverb_add_delays(&effect->slot[0]->data, fs, tracks,
                              delays, gains, num, 1.25f,
                              lb_depth, lb_gain);
      do
      {
         _aaxRingBufferReverbData *reverb = effect->slot[0]->data;
         _aaxRingBufferFreqFilterData *flt = reverb->freq_filter;

         if (!flt) {
            flt = calloc(2, sizeof(_aaxRingBufferFreqFilterData));
         }

         reverb->info = effect->info;
         reverb->inverse = (state & AAX_INVERSE) ? 1 : 0;
         reverb->freq_filter = flt;
         if (flt)
         {
            _aaxRingBufferFreqFilterData *flt_dp;
            const char *ptr;
            float dfact;

            /* set up the direct path frequency filter */
            ptr = (char*)flt + sizeof(_aaxRingBufferFreqFilterData);
            flt_dp = (_aaxRingBufferFreqFilterData*)ptr;

            reverb->fc_dp = effect->slot[1]->param[AAX_CUTOFF_FREQUENCY];

            flt_dp->run = _freqfilter_run;
            flt_dp->lfo = 0;
            flt_dp->fs = fs;
            flt_dp->Q = 0.6f;
            flt_dp->no_stages = 1;

#if 0
            flt_dp->high_gain = fabsf(effect->slot[1]->param[AAX_HF_GAIN]);
#else
            flt_dp->high_gain = 1.0f;
#endif
            flt_dp->high_gain *= _lin2log(reverb->fc_dp)/4.343409f;

            flt_dp->low_gain = fabsf(effect->slot[1]->param[AAX_LF_GAIN]);
            if (flt_dp->low_gain < LEVEL_128DB) flt_dp->low_gain = 0.0f;

            flt_dp->k = flt_dp->low_gain/flt_dp->high_gain;

            _aax_butterworth_compute(reverb->fc_dp, flt_dp);
            reverb->freq_filter_dp = flt_dp;
            

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
            flt->high_gain = 1.75f-0.75f*dfact;
            flt->low_gain = 0.33f*dfact;
            flt->k = flt->low_gain/flt->high_gain;

            _aax_butterworth_compute(reverb->fc, flt);
         }
      }
      while(0);

      break;
   }
   case AAX_FALSE:
      effect->slot[0]->destroy(effect->slot[0]->data);
      effect->slot[0]->data = NULL;
      break;
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
      break;
   }
   rv = effect;
   return rv;
}

_effect_t*
_aaxNewReverbEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 1);

   if (rv)
   {
      unsigned int size = sizeof(_aaxEffectInfo);

       memcpy(rv->slot[0], &p2d->effect[rv->pos], size);
      rv->slot[0]->destroy = _reverb_destroy;
      rv->slot[0]->data = NULL;

      rv->state = p2d->effect[rv->pos].state;
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
    { {50.0f, 0.0f, 0.0f, 0.0f }, { 22000.0f, 0.07f, 1.0f, 0.7f } },
    { {50.0f, 0.0f, 0.0f, 0.0f }, { 22000.0f, 1.0f,  1.0f, AAX_FPINFINITE } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f, 0.0f,  0.0f, 0.0f } }
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
   _reverb_destory_delays(&ptr);
   free(ptr);
}

static void
_reverb_run(void *rb, MIX_PTR_T dptr, CONST_MIX_PTR_T sptr, MIX_PTR_T scratch,
            size_t no_samples, size_t ds, unsigned int track,
            const void *data, _aaxMixerInfo *info)
{
   float dst = info ? _MAX(info->speaker[track].v4[0]*info->frequency*track/343.0,0.0f) : 0;
   _aaxRingBufferSample *rbd = (_aaxRingBufferSample*)rb;
   _aaxRingBufferFreqFilterData *filter, *filter_dp;
   const _aaxRingBufferReverbData *reverb = data;
   int snum;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(dptr != 0);
   assert(sptr != 0);
   assert(scratch != 0);
   assert(track < _AAX_MAX_SPEAKERS);

   filter = reverb->freq_filter;
   filter_dp = reverb->freq_filter_dp;

   _aax_memcpy(scratch, sptr, no_samples*sizeof(MIX_T));

   /* reverb (1st order reflections) */
   snum = reverb->no_delays;
   if (snum > 0)
   {
      int q;

      for(q=0; q<snum; ++q)
      {  
         float volume = reverb->delay[q].gain / (snum+1);
         if ((volume > 0.001f) || (volume < -0.001f))
         {  
            ssize_t offs = reverb->delay[q].sample_offs[track] + dst;
            if (offs && offs < (ssize_t)ds) {
               rbd->add(scratch, sptr-offs, no_samples, volume, 0.0f);
            }
         }
      }

      if (reverb->fc < 15000.0f) {
         filter->run(rbd, scratch, scratch, 0, no_samples, 0, track, filter, NULL, 0);
      }
   }

   /* loopback for reverb (2nd order reflections) */
   snum = reverb->no_loopbacks;
   if (snum > 0)
   {
      size_t bytes = ds*sizeof(MIX_T);
      int q;

      _aax_memcpy(scratch-ds, reverb->reverb_history[track], bytes);
      for(q=0; q<snum; ++q)
      {
         float volume = reverb->loopback[q].gain / (snum+1);
         if ((volume > 0.001f) || (volume < -0.001f))
         {
            ssize_t offs = reverb->loopback[q].sample_offs[track] + dst;
            if (offs && offs < (ssize_t)ds) {
               rbd->add(scratch, scratch-offs, no_samples, volume, 0.0f);
            }
         }
      }
      _aax_memcpy(reverb->reverb_history[track], scratch+no_samples-ds, bytes);
   }

   filter->run(rbd, dptr, scratch, 0, no_samples, 0, track, filter, NULL, 0);
   

   /* add the dirtect path */
   if (filter_dp->low_gain > LEVEL_96DB)
   {
      if (reverb->fc_dp > 15000.0f) {
         rbd->add(dptr, sptr, no_samples, filter_dp->low_gain, 0.0f);
      }
      else
      {
         filter_dp->run(rbd, scratch, sptr, 0, no_samples, 0, track, filter_dp, NULL, 0);
         rbd->add(dptr, scratch, no_samples, 1.0f, 0.0f);
      }
   }
}

static void
_reverb_add_delays(void **data, float fs, unsigned int tracks, const float *delays, const float *gains, size_t num, float igain, float lb_depth, float lb_gain)
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
      size_t track;

      reverb->run = _reverb_run;

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
               for (track=0; track<tracks; ++track) {
                  reverb->delay[i].sample_offs[track]=(ssize_t)(delays[i] * fs);
               }
               reverb->delay[i].gain = gains[i];
#if 0
 printf("delay[%zi]: %zi\n", i, reverb->delay[i].sample_offs[0]);
#endif
            }
            else {
               reverb->no_delays--;
            }
         }
      }

      // http://www.sae.edu/reference_material/pages/Coefficient%20Chart.htm
      // https://web.archive.org/web/20150416071915/http://www.sae.edu/reference_material/pages/Coefficient%20Chart.htm
      if ((num > 0) && (lb_depth != 0) && (lb_gain != 0))
      {
         static const float max_depth = REVERB_EFFECTS_TIME*0.6877777f;
         float dlb, dlbp;

         num = 5;
         reverb->loopback[0].gain = lb_gain*0.95015f;   // conrete/brick = 0.95
         reverb->loopback[1].gain = lb_gain*0.87075f;
         reverb->loopback[2].gain = lb_gain*0.91917f;
         reverb->loopback[3].gain = lb_gain*0.72317f;   // carpet     = 0.853
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
 printf(" loopback[%i]: %zi\n", i, reverb->loopback[i].sample_offs[0]);
#endif
      }
      *data = reverb;
   }
}

static void
_reverb_destory_delays(void **data)
{
   _aaxRingBufferReverbData *reverb;

   assert(data != 0);

   reverb = *data;
   if (reverb)
   {
      reverb->no_delays = 0;
      reverb->no_loopbacks = 0;
      reverb->delay[0].gain = 1.0f;
#if BYTE_ALIGN
      _aax_free(reverb->history_ptr);
#else
      free(reverb->history_ptr);
#endif
      free(reverb->freq_filter);
      reverb->freq_filter = 0;
      reverb->history_ptr = 0;
   }
}
