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

#define _AAX_RENDER_BACKENDS	1

/* forward declaration */
struct _aaxRenderer_t;

typedef int (_renderer_detect_fn)();
typedef void* (_renderer_new_handle_fn)(int);
typedef void* (_renderer_open_fn)(void*);
typedef int (_renderer_close_fn)(void*);
typedef int (_render_apply_fn)(struct _aaxRenderer_t*, _aaxRingBuffer*, _aax2dProps*, _aaxDelayed3dProps*, _intBuffers*, int, int, int, const _aaxDriverBackend*, void*);
typedef void (_render_wait_fn)(struct _aaxRenderer_t *);

typedef struct _aaxRenderer_t
{
   void *id;
   _renderer_detect_fn *detect;
   _renderer_new_handle_fn *setup;

   _renderer_open_fn *open;
   _renderer_close_fn *close;

   _render_apply_fn *update;
   _render_wait_fn *wait;

} _aaxRenderer;

typedef _aaxRenderer* (_aaxRendererDetect)(void);

extern _aaxRendererDetect* _aaxRenderTypes[];

_aaxRendererDetect _aaxDetectCPURenderer;

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* _AAX_RENDERTYPE_H */

