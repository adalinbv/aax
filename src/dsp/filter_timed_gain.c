/*
 * Copyright 2007-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
_aaxTimedGainFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   unsigned int size = sizeof(_filter_t);
   _filter_t* flt;
   aaxFilter rv = NULL;

   size += (_MAX_ENVELOPE_STAGES/2)*sizeof(_aaxFilterInfo);
   flt = calloc(1, size);
   if (flt)
   {
      char *ptr;
      int i;

      flt->id = FILTER_ID;
      flt->state = AAX_FALSE;
      flt->info = info;

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
   return rv;
}

static int
_aaxTimedGainFilterDestroy(_filter_t* filter)
{
   free(filter->slot[0]->data);
   filter->slot[0]->data = NULL;
   free(filter);

   return AAX_TRUE;
}

static aaxFilter
_aaxTimedGainFilterSetState(_filter_t* filter, int state)
{
   void *handle = filter->handle;
   aaxFilter rv = NULL;

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
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewTimedGainFilterHandle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   unsigned int size = sizeof(_filter_t);
   _filter_t* rv = NULL;

   size += (_MAX_ENVELOPE_STAGES/2)*sizeof(_aaxFilterInfo);
   rv = calloc(1, size);
   if (rv)
   {
      _handle_t *handle = get_driver_handle(config);
      _aaxMixerInfo* info = handle ? handle->info : _info;
      char *ptr = (char*)rv + sizeof(_filter_t);
      _aaxRingBufferEnvelopeData *env;
      unsigned int no_steps;
      float dt, value;
      int i, stages;

      rv->id = FILTER_ID;
      rv->info = info;
      rv->handle = handle;
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
   return rv;
}

static float
_aaxTimedGainFilterSet(float val, int ptype, UNUSED(unsigned char param))
{
   float rv = val;
   if (ptype == AAX_LOGARITHMIC) {
      rv = _lin2db(val);
   }
   return rv;
}

static float
_aaxTimedGainFilterGet(float val, int ptype, UNUSED(unsigned char param))
{
   float rv = val;
   if (ptype == AAX_LOGARITHMIC) {
      rv = _db2lin(val);
   }
   return rv;
}

static float
_aaxTimedGainFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxTimedGainRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, MAXFLOAT, 4.0f, MAXFLOAT } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, MAXFLOAT, 4.0f, MAXFLOAT } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, MAXFLOAT, 4.0f, MAXFLOAT } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxTimedGainRange[slot].min[param],
                       _aaxTimedGainRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxTimedGainFilter =
{
   AAX_FALSE,
   "AAX_timed_gain_filter", 1.0f,
   (_aaxFilterCreate*)&_aaxTimedGainFilterCreate,
   (_aaxFilterDestroy*)&_aaxTimedGainFilterDestroy,
   (_aaxFilterSetState*)&_aaxTimedGainFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewTimedGainFilterHandle,
   (_aaxFilterConvert*)&_aaxTimedGainFilterSet,
   (_aaxFilterConvert*)&_aaxTimedGainFilterGet,
   (_aaxFilterConvert*)&_aaxTimedGainFilterMinMax
};

