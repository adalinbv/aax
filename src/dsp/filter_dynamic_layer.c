/*
 * Copyright 2007-2021 by Erik Hofman.
 * Copyright 2009-2021 by Adalin B.V.
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

#include "lfo.h"
#include "filters.h"
#include "api.h"

#define VERSION	1.0
#define DSIZE	sizeof(_aaxLFOData)

static float _aaxDynamicTimbreFilterMinMax(float, int, unsigned char);

static aaxFilter
_aaxDynamicTimbreFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, 1, DSIZE);
   aaxFilter rv = NULL;

   if (flt)
   {
      _aaxSetDefaultFilter2d(flt->slot[0], flt->pos, 0);
      flt->slot[0]->destroy = _lfo_destroy;
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxDynamicTimbreFilterDestroy(_filter_t* filter)
{
   if (filter->slot[0]->data)
   {
      filter->slot[0]->destroy(filter->slot[0]->data);
      filter->slot[0]->data = NULL;
   }
   free(filter);

   return AAX_TRUE;
}

static aaxFilter
_aaxDynamicTimbreFilterSetState(_filter_t* filter, int state)
{
   void *handle = filter->handle;
   aaxFilter rv = AAX_FALSE;
   int stereo;

   assert(filter->info);

   stereo = (state & AAX_LFO_STEREO) ? AAX_TRUE : AAX_FALSE;
   state &= ~AAX_LFO_STEREO;

   filter->state = state;
   switch (state & ~AAX_INVERSE)
   {
   case AAX_CONSTANT_VALUE:
   case AAX_TRIANGLE_WAVE:
   case AAX_SINE_WAVE:
   case AAX_SQUARE_WAVE:
   case AAX_IMPULSE_WAVE:
   case AAX_SAWTOOTH_WAVE:
   case AAX_RANDOMNESS:
   case AAX_TIMED_TRANSITION:
   case (AAX_TIMED_TRANSITION|AAX_ENVELOPE_FOLLOW_LOG):
   case AAX_ENVELOPE_FOLLOW:
   case AAX_ENVELOPE_FOLLOW_LOG:
   case AAX_ENVELOPE_FOLLOW_MASK:
   {
      _aaxLFOData* lfo = filter->slot[0]->data;
      if (lfo == NULL) {
         filter->slot[0]->data = lfo = _lfo_create();
      }

      if (lfo)
      {
         int constant;

         if ((state & (AAX_ENVELOPE_FOLLOW | AAX_TIMED_TRANSITION)) &&
             (state & AAX_ENVELOPE_FOLLOW_LOG))
         {
            lfo->convert = _exponential;
         }
         else
         {
            lfo->convert = _linear;
         }

         lfo->state = filter->state;
         lfo->fs = filter->info->frequency;
         lfo->period_rate = filter->info->period_rate;
         lfo->stereo_lnk = !stereo;

         lfo->min_sec = filter->slot[0]->param[AAX_LFO_OFFSET]/lfo->fs;
         lfo->max_sec = filter->slot[0]->param[AAX_LFO_DEPTH]/lfo->fs;
         lfo->depth = 1.0f;
         lfo->offset = 0.0f;
         lfo->f = filter->slot[0]->param[AAX_LFO_FREQUENCY];
         lfo->delay = filter->slot[0]->param[AAX_INITIAL_DELAY];
         lfo->inv = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;

         constant = _lfo_set_timing(lfo);
         if (!_lfo_set_function(lfo, constant)) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      break;
   }
   case AAX_FALSE:
      if (filter->slot[0]->data)
      {
         filter->slot[0]->destroy(filter->slot[0]->data);
         filter->slot[0]->data = NULL;
      }
      break;
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
      break;
   }
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewDynamicTimbreFilterHandle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _filter_t* rv = _aaxFilterCreateHandle(info, type, 1, DSIZE);

   if (rv)
   {
      _aax_dsp_copy(rv->slot[0], &p2d->filter[rv->pos]);
      rv->slot[0]->destroy = _lfo_destroy;
      rv->slot[0]->data = NULL;

      rv->state = p2d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxDynamicTimbreFilterSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxDynamicTimbreFilterGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxDynamicTimbreFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxDynamicTimbreRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.01f, 0.0f, 0.0f }, { 10.0f, 50.0f, 1.0f, 1.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {  0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {  0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {  0.0f,  0.0f, 0.0f, 0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxDynamicTimbreRange[slot].min[param],
                       _aaxDynamicTimbreRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxDynamicTimbreFilter =
{
   AAX_FALSE,
   "AAX_dynamic_layer_filter_"AAX_MKSTR(VERSION), VERSION,
   (_aaxFilterCreate*)&_aaxDynamicTimbreFilterCreate,
   (_aaxFilterDestroy*)&_aaxDynamicTimbreFilterDestroy,
   (_aaxFilterReset*)&_lfo_reset,
   (_aaxFilterSetState*)&_aaxDynamicTimbreFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewDynamicTimbreFilterHandle,
   (_aaxFilterConvert*)&_aaxDynamicTimbreFilterSet,
   (_aaxFilterConvert*)&_aaxDynamicTimbreFilterGet,
   (_aaxFilterConvert*)&_aaxDynamicTimbreFilterMinMax
};

