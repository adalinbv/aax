/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <api.h>

#include "arch3d_simd.h"

#ifdef __AVX__


#if 0
FN_PREALIGN void
_vec4dMatrix4_avx(vec4d_ptr d, const vec4d_ptr vi, const mtx4d_ptr m)
{
   int i;

   d->s4 = _mm256_mul_pd(m->s4x4[0], _mm256_set1_pd(vi->v4[0]));
   for (i=1; i<3; ++i) {
      __m256d row = _mm256_mul_pd(m->s4x4[i], _mm256_set1_pd(vi->v4[i]));
      d->s4 = _mm256_add_pd(d->s4, row);
   }
}

FN_PREALIGN void
_pt4dMatrix4_avx(vec4d_ptr d, const vec4d_ptr vi, const mtx4d_ptr m)
{
   int i;

   d->s4 = _mm256_mul_pd(m->s4x4[0], _mm256_set1_pd(vi->v4[0]));
   for (i=1; i<3; ++i) {
      __m256d row = _mm256_mul_pd(m->s4x4[i], _mm256_set1_pd(vi->v4[i]));
      d->s4 = _mm256_add_pd(d->s4, row);
   }
   d->s4 = _mm256_add_pd(d->s4, m->s4x4[3]);
}

FN_PREALIGN void
_mtx4dMul_avx(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2)
{
   int i;

   for (i=0; i<4; ++i) {
      __m256d row = _mm256_mul_pd(m1->s4x4[0], _mm256_set1_pd(m2->m4[i][0]));
      for (int j=1; j<4; ++j) {
          __m256d col = _mm256_set1_pd(m2->m4[i][j]);
          row = _mm256_add_pd(row, _mm256_mul_pd(m1->s4x4[j], col));
      }
      d->s4x4[i] = row;
   }
}
#endif

#else
typedef int make_iso_compilers_happy;
#endif /* AVX */

