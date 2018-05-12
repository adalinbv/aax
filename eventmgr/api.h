/*
 * Copyright 2007-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#ifndef AAX_API_H
#define AAX_API_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <aax/aax.h>

/* --- Events --- */
#define EVENT_ID        0x9173652A

typedef struct
{
   unsigned int id;
   unsigned int emitter_pos;
   enum aaxEventType event;
   void *data;
   aaxEventCallbackFn callback;
   void *user_data;
} _event_t;

typedef struct
{
   void *handle;
   _intBuffers *buffers;
   _intBuffers *emitters;
   _intBuffers *frames;

   struct threat_t thread;

} _aaxEventMgr;

void* _aaxEventThread(void*);

/* --- Instrument --- */
#define INSTRUMENT_ID   0x0EB9A645

typedef struct
{
    aaxBuffer buffer;
    struct {
       unsigned int min;
       unsigned int max;
    } note;
} _timbre_t;

typedef struct
{
    aaxEmitter emitter;
    float pitchbend;
    float velocity;
    float aftertouch;
    float displacement;
//  float sustain;
//  float soften;
} _note_t;

typedef struct
{
    unsigned int id;
    void *handle;
    aaxFrame frame;

    float soften;
    float sustain;
    unsigned int polyphony;
    _intBuffers *note;
//  enum aaxFormat format;
//  unsigned int frequency_hz;
//  aaxFilter filter[AAX_FILTER_MAX];
//  aaxEffect effect[AAX_EFFECT_MAX];
} _controller_t;

typedef struct
{
    _intBuffers *timbres;
    _intBuffers *controllers;
} _soundbank_t;

_controller_t *get_controller(aaxController);
_controller_t *get_valid_controller(aaxController);


#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif

