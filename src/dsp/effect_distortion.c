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
#include <base/random.h>

#include <software/rbuf_int.h>
#include "effects.h"
#include "api.h"
#include "arch.h"

#define VERSION	1.10
#define DSIZE	sizeof(_aaxRingBufferDistoritonData)

static int _distortion_run(void*, MIX_PTR_T, CONST_MIX_PTR_T, size_t, size_t, size_t, unsigned int, void*, void*);

static aaxEffect
_aaxDistortionEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 2, DSIZE);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxSetDefaultEffect2d(eff->slot[0], eff->pos, 0);
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxDistortionEffectReset(void *data)
{
   return true;
}


static aaxEffect
_aaxDistortionEffectSetState(_effect_t* effect, int state)
{
   void *handle = effect->handle;
   aaxEffect rv = false;

   effect->state = state;
   switch (state & AAX_SOURCE_MASK)
   {
   case AAX_CONSTANT:
   case AAX_SAWTOOTH:
   case AAX_SQUARE:
   case AAX_TRIANGLE:
   case AAX_SINE:
   case AAX_CYCLOID:
   case AAX_IMPULSE:
   case AAX_RANDOMNESS:
   case AAX_RANDOM_SELECT:
   case AAX_ENVELOPE_FOLLOW:
   case AAX_TIMED_TRANSITION:
   {
      _aaxRingBufferDistoritonData *data = effect->slot[0]->data;

      if (data) effect->slot[0]->destroy(data);

      data = _aax_aligned_alloc(DSIZE + sizeof(_aaxLFOData));
      if (data)
      {
         effect->slot[0]->data = data;
         effect->slot[0]->data_size = DSIZE;
      }

      if (data)
      {
         float fc = effect->slot[1]->param[AAX_WET_CUTOFF_FREQUENCY & 0xF];
         float fmax = effect->slot[1]->param[AAX_WET_CUTOFF_FREQUENCY_HF & 0xF];
         float gain = effect->slot[1]->param[AAX_WET_GAIN & 0xF];
         _aaxRingBufferFreqFilterData *flt;
         float fs = 48000.0f;
         _aaxLFOData *lfo;
         int constant;
         char *ptr;

         if (effect->info) {
            fs = effect->info->frequency;
         }
         fc = CLIP_FREQUENCY(fc, fs);
         fmax = CLIP_FREQUENCY(fmax, fs);

         memset(data, 0, DSIZE + sizeof(_aaxLFOData));

         ptr = (char*)data + DSIZE;
         data->lfo = (_aaxLFOData*)ptr;

         data->run = _distortion_run;

         flt = data->freq_filter;
         if (fc > MINIMUM_CUTOFF && fc < HIGHEST_CUTOFF(fs))
         {
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
               }
            }
            else
            {
               _aax_aligned_free(flt);
               flt = NULL;
            }
         }
         else if (flt)
         {
            _aax_aligned_free(flt);
            flt = NULL;
         }
         data->freq_filter = flt;


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
         else if (flt) // add a frequency filter
         {
            int stages;

            flt->fs = fs;
            flt->run = _freqfilter_run;

            flt->low_gain = (gain >= 0.0f) ? gain : 0.0f;
            flt->high_gain = (gain < 0.0f) ? -gain : 0.0f;

            if (state & AAX_48DB_OCT) stages = 4;
            else if (state & AAX_36DB_OCT) stages = 3;
            else if (state & AAX_24DB_OCT) stages = 2;
            else if (state & AAX_6DB_OCT) stages = 0;
            else stages = 1;

            flt->no_stages = stages;
            flt->state = (state & AAX_BESSEL) ? AAX_BESSEL : AAX_BUTTERWORTH;
            flt->Q = effect->slot[1]->param[AAX_WET_RESONANCE & 0xF];
            flt->type = (flt->high_gain >= flt->low_gain) ? LOWPASS : HIGHPASS;
            flt->fc_low = fc;
            flt->fc_high = fmax;

            if ((state & AAX_SOURCE_MASK) == AAX_RANDOM_SELECT)
            {
               float lfc2 = _lin2log(fmax);
               float lfc1 = _lin2log(fc);

               flt->random = true;

               lfc1 += (lfc2 - lfc1)*_aax_random();
               fc = _log2lin(lfc1);
            }

            if (flt->state == AAX_BESSEL) {
                _aax_bessel_compute(fc, flt);
            }
            else
            {
               if (flt->type == HIGHPASS)
               {
                  float g = flt->high_gain;
                  flt->high_gain = flt->low_gain;
                  flt->low_gain = g;
               }
               _aax_butterworth_compute(fc, flt);
            }
         }
      }
      break;
   }
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
      // intentional fall-through
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
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 2, 0);

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
    { {  0.0f,  0.0f,   0.0f, 0.0f  }, {     4.0f,     1.0f,  1.0f,  1.0f } },
    { { 20.0f, 20.0f, -0.98f, 0.01f }, { 22050.0f, 22050.0f, 0.98f, 80.0f } },
    { {  0.0f,  0.0f,   0.0f, 0.0f  }, {     0.0f,     0.0f,  0.0f,  0.0f } },
    { {  0.0f,  0.0f,   0.0f, 0.0f  }, {     0.0f,     0.0f,  0.0f,  0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxDistortionRange[slot].min[param],
                       _aaxDistortionRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxDistortionEffect =
{
   "AAX_distortion_effect_"AAX_MKSTR(VERSION), VERSION,
   (_aaxEffectCreateFn*)&_aaxDistortionEffectCreate,
   (_aaxEffectDestroyFn*)&_aaxEffectDestroy,
   (_aaxEffectResetFn*)&_aaxDistortionEffectReset,
   (_aaxEffectSetStateFn*)&_aaxDistortionEffectSetState,
   NULL,
   (_aaxNewEffectHandleFn*)&_aaxNewDistortionEffectHandle,
   (_aaxEffectConvertFn*)&_aaxDistortionEffectSet,
   (_aaxEffectConvertFn*)&_aaxDistortionEffectGet,
   (_aaxEffectConvertFn*)&_aaxDistortionEffectMinMax
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
   _aaxRingBufferFreqFilterData *flt = dist_data->freq_filter;
   _aaxLFOData* lfo = dist_data->lfo;
   float *params = dist_effect->param;
   float clip, asym, fact, mix;
   int rv = false;
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

   assert(lfo);
   if (lfo->state != AAX_CONSTANT)
   {
      lfo_fact = lfo->get(lfo, env, sptr, track, no_samples);
      if (flt) // && !ctr
      {
         float fc = flt->fc_low + lfo_fact*(flt->fc_high-flt->fc_low);
         fc = CLIP_FREQUENCY(fc, flt->fs);

         if (flt->resonance > 0.0f) {
            if (flt->type > BANDPASS) { // HIGHPASS
                flt->Q = _MAX(flt->resonance*(flt->fc_high - fc), 1.0f);
            } else {
               flt->Q = flt->resonance*fc;
            }
         }

         if (flt->state == AAX_BESSEL) {
            _aax_bessel_compute(fc, flt);
         } else {
            _aax_butterworth_compute(fc, flt);
         }
      }
   }
   fact = params[AAX_DISTORTION_FACTOR]*lfo_fact;
   clip = params[AAX_CLIPPING_FACTOR];
   mix  = params[AAX_MIX_FACTOR]*lfo_fact;
   asym = params[AAX_ASYMMETRY];

   if ((mix > 0.01f && fact > 0.0013f) || flt)
   {
      float mix_factor;

      /* make dptr the wet signal */
      _aax_memcpy(dptr, sptr, no_samples*bps);

      /* frequency filter first, if defined */
      if (flt) {
         flt->run(rbd, dptr, dptr, 0, no_samples, 0, track, flt, env, 1.0f);
      }

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

      rv = true;
   }
   return rv;
}

