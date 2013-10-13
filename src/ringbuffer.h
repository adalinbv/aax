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

#ifndef _AAX_RINGBUFFER_H
#define _AAX_RINGBUFFER_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <aax/aax.h>
#include <base/geometry.h>
#include <base/types.h>

#include "devices.h"
#include "driver.h"

#define BYTE_ALIGN		1

#define _AAX_MAX_DELAYS		8
#define _AAX_MAX_LOOPBACKS	8
#define _AAX_MAX_FILTERS	2
#define _MAX_ENVELOPE_STAGES	6
#define _AAX_MAX_EQBANDS	8
#define _MAX_SLOTS		3
#define CUBIC_SAMPS		4
#define DELAY_EFFECTS_TIME	0.070f
#define REVERB_EFFECTS_TIME	0.700f
#if 0
#define NO_DELAY_EFFECTS_TIME
#undef DELAY_EFFECTS_TIME
#define DELAY_EFFECTS_TIME	0.0f
#endif


#define DEFAULT_IMA4_BLOCKSIZE		36
#define IMA4_SMP_TO_BLOCKSIZE(a)	(((a)/2)+4)
#define BLOCKSIZE_TO_SMP(a)		((a) > 1) ? (((a)-4)*2) : 1
#define ENVELOPE_FOLLOW_STEP_CVT(a)	_MINMAX(-0.1005f+powf((a), 0.25f)/3.15f, 0.0f, 1.0f)
#ifndef NDEBUG
# define DBG_MEMCLR(a, b, c, d)		if (a) memset((b), 0, (c)*(d))
# define WRITE(a, b, dptr, ds, no_samples) \
   if (a) { static int ct = 0; if (++ct > (b)) { \
             WRITE_BUFFER_TO_FILE(dptr-ds, ds+no_samples); } }
#else
# define DBG_MEMCLR(a, b, c, d)
# define WRITE(a, b, dptr, ds, no_samples) \
	printf("Need to turn on debugging to use the WRITE macro\n")
#endif


enum
{
    MODEL_MTX = 0,			/* 4x4 model view matrix*/
    DIR_RIGHT = MODEL_MTX,		/* vec4*/
    DIR_UPWD,				/* vec4*/
    DIR_BACK,				/* vec4*/
    LOCATION,				/* vec4*/
    OFFSET = LOCATION,
    VELOCITY = LOCATION,
    GAIN = LOCATION,

    MAX_OBJECT
};

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
    DISTANCE_FILTER = 0,	/* distance attennuation */
    ANGULAR_FILTER,		/* audio cone support    */
    MAX_3D_FILTER,

    /* stereo effects */
    PITCH_EFFECT = 0,
    REVERB_EFFECT,
    DYNAMIC_PITCH_EFFECT,
    TIMED_PITCH_EFFECT,
    DISTORTION_EFFECT,
    DELAY_EFFECT,		/* phasing, chorus, flanging  */
    MAX_STEREO_EFFECT,

    /* 3d effects */
    VELOCITY_EFFECT = 0,	/* Doppler               */
    MAX_3D_EFFECT,
};

enum
{
    SCRATCH_BUFFER0 = _AAX_MAX_SPEAKERS,
    SCRATCH_BUFFER1,

    MAX_SCRATCH_BUFFERS
};

enum _oalRingBufferParam
{
   RB_VOLUME = 0,
   RB_VOLUME_MIN,
   RB_VOLUME_MAX,
   RB_FREQUENCY,
   RB_DURATION_SEC,
   RB_OFFSET_SEC,
   RB_LOOPPOINT_START,
   RB_LOOPPOINT_END,
   RB_LOOPING,
   RB_FORMAT,
   RB_NO_TRACKS,
   RB_NO_SAMPLES,
   RB_TRACKSIZE,
   RB_BYTES_SAMPLE,
   RB_OFFSET_SAMPLES,
   RB_IS_PLAYING

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

#define PITCH_CHANGE			(PITCH_CHANGED | DYNAMIC_PITCH_DEFINED)

/* 3d properties */
#define _PROP3D_CLEAR(q)		((q)->state3d &= (CONE_DEFINED|DYNAMIC_PITCH_DEFINED))
#define _PROP3D_PITCH_HAS_CHANGED(q)	((q)->state3d & PITCH_CHANGE)
#define _PROP3D_GAIN_HAS_CHANGED(q)	((q)->state3d & GAIN_CHANGED)
#define _PROP3D_DIST_HAS_CHANGED(q)	((q)->state3d & DIST_CHANGED)
#define _PROP3D_MTX_HAS_CHANGED(q)	((q)->state3d & MTX_CHANGED)
#define _PROP3D_SPEED_HAS_CHANGED(q)	((q)->state3d & SPEED_CHANGED)
#define _PROP3D_MTXSPEED_HAS_CHANGED(q)	((q)->state3d & (SPEED_CHANGED|MTX_CHANGED))
#define _PROP3D_CONE_IS_DEFINED(q)	((q)->state3d & CONE_DEFINED)

#define _PROP3D_PITCH_SET_CHANGED(q)	((q)->state3d |= PITCH_CHANGED)
#define _PROP3D_GAIN_SET_CHANGED(q)	((q)->state3d |= GAIN_CHANGED)
#define _PROP3D_DIST_SET_CHANGED(q)	((q)->state3d |= DIST_CHANGED)
#define _PROP3D_MTX_SET_CHANGED(q)	((q)->state3d |= MTX_CHANGED)
#define _PROP3D_SPEED_SET_CHANGED(q)	((q)->state3d |= SPEED_CHANGED)
#define _PROP3D_CONE_SET_DEFINED(q)	((q)->state3d |= CONE_DEFINED)
#define _PROP3D_DYNAMIC_PITCH_SET_DEFINED(q) ((q)->state3d |= DYNAMIC_PITCH_DEFINED)

#define _PROP3D_PITCH_CLEAR_CHANGED(q)	((q)->state3d &= ~PITCH_CHANGED)
#define _PROP3D_GAIN_CLEAR_CHANGED(q)	((q)->state3d &= ~GAIN_CHANGED)
#define _PROP3D_DIST_CLEAR_CHANGED(q)	((q)->state3d &= ~DIST_CHANGED)
#define _PROP3D_MTX_CLEAR_CHANGED(q)	((q)->state3d &= ~MTX_CHANGED)
#define _PROP3D_SPEED_CLEAR_CHANGED(q)	((q)->state3d &= ~SPEED_CHANGED)
#define _PROP3D_CONE_CLEAR_DEFINED(q)	((q)->state3d &= ~CONE_DEFINED)
#define _PROP3D_DYNAMIC_PITCH_CLEAR_DEFINED(q) ((q)->state3d &= ~DYNAMIC_PITCH_DEFINED)

/* delayed 3d properties */
#define _PROP_CLEAR(q)			_PROP3D_CLEAR((q)->dprops3d)
#define _PROP_PITCH_HAS_CHANGED(q)	_PROP3D_PITCH_HAS_CHANGED((q)->dprops3d)
#define _PROP_GAIN_HAS_CHANGED(q)	_PROP3D_GAIN_HAS_CHANGED((q)->dprops3d)
#define _PROP_DIST_HAS_CHANGED(q)	_PROP3D_DIST_HAS_CHANGED((q)->dprops3d)
#define _PROP_MTX_HAS_CHANGED(q)	_PROP3D_MTX_HAS_CHANGED((q)->dprops3d)
#define _PROP_SPEED_HAS_CHANGED(q)	_PROP3D_SPEED_HAS_CHANGED((q)->dprops3d)
#define _PROP_CONE_IS_DEFINED(q)	_PROP3D_CONE_IS_DEFINED((q)->dprops3d)

#define _PROP_PITCH_SET_CHANGED(q)	_PROP3D_PITCH_SET_CHANGED((q)->dprops3d)
#define _PROP_GAIN_SET_CHANGED(q)	_PROP3D_GAIN_SET_CHANGED((q)->dprops3d)
#define _PROP_DIST_SET_CHANGED(q)	_PROP3D_DIST_SET_CHANGED((q)->dprops3d)
#define _PROP_MTX_SET_CHANGED(q)	_PROP3D_MTX_SET_CHANGED((q)->dprops3d)
#define _PROP_SPEED_SET_CHANGED(q)	_PROP3D_SPEED_SET_CHANGED((q)->dprops3d)
#define _PROP_CONE_SET_DEFINED(q)	_PROP3D_CONE_SET_DEFINED((q)->dprops3d)
#define _PROP_DYNAMIC_PITCH_SET_DEFINED(q) _PROP3D_DYNAMIC_PITCH_SET_DEFINED((q)->dprops3d)

#define _PROP_PITCH_CLEAR_CHANGED(q)	_PROP3D_PITCH_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_GAIN_CLEAR_CHANGED(q)	_PROP3D_GAIN_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_DIST_CLEAR_CHANGED(q)	_PROP3D_DIST_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_MTX_CLEAR_CHANGED(q)	_PROP3D_MTX_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_SPEED_CLEAR_CHANGED(q)	_PROP3D_SPEED_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_CONE_CLEAR_DEFINED(q)	_PROP3D_CONE_CLEAR_DEFINED((q)->dprops3d)
#define _PROP_DYNAMIC_PITCH_CLEAR_DEFINED(q) _PROP3D_DYNAMIC_PITCH_CLEAR_DEFINED((q)->dprops3d)

/* 3d properties: AAX Scene extension*/
#define _PROP3D_SCENE_IS_DEFINED(q)	((q)->state3d & SCENE_CHANGED)
#define _PROP3D_REVERB_IS_DEFINED(q)	((q)->state3d & REVERB_CHANGED)
#define _PROP3D_DISTDELAY_IS_DEFINED(q)	((q)->state3d & DISTDELAY_CHANGED)
#define _PROP3D_DISTQUEUE_IS_DEFINED(q)	((q)->state3d & DISTQUEUE_CHANGED)
#define _PROP3D_WIND_IS_DEFINED(q)	((q)->state3d & WIND_CHANGED)

#define _PROP3D_SCENE_SET_CHANGED(q)	((q)->state3d |= SCENE_CHANGED)
#define _PROP3D_REVERB_SET_CHANGED(q)	((q)->state3d |= REVERB_CHANGED)
#define _PROP3D_DISTDELAY_SET_DEFINED(q) ((q)->state3d |= DISTDELAY_CHANGED)
#define _PROP3D_DISTQUEUE_SET_DEFINED(q) ((q)->state3d |= (DISTQUEUE_CHANGED|DISTDELAY_CHANGED))
#define _PROP3D_WIND_SET_CHANGED(q)	((q)->state3d |= WIND_CHANGED)

#define _PROP3D_SCENE_CLEAR_CHANGED(q)	((q)->state3d &= ~SCENE_CHANGED)
#define _PROP3D_REVERB_CLEAR_CHANGED(q)	((q)->state3d &= ~REVERB_CHANGED)
#define _PROP3D_DISTDELAY_CLEAR_DEFINED(q) ((q)->state3d &= ~DISTDELAY_CHANGED)
#define _PROP3D_DISTQUEUE_CLEAR_DEFINED(q) ((q)->state3d &= ~(DISTQUEUE_CHANGED|DISTDELAY_CHANGED))
#define _PROP3D_WIND_CLEAR_CHANGED(q)	((q)->state3d &= ~WIND_CHANGED)

/* delayed 3d properties: AAX Scene extension*/
#define _PROP_SCENE_IS_DEFINED(q)	_PROP3D_SCENE_IS_DEFINED((q)->dprops3d)
#define _PROP_REVERB_IS_DEFINED(q)	_PROP3D_REVERB_IS_DEFINED((q)->dprops3d)
#define _PROP_DISTDELAY_IS_DEFINED(q)	_PROP3D_DISTDELAY_IS_DEFINED((q)->dprops3d)
#define _PROP_DISTQUEUE_IS_DEFINED(q)	_PROP3D_DISTQUEUE_IS_DEFINED((q)->dprops3d)
#define _PROP_WIND_IS_DEFINED(q)	_PROP3D_WIND_IS_DEFINED((q)->dprops3d)

#define _PROP_SCENE_SET_CHANGED(q)	_PROP3D_SCENE_SET_CHANGED((q)->dprops3d)
#define _PROP_REVERB_SET_CHANGED(q)	_PROP3D_REVERB_SET_CHANGED((q)->dprops3d)
#define _PROP_DISTDELAY_SET_DEFINED(q)	_PROP3D_DISTDELAY_SET_DEFINED((q)->dprops3d)
#define _PROP_DISTQUEUE_SET_DEFINED(q)	_PROP3D_DISTQUEUE_SET_DEFINED((q)->dprops3d)
#define _PROP_WIND_SET_CHANGED(q)	_PROP3D_WIND_SET_CHANGED((q)->dprops3d)

#define _PROP_SCENE_CLEAR_CHANGED(q)	_PROP3D_SCENE_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_REVERB_CLEAR_CHANGED(q)	_PROP3D_REVERB_CLEAR_CHANGED(((q)->dprops3d)
#define _PROP_DISTDELAY_CLEAR_DEFINED(q) _PROP3D_DISTDELAY_CLEAR_DEFINED((q)->dprops3d)
#define _PROP_DISTQUEUE_CLEAR_DEFINED(q) _PROP3D_DISTQUEUE_CLEAR_DEFINED((q)->dprops3d)
#define _PROP_WIND_CLEAR_CHANGED(q)	_PROP3D_WIND_CLEAR_CHANGED((q)->dprops3d

/* filters */
#define _FILTER_GET_SLOT(F, s, p)	F->slot[s]->param[p]
#define _FILTER_GET_SLOT_STATE(F)	F->slot[0]->state
#define _FILTER_GET_SLOT_DATA(F, s)	F->slot[s]->data
#define _FILTER_SET_SLOT(F, s, p, v)	F->slot[s]->param[p] = v
#define _FILTER_SET_SLOT_DATA(F, s, v)	F->slot[s]->data = v

#define _FILTER_GET(P, f, p)		P->filter[f].param[p]
#define _FILTER_GET_STATE(P, f)		P->filter[f].state
#define _FILTER_GET_DATA(P, f)		P->filter[f].data
#define _FILTER_SET(P, f, p, v)		P->filter[f].param[p] = v
#define _FILTER_SET_STATE(P, f, v)	P->filter[f].state = v;
#define _FILTER_SET_DATA(P, f, v)	P->filter[f].data = v
#define _FILTER_COPY(P1, P2, f, p)	P1->filter[f].param[p] = P2->filter[f].param[p]
#define _FILTER_COPY_DATA(P1, P2, f)	P1->filter[f].data = P2->filter[f].data
#define _FILTER_COPY_STATE(P1, P2, f)	P1->filter[f].state = P2->filter[f].state

#define _FILTER_GET2D(G, f, p)		_FILTER_GET(G->props2d, f, p)
#define _FILTER_GET2D_DATA(G, f)	_FILTER_GET_DATA(G->props2d, f)
#define _FILTER_GET3D(G, f, p)		_FILTER_GET(G->dprops3d, f, p)
#define _FILTER_GET3D_DATA(G, f)	_FILTER_GET_DATA(G->dprops3d, f)
#define _FILTER_SET2D(G, f, p, v)	_FILTER_SET(G->props2d, f, p, v)
#define _FILTER_SET2D_DATA(G, f, v)	_FILTER_SET_DATA(G->props2d, f, v)
#define _FILTER_SET3D(G, f, p, v)	_FILTER_SET(G->dprops3d, f, p, v)
#define _FILTER_SET3D_DATA(G, f, v)	_FILTER_SET_DATA(G->dprops3d, f, v)
#define _FILTER_COPY2D_DATA(G1, G2, f)	_FILTER_COPY_DATA(G1->props2d, G2->props2d, f)
#define _FILTER_COPY3D_DATA(G1, G2, f)	_FILTER_COPY_DATA(G1->dprops3d, G2->dprops3d, f)

#define _FILTER_GETD3D(G, f, p)		_FILTER_GET(G->props3d, f, p)
#define _FILTER_SETD3D_DATA(G, f, v)	_FILTER_SET_DATA(G->props3d, f, v)
#define _FILTER_COPYD3D_DATA(G1, G2, f)	_FILTER_COPY_DATA(G1->props3d, G2->props3d, f)

#define _FILTER_SWAP_SLOT_DATA(P, f, F, s)	 			\
    do { void* ptr = P->filter[f].data;					\
    P->filter[f].data = F->slot[s]->data; F->slot[s]->data = ptr; 	\
    if (!s) aaxFilterSetState(F, P->filter[f].state); } while (0);

/* effects */
#define _EFFECT_GET_SLOT		_FILTER_GET_SLOT
#define _EFFECT_GET_SLOT_STATE		_FILTER_GET_SLOT_STATE
#define _EFFECT_GET_SLOT_DATA		_FILTER_GET_SLOT_DATA

#define _EFFECT_GET(P, f, p)		P->effect[f].param[p]
#define _EFFECT_GET_STATE(P, f)		P->effect[f].state
#define _EFFECT_GET_DATA(P, f)		P->effect[f].data
#define _EFFECT_SET(P, f, p, v)		P->effect[f].param[p] = v
#define _EFFECT_SET_STATE(P, f, v)	P->effect[f].state = v;
#define _EFFECT_SET_DATA(P, f, v)	P->effect[f].data = v
#define _EFFECT_COPY(P1, P2, f, p)	\
				P1->effect[f].param[p] = P2->effect[f].param[p]
#define _EFFECT_COPY_DATA(P1, P2, f)	P1->effect[f].data = P2->effect[f].data

#define _EFFECT_GET2D(G, f, p)		_EFFECT_GET(G->props2d, f, p)
#define _EFFECT_GET2D_DATA(G, f)	_EFFECT_GET_DATA(G->props2d, f)
#define _EFFECT_GET3D(G, f, p)		_EFFECT_GET(G->dprops3d, f, p)
#define _EFFECT_GET3D_DATA(G, f)	_EFFECT_GET_DATA(G->dprops3d, f)
#define _EFFECT_SET2D(G, f, p, v)	_EFFECT_SET(G->props2d, f, p, v)
#define _EFFECT_SET2D_DATA(G, f, v)	_EFFECT_SET_DATA(G->props2d, f, v)
#define _EFFECT_SET3D(G, f, p, v)	_EFFECT_SET(G->dprops3d, f, p, v)
#define _EFFECT_SET3D_DATA(G, f, v)	_EFFECT_SET_DATA(G->dprops3d, f, v)
#define _EFFECT_COPY2D(G1, G2, f, p)	_EFFECT_COPY(G1->props2d, G2->props2d, f, p)
#define _EFFECT_COPY3D(G1, G2, f, p)	_EFFECT_COPY(G1->dprops3d, G2->dprops3d, f, p)
#define _EFFECT_COPY2D_DATA(G1, G2, f)  _EFFECT_COPY_DATA(G1->props2d, G2->props2d, f)
#define _EFFECT_COPY3D_DATA(G1, G2, f)  _EFFECT_COPY_DATA(G1->dprops3d, G2->dprops3d, f)

#define _EFFECT_GETD3D(G, f, p)		_EFFECT_GET(G->props3d, f, p)
#define _EFFECT_SETD3D_DATA(G, f, v)	_EFFECT_SET_DATA(G->props3d, f, v)
#define _EFFECT_COPYD3D(G1, G2, f, p)	_EFFECT_COPY(G1->props3d, G2->props3d, f, p)
#define _EFFECT_COPYD3D_DATA(G1, G2, f)	_EFFECT_COPY_DATA(G1->props3d, G2->props3d, f)

#define _EFFECT_SWAP_SLOT_DATA(P, f, F, s)	 			\
    do { void* ptr = P->effect[f].data;					\
    P->effect[f].data = F->slot[s]->data; F->slot[s]->data = ptr; 	\
    if (!s) aaxEffectSetState(F, P->effect[f].state); } while (0);

typedef float _convert_fn(float, float);
typedef float _oalRingBufferDistFunc(float, float, float, float, float, float);
typedef float _oalRingBufferPitchShiftFunc(float, float, float);
typedef float _oalRingBufferLFOGetFunc(void*, const void*, unsigned, unsigned int);

typedef struct
{
   float f, min, max;
   float gate_threshold, gate_period;
   float step[_AAX_MAX_SPEAKERS];	/* step = frequency / refresh_rate */
   float down[_AAX_MAX_SPEAKERS];	/* compressor release rate         */
   float value[_AAX_MAX_SPEAKERS];	/* current value                   */
   float average[_AAX_MAX_SPEAKERS];	/* average value over time         */
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

typedef struct			/* static information about the sample*/
{
    void** track;

    _aaxCodec* codec;
    void** scratch;		/* resident scratch buffer*/
    unsigned char no_tracks;
    unsigned char bytes_sample;
    unsigned short ref_counter;

    float frequency_hz;
    float duration_sec;
    float loop_start_sec;
    float loop_end_sec;

    unsigned int no_samples;		/* actual no. samples*/
    unsigned int no_samples_avail;	/* maximum available no. samples*/
    unsigned int track_len_bytes;
    unsigned int dde_samples;

} _oalRingBufferSample;

typedef struct		/* playback related information about the sample*/
{
    _oalRingBufferSample* sample;		/* shared, constat settings */

    float peak[_AAX_MAX_SPEAKERS+1];		/* for the vu meter */
    float average[_AAX_MAX_SPEAKERS+1];

    float elapsed_sec;
    float pitch_norm;
    float volume_norm, gain_agc;
    float volume_min, volume_max;

    float dde_sec;
    float curr_pos_sec;
    unsigned int curr_sample;

    int format;

    unsigned int loop_max;
    unsigned int loop_no;
    char looping;

    char playing;
    char stopped;
    char streaming;

} _oalRingBuffer;

typedef struct
{
   float param[4];
   void* data;		/* filter specific interal data structure */
   int state;
} _oalRingBufferFilterInfo;

typedef ALIGN16 struct
{
   /* modelview matrix and velocity */
   mtx4_t matrix;
   mtx4_t velocity;

   float pitch, gain;
   int state3d;

} _oalRingBufferDelayed3dProps ALIGN16C;

typedef struct
{
   
   _oalRingBufferDelayed3dProps* dprops3d;	/* current   */
   _oalRingBufferDelayed3dProps* m_dprops3d;	/* modiefied */
   float buf3dq_step;
   int state;

   /* 3d filters and effects */
   _oalRingBufferFilterInfo filter[MAX_3D_FILTER];
   _oalRingBufferFilterInfo effect[MAX_3D_EFFECT];

} _oalRingBuffer3dProps;

typedef ALIGN16 struct
{
      /* pos[0] position; -1.0 left,  0.0 center, 1.0 right */
      /* pos[1] position; -1.0 down,  0.0 center, 1.0 up    */
      /* pos[2] position; -1.0 front, 0.0 center, 1.0 back  */
   vec4_t speaker[_AAX_MAX_SPEAKERS];

      /* head[0] side delay sec    */
      /* head[1] up delay sec      */
      /* head[2] forward delay sec */
      /* head[3] up offset sec     */
   vec4_t head;
   vec4_t hrtf[2];
   vec4_t hrtf_prev[2];

   /* stereo filters */
   _oalRingBufferFilterInfo filter[MAX_STEREO_FILTER];
   _oalRingBufferFilterInfo effect[MAX_STEREO_EFFECT];

   float prev_gain[_AAX_MAX_SPEAKERS];
   float prev_freq_fact;

   float dist_delay_sec;	/* time to keep playing after a stop request */
   float bufpos3dq;		/* distance delay queue buffer position      */

   struct {
      float pitch_lfo;
      float pitch;
      float gain_lfo;
      float gain;
   } final;

} _oalRingBuffer2dProps ALIGN16C;


typedef int
_oalRingBufferMix1NFunc(_oalRingBuffer*, _oalRingBuffer*, char,
                        _oalRingBuffer2dProps*, _oalRingBuffer2dProps*, 
                        unsigned char, unsigned char, unsigned int);
typedef int
_oalRingBufferMixMNFunc(_oalRingBuffer*, _oalRingBuffer*,
                        _oalRingBuffer2dProps*, _oalRingBuffer2dProps*,
                        unsigned char, unsigned int);


/**
 * Initialize a new sound buffer that holds no data.
 * The default values are for a single, 16 bits per sample track at 44100Hz.
 *
 * @param dde specifies if memory needs to be allocated for the delay effects
 *            buffer prior to the data section.
 *
 * returns the new ringbuffer or NULL if an error occured.
 */
_oalRingBuffer*
_oalRingBufferCreate(float);

/**
 * Test is a ringbuffer is ready for use or not
 *
 * @param rb Ringbuffer pointer to test
 *
 * returns true if useable, false otherwise
 */
int
_oalRingBufferIsValid(_oalRingBuffer*);


void
_oalRingBufferInit(_oalRingBuffer*, char);

#if 1
/**
 * Initialize the sound buffer
 *
 * @param data pointer to previously defined ringbuffer sample (or NULL)
 * @param codecs pointer to an array of block conversion funcs; 0 use default
 * @param channels no. of supported tracks
 * @param frequency recording frequency
 * @param format recording format
 * @param duration playback duration of every track
 * @param keep_original 0 to allow for speed optimizations of the data
 *
 * returns the new ringbuffer
 */
_oalRingBuffer*
_oalRingBufferInitSec(_oalRingBufferSample*, _aaxCodec**, unsigned char, float, enum aaxFormat, float, char);

/**
 * Initialize the sound buffer
 *
 * @param data pointer to previously defined ringbuffer sample (or NULL)
 * @param codecs pointer to an array of block conversion funcs; 0 use default
 * @param channels number of supported tracks
 * @param frequency recording frequency
 * @param format recording format
 * @param samples number of samples per channel to store in the buffer
 * @param keep_original 0 to allow for speed optimizations of the data
 *
 * return the new ringbuffer
 */
_oalRingBuffer*
_oalRingBufferInitSamples(_oalRingBufferSample*, _aaxCodec**, unsigned char, float, enum aaxFormat, unsigned int, char);

/**
 * Initialize the sound buffer
 *
 * @param data pointer to previously defined ringbuffer sample (or NULL)
 * @param codecs pointer to an array of block conversion funcs; 0 use default
 * @param channels number of supported tracks
 * @param frequency recording frequency
 * @param format recording format
 * @param size number of bytes per channel to store in the buffer
 * @param keep_original 0 to allow for speed optimizations of the data
 *
 * returns the new ringbuffer
 */
_oalRingBuffer*
_oalRingBufferInitBytes(_oalRingBufferSample*, _aaxCodec**, unsigned char, float, enum aaxFormat, unsigned int, char);
#endif

/**
 * Reference another RingBuffer, possibly sharing it's sample data.
 *
 * @param rb the ringbuffer to reference
 *
 * returns the newly created reference ringbuffer
 */
_oalRingBuffer*
_oalRingBufferReference(_oalRingBuffer*);

/**
 * Duplicate a RingBuffer based on another ringbuffer.
 *
 * @param rb the ringbuffer to duplicate
 * @param copy, true if the sample data needs to be copied, false otherwise
 * @param dde, true if the delay effects buffers needs to be copied
 *
 * returns the newly created ringbuffer
 */
_oalRingBuffer*
_oalRingBufferDuplicate(_oalRingBuffer*, char, char);

/**
 * Fill the buffer with sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample
 * @param track array of pointers to the separate sound tracks
 *              the number of tracks is defined within rb
 * @param blocksize Number of samples per block for each channel.
 * @param looping boolean value defining wether this sound should loop
 */
void
_oalRingBufferFillNonInterleaved(_oalRingBuffer*, const void*, unsigned, char);

/**
 * Fill the buffer with sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample
 * @param data a block of memory holding the interleaved sound sample
 *             the number of tracks is defined within rb
 * @param blocksize Number of samples per block for each channel.
 * @param looping boolean value defining wether this sound should loop
 */
void
_oalRingBufferFillInterleaved(_oalRingBuffer*, const void*, unsigned, char);

/**
 * Get the interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample 
 * @param data a memory block large enough for the interleaved tracks.
 * @param fact rersampling factor
 */
void
_oalRingBufferGetDataInterleaved(_oalRingBuffer*, void*, unsigned int, int, float);

/**
 * Get the interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample 
 * @param fact rersampling factor
 *
 * returns a memory block containing the interleaved tracks.
 */

void*
_oalRingBufferGetDataInterleavedMalloc(_oalRingBuffer*, int, float);

/**
 * Get the non-interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample
 * @param data a memory block large enough for the non-interleaved tracks.
 * @param fact rersampling factor
 */
void
_oalRingBufferGetDataNonInterleaved(_oalRingBuffer*, void*, unsigned int, int, float);

/**
 * Get the interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample 
 * @param fact rersampling factor
 *
 * returns a memory block containing the non-interleaved tracks.
 */

void*
_oalRingBufferGetDataNonInterleavedMalloc(_oalRingBuffer*, int, float);

/**
 * Clear the sound data of the ringbuffer
 *
 * @param rb the ringbuffer to clear
 */
void
_oalRingBufferClear(void*);

/**
 * Remove the ringbuffer and all it's tracks from memory.
 *
 * @param rb the ringbuffer to delete
 */
_oalRingBuffer*
_oalRingBufferDelete(_oalRingBuffer*);

/**
 * Set the playback position of the ringbuffer to zero sec.
 *
 * @param rb the ringbuffer to rewind
 */
void
_oalRingBufferRewind(_oalRingBuffer*);

/**
 * Set the playback position of the ringbuffer to the end of the buffer
 *
 * @param rb the ringbuffer to forward
 */
void
_oalRingBufferForward(_oalRingBuffer*);

/**
 * Starts playback of the ringbuffer.
 *
 * @param rb the ringbuffer to stop
 */
void
_oalRingBufferStart(_oalRingBuffer*);

/**
 * Stop playback of the ringbuffer.
 *
 * @param rb the ringbuffer to stop
 */
void
_oalRingBufferStop(_oalRingBuffer*);

/**
 * Starts streaming playback of the ringbuffer.
 *
 * @param rb the ringbuffer to stop
 */
void
_oalRingBufferStartStreaming(_oalRingBuffer*);

/**
 * Copy the delay effetcs buffers from one ringbuffer to the other.
 * 
 * @param dest destination ringbuffer
 * @param src source ringbuffer
 */
void
_oalRingBufferCopyDelyEffectsData(_oalRingBuffer*, const _oalRingBuffer*);

/**
 * M:N channel ringbuffer mixer.
 *
 * @param dest multi track destination buffer
 * @param src single track source buffer
 * @param ep2d 3d positioning information structure of the source
 * @param fp2f 3d positioning information structure of the parents frame
 * @param ctr update-rate counter:
 *     - Rendering to the destination buffer is done every frame at the
 *       interval rate. Updating of 3d properties and the like is done
 *       once every 'ctr' frame updates. so if ctr == 1, updates are
 *       done every frame.
 * @param nbuf number of buffers in the source queue (>1 means streaming)
 *
 * returns 0 if the sound has stopped playing, 1 otherwise.
 */
int _oalRingBufferMixMulti16(_oalRingBuffer *dest, _oalRingBuffer *src, _oalRingBuffer2dProps *ep2d, _oalRingBuffer2dProps *fp2d, unsigned char ctr, unsigned int nbuf);


/**
 * 1:N channel ringbuffer mixer.
 *
 * @param dest multi track destination buffer
 * @param src single track source buffer
 * @param mode mixing mode (resembles enum aaxRenderMode)
 * @param ep2d 3d positioning information structure of the source
 * @param fp2f 3d positioning information structure of the parents frame
 * @param ch channel to use from the source buffer if it is multi-channel
 * @param ctr update-rate counter:
 *     - Rendering to the destination buffer is done every frame at the
 *       interval rate. Updating of 3d properties and the like is done
 *       once every 'ctr' frame updates. so if ctr == 1, updates are
 *       done every frame.
 * @param nbuf number of buffers in the source queue (>1 means streaming)
 *
 * returns 0 if the sound has stopped playing, 1 otherwise.
 */
int _oalRingBufferMixMono16(_oalRingBuffer *dest, _oalRingBuffer *src, enum aaxRenderMode mode, _oalRingBuffer2dProps *ep2d, _oalRingBuffer2dProps *fp2d, unsigned char ch, unsigned char ctr, unsigned int nbuf);


int _oalRingBufferSetParamf(_oalRingBuffer*, enum _oalRingBufferParam, float);
int _oalRingBufferSetParami(_oalRingBuffer*, enum _oalRingBufferParam, unsigned int);
float _oalRingBufferGetParamf(const _oalRingBuffer*, enum _oalRingBufferParam);
unsigned int _oalRingBufferGetParami(const _oalRingBuffer*, enum _oalRingBufferParam);
int _oalRingBufferSetFormat(_oalRingBuffer*, _aaxCodec **, enum aaxFormat);

#define _oalRingBufferCopyParamf(dbr, srb, param) \
    _oalRingBufferSetParamf(drb, param, _oalRingBufferGetParamf(srb, param))
#define _oalRingBufferCopyParami(dbr, srb, param) \
    _oalRingBufferSetParami(drb, param, _oalRingBufferGetParami(srb, param))

void _oalRingBufferDelaysAdd(void**, float, unsigned int, const float*, const float*, unsigned int, float, float, float);
void _oalRingBufferDelaysRemove(void**);
// void _oalRingBufferDelayRemoveNum(_oalRingBuffer*, unsigned int);

unsigned int _oalRingBufferGetSource();
unsigned int _oalRingBufferPutSource();
unsigned int _oalRingBufferGetNoSources();
unsigned int _oalRingBufferSetNoSources(unsigned int);


/* --------------------------------------------------------------------------*/

typedef struct {
   unsigned char bits;
   enum aaxFormat format;
} _oalFormat_t;

extern _aaxDriverCompress _aaxProcessCompression;
extern _oalFormat_t _oalRingBufferFormat[AAX_FORMAT_MAX];

extern _aaxCodec* _oalRingBufferCodecs[];
extern _aaxCodec* _oalRingBufferCodecs_w8s[];

extern _oalRingBufferDistFunc* _oalRingBufferDistanceFunc[];
extern _oalRingBufferDistFunc* _oalRingBufferALDistanceFunc[];
extern _oalRingBufferPitchShiftFunc* _oalRingBufferDopplerFunc[];

void _aaxProcessCodec(int32_t*, void*, _aaxCodec*, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned char, char);
int32_t**_aaxProcessMixer(_oalRingBuffer*, _oalRingBuffer*,  _oalRingBuffer2dProps *, float, unsigned int*, unsigned int*, unsigned char, unsigned int);

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

void bufConvertDataToPCM24S(void*, void*, unsigned int, enum aaxFormat);
void bufConvertDataFromPCM24S(void*, void*, unsigned int, unsigned int, enum aaxFormat, unsigned int);


void _oalRingBufferCreateHistoryBuffer(void**, int32_t*[_AAX_MAX_SPEAKERS], float, int, float);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_RINGBUFFER_H*/

