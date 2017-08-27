/*
 * Copyright 2007-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
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

static float _aaxCompressorMinMax(float, int, unsigned char);

static aaxFilter
_aaxCompressorCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   unsigned int size = sizeof(_filter_t) + 2*sizeof(_aaxFilterInfo);
  _filter_t* flt = calloc(1, size);
   aaxFilter rv = NULL;

   if (flt)
   {
      char *ptr;

      flt->id = FILTER_ID;
      flt->state = AAX_FALSE;
      flt->info = info;

      ptr = (char*)flt + sizeof(_filter_t);
      flt->slot[0] = (_aaxFilterInfo*)ptr;
      flt->pos = _flt_cvt_tbl[type].pos;
      flt->type = type;

      size = sizeof(_aaxFilterInfo);
      flt->slot[1] = (_aaxFilterInfo*)(ptr + size);
      flt->slot[1]->param[AAX_GATE_PERIOD & 0xF] = 0.25f;
      flt->slot[1]->param[AAX_GATE_THRESHOLD & 0xF] = 0.0f;
      _aaxSetDefaultFilter2d(flt->slot[0], flt->pos);
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxCompressorDestroy(_filter_t* filter)
{
   free(filter->slot[0]->data);
   filter->slot[0]->data = NULL;
   free(filter);

   return AAX_TRUE;
}

static aaxFilter
_aaxCompressorSetState(_filter_t* filter, int state)
{
   void *handle = filter->handle;
   aaxFilter rv = NULL;

   state = state ? state|AAX_ENVELOPE_FOLLOW : AAX_FALSE;
   switch (state & ~AAX_INVERSE)
   {
   case AAX_ENVELOPE_FOLLOW:
   {
      _aaxRingBufferLFOData* lfo = filter->slot[0]->data;
      if (lfo == NULL)
      {
         lfo = malloc(sizeof(_aaxRingBufferLFOData));
         filter->slot[0]->data = lfo;
      }

      if (lfo)
      {
         float depth, offs = 0.0f;
         int t;
			// AAX_LFO_DEPTH == AAX_COMPRESSION_RATIO
         depth = _MAX(filter->slot[0]->param[AAX_LFO_DEPTH], 0.01f);
         if ((state & ~AAX_INVERSE) == AAX_ENVELOPE_FOLLOW)
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

                  lfo->get = _aaxRingBufferLFOGetCompressor;
               }
               else
               {
                  lfo->get = _aaxRingBufferLFOGetGainFollow;
                  lfo->max *= 10.0f; // maximum compression factor
               }
               lfo->envelope = AAX_TRUE;
               lfo->stereo_lnk = AAX_TRUE;
               break;
            default:
               break;
            }
         } else {
            lfo->get = _aaxRingBufferLFOGetFixedValue;
         }
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      break;
   }
   case AAX_FALSE:
      free(filter->slot[0]->data);
      filter->slot[0]->data = NULL;
      break;
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
      break;
   }
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewCompressorHandle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   unsigned int size = sizeof(_filter_t) + 2*sizeof(_aaxFilterInfo);
   _filter_t* rv = calloc(1, size);

   if (rv)
   {
      _handle_t *handle = get_driver_handle(config);
      _aaxMixerInfo* info = handle ? handle->info : _info;
      char *ptr = (char*)rv + sizeof(_filter_t);

      rv->id = FILTER_ID;
      rv->info = info;
      rv->handle = handle;
      rv->slot[0] = (_aaxFilterInfo*)ptr;
      rv->pos = _flt_cvt_tbl[type].pos;
      rv->state = p2d->filter[rv->pos].state;
      rv->type = type;

      size = sizeof(_aaxFilterInfo);
      rv->slot[1] = (_aaxFilterInfo*)(ptr + size);
      rv->slot[1]->param[AAX_GATE_PERIOD & 0xF] = 0.25f;
      rv->slot[1]->param[AAX_GATE_THRESHOLD & 0xF] = 0.0f;
      memcpy(rv->slot[0], &p2d->filter[rv->pos], size);
      rv->slot[0]->data = NULL;
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
    { { 1e-3f, 1e-3f, 0.0f, 0.0f }, { 0.25f, 10.0f, 1.0f, 1.0f } },
    { {  0.0f, 1e-3f, 0.0f, 0.0f }, { 0.0f,  10.0f, 0.0f, 1.0f } },
    { {  0.0f,  0.0f, 0.0f, 0.0f }, { 0.0f,   0.0f, 0.0f, 0.0f } }
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
   (_aaxFilterSetState*)&_aaxCompressorSetState,
   (_aaxNewFilterHandle*)&_aaxNewCompressorHandle,
   (_aaxFilterConvert*)&_aaxCompressorSet,
   (_aaxFilterConvert*)&_aaxCompressorGet,
   (_aaxFilterConvert*)&_aaxCompressorMinMax
};

