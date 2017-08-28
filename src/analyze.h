/*
 * Copyright 2007-2017 by Erik Hofman.
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

#ifndef AAX_SYNTHESIZE_H
#define AAX_SYNTHESIZE_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#define _AAX_SYNTH_MAX_WAVEFORMS	4
#define _AAX_SYNTH_MAX_HARMONICS	16

enum
{
   _SQUARE_WAVE = 0,
   _TRIANGLE_WAVE,
   _SAWTOOTH_WAVE,
   _IMPULSE_WAVE,
   _SINE_WAVE,

   MAX_WAVE
};

float _harmonics[MAX_WAVE][_AAX_SYNTH_MAX_HARMONICS];

float **_aax_analyze_waveforms(void**, unsigned int, float);
float **_aax_analyze_envelopes(void**, unsigned int, float);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif

