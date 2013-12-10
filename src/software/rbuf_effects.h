/*
 * Copyright 2005-2013 by Erik Hofman.
 * Copyright 2009-2013 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_RBUF_EFFECTS_H
#define _AAX_RBUF_EFFECTS_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "driver.h"

#define _AAX_MAX_DELAYS         8
#define _AAX_MAX_LOOPBACKS      8
#define _AAX_MAX_FILTERS        2
#define _MAX_ENVELOPE_STAGES    6
#define _AAX_MAX_EQBANDS        8
#define _MAX_SLOTS              3
#define DELAY_EFFECTS_TIME      0.070f
#define REVERB_EFFECTS_TIME     0.700f
#if 0
#define NO_DELAY_EFFECTS_TIME
#undef DELAY_EFFECTS_TIME
#define DELAY_EFFECTS_TIME      0.0f
#endif

#define ENVELOPE_FOLLOW_STEP_CVT(a)     _MINMAX(-0.1005f+powf((a), 0.25f)/3.15f, 0.0f, 1.0f)


enum
{
    SCRATCH_BUFFER0 = _AAX_MAX_SPEAKERS,
    SCRATCH_BUFFER1,

    MAX_SCRATCH_BUFFERS
};

enum
{
    PITCH_CHANGED          = 0x00000001,
    GAIN_CHANGED           = 0x00000002,
    DIST_CHANGED           = 0x00000004,
    MTX_CHANGED            = 0x00000008,
    SPEED_CHANGED          = 0x00000010,

    CONE_DEFINED           = 0x00010000,
    DYNAMIC_PITCH_DEFINED  = 0x00020000,

    /* SCENE*/
    DISTQUEUE_CHANGED      = 0x08000000,
    SCENE_CHANGED          = 0x10000000,
    REVERB_CHANGED         = 0x20000000,
    DISTDELAY_CHANGED      = 0x40000000,
    WIND_CHANGED           = 0x80000000
};

#define PITCH_CHANGE		(PITCH_CHANGED | DYNAMIC_PITCH_DEFINED)

enum
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

    /* 3d filters */
    DISTANCE_FILTER = 0,        /* distance attennuation */
    ANGULAR_FILTER,             /* audio cone support    */
    MAX_3D_FILTER,

    /* stereo effects */
    PITCH_EFFECT = 0,
    REVERB_EFFECT,
    DYNAMIC_PITCH_EFFECT,
    TIMED_PITCH_EFFECT,
    DISTORTION_EFFECT,
    DELAY_EFFECT,               /* phasing, chorus, flanging  */
    MAX_STEREO_EFFECT,

    /* 3d effects */
    VELOCITY_EFFECT = 0,        /* Doppler               */
    MAX_3D_EFFECT,
};

/* 3d properties */
#define _PROP3D_CLEAR(q)                ((q)->state3d &= (CONE_DEFINED|DYNAMIC_PITCH_DEFINED))
#define _PROP3D_PITCH_HAS_CHANGED(q)    ((q)->state3d & PITCH_CHANGE)
#define _PROP3D_GAIN_HAS_CHANGED(q)     ((q)->state3d & GAIN_CHANGED)
#define _PROP3D_DIST_HAS_CHANGED(q)     ((q)->state3d & DIST_CHANGED)
#define _PROP3D_MTX_HAS_CHANGED(q)      ((q)->state3d & MTX_CHANGED)
#define _PROP3D_SPEED_HAS_CHANGED(q)    ((q)->state3d & SPEED_CHANGED)
#define _PROP3D_MTXSPEED_HAS_CHANGED(q) ((q)->state3d & (SPEED_CHANGED|MTX_CHANGED))
#define _PROP3D_CONE_IS_DEFINED(q)      ((q)->state3d & CONE_DEFINED)

#define _PROP3D_PITCH_SET_CHANGED(q)    ((q)->state3d |= PITCH_CHANGED)
#define _PROP3D_GAIN_SET_CHANGED(q)     ((q)->state3d |= GAIN_CHANGED)
#define _PROP3D_DIST_SET_CHANGED(q)     ((q)->state3d |= DIST_CHANGED)
#define _PROP3D_MTX_SET_CHANGED(q)      ((q)->state3d |= MTX_CHANGED)
#define _PROP3D_SPEED_SET_CHANGED(q)    ((q)->state3d |= SPEED_CHANGED)
#define _PROP3D_CONE_SET_DEFINED(q)     ((q)->state3d |= CONE_DEFINED)
#define _PROP3D_DYNAMIC_PITCH_SET_DEFINED(q) ((q)->state3d |= DYNAMIC_PITCH_DEFINED)

#define _PROP3D_PITCH_CLEAR_CHANGED(q)  ((q)->state3d &= ~PITCH_CHANGED)
#define _PROP3D_GAIN_CLEAR_CHANGED(q)   ((q)->state3d &= ~GAIN_CHANGED)
#define _PROP3D_DIST_CLEAR_CHANGED(q)   ((q)->state3d &= ~DIST_CHANGED)
#define _PROP3D_MTX_CLEAR_CHANGED(q)    ((q)->state3d &= ~MTX_CHANGED)
#define _PROP3D_SPEED_CLEAR_CHANGED(q)  ((q)->state3d &= ~SPEED_CHANGED)
#define _PROP3D_CONE_CLEAR_DEFINED(q)   ((q)->state3d &= ~CONE_DEFINED)
#define _PROP3D_DYNAMIC_PITCH_CLEAR_DEFINED(q) ((q)->state3d &= ~DYNAMIC_PITCH_DEFINED)

/* 3d properties: AAX Scene extension*/
#define _PROP3D_SCENE_IS_DEFINED(q)     ((q)->state3d & SCENE_CHANGED)
#define _PROP3D_REVERB_IS_DEFINED(q)    ((q)->state3d & REVERB_CHANGED)
#define _PROP3D_DISTDELAY_IS_DEFINED(q) ((q)->state3d & DISTDELAY_CHANGED)
#define _PROP3D_DISTQUEUE_IS_DEFINED(q) ((q)->state3d & DISTQUEUE_CHANGED)
#define _PROP3D_WIND_IS_DEFINED(q)      ((q)->state3d & WIND_CHANGED)

#define _PROP3D_SCENE_SET_CHANGED(q)    ((q)->state3d |= SCENE_CHANGED)
#define _PROP3D_REVERB_SET_CHANGED(q)   ((q)->state3d |= REVERB_CHANGED)
#define _PROP3D_DISTDELAY_SET_DEFINED(q) ((q)->state3d |= DISTDELAY_CHANGED)
#define _PROP3D_DISTQUEUE_SET_DEFINED(q) ((q)->state3d |= (DISTQUEUE_CHANGED|DISTDELAY_CHANGED))
#define _PROP3D_WIND_SET_CHANGED(q)     ((q)->state3d |= WIND_CHANGED)

#define _PROP3D_SCENE_CLEAR_CHANGED(q)  ((q)->state3d &= ~SCENE_CHANGED)
#define _PROP3D_REVERB_CLEAR_CHANGED(q) ((q)->state3d &= ~REVERB_CHANGED)
#define _PROP3D_DISTDELAY_CLEAR_DEFINED(q) ((q)->state3d &= ~DISTDELAY_CHANGED)
#define _PROP3D_DISTQUEUE_CLEAR_DEFINED(q) ((q)->state3d &= ~(DISTQUEUE_CHANGED|DISTDELAY_CHANGED))
#define _PROP3D_WIND_CLEAR_CHANGED(q)   ((q)->state3d &= ~WIND_CHANGED)


typedef float _convert_fn(float, float);
typedef float _oalRingBufferDistFunc(float, float, float, float, float, float);
typedef float _oalRingBufferPitchShiftFunc(float, float, float);
typedef float _oalRingBufferLFOGetFunc(void*, const void*, unsigned, unsigned int);

typedef struct
{
   float f, min, max;
   float gate_threshold, gate_period;
   float step[_AAX_MAX_SPEAKERS];       /* step = frequency / refresh_rate */
   float down[_AAX_MAX_SPEAKERS];       /* compressor release rate         */
   float value[_AAX_MAX_SPEAKERS];      /* current value                   */
   float average[_AAX_MAX_SPEAKERS];    /* average value over time         */
   float compression[_AAX_MAX_SPEAKERS]; /* compression level              */
   _oalRingBufferLFOGetFunc *get;
   _convert_fn *convert;
   char inv, envelope, stereo_lnk;
} _oalRingBufferLFOInfo;

typedef struct
{
   float value;
   uint32_t pos;
   unsigned int stage, max_stages;
   float step[_MAX_ENVELOPE_STAGES];
   uint32_t max_pos[_MAX_ENVELOPE_STAGES];
} _oalRingBufferEnvelopeInfo;

typedef struct
{
   float gain;
   unsigned int sample_offs[_AAX_MAX_SPEAKERS];
} _oalRingBufferDelayInfo;

typedef struct
{
   float coeff[4];
   float Q, k, fs, lf_gain, hf_gain;
   float freqfilter_history[_AAX_MAX_SPEAKERS][2];
   _oalRingBufferLFOInfo *lfo;
} _oalRingBufferFreqFilterInfo;

typedef struct
{
   _oalRingBufferFreqFilterInfo band[_AAX_MAX_EQBANDS];
} _oalRingBufferEqualizerInfo;

typedef struct
{
   _oalRingBufferLFOInfo lfo;
   _oalRingBufferDelayInfo delay;

   int32_t* delay_history[_AAX_MAX_SPEAKERS];
   void* history_ptr;

   /* temporary storage, track specific. */
   unsigned int curr_noffs[_AAX_MAX_SPEAKERS];
   unsigned int curr_coffs[_AAX_MAX_SPEAKERS];
   unsigned int curr_step[_AAX_MAX_SPEAKERS];

   char loopback;
} _oalRingBufferDelayEffectData;

typedef struct
{
   /* reverb*/
   float gain;
   unsigned int no_delays;
   _oalRingBufferDelayInfo delay[_AAX_MAX_DELAYS];

    unsigned int no_loopbacks;
    _oalRingBufferDelayInfo loopback[_AAX_MAX_LOOPBACKS];
    int32_t* reverb_history[_AAX_MAX_SPEAKERS];
    void* history_ptr;

    _oalRingBufferFreqFilterInfo *freq_filter;

} _oalRingBufferReverbData;


typedef struct
{
   float param[4];
   void* data;          /* filter specific interal data structure */
   int state;
} _oalRingBufferFilterInfo;

void _oalRingBufferDelaysAdd(void**, float, unsigned int, const float*, const float*, unsigned int, float, float, float);
void _oalRingBufferDelaysRemove(void**);
// void _oalRingBufferDelayRemoveNum(_oalRingBuffer*, unsigned int);

/* -------------------------------------------------------------------------- */

extern _oalRingBufferDistFunc* _oalRingBufferDistanceFunc[];
extern _oalRingBufferDistFunc* _oalRingBufferALDistanceFunc[];
extern _oalRingBufferPitchShiftFunc* _oalRingBufferDopplerFunc[];

extern _aaxDriverCompress _aaxProcessCompression;

float _lin(float v);
float _square(float v);
float _lin2log(float v);
float _log2lin(float v);
float _lin2db(float v);
float _db2lin(float v);
float _rad2deg(float v);
float _deg2rad(float v);
float _cos_deg2rad_2(float v);
float _2acos_rad2deg(float v);
float _cos_2(float v);
float _2acos(float v);

float _linear(float v, float f);
float _compress(float v, float f);

float _oalRingBufferLFOGetSine(void*, const void*, unsigned, unsigned int);
float _oalRingBufferLFOGetSquare(void*, const void*, unsigned, unsigned int);
float _oalRingBufferLFOGetTriangle(void*, const void*, unsigned, unsigned int);
float _oalRingBufferLFOGetSawtooth(void*, const void*, unsigned, unsigned int);
float _oalRingBufferLFOGetFixedValue(void*, const void*, unsigned,unsigned int);
float _oalRingBufferLFOGetGainFollow(void*, const void*, unsigned, unsigned int);
float _oalRingBufferLFOGetCompressor(void*, const void*, unsigned, unsigned int);
float _oalRingBufferLFOGetPitchFollow(void*, const void*, unsigned, unsigned int);

float _oalRingBufferEnvelopeGet(_oalRingBufferEnvelopeInfo*, char);


void bufEffectsApply(int32_ptr, const int32_ptr, int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned char, void*, void*, void*);
void bufFilterFrequency(int32_ptr, const int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, void*, unsigned char);
void bufEffectDistort(int32_ptr, const int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, void*);
void bufEffectDelay(int32_ptr, const int32_ptr, int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, void*, unsigned int);
void bufEffectReflections(int32_t*, const int32_ptr, const int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, const void*);
void bufEffectReverb(int32_t*, unsigned int, unsigned int, unsigned int, unsigned int, const void*);

void iir_compute_coefs(float, float, float*, float*, float);

void bufCompress(void*, unsigned int*, unsigned int*, float, float);
void bufCompressElectronic(void*, unsigned int*, unsigned int*);
void bufCompressDigital(void*, unsigned int*, unsigned int*);
void bufCompressValve(void*, unsigned int*, unsigned int*);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_RBUF_EFFECTS_H */

