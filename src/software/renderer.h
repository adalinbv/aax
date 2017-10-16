/*
 * Copyright 2013-2017 by Erik Hofman.
 * Copyright 2013-2017 by Adalin B.V.
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

#ifndef _AAX_RENDERTYPE_H
#define _AAX_RENDERTYPE_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "ringbuffer.h"
#include "arch.h"

/* forward declaration */
struct _aaxRendererData_t;
typedef int (_aaxRendererCallback)(_aaxRingBuffer*, struct _aaxRendererData_t*, _intBufferData*, unsigned int);

_aaxRendererCallback _aaxProcessEmitter;

typedef struct _aaxRendererData_t
{
   _aaxRingBuffer *drb;
   const _aaxMixerInfo *info;
   _aaxDelayed3dProps *fdp3d_m;
   _aax2dProps *fp2d;
   _intBuffers *e2d;
   _intBuffers *e3d;
   _aaxRendererCallback *callback;
   const _aaxDriverBackend *be;
   void *be_handle;

   float ssv;
   float sdf;
   float dt;

} _aaxRendererData;


/* forward declaration */
struct _aaxRenderer_t;

typedef int (_renderer_detect_fn)();
typedef void* (_renderer_new_handle_fn)(int);
typedef const char* (_render_get_info_fn)(void*);
typedef void* (_renderer_open_fn)(void*);
typedef int (_renderer_close_fn)(void*);
typedef int (_render_process_fn)(struct _aaxRenderer_t *, _aaxRendererData*);

typedef struct _aaxRenderer_t
{
   void *id;
   unsigned int refctr;

   _renderer_detect_fn *detect;
   _renderer_new_handle_fn *setup;
   _render_get_info_fn *info;

   _renderer_open_fn *open;
   _renderer_close_fn *close;
   _render_process_fn *process;

} _aaxRenderer;

typedef _aaxRenderer* (_aaxRendererDetect)(void);

extern _aaxRendererDetect* _aaxRenderTypes[];

_aaxRendererDetect _aaxDetectCPURenderer;
_aaxRendererDetect _aaxDetectPoolRenderer;
_aaxRenderer* _aaxSoftwareInitRenderer(float, enum aaxRenderMode, int);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* _AAX_RENDERTYPE_H */

