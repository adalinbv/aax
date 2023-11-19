/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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
#include <base/databuffer.h>

#include <dsp/common.h>
#include <dsp/lfo.h>
#include <objects.h>

#define RB_MAX_TRACKS			(_AAX_MAX_SPEAKERS*4)
#define _AAX_SYNTH_MAX_WAVEFORMS	4
#define _AAX_SYNTH_MAX_HARMONICS	16

enum _aaxRingBufferParam
{
   RB_ALL_TRACKS = -1,
   RB_VOLUME = 0,
   RB_VOLUME_MIN,
   RB_VOLUME_MAX,
   RB_AGC_VALUE,
   RB_FREQUENCY,
   RB_DURATION_SEC,
   RB_OFFSET_SEC,
   RB_FORWARD_SEC,	/* takes looping into acount, and stops if necessary */
   RB_SAMPLED_RELEASE,
   RB_FAST_RELEASE,
   RB_ENVELOPE_SUSTAIN,
   RB_LOOPPOINT_START,
   RB_LOOPPOINT_END,
   RB_LOOPING,
   RB_FORMAT,
   RB_STATE,
   RB_NO_TRACKS,
   RB_NO_BLOCKS,
   RB_NO_LAYERS,
   RB_NO_TRACKS_OR_LAYERS,
   RB_NO_SAMPLES,
   RB_BLOCK_SIZE,
   RB_TRACKSIZE, 	/* no bytes of data in every track */
   RB_BYTES_SAMPLE,
   RB_OFFSET_SAMPLES,
   RB_INTERNAL_FORMAT,
   RB_NO_SAMPLES_AVAIL,
   RB_DDE_SAMPLES,
   RB_IS_PLAYING,
   RB_IS_MIXER_BUFFER,

   RB_PEAK_VALUE = 0x1000,
   RB_PEAK_VALUE_MAX = RB_PEAK_VALUE+RB_MAX_TRACKS,
   RB_AVERAGE_VALUE,
   RB_AVERAGE_VALUE_MAX = RB_AVERAGE_VALUE+RB_MAX_TRACKS,

   RB_ENVELOPE_LEVEL     = 0x2000,
   RB_ENVELOPE_LEVEL_MAX = 0x2010,
   RB_ENVELOPE_RATE       = 0x2010,
   RB_ENVELOPE_RATE_MAX   = 0x2020,

   RB_TREMOLO_DEPTH,
   RB_TREMOLO_RATE,
   RB_TREMOLO_SWEEP,
   RB_VIBRATO_DEPTH,
   RB_VIBRATO_RATE,
   RB_VIBRATO_SWEEP
};

enum _aaxRingBufferState
{
   RB_CLEARED = 0,
   RB_CLEARED_DDE,
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

#define MIX_BPS			24	// max. 24-bits in a 32-bits sample
#define MIX_T			float
#define MIX_PTR_T		float32_ptr
#define MIX_PTRPTR_T		float32_ptrptr
#define CONST_MIX_PTR_T		const_float32_ptr
#define CONST_MIX_PTRPTR_T	const_float32_ptrptr

extern float _aax_cubic_threshold;

/** forwrad declaration */
typedef struct _aaxRingBuffer_t __aaxRingBuffer;

/**
 * Filtes and Effects
 */

#define _AAX_MAX_STAGES		4
#define _AAX_MAX_DELAYS         RB_MAX_TRACKS
#define _AAX_MAX_LOOPBACKS      RB_MAX_TRACKS
#define _AAX_MAX_EQBANDS        8

typedef struct
{
   int (*run)(MIX_PTR_T, size_t, size_t, void*, void*, unsigned int);
   int (*add_noise)(MIX_PTR_T, size_t, size_t, void*, void*, unsigned int);

   _aaxLFOData lfo;
   _aaxLFOData env;

   float fs;
   float staticity;

   // moving average filters
   float prev[3];
   float fact[3];
   float alpha[3];

} _aaxRingBufferBitCrusherData;

typedef ALIGN16 struct {
   float history[RB_MAX_TRACKS][2*_AAX_MAX_STAGES];
} _aaxRingBufferFreqFilterHistoryData ALIGN16C;

typedef struct
{
   ALIGN16 float coeff[4*_AAX_MAX_STAGES] ALIGN16C;
   float high_gain, low_gain;
   float fc_low, fc_high;
   float Q, k, fs, fc;
   float resonance;

   unsigned int state;
   unsigned char no_stages;
   signed char type;
   bool random;

   _aaxLFOData *lfo;
   int (*run)(void*, MIX_PTR_T, CONST_MIX_PTR_T, size_t, size_t, size_t,
              unsigned int, void*, void*, float, unsigned char);

   _aaxRingBufferFreqFilterHistoryData *freqfilter;

} _aaxRingBufferFreqFilterData;

typedef struct
{
   _aaxLFOData *lfo;
   int (*run)(void*, MIX_PTR_T, CONST_MIX_PTR_T, size_t, size_t, size_t,
              unsigned int, void*, void*);

   _aaxRingBufferFreqFilterData *freq_filter;

} _aaxRingBufferDistoritonData;

typedef struct
{
   float T_K;		/* Temperature in Kelvin             */
   float pa_kPa;	/* Atmospherik pressure in kilpascal */
   float hr_pct;	/* relative humitidy in percents     */
   float unit_m;	/* distance scale factor to meters   */
   float a;

} _aaxEnvData;

typedef struct
{
   _aaxDistFn *run;
   float f2, dist, ref_dist, max_dist, rolloff;
   _aaxEnvData prev, next;

} _aaxRingBufferDistanceData;

typedef struct
{
   _aaxRingBufferFreqFilterData band[_AAX_MAX_EQBANDS];
   float rms[_AAX_MAX_EQBANDS];
   float peak[_AAX_MAX_EQBANDS];
} _aaxRingBufferEqualizerData;

typedef struct
{
   size_t noffs[RB_MAX_TRACKS];
   size_t coffs[RB_MAX_TRACKS];
   size_t step[RB_MAX_TRACKS];
} _aaxRingBufferOffsetData;

typedef struct
{
   ALIGN16 size_t sample_offs[RB_MAX_TRACKS] ALIGN16C;
  float gain;

} _aaxRingBufferDelayData;

typedef struct
{
   MIX_T* history[RB_MAX_TRACKS];
   void* ptr;
} _aaxRingBufferHistoryData;

typedef ALIGN16 struct
{
   _aaxRingBufferDelayData delay;

   size_t (*prepare)(MIX_PTR_T, MIX_PTR_T, size_t, void*, unsigned int);
   int (*run)(void*, MIX_PTR_T, MIX_PTR_T, MIX_PTR_T, size_t, size_t,
              size_t, size_t, void*, void*, unsigned int);

   int state;
   int no_tracks;
   int no_delays;
   float feedback;

   _aaxLFOData lfo;
   _aaxRingBufferOffsetData *offset;
   _aaxRingBufferHistoryData *history;
   _aaxRingBufferHistoryData *feedback_history;
   _aaxRingBufferFreqFilterData *freq_filter;
   size_t history_samples;

} _aaxRingBufferDelayEffectData ALIGN16C;

typedef struct
{
   vec4f_t occlusion;
   float magnitude;

   float level;			// obstruction level
   float gain_reverb;		// reverb gain (e.q. a closed door)
   float gain;			// direct-path gain and cutoff-frequency
   float fc;

   _aaxRingBufferFreqFilterData freq_filter;

   bool inverse;

   void (*prepare)(_aaxEmitter*, const _aax3dProps*, void*);
   int (*run)(void*, MIX_PTR_T, CONST_MIX_PTR_T, MIX_PTR_T, size_t,
              unsigned int, const void*);

} _aaxRingBufferOcclusionData;

typedef ALIGN16 struct
{
   /* 1st order reflections */
   _aaxRingBufferDelayData delay[_AAX_MAX_DELAYS];
   size_t history_samples;
   unsigned int no_delays; 

   _aaxRingBufferHistoryData *history;

} _aaxRingBufferReflectionData ALIGN16C;

typedef ALIGN16 struct
{
   /* 2nd roder reflections */
   _aaxRingBufferDelayData loopback[_AAX_MAX_LOOPBACKS];
   unsigned int no_loopbacks;

   _aaxRingBufferHistoryData *reverb;

} _aaxRingBufferLoopbackData ALIGN16C;

typedef struct
{
   void (*prepare)(_aaxEmitter*, const _aax3dProps*, void*);
   void (*reflections_prepare)(MIX_PTR_T, MIX_PTR_T, size_t, void*, unsigned int);
   int (*run)(void*, MIX_PTR_T, CONST_MIX_PTR_T, MIX_PTR_T, size_t, size_t,
              unsigned int, const void*,const void*, _aaxMixerInfo*,
              unsigned char, int, void*, unsigned char);

   int state;

   _aaxLFOData *lfo;
   _aaxMixerInfo *info;
   _aaxRingBufferOcclusionData *occlusion;
   _aaxRingBufferReflectionData *reflections;
   _aaxRingBufferLoopbackData *loopbacks;
   _aaxRingBufferFreqFilterData *freq_filter;
   _aaxRingBufferHistoryData *direct_path;
   void** track_prev;

   size_t no_samples;
   float fc;

} _aaxRingBufferReverbData;

typedef struct
{
   int (*run)(const _aaxDriverBackend*, const void*, void*, void*);

   _aaxMixerInfo *info;
   _aaxRingBufferOcclusionData *occlusion;
   _aaxRingBufferFreqFilterData *freq_filter;

   float fc;
   float rms;
   float delay_gain;
   float threshold;

   _aaxRingBufferHistoryData *history;
   int history_start[RB_MAX_TRACKS];
   unsigned int history_samples;
   unsigned int history_max;

   unsigned int step;

   size_t no_samples;
   void **sample_ptr;
   MIX_T *sample;

   void *tid[RB_MAX_TRACKS];

#if 0
#if __LINUX__
   int fd;
   struct gbm_device *gbm;
#endif
   EGLDisplay display;
   GLuint shader;
   GLuint cptr;
   GLuint hptr[RB_MAX_TRACKS];
   GLuint sptr[RB_MAX_TRACKS];
#endif

} _aaxRingBufferConvolutionData;

typedef struct
{
   int (*run)(void*, void*);
   FLOAT (*prepare)(_aax3dProps*, _aaxDelayed3dProps*, _aaxDelayed3dProps*, _aaxDelayed3dProps*, vec3f_ptr, float, float, float);

   _aaxPitchShiftFn *dopplerfn;

   size_t no_samples;
   void **sample_ptr;
   MIX_T *sample;

} _aaxRingBufferVelocityEffectData;

typedef struct
{
   int (*run)(MIX_PTR_T, size_t, size_t, void*, void*, unsigned int);

   float gain;
   float phase[RB_MAX_TRACKS];
   _aaxLFOData lfo;

   bool amplitude;

} _aaxRingBufferModulatorData;


/**
 * Function type definitions
 */

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
 * returns true on success or false otherwise.
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
 * Actual rendering is done in the struct _aaxRingBufferMixMNFn* call;
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
 * @param history holds the last CUBIC_SAMPS of the previous run for every track
 *
 * returns 0 if the sound has stopped playing, 1 otherwise.
 */
typedef int
_aaxRingBufferMixStereoFn(struct _aaxRingBuffer_t*, struct _aaxRingBuffer_t*, const void*, _aax2dProps*, unsigned char, float, _history_t);

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
 * @param data rendering structure containing information about the parent frame
 * @param ch channel to use from the source buffer if it is multi-channel
 * @param ctr update-rate counter:
 *     - Rendering to the destination buffer is done every frame at the
 *       interval rate. Updating of 3d properties and the like is done
 *       once every 'ctr' frame updates. so if ctr == 1, updates are
 *       done every frame.
 * @param nbuf number of buffers in the source queue (>1 means streaming)r
 * @param history holds the last CUBIC_SAMPS of the previous run for every track
 *
 * returns 0 if the sound has stopped playing, 1 otherwise.
 */
typedef int
_aaxRingBufferMixMonoFn(struct _aaxRingBuffer_t*, struct _aaxRingBuffer_t*, _aax2dProps*, void*, unsigned char, unsigned char, float, _history_t);


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
 * returns true if successful, false otherwise.
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
 * returns true if successful, false otherwise.
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
 * returns true if successful, false otherwise.
 */
typedef int
_aaxRingBufferSetParamfFn(struct _aaxRingBuffer_t*, enum _aaxRingBufferParam, float);

typedef int
_aaxRingBufferSetParamdFn(struct _aaxRingBuffer_t*, enum _aaxRingBufferParam, FLOAT);

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
 * returns true if successful, false otherwise.
 */
typedef int
_aaxRingBufferDataClearFn(struct _aaxRingBuffer_t*, int);

/**
 * Mix the audio data of two ringbuffers
 *
 * @param drb destination buffer which holds the mixed audio data afterwards
 * @param srb source ringbuffer to mix with the destination ringbuffer
 * @param lfo optional gain-envelope information, NULL if unused
 *
 * returns true if successful, false otherwise.
 */
typedef int
_aaxRingBufferDataMixDataFn(struct _aaxRingBuffer_t*, struct _aaxRingBuffer_t*, _aax2dProps*, unsigned char);

/**
 * Alter the gain of the ringbuffer audio data
 *
 * @param rb ringbuffer to adjust the gain for
 * @param offs starting position in samples
 * @param no_samples number of samples to process
 * @param factor gainn multiplication factor
 *
 * returns true if successful, false otherwise.
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
 * returns true if successful, false otherwise.
 */
typedef int
_aaxRingBufferDataMixWaveformFn(struct _aaxRingBuffer_t*, _data_t*, enum aaxSourceType, int, float, float, float, unsigned char, unsigned char);

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
 * returns true if successful, false otherwise.
 */
typedef int
_aaxRingBufferDataMixNoiseFn(struct _aaxRingBuffer_t*, _data_t*, enum aaxSourceType, int, float, float, float, uint64_t, char, unsigned char, unsigned char);

/**
 * Limit the audio data in the ringbuffer
 *
 * @param rb ringbuffer to limit
 * @param type type of limiter/compressor to use
 */
typedef void
_aaxRingBufferDataLimiterFn(struct _aaxRingBuffer_t*, enum _aaxLimiterType);

/**
 */
typedef void
_aaxRingBufferDataDitherFn(struct _aaxRingBuffer_t*, unsigned int bits);

/**
 */
size_t _aaxRingBufferCreateHistoryBuffer(_aaxRingBufferHistoryData**, size_t, int);


typedef struct _aaxRingBuffer_t
{
// public:
   unsigned int id;
   void *handle;

   _aaxRingBufferInitFn *init;
   _aaxRingBufferReferenceFn *reference;
   _aaxRingBufferDuplicateFn *duplicate;
   _aaxRingBufferDestroyFn *destroy;

   _aaxRingBufferSetFormatFn *set_format;
   _aaxRingBufferSetStateFn *set_state;
   _aaxRingBufferGetStateFn *get_state;

   _aaxRingBufferSetParamdFn *set_paramd;
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

