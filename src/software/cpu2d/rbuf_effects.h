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

    /* stereo effects */
    PITCH_EFFECT = 0,
    REVERB_EFFECT,
    DYNAMIC_PITCH_EFFECT,
    TIMED_PITCH_EFFECT,
    DISTORTION_EFFECT,
    DELAY_EFFECT,               /* phasing, chorus, flanging  */
    MAX_STEREO_EFFECT,
};

typedef float _convert_fn(float, float);
typedef float _aaxRingBufferPitchShiftFn(float, float, float);
typedef float _aaxRingBufferLFOGetFn(void*, const void*, unsigned, unsigned int);

typedef struct
{
   float f, min, max;
   float gate_threshold, gate_period;
   float step[_AAX_MAX_SPEAKERS];       /* step = frequency / refresh_rate */
   float down[_AAX_MAX_SPEAKERS];       /* compressor release rate         */
   float value[_AAX_MAX_SPEAKERS];      /* current value                   */
   float average[_AAX_MAX_SPEAKERS];    /* average value over time         */
   float compression[_AAX_MAX_SPEAKERS]; /* compression level              */
   _aaxRingBufferLFOGetFn *get;
   _convert_fn *convert;
   char inv, envelope, stereo_lnk;
} _aaxRingBufferLFOInfo;

typedef struct
{
   float value;
   uint32_t pos;
   unsigned int stage, max_stages;
   float step[_MAX_ENVELOPE_STAGES];
   uint32_t max_pos[_MAX_ENVELOPE_STAGES];
} _aaxRingBufferEnvelopeInfo;

typedef struct
{
   float gain;
   unsigned int sample_offs[_AAX_MAX_SPEAKERS];
} _aaxRingBufferDelayInfo;

typedef struct
{
   float coeff[4];
   float Q, k, fs, lf_gain, hf_gain;
   float freqfilter_history[_AAX_MAX_SPEAKERS][2];
   _aaxRingBufferLFOInfo *lfo;
} _aaxRingBufferFreqFilterInfo;

typedef struct
{
   _aaxRingBufferFreqFilterInfo band[_AAX_MAX_EQBANDS];
} _aaxRingBufferEqualizerInfo;

typedef struct
{
   _aaxRingBufferLFOInfo lfo;
   _aaxRingBufferDelayInfo delay;

   int32_t* delay_history[_AAX_MAX_SPEAKERS];
   void* history_ptr;

   /* temporary storage, track specific. */
   unsigned int curr_noffs[_AAX_MAX_SPEAKERS];
   unsigned int curr_coffs[_AAX_MAX_SPEAKERS];
   unsigned int curr_step[_AAX_MAX_SPEAKERS];

   char loopback;
} _aaxRingBufferDelayEffectData;

typedef struct
{
   /* reverb*/
   float gain;
   unsigned int no_delays;
   _aaxRingBufferDelayInfo delay[_AAX_MAX_DELAYS];

    unsigned int no_loopbacks;
    _aaxRingBufferDelayInfo loopback[_AAX_MAX_LOOPBACKS];
    int32_t* reverb_history[_AAX_MAX_SPEAKERS];
    void* history_ptr;

    _aaxRingBufferFreqFilterInfo *freq_filter;

} _aaxRingBufferReverbData;


typedef struct
{
   float param[4];
   void* data;          /* filter specific interal data structure */
   int state;
} _aaxRingBufferFilterInfo;

void _aaxRingBufferDelaysAdd(void**, float, unsigned int, const float*, const float*, unsigned int, float, float, float);
void _aaxRingBufferDelaysRemove(void**);
// void _aaxRingBufferDelayRemoveNum(_aaxRingBuffer*, unsigned int);

/* -------------------------------------------------------------------------- */

extern _aaxRingBufferPitchShiftFn* _aaxRingBufferDopplerFn[];

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

float _aaxRingBufferLFOGetSine(void*, const void*, unsigned, unsigned int);
float _aaxRingBufferLFOGetSquare(void*, const void*, unsigned, unsigned int);
float _aaxRingBufferLFOGetTriangle(void*, const void*, unsigned, unsigned int);
float _aaxRingBufferLFOGetSawtooth(void*, const void*, unsigned, unsigned int);
float _aaxRingBufferLFOGetFixedValue(void*, const void*, unsigned,unsigned int);
float _aaxRingBufferLFOGetGainFollow(void*, const void*, unsigned, unsigned int);
float _aaxRingBufferLFOGetCompressor(void*, const void*, unsigned, unsigned int);
float _aaxRingBufferLFOGetPitchFollow(void*, const void*, unsigned, unsigned int);

float _aaxRingBufferEnvelopeGet(_aaxRingBufferEnvelopeInfo*, char);


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

