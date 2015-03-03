/*
 * Copyright 2005-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
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
#include <objects.h>

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
   RB_IS_MIXER_BUFFER,

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

   RB_RW_MAX 
};


/**
 * LFO: Low Frequency Oscillator
 */
#define _MAX_ENVELOPE_STAGES		6
#define ENVELOPE_FOLLOW_STEP_CVT(a)	_MINMAX(-0.1005f+powf((a), 0.25f)/3.15f, 0.0f, 1.0f)

typedef float _aaxRingBufferLFOGetFn(void *, void*, const void*, unsigned, size_t);
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
} _aaxRingBufferLFOData;

typedef struct
{
   float value;
   uint32_t pos;
   unsigned int stage, max_stages;
   float step[_MAX_ENVELOPE_STAGES];
   uint32_t max_pos[_MAX_ENVELOPE_STAGES];
   float ctr;
} _aaxRingBufferEnvelopeData;


/**
 * Filtes and Effects
 */

#define _AAX_MAX_DELAYS         8
#define _AAX_MAX_LOOPBACKS      8
#define _AAX_MAX_EQBANDS        8

typedef struct
{
   float gain;
   size_t sample_offs[_AAX_MAX_SPEAKERS];
} _aaxRingBufferDelayData;

typedef struct
{
   float coeff[4];
   float Q, k, fs, lf_gain, hf_gain;
   float freqfilter_history[_AAX_MAX_SPEAKERS][2];
   _aaxRingBufferLFOData *lfo;
} _aaxRingBufferFreqFilterData;

typedef struct
{
   _aaxRingBufferFreqFilterData band[_AAX_MAX_EQBANDS];
} _aaxRingBufferEqualizerData;

typedef struct
{
   _aaxRingBufferLFOData lfo;
   _aaxRingBufferDelayData delay;

   int32_t* delay_history[_AAX_MAX_SPEAKERS];
   void* history_ptr;

   /* temporary storage, track specific. */
   size_t curr_noffs[_AAX_MAX_SPEAKERS];
   size_t curr_coffs[_AAX_MAX_SPEAKERS];
   size_t curr_step[_AAX_MAX_SPEAKERS];

   char loopback;
} _aaxRingBufferDelayEffectData;

typedef struct
{
   /* reverb*/
   float gain;
   unsigned int no_delays;
   _aaxRingBufferDelayData delay[_AAX_MAX_DELAYS];

    unsigned int no_loopbacks;
    _aaxRingBufferDelayData loopback[_AAX_MAX_LOOPBACKS];
    int32_t* reverb_history[_AAX_MAX_SPEAKERS];
    void* history_ptr;

    _aaxRingBufferFreqFilterData *freq_filter;

} _aaxRingBufferReverbData;



/**
 * Function type definitions
 */

/** forwrad declaration */
typedef struct _aaxRingBuffer_t __aaxRingBuffer;

/**
 * Initialize a new audio ringbuffer that holds no data.
 * The default values are for a single, 16 bits per sample track at 44100Hz.
 *
 * @param dde specifies if memory needs to be allocated for the delay effects
 *            buffer prior to the data section.
 * @param mode rendering mode to use for this ringbuffer.
 *
 * returns the new ringbuffer or NULL if an error occured.
 */
struct _aaxRingBuffer_t *
_aaxRingBufferCreate(float, enum aaxRenderMode);


/**
 * Remove all ringbuffer associated data from memory.
 *
 * @param rb the ringbuffer to delete
 */
typedef void
_aaxRingBufferDestroyFn(struct _aaxRingBuffer_t*);

void
_aaxRingBufferFree(void*);


/**
 * Initialize the ringbuffer to a  usable state.
 *
 * @param rb the ringbuffer to initialize.
 * @param add_scratchbuf set to something other than 0 to add scratchbuffers
 */
typedef void
_aaxRingBufferInitFn(struct _aaxRingBuffer_t*, char);


/**
 * Reference another RingBuffer, sharing it's sample data.
 *
 * @param rb the ringbuffer to reference
 *
 * returns the newly created referencing ringbuffer
 */
typedef struct _aaxRingBuffer_t*
_aaxRingBufferReferenceFn(struct _aaxRingBuffer_t*);


/**
 * Duplicate a RingBuffer based on another ringbuffer.
 *
 * @param rb the ringbuffer to duplicate
 * @param copy, true if the sample data needs to be copied, false otherwise
 * @param dde, true if the delay effects buffers needs to be copied
 *
 * returns the newly created ringbuffer
 */
typedef struct _aaxRingBuffer_t*
_aaxRingBufferDuplicateFn(struct _aaxRingBuffer_t*, char, char);


/**
 * Request access to the ringbuffer track data.
 *
 * From the outside tingbuffer audio data is always 24-bit 32-bit aligned,
 * one buffer per track. Internally the ringbuffer can be anything and might
 * not even be in internal memory but the code assures the proper format 
 * and access to the latest data.
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
 * returns an array of pointers that locate the non-interleaved tracks.
 */

typedef int32_t**
_aaxRingBufferGetTracksPtrFn(struct _aaxRingBuffer_t*, enum _aaxRingBufferMode);


/**
 * Release access to the ringbuffer track data gain.
 *
 * This will unlock the memory area again if the data was locked by the
 * function that reqests access to the data.
 *
 * For write or rw mode the data might get copied back to dedicated hadrware
 * during this function call.
 *
 * @param rb the ringbuffer which holds the sound data.
 * 
 * returns AAX_TRUE on success or AAX_FALSE otherwise.
 */
typedef int
_aaxRingBufferReleaseTracksPtrFn(struct _aaxRingBuffer_t*);


/**
 * Get the pointer to the scratch buffer.
 * A scratch buffer is a memory location that has no real function but could
 * be used by the ringbuffer code or the application to store temporary data.
 * Allocating a scratch buffer in advance saves a lot of malloc/free at runtime.
 *
 * @param rb the ringbuffer for the scratchbuffer.
 *
 * returns an array of pointers to a memory block containing a scratch memory
 * area, one for each track.
 */

typedef void**
_aaxRingBufferGetScratchBufferPtrFn(struct _aaxRingBuffer_t*);

/**
 * Copy the delay effetcs data from one ringbuffer to another.
 * 
 * @param dest destination ringbuffer
 * @param src source ringbuffer
 */
typedef void
_aaxRingBufferCopyDelyEffectsDataFn(struct _aaxRingBuffer_t*, const struct _aaxRingBuffer_t*);


/**
 * Multi channel ringbuffer mixer.
 *
 * This function does all the preparations like audio format conversion to the
 * internal format of the mixer, resampling in case of different frequences or
 * pitch settings, gain and pitch calculation and status updates.
 * Actual rendering is done in the struct _aaxRingBuffer_tMixMNFn* call;
 *
 * @param drb multi track destination buffer
 * @param srb multi track source buffer
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
_aaxRingBufferMixStereoFn(struct _aaxRingBuffer_t*, struct _aaxRingBuffer_t*, _aax2dProps*, _aax2dProps*, unsigned char, unsigned int);

/**
 * Single channel ringbuffer mixer.
 *
 * This function does all the preparations like audio format conversion to the
 * internal format of the mixer, resampling in case of different frequences or
 * pitch settings, gain and pitch calculation and status updates.
 * Actual rendering is done in the _aaxRingBufferMix1NFn* call;
 *
 * @param drb multi track destination buffer
 * @param srb single (or milti) track source buffer
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
_aaxRingBufferMixMonoFn(struct _aaxRingBuffer_t*, struct _aaxRingBuffer_t*, _aax2dProps*, _aax2dProps*, unsigned char, unsigned char, unsigned int);


/**
 * Functions to get or set the state of the ringbuffer.
 */

/**
 * Set the ringbuffer to a different state
 *
 * @param rb ringbuffer object for which to alter the internal state
 * @param state new ringbuffer state
 */
typedef void
_aaxRingBufferSetStateFn(struct _aaxRingBuffer_t*, enum _aaxRingBufferState);

/**
 * Get the ringbuffer state
 *
 * @param rb ringbuffer object for which to alter the internal state
 * @param state which state to get, currently only RB_IS_VALID
 *
 * returns the state if valid or 0 otherwise.
 */
typedef int
_aaxRingBufferGetStateFn(struct _aaxRingBuffer_t*, enum _aaxRingBufferState);


/**
 * Functions to get or set internal ringbuffer parameters.
 */

/**
 * Set the required format for the ringbuffer
 *
 * @param rb ringbuffer to set
 * @param format internal fromat for the ringbuffer data.
 *
 * returns AAX_TRUE if successful, AAX_FALSE otherwise.
 *
 * Note: the format can be retrieved by calling _aaxRingBufferGetParami with
 *       param set to RB_FORMAT
 */
typedef int
_aaxRingBufferSetFormatFn(struct _aaxRingBuffer_t*, enum aaxFormat, int);

/**
 * Set a single integer ringbuffer parameter
 *
 * @param rb ringbuffer to set the parameter for
 * @param param the parameter to set
 * @param val the value to set the parameter to
 *
 * returns AAX_TRUE if successful, AAX_FALSE otherwise.
 */
typedef int
_aaxRingBufferSetParamiFn(struct _aaxRingBuffer_t*, enum _aaxRingBufferParam, unsigned int);

/**
 * Set a single floating-point ringbuffer parameter
 *
 * @param rb ringbuffer to set the parameter for
 * @param param the parameter to set
 * @param val the value to set the parameter to
 *
 * returns AAX_TRUE if successful, AAX_FALSE otherwise.
 */
typedef int
_aaxRingBufferSetParamfFn(struct _aaxRingBuffer_t*, enum _aaxRingBufferParam, float);

/**
 * Get the value of an integer ringbuffer parameter
 *
 * @param rb ringbuffer to set the parameter for
 * @param param the parameter to get
 *
 * returns the value of the parameter.
 */
typedef unsigned int
_aaxRingBufferGetParamiFn(const struct _aaxRingBuffer_t*, enum _aaxRingBufferParam);

/**
 * Get the value of a floating-point ringbuffer parameter
 *
 * @param rb ringbuffer to set the parameter for
 * @param param the parameter to get
 *
 * returns the value of the parameter.
 */
typedef float
_aaxRingBufferGetParamfFn(const struct _aaxRingBuffer_t*, enum _aaxRingBufferParam);


/*
 * Functions to let the ringbuffer alter the audio data in the tracks buffer.
 */

/**
 * Set the ringbuffer audio data to silence
 *
 * @param rb ringbuffer to silence the audio data
 *
 * returns AAX_TRUE if successful, AAX_FALSE otherwise.
 */
typedef int
_aaxRingBufferDataClearFn(struct _aaxRingBuffer_t*);

/**
 * Mix the audio data of two ringbuffers
 *
 * @param drb destination buffer which holds the mixed audio data afterwards
 * @param srb source ringbuffer to mix with the destination ringbuffer
 * @param lfo optional gain-envelope information, NULL if unused
 *
 * returns AAX_TRUE if successful, AAX_FALSE otherwise.
 */
typedef int
_aaxRingBufferDataMixDataFn(struct _aaxRingBuffer_t*, struct _aaxRingBuffer_t*, _aaxRingBufferLFOData*);

/**
 * Alter the gain of the ringbuffer audio data
 *
 * @param rb ringbuffer to adjust the gain for
 * @param offs starting position in samples
 * @param no_samples number of samples to process
 * @param factor gainn multiplication factor
 *
 * returns AAX_TRUE if successful, AAX_FALSE otherwise.
 */
typedef int
_aaxRingBufferDataMultiplyFn(struct _aaxRingBuffer_t*, size_t, size_t, float);

/**
 * Mix a waveform type from the waveform generator with existing data
 *
 * @param rb ringbuffer to mix the wavefrom with
 * @param type waveform type to mic with the ringbuffer
 * @param pitch pitch of the waveform (in relation to the ringbuffer frequency)
 * @param ratio volume mixing ratio: 1.0 is overwrite, 0.0f is mix nothing
 * @param phase phase of the waveform, in radians
 *
 * returns AAX_TRUE if successful, AAX_FALSE otherwise.
 */
typedef int
_aaxRingBufferDataMixWaveformFn(struct _aaxRingBuffer_t*, enum aaxWaveformType, float, float, float);

/**
 * Mix a noise type from the waveform generator with existing data
 *
 * @param rb ringbuffer to mix the wavefrom with
 * @param type noise type to mic with the ringbuffer
 * @param pitch pitch of the waveform (in relation to the ringbuffer frequency)
 * @param ratio volume mixing ratio: 1.0 is overwrite, 0.0f is mix nothing
 * @param dc duty-cycle of the noise.
 * @param skip 0 for non-static and 100 for highly static noise
 *
 * returns AAX_TRUE if successful, AAX_FALSE otherwise.
 */
typedef int
_aaxRingBufferDataMixNoiseFn(struct _aaxRingBuffer_t*, enum aaxWaveformType, float, float, float, float, char);

/**
 * Limit the audio data in the ringbuffer
 *
 * @param rb ringbuffer to limit
 * @param type type of limiter/compressor to use
 */
typedef void
_aaxRingBufferDataLimiterFn(struct _aaxRingBuffer_t*, enum _aaxLimiterType);



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

// protected:
   _aaxRingBufferDataClearFn *data_clear;
   _aaxRingBufferDataMixDataFn *data_mix;
   _aaxRingBufferDataMixWaveformFn *data_mix_waveform;
   _aaxRingBufferDataMixNoiseFn *data_mix_noise;
   _aaxRingBufferDataMultiplyFn *data_multiply;
   _aaxRingBufferDataLimiterFn *limit;

// private:	/* TODO: Get rid of these */
   _aaxRingBufferMixMonoFn *mix3d;
   _aaxRingBufferMixStereoFn *mix2d;
   _aaxRingBufferGetScratchBufferPtrFn *get_scratch;
   _aaxRingBufferCopyDelyEffectsDataFn *copy_effectsdata;

} _aaxRingBuffer;

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_RINGBUFFER_H*/

