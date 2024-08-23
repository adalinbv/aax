/*
 * SPDX-FileCopyrightText: Copyright © 2005-2024 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2024 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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

