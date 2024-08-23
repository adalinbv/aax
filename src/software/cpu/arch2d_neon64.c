/*
 * SPDX-FileCopyrightText: Copyright © 2005-2024 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2024 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */
#if defined __aarch64__

#include "software/rbuf_int.h"
#include "arch2d_simd.h"

void
_aax_init_NEON64()
{
   const char *env = getenv("AAX_ENABLE_FTZ");

   if (env && _aax_getbool(env))
   {
      uint64_t fpcr;
      // Load the FPCR register
      ASM( "mrs %0,   fpcr" : "=r"( fpcr ));
      // Set the 24th bit (FTZ) to 1
      ASM( "msr fpcr, %0"   :: "r"( fpcr | (1 << 24) ));
   }
}

#define A neon64
#include "arch2d_neon_template.c"

#endif /* NEON */

