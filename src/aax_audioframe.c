/*
 * Copyright 2011 by Erik Hofman.
 * Copyright 2011 by Adalin B.V.
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

#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <sys/time.h>		/* for struct timeval */
#include <errno.h>		/* for ETIMEDOUT */
#include <assert.h>
#include <math.h>
#include <aax.h>

#include <base/threads.h>
#include "api.h"
#include "arch.h"
#include "driver.h"

static int _aaxAudioFrameStart(_frame_t*);
static int _aaxAudioFrameSignal(_frame_t*);
static void* _aaxAudioFrameThread(void*);

aaxFrame
aaxAudioFrameCreate(aaxConfig config)
{
   _handle_t *handle = get_handle(config);
   aaxFrame rv = NULL;
   if (handle && VALID_HANDLE(handle))
   {
      unsigned long size;
      char* ptr2;
      void* ptr1;

      size = sizeof(_frame_t) + sizeof(_aaxAudioFrame);
      ptr2 = (char*)size;
      size += sizeof(_oalRingBuffer2dProps);
      ptr1 = _aax_calloc(&ptr2, 1, size);
      if (ptr1)
      {
         _frame_t* frame = (_frame_t*)ptr1;
         _aaxAudioFrame* submix;
         int res;

         frame->id = AUDIOFRAME_ID;
         frame->pos = UINT_MAX;
         _SET_INITIAL(frame);

         size = sizeof(_frame_t);
         submix = (_aaxAudioFrame*)((char*)frame + size);
         frame->submix = submix;

         submix->props2d = (_oalRingBuffer2dProps*)ptr2;
         _aaxSetDefault2dProps(submix->props2d);
         _EFFECT_SET2D(submix, PITCH_EFFECT, AAX_PITCH, handle->info->pitch);

         ptr2 = (char*)0;
         size = sizeof(_oalRingBuffer3dProps);
         ptr1 = _aax_malloc(&ptr2, size);
         submix->props3d_ptr = ptr1;
         submix->props3d = (_oalRingBuffer3dProps*)ptr2;
         if (ptr1)
         {
            const _intBufferData* dptr;

            _aaxSetDefault3dProps(submix->props3d);

            dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               _aaxAudioFrame* mixer = sensor->mixer;
               _FILTER_COPY3D_DATA(submix, mixer, DISTANCE_FILTER);
               _EFFECT_COPY3D(submix,mixer,VELOCITY_EFFECT, AAX_SOUND_VELOCITY);
               _EFFECT_COPY3D(submix,mixer,VELOCITY_EFFECT, AAX_DOPPLER_FACTOR);
               _EFFECT_COPY3D_DATA(submix, mixer, VELOCITY_EFFECT);

               submix->info = sensor->mixer->info;
               _intBufReleaseData(dptr, _AAX_SENSOR);
            }
         }
         else {
            _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         }

         res = _intBufCreate(&submix->emitters_3d, _AAX_EMITTER);
         if (res != UINT_MAX) {
            res = _intBufCreate(&submix->emitters_2d, _AAX_EMITTER);
         }
         if (res != UINT_MAX) {
            res = _intBufCreate(&submix->ringbuffers, _AAX_RINGBUFFER);
         }
         if (res != UINT_MAX) {
            rv = (aaxFrame)frame;
         } else {
            free(ptr1);
         }
      }
      else {
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

int
aaxAudioFrameDestroy(aaxFrame frame)
{
   _frame_t* handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      if (!handle->handle)
      {
         _oalRingBufferDelayEffectData* effect;
         _aaxAudioFrame* mixer = handle->submix;

         free(_FILTER_GET2D_DATA(mixer, FREQUENCY_FILTER));
         free(_FILTER_GET2D_DATA(mixer, DYNAMIC_GAIN_FILTER));
         free(_FILTER_GET2D_DATA(mixer, TIMED_GAIN_FILTER));
         free(_EFFECT_GET2D_DATA(mixer, DYNAMIC_PITCH_EFFECT));

         effect = _EFFECT_GET2D_DATA(mixer, DELAY_EFFECT);
         if (effect) free(effect->history_ptr);
         free(effect);
         free(mixer->props3d_ptr);

         /* mixer->ringbuffer gets removed bij the mixer thread */
         /* _oalRingBufferDelete(handle->submix->ringbuffer); */
         _intBufErase(&mixer->frames, _AAX_FRAME, 0, 0);
         _intBufErase(&mixer->emitters_2d, _AAX_EMITTER, 0, 0);
         _intBufErase(&mixer->emitters_3d, _AAX_EMITTER, 0, 0);
         _intBufErase(&mixer->ringbuffers, _AAX_RINGBUFFER,
                      _aaxRemoveRingBufferByPos, mixer);

         /* safeguard against using already destroyed handles */
         handle->id = 0xdeadbeef;
         free(handle);
         handle = 0;
         rv = AAX_TRUE;
      }
      else {
         _aaxErrorSet(AAX_INVALID_STATE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

int
aaxAudioFrameSetMatrix(aaxFrame frame, aaxMtx4f mtx)
{
   _frame_t *handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      if (mtx && !detect_nan_vec4(mtx[0]) && !detect_nan_vec4(mtx[1]) &&
                 !detect_nan_vec4(mtx[2]) && !detect_nan_vec4(mtx[3]))
      {
         _aaxAudioFrame* mixer = handle->submix;

         mtx4Copy(mixer->props3d->matrix, mtx);
         if (_IS_RELATIVE(handle)) {
            mixer->props3d->matrix[LOCATION][3] = 0.0f;
         } else {
            mixer->props3d->matrix[LOCATION][3] = 1.0f;
         }
         _PROP_MTX_SET_CHANGED(mixer->props3d);
         rv = AAX_TRUE;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_frame(frame);
   return rv;
}

int
aaxAudioFrameGetMatrix(aaxFrame frame, aaxMtx4f mtx)
{
   _frame_t *handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      if (mtx)
      {
         mtx4Copy(mtx, handle->submix->props3d->matrix);
         rv = AAX_TRUE;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_frame(frame);
   return rv;
}

int
aaxAudioFrameSetVelocity(aaxFrame frame, const aaxVec3f velocity)
{
   _frame_t *handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      if (velocity && !detect_nan_vec3(velocity))
      {
         vec3Copy(handle->submix->props3d->velocity, velocity);
         rv = AAX_TRUE;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_frame(frame);
   return rv;
}

int
aaxAudioFrameGetVelocity(aaxFrame frame, aaxVec3f velocity)
{
   _frame_t *handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      if (velocity)
      {
         vec3Copy(velocity, handle->submix->props3d->velocity);
         rv = AAX_TRUE;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_frame(frame);
   return rv;
}

int
aaxAudioFrameSetFilter(aaxFrame frame, aaxFilter f)
{
   _frame_t *handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      _filter_t* filter = get_filter(f);
      if (filter)
      {
         int type = filter->pos;
         switch (filter->type)
         {
#if 0
         case AAX_EQUALIZER:
         {
            _oalRingBuffer2dProps *p2d = handle->submix->props2d;
            type = EQUALIZER_HF;
            _FILTER_SET(p2d, type, 0, _FILTER_GET_SLOT(filter, 1, 0));
            _FILTER_SET(p2d, type, 1, _FILTER_GET_SLOT(filter, 1, 1));
            _FILTER_SET(p2d, type, 2, _FILTER_GET_SLOT(filter, 1, 2));
            _FILTER_SET(p2d, type, 3, _FILTER_GET_SLOT(filter, 1, 3));
            _FILTER_SET_STATE(p2d, type, _FILTER_GET_SLOT_STATE(filter));
            _FILTER_SWAP_SLOT_DATA(p2d, EQUALIZER_HF, filter, 1);

            type = EQUALIZER_LF;
            /* break is not needed */
         }
#endif
         case AAX_FREQUENCY_FILTER:
         case AAX_DYNAMIC_GAIN_FILTER:
         case AAX_VOLUME_FILTER:
         case AAX_TIMED_GAIN_FILTER:
         {
            _oalRingBuffer2dProps *p2d = handle->submix->props2d;
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
         case AAX_ANGULAR_FILTER:
         {
            _oalRingBuffer3dProps *p3d = handle->submix->props3d;
            _FILTER_SET(p3d, type, 0, _FILTER_GET_SLOT(filter, 0, 0));
            _FILTER_SET(p3d, type, 1, _FILTER_GET_SLOT(filter, 0, 1));
            _FILTER_SET(p3d, type, 2, _FILTER_GET_SLOT(filter, 0, 2));
            _FILTER_SET(p3d, type, 3, _FILTER_GET_SLOT(filter, 0, 3));
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
   put_frame(frame);
   return rv;
}

const aaxFilter
aaxAudioFrameGetFilter(aaxFrame frame, enum aaxFilterType type)
{
   _frame_t *handle = get_frame(frame);
   aaxFilter rv = AAX_FALSE;
   if (handle)
   {
      switch(type)
      {
      case AAX_FREQUENCY_FILTER:
      case AAX_DYNAMIC_GAIN_FILTER:
      case AAX_VOLUME_FILTER:
      case AAX_TIMED_GAIN_FILTER:
      case AAX_DISTANCE_FILTER:
      case AAX_ANGULAR_FILTER:
      {
         _aaxAudioFrame* submix = handle->submix;
         rv = new_filter_handle(submix->info, type, submix->props2d,
                                                    submix->props3d);
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_frame(frame);
   return rv;
}

int
aaxAudioFrameSetEffect(aaxFrame frame, aaxEffect e)
{
   _frame_t *handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      _filter_t* effect = get_effect(e);
      if (effect)
      {
         _aaxAudioFrame* mixer = handle->submix;
         int type = effect->pos;
         switch (effect->type)
         {
         case AAX_DYNAMIC_PITCH_EFFECT:
         case AAX_DISTORTION_EFFECT:
         case AAX_PHASING_EFFECT:
         case AAX_CHORUS_EFFECT:
         case AAX_FLANGING_EFFECT:
         case AAX_PITCH_EFFECT:
         {
            _oalRingBuffer2dProps *p2d = mixer->props2d;
            _EFFECT_SET(p2d, type, 0, _EFFECT_GET_SLOT(effect, 0, 0));
            _EFFECT_SET(p2d, type, 1, _EFFECT_GET_SLOT(effect, 0, 1));
            _EFFECT_SET(p2d, type, 2, _EFFECT_GET_SLOT(effect, 0, 2));
            _EFFECT_SET(p2d, type, 3, _EFFECT_GET_SLOT(effect, 0, 3));
            _EFFECT_SET_STATE(p2d, type, _EFFECT_GET_SLOT_STATE(effect));
            _EFFECT_SWAP_SLOT_DATA(p2d, type, effect, 0);
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
      put_frame(handle);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

const aaxEffect
aaxAudioFrameGetEffect(aaxFrame frame, enum aaxEffectType type)
{
   _frame_t *handle = get_frame(frame);
   aaxEffect rv = AAX_FALSE;
   if (handle)
   {
      switch(type)
      {
      case AAX_DYNAMIC_PITCH_EFFECT:
      case AAX_DISTORTION_EFFECT:
      case AAX_PHASING_EFFECT:
      case AAX_CHORUS_EFFECT:
      case AAX_FLANGING_EFFECT:
      case AAX_PITCH_EFFECT:
      {
         _aaxAudioFrame* mixer = handle->submix;
         rv = new_effect_handle(mixer->info, type, mixer->props2d,
                                                   mixer->props3d);
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
      put_frame(handle);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

int
aaxAudioFrameSetMode(aaxFrame frame, enum aaxModeType type, int mode)
{
   _frame_t *handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      _aaxAudioFrame* mixer = handle->submix;
      switch(type)
      {
      case AAX_POSITION:
      {
         int m = (mode > AAX_MODE_NONE) ? AAX_TRUE : AAX_FALSE;
         _TAS_POSITIONAL(handle, m);
         if TEST_FOR_TRUE(m)
         {
            m = (mode == AAX_RELATIVE) ? AAX_TRUE : AAX_FALSE;
            _TAS_RELATIVE(handle, m);
            if TEST_FOR_TRUE(m) {
               mixer->props3d->matrix[LOCATION][3] = 0.0f;
            } else {
               mixer->props3d->matrix[LOCATION][3] = 1.0f;
            }
         }
         rv = AAX_TRUE;
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_frame(handle);
   return rv;
}

int
aaxAudioFrameRegisterSensor(const aaxFrame frame, const aaxConfig sensor)
{
   _frame_t* handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      _handle_t* config = get_read_handle(sensor);
      if (config && !config->thread.started)
      {
         if (config->pos == UINT_MAX)
         {
            _aaxAudioFrame* mixer = handle->submix;
            _intBuffers *hs = mixer->sensors;
            unsigned int pos = UINT_MAX;

            if (hs == NULL)
            {
               unsigned int res;

               res = _intBufCreate(&mixer->sensors, _AAX_DEVICE);
               if (res != UINT_MAX) {
                  hs = mixer->sensors;
               }
            }

            if (hs && (mixer->no_registered < mixer->info->max_registered))
            {
               aaxBuffer buf; /* clear the sensors buffer queue */
               while ((buf = aaxSensorGetBuffer(config)) != NULL) {
                  aaxBufferDestroy(buf);
               }
               pos = _intBufAddData(hs, _AAX_DEVICE, config);
               mixer->no_registered++;
            }
            else {
               _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            }

            if (pos != UINT_MAX)
            {
               _intBufferData *dptr;

               dptr = _intBufGet(config->sensors, _AAX_SENSOR, 0);
               if (dptr)
               {
                  _sensor_t* sensor = _intBufGetDataPtr(dptr);
                  _oalRingBuffer3dProps *mp3d, *sp3d;
                  _aaxAudioFrame *submix;
                  _oalRingBuffer *rb;

                  submix = sensor->mixer;

                  mp3d = mixer->props3d;
                  sp3d = submix->props3d;

                  submix->info->frequency = mixer->info->frequency;
                  while (submix->info->frequency > 48000.0f) {
                     submix->info->frequency /= 2.0f;
                  }
                  submix->info->refresh_rate = mixer->info->refresh_rate;
                  submix->dist_delaying = mixer->dist_delaying;
                  if (_FILTER_GET_DATA(sp3d, DISTANCE_FILTER) == NULL) {
                     _FILTER_COPY_DATA(sp3d, mp3d, DISTANCE_FILTER);
                  }

                  if (_EFFECT_GET_DATA(sp3d, VELOCITY_EFFECT) == NULL)
                  {
                     _EFFECT_COPY(sp3d,mp3d,VELOCITY_EFFECT,AAX_SOUND_VELOCITY);
                     _EFFECT_COPY(sp3d,mp3d,VELOCITY_EFFECT,AAX_DOPPLER_FACTOR);
                     _EFFECT_COPY_DATA(sp3d, mp3d, VELOCITY_EFFECT);
                  }

                  config->handle = handle;
                  config->pos = pos;
                  submix->refcount++;
                  submix->thread = AAX_TRUE;

                  if (!submix->ringbuffer) {
                     submix->ringbuffer = _oalRingBufferCreate(AAX_TRUE);
                  }

                  rb = submix->ringbuffer;
                  if (rb)
                  {
                     _aaxMixerInfo* info = submix->info;
                     const _aaxDriverBackend *be;
                     float delay_sec;

                     be = _aaxGetDriverBackendLoopback();
                     delay_sec = 1.0f / info->refresh_rate;

                     _oalRingBufferSetFormat(rb, be->codecs, AAX_PCM24S);
                     _oalRingBufferSetFrequency(rb, info->frequency);
                     _oalRingBufferSetNoTracks(rb, 2);

                     /* create a ringbuffer with a but of overrun space */
                     _oalRingBufferSetDuration(rb, delay_sec*1.0f);
                     _oalRingBufferInit(rb, AAX_TRUE);

                     /* 
                      * Now set the actual duration, this will not alter the
                      * allocated space since it is lower that the initial
                      * duration.
                      */
                     _oalRingBufferSetDuration(rb, delay_sec);
                     _oalRingBufferStart(rb);
                  }

                  _intBufReleaseData(dptr, _AAX_SENSOR);
                  rv = AAX_TRUE;
               }
            }
            else {
               _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            }
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_STATE);
      }
      put_frame(handle);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

int
aaxAudioFrameDeregisterSensor(const aaxFrame frame, const aaxConfig sensor)
{
   _frame_t* handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      _handle_t* config = get_handle(sensor);
      if (config && config->pos != UINT_MAX)
      {
         _intBuffers *hs = handle->submix->sensors;
         _intBufferData *dptr;

         _intBufRemove(hs, _AAX_DEVICE, config->pos, AAX_FALSE);

         dptr = _intBufGet(config->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            handle->submix->no_registered--;
            sensor->mixer->refcount--;
            config->handle = NULL;
            config->pos = UINT_MAX;

            _intBufReleaseData(dptr, _AAX_SENSOR);
            rv = AAX_TRUE;
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
      put_frame(frame);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}


int
aaxAudioFrameRegisterEmitter(const aaxFrame frame, const aaxEmitter em)
{
   _frame_t* handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      _emitter_t* emitter = get_emitter_unregistered(em);
      if (emitter && !emitter->handle && emitter->pos == UINT_MAX)
      {
         _aaxAudioFrame* mixer = handle->submix;
         _aaxEmitter *src = emitter->source;
         unsigned int pos = UINT_MAX;
         unsigned int positional;
         _intBuffers *he;

         positional = _IS_POSITIONAL(src);
         if (!positional) {
            he = mixer->emitters_2d;
         } else {
            he = mixer->emitters_3d;
         }

         if (mixer->no_registered < mixer->info->max_registered)
         {
            if (_oalRingBufferGetSource())
            {
               pos = _intBufAddData(he, _AAX_EMITTER, emitter);
               mixer->no_registered++;
            }
         }

         if (pos != UINT_MAX)
         {
            _aaxEmitter *src = emitter->source;
            emitter->handle = handle;
            emitter->pos = pos;

            /*
             * The mixer frequency is not know by the frequency filter
             * (which needs it to properly compute the filter coefficients)
             * until it has been registered to the mixer.
             * So we need to update the filter coefficients one more time.
             */
            if (_FILTER_GET2D_DATA(src, FREQUENCY_FILTER))
            {
               aaxFilter f = aaxEmitterGetFilter(emitter, AAX_FREQUENCY_FILTER);
               _filter_t *filter = (_filter_t *)f;
               aaxFilterSetState(f, filter->slot[0]->state);
               aaxEmitterSetFilter(emitter, f);
            }

            src->info = mixer->info;
            if (src->update_rate == 0) {
               src->update_rate = mixer->info->update_rate;
            }
            src->update_ctr = src->update_rate;

            if (positional)
            {
               _oalRingBuffer3dProps *mp3d, *ep3d = src->props3d;

               mp3d = mixer->props3d;
               if (mixer->dist_delaying) {
                  _PROP_DISTDELAY_SET_DEFINED(ep3d);
               }

               if (_FILTER_GET_DATA(ep3d, DISTANCE_FILTER) == NULL) {
                  _FILTER_COPY_DATA(ep3d, mp3d, DISTANCE_FILTER);
               }

               if (_EFFECT_GET_DATA(ep3d, VELOCITY_EFFECT) == NULL)
               {
                  _EFFECT_COPY(ep3d, mp3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);
                  _EFFECT_COPY(ep3d, mp3d, VELOCITY_EFFECT, AAX_DOPPLER_FACTOR);
                  _EFFECT_COPY_DATA(ep3d, mp3d, VELOCITY_EFFECT);
               }
            }
            rv = AAX_TRUE;
         }
         else {
            _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         }
      }
      else {
//       if (emitter->handle) put_emitter(emitter);
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_frame(frame);
   return rv;
}

int
aaxAudioFrameDeregisterEmitter(const aaxFrame frame, const aaxEmitter em)
{
   _frame_t* handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      _emitter_t* emitter = get_emitter(em);
      if (emitter && emitter->pos != UINT_MAX)
      {
         _aaxAudioFrame* mixer = handle->submix;
         _aaxEmitter *src = emitter->source;
         _intBuffers *he;

         if (_IS_POSITIONAL(src))
         {
            he = mixer->emitters_3d;
            _PROP_DISTDELAY_CLEAR_DEFINED(src->props3d);
         } else {
            he = mixer->emitters_2d;
         }

         _intBufRemove(he, _AAX_EMITTER, emitter->pos, AAX_FALSE);
         _oalRingBufferPutSource();
         mixer->no_registered--;
         emitter->handle = NULL;
         emitter->pos = UINT_MAX;
         rv = AAX_TRUE;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
      put_emitter(emitter);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_frame(frame);
   return rv;
}

int
aaxAudioFrameRegisterAudioFrame(const aaxFrame frame, const aaxFrame subframe)
{
   _frame_t* handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      _frame_t* sframe = get_frame(subframe);
      if (sframe && !sframe->handle && !sframe->thread.started)
      {
         if (sframe->pos == UINT_MAX)
         {
            _aaxAudioFrame *mixer =  handle->submix;
            _intBuffers *hf = mixer->frames;
            unsigned int pos = UINT_MAX;

            if (hf == NULL)
            {
               unsigned int res;

               res = _intBufCreate(&mixer->frames, _AAX_FRAME);
               if (res != UINT_MAX) {
                  hf = mixer->frames;
               }
            }

            if (hf && (mixer->no_registered < mixer->info->max_registered))
            {
               aaxBuffer buf; /* clear the frames buffer queue */
               while ((buf = aaxAudioFrameGetBuffer(sframe)) != NULL) {
                  aaxBufferDestroy(buf);
               }
               pos = _intBufAddData(hf, _AAX_FRAME, sframe);
               mixer->no_registered++;
            }
            else {
               _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            }

            if (pos != UINT_MAX)
            {
               _aaxAudioFrame *submix = sframe->submix;
               _oalRingBuffer3dProps *mp3d, *fp3d;

               mp3d = mixer->props3d;
               fp3d = submix->props3d;

               submix->dist_delaying = mixer->dist_delaying;
               if (_FILTER_GET_DATA(fp3d, DISTANCE_FILTER) == NULL) {
                  _FILTER_COPY_DATA(fp3d, mp3d, DISTANCE_FILTER);
               }

               if (_EFFECT_GET_DATA(fp3d, VELOCITY_EFFECT) == NULL)
               {
                  _EFFECT_COPY(fp3d, mp3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);
                  _EFFECT_COPY(fp3d, mp3d, VELOCITY_EFFECT, AAX_DOPPLER_FACTOR);
                  _EFFECT_COPY_DATA(fp3d, mp3d, VELOCITY_EFFECT);
               }
               rv = AAX_TRUE;

               submix->thread = AAX_FALSE;
               submix->refcount++;
               sframe->handle = handle;
               sframe->pos = pos;
            }
            else {
               _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            }
         }

         /* No need to put the sframe since it was not registered yet.
          * This means there is no lock to unlock 
          * put_frame(sframe);
          */
      }
      else
      {
         if (sframe->handle) put_frame(sframe);
         _aaxErrorSet(AAX_INVALID_STATE);
      }
      put_frame(handle);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

int
aaxAudioFrameDeregisterAudioFrame(const aaxFrame frame, const aaxFrame subframe)
{
   _frame_t* handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      _frame_t* frame = get_frame(subframe);
      if (frame && frame->pos != UINT_MAX)
      {
         _intBuffers *hf = handle->submix->frames;

         _intBufRemove(hf, _AAX_FRAME, frame->pos, AAX_FALSE);
         handle->submix->no_registered--;
         frame->submix->refcount--;
         frame->handle = NULL;
         frame->pos = UINT_MAX;
         rv = AAX_TRUE;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
      put_frame(frame);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

int
aaxAudioFrameSetState(aaxFrame frame, enum aaxState state)
{
   _frame_t* handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      switch (state)
      {
      case AAX_UPDATE:
         rv = _aaxAudioFrameSignal(handle);
         break;
      case AAX_SUSPENDED:
         _SET_PAUSED(handle);
         rv = AAX_TRUE;
         break;
      case AAX_STANDBY:
         _SET_STANDBY(handle);
         rv = AAX_TRUE;
         break;
      case AAX_PLAYING:
         put_frame(frame);
         rv = _aaxAudioFrameStart(handle);
         handle = get_frame(frame);
         if (rv) _SET_PLAYING(handle);
         break;
      case AAX_STOPPED:
         rv = _aaxAudioFrameStop(handle);
         if (rv) _SET_PROCESSED(handle);
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_frame(frame);
   return rv;
}

int
aaxAudioFrameWaitForBuffer(const aaxFrame frame, float timeout)
{
   _frame_t* handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      static struct timespec sleept = {0, 1000};
      float sleep, duration = 0.0f;
      _aaxAudioFrame* mixer;
      int nbuf;

      sleep = 1.0f / (handle->submix->info->refresh_rate * 10.0f);
      sleept.tv_nsec = sleep * 1e9f;

      mixer = handle->submix;
      put_frame(frame);

      do
      {
         nbuf = 0;
         duration += sleep;

         nbuf = _intBufGetNumNoLock(mixer->ringbuffers, _AAX_RINGBUFFER);
         if (!nbuf)
         {
            int err = nanosleep(&sleept, 0);
            if (err < 0) break;
         }
      }
      while ((nbuf == 0) && (duration < timeout));

      if (nbuf) rv = AAX_TRUE;
      else _aaxErrorSet(AAX_TIMEOUT);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

aaxBuffer
aaxAudioFrameGetBuffer(const aaxFrame frame)
{
   _frame_t* handle = get_frame(frame);
   aaxBuffer buffer = NULL;
   if (handle)
   {
      _aaxAudioFrame* mixer = handle->submix;
      _intBuffers *ringbuffers = mixer->ringbuffers;
      unsigned int nbuf;

      nbuf = _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);
      if (nbuf > 0)
      {
         void **ptr = _intBufShiftIndex(ringbuffers, _AAX_RINGBUFFER, 0, 1);
         if (ptr)
         {
            _buffer_t *buf = calloc(1, sizeof(_buffer_t));
            if (buf)
            {
               _oalRingBuffer *rb = (_oalRingBuffer *)ptr[0];
               buf->ringbuffer = rb;
               buf->format = _oalRingBufferGetFormat(rb);
               buf->ref_counter = 1;
               buf->mipmap = AAX_FALSE;
               buf->id = BUFFER_ID;
               buffer = (aaxBuffer)buf;
            }
            free(ptr);
         }
         else {
            _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         }
      }
      _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);
   }
   put_frame(frame);

   return buffer;
}

/* -------------------------------------------------------------------------- */

_frame_t*
get_frame(aaxFrame f)
{
   _frame_t* frame = (_frame_t*)f;
   _frame_t* rv = NULL;
 
   if (frame && (frame->id == AUDIOFRAME_ID))
   {
      _handle_t *handle = frame->handle;
      if (handle && handle->id == HANDLE_ID)
      {
         _intBufferData *dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = sensor->mixer;
            _intBufferData *dptr_frame;
            _intBuffers *hf;

            hf = mixer->frames;
            dptr_frame = _intBufGet(hf, _AAX_FRAME, frame->pos);
            frame = _intBufGetDataPtr(dptr_frame);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
      else if (handle && handle->id == AUDIOFRAME_ID)
      {
         _frame_t *handle = (_frame_t*)frame->handle;
         _intBufferData *dptr_frame;
         _intBuffers *hf;

         hf =  handle->submix->frames;
         dptr_frame = _intBufGet(hf, _AAX_FRAME, frame->pos);
         frame = _intBufGetDataPtr(dptr_frame);
      }
      rv = frame;
   }
   return rv;
}

void
put_frame(aaxFrame f)
{
   _frame_t* frame = (_frame_t*)f;

   if (frame && (frame->id == AUDIOFRAME_ID))
   {
      _handle_t *handle = frame->handle;
      if (handle && handle->id == HANDLE_ID)
      {
          _intBufferData *dptr =_intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = sensor->mixer;
            _intBuffers *hf;

            hf = mixer->frames;
            _intBufRelease(hf, _AAX_FRAME, frame->pos);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
      else if (handle && handle->id == AUDIOFRAME_ID)
      {
         _frame_t *handle = (_frame_t*)frame->handle;
         _intBuffers *hf;

         hf =  handle->submix->frames;
         _intBufRelease(hf, _AAX_FRAME, frame->pos);
      }
   }
}

int
_aaxAudioFrameStart(_frame_t *frame)
{
   int rv = AAX_FALSE;

   assert(frame);

// if (frame->handle && TEST_FOR_FALSEframe->thread.started))
   if ( (_IS_INITIAL(frame) || _IS_PROCESSED(frame)) && (frame->submix->thread) )
   {
      int r;

      frame->thread.ptr = _aaxThreadCreate();
      assert(frame->thread.ptr != 0);

      frame->thread.condition = _aaxConditionCreate();
      assert(frame->thread.condition != 0);

      frame->thread.mutex = _aaxMutexCreate(0);
      assert(frame->thread.mutex != 0);

      frame->thread.started = AAX_TRUE;
      r = _aaxThreadStart(frame->thread.ptr, _aaxAudioFrameThread, frame);
      if (r == 0)
      {
         int p = 0;
         do
         {
            static const struct timespec sleept = {0, 100000};
            nanosleep(&sleept, 0);
            r = (frame->submix->ringbuffer != 0);
            if (p++ > 5000) break;
         }
         while (r == 0);

         if (r == 0)
         {
            _aaxErrorSet(AAX_TIMEOUT);
            frame->thread.started = AAX_FALSE;
         }
         else
         {
            frame->submix->capturing = AAX_TRUE; // REGISTERED_FRAME;
            rv = AAX_TRUE;
         }
      }   
      else {
         _aaxErrorSet(AAX_INVALID_STATE);
      }
   }
   else if (_IS_STANDBY(frame) || !frame->submix->thread) {
      rv = AAX_TRUE;
   }
   return rv;
}

int
_aaxAudioFrameStop(_frame_t *frame)
{
   int rv = AAX_FALSE;
   if TEST_FOR_TRUE(frame->thread.started)
   {
      frame->thread.started = AAX_FALSE;
      _aaxConditionSignal(frame->thread.condition);
      _aaxThreadJoin(frame->thread.ptr);

      _aaxConditionDestroy(frame->thread.condition);
      _aaxMutexDestroy(frame->thread.mutex);
      _aaxThreadDestroy(frame->thread.ptr);
      rv = AAX_TRUE;
   } else if (!frame->submix->thread) {
      rv = AAX_TRUE;
   }
   return rv;
}

int
_aaxAudioFrameSignal(_frame_t *frame)
{
   int rv = AAX_FALSE;
   if TEST_FOR_TRUE(frame->thread.started)
   {
      _aaxConditionSignal(frame->thread.condition);
      rv = AAX_TRUE;
   }
   return rv;
}

void
_aaxAudioFramePlayFrame(void* frame, const void* backend, void* sensor, void* be_handle)
{
   const _aaxDriverBackend* be = (const _aaxDriverBackend*)backend;
   _aaxAudioFrame* mixer = (_aaxAudioFrame*)frame;
   _oalRingBuffer *dest_rb = mixer->ringbuffer;
   // unsigned int rv;

   /** process registered sensors */
   if (mixer->sensors) {
      _aaxSoftwareMixerMixSensors(dest_rb, mixer->sensors, mixer->props2d);
   }

   /* postprocess registered (non threaded) audio frames */
   if (mixer->frames)
   {
      _oalRingBuffer2dProps sp2d;
      _oalRingBuffer3dProps sp3d;
      unsigned int i, num;
      _intBuffers *hf;

      /* copying here prevents locking the listener the whole time */
      /* it's used for just one time-frame anyhow                  */
      memcpy(&sp2d, mixer->props2d, sizeof(_oalRingBuffer2dProps));
      memcpy(&sp2d.pos, mixer->info->speaker, _AAX_MAX_SPEAKERS*sizeof(vec4));
      memcpy(&sp2d.hrtf, mixer->info->hrtf, 2*sizeof(vec4));
      memcpy(&sp3d, mixer->props3d, sizeof(_oalRingBuffer3dProps));

      hf = mixer->frames;
      num = _intBufGetMaxNum(hf, _AAX_FRAME);
      for (i=0; i<num; i++)
      {
         _intBufferData *dptr = _intBufGet(hf, _AAX_FRAME, i);
         if (dptr)
         {
            _frame_t* subframe = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *fmixer = subframe->submix;

            _intBufReleaseData(dptr, _AAX_FRAME);

            _aaxSoftwareMixerProcessFrame(dest_rb, mixer->info, &sp2d, &sp3d,
                                          fmixer->props2d, fmixer->props3d,
                                          fmixer->emitters_2d,
                                          fmixer->emitters_3d,
                                          be, NULL);
         }
      }
      _intBufReleaseNum(hf, _AAX_FRAME);
      _aaxSoftwareMixerMixFrames(dest_rb, mixer->frames);
   }

   be->effects(be_handle, dest_rb, mixer->props2d);
   be->postprocess(be_handle, dest_rb, sensor);

   if TEST_FOR_TRUE(mixer->capturing)
   {
      char dde = (_EFFECT_GET2D_DATA(mixer, DELAY_EFFECT) != NULL);
      _intBuffers *ringbuffers = mixer->ringbuffers;
      unsigned int nbuf; 
 
      nbuf = _intBufGetNumNoLock(ringbuffers, _AAX_RINGBUFFER);
      if (nbuf == 0)
      {
         _oalRingBuffer *nrb = _oalRingBufferDuplicate(dest_rb, AAX_TRUE, dde);
         _intBufAddData(ringbuffers, _AAX_RINGBUFFER, dest_rb);
         dest_rb = nrb;
      }
      else
      {
         _intBufferData *buf;
         _oalRingBuffer *nrb;

         _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);
         buf = _intBufPopData(ringbuffers, _AAX_RINGBUFFER);

         /* switch ringbuffers */
         nrb = _intBufSetDataPtr(buf, dest_rb);
         _intBufPushData(ringbuffers, _AAX_RINGBUFFER, buf);
         _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);

         if (dde) {
            _oalRingBufferCopyDelyEffectsData(nrb, dest_rb);
         }
         dest_rb = nrb;
      }
      mixer->ringbuffer = dest_rb;
      mixer->capturing++;
   }

   _oalRingBufferClear(dest_rb);
   _oalRingBufferStart(dest_rb);
}


void*
_aaxAudioFrameThread(void* config)
{
   _frame_t *frame = (_frame_t *)config;
   _aaxAudioFrame *smixer, *fmixer, *mixer;
   const _aaxDriverBackend *be;
   _oalRingBuffer *dest_rb;
   _handle_t* handle;
   struct timespec ts;
   float delay_sec;
   float elapsed;
   float dt = 0.0f;
   int res = 0;

   if (!frame || !frame->submix->info->no_tracks) {
      return NULL;
   }
   handle = frame->handle;

   dest_rb = _oalRingBufferCreate(AAX_TRUE);
   if (!dest_rb) {
      return NULL;
   }

   fmixer = NULL;
   smixer = frame->submix;
   delay_sec = 1.0f / smixer->info->refresh_rate;

   be = _aaxGetDriverBackendLoopback();		/* be = handle->backend.ptr */
   if (be)
   {
      _aaxMixerInfo* info = smixer->info;
      int tracks;

      /* unregistered frames that are positional are mono */
      tracks = (!handle && _IS_POSITIONAL(frame)) ? 1 : info->no_tracks;
      _oalRingBufferSetNoTracks(dest_rb, tracks);

      _oalRingBufferSetFormat(dest_rb, be->codecs, AAX_PCM24S);
      _oalRingBufferSetFrequency(dest_rb, info->frequency);
      _oalRingBufferSetDuration(dest_rb, delay_sec);
      _oalRingBufferInit(dest_rb, AAX_TRUE);
      _oalRingBufferStart(dest_rb);
   }

   mixer = smixer;
   if (handle)	/* frame is registered */
   {
      assert (handle->id == HANDLE_ID);

      if (handle->id == HANDLE_ID)
      {
         _intBufferData *dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            smixer = sensor->mixer;
            fmixer = frame->submix;
            mixer = fmixer;
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
   }
#if 0
   else /* frame is not registered */
   {
      mixer = smixer = fmixer = frame->submix;
      memcpy(&sp2d, smixer->props2d, sizeof(_oalRingBuffer2dProps));
      memcpy(&sp2d.pos, smixer->info->speaker,
                                     _AAX_MAX_SPEAKERS*sizeof(vec4));
      memcpy(&sp2d.hrtf, smixer->info->hrtf, 2*sizeof(vec4));
      memcpy(&sp3d, smixer->props3d, sizeof(_oalRingBuffer3dProps));
   }
#endif

   /* get real duration, it might have been altered for better performance */
   mixer->ringbuffer = dest_rb;
   delay_sec = _oalRingBufferGetDuration(dest_rb);

   elapsed = 0.0f;
   _aaxMutexLock(frame->thread.mutex);
   do
   {
      float delay = delay_sec;			/* twice as slow when standby */

//    if (_IS_STANDBY(frame)) delay *= 2;
      elapsed -= delay;
      if (elapsed <= 0.0f)
      {
         struct timeval now;
         float fdt;

         elapsed += 60.0f;               /* resync the time every 60 seconds */

         dt = delay;
         fdt = floorf(dt);

         gettimeofday(&now, 0);
         ts.tv_sec = now.tv_sec + fdt;

         dt -= fdt;
         dt += now.tv_usec*1e-6f;
         ts.tv_nsec = dt*1e9f;
         if (ts.tv_nsec >= 1e9f)
         {
            ts.tv_sec++;
            ts.tv_nsec -= 1e9f;
         }
      }
      else
      {
         dt += delay;
         if (dt >= 1.0f)
         {
            float fdt = floorf(dt);
            ts.tv_sec += fdt;
            dt -= fdt;
         }
         ts.tv_nsec = dt*1e9f;
      }

      if TEST_FOR_FALSE(frame->thread.started) {
         break;
      }

      if (_IS_PLAYING(frame) || _IS_STANDBY(frame))
      {
         if (mixer->emitters_3d || mixer->emitters_2d || mixer->frames)
         {
            if (_IS_PLAYING(frame) && be->is_available(NULL)) {
               _aaxAudioFrameProcessFrame(handle,frame,mixer,smixer,fmixer,be);
            }
            else { /* if (_IS_STANDBY(frame)) */
               _aaxNoneDriverProcessFrame(mixer);
            }
         }
      }

      /**
       * _aaxSoftwareMixerSignalFrames uses _aaxConditionSignal to let the
       * frame procede in advance, before the main thread starts mixing so
       * threads will be finished soon after the main thread.
       * As a result _aaxConditionWaitTimed may return 0 instead, which is
       * not a problem since earlier in the loop there is a test to see if
       * the thread really is finished and then breaks the loop.
       *
       * Note: the thread will not be signaled to start mixing if there's
       *       already a buffer in it's buffer queue.
       */
      res = _aaxConditionWaitTimed(frame->thread.condition,
                                   frame->thread.mutex, &ts);
   }
   while ((res == ETIMEDOUT) || (res == 0));

   _aaxMutexUnLock(frame->thread.mutex);
   _oalRingBufferStop(mixer->ringbuffer);
   _oalRingBufferDelete(mixer->ringbuffer);
   mixer->ringbuffer = 0;

   return frame;
}

void
_aaxAudioFrameProcessFrame(_handle_t* handle, _frame_t *frame,
          _aaxAudioFrame *mixer, _aaxAudioFrame *smixer, _aaxAudioFrame *fmixer,
          const _aaxDriverBackend *be)
{
   void* be_handle = NULL;
   _oalRingBuffer2dProps sp2d;
   _oalRingBuffer3dProps sp3d;
   _oalRingBuffer2dProps fp2d;
   _oalRingBuffer3dProps fp3d;

   if (handle) /* frame is registered */
   {
      _handle_t* handle = frame->handle;
      _intBufferData *dptr;

       /* copying prevents locking the listener the whole time */
       /* it's used for just one time-frame anyhow             */
       dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
       if (dptr)
       {
          memcpy(&sp2d, smixer->props2d, sizeof(_oalRingBuffer2dProps));
          memcpy(&sp2d.pos, smixer->info->speaker, _AAX_MAX_SPEAKERS*sizeof(vec4));
          memcpy(&sp2d.hrtf, smixer->info->hrtf, 2*sizeof(vec4));
          memcpy(&sp3d, smixer->props3d, sizeof(_oalRingBuffer3dProps));
          _intBufReleaseData(dptr, _AAX_SENSOR);

       }
       be_handle = handle->backend.handle; 
   }
   memcpy(&fp3d, fmixer->props3d, sizeof(_oalRingBuffer3dProps));
   memcpy(&fp2d, fmixer->props2d, sizeof(_oalRingBuffer2dProps));
   memcpy(&fp2d.pos, fmixer->info->speaker, _AAX_MAX_SPEAKERS*sizeof(vec4));
   memcpy(&fp2d.hrtf, fmixer->info->hrtf, 2*sizeof(vec4));

   /** process threaded frames */
   _aaxSoftwareMixerProcessFrame(mixer->ringbuffer, mixer->info,
                                              &sp2d, &sp3d, &fp2d, &fp3d,
                                              mixer->emitters_2d,
                                              mixer->emitters_3d,
                                              be, be_handle);

   /** process registered audio-frames and sensors */
   _aaxAudioFramePlayFrame(mixer, be, NULL, be_handle);
}
