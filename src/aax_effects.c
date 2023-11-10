/*
 * SPDX-FileCopyrightText: Copyright © 2007-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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

#include <base/memory.h>
#include <base/types.h>		/*  for rintf */
#include <base/gmath.h>

#include <dsp/effects.h>

#include "api.h"
#include "arch.h"
#include "ringbuffer.h"

AAX_API aaxEffect AAX_APIENTRY
aaxEffectCreate(aaxDSP config, enum aaxEffectType type)
{
   _handle_t *handle = get_handle(config, __func__);
   _aaxMixerInfo *info = (handle && handle->info) ? handle->info : _info;
   aaxEffect rv = NULL;

   if (info && (type > 0 && type < AAX_EFFECT_MAX))
   {
      _eff_function_tbl *eff = _aaxEffects[type-1];
      rv = eff->create(info, type);
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

AAX_API int AAX_APIENTRY
aaxEffectSetSlot(aaxEffect e, unsigned slot, int ptype, float p1, float p2, float p3, float p4)
{
   aaxVec4f v = { p1, p2, p3, p4 };
   return aaxEffectSetSlotParams(e, slot, ptype, v);
}

AAX_API int AAX_APIENTRY
aaxEffectSetSlotParams(aaxEffect e, unsigned slot, int ptype, aaxVec4f p)
{
   int rv = AAX_FALSE;
   _effect_t* effect = get_effect(e);
   if (effect)
   {
      void *handle = effect->handle;
      if (p)
      {
         if ((slot < _MAX_FE_SLOTS) && effect->slot[slot])
         {
            int i;
            for(i=0; i<4; i++)
            {
               if (!is_nan(p[i]))
               {
                  _eff_function_tbl *eff = _aaxEffects[effect->type-1];
                  int pn = slot << 4 | i;
                  effect->slot[slot]->param[i] =
                                 eff->limit(eff->get(p[i], ptype, pn), slot, i);
               }
            }
            if TEST_FOR_TRUE(effect->state) {
               rv = aaxEffectSetState(effect, effect->state);
            } else {
               rv = AAX_TRUE;
            }
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER + 1);
      }
   }
   else {
      __aaxErrorSet(AAX_INVALID_HANDLE, __func__);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEffectSetParam(const aaxEffect e, int p, int ptype, float value)
{
   _effect_t* effect = get_effect(e);
   unsigned slot = p >> 4;
   int param = p & 0xF;
   int rv = __release_mode;

   if (!rv)
   {
      void *handle = effect ? effect->handle : NULL;
      if (!effect) {
         __aaxErrorSet(AAX_INVALID_HANDLE, __func__);
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
      effect->slot[slot]->param[param] = eff->get(value, ptype, p);
      
      if TEST_FOR_TRUE(effect->state) {
         aaxEffectSetState(effect, effect->state);
      }
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEffectAddBuffer(aaxEffect eff, aaxBuffer buf)
{
   _effect_t* effect = get_effect(eff);
   _buffer_t* buffer = get_buffer(buf, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      void *handle = effect ? effect->handle : NULL;
      if (!effect) {
         __aaxErrorSet(AAX_INVALID_HANDLE, __func__);
      } else if (!buffer) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
      else
      {
         _aaxRingBuffer *rb = buffer->ringbuffer[0];
         if (!rb->get_state(rb, RB_IS_VALID)) {
            _aaxErrorSet(AAX_INVALID_STATE+1);
         } else if (rb->get_parami(rb, RB_NO_TRACKS) > _AAX_MAX_SPEAKERS) {
            _aaxErrorSet(AAX_INVALID_STATE+1);
         }
         else
         {
            _eff_function_tbl *eff = _aaxEffects[effect->type-1];
            if (!eff->data) {
               _aaxErrorSet(AAX_INVALID_STATE);
            } else {
               rv = AAX_TRUE;
            }
         }
      }
   }

   if (rv)
   {
      _eff_function_tbl *eff = _aaxEffects[effect->type-1];
      eff->data(effect, buf);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEffectSetState(aaxEffect e, int state)
{
   _effect_t* effect = get_effect(e);
   int rv = AAX_FALSE;
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
            int i;
            for(i=0; i<4; i++)
            {
               if (!is_nan(effect->slot[slot]->param[i]))
               {
                  effect->slot[slot]->param[i] =
                              eff->limit(effect->slot[slot]->param[i], slot, i);
               }
            }
            slot++;
         }

         rv = eff->state(effect, state) ? AAX_TRUE : AAX_FALSE;
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
aaxEffectGetParam(const aaxEffect e, int p, int ptype)
{
   _effect_t* effect = get_effect(e);
   float rv = 0.0f;
   if (effect)
   {
      void *handle = effect->handle;
      unsigned slot = p >> 4;
      if ((slot < _MAX_FE_SLOTS) && effect->slot[slot])
      {
         int param = p & 0xF;
         if ((param >= 0) && (param < 4))
         {
            _eff_function_tbl *eff = _aaxEffects[effect->type-1];
            rv = eff->set(effect->slot[slot]->param[param], ptype, p);
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
      __aaxErrorSet(AAX_INVALID_HANDLE, __func__);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEffectGetSlot(const aaxEffect e, unsigned slot, int ptype, float* p1, float* p2, float* p3, float* p4)
{
   aaxVec4f v = { 0.0f, 0.0f, 0.0f, 0.0f };
   int rv = aaxEffectGetSlotParams(e, slot, ptype, v);
   if (rv)
   {
      if(p1) *p1 = v[0];
      if(p2) *p2 = v[1];
      if(p3) *p3 = v[2];
      if(p4) *p4 = v[3];
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEffectGetSlotParams(const aaxEffect e, unsigned slot, int ptype, aaxVec4f p)
{
   _effect_t* effect = get_effect(e);
   int rv = AAX_FALSE;
   if (effect)
   {
      void *handle = effect->handle;
      if (p)
      {
         if ((slot < _MAX_FE_SLOTS) && effect->slot[slot])
         {
            int i;
            for (i=0; i<4; i++)
            {
               _eff_function_tbl *eff = _aaxEffects[effect->type-1];
               int pn = slot << 4 | i;
               p[i] = eff->set(effect->slot[slot]->param[i], ptype, pn);
            }
            rv = AAX_TRUE;
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
      }
   }
   else {
      __aaxErrorSet(AAX_INVALID_HANDLE, __func__);
   }
   return rv;
}

AAX_API const char* AAX_APIENTRY
aaxEffectGetNameByType(aaxConfig handle, enum aaxEffectType type)
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
