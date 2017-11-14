/*
 * Copyright 2007-2017 by Erik Hofman.
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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif
#include <stdio.h>
#include <aax/aax.h>

#include <dsp/common.h>
#include <3rdparty/pffft.h>

#include <base/memory.h>

#include "analyze.h"
#include "arch.h"

#define N       8192
#define NMAX    (N/2)


float **
_aax_analyze_waveforms(void **data, unsigned int samples, float fs)
{
   float (*rv)[_AAX_SYNTH_MAX_HARMONICS];
   float *output, *tmp, *Re, *Im;
   float max, magnitude[NMAX];
   PFFFT_Setup *fft;
   int i, j, n, num;
   float *d;

   output = _aax_aligned_alloc(2*N*sizeof(float));
   tmp = _aax_aligned_alloc(2*N*sizeof(float));
   rv = malloc(sizeof(*rv) * _AAX_SYNTH_MAX_WAVEFORMS);

   if (!output || !tmp || !rv)
   {
      if (output) _aax_aligned_free(output);
      if (tmp) _aax_aligned_free(tmp);
      if (rv) free(rv);
      return NULL;
   }

   num = 2*samples/N;
   memset(output, 0, 2*N*sizeof(float));

   d = (float*)*data;
   fft = pffft_new_setup(N, PFFFT_COMPLEX);
   for (i=0; i<num; ++i)
   {
      pffft_transform(fft, d, tmp, 0, PFFFT_FORWARD);
      for (j=0; j<2*N; ++j) {
         output[j] += tmp[j];
      }
      d += N/2;
   }
   _aax_aligned_free(tmp);
   pffft_destroy_setup(fft);

   for (j=0; j<2*N; ++j) {
     output[j] /= num;
   }

   n = 1;
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
      float freq = _MAX(fs*i/N, 1e-9f);
      float q = magnitude[i]/max;		// normalized gain component
      if (q > LEVEL_96DB)
      {
         int harmonic;

         if (!f0) f0 = freq;		// set the base frequency
         harmonic = (int)(freq/f0);	// normalized frequency component

         if (harmonic <= _AAX_SYNTH_MAX_HARMONICS) {
            rv[0][harmonic-1] = q;
         }
         printf("% 6.0f Hz (harmonic: %i): %5.4f\n", freq, harmonic, q);
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
      printf("wave: %i, err: %f %%\n", i, err);
   }

   return (float**)rv;
}

