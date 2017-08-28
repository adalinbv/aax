/*
 * Copyright 2013-2017 by Erik Hofman.
 * Copyright 2013-2017 by Adalin B.V.
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
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

#include <base/threads.h>
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

static void*
_aaxConvolutionThread(void *id)
{
   _render_t *handle = (_render_t*)id;
   _aaxRendererData *data = handle->data;
   int *num = &handle->no_tracks;
   int track = _aaxAtomicIntSub(num, 1) - 1;

   data->callback(data->drb, data, NULL, track);

   return id;
}

static int
_aaxCPUDetect()
{
   return AAX_TRUE;
}

static void*
_aaxCPUOpen(void* id)
{
   return id;
}

static int
_aaxCPUClose(UNUSED(void* id))
{
   return AAX_TRUE;
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
#if RB_FLOAT_DATA
      snprintf(info, 32, "FP %s", hwstr);
#else
      snprintf(info, 32, "%s", hwstr);
#endif
   }

   return info;
}

static int
_aaxCPUProcess(UNUSED(struct _aaxRenderer_t *render), _aaxRendererData *data)
{
   _intBuffers *he = data->e3d;
   unsigned int stage;
   int rv = AAX_TRUE;

   /*
    * process emitters
    */
   if (he)
   {
      stage = 2;
      do
      {
         unsigned int no_emitters;

         no_emitters = _intBufGetNum(he, _AAX_EMITTER);
         if (no_emitters)
         {
            unsigned int pos = 0;
            do
            {
               _intBufferData *dptr_src;

               if ((dptr_src = _intBufGet(he, _AAX_EMITTER, pos++)) != NULL)
               {
                  // _aaxProcessEmitter calls
                  // _intBufReleaseData(dptr_src, _AAX_EMITTER);
                  _aaxProcessEmitter(data->drb, data, dptr_src, stage);
               }
            }
            while (--no_emitters);

            rv = AAX_TRUE;
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
   }

   /*
    * process convolution
    */
   else
   {
      _aaxRingBufferConvolutionData *convolution = data->be_handle;
      _aaxRingBuffer *rb = data->drb;
      unsigned int t, no_tracks;
      _render_t handle;

      no_tracks = rb->get_parami(rb, RB_NO_TRACKS);

      handle.no_tracks = no_tracks;
      handle.data = data;

      for (t=0; t<no_tracks; ++t) {
         _aaxThreadStart(convolution->tid[t], _aaxConvolutionThread, &handle,0);
      }

      for (t=0; t<no_tracks; ++t) {
         _aaxThreadJoin(convolution->tid[t]);
      }

      rv = AAX_TRUE;
   }

   return rv;
}

