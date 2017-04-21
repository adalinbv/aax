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
      _aaxSetDefaultEffect3d(eff->slot[0], eff->pos);

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
      unsigned int t;
      for (t=0; t<_AAX_MAX_SPEAKERS; ++t) 
      {
         _aaxThreadJoin(data->tid[t]);
         _aaxThreadDestroy(data->tid[t]);
      }

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
      unsigned int freq, tracks = effect->info->no_tracks;
      unsigned int no_samples = effect->info->no_samples;
      _aaxRingBufferFreqFilterData *flt;
      float fs = effect->info->frequency;
      float fc, lfgain;
      void **data;

      convolution->fc = effect->slot[0]->param[AAX_CUTOFF_FREQUENCY];
      convolution->delay_gain = effect->slot[0]->param[AAX_MAX_GAIN];
      convolution->threshold = effect->slot[0]->param[AAX_THRESHOLD];

      flt = convolution->freq_filter;
      fc = convolution->fc;
      lfgain = effect->slot[0]->param[AAX_LF_GAIN];

      if (fc < 15000.0f || lfgain < (1.0f - LEVEL_96DB))
      {
         if (!flt) {
            flt = calloc(1, sizeof(_aaxRingBufferFreqFilterData));
         }

         flt->lfo = 0;
         flt->fs = fs;
         flt->Q = 0.6f;
         flt->no_stages = 1;

         flt->high_gain = _lin2log(fc)/4.343409f;
         flt->low_gain = lfgain;
         flt->k = flt->low_gain/flt->high_gain;

         _aax_butterworth_compute(fc, flt);
      }
      else if (convolution->freq_filter)
      {
         free(flt);
         flt = NULL;
      }
      convolution->freq_filter = flt;

      /*
       * convert the buffer data to floats in the range 0.0 .. 1.0
       * using the mixer frequency
       */
      freq = aaxBufferGetSetup(buffer, AAX_FREQUENCY);
      convolution->step = (int)fs/freq;
      if (effect->info->no_cores == 1 || effect->info->sse_level < AAX_SIMD_AVX)
      {
         if (freq > 16000) convolution->step = (int)fs/16000;
      }
      aaxBufferSetSetup(buffer, AAX_FORMAT, AAX_FLOAT);
      aaxBufferSetSetup(buffer, AAX_FREQUENCY, fs);
      data = aaxBufferGetData(buffer);

      if (data)
      {
         size_t buffer_samples = aaxBufferGetSetup(buffer, AAX_NO_SAMPLES);
         float *start = *data;
         float *end =  start + buffer_samples;

         while (end > start && fabsf(*end--) < convolution->threshold);
         convolution->no_samples = end-start;

         if (end > start)
         {
            unsigned int t;
            float rms, peak;

            _batch_get_average_rms(start, convolution->no_samples, &rms, &peak);
            convolution->rms = rms;

            no_samples += convolution->no_samples;

            free(convolution->sample_ptr);
            convolution->sample_ptr = data;
            convolution->sample = *data;

            free(convolution->history_ptr);
            _aaxRingBufferCreateHistoryBuffer(&convolution->history_ptr,
                                              (int32_t**)convolution->history,
                                              2*no_samples, tracks);
            convolution->history_samples = no_samples;
            convolution->history_max = 2*no_samples;

            for (t=0; t<_AAX_MAX_SPEAKERS; ++t)
            {
               convolution->tid[t] = _aaxThreadCreate();
               convolution->history_start[t] = 0;
            }
            
            rv = effect;
         }
      }
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
   if (ptype == AAX_LOGARITHMIC) {
      rv = _lin2db(val);
   }
   return rv;
}
   
static float
_aaxConvolutionEffectGet(float val, int ptype, unsigned char param)
{  
   float rv = val;
   if (param < 3 && ptype == AAX_LOGARITHMIC) {
      rv = _db2lin(val);
   }
   return rv;
}

static float
_aaxConvolutionEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxConvolutionRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 22050.0f, 1.0f, 1.0f, 1.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f, 0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f, 0.0f, 0.0f, 0.0f } }
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

