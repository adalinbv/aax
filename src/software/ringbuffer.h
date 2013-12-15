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

#include "cpu2d/rbuf_effects.h"
#include "devices.h"
#include "driver.h"

#define BYTE_ALIGN		1
#define CUBIC_SAMPS		4

#define DEFAULT_IMA4_BLOCKSIZE		36
#define IMA4_SMP_TO_BLOCKSIZE(a)	(((a)/2)+4)
#define BLOCKSIZE_TO_SMP(a)		((a) > 1) ? (((a)-4)*2) : 1

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

enum _aaxRingBufferParam
{
   RB_VOLUME = 0,
   RB_VOLUME_MIN,
   RB_VOLUME_MAX,
   RB_AGC_VALUE,
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
   RB_NO_SAMPLES_AVAIL,
   RB_DDE_SAMPLES,
   RB_IS_PLAYING,

   RB_PEAK_VALUE = 0x1000,
   RB_PEAK_VALUE_MAX = RB_PEAK_VALUE+_AAX_MAX_SPEAKERS,
   RB_AVERAGE_VALUE,
   RB_AVERAGE_VALUE_MAX = RB_AVERAGE_VALUE+_AAX_MAX_SPEAKERS
};

enum _aaxRingBufferState
{
   RB_CLEARED = 0,
   RB_REWINDED,
   RB_FORWARDED,
   RB_STARTED,
   RB_STOPPED,
   RB_STARTED_STREAMING,

   RB_IS_VALID
};

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

} _aaxRingBufferSample;

typedef struct		/* playback related information about the sample*/
{
    _aaxRingBufferSample* sample;		/* shared, constat settings */

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

#ifndef NDEBUG
    void *parent;
#endif

} _aaxRingBufferData;

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
   _aaxRingBufferFilterInfo filter[MAX_STEREO_FILTER];
   _aaxRingBufferFilterInfo effect[MAX_STEREO_EFFECT];

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

} _aaxRingBuffer2dProps ALIGN16C;

/*forwrad declaration */
typedef struct _aaxRingBuffer_t _aaxRingBuffer;

/**
 * Initialize a new sound buffer that holds no data.
 * The default values are for a single, 16 bits per sample track at 44100Hz.
 *
 * @param dde specifies if memory needs to be allocated for the delay effects
 *            buffer prior to the data section.
 *
 * returns the new ringbuffer or NULL if an error occured.
 */
struct _aaxRingBuffer_t *
_aaxRingBufferCreate(float);


/**
 * Remove the ringbuffer and all it's tracks from memory.
 *
 * @param rb the ringbuffer to delete
 */
void
_aaxRingBufferDestroy(void*);


/**
 * Initialize the sound buffer
 *
 * @param rb the ringbuffer to reference
 * @param add_scratchbuf set to something other than 0 to add scratchbuffers
 */
typedef void
_aaxRingBufferInitFn(_aaxRingBuffer*, char);


/**
 * Reference another RingBuffer, possibly sharing it's sample data.
 *
 * @param rb the ringbuffer to reference
 *
 * returns the newly created reference ringbuffer
 */
typedef _aaxRingBuffer*
_aaxRingBufferReferenceFn(_aaxRingBuffer*);


/**
 * Duplicate a RingBuffer based on another ringbuffer.
 *
 * @param rb the ringbuffer to duplicate
 * @param copy, true if the sample data needs to be copied, false otherwise
 * @param dde, true if the delay effects buffers needs to be copied
 *
 * returns the newly created ringbuffer
 */
typedef _aaxRingBuffer*
_aaxRingBufferDuplicateFn(_aaxRingBuffer*, char, char);


/**
 * Fill the buffer with sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample
 * @param data a block of memory holding the interleaved sound sample
 *             the number of tracks is defined within rb
 * @param blocksize Number of samples per block for each channel.
 * @param looping boolean value defining wether this sound should loop
 */
typedef void
_aaxRingBufferFillInterleavedFn(_aaxRingBufferData*, const void*, unsigned, char);


/**
 * Get the interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample 
 * @param data a memory block large enough for the interleaved tracks.
 * @param fact rersampling factor
 */
typedef void
_aaxRingBufferGetDataInterleavedFn(_aaxRingBufferData*, void*, unsigned int, int, float);


/**
 * Get the interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample 
 * @param fact rersampling factor
 *
 * returns a memory block containing the interleaved tracks.
 */
typedef void*
_aaxRingBufferGetDataInterleavedMallocFn(_aaxRingBufferData*, int, float);


/**
 * Get the pointer to the interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample 
 *
 * returns a pointer to a memory block containing the interleaved tracks.
 */

typedef const void**
_aaxRingBufferGetDataInterleavedPtrFn(_aaxRingBufferData*);


/**
 * Fill the buffer with sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample
 * @param track array of pointers to the separate sound tracks
 *              the number of tracks is defined within rb
 * @param blocksize Number of samples per block for each channel.
 * @param looping boolean value defining wether this sound should loop
 */
typedef void
_aaxRingBufferFillNonInterleavedFn(_aaxRingBufferData*, const void*, unsigned, char);


/**
 * Get the non-interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample
 * @param data a memory block large enough for the non-interleaved tracks.
 * @param fact rersampling factor
 */
typedef void
_aaxRingBufferGetDataNonInterleavedFn(_aaxRingBufferData*, void*, unsigned int, int, float);


/**
 * Get the interleaved sound data.
 *
 * @param rb the ringbuffer which will hold the sound sample 
 * @param fact rersampling factor
 *
 * returns a memory block containing the non-interleaved tracks.
 */

typedef void*
_aaxRingBufferGetDataNonInterleavedMallocFn(_aaxRingBufferData*, int, float);


/**
 * Get the pointer to the scratch buffer.
 *
 * @param rb the ringbuffer for the scratchbuffer.
 *
 * returns a pointer to a memory block containing scratch memory area.
 */

typedef void**
_aaxRingBufferGetScratchBufferPtrFn(_aaxRingBufferData*);

/**
 * Copy the delay effetcs buffers from one ringbuffer to the other.
 * 
 * @param dest destination ringbuffer
 * @param src source ringbuffer
 */
typedef void
_aaxRingBufferCopyDelyEffectsDataFn(_aaxRingBufferData*, const _aaxRingBufferData*);


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
typedef int
_aaxRingBufferMixMNFn(_aaxRingBufferData*, _aaxRingBufferData*, _aaxRingBuffer2dProps*, _aaxRingBuffer2dProps*, unsigned char, unsigned int);


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
typedef int
_aaxRingBufferMix1NFn(_aaxRingBufferData*, _aaxRingBufferData*, enum aaxRenderMode, _aaxRingBuffer2dProps*, _aaxRingBuffer2dProps*, unsigned char, unsigned char, unsigned int);


/**
 *
 */
typedef void _aaxRingBufferSetStateFn(_aaxRingBuffer*, enum _aaxRingBufferState);
typedef int _aaxRingBufferGetStateFn(_aaxRingBuffer*, enum _aaxRingBufferState);


/**
 *
 */
typedef int _aaxRingBufferSetParamfFn(_aaxRingBuffer*, enum _aaxRingBufferParam, float);
typedef int _aaxRingBufferSetParamiFn(_aaxRingBuffer*, enum _aaxRingBufferParam, unsigned int);
typedef float _aaxRingBufferGetParamfFn(const _aaxRingBuffer*, enum _aaxRingBufferParam);
typedef unsigned int _aaxRingBufferGetParamiFn(const _aaxRingBuffer*, enum _aaxRingBufferParam);


/**
 *
 */
typedef int _aaxRingBufferSetFormatFn(_aaxRingBuffer*, _aaxCodec **, enum aaxFormat);

/*
 *
 */
typedef int
_aaxRingBufferDataClearFn(_aaxRingBuffer*);
typedef int
_aaxRingBufferDataMixDataFn(_aaxRingBuffer*, _aaxRingBuffer*, _aaxRingBufferLFOInfo*);
typedef int
_aaxRingBufferDataMultiplyFn(_aaxRingBuffer*, size_t, size_t, float);
typedef int
_aaxRingBufferDataMixWaveformFn(_aaxRingBuffer*, enum aaxWaveformType, float, float, float);
typedef int
_aaxRingBufferDataMixNoiseFn(_aaxRingBuffer*, enum aaxWaveformType, float, float, float, char);




typedef struct _aaxRingBuffer_t
{
   _aaxRingBufferData *id;

   _aaxRingBufferInitFn *init;
   _aaxRingBufferReferenceFn *reference;
   _aaxRingBufferDuplicateFn *duplicate;

   _aaxRingBufferSetFormatFn *set_format;

   _aaxRingBufferSetStateFn *set_state;
   _aaxRingBufferGetStateFn *get_state;

   _aaxRingBufferSetParamfFn *set_paramf;
   _aaxRingBufferSetParamiFn *set_parami;
   _aaxRingBufferGetParamfFn *get_paramf;
   _aaxRingBufferGetParamiFn *get_parami;

   _aaxRingBufferMixMNFn *mix2d;
   _aaxRingBufferMix1NFn *mix3d;

   _aaxRingBufferFillNonInterleavedFn *set_data_noninterleaved;
   _aaxRingBufferGetDataInterleavedPtrFn *get_dataptr_noninterleaved;

   _aaxRingBufferFillInterleavedFn *set_data_interleaved;
   _aaxRingBufferGetDataInterleavedFn *get_data_interleaved;

   _aaxRingBufferGetScratchBufferPtrFn *get_scratch;

   /* data mangling */
   _aaxRingBufferDataClearFn *data_clear;
   _aaxRingBufferDataMixDataFn *data_mix;
   _aaxRingBufferDataMultiplyFn *data_multiply;
   _aaxRingBufferDataMixWaveformFn *data_mix_waveform;
   _aaxRingBufferDataMixNoiseFn *data_mix_noise;

   // TODO: Get rid of this, it's used only once in sofwtare/frame.c
   _aaxRingBufferCopyDelyEffectsDataFn *copy_effectsdata;

} _aaxRingBuffer;

/* --------------------------------------------------------------------------*/

/** CODECs */
typedef struct {
   unsigned char bits;
   enum aaxFormat format;
} _aaxFormat_t;

extern _aaxFormat_t _aaxRingBufferFormat[AAX_FORMAT_MAX];

extern _aaxCodec* _aaxRingBufferCodecs[];
extern _aaxCodec* _aaxRingBufferCodecs_w8s[];

void _aaxProcessCodec(int32_t*, void*, _aaxCodec*, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned char, char);


/** MIXER */
_aaxRingBufferMixMNFn _aaxRingBufferMixMulti16;
_aaxRingBufferMix1NFn _aaxRingBufferMixMono16;

int32_t**_aaxProcessMixer(_aaxRingBufferData*, _aaxRingBufferData*,  _aaxRingBuffer2dProps *, float, unsigned int*, unsigned int*, unsigned char, unsigned int);

/** BUFFER */
void bufConvertDataToPCM24S(void*, void*, unsigned int, enum aaxFormat);
void bufConvertDataFromPCM24S(void*, void*, unsigned int, unsigned int, enum aaxFormat, unsigned int);

void _bufferMixWhiteNoise(void**, unsigned int, char, int, float, float, unsigned char);
void _bufferMixPinkNoise(void**, unsigned int, char, int, float, float, float, unsigned char);
void _bufferMixBrownianNoise(void**, unsigned int, char, int, float, float, float, unsigned char);
void _bufferMixSineWave(void**, float, char, unsigned int, int, float, float);
void _bufferMixSquareWave(void**, float, char, unsigned int, int, float, float);
void _bufferMixTriangleWave(void**, float, char, unsigned int, int, float, float);
void _bufferMixSawtooth(void**, float, char, unsigned int, int, float, float);
void _bufferMixImpulse(void**, float, char, unsigned int, int, float, float);

void _aaxRingBufferCreateHistoryBuffer(void**, int32_t*[_AAX_MAX_SPEAKERS], float, int, float);


#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_RINGBUFFER_H*/

