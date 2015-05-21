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

/* see above for the proper sequence */
const _flt_minmax_tbl_t _flt_minmax_tbl[_MAX_FE_SLOTS][AAX_FILTER_MAX] =
{   /* min[4] */          /* max[4] */
  {
    /* AAX_FILTER_NONE      */
    { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,  0.0f,     0.0f } },
    /* AAX_EQUALIZER        */
    { { 20.0f,  0.0f, 0.0f, 1.0f }, { 22050.0f,    10.0f, 10.0f,   100.0f } },
    /* AAX_VOLUME_FILTER    */
    { {  0.0f,  0.0f, 0.0f, 0.0f }, {    10.0f,     1.0f, 10.0f,    10.0f } },
    /* AAX_DYNAMIC_GAIN_FILTER   */
    { { 0.0f,  0.01f, 0.0f, 0.0f }, {     0.0f,    50.0f,  1.0f,     1.0f } },
    /* AAX_TIMED_GAIN_FILTER */
    { {  0.0f,  0.0f, 0.0f, 0.0f }, {     4.0f, MAXFLOAT,  4.0f, MAXFLOAT } },
    /* AAX_ANGULAR_FILTER   */
    { { -1.0f, -1.0f, 0.0f, 0.0f }, {     1.0f,     1.0f,  1.0f,     0.0f } },
    /* AAX_DISTANCE_FILTER  */
    { {  0.0f,  0.1f, 0.0f, 0.0f }, { MAXFLOAT, MAXFLOAT,  1.0f,     0.0f } },
    /* AAX_FREQUENCY_FILTER */
    { { 20.0f,  0.0f, 0.0f, 1.0f }, { 22050.0f,    10.0f, 10.0f,   100.0f } },
    /* AAX_GRAPHIC_EQUALIZER */
    { {  0.0f,  0.0f, 0.0f, 0.0f }, {    2.0f,      2.0f,  2.0f,     2.0f } },
    /* AAX_COMPRESSOR        */
    { { 1e-3f, 1e-3f, 0.0f, 0.0f }, {   0.25f,     10.0f,  1.0f,     1.0f } },
  },
  {
     /* AAX_FILTER_NONE      */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_EQUALIZER        */
     { { 20.0f,  0.0f, 0.0f, 1.0f }, { 22050.0f,    10.0f,    10.0f, 100.0f } },
     /* AAX_VOLUME_FILTER    */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_DYNAMIC_GAIN_FILTER   */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_TIMED_GAIN_FILTER */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     4.0f, MAXFLOAT,   4.0f, MAXFLOAT } },
     /* AAX_ANGULAR_FILTER   */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_DISTANCE_FILTER  */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_FREQUENCY_FILTER */
     { { 20.0f, 0.0f, 0.0f, 0.01f }, { 22050.0f,     1.0f,     1.0f,  50.0f } },
     /* AAX_GRAPHIC_EQUALIZER */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {    2.0f,      2.0f,     2.0f,   2.0f } },
     /* AAX_COMPRESSOR        */
     { {  0.0f, 1e-3f, 0.0f, 0.0f }, {    0.0f,     10.0f,     0.0f,   1.0f } },
  },
  {
     /* AAX_FILTER_NONE      */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_EQUALIZER        */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_VOLUME_FILTER    */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_DYNAMIC_GAIN_FILTER   */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_TIMED_GAIN_FILTER */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     4.0f, MAXFLOAT,   4.0f, MAXFLOAT } },
     /* AAX_ANGULAR_FILTER   */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_DISTANCE_FILTER  */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_FREQUENCY_FILTER */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_GRAPHIC_EQUALIZER */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_COMPRESSOR        */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } }
  }
};

cvtfn_t
filter_get_cvtfn(enum aaxFilterType type, int ptype, int mode, char param)
{
   cvtfn_t rv = _lin;
   switch (type)
   {
   case AAX_TIMED_GAIN_FILTER:
   case AAX_VOLUME_FILTER:
      if (ptype == AAX_LOGARITHMIC)
      {
         if (mode == WRITEFN) {
            rv = _lin2db;
         } else {
            rv = _db2lin;
         }
      }
      break;
   case AAX_FREQUENCY_FILTER:
      if (param > 0)
      {
         if (ptype == AAX_LOGARITHMIC)
         {
            if (mode == WRITEFN) {
               rv = _lin2db;
            } else {
               rv = _db2lin;
            }
         }
      }
      break;
   case AAX_ANGULAR_FILTER:
      if (param < 2)
      {
         if (ptype == AAX_DEGREES)
         {
            if (mode == WRITEFN) {
               rv = _cos_deg2rad_2;
            } else {
               rv = _2acos_rad2deg;
            }
         }
         else
         {
            if (mode == WRITEFN) {
               rv = _cos_2;
            } else {
               rv = _2acos;
            }
         }
      }
      break;
   default:
      break;
   }
   return rv;
}

