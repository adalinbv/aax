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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>	/* alloc and free */
#endif
#include <assert.h>

#include <aax/eventmgr.h>

#include <base/buffers.h>
#include <base/threads.h>

#include "api.h"

enum _aaxEventType
{
   AAX_USER_EVENTS = 0,

   /*
    * user events are defined in
    * enum aaxEventType in aax/eventmgr.h
    */

   AAX_SYSTEM_EVENTS = 0x8000,
   AAX_EVENT_STREAM_FILE,
   AAX_EVENT_PLAY_BUFFER,
   AAX_EVENT_DESTORY_EMITTER,

   AAX_SYSTEM_EVENT_MAX
};

typedef struct
{
   enum _aaxEventType event;
   void *data;
   aaxEventCallbackFn callback;
   void *user_data;
} _event_queue_t;

typedef struct
{
   aaxBuffer buffer;
   unsigned int ref_counter;
   char *fname;
} _buffer_cache_t;

typedef struct
{
   aaxEmitter emitter;
   aaxFrame frame;		/* parent frame */
   unsigned int buffer_pos;
   unsigned int pos;
   _intBuffers *event_queue;
} _emitter_cache_t;

typedef struct
{
   aaxFrame frame;
   unsigned int pos;
   _intBuffers *event_queue;
} _frame_cache_t;

static void _aaxFreeFrameCache(void*);
static void _aaxFreeBufferCache(void*);
static void _aaxFreeEmitterCache(void*);
static aaxBuffer _aaxGetBufferFromCache(_event_t*, const char*);


AAX_API int AAX_APIENTRY
aaxEventManagerCreate(aaxConfig config)
{
   _handle_t *handle = get_handle(config);
   int rv = AAX_FALSE;
   if (handle && !handle->eventmgr)
   {
      handle->eventmgr = calloc(1, sizeof(_event_t));
      if (handle->eventmgr)
      {
         _event_t *event = handle->eventmgr;
         int r;

         _intBufCreate(&event->frames, _AAX_FRAME_CACHE);
         _intBufCreate(&event->buffers, _AAX_BUFFER_CACHE);
         _intBufCreate(&event->emitters, _AAX_EMITTER_CACHE);

         event->thread.ptr = _aaxThreadCreate();
         assert(event->thread.ptr != 0);

         event->thread.condition = _aaxConditionCreate();
         assert(event->thread.condition != 0);

         event->thread.started = AAX_TRUE;
         r =_aaxThreadStart(event->thread.ptr, _aaxEventThread, event, 1000);
         if (r == 0)
         {
            event->thread.mutex = _aaxMutexCreate(event->thread.mutex);
            rv = AAX_TRUE;
         }
         else
         {
            aaxEventManagerDestory(config);
            _aaxErrorSet(AAX_INVALID_STATE);
         }
      }
   }
   else if (handle) {
      _aaxErrorSet(AAX_INVALID_STATE);
   } else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEventManagerDestory(aaxConfig config)
{
   _handle_t *handle = get_handle(config);
   int rv = AAX_FALSE;
   if (handle && handle->eventmgr)
   {
      _event_t *event = handle->eventmgr;

      if TEST_FOR_TRUE(event->thread.started)
      {
         event->thread.started = AAX_FALSE;
         _aaxConditionSignal(event->thread.condition);
         _aaxThreadJoin(event->thread.ptr);

         _aaxConditionDestroy(event->thread.condition);
         _aaxMutexDestroy(event->thread.mutex);
         _aaxThreadDestroy(event->thread.ptr);
      }

      _intBufErase(&event->emitters, _AAX_EMITTER_CACHE, _aaxFreeEmitterCache);
      _intBufErase(&event->buffers, _AAX_BUFFER_CACHE, _aaxFreeBufferCache);
      _intBufErase(&event->frames, _AAX_FRAME_CACHE, _aaxFreeFrameCache);

      free(handle->eventmgr);
      handle->eventmgr = NULL;
   }
   else if (handle) {
      _aaxErrorSet(AAX_INVALID_STATE);
   } else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEventManagerSetState(aaxConfig config, enum aaxState state)
{
   _handle_t *handle = get_handle(config);
   int rv = AAX_FALSE;
   if (handle && handle->eventmgr)
   {
   }
   else if (handle) {
      _aaxErrorSet(AAX_INVALID_STATE);
   } else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API aaxFrame AAX_APIENTRY
aaxEventManagerCreateAudioFrame(aaxConfig config)
{
   _handle_t *handle = get_handle(config);
   aaxFrame rv = NULL;
   if (handle && handle->eventmgr)
   {
      rv = aaxAudioFrameCreate(config);
      if (rv)
      {
         _frame_cache_t *fr = malloc(sizeof(_frame_cache_t));
         if (fr)
         {
            _event_t *event = handle->eventmgr;

            aaxAudioFrameSetMode(rv, AAX_POSITION, AAX_RELATIVE);

            aaxMixerRegisterAudioFrame(config, rv);

            fr->frame = rv;
            fr->pos = _intBufAddData(event->frames, _AAX_FRAME_CACHE, fr);
         }
         else
         {
            aaxAudioFrameDestroy(rv);
            _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            rv = NULL;
         }
      }
      /* aaxAudioFrameCreate already sets an error */
   }
   else if (handle) {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   } else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API aaxEmitter AAX_APIENTRY
aaxEventManagerRegisterEmitter(aaxFrame frame, const char *file)
{
   _frame_t* handle = get_frame(frame);
   aaxEmitter rv = NULL;
   if (handle)
   {
      _handle_t *config = get_driver_handle(frame);
      if (config && config->eventmgr)
      {
         _event_t *event = config->eventmgr;
         aaxBuffer buffer = _aaxGetBufferFromCache(event, file);
         if (buffer)
         {
            rv = aaxEmitterCreate();
            if (rv)
            {
               _emitter_cache_t *em = malloc(sizeof(_emitter_cache_t));
               if (em)
               {
                  aaxEmitterAddBuffer(rv, buffer);
                  aaxEmitterSetMode(rv, AAX_POSITION, AAX_RELATIVE);

                  aaxAudioFrameRegisterEmitter(frame, rv);

                  em->emitter = rv;
                  em->frame = frame;
                  em->pos = _intBufAddData(event->emitters, _AAX_EMITTER_CACHE, em);
               }
               else
               {
                  aaxEmitterDestroy(rv);
                  _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
                  rv = NULL;
               }
            }
            /* aaxEmitterCreate already sets an error */
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER+1);
         }
      }
      else if (config) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_frame(frame);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEventManagerDestoryEmitter(aaxFrame frame, aaxEmitter emitter)
{
   _frame_t *handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      _handle_t *config = get_driver_handle(frame);
      if (config && config->eventmgr)
      {
      }
      else if (config) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   put_frame(frame);
   return rv;
}

AAX_API aaxEmitter AAX_APIENTRY
aaxEventManagerPlayTone(aaxConfig config, enum aaxWaveformType type, float frequency, float duration, float gain, float delay)
{
   _handle_t *handle = get_handle(config);
   aaxEmitter rv = NULL;
   if (handle && handle->eventmgr)
   {
      /* set up an event so the event thread knows what to do */
   }
   else if (handle) {
      _aaxErrorSet(AAX_INVALID_STATE);
   } else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API aaxEmitter AAX_APIENTRY
aaxEventManagerPlayFile(aaxConfig config, const char *file, float gain, float delay)
{
   _handle_t *handle = get_handle(config);
   aaxEmitter rv = NULL;
   if (handle && handle->eventmgr)
   {
      /* set up an event so the event thread knows what to do */
   }
   else if (handle) {
      _aaxErrorSet(AAX_INVALID_STATE);
   } else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

static aaxBuffer
_aaxGetBufferFromCache(_event_t *event, const char *fname)
{
   aaxBuffer rv = NULL;
   if (fname)
   {
   }
   return rv;
}

static void _aaxFreeFrameCache(void *ptr)
{
   _frame_cache_t *fr = ptr;
   _handle_t *handle;

   handle = get_driver_handle(fr->frame);

   aaxAudioFrameSetState(fr->frame, AAX_PROCESSED);
   aaxMixerDeregisterAudioFrame(handle, fr->frame);
   aaxAudioFrameDestroy(fr->frame);
   _intBufErase(&fr->event_queue, _AAX_EVENT_QUEUE, free);
   free(fr);
}

static void _aaxFreeBufferCache(void *ptr)
{
   _buffer_cache_t *buf = ptr;

   aaxBufferDestroy(buf->buffer);
   free(buf->fname);
   free(buf);
}

static void _aaxFreeEmitterCache(void *ptr)
{
   _emitter_cache_t *em = ptr;

   aaxEmitterSetState(em->emitter, AAX_PROCESSED);
   aaxAudioFrameDeregisterEmitter(em->frame, em->emitter);
   aaxEmitterDestroy(em->emitter);
   _intBufErase(&em->event_queue, _AAX_EVENT_QUEUE, free);
   free(em);
}

void*
_aaxEventThread(void *eventmgr)
{
   return NULL;
}
