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

uint8_t _linear2alaw(int16_t);
uint8_t _linear2mulaw(int16_t);
int16_t _alaw2linear(uint8_t);
int16_t _mulaw2linear(uint8_t);
int16_t _adpcm2linear (uint8_t, int16_t*, uint8_t*);
void    _linear2adpcm(int16_t*, int16_t, uint8_t*, uint8_t*);

void  _sw_bufcpy_ima_adpcm(void*, const void*, unsigned char, unsigned int);

/* sensor */
void _aaxSensorsProcess(_aaxRingBuffer*, const _intBuffers*, _aax2dProps*, int);
void *_aaxSensorCapture(_aaxRingBuffer*, const _aaxDriverBackend*, void*,
                        float*, float, int, float, float);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif

