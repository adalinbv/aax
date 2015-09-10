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
_aaxDistortionEffectCreate(_handle_t *handle, enum aaxEffectType type)
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
_aaxDistortionEffectDestroy(_effect_t* effect)
{
   free(effect);

   return AAX_TRUE;
}

static aaxEffect
_aaxDistortionEffectSetState(_effect_t* effect, int state)
{
   aaxEffect rv = AAX_FALSE;

   switch (state & ~AAX_INVERSE)
   {
   case AAX_ENVELOPE_FOLLOW:
   {
      _aaxRingBufferLFOData* lfo = effect->slot[0]->data;
      if (lfo == NULL)
      {
         lfo = malloc(sizeof(_aaxRingBufferLFOData));
         effect->slot[0]->data = lfo;
      }

      if (lfo)
      {
         int t;

         lfo->inv = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;
         lfo->convert = _linear; // _log2lin;
         lfo->min = 0.15f;
         lfo->max = 0.99f;
         lfo->f = 0.33f;

         for(t=0; t<_AAX_MAX_SPEAKERS; t++)
         {
            lfo->step[t] = 0.0f;
            lfo->value[t] = 0.0f;
            switch (state & ~AAX_INVERSE)
            {
            case AAX_SAWTOOTH_WAVE:
               lfo->step[t] *= 0.5f;
               break;
            case AAX_ENVELOPE_FOLLOW:
               lfo->f = 0.33f;
               lfo->value[t] = 0.0f;
               lfo->step[t] = ENVELOPE_FOLLOW_STEP_CVT(lfo->f);
               break;
            default:
               break;
            }
         }

         switch (state & ~AAX_INVERSE)
         {
         case AAX_SINE_WAVE:
            lfo->get = _aaxRingBufferLFOGetSine;
            break;
         default:
            lfo->get = _aaxRingBufferLFOGetGainFollow;
            lfo->envelope = AAX_TRUE;
            break;
         }
      }
      break;
   }
   case AAX_CONSTANT_VALUE:
   case AAX_FALSE:
      free(effect->slot[0]->data);
      effect->slot[0]->data = NULL;
      break;
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
      break;
   }
   rv = effect;
   return rv;
}

static _effect_t*
_aaxNewDistortionEffectHandle(_aaxMixerInfo* info, enum aaxEffectType type, _aax2dProps* p2d, _aax3dProps* p3d)
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

_eff_function_tbl _aaxDistortionEffect =
{
   AAX_FALSE,
   "AAX_distortion_effect", 1.0f,
   (_aaxEffectCreate*)&_aaxDistortionEffectCreate,
   (_aaxEffectDestroy*)&_aaxDistortionEffectDestroy,
   (_aaxEffectSetState*)&_aaxDistortionEffectSetState,
   (_aaxNewEffectHandle*)&_aaxNewDistortionEffectHandle
};

