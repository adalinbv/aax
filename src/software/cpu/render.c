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
 * The CPU renderer uses a thread worker with one thread tied to every
 * physical CPU core. This wil get the optimum rendering speed since it
 * can utilize the SSE registeres for every core simultaniously without
 * the possibility of choking the CPU caches.
 */

static _renderer_detect_fn _aaxWorkerDetect;
static _renderer_new_handle_fn _aaxWorkerSetup;
static _renderer_open_fn _aaxWorkerOpen;
static _renderer_close_fn _aaxWorkerClose;

static _render_wait_fn _aaxWorkerWait;
static _render_apply_fn _aaxWorkerUpdate;
static _render_finish_fn _aaxWorkerFinish;


_aaxRenderer*
_aaxDetectCPURenderer()
{
   _aaxRenderer* rv = calloc(1, sizeof(_aaxRenderer));
   if (rv)
   {
      rv->detect = _aaxWorkerDetect;
      rv->setup = _aaxWorkerSetup;
      rv->open = _aaxWorkerOpen;
      rv->close = _aaxWorkerClose;

      rv->wait = _aaxWorkerWait;
      rv->update = _aaxWorkerUpdate;
      rv->finish = _aaxWorkerFinish;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

#define _AAX_MAX_NO_WORKERS		8

typedef struct worker_data
{
   _aaxRingBuffer *drb;
   _aaxRendererData data;
   struct threat_t thread;
   _aaxSignal active;
   unsigned char state;

} worker_data_t;

typedef struct
{
   int worker_no;
   int no_workers;
   int no_cpu_cores;

   _aaxSignal signal;
   struct worker_data pool[_AAX_MAX_NO_WORKERS];

   unsigned char finish;

} _render_t;

static void* _aaxWorkerThread(void*);

enum _aaxWorkerThreadState
{
   AAX_WORKER_AVAILABLE = 0,
   AAX_WORKER_RESERVED,
   AAX_WORKER_BUSY
};


static int
_aaxWorkerDetect() {
   return AAX_TRUE;
}

static void*
_aaxWorkerOpen(void* id)
{
   return id;
}

static int
_aaxWorkerClose(void* id)
{
   _render_t *handle = (_render_t*)id;
   int i;

   for (i=0; i<handle->no_workers; i++)
   {
      struct worker_data *worker = &handle->pool[i];

      if (worker->thread.started)
      {
         worker->thread.started = AAX_FALSE;
         _aaxSignalTrigger(&worker->thread.signal);
         _aaxThreadJoin(worker->thread.ptr);
      }

      _aaxSignalFree(&worker->thread.signal);
      if (worker->thread.ptr) {
         _aaxThreadDestroy(worker->thread.ptr);
      }

      _aaxSignalFree(&worker->active);
   }
   _aaxSignalFree(&handle->signal);
   free(handle);

   return AAX_TRUE;
}

static void*
_aaxWorkerSetup(int dt)
{
   _render_t *handle = calloc(1, sizeof(_render_t));
   if (handle)
   {
      int i, res;

      _aaxSignalInit(&handle->signal);
      _aaxSignalLock(&handle->signal);

      handle->no_cpu_cores = _aaxGetNoCores();
      handle->no_workers = _MIN(_aaxGetNoCores(), _AAX_MAX_NO_WORKERS);
      
      for (i=0; i<handle->no_workers; i++)
      {
         struct worker_data *worker = &handle->pool[i];

         worker->state = AAX_WORKER_BUSY;

         _aaxSignalInit(&worker->active);

         worker->thread.ptr = _aaxThreadCreate();
         _aaxSignalInit(&worker->thread.signal);

         handle->worker_no = i;
         res =_aaxThreadStart(worker->thread.ptr, _aaxWorkerThread, handle, dt);
         if (res == 0)
         {
            int q = 100;
            while (q-- && worker->state != AAX_WORKER_AVAILABLE) {
               msecSleep(1);
            }
            worker->thread.started = AAX_TRUE;
         }
         else {
            _AAX_LOG(LOG_WARNING,  "CPU renderer: thread failed");
         }
      }
      handle->worker_no = 0;
      _aaxSignalUnLock(&handle->signal);
   }
   else {
      _AAX_LOG(LOG_WARNING, "CPU renderer: Insufficient memory");
   }

   return (void*)handle;
}

/*
 * Set up the data structure for the appropriate worker thread,
 * and return the data provided by the previous run of the worker thread.
 * Then signal the thread to start operating.
 */
static int
_aaxWorkerUpdate(struct _aaxRenderer_t *renderer, _aaxRendererData *data)
{
   _render_t *handle = (_render_t*)renderer->id;
   int worker_no, res = AAX_FALSE;

   worker_no = data->thread_no;

   assert(worker_no >= 0);
   assert(worker_no < handle->no_workers);

   // signal the tread to start working
   res = _aaxSignalTrigger(&handle->pool[worker_no].thread.signal);

   return res;
}

/*
 * Wait for a worker thread to become ready.
 * - sets data->thread_no so _aaxWorkerUpdate knows which thread to use.
 */
static int
_aaxWorkerWait(struct _aaxRenderer_t *renderer, _aaxRendererData *data, int finish)
{
   _render_t *handle = (_render_t*)renderer->id;
   int worker_no = handle->worker_no;
   struct worker_data *worker;
   int i, rv = AAX_FALSE;

   if (!finish || data->next)
   {
      // wait for the next free worker thread
      _aaxSignalLock(&handle->signal);
      _aaxSignalWait(&handle->signal);

      // find the next free worker thread
      worker_no = handle->worker_no;
      for (i=0; i<handle->no_workers; i++)
      {
         worker_no = (worker_no+1) % handle->no_cpu_cores;

         worker = &handle->pool[worker_no];
         if (worker->state == AAX_WORKER_AVAILABLE)
         {
            _aaxRendererData cdata;

            // Mark the worker-thread to be in use.
            // This prevents the next call to _aaxWorkerWait from claiming
            // it again in case the scheduler did not assign any time to
            // the thread.
            worker->state = AAX_WORKER_RESERVED;

            // save the thread number for the next call
            handle->worker_no = worker_no;

            // keep a backup of the data of the previous run
            memcpy(&cdata, &worker->data, sizeof(_aaxRendererData));

            // set the new data for this worker thread.
            memcpy(&worker->data, data, sizeof(_aaxRendererData));
            worker->data.valid = AAX_TRUE;

            // copy the backup data to the caller
            memcpy(data, &cdata,sizeof(_aaxRendererData));

            // save the thread number for _aaxWorkerUpdate
            data->thread_no = worker_no;

            rv = AAX_TRUE;
            break;
         }
      }
      _aaxSignalUnLock(&handle->signal);
   }
   else	// finish the active emitters
   {
      worker_no = handle->worker_no;
      for (i=0; i<handle->no_workers; i++)
      {
         worker_no = (worker_no+1) % handle->no_cpu_cores;

         worker = &handle->pool[worker_no];
         if (worker->data.valid)
         {
            if (worker->state != AAX_WORKER_AVAILABLE)
            {
               while (worker->state == AAX_WORKER_RESERVED) {
                  _aaxThreadSwitch();
               }
            
               // Wait until the thread is actually started
               _aaxSignalLock(&worker->active);
               _aaxSignalWait(&worker->active);
               _aaxSignalUnLock(&worker->active);
            }

            // Wait until the thread is finished
            _aaxSignalLock(&worker->thread.signal);

            memcpy(data, &worker->data, sizeof(_aaxRendererData));
            worker->data.valid = AAX_FALSE;

            _aaxSignalUnLock(&worker->thread.signal);

            assert(worker->state == AAX_WORKER_AVAILABLE);

            rv = AAX_TRUE;
            break;
         }
      }
   }

   return rv;
}

static int
_aaxWorkerFinish(struct _aaxRenderer_t *renderer)
{
   _render_t *handle = (_render_t*)renderer->id;
   int i;

   handle->finish = AAX_TRUE;
   for (i=0; i<handle->no_workers; i++)
   {
      struct worker_data *worker = &handle->pool[i];

      assert(worker->state == AAX_WORKER_AVAILABLE);

      worker->data.next = 0;
      worker->data.src = NULL;

      if (worker->data.drb)
      {
         _aaxSignalTrigger(&worker->thread.signal);

         // Wait until the thread is actually started
         _aaxSignalLock(&worker->active);
         _aaxSignalWait(&worker->active);
         _aaxSignalUnLock(&worker->active);

         // Wait until the thread is finished
         _aaxSignalLock(&worker->thread.signal);
         _aaxSignalUnLock(&worker->thread.signal);
      }
   }
   handle->finish = AAX_FALSE;

   return AAX_TRUE;
}

/* ------------------------------------------------------------------------- */

static void*
_aaxWorkerThread(void *id)
{
   _render_t *handle = (_render_t*)id;
   struct worker_data *worker;
   _aaxRendererData *data;
   int worker_no;

   worker_no = handle->worker_no;
   worker = &handle->pool[worker_no];
   data = &worker->data;

   _aaxThreadSetAffinity(worker->thread.ptr, worker_no % handle->no_cpu_cores);

   _aaxSignalLock(&worker->thread.signal);
   worker->state = AAX_WORKER_AVAILABLE;

   do
   {
      _aaxSignalTrigger(&handle->signal);
      _aaxSignalWait(&worker->thread.signal);
   }
   while (!data->drb && TEST_FOR_TRUE(worker->thread.started));

   if TEST_FOR_TRUE(worker->thread.started)
   {
      assert(data->drb != NULL);

      worker->drb = data->drb->duplicate(data->drb, AAX_TRUE, AAX_FALSE);
      worker->drb->set_state(worker->drb, RB_STARTED);
      do
      {
         // signal there's a thread ready for action
         worker->state = AAX_WORKER_BUSY;

         // let _aaxWorkerWait know the thread was active
         _aaxSignalTrigger(&worker->active);

         if TEST_FOR_FALSE(worker->thread.started) {
            break;
         }

         if (handle->finish)
         {
            _aaxSignalLock(&handle->signal);
            data->drb->data_mix(data->drb, worker->drb, NULL);
            _aaxSignalUnLock(&handle->signal);

            worker->drb->data_clear(worker->drb);
            worker->drb->set_state(worker->drb, RB_REWINDED);
         }
         else if (data->src)
         {
            _aax2dProps *ep2d = data->src->props2d;

            // 2d/3d mixing to our own ringbuffer
            if (data->stage == 2)
            {
               data->next = data->drb->mix3d(worker->drb, data->srb,
                                             ep2d, data->fp2d, data->track,
                                             data->ctr,data->nbuf);
            }
            else
            {
               data->next = data->drb->mix2d(worker->drb, data->srb,
                                             ep2d, data->fp2d, data->ctr,
                                             data->nbuf);
            }
            worker->drb->set_state(worker->drb, RB_REWINDED);
         }

         worker->state = AAX_WORKER_AVAILABLE;
         _aaxSignalTrigger(&handle->signal);
      }
      while(_aaxSignalWait(&worker->thread.signal));
      worker->drb->destroy(worker->drb);
   }
   _aaxSignalUnLock(&worker->thread.signal);

   return handle;
}

