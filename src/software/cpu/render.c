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


/*
 * The CPU renderer uses a thread pool with one thread tied to every
 * physical CPU core. This wil get the optimum rendering speed since it
 * can utilize the SSE registeres for every core simultaniously without
 * the possibility of choking the CPU caches.
 */

static _renderer_detect_fn _aaxCPUDetect;
static _renderer_new_handle_fn _aaxCPUSetup;
static _renderer_open_fn _aaxCPUOpen;
static _renderer_close_fn _aaxCPUClose;

static _render_wait_fn _aaxCPUWait;
static _render_apply_fn _aaxCPUUpdate;
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

      rv->wait = _aaxCPUWait;
      rv->update = _aaxCPUUpdate;
      rv->finish = _aaxCPUFinish;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

#define _AAX_MAX_THREADS_POOL		8

typedef struct
{
   int thread_no;
   int no_threads;

   _aaxSignal signal;
   _aaxSignal thread_active[_AAX_MAX_THREADS_POOL];

   char avail[_AAX_MAX_THREADS_POOL];
   struct threat_t thread[_AAX_MAX_THREADS_POOL];
   _aaxRendererData data[_AAX_MAX_THREADS_POOL];

   _aaxRingBuffer *drb;
   char working;
   char finish;

} _render_t;

static void* _aaxCPUThread(void*);


static int
_aaxCPUDetect() {
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
   _render_t *handle = (_render_t*)id;
   int i;

   for (i=0; i<handle->no_threads; i++)
   {
      if (handle->thread[i].started)
      {
         handle->thread[i].started = AAX_FALSE;
         _aaxSignalTrigger(&handle->thread[i].signal);
         _aaxThreadJoin(handle->thread[i].ptr);
      }

      _aaxSignalFree(&handle->thread[i].signal);
      if (handle->thread[i].ptr) {
         _aaxThreadDestroy(handle->thread[i].ptr);
      }

      _aaxSignalFree(&handle->thread_active[i]);
   }
   _aaxSignalFree(&handle->signal);

   if (handle->drb) {
      handle->drb->destroy(handle->drb);
   }
   free(handle);

   return AAX_TRUE;
}

static void*
_aaxCPUSetup(int dt)
{
   _render_t *handle = calloc(1, sizeof(_render_t));
   if (handle)
   {
      int i, res;

      _aaxSignalInit(&handle->signal);
      _aaxSignalLock(&handle->signal);

      handle->no_threads = _MIN(_aaxGetNoCores(), _AAX_MAX_THREADS_POOL);
      for (i=0; i<handle->no_threads; i++)
      {
         handle->avail[i] = AAX_FALSE;

         _aaxSignalInit(&handle->thread_active[i]);

         handle->thread[i].ptr = _aaxThreadCreate();
         _aaxSignalInit(&handle->thread[i].signal);

         handle->thread_no = i;
         res =_aaxThreadStart(handle->thread[i].ptr, _aaxCPUThread, handle, dt);
         if (res == 0)
         {
            int q = 100;
            while (q-- && handle->avail[i] == AAX_FALSE) {
               msecSleep(1);
            }
            handle->thread[i].started = AAX_TRUE;
         }
         else {
            _AAX_LOG(LOG_WARNING,  "CPU renderer: thread failed");
         }
      }
      handle->thread_no = 0;
      _aaxSignalUnLock(&handle->signal);
   }
   else {
      _AAX_LOG(LOG_WARNING, "CPU renderer: Insufficient memory");
   }

   return (void*)handle;
}

static int
_aaxCPUUpdate(struct _aaxRenderer_t *renderer, _aaxRendererData *data)
{
   _render_t *handle = (_render_t*)renderer->id;
   int thread_no = handle->no_threads;
   int res = AAX_FALSE;

   if (!data->src)
   {
      _aaxSignalTrigger(&handle->signal);
   }
   else 
   {
      int thread, next;

      thread_no = data->thread_no;

      assert(thread_no >= 0);
      assert(thread_no < handle->no_threads);

      thread = handle->data[thread_no].thread_no;
      next = handle->data[thread_no].next;

      memcpy(&handle->data[thread_no], data, sizeof(_aaxRendererData));

      data->next = next;
      data->thread_no = thread;

      _aaxSignalTrigger(&handle->thread[thread_no].signal);
//    _aaxThreadSwitch();
   }

   return res;
}

static int
_aaxCPUWait(struct _aaxRenderer_t *renderer, _aaxRendererData *data, int finish)
{
   _render_t *handle = (_render_t*)renderer->id;
   int thread_no = handle->thread_no;
   int i, rv = AAX_FALSE;

   if (!finish)
   {
      if (data->working)
      {
         data->working = AAX_FALSE;

         _aaxSignalLock(&handle->signal);
         _aaxSignalWait(&handle->signal);

         thread_no = handle->thread_no;
         for (i=0; i<handle->no_threads; i++)
         {
            thread_no = (thread_no+1) % handle->no_threads;

            if (handle->avail[thread_no] == AAX_TRUE)
            {
               handle->thread_no = thread_no;
               if (!data->next) {
                  handle->avail[thread_no] = AAX_FALSE;
               }

               memcpy(data, &handle->data[i], sizeof(_aaxRendererData));
               data->thread_no = thread_no;
               rv = AAX_TRUE;
            }

            if (rv == AAX_TRUE) break;
         }
         _aaxSignalUnLock(&handle->signal);
      }
      else {
         rv = AAX_TRUE;
      }
   }
   else	// finish the last remaining threads
   {
      for (i=0; i<handle->no_threads; i++)
      {
         if (handle->avail[i] == AAX_FALSE)
         {
            // Wait until the thread is actually started
            _aaxSignalLock(&handle->thread_active[i]);
            _aaxSignalWait(&handle->thread_active[i]);
            _aaxSignalUnLock(&handle->thread_active[i]);

            // Wait until the thread is finished
            _aaxSignalLock(&handle->thread[i].signal);
            memcpy(data, &handle->data[i], sizeof(_aaxRendererData));
            _aaxSignalUnLock(&handle->thread[i].signal);

            rv = AAX_TRUE;
            break;
         }
      }
   }

   return rv;
}

static int
_aaxCPUFinish(struct _aaxRenderer_t *renderer)
{
   _render_t *handle = (_render_t*)renderer->id;

   return AAX_TRUE;
}

/* ------------------------------------------------------------------------- */

static void*
_aaxCPUThread(void *id)
{
   _render_t *handle = (_render_t*)id;
   struct threat_t *thread;
   _aaxRendererData *data;
   int thread_no;

   thread_no = handle->thread_no;
   thread = &handle->thread[thread_no];
   data = &handle->data[thread_no];

   _aaxThreadSetAffinity(thread->ptr, thread_no);

   _aaxSignalLock(&thread->signal);
   handle->avail[thread_no] = AAX_TRUE;

   do
   {
      _aaxSignalTrigger(&handle->signal);
      _aaxSignalWait(&thread->signal);
   }
   while (!data->drb && TEST_FOR_TRUE(thread->started));

   if TEST_FOR_TRUE(thread->started)
   {
      assert(data->drb != NULL);
      handle->drb = data->drb->duplicate(data->drb, AAX_TRUE, AAX_FALSE);
      handle->drb->set_state(handle->drb, RB_STARTED);
      do
      {
         /* signal there's a thread ready for action */
         handle->avail[thread_no] = AAX_FALSE;

         /* let _aaxCPUWait know the thread was active */
         data->working = AAX_TRUE;
         _aaxSignalTrigger(&handle->thread_active[thread_no]);

         if TEST_FOR_FALSE(thread->started) {
            break;
         }

         if (handle->finish)
         {
            _aaxSignalLock(&handle->signal);
            data->drb->data_mix(data->drb, handle->drb, NULL);
            _aaxSignalUnLock(&handle->signal);

            handle->drb->data_clear(handle->drb);
            handle->drb->set_state(handle->drb, RB_REWINDED);
         }
         else
         {
            _aax2dProps *ep2d = data->src->props2d;

            /* 2d/3d mixing to our own ringbuffer*/
            if (data->stage == 2)
            {
               data->next = data->drb->mix3d(handle->drb, data->srb, ep2d,
                                             data->fp2d, data->track, data->ctr,
                                             data->nbuf);
            }
            else
            {
               data->next = data->drb->mix2d(handle->drb, data->srb, ep2d,
                                             data->fp2d, data->ctr, data->nbuf);
            }

#if 1
            /* mix our own ringbuffer with the mixer ringbuffer */
            _aaxSignalLock(&handle->signal);
            data->drb->data_mix(data->drb, handle->drb, NULL);
            _aaxSignalUnLock(&handle->signal);

            handle->drb->data_clear(handle->drb);
#endif
            handle->drb->set_state(handle->drb, RB_REWINDED);
         }

         handle->avail[thread_no] = AAX_TRUE;
         _aaxSignalTrigger(&handle->signal);
      }
      while(_aaxSignalWait(&thread->signal));
   }
   _aaxSignalUnLock(&thread->signal);

   return handle;
}

