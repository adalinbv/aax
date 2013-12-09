/*
 * Copyright 2007-2013 by Erik Hofman.
 * Copyright 2009-2013 by Adalin B.V.
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
# include <string.h>
#endif
#ifdef HAVE_LIBIO_H
#include <libio.h>              /* for NULL */
#endif
#include <math.h>		/* for fabs */
#include <assert.h>
#include <stdio.h>

#include <base/gmath.h>

#include "api.h"
#include "devices.h"
#include "software/arch.h"
#include "software/ringbuffer.h"

static void _aaxFreeEmitterBuffer(void *);


AAX_API aaxEmitter AAX_APIENTRY
aaxEmitterCreate()
{
   aaxEmitter rv = NULL;
   unsigned long size;
   void *ptr1;
   char* ptr2;

   size = sizeof(_emitter_t) + sizeof(_aaxEmitter);
   ptr2 = (char*)size;

   size += sizeof(_oalRingBuffer2dProps);
   ptr1 = _aax_calloc(&ptr2, 1, size);
   if (ptr1)
   {
      _emitter_t* handle = (_emitter_t*)ptr1;
      _aaxEmitter* src;

      src = (_aaxEmitter*)((char*)ptr1 + sizeof(_emitter_t));
      handle->source = src;
      src->buffer_pos = UINT_MAX;

      assert(((long int)ptr2 & 0xF) == 0);
      src->props2d = (_oalRingBuffer2dProps*)ptr2;
      _aaxSetDefault2dProps(src->props2d);

      /* unfortunatelly postponing the allocation of the 3d data info buffer
       * is not possible since it prevents setting 3d position and orientation
       * before the emitter is set to 3d mode
       */
      src->props3d = _aax3dPropsCreate();
      if (src->props3d)
      {
          _SET_INITIAL(src->props3d);

         _intBufCreate(&src->buffers, _AAX_EMITTER_BUFFER);
         if (src->buffers)
         {
            handle->id = EMITTER_ID;
            handle->cache_pos = UINT_MAX;
            handle->mixer_pos = UINT_MAX;
            handle->looping = AAX_FALSE;
            _SET_INITIAL(src->props3d);

            rv = (aaxEmitter)handle;
         }
      }

      if (!src->buffers)
      {
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         _aax_aligned_free(handle->source->props3d->dprops3d);
         free(handle->source->props3d);
         free(handle);
      }
   }
   else {
      _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterDestroy(aaxEmitter emitter)
{
   _emitter_t *handle = get_emitter(emitter);
   int rv = AAX_FALSE;
   if (handle)
   {
      _aaxEmitter *src = handle->source;
      if (!handle->handle && _IS_PROCESSED(src->props3d))
      {
         _oalRingBufferDelayEffectData* effect;

         _intBufErase(&src->buffers, _AAX_EMITTER_BUFFER,_aaxFreeEmitterBuffer);

         free(_FILTER_GET2D_DATA(src, FREQUENCY_FILTER));
         free(_FILTER_GET2D_DATA(src, DYNAMIC_GAIN_FILTER));
         free(_FILTER_GET2D_DATA(src, TIMED_GAIN_FILTER));
         free(_EFFECT_GET2D_DATA(src, DYNAMIC_PITCH_EFFECT));
         free(_EFFECT_GET2D_DATA(src, TIMED_PITCH_EFFECT));

         effect = _EFFECT_GET2D_DATA(src, DELAY_EFFECT);
         if (effect) free(effect->history_ptr);
         free(effect);

         _intBufErase(&src->p3dq, _AAX_DELAYED3D, _aax_aligned_free);
         _aax_aligned_free(src->props3d->dprops3d);
         free(src->props3d);

         /* safeguard against using already destroyed handles */
         handle->id = 0xdeadbeef;
         free(handle);
         handle = 0;
         rv = AAX_TRUE;
      }
      else
      {
         put_emitter(emitter);
         _aaxErrorSet(AAX_INVALID_STATE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterAddBuffer(aaxEmitter emitter, aaxBuffer buf)
{
   _emitter_t* handle = get_emitter(emitter);
   int rv = AAX_FALSE;
   if (handle)
   {
      _buffer_t* buffer = get_buffer(buf);
      if (buffer && _oalRingBufferIsValid(buffer->ringbuffer))
      {
         const _aaxEmitter *src = handle->source;
         if (_intBufGetNumNoLock(src->buffers, _AAX_EMITTER_BUFFER) == 0) {
            handle->track = 0;
         }

         if (handle->track < _oalRingBufferGetParami(buffer->ringbuffer,
                                                     RB_NO_TRACKS))
         {
            _embuffer_t* embuf = malloc(sizeof(_embuffer_t));
            if (embuf)
            {
               embuf->ringbuffer = _oalRingBufferReference(buffer->ringbuffer);
               embuf->id = EMBUFFER_ID;
               embuf->buffer = buffer;
               buffer->ref_counter++;

               _intBufAddData(src->buffers, _AAX_EMITTER_BUFFER, embuf);

               rv = AAX_TRUE;
            }
            else {
               _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            }
         }
         else {
            _aaxErrorSet(AAX_INVALID_STATE);
         }
      }
      else if (buffer) {
         _aaxErrorSet(AAX_INVALID_STATE);
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterRemoveBuffer(aaxEmitter emitter)
{
   _emitter_t* handle = get_emitter(emitter);
   int rv = AAX_FALSE;
   if (handle)
   {
      _aaxEmitter *src = handle->source;
      if (_IS_PROCESSED(src->props3d) || src->buffer_pos > 0)
      {
         _intBufferData *buf;

         buf = _intBufPop(src->buffers, _AAX_EMITTER_BUFFER);
         if (buf)
         {
            _embuffer_t *embuf = _intBufGetDataPtr(buf);
            if (embuf)
            {
               assert(embuf->id == EMBUFFER_ID);

               free_buffer(embuf->buffer);
               _oalRingBufferDelete(embuf->ringbuffer);
               embuf->ringbuffer = NULL;
               embuf->id = 0xdeadbeef;
               free(embuf);
            }
            _intBufDestroyDataNoLock(buf);

            if (src->buffer_pos > 0) {
               src->buffer_pos--;
            }
            rv = AAX_TRUE;
         }
         else {
            _aaxErrorSet(AAX_INVALID_REFERENCE);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_STATE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API aaxBuffer AAX_APIENTRY
aaxEmitterGetBufferByPos(const aaxEmitter emitter, unsigned int pos, int copy)
{
   _emitter_t* handle = get_emitter(emitter);
   aaxBuffer rv = NULL;
   if (handle)
   {
      const _aaxEmitter *src = handle->source;
      _intBufferData *dptr;
      dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, pos);
      if (dptr)
      {
         _embuffer_t *embuf = _intBufGetDataPtr(dptr);
         _buffer_t* buf = embuf->buffer;

         assert(embuf->id == EMBUFFER_ID);
         if (copy) buf->ref_counter++;
         rv = embuf->buffer;
         _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API unsigned int AAX_APIENTRY
aaxEmitterGetNoBuffers(const aaxEmitter emitter, enum aaxState state)
{
   _emitter_t* handle = get_emitter(emitter);
   unsigned rv = 0;
   if (handle)
   {
      const _aaxEmitter *src = handle->source;
      switch (state)
      {
      case AAX_PROCESSED:
         if (_IS_PROCESSED(src->props3d)) {
            rv = _intBufGetNumNoLock(src->buffers, _AAX_EMITTER_BUFFER);
         } else if (src->buffer_pos > 0) {
            rv = src->buffer_pos;
         }
         break;
      case AAX_PLAYING:
         if (_IS_PLAYING(src->props3d)) {
            rv = _intBufGetNumNoLock(src->buffers, _AAX_EMITTER_BUFFER);
            rv -= src->buffer_pos;
         }
         break;
      case AAX_MAXIMUM:
         rv = _intBufGetNumNoLock(src->buffers, _AAX_EMITTER_BUFFER);
         break;
      default:
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetState(aaxEmitter emitter, enum aaxState state)
{
   _emitter_t* handle = get_emitter(emitter);
   int rv = AAX_FALSE;
   if (handle)
   {
      _aaxEmitter *src = handle->source;
      switch (state)
      {
      case AAX_PLAYING:
         if (!_IS_PLAYING(src->props3d) || _IS_STOPPED(src->props3d))
         {
            unsigned int num;
            num = _intBufGetNumNoLock(src->buffers, _AAX_EMITTER_BUFFER);
            if (num)
            {
               src->buffer_pos = 0;
               _SET_PLAYING(src->props3d);
            }
         }
         else if (_IS_PAUSED(src->props3d)) {
            _TAS_PAUSED(src->props3d, AAX_FALSE);
         }
         /* break not needed */
      case AAX_UPDATE:				/* update distance delay */
         if (handle->mixer_pos != UINT_MAX)	/* emitter is registered */
         {
            _handle_t *phandle = handle->handle;
            if (phandle->id == HANDLE_ID)
            {
               _intBufferData *dptr;
               dptr = _intBufGet(phandle->sensors, _AAX_SENSOR, 0);
               if (dptr)
               {
                  _sensor_t* sensor = _intBufGetDataPtr(dptr);
                  _aaxAudioFrame *pmixer = sensor->mixer;

                  _aaxEMitterResetDistDelay(src, pmixer);
                  _intBufReleaseData(dptr, _AAX_SENSOR);
               }
            }
            else if (phandle->id == AUDIOFRAME_ID)
            {
               _aaxAudioFrame *pmixer = ((_frame_t*)phandle)->submix;
               _aaxEMitterResetDistDelay(src, pmixer);
            }
         }
         rv = AAX_TRUE;
         break;
      case AAX_STOPPED:
         if (_IS_PLAYING(src->props3d))
         {
            if (!_PROP_DISTDELAY_IS_DEFINED(src->props3d))
            {
               _SET_PROCESSED(src->props3d);
               src->buffer_pos = UINT_MAX;
            }
            else {
               _SET_STOPPED(src->props3d);
            }
         }
         rv = AAX_TRUE;
         break;
      case AAX_PROCESSED:
         if (_IS_PLAYING(src->props3d))
         {
            _SET_PROCESSED(src->props3d);
            src->buffer_pos = UINT_MAX;
         }
         rv = AAX_TRUE;
         break;
      case AAX_SUSPENDED:
         if (_IS_PLAYING(src->props3d)) {
            _SET_PAUSED(src->props3d);
         }
         rv = AAX_TRUE;
         break;
      case AAX_INITIALIZED:	/* or rewind */
      {
         const _intBufferData* dptr;

         src->buffer_pos = 0;
         dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, 0);
         if (dptr)
         {
            _oalRingBufferEnvelopeInfo* env;

            _embuffer_t *embuf = _intBufGetDataPtr(dptr);
            _oalRingBufferRewind(embuf->ringbuffer);
            src->buffer_pos = 0;
            _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);

            env = _FILTER_GET2D_DATA(src, TIMED_GAIN_FILTER);
            if (env)
            {
               env->value = _FILTER_GET(src->props2d, TIMED_GAIN_FILTER, 0);
               env->stage =  env->pos = 0;
            }

            env = _EFFECT_GET2D_DATA(src, TIMED_PITCH_EFFECT);
            if (env)
            {
               env->value = _EFFECT_GET(src->props2d, TIMED_PITCH_EFFECT, 0);
               env->stage =  env->pos = 0;
            }
         }
         rv = AAX_TRUE;
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetFilter(aaxEmitter emitter, aaxFilter f)
{
   _emitter_t* handle = get_emitter(emitter);
   int rv = AAX_FALSE;
   if (handle)
   {
      _filter_t* filter = get_filter(f);
      if (filter)
      {
         _aaxEmitter *src = handle->source;
         int type = filter->pos;
         switch (filter->type)
         {
         case AAX_TIMED_GAIN_FILTER:
            _PROP_DISTDELAY_SET_DEFINED(src->props3d);
            /* break not needed */
         case AAX_FREQUENCY_FILTER:
         case AAX_VOLUME_FILTER:
         case AAX_DYNAMIC_GAIN_FILTER:
         {
            _oalRingBuffer2dProps *p2d = src->props2d;
            _FILTER_SET(p2d, type, 0, _FILTER_GET_SLOT(filter, 0, 0));
            _FILTER_SET(p2d, type, 1, _FILTER_GET_SLOT(filter, 0, 1));
            _FILTER_SET(p2d, type, 2, _FILTER_GET_SLOT(filter, 0, 2));
            _FILTER_SET(p2d, type, 3, _FILTER_GET_SLOT(filter, 0, 3));
            _FILTER_SET_STATE(p2d, type, _FILTER_GET_SLOT_STATE(filter));
            _FILTER_SWAP_SLOT_DATA(p2d, type, filter, 0);
            rv = AAX_TRUE;
            break;
         }      
         case AAX_DISTANCE_FILTER:
         {
            _oalRingBuffer3dProps *p3d = src->props3d;
            _FILTER_SET(p3d, type, 0, _FILTER_GET_SLOT(filter, 0, 0));
            _FILTER_SET(p3d, type, 1, _FILTER_GET_SLOT(filter, 0, 1));
            _FILTER_SET(p3d, type, 2, _FILTER_GET_SLOT(filter, 0, 2));
            _FILTER_SET(p3d, type, 3, _FILTER_GET_SLOT(filter, 0, 3));
            _FILTER_SET_STATE(p3d, type, _FILTER_GET_SLOT_STATE(filter));
            _FILTER_SWAP_SLOT_DATA(p3d, type, filter, 0);
            rv = AAX_TRUE;
            break;
         }
         case AAX_ANGULAR_FILTER:
         {
            _oalRingBuffer3dProps *p3d = src->props3d;
            float inner_vec = _FILTER_GET_SLOT(filter, 0, 0);
            float outer_vec = _FILTER_GET_SLOT(filter, 0, 1);
            float outer_gain = _FILTER_GET_SLOT(filter, 0, 2);
            float tmp = _FILTER_GET_SLOT(filter, 0, 3);

            if ((inner_vec >= 0.995f) || (outer_gain >= 0.99f)) {
               _PROP_CONE_CLEAR_DEFINED(p3d);
            } else {
               _PROP_CONE_SET_DEFINED(p3d);
            }
            _FILTER_SET(p3d, type, 0, inner_vec);
            _FILTER_SET(p3d, type, 1, outer_vec);
            _FILTER_SET(p3d, type, 2, outer_gain);
            _FILTER_SET(p3d, type, 3, tmp);
            _FILTER_SET_STATE(p3d, type, _FILTER_GET_SLOT_STATE(filter));
            _FILTER_SWAP_SLOT_DATA(p3d, type, filter, 0);
            rv = AAX_TRUE;
            break;
         }
         default:
            _aaxErrorSet(AAX_INVALID_ENUM);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxEmitterGetFilter(const aaxEmitter emitter, enum aaxFilterType type)
{
   _emitter_t* handle = get_emitter(emitter);
   aaxFilter rv = AAX_FALSE;
   if (handle)
   {
      switch(type)
      {
      case AAX_FREQUENCY_FILTER:
      case AAX_VOLUME_FILTER:
      case AAX_TIMED_GAIN_FILTER:
      case AAX_DYNAMIC_GAIN_FILTER:
      case AAX_DISTANCE_FILTER:
      case AAX_ANGULAR_FILTER:
      {
         _aaxEmitter *src = handle->source;
         _handle_t *cfg = get_driver_handle(handle->handle);
         _aaxMixerInfo* info = (cfg) ? cfg->info : _info;
         rv = new_filter_handle(info, type, src->props2d, src->props3d);
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetEffect(aaxEmitter emitter, aaxEffect e)
{
   _emitter_t* handle = get_emitter(emitter);
   int rv = AAX_FALSE;
   if (handle)
   {
      _filter_t* effect = get_effect(e);
      if (effect)
      {
         _aaxEmitter *src = handle->source;
         int type = effect->pos;
         switch (effect->type)
         {
         case AAX_PITCH_EFFECT:
         case AAX_TIMED_PITCH_EFFECT:
            _PROP_PITCH_SET_CHANGED(src->props3d);
            /* break not needed */
         case AAX_DISTORTION_EFFECT:
         {
            _oalRingBuffer2dProps *p2d = src->props2d;
            _EFFECT_SET(p2d, type, 0, _EFFECT_GET_SLOT(effect, 0, 0));
            _EFFECT_SET(p2d, type, 1, _EFFECT_GET_SLOT(effect, 0, 1));
            _EFFECT_SET(p2d, type, 2, _EFFECT_GET_SLOT(effect, 0, 2));
            _EFFECT_SET(p2d, type, 3, _EFFECT_GET_SLOT(effect, 0, 3));
            _EFFECT_SET_STATE(p2d, type, _EFFECT_GET_SLOT_STATE(effect));
            _EFFECT_SWAP_SLOT_DATA(p2d, type, effect, 0);
            rv = AAX_TRUE;
            break;
         }
         case AAX_FLANGING_EFFECT:
         case AAX_PHASING_EFFECT:
         case AAX_CHORUS_EFFECT:
         {
            _oalRingBuffer2dProps *p2d = src->props2d;
            _EFFECT_SET(p2d, type, 0, _EFFECT_GET_SLOT(effect, 0, 0));
            _EFFECT_SET(p2d, type, 1, _EFFECT_GET_SLOT(effect, 0, 1));
            _EFFECT_SET(p2d, type, 2, _EFFECT_GET_SLOT(effect, 0, 2));
            _EFFECT_SET(p2d, type, 3, _EFFECT_GET_SLOT(effect, 0, 3));
            _EFFECT_SET_STATE(p2d, type, _EFFECT_GET_SLOT_STATE(effect));
            _EFFECT_SWAP_SLOT_DATA(p2d, type, effect, 0);
            if (_intBufGetNumNoLock(src->buffers, _AAX_EMITTER_BUFFER) > 1)
            {
               _oalRingBufferDelayEffectData* data;
               data = _EFFECT_GET2D_DATA(src, DELAY_EFFECT);
               if (data && !data->history_ptr)
               {
                  unsigned int tracks = effect->info->no_tracks;
                  float frequency = effect->info->frequency;
                  _oalRingBufferCreateHistoryBuffer(&data->history_ptr,
                                                    data->delay_history,
                                                    frequency, tracks,
                                                    DELAY_EFFECTS_TIME);
               }
            }
            rv = AAX_TRUE;
            break;
         }
         case AAX_DYNAMIC_PITCH_EFFECT:
         {
            _oalRingBuffer2dProps *p2d = src->props2d;
            _oalRingBufferLFOInfo *lfo;

            _EFFECT_SET(p2d, type, 0, _EFFECT_GET_SLOT(effect, 0, 0));
            _EFFECT_SET(p2d, type, 1, _EFFECT_GET_SLOT(effect, 0, 1));
            _EFFECT_SET(p2d, type, 2, _EFFECT_GET_SLOT(effect, 0, 2));
            _EFFECT_SET(p2d, type, 3, _EFFECT_GET_SLOT(effect, 0, 3));
            _EFFECT_SET_STATE(p2d, type, _EFFECT_GET_SLOT_STATE(effect));
            _EFFECT_SWAP_SLOT_DATA(p2d, type, effect, 0);

            lfo = _EFFECT_GET_DATA(p2d, DYNAMIC_PITCH_EFFECT);
            if (lfo) /* enabled */
            {
               float lfo_val = _EFFECT_GET_SLOT(effect, 0, AAX_LFO_FREQUENCY);
               _PROP_DYNAMIC_PITCH_SET_DEFINED(src->props3d);
		/*
		 * The vibrato effect is not gradual like tremolo but is
		 * adjusted every update and stays constant which requires
		 * the fastest update rate when the LFO is faster than 1Hz.
		 */
               if ((lfo_val > 1.0f) && (src->update_rate < 4*lfo_val)) {
                  src->update_rate = 1;
               }
            }
            else
            { 
               _PROP_DYNAMIC_PITCH_CLEAR_DEFINED(src->props3d);
               src->update_rate = 0;
            }
            rv = AAX_TRUE;
            break;
         }
         case AAX_VELOCITY_EFFECT:
         {
            _oalRingBuffer3dProps *p3d = src->props3d;
            _EFFECT_SET(p3d, type, 0, _EFFECT_GET_SLOT(effect, 0, 0));
            _EFFECT_SET(p3d, type, 1, _EFFECT_GET_SLOT(effect, 0, 1));
            _EFFECT_SET(p3d, type, 2, _EFFECT_GET_SLOT(effect, 0, 2));
            _EFFECT_SET(p3d, type, 3, _EFFECT_GET_SLOT(effect, 0, 3));
            _EFFECT_SET_STATE(p3d, type, _EFFECT_GET_SLOT_STATE(effect));
            _EFFECT_SWAP_SLOT_DATA(p3d,  type, effect, 0);
            rv = AAX_TRUE;
            break;
         }
         default:
            _aaxErrorSet(AAX_INVALID_ENUM);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API aaxEffect AAX_APIENTRY
aaxEmitterGetEffect(const aaxEmitter emitter, enum aaxEffectType type)
{
   _emitter_t* handle = get_emitter(emitter);
   aaxEffect rv = AAX_FALSE;
   if (handle)
   {
      switch(type)
      {
      case AAX_PITCH_EFFECT:
      case AAX_TIMED_PITCH_EFFECT:
      case AAX_DYNAMIC_PITCH_EFFECT:
      case AAX_DISTORTION_EFFECT:
      case AAX_PHASING_EFFECT:
      case AAX_CHORUS_EFFECT:
      case AAX_FLANGING_EFFECT:
      case AAX_VELOCITY_EFFECT:
      {
         _aaxEmitter *src = handle->source;
         rv = new_effect_handle(src->info, type, src->props2d, src->props3d);
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetMode(aaxEmitter emitter, enum aaxModeType type, int mode)
{
   _emitter_t* handle = get_emitter(emitter);
   int rv = AAX_FALSE;
   if (handle)
   {
      _aaxEmitter *src = handle->source;
      switch(type)
      {
      case AAX_POSITION:
      {
         int m = (mode > AAX_MODE_NONE) ? AAX_TRUE : AAX_FALSE;
         _TAS_POSITIONAL(src->props3d, m);
         if TEST_FOR_TRUE(m)
         {
            m = (mode == AAX_RELATIVE) ? AAX_TRUE : AAX_FALSE;
            _TAS_RELATIVE(src->props3d, m);
            if TEST_FOR_TRUE(m)
            {
               src->props3d->dprops3d->matrix[LOCATION][3] = 0.0f;
               src->props3d->dprops3d->velocity[VELOCITY][3] = 0.0f;
            }
            else
            {
               src->props3d->dprops3d->matrix[LOCATION][3] = 1.0f;
               src->props3d->dprops3d->velocity[VELOCITY][3] = 1.0f;
            }
         }
         rv = AAX_TRUE;
         break;
      }
      case AAX_LOOPING:
      {
         _intBufferData *dptr =_intBufGet(src->buffers, _AAX_EMITTER_BUFFER, 0);
         if (dptr)
         {
            _embuffer_t *embuf = _intBufGetDataPtr(dptr);
            _oalRingBufferSetParami(embuf->ringbuffer, RB_LOOPING, mode);
            _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);
         }
         handle->looping = mode;
         rv = AAX_TRUE;
         break;
      }
      case AAX_BUFFER_TRACK:
      {
         _intBufferData *dptr =_intBufGet(src->buffers, _AAX_EMITTER_BUFFER, 0);
         if (dptr)
         {
            _embuffer_t *embuf = _intBufGetDataPtr(dptr);
            if (mode < _oalRingBufferGetParami(embuf->buffer->ringbuffer, RB_NO_TRACKS))
            {
               handle->track = mode;
               rv = AAX_TRUE;
            }
            _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);
         }
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetMatrix(aaxEmitter emitter, aaxMtx4f mtx)
{
   _emitter_t *handle = get_emitter(emitter);
   int rv = AAX_FALSE;
   if (handle)
   {
      if (mtx && !detect_nan_vec4(mtx[0]) && !detect_nan_vec4(mtx[1]) &&
                 !detect_nan_vec4(mtx[2]) && !detect_nan_vec4(mtx[3]))
      {
         _aaxEmitter *src = handle->source;
         mtx4Copy(src->props3d->dprops3d->matrix, mtx);
         if (_IS_RELATIVE(src->props3d)) {
            src->props3d->dprops3d->matrix[LOCATION][3] = 0.0f;
         } else {
            src->props3d->dprops3d->matrix[LOCATION][3] = 1.0f;
         }
         _PROP_MTX_SET_CHANGED(src->props3d);
         rv = AAX_TRUE;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetVelocity(aaxEmitter emitter, const aaxVec3f velocity)
{
   _emitter_t* handle = get_emitter(emitter);
   int rv = AAX_FALSE;
   if (handle)
   {
      if (velocity && !detect_nan_vec3(velocity))
      {
         _oalRingBufferDelayed3dProps *dp3d;
 
         dp3d = handle->source->props3d->dprops3d;
         vec3Copy(dp3d->velocity[VELOCITY], velocity);
         _PROP_SPEED_SET_CHANGED(handle->source->props3d);

         rv = AAX_TRUE;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterGetMatrix(const aaxEmitter emitter, aaxMtx4f mtx)
{
   _emitter_t *handle = get_emitter(emitter);
   int rv = AAX_FALSE;
   if (handle)
   {
      if (mtx)
      {
         _aaxEmitter *src = handle->source;
         mtx4Copy(mtx, src->props3d->dprops3d->matrix);
         rv = AAX_TRUE;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetOffset(aaxEmitter emitter, unsigned long offs, enum aaxType type)
{
   _emitter_t* handle = get_emitter(emitter);
   int rv = AAX_FALSE;
   if (handle)
   {
      _aaxEmitter *src = handle->source;
      _intBufferData *dptr;

      _intBufGetNum(src->buffers, _AAX_EMITTER_BUFFER);
      dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, 0);
      if (dptr)
      {
         _embuffer_t *embuf = _intBufGetDataPtr(dptr);
         _oalRingBuffer *rb = embuf->ringbuffer;
         float duration, fpos = (float)offs*1e-6f;
         unsigned int samples, pos = 0;

         switch (type)
         {
         case AAX_BYTES:   
            offs /= _oalRingBufferGetParami(rb, RB_BYTES_SAMPLE);
         case AAX_FRAMES:
         case AAX_SAMPLES:
            samples = _oalRingBufferGetParami(rb, RB_NO_SAMPLES);
            while (offs > samples)
            {
               pos++;
               offs -= samples;
               _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);

               dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, pos);
               if (!dptr) break;

               embuf = _intBufGetDataPtr(dptr);
               rb = embuf->ringbuffer;
               samples = _oalRingBufferGetParami(rb, RB_NO_SAMPLES);
            }
            if (dptr)
            {
               handle->mixer_pos = pos;
               _oalRingBufferSetParami(rb, RB_OFFSET_SAMPLES, offs);
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_MICROSECONDS:
            duration = _oalRingBufferGetParamf(rb, RB_DURATION_SEC);
            while (fpos > duration)
            {
               pos++;
               fpos -= duration;
               _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);

               dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, pos);
               if (!dptr) break;

               embuf = _intBufGetDataPtr(dptr);
               rb = embuf->ringbuffer;
               duration = _oalRingBufferGetParamf(rb, RB_DURATION_SEC);
            }
            if (dptr)
            {
               handle->mixer_pos = pos;
               _oalRingBufferSetParamf(rb, RB_OFFSET_SEC, fpos);
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
          default:
            _aaxErrorSet(AAX_INVALID_ENUM);
         }
         _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);
         
      }
      else {
         _aaxErrorSet(AAX_INVALID_REFERENCE);
      }
      _intBufReleaseNum(src->buffers, _AAX_EMITTER_BUFFER);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetOffsetSec(aaxEmitter emitter, float offs)
{
   _emitter_t* handle = get_emitter(emitter);
   int rv = AAX_FALSE;
   if (handle)
   {
      if (!is_nan(offs))
      {
         _aaxEmitter *src = handle->source;
         _intBufferData *dptr;

         _intBufGetNum(src->buffers, _AAX_EMITTER_BUFFER);
         dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, 0);
         if (dptr)
         {
            _embuffer_t *embuf = _intBufGetDataPtr(dptr);
            _oalRingBuffer *rb = embuf->ringbuffer;
            unsigned int pos = 0;
            float duration;

            duration = _oalRingBufferGetParamf(rb, RB_DURATION_SEC);
            while (offs > duration)
            {
               pos++;
               offs -= duration;
               _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);

               dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, pos);
               if (!dptr) break;

               embuf = _intBufGetDataPtr(dptr);
               rb = embuf->ringbuffer;
               duration = _oalRingBufferGetParamf(rb, RB_DURATION_SEC);
            }
            if (dptr)
            {
               handle->mixer_pos = pos;
               _oalRingBufferSetParamf(rb, RB_OFFSET_SEC, offs);
               rv = AAX_TRUE;
            }
            else {
               _aaxErrorSet(AAX_INVALID_PARAMETER);
            }
            _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);
         }
         else {
            _aaxErrorSet(AAX_INVALID_REFERENCE);
         }
         _intBufReleaseNum(src->buffers, _AAX_EMITTER_BUFFER);
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterGetMode(const aaxEmitter emitter, enum aaxModeType type)
{
   _emitter_t* handle = get_emitter(emitter);
   int rv = AAX_FALSE;
   if (handle)
   {
      const _aaxEmitter *src = handle->source;
      switch(type)
      {
      case AAX_POSITION:
         if (_IS_POSITIONAL(src->props3d))
         {
            if (_IS_RELATIVE(src->props3d)) {
               rv = AAX_RELATIVE;
            } else {
               rv = AAX_ABSOLUTE;
            }
         } else {
            rv = AAX_MODE_NONE;
         }
         break;
      case AAX_LOOPING:
      {
         _intBufferData *dptr =_intBufGet(src->buffers, _AAX_EMITTER_BUFFER, 0);
         if (dptr)
         {
            _embuffer_t *embuf = _intBufGetDataPtr(dptr);
            rv = _oalRingBufferGetParami(embuf->ringbuffer, RB_LOOPING);
            _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);
         }
         break;
      }
      case AAX_BUFFER_TRACK:
         rv = handle->track;
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
         break;
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterGetVelocity(const aaxEmitter emitter, aaxVec3f velocity)
{
   _emitter_t* handle = get_emitter(emitter);
   int rv = AAX_FALSE;
   if (handle)
   {
      if (velocity)
      {
         _oalRingBufferDelayed3dProps *dp3d;

         dp3d = handle->source->props3d->dprops3d;
         vec3Copy(velocity, dp3d->velocity[VELOCITY]);

         rv = AAX_TRUE;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterGetState(const aaxEmitter emitter)
{
   _emitter_t* handle = get_emitter(emitter);
   enum aaxState ret = AAX_STATE_NONE;
   if (handle)
   {
      _handle_t *thread = get_valid_handle(handle->handle);
      if (thread)
      {
         const _aaxEmitter *src = handle->source;
         if (_IS_PROCESSED(src->props3d)) ret = AAX_PROCESSED;
         else if (_IS_STOPPED(src->props3d)) ret = AAX_STOPPED;
         else if (_IS_PAUSED(src->props3d)) ret = AAX_SUSPENDED;
         else if (_IS_PLAYING(src->props3d)) ret = AAX_PLAYING;
         else ret = AAX_INITIALIZED;
       }
       else ret = AAX_INITIALIZED;
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return ret;
}

AAX_API unsigned long AAX_APIENTRY
aaxEmitterGetOffset(const aaxEmitter emitter, enum aaxType type)
{
   _emitter_t* handle = get_emitter(emitter);
   unsigned long rv = 0;
   if (handle)
   {
      const _aaxEmitter *src = handle->source;
      _intBufferData *dptr;
      dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, 0);
      if (dptr)
      {
         _embuffer_t *embuf = _intBufGetDataPtr(dptr);
         unsigned int i;

         switch (type)
         {
         case AAX_BYTES:
         case AAX_FRAMES:
         case AAX_SAMPLES:
            _intBufGetNum(src->buffers, _AAX_EMITTER_BUFFER);
            for (i=0; i<handle->mixer_pos; i++)
            {
               rv += _oalRingBufferGetParami(embuf->ringbuffer, RB_NO_SAMPLES);
               _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);

               dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, i);
               embuf = _intBufGetDataPtr(dptr);
            }
            _intBufReleaseNum(src->buffers, _AAX_EMITTER_BUFFER);

            rv += _oalRingBufferGetParami(embuf->ringbuffer, RB_OFFSET_SAMPLES);
            if (type == AAX_BYTES) {
               rv *= _oalRingBufferGetParami(embuf->ringbuffer, RB_BYTES_SAMPLE);
            }
            break;
         case AAX_MICROSECONDS:
            rv = (unsigned long)(src->curr_pos_sec * 1e6f);
            break;
         default:
            _aaxErrorSet(AAX_INVALID_ENUM);
         }
         _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);
      }
      else {
         _aaxErrorSet(AAX_INVALID_REFERENCE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

AAX_API float AAX_APIENTRY
aaxEmitterGetOffsetSec(const aaxEmitter emitter)
{
   _emitter_t* handle = get_emitter(emitter);
   float rv = 0.0f;
   if (handle)
   {
      const _aaxEmitter *src = handle->source;
      rv = src->curr_pos_sec;
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_emitter(handle);
   return rv;
}

/* -------------------------------------------------------------------------- */

_emitter_t*
get_emitter_unregistered(aaxEmitter em)
{
   _emitter_t *emitter = (_emitter_t *)em;
   _emitter_t *rv = NULL;

   if (emitter && emitter->id == EMITTER_ID && !emitter->handle) {
      rv = emitter;
   }
   return rv;
}

_emitter_t*
get_emitter(aaxEmitter em)
{
   _emitter_t *emitter = (_emitter_t *)em;
   _emitter_t *rv = NULL;

   if (emitter && emitter->id == EMITTER_ID)
   {
      _handle_t *handle = emitter->handle;
      if (handle && handle->id == HANDLE_ID)
      {
         _intBufferData *dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = sensor->mixer;
            _intBufferData *dptr_src;
            _intBuffers *he;

            if (!_IS_POSITIONAL(emitter->source->props3d)) {
               he = mixer->emitters_2d;
            } else {
               he = mixer->emitters_3d;
            }
            dptr_src = _intBufGet(he, _AAX_EMITTER, emitter->mixer_pos);
            emitter = _intBufGetDataPtr(dptr_src);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
      else if (handle && handle->id == AUDIOFRAME_ID)
      {
         _frame_t *handle = (_frame_t*)emitter->handle;
         _intBufferData *dptr_src;
         _intBuffers *he;

         if (!_IS_POSITIONAL(emitter->source->props3d)) {
            he = handle->submix->emitters_2d;
         } else {
            he = handle->submix->emitters_3d;
         }
         dptr_src = _intBufGet(he, _AAX_EMITTER, emitter->mixer_pos);
         emitter = _intBufGetDataPtr(dptr_src);
      }
      rv = emitter;
   }
   return rv;
}

void
put_emitter(aaxEmitter em)
{
   _emitter_t *emitter = (_emitter_t *)em;

   if (emitter && emitter->id == EMITTER_ID)
   {
      _handle_t *handle = emitter->handle;
      if (handle && handle->id == HANDLE_ID)
      {
          _intBufferData *dptr =_intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = sensor->mixer;
            _intBuffers *he;

            if (!_IS_POSITIONAL(emitter->source->props3d)) {
               he = mixer->emitters_2d;
            } else {
               he = mixer->emitters_3d;
            }
            _intBufRelease(he, _AAX_EMITTER, emitter->mixer_pos);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
      else if (handle && handle->id == AUDIOFRAME_ID)
      {
         _frame_t *handle = (_frame_t*)emitter->handle;
         _intBuffers *he;

         if (!_IS_POSITIONAL(emitter->source->props3d)) {
            he = handle->submix->emitters_2d;
         } else {
            he = handle->submix->emitters_3d;
         }
         _intBufRelease(he, _AAX_EMITTER, emitter->mixer_pos);
      }
   }
}

void
_aaxEMitterResetDistDelay(_aaxEmitter *src, _aaxAudioFrame *mixer)
{
   int dist_delaying;

   assert(src);
   assert(mixer);

   dist_delaying = _FILTER_GET_STATE(src->props3d, DISTANCE_FILTER);
   dist_delaying |= _FILTER_GET_STATE(mixer->props3d, DISTANCE_FILTER);
   dist_delaying &= AAX_DISTANCE_DELAY;
   if (dist_delaying)
   {
      _oalRingBuffer3dProps *fp3d = mixer->props3d;
      _oalRingBufferDelayed3dProps *fdp3d_m = fp3d->m_dprops3d;
      _oalRingBuffer3dProps *ep3d = src->props3d;
      _oalRingBufferDelayed3dProps *edp3d_m = ep3d->m_dprops3d;
      _oalRingBufferDelayed3dProps *edp3d = ep3d->dprops3d;
      _oalRingBuffer2dProps *ep2d = src->props2d;
      float dist, vs;

      vs = _EFFECT_GET(fp3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);

      /**
       * Align the modified emitter matrix with the sensor by multiplying 
       * the emitter matrix by the modified frame matrix.
       */ 
      mtx4Mul(edp3d_m->matrix, fdp3d_m->matrix, edp3d->matrix);
      dist = vec3Magnitude(edp3d_m->matrix[LOCATION]);
      ep2d->dist_delay_sec = dist / vs;

#if 0
 printf("# emitter parent:\t\t\t\temitter:\n");
 PRINT_MATRICES(fdp3d_m->matrix, edp3d->matrix);
 printf("# modified emitter\n");
 PRINT_MATRIX(edp3d_m->matrix);
 printf("delay: %f, dist: %f, vs: %f\n", ep2d->dist_delay_sec, dist, vs);
#endif

      _PROP_DISTQUEUE_SET_DEFINED(ep3d);
      src->props3d->buf3dq_step = 1.0f;

      if (!src->p3dq) {
         _intBufCreate(&src->p3dq, _AAX_DELAYED3D);
      } else {
         _intBufClear(src->p3dq, _AAX_DELAYED3D, _aax_aligned_free);
      }
   }
}

static void
_aaxFreeEmitterBuffer(void *sbuf)
{
   _embuffer_t *embuf = (_embuffer_t*)sbuf;

   free_buffer(embuf->buffer);
   _oalRingBufferDelete(embuf->ringbuffer);
   embuf->ringbuffer = NULL;
   embuf->id = 0xdeadbeef;
   free(embuf);
   embuf = NULL;
}

