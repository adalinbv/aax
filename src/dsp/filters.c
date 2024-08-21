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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif

#include "common.h"
#include "filters.h"
#include "arch.h"
#include "dsp.h"

static int _flt_cvt_tbl[AAX_FILTER_MAX];
static int _cvt_flt_tbl[MAX_STEREO_FILTER];

aaxFilter
_aaxFilterCreateHandle(_aaxMixerInfo *info, enum aaxFilterType type, unsigned slots, size_t dsize)
{
   aaxFilter rv = NULL;
   unsigned int size;
   _filter_t* flt;

   size = sizeof(_filter_t) + slots*sizeof(_aaxFilterInfo);
   flt = calloc(1, size);
   if (flt)
   {
      unsigned s;
      char *ptr;

      flt->id = FILTER_ID;
      flt->state = false;
      flt->info = info;

      ptr = (char*)flt + sizeof(_filter_t);
      flt->slot[0] = (_aaxFilterInfo*)ptr;
      flt->pos = _flt_cvt_tbl[type];
      flt->type = type;

      size = sizeof(_aaxFilterInfo);
      for (s=0; s<slots; ++s) {
         flt->slot[s] = (_aaxFilterInfo*)(ptr + s*size);
      }
      flt->slot[0]->swap = _aax_dsp_swap;
      flt->slot[0]->destroy = _aax_dsp_destroy;

      flt->slot[0]->data_size = dsize;
      if (dsize)
      {
         flt->slot[0]->data = _aax_aligned_alloc(dsize);
         if (flt->slot[0]->data)
         {
            memset(flt->slot[0]->data, 0, dsize);
            flt->slot[0]->data_size = dsize;
         }
      }

      rv = (aaxFilter)flt;
   }
   return rv;
}

bool
_aaxFilterDestroy(aaxFilter* dsp)
{  
   _filter_t* flt = (_filter_t*)dsp;
   if (flt->slot[0]->data)
   {
      flt->slot[0]->destroy(flt->slot[0]->data);
      flt->slot[0]->data_size = 0;
      flt->slot[0]->data = NULL;
   }
   free(flt);

   return true;
}

_flt_function_tbl *_aaxFilters[AAX_FILTER_MAX] =
{ // order matches enum aaxFilterType
   &_aaxEqualizer,
   &_aaxVolumeFilter,
   &_aaxDynamicGainFilter,
   &_aaxTimedGainFilter,
   &_aaxDirectionalFilter,
   &_aaxDistanceFilter,
   &_aaxFrequencyFilter,
   &_aaxBitCrusherFilter,
   &_aaxGraphicEqualizer,
   &_aaxCompressor,
   &_aaxDynamicLayerFilter,
   &_aaxTimedLayerFilter
};

_filter_t*
new_filter_handle(const void *config, enum aaxFilterType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   _filter_t* rv = NULL;
   if (type > 0 && type <= AAX_FILTER_MAX)
   {
      _flt_function_tbl *flt = _aaxFilters[type-1];
      rv = flt->handle(config, type, p2d, p3d);
   }
   return rv;
}

_filter_t*
get_filter(aaxFilter f)
{
   _filter_t* rv = (_filter_t*)f;

   if (rv && rv->id == FILTER_ID) {
      return rv;
   }
   else if (rv && rv->id == FADEDBAD) {
      __aaxErrorSet(AAX_DESTROYED_HANDLE, __func__);
   }

   return NULL;
}

void
reset_filter(_aax2dProps* p2d, enum _aax2dFiltersEffects type)
{
   assert(type >= 0);
   assert(type < MAX_STEREO_FILTER);

   int pos = _cvt_flt_tbl[type];
   _flt_function_tbl *flt = _aaxFilters[pos-1];
   void *data = _FILTER_GET_DATA(p2d, type);
   if (flt->reset && data) flt->reset(data);
}

void
_aaxSetDefaultEqualizer(_aaxFilterInfo filter[MAX_FILTERS])
{
   int i;

   /* parametric equalizer */
   for (i=0; i<_MAX_PARAM_EQ; i++)
   {
      filter[i].param[AAX_CUTOFF_FREQUENCY] = MAXIMUM_CUTOFF;
      filter[i].param[AAX_LF_GAIN] = 1.0f;
      filter[i].param[AAX_HF_GAIN] = 1.0f;
      filter[i].param[AAX_RESONANCE] = 1.0f;
      filter[i].state = false;
   }

   /* Surround Crossover filter */
   filter[SURROUND_CROSSOVER_LP].param[AAX_CUTOFF_FREQUENCY] = 80.0f;
   filter[SURROUND_CROSSOVER_LP].param[AAX_LF_GAIN] = 1.0f;
   filter[SURROUND_CROSSOVER_LP].param[AAX_HF_GAIN] = 0.0f;
   filter[SURROUND_CROSSOVER_LP].param[AAX_RESONANCE] = 1.0f;
   filter[SURROUND_CROSSOVER_LP].state = false;
}

void
_aaxSetDefaultFilter2d(_aaxFilterInfo *filter, unsigned int type, UNUSED(unsigned slot))
{
   assert(type < MAX_STEREO_FILTER);
   assert(slot < _MAX_FE_SLOTS);

   filter->state = 0;
   filter->updated = 0;
   memset(filter->param, 0, sizeof(float[4]));
   switch(type)
   {
   case VOLUME_FILTER:
      if (slot == 0) {
         filter->param[AAX_GAIN] = 1.0f;
         filter->param[AAX_MAX_GAIN] = 1.0f;
         filter->state = true;
      }
      break;
   case FREQUENCY_FILTER:
      filter->param[AAX_CUTOFF_FREQUENCY] = MAXIMUM_CUTOFF;
      filter->param[AAX_LF_GAIN] = 1.0f;
      filter->param[AAX_HF_GAIN] = 1.0f;
      filter->param[AAX_RESONANCE] = 1.0f;
      break;
   default:
      break;
   }
}

void
_aaxSetDefaultFilter3d(_aaxFilterInfo *filter, unsigned int type, UNUSED(unsigned slot))
{
   assert(type < MAX_3D_FILTER);

   filter->state = 0;
   filter->updated = 0;
   memset(filter->param, 0, sizeof(float[4]));
   switch(type)
   {
   case DISTANCE_FILTER:
   {
      _aaxRingBufferDistanceData *data = filter->data;
      size_t dsize = sizeof(_aaxRingBufferDistanceData);
      float f = 5000.0f;	// Midband frequency in Hz

      if (!data) data = _aax_aligned_alloc(dsize);
      if (data)
      {
         filter->data = data;
         filter->data_size = dsize;

         memset(data, 0, dsize);
         data->run = _aaxDistanceFn[1];
         data->f2 = f*f;
         data->prev.pa_kPa = 101.325f;
         data->prev.T_K = 293.15f;
         data->prev.hr_pct = 60.0f;
      }

      filter->destroy = _distance_destroy;
      filter->swap = _distance_swap;

      filter->param[AAX_REF_DISTANCE] = 1.0f;
      filter->param[AAX_MAX_DISTANCE] = FLT_MAX;
      filter->param[AAX_ROLLOFF_FACTOR] = 1.0f;
      break;
   }
   case DIRECTIONAL_FILTER:
      filter->param[AAX_INNER_ANGLE] = 1.0f;
      filter->param[AAX_OUTER_ANGLE] = 1.0f;
      filter->param[AAX_OUTER_GAIN] = 1.0f;
      filter->param[AAX_FORWARD_GAIN] = 1.0f;
      filter->state = true;
      break;
   case OCCLUSION_FILTER:
      if (slot == 0) {
         filter->param[AAX_GAIN] = 1.0f;
         filter->param[AAX_MAX_GAIN] = 1.0f;
      }
      break;
   default:
      break;
   }
}

/* -------------------------------------------------------------------------- */

static int _flt_cvt_tbl[AAX_FILTER_MAX] =
{
  MAX_STEREO_FILTER,		// AAX_FILTER_NONE
  FREQUENCY_FILTER,		// AAX_EQUALIZER
  VOLUME_FILTER,		// AAX_VOLUME_FILTER
  DYNAMIC_GAIN_FILTER,		// AAX_DYNAMIC_GAIN_FILTER
  TIMED_GAIN_FILTER,		// AAX_TIMED_GAIN_FILTER
  DIRECTIONAL_FILTER,		// AAX_DIRECTIONAL_FILTER
  DISTANCE_FILTER,		// AAX_DISTANCE_FILTER
  FREQUENCY_FILTER,		// AAX_FREQUENCY_FILTER
  BITCRUSHER_FILTER,		// AAX_BITCRUSHER_FILTER
  FREQUENCY_FILTER,		// AAX_GRAPHIC_EQUALIZER
  DYNAMIC_GAIN_FILTER,		// AAX_COMPRESSOR
  DYNAMIC_LAYER_FILTER,		// AAX_DYNAMIC_LAYER_FILTER
  TIMED_LAYER_FILTER		// AAX_TIMED_LAYER_FILTER
};

static int _cvt_flt_tbl[MAX_STEREO_FILTER] =
{
  AAX_VOLUME_FILTER,		// VOLUME_FILTER
  AAX_DYNAMIC_GAIN_FILTER,	// DYNAMIC_GAIN_FILTER
  AAX_TIMED_GAIN_FILTER,	// TIMED_GAIN_FILTER
  AAX_FREQUENCY_FILTER,		// FREQUENCY_FILTER
  AAX_BITCRUSHER_FILTER,	// BITCRUSHER_FILTER
  AAX_DYNAMIC_LAYER_FILTER,	// DYNAMIC_LAYER_FILTER
  AAX_TIMED_LAYER_FILTER	// TIMED_LAYER_FILTER
};
