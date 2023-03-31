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

#ifndef AUDIO_H
#define AUDIO_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <base/types.h>
#include <ringbuffer.h>
#include <objects.h>
#include <api.h>

#define NOISE_PADDING		64

// y = 1x^2 + 0.5x + 0.5
// #define NORM_TO_PITCH(a)	(0.5f + 0.5f*(a) + (a)*(a))
// #define NORM_TO_PITCH(a)	((a)<=1.0f) ? (a) : ((a)*1.027f)
#define NORM_TO_PITCH(a)        (a)


uint8_t _linear2alaw(int16_t);
uint8_t _linear2mulaw(int16_t);
int16_t _alaw2linear(uint8_t);
int16_t _mulaw2linear(uint8_t);
int16_t _adpcm2linear (uint8_t, int16_t*, uint8_t*);
void    _linear2adpcm(int16_t*, int16_t, uint8_t*, uint8_t*);

void  _sw_bufcpy_ima_adpcm(void*, const void*, size_t);

/* sensor */
char _aaxSensorsProcessSensor(void*, _aaxRingBuffer*, _aax2dProps*, int, char);
char _aaxSensorsProcess(_aaxRingBuffer*, const _intBuffers*, _aax2dProps*, int, char);
void *_aaxSensorCapture(_aaxRingBuffer*, const _aaxDriverBackend*, void*,
                     float*, float, unsigned int, float, float, ssize_t*, char);

/* frame */
char _aaxAudioFrameProcess(_aaxRingBuffer*, _frame_t*, void*, _aaxAudioFrame*, float, float, _aax2dProps*, _aax3dProps*, _aaxDelayed3dProps*, const _aaxDriverBackend*, void*, char, char);
void _aaxAudioFrameProcessDelayQueue(_aaxAudioFrame *);

/* emitter */
char _aaxEmittersProcess(_aaxRingBuffer*, const _aaxMixerInfo*, float, float, _aax2dProps*, _aax3dProps*, _intBuffers*, _intBuffers*, const _aaxDriverBackend*, void*);
void _aaxEmitterPrepare3d(_aaxEmitter*, const void*);



#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif

