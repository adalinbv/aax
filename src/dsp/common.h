/*
 * Copyright 2005-2015 by Erik Hofman.
 * Copyright 2009-2015 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_FE_COMMON_H
#define _AAX_FE_COMMON_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <driver.h>

#include "filters.h"
#include "effects.h"

#define DELAY_EFFECTS_TIME      0.070f
#define REVERB_EFFECTS_TIME     0.700f

#define _AAX_MAX_FILTERS        2
#if 0
#define NO_DELAY_EFFECTS_TIME
#undef DELAY_EFFECTS_TIME
#define DELAY_EFFECTS_TIME      0.0f
#endif

enum _aax3dFiltersEffects
{
    /* 3d filters */
    DISTANCE_FILTER = 0,        /* distance attennuation */
    ANGULAR_FILTER,             /* audio cone support    */
    MAX_3D_FILTER,

    /* 3d effects */
    VELOCITY_EFFECT = 0,        /* Doppler               */
    MAX_3D_EFFECT,
};

enum _aax2dFiltersEffects
{
    /* final mixer stage */
    EQUALIZER_LF = 0,
    EQUALIZER_HF,
    EQUALIZER_MAX,

    /* stereo filters */
    VOLUME_FILTER = 0,
    DYNAMIC_GAIN_FILTER,
    TIMED_GAIN_FILTER,
    FREQUENCY_FILTER,
    MAX_STEREO_FILTER,

    /* stereo effects */
    PITCH_EFFECT = 0,
    REVERB_EFFECT,
    DYNAMIC_PITCH_EFFECT,
    TIMED_PITCH_EFFECT,
    DISTORTION_EFFECT,
    DELAY_EFFECT,               /* phasing, chorus, flanging  */
    MAX_STEREO_EFFECT,
};

enum _aaxLimiterType
{
    RB_LIMITER_ELECTRONIC = 0,
    RB_LIMITER_DIGITAL,
    RB_LIMITER_VALVE,

    RB_LIMITER_MAX
};

typedef float _aaxRingBufferPitchShiftFn(float, float, float);
extern _aaxRingBufferPitchShiftFn* _aaxRingBufferDopplerFn[];

typedef float _aaxRingBufferDistFn(float, float, float, float, float, float);
extern _aaxRingBufferDistFn* _aaxRingBufferDistanceFn[];
extern _aaxRingBufferDistFn* _aaxRingBufferALDistanceFn[];

void iir_compute_coefs(float, float, float*, float*, float);
void _aaxRingBufferDelaysAdd(void**, float, unsigned int, const float*, const float*, size_t, float, float, float);
void _aaxRingBufferDelaysRemove(void**);
void _aaxRingBufferCreateHistoryBuffer(void**, int32_t*[_AAX_MAX_SPEAKERS], float, int, float);

float _lin(float v);
float _lin2db(float v);
float _db2lin(float v);
float _square(float v);
float _lin2log(float v);
float _log2lin(float v);
float _rad2deg(float v);
float _deg2rad(float v);
float _cos_deg2rad_2(float v);
float _2acos_rad2deg(float v);
float _cos_2(float v);
float _2acos(float v);

#endif /* _AAX_FE_COMMON_H */

