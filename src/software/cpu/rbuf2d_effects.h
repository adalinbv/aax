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

#include "software/rbuf_int.h"

void _aaxRingBufferFilterFrequency(_aaxRingBufferSample*, MIX_PTR_T, CONST_MIX_PTR_T, unsigned int, unsigned int, unsigned int, unsigned int, void*, unsigned char);
void _aaxRingBufferEffectDistort(_aaxRingBufferSample*, MIX_PTR_T, CONST_MIX_PTR_T, unsigned int, unsigned int, unsigned int, unsigned int, void*);
void _aaxRingBufferEffectDelay(_aaxRingBufferSample*, MIX_PTR_T, CONST_MIX_PTR_T, MIX_PTR_T, unsigned int, unsigned int, unsigned int, unsigned int, void*, unsigned int);
void _aaxRingBufferEffectReflections(_aaxRingBufferSample*, MIX_PTR_T, CONST_MIX_PTR_T, MIX_PTR_T, unsigned int, unsigned int, unsigned int, unsigned int, const void*);
void _aaxRingBufferEffectReverb(_aaxRingBufferSample*, MIX_PTR_T, unsigned int, unsigned int, unsigned int, unsigned int, const void*);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_RBUF_EFFECTS_H */

