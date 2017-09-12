/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

