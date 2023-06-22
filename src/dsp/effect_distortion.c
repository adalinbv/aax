/*
 * Copyright 2007-2023 by Erik Hofman.
 * Copyright 2009-2023 by Adalin B.V.
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

#include <aax/aax.h>

#include <base/types.h>		/*  for rintf */
#include <base/gmath.h>

#include <software/rbuf_int.h>
#include "effects.h"
#include "api.h"
#include "arch.h"

#define VERSION	1.02
#define DSIZE	sizeof(_aaxRingBufferDistoritonData)

static int _distortion_run(void*, MIX_PTR_T, CONST_MIX_PTR_T, size_t, size_t, size_t, unsigned int, void*, void*);

static aaxEffect
_aaxDistortionEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 1, DSIZE);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxSetDefaultEffect2d(eff->slot[0], eff->pos, 0);
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxDistortionEffectDestroy(_effect_t* effect)
{
   if (effect->slot[0]->data)
   {
      effect->slot[0]->destroy(effect->slot[0]->data);
      effect->slot[0]->data = NULL;
   }
   free(effect);

   return AAX_TRUE;
}

static int
_aaxDistortionEffectReset(void *data)
{
   return AAX_TRUE;
}


static aaxEffect
_aaxDistortionEffectSetState(_effect_t* effect, int state)
{
   void *handle = effect->handle;
   aaxEffect rv = AAX_FALSE;

   effect->state = state;
   switch (state & AAX_SOURCE_MASK)
   {
   case AAX_RANDOMNESS:
   case AAX_TIMED_TRANSITION:
   case AAX_ENVELOPE_FOLLOW:
   {
      _aaxRingBufferDistoritonData *data = effect->slot[0]->data;

      if (data) effect->slot[0]->destroy(data);
      data = _aax_aligned_alloc(DSIZE + sizeof(_aaxLFOData));
      effect->slot[0]->data = data;
      if (data)
      {
         _aaxLFOData *lfo;
         int constant;
         char *ptr;

         memset(data, 0, DSIZE + sizeof(_aaxLFOData));

         ptr = (char*)data + DSIZE;
         data->lfo = (_aaxLFOData*)ptr;

         data->run = _distortion_run;

         lfo = data->lfo;
         _lfo_setup(lfo, effect->info, effect->state);
         if (state & AAX_LFO_EXPONENTIAL) {
            lfo->convert = _exp_distortion;
         }

         lfo->min_sec = 0.15f/lfo->fs;
         lfo->max_sec = 0.99f/lfo->fs;
         lfo->f = 3.33f;

         constant = _lfo_set_timing(lfo);
         if (!_lfo_set_function(lfo, constant)) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
      }
      break;
   }
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
      // inetnional fall-through
   case AAX_CONSTANT:
   case AAX_FALSE:
      do {
         _aaxRingBufferDistoritonData *data = effect->slot[0]->data;

         if (data) effect->slot[0]->destroy(data);
         data = _aax_aligned_alloc(DSIZE);
         effect->slot[0]->data = data;
         if (data)
         {
            memset(data, 0, DSIZE);
            data->run = _distortion_run;
            data->lfo = NULL;
         }
      } while (0);
      break;
   }
   rv = effect;
   return rv;
}

static _effect_t*
_aaxNewDistortionEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 1, 0);

   if (rv)
   {
      _aax_dsp_copy(rv->slot[0], &p2d->effect[rv->pos]);
      rv->state = p2d->effect[rv->pos].state;
   }
   return rv;
}

static float
_aaxDistortionEffectSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{  
   float rv = val;
   return rv;
}
   
static float
_aaxDistortionEffectGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{  
   float rv = val;
   return rv;
}

static float
_aaxDistortionEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxDistortionRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, 1.0f, 1.0f, 1.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxDistortionRange[slot].min[param],
                       _aaxDistortionRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxDistortionEffect =
{
   AAX_FALSE,
   "AAX_distortion_effect_"AAX_MKSTR(VERSION), VERSION,
   (_aaxEffectCreate*)&_aaxDistortionEffectCreate,
   (_aaxEffectDestroy*)&_aaxDistortionEffectDestroy,
   (_aaxEffectReset*)&_aaxDistortionEffectReset,
   (_aaxEffectSetState*)&_aaxDistortionEffectSetState,
   NULL,
   (_aaxNewEffectHandle*)&_aaxNewDistortionEffectHandle,
   (_aaxEffectConvert*)&_aaxDistortionEffectSet,
   (_aaxEffectConvert*)&_aaxDistortionEffectGet,
   (_aaxEffectConvert*)&_aaxDistortionEffectMinMax
};

static int
_distortion_run(void *rb, MIX_PTR_T d, CONST_MIX_PTR_T s,
                size_t dmin, size_t dmax, size_t ds,
                unsigned int track, void *data, void *env)
{
   static const size_t bps = sizeof(MIX_T);
   _aaxRingBufferSample *rbd = (_aaxRingBufferSample*)rb;
   _aaxFilterInfo *dist_effect = (_aaxFilterInfo*)data;
   _aaxRingBufferDistoritonData *dist_data = dist_effect->data;
   _aaxLFOData* lfo = dist_data->lfo;
   float *params = dist_effect->param;
   float clip, asym, fact, mix;
   int rv = AAX_FALSE;
   size_t no_samples;
   float lfo_fact = 1.0;
   CONST_MIX_PTR_T sptr;
   MIX_T *dptr;


   _AAX_LOG(LOG_DEBUG, __func__);

   assert(s != 0);
   assert(d != 0);
   assert(data != 0);
   assert(dmin < dmax);
   assert(data != NULL);
   assert(track < _AAX_MAX_SPEAKERS);

   sptr = s - ds + dmin;
   dptr = d - ds + dmin;

   no_samples = dmax+ds-dmin;
// DBG_MEMCLR(1, d-ds, ds+dmax, bps);

   if (lfo) {
      lfo_fact = lfo->get(lfo, env, sptr, track, no_samples);
   }
   fact = params[AAX_DISTORTION_FACTOR]*lfo_fact;
   clip = params[AAX_CLIPPING_FACTOR];
   mix  = params[AAX_MIX_FACTOR]*lfo_fact;
   asym = params[AAX_ASYMMETRY];

   if (mix > 0.01f && fact > 0.0013f)
   {
      float mix_factor;

      _aax_memcpy(dptr, sptr, no_samples*bps);

      /* make dptr the wet signal */
      if (fact > 0.0013f) {
         rbd->multiply(dptr, dptr, bps, no_samples, 1.0f+64.0f*fact);
      }

      if ((fact > 0.01f) || (asym > 0.01f)) {
         _aaxRingBufferLimiter(dptr, no_samples, clip, 4*asym);
      }

      /* mix with the dry signal */
      mix_factor = mix/(0.5f+powf(fact, 0.25f));
      rbd->multiply(dptr, dptr, bps, no_samples, mix_factor);
      if (mix < 0.99f) {
         rbd->add(dptr, sptr, no_samples, 1.0f-mix, 0.0f);
      }  

      rv = AAX_TRUE;
   }
   return rv;
}

