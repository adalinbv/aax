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
# include <string.h>
#endif

#include "common.h"
#include "effects.h"

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
_aaxSetDefaultEffect2d(_aaxEffectInfo *effect, unsigned int type)
{
   assert(type < MAX_STEREO_EFFECT);

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
   case AAX_REVERB_EFFECT:
      effect->param[AAX_CUTOFF_FREQUENCY] = 10000.0f;
      effect->param[AAX_DELAY_DEPTH] = 0.27f;
      effect->param[AAX_DECAY_LEVEL] = 0.3f;
      effect->param[AAX_DECAY_DEPTH] = 0.7f;
      break;
   default:
      break;
   }
}

void
_aaxSetDefaultEffect3d(_aaxEffectInfo *effect, unsigned int type)
{
   assert(type < MAX_3D_EFFECT);

   memset(effect, 0, sizeof(_aaxEffectInfo));
   switch(type)
   {
   case VELOCITY_EFFECT:
      effect->param[AAX_SOUND_VELOCITY] = 343.0f;
      effect->param[AAX_DOPPLER_FACTOR] = 1.0f;
      effect->state = AAX_TRUE;
      effect->data = *(void**)&_aaxRingBufferDopplerFn[0];
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

