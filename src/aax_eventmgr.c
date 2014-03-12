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

#if USE_MIDI

enum _aaxEventType
{
   /*
    * user events are defined in
    * enum aaxEventType in aax/eventmgr.h
    */
   AAX_EVENT_STREAM_FILE = (AAX_SYSTEM_EVENTS | AAX_SPECIAL_EVENTS),
   AAX_EVENT_PLAY_BUFFER,

   AAX_SPECIAL_SYSTEM_EVENTS_MAX
};

typedef struct
{
   aaxBuffer buffer;
   unsigned int ref_counter;
   char *fname;
} _buffer_cache_t;

typedef struct
{
   _emitter_t *emitter;
   _frame_t *frame;		/* parent frame */
   unsigned int buffer_pos;
   _intBuffers *events;
} _emitter_cache_t;

typedef struct
{
   _frame_t *frame;
// _intBuffers *events;
} _frame_cache_t;

static void _aaxFreeFrameCache(void*);
static void _aaxFreeBufferCache(void*);
static void _aaxFreeEmitterCache(void*);
static int _aaxEventStart(_aaxEventMgr*);
static int _aaxEventStop(_aaxEventMgr*);

static aaxBuffer _aaxGetBufferFromCache(_aaxEventMgr*, const char*);


AAX_API int AAX_APIENTRY
aaxEventManagerCreate(aaxConfig config)
{
   _handle_t *handle = get_handle(config);
   int rv = AAX_FALSE;
   if (handle && !handle->eventmgr)
   {
      handle->eventmgr = calloc(1, sizeof(_aaxEventMgr));
      if (handle->eventmgr)
      {
         _aaxEventMgr *eventmgr = handle->eventmgr;

         _intBufCreate(&eventmgr->frames, _AAX_FRAME_CACHE);
         _intBufCreate(&eventmgr->buffers, _AAX_BUFFER_CACHE);
         _intBufCreate(&eventmgr->emitters, _AAX_EMITTER_CACHE);

         rv = AAX_TRUE;
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
      _aaxEventMgr *eventmgr = handle->eventmgr;

      _intBufErase(&eventmgr->emitters,_AAX_EMITTER_CACHE,_aaxFreeEmitterCache);
      _intBufErase(&eventmgr->buffers, _AAX_BUFFER_CACHE, _aaxFreeBufferCache);
      _intBufErase(&eventmgr->frames, _AAX_FRAME_CACHE, _aaxFreeFrameCache);

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
      _aaxEventMgr *eventmgr = handle->eventmgr;
      switch(state)
      {
      case AAX_PLAYING:
         rv = _aaxEventStart(eventmgr);
         break;
      case AAX_STOPPED:
         rv = _aaxEventStop(eventmgr);
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
         break;
      }
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
         _frame_cache_t *fr = calloc(1, sizeof(_frame_cache_t));
         if (fr)
         {
            _aaxEventMgr *eventmgr = handle->eventmgr;

            aaxAudioFrameSetMode(rv, AAX_POSITION, AAX_RELATIVE);
            aaxMixerRegisterAudioFrame(config, rv);

            fr->frame = rv;
            fr->frame->cache_pos = _intBufAddData(eventmgr->frames,
                                                  _AAX_FRAME_CACHE, fr);
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
         _aaxEventMgr *eventmgr = config->eventmgr;
         aaxBuffer buffer = _aaxGetBufferFromCache(eventmgr, file);
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

                  em->frame = frame;
                  em->emitter = rv;
                  em->emitter->cache_pos = _intBufAddData(eventmgr->emitters,
                                                        _AAX_EMITTER_CACHE, em);
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

AAX_API aaxEvent AAX_APIENTRY
aaxEventManagerRegisterEvent(aaxConfig config, aaxEmitter emitter, enum aaxEventType event_type, aaxEventCallbackFn callback, void *user_data)
{
   _handle_t *handle = get_handle(config);
   aaxEvent rv = NULL;
   if (handle && handle->eventmgr)
   {
      if (event_type > AAX_EVENT_NONE && event_type < AAX_EVENT_MAX)
      {
         if (callback)
         {
            _emitter_t *em = get_emitter(emitter);
            if (em && em->cache_pos != UINT_MAX)
            {
               _aaxEventMgr *eventmgr = handle->eventmgr;
               unsigned int pos = em->cache_pos;
               _intBufferData *dptr;

               dptr = _intBufGet(eventmgr->emitters, _AAX_EMITTER_CACHE, pos);
               if (dptr)
               {
                  _emitter_cache_t *em_cache = _intBufGetDataPtr(dptr);
                  _event_t *ev = calloc(1, sizeof(_event_t));
                  if (ev)
                  {
                     ev->id = EVENT_ID;
                     ev->emitter_pos = pos;
                     ev->data = emitter;
                     ev->event = event_type;

                     ev->callback = callback;
                     ev->user_data = user_data;
         
                     _intBufAddData(em_cache->events, _AAX_EVENT_QUEUE, ev);
                     rv = ev;
                  }
                  else {
                     _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
                  }
                  _intBufReleaseData(dptr, _AAX_EMITTER_CACHE);
               }
               else {
                  _aaxErrorSet(AAX_INVALID_REFERENCE);
               }
            }
            else if (em) {
               _aaxErrorSet(AAX_INVALID_REFERENCE);
            } else {
               _aaxErrorSet(AAX_INVALID_PARAMETER+1);
            }
            put_emitter(emitter);
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER+3);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_ENUM);
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
aaxEventManagerDeregisterEvent(aaxConfig config, aaxEvent eventmgr)
{
   _handle_t *handle = get_handle(config);
   int rv = AAX_FALSE;
   if (handle && handle->eventmgr && eventmgr)
   {
      rv = AAX_TRUE;
   }
   else if (handle) {
      _aaxErrorSet(AAX_INVALID_STATE);
   } else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;

}

AAX_API aaxEmitter AAX_APIENTRY
aaxEventManagerPlayTone(aaxConfig config, enum aaxWaveformType type, float frequency, float duration, float gain, float delay)
{
   _handle_t *handle = get_handle(config);
   aaxEmitter rv = NULL;
   if (handle && handle->eventmgr)
   {
      /* set up an eventmgr so the eventmgr thread knows what to do */
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
      /* set up an eventmgr so the eventmgr thread knows what to do */
   }
   else if (handle) {
      _aaxErrorSet(AAX_INVALID_STATE);
   } else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

static int
_aaxEventStart(_aaxEventMgr *eventmgr)
{
   int res, rv = AAX_FALSE;

   assert(eventmgr);

   eventmgr->thread.ptr = _aaxThreadCreate();
   eventmgr->thread.condition = _aaxConditionCreate();
   eventmgr->thread.mutex = _aaxMutexCreate(eventmgr->thread.mutex);

   res = _aaxThreadStart(eventmgr->thread.ptr, _aaxEventThread, eventmgr, 20);
   if (res == 0)
   {
      eventmgr->thread.started = AAX_TRUE;
      rv = AAX_TRUE;
   }
   else {
      _aaxErrorSet(AAX_INVALID_STATE);
   }
   return rv;
}

static int
_aaxEventStop(_aaxEventMgr *eventmgr)
{
   int rv = AAX_FALSE;

   assert(eventmgr);

   if TEST_FOR_TRUE(eventmgr->thread.started)
   {
      eventmgr->thread.started = AAX_FALSE;
      _aaxConditionSignal(eventmgr->thread.condition);
      _aaxThreadJoin(eventmgr->thread.ptr);

      _aaxConditionDestroy(eventmgr->thread.condition);
      _aaxMutexDestroy(eventmgr->thread.mutex);
      _aaxThreadDestroy(eventmgr->thread.ptr);
   }

   return rv;
}

static aaxBuffer
_aaxGetBufferFromCache(_aaxEventMgr *eventmgr, const char *fname)
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
// _intBufErase(&fr->events, _AAX_EVENT_QUEUE, free);
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
   _intBufErase(&em->events, _AAX_EVENT_QUEUE, free);
   free(em);
}

void*
_aaxEventThread(void *eventmgr)
{
   return NULL;
}

#endif
