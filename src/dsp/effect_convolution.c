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

#include <aax/aax.h>

#include <base/types.h>		/*  for rintf */
#include <base/gmath.h>

#include <software/rbuf_int.h>
#include <software/renderer.h>

#include "effects.h"
#include "arch.h"
#include "dsp.h"
#include "api.h"

#define DSIZE	sizeof(_aaxRingBufferConvolutionData)

static void _convolution_swap(void*, void*);
static void _convolution_destroy(void*);
static int _convolution_run(const _aaxDriverBackend*, const void*, void*, void*);

static aaxEffect
_aaxConvolutionEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 2, DSIZE);
   aaxEffect rv = NULL;

   if (eff)
   {

      _aaxSetDefaultEffect3d(eff->slot[0], eff->pos, 0);
      _aaxSetDefaultEffect3d(eff->slot[1], eff->pos, 1);
      eff->slot[0]->destroy = _convolution_destroy;
      eff->slot[0]->swap = _convolution_swap;
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxConvolutionEffectDestroy(_effect_t* effect)
{
   if (effect->slot[0]->data)
   {
      effect->slot[0]->destroy(effect->slot[0]->data);
      effect->slot[0]->data = NULL;
   }
   free(effect);

   return true;
}

static aaxEffect
_aaxConvolutionEffectSetState(_effect_t* effect, int state)
{
   void *handle = effect->handle;

   effect->slot[0]->state = state ? true : false;
   switch (state & ~AAX_INVERSE)
   {
   case AAX_CONSTANT:
   {
      _aaxRingBufferConvolutionData *convolution = effect->slot[0]->data;

      if (!convolution)
      {
         convolution = _aax_aligned_alloc(DSIZE);
         effect->slot[0]->data = convolution;
         if (convolution) memset(convolution, 0, DSIZE);
      }

      if (convolution)
      {
         _aaxRingBufferFreqFilterData *flt = convolution->freq_filter;
         float  fs = 48000.0f;

         convolution->run = _convolution_run;

         if (effect->info) {
            fs = effect->info->frequency;
         }

         if (!flt)
         {
            flt = _aax_aligned_alloc(sizeof(_aaxRingBufferFreqFilterData));
            if (flt)
            {
               memset(flt, 0, sizeof(_aaxRingBufferFreqFilterData));
               flt->freqfilter = _aax_aligned_alloc(sizeof(_aaxRingBufferFreqFilterHistoryData));
               if (flt->freqfilter) {
                  memset(flt->freqfilter, 0, sizeof(_aaxRingBufferFreqFilterHistoryData));
               }
               else
               {
                  _aax_aligned_free(flt);
                  flt = NULL;
               }
            }
         }

         convolution->info = effect->info;
         convolution->freq_filter = flt;
         convolution->occlusion = _occlusion_create(convolution->occlusion, effect->slot[1], state, fs);

         convolution->fc = effect->slot[0]->param[AAX_CUTOFF_FREQUENCY];
         convolution->fc = CLIP_FREQUENCY(convolution->fc, fs);

         convolution->delay_gain = effect->slot[0]->param[AAX_MAX_GAIN];
         convolution->threshold = effect->slot[0]->param[AAX_THRESHOLD];

         if (flt)
         {
            float lfgain = effect->slot[0]->param[AAX_LF_GAIN];
            float fc = convolution->fc;

            flt->run = _freqfilter_run;

            flt->lfo = NULL;
            flt->fs = fs;
            flt->Q = 0.6f;
            flt->no_stages = 1;

            flt->high_gain = _lin2log(fc)/4.343409f;
            flt->low_gain = lfgain;
            flt->k = flt->low_gain/flt->high_gain;

            _aax_butterworth_compute(fc, flt);
         }
      }
      break;
   }
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
      // intentional fall-through
   case AAX_FALSE:
      if (effect->slot[0]->data)
      {
         effect->slot[0]->destroy(effect->slot[0]->data);
         effect->slot[0]->data = NULL;
      }
      break;
   }
   return effect;
}

static aaxEffect
_aaxConvolutionEffectSetData(_effect_t* effect, aaxBuffer buffer)
{
   _aaxRingBufferConvolutionData *convolution = effect->slot[0]->data;
   _aaxMixerInfo *info = effect->info;
   void *handle = effect->handle;
   aaxEffect rv = false;

   if (!convolution)
   {
      convolution = _aax_aligned_alloc(DSIZE);
      effect->slot[0]->data = convolution;
      if (convolution) memset(convolution, 0, DSIZE);
   }

   if (convolution && info)
   {
      int simd256 = info->capabilities & AAX_SIMD256;
      int no_cores = (info->capabilities & AAX_CPU_CORES)+1;
      int step, freq, tracks = info->no_tracks;
      int no_samples = info->no_samples;
      float fs = info->frequency;
      void **data;

      /*
       * convert the buffer data to floats in the range 0.0 .. 1.0
       * using the mixer frequency
       */
      freq = aaxBufferGetSetup(buffer, AAX_FREQUENCY);
      step = (int)fs/freq;
      if (no_cores == 1 || !simd256) {
         step = (int)fs/16000;
      }
      convolution->step = _MAX(step, 1);

      aaxBufferSetSetup(buffer, AAX_FORMAT, AAX_FLOAT);
      aaxBufferSetSetup(buffer, AAX_FREQUENCY, fs);
      data = aaxBufferGetData(buffer);

      if (data)
      {
         size_t buffer_samples = aaxBufferGetSetup(buffer, AAX_NO_SAMPLES);
         float *start = *data;
         float *end =  start + buffer_samples-1;

         // find the last sample above the threshold
         while (end > start && fabsf(*end--) < convolution->threshold);
         convolution->no_samples = end-start;

         if (end > start)
         {
            float rms, peak;
            int t;

            _batch_get_average_rms(start, convolution->no_samples, &rms, &peak);
            convolution->rms = .25f*rms/peak;

            no_samples += convolution->no_samples;

            if (convolution->sample_ptr) _aax_aligned_free(convolution->sample_ptr);
            convolution->sample_ptr = data;
            convolution->sample = *data;

            if (convolution->history) free(convolution->history);
            _aaxRingBufferCreateHistoryBuffer(&convolution->history,
                                              2*no_samples, tracks);
            convolution->history_samples = no_samples;
            convolution->history_max = 2*no_samples;

            for (t=0; t<_AAX_MAX_SPEAKERS; ++t)
            {
               float dst = info->speaker[t].v4[DIR_RIGHT]*info->frequency*t/343.0;
               convolution->tid[t] = _aaxThreadCreate();
               convolution->history_start[t] = _MAX(dst, 0);
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
_aaxNewConvolutionEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 2, 0);

   if (rv)
   {
      _aax_dsp_copy(rv->slot[1], &p2d->effect[rv->pos]);
      _aax_dsp_copy(rv->slot[0], &p2d->effect[rv->pos]);
      rv->slot[0]->destroy = _convolution_destroy;
      rv->slot[0]->swap = _convolution_swap;

      rv->state = p2d->effect[rv->pos].state;
   }
   return rv;
}

static float
_aaxConvolutionEffectSet(float val, int ptype, UNUSED(unsigned char param))
{  
   float rv = val;
   if (ptype == AAX_DECIBEL) {
      rv = _lin2db(val);
   }
   return rv;
}
   
static float
_aaxConvolutionEffectGet(float val, int ptype, UNUSED(unsigned char param))
{  
   float rv = val;
   if (ptype == AAX_DECIBEL) {
      rv = _db2lin(val);
   }
   return rv;
}

static float
_aaxConvolutionEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxConvolutionRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 22050.0f,    1.0f,    1.0f, 1.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {  FLT_MAX, FLT_MAX, FLT_MAX, 1.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f,    0.0f,    0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f,    0.0f,    0.0f, 0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxConvolutionRange[slot].min[param],
                       _aaxConvolutionRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxConvolutionEffect =
{
   "AAX_convolution_effect", 1.0f,
   (_aaxEffectCreate*)&_aaxConvolutionEffectCreate,
   (_aaxEffectDestroy*)&_aaxConvolutionEffectDestroy,
   NULL,
   (_aaxEffectSetState*)&_aaxConvolutionEffectSetState,
   (_aaxEffectSetData*)&_aaxConvolutionEffectSetData,
   (_aaxNewEffectHandle*)&_aaxNewConvolutionEffectHandle,
   (_aaxEffectConvert*)&_aaxConvolutionEffectSet,
   (_aaxEffectConvert*)&_aaxConvolutionEffectGet,
   (_aaxEffectConvert*)&_aaxConvolutionEffectMinMax
};

void
_convolution_swap(void *d, void *s)
{
   _aaxFilterInfo *dst = d, *src = s;

   if (src->data && src->data_size)
   {
      if (!dst->data) {
          dst->data = _aaxAtomicPointerSwap(&src->data, dst->data);
          dst->data_size = src->data_size;
      }
      else
      {
         _aaxRingBufferConvolutionData *dconv = dst->data;
         _aaxRingBufferConvolutionData *sconv = src->data;

         assert(dst->data_size == src->data_size);

         dconv->fc = sconv->fc;
         dconv->delay_gain = sconv->delay_gain;
         dconv->threshold = dconv->threshold;
      }
   }
   dst->destroy = src->destroy;
   dst->swap = src->swap;
}

static void
_convolution_destroy(void *ptr)
{
   _aaxRingBufferConvolutionData* data = ptr;
   if (data)
   {
      int t;
      for (t=0; t<_AAX_MAX_SPEAKERS; ++t)
      {
         if (data->tid[t])
         {
//       _aaxThreadJoin(data->tid[t]); // this is done by the renderer already
            _aaxThreadDestroy(data->tid[t]);
         }
      }

      if (data->history) free(data->history);
      if (data->sample_ptr) free(data->sample_ptr);
      _occlusion_destroy(data->occlusion);
      _freqfilter_destroy(data->freq_filter);
      _aax_aligned_free(data);
   }
}

/** Convolution Effect */
// irnum = convolution->no_samples
// for (q=0; q<dnum; ++q) {
//    float volume = *sptr++;
//    rbd->add(hptr++, cptr, irnum, volume, 0.0f);
// }
static int
_convolution_thread(_aaxRingBuffer *rb, _aaxRendererData *data, UNUSED(_intBufferData *dptr_src), unsigned int track)
{
   _aaxRingBufferConvolutionData *convolution;
   _aaxRingBufferOcclusionData *occlusion;
   int cnum, dnum, hpos;
   MIX_T *sptr, *dptr, *hptr;
   MIX_T *hcptr, *scratch;
   _aaxRingBufferSample *rbd;
   _aaxRingBufferData *rbi;

   convolution = data->be_handle;
   hptr = (MIX_T*)convolution->history->history[track];
   hpos = convolution->history_start[track];
   cnum = convolution->no_samples - hpos;
   occlusion = convolution->occlusion;
   hcptr = hptr + hpos;

   rbi = rb->handle;
   rbd = rbi->sample;
   dptr = sptr = rbd->track[track];
   scratch = data->scratch[track];
   dnum = rb->get_parami(rb, RB_NO_SAMPLES);
   {
      MIX_T *cptr;
      float v, threshold;
      int step;

      v = convolution->rms * convolution->delay_gain;
      threshold = convolution->threshold * (float)(1<<23);
      step = convolution->step;

      cptr = convolution->sample;
      threshold = convolution->threshold;
      _batch_convolution(hcptr, cptr, sptr, cnum, dnum, step, v, threshold);
   }

#if 1
   /* add the direct path */
   if (occlusion) {
      occlusion->run(rbd, dptr, hcptr, scratch, dnum, track, occlusion);
   } else {
      rbd->add(dptr, hcptr, dnum, 1.0f, 0.0f);
   }
#else
   if (convolution->freq_filter)
   {
      _aaxRingBufferFreqFilterData *flt = convolution->freq_filter;

      if (convolution->fc > 15000.0f) {
         rbd->add(dptr, hptr+hpos, dnum, flt->low_gain, 0.0f);
      }
      else
      {
         _aaxRingBufferFreqFilterData *filter = convolution->freq_filter;
         filter->run(rbd, dptr, sptr, 0, dnum, 0, track, filter, NULL, 0);
         rbd->add(dptr, hptr+hpos, dnum, 1.0f, 0.0f);
      }
   }
   else {
      rbd->add(dptr, hptr+hpos, dnum, 1.0f, 0.0f);
   }
#endif

   hpos += dnum;
// if ((hpos + cnum) > convolution->history_samples)
   {
      memmove(hptr, hptr+hpos, cnum*sizeof(MIX_T));
      hpos = 0;
   }
   memset(hptr+hpos+cnum, 0, dnum*sizeof(MIX_T));
   convolution->history_start[track] = hpos;

   return 0;
}

static int
_convolution_run(const _aaxDriverBackend *be, const void *be_handle, void *rbd, void *data)
{
   _aaxRingBufferConvolutionData *convolution = data;
   _aaxRingBuffer *rb = rbd;
   int rv = false;

   if (convolution->delay_gain > convolution->threshold)
   {
      _aaxRenderer *render = be->render(be_handle);
      _aaxRendererData data;

      data.mode = THREAD_PROCESS_CONVOLUTION;

      data.drb = rb;
      data.be_handle = convolution;

      data.callback = _convolution_thread;

      render->process(render, &data);

      rv = true;
   }
   return rv;
}
