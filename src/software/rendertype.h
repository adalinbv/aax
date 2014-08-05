/*
 * Copyright 2013-2014 by Erik Hofman.
 * Copyright 2013-2014 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_RENDERTYPE_H
#define _AAX_RENDERTYPE_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "ringbuffer.h"
#include "arch.h"

#define _AAX_RENDER_BACKENDS	1

/* forward declaration */
struct _aaxRenderer_t;
struct _aaxRendererData_t;
typedef int (_aaxRendererCallback)(struct _aaxRendererData_t*);

typedef struct _aaxRendererData_t
{
   _aaxRendererCallback *preprocess;
   _aaxRendererCallback *postprocess;

   _aaxRingBuffer *drb;
   _aaxRingBuffer *srb;
   _aax2dProps *fp2d;
   _aax2dProps *ep2d;

   _intBuffers *he;
   unsigned int pos;

   unsigned int ctr;
   unsigned char streaming;
   unsigned char looping;
   unsigned char track;
   unsigned char stage;

   unsigned char next;

   float dt;

} _aaxRendererData;


typedef int (_renderer_detect_fn)();
typedef void* (_renderer_new_handle_fn)(int);
typedef void* (_renderer_open_fn)(void*);
typedef int (_renderer_close_fn)(void*);
typedef int (_render_process_fn)(struct _aaxRenderer_t *, _aaxRendererData*);
typedef int (_render_finish_fn)(struct _aaxRenderer_t *);

typedef struct _aaxRenderer_t
{
   void *id;
   unsigned int refctr;

   _renderer_detect_fn *detect;
   _renderer_new_handle_fn *setup;

   _renderer_open_fn *open;
   _renderer_close_fn *close;

   _render_process_fn *process;
   _render_finish_fn *finish;

} _aaxRenderer;

typedef _aaxRenderer* (_aaxRendererDetect)(void);

extern _aaxRendererDetect* _aaxRenderTypes[];

_aaxRendererDetect _aaxDetectCPURenderer;
_aaxRendererDetect _aaxDetectPoolRenderer;
_aaxRenderer* _aaxSoftwareInitRenderer(float);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* _AAX_RENDERTYPE_H */

