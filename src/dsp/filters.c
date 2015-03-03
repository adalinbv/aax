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
# include <string.h>
#endif

#include "common.h"
#include "filters.h"

void
_aaxSetDefaultEqualizer(_aaxFilterInfo filter[2])
{
   int i;
   for (i=0; i<2; i++)
   {
      filter[i].param[AAX_CUTOFF_FREQUENCY] = 22050.0f;
      filter[i].param[AAX_LF_GAIN] = 1.0f;
      filter[i].param[AAX_HF_GAIN] = 1.0f;
      filter[i].param[AAX_RESONANCE] = 1.0f;
      filter[i].state = AAX_FALSE;
   }
}

void
_aaxSetDefaultFilter2d(_aaxFilterInfo *filter, unsigned int type)
{
   assert(type < MAX_STEREO_FILTER);

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
      break;
   case ANGULAR_FILTER:
      filter->param[AAX_INNER_ANGLE] = 1.0f;
      filter->param[AAX_OUTER_ANGLE] = 1.0f;
      filter->param[AAX_OUTER_GAIN] = 1.0f;
      filter->state = AAX_TRUE;
      break;
   default:
      break;
   }
}

