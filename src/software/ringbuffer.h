/*
 * Copyright 2005-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _INT_RINGBUFFER_H
#define _INT_RINGBUFFER_H 1

#if defined(__cplusplus)
extern "C" {
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
    DISTANCE_FILTER = 0,	/* distance attewnuation */
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
    PITCH_CHANGED		= 0x00000001,
    GAIN_CHANGED		= 0x00000002,
    DIST_CHANGED		= 0x00000004,
    MTX_CHANGED			= 0x00000008,
    CONE_DEFINED		= 0x00000010,
    DYNAMIC_PITCH_DEFINED	= 0x00000020,

    /* SCENE*/
    SCENE_CHANGED		= 0x10000000,
    REVERB_CHANGED		= 0x20000000,
    DISTDELAY_CHANGED		= 0x40000000,
    WIND_CHANGED		= 0x80000000
};

enum
{
    SCRATCH_BUFFER0 = _AAX_MAX_SPEAKERS,
    SCRATCH_BUFFER1,

    MAX_SCRATCH_BUFFERS
};

enum _intRingBufferParam
{
   RB_PITCH = 0,
   RB_VOLUME,
   RB_VOLUME_MIN,
   RB_VOLUME_MAX,
   RB_FREQUENCY,
   RB_DURATION_SEC,
   RB_OFFSET_SEC,
   RB_LOOP_STARTPOINT,
   RB_LOOP_ENDPOINT,
   RB_LOOPING,
   RB_PLAYING,
   RB_STOPPED,
   RB_STREAMING,
   RB_FORMAT,
   RB_NO_TRACKS,
   RB_NO_SAMPLES,
   RB_NO_SAMPLES_AVAIL,
   RB_NO_DELAY_EFFECT_SAMPLES,
   RB_TRACKSIZE,
   RB_BYTES_SAMPLE,
   RB_OFFSET_SAMPLES,
   RB_IS_PLAYING,

   RB_PEAK_VALUE = 0x100,
   RB_AVERAGE_VALUE = 0x110,

};

#define PITCH_CHANGE			(PITCH_CHANGED | DYNAMIC_PITCH_DEFINED)
#define _PROP_PITCH_HAS_CHANGED(q)	((q)->state & PITCH_CHANGE)
#define _PROP_GAIN_HAS_CHANGED(q)	((q)->state & GAIN_CHANGED)
#define _PROP_DIST_HAS_CHANGED(q)	((q)->state & DIST_CHANGED)
#define _PROP_MTX_HAS_CHANGED(q)	((q)->state & MTX_CHANGED)
#define _PROP_CONE_IS_DEFINED(q)	((q)->state & CONE_DEFINED)

#define _PROP_PITCH_SET_CHANGED(q)	((q)->state |= PITCH_CHANGED)
#define _PROP_GAIN_SET_CHANGED(q)	((q)->state |= GAIN_CHANGED)
#define _PROP_DIST_SET_CHANGED(q)	((q)->state |= DIST_CHANGED)
#define _PROP_MTX_SET_CHANGED(q)	((q)->state |= MTX_CHANGED)
#define _PROP_CONE_SET_DEFINED(q)	((q)->state |= CONE_DEFINED)
#define _PROP_DYNAMIC_PITCH_SET_DEFINED(q)	((q)->state |= DYNAMIC_PITCH_DEFINED)

#define _PROP_PITCH_CLEAR_CHANGED(q)	((q)->state &= ~PITCH_CHANGED)
#define _PROP_GAIN_CLEAR_CHANGED(q)	((q)->state &= ~GAIN_CHANGED)
#define _PROP_DIST_CLEAR_CHANGED(q)	((q)->state &= ~DIST_CHANGED)
#define _PROP_MTX_CLEAR_CHANGED(q)	((q)->state &= ~MTX_CHANGED)
#define _PROP_CONE_CLEAR_DEFINED(q)	((q)->state &= ~CONE_DEFINED)
#define _PROP_DYNAMIC_PITCH_CLEAR_DEFINED(q)	((q)->state &= ~DYNAMIC_PITCH_DEFINED)

/* AAX Scene extension*/
#define _PROP_SCENE_IS_DEFINED(q)	((q)->state & SCENE_CHANGED)
#define _PROP_REVERB_IS_DEFINED(q)	((q)->state & REVERB_CHANGED)
#define _PROP_DISTDELAY_IS_DEFINED(q)	((q)->state & DISTDELAY_CHANGED)
#define _PROP_WIND_IS_DEFINED(q)	((q)->state & WIND_CHANGED)

#define _PROP_SCENE_SET_CHANGED(q)	((q)->state |= SCENE_CHANGED)
#define _PROP_REVERB_SET_CHANGED(q)	((q)->state |= REVERB_CHANGED)
#define _PROP_DISTDELAY_SET_DEFINED(q)	((q)->state |= DISTDELAY_CHANGED)
#define _PROP_WIND_SET_CHANGED(q)	((q)->state |= WIND_CHANGED)

#define _PROP_SCENE_CLEAR_CHANGED(q)	((q)->state &= ~SCENE_CHANGED)
#define _PROP_REVERB_CLEAR_CHANGED(q)	((q)->state &= ~REVERB_CHANGED)
#define _PROP_DISTDELAY_CLEAR_DEFINED(q) ((q)->state &= ~DISTDELAY_CHANGED)
#define _PROP_WIND_CLEAR_CHANGED(q)	((q)->state &= ~WIND_CHANGED)


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

#define _FILTER_GET2D(G, f, p)		_FILTER_GET(G->props2d, f, p)
#define _FILTER_GET2D_DATA(G, f)	_FILTER_GET_DATA(G->props2d, f)
#define _FILTER_GET3D(G, f, p)		_FILTER_GET(G->props3d, f, p)
#define _FILTER_GET3D_DATA(G, f)	_FILTER_GET_DATA(G->props3d, f)
#define _FILTER_SET2D(G, f, p, v)	_FILTER_SET(G->props2d, f, p, v)
#define _FILTER_SET2D_DATA(G, f, v)	_FILTER_SET_DATA(G->props2d, f, v)
#define _FILTER_SET3D(G, f, p, v)	_FILTER_SET(G->props3d, f, p, v)
#define _FILTER_SET3D_DATA(G, f, v)	_FILTER_SET_DATA(G->props3d, f, v)
#define _FILTER_COPY2D_DATA(G1, G2, f)	_FILTER_COPY_DATA(G1->props2d, G2->props2d, f)
#define _FILTER_COPY3D_DATA(G1, G2, f)	_FILTER_COPY_DATA(G1->props3d, G2->props3d, f)

#define _FILTER_SWAP_SLOT_DATA(P, f, F, s)	 			\
    do { void* ptr = P->filter[f].data;					\
    P->filter[f].data = F->slot[s]->data; F->slot[s]->data = ptr; 	\
    if (!s) aaxFilterSetState(F, P->filter[f].state); } while (0);

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
#define _EFFECT_GET3D(G, f, p)		_EFFECT_GET(G->props3d, f, p)
#define _EFFECT_GET3D_DATA(G, f)	_EFFECT_GET_DATA(G->props3d, f)
#define _EFFECT_SET2D(G, f, p, v)	_EFFECT_SET(G->props2d, f, p, v)
#define _EFFECT_SET2D_DATA(G, f, v)	_EFFECT_SET_DATA(G->props2d, f, v)
#define _EFFECT_SET3D(G, f, p, v)	_EFFECT_SET(G->props3d, f, p, v)
#define _EFFECT_SET3D_DATA(G, f, v)	_EFFECT_SET_DATA(G->props3d, f, v)
#define _EFFECT_COPY2D(G1, G2, f, p)	_EFFECT_COPY(G1->props2d, G2->props2d, f, p)
#define _EFFECT_COPY3D(G1, G2, f, p)	_EFFECT_COPY(G1->props3d, G2->props3d, f, p)
#define _EFFECT_COPY2D_DATA(G1, G2, f)  _EFFECT_COPY_DATA(G1->props2d, G2->props2d, f)
#define _EFFECT_COPY3D_DATA(G1, G2, f)  _EFFECT_COPY_DATA(G1->props3d, G2->props3d, f)

#define _EFFECT_SWAP_SLOT_DATA(P, f, F, s)	 			\
    do { void* ptr = P->effect[f].data;					\
    P->effect[f].data = F->slot[s]->data; F->slot[s]->data = ptr; 	\
    if (!s) aaxEffectSetState(F, P->effect[f].state); } while (0);

typedef float _convert_fn(float, float);
typedef float _intRingBufferDistFunc(float, float, float, float, float, float);
typedef float _intRingBufferPitchShiftFunc(float, float, float);
typedef float _intRingBufferLFOGetFunc(void*, const void*, unsigned, unsigned int);

typedef struct
{
   float f, min, max;
   float gate_threshold, gate_period;
   float step[_AAX_MAX_SPEAKERS];	/* step = frequency / refresh_rate */
   float down[_AAX_MAX_SPEAKERS];	/* compressor release rate         */
   float value[_AAX_MAX_SPEAKERS];	/* current value                   */
   float average[_AAX_MAX_SPEAKERS];	/* average value over time         */
   float compression[_AAX_MAX_SPEAKERS]; /* compression level              */
   _intRingBufferLFOGetFunc *get;
   _convert_fn *convert;
   char inv, envelope, stereo_lnk;
} _intRingBufferLFOInfo;

typedef struct
{
   float value;
   uint32_t pos;
   unsigned int stage, max_stages;
   float step[_MAX_ENVELOPE_STAGES];
   uint32_t max_pos[_MAX_ENVELOPE_STAGES];
} _intRingBufferEnvelopeInfo;

typedef struct
{
   float gain;
   unsigned int sample_offs[_AAX_MAX_SPEAKERS];
} _intRingBufferDelayInfo;

typedef struct
{
   float coeff[4];
   float Q, k, fs, lf_gain, hf_gain;
   float freqfilter_history[_AAX_MAX_SPEAKERS][2];
   _intRingBufferLFOInfo *lfo;
} _intRingBufferFreqFilterInfo;

typedef struct
{
   _intRingBufferFreqFilterInfo band[_AAX_MAX_EQBANDS];
} _intRingBufferEqualizerInfo;

typedef struct
{
   _intRingBufferLFOInfo lfo;
   _intRingBufferDelayInfo delay;

   int32_t* delay_history[_AAX_MAX_SPEAKERS];
   void* history_ptr;

   /* temporary storage, track specific. */
   unsigned int curr_noffs[_AAX_MAX_SPEAKERS];
   unsigned int curr_coffs[_AAX_MAX_SPEAKERS];
   unsigned int curr_step[_AAX_MAX_SPEAKERS];

   char loopback;
} _intRingBufferDelayEffectData;

typedef struct
{
   /* reverb*/
   float gain;
   unsigned int no_delays;
   _intRingBufferDelayInfo delay[_AAX_MAX_DELAYS];

    unsigned int no_loopbacks;
    _intRingBufferDelayInfo loopback[_AAX_MAX_LOOPBACKS];
    int32_t* reverb_history[_AAX_MAX_SPEAKERS];
    void* history_ptr;

    _intRingBufferFreqFilterInfo *freq_filter;

} _intRingBufferReverbData;

typedef struct		/* static information about the sample*/
{
    void** track;			/* audio track buffer */
    void** scratch;			/* resident scratch buffer */

    _aaxCodec* codec;
    unsigned char no_tracks;
    unsigned char bytes_sample;
    unsigned short ref_counter;

    float frequency_hz;
    float duration_sec;
    float loop_start_sec;
    float loop_end_sec;

    unsigned int no_samples;		/* actual no. samples */
    unsigned int no_samples_avail;	/* maximum available no. samples*/
    unsigned int track_len_bytes;
    unsigned int dde_samples;

} _intRingBufferSample;

typedef struct		/* playback related information about the sample */
{
    _intRingBufferSample* sample;	/* shared, constat settings */

    float peak[_AAX_MAX_SPEAKERS];	/* for the vu meter */
    float average[_AAX_MAX_SPEAKERS];

    float elapsed_sec;
    float pitch_norm;
    float volume_norm;
    float volume_min, volume_max;

    float dde_sec;
    float curr_pos_sec;
    unsigned int curr_sample;

    int format;
    char playing;
    char stopped;
    char looping;
    char streaming;

} _intRingBuffer;

#if 0
/**
 * Initialize a new sound buffer that holds no data.
 * The default values are for a single, 16 bits per sample track at 44100Hz.
 *
 * @param dde specifies if memory needs to be allocated for the delay effects
 *            buffer prior to the data section.
 *
 * returns the new ringbuffer or NULL if an error occured.
 */
_intRingBuffer*
_intRingBufferCreate(float);
#endif

/**
 * Initialize an already crrated sound buffer.
 * The default values are for a single, 16 bits per sample track at 44100Hz.
 *
 * @param rb  pointer to the existing ringbuffer.
 * @param dde specifies if memory needs to be allocated for the delay effects
 *            buffer prior to the data section.
 *
 * returns true if successfule, false (0) otherwise.
 */
int
_intRingBufferSetup(_intRingBuffer*, float);

/**
 * Test is a ringbuffer is ready for use or not
 *
 * @param rb Ringbuffer pointer to test
 *
 * returns true if useable, false otherwise
 */
int
_intRingBufferIsValid(_intRingBuffer*);


void
_intRingBufferInit(_intRingBuffer*, char);

#if 0
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
_intRingBuffer*
_intRingBufferInitSec(_intRingBufferSample*, _aaxCodec**, unsigned char, float, enum aaxFormat, float, char);

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
_intRingBuffer*
_intRingBufferInitSamples(_intRingBufferSample*, _aaxCodec**, unsigned char, float, enum aaxFormat, unsigned int, char);

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
_intRingBuffer*
_intRingBufferInitBytes(_intRingBufferSample*, _aaxCodec**, unsigned char, float, enum aaxFormat, unsigned int, char);
#endif

/**
 * Reference another RingBuffer, possibly sharing it's sample data.
 *
 * @param rb the ringbuffer to reference
 *
 * returns the newly created reference ringbuffer
 */
_intRingBuffer*
_intRingBufferReference(const _intRingBuffer*);
int 
_intRingBufferReferenceSetup(_intRingBuffer*, const _intRingBuffer*);

/**
 * Duplicate a RingBuffer based on another ringbuffer.
 *
 * @param rb the ringbuffer to duplicate
 * @param copy, true if the sample data needs to be copied, false otherwise
 * @param dde, true if the delay effects buffers needs to be copied
 *
 * returns the newly created ringbuffer
 */
_intRingBuffer*
_intRingBufferDuplicate(const _intRingBuffer*, char, char);
int
_intRingBufferDuplicateSetup(_intRingBuffer*, const _intRingBuffer*, char, char);

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
_intRingBufferFillNonInterleaved(_intRingBuffer*, const void*, unsigned, char);

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
_intRingBufferFillInterleaved(_intRingBuffer*, const void*, unsigned, char);

/**
 * Get the interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample 
 * @param data a memory block large enough for the interleaved tracks.
 * @param fact rersampling factor
 */
void
_intRingBufferGetDataInterleaved(_intRingBuffer*, void*, unsigned int, float);

/**
 * Get the interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample 
 * @param fact rersampling factor
 *
 * returns a memory block containing the interleaved tracks.
 */

void*
_intRingBufferGetDataInterleavedMalloc(_intRingBuffer*, float);

/**
 * Get the non-interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample
 * @param data a memory block large enough for the non-interleaved tracks.
 * @param fact rersampling factor
 */
void
_intRingBufferGetDataNonInterleaved(_intRingBuffer*, void*, unsigned int, float);

/**
 * Get the interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample 
 * @param fact rersampling factor
 *
 * returns a memory block containing the non-interleaved tracks.
 */

void*
_intRingBufferGetDataNonInterleavedMalloc(_intRingBuffer*, float);

_aaxCodec* 
_intRingBufferGetCodecFn(_intRingBuffer*);
void**
_intRingBufferGetTrackBufferPtr(_intRingBuffer*);
void**
_intRingBufferGetScratchBufferPtr(_intRingBuffer*);

/**
 * Clear the sound data of the ringbuffer
 *
 * @param rb the ringbuffer to clear
 */
void
_intRingBufferClear(_intRingBuffer*);

/**
 * Remove the ringbuffer and all it's tracks from memory.
 *
 * @param rb the ringbuffer to delete
 */
void
_intRingBufferDelete(_intRingBuffer*);

/**
 * Set the playback position of the ringbuffer to zero sec.
 *
 * @param rb the ringbuffer to rewind
 */
void
_intRingBufferRewind(_intRingBuffer*);

/**
 * Set the playback position of the ringbuffer to the end of the buffer
 *
 * @param rb the ringbuffer to forward
 */
void
_intRingBufferForward(_intRingBuffer*);

/**
 * Starts playback of the ringbuffer.
 *
 * @param rb the ringbuffer to stop
 */
void
_intRingBufferStart(_intRingBuffer*);

/**
 * Stop playback of the ringbuffer.
 *
 * @param rb the ringbuffer to stop
 */
void
_intRingBufferStop(_intRingBuffer*);

/**
 * Imediately finish playback of the ringbuffer.
 *
 * @param rb the ringbuffer to stop
 */
void
_intRingBufferFinish(_intRingBuffer*);

/**
 * Starts streaming playback of the ringbuffer.
 *
 * @param rb the ringbuffer to stop
 */
void
_intRingBufferStartStreaming(_intRingBuffer*);

/**
 * Copy the delay effetcs buffers from one ringbuffer to the other.
 * 
 * @param dest destination ringbuffer
 * @param src source ringbuffer
 */
void
_intRingBufferCopyDelyEffectsData(_intRingBuffer*, const _intRingBuffer*);

int _intRingBufferSetParamf(_intRingBuffer*, enum _intRingBufferParam, float);
int _intRingBufferSetParami(_intRingBuffer*, enum _intRingBufferParam, unsigned int);
float _intRingBufferGetParamf(const _intRingBuffer*, enum _intRingBufferParam);
unsigned int _intRingBufferGetParami(const _intRingBuffer*, enum _intRingBufferParam);
int _intRingBufferSetFormat(_intRingBuffer*, _aaxCodec **, enum aaxFormat);

void _intRingBufferDelaysAdd(void**, float, unsigned int, const float*, const float*, unsigned int, float, float, float);
void _intRingBufferDelaysRemove(void**);
// void _intRingBufferDelayRemoveNum(_intRingBuffer*, unsigned int);

unsigned int _intRingBufferGetSource();
unsigned int _intRingBufferPutSource();
unsigned int _intRingBufferGetNoSources();
unsigned int _intRingBufferSetNoSources(unsigned int);


/* --------------------------------------------------------------------------*/

typedef struct {
   unsigned char bits;
   enum aaxFormat format;
} _intFormat_t;

extern _aaxDriverCompress _aaxProcessCompression;
extern _intFormat_t _intRingBufferFormat[AAX_FORMAT_MAX];

extern _aaxCodec* _intRingBufferCodecs[];
extern _aaxCodec* _intRingBufferCodecs_w8s[];

extern _intRingBufferDistFunc* _intRingBufferDistanceFunc[];
extern _intRingBufferDistFunc* _intRingBufferALDistanceFunc[];
extern _intRingBufferPitchShiftFunc* _intRingBufferDopplerFunc[];

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

float _intRingBufferLFOGetSine(void*, const void*, unsigned, unsigned int);
float _intRingBufferLFOGetSquare(void*, const void*, unsigned, unsigned int);
float _intRingBufferLFOGetTriangle(void*, const void*, unsigned, unsigned int);
float _intRingBufferLFOGetSawtooth(void*, const void*, unsigned, unsigned int);
float _intRingBufferLFOGetFixedValue(void*, const void*, unsigned,unsigned int);
float _intRingBufferLFOGetGainFollow(void*, const void*, unsigned, unsigned int);
float _intRingBufferLFOGetCompressor(void*, const void*, unsigned, unsigned int);
float _intRingBufferLFOGetPitchFollow(void*, const void*, unsigned, unsigned int);

float _intRingBufferEnvelopeGet(_intRingBufferEnvelopeInfo*, char);

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


void _intRingBufferCreateHistoryBuffer(void**, int32_t*[_AAX_MAX_SPEAKERS], float, int, float);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_INT_RINGBUFFER_H*/

