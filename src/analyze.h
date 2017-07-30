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

