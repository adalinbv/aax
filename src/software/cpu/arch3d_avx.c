/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#include <api.h>

#include "arch3d_simd.h"

#ifdef __AVX__

FN_PREALIGN void
_mtx4dMul_avx(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2)
{
   __m256d row, col;
   int i, j;

   for (i=0; i<4; ++i) {
      row = _mm256_mul_pd(m1->s4x4[0].avx, _mm256_set1_pd(m2->m4[i][0]));
      for (j=1; j<4; ++j) {
          col = _mm256_set1_pd(m2->m4[i][j]);
          row = _mm256_add_pd(row, _mm256_mul_pd(m1->s4x4[j].avx, col));
      }
      d->s4x4[i].avx = row;
   }
}

FN_PREALIGN void
_mtx4dMulVec4_avx(vec4d_ptr d, const mtx4d_ptr m, const vec4d_ptr v)
{
   int i;

   d->s4.avx = _mm256_mul_pd(m->s4x4[0].avx, _mm256_set1_pd(v->v4[0]));
   for (i=1; i<4; ++i) {
      __m256d row = _mm256_mul_pd(m->s4x4[i].avx, _mm256_set1_pd(v->v4[i]));
      d->s4.avx = _mm256_add_pd(d->s4.avx, row);
   }
}


#else
typedef int make_iso_compilers_happy;
#endif /* AVX */

