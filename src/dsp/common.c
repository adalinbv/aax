/*
 * Copyright 2005-2023 by Erik Hofman.
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
#include <base/threads.h>

#include "arch.h"
#include "common.h"

void _aax_dsp_destroy(void *ptr) { if (ptr) _aax_aligned_free(ptr); }
void _aax_dsp_aligned_destroy(void *ptr) { if (ptr) _aax_aligned_free(ptr); }
void _aax_dsp_copy(void *d, void *s) {
   _aaxFilterInfo *dst = d, *src = s;
   dst->state = src->state;
   dst->updated = src->updated;
   memcpy(dst->param, src->param, sizeof(float[4]));
}
void _aax_dsp_swap(void *d, void *s) {
   _aaxFilterInfo *dst = d, *src = s;
   if (src->data && src->data_size) {
      if (!dst->data) {
          dst->data = _aaxAtomicPointerSwap(&src->data, dst->data);
          dst->data_size = src->data_size;
      } else {
         assert(dst->data_size == src->data_size);
         memcpy(dst->data, src->data, src->data_size);
      }
   }
   dst->destroy = src->destroy;
   dst->swap = src->swap;
}

inline float _lin(float v) { return v; }
inline float _square(float v) { return v*v; }
inline float _lin2log(float v) { return log10f(_MAX(v, 1e-9f)); }
inline float _log2lin(float v) { return powf(10.0f,v); }
inline float _lin2db(float v) { return 20.0f*log10f(_MAX(v, 1e-9f)); }
inline float _db2lin(float v) { return _MINMAX(powf(10.0f,v/20.0f),0.0f,10.0f); }
inline float _rad2deg(float v) { return v*GMATH_RAD_TO_DEG; }
inline float _deg2rad(float v) { return fmodf(v, 360.0f)*GMATH_DEG_TO_RAD; }
inline float _cos_deg2rad_2(float v) { return cosf(_deg2rad(v)/2); }
inline float _2acos_rad2deg(float v) { return 2*_rad2deg(acosf(v)); }
inline float _cos_2(float v) { return cosf(v/2); }
inline float _2acos(float v) { return 2*acosf(v); }
inline float _degC2K(float v) { return 273.15f+v; }
inline float _K2degC(float v) { return v-273.15f; }
inline float _degF2K(float v) { return (v+459.67f)*5.0f/9.0f; }
inline float _K2degF(float v) { return v*9.0f/5.0f - 459.67f; }
inline float _atm2kpa(float v) { return v*0.0098692327f; }
inline float _kpa2atm(float v) { return v*101.325f; }
inline float _bar2kpa(float v) { return v*0.01f; }
inline float _kpa2bar(float v) { return v*100.0f; }
inline float _psi2kpa(float v) { return v*0.1450377377f; }
inline float _kpa2psi(float v) { return v*6.8947572932f; }

inline int _freq2note(float v) { return rintf(12*(logf(v/220.0f)/log(2))+57); }
inline float _note2freq(int n) { return 440.0f*powf(2.0f, ((float)n-69.0f)/12.0f); }

char* _note2name(int n)
{
   static const char *notes[] =
    { "A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#" };
   static char rv[16];
   snprintf(rv, 16, "%s%i", notes[(n+3) % 12], n/12-2);
   return rv;
}

inline FLOAT _lorentz(FLOAT v2, FLOAT c2) { return sqrt(1.0 - (v2/c2)) - 1.0f; }
