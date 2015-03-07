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
#include "filters.h"
#include "api.h"

static aaxFilter
_aaxTimedGainFilterCreate(aaxConfig config, enum aaxFilterType type)
{
   _handle_t *handle = get_handle(config);
   aaxFilter rv = NULL;
   if (handle)
   {
      unsigned int size = sizeof(_filter_t);
      _filter_t* flt;

      size += (_MAX_ENVELOPE_STAGES/2)*sizeof(_aaxFilterInfo);
      flt = calloc(1, size);
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
         for (i=0; i<_MAX_ENVELOPE_STAGES/2; i++)
         {
            flt->slot[i] = (_aaxFilterInfo*)(ptr + i*size);
            _aaxSetDefaultFilter2d(flt->slot[i], flt->pos);
         }
         rv = (aaxFilter)flt;
      }
   }
   return rv;
}

static int
_aaxTimedGainFilterDestroy(aaxFilter f)
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
_aaxTimedGainFilterSetState(aaxFilter f, int state)
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
      if TEST_FOR_TRUE(state)
      {
         _aaxRingBufferEnvelopeData* env = filter->slot[0]->data;
         if (env == NULL)
         {
            env =  calloc(1, sizeof(_aaxRingBufferEnvelopeData));
            filter->slot[0]->data = env;
         }

         if (env)
         {
            float nextval = filter->slot[0]->param[AAX_LEVEL0];
            float period = filter->info->period_rate;
            float timestep = 1.0f / period;
            int i;

            env->value = nextval;
            env->max_stages = _MAX_ENVELOPE_STAGES;
            for (i=0; i<_MAX_ENVELOPE_STAGES/2; i++)
            {
               float dt, value = nextval;
               uint32_t max_pos;

               max_pos = (uint32_t)-1;
               dt = filter->slot[i]->param[AAX_TIME0];
               if (dt != MAXFLOAT)
               {
                  if (dt < timestep && dt > EPS) dt = timestep;
                  max_pos = rintf(dt * period);
               }
               if (max_pos == 0)
               {
                  env->max_stages = 2*i;
                  break;
               }

               nextval = filter->slot[i]->param[AAX_LEVEL1];
               if (nextval == 0.0f) nextval = -1e-2f;
               env->step[2*i] = (nextval - value)/max_pos;
               env->max_pos[2*i] = max_pos;;

               /* prevent a core dump for accessing an illegal slot */
               if (i == (_MAX_ENVELOPE_STAGES/2)-1) break;

               max_pos = (uint32_t)-1;
               dt = filter->slot[i]->param[AAX_TIME1];
               if (dt != MAXFLOAT)
               {
                  if (dt < timestep && dt > EPS) dt = timestep;
                  max_pos = rintf(dt * period);
               }
               if (max_pos == 0)
               {
                  env->max_stages = 2*i+1;
                  break;
               }

               value = nextval;
               nextval = filter->slot[i+1]->param[AAX_LEVEL0];
               if (nextval == 0.0f) nextval = -1e-2f;
               env->step[2*i+1] = (nextval - value)/max_pos;
               env->max_pos[2*i+1] = max_pos;
            }
         }
         else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      }
      else
      {
         free(filter->slot[0]->data);
         filter->slot[0]->data = NULL;
      }
   }
#endif
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewTimedGainFilterHandle(_aaxMixerInfo* info, enum aaxFilterType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   _filter_t* rv = NULL;
   if (type < AAX_FILTER_MAX)
   {
      unsigned int size = sizeof(_filter_t);

      size += (_MAX_ENVELOPE_STAGES/2)*sizeof(_aaxFilterInfo);
      rv = calloc(1, size);
      if (rv)
      {
         char *ptr = (char*)rv + sizeof(_filter_t);
         _aaxRingBufferEnvelopeData *env;
         unsigned int no_steps;
         float dt, value;
         int i, stages;

         rv->id = FILTER_ID;
         rv->info = info ? info : _info;
         rv->slot[0] = (_aaxFilterInfo*)ptr;
         rv->pos = _flt_cvt_tbl[type].pos;
         rv->state = p2d->filter[rv->pos].state;
         rv->type = type;

         size = sizeof(_aaxFilterInfo);

         env = (_aaxRingBufferEnvelopeData*)p2d->filter[rv->pos].data;
         memcpy(rv->slot[0], &p2d->filter[rv->pos], size);
         rv->slot[0]->data = NULL;

         i = 0;
         if (env->max_pos[1] > env->max_pos[0]) i = 1;
         dt = p2d->filter[rv->pos].param[2*i+1] / env->max_pos[i];

         no_steps = env->max_pos[1];
         value = p2d->filter[rv->pos].param[AAX_LEVEL1];
         value += env->step[1] * no_steps;

         stages = _MIN(1+env->max_stages/2, _MAX_ENVELOPE_STAGES/2);
         for (i=1; i<stages; i++)
         {
            _aaxFilterInfo* slot;

            slot = (_aaxFilterInfo*)(ptr + i*size);
            rv->slot[i] = slot;

            no_steps = env->max_pos[2*i];
            slot->param[0] = value;
            slot->param[1] = no_steps * dt;

            value += env->step[2*i] * no_steps;
            no_steps = env->max_pos[2*i+1];
            slot->param[2] = value;
            slot->param[3] = no_steps * dt;

            value += env->step[2*i+1] * no_steps;
         }
      }
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxTimedGainFilter =
{
   AAX_FALSE,
   "AAX_timed_gain_filter",
   (_aaxFilterCreate*)&_aaxTimedGainFilterCreate,
   (_aaxFilterDestroy*)&_aaxTimedGainFilterDestroy,
   (_aaxFilterSetState*)&_aaxTimedGainFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewTimedGainFilterHandle
};

