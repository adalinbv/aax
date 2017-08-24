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
# include <string.h>
#endif

#include "common.h"
#include "filters.h"

_flt_function_tbl *_aaxFilters[AAX_FILTER_MAX] =
{
   &_aaxEqualizer,
   &_aaxVolumeFilter,
   &_aaxDynamicGainFilter,
   &_aaxTimedGainFilter,
   &_aaxAngularFilter,
   &_aaxDistanceFilter,
   &_aaxFrequencyFilter,
   &_aaxGraphicEqualizer,
   &_aaxCompressor
};

_filter_t*
new_filter_handle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   _filter_t* rv = NULL;
   if (type <= AAX_FILTER_MAX)
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
_aaxSetDefaultEqualizer(_aaxFilterInfo filter[EQUALIZER_MAX])
{
   int i;
 
   /* parametric equalizer */
   for (i=0; i<2; i++)
   {
      filter[i].param[AAX_CUTOFF_FREQUENCY] = 22050.0f;
      filter[i].param[AAX_LF_GAIN] = 1.0f;
      filter[i].param[AAX_HF_GAIN] = 1.0f;
      filter[i].param[AAX_RESONANCE] = 1.0f;
      filter[i].state = AAX_FALSE;
   }

   /* Surround Crossover filter */
   filter[SURROUND_CROSSOVER_LP].param[AAX_CUTOFF_FREQUENCY] = 80.0f;
   filter[SURROUND_CROSSOVER_LP].param[AAX_LF_GAIN] = 1.0f;
   filter[SURROUND_CROSSOVER_LP].param[AAX_HF_GAIN] = 0.0f;
   filter[SURROUND_CROSSOVER_LP].param[AAX_RESONANCE] = 1.0f;
   filter[SURROUND_CROSSOVER_LP].state = AAX_FALSE;
}

void
_aaxSetDefaultFilter2d(_aaxFilterInfo *filter, unsigned int type)
{
   assert(type < MAX_STEREO_FILTER);

   free(filter->data);
   memset(filter, 0, sizeof(_aaxFilterInfo));
   switch(type)
   {
   case VOLUME_FILTER:
      filter->param[AAX_GAIN] = 1.0f;
      filter->param[AAX_MAX_GAIN] = 1.0f;
      filter->state = AAX_TRUE;
      break;
   case FREQUENCY_FILTER:
      filter->param[AAX_CUTOFF_FREQUENCY] = 22050.0f;
      filter->param[AAX_LF_GAIN] = 1.0f;
      filter->param[AAX_HF_GAIN] = 1.0f;
      filter->param[AAX_RESONANCE] = 1.0f;
      break;
   default:
      break;
   }
}

void
_aaxSetDefaultFilter3d(_aaxFilterInfo *filter, unsigned int type)
{
   assert(type < MAX_3D_FILTER);

   memset(filter, 0, sizeof(_aaxFilterInfo));
   switch(type)
   {
   case DISTANCE_FILTER:
      filter->param[AAX_REF_DISTANCE] = 1.0f;
      filter->param[AAX_MAX_DISTANCE] = MAXFLOAT;
      filter->param[AAX_ROLLOFF_FACTOR] = 1.0f;
      filter->state = AAX_EXPONENTIAL_DISTANCE;
      filter->data = *(void**)&_aaxRingBufferDistanceFn[1];
      break;
   case ANGULAR_FILTER:
      filter->param[AAX_INNER_ANGLE] = 1.0f;
      filter->param[AAX_OUTER_ANGLE] = 1.0f;
      filter->param[AAX_OUTER_GAIN] = 1.0f;
      filter->param[AAX_FORWARD_GAIN] = 1.0f;
      filter->state = AAX_TRUE;
      break;
   default:
      break;
   }
}

/* -------------------------------------------------------------------------- */

const _flt_cvt_tbl_t _flt_cvt_tbl[AAX_FILTER_MAX] =
{
  { AAX_FILTER_NONE,            MAX_STEREO_FILTER },
  { AAX_EQUALIZER,              FREQUENCY_FILTER },
  { AAX_VOLUME_FILTER,          VOLUME_FILTER },
  { AAX_DYNAMIC_GAIN_FILTER,    DYNAMIC_GAIN_FILTER },
  { AAX_TIMED_GAIN_FILTER,      TIMED_GAIN_FILTER },
  { AAX_ANGULAR_FILTER,         ANGULAR_FILTER },
  { AAX_DISTANCE_FILTER,        DISTANCE_FILTER },
  { AAX_FREQUENCY_FILTER,       FREQUENCY_FILTER },
  { AAX_GRAPHIC_EQUALIZER,      FREQUENCY_FILTER },
  { AAX_COMPRESSOR,             DYNAMIC_GAIN_FILTER }
};

