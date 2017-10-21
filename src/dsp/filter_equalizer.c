/*
 * Copyright 2007-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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

#include <base/types.h>		/* for rintf */
#include <base/gmath.h>

#include "common.h"
#include "filters.h"
#include "api.h"

static aaxFilter
_aaxEqualizerCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, EQUALIZER_MAX);
   aaxFilter rv = NULL;

   if (flt)
   {
      _aaxSetDefaultFilter2d(flt->slot[1], flt->pos);
      _aaxSetDefaultFilter2d(flt->slot[0], flt->pos);
      flt->slot[0]->destroy = destroy;
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxEqualizerDestroy(_filter_t* filter)
{
   filter->slot[0]->destroy(filter->slot[0]->data);
   filter->slot[0]->data = NULL;
   filter->slot[1]->data = NULL;
   free(filter);

   return AAX_TRUE;
}

static aaxFilter
_aaxEqualizerSetState(_filter_t* filter, int state)
{
   void *handle = filter->handle;
   aaxFilter rv = NULL;

   if (state == AAX_TRUE)
   {
      _aaxRingBufferFreqFilterData *flt_lf = filter->slot[EQUALIZER_LF]->data;
      _aaxRingBufferFreqFilterData *flt_hf = filter->slot[EQUALIZER_HF]->data;
      if (flt_lf == NULL)
      {
         char *ptr;
         flt_lf = calloc(EQUALIZER_MAX,sizeof(_aaxRingBufferFreqFilterData));
         flt_lf->no_stages = 1;
         filter->slot[EQUALIZER_LF]->data = flt_lf;

         ptr = (char*)flt_lf + sizeof(_aaxRingBufferFreqFilterData);
         flt_hf = (_aaxRingBufferFreqFilterData*)ptr;
         flt_lf->no_stages = 1;
         filter->slot[EQUALIZER_HF]->data = flt_hf;
      }

      if (flt_lf && flt_hf)
      {
         float lf_gain, mf_gain, hf_gain;
         float fcl, fch;

         flt_lf->fs = filter->info->frequency;
         flt_hf->fs = filter->info->frequency;

         /* frequencies */
         fcl = filter->slot[EQUALIZER_LF]->param[AAX_CUTOFF_FREQUENCY];
         fch = filter->slot[EQUALIZER_HF]->param[AAX_CUTOFF_FREQUENCY];

         if (fch < fcl)
         {
            size_t plen, dlen = sizeof(_aaxRingBufferFreqFilterData);
            _aaxRingBufferFreqFilterData data;
            _aaxFilterInfo slot;

            plen = sizeof(slot.param);
            memcpy(&slot.param, filter->slot[EQUALIZER_LF]->param, plen);
            memcpy(&data, filter->slot[EQUALIZER_LF]->data, dlen);

            memcpy(filter->slot[EQUALIZER_LF]->param,
                   filter->slot[EQUALIZER_HF]->param, plen);
            memcpy(filter->slot[EQUALIZER_LF]->data,
                   filter->slot[EQUALIZER_HF]->data, dlen);

            memcpy(filter->slot[EQUALIZER_HF]->param, &slot.param, plen);
            memcpy(filter->slot[EQUALIZER_HF]->data, &data, dlen);

         }

         if (fabsf(fch - fcl) < 200.0f)
         {
            fcl *= 0.9f;
            fch *= 1.1f;
            filter->slot[EQUALIZER_LF]->param[AAX_CUTOFF_FREQUENCY] = fcl;
            filter->slot[EQUALIZER_HF]->param[AAX_CUTOFF_FREQUENCY] = fch;
         }

         /* gains */
         lf_gain = fabsf(filter->slot[EQUALIZER_LF]->param[AAX_LF_GAIN]);
         if (fabsf(lf_gain - 1.0f) < LEVEL_128DB) lf_gain = 1.0f;
         else if (lf_gain < LEVEL_128DB) lf_gain = 0.0f;

         mf_gain = filter->slot[EQUALIZER_LF]->param[AAX_HF_GAIN];
         if (fabsf(mf_gain - 1.0f) < LEVEL_128DB) mf_gain = 1.0f;
         else if (mf_gain < LEVEL_128DB) mf_gain = 0.0f;

         if (lf_gain >= mf_gain)
         {
            flt_lf->type = LOWPASS;
            flt_lf->high_gain = lf_gain;
            flt_lf->low_gain = mf_gain;
         }
         else
         {
            flt_lf->type = HIGHPASS;
            flt_lf->high_gain = mf_gain;
            flt_lf->low_gain = lf_gain;
         }

         mf_gain = filter->slot[EQUALIZER_HF]->param[AAX_LF_GAIN];
         if (fabsf(mf_gain - 1.0f) < LEVEL_128DB) mf_gain = 1.0f;
         else if (mf_gain < LEVEL_128DB) mf_gain = 0.0f;

         hf_gain = fabsf(filter->slot[EQUALIZER_HF]->param[AAX_HF_GAIN]);
         if (fabsf(hf_gain - 1.0f) < LEVEL_128DB) hf_gain = 1.0f;
         else if (hf_gain < LEVEL_128DB) hf_gain = 0.0f;

         if (mf_gain >= hf_gain)
         {
            flt_hf->type = LOWPASS;
            flt_hf->high_gain = mf_gain;
            flt_hf->low_gain = hf_gain;
         }
         else
         {
            flt_hf->type = HIGHPASS;
            flt_hf->high_gain = hf_gain;
            flt_hf->low_gain = mf_gain;
         }

         if (flt_lf->type == LOWPASS && flt_hf->type == HIGHPASS)
         {
            /* LF frequency setup */
            flt_lf->k = 0.5f*flt_lf->low_gain/flt_lf->high_gain;
            flt_lf->Q = filter->slot[EQUALIZER_LF]->param[AAX_RESONANCE];
            _aax_butterworth_compute(fcl, flt_lf);
            flt_lf->low_gain = 0.0f;

            /* HF frequency setup */
            flt_hf->k = 0.5f*flt_hf->low_gain/flt_hf->high_gain;
            flt_hf->Q = filter->slot[EQUALIZER_HF]->param[AAX_RESONANCE];
            _aax_butterworth_compute(fch, flt_hf);
            flt_hf->low_gain = 0.0f;
         }
         else if (flt_lf->type == HIGHPASS && flt_hf->type == LOWPASS)
         {
            /* LF frequency setup */
            flt_lf->k = flt_lf->low_gain;
            flt_lf->Q = filter->slot[EQUALIZER_LF]->param[AAX_RESONANCE];
            _aax_butterworth_compute(fcl, flt_lf);
            flt_lf->low_gain = 0.0f;

            /* HF frequency setup */
            flt_hf->k = flt_hf->low_gain/flt_hf->high_gain;
            flt_hf->Q = filter->slot[EQUALIZER_HF]->param[AAX_RESONANCE];
            _aax_butterworth_compute(fch, flt_hf);
            flt_hf->low_gain = 0.0f;
         }
         else if (flt_lf->type == LOWPASS && flt_hf->type == LOWPASS)
         {
            /* LF frequency setup */
            flt_lf->k = 0.5f*flt_lf->low_gain/flt_lf->high_gain;
            flt_lf->Q = filter->slot[EQUALIZER_LF]->param[AAX_RESONANCE];
            _aax_butterworth_compute(fcl, flt_lf);
            flt_lf->low_gain = 0.0f;

            /* HF frequency setup */
            flt_lf->high_gain = 1.0f;

            flt_hf->k = flt_hf->low_gain/flt_lf->high_gain;
            flt_hf->Q = filter->slot[EQUALIZER_HF]->param[AAX_RESONANCE];
            _aax_butterworth_compute(fch, flt_hf);
            flt_hf->low_gain = 0.0f;
         }
         else
         {
            /* LF frequency setup */
            flt_lf->high_gain = 1.0f;

            flt_lf->k = flt_lf->low_gain/flt_lf->high_gain;
            flt_lf->Q = filter->slot[EQUALIZER_LF]->param[AAX_RESONANCE];
            _aax_butterworth_compute(fcl, flt_lf);
            flt_lf->low_gain = 0.0f;

            /* HF frequency setup */
            flt_hf->k = 0.5f*flt_hf->low_gain/flt_lf->high_gain;
            flt_hf->Q = filter->slot[EQUALIZER_HF]->param[AAX_RESONANCE];
            _aax_butterworth_compute(fch, flt_hf);
            flt_hf->low_gain = 0.0f;
         }
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      rv = filter;
   }
   else if (state == AAX_FALSE)
   {
      filter->slot[0]->destroy(filter->slot[0]->data);
      filter->slot[0]->data = NULL;
      filter->slot[1]->data = NULL;
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
   _filter_t* rv = _aaxFilterCreateHandle(info, type, EQUALIZER_MAX);

   if (rv)
   {
      unsigned int size = sizeof(_aaxFilterInfo);

      memcpy(rv->slot[1], &p2d->filter[rv->pos], size);
      rv->slot[1]->data = NULL;

      memcpy(rv->slot[0], &p2d->filter[rv->pos], size);
      rv->slot[0]->destroy = destroy;
      rv->slot[0]->data = NULL;

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

static float
_aaxEqualizerMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxEqualizerRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 20.0f, 0.0f, 0.0f, 1.0f }, { 22050.0f, 10.0f, 10.0f, 100.0f } },
    { { 20.0f, 0.0f, 0.0f, 1.0f }, { 22050.0f, 10.0f, 10.0f, 100.0f } },
    { {  0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f,  0.0f,  0.0f,   0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxEqualizerRange[slot].min[param],
                       _aaxEqualizerRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxEqualizer =
{
   AAX_FALSE,
   "AAX_equalizer_1.01", 1.01f,
   (_aaxFilterCreate*)&_aaxEqualizerCreate,
   (_aaxFilterDestroy*)&_aaxEqualizerDestroy,
   (_aaxFilterSetState*)&_aaxEqualizerSetState,
   (_aaxNewFilterHandle*)&_aaxNewEqualizerHandle,
   (_aaxFilterConvert*)&_aaxEqualizerSet,
   (_aaxFilterConvert*)&_aaxEqualizerGet,
   (_aaxFilterConvert*)&_aaxEqualizerMinMax
};

