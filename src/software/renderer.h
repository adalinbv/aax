/*
 * SPDX-FileCopyrightText: Copyright © 2013-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2013-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef _AAX_RENDERTYPE_H
#define _AAX_RENDERTYPE_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "ringbuffer.h"
#include "arch.h"

enum {
   THREAD_PROCESS_AUDIOFRAME = 0,
   THREAD_PROCESS_EMITTER,
   THREAD_PROCESS_CONVOLUTION
};

/* forward declaration */
struct _aaxRendererData_t;
typedef int (_aaxRendererCallback)(_aaxRingBuffer*, struct _aaxRendererData_t*, _intBufferData*, unsigned int);

_aaxRendererCallback _aaxProcessEmitter;

typedef struct _aaxRendererData_t
{
   char mode;

   bool ssr;
   bool mono;
   void *subframe;
   void *sensor;

   _aaxRingBuffer *drb;
   const _aaxMixerInfo *info;
   _aax2dProps *fp2d;
   _aax3dProps *fp3d;
   _intBuffers *e2d;
   _intBuffers *e3d;
   _aaxRendererCallback *callback;
   const _aaxDriverBackend *be;
   void *be_handle;
   MIX_T **scratch;

   float ssv;
   float sdf;
   float dt;

} _aaxRendererData;


/* forward declaration */
struct _aaxRenderer_t;

typedef int (_renderer_detect_fn)(void);
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

