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

#include "api.h"
#include "arch.h"


AAX_API aaxEffect AAX_APIENTRY
aaxEffectCreate(aaxConfig config, enum aaxEffectType type)
{
   _handle_t *handle = get_handle(config);
   aaxEffect rv = NULL;
   if (handle)
   {
      unsigned int size = sizeof(_effect_t) sizeof(_aaxEffectInfo);
     _effect_t* eff = calloc(1, size);

      if (eff)
      {
         char *ptr;
         int i;

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

AAX_API int AAX_APIENTRY
aaxEffectDestroy(aaxEffect f)
{
   int rv = AAX_FALSE;
   _effect_t* effect = get_effect(f);
   if (effect)
   {
      free(effect);
      rv = AAX_TRUE;
   }
   return rv;
}

AAX_API aaxEffect AAX_APIENTRY
aaxEffectSetState(aaxEffect e, int state)
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

         lfo->min = 0.15f;
         lfo->max = 0.99f;
         lfo->f = 0.33f;
         lfo->inv = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;
         lfo->convert = _linear;

         for(t=0; t<_AAX_MAX_SPEAKERS; t++)
         {
            lfo->value[t] = 0.0f;
            lfo->step[t] = ENVELOPE_FOLLOW_STEP_CVT(lfo->f);
         }

         lfo->get = _aaxRingBufferLFOGetGainFollow;
         lfo->envelope = AAX_TRUE;
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

/* -------------------------------------------------------------------------- */

_effect_t*
new_effect_handle(_aaxMixerInfo* info, enum aaxEffectType type, _aax2dProps* p2d, _aax3dProps* p3d)
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

