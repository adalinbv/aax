/*
 * Copyright 2007-2015 by Erik Hofman.
 * Copyright 2009-2015 by Adalin B.V.
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
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <malloc.h>
#endif

#include <aax/aax.h>

#include <base/types.h>		/*  for rintf */
#include <base/gmath.h>

#include "effects.h"
#include "api.h"
#include "arch.h"


static aaxEffect
_aaxReverbEffectCreate(_handle_t *handle, enum aaxEffectType type)
{
   unsigned int size = sizeof(_effect_t) + sizeof(_aaxEffectInfo);
   _effect_t* eff = calloc(1, size);
   aaxEffect rv = NULL;

   if (eff)
   {
      char *ptr;

      eff->id = EFFECT_ID;
      eff->state = AAX_FALSE;
      eff->info = handle->info ? handle->info : _info;

      ptr = (char*)eff + sizeof(_effect_t);
      eff->slot[0] = (_aaxEffectInfo*)ptr;
      eff->pos = _eff_cvt_tbl[type].pos;
      eff->type = type;

      size = sizeof(_aaxEffectInfo);
      _aaxSetDefaultEffect2d(eff->slot[0], eff->pos);
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxReverbEffectDestroy(_effect_t* effect)
{
   _aaxRingBufferReverbData* data = effect->slot[0]->data;
   if (data) free(data->history_ptr);
   free(effect->slot[0]->data);
   effect->slot[0]->data = NULL;
   free(effect);

   return AAX_TRUE;
}

static aaxEffect
_aaxReverbEffectSetState(_effect_t* effect, int state)
{
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
      gains[0] = igain*0.9484f;	// conrete/brick = 0.95
      gains[1] = igain*0.8935f;	// wood floor    = 0.90
      gains[2] = igain*0.8254f;	// carpet        = 0.853
      gains[3] = igain*0.8997f;
      gains[4] = igain*0.8346f;
      gains[5] = igain*0.7718f;
      gains[6] = igain*0.7946f;

      depth = effect->slot[0]->param[AAX_DELAY_DEPTH]/0.07f;
      idepth = 0.005f+0.045f*depth;
      idepth_offs = (max_depth-idepth)*depth;
      idepth_offs = _MINMAX(idepth_offs, 0.01f, max_depth-0.05f);
      assert(idepth_offs+idepth*0.9876543f <= REVERB_EFFECTS_TIME);

      delays[0] = idepth_offs + idepth*0.9876543f;
      delays[2] = idepth_offs + idepth*0.5019726f;
      delays[1] = idepth_offs + idepth*0.3333333f;
      delays[6] = idepth_offs + idepth*0.1992736f;
      delays[4] = idepth_offs + idepth*0.1428571f;
      delays[5] = idepth_offs + idepth*0.0909091f;
      delays[3] = idepth_offs + idepth*0.0769231f;

      /* calculate initial and loopback samples                      */
      lb_depth = effect->slot[0]->param[AAX_DECAY_DEPTH]/0.7f;
      lb_gain = 0.01f+effect->slot[0]->param[AAX_DECAY_LEVEL]*0.99f;
      _aaxRingBufferDelaysAdd(&effect->slot[0]->data, fs, tracks,
                              delays, gains, num, 1.25f,
                              lb_depth, lb_gain);
      do
      {
         _aaxRingBufferReverbData *reverb = effect->slot[0]->data;
         _aaxRingBufferFreqFilterData *flt = reverb->freq_filter;
         if (!flt) {
            flt = calloc(1, sizeof(_aaxRingBufferFreqFilterData));
         }

         reverb->freq_filter = flt;
         if (flt)
         {
            float *cptr = flt->coeff;
            float dfact, fc, k, Q;
            int stages;

            /* set up a cut-off frequency between 100Hz and 15000Hz
             * the lower the cut-off frequency, the more the low
             * frequencies get exaggerated.
             *
             * low: 100Hz/1.75*gain .. 15000Hz/1.0*gain
             * high: 100Hz/0.0*gain .. 15000Hz/0.33*gain
             *
             * Q is set to 0.6 to dampen the frequency response too
             * much to provide a bit smoother frequency response
             * around the cut-off frequency.
             */
            k = 1.0f;
            Q = 0.6f;
            stages = 1;
            fc = effect->slot[0]->param[AAX_CUTOFF_FREQUENCY];
            _aax_butterworth_compute(fc, fs, cptr, &k, Q, stages, AAX_TRUE);

            dfact = powf(fc*0.00005f, 0.2f);
            flt->lf_gain = 1.75f-0.75f*dfact;
            flt->hf_gain = 0.33f*dfact;
            flt->no_stages = stages;
            flt->lfo = 0;
            flt->fs = fs;
            flt->Q = Q;
            flt->k = k;
         }
      }
      while(0);

      break;
   }
   case AAX_FALSE:
      _aaxRingBufferDelaysRemove(&effect->slot[0]->data);
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
_aaxNewReverbEffectHandle(_aaxMixerInfo* info, enum aaxEffectType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   unsigned int size = sizeof(_effect_t) + sizeof(_aaxEffectInfo);
   _effect_t* rv = calloc(1, size);

   if (rv)
   {
      char *ptr = (char*)rv + sizeof(_effect_t);

      rv->id = EFFECT_ID;
      rv->info = info ? info : _info;
      rv->slot[0] = (_aaxEffectInfo*)ptr;
      rv->pos = _eff_cvt_tbl[type].pos;
      rv->state = p2d->effect[rv->pos].state;
      rv->type = type;

      size = sizeof(_aaxEffectInfo);
      memcpy(rv->slot[0], &p2d->effect[rv->pos], size);
      rv->slot[0]->data = NULL;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxReverbEffect =
{
   AAX_TRUE,
   "AAX_reverb_effect",
   (_aaxEffectCreate*)&_aaxReverbEffectCreate,
   (_aaxEffectDestroy*)&_aaxReverbEffectDestroy,
   (_aaxEffectSetState*)&_aaxReverbEffectSetState,
   (_aaxNewEffectHandle*)&_aaxNewReverbEffectHandle
};

