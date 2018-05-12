/*
 * Copyright 2007-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#include <dsp/effects.h>

#include "api.h"
#include "arch.h"
#include "ringbuffer.h"

AAX_API aaxEffect AAX_APIENTRY
aaxEffectCreate(aaxConfig config, enum aaxEffectType type)
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
                  effect->slot[slot]->param[i] =
                                  eff->limit(eff->get(p[i], ptype, i), slot, i);
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
aaxEffectSetParam(const aaxEffect e, int param, int ptype, float value)
{
   _effect_t* effect = get_effect(e);
   unsigned slot = param >> 4;
   int rv = __release_mode;

   param &= 0xF;
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
      effect->slot[slot]->param[param] = eff->get(value, ptype, param);
      
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
         _aaxRingBuffer *rb = buffer->ringbuffer;
         if (!rb->get_state(rb, RB_IS_VALID)) {
            _aaxErrorSet(AAX_INVALID_STATE+1);
         } else if (rb->get_parami(rb, RB_NO_TRACKS) != 1) {
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
aaxEffectGetParam(const aaxEffect e, int param, int ptype)
{
   _effect_t* effect = get_effect(e);
   float rv = 0.0f;
   if (effect)
   {
      void *handle = effect->handle;
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
      __aaxErrorSet(AAX_INVALID_HANDLE, __func__);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEffectGetSlot(const aaxEffect e, unsigned slot, int ptype, float* p1, float* p2, float* p3, float* p4)
{
   aaxVec4f v;
   int rv = aaxEffectGetSlotParams(e, slot, ptype, v);
   if(p1) *p1 = v[0];
   if(p2) *p2 = v[1];
   if(p3) *p3 = v[2];
   if(p4) *p4 = v[3];
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
               p[i] = eff->set(effect->slot[slot]->param[i], ptype, i);
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

AAX_API enum aaxEffectType AAX_APIENTRY
aaxEffectGetByName(UNUSED(aaxConfig handle), const char *name)
{
   enum aaxEffectType rv = AAX_EFFECT_NONE;
   char type[256];
   int i, slen;
   char *end;

   if (!strncasecmp(name, "AAX_", 4)) {
      name += 4;
   }
   
   strncpy(type, name, 256);
   name = type;
   type[255] = 0;
   
   slen = strlen(name);
   for (i=0; i<slen; ++i) {
      if (type[i] == '-') type[i] = '_';
   }
   
   end = strchr(name, '.');
   while (end > name && *end != '_') --end;
   if (end) type[end-name] = 0;

   end = strrchr(name, '_');
   if (end && !strcasecmp(end+1, "EFFECT")) {
      type[end-name] = 0;
   } 
   slen = strlen(name);

   if (!strncasecmp(name, "pitch", slen)) {
      rv = AAX_PITCH_EFFECT;
   }
   else if (!strncasecmp(name, "dynamic_pitch", slen) ||
            !strncasecmp(name, "vibrato", slen)) {
      rv = AAX_DYNAMIC_PITCH_EFFECT;
   }
   else if (!strncasecmp(name, "timed_pitch", slen) ||
            !strncasecmp(name, "envelope", slen)) {
      rv = AAX_TIMED_PITCH_EFFECT;
   }
   else if (!strncasecmp(name, "distortion", slen)) {
      rv = AAX_DISTORTION_EFFECT;
   }
   else if (!strncasecmp(name, "phasing", slen)) {
      rv = AAX_PHASING_EFFECT;
   }
   else if (!strncasecmp(name, "chorus", slen)) {
      rv = AAX_CHORUS_EFFECT;
   }
   else if (!strncasecmp(name, "flanging", slen)) {
      rv = AAX_FLANGING_EFFECT;
   }
   else if (!strncasecmp(name, "reverb", slen)) {
      rv = AAX_REVERB_EFFECT;
   }
   else if (!strncasecmp(name, "convolution", slen)) {
      rv = AAX_CONVOLUTION_EFFECT;
   }

   else if (!strncasecmp(name, "velocity", slen)) {
      rv = AAX_VELOCITY_EFFECT;
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }

   return rv;
}

