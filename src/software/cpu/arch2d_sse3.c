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

#include <software/rbuf_int.h>
#include "arch2d_simd.h"

#ifdef __SSE3__

void
_batch_imul_value_sse3(void* dptr, const void* sptr, unsigned bps, size_t num, float f)
{
   size_t i = num;

   if (num)
   {
      switch (bps)
      {
      case 1:
      {
         int8_t* s = (int8_t*)sptr;
         int8_t* d = (int8_t*)dptr;
         do {
            *d++ = *s++ * f;
         }
         while (--i);
         break;
      }
      case 2:
      {
         int16_t* s = (int16_t*)sptr;
         int16_t* d = (int16_t*)dptr;
         do {
            *d++ = *s++ * f;
         }
         while (--i);
         break;
         }
      case 4:
      {
         int32_t* s = (int32_t*)sptr;
         int32_t* d = (int32_t*)dptr;
         do {
            *d++ = *s++ * f;
         }
         while (--i);
         break;
      }
      default:
         break;
      }
   }
}

#else
typedef int make_iso_compilers_happy;
#endif /* SSE3 */

