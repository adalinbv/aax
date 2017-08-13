/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#include "software/rbuf_int.h"

void _aaxRingBufferFilterFrequency(_aaxRingBufferSample*, MIX_PTR_T, CONST_MIX_PTR_T, size_t, size_t, size_t, unsigned int, void*, void *, unsigned char);
void _aaxRingBufferEffectDistort(_aaxRingBufferSample*, MIX_PTR_T, CONST_MIX_PTR_T, size_t, size_t, size_t, unsigned int, void*, void*);
void _aaxRingBufferEffectDelay(_aaxRingBufferSample*, MIX_PTR_T, CONST_MIX_PTR_T, MIX_PTR_T, size_t, size_t, size_t, size_t, void*, void*, unsigned int);
void _aaxRingBufferEffectReflections(_aaxRingBufferSample*, MIX_PTR_T, CONST_MIX_PTR_T, MIX_PTR_T, size_t, size_t, size_t, unsigned int, const void*, _aaxMixerInfo*);
void _aaxRingBufferEffectReverb(_aaxRingBufferSample*, MIX_PTR_T, size_t, size_t, size_t, unsigned int, const void*, _aaxMixerInfo*);
void _aaxRingBufferEffectConvolution(const _aaxDriverBackend*, const void*, _aaxRingBuffer*, void*);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_RBUF_EFFECTS_H */

