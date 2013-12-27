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

#include <base/geometry.h>
#include <driver.h>

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
   RB_STATE,
   RB_NO_TRACKS,
   RB_NO_SAMPLES,
   RB_TRACKSIZE,
   RB_BYTES_SAMPLE,
   RB_OFFSET_SAMPLES,
   RB_INTERNAL_FORMAT,
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

enum _aaxRingBufferMode
{
   RB_NONE = 0x00,
   RB_READ = 0x01,
   RB_WRITE = 0x02,
   RB_RW = RB_READ | RB_WRITE,
};


/**
 * LFO: Low Frequency Oscillator
 */
#define _MAX_ENVELOPE_STAGES		6
#define ENVELOPE_FOLLOW_STEP_CVT(a)	_MINMAX(-0.1005f+powf((a), 0.25f)/3.15f, 0.0f, 1.0f)

typedef float _aaxRingBufferLFOGetFn(void*, const void*, unsigned, unsigned int);
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetSine;
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetSquare;
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetTriangle;
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetSawtooth;
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetFixedValue;
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetGainFollow;
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetCompressor;
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetPitchFollow;

typedef float _convert_fn(float, float);
_convert_fn _linear;
_convert_fn _compress;

typedef struct
{
   float f, min, max;
   float gate_threshold, gate_period;
   float step[_AAX_MAX_SPEAKERS];	/* step = frequency / refresh_rate */
   float down[_AAX_MAX_SPEAKERS];	/* compressor release rate         */
   float value[_AAX_MAX_SPEAKERS];	/* current value                   */
   float average[_AAX_MAX_SPEAKERS];	/* average value over time         */
   float compression[_AAX_MAX_SPEAKERS];	/* compression level       */
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


/** Filtes and Effects */
#define DELAY_EFFECTS_TIME	0.070f
#define REVERB_EFFECTS_TIME	0.700f

#define _AAX_MAX_DELAYS         8
#define _AAX_MAX_LOOPBACKS      8
#define _AAX_MAX_FILTERS        2
#define _AAX_MAX_EQBANDS        8
#if 0
#define NO_DELAY_EFFECTS_TIME
#undef DELAY_EFFECTS_TIME
#define DELAY_EFFECTS_TIME      0.0f
#endif

typedef float _aaxRingBufferPitchShiftFn(float, float, float);
extern _aaxRingBufferPitchShiftFn* _aaxRingBufferDopplerFn[];

typedef float _aaxRingBufferDistFn(float, float, float, float, float, float);
extern _aaxRingBufferDistFn* _aaxRingBufferDistanceFn[];
extern _aaxRingBufferDistFn* _aaxRingBufferALDistanceFn[];


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

enum _aaxCompressionType
{
    RB_COMPRESS_ELECTRONIC = 0,
    RB_COMPRESS_DIGITAL,
    RB_COMPRESS_VALVE,

    RB_COMPRESS_MAX
};

typedef struct
{
   float param[4];
   void* data;          /* filter specific interal data structure */
   int state;
} _aaxRingBufferFilterInfo;

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

void iir_compute_coefs(float, float, float*, float*, float);
void _aaxRingBufferDelaysAdd(void**, float, unsigned int, const float*, const float*, unsigned int, float, float, float);
void _aaxRingBufferDelaysRemove(void**);
void _aaxRingBufferCreateHistoryBuffer(void**, int32_t*[_AAX_MAX_SPEAKERS], float, int, float);


/** 3d properties in 2d */
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

   float dist_delay_sec;        /* time to keep playing after a stop request */
   float bufpos3dq;             /* distance delay queue buffer position      */

   struct {
      float pitch_lfo;
      float pitch;
      float gain_lfo;
      float gain;
   } final;

} _aaxRingBuffer2dProps ALIGN16C;



/** forwrad declaration */
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
_aaxRingBufferCreate(float, enum aaxRenderMode);


/**
 * Remove the ringbuffer and all it's tracks from memory.
 *
 * @param rb the ringbuffer to delete
 */
typedef void
_aaxRingBufferDestroyFn(_aaxRingBuffer*);

void
_aaxRingBufferFree(void*);


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
 * Request access to the ringbuffer track data.
 *
 * The function is free to lock the memory area, so it is adviced to make
 * access to the memory as short as possible.
 *
 * In cases where the actual ringbuffer data is located in dedicated hardware
 * it could be possible the data needs to be copied to main memory when calling
 * this function (RB_READ or RB_RW) or the data needs to be copied back to
 * the dedicated hardware when releasing it again (RB_WRITE or RB_RW).
 *
 * @param rb the ringbuffer which holds the sound data.
 * @param mode the acces method required: read, write or rw.
 *
 * returns a pointer to a memory block containing the interleaved tracks.
 */

typedef int32_t**
_aaxRingBufferGetTracksPtrFn(_aaxRingBuffer*, enum _aaxRingBufferMode);


/**
 * Release access to the ringbuffer track data gain.
 *
 * This will unlock the memory area again if the data was locked by the
 * function that reqests access to the data.
 *
 * @param rb the ringbuffer which holds the sound data.
 * 
 * returns AAX_TRUE on success or AAX_FALSE otherwise.
 */
typedef int
_aaxRingBufferReleaseTracksPtrFn(_aaxRingBuffer*);


/**
 * Get the pointer to the scratch buffer.
 *
 * @param rb the ringbuffer for the scratchbuffer.
 *
 * returns a pointer to a memory block containing scratch memory area.
 */

typedef void**
_aaxRingBufferGetScratchBufferPtrFn(_aaxRingBuffer*);

/**
 * Copy the delay effetcs buffers from one ringbuffer to the other.
 * 
 * @param dest destination ringbuffer
 * @param src source ringbuffer
 */
typedef void
_aaxRingBufferCopyDelyEffectsDataFn(_aaxRingBuffer*, const _aaxRingBuffer*);


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
_aaxRingBufferMixStereoFn(_aaxRingBuffer*, _aaxRingBuffer*, _aaxRingBuffer2dProps*, _aaxRingBuffer2dProps*, unsigned char, unsigned int);

typedef void
_aaxRingBufferMixMNFn(_aaxRingBuffer*, const _aaxRingBuffer*, const int32_ptrptr, _aaxRingBuffer2dProps*, unsigned int, unsigned int, float, float, float);



/**
 * 1:N channel ringbuffer mixer.
 *
 * @param dest multi track destination buffer
 * @param src single track source buffer
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
_aaxRingBufferMixMonoFn(_aaxRingBuffer*, _aaxRingBuffer*, _aaxRingBuffer2dProps*, _aaxRingBuffer2dProps*, unsigned char, unsigned char, unsigned int);

typedef void
_aaxRingBufferMix1NFn(_aaxRingBuffer*, const int32_ptrptr, _aaxRingBuffer2dProps*, unsigned char, unsigned int, unsigned int, float, float, float);



/**
 * Functions to get or set the state of the ringbuffer.
 */
typedef int
_aaxRingBufferSetFormatFn(_aaxRingBuffer*, _aaxCodec **, enum aaxFormat);
typedef void
_aaxRingBufferSetStateFn(_aaxRingBuffer*, enum _aaxRingBufferState);
typedef int
_aaxRingBufferGetStateFn(_aaxRingBuffer*, enum _aaxRingBufferState);


/**
 * Functions to get or set internal ringbuffer parameters.
 */
typedef int
_aaxRingBufferSetParamfFn(_aaxRingBuffer*, enum _aaxRingBufferParam, float);
typedef int
_aaxRingBufferSetParamiFn(_aaxRingBuffer*, enum _aaxRingBufferParam, unsigned int);
typedef float
_aaxRingBufferGetParamfFn(const _aaxRingBuffer*, enum _aaxRingBufferParam);
typedef unsigned int
_aaxRingBufferGetParamiFn(const _aaxRingBuffer*, enum _aaxRingBufferParam);


/*
 * Functions to let the ringbuffer alter the audio data in the tracks buffer.
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
typedef void
_aaxRingBufferDataCompressFn(_aaxRingBuffer*, enum _aaxCompressionType);



typedef struct _aaxRingBuffer_t
{
// public:
   void *handle;

   _aaxRingBufferInitFn *init;
   _aaxRingBufferReferenceFn *reference;
   _aaxRingBufferDuplicateFn *duplicate;
   _aaxRingBufferDestroyFn *destroy;

   _aaxRingBufferSetFormatFn *set_format;
   _aaxRingBufferSetStateFn *set_state;
   _aaxRingBufferGetStateFn *get_state;

   _aaxRingBufferSetParamfFn *set_paramf;
   _aaxRingBufferSetParamiFn *set_parami;
   _aaxRingBufferGetParamfFn *get_paramf;
   _aaxRingBufferGetParamiFn *get_parami;

   _aaxRingBufferGetTracksPtrFn *get_tracks_ptr;
   _aaxRingBufferReleaseTracksPtrFn *release_tracks_ptr;

   _aaxRingBufferMixMonoFn *mix3d;
   _aaxRingBufferMixStereoFn *mix2d;

// protected:
   _aaxRingBufferDataClearFn *data_clear;
   _aaxRingBufferDataMixDataFn *data_mix;
   _aaxRingBufferDataMixWaveformFn *data_mix_waveform;
   _aaxRingBufferDataMixNoiseFn *data_mix_noise;
   _aaxRingBufferDataMultiplyFn *data_multiply;
   _aaxRingBufferDataCompressFn *compress;

// private:	/* TODO: Get rid of these */
   _aaxRingBufferGetScratchBufferPtrFn *get_scratch;
   _aaxRingBufferCopyDelyEffectsDataFn *copy_effectsdata;

} _aaxRingBuffer;

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_RINGBUFFER_H*/

