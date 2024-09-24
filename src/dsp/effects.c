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
# include <string.h>
#endif

#include "common.h"
#include "effects.h"
#include "arch.h"

static int _eff_cvt_tbl[AAX_EFFECT_MAX];
static int _cvt_eff_tbl[MAX_STEREO_EFFECT];

aaxEffect
_aaxEffectCreateHandle(_aaxMixerInfo *info, enum aaxEffectType type, unsigned slots, size_t dsize)
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
      eff->state = false;
      eff->info = info;

      ptr = (char*)eff + sizeof(_effect_t);
      eff->slot[0] = (_aaxEffectInfo*)ptr;
      eff->pos = _eff_cvt_tbl[type];
      eff->type = type;

      size = sizeof(_aaxEffectInfo);
      for (s=0; s<slots; ++s) {
         eff->slot[s] = (_aaxEffectInfo*)(ptr + s*size);
      }
      eff->slot[0]->swap = _aax_dsp_swap;
      eff->slot[0]->destroy = _aax_dsp_destroy;

      eff->slot[0]->data_size = dsize;
      if (dsize)
      {
         eff->slot[0]->data = _aax_aligned_alloc(dsize);
         if (eff->slot[0]->data) {
            memset(eff->slot[0]->data, 0, dsize);
         }
      }

      rv = (aaxEffect)eff;
   }
   return rv;
}

bool
_aaxEffectDestroy(aaxEffect* dsp)
{
   _effect_t* eff = (_effect_t*)dsp;
   if (eff->slot[0]->data)
   {
      eff->slot[0]->destroy(eff->slot[0]->data);
      eff->slot[0]->data_size = 0;
      eff->slot[0]->data = NULL;
   }
   free(eff);

   return true;
}

_eff_function_tbl *_aaxEffects[AAX_EFFECT_MAX] =
{ // order matches enum aaxEffectType
   &_aaxPitchEffect,
   &_aaxDynamicPitchEffect,
   &_aaxTimedPitchEffect,
   &_aaxDistortionEffect,
   &_aaxPhasingEffect,
   &_aaxChorusEffect,
   &_aaxFlangingEffect,
   &_aaxVelocityEffect,
   &_aaxReverbEffect,
   &_aaxConvolutionEffect,
   &_aaxModulatorEffect,
   &_aaxDelayLineEffect,
   &_aaxWaveFoldEffect,
   &_aaxFrequencyShiftEffect
};

_effect_t*
new_effect_handle(const void *config, enum aaxEffectType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   _effect_t* rv = NULL;
   if (type > 0 && type <= AAX_EFFECT_MAX)
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
reset_effect(_aax2dProps* p2d, enum _aax2dFiltersEffects type)
{
   assert(type >= 0);
   assert(type < MAX_STEREO_EFFECT);

   int pos = _cvt_eff_tbl[type];
   _eff_function_tbl *eff = _aaxEffects[pos-1];
   void *data = _EFFECT_GET_DATA(p2d, type);
   if (eff->reset && data) eff->reset(data);
}

void
_aaxSetDefaultEffect2d(_aaxEffectInfo *effect, unsigned int type, unsigned slot)
{
   assert(type < MAX_STEREO_EFFECT);
   assert(slot < _MAX_FE_SLOTS);

   effect->state = 0;
   effect->updated = 0;
   memset(effect->param, 0, sizeof(float[4]));
   switch(type)
   {
   case PITCH_EFFECT:
      effect->param[AAX_PITCH] = 1.0f;
      effect->param[AAX_MAX_PITCH] = 4.0f;
      effect->param[AAX_PITCH_START] = 1.0f;
      effect->state = true;
      break;
   case TIMED_PITCH_EFFECT:
      effect->param[AAX_LEVEL0] = 1.0f;
      effect->param[AAX_LEVEL1] = 1.0f;
      break;
   case DISTORTION_EFFECT:
      effect->param[AAX_CLIPPING_FACTOR] = 0.3f;
      effect->param[AAX_ASYMMETRY] = 0.7f;
      break;
   case RINGMODULATE_EFFECT:
      effect->param[AAX_GAIN] = 1.0f;
      break;
   case REVERB_EFFECT:
      if (slot == 0)
      {
         effect->param[AAX_CUTOFF_FREQUENCY] = 0.5f*MAXIMUM_CUTOFF;
         effect->param[AAX_DELAY_DEPTH] = 0.027f;
         effect->param[AAX_DECAY_LEVEL] = 0.3f;
         effect->param[AAX_DECAY_DEPTH] = 0.7f;
      }
      break;
   case CONVOLUTION_EFFECT:
      effect->param[AAX_CUTOFF_FREQUENCY] = MAXIMUM_CUTOFF;
      effect->param[AAX_LF_GAIN] = 1.0f;
      effect->param[AAX_MAX_GAIN] = 1.0f;
      effect->param[AAX_THRESHOLD] = LEVEL_64DB;
      break;
   default:
      break;
   }
}

void
_aaxSetDefaultEffect3d(_aaxEffectInfo *effect, unsigned int type, unsigned slot)
{
   assert(type < MAX_3D_EFFECT);

   effect->state = 0;
   effect->updated = 0;
   memset(effect->param, 0, sizeof(float[4]));
   switch(type)
   {
   case REVERB_OCCLUSION_EFFECT:
      if (slot == 0)
      {
         effect->param[AAX_CUTOFF_FREQUENCY] = MAXIMUM_CUTOFF;
         effect->param[AAX_DELAY_DEPTH] = 0.27f;
         effect->param[AAX_DECAY_LEVEL] = 0.3f;
         effect->param[AAX_DECAY_DEPTH] = 0.7f;
      }
      break;
   case VELOCITY_EFFECT:
   {
      _aaxRingBufferVelocityEffectData *data = effect->data;
      size_t dsize = sizeof(_aaxRingBufferVelocityEffectData);

      data = _aax_aligned_alloc(dsize);
      if (data)
      {
         effect->data = data;
         effect->data_size = dsize;
         effect->destroy = _velocity_destroy;
         effect->swap = _velocity_swap;

         memset(data, 0, dsize);
         data->dopplerfn = _aaxDopplerFn[0];
         data->prepare = _velocity_prepare;
         data->run = _velocity_run;
      }

      effect->param[AAX_SOUND_VELOCITY] = 343.0f;
      effect->param[AAX_DOPPLER_FACTOR] = 1.0f;
//    effect->param[AAX_LIGHT_VELOCITY] = 299792458.0f;
      effect->state = true;
      break;
   }
   default:
      break;
   }
}

/* -------------------------------------------------------------------------- */

static int _eff_cvt_tbl[AAX_EFFECT_MAX] =
{
  MAX_STEREO_EFFECT,		// AAX_EFFECT_NONE
  PITCH_EFFECT,			// AAX_PITCH_EFFECT
  DYNAMIC_PITCH_EFFECT,		// AAX_DYNAMIC_PITCH_EFFECT
  TIMED_PITCH_EFFECT,		// AAX_TIMED_PITCH_EFFECT
  DISTORTION_EFFECT,		// AAX_DISTORTION_EFFECT
  DELAY_EFFECT,			// AAX_PHASING_EFFECT
  DELAY_EFFECT,			// AAX_CHORUS_EFFECT
  DELAY_EFFECT,			// AAX_FLANGING_EFFECT
  VELOCITY_EFFECT,		// AAX_VELOCITY_EFFECT
  REVERB_EFFECT,		// AAX_REVERB_EFFECT
  CONVOLUTION_EFFECT,		// AAX_CONVOLUTION_EFFECT
  RINGMODULATE_EFFECT,		// AAX_RINGMODULATE_EFFECT
  DELAY_LINE_EFFECT,		// AAX_DELAY_EFFECT
  WAVEFOLD_EFFECT,		// AAX_WAVEFOLD_EFFECT
  FREQUENCY_SHIFT_EFFECT	// AAX_FREQUENCY_SHIFT_EFFECT
};

static int _cvt_eff_tbl[MAX_STEREO_EFFECT] =
{
  AAX_PITCH_EFFECT,		// PITCH_EFFECT
  AAX_REVERB_EFFECT,		// REVERB_EFFECT
  AAX_CONVOLUTION_EFFECT,	// CONVOLUTION_EFFECT
  AAX_DYNAMIC_PITCH_EFFECT,	// DYNAMIC_PITCH_EFFECT
  AAX_TIMED_PITCH_EFFECT,	// TIMED_PITCH_EFFECT
  AAX_DISTORTION_EFFECT,	// DISTORTION_EFFECT
  AAX_CHORUS_EFFECT,		// DELAY_EFFECT
  AAX_RINGMODULATOR_EFFECT,	// RINGMODULATE_EFFECT
  AAX_DELAY_EFFECT,		// DELAY EFFECT
  AAX_WAVEFOLD_EFFECT,		// WAVEFOLD_EFFECT
  AAX_FREQUENCY_SHIFT_EFFECT	// FREQUENCY_SHIFT_EFFECT
};
