/*
 * Copyright 2005-2020 by Erik Hofman.
 * Copyright 2009-2020 by Adalin B.V.
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

#ifndef _AAX_FE_COMMON_H
#define _AAX_FE_COMMON_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <base/geometry.h>
#include <backends/driver.h>

#define WRITEFN         0
#define READFN		!WRITEFN
#define EPS		1e-5
#define _MAX_FE_SLOTS	4

#define LEVEL_32DB	0.02511886432f
#define LEVEL_64DB	0.00063095734f
#define LEVEL_90DB	0.00003162278f
#define LEVEL_96DB	0.00001584893f
#define LEVEL_128DB	0.00000039811f

#define MINIMUM_CUTOFF	50.0f
#define MAXIMUM_CUTOFF	15000.0f

enum _aax3dFiltersEffects
{
    /* 3d filters */
    OCCLUSION_FILTER = 0,	/* direct path sound obstruction	*/
    DISTANCE_FILTER,		/* distance attennuation		*/
    DIRECTIONAL_FILTER,		/* directional audio cone support	*/
    MAX_3D_FILTER,

    /* 3d effects */
    VELOCITY_EFFECT = 0,	/* Doppler				*/
    REVERB_OCCLUSION_EFFECT,	/* reverb direct path sound obstruction */
    CONVOLUTION_OCCLUSION_EFFECT,/* convolution direct path sound obstruction */
    MAX_3D_EFFECT,
};

// WARNING: Changing this table requires changing the conversion tables
//          in effects.c and filters.c
enum _aax2dFiltersEffects
{
    /* *
     * final mixer stage for the mixer and frames
     * These filters are stored in a different location than the stereo
     * filters blow and hence do not interfere with eachtoher.
     */
    EQUALIZER_LF = 0,
    EQUALIZER_HF,
    HRTF_HEADSHADOW,
    SURROUND_CROSSOVER_LP = HRTF_HEADSHADOW,
    EQUALIZER_MAX,

    /* stereo filters */
    VOLUME_FILTER = 0,		// must be the same as OCCLUSION_FILTER
    DYNAMIC_GAIN_FILTER,
    TIMED_GAIN_FILTER,
    FREQUENCY_FILTER,
    BITCRUSHER_FILTER,
    MAX_STEREO_FILTER,

    /* stereo effects */
    PITCH_EFFECT = 0,
    REVERB_EFFECT,		// must be the same as REVERB_OCCLUSION_EFFECT
    CONVOLUTION_EFFECT,		// the same as CONVOLUTION_OCCLUSION_EFFECT
    DYNAMIC_PITCH_EFFECT,
    TIMED_PITCH_EFFECT,
    DISTORTION_EFFECT,
    DELAY_EFFECT,               /* phasing, chorus or flanging  */
    RINGMODULATE_EFFECT,
    MAX_STEREO_EFFECT,
};

enum _aaxLimiterType
{
    RB_LIMITER_ELECTRONIC = 0,
    RB_LIMITER_DIGITAL,
    RB_LIMITER_VALVE,
    RB_COMPRESS,

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
   int state;
   int updated;
   float param[4];

   size_t data_size;
   void* data;          /* filter and effect specific interal data structure */
   void (*destroy)(void*); /* function to call to free the data structure    */
   void (*swap)(void*,void*); /* function to swap the data structures        */

} _aaxDSPInfo;

typedef _aaxDSPInfo _aaxFilterInfo;
typedef _aaxDSPInfo _aaxEffectInfo;


void _aax_dsp_aligned_destroy(void *ptr);
void _aax_dsp_destroy(void *ptr);
void _aax_dsp_copy(void*, void*);
void _aax_dsp_swap(void*, void*);

typedef float _aaxPitchShiftFn(float, float);
extern _aaxPitchShiftFn* _aaxDopplerFn[];

typedef float _aaxDistFn(void*);
extern _aaxDistFn* _aaxDistanceFn[];
extern _aaxDistFn* _aaxALDistanceFn[];

void _aaxRingBufferDelaysAdd(void**, float, unsigned int, const float*, const float*, size_t, float, float, float);
void _aaxRingBufferDelaysRemove(void**);
//size_t _aaxRingBufferCreateHistoryBuffer(void**, int32_t*[_AAX_MAX_SPEAKERS], size_t, int);

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
float _degC2K(float v);
float _K2degC(float v);
float _degF2K(float v);
float _K2degF(float v);
float _atm2kpa(float v);
float _kpa2atm(float v);
float _bar2kpa(float v);
float _kpa2bar(float v);
float _psi2kpa(float v);
float _kpa2psi(float v);

typedef float (*cvtfn_t)(float);

FLOAT _lorentz(FLOAT v, FLOAT c);

#endif /* _AAX_FE_COMMON_H */

