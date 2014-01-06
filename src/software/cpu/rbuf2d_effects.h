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

#ifndef _AAX_RBUF_EFFECTS_H
#define _AAX_RBUF_EFFECTS_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <ringbuffer.h>
#include <driver.h>

float _aaxRingBufferEnvelopeGet(_aaxRingBufferEnvelopeData*, char);


void _aaxRingBufferFilterFrequency(int32_ptr, const int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, void*, unsigned char);
void _aaxRingBufferEffectDistort(int32_ptr, const int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, void*);
void _aaxRingBufferEffectDelay(int32_ptr, const int32_ptr, int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, void*, unsigned int);
void _aaxRingBufferEffectReflections(int32_t*, const int32_ptr, const int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, const void*);
void _aaxRingBufferEffectReverb(int32_t*, unsigned int, unsigned int, unsigned int, unsigned int, const void*);

void _aaxRingBufferCompress(int32_t*, unsigned int*, unsigned int*, float, float);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_RBUF_EFFECTS_H */

