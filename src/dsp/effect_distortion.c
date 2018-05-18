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
# include <stdlib.h>
# include <malloc.h>
#endif

#include <aax/aax.h>

#include <base/types.h>		/*  for rintf */
#include <base/gmath.h>

#include <software/rbuf_int.h>
#include "effects.h"
#include "api.h"
#include "arch.h"

static void _distortion_run(void*, MIX_PTR_T, CONST_MIX_PTR_T, size_t, size_t, size_t, unsigned int, void*, void*);

static aaxEffect
_aaxDistortionEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 1);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxSetDefaultEffect2d(eff->slot[0], eff->pos, 0);
      eff->slot[0]->destroy = destroy;
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxDistortionEffectDestroy(_effect_t* effect)
{
   effect->slot[0]->destroy(effect->slot[0]->data);
   effect->slot[0]->data = NULL;
   free(effect);

   return AAX_TRUE;
}

static aaxEffect
_aaxDistortionEffectSetState(_effect_t* effect, int state)
{
   void *handle = effect->handle;
   aaxEffect rv = AAX_FALSE;

   effect->state = state;
   switch (state & ~AAX_INVERSE)
   {
   case AAX_ENVELOPE_FOLLOW:
   {
      _aaxRingBufferDistoritonData *data = effect->slot[0]->data;

      effect->slot[0]->destroy(data);
      if (data == NULL)
      {
         data = malloc(sizeof(_aaxRingBufferDistoritonData) +
                       sizeof(_aaxLFOData));
         effect->slot[0]->data = data;
         if (data)
         {
            char *ptr = (char*)data + sizeof(_aaxRingBufferDistoritonData);
            data->lfo = (_aaxLFOData*)ptr;
         }
      }

      if (data)
      {
         _aaxLFOData *lfo = data->lfo;
         int constant;

         data->run = _distortion_run;

         lfo->convert = _linear; // _log2lin;
         lfo->state = effect->state;
         lfo->fs = effect->info->frequency;
         lfo->period_rate = effect->info->period_rate;

         lfo->min_sec = 0.15f/lfo->fs;
         lfo->max_sec = 0.99f/lfo->fs;
         lfo->f = 0.33f;
         lfo->depth = 1.0f;
         lfo->offset = 0.0f;
         lfo->inv = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;
         lfo->stereo_lnk = AAX_TRUE;

         constant = _lfo_set_timing(lfo);
         if (!_lfo_set_function(lfo, constant)) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
      }
      break;
   }
   case AAX_CONSTANT_VALUE:
   case AAX_FALSE:
   {
      _aaxRingBufferDistoritonData *data = effect->slot[0]->data;

      effect->slot[0]->destroy(data);
      if (data == NULL)
      {
         data = malloc(sizeof(_aaxRingBufferDistoritonData));
         effect->slot[0]->data = data;
      }

      if (data)
      {
         data->run = _distortion_run;
         data->lfo = NULL;
      }
      break;
   }
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
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
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 1);

   if (rv)
   {
      unsigned int size = sizeof(_aaxEffectInfo);

      memcpy(rv->slot[0], &p2d->effect[rv->pos], size);
      rv->slot[0]->destroy = destroy;
      rv->slot[0]->data = NULL;

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
   "AAX_distortion_effect", 1.0f,
   (_aaxEffectCreate*)&_aaxDistortionEffectCreate,
   (_aaxEffectDestroy*)&_aaxDistortionEffectDestroy,
   (_aaxEffectSetState*)&_aaxDistortionEffectSetState,
   NULL,
   (_aaxNewEffectHandle*)&_aaxNewDistortionEffectHandle,
   (_aaxEffectConvert*)&_aaxDistortionEffectSet,
   (_aaxEffectConvert*)&_aaxDistortionEffectGet,
   (_aaxEffectConvert*)&_aaxDistortionEffectMinMax
};

void
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

   _aax_memcpy(dptr, sptr, no_samples*bps);
   if (mix > 0.01f)
   {
      float mix_factor;

      /* make dptr the wet signal */
      if (fact > 0.0013f) {
         rbd->multiply(dptr, bps, no_samples, 1.0f+64.0f*fact);
      }

      if ((fact > 0.01f) || (asym > 0.01f))
      {
         size_t average = 0;
         size_t peak = no_samples;
         _aaxRingBufferLimiter(dptr, &average, &peak, clip, 4*asym);
      }

      /* mix with the dry signal */
      mix_factor = mix/(0.5f+powf(fact, 0.25f));
      rbd->multiply(dptr, bps, no_samples, mix_factor);
      if (mix < 0.99f) {
         rbd->add(dptr, sptr, no_samples, 1.0f-mix, 0.0f);
      }  
   }
}

