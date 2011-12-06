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

#ifndef _AAX_RINGBUFFER_H
#define _AAX_RINGBUFFER_H 1

#include <aax.h>
#include <base/geometry.h>
#include <base/types.h>

#include "devices.h"
#include "driver.h"

#define BYTE_ALIGN		1

#define _AAX_MAX_DELAYS		8
#define _AAX_MAX_LOOPBACKS	3
#define _AAX_MAX_FILTERS	2
#define _MAX_ENVELOPE_STAGES	6
#define _AAX_MAX_EQBANDS	8
#define _MAX_SLOTS		3
#define CUBIC_SAMPS		4
#define DELAY_EFFECTS_TIME	0.070f
#if 0
#define NO_DELAY_EFFECTS_TIME
#undef DELAY_EFFECTS_TIME
#define DELAY_EFFECTS_TIME	0.0f
#endif


#define DEFAULT_IMA4_BLOCKSIZE		36
#define IMA4_SMP_TO_BLOCKSIZE(a)	(((a)/2)+4)
#define BLOCKSIZE_TO_SMP(a)		((a) > 1) ? (((a)-4)*2) : 1


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
    TREMOLO_FILTER,
    TIMED_GAIN_FILTER,
    FREQUENCY_FILTER,
    MAX_STEREO_FILTER,

    /* 3d filters */
    DISTANCE_FILTER = 0,	/* distance attewnuation */
    ANGULAR_FILTER,		/* audio cone support    */
    MAX_3D_FILTER,

    /* stereo effects */
    PITCH_EFFECT = 0,
    VIBRATO_EFFECT,
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
    VIBRATO_DEFINED		= 0x00000020,

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

#define PITCH_CHANGE			(PITCH_CHANGED | VIBRATO_DEFINED)
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
#define _PROP_VIBRATO_SET_DEFINED(q)	((q)->state |= VIBRATO_DEFINED)

#define _PROP_PITCH_CLEAR_CHANGED(q)	((q)->state &= ~PITCH_CHANGED)
#define _PROP_GAIN_CLEAR_CHANGED(q)	((q)->state &= ~GAIN_CHANGED)
#define _PROP_DIST_CLEAR_CHANGED(q)	((q)->state &= ~DIST_CHANGED)
#define _PROP_MTX_CLEAR_CHANGED(q)	((q)->state &= ~MTX_CHANGED)
#define _PROP_CONE_CLEAR_DEFINED(q)	((q)->state &= ~CONE_DEFINED)
#define _PROP_VIBRATO_CLEAR_DEFINED(q)	((q)->state &= ~VIBRATO_DEFINED)

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
#define _FILTER_GET_SLOT_DATA(F, s)	F->slot[s]->data
#define _FILTER_SET_SLOT(F, s, p, v)	F->slot[s]->param[p] = v
#define _FILTER_SET_SLOT_DATA(F, s, v)	F->slot[s]->data = v
 

#define _FILTER_GET(P, f, p)		P->filter[f].param[p]
#define _FILTER_GET_DATA(P, f)		P->filter[f].data
#define _FILTER_SET(P, f, p, v)		P->filter[f].param[p] = v
#define _FILTER_SET_DATA(P, f, v)	P->filter[f].data = v
#define _FILTER_COPY(P1, P2, f, p)	\
				P1->filter[f].param[p] = P2->filter[f].param[p]
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
    P->filter[f].data = F->slot[s]->data; F->slot[s]->data = ptr; } while (0);

#define _EFFECT_GET_SLOT		_FILTER_GET_SLOT
#define _EFFECT_GET_SLOT_DATA		_FILTER_GET_SLOT_DATA

#define _EFFECT_GET(P, f, p)		P->effect[f].param[p]
#define _EFFECT_GET_DATA(P, f)		P->effect[f].data
#define _EFFECT_SET(P, f, p, v)		P->effect[f].param[p] = v
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
    P->effect[f].data = F->slot[s]->data; F->slot[s]->data = ptr; } while (0);

typedef int16_t _oalRingBufferGetData(const void*, unsigned char, unsigned int);
typedef float _oalRingBufferDistFunc(float, float, float, float, float, float);
typedef float _oalRingBufferPitchShiftFunc(float, float, float);
typedef float _oalRingBufferLFOGetFunc(void*);

typedef struct
{
   float step;			/* step = frequency / refresh_rate */
   float value;			/* current value */
   float min, max;
   _oalRingBufferLFOGetFunc *get;
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
   unsigned int sample_offs;
} _oalRingBufferDelayInfo;

typedef struct
{
   _oalRingBufferLFOInfo lfo;
   _oalRingBufferDelayInfo delay;

   int32_t* reverb_history[_AAX_MAX_SPEAKERS];
   void* history_ptr;
} _oalRingBufferDelayEffectData;

typedef struct
{
   /* reverb*/
   unsigned int no_delays;
   _oalRingBufferDelayInfo delay[_AAX_MAX_DELAYS];

    unsigned int no_loopbacks;
    _oalRingBufferDelayInfo loopback[_AAX_MAX_LOOPBACKS];
    int32_t* reverb_history[_AAX_MAX_SPEAKERS];
    void* history_ptr;

} _oalRingBufferReverbData;

typedef struct
{
   float coeff[4];
   float k, lf_gain, hf_gain;
   float freqfilter_history[_AAX_MAX_SPEAKERS][2];
} _oalRingBufferFreqFilterInfo;

typedef struct
{
   _oalRingBufferFreqFilterInfo band[_AAX_MAX_EQBANDS];
} _oalRingBufferEqualizerInfo;

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
//  float delay_sec;
    float loop_start_sec;
    float loop_end_sec;

    unsigned int no_samples;		/* actual no. samples*/
    unsigned int no_samples_avail;	/* available no. samples*/
    unsigned int track_len_bytes;
    unsigned int dde_samples;

} _oalRingBufferSample;

typedef struct		/* playback related information about the sample*/
{
    _oalRingBufferSample* sample;
    _oalRingBufferReverbData* reverb;

    float elapsed_sec;
    float pitch_norm;
    float volume_norm;

    float curr_pos_sec;
    unsigned int curr_sample;

    int format;
    char playing;
    char stopped;
    char looping;
    char streaming;
    char add_dde;

} _oalRingBuffer;

typedef struct
{
   float param[4];
   void* data;		/* filter specific interal data structure */
} _oalRingBufferFilterInfo;

typedef struct
{
   /* modelview matrix and velocity */
   mtx4 matrix;
   vec4 velocity;

   int state;

   /* 3d filters and effects */
   _oalRingBufferFilterInfo filter[MAX_3D_FILTER];
   _oalRingBufferFilterInfo effect[MAX_3D_EFFECT];

} _oalRingBuffer3dProps ALIGN16;

typedef struct
{
      /* pos[0] position; -1.0 left,  0.0 center, 1.0 right */
      /* pos[1] position; -1.0 down,  0.0 center, 1.0 up    */
      /* pos[2] position; -1.0 front, 0.0 center, 1.0 back  */
   vec4 pos[_AAX_MAX_SPEAKERS];

      /* head[0] side delay sec    */
      /* head[1] up delay sec      */
      /* head[2] forward delay sec */
      /* head[3] up offset sec     */
   vec4 head;
   vec4 hrtf[2];
   vec4 hrtf_prev[2];

   /* stereo filters */
   _oalRingBufferFilterInfo filter[MAX_STEREO_FILTER];
   _oalRingBufferFilterInfo effect[MAX_STEREO_EFFECT];

   float prev_gain[_AAX_MAX_SPEAKERS];
   float prev_freq_fact;
   float delay_sec;

   struct {
      float pitch;
      float gain;
   } final;

} _oalRingBuffer2dProps ALIGN16;


typedef int
_oalRingBufferMix1NFunc(_oalRingBuffer*, _oalRingBuffer*,
                        _oalRingBuffer2dProps*, _oalRingBuffer2dProps*, 
                        float, unsigned char);
typedef int
_oalRingBufferMixMNFunc(_oalRingBuffer*, _oalRingBuffer*,
                        _oalRingBuffer2dProps*, _oalRingBuffer2dProps*,
                        float, float);


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
_oalRingBufferCreate(char);

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
 * @param copy, true if data needs to be copied, false otherwise
 *
 * returns the newly created ringbuffer
 */
_oalRingBuffer*
_oalRingBufferDuplicate(_oalRingBuffer*, char);

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
 */
void
_oalRingBufferGetDataInterleaved(_oalRingBuffer*, void*);

/**
 * Get the interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample 
 *
 * returns a memory block containing the interleaved tracks.
 */

void*
_oalRingBufferGetDataInterleavedMalloc(_oalRingBuffer*);

/**
 * Get the non-interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample
 * @param data a memory block large enough for the non-interleaved tracks.
 */
void
_oalRingBufferGetDataNonInterleaved(_oalRingBuffer*, void*);

/**
 * Get the interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample 
 *
 * returns a memory block containing the non-interleaved tracks.
 */

void*
_oalRingBufferGetDataNonInterleavedMalloc(_oalRingBuffer*);

/**
 * Clear the sound data of the ringbuffer
 *
 * @param rb the ringbuffer to clear
 */
void
_oalRingBufferClear(_oalRingBuffer*);

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
 * Get a single audio sample from the soundbuffer
 *
 * @param rb the audiobuffer
 * @param track_no get a sample from this track
 * @param pos sample offset position in seconds within the buffer
 */
int
_oalRingBufferGetSample(_oalRingBuffer*, unsigned char, float);

/**
 * Get the next audio sample from the soundbuffer
 * 
 * @param rb the audiobuffer
 * @param track_no get a sample from this track
 * @param step the number of seconds to skip
 */
int
_oalRingBufferGetNextSample(_oalRingBuffer*, unsigned char, float);

/**
 * M:N channel ringbuffer mixer.
 *
 * @param dest the pre initialized destination ringbuffer
 * @param src the source of the ringbuffer to mix with the destination buffer
 * @param pitch speed multiplier for the src frequency
 * @param volume volume multiplier
 *
 * returns 0 if the sound has stopped playing, 1 otherwise.
 */
extern _oalRingBufferMixMNFunc* _oalRingBufferMixMulti16;

/**
 * 1:N channel ringbuffer mixer.
 *
 * @param dest the pre initialized destination ringbuffer
 * @param src the source of the ringbuffer to mix with the destination buffer
 * @param p2d the structure holding the 3d positioning information
 *
 * returns 0 if the sound has stopped playing, 1 otherwise.
 */
extern _oalRingBufferMix1NFunc* _oalRingBufferMixMono16;


void
_oalRingBufferPrepare3d(_oalRingBuffer3dProps*, _oalRingBuffer3dProps*, const void*, const _oalRingBuffer2dProps*, void*);

char
_oalRingBufferTestPlaying(const _oalRingBuffer*);

void _oalRingBufferSetFrequency(_oalRingBuffer*, float);
void _oalRingBufferSetDuration(_oalRingBuffer*, float);
void _oalRingBufferSetTrackSize(_oalRingBuffer*, unsigned int);
void _oalRingBufferSetNoSamples(_oalRingBuffer*, unsigned int);
void _oalRingBufferSetBytesPerSample(_oalRingBuffer*, unsigned char);
void _oalRingBufferSetNoTracks(_oalRingBuffer*, unsigned char);
void _oalRingBufferSetFormat(_oalRingBuffer*, _aaxCodec**codecs, enum aaxFormat);
void _oalRingBufferSetLooping(_oalRingBuffer*, unsigned char);
void _oalRingBufferSetLoopPoints(_oalRingBuffer*, float, float);
void _oalRingBufferSetOffsetSec(_oalRingBuffer*, float);
void _oalRingBufferSetOffsetSamples(_oalRingBuffer*, unsigned int);

float _oalRingBufferGetFrequency(const _oalRingBuffer*);
float _oalRingBufferGetDuration(const _oalRingBuffer*);
unsigned int _oalRingBufferGetTrackSize(const _oalRingBuffer*);
unsigned int _oalRingBufferGetNoSamples(const _oalRingBuffer*);
unsigned char _oalRingBufferGetBytesPerSample(const _oalRingBuffer*);
unsigned char _oalRingBufferGetNoTracks(const _oalRingBuffer*);
enum aaxFormat _oalRingBufferGetFormat(const _oalRingBuffer*);
unsigned char _oalRingBufferGetLooping(const _oalRingBuffer*);
void _oalRingBufferGetLoopPoints(const _oalRingBuffer*, unsigned int*, unsigned int*);

float _oalRingBufferGetOffsetSec(const _oalRingBuffer*);
unsigned int _oalRingBufferGetOffsetSamples(const _oalRingBuffer*);

void _oalRingBufferDelaysAdd(_oalRingBuffer*, float, unsigned int, const float*, const float*, unsigned int, float, float);
void _oalRingBufferDelaysRemove(_oalRingBuffer*);
void _oalRingBufferDelayRemoveNum(_oalRingBuffer*, unsigned int);

int _oalRingBufferGetNoSources();
int _oalRingBufferSetNoSources(int);

void _oalRingBufferCreateHistoryBuffer(void*, float, int);

/* --------------------------------------------------------------------------*/

extern _aaxDriverCompress _aaxProcessCompression;
extern unsigned char _oalRingBufferFormatsBPS[AAX_FORMAT_MAX];

extern _aaxCodec* _oalRingBufferCodecs[];
extern _aaxCodec* _oalRingBufferCodecs_w8s[];

extern _oalRingBufferDistFunc* _oalRingBufferDistanceFunc[];
extern _oalRingBufferDistFunc* _oalRingBufferALDistanceFunc[];
extern _oalRingBufferPitchShiftFunc* _oalRingBufferDopplerFunc[];

void _aaxProcessCodec(int32_t*, void*, _aaxCodec*, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned char, char);
int32_t**_aaxProcessMixer(_oalRingBuffer*, _oalRingBuffer*,  _oalRingBuffer2dProps *, float, unsigned int*, unsigned int*);

float _oalRingBufferLFOGetSine(void*);
float _oalRingBufferLFOGetSquare(void*);
float _oalRingBufferLFOGetTriangle(void*);
float _oalRingBufferLFOGetSawtooth(void*);
float _oalRingBufferLFOGetFixedValue(void*);
float _oalRingBufferEnvelopeGet(_oalRingBufferEnvelopeInfo*, char);

void bufEffectsApply(int32_ptr, int32_ptr, int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, void*, void*, void*);
void bufFilterFrequency(int32_ptr, const int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, void*);
void bufEffectDistort(int32_ptr, const int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, void*);
void bufEffectDelay(int32_ptr, const int32_ptr, int32_ptr, unsigned int, unsigned int, unsigned int, void*);
void bufEffectReflections(int32_t*, const int32_ptr, unsigned int, unsigned int, unsigned int, const void*);
void bufEffectReverb(int32_t*, unsigned int, unsigned int, unsigned int, unsigned int, const void*);

void iir_compute_coefs(float, float, float*, float*);
void _oalRingBufferMixMonoSetRenderer(enum aaxRenderMode);
void bufCompressFast(void*, unsigned int, unsigned int);
void bufCompressHQ(void*, unsigned int, unsigned int);
void bufCompressValve(void*, unsigned int, unsigned int);

#endif /* !_AAX_RINGBUFFER_H*/

