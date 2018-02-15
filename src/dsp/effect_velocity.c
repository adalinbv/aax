/*
 * Copyright 2007-2018 by Erik Hofman.
 * Copyright 2009-2018 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <malloc.h>
#endif

#include <aax/aax.h>

#include <base/types.h>		/*  for rintf */
#include <base/gmath.h>

#include "effects.h"
#include "api.h"
#include "arch.h"


static aaxEffect
_aaxVelocityEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 1);
   aaxEffect rv = NULL;

   if (eff)
   {
      eff->slot[0]->data = *(void**)&_aaxDopplerFn[0];
      _aaxSetDefaultEffect3d(eff->slot[0], eff->pos, 0);
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxVelocityEffectDestroy(_effect_t* effect)
{
   free(effect);
   return AAX_TRUE;
}

static aaxEffect
_aaxVelocityEffectSetState(UNUSED(_effect_t* effect), UNUSED(int state))
{
   return  effect;
}

static _effect_t*
_aaxNewVelocityEffectHandle(const aaxConfig config, enum aaxEffectType type, UNUSED(_aax2dProps* p2d), _aax3dProps* p3d)
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 1);

   if (rv)
   {
      unsigned int size = sizeof(_aaxEffectInfo);

      memcpy(rv->slot[0], &p3d->effect[rv->pos], size);
      rv->slot[0]->data = *(void**)&_aaxDopplerFn[0];

      rv->state = p3d->effect[rv->pos].state;
   }
   return rv;
}

static float
_aaxVelocityEffectSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{  
   float rv = val;
   return rv;
}
   
static float
_aaxVelocityEffectGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{  
   float rv = val;
   return rv;
}

static float
_aaxVelocityEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxVelocityRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { MAXFLOAT, 10.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f,  0.0f, 0.0f, 0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxVelocityRange[slot].min[param],
                       _aaxVelocityRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxVelocityEffect =
{
   AAX_TRUE,
   "AAX_velocity_effect", 1.0f,
   (_aaxEffectCreate*)&_aaxVelocityEffectCreate,
   (_aaxEffectDestroy*)&_aaxVelocityEffectDestroy,
   (_aaxEffectSetState*)&_aaxVelocityEffectSetState,
   NULL,
   (_aaxNewEffectHandle*)&_aaxNewVelocityEffectHandle,
   (_aaxEffectConvert*)&_aaxVelocityEffectSet,
   (_aaxEffectConvert*)&_aaxVelocityEffectGet,
   (_aaxEffectConvert*)&_aaxVelocityEffectMinMax
};

static _aaxPitchShiftFn _aaxDopplerShift;

_aaxPitchShiftFn *_aaxDopplerFn[] =
{
   (_aaxPitchShiftFn *)&_aaxDopplerShift
};

/*
 * Sources:
 * http://en.wikipedia.org/wiki/Doppler_effect
 */
static float
_aaxDopplerShift(float ve, float vsound)
{
#if 1
   float vse, rv;

   /* relative speed */
   vse = _MIN(ve, vsound);
   rv =  vsound/_MAX(vsound - vse, 1.0f);

   return rv;
#else
   float ves;
   ves = _MAX(vsound - _MIN(ve, vsound), 1.0f);
   return vsound/ves;
#endif
}

