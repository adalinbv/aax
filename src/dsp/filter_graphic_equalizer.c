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

#include <base/types.h>		/* for rintf */
#include <base/gmath.h>

#include <software/rbuf_int.h>

#include <arch.h>

#include "filters.h"
#include "dsp.h"
#include "api.h"

#define DSIZE	sizeof(_aaxRingBufferEqualizerData)

static void _grapheq_destroy(void*);
static void _grapheq_swap(void*,void*);

/*
 * Implement en 8-band graphic equalizer with frequency bands at the following
 * center frequencies: 42Hz, 100Hz, 225Hz, 515Hz, 1.2kHz, 2.7kHz, 6.3KHz, 15khz
 */
static aaxFilter
_aaxGraphicEqualizerCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, EQUALIZER_MAX, DSIZE);
   aaxFilter rv = NULL;

   if (flt)
   {
      int i;

      // Set all bands to 0dB */
      for (i=0; i<_AAX_MAX_EQBANDS; ++i) {
         flt->slot[i/4]->param[i % 4] = 1.0f;
      }

      flt->slot[EQUALIZER_LF]->destroy = NULL;
      flt->slot[EQUALIZER_LF]->swap = NULL;

      flt->slot[EQUALIZER_HF]->destroy = _grapheq_destroy;
      flt->slot[EQUALIZER_HF]->swap = _grapheq_swap;

      assert(flt->slot[EQUALIZER_HF]->data == NULL);
      flt->slot[EQUALIZER_HF]->data = flt->slot[EQUALIZER_LF]->data;
      flt->slot[EQUALIZER_LF]->data = NULL;

      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxGraphicEqualizerDestroy(_filter_t* filter)
{
   assert(filter->slot[EQUALIZER_LF]->data == NULL);
   if (filter->slot[EQUALIZER_HF]->data)
   {
      filter->slot[EQUALIZER_HF]->destroy(filter->slot[EQUALIZER_HF]->data);
      filter->slot[EQUALIZER_HF]->data = NULL;
   }
   free(filter);

   return AAX_TRUE;
}

static aaxFilter
_aaxGraphicEqualizerSetState(_filter_t* filter, int state)
{
   void *handle = filter->handle;
   aaxFilter rv = NULL;

   if (state == AAX_TRUE)
   {
      _aaxRingBufferEqualizerData *eq = filter->slot[EQUALIZER_HF]->data;

      /*
       * use EQUALIZER_HF to distinquish between GRAPHIC_EQUALIZER
       * and FREQ_FILTER (which only uses EQUALIZER_LF)
       */
      if (eq == NULL)
      {
         eq = _aax_aligned_alloc(DSIZE);
         if (!eq) return rv;

         filter->slot[EQUALIZER_LF]->data = NULL;
         filter->slot[EQUALIZER_HF]->data = eq;
         memset(eq, 0, DSIZE);
      }

      if (eq && !eq->band[0].freqfilter)
      {
         size_t fsize = sizeof(_aaxRingBufferFreqFilterHistoryData);
         size_t dsize = _AAX_MAX_EQBANDS*(fsize+MEMALIGN);
         char *ptr;

         if ((ptr = _aax_aligned_alloc(dsize)) != NULL)
         {
            int i;

            memset(ptr, 0, dsize);
            for (i=0; i<_AAX_MAX_EQBANDS; ++i)
            {
               size_t tmp;

               eq->band[i].freqfilter = (_aaxRingBufferFreqFilterHistoryData*)ptr;
               ptr += sizeof(_aaxRingBufferFreqFilterHistoryData);
               tmp = (size_t)ptr & MEMMASK;
               if (tmp)
               {
                  tmp = MEMALIGN - tmp;
                  ptr += tmp;
               }
            }
         }
         else
         {
            _aax_aligned_free(eq);
            _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            return rv;
         }
      }

      if (eq)	/* fill in the fixed frequencies */
      {
         float fband = _lin2log(22000.0f);
         float fs = filter->info->frequency;
         int s, b, pos = _AAX_MAX_EQBANDS-1;
         int bands = 24;
         int stages = 2;

         do
         {
            _aaxRingBufferFreqFilterData *flt;
            float fc, gain;

            flt = &eq->band[pos];

            s = pos / 4;
            b = pos % 4;

            gain = filter->slot[s]->param[b];
            if (gain < LEVEL_128DB) gain = 0.0f;
            else if (fabsf(gain - 1.0f) < LEVEL_128DB) gain = 1.0f;
            flt->high_gain = gain;
            flt->low_gain = 0.0f;
            if (pos == 0) {
               flt->type = LOWPASS;
            } else if (pos == 7) {
               flt->type = HIGHPASS;
            }
            else
            {
               flt->high_gain *= (2.0f*stages);
               flt->type = BANDPASS;
            }
            // start at band 9 (42.5Hz) of 24 bands and use every odd band
            // which equals to 8 bands in total.
            fc = _log2lin((9+pos*2)*fband/bands);

            flt->k = 0.0f;
            flt->Q = 0.66f; // _MAX(1.4142f/stages, 1.0f);
            flt->fs = fs;
            filter->state = 0;
            flt->no_stages = stages;
            _aax_butterworth_compute(fc, flt);
         }
         while (pos--);
      }
      rv = filter;
   }
   else if (state == AAX_FALSE)
   {
      assert(filter->slot[EQUALIZER_LF]->data == NULL);
      if (filter->slot[EQUALIZER_HF]->data)
      {
         filter->slot[EQUALIZER_HF]->destroy(filter->slot[EQUALIZER_HF]->data);
         filter->slot[EQUALIZER_HF]->data = NULL;
      }
      rv = filter;
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }

   return rv;
}

static _filter_t*
_aaxNewGraphicEqualizerHandle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _filter_t* rv = _aaxFilterCreateHandle(info, type, EQUALIZER_MAX, 0);

   if (rv)
   {
      rv->slot[0]->param[0] = 1.0f; rv->slot[1]->param[0] = 1.0f;
      rv->slot[0]->param[1] = 1.0f; rv->slot[1]->param[1] = 1.0f;
      rv->slot[0]->param[2] = 1.0f; rv->slot[1]->param[2] = 1.0f;
      rv->slot[0]->param[3] = 1.0f; rv->slot[1]->param[3] = 1.0f;

      rv->slot[EQUALIZER_LF]->destroy = NULL;
      rv->slot[EQUALIZER_LF]->swap = NULL;

      rv->slot[EQUALIZER_HF]->destroy = _grapheq_destroy;
      rv->slot[EQUALIZER_HF]->swap = _grapheq_swap;

      rv->slot[EQUALIZER_HF]->data = rv->slot[EQUALIZER_LF]->data;
      rv->slot[EQUALIZER_LF]->data = NULL;

      rv->state = p2d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxGraphicEqualizerSet(float val, int ptype, UNUSED(unsigned char param))
{
   float rv = val;
   if (ptype == AAX_DECIBEL) {
      rv = _lin2db(val);
   }
   return rv;
}

static float
_aaxGraphicEqualizerGet(float val, int ptype, UNUSED(unsigned char param))
{
   float rv = val;
   if (ptype == AAX_DECIBEL) {
      rv = _db2lin(val);
   }
   return rv;
}

static float
_aaxGraphicEqualizerMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxGraphicEqualizerRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f, 2.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f, 2.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxGraphicEqualizerRange[slot].min[param],
                       _aaxGraphicEqualizerRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxGraphicEqualizer =
{
   AAX_TRUE,
   "AAX_graphic_equalizer", 1.0f,
   (_aaxFilterCreate*)&_aaxGraphicEqualizerCreate,
   (_aaxFilterDestroy*)&_aaxGraphicEqualizerDestroy,
   NULL,
   (_aaxFilterSetState*)&_aaxGraphicEqualizerSetState,
   (_aaxNewFilterHandle*)&_aaxNewGraphicEqualizerHandle,
   (_aaxFilterConvert*)&_aaxGraphicEqualizerSet,
   (_aaxFilterConvert*)&_aaxGraphicEqualizerGet,
   (_aaxFilterConvert*)&_aaxGraphicEqualizerMinMax
};

void
_grapheq_swap(void *d, void *s)
{
   _aaxFilterInfo *dst = d, *src = s;

   if (src->data)
   {
      if (!dst->data) {
          dst->data = _aaxAtomicPointerSwap(&src->data, dst->data);
          dst->data_size = src->data_size;
      }
      else
      {
         _aaxRingBufferEqualizerData *deq = dst->data;
         _aaxRingBufferEqualizerData *seq = src->data;
         int i;

         assert(dst->data_size == src->data_size);

         for (i=0; i<_AAX_MAX_EQBANDS; ++i) {
            _freqfilter_data_swap(&deq->band[i], &seq->band[i]);
         }
      }
   }
   dst->destroy = src->destroy;
   dst->swap = src->swap;
}


static void
_grapheq_destroy(void *ptr)
{
   _aaxRingBufferEqualizerData *eq = (_aaxRingBufferEqualizerData*)ptr;
   if (eq && eq->band[0].freqfilter)
   {
      _aax_aligned_free(eq->band[0].freqfilter);
      _aax_aligned_free(eq);
   }
}

int
_grapheq_run(void *rb, MIX_PTR_T dptr, MIX_PTR_T sptr, MIX_PTR_T tmp,
             size_t dmin, size_t dmax, unsigned int track,
             _aaxRingBufferEqualizerData *eq)
{
   _aaxRingBufferSample *rbd = (_aaxRingBufferSample*)rb;
   _aaxRingBufferFreqFilterData* filter;
   int rv = AAX_FALSE;
   size_t no_samples;
   int band;

   sptr += dmin;
   no_samples = dmax - dmin;

   // TODO: Make sure we actually need to filter something, otherwise return
   //       AAX_FALSE, although the impact is rather low being mixer-only.
   rv = AAX_TRUE;

   // first band, straight into dptr to save a bzero() and rbd->add()
   band = _AAX_MAX_EQBANDS;
   filter = &eq->band[--band];
   rbd->freqfilter(dptr, sptr, track, no_samples, filter);

   // next 7 bands
   do
   {
      filter = &eq->band[--band];
      rbd->freqfilter(tmp, sptr, track, no_samples, filter);
      rbd->add(dptr, tmp, no_samples, 1.0f, 0.0f);
   }
   while(band);

   return rv;
}
