/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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

int _freqfilter_run(void*, MIX_PTR_T, CONST_MIX_PTR_T, size_t, size_t, size_t, unsigned int, void*, void*, float);
void _freqfilter_reset(void*);
void _freqfilter_data_swap( _aaxRingBufferFreqFilterData *dflt, _aaxRingBufferFreqFilterData *sflt);
void _freqfilter_destroy(void*);

// equalizers
void _equalizer_swap(void*, void*);
int _equalizer_run(void*, MIX_PTR_T, MIX_PTR_T, size_t, size_t, unsigned int, void*, void*, void*);
int _grapheq_run(void*, MIX_PTR_T, MIX_PTR_T, MIX_PTR_T, size_t, size_t, unsigned int, _aaxRingBufferEqualizerData*);

// delay effects
#define CHORUS_MIN		 10e-3f
#define CHORUS_MAX		DELAY_EFFECTS_TIME
#define CHORUS_DEPTH		(CHORUS_MAX-CHORUS_MIN)
#define CHORUS_MAX_ORG		 60e-3f
#define CHORUS_NORM_FACT	(CHORUS_MAX/CHORUS_MAX_ORG)

#define PHASING_MIN		50e-6f
#define PHASING_MAX		10e-3f
#define PHASING_DEPTH		(PHASING_MAX-PHASING_MIN)
#define PHASING_NORM_FACT	(CHORUS_MAX/PHASING_MAX)

#define DELAY_MIN		 60e-3f
#define DELAY_MAX		DELAY_LINE_EFFECTS_TIME
#define DELAY_DEPTH		(DELAY_MAX-DELAY_MIN)
#define DELAY_MAX_ORG		200e-3f
#define DELAY_NORM_FACT		(DELAY_MAX/DELAY_MAX_ORG)

// occlusion
_aaxRingBufferOcclusionData* _occlusion_create(_aaxRingBufferOcclusionData*, _aaxFilterInfo*, int, float);
void _occlusion_prepare(_aaxEmitter*, const _aax3dProps*, void*);
int _occlusion_run(void*, MIX_PTR_T, CONST_MIX_PTR_T, MIX_PTR_T, size_t, unsigned int, const void*);
void _occlusion_destroy(void*);
void _occlusion_to_effect(_aaxEffectInfo*, _aaxRingBufferOcclusionData*);

#endif /* _AAX_DSP_H */

