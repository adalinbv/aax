/*
 * Copyright 2007-2023 by Erik Hofman.
 * Copyright 2009-2023 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif
#include <stdio.h>              /* for NULL */
#include <math.h>		/* for fabs */
#include <assert.h>
#include <stdio.h>

#include <xml.h>

#include <base/gmath.h>

#include <dsp/filters.h>
#include <dsp/effects.h>
#include <dsp/lfo.h>

#include "api.h"
#include "arch.h"
#include "ringbuffer.h"


static void _aaxFreeEmitterBuffer(void *);
static int _emitterSetFilter(_emitter_t*, _filter_t*);
static int _emitterSetEffect(_emitter_t*, _effect_t*);
static void _emitterSetPitch(const _aaxEmitter*, _aax2dProps *);

AAX_API aaxEmitter AAX_APIENTRY
aaxEmitterCreate()
{
   aaxEmitter rv = NULL;
   size_t offs, size;
   void *ptr1;
   char* ptr2;

   offs = sizeof(_emitter_t) + sizeof(_aaxEmitter);
   size = sizeof(_aax2dProps);
   ptr1 = _aax_calloc(&ptr2, offs, 1, size);
   if (ptr1)
   {
      _emitter_t* handle = (_emitter_t*)ptr1;
      _aaxEmitter* src;

      src = (_aaxEmitter*)((char*)ptr1 + sizeof(_emitter_t));
      handle->source = src;
      src->buffer_pos = UINT_MAX;

      assert(((long int)ptr2 & MEMMASK) == 0);
      src->props2d = (_aax2dProps*)ptr2;
      _aaxSetDefault2dProps(src->props2d);

      /* unfortunatelly postponing the allocation of the 3d data info buffer
       * is not possible since it prevents setting 3d position and orientation
       * before the emitter is set to 3d mode
       */
      src->props3d = _aax3dPropsCreate();
      if (src->props3d)
      {
          _PROP_MTX_SET_CHANGED(src->props3d);
          _SET_INITIAL(src->props3d);

         _intBufCreate(&src->buffers, _AAX_EMITTER_BUFFER);
         if (src->buffers)
         {
            handle->id = EMITTER_ID;
            handle->mixer_pos = UINT_MAX;
            handle->looping = -1;
            _SET_INITIAL(src->props3d);

            handle->midi.attack_factor = 1.0f;
            handle->midi.release_factor = 1.0f;
            handle->midi.decay_factor = 1.0f;
            handle->midi.mode = AAX_RENDER_NORMAL;

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
      __aaxErrorSet(AAX_INSUFFICIENT_RESOURCES, __func__);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterDestroy(aaxEmitter emitter)
{
   _emitter_t *handle = get_emitter(emitter, _LOCK, __func__);
   int rv = AAX_FALSE;
   if (handle)
   {
//    _aaxRingBufferDelayEffectData* data;
      _aaxEmitter *src = handle->source;

      if (!handle->parent && _IS_PROCESSED(src->props3d))
      {
         int i;

         _intBufErase(&src->buffers, _AAX_EMITTER_BUFFER,_aaxFreeEmitterBuffer);

         _aaxMutexLock(src->props2d->mutex);
         for (i=0; i<MAX_STEREO_FILTER; ++i) {
            _FILTER_FREE2D_DATA(src, i);
         }
         for (i=0; i<MAX_STEREO_EFFECT; ++i) {
            _EFFECT_FREE2D_DATA(src, i);
         }
         _aaxMutexDestroy(src->props2d->mutex);

         _aaxMutexLock(src->props3d->mutex);
         for (i=0; i<MAX_3D_FILTER; ++i) {
            _FILTER_FREE3D_DATA(src, i);
         }
         for (i=0; i<MAX_3D_EFFECT; ++i) {
            _EFFECT_FREE3D_DATA(src, i);
         }
         _aaxMutexDestroy(src->props3d->mutex);

         _intBufErase(&src->p3dq, _AAX_DELAYED3D, _aax_aligned_free);
         _aax_aligned_free(src->props3d->dprops3d);
         free(src->props3d);

         /* safeguard against using already destroyed handles */
         handle->id = FADEDBAD;
         free(handle);
         handle = 0;
         rv = AAX_TRUE;
      }
      else
      {
         put_emitter(handle);
         _aaxErrorSet(AAX_INVALID_STATE);
      }
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterAddBuffer(aaxEmitter emitter, aaxBuffer buf)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
   _buffer_t* buffer = get_buffer(buf, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
          _aaxErrorSet(AAX_INVALID_HANDLE);
      }
      else
      {
         if (!buffer) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
         else
         {
            _aaxRingBuffer *rb = buffer->ringbuffer[0];
            if (rb)
            {
               if (!rb->get_state(rb, RB_IS_VALID)) {
                  _aaxErrorSet(AAX_INVALID_STATE);
               } else if (handle->track >= rb->get_parami(rb, RB_NO_TRACKS)) {
                  _aaxErrorSet(AAX_INVALID_STATE);
               }
               else
               {
                  if (!handle->midi.mode) {
                     handle->midi.mode = buffer->midi_mode;
                  }
                  rv = AAX_TRUE;
               }
            } else if (!buffer->aaxs) {
               _aaxErrorSet(AAX_INVALID_STATE);
            } else {
               rv = AAX_TRUE;
            }
         }
      }
   }

   if (rv)
   {
      _aax2dProps *ep2d = handle->source->props2d;
      float pitch = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_PITCH);
      int mip_level = 0;
      _aaxRingBuffer *rb;

      if (!buffer->root) {
         buffer->root = handle->root;
      }
      if (!handle->root) {
         handle->root = buffer->root;
      }

      ep2d->mip_levels = buffer->mip_levels;
      if (ep2d->mip_levels > 1)
      {
         mip_level = _getMaxMipLevels(ceilf(pitch));
         if (mip_level >= ep2d->mip_levels) {
            mip_level = ep2d->mip_levels-1;
         }
         ep2d->pitch_factor = 1.0f/(float)(1 << (mip_level));
      }

      rb = buffer->ringbuffer[mip_level];
      if (rb)
      {
         const _aaxEmitter *src = handle->source;
         _embuffer_t* embuf;

         _EFFECT_SET(ep2d, PITCH_EFFECT, AAX_MAX_PITCH,
                           _MAX(4.0f, (float)(1 << buffer->mip_levels)));
         if (handle->looping >= 0) {
            rb->set_parami(rb, RB_LOOPING, handle->looping);
         }
         embuf = calloc(1, sizeof(_embuffer_t));
         if (embuf)
         {
            embuf->ringbuffer = rb->reference(rb);
            embuf->id = EMBUFFER_ID;
            embuf->buffer = buffer;
            buffer->ref_counter++;
            handle->sampled_release = buffer->info.sampled_release;

            _intBufAddData(src->buffers, _AAX_EMITTER_BUFFER, embuf);
         }
         else
         {
            _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            rv = AAX_FALSE;
         }
         _emitterCreateEFFromRingbuffer(handle, embuf);
         _emitterSetPitch(src, ep2d);

#if 0
         // This is done when the buffer is being used for the first time
         // in software/emitters.c
         if (rv && buffer->aaxs) {
            rv = _emitterCreateEFFromAAXS(handle, embuf, buffer->aaxs);
         }
#endif
      }
   }
   put_emitter(handle);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterRemoveBuffer(aaxEmitter emitter)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
   int rv = __release_mode;

   if (!rv && handle)
   {
      _aaxEmitter *src = handle->source;
      if (!_IS_PROCESSED(src->props3d) && src->buffer_pos == 0) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxEmitter *src = handle->source;
      _intBufferData *buf;

      buf = _intBufPop(src->buffers, _AAX_EMITTER_BUFFER);
      if (buf)
      {
         _embuffer_t *embuf = _intBufGetDataPtr(buf);
         _aaxFreeEmitterBuffer(embuf);
         _intBufDestroyDataNoLock(buf);

         if (src->buffer_pos > 0) {
            src->buffer_pos--;
         }

         if (_intBufGetNumNoLock(src->buffers, _AAX_EMITTER_BUFFER) == 0) {
            handle->track = 0;
         }
      }
      else
      {
         _aaxErrorSet(AAX_INVALID_REFERENCE);
         rv = AAX_FALSE;
      }
   }
   put_emitter(handle);

   return rv;
}

AAX_API aaxBuffer AAX_APIENTRY
aaxEmitterGetBufferByPos(const aaxEmitter emitter, unsigned int pos, int copy)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
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
   put_emitter(handle);
   return rv;
}

AAX_API unsigned int AAX_APIENTRY
aaxEmitterGetNoBuffers(const aaxEmitter emitter, enum aaxState state)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
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
            rv = (src->buffer_pos == UINT_MAX) ? 0 : src->buffer_pos;
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
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetState(aaxEmitter emitter, enum aaxState state)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
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
         // intentional fallthrough
      case AAX_UPDATE:				/* update distance delay */
         if (handle->mixer_pos != UINT_MAX)	/* emitter is registered */
         {
            _handle_t *phandle = handle->parent;
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
            if (!handle->sampled_release &&
                !_PROP_DISTDELAY_IS_DEFINED(src->props3d))
            {
#if 0
               _SET_PROCESSED(src->props3d);
               src->buffer_pos = UINT_MAX;
#else
               // MIDI needs this to prevent a note being started and stopped
               // again before actual playback began to not be audible.
               // (Jazz_-_Cabaret.mid example, toms at the start of the song)
               _SET_STOPPED(src->props3d);
#endif
            }
            else {
               _SET_STOPPED(src->props3d);
            }
         }
         rv = AAX_TRUE;
         break;
      case AAX_SUSPENDED:
         if (_IS_PLAYING(src->props3d)) {
            _SET_PAUSED(src->props3d);
         }
         rv = AAX_TRUE;
         break;
      case AAX_PROCESSED:
         if (!_IS_PROCESSED(src->props3d))
         {
            _SET_PLAYING(src->props3d); // In case rthe caller never did that
            _SET_PROCESSED(src->props3d);
            src->buffer_pos = UINT_MAX;
         }
         rv = AAX_TRUE;
         break;
      case AAX_INITIALIZED:	/* or rewind */
      {
         const _intBufferData* dptr;

         src->buffer_pos = 0;
         src->curr_pos_sec = 0.0f;

         handle->mtx_set = AAX_FALSE;
         dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, 0);
         if (dptr)
         {
            _embuffer_t *embuf = _intBufGetDataPtr(dptr);
            _aaxRingBuffer *rb = embuf->ringbuffer;
            _aax2dProps *p2d = src->props2d;
            int i;

            rb->set_state(rb, RB_REWINDED);
            p2d->curr_pos_sec = 0.0f;

            _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);

//          _aaxMutexLock(src->props2d->mutex);
            for (i=0; i<MAX_STEREO_FILTER; ++i) {
               reset_filter(src->props2d, i);
            }

            for (i=0; i<MAX_STEREO_EFFECT; ++i) {
               reset_effect(src->props2d, i);
            }
//          _aaxMutexDestroy(src->props2d->mutex);
         }
         rv = AAX_TRUE;
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetFilter(aaxEmitter emitter, aaxFilter f)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
   int rv = AAX_FALSE;
   if (handle)
   {
      _filter_t* filter = get_filter(f);
      if (filter)
       {
         rv = _emitterSetFilter(handle, filter);
         if (!rv) {
            _aaxErrorSet(AAX_INVALID_ENUM);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   put_emitter(handle);
   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxEmitterGetFilter(const aaxEmitter emitter, enum aaxFilterType type)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
   aaxFilter rv = AAX_FALSE;
   if (handle)
   {
      switch(type)
      {
      case AAX_FREQUENCY_FILTER:
      case AAX_VOLUME_FILTER:
      case AAX_TIMED_GAIN_FILTER:
      case AAX_DYNAMIC_GAIN_FILTER:
      case AAX_DYNAMIC_LAYER_FILTER:
      case AAX_BITCRUSHER_FILTER:
      case AAX_DISTANCE_FILTER:
      case AAX_DIRECTIONAL_FILTER:
      {
         _aaxEmitter *src = handle->source;
         rv = new_filter_handle(emitter, type, src->props2d, src->props3d);
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetEffect(aaxEmitter emitter, aaxEffect e)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
   int rv = AAX_FALSE;
   if (handle)
   {
      _effect_t* effect = get_effect(e);
      if (effect)
      {
         rv = _emitterSetEffect(handle, effect);
         if (!rv) {
            _aaxErrorSet(AAX_INVALID_ENUM);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   put_emitter(handle);
   return rv;
}

AAX_API aaxEffect AAX_APIENTRY
aaxEmitterGetEffect(const aaxEmitter emitter, enum aaxEffectType type)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
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
      case AAX_RINGMODULATOR_EFFECT:
      case AAX_VELOCITY_EFFECT:
      {
         _aaxEmitter *src = handle->source;
         rv = new_effect_handle(emitter, type, src->props2d, src->props3d);
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetMode(aaxEmitter emitter, enum aaxModeType type, int mode)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
   int rv = AAX_FALSE;
   if (handle)
   {
      _aaxEmitter *src = handle->source;
      switch(type)
      {
      case AAX_POSITION:
      {
         int m = (mode > AAX_MODE_NONE) ? AAX_TRUE : AAX_FALSE;
         _aax3dProps *ep3d = src->props3d;
         _TAS_POSITIONAL(ep3d, m);
         if TEST_FOR_TRUE(m)
         {
            m = (mode == AAX_RELATIVE) ? AAX_TRUE : AAX_FALSE;
            _TAS_RELATIVE(ep3d, m);
            if (_IS_RELATIVE(ep3d))
            {
               if (handle->parent && (handle->parent == handle->root)) {
                  ep3d->dprops3d->matrix.m4[LOCATION][3] = 0.0;
                  ep3d->dprops3d->velocity.m4[VELOCITY][3] = 0.0f;
               }
            }
         }
         rv = AAX_TRUE;
         break;
      }
      case AAX_LOOPING:
      {
         _aaxEmitter *src = handle->source;
         _intBufferData *dptr =_intBufGet(src->buffers, _AAX_EMITTER_BUFFER, 0);
         if (dptr)
         {
            _embuffer_t *embuf = _intBufGetDataPtr(dptr);
            _aaxRingBuffer *rb = embuf->ringbuffer;
            rb->set_parami(rb, RB_LOOPING, mode);
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
            _aaxRingBuffer *rb = embuf->buffer->ringbuffer[0];
            if (mode < (int)rb->get_parami(rb, RB_NO_TRACKS))
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
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetMatrix64(aaxEmitter emitter, aaxMtx4d mtx64)
{
   _emitter_t *handle = get_emitter(emitter, _LOCK, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!mtx64 || detect_nan_mtx4d(mtx64)) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxEmitter *src = handle->source;
      _aax3dProps *ep3d = src->props3d;
      _aaxDelayed3dProps *edp3d = ep3d->dprops3d;
      mtx4dFill(edp3d->matrix.m4, mtx64);
      if (_IS_RELATIVE(ep3d) &&
          handle->parent && (handle->parent == handle->root))
      {
         edp3d->matrix.m4[LOCATION][3] = 0.0;
      } else {
         edp3d->matrix.m4[LOCATION][3] = 1.0;
      }
      _PROP_MTX_SET_CHANGED(ep3d);
      handle->mtx_set = AAX_TRUE;
   }
   put_emitter(handle);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterGetMatrix64(const aaxEmitter emitter, aaxMtx4d mtx64)
{
   _emitter_t *handle = get_emitter(emitter, _LOCK, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!mtx64) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxEmitter *src = handle->source;
      mtx4dFill(mtx64, src->props3d->dprops3d->matrix.m4);
   }
   put_emitter(handle);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetVelocity(aaxEmitter emitter, aaxVec3f velocity)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!velocity || detect_nan_vec3(velocity)) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxEmitter *src = handle->source;
      _aax3dProps *ep3d = src->props3d;
      _aaxDelayed3dProps *edp3d = ep3d->dprops3d;

      vec3fFill(edp3d->velocity.m4[VELOCITY], velocity);
      if (_IS_RELATIVE(ep3d) &&
          handle->parent && (handle->parent == handle->root))
      {
         edp3d->velocity.m4[VELOCITY][3] = 0.0f;
      } else {
         edp3d->velocity.m4[VELOCITY][3] = 1.0f;
      }
      _PROP_SPEED_SET_CHANGED(ep3d);
   }
   put_emitter(handle);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterGetVelocity(const aaxEmitter emitter, aaxVec3f velocity)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!velocity) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }

   if (rv)
   {
      _aaxDelayed3dProps *dp3d;

      dp3d = handle->source->props3d->dprops3d;
      vec3fFill(velocity, dp3d->velocity.m4[VELOCITY]);
   }
   put_emitter(handle);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetOffset(aaxEmitter emitter, unsigned long offs, enum aaxType type)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
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
         _aaxRingBuffer *rb = embuf->ringbuffer;
         float duration, fpos = (float)offs*1e-6f;
         unsigned int samples, pos = 0;

         switch (type)
         {
         case AAX_BYTES:
            offs /= rb->get_parami(rb, RB_BYTES_SAMPLE);
            // intentional fallthrough
         case AAX_FRAMES:
         case AAX_SAMPLES:
            samples = rb->get_parami(rb, RB_NO_SAMPLES);
            while (offs > samples)
            {
               pos++;
               offs -= samples;
               _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);

               dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, pos);
               if (!dptr) break;

               embuf = _intBufGetDataPtr(dptr);
               rb = embuf->ringbuffer;
               samples = rb->get_parami(rb, RB_NO_SAMPLES);
            }
            if (dptr)
            {
               handle->mixer_pos = pos;
               rb->set_parami(rb, RB_OFFSET_SAMPLES, offs);
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_MICROSECONDS:
            duration = rb->get_paramf(rb, RB_DURATION_SEC);
            while (fpos > duration)
            {
               pos++;
               fpos -= duration;
               _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);

               dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, pos);
               if (!dptr) break;

               embuf = _intBufGetDataPtr(dptr);
               rb = embuf->ringbuffer;
               duration = rb->get_paramf(rb, RB_DURATION_SEC);
            }
            if (dptr)
            {
               handle->mixer_pos = pos;
               rb->set_paramf(rb, RB_OFFSET_SEC, fpos);
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
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetOffsetSec(aaxEmitter emitter, float offs)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (is_nan(offs)) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxEmitter *src = handle->source;
      _intBufferData *dptr;

      _intBufGetNum(src->buffers, _AAX_EMITTER_BUFFER);
      dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, 0);
      if (dptr)
      {
         _embuffer_t *embuf = _intBufGetDataPtr(dptr);
         _aaxRingBuffer *rb = embuf->ringbuffer;
         unsigned int pos = 0;
         float duration;

         duration = rb->get_paramf(rb, RB_DURATION_SEC);
         while (offs > duration)
         {
            pos++;
            offs -= duration;
            _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);

            dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, pos);
            if (!dptr) break;

            embuf = _intBufGetDataPtr(dptr);
            rb = embuf->ringbuffer;
            duration = rb->get_paramf(rb, RB_DURATION_SEC);
         }

         if (dptr)
         {
            handle->mixer_pos = pos;
            rb->set_paramf(rb, RB_OFFSET_SEC, offs);
         }
         else
         {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
            rv = AAX_FALSE;
         }
         _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);
      }
      else
      {
         _aaxErrorSet(AAX_INVALID_REFERENCE);
         rv = AAX_FALSE;
      }
      _intBufReleaseNum(src->buffers, _AAX_EMITTER_BUFFER);
   }
   put_emitter(handle);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterGetMode(const aaxEmitter emitter, enum aaxModeType type)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
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
            _aaxRingBuffer *rb = embuf->ringbuffer;
            rv = rb->get_parami(rb, RB_LOOPING);
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
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterGetState(const aaxEmitter emitter)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
   enum aaxState ret = AAX_STATE_NONE;
   if (handle)
   {
      _handle_t *thread = get_valid_handle(handle->parent, __func__);
      if (thread)
      {
         const _aaxEmitter *src = handle->source;
         if (_IS_PROCESSED(src->props3d)) ret = AAX_PROCESSED;
         else if (_IS_STOPPED(src->props3d)) ret = AAX_STOPPED;
         else if (_IS_PAUSED(src->props3d)) ret = AAX_SUSPENDED;
         else if (_IS_PLAYING(src->props3d)) ret = AAX_PLAYING;
         else ret = AAX_INITIALIZED;
       }
       else
       {
          // get_valid_handle may() set an error which is bogus here
          __aaxErrorSet(AAX_ERROR_NONE, NULL);
          ret = AAX_INITIALIZED;
       }
   }
   put_emitter(handle);
   return ret;
}

AAX_API unsigned long AAX_APIENTRY
aaxEmitterGetOffset(const aaxEmitter emitter, enum aaxType type)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
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
            _aaxRingBuffer *rb = embuf->ringbuffer;
            for (i=0; i<handle->mixer_pos; i++)
            {
               rv += rb->get_parami(rb, RB_NO_SAMPLES);
               _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);

               dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, i);
               embuf = _intBufGetDataPtr(dptr);
            }
            _intBufReleaseNum(src->buffers, _AAX_EMITTER_BUFFER);

            rv += rb->get_parami(rb, RB_OFFSET_SAMPLES);
            if (type == AAX_BYTES) {
               rv *= rb->get_parami(rb, RB_BYTES_SAMPLE);
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
   put_emitter(handle);
   return rv;
}

AAX_API float AAX_APIENTRY
aaxEmitterGetOffsetSec(const aaxEmitter emitter)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
   float rv = 0.0f;
   if (handle)
   {
      const _aaxEmitter *src = handle->source;
      rv = src->curr_pos_sec;
   }
   put_emitter(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxEmitterSetSetup(aaxEmitter emitter, enum aaxSetupType type, unsigned int setup)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
   _aax2dProps *p2d = handle->source->props2d;
   int rv = AAX_FALSE;
   switch(type)
   {
   case AAX_MIDI_PRESSURE_FACTOR:
      p2d->note.pressure = (float)setup/127.0f;			// 0.0 .. 1.0
      break;
   case AAX_MIDI_ATTACK_VELOCITY_FACTOR:
      p2d->note.velocity = (float)setup/127.0f;			// 0.0 .. 1.0
      p2d->note.velocity = 0.66f + 0.66f*p2d->note.velocity*p2d->note.velocity;
      break;
   case AAX_MIDI_RELEASE_VELOCITY_FACTOR:
      p2d->note.release = (float)setup/64.0f;			// 0.0 .. 2.0
      break;
      break;
   case AAX_MIDI_SOFT_FACTOR:
      p2d->note.soft = (float)setup/127.0f;			// 0.0 .. 1.0
      break;
   case AAX_MIDI_ATTACK_FACTOR:
      handle->midi.attack_factor = (float)setup/64.0f;		// 0.0 .. 2.0
      break;
   case AAX_MIDI_DECAY_FACTOR:
      handle->midi.decay_factor = (float)setup/64.0f;		// 0.0 .. 2.0
      break;
   case AAX_RELEASE_FACTOR:
      handle->midi.release_factor = (float)setup/64.0f;		// 0.0 .. 2.0
      break;
   default:
      break;
   }
   put_emitter(handle);
   return rv;
}

AAX_API unsigned int AAX_APIENTRY
aaxEmitterGetSetup(const aaxEmitter emitter, enum aaxSetupType type)
{
   _emitter_t* handle = get_emitter(emitter, _LOCK, __func__);
   _aax2dProps *p2d = handle->source->props2d;
   unsigned int rv = AAX_FALSE;
   switch(type)
   {
   case AAX_MIDI_ATTACK_FACTOR:
      rv = 64.0f*handle->midi.attack_factor;
      break;
   case AAX_RELEASE_FACTOR:
      rv = 64.0f*handle->midi.release_factor;
      break;
   case AAX_MIDI_DECAY_FACTOR:
      rv = 64.0f*handle->midi.decay_factor;
      break;
   case AAX_MIDI_ATTACK_VELOCITY_FACTOR:
      rv = sqrtf((p2d->note.velocity - 0.66f)/0.66f);
      rv *= 127.0f;
      break;
   case AAX_MIDI_RELEASE_VELOCITY_FACTOR:
      rv = 64.0f*p2d->note.release;
      break;
   case AAX_MIDI_PRESSURE_FACTOR:
      rv = 127.0f*p2d->note.pressure;
      break;
   case AAX_MIDI_SOFT_FACTOR:
      rv = 127.0f*p2d->note.soft;
      break;
   default:
      break;
   }
   put_emitter(handle);
   return rv;
}

/* -------------------------------------------------------------------------- */

_emitter_t*
get_emitter_unregistered(aaxEmitter em, const char *func)
{
   _emitter_t *emitter = (_emitter_t *)em;
   _emitter_t *rv = NULL;

   if (emitter && emitter->id == EMITTER_ID && !emitter->parent) {
      rv = emitter;
   }
   else if (emitter && emitter->id == FADEDBAD) {
      __aaxErrorSet(AAX_DESTROYED_HANDLE, func);
   }
   else {
      __aaxErrorSet(AAX_INVALID_HANDLE, func);
   }

   return rv;
}

_emitter_t*
get_emitter(aaxEmitter em, int lock, const char *func)
{
   _emitter_t *emitter = (_emitter_t *)em;
   _emitter_t *rv = NULL;

   if (emitter && emitter->id == EMITTER_ID)
   {
      _handle_t *handle = emitter->parent;
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

            if (lock) {
               dptr_src = _intBufGet(he, _AAX_EMITTER, emitter->mixer_pos);
            } else {
               dptr_src =_intBufGetNoLock(he, _AAX_EMITTER, emitter->mixer_pos);
            }
            emitter = _intBufGetDataPtr(dptr_src);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
      else if (handle && handle->id == AUDIOFRAME_ID)
      {
         _frame_t *handle = (_frame_t*)emitter->parent;
         _intBufferData *dptr_src;
         _intBuffers *he;

         if (!_IS_POSITIONAL(emitter->source->props3d)) {
            he = handle->submix->emitters_2d;
         } else {
            he = handle->submix->emitters_3d;
         }

         if (lock) {
            dptr_src = _intBufGet(he, _AAX_EMITTER, emitter->mixer_pos);
         } else {
            dptr_src = _intBufGetNoLock(he, _AAX_EMITTER, emitter->mixer_pos);
         }
         emitter = _intBufGetDataPtr(dptr_src);
      }
      rv = emitter;
   }
   else if (emitter && emitter->id == FADEDBAD) {
      __aaxErrorSet(AAX_DESTROYED_HANDLE, func);
   }
   else {
      __aaxErrorSet(AAX_INVALID_HANDLE, func);
   }

   return rv;
}

void
put_emitter(aaxEmitter em)
{
   _emitter_t *emitter = (_emitter_t *)em;

   if (emitter && emitter->id == EMITTER_ID)
   {
      _handle_t *handle = emitter->parent;
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
         _frame_t *handle = (_frame_t*)emitter->parent;
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
      _aax3dProps *fp3d = mixer->props3d;
      _aaxDelayed3dProps *fdp3d_m = fp3d->m_dprops3d;
      _aax3dProps *ep3d = src->props3d;
      _aaxDelayed3dProps *edp3d_m = ep3d->m_dprops3d;
      _aaxDelayed3dProps *edp3d = ep3d->dprops3d;
      _aax2dProps *ep2d = src->props2d;
      double dist;
      float vs;

      vs = _EFFECT_GET(fp3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);

      /**
       * Align the modified emitter matrix with the sensor by multiplying
       * the emitter matrix by the modified frame matrix.
       */
      mtx4dMul(&edp3d_m->matrix, &fdp3d_m->matrix, &edp3d->matrix);
      dist = vec3dMagnitude(&edp3d_m->matrix.v34[LOCATION]);
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
   if (embuf)
   {
      free_buffer(embuf->buffer);
      _aaxRingBufferFree(embuf->ringbuffer);
      embuf->ringbuffer = NULL;
      embuf->id = FADEDBAD;
      free(embuf);
      embuf = NULL;
   }
}

static int
_emitterSetFilter(_emitter_t *handle, _filter_t *filter)
{
   _aaxEmitter *src = handle->source;
   _aax2dProps *p2d = src->props2d;
   _aax3dProps *p3d = src->props3d;
   int type = filter->pos;
   int rv = AAX_TRUE;

   switch (filter->type)
   {
   case AAX_TIMED_GAIN_FILTER:
      _PROP_DISTDELAY_SET_DEFINED(src->props3d);
      _FILTER_SWAP_SLOT(p2d, type, filter, 0);
      break;
   case AAX_VOLUME_FILTER:
   case AAX_DYNAMIC_GAIN_FILTER:
   case AAX_DYNAMIC_LAYER_FILTER:
   case AAX_BITCRUSHER_FILTER:
      _FILTER_SWAP_SLOT(p2d, type, filter, 0);
      break;
   case AAX_FREQUENCY_FILTER:
      _FILTER_SWAP_SLOT(p2d, type, filter, 0);
      if (p2d->note.velocity > 1.0f)
      {
         _aaxRingBufferFreqFilterData *flt = _FILTER_GET_DATA(p2d, FREQUENCY_FILTER);
         _aaxLFOData* lfo = flt->lfo;
         if (flt->lfo) {
            float max = lfo->max_sec*lfo->fs;
            lfo->max = max*(0.875f+0.125f*p2d->note.velocity);
         }
      }
      else if (p2d->note.release < 1.0f)
      {
         _aaxRingBufferFreqFilterData *flt = _FILTER_GET_DATA(p2d, FREQUENCY_FILTER);
         _aaxLFOData* lfo = flt->lfo;
         if (flt->lfo) {
            float min = lfo->min_sec*lfo->fs;
            lfo->min = min*(0.875f+0.125f*p2d->note.release);
         }
      }
      break;
   case AAX_DISTANCE_FILTER:
      _FILTER_SWAP_SLOT(p3d, type, filter, 0);
      if (_EFFECT_GET_UPDATED(p3d, VELOCITY_EFFECT) == AAX_FALSE)
      {
         _aaxRingBufferDistanceData *data;
         data = _FILTER_GET_DATA(p3d, DISTANCE_FILTER);
         if (data->next.T_K != 0.0f && data->next.hr_pct != 0.0f)
         {
            if (_FILTER_GET_SLOT_STATE(filter) & AAX_ISO9613_DISTANCE)
            {
               float vs = _velocity_calculcate_vs(&data->next);
               _EFFECT_SET(p3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY, vs);
            }
         }
      }
      break;
   case AAX_DIRECTIONAL_FILTER:
   {
      float inner_vec = _FILTER_GET_SLOT(filter, 0, 0);
      float outer_gain = _FILTER_GET_SLOT(filter, 0, 2);

      if ((inner_vec >= 1.0f) || (outer_gain >= 1.0f)) {
         _PROP_CONE_CLEAR_DEFINED(p3d);
      } else {
         _PROP_CONE_SET_DEFINED(p3d);
      }

      _FILTER_SWAP_SLOT(p3d, type, filter, 0);
      break;
   }
   default:
      rv = AAX_FALSE;
      break;
   }

   return rv;
}

static int
_emitterSetEffect(_emitter_t *handle, _effect_t *effect)
{
   _aaxEmitter *src = handle->source;
   _aax2dProps *p2d = src->props2d;
   _aax3dProps *p3d = src->props3d;
   int type = effect->pos;
   int rv = AAX_TRUE;

   switch (effect->type)
   {
   case AAX_PITCH_EFFECT:
      _PROP_PITCH_SET_CHANGED(p3d);
      _EFFECT_SWAP_SLOT(p2d, type, effect, 0);
      _emitterSetPitch(src, p2d);
      break;
   case AAX_TIMED_PITCH_EFFECT:
      _PROP_PITCH_SET_CHANGED(p3d);
      // intentional fallthrough
   case AAX_DISTORTION_EFFECT:
   case AAX_RINGMODULATOR_EFFECT:
   case AAX_FLANGING_EFFECT:
   case AAX_PHASING_EFFECT:
   case AAX_CHORUS_EFFECT:
      _EFFECT_SWAP_SLOT(p2d, type, effect, 0);
      break;
   case AAX_DYNAMIC_PITCH_EFFECT:
   {
      _aaxLFOData *lfo;

      _EFFECT_SWAP_SLOT(p2d, type, effect, 0);
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
      break;
   }
   case AAX_VELOCITY_EFFECT:
      _EFFECT_SWAP_SLOT(p3d, type, effect, 0);
      // Must be after _EFFECT_SWAP_SLOT_DATA which calls effect->state()
      _EFFECT_SET_UPDATED(p3d, type, _EFFECT_GET_SLOT_UPDATED(effect));
      break;
   default:
      rv = AAX_FALSE;
      break;
   }

   return rv;
}

void
_emitterSetPitch(const _aaxEmitter *src, _aax2dProps *p2d)
{
   _intBufferData *dptr = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, 0);
   float pitch, fraction = 1.0f;
   if (dptr)
   {
      _embuffer_t *embuf = _intBufGetDataPtr(dptr);
      _buffer_t* buf = embuf->buffer;

      assert(embuf->id == EMBUFFER_ID);
      fraction = buf->info.pitch_fraction;
      _intBufReleaseData(dptr, _AAX_EMITTER_BUFFER);
   }

   pitch = _EFFECT_GET(p2d, PITCH_EFFECT, AAX_PITCH);
   pitch = fraction*(pitch - 1.0f) + 1.0f;
   _EFFECT_SET(p2d, PITCH_EFFECT, AAX_PITCH, pitch);

   pitch = _EFFECT_GET(p2d, PITCH_EFFECT, AAX_PITCH_START);
   pitch = fraction*(pitch - 1.0f) + 1.0f;
   _EFFECT_SET(p2d, PITCH_EFFECT, AAX_PITCH_START, pitch);
}

int
_emitterCreateEFFromRingbuffer(_emitter_t *handle, _embuffer_t *embuf)
{
   aaxConfig config = handle->root;
   int rv = AAX_FALSE;

   if (config)
   {
      aaxFilter flt = aaxFilterCreate(config, AAX_TIMED_GAIN_FILTER);
      if (flt)
      {
         _aaxRingBuffer *rb = embuf->ringbuffer;
         _filter_t* filter;
         int i, param = 0;
         float sum = 0.0f;

         for (i=0; i<_MAX_ENVELOPE_STAGES/2; ++i)
         {
            float level = rb->get_paramf(rb, RB_ENVELOPE_LEVEL+i);
            float rate = rb->get_paramf(rb, RB_ENVELOPE_RATE+i);

            sum += level;
            aaxFilterSetParam(flt, param, AAX_LINEAR, level);
            aaxFilterSetParam(flt, ++param, AAX_LINEAR, rate);

            if ((++param % 4) == 0) param += 0x10 - 4;
         }
         aaxFilterSetState(flt, AAX_TRUE);

         // only apply the timed-gain filter when at least one level is non-zero.
         if (sum)
         {
            filter = get_filter(flt);
            _emitterSetFilter(handle, filter);
         }
         aaxFilterDestroy(flt);
         rv = AAX_TRUE;
      }
   }
   return rv;
}

static int
_emitterCreateTriggerFromAAXS(_emitter_t *handle, _embuffer_t *embuf, xmlId *xmid)
{
   _aax2dProps *ep2d = handle->source->props2d;
   float pitch = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_PITCH);
   float freq = pitch*embuf->buffer->info.base_frequency;
   unsigned int i, num = xmlNodeGetNum(xmid, "filter");
   aaxConfig config = handle->root;
   int clear = AAX_FALSE;
   xmlId *xeid, *xfid;

   if (xmlAttributeExists(xmid, "mode")) {
      clear = xmlAttributeCompareString(xmid, "mode", "append");
   }

   if (xmlAttributeExists(xmid, "looping"))
   {
      int mode = xmlAttributeGetBool(xmid, "looping");
      _aaxRingBuffer *rb = embuf->ringbuffer;
      rb->set_parami(rb, RB_LOOPING, mode);
      handle->looping = mode;
   }

   if (!handle->mtx_set)
   {
      float pan = xmlAttributeGetDouble(xmid, "pan");
      if (fabsf(pan) > 0.01f)
      {
          static aaxVec3f _at = { 0.0f, 0.0f, -1.0f };
          _aaxEmitter *src = handle->source;
          _aax3dProps *ep3d = src->props3d;
          _aaxDelayed3dProps *edp3d = ep3d->dprops3d;
          aaxMtx4d mtx641, mtx642;
          aaxVec3f at, up;
          aaxVec3d pos;

//        aaxEmitterGetMatrix64(emitter, mtx641);
          mtx4dFill(mtx641, edp3d->matrix.m4);

          aaxMatrix64GetOrientation(mtx641, pos, at, up);

          aaxMatrix64SetIdentityMatrix(mtx641);
          aaxMatrix64SetOrientation(mtx641, pos, _at, up);

          aaxMatrix64SetIdentityMatrix(mtx642);
          aaxMatrix64Rotate(mtx642, -1.57*pan, 0.0, 1.0, 0.0);

          aaxMatrix64Multiply(mtx642, mtx641);

//        aaxEmitterSetMatrix64(emitter, mtx642);
          mtx4dFill(edp3d->matrix.m4, mtx642);
          if (_IS_RELATIVE(ep3d) &&
              handle->parent && (handle->parent == handle->root))
          {
             edp3d->matrix.m4[LOCATION][3] = 0.0;
          } else {
             edp3d->matrix.m4[LOCATION][3] = 1.0;
          }
          _PROP_MTX_SET_CHANGED(ep3d);
          handle->mtx_set = AAX_TRUE;
      }
   }

   if (clear)
   {
      _aaxEmitter *src = handle->source;
      _aaxSetDefault2dFiltersEffects(src->props2d);
   }

   xfid = xmlMarkId(xmid);
   if (xfid)
   {
      for (i=0; i<num; i++)
      {
         if (xmlNodeGetPos(xmid, xfid, "filter", i) != 0)
         {
            int non_optional = AAX_TRUE;
            if (xmlAttributeExists(xfid, "optional")) {
               non_optional = !xmlAttributeGetBool(xfid, "optional");
            }
            if (non_optional || !get_low_resource())
            {
                aaxFilter flt = _aaxGetFilterFromAAXS(config, xfid, freq,
                                               0.0f, 0.0f, &handle->midi);
                if (flt)
                {
                  _filter_t* filter = get_filter(flt);
                   _emitterSetFilter(handle, filter);
                   aaxFilterDestroy(flt);
          }
             }
         }
      }
      xmlFree(xfid);
   }

   xeid = xmlMarkId(xmid);
   if (xeid)
   {
      num = xmlNodeGetNum(xmid, "effect");
      for (i=0; i<num; i++)
      {
         if (xmlNodeGetPos(xmid, xeid, "effect", i) != 0)
         {
            int non_optional = AAX_TRUE;
            if (xmlAttributeExists(xeid, "optional")) {
               non_optional = !xmlAttributeGetBool(xeid, "optional");
            }
            if (non_optional || !get_low_resource())
            {
               aaxEffect eff = _aaxGetEffectFromAAXS(config, xeid, freq,
                                               0.0f, 0.0f, &handle->midi);
               if (eff)
               {
                  _effect_t* effect = get_effect(eff);
                  _emitterSetEffect(handle, effect);
                  aaxEffectDestroy(eff);
               }
            }
         }
      }
      xmlFree(xeid);
   }

   return AAX_TRUE;
}

int
_emitterCreateEFFromAAXS(_emitter_t *handle, _embuffer_t *embuf, const char *aaxs)
{
   aaxConfig config = handle->root;
   int rv = AAX_TRUE;
   xmlId *xid;

   if (!config) {
      config = handle->root = embuf->buffer->root;
   }

   xid = xmlInitBuffer(aaxs, strlen(aaxs));
   if (xid)
   {
      xmlId *xmid = xmlNodeGet(xid, "aeonwave/emitter");
      if (xmid)
      {
         if (xmlAttributeExists(xmid, "include"))
         {
            char file[1024];
            int len = xmlAttributeCopyString(xmid, "include", file, 1024);
            if (len < 1024-strlen(".aaxs"))
            {
               _buffer_t* buffer = embuf->buffer;
               _buffer_info_t *info = &buffer->info;
               char **data, *url;

               strcat(file, ".aaxs");
               url = _aaxURLConstruct(buffer->url, file);

               data =_bufGetDataFromStream(NULL, url, info,*buffer->mixer_info);
               if (data)
               {
                  xmlId *xid = xmlInitBuffer(data[0], strlen(data[0]));
                  if (xid)
                  {
                     xmlId *xamid = xmlNodeGet(xid, "aeonwave/emitter");
                     if (xamid)
                     {
                        rv =_emitterCreateTriggerFromAAXS(handle, embuf, xamid);
                        xmlFree(xamid);
                     }
                     xmlClose(xid);
                  }
                  free(data);
               }
               free(url);
            }
         }
         rv = _emitterCreateTriggerFromAAXS(handle, embuf, xmid);
         xmlFree(xmid);
      }
      xmlClose(xid);
   }
   else
   {
      _aaxErrorSet(AAX_INVALID_STATE);
      rv = AAX_FALSE;
   }
   return rv;
}
