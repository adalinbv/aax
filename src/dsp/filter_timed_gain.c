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
#include "arch.h"
#include "api.h"

#define VERSION	1.03
#define DSIZE	sizeof(_aaxEnvelopeData)

static aaxFilter
_aaxTimedGainFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, _MAX_ENVELOPE_STAGES/2, DSIZE);
   aaxFilter rv = NULL;

   if (flt)
   {
      unsigned s;
      for (s=0; s<_MAX_ENVELOPE_STAGES/2; s++) {
         _aaxSetDefaultFilter2d(flt->slot[s], flt->pos, s);
      }
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxTimedGainFilterDestroy(_filter_t* filter)
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
_aaxTimedGainFilterSetState(_filter_t* filter, int state)
{
   void *handle = filter->handle;
   aaxFilter rv = NULL;

   if TEST_FOR_TRUE(state)
   {
      _aaxEnvelopeData* env = filter->slot[0]->data;
      if (env == NULL)
      {
         env = _aax_aligned_alloc(DSIZE);
         filter->slot[0]->data = env;
         if (env) memset(env, 0, DSIZE);
      }

      if (env)
      {
         float nextval = filter->slot[0]->param[AAX_LEVEL0];
         float period = filter->info->period_rate;
         float timestep = 1.0f / period;
         float release_factor = 1.0f;
         float max_dt = 0.0f;
         int i, stage;

         env->state = state;
         if (state & AAX_REPEAT)
         {
            env->repeat0 = (state & ~AAX_REPEAT);
            if (env->repeat0 > 1)
            {
               if (env->repeat0 == AAX_MAX_REPEAT) {
                  env->repeat0 = UINT_MAX;
               }
               env->sustain = AAX_TRUE;
            }
            env->repeat = env->repeat0;
         }
         else if (state & AAX_RELEASE_FACTOR) {
            release_factor = 0.01f*(state & AAX_REPEAT_MASK);
         }

         stage = 0;
         env->step_finish = -(2.5f/release_factor)*timestep;
         env->value0 = env->value = env->value_max = nextval;
         env->max_stages = _MAX_ENVELOPE_STAGES-1;
         for (i=0; i<_MAX_ENVELOPE_STAGES/2; i++)
         {
            uint32_t max_pos;
            float dt, value;

            max_pos = (uint32_t)-1;
            dt = filter->slot[i]->param[AAX_TIME0];
            if (dt != FLT_MAX)
            {
               if (dt < timestep && dt > EPS) dt = timestep;
               max_pos = rintf(dt * period);
               if (!env->sustain_stage && dt > max_dt)
               {
                   env->sustain_stage = stage;
                   max_dt = dt;
               }
            }
            else
            {
               env->sustain = AAX_TRUE;
               env->sustain_stage = stage;
            }

            if (max_pos == 0)
            {
               env->max_stages = stage;
               max_pos = 1;
            }

            value = _MAX(nextval, 0.0f);
            nextval = filter->slot[i]->param[AAX_LEVEL1];
            if (value > env->value_max) env->value_max = value;
            if (nextval == 0.0f) nextval = -5e-2f;
            env->step[stage] = (nextval - value)/max_pos;
            env->max_pos[stage] = max_pos;
            stage++;

            max_pos = (uint32_t)-1;
            dt = filter->slot[i]->param[AAX_TIME1];
            if (dt != FLT_MAX)
            {
               if (dt < timestep && dt > EPS) dt = timestep;
               max_pos = rintf(dt * period);
               if (!env->sustain_stage && dt > max_dt)
               {
                   env->sustain_stage = stage;
                   max_dt = dt;
               }
            }
            else
            {
               env->sustain = AAX_TRUE;
               env->sustain_stage = stage;
            }

            if (max_pos == 0)
            {
               env->max_stages = stage;
               max_pos = 1;
            }

            value = _MAX(nextval, 0.0f);
            nextval = (i < 2) ? filter->slot[i+1]->param[AAX_LEVEL0] : 0.0f;
            if (nextval == 0.0f) nextval = -5e-2f;
            env->step[stage] = (nextval - value)/max_pos;
            env->max_pos[stage] = max_pos;
            stage++;
         }
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
   }
   else
   {
      if (filter->slot[0]->data)
      {
         filter->slot[0]->destroy(filter->slot[0]->data);
         filter->slot[0]->data = NULL;
      }
   }
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewTimedGainFilterHandle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _filter_t* rv = _aaxFilterCreateHandle(info, type, _MAX_ENVELOPE_STAGES/2,0);

   if (rv)
   {
      _aaxEnvelopeData *env;
      unsigned int no_steps;
      float dt, value;
      int i, stages;

      _aax_dsp_copy(rv->slot[0], &p2d->filter[rv->pos]);
      rv->state = p2d->filter[rv->pos].state;

      i = 0;
      env = (_aaxEnvelopeData*)p2d->filter[rv->pos].data;
      if (env)
      {
         if (env->max_pos[1] > env->max_pos[0]) i = 1;
         dt = p2d->filter[rv->pos].param[2*i+1] / env->max_pos[i];

         no_steps = env->max_pos[1];
         value = p2d->filter[rv->pos].param[AAX_LEVEL1];
         value += env->step[1] * no_steps;

         stages = _MIN(1+env->max_stages/2, _MAX_ENVELOPE_STAGES/2);
         for (i=1; i<stages; i++)
         {
            _aaxFilterInfo* slot = rv->slot[i];

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

static float
_aaxTimedGainFilterSet(float val, int ptype, unsigned char param)
{
   float rv = val;

   if ((param % 2) == 0)
   {
      if (ptype == AAX_DECIBEL) {
         rv = _lin2db(val);
      }
   }
   else
   {
      if (ptype == AAX_SECONDS) {
         rv = val;
      } else if (ptype == AAX_MILLISECONDS) {
         rv = val*1e3f;
      } else if (ptype == AAX_MICROSECONDS) {
         rv = val*1e6f;
      }
   }
   return rv;
}

static float
_aaxTimedGainFilterGet(float val, int ptype, unsigned char param)
{
   float rv = val;
   if ((param % 2) == 0)
   {
      if (ptype == AAX_DECIBEL) {
         rv = _db2lin(val);
      }
   }
   else
   {
      if (ptype == AAX_SECONDS) {
         rv = val;
      } else if (ptype == AAX_MILLISECONDS) {
         rv = val*1e-3f;
      } else if (ptype == AAX_MICROSECONDS) {
         rv = val*1e-6f;
      }
   }
   return rv;
}

static float
_aaxTimedGainFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxTimedGainRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, FLT_MAX, 4.0f, FLT_MAX } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, FLT_MAX, 4.0f, FLT_MAX } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, FLT_MAX, 4.0f, FLT_MAX } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, FLT_MAX, 4.0f, FLT_MAX } }
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
   "AAX_timed_gain_filter_"AAX_MKSTR(VERSION), VERSION,
   (_aaxFilterCreate*)&_aaxTimedGainFilterCreate,
   (_aaxFilterDestroy*)&_aaxTimedGainFilterDestroy,
   (_aaxFilterReset*)&_env_reset,
   (_aaxFilterSetState*)&_aaxTimedGainFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewTimedGainFilterHandle,
   (_aaxFilterConvert*)&_aaxTimedGainFilterSet,
   (_aaxFilterConvert*)&_aaxTimedGainFilterGet,
   (_aaxFilterConvert*)&_aaxTimedGainFilterMinMax
};

