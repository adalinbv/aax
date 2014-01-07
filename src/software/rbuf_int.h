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

#ifndef _AAX_CPU_RINGBUFFER_H
#define _AAX_CPU_RINGBUFFER_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <aax/aax.h>
#include <base/geometry.h>
#include <base/types.h>

#include <arch.h>
#include "cpu/rbuf2d_effects.h"

#define RB_FLOAT_DATA		0
#define BYTE_ALIGN		1
#define CUBIC_SAMPS		4

/** forwrad declaration */
typedef struct _aaxRingBufferData_t _aaxRingBufferData;

typedef int32_t**_aaxProcessMixerFn(struct _aaxRingBufferData_t*, struct _aaxRingBufferData_t*, _aax2dProps*, float, unsigned int*, unsigned int*, unsigned char, unsigned int);
typedef void _aaxProcessCodecFn(int32_t*, void*, _batch_codec_proc, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned char, char);
typedef void _aaxEffectsApplyFn(int32_ptr, int32_ptr, int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned char, void*, void*, void*);

enum
{
    SCRATCH_BUFFER0 = _AAX_MAX_SPEAKERS,
    SCRATCH_BUFFER1,

    MAX_SCRATCH_BUFFERS
};

typedef struct			/* static information about the sample */
{
    void** track;

    void** scratch;		/* resident scratch buffer */
    unsigned char no_tracks;
    unsigned char bytes_sample;
    unsigned short ref_counter;

    float frequency_hz;
    float duration_sec;
    float loop_start_sec;
    float loop_end_sec;

    unsigned int no_samples;		/* actual no. samples */
    unsigned int no_samples_avail;	/* maximum available no. samples */
    unsigned int track_len_bytes;
    unsigned int dde_samples;

    enum aaxFormat format;
    _batch_codec_proc codec;

} _aaxRingBufferSample;


/**
 * M:N channel ringbuffer data manipulation
 *
 * @param drb multi track destination buffer
 * @param srb multi track source buffer
 * @param sptr multi track source audio data in mixer format and frequency
 * @param ep2d 3d positioning information structure of the source
 * @param offs starting offset in number of samples
 * @param dno_samples total number of samples to mix
 * @param gain multiplication factor for the data in the source buffer
 * @param svol volume at the start of the mixing process (envelope following)
 * @param evol volume at the end of the mixing process (envelope following)
 */
typedef void
_aaxRingBufferMixMNFn(_aaxRingBufferSample*, const _aaxRingBufferSample*, const int32_ptrptr, _aax2dProps*, unsigned int, unsigned int, float, float, float);

/**
 * 1:N channel ringbuffer data manipulation
 *
 * @param drb multi track destination buffer
 * @param sptr multi track source audio data in mixer format and frequency
 * @param ep2d 3d positioning information structure of the source
 * @param ch channel to use from the source buffer (if it is multi-channel)
 * @param offs starting offset in number of samples
 * @param dno_samples total number of samples to mix
 * @param gain multiplication factor for the data in the source buffer
 * @param svol volume at the start of the mixing process (envelope following)
 * @param evol volume at the end of the mixing process (envelope following)
 */
typedef void
_aaxRingBufferMix1NFn(_aaxRingBufferSample*, const int32_ptrptr, _aax2dProps*, unsigned char, unsigned int, unsigned int, float, float, float);


/* playback related information about the sample */
typedef struct _aaxRingBufferData_t
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

    unsigned int loop_max;
    unsigned int loop_no;
    char looping;

    char playing;
    char stopped;
    char streaming;

#ifndef NDEBUG
    void *parent;
#endif

   enum aaxRenderMode mode;
   enum _aaxRingBufferMode access;

   _aaxProcessCodecFn *codec;
   _batch_resample_proc resample;
   _aaxEffectsApplyFn *effects;
   _aaxProcessMixerFn *mix;

   /* called by the mix function above */
   _aaxRingBufferMix1NFn *mix1n;
   _aaxRingBufferMixMNFn *mixmn;

} _aaxRingBufferData;


/* --------------------------------------------------------------------------*/

/** CODECs */
typedef struct {
   unsigned char bits;
   enum aaxFormat format;
} _aaxFormat_t;

extern _batch_codec_proc _aaxRingBufferCodecs[];
extern _batch_codec_proc _aaxRingBufferCodecs_w8s[];

void _aaxRingBufferProcessCodec(int32_t*, void*, _batch_codec_proc, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned char, char);
void _aaxRingBufferEffectsApply(int32_ptr, int32_ptr, int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned char, void*, void*, void*);


/** MIXER */

int32_t**_aaxRingBufferProcessMixer(_aaxRingBufferData*, _aaxRingBufferData*, _aax2dProps*, float, unsigned int*, unsigned int*, unsigned char, unsigned int);

_aaxRingBufferMixMNFn _aaxRingBufferMixStereo16;
_aaxRingBufferMix1NFn _aaxRingBufferMixMono16Stereo;
_aaxRingBufferMix1NFn _aaxRingBufferMixMono16Spatial;
_aaxRingBufferMix1NFn _aaxRingBufferMixMono16Surround;
_aaxRingBufferMix1NFn _aaxRingBufferMixMono16HRTF;


/** BUFFER */
void _bufferMixWhiteNoise(void**, unsigned int, char, int, float, float, unsigned char);
void _bufferMixPinkNoise(void**, unsigned int, char, int, float, float, float, unsigned char);
void _bufferMixBrownianNoise(void**, unsigned int, char, int, float, float, float, unsigned char);
void _bufferMixSineWave(void**, float, char, unsigned int, int, float, float);
void _bufferMixSquareWave(void**, float, char, unsigned int, int, float, float);
void _bufferMixTriangleWave(void**, float, char, unsigned int, int, float, float);
void _bufferMixSawtooth(void**, float, char, unsigned int, int, float, float);
void _bufferMixImpulse(void**, float, char, unsigned int, int, float, float);


#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_CPU_RINGBUFFER_H*/

