/*
 * Copyright 2005-2023 by Erik Hofman.
 * Copyright 2009-2023 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#ifndef _RBUF_INT_H
#define _RBUF_INT_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <aax/aax.h>
#include <base/geometry.h>
#include <base/types.h>

#include <software/cpu/waveforms.h>
#include <ringbuffer.h>
#include <arch.h>
#include <api.h>

MIX_T** _aaxRingBufferCreateScratch(_aaxRingBuffer*);


/** forwrad declaration */
typedef struct _aaxRingBufferData_t __aaxRingBufferData;
typedef struct _aaxRingBufferSample_t __aaxRingBufferSample;

typedef CONST_MIX_PTRPTR_T _aaxProcessMixerFn(MIX_T**, _aaxRingBuffer*, _aaxRingBuffer*, _aax2dProps*, FLOAT, size_t*, size_t*, unsigned char, _history_t);
typedef void _aaxProcessCodecFn(int32_t*, void*, _batch_codec_proc, size_t, size_t, size_t, size_t, size_t, unsigned char, char);
typedef void
_aaxEffectsApplyFn(struct _aaxRingBufferSample_t*, MIX_PTR_T, MIX_PTR_T, MIX_PTR_T, size_t, size_t, size_t, size_t, unsigned int, _aax2dProps*, unsigned char, unsigned char);


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
_aaxRingBufferMixMNFn(struct _aaxRingBufferSample_t*, const struct _aaxRingBufferSample_t*, CONST_MIX_PTRPTR_T, const unsigned char*, _aax2dProps*, size_t, size_t, float, float, float, char);

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
_aaxRingBufferMix1NFn(struct _aaxRingBufferSample_t*, CONST_MIX_PTRPTR_T, const unsigned char*, _aax2dProps*, unsigned char, size_t, size_t, float, float, float, float, char);


enum
{
    SCRATCH_BUFFER0 = RB_MAX_TRACKS,
    SCRATCH_BUFFER1,

    MAX_SCRATCH_BUFFERS
};

typedef struct _aaxRingBufferSample_t  /* static information about the sample */
{
    void** scratch;		/* resident scratch buffer */
    void** track;		/* audio tracks data */
    size_t tracksize;		/* size of the allocated audio tracks data */

    size_t no_blocks;
    size_t dde_samples;
    size_t no_samples;		/* actual no. samples */
    size_t no_samples_avail;	/* maximum available no. samples */

    size_t track_len_bytes;	/* as specified by the audio file */
    unsigned int block_size;

    unsigned short bits_sample;
    unsigned short no_tracks;
    unsigned short no_layers;

    unsigned short ref_counter;

    float frequency_hz;
    float duration_sec;
    float loop_start_sec;
    float loop_end_sec;
    float dde_sec;

    float freqfilter_history_x[RB_MAX_TRACKS];
    float freqfilter_history_y[RB_MAX_TRACKS];

    float volume_envelope[2*_MAX_ENVELOPE_STAGES];
    unsigned char envelope_sustain;
    unsigned char sampled_release;
    unsigned char fast_release;

    struct {
       float depth, rate, sweep;
    } vibrato, tremolo;

    unsigned char track_len_set;
    unsigned char block_size_set;
    unsigned char mixer_fmt;    /* 1 if the ringbuffer is part of the mixer */

    enum aaxFormat format;
    _batch_codec_proc codec;
    _batch_fmadd_proc add;
    _batch_mul_value_proc multiply;
    _batch_resample_float_proc resample;
    _batch_freqfilter_float_proc freqfilter;

   /* called by the mix function above */
   _aaxRingBufferMix1NFn *mix1;
   _aaxRingBufferMix1NFn *mix1n;
   _aaxRingBufferMixMNFn *mixmn;

} _aaxRingBufferSample;


/* playback related information about the sample */
typedef struct _aaxRingBufferData_t
{
    _aaxRingBufferSample* sample;		/* shared, constat settings */

    float peak[RB_MAX_TRACKS+1];		/* for the vu meter */
    float average[RB_MAX_TRACKS+1];

    float elapsed_sec;
    float volume_norm, gain_agc;
    float volume_min, volume_max;
    FLOAT pitch_norm;

    FLOAT curr_pos_sec;
    size_t curr_sample;

    unsigned int loop_max;
    unsigned int loop_no;
    unsigned char loop_mode;
    unsigned char sampled_release;

    unsigned char looping;
    unsigned char playing;
    unsigned char stopped;
    unsigned char streaming;

#ifndef NDEBUG
    void *parent;
#endif

   enum aaxRenderMode mode;
   enum _aaxRingBufferMode access;

   _aaxProcessCodecFn *codec;
   _aaxEffectsApplyFn *effects;
   _aaxProcessMixerFn *mix;

} _aaxRingBufferData;


/* --------------------------------------------------------------------------*/

#define CUBIC_TRESHOLD		(_aax_cubic_threshold)

/** CODECs */
typedef struct {
   unsigned char bits;
   enum aaxFormat format;
} _aaxFormat_t;

extern _batch_codec_proc _aaxRingBufferCodecs[];

void _aaxRingBufferProcessCodec(int32_t*, void*, _batch_codec_proc, size_t, size_t, size_t, size_t, size_t, unsigned char, char);
void _aaxRingBufferEffectsApply(_aaxRingBufferSample*, MIX_PTR_T, MIX_PTR_T, MIX_PTR_T, size_t, size_t, size_t, size_t, unsigned int, _aax2dProps*, unsigned char, unsigned char);


/** MIXER */

CONST_MIX_PTRPTR_T _aaxRingBufferProcessMixer(MIX_T**, _aaxRingBuffer*, _aaxRingBuffer*, _aax2dProps*, FLOAT, size_t*, size_t*, unsigned char, _history_t history);

_aaxRingBufferMixMNFn _aaxRingBufferMixStereo16;
_aaxRingBufferMix1NFn _aaxRingBufferMixMono16Mono;
_aaxRingBufferMix1NFn _aaxRingBufferMixMono16Stereo;
_aaxRingBufferMix1NFn _aaxRingBufferMixMono16Spatial;
_aaxRingBufferMix1NFn _aaxRingBufferMixMono16Surround;
_aaxRingBufferMix1NFn _aaxRingBufferMixMono16HRTF;

void _aaxRingBufferLimiter(MIX_PTR_T, size_t, float, float);
void _aaxRingBufferCompress(MIX_PTR_T, size_t, float, float);


/** BUFFER */
void _bufferMixWaveform(int32_t*, _data_t*, enum aaxSourceType, float, char, size_t, float, float, unsigned char, limitType);

void _bufferMixWhiteNoise(int32_t*, _data_t*, size_t, char, float, float, float, uint64_t, unsigned char, unsigned char, limitType);
void _bufferMixPinkNoise(int32_t*, _data_t*, size_t, char, float, float, float, uint64_t, unsigned char, unsigned char, limitType);
void _bufferMixBrownianNoise(int32_t*, _data_t*, size_t, char, float, float, float, uint64_t, unsigned char, unsigned char, limitType);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_RBUF_INT_H*/

