/*
 * Copyright 2011-2013 by Erik Hofman.
 * Copyright 2011-2013 by Adalin B.V.
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

#include <errno.h>		/* for ETIMEDOUT */
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>		/* for struct time */
#endif
#include <assert.h>
#include <math.h>

#include <aax/aax.h>

#include <base/threads.h>
#include <base/timer.h>		/* for msecSleep */
#include "api.h"
#include "arch.h"
#include "driver.h"

static int _aaxAudioFrameStart(_frame_t*);
static int _aaxAudioFrameUpdate(_frame_t*);

AAX_API aaxFrame AAX_APIENTRY
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

         submix->props3d = _aax3dPropsCreate();
         if (submix->props3d)
         {
            const _intBufferData* dptr;
            dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               _aaxAudioFrame* mixer = sensor->mixer;
               _FILTER_COPYD3D_DATA(submix, mixer, DISTANCE_FILTER);
               _EFFECT_COPYD3D(submix,mixer,VELOCITY_EFFECT,AAX_SOUND_VELOCITY);
               _EFFECT_COPYD3D(submix,mixer,VELOCITY_EFFECT,AAX_DOPPLER_FACTOR);
               _EFFECT_COPYD3D_DATA(submix, mixer, VELOCITY_EFFECT);

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
            res = _intBufCreate(&submix->frame_ringbuffers, _AAX_RINGBUFFER);
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

AAX_API int AAX_APIENTRY
aaxAudioFrameDestroy(aaxFrame frame)
{
   _frame_t* handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      if (!handle->handle)
      {
         _oalRingBufferDelayEffectData* effect;
         _aaxAudioFrame* fmixer = handle->submix;

         free(_FILTER_GET2D_DATA(fmixer, FREQUENCY_FILTER));
         free(_FILTER_GET2D_DATA(fmixer, DYNAMIC_GAIN_FILTER));
         free(_FILTER_GET2D_DATA(fmixer, TIMED_GAIN_FILTER));
         free(_EFFECT_GET2D_DATA(fmixer, DYNAMIC_PITCH_EFFECT));

         effect = _EFFECT_GET2D_DATA(fmixer, DELAY_EFFECT);
         if (effect) free(effect->history_ptr);
         free(effect);

         if (fmixer->p3dq) {
            _intBufErase(&fmixer->p3dq, _AAX_DELAYED3D, _aax_aligned_free);
         }
         _aax_aligned_free(fmixer->props3d->dprops3d);
         free(fmixer->props3d);

         /* handle->ringbuffer gets removed bij the frame thread */
         /* _oalRingBufferDelete(handle->ringbuffer); */
         _intBufErase(&fmixer->frames, _AAX_FRAME, free);
         _intBufErase(&fmixer->devices, _AAX_DEVICE, free);
         _intBufErase(&fmixer->emitters_2d, _AAX_EMITTER, free);
         _intBufErase(&fmixer->emitters_3d, _AAX_EMITTER, free);
         _intBufErase(&fmixer->ringbuffers, _AAX_RINGBUFFER,
                      _oalRingBufferClear);
         _intBufErase(&fmixer->frame_ringbuffers, _AAX_RINGBUFFER,
                      _oalRingBufferClear);

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

AAX_API int AAX_APIENTRY
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

         mtx4Copy(mixer->props3d->dprops3d->matrix, mtx);
         if (_IS_RELATIVE(handle))
         {
            mixer->props3d->dprops3d->matrix[LOCATION][3] = 0.0f;
            mixer->props3d->dprops3d->velocity[VELOCITY][3] = 0.0f;
         }
         else
         {
            mixer->props3d->dprops3d->matrix[LOCATION][3] = 1.0f;
            mixer->props3d->dprops3d->velocity[VELOCITY][3] = 1.0f;
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

AAX_API int AAX_APIENTRY
aaxAudioFrameGetMatrix(aaxFrame frame, aaxMtx4f mtx)
{
   _frame_t *handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      if (mtx)
      {
         mtx4Copy(mtx, handle->submix->props3d->dprops3d->matrix);
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

AAX_API int AAX_APIENTRY
aaxAudioFrameSetVelocity(aaxFrame frame, const aaxVec3f velocity)
{
   _frame_t *handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      if (velocity && !detect_nan_vec3(velocity))
      {
         _oalRingBufferDelayed3dProps *dp3d;

         dp3d = handle->submix->props3d->dprops3d;
         vec3Copy(dp3d->velocity[VELOCITY], velocity);
         _PROP_SPEED_SET_CHANGED(handle->submix->props3d);

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

AAX_API int AAX_APIENTRY
aaxAudioFrameGetVelocity(aaxFrame frame, aaxVec3f velocity)
{
   _frame_t *handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      if (velocity)
      {
         _oalRingBufferDelayed3dProps *dp3d;

         dp3d = handle->submix->props3d->dprops3d;
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
   put_frame(frame);
   return rv;
}

AAX_API unsigned int AAX_APIENTRY
aaxAudioFrameGetSetup(const aaxConfig frame, enum aaxSetupType type)
{
   _frame_t *handle = get_frame(frame);
   unsigned int rv = AAX_FALSE;
   
   if (handle)
   {
      if (type & AAX_COMPRESSION_VALUE)
      {
         unsigned int track = type & 0x3F;
         if (track < _AAX_MAX_SPEAKERS)
         {
            _aaxAudioFrame* fmixer = handle->submix;
            _oalRingBufferLFOInfo *lfo;

            lfo = _FILTER_GET2D_DATA(fmixer, DYNAMIC_GAIN_FILTER);
            if (lfo) {
               rv = 256*32768*lfo->compression[track];
            }
         }
      }
      else if (type & AAX_GATE_ENABLED)
      {
         unsigned int track = type & 0x3F;
         if (track < _AAX_MAX_SPEAKERS)
         {
            _aaxAudioFrame* fmixer = handle->submix;
            _oalRingBufferLFOInfo *lfo;

            lfo = _FILTER_GET2D_DATA(fmixer, DYNAMIC_GAIN_FILTER);
            if (lfo && (lfo->average[track] <= lfo->gate_threshold)) {
               rv = AAX_TRUE;
            }
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_frame(frame);
   return rv;
}

AAX_API int AAX_APIENTRY
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
         case AAX_GRAPHIC_EQUALIZER:
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
         case AAX_COMPRESSOR:
         {
            _oalRingBuffer2dProps *p2d = handle->submix->props2d;
            _FILTER_SET(p2d, type, 0, _FILTER_GET_SLOT(filter, 0, 0));
            _FILTER_SET(p2d, type, 1, _FILTER_GET_SLOT(filter, 0, 1));
            _FILTER_SET(p2d, type, 2, _FILTER_GET_SLOT(filter, 0, 2));
            _FILTER_SET(p2d, type, 3, _FILTER_GET_SLOT(filter, 0, 3));
            _FILTER_SET_STATE(p2d, type, _FILTER_GET_SLOT_STATE(filter));
            _FILTER_SWAP_SLOT_DATA(p2d, type, filter, 0);
            if (filter->type == AAX_DYNAMIC_GAIN_FILTER ||
                filter->type == AAX_COMPRESSOR) {
               p2d->final.gain_lfo = 1.0f;
            }
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

AAX_API aaxFilter AAX_APIENTRY
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
      case AAX_COMPRESSOR:
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

AAX_API int AAX_APIENTRY
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
            if ((enum aaxEffectType)effect->type == AAX_DYNAMIC_PITCH_EFFECT) {
               p2d->final.pitch_lfo = 1.0f;
            }
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

AAX_API aaxEffect AAX_APIENTRY
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

AAX_API int AAX_APIENTRY
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
               mixer->props3d->dprops3d->matrix[LOCATION][3] = 0.0f;
            } else {
               mixer->props3d->dprops3d->matrix[LOCATION][3] = 1.0f;
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

AAX_API int AAX_APIENTRY
aaxAudioFrameRegisterSensor(const aaxFrame frame, const aaxConfig sensor)
{
   _frame_t* fhandle = get_frame(frame);
   int rv = AAX_FALSE;
   if (fhandle)
   {
      _handle_t* ssr_config = get_read_handle(sensor);
      if (ssr_config && !ssr_config->thread.started)
      {
         if (ssr_config->pos == UINT_MAX)
         {
            _aaxAudioFrame* fmixer = fhandle->submix;
            _intBuffers *hd = fmixer->devices;
            unsigned int pos = UINT_MAX;

            if (hd == NULL)
            {
               unsigned int res;

               res = _intBufCreate(&fmixer->devices, _AAX_DEVICE);
               if (res != UINT_MAX) {
                  hd = fmixer->devices;
               }
            }

            if (hd && (fmixer->no_registered < fmixer->info->max_registered))
            {
               aaxBuffer buf; /* clear the sensors buffer queue */
               while ((buf = aaxSensorGetBuffer(ssr_config)) != NULL) {
                  aaxBufferDestroy(buf);
               }
               pos = _intBufAddData(hd, _AAX_DEVICE, ssr_config);
               fmixer->no_registered++;
            }
            else {
               _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            }

            if (pos != UINT_MAX)
            {
               _intBufferData *dptr;

               dptr = _intBufGet(ssr_config->sensors, _AAX_SENSOR, 0);
               if (dptr)
               {
                  _sensor_t* sensor = _intBufGetDataPtr(dptr);
                  _oalRingBuffer3dProps *mp3d, *sp3d;
                  _aaxAudioFrame *submix;
                  _oalRingBuffer *rb;

                  submix = sensor->mixer;

                  mp3d = fmixer->props3d;
                  sp3d = submix->props3d;

                  submix->info->frequency = fmixer->info->frequency;
                  while (submix->info->frequency > 48000.0f) {
                     submix->info->frequency /= 2.0f;
                  }
                  submix->info->refresh_rate = fmixer->info->refresh_rate;
                  submix->info->update_rate = fmixer->info->update_rate;
                  if (_FILTER_GET_DATA(sp3d, DISTANCE_FILTER) == NULL)
                  {
                     _FILTER_COPY_STATE(sp3d, mp3d, DISTANCE_FILTER);
                     _FILTER_COPY_DATA(sp3d, mp3d, DISTANCE_FILTER);
                  }
                  _aaxAudioFrameResetDistDelay(submix, fmixer);

                  if (_EFFECT_GET_DATA(sp3d, VELOCITY_EFFECT) == NULL)
                  {
                     _EFFECT_COPY(sp3d,mp3d,VELOCITY_EFFECT,AAX_SOUND_VELOCITY);
                     _EFFECT_COPY(sp3d,mp3d,VELOCITY_EFFECT,AAX_DOPPLER_FACTOR);
                     _EFFECT_COPY_DATA(sp3d, mp3d, VELOCITY_EFFECT);
                  }

                  ssr_config->handle = fhandle;
                  ssr_config->pos = pos;
                  submix->refcount++;
                  submix->thread = AAX_TRUE;

                  if (!submix->ringbuffer) {
                     submix->ringbuffer = _oalRingBufferCreate(DELAY_EFFECTS_TIME);
                  }

                  rb = submix->ringbuffer;
                  if (rb)
                  {
                     _aaxMixerInfo* info = submix->info;
                     const _aaxDriverBackend *be;
                     unsigned int pos;
                     float delay_sec;

                     be = _aaxGetDriverBackendLoopback(&pos);
                     delay_sec = 1.0f / info->refresh_rate;

                     _oalRingBufferSetFormat(rb, be->codecs, AAX_PCM24S);
                     _oalRingBufferSetParamf(rb, RB_FREQUENCY, info->frequency);
                     _oalRingBufferSetParami(rb, RB_NO_TRACKS, 2);

                     /* create a ringbuffer with a but of overrun space */
                     _oalRingBufferSetParamf(rb, RB_DURATION_SEC, delay_sec*1.0f);
                     _oalRingBufferInit(rb, AAX_TRUE);

                     /* 
                      * Now set the actual duration, this will not alter the
                      * allocated space since it is lower that the initial
                      * duration.
                      */
                     _oalRingBufferSetParamf(rb, RB_DURATION_SEC, delay_sec);
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
      put_frame(fhandle);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameDeregisterSensor(const aaxFrame frame, const aaxConfig sensor)
{
   _frame_t* fhandle = get_frame(frame);
   int rv = AAX_FALSE;
   if (fhandle)
   {
      _handle_t* ssr_config = get_handle(sensor);
      if (ssr_config && ssr_config->pos != UINT_MAX)
      {
         _intBuffers *hd = fhandle->submix->devices;
         _handle_t* ptr;

         ptr = _intBufRemove(hd, _AAX_DEVICE, ssr_config->pos, AAX_FALSE);
         if (ptr)
         {
            _intBufferData *dptr;

            fhandle->submix->no_registered--;
            dptr = _intBufGet(ssr_config->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               sensor->mixer->refcount--;
               ssr_config->handle = NULL;
               ssr_config->pos = UINT_MAX;

               _intBufReleaseData(dptr, _AAX_SENSOR);
               rv = AAX_TRUE;
            }
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


AAX_API int AAX_APIENTRY
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

         positional = _IS_POSITIONAL(src->props3d);
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
               _oalRingBuffer3dProps *ep3d = src->props3d;
               _oalRingBuffer3dProps *mp3d = mixer->props3d;

               if (_FILTER_GET_DATA(ep3d, DISTANCE_FILTER) == NULL)
               {
                  _FILTER_COPY_STATE(ep3d, mp3d, DISTANCE_FILTER);
                  _FILTER_COPY_DATA(ep3d, mp3d, DISTANCE_FILTER);
               }
               _aaxEMitterResetDistDelay(src, mixer);

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

AAX_API int AAX_APIENTRY
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

         if (_IS_POSITIONAL(src->props3d))
         {
            he = mixer->emitters_3d;
            _PROP_DISTQUEUE_CLEAR_DEFINED(src->props3d);
         } else {
            he = mixer->emitters_2d;
         }

			/* get_emitter already locks the emitter */
         _intBufRemove(he, _AAX_EMITTER, emitter->pos, AAX_TRUE);
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

AAX_API int AAX_APIENTRY
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
               _oalRingBuffer3dProps *mp3d, *fp3d;
               _aaxAudioFrame *submix = sframe->submix;
               const _aaxDriverBackend *be = NULL;
               unsigned int be_pos;

               be = _aaxGetDriverBackendLoopback(&be_pos);
               if (be)
               {
                  mp3d = mixer->props3d;
                  fp3d = submix->props3d;

                  if (_FILTER_GET_DATA(fp3d, DISTANCE_FILTER) == NULL)
                  {
                     _FILTER_COPY_STATE(fp3d, mp3d, DISTANCE_FILTER);
                     _FILTER_COPY_DATA(fp3d, mp3d, DISTANCE_FILTER);
                  }
                  _aaxAudioFrameResetDistDelay(submix, mixer);

                  if (_EFFECT_GET_DATA(fp3d, VELOCITY_EFFECT) == NULL)
                  {
                     _EFFECT_COPY(fp3d,mp3d,VELOCITY_EFFECT,AAX_SOUND_VELOCITY);
                     _EFFECT_COPY(fp3d,mp3d,VELOCITY_EFFECT,AAX_DOPPLER_FACTOR);
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
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   put_frame(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
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

			/* get_frame already locks the frame */
         _intBufRemove(hf, _AAX_FRAME, frame->pos, AAX_TRUE);
         frame->submix->refcount--;
         frame->handle = NULL;
         frame->pos = UINT_MAX;

         handle->submix->no_registered--;
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
   put_frame(handle);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameSetState(aaxFrame frame, enum aaxState state)
{
   _frame_t* handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      switch (state)
      {
      case AAX_UPDATE:
         rv = _aaxAudioFrameUpdate(handle);
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

AAX_API int AAX_APIENTRY
aaxAudioFrameWaitForBuffer(const aaxFrame frame, float timeout)
{
   _frame_t* handle = get_frame(frame);
   int rv = AAX_FALSE;
   if (handle)
   {
      float duration = 0.0f, refrate = handle->submix->info->refresh_rate;
      unsigned int sleep_ms;
      _aaxAudioFrame* mixer;
      int nbuf;

      mixer = handle->submix;
      put_frame(frame);

     sleep_ms = (unsigned int)_MAX(100.0f/refrate, 1.0f);
     do
     {
         nbuf = 0;
         duration += sleep_ms*0.001f;
         if (duration >= timeout) break;

         nbuf = _intBufGetNumNoLock(mixer->ringbuffers, _AAX_RINGBUFFER);
         if (!nbuf)
         {
            int err = msecSleep(sleep_ms);
            if ((err < 0) || !handle) break;
         }
      }
      while (!nbuf);

      if (nbuf) rv = AAX_TRUE;
      else _aaxErrorSet(AAX_TIMEOUT);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API aaxBuffer AAX_APIENTRY
aaxAudioFrameGetBuffer(const aaxFrame frame)
{
   _frame_t* handle = get_frame(frame);
   aaxBuffer buffer = NULL;
   if (handle)
   {
      _aaxAudioFrame* mixer = handle->submix;
      _intBuffers *ringbuffers = mixer->ringbuffers;
      _intBufferData *rbuf;

      rbuf = _intBufPop(ringbuffers, _AAX_RINGBUFFER);
      if (rbuf)
      {
         _oalRingBuffer *rb = _intBufGetDataPtr(rbuf);
         _buffer_t *buf = calloc(1, sizeof(_buffer_t));
         if (buf)
         {
            buf->ringbuffer = rb;
            buf->format = _oalRingBufferGetParami(rb, RB_FORMAT);
            buf->ref_counter = 1;
            buf->mipmap = AAX_FALSE;
            buf->id = BUFFER_ID;
            buffer = (aaxBuffer)buf;
         }
         else {
            _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         }
         _intBufDestroyDataNoLock(rbuf);
      }
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
         {			/* frame is registered at the final mixer */
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
      {				/* subframe is registered at another frame */
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

_handle_t *
get_driver_handle(aaxFrame f)
{
   _frame_t* frame = (_frame_t*)f;
   _handle_t* rv = NULL;

   if (frame)
   {
      if (frame->id == HANDLE_ID) {
         rv = (_handle_t*)frame;
      } else if (frame->id == AUDIOFRAME_ID) {
         rv = frame->submix->info->backend;
      }
   }
   return rv;
}

void
_aaxAudioFrameResetDistDelay(_aaxAudioFrame *frame, _aaxAudioFrame *mixer)
{
   int dist_delaying;

   assert(frame);
   assert(mixer);

   dist_delaying = _FILTER_GET_STATE(frame->props3d, DISTANCE_FILTER);
   dist_delaying |= _FILTER_GET_STATE(mixer->props3d, DISTANCE_FILTER);
   dist_delaying &= AAX_DISTANCE_DELAY;
   if (dist_delaying)
   {
      _oalRingBuffer3dProps *pp3d = mixer->props3d;
      _oalRingBufferDelayed3dProps *pdp3d_m = pp3d->m_dprops3d;
      _oalRingBuffer3dProps *fp3d = frame->props3d;
      _oalRingBufferDelayed3dProps *fdp3d_m = fp3d->m_dprops3d;
      _oalRingBufferDelayed3dProps *fdp3d = fp3d->dprops3d;
      _oalRingBuffer2dProps *fp2d = frame->props2d;
      float dist, vs;

      vs = _EFFECT_GET(pp3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);

      /**
       * Align the modified frame matrix with the sensor by multiplying 
       * the frame matrix by the modified parent matrix.
       */
      mtx4Mul(fdp3d_m->matrix, pdp3d_m->matrix, fdp3d->matrix);
      dist = vec3Magnitude(fdp3d_m->matrix[LOCATION]);
      fp2d->dist_delay_sec = dist / vs;

#if 0
 printf("# frame parent:\t\t\t\tfrmae:\n");
 PRINT_MATRICES(pdp3d_m->matrix, fdp3d->matrix);
 printf("# modified frame\n");
 PRINT_MATRIX(fdp3d_m->matrix);
 printf("delay: %f, dist: %f, vs: %f\n", fp2d->dist_delay_sec, dist, vs);
#endif

      _PROP_DISTQUEUE_SET_DEFINED(frame->props3d);
      fp3d->buf3dq_step = 1.0f;

      if (!frame->p3dq) {
         _intBufCreate(&frame->p3dq, _AAX_DELAYED3D);
      } else {
         _intBufClear(frame->p3dq, _AAX_DELAYED3D, _aax_aligned_free);
      }
   }
}


static int
_aaxAudioFrameStart(_frame_t *frame)
{
   int rv = AAX_FALSE;

   assert(frame);

   if (_IS_INITIAL(frame) || _IS_PROCESSED(frame))
   {
      if  (frame->submix->thread)	/* threaded frame */
      {	
         unsigned int ms;
         int r;

         frame->thread.ptr = _aaxThreadCreate();
         assert(frame->thread.ptr != 0);

         frame->thread.condition = _aaxConditionCreate();
         assert(frame->thread.condition != 0);

         frame->thread.mutex = _aaxMutexCreate(0);
         assert(frame->thread.mutex != 0);

         frame->thread.started = AAX_TRUE;
         ms = rintf(1000/frame->submix->info->refresh_rate);
         r =_aaxThreadStart(frame->thread.ptr, _aaxAudioFrameThread, frame, ms);
         if (r == 0)
         {
            int p = 0;
            do
            {
               msecSleep(100);
               r = (frame->ringbuffer != 0);
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
               _aaxAudioFrame* mixer = frame->submix;
               mixer->frame_ready = _aaxConditionCreate();
               if (mixer->frame_ready)
               {
                  _handle_t *phandle = frame->handle;
                  if (phandle->id == HANDLE_ID)
                  {
                     _intBufferData *dptr;
                     dptr = _intBufGet(phandle->sensors, _AAX_SENSOR, 0);
                     if (dptr)
                     {
                        _sensor_t* sensor = _intBufGetDataPtr(dptr);
                        _aaxAudioFrame *pmixer = sensor->mixer;

                        _aaxAudioFrameResetDistDelay(mixer, pmixer);
                        _intBufReleaseData(dptr, _AAX_SENSOR);
                     }
                  }
                  else if (phandle->id == AUDIOFRAME_ID)
                  {
                     _aaxAudioFrame *pmixer = ((_frame_t*)phandle)->submix;
                     _aaxAudioFrameResetDistDelay(mixer, pmixer);
                  }

                  mixer->capturing = AAX_TRUE;
                  rv = AAX_TRUE;
               }
               else {
                  _aaxAudioFrameStop(frame);
               }
            }
         }   
         else {
            _aaxErrorSet(AAX_INVALID_STATE);
         }
      }
      else				/* unthreaded frame */
      {
         frame->submix->capturing = AAX_TRUE;
         rv = AAX_TRUE;
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
      _aaxAudioFrame* mixer = frame->submix;

      frame->thread.started = AAX_FALSE;
      _aaxConditionSignal(frame->thread.condition);
      _aaxThreadJoin(frame->thread.ptr);

      _aaxConditionDestroy(frame->thread.condition);
      _aaxMutexDestroy(frame->thread.mutex);
      _aaxThreadDestroy(frame->thread.ptr);

      _aaxConditionDestroy(mixer->frame_ready);
      rv = AAX_TRUE;
   }
   else if (!frame->submix->thread) {
      rv = AAX_TRUE;
   }
   return rv;
}

static int
_aaxAudioFrameUpdate(_frame_t *frame)
{
   int rv = AAX_FALSE;
   if TEST_FOR_TRUE(frame->thread.started)
   {
      _aaxConditionSignal(frame->thread.condition);
      rv = AAX_TRUE;
   }
   return rv;
}

