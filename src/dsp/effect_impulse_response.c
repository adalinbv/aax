/*
 * Copyright 2007-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#include "effects.h"
#include "api.h"
#include "arch.h"


static aaxEffect
_aaxImpulseResponseEffectCreate(_handle_t *handle, enum aaxEffectType type)
{
   unsigned int size = sizeof(_effect_t) + sizeof(_aaxEffectInfo);
   _effect_t* eff = calloc(1, size);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxRingBufferImpulseResponseData* data;
      char *ptr;

      eff->id = EFFECT_ID;
      eff->state = AAX_FALSE;
      eff->info = handle->info ? handle->info : _info;

      ptr = (char*)eff + sizeof(_effect_t);
      eff->slot[0] = (_aaxEffectInfo*)ptr;
      eff->pos = _eff_cvt_tbl[type].pos;
      eff->type = type;

      size = sizeof(_aaxEffectInfo);
      _aaxSetDefaultEffect2d(eff->slot[0], eff->pos);

      data = calloc(1, sizeof(_aaxRingBufferImpulseResponseData));
      eff->slot[0]->data = data;

      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxImpulseResponseEffectDestroy(_effect_t* effect)
{
   _aaxRingBufferImpulseResponseData* data = effect->slot[0]->data;
   if (data)
   {
      free(data->history_ptr);
      free(data->ir_ptr);
   }
   free(effect->slot[0]->data);
   effect->slot[0]->data = NULL;
   free(effect);

   return AAX_TRUE;
}

static aaxEffect
_aaxImpulseResponseEffectSetState(_effect_t* effect, int state)
{
   effect->slot[0]->state = state ? AAX_TRUE : AAX_FALSE;
   return effect;
}

static aaxEffect
_aaxImpulseResponseEffectSetData(_effect_t* effect, aaxBuffer buffer)
{
   _aaxRingBufferImpulseResponseData *ird = effect->slot[0]->data;
   void *handle = effect->handle;
   aaxEffect rv = AAX_FALSE;

   if (ird)
   {
      unsigned int no_samples, tracks = effect->info->no_tracks;
      float dt, fs = 48000.0f;
      size_t size;

      dt = no_samples = aaxBufferGetSetup(buffer, AAX_NO_SAMPLES);
      dt /= aaxBufferGetSetup(buffer, AAX_FREQUENCY);

      if (effect->info) {
         fs = effect->info->frequency;
      }

      /* convert the buffer data to mixer frequency and format */
      aaxBufferSetSetup(buffer, AAX_FORMAT, AAX_PCM24S);
      aaxBufferSetSetup(buffer, AAX_FREQUENCY, fs);

      free(ird->ir_ptr);
      ird->ir_ptr = aaxBufferGetData(buffer);
      ird->impulse_repsonse = (MIX_T*)(*ird->ir_ptr);
      ird->no_samples = no_samples;

      free(ird->history_ptr);
      size = _aaxRingBufferCreateHistoryBuffer(&ird->history_ptr,
                                    (int32_t**)ird->ir_history, fs, tracks, dt);
      ird->history_size = size;
      rv = effect;
   }
   else {
      _aaxErrorSet(AAX_INVALID_STATE);
   }

   return rv;
}

_effect_t*
_aaxNewImpulseResponseEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   unsigned int size = sizeof(_effect_t) + sizeof(_aaxEffectInfo);
   _effect_t* rv = calloc(1, size);

   if (rv)
   {
      _handle_t *handle = get_driver_handle(config);
      _aaxMixerInfo* info = handle ? handle->info : _info;
      char *ptr = (char*)rv + sizeof(_effect_t);

      rv->id = EFFECT_ID;
      rv->info = info;
      rv->handle = handle;
      rv->slot[0] = (_aaxEffectInfo*)ptr;
      rv->pos = _eff_cvt_tbl[type].pos;
      rv->state = p2d->effect[rv->pos].state;
      rv->type = type;

      size = sizeof(_aaxEffectInfo);
      memcpy(rv->slot[0], &p2d->effect[rv->pos], size);
      rv->slot[0]->data = NULL;
   }
   return rv;
}

static float
_aaxImpulseResponseEffectSet(float val, int ptype, unsigned char param)
{  
   float rv = val;
   return rv;
}
   
static float
_aaxImpulseResponseEffectGet(float val, int ptype, unsigned char param)
{  
   float rv = val;
   return rv;
}

static float
_aaxImpulseResponseEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxImpulseResponseRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f, 0.0f,  0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f, 0.0f,  0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f, 0.0f,  0.0f, 0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxImpulseResponseRange[slot].min[param],
                       _aaxImpulseResponseRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxImpulseResponseEffect =
{
   AAX_TRUE,
   "AAX_impulse_reponse_effect", 1.0f,
   (_aaxEffectCreate*)&_aaxImpulseResponseEffectCreate,
   (_aaxEffectDestroy*)&_aaxImpulseResponseEffectDestroy,
   (_aaxEffectSetState*)&_aaxImpulseResponseEffectSetState,
   (_aaxEffectSetData*)&_aaxImpulseResponseEffectSetData,
   (_aaxNewEffectHandle*)&_aaxNewImpulseResponseEffectHandle,
   (_aaxEffectConvert*)&_aaxImpulseResponseEffectSet,
   (_aaxEffectConvert*)&_aaxImpulseResponseEffectGet,
   (_aaxEffectConvert*)&_aaxImpulseResponseEffectMinMax
};

