/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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

#define LOG_60DB	-3.0f   // logf(LEVEL_60DB)

#define LEVEL_32DB	0.02511886321008205413818f
#define LEVEL_60DB	0.00100000004749745130539f
#define LEVEL_64DB	0.00063095724908635020256f
#define LEVEL_90DB	0.00003162277789670042694f
#define LEVEL_96DB	0.00001584892561368178576f
#define LEVEL_128DB	0.00000039810709040466463f

#define MINIMUM_CUTOFF	20.0f
#define MAXIMUM_CUTOFF	20000.0f
#define HIGHEST_CUTOFF(fs)    _MIN(0.8f*0.5f*fs, MAXIMUM_CUTOFF)
#define CLIP_FREQUENCY(f, fs) _MINMAX(f, MINIMUM_CUTOFF, HIGHEST_CUTOFF(fs))

#define PRINT_EFF_T(e) { int s; \
 printf("%s\n", __func__); \
 printf("id: %x\n", (e)->id); \
 printf("pos: %i\n", (e)->pos); \
 printf("state: %x\n", (e)->state); \
 printf("type: %i\n", (e)->type); \
 for(s=0; s<_MAX_FE_SLOTS && (e)->slot[s]; ++s) { \
  printf(" slot[%i].src: %i\n", s, (e)->slot[s]->src); \
  printf(" slot[%i].state: %x\n", s, (e)->slot[s]->state); \
  printf(" slot[%i].updated: %i\n", s, (e)->slot[s]->updated); \
  printf(" slot[%i].param: (%.1g, %.1g, %.1g, %.1g)\n", s, (e)->slot[s]->param[0], (e)->slot[s]->param[1], (e)->slot[s]->param[2], (e)->slot[s]->param[3]); \
  printf(" slot[%i].data_size: %li\n", s, (e)->slot[s]->data_size); \
  printf(" slot[%i].data: %p\n", s, (e)->slot[s]->data); \
  printf(" slot[%i].destroy: %p\n", s, (e)->slot[s]->destroy); \
  printf(" slot[%i].swap: %p\n", s, (e)->slot[s]->swap); \
 } printf("info: %p\n", (e)->info); \
 printf("handle: %p\n", (e)->handle); \
}


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
    EQUALIZER_LMF, // low-mid
    EQUALIZER_HMF, // high-mid
    HRTF_HEADSHADOW,
    SURROUND_CROSSOVER_LP = HRTF_HEADSHADOW,
    MAX_FILTERS,

    _MAX_GRAPH_EQ = 4,
    _MAX_PARAM_EQ = 4,

    /* stereo filters */
    VOLUME_FILTER = 0,		// must be the same as OCCLUSION_FILTER
    DYNAMIC_GAIN_FILTER,
    TIMED_GAIN_FILTER,
    FREQUENCY_FILTER,
    BITCRUSHER_FILTER,
    DYNAMIC_LAYER_FILTER,
    TIMED_LAYER_FILTER,
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
    DELAY_LINE_EFFECT,		/* delay-line  */
    WAVEFOLD_EFFECT,
    FREQUENCY_SHIFT_EFFECT,
    MAX_STEREO_EFFECT
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
   HIGHPASS = 1,
   ALLPASS = 2
};

typedef struct
{
   int src;
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


void _aax_dsp_destroy(void*);
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
float _ln(float v);
float _exp(float v);
float _log(float v);
float _pow(float v);
float _lin2db(float v);
float _db2lin(float v);
float _square(float v);
float _sqroot(float v);
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

float _wrap_max(float v, float max);               // wrap between 0 and max
float _wrap_minmax(float v, float min, float max); // wrap between min and max

float _note2freq(int n);
int _freq2note(float v);
char *_note2name(int n);

float reverb_time_to_decay_level(float reverb_time);
float decay_level_to_reverb_time(float decay_level);

typedef float (*cvtfn_t)(float);

FLOAT _lorentz(FLOAT v, FLOAT c);

#endif /* _AAX_FE_COMMON_H */

