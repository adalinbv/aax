/*
 * Copyright 2007-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to thcd parties, copied or
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
_aaxConvolutionEffectCreate(_handle_t *handle, enum aaxEffectType type)
{
   unsigned int size = sizeof(_effect_t) + sizeof(_aaxEffectInfo);
   _effect_t* eff = calloc(1, size);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxRingBufferConvolutionData* data;
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

      data = calloc(1, sizeof(_aaxRingBufferConvolutionData));
      eff->slot[0]->data = data;

      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxConvolutionEffectDestroy(_effect_t* effect)
{
   _aaxRingBufferConvolutionData* data = effect->slot[0]->data;
   if (data)
   {
      free(data->history_ptr);
      free(data->sample_ptr);
   }
   free(effect->slot[0]->data);
   effect->slot[0]->data = NULL;
   free(effect);

   return AAX_TRUE;
}

static aaxEffect
_aaxConvolutionEffectSetState(_effect_t* effect, int state)
{
   effect->slot[0]->state = state ? AAX_TRUE : AAX_FALSE;
   return effect;
}

static aaxEffect
_aaxConvolutionEffectSetData(_effect_t* effect, aaxBuffer buffer)
{
   _aaxRingBufferConvolutionData *convolution = effect->slot[0]->data;
   void *handle = effect->handle;
   aaxEffect rv = AAX_FALSE;

   if (convolution && effect->info)
   {
      unsigned int tracks = effect->info->no_tracks;
      unsigned int no_samples = effect->info->no_samples;
      float fs = effect->info->frequency;
      void **data;

      /*
       * convert the buffer data to floats in the range 0.0 .. 1.0
       * using the mixer frequency
       */
      aaxBufferSetSetup(buffer, AAX_FORMAT, AAX_FLOAT);
      aaxBufferSetSetup(buffer, AAX_FREQUENCY, fs);
      data = aaxBufferGetData(buffer);

      convolution->no_samples = aaxBufferGetSetup(buffer, AAX_NO_SAMPLES);
      no_samples += convolution->no_samples;

      free(convolution->sample_ptr);
      convolution->sample_ptr = data;
      convolution->sample = *data;

      free(convolution->history_ptr);
      _aaxRingBufferCreateHistoryBuffer(&convolution->history_ptr,
                                        (int32_t**)convolution->history,
                                        no_samples, tracks);
      convolution->history_samples = no_samples;
      rv = effect;
   }
   else {
      _aaxErrorSet(AAX_INVALID_STATE);
   }

   return rv;
}

_effect_t*
_aaxNewConvolutionEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, _aax3dProps* p3d)
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
_aaxConvolutionEffectSet(float val, int ptype, unsigned char param)
{  
   float rv = val;
   return rv;
}
   
static float
_aaxConvolutionEffectGet(float val, int ptype, unsigned char param)
{  
   float rv = val;
   return rv;
}

static float
_aaxConvolutionEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxConvolutionRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f, 0.0f,  0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f, 0.0f,  0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f, 0.0f,  0.0f, 0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxConvolutionRange[slot].min[param],
                       _aaxConvolutionRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxConvolutionEffect =
{
   AAX_TRUE,
   "AAX_convolution_effect", 1.0f,
   (_aaxEffectCreate*)&_aaxConvolutionEffectCreate,
   (_aaxEffectDestroy*)&_aaxConvolutionEffectDestroy,
   (_aaxEffectSetState*)&_aaxConvolutionEffectSetState,
   (_aaxEffectSetData*)&_aaxConvolutionEffectSetData,
   (_aaxNewEffectHandle*)&_aaxNewConvolutionEffectHandle,
   (_aaxEffectConvert*)&_aaxConvolutionEffectSet,
   (_aaxEffectConvert*)&_aaxConvolutionEffectGet,
   (_aaxEffectConvert*)&_aaxConvolutionEffectMinMax
};

