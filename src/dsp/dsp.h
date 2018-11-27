/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#ifndef _AAX_DSP_H
#define _AAX_DSP_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <ringbuffer.h>

// frequency filters
#define _aax_movingaverage_compute(fc, fs)	(1.f-expf(-GMATH_2PI*(fc)/(fs)))

#define _QfromBW(fc, BW)		((fc)/(BW))
#define _QfromF1F2(f1, f2)		_QfromBW(0.5f*((f1)+(f2)), (f2)-(f1))
void _aax_bessel_compute(float, void*);
void _aax_butterworth_compute(float, void*);

void _freqfilter_run(void*, MIX_PTR_T, CONST_MIX_PTR_T, size_t, size_t, size_t, unsigned int, void*, void*, unsigned char);
void _freqfilter_swap(void*, void*);
void _freqfilter_destroy(void*);

// equalizers
void _equalizer_run(void*, MIX_PTR_T, MIX_PTR_T, size_t, size_t, unsigned int, void*, void*);
void _grapheq_run(void*, MIX_PTR_T, MIX_PTR_T, MIX_PTR_T, size_t, size_t, unsigned int, _aaxRingBufferEqualizerData*);

// delay effects
void _delay_swap(void*, void*);
void _delay_destroy(void *);

// occlusion
_aaxRingBufferOcclusionData* _occlusion_create(_aaxRingBufferOcclusionData*, _aaxFilterInfo*, int, float);
void _occlusion_prepare(_aaxEmitter*, _aax3dProps*, void*);
void _occlusion_run(void*, MIX_PTR_T, CONST_MIX_PTR_T, MIX_PTR_T, size_t, unsigned int, const void*);
void _occlusion_destroy(void*);

#endif /* _AAX_DSP_H */

