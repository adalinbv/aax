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
# include <string.h>
#endif

#include "common.h"
#include "effects.h"

aaxEffect
_aaxEffectCreateHandle(_aaxMixerInfo *info, enum aaxEffectType type, unsigned slots)
{
   aaxEffect rv = NULL;
   unsigned int size;
   _effect_t* eff;

   size = sizeof(_effect_t) + slots*sizeof(_aaxEffectInfo);
   eff = calloc(1, size);
   if (eff)
   {
      unsigned s;
      char *ptr;

      eff->id = EFFECT_ID;
      eff->state = AAX_FALSE;
      eff->info = info;

      ptr = (char*)eff + sizeof(_effect_t);
      eff->slot[0] = (_aaxEffectInfo*)ptr;
      eff->pos = _eff_cvt_tbl[type].pos;
      eff->type = type;

      size = sizeof(_aaxEffectInfo);
      for (s=0; s<slots; ++s) {
         eff->slot[s] = (_aaxEffectInfo*)(ptr + s*size);
      }

      rv = (aaxEffect)eff;
   }
   return rv;
}

_eff_function_tbl *_aaxEffects[AAX_EFFECT_MAX] =
{
   &_aaxPitchEffect,
   &_aaxDynamicPitchEffect,
   &_aaxTimedPitchEffect,
   &_aaxDistortionEffect,
   &_aaxPhasingEffect,
   &_aaxChorusEffect,
   &_aaxFlangingEffect,
   &_aaxVelocityEffect,
   &_aaxReverbEffect,
   &_aaxConvolutionEffect
};

_effect_t*
new_effect_handle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   _effect_t* rv = NULL;
   if (type <= AAX_EFFECT_MAX)
   {
      _eff_function_tbl *eff = _aaxEffects[type-1];
      rv = eff->handle(config, type, p2d, p3d);
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
   else if (rv && rv->id == FADEDBAD) {
      __aaxErrorSet(AAX_DESTROYED_HANDLE, __func__);
   }

   return NULL;
}

void
_aaxSetDefaultEffect2d(_aaxEffectInfo *effect, unsigned int type, unsigned slot)
{
   assert(type < MAX_STEREO_EFFECT);
   assert(slot < _MAX_FE_SLOTS);

   memset(effect, 0, sizeof(_aaxEffectInfo));
   switch(type)
   {
   case PITCH_EFFECT:
      effect->param[AAX_PITCH] = 1.0f;
      effect->param[AAX_MAX_PITCH] = 4.0f;
      effect->state = AAX_TRUE;
      break;
   case TIMED_PITCH_EFFECT:
      effect->param[AAX_LEVEL0] = 1.0f;
      effect->param[AAX_LEVEL1] = 1.0f;
      break;
   case DISTORTION_EFFECT:
      effect->param[AAX_CLIPPING_FACTOR] = 0.3f;
      effect->param[AAX_ASYMMETRY] = 0.7f;
      break;
   case REVERB_EFFECT:
      if (slot == 0) {
         effect->param[AAX_CUTOFF_FREQUENCY] = 15000.0f;
         effect->param[AAX_DELAY_DEPTH] = 0.27f;
         effect->param[AAX_DECAY_LEVEL] = 0.3f;
         effect->param[AAX_DECAY_DEPTH] = 0.7f;
      }
      break;
   case CONVOLUTION_EFFECT:
      effect->param[AAX_CUTOFF_FREQUENCY] = 22050.0f;
      effect->param[AAX_LF_GAIN] = 1.0f;
      effect->param[AAX_MAX_GAIN] = 1.0f;
      effect->param[AAX_THRESHOLD] = LEVEL_64DB;
      break;
   default:
      break;
   }
}

void
_aaxSetDefaultEffect3d(_aaxEffectInfo *effect, unsigned int type, UNUSED(unsigned slot))
{
   assert(type < MAX_3D_EFFECT);

   memset(effect, 0, sizeof(_aaxEffectInfo));
   switch(type)
   {
   case REVERB_OCCLUSION_EFFECT:
      if (slot == 0) {
         effect->param[AAX_CUTOFF_FREQUENCY] = 15000.0f;
         effect->param[AAX_DELAY_DEPTH] = 0.27f;
         effect->param[AAX_DECAY_LEVEL] = 0.3f;
         effect->param[AAX_DECAY_DEPTH] = 0.7f;
      }
      break;
   case VELOCITY_EFFECT:
      effect->param[AAX_SOUND_VELOCITY] = 343.0f;
      effect->param[AAX_DOPPLER_FACTOR] = 1.0f;
//    effect->param[AAX_LIGHT_VELOCITY] = 299792458.0f;
      effect->state = AAX_TRUE;
      effect->data = *(void**)&_aaxDopplerFn[0];
      break;
   default:
      break;
   }
}

/* -------------------------------------------------------------------------- */

const _eff_cvt_tbl_t _eff_cvt_tbl[AAX_EFFECT_MAX] =
{
  { AAX_EFFECT_NONE,            MAX_STEREO_EFFECT },
  { AAX_PITCH_EFFECT,           PITCH_EFFECT },
  { AAX_DYNAMIC_PITCH_EFFECT,   DYNAMIC_PITCH_EFFECT },
  { AAX_TIMED_PITCH_EFFECT,     TIMED_PITCH_EFFECT },
  { AAX_DISTORTION_EFFECT,      DISTORTION_EFFECT},
  { AAX_PHASING_EFFECT,         DELAY_EFFECT },
  { AAX_CHORUS_EFFECT,          DELAY_EFFECT },
  { AAX_FLANGING_EFFECT,        DELAY_EFFECT },
  { AAX_VELOCITY_EFFECT,        VELOCITY_EFFECT },
  { AAX_REVERB_EFFECT,          REVERB_EFFECT },
  { AAX_CONVOLUTION_EFFECT,     CONVOLUTION_EFFECT }
};

