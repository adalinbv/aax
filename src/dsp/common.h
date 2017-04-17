/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#define WRITEFN         0
#define READFN		!WRITEFN
#define EPS		1e-5
#define _MAX_FE_SLOTS	3

#define LEVEL_32DB	0.02511886432f
#define LEVEL_64DB	0.00063095734f
#define LEVEL_96DB	0.00001584893f
#define LEVEL_128DB	0.00000039811f

enum _aax3dFiltersEffects
{
    /* 3d filters */
    DISTANCE_FILTER = 0,        /* distance attennuation */
    ANGULAR_FILTER,             /* audio cone support    */
    MAX_3D_FILTER,

    /* 3d effects */
    VELOCITY_EFFECT = 0,        /* Doppler               */
    CONVOLUTION_EFFECT,
    MAX_3D_EFFECT,
};

enum _aax2dFiltersEffects
{
    /* final mixer stage */
    EQUALIZER_LF = 0,
    EQUALIZER_HF,
    HRTF_HEADSHADOW,
    SURROUND_CROSSOVER_LP = HRTF_HEADSHADOW,
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
    DELAY_EFFECT,               /* phasing, chorus or flanging  */
    MAX_STEREO_EFFECT,
};

enum _aaxLimiterType
{
    RB_LIMITER_ELECTRONIC = 0,
    RB_LIMITER_DIGITAL,
    RB_LIMITER_VALVE,

    RB_LIMITER_MAX
};

enum _aaxFreqFilterType
{
   LOWPASS = -1,
   BANDPASS = 0,
   HIGHPASS = 1
};

typedef struct
{
   float param[4];
   int state;
   void* data;          /* filter specific interal data structure */

} _aaxFilterInfo;

typedef struct
{
   float param[4];
   int state;
   void* data;          /* effect specific interal data structure */


} _aaxEffectInfo;

typedef float _aaxRingBufferPitchShiftFn(float, float, float);
extern _aaxRingBufferPitchShiftFn* _aaxRingBufferDopplerFn[];

typedef float _aaxRingBufferDistFn(float, float, float, float, float, float);
extern _aaxRingBufferDistFn* _aaxRingBufferDistanceFn[];
extern _aaxRingBufferDistFn* _aaxRingBufferALDistanceFn[];

void _aaxRingBufferDelaysAdd(void**, float, unsigned int, const float*, const float*, size_t, float, float, float);
void _aaxRingBufferDelaysRemove(void**);
size_t _aaxRingBufferCreateHistoryBuffer(void**, int32_t*[_AAX_MAX_SPEAKERS], size_t, int);

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

typedef float (*cvtfn_t)(float);

/* frequency filters */
#define _aax_movingaverage_compute(fc, fs)	(1.f-expf(-GMATH_2PI*(fc)/(fs)))

#define _QfromBW(fc, BW)		((fc)/(BW))
#define _QfromF1F2(f1, f2)		_QfromBW(0.5f*((f1)+(f2)), (f2)-(f1))
void _aax_bessel_compute(float, void*);
void _aax_butterworth_compute(float, void*);


#endif /* _AAX_FE_COMMON_H */

