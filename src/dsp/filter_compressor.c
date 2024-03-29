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

#include "lfo.h"
#include "filters.h"
#include "api.h"

#define DSIZE	sizeof(_aaxLFOData)

static float _aaxCompressorMinMax(float, int, unsigned char);

static aaxFilter
_aaxCompressorCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, 2, DSIZE);
   aaxFilter rv = NULL;

   if (flt)
   {
      flt->slot[1]->param[AAX_GATE_PERIOD & 0xF] = 0.25f;
      flt->slot[1]->param[AAX_GATE_THRESHOLD & 0xF] = 0.0f;
      _aaxSetDefaultFilter2d(flt->slot[0], flt->pos, 0);
      flt->slot[0]->destroy = _lfo_destroy;
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxCompressorDestroy(_filter_t* filter)
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
_aaxCompressorSetState(_filter_t* filter, int state)
{
   void *handle = filter->handle;
   aaxFilter rv = NULL;
   int mask;

   // backwards compatibility, can be removed after AeonWave 4.0
   if (state & AAX_ENVELOPE_FOLLOW_LOG && !(state & AAX_WAVEFORM_MASK) &&
       !(state & (AAX_ENVELOPE_FOLLOW|AAX_TIMED_TRANSITION)))
   {
      state -= AAX_ENVELOPE_FOLLOW_LOG;
      state += AAX_ENVELOPE_FOLLOW;
   }

   state = state ? state|AAX_ENVELOPE_FOLLOW_MASK : AAX_FALSE;
   if (state & ~AAX_TRUE) {
      state &= ~AAX_TRUE;
   }

   mask = (AAX_INVERSE|AAX_LFO_STEREO|AAX_ENVELOPE_FOLLOW_LOG);
   switch (state & ~mask)
   {
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
         float depth, offs = 0.0f;
         int t;

			// AAX_LFO_DEPTH == AAX_COMPRESSION_RATIO
         depth = _MAX(filter->slot[0]->param[AAX_LFO_DEPTH], 0.01f);
         if ((state & ~AAX_INVERSE) == AAX_ENVELOPE_FOLLOW ||
             (state & ~AAX_INVERSE) == AAX_ENVELOPE_FOLLOW_LOG ||
             (state & ~AAX_INVERSE) == AAX_ENVELOPE_FOLLOW_MASK)
         {
            if (filter->type == AAX_COMPRESSOR)
            {
               lfo->min = filter->slot[0]->param[AAX_THRESHOLD];
               lfo->max = depth;
            }
            else
            {
               offs = 0.49f*filter->slot[0]->param[AAX_LFO_OFFSET];
               depth *= 0.5f;
               lfo->min = offs;
               lfo->max = offs + depth;
            }
         }
         else 
         {
            lfo->min = offs;
            lfo->max = offs + depth;
         }
         lfo->envelope = AAX_FALSE;
         lfo->stereo_lnk = AAX_FALSE;
         lfo->f = filter->slot[0]->param[AAX_LFO_FREQUENCY];
         lfo->inv = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;
         lfo->convert = _linear;

         for (t=0; t<_AAX_MAX_SPEAKERS; t++)
         {
            lfo->step[t] = 2.0f*depth * lfo->f;
            lfo->step[t] *= (lfo->max - lfo->min);
            lfo->step[t] /= filter->info->period_rate;
            lfo->value[t] = 1.0f;

            switch (state & ~AAX_INVERSE)
            {
            case AAX_ENVELOPE_FOLLOW:
            case AAX_ENVELOPE_FOLLOW_LOG:
            case AAX_ENVELOPE_FOLLOW_MASK:
            {
               if (filter->type == AAX_COMPRESSOR)
               {		// 10dB
                  float dt = 3.16228f/filter->info->period_rate;
                  float rate;

                  /*
                   * We're implementing an upward dynamic range
                   * compressor, which means that attack is down!
                   */
                  rate = filter->slot[0]->param[AAX_RELEASE_RATE];
                  rate = _aaxCompressorMinMax(rate, 0, AAX_RELEASE_RATE);
                  lfo->step[t] = _MIN(dt/rate, 2.0f);

                  rate = filter->slot[0]->param[AAX_ATTACK_RATE];
                  rate = _aaxCompressorMinMax(rate, 0, AAX_ATTACK_RATE);
                  lfo->down[t] = _MIN(dt/rate, 2.0f);
               }
               else {
                  lfo->step[t] = atanf(lfo->f*0.1f)/atanf(100.0f);
               }
               break;
            }
            default:
               break;
            }
         }

         if (depth > 0.001f)
         {
            switch (state & ~AAX_INVERSE)
            {
            case AAX_ENVELOPE_FOLLOW:
            case AAX_ENVELOPE_FOLLOW_LOG:
            case AAX_ENVELOPE_FOLLOW_MASK:
               lfo->envelope = AAX_TRUE;
               lfo->stereo_lnk = AAX_TRUE;
               if (filter->type == AAX_COMPRESSOR)
               {
                  float dt = 1.0f/filter->info->period_rate;
                  float f;

                  f = filter->slot[1]->param[AAX_GATE_PERIOD & 0xF];
                  f = _aaxCompressorMinMax(f, 1, AAX_GATE_PERIOD & 0xF);
                  lfo->gate_period = GMATH_E1 * _MIN(dt/f, 2.0f);

                  f = filter->slot[1]->param[AAX_GATE_THRESHOLD & 0xF];
                  f = _aaxCompressorMinMax(f, 1, AAX_GATE_THRESHOLD & 0xF);
                  lfo->gate_threshold = f;

                  lfo->get = _aaxLFOGetCompressor;
               }
               else
               {
                  lfo->get = _aaxLFOGetGainFollow;
                  lfo->max *= 10.0f; // maximum compression factor
               }
               break;
            default:
               break;
            }
         } else {
            lfo->get = _aaxLFOGetFixedValue;
         }
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      break;
   }
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
      // inetnional fall-through
   case AAX_FALSE:
      if (filter->slot[0]->data)
      {
         filter->slot[0]->destroy(filter->slot[0]->data);
         filter->slot[0]->data = NULL;
      }
      break;
   }
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewCompressorHandle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _filter_t* rv = _aaxFilterCreateHandle(info, type, 2, 0);

   if (rv)
   {
      _aax_dsp_copy(rv->slot[0], &p2d->filter[rv->pos]);
      rv->slot[0]->destroy = _lfo_destroy;

      rv->slot[1]->param[AAX_GATE_PERIOD & 0xF] = 0.25f;
      rv->slot[1]->param[AAX_GATE_THRESHOLD & 0xF] = 0.0f;

      rv->state = p2d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxCompressorSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxCompressorGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxCompressorMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxCompressorRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 1e-3f, 1e-3f, 0.0f,  0.0f }, { 0.25f, 10.0f, 1.0f, 1.0f } },
    { {  0.0f, 1e-3f, 0.0f, 1e-3f }, { 0.0f,  10.0f, 0.0f, 1.0f } },
    { {  0.0f,  0.0f, 0.0f,  0.0f }, { 0.0f,   0.0f, 0.0f, 0.0f } },
    { {  0.0f,  0.0f, 0.0f,  0.0f }, { 0.0f,   0.0f, 0.0f, 0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxCompressorRange[slot].min[param],
                       _aaxCompressorRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxCompressor =
{
   AAX_TRUE,
   "AAX_compressor", 1.0f,
   (_aaxFilterCreate*)&_aaxCompressorCreate,
   (_aaxFilterDestroy*)&_aaxCompressorDestroy,
   NULL,
   (_aaxFilterSetState*)&_aaxCompressorSetState,
   (_aaxNewFilterHandle*)&_aaxNewCompressorHandle,
   (_aaxFilterConvert*)&_aaxCompressorSet,
   (_aaxFilterConvert*)&_aaxCompressorGet,
   (_aaxFilterConvert*)&_aaxCompressorMinMax
};

