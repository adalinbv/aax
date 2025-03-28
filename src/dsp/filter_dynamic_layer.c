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

#include <base/types.h>		/* for rintf */
#include <base/gmath.h>
#include <base/random.h>

#include "lfo.h"
#include "filters.h"
#include "api.h"

#define VERSION	1.0
#define DSIZE	sizeof(_aaxLFOData)

static float _aaxDynamicLayerFilterMinMax(float, int, unsigned char);

static aaxFilter
_aaxDynamicLayerFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
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

static aaxFilter
_aaxDynamicLayerFilterSetState(_filter_t* filter, int state)
{
   void *handle = filter->handle;
   aaxFilter rv = false;

   assert(filter->info);

   filter->state = state;
   switch (state & (AAX_SOURCE_MASK & ~AAX_PURE_WAVEFORM))
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
      _aaxLFOData* lfo = filter->slot[0]->data;
      if (lfo == NULL)
      {
         filter->slot[0]->data = lfo = _lfo_create();
         if (lfo) filter->slot[0]->data_size = DSIZE;
      }

      if (lfo)
      {
         int constant;

         _lfo_setup(lfo, filter->info, filter->state);

         lfo->min_sec = filter->slot[0]->param[AAX_LFO_MIN]/lfo->fs;
         lfo->max_sec = filter->slot[0]->param[AAX_LFO_MAX]/lfo->fs;
         lfo->f = filter->slot[0]->param[AAX_LFO_FREQUENCY];
         lfo->delay = filter->slot[0]->param[AAX_INITIAL_DELAY];

         if ((state & AAX_SOURCE_MASK) == AAX_RANDOM_SELECT)
         {
            lfo->min_sec += (lfo->max_sec-lfo->min_sec)*_aax_random();
            lfo->max_sec = 0.0f;
         }

         constant = _lfo_set_timing(lfo);
         if (!_lfo_set_function(lfo, constant)) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      break;
   }
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
      // intentional fall-through
   case AAX_FALSE:
      if (filter->slot[0]->data)
      {
         filter->slot[0]->destroy(filter->slot[0]->data);
         filter->slot[0]->data_size = 0;
         filter->slot[0]->data = NULL;
      }
      break;
   }
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewDynamicLayerFilterHandle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _filter_t* rv = _aaxFilterCreateHandle(info, type, 1, 0);

   if (rv)
   {
      _aax_dsp_copy(rv->slot[0], &p2d->filter[rv->pos]);
      rv->slot[0]->destroy = _lfo_destroy;
      rv->state = p2d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxDynamicLayerFilterSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxDynamicLayerFilterGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxDynamicLayerFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxDynamicLayerRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.01f, 0.0f, 0.0f }, { 10.0f, 50.0f, 1.0f, 1.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {  0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {  0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {  0.0f,  0.0f, 0.0f, 0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxDynamicLayerRange[slot].min[param],
                       _aaxDynamicLayerRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxDynamicLayerFilter =
{
   "AAX_dynamic_layer_filter", VERSION,
   (_aaxFilterCreateFn*)&_aaxDynamicLayerFilterCreate,
   (_aaxFilterDestroyFn*)&_aaxFilterDestroy,
   (_aaxFilterResetFn*)&_lfo_reset,
   (_aaxFilterSetStateFn*)&_aaxDynamicLayerFilterSetState,
   (_aaxNewFilterHandleFn*)&_aaxNewDynamicLayerFilterHandle,
   (_aaxFilterConvertFn*)&_aaxDynamicLayerFilterSet,
   (_aaxFilterConvertFn*)&_aaxDynamicLayerFilterGet,
   (_aaxFilterConvertFn*)&_aaxDynamicLayerFilterMinMax
};

