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
_aaxFlangingEffectCreate(aaxConfig config, enum aaxEffectType type)
{
   _handle_t *handle = get_handle(config);
   aaxEffect rv = NULL;
   if (handle)
   {
      unsigned int size = sizeof(_effect_t) + sizeof(_aaxEffectInfo);
     _effect_t* eff = calloc(1, size);

      if (eff)
      {
         char *ptr;

         eff->id = EFFECT_ID;
         eff->state = AAX_FALSE;
         if VALID_HANDLE(handle) eff->info = handle->info;

         ptr = (char*)eff + sizeof(_effect_t);
         eff->slot[0] = (_aaxEffectInfo*)ptr;
         eff->pos = _eff_cvt_tbl[type].pos;
         eff->type = type;

         size = sizeof(_aaxEffectInfo);
         _aaxSetDefaultEffect2d(eff->slot[0], eff->pos);
         rv = (aaxEffect)eff;
      }
   }
   return rv;
}

static int
_aaxFlangingEffectDestroy(aaxEffect f)
{
   int rv = AAX_FALSE;
   _effect_t* effect = get_effect(f);
   if (effect)
   {
      _aaxRingBufferDelayEffectData* data = effect->slot[0]->data;
      if (data) free(data->history_ptr);

      free(effect->slot[0]->data);
      effect->slot[0]->data = NULL;
      free(effect);
      rv = AAX_TRUE;
   }
   return rv;
}

static aaxEffect
_aaxFlangingEffectSetState(aaxEffect e, int state)
{
   _effect_t* effect = get_effect(e);
   aaxEffect rv = AAX_FALSE;
   unsigned slot;

   assert(e);

   effect->state = state;
   effect->slot[0]->state = state;

   /*
    * Make sure parameters are actually within their expected boundaries.
    */
   slot = 0;
   while ((slot < _MAX_FE_SLOTS) && effect->slot[slot])
   {
      int i, type = effect->type;
      for(i=0; i<4; i++)
      {
         if (!is_nan(effect->slot[slot]->param[i]))
         {
            float min = _eff_minmax_tbl[slot][type].min[i];
            float max = _eff_minmax_tbl[slot][type].max[i];
            cvtfn_t cvtfn = effect_get_cvtfn(effect->type, AAX_LINEAR, WRITEFN, i);
            effect->slot[slot]->param[i] =
                      _MINMAX(cvtfn(effect->slot[slot]->param[i]), min, max);
         }
      }
      slot++;
   }

#if !ENABLE_LITE
   if EBF_VALID(effect)
   {
      switch (state & ~AAX_INVERSE)
      {
      case AAX_CONSTANT_VALUE:
      case AAX_TRIANGLE_WAVE:
      case AAX_SINE_WAVE:
      case AAX_SQUARE_WAVE:
      case AAX_SAWTOOTH_WAVE:
      case AAX_ENVELOPE_FOLLOW:
      {
         _aaxRingBufferDelayEffectData* data = effect->slot[0]->data;
         if (data == NULL)
         {
            int t;
            data  = malloc(sizeof(_aaxRingBufferDelayEffectData));
            effect->slot[0]->data = data;
            data->history_ptr = 0;
            for (t=0; t<_AAX_MAX_SPEAKERS; t++)
            {
               data->lfo.value[t] = 0.0f;
               data->lfo.step[t] = 0.0f;
            }
         }

         if (data)
         {
            float depth = effect->slot[0]->param[AAX_LFO_DEPTH];
            float offset = effect->slot[0]->param[AAX_LFO_OFFSET];
            unsigned int tracks = effect->info->no_tracks;
            float sign, range, step;
            float fs = 48000.0f;

            if (effect->info) {
               fs = effect->info->frequency;
            }

            if ((offset + depth) > 1.0f) {
               depth = 1.0f - offset;
            }

            // AAX_FLANGING_EFFECT
            range = (60e-3f - 10e-3f);		// 10ms .. 60ms
            depth *= range * fs;		// convert to samples
            data->lfo.min = (range * offset + 10e-3f)*fs;
            data->loopback = AAX_TRUE;
            _aaxRingBufferCreateHistoryBuffer(&data->history_ptr,
                                              data->delay_history,
                                              fs, tracks,
                                              DELAY_EFFECTS_TIME);
            // AAX_FLANGING_EFFECT

            data->lfo.convert = _linear;
            data->lfo.max = data->lfo.min + depth;
            data->lfo.f = effect->slot[0]->param[AAX_LFO_FREQUENCY];
            data->delay.gain = effect->slot[0]->param[AAX_DELAY_GAIN];
            data->lfo.inv = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;

            if (depth > 0.05f)
            {
               int t;
               for (t=0; t<_AAX_MAX_SPEAKERS; t++)
               {
                  // slowly work towards the new settings
                  step = data->lfo.step[t];
                  sign = step ? (step/fabsf(step)) : 1.0f;

                  data->lfo.step[t] = 2.0f*sign * data->lfo.f;
                  data->lfo.step[t] *= (data->lfo.max - data->lfo.min);
                  data->lfo.step[t] /= effect->info->period_rate;

                  if ((data->lfo.value[t] == 0)
                      || (data->lfo.value[t] < data->lfo.min)) {
                     data->lfo.value[t] = data->lfo.min;
                  } else if (data->lfo.value[t] > data->lfo.max) {
                     data->lfo.value[t] = data->lfo.max;
                  }
                  data->delay.sample_offs[t] = (size_t)data->lfo.value[t];

                  switch (state & ~AAX_INVERSE)
                  {
                  case AAX_SAWTOOTH_WAVE:
                     data->lfo.step[t] *= 0.5f;
                     break;
                  case AAX_ENVELOPE_FOLLOW:
                  {
                     float fact = data->lfo.f;
                     data->lfo.value[t] /= data->lfo.max;
                     data->lfo.step[t] = ENVELOPE_FOLLOW_STEP_CVT(fact);
                     break;
                  }
                  default:
                     break;
                  }
               }

               switch (state & ~AAX_INVERSE)
               {
               case AAX_CONSTANT_VALUE: /* equals to AAX_TRUE */
                  data->lfo.get = _aaxRingBufferLFOGetFixedValue;
                  break;
               case AAX_TRIANGLE_WAVE:
                  data->lfo.get = _aaxRingBufferLFOGetTriangle;
                  break;
               case AAX_SINE_WAVE:
                  data->lfo.get = _aaxRingBufferLFOGetSine;
                  break;
               case AAX_SQUARE_WAVE:
                  data->lfo.get = _aaxRingBufferLFOGetSquare;
                  break;
               case AAX_SAWTOOTH_WAVE:
                  data->lfo.get = _aaxRingBufferLFOGetSawtooth;
                  break;
               case AAX_ENVELOPE_FOLLOW:
                   data->lfo.get = _aaxRingBufferLFOGetGainFollow;
                   data->lfo.envelope = AAX_TRUE;
                  break;
               default:
                  _aaxErrorSet(AAX_INVALID_PARAMETER);
                  break;
               }
            }
            else
            {
               int t;
               for (t=0; t<_AAX_MAX_SPEAKERS; t++)
               {
                  data->lfo.value[t] = data->lfo.min;
                  data->delay.sample_offs[t] = (size_t)data->lfo.value[t];
               }
               data->lfo.get = _aaxRingBufferLFOGetFixedValue;
            }
         }
         else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         break;
      }
      case AAX_FALSE:
      {
         _aaxRingBufferDelayEffectData* data = effect->slot[0]->data;
         if (data) data->lfo.envelope = AAX_FALSE;

         effect->slot[0]->data = NULL;
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      }
   }
#endif
   rv = effect;
   return rv;
}

static _effect_t*
_aaxNewFlangingEffectHandle(_aaxMixerInfo* info, enum aaxEffectType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   _effect_t* rv = NULL;
   if (type < AAX_EFFECT_MAX)
   {
      unsigned int size = sizeof(_effect_t) + sizeof(_aaxEffectInfo);

      rv = calloc(1, size);
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
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxFlangingEffect =
{
   "AAX_flanging_effect",
   (_aaxEffectCreate*)&_aaxFlangingEffectCreate,
   (_aaxEffectDestroy*)&_aaxFlangingEffectDestroy,
   (_aaxEffectSetState*)&_aaxFlangingEffectSetState,
   (_aaxNewEffectHandle*)&_aaxNewFlangingEffectHandle
};

