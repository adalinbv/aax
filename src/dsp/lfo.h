/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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

#ifndef _AAX_FE_LFO_H
#define _AAX_FE_LFO_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <driver.h>


typedef float _convert_fn(float, float);
_convert_fn _linear;
_convert_fn _compress;

#define _MAX_ENVELOPE_STAGES            6
#define ENVELOPE_FOLLOW_STEP_CVT(a)     _MINMAX(-0.1005f+powf((a), 0.25f)/3.15f, 0.0f, 1.0f)

typedef float _aaxRingBufferLFOGetFn(void *, void*, const void*, unsigned, size_t);
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetSine;
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetSquare;
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetTriangle;
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetSawtooth;
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetFixedValue;
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetGainFollow;
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetCompressor;
_aaxRingBufferLFOGetFn _aaxRingBufferLFOGetPitchFollow;

typedef struct
{
   int state;
   float fs, period_rate, offset, depth;
   float min_sec, range_sec;

   float f, min, max;
   float gate_threshold, gate_period;
   float step[_AAX_MAX_SPEAKERS];       /* step = frequency / refresh_rate */
   float down[_AAX_MAX_SPEAKERS];       /* compressor release rate         */
   float value[_AAX_MAX_SPEAKERS];      /* current value                   */
   float average[_AAX_MAX_SPEAKERS];    /* average value over time         */
   float compression[_AAX_MAX_SPEAKERS];        /* compression level       */
   _aaxRingBufferLFOGetFn *get;
   _convert_fn *convert;
   char inv, envelope, stereo_lnk;
} _aaxLFOData;

_aaxLFOData* _lfo_create(void);
void _lfo_destroy(void*);

int _lfo_set_timing(_aaxLFOData*);
int _compressor_set_timing(_aaxLFOData*);


#endif /* _AAX_FE_LFO_H */

