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


static _renderer_detect_fn _aaxCPUDetect;
static _renderer_new_handle_fn _aaxCPUSetup;
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

      rv->open = _aaxCPUOpen;
      rv->close = _aaxCPUClose;
      rv->process = _aaxCPUProcess;

   }
   return rv;
}

/* -------------------------------------------------------------------------- */

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
_aaxCPUClose(void* id)
{
   return AAX_TRUE;
}

static void*
_aaxCPUSetup(int dt)
{
   return NULL;
}

static int
_aaxCPUProcess(struct _aaxRenderer_t *render, _aaxRendererData *data)
{
   _intBuffers *he = data->e3d;
   unsigned int stage;
   int rv = AAX_TRUE;

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
               data->mix_emitter(data->drb, data, dptr_src, stage);
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

   return rv;
}

