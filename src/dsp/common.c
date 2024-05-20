/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <malloc.h>
# include <string.h>
#endif

#include <base/types.h>
#include <base/geometry.h>

#include "api.h"
#include "arch.h"
#include "common.h"

void _aax_dsp_destroy(void *ptr)
{
    if (ptr) _aax_aligned_free(ptr);
}

void _aax_dsp_copy(void *d, void *s)
{
   _aaxFilterInfo *dst = d, *src = s;
   dst->state = src->state;
   dst->updated = src->updated;
   memcpy(dst->param, src->param, sizeof(float[4]));
}

void _aax_dsp_swap(void *d, void *s) {
   _aaxFilterInfo *dst = d, *src = s;
   if (src->data && src->data_size)
   {
      if (!dst->data)
      {
          dst->data = _aaxAtomicPointerSwap(&src->data, dst->data);
          dst->data_size = src->data_size;
      }
      else
      {
         assert(dst->data_size == src->data_size);
         memcpy(dst->data, src->data, src->data_size);
      }
   }
   dst->destroy = src->destroy;
   dst->swap = src->swap;
}

float _lin(float v) { return v; }
float _ln(float v) { return powf(v, GMATH_1_E1); }
float _exp(float v) { return powf(v, GMATH_E1); }
float _log(float v) { return powf(v, 1.0f/10.0f); }
float _pow(float v) { return powf(v, 10.0f); }
float _square(float v) { return v*v; }
float _lin2log(float v) { return log10f(_MAX(v, 1e-9f)); }
float _log2lin(float v) { return powf(10.0f,v); }
float _lin2db(float v) { return 20.0f*log10f(_MAX(v, 1e-9f)); }
float _db2lin(float v) { return _MINMAX(powf(10.0f,v/20.0f),0.0f,10.0f); }
float _rad2deg(float v) { return v*GMATH_RAD_TO_DEG; }
float _deg2rad(float v) { return fmodf(v, 360.0f)*GMATH_DEG_TO_RAD; }
float _cos_deg2rad_2(float v) { return cosf(_deg2rad(v)/2); }
float _2acos_rad2deg(float v) { return 2*_rad2deg(acosf(v)); }
float _cos_2(float v) { return cosf(v/2); }
float _2acos(float v) { return 2*acosf(v); }
float _degC2K(float v) { return 273.15f+v; }
float _K2degC(float v) { return v-273.15f; }
float _degF2K(float v) { return (v+459.67f)*5.0f/9.0f; }
float _K2degF(float v) { return v*9.0f/5.0f - 459.67f; }
float _atm2kpa(float v) { return v*0.0098692327f; }
float _kpa2atm(float v) { return v*101.325f; }
float _bar2kpa(float v) { return v*0.01f; }
float _kpa2bar(float v) { return v*100.0f; }
float _psi2kpa(float v) { return v*0.1450377377f; }
float _kpa2psi(float v) { return v*6.8947572932f; }

int _freq2note(float v) { return rintf(12.0f*(logf(v/220.0f)/log(2)))+57; }
float _note2freq(int n) { return 440.0f*powf(2.0f, ((float)n-69.0f)/12.0f); }

float reverb_time_to_decay_level(float reverb_time)
{
   float refresh_rate = _info->refresh_rate;
   return powf(10.0f, LOG_60DB/(reverb_time*refresh_rate));
}

float decay_level_to_reverb_time(float decay_level)
{
   float refresh_rate = _info->refresh_rate;
   return LOG_60DB/(refresh_rate*log10f(_MIN(decay_level, 0.998f)));
}

char* _note2name(int n)
{
   static const char *notes[] =
    { "A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#" };
   static char rv[16];
   snprintf(rv, 16, "%s%i", notes[(n+3) % 12], n/12-2);
   return rv;
}

inline FLOAT _lorentz(FLOAT v2, FLOAT c2) { return sqrt(1.0 - (v2/c2)) - 1.0f; }
