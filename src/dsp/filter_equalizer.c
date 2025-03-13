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
#include <stdlib.h>

#include <aax/aax.h>

#include <base/types.h>		/* for rintf */
#include <base/gmath.h>

#include <software/rbuf_int.h>

#include <arch.h>

#include "filters.h"
#include "dsp.h"
#include "api.h"

#define VERSION	1.2
#define DSIZE	_MAX_PARAM_EQ*(sizeof(_aaxRingBufferFreqFilterData)+MEMALIGN)
#define EQBANDS	(_MAX_PARAM_EQ+1)

static aaxFilter
_aaxEqualizerCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, _MAX_PARAM_EQ, 0);
   aaxFilter rv = NULL;

   if (flt)
   {
      unsigned s;

      for (s=0; s<_MAX_PARAM_EQ; ++s)
      {
         _aaxSetDefaultFilter2d(flt->slot[s], flt->pos, s);
         flt->slot[s]->swap = _equalizer_swap;
      }
      flt->slot[EQUALIZER_LF]->destroy = _freqfilter_destroy;
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxEqualizerDestroy(_filter_t* filter)
{
   _aaxRingBufferFreqFilterData *flt = filter->slot[EQUALIZER_LF]->data;
   if (flt)
   {
      unsigned s;
      filter->slot[EQUALIZER_LF]->destroy(flt);
      for (s=0; s<_MAX_PARAM_EQ; ++s) {
         filter->slot[s]->data = NULL;
      }
   }
   free(filter);

   return true;
}

static int
eqcompare(const void *p1, const void *p2)
{
   _aaxFilterInfo *s1 = *(_aaxFilterInfo**)p1;
   _aaxFilterInfo *s2 = *(_aaxFilterInfo**)p2;
   float fc1 = s1->param[AAX_CUTOFF_FREQUENCY];
   float fc2 = s2->param[AAX_CUTOFF_FREQUENCY];
   return (int)(fc1 - fc2);
}

static aaxFilter
_aaxEqualizerSetState(_filter_t* filter, int state)
{
   void *handle = filter->handle;
   aaxFilter rv = NULL;

   if (state == true)
   {
      _aaxRingBufferFreqFilterData *flt[_MAX_PARAM_EQ];
      unsigned s;

      flt[0] = filter->slot[EQUALIZER_LF]->data;
      flt[1] = filter->slot[EQUALIZER_LMF]->data;
      flt[2] = filter->slot[EQUALIZER_HMF]->data;
      flt[3] = filter->slot[EQUALIZER_HF]->data;

      if (flt[0] == NULL)
      {
         // flt[0] holds the data for all slots
         flt[0] = _aax_aligned_alloc(DSIZE);
         if (!flt[0]) return rv;

         memset(flt[0], 0, DSIZE);
         flt[0]->no_stages = 1;
         filter->slot[EQUALIZER_LF]->data = flt[0];
         filter->slot[EQUALIZER_LF]->data_size = DSIZE;
      }

      if (!flt[0]->freqfilter)
      {
         size_t dsize= _MAX_PARAM_EQ*(sizeof(_aaxRingBufferFreqFilterHistoryData)+MEMALIGN);
         flt[0]->freqfilter = _aax_aligned_alloc(dsize);
         if (flt[0]->freqfilter) {
            memset(flt[0]->freqfilter, 0, dsize);
         }
         else
         {
            _aax_aligned_free(flt[0]);
            return rv;
         }
      }

      if (flt[0] && !flt[1])
      {
         size_t tmp;
         char *ptr;

         for (s=1; s<_MAX_PARAM_EQ; ++s)
         {
            ptr = (char*)flt[s-1];
            ptr += sizeof(_aaxRingBufferFreqFilterData);
            tmp = (size_t)ptr & MEMMASK;
            if (tmp)
            {
               tmp = MEMALIGN - tmp;
               ptr += tmp;
            }
            flt[s] = (_aaxRingBufferFreqFilterData*)ptr;

            ptr = (char*)flt[s-1]->freqfilter;
            ptr += sizeof(_aaxRingBufferFreqFilterHistoryData);
            tmp = (size_t)ptr & MEMMASK;
            if (tmp)
            {
               tmp = MEMALIGN - tmp;
               ptr += tmp;
            }
            flt[s]->freqfilter = (_aaxRingBufferFreqFilterHistoryData*)ptr;
            flt[s]->no_stages = 1;
            filter->slot[s]->data = flt[s];
         }
      }

      if (flt[0] && flt[1] && flt[2] && flt[3])
      {
         float fs = filter->info->frequency;
         float gain[EQBANDS], gprev;

         // sort filter slots based on cutoff frequency
         qsort(filter->slot, _MAX_FE_SLOTS, sizeof(_aaxFilterInfo*), eqcompare);

         gprev = fabsf(filter->slot[0]->param[AAX_LF_GAIN]);
         for (s=0; s<_MAX_PARAM_EQ; ++s)
         {
            float fc;

            fc = filter->slot[s]->param[AAX_CUTOFF_FREQUENCY];
            fc = CLIP_FREQUENCY(fc, fs);
            if (fc >= HIGHEST_CUTOFF(fs))
            {
               int i;

               for (i=s; i<_MAX_PARAM_EQ; ++i) {
                  gain[i] = gprev;
               }
               s = i;
               break;
            }

            gain[s] = 0.5f*(gprev + fabsf(filter->slot[s]->param[AAX_LF_GAIN]));
            if (s) gain[s] = sqrtf(gain[s]);

            if (fabsf(gain[s] - 1.0f) < LEVEL_128DB) gain[s] = 1.0f;
            else if (gain[s] < LEVEL_128DB) gain[s] = 0.0f;

            gprev = fabsf(filter->slot[s]->param[AAX_HF_GAIN]);
         }
         gain[s] = gprev;
         if (fabsf(gain[s] - 1.0f) < LEVEL_128DB) gain[s] = 1.0f;
         else if (gain[s] < LEVEL_128DB) gain[s] = 0.0f;

         for (s=0; s<_MAX_PARAM_EQ; ++s)
         {
            float fc = filter->slot[s]->param[AAX_CUTOFF_FREQUENCY];
            int lp = (gain[s] >= gain[s+1]) ? true : false;
            int stages, state = filter->slot[s]->src;
            int ostate = state & AAX_ORDER_MASK;

            fc = CLIP_FREQUENCY(fc, fs);
            if (fc >= HIGHEST_CUTOFF(fs)) stages = 0;
            else if (ostate == AAX_48DB_OCT) stages = 4;
            else if (ostate == AAX_36DB_OCT) stages = 3;
            else if (ostate == AAX_24DB_OCT) stages = 2;
            else stages = 1;

            flt[s]->no_stages = stages;
            flt[s]->state = AAX_BUTTERWORTH;
            flt[s]->type = lp ? LOWPASS : HIGHPASS;
            flt[s]->fs = fs;
            flt[s]->low_gain = lp ? gain[s+1] : gain[s];
            flt[s]->high_gain = lp ? gain[s] : gain[s+1];
            _freqfilter_normalize_gains(flt[s]);
            flt[s]->Q = filter->slot[s]->param[AAX_RESONANCE];
            _aax_butterworth_compute(fc, flt[s]);
         }
#if 0
 for (s=0; s<_MAX_PARAM_EQ; ++s) {
  if (flt[s]->no_stages) {
   printf("Filter: %i\n", s);
   printf(" Fc: % 7.1f Hz\n", filter->slot[s]->param[AAX_CUTOFF_FREQUENCY]);
   printf(" no. stages: %i\n", flt[s]->no_stages);
   printf(" state: %s\n", (flt[s]->state == AAX_BUTTERWORTH) ? "Butterworth" : "Bessel");
   printf(" type: %s\n", flt[s]->type == LOWPASS ? "low-pass" : "high-pass");
   printf(" Fs: % 7.1f Hz\n", flt[s]->fs);
   printf(" low gain:  %5.4f\n high gain: %5.4f\n", flt[s]->low_gain, flt[s]->high_gain);
   printf(" Q: %3.1f\n", flt[s]->Q);
   printf(" k: %5.4f\n ", flt[s]->k);
  }
 }
 printf("\n");
#endif
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      rv = filter;
   }
   else if (state == false)
   {
      if (filter->slot[EQUALIZER_LF]->data)
      {
         unsigned s;

         filter->slot[EQUALIZER_LF]->destroy(filter->slot[EQUALIZER_LF]->data);
         for (s=0; s<_MAX_PARAM_EQ; ++s) {
            filter->slot[s]->data = NULL;
         }
      }
      rv = filter;
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }

   return rv;
}

static _filter_t*
_aaxNewEqualizerHandle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _filter_t* rv = _aaxFilterCreateHandle(info, type, _MAX_PARAM_EQ, 0);

   if (rv)
   {
      unsigned s;
      for (s=0; s<_MAX_PARAM_EQ; ++s)
      {
         _aax_dsp_copy(rv->slot[s], &p2d->filter[rv->pos]);
         rv->slot[s]->swap = _equalizer_swap;
      }
      rv->slot[EQUALIZER_LF]->destroy = _freqfilter_destroy;

      rv->state = p2d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxEqualizerSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxEqualizerGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

#define MINF	MINIMUM_CUTOFF
#define MAXF	MAXIMUM_CUTOFF
static float
_aaxEqualizerMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxEqualizerRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { MINF, 0.0f, 0.0f, 1.0f }, { MAXF, 10.0f, 10.0f, 100.0f } },
    { { MINF, 0.0f, 0.0f, 1.0f }, { MAXF, 10.0f, 10.0f, 100.0f } },
    { { MINF, 0.0f, 0.0f, 1.0f }, { MAXF, 10.0f, 10.0f, 100.0f } },
    { { MINF, 0.0f, 0.0f, 1.0f }, { MAXF, 10.0f, 10.0f,  10.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxEqualizerRange[slot].min[param],
                       _aaxEqualizerRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxEqualizer =
{
   "AAX_equalizer_"AAX_MKSTR(VERSION), VERSION,
   (_aaxFilterCreateFn*)&_aaxEqualizerCreate,
   (_aaxFilterDestroyFn*)&_aaxEqualizerDestroy,
   NULL,
   (_aaxFilterSetStateFn*)&_aaxEqualizerSetState,
   (_aaxNewFilterHandleFn*)&_aaxNewEqualizerHandle,
   (_aaxFilterConvertFn*)&_aaxEqualizerSet,
   (_aaxFilterConvertFn*)&_aaxEqualizerGet,
   (_aaxFilterConvertFn*)&_aaxEqualizerMinMax
};

void
_equalizer_swap(void *d, void *s)
{
   _aaxFilterInfo *dst = d, *src = s;

   if (src->data)
   {
      if (!dst->data)
      {
         _aaxAtomicPointerSwap(&src->data, &dst->data);
         dst->data_size = src->data_size;
      }
      else if (dst->data_size)
      {
         assert(dst->data_size == src->data_size);
        _freqfilter_data_swap(dst->data, src->data);
      }
   }
   dst->destroy = src->destroy;
   dst->swap = src->swap;
}

int
_equalizer_run(void *rb, MIX_PTR_T dptr, UNUSED(MIX_PTR_T scratch),
               size_t dmin, size_t dmax, unsigned int track,
               _aaxRingBufferFreqFilterData *filter[_MAX_PARAM_EQ])
{
   _aaxRingBufferSample *rbd = (_aaxRingBufferSample*)rb;
   int s;

   assert(dptr != 0);
   assert(filter != NULL);
   assert(dmin < dmax);
   assert(track < _AAX_MAX_SPEAKERS);

   dptr += dmin;
   dmax -= dmin;

   for (s=0; s<_MAX_PARAM_EQ; ++s) {
      if (filter[s]->no_stages) {
         rbd->freqfilter(dptr, dptr, track, dmax, filter[s]);
      }
   }

   return true;
}
