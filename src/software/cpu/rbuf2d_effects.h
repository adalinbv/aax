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

#ifndef _AAX_RBUF_EFFECTS_H
#define _AAX_RBUF_EFFECTS_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "driver.h"

float _aaxRingBufferEnvelopeGet(_aaxRingBufferEnvelopeData*, char);


void bufEffectsApply(int32_ptr, const int32_ptr, int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned char, void*, void*, void*);
void bufFilterFrequency(int32_ptr, const int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, void*, unsigned char);
void bufEffectDistort(int32_ptr, const int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, void*);
void bufEffectDelay(int32_ptr, const int32_ptr, int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, void*, unsigned int);
void bufEffectReflections(int32_t*, const int32_ptr, const int32_ptr, unsigned int, unsigned int, unsigned int, unsigned int, const void*);
void bufEffectReverb(int32_t*, unsigned int, unsigned int, unsigned int, unsigned int, const void*);

void bufCompress(int32_t*, unsigned int*, unsigned int*, float, float);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_RBUF_EFFECTS_H */

