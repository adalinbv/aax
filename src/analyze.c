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

#include <base/memory.h>

#include "analyze.h"
#include "arch.h"

#define N       4096
#define NMAX    (N/2)


float **
_aax_analyze_waveforms(void **data, VOID(unsigned int dlen), float fs)
{
   float (*rv)[_AAX_SYNTH_MAX_HARMONICS];
   float *output, *Re, *Im;
   float max, magnitude[NMAX];
   PFFFT_Setup *fft;
   int i, n;

   output = _aax_aligned_alloc(2*N*sizeof(float));
   rv = malloc(sizeof(*rv) * _AAX_SYNTH_MAX_WAVEFORMS);

   if (!output || !rv)
   {
      if (output) _aax_aligned_free(output);
      if (rv) free(rv);
      return NULL;
   }

   fft = pffft_new_setup(N, PFFFT_COMPLEX);
   pffft_transform(fft, (float*)*data, output, 0, PFFFT_FORWARD);
   pffft_destroy_setup(fft);

   n = 0;
   max = 0.0f;
   Re = output;
   Im = output + N;
   for (i=0; i<NMAX; ++i)
   {
      float re = *Re++;
      float im = *Im++;
      magnitude[i] = sqrtf(re*re + im*im);
//    phase[i] = atan2(im, re);

      if (magnitude[i] > max) {
         max = magnitude[i]; n = i;
      }
   }
   _aax_aligned_free(output);

   float f0 = 0.0f;
   for (i=n; i<NMAX-1; i += n)
   {
      float q = magnitude[i]/max;		// normalized gain component
      if (q > LEVEL_96DB)
      {
         float freq = _MAX(fs*i/N, 1e-9f);
         int harmonic;

         if (!f0) f0 = freq;		// set the base frequency
         harmonic = (int)(freq/f0);	// normalized frequency component

         if (harmonic <= _AAX_SYNTH_MAX_HARMONICS) {
            rv[0][harmonic-1] = q;
//          printf("% 6.0f Hz (harmonic: %i): %5.4f\n", freq, harmonic, q);
         }
      }
   }

   for (i=0; i<MAX_WAVE; ++i)
   {
      float err = 0.0f;
      int harmonic;
      for (harmonic=0; harmonic<_AAX_SYNTH_MAX_HARMONICS; ++harmonic) {
         float h0 = _harmonics[i][harmonic];
         float h1 = rv[0][harmonic];
         if (h0) {
            err += h1/h0;
         } else if (h1) {
            err += 1.0f;
         }
      }
      err /= _AAX_SYNTH_MAX_HARMONICS;
//    printf("wave: %i, err: %f %%\n", i, err);
   }

   return (float**)rv;
}

