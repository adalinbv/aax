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
#include "arch.h"

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
static _render_apply_fn _aaxCPUUpdate;
static _render_wait_fn _aaxCPUWait;


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
      rv->update = _aaxCPUUpdate;
      rv->wait = _aaxCPUWait;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

#define _AAX_MAX_THREADS_POOL		8

typedef struct
{
   const _aaxDriverBackend* be;
   void *be_handle;

   _aaxRingBuffer *drb;
   _aaxDelayed3dProps *fdp3d_m;
   _aax2dProps *fp2d;
   _aaxEmitter *src;
   int track;
   int looping;
   int stage;

   int res;

} _render_data_t;

typedef struct
{
   int thread_no;
   int no_threads;

   _aaxSignal signal;

   char avail[_AAX_MAX_THREADS_POOL];
   struct threat_t thread[_AAX_MAX_THREADS_POOL];
   _render_data_t data[_AAX_MAX_THREADS_POOL];

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

   _aaxMutexLock(handle->signal.mutex);
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
   }
   _aaxSignalFree(&handle->signal);
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
      _aaxMutexLock(handle->signal.mutex);

      handle->no_threads = _MIN(_aaxGetNoCores(), _AAX_MAX_THREADS_POOL);
      for (i=0; i<handle->no_threads; i++)
      {
         handle->avail[i] = AAX_FALSE;

         handle->thread[i].ptr = _aaxThreadCreate();
         handle->thread[i].signal.mutex = _aaxMutexCreate(NULL);
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
      _aaxMutexUnLock(handle->signal.mutex);
   }
   else {
      _AAX_LOG(LOG_WARNING, "CPU renderer: Insufficient memory");
   }

   return (void*)handle;
}

static int
_aaxCPUUpdate(struct _aaxRenderer_t *renderer, _aaxRingBuffer *drb, _aax2dProps *fp2d, _aaxDelayed3dProps *fdp3d_m, _aaxEmitter *src, int track, int looping, int stage, const _aaxDriverBackend *be, void *be_handle)
{
   _render_t *handle = (_render_t*)renderer->id;
   int i, thread_no = handle->no_threads;
   int res = AAX_FALSE;

   _aaxMutexLock(handle->signal.mutex);
   while (1)
   {
      res = _aaxSignalWait(&handle->signal);
      if (res == AAX_FALSE) {
         break;
      }
 
      for (i=0; i<handle->no_threads; i++)
      {
         thread_no = handle->thread_no;		/* round-robin */
         handle->thread_no = (handle->thread_no+1) % handle->no_threads;

         if (handle->avail[thread_no] == AAX_TRUE)
         {
            handle->avail[thread_no] = AAX_FALSE;
            break;
         }
      }
      if (i != handle->no_threads) {
         break;
      }
   }
   _aaxMutexUnLock(handle->signal.mutex);

   if (thread_no < handle->no_threads)
   {
      handle->data[thread_no].drb = drb;
      handle->data[thread_no].fp2d = fp2d;
      handle->data[thread_no].fdp3d_m = fdp3d_m;
      handle->data[thread_no].src = src;
      handle->data[thread_no].track = track;
      handle->data[thread_no].looping = looping;
      handle->data[thread_no].stage = stage;
      handle->data[thread_no].be = be;
      handle->data[thread_no].be_handle = be_handle;
      _aaxSignalTrigger(&handle->thread[thread_no].signal);
      _aaxThreadSwitch();
   }

   return res;
}

static void
_aaxCPUWait(struct _aaxRenderer_t *renderer)
{
   _render_t *handle = (_render_t*)renderer->id;
   int i;

   for (i=0; i<handle->no_threads; i++)
   {
      if (handle->avail[i] == AAX_FALSE) {
         msecSleep(1);
      }
   }
}

static void*
_aaxCPUThread(void *id)
{
   _render_t *handle = (_render_t*)id;
   int thread_no = handle->thread_no;
   struct threat_t *thread;
   _render_data_t *data;
 
   thread = &handle->thread[thread_no];
   data = &handle->data[thread_no];

   _aaxThreadSetAffinity(thread->ptr, thread_no);

   _aaxMutexLock(thread->signal.mutex);
   do
   {
      /* signal there's a thread ready for action */
      _aaxMutexLock(handle->signal.mutex);
      handle->avail[thread_no] = AAX_TRUE;
      _aaxMutexUnLock(handle->signal.mutex);

      _aaxSignalTrigger(&handle->signal);
      _aaxSignalWait(&thread->signal);

      _aaxMutexLock(handle->signal.mutex);
      handle->avail[thread_no] = AAX_FALSE;
      _aaxMutexUnLock(handle->signal.mutex);

      if TEST_FOR_TRUE(thread->started)
      {
         _aaxRingBuffer *drb = data->drb;
         _aaxEmitter *src = data->src;
         _intBufferData *dptr_sbuf;
         unsigned int nbuf;
         int streaming;

         nbuf = _intBufGetNum(src->buffers, _AAX_EMITTER_BUFFER);
         assert(nbuf > 0);

         streaming = (nbuf > 1);
         dptr_sbuf = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER,
                                              src->buffer_pos);
         if (dptr_sbuf)
         {
            _embuffer_t *embuf = _intBufGetDataPtr(dptr_sbuf);
            _aaxRingBuffer *srb = embuf->ringbuffer;
            unsigned int res = 0;

            do
            {
               _aax2dProps *ep2d = src->props2d;

               if (_IS_STOPPED(src->props3d)) {
                  srb->set_state(srb, RB_STOPPED);
               }
               else if (srb->get_parami(srb, RB_IS_PLAYING) == 0)
               {
                  if (streaming) {
                     srb->set_state(srb, RB_STARTED_STREAMING);
                  } else {
                     srb->set_state(srb, RB_STARTED);
                  }
               }

               ep2d->curr_pos_sec = src->curr_pos_sec;
               src->curr_pos_sec += drb->get_paramf(drb, RB_DURATION_SEC);

               /* 3d mixing */
               if (data->stage == 2)
               {
                  res = AAX_FALSE;
                  if (ep2d->curr_pos_sec >= ep2d->dist_delay_sec)
                  {
                     res = drb->mix3d(drb, srb, ep2d, data->fp2d, data->track,
                                      src->update_ctr, nbuf, handle->signal.mutex);
                  }
               }
               else
               {
                  assert(!_IS_POSITIONAL(src->props3d));
                  res = drb->mix2d(drb, srb, ep2d, data->fp2d, src->update_ctr,
                                   nbuf, handle->signal.mutex);
               }

               /*
                * The current buffer of the source has finished playing.
                * Decide what to do next.
                */
               if (res)
               {
                  if (streaming)
                  {
                     /* is there another buffer ready to play? */
                     if (++src->buffer_pos == nbuf)
                     {
                        /*
                         * The last buffer was processed, return to the
                         * first buffer or stop? 
                         */
                        if TEST_FOR_TRUE(data->looping) {
                           src->buffer_pos = 0;
                        }
                        else
                        {
                           _SET_STOPPED(src->props3d);
                           _SET_PROCESSED(src->props3d);
                           break;
                        }
                     }

                     res &= drb->get_parami(drb, RB_IS_PLAYING);
                     if (res)
                     {
                        _intBufReleaseData(dptr_sbuf,_AAX_EMITTER_BUFFER);
                        dptr_sbuf = _intBufGet(src->buffers,
                                               _AAX_EMITTER_BUFFER,
                                               src->buffer_pos);
                        embuf = _intBufGetDataPtr(dptr_sbuf);
                        srb = embuf->ringbuffer;
                     }
                  }
                  else /* !streaming */
                  {
                     _SET_PROCESSED(src->props3d);
                     break;
                  }
               }
            }
            while (res);
            _intBufReleaseData(dptr_sbuf, _AAX_EMITTER_BUFFER);
         }
         _intBufReleaseNum(src->buffers, _AAX_EMITTER_BUFFER);
      }
   }
   while(thread->started);
   _aaxMutexUnLock(thread->signal.mutex);

   return handle;
}

