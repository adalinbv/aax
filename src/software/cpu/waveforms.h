/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef WAVEFORMS_H
#define WAVEFORMS_H 1

#include <ringbuffer.h>

// must be a multiple of 4 because of SIMD optimizations
#define MAX_HARMONICS           _AAX_SYNTH_MAX_HARMONICS

extern float ALIGN _harmonic_phases[AAX_MAX_WAVE][2*MAX_HARMONICS];
extern float ALIGN _harmonics[AAX_MAX_WAVE][2*MAX_HARMONICS];

#endif /* WAVEFORMS_H */
