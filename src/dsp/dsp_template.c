/*
 * SPDX-FileCopyrightText: Copyright © 2024 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2024 by Adalin B.V.
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

#include "common.h"
#include "dsptypes.h"
#include "dsp.h"
#include "api.h"
#include "arch.h"


/*
 * 1. Replace DSPtype with Filter or Effect
 * 2. Replace dsptype with filter or effect
 * 3. Replace DSPname with the name of the filter or effect
 * 4. Replace dspname with the name of the filter or effect
 * 5. Replace NO_SLOTS by the number of filter or effects slots
 * 6. Replace _aaxRingBufferDSPnameData with the name of the data-structure
 * 7. Replace AAX_DSP_template with the appropriate filter or effect
 *    extension name.
 * 8. Adjust the table in _aax<DSP>MinMax for the minimum and maximum
 *    allowed values vor every parameter in every slot
 * 9. Implement the _aax<DSPname>SetState function
 */

#define VERSION 1.0
#define DSIZE   sizeof(_aaxRingBufferDSPnameData)

static int _dspname_run(MIX_PTR_T, size_t, size_t, void*, void*, unsigned int);
static void _dspname_swap(void*, void*);

static aaxDSPtype
_aaxDSPnameCreate(_aaxMixerInfo *info, enum aaxDSPtypeType type)
{
   size_t dsize = sizeof(_aaxRingBufferDSPnameData);
   _dsptype_t* eff = _aaxDSPtypeCreateHandle(info, type, 1, dsize);
   aaxDSPtype rv = NULL;

   if (eff)
   {
      _aaxSetDefaultDSPtype2d(eff->slot[0], eff->pos, 0);
      eff->slot[0]->swap = _dspname_swap;
      rv = (aaxDSPtype)eff;
   }
   return rv;
}

static int
_dspname_reset(void *data)
{
   _aaxRingBufferDSPnameData *dspname = data;
   if (dspname) {
      _lfo_reset(&dspname->lfo);
   }

   return true;
}

void
_dspname_swap(void *d, void *s)
{
   _aaxDSPtypeInfo *dst = d, *src = s;

   if (src->data && src->data_size)
   {
      if (!dst->data) {
          dst->data = _aaxAtomicPointerSwap(&src->data, dst->data);
          dst->data_size = src->data_size;
      }
      else
      {
         _aaxRingBufferDSPnameData *deff = dst->data;
         _aaxRingBufferDSPnameData *seff = src->data;

         assert(dst->data_size == src->data_size);

         _lfo_swap(&deff->lfo, &seff->lfo);
      }
   }
   dst->destroy = src->destroy;
   dst->swap = src->swap;
}

static _dsptype_t*
_aaxNewDSPnameHandle(const aaxConfig config, enum aaxDSPtypeType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _dsptype_t* rv = _aaxDSPtypeCreateHandle(info, type, 2, 0);

   if (rv)
   {
      _aax_dsp_copy(rv->slot[0], &p2d->dsptype[rv->pos]);
      rv->slot[0]->swap = _dspname_swap;
      rv->state = p2d->dsptype[rv->pos].state;
   }
   return rv;
}

static aaxDSPtype
_aaxDSPnameSetState(_dsptype_t* dsptype, UNUSED(int state))
{
   void *handle = dsptype->handle;
   aaxDSPtype rv = false;
   int wstate;

   assert(dsptype->info);

   if ((state & AAX_SOURCE_MASK) == 0) {
      state |= true;
   }
   
   dsptype->state = state;
   wstate = state & (AAX_SOURCE_MASK & ~AAX_PURE_WAVEFORM);
   switch (wstate)
   {
   case AAX_CONSTANT:
   case AAX_TRIANGLE:
   case AAX_SINE:
   case AAX_SQUARE:
   case AAX_IMPULSE:
   case AAX_SAWTOOTH:
   case AAX_CYCLOID:
   case AAX_RANDOMNESS:
   case AAX_RANDOM_SELECT:
   case AAX_ENVELOPE_FOLLOW:
   case AAX_TIMED_TRANSITION:
   {
      _aaxRingBufferDSPnameData *dspname = dsptype->slot[0]->data;
      if (dspname == NULL)
      {
         dspname = _aax_aligned_alloc(DSIZE);
         dsptype->slot[0]->data = dspname;
         if (dspname)
         {
            dsptype->slot[0]->data_size = DSIZE;
            memset(dspname, 0, DSIZE);
         }
      }

      if (dspname)
      {
         float min, max;
         int constant;

         dspname->run = _dspname_run;

         _lfo_setup(&dspname->lfo, dsptype->info, dsptype->state);
         if (wstate == AAX_CONSTANT)
         {
            min = dsptype->slot[0]->param[AAX_DC_OFFSET];
            max = 0.0f;
         }
         else
         {
            min = dsptype->slot[0]->param[AAX_LFO_MIN];
            max = dsptype->slot[0]->param[AAX_DC_OFFSET];
         }
         dspname->lfo.min_sec = min/dspname->lfo.fs;
         dspname->lfo.max_sec = max/dspname->lfo.fs;
         dspname->lfo.f = dsptype->slot[0]->param[AAX_LFO_FREQUENCY];

         constant = _lfo_set_timing(&dspname->lfo);
         if (!_lfo_set_function(&dspname->lfo, constant)) {
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
      if (dsptype->slot[0]->data)
      {
         dsptype->slot[0]->destroy(dsptype->slot[0]->data);
         dsptype->slot[0]->data_size = 0;
         dsptype->slot[0]->data = NULL;
      }
      break;
   }
   rv = dsptype;
   return rv;
}

static float
_aaxDSPnameSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;

   // convert rv from linear to ptype if necessary
   // - param is the parameter name as defined in enum aaxParameter
   // - ptype is the type name as defined in enum aaxType

   return rv;
}

static float
_aaxDSPnameGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;

   // convert rv from ptype to linear if necessary
   // - param is the parameter name as defined in enum aaxParameter
   // - ptype is the type name as defined in enum aaxType

   return rv;
}


static float
_aaxDSPnameMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxDSPnameRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxDSPnameRange[slot].min[param],
                       _aaxDSPnameRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxDSPname =
{
   "AAX_DSP_template", VERSION,
   (_aaxDSPtypeCreateFn*)&_aaxDSPnameCreate,
   (_aaxDSPtypeDestroyFn*)&_aaxDSPtypeDestroy,
   (_aaxDSPtypeResetFn*)&_dspname_reset,
   (_aaxDSPtypeSetStateFn*)&_aaxDSPnameSetState,
   NULL,
   (_aaxNewDSPtypeHandleFn*)&_aaxNewDSPnameHandle,
   (_aaxDSPtypeConvertFn*)&_aaxDSPnameSet,
   (_aaxDSPtypeConvertFn*)&_aaxDSPnameGet,
   (_aaxDSPtypeConvertFn*)&_aaxDSPnameMinMax
};

int
_dspname_run(MIX_PTR_T s, size_t end, size_t no_samples,
              void *data, void *env, unsigned int track)
{
   int rv = false;
   return rv;
}

