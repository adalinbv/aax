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
_aaxGraphicEqualizerCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, EQUALIZER_MAX);
   aaxFilter rv = NULL;

   if (flt)
   {
      flt->slot[0]->param[0] = 1.0f; flt->slot[1]->param[0] = 1.0f;
      flt->slot[0]->param[1] = 1.0f; flt->slot[1]->param[1] = 1.0f;
      flt->slot[0]->param[2] = 1.0f; flt->slot[1]->param[2] = 1.0f;
      flt->slot[0]->param[3] = 1.0f; flt->slot[1]->param[3] = 1.0f;
      flt->slot[EQUALIZER_HF]->destroy = destroy;
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxGraphicEqualizerDestroy(_filter_t* filter)
{
   filter->slot[EQUALIZER_HF]->destroy(filter->slot[EQUALIZER_LF]->data);
   filter->slot[EQUALIZER_LF]->data = NULL;
   filter->slot[EQUALIZER_HF]->destroy(filter->slot[EQUALIZER_HF]->data);
   filter->slot[EQUALIZER_HF]->data = NULL;
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
         eq = calloc(1, sizeof(_aaxRingBufferEqualizerData));
         filter->slot[EQUALIZER_LF]->data = NULL;
         filter->slot[EQUALIZER_HF]->data = eq;
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);

      if (eq)	/* fill in the fixed frequencies */
      {
         float fband = logf(44000.0f/67.0f)/8.0f;
         float fs = filter->info->frequency;
         int s, b, pos = _AAX_MAX_EQBANDS-1;
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
            if (pos == 0)
            {
               flt->type = LOWPASS;
               fc = expf((float)(pos-0.5f)*fband)*67.0f;
            }
            else if (pos == 7)
            {
               flt->type = HIGHPASS;
               fc = expf((float)(pos-0.5f)*fband)*67.0f;
            }
            else
            {
               flt->high_gain *= (2.0f*stages);
               flt->type = BANDPASS;
               fc = expf(((float)(pos-0.5f))*fband)*67.0f;
            }

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
      filter->slot[EQUALIZER_HF]->destroy(filter->slot[EQUALIZER_LF]->data);
      filter->slot[EQUALIZER_LF]->data = NULL;
      filter->slot[EQUALIZER_HF]->destroy(filter->slot[EQUALIZER_HF]->data);
      filter->slot[EQUALIZER_HF]->data = NULL;
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
   _filter_t* rv = _aaxFilterCreateHandle(info, type, EQUALIZER_MAX);

   if (rv)
   {
      rv->slot[0]->param[0] = 1.0f; rv->slot[1]->param[0] = 1.0f;
      rv->slot[0]->param[1] = 1.0f; rv->slot[1]->param[1] = 1.0f;
      rv->slot[0]->param[2] = 1.0f; rv->slot[1]->param[2] = 1.0f;
      rv->slot[0]->param[3] = 1.0f; rv->slot[1]->param[3] = 1.0f;
      rv->slot[0]->data = NULL;     rv->slot[1]->data = NULL;
      rv->slot[EQUALIZER_HF]->destroy = destroy;

      rv->state = p2d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxGraphicEqualizerSet(float val, int ptype, UNUSED(unsigned char param))
{
   float rv = val;
   if (ptype == AAX_LOGARITHMIC) {
      rv = _lin2db(val);
   }
   return rv;
}

static float
_aaxGraphicEqualizerGet(float val, int ptype, UNUSED(unsigned char param))
{
   float rv = val;
   if (ptype == AAX_LOGARITHMIC) {
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
   (_aaxFilterSetState*)&_aaxGraphicEqualizerSetState,
   (_aaxNewFilterHandle*)&_aaxNewGraphicEqualizerHandle,
   (_aaxFilterConvert*)&_aaxGraphicEqualizerSet,
   (_aaxFilterConvert*)&_aaxGraphicEqualizerGet,
   (_aaxFilterConvert*)&_aaxGraphicEqualizerMinMax
};

