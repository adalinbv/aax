/*
 * Copyright 2007-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
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

#include <dsp/effects.h>

#include "api.h"
#include "arch.h"

AAX_API aaxEffect AAX_APIENTRY
aaxEffectCreate(aaxConfig config, enum aaxEffectType type)
{
   _handle_t *handle = get_handle(config);
   aaxEffect rv = NULL;
   if (handle && (type < AAX_EFFECT_MAX))
   {
      _eff_function_tbl *eff = _aaxEffects[type-1];
      rv = eff->create(handle, type);
   }
   return rv;

}

AAX_API int AAX_APIENTRY
aaxEffectDestroy(aaxEffect e)
{
   _effect_t* effect = get_effect(e);
   int rv = AAX_FALSE;
   if (effect)
   {
      _eff_function_tbl *eff = _aaxEffects[effect->type-1];
      rv = eff->destroy(e);
   }
   return rv;
}

AAX_API aaxEffect AAX_APIENTRY
aaxEffectSetSlot(aaxEffect e, unsigned slot, int ptype, float p1, float p2, float p3, float p4)
{
   aaxVec4f v = { p1, p2, p3, p4 };
   return aaxEffectSetSlotParams(e, slot, ptype, v);
}

AAX_API aaxEffect AAX_APIENTRY
aaxEffectSetSlotParams(aaxEffect e, unsigned slot, int ptype, aaxVec4f p)
{
   aaxEffect rv = AAX_FALSE;
   _effect_t* effect = get_effect(e);
   if (effect && p)
   {
      if ((slot < _MAX_FE_SLOTS) && effect->slot[slot])
      {
         int i, type = effect->type;
         for(i=0; i<4; i++)
         {
            if (!is_nan(p[i]))
            {
               float min = _eff_minmax_tbl[slot][type].min[i];
               float max = _eff_minmax_tbl[slot][type].max[i];
               _eff_function_tbl *eff = _aaxEffects[effect->type-1];
               effect->slot[slot]->param[i] = _MINMAX(eff->get(p[i], ptype, i), min, max);
            }
         }
         if TEST_FOR_TRUE(effect->state) {
            rv = aaxEffectSetState(effect, effect->state);
         } else {
            rv = effect;
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEffectSetParam(const aaxEffect e, int param, int ptype, float value)
{
   _effect_t* effect = get_effect(e);
   unsigned slot = param >> 4;
   int rv = __release_mode;

   param &= 0xF;
   if (!rv)
   {
      if (!effect) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if ((slot >=_MAX_FE_SLOTS) || !effect->slot[slot]) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (param < 0 || param >= 4) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (is_nan(value)) {
         _aaxErrorSet(AAX_INVALID_PARAMETER + 2);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _eff_function_tbl *eff = _aaxEffects[effect->type-1];
      effect->slot[slot]->param[param] = eff->get(value, ptype, param);
      
      if TEST_FOR_TRUE(effect->state) {
         aaxEffectSetState(effect, effect->state);
      }
   }
   return rv;
}


AAX_API aaxEffect AAX_APIENTRY
aaxEffectSetState(aaxEffect e, int state)
{
   _effect_t* effect = get_effect(e);
   aaxEffect rv = NULL;
   if (effect)
   {
      _eff_function_tbl *eff = _aaxEffects[effect->type-1];
      if (eff->lite || EBF_VALID(effect))
      {
         unsigned slot;

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
                  effect->slot[slot]->param[i] =
                         _MINMAX(effect->slot[slot]->param[i], min, max);
               }
            }
            slot++;
         }

         rv = eff->state(effect, state);
      }
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEffectGetState(aaxEffect e)
{
   _effect_t* effect = get_effect(e);
   int rv = AAX_FALSE;
   if (effect) {
      rv = effect->state;
   }
   return rv;
}


AAX_API float AAX_APIENTRY
aaxEffectGetParam(const aaxEffect e, int param, int ptype)
{
   _effect_t* effect = get_effect(e);
   float rv = 0.0f;
   if (effect)
   {
      unsigned slot = param >> 4;
      if ((slot < _MAX_FE_SLOTS) && effect->slot[slot])
      {
         param &= 0xF;
         if ((param >= 0) && (param < 4))
         {
            _eff_function_tbl *eff = _aaxEffects[effect->type-1];
            rv = eff->set(effect->slot[slot]->param[param], ptype, param);
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER + 1);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API aaxEffect AAX_APIENTRY
aaxEffectGetSlot(const aaxEffect e, unsigned slot, int ptype, float* p1, float* p2, float* p3, float* p4)
{
   aaxVec4f v;
   aaxEffect rv = aaxEffectGetSlotParams(e, slot, ptype, v);
   if(p1) *p1 = v[0];
   if(p2) *p2 = v[1];
   if(p3) *p3 = v[2];
   if(p4) *p4 = v[3];
   return rv;
}

AAX_API aaxEffect AAX_APIENTRY
aaxEffectGetSlotParams(const aaxEffect e, unsigned slot, int ptype, aaxVec4f p)
{
   _effect_t* effect = get_effect(e);
   aaxEffect rv = AAX_FALSE;
   if (effect && p)
   {
      if ((slot < _MAX_FE_SLOTS) && effect->slot[slot])
      {
         int i;
         for (i=0; i<4; i++)
         {
            _eff_function_tbl *eff = _aaxEffects[effect->type-1];
            p[i] = eff->set(effect->slot[slot]->param[i], ptype, i);
         }
         rv = effect;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API const char* AAX_APIENTRY
aaxEffectGetNameByType(aaxConfig cfg, enum aaxEffectType type)
{
   const char *rv = NULL;
   if (type < AAX_EFFECT_MAX) {
      rv = _aaxEffects[type-1]->name;
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

/* internal use only, used by aaxdefs.h */
AAX_API aaxEffect AAX_APIENTRY
aaxEffectApply(aaxEffectFn fn, void *handle, aaxEffect e)
{
   if (e)
   {
      if (!fn(handle, e))
      {
         aaxEffectDestroy(e);
         e = NULL;
      }
   }
   return e;
}

AAX_API float AAX_APIENTRY
aaxEffectApplyParam(const aaxEffect e, int s, int p, int ptype)
{
   float rv = 0.0f;
   if ((p >= 0) && (p < 4))
   {
      _effect_t* effect = get_effect(e);
      if (effect)
      {
         _eff_function_tbl *eff = _aaxEffects[effect->type-1];
         rv = eff->set(effect->slot[0]->param[p], ptype, p);
         free(effect);
      }
   }
   return rv;
}

_effect_t*
new_effect_handle(_aaxMixerInfo* info, enum aaxEffectType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   _effect_t* rv = NULL;
   if (type <= AAX_EFFECT_MAX)
   {
      _eff_function_tbl *eff = _aaxEffects[type-1];
      rv = eff->handle(info, type, p2d, p3d);
   }
   return rv;
}

_effect_t*
get_effect(const aaxEffect e)
{
   _effect_t* rv = (_effect_t*)e;
   if (rv && rv->id == EFFECT_ID) {
      return rv;
   }
   return NULL;
}

