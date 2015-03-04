/*
 * Copyright 2007-2015 by Erik Hofman.
 * Copyright 2009-2015 by Adalin B.V.
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
#include "api.h"

static aaxFilter
_aaxDynamicGainFilterCreate(aaxConfig config, enum aaxFilterType type)
{
   _handle_t *handle = get_handle(config);
   aaxFilter rv = NULL;
   if (handle)
   {
      unsigned int size = sizeof(_filter_t) + sizeof(_aaxFilterInfo);
     _filter_t* flt = calloc(1, size);

      if (flt)
      {
         char *ptr;
         int i;

         flt->id = FILTER_ID;
         flt->state = AAX_FALSE;
         flt->info = handle->info ? handle->info : _info;

         ptr = (char*)flt + sizeof(_filter_t);
         flt->slot[0] = (_aaxFilterInfo*)ptr;
         flt->pos = _flt_cvt_tbl[type].pos;
         flt->type = type;

         size = sizeof(_aaxFilterInfo);
         _aaxSetDefaultFilter2d(flt->slot[0], flt->pos);
         rv = (aaxFilter)flt;
      }
   }
   return rv;
}

static int
_aaxDynamicGainFilterDestroy(aaxFilter f)
{
   _filter_t* filter = get_filter(f);
   int rv = AAX_FALSE;
   if (filter)
   {
      free(filter->slot[0]->data);
      filter->slot[0]->data = NULL;
      free(filter);
      rv = AAX_TRUE;
   }
   return rv;
}

static aaxFilter
_aaxDynamicGainFilterSetState(aaxFilter f, int state)
{
   _filter_t* filter = get_filter(f);
   aaxFilter rv = NULL;
   unsigned slot;

   assert(f);

   filter->state = state;
   filter->slot[0]->state = state;

   /*
    * Make sure parameters are actually within their expected boundaries.
    */
   slot = 0;
   while ((slot < _MAX_FE_SLOTS) && filter->slot[slot])
   {
      int i, type = filter->type;
      for(i=0; i<4; i++)
      {
         if (!is_nan(filter->slot[slot]->param[i]))
         {
            float min = _flt_minmax_tbl[slot][type].min[i];
            float max = _flt_minmax_tbl[slot][type].max[i];
            cvtfn_t cvtfn = filter_get_cvtfn(filter->type, AAX_LINEAR, WRITEFN, i);
            filter->slot[slot]->param[i] =
                      _MINMAX(cvtfn(filter->slot[slot]->param[i]), min, max);
         }
      }
      slot++;
   }

#if !ENABLE_LITE
   if EBF_VALID(filter)
   {
      switch (state & ~AAX_INVERSE)
      {
      case AAX_CONSTANT_VALUE:
      case AAX_TRIANGLE_WAVE:
      case AAX_SINE_WAVE:
      case AAX_SQUARE_WAVE:
      case AAX_SAWTOOTH_WAVE:
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
               case AAX_SAWTOOTH_WAVE:
                  lfo->step[t] *= 0.5f;
                  break;
               case AAX_ENVELOPE_FOLLOW:
               {
                  if (filter->type == AAX_COMPRESSOR)
                  {		// 10dB
                     float dt = 3.16228f/filter->info->period_rate;
                     float min, max, rate;

                     /*
                      * We're implementing an upward dynamic range
                      * compressor, which means that attack is down!
                      */
                     min = _flt_minmax_tbl[0][AAX_COMPRESSOR].min[AAX_RELEASE_RATE];
                     max = _flt_minmax_tbl[0][AAX_COMPRESSOR].max[AAX_RELEASE_RATE];
                     rate = filter->slot[0]->param[AAX_RELEASE_RATE];
                     rate = _MINMAX(rate, min, max);
                     lfo->step[t] = _MIN(dt/rate, 2.0f);

                     min = _flt_minmax_tbl[0][AAX_COMPRESSOR].min[AAX_ATTACK_RATE];
                     max = _flt_minmax_tbl[0][AAX_COMPRESSOR].max[AAX_ATTACK_RATE];
                     rate = filter->slot[0]->param[AAX_ATTACK_RATE];
                     rate = _MINMAX(rate, min, max);
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
               case AAX_CONSTANT_VALUE: /* equals to AAX_TRUE */
                  lfo->get = _aaxRingBufferLFOGetFixedValue;
                  break;
               case AAX_TRIANGLE_WAVE:
                  lfo->get = _aaxRingBufferLFOGetTriangle;
                  break;
               case AAX_SINE_WAVE:
                  lfo->get = _aaxRingBufferLFOGetSine;
                  break;
               case AAX_SQUARE_WAVE:
                  lfo->get = _aaxRingBufferLFOGetSquare;
                  break;
               case AAX_SAWTOOTH_WAVE:
                  lfo->get = _aaxRingBufferLFOGetSawtooth;
                  break;
               case AAX_ENVELOPE_FOLLOW:
                  if (filter->type == AAX_COMPRESSOR)
                  {
                     float dt = 1.0f/filter->info->period_rate;
                     float min, max, f;

                     min = _flt_minmax_tbl[1][AAX_COMPRESSOR].min[AAX_GATE_PERIOD & 0xF];
                     max = _flt_minmax_tbl[1][AAX_COMPRESSOR].max[AAX_GATE_PERIOD & 0xF];
                     f = filter->slot[1]->param[AAX_GATE_PERIOD & 0xF];
                     f = _MINMAX(f, min, max);
                     lfo->gate_period = GMATH_E1 * _MIN(dt/f, 2.0f);

                     min = _flt_minmax_tbl[1][AAX_COMPRESSOR].min[AAX_GATE_THRESHOLD & 0xF];            
                     max = _flt_minmax_tbl[1][AAX_COMPRESSOR].max[AAX_GATE_THRESHOLD & 0xF];
                     f = filter->slot[1]->param[AAX_GATE_THRESHOLD & 0xF];
                     f = _MINMAX(f, min, max);
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
   }
#endif
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewDynamicGainFilterHandle(_aaxMixerInfo* info, enum aaxFilterType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   _filter_t* rv = NULL;
   if (type < AAX_FILTER_MAX)
   {
      unsigned int size = sizeof(_filter_t) + sizeof(_aaxFilterInfo);

      rv = calloc(1, size);
      if (rv)
      {
         char *ptr = (char*)rv + sizeof(_filter_t);

         rv->id = FILTER_ID;
         rv->info = info ? info : _info;
         rv->slot[0] = (_aaxFilterInfo*)ptr;
         rv->pos = _flt_cvt_tbl[type].pos;
         rv->state = p2d->filter[rv->pos].state;
         rv->type = type;

         size = sizeof(_aaxFilterInfo);
         memcpy(rv->slot[0], &p2d->filter[rv->pos], size);
         rv->slot[0]->data = NULL;
      }
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxDynamicGainFilter =
{
   "AAX_dynamic_gain_filter",
   _aaxDynamicGainFilterCreate,
   _aaxDynamicGainFilterDestroy,
   _aaxDynamicGainFilterSetState,
   _aaxNewDynamicGainFilterHandle
};

