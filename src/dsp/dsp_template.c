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

#include "common.h"
#include "api.h"
#include "arch.h"


/*
 * 1. Replace aaxDSP with aaxFilter or aaxEffect
 * 2. Replace DSP with the name of the dsp or effect
 * 3. Adjust NO_SLOTS if necessary
 * 4. Adjust DATA_TYPE to the name of the data-structure
 * 5. Implement the _aax<DSP>SetState function
 * 6. Adjust the table in _aax<DSP>MinMax for the minimum and maximum
 *    allowed values vor every parameter in every slot
 * 7. Replace AAX_DSP_template with the appropriate dsp or effect name
 */

#define NO_SLOTS	1
#define DATA_TYPE	void

static aaxDSP
_aaxDSPCreate(_aaxMixerInfo *info, enum aaxDSPType type)
{
   size_t dsize = sizeof(DATA_TYPE);
   _dsp_t* dsp = _aaxDSPCreateHandle(info, type, NO_SLOTS, dsize);
   aaxDSP rv = NULL;

   if (eff)
   {
      int i;

      assert(NO_SLOTS < _MAX_FE_SLOTS);
      for (i=0; i<NO_SLOTS; ++i) {
         _aaxSetDefaultDSP2d(eff->slot[i], eff->pos, i);
      }
      dsp->slot[0]->destroy = destroy;
      rv = (aaxDSP)dsp;
   }
   return rv;
}

static _dsp_t*
_aaxNewDSPHandle(const aaxConfig config, enum aaxDSPType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   size_t dsize = sizeof(DATA_TYPE);
   _dsp_t* eff = _aaxDSPCreateHandle(info, type, NO_SLOTS, dsize);

   if (rv)
   {
      unsigned int i, size = sizeof(_aaxDSPInfo);

      for (i=0; i<NO_SLOTS; ++i)
      {
         memcpy(rv->slot[i], &p2d->dsp[rv->pos], size);
         rv->slot[i]->data = NULL;
      }
      rv->slot[0]->destroy = destroy;

      rv->state = p2d->dsp[rv->pos].state;
   }
   return rv;
}

// Note: If your Destroy function looks exactly like the following code then
// it's better to reference aaxFilteDestroy or aaxEffectDestroy in the
// _dsp_function_tbl function table below.
static int
_aaxDSPDestroy(_dsp_t* dsp)
{
   if (dsp->slot[0]->data)
   {
      dsp->slot[0]->destroy(dsp->slot[0]->data);
      dsp->slot[0]->data_size = 0;
      dsp->slot[0]->data = NULL;
   }
   free(dsp);

   return true;
}

static aaxDSP
_aaxDSPSetState(_dsp_t* dsp, UNUSED(int state))
{
   if (state) {
      dsp->slot[0]->data = dsp->data
   }

   // Initialize the data structure based on the value of state

   return dsp;
}

static float
_aaxDSPSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;

   // convert rv from linear to ptype if necessary
   // - param is the parameter name as defined in enum aaxParameter
   // - ptype is the type name as defined in enum aaxType

   return rv;
}

static float
_aaxDSPGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;

   // convert rv from ptype to linear if necessary
   // - param is the parameter name as defined in enum aaxParameter
   // - ptype is the type name as defined in enum aaxType

   return rv;
}


static float
_aaxDSPMinMax(float val, int slot, unsigned char param)
{
   static const _dsp_minmax_tbl_t _aaxDSPRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxDSPRange[slot].min[param],
                       _aaxDSPRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_dsp_function_tbl _aaxDSP =
{
   true,
   "AAX_DSP_template", 1.0f,
   (_aaxDSPCreate*)&_aaxDSPCreate,
   (_aaxDSPDestroy*)&_aaxDSPDestroy,
   (_aaxDSPSetState*)&_aaxDSPSetState,
   NULL,
   (_aaxNewDSPHandle*)&_aaxNewDSPHandle,
   (_aaxDSPConvert*)&_aaxDSPSet,
   (_aaxDSPConvert*)&_aaxDSPGet,
   (_aaxDSPConvert*)&_aaxDSPMinMax
};

