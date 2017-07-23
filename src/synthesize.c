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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif

#include <dsp/common.h>

#include <3rdparty/pffft.h>

#include "synthesize.h"
#include "arch.h"

#define N       4096
#define NMAX    (N/2)

// WARNING: not thread-safe
float **
_aax_get_frequencies(void **data, unsigned int dlen, float fs)
{
   static float rv[_AAX_SYNTH_MAX_WAVEFORMS][_AAX_SYNTH_MAX_HARMONICS];
   PFFFT_Setup *fft;
   float *output, *ptr;
   float *power;
   int i;

   output = _aax_aligned_alloc(2*N*sizeof(float));

   fft = pffft_new_setup(N, PFFFT_COMPLEX);
   pffft_transform_ordered(fft, (float*)*data, output, 0, PFFFT_FORWARD);
   pffft_destroy_setup(fft);

   ptr = output;
   power = _aax_aligned_alloc(NMAX*sizeof(float));
   for (i=0; i<NMAX; ++i)
   {
      float Re = *ptr++;
      float Im = *ptr++;
      power[i] = Re*Re + Im*Im;
   }
   _aax_aligned_free(output);

   int n = 0;
   float max = 0;
   for (i=1; i<NMAX-1; ++i)
   {
      if (power[i] > max) {
         max = power[i]; n = i;
      }
   }

   float f0 = 0.0f;
   float fs2 = 0.5f * fs;
   for (i=n; i<NMAX-1; i += n)
   {
      float q = power[i]/max;		// normalized gain component
      if (q > LEVEL_96DB)
      {
         float freq = _MAX(fs2*i/N, 1e-9f);
         int harmonic;

         if (!f0) f0 = freq;		// set the base frequency
         harmonic = (int)(freq/f0);	// normalized frequency component

         if (harmonic <= _AAX_SYNTH_MAX_HARMONICS) {
            rv[0][harmonic-1] = q;
            printf("% 6.0f Hz (harmonic: %i): %f\n", freq, harmonic, q);
         }
      }
   }

   _aax_aligned_free(power);

   return (float**)rv;
}
