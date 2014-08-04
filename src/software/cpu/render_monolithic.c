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

#include "software/rendertype.h"


static _renderer_detect_fn _aaxCPUDetect;
static _renderer_new_handle_fn _aaxCPUSetup;
static _renderer_open_fn _aaxCPUOpen;
static _renderer_close_fn _aaxCPUClose;

static _render_process_fn _aaxCPUProcess;
static _render_finish_fn _aaxCPUFinish;


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
      rv->finish = _aaxCPUFinish;

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
   int rv = AAX_TRUE;

   data->preprocess(data);
   do
   {
      _aaxEmitter *src = data->src;
      _aax2dProps *ep2d = src->props2d;

      if (data->stage == 2)
      {
         data->next = AAX_FALSE;
         if (ep2d->curr_pos_sec >= ep2d->dist_delay_sec) {
            data->next = data->drb->mix3d(data->drb, data->srb, ep2d,
                                          data->fp2d, data->track, data->ctr,
                                          data->nbuf);
         }
      }
      else
      {
         data->next = data->drb->mix2d(data->drb, data->srb, ep2d, data->fp2d,
                                       data->ctr, data->nbuf);
      }
      data->postprocess(data);
   }
   while (data->next);

   return rv;
}

static int
_aaxCPUFinish(struct _aaxRenderer_t *renderer)
{
   return AAX_TRUE;
}
