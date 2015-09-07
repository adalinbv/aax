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
   &_aaxReverbEffect
};

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
  { AAX_REVERB_EFFECT,          REVERB_EFFECT }
};

const _eff_minmax_tbl_t _eff_minmax_tbl[_MAX_FE_SLOTS][AAX_EFFECT_MAX] =
{    /* min[4] */                  /* max[4] */
  {
    /* AAX_EFFECT_NONE      */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_PITCH_EFFECT     */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {    2.00f,    2.00f, 0.0f,     0.0f } },
    /* AAX_DYNAMIC_PITCH_EFFECT   */
    { { 1.0f, 0.01f, 0.0f, 0.0f }, {     1.0f,    50.0f, 1.0f,     1.0f } },
    /* AAX_TIMED_PITCH_EFFECT */
    { {  0.0f, 0.0f, 0.0f, 0.0f }, {     4.0f, MAXFLOAT, 4.0f, MAXFLOAT } },
    /* AAX_DISTORTION_EFFECT */
    { {  0.0f, 0.0f, 0.0f, 0.0f }, {     4.0f,     1.0f, 1.0f,     1.0f } },
    /* AAX_PHASING_EFFECT   */
    { { 0.0f, 0.01f, 0.0f, 0.0f }, {     1.0f,    10.0f, 1.0f,     1.0f } },
    /* AAX_CHORUS_EFFECT    */
    { { 0.0f, 0.01f, 0.0f, 0.0f }, {     1.0f,    10.0f, 1.0f,     1.0f } },
    /* AAX_FLANGING_EFFECT  */
    { { 0.0f, 0.01f, 0.0f, 0.0f }, {     1.0f,    10.0f, 1.0f,     1.0f } },
    /* AAX_VELOCITY_EFFECT  */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, { MAXFLOAT,    10.0f, 0.0f,     0.0f } },
    /* AAX_REVERB_EFFECT     */
    { {50.0f, 0.0f,  0.0f, 0.0f }, { 22000.0f,    0.07f, 1.0f,     0.7f } }
  },
  {
    /* AAX_EFFECT_NONE      */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_PITCH_EFFECT     */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_DYNAMIC_PITCH_EFFECT */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_TIMED_PITCH_EFFECT */
    { {  0.0f, 0.0f, 0.0f, 0.0f }, {     4.0f, MAXFLOAT, 4.0f, MAXFLOAT } },
    /* AAX_DISTORTION_EFFECT */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_PHASING_EFFECT   */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_CHORUS_EFFECT    */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_FLANGING_EFFECT  */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_VELOCITY_EFFECT  */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_REVERB_EFFECT     */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } }
  },
  {
    /* AAX_EFFECT_NONE      */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_PITCH_EFFECT     */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_DYNAMIC_PITCH_EFFECT */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_TIMED_PITCH_EFFECT */
    { {  0.0f, 0.0f, 0.0f, 0.0f }, {     4.0f, MAXFLOAT, 4.0f, MAXFLOAT } },
    /* AAX_DISTORTION_EFFECT */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_PHASING_EFFECT   */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_CHORUS_EFFECT    */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_FLANGING_EFFECT  */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_VELOCITY_EFFECT  */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
    /* AAX_REVERB_EFFECT     */
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } }
  }
};

cvtfn_t
effect_get_cvtfn(enum aaxEffectType type, int ptype, int mode, char param)
{
   cvtfn_t rv = _lin;
   switch (type)
   {
   case AAX_PHASING_EFFECT:
   case AAX_CHORUS_EFFECT:
   case AAX_FLANGING_EFFECT:
      if ((param == 0) && (ptype == AAX_LOGARITHMIC))
      {
         if (mode == WRITEFN) {
            rv = _lin2db;
         } else {
            rv = _db2lin;
         }
      }
      break;
   default:
      break;
   }
   return rv;
}

