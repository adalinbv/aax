/*
 * Copyright 2007-2018 by Erik Hofman.
 * Copyright 2009-2018 by Adalin B.V.
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

#include "common.h"
#include "filters.h"
#include "arch.h"
#include "api.h"

#define DSIZE	sizeof(_aaxRingBufferBitCrusherData)

static float _aaxBitCrusherFilterMinMax(float, int, unsigned char);

static aaxFilter
_aaxBitCrusherFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, 1, DSIZE);
   aaxFilter rv = NULL;

   if (flt)
   {
      _aaxSetDefaultFilter2d(flt->slot[0], flt->pos, 0);
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxBitCrusherFilterDestroy(_filter_t* filter)
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
_aaxBitCrusherFilterSetState(_filter_t* filter, int state)
{
   void *handle = filter->handle;
   aaxFilter rv = AAX_FALSE;
   int stereo;

   assert(filter->info);

   stereo = (state & AAX_LFO_STEREO) ? AAX_TRUE : AAX_FALSE;
   state &= ~AAX_LFO_STEREO;

   filter->state = state;
   switch (state & ~(AAX_INVERSE|AAX_ENVELOPE_FOLLOW))
   {
   case AAX_CONSTANT_VALUE:
   case AAX_TRIANGLE_WAVE:
   case AAX_SINE_WAVE:
   case AAX_SQUARE_WAVE:
   case AAX_IMPULSE_WAVE:
   case AAX_SAWTOOTH_WAVE:
   {
      _aaxRingBufferBitCrusherData *bitcrush = filter->slot[0]->data;
      if (bitcrush == NULL)
      {
         bitcrush = _aax_aligned_alloc(DSIZE);
         filter->slot[0]->data = bitcrush;
         if (bitcrush) memset(bitcrush, 0, DSIZE);
      }

      if (bitcrush)
      {
         float offs, depth;
         int constant;

         /* bit reduction */
         bitcrush->lfo.convert = _linear;
         bitcrush->lfo.state = filter->state & ~AAX_ENVELOPE_FOLLOW;
         bitcrush->lfo.fs = filter->info->frequency;
         bitcrush->lfo.period_rate = filter->info->period_rate;
         bitcrush->lfo.stereo_lnk = !stereo;

         offs = filter->slot[0]->param[AAX_LFO_OFFSET];
         depth = filter->slot[0]->param[AAX_LFO_DEPTH];
         if ((offs + depth) > 1.0f) {
            depth = 1.0f - offs;
         }

         bitcrush->lfo.min_sec = offs/bitcrush->lfo.fs;
         bitcrush->lfo.max_sec = bitcrush->lfo.min_sec + depth/bitcrush->lfo.fs;
         bitcrush->lfo.depth = 1.0f;
         bitcrush->lfo.offset = 0.0f;
         bitcrush->lfo.f = filter->slot[0]->param[AAX_LFO_FREQUENCY];
         bitcrush->lfo.inv = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;

         constant = _lfo_set_timing(&bitcrush->lfo);
         bitcrush->lfo.envelope = AAX_FALSE;
         if (!_lfo_set_function(&bitcrush->lfo, constant)) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }

         /* noise */
         memcpy(&bitcrush->env, &bitcrush->lfo, sizeof(_aaxLFOData));
         bitcrush->env.state = state & (AAX_INVERSE|AAX_ENVELOPE_FOLLOW);
         bitcrush->env.f = 10.0f;

         constant = _lfo_set_timing(&bitcrush->env);
         bitcrush->env.envelope = AAX_FALSE;
         if (!_lfo_set_function(&bitcrush->env, constant)) {
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
_aaxNewBitCrusherFilterHandle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _filter_t* rv = _aaxFilterCreateHandle(info, type, 1, DSIZE);

   if (rv)
   {
      _aax_dsp_copy(rv->slot[0], &p2d->filter[rv->pos]);
      rv->slot[0]->data = NULL;

      rv->state = p2d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxBitCrusherFilterSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxBitCrusherFilterGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxBitCrusherFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxBitCrusherRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.01f, 0.0f, 0.0f }, { 2.0f, 50.0f, 2.0f, 2.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, { 0.0f,  0.0f, 0.0f, 0.0f } }, 
    { { 0.0f, 0.0f,  0.0f, 0.0f }, { 0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, { 0.0f,  0.0f, 0.0f, 0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxBitCrusherRange[slot].min[param],
                       _aaxBitCrusherRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxBitCrusherFilter =
{
   AAX_TRUE,
   "AAX_bitcrusher_filter", 1.0f,
   (_aaxFilterCreate*)&_aaxBitCrusherFilterCreate,
   (_aaxFilterDestroy*)&_aaxBitCrusherFilterDestroy,
   (_aaxFilterSetState*)&_aaxBitCrusherFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewBitCrusherFilterHandle,
   (_aaxFilterConvert*)&_aaxBitCrusherFilterSet,
   (_aaxFilterConvert*)&_aaxBitCrusherFilterGet,
   (_aaxFilterConvert*)&_aaxBitCrusherFilterMinMax
};

