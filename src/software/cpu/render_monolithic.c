/*
 * SPDX-FileCopyrightText: Copyright © 2013-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2013-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
#endif

#include <aax/aax.h>

#include <base/logging.h>

#include <api.h>

#include "software/renderer.h"
#include "software/rbuf_int.h"

static _renderer_detect_fn _aaxCPUDetect;
static _renderer_new_handle_fn _aaxCPUSetup;
static _render_get_info_fn _aaxCPUInfo;
static _renderer_open_fn _aaxCPUOpen;
static _renderer_close_fn _aaxCPUClose;
static _render_process_fn _aaxCPUProcess;


_aaxRenderer*
_aaxDetectCPURenderer()
{
   _aaxRenderer* rv = calloc(1, sizeof(_aaxRenderer));
   if (rv)
   {
      rv->detect = _aaxCPUDetect;
      rv->setup = _aaxCPUSetup;
      rv->info = _aaxCPUInfo;

      rv->open = _aaxCPUOpen;
      rv->close = _aaxCPUClose;
      rv->process = _aaxCPUProcess;

   }
   return rv;
}

/* -------------------------------------------------------------------------- */

typedef struct
{
   int no_tracks;
   _aaxRendererData *data;

} _render_t;

static int
_aaxConvolutionThread(void *id)
{
   _render_t *handle = (_render_t*)id;
   _aaxRendererData *data = handle->data;
   int *num = &handle->no_tracks;
   int track = _aaxAtomicIntSub(num, 1) - 1;

   data->callback(data->drb, data, NULL, track);

   return id ? true : false;
}

static int
_aaxCPUDetect()
{
   return true;
}

static void*
_aaxCPUOpen(void* id)
{
   return id;
}

static int
_aaxCPUClose(void* id)
{
   if (id) free(id);
   return true;
}

static void*
_aaxCPUSetup(UNUSED(int dt))
{
   return (void*)-1;
}

static const char*
_aaxCPUInfo(UNUSED(void *id))
{
   static char info[32] = "";

   if (strlen(info) == 0)
   {
      const char *hwstr = _aaxGetSIMDSupportString();
      snprintf(info, 32, "%s", hwstr);
   }

   return info;
}

static int
_aaxCPUProcess(struct _aaxRenderer_t *render, _aaxRendererData *data)
{
   int rv = true;

   if (render->id == (void*)-1) {
      render->id = _aaxRingBufferCreateScratch(data->drb);
   }
   data->scratch = render->id;

   switch(data->mode)
   {
   case THREAD_PROCESS_EMITTER:
   {
      _intBuffers *he = data->e3d;
      int stage = 2;
      do
      {
         int no_emitters;

         no_emitters = _intBufGetNum(he, _AAX_EMITTER);
         if (no_emitters)
         {
            int pos = 0;
            do
            {
               _intBufferData *dptr_src;

               if ((dptr_src = _intBufGet(he, _AAX_EMITTER, pos++)) != NULL)
               {
                  // _aaxProcessEmitter calls
                  // _intBufReleaseData(dptr_src, _AAX_EMITTER);
                  rv |= _aaxProcessEmitter(data->drb, data, dptr_src, stage);
               }
            }
            while (--no_emitters);
         }
         _intBufReleaseNum(he, _AAX_EMITTER);

         /*
          * stage == 2 is 3d positional audio
          * stage == 1 is stereo audio
          */
         if (stage == 2) {
            he = data->e2d;	/* switch to stereo */
         }
      }
      while (--stage); /* process 3d positional and stereo emitters */
      break;
   }
   case THREAD_PROCESS_AUDIOFRAME:
   {
      _aaxRingBuffer *rb = data->drb;
      int t, no_tracks;

      no_tracks = data->mono ? 1 : rb->get_parami(rb, RB_NO_TRACKS);
      for (t=0; t<no_tracks; ++t) {
         data->callback(rb, data, NULL, t);
      }

      rv = true;
      break;
   }
   case THREAD_PROCESS_CONVOLUTION:
   {
      _aaxRingBufferConvolutionData *convolution = data->be_handle;
      _aaxRingBuffer *rb = data->drb;
      int t, no_tracks;
      _render_t handle;

      no_tracks = rb->get_parami(rb, RB_NO_TRACKS);

      handle.no_tracks = no_tracks;
      handle.data = data;

      for (t=0; t<no_tracks; ++t) {
         _aaxThreadStart(convolution->tid[t], _aaxConvolutionThread, &handle,
			 0, "aaxConvolution");
      }

      for (t=0; t<no_tracks; ++t) {
         _aaxThreadJoin(convolution->tid[t]);
      }

      rv = true;
      break;
   }
   default:
      break;
   }

   return rv;
}

