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

#include "lfo.h"
#include "filters.h"
#include "api.h"

#define VERSION	1.02
#define DSIZE	sizeof(_aaxLFOData)

static float _aaxDynamicGainFilterMinMax(float, int, unsigned char);

static aaxFilter
_aaxDynamicGainFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
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
_aaxDynamicGainFilterSetState(_filter_t* filter, int state)
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
         _lfo_setup(lfo, filter->info, filter->state);
         if (filter->type == AAX_COMPRESSOR)
         {
            float f;

            lfo->convert = _linear;
            lfo->envelope = true;
            lfo->stereo_link = true;

            f = filter->slot[0]->param[AAX_RELEASE_RATE];
            lfo->min_sec = _aaxDynamicGainFilterMinMax(f, 0, AAX_RELEASE_RATE);

            f = filter->slot[0]->param[AAX_ATTACK_RATE];
            lfo->max_sec = _aaxDynamicGainFilterMinMax(f, 0, AAX_ATTACK_RATE);

            f = filter->slot[1]->param[AAX_GATE_PERIOD & 0xF];
            lfo->offset = _aaxDynamicGainFilterMinMax(f, 1, AAX_GATE_PERIOD & 0xF);

            f = filter->slot[1]->param[AAX_GATE_THRESHOLD & 0xF];
            lfo->gate_threshold = _aaxDynamicGainFilterMinMax(f, 1, AAX_GATE_THRESHOLD & 0xF);

            lfo->min = filter->slot[0]->param[AAX_THRESHOLD];
            lfo->max = filter->slot[0]->param[AAX_LFO_DEPTH];
            lfo->delay = filter->slot[0]->param[AAX_INITIAL_DELAY];

            _compressor_set_timing(lfo);

            lfo->get = _aaxLFOGetCompressor;
         }
         else
         {
            float depth = filter->slot[0]->param[AAX_LFO_DEPTH];
            float offset = 1.0f - depth;
            int constant;

            if (offset == 0.0f && depth == 0.0f) {
                offset = 1.0f;
            }

            lfo->envelope = false;
            lfo->min_sec = offset/lfo->fs;
            lfo->max_sec =  lfo->min_sec + depth/lfo->fs;
            lfo->delay = -filter->slot[0]->param[AAX_INITIAL_DELAY];
            lfo->f = filter->slot[0]->param[AAX_LFO_FREQUENCY];

            if ((state & AAX_SOURCE_MASK) == AAX_ENVELOPE_FOLLOW)
            {
               lfo->min_sec = 0.5f*depth/lfo->fs;
               lfo->max_sec = 0.5f*depth/lfo->fs + lfo->min_sec;
            }

            constant = _lfo_set_timing(lfo);
            if (!_lfo_set_function(lfo, constant)) {
               _aaxErrorSet(AAX_INVALID_PARAMETER);
            }
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
_aaxNewDynamicGainFilterHandle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
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
_aaxDynamicGainFilterSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxDynamicGainFilterGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxDynamicGainFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxDynamicGainRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.01f, 0.0f, 0.0f }, { 10.0f, 50.0f, 1.0f, 1.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {  0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {  0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {  0.0f,  0.0f, 0.0f, 0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxDynamicGainRange[slot].min[param],
                       _aaxDynamicGainRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxDynamicGainFilter =
{
   "AAX_dynamic_gain_filter_"AAX_MKSTR(VERSION), VERSION,
   (_aaxFilterCreateFn*)&_aaxDynamicGainFilterCreate,
   (_aaxFilterDestroyFn*)&_aaxFilterDestroy,
   (_aaxFilterResetFn*)&_lfo_reset,
   (_aaxFilterSetStateFn*)&_aaxDynamicGainFilterSetState,
   (_aaxNewFilterHandleFn*)&_aaxNewDynamicGainFilterHandle,
   (_aaxFilterConvertFn*)&_aaxDynamicGainFilterSet,
   (_aaxFilterConvertFn*)&_aaxDynamicGainFilterGet,
   (_aaxFilterConvertFn*)&_aaxDynamicGainFilterMinMax
};

