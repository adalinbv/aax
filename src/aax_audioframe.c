/*
 * Copyright 2011-2017 by Erik Hofman.
 * Copyright 2011-2017 by Adalin B.V.
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
#include <xml.h>

#include <base/threads.h>
#include <base/timer.h>		/* for msecSleep */
#include <dsp/filters.h>
#include <dsp/effects.h>

#include "ringbuffer.h"
#include "arch.h"
#include "devices.h"
#include "api.h"

static int _aaxAudioFrameStart(_frame_t*);
static int _aaxAudioFrameUpdate(_frame_t*);
static int _frameCreateEFFromAAXS(aaxFrame, const char*);

AAX_API aaxFrame AAX_APIENTRY
aaxAudioFrameCreate(aaxConfig config)
{
   _handle_t *handle = get_handle(config, __func__);
   aaxFrame rv = NULL;

   if (handle && VALID_HANDLE(handle))
   {
      unsigned long size;
      char* ptr2;
      void* ptr1;

      size = sizeof(_frame_t) + sizeof(_aaxAudioFrame);
      ptr2 = (char*)size;
      size += sizeof(_aax2dProps);
      ptr1 = _aax_calloc(&ptr2, 1, size);
      if (ptr1)
      {
         _frame_t* frame = (_frame_t*)ptr1;
         _aaxAudioFrame* submix;
         unsigned int res;

         frame->id = AUDIOFRAME_ID;
         frame->cache_pos = UINT_MAX;
         frame->mixer_pos = UINT_MAX;
         _SET_INITIAL(frame);

         size = sizeof(_frame_t);
         submix = (_aaxAudioFrame*)((char*)frame + size);
         frame->submix = submix;

         submix->props2d = (_aax2dProps*)ptr2;
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
               _aaxAudioFrame* smixer = sensor->mixer;
               _FILTER_COPYD3D_DATA(submix, smixer, DISTANCE_FILTER);
               _EFFECT_COPYD3D(submix, smixer, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);
               _EFFECT_COPYD3D(submix, smixer, VELOCITY_EFFECT, AAX_DOPPLER_FACTOR);
               _EFFECT_COPYD3D_DATA(submix, smixer, VELOCITY_EFFECT);

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
            res = _intBufCreate(&submix->play_ringbuffers, _AAX_RINGBUFFER);
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
   _frame_t* handle;
   int rv = __release_mode;

   aaxAudioFrameSetState(frame, AAX_STOPPED);

   handle = get_frame(frame, __func__);
   if (!rv && handle)
   {
      if (handle->handle) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxRingBufferDelayEffectData* effect;
      _aaxAudioFrame* fmixer = handle->submix;

      free(_FILTER_GET2D_DATA(fmixer, FREQUENCY_FILTER));
      free(_FILTER_GET2D_DATA(fmixer, DYNAMIC_GAIN_FILTER));
      free(_FILTER_GET2D_DATA(fmixer, TIMED_GAIN_FILTER));
      free(_EFFECT_GET2D_DATA(fmixer, DYNAMIC_PITCH_EFFECT));

      effect = _EFFECT_GET2D_DATA(fmixer, DELAY_EFFECT);
      if (effect) free(effect->history_ptr);
      free(effect);

      _intBufErase(&fmixer->p3dq, _AAX_DELAYED3D, _aax_aligned_free);
      _aax_aligned_free(fmixer->props3d->dprops3d);
      free(fmixer->props3d);

      /* handle->ringbuffer gets removed bij the frame thread */
      /* be->destroy_ringbuffer(handle->ringbuffer); */
      _intBufErase(&fmixer->frames, _AAX_FRAME, _aaxAudioFrameFree);
      _intBufErase(&fmixer->devices, _AAX_DEVICE, _aaxDriverFree);
      _intBufErase(&fmixer->emitters_2d, _AAX_EMITTER, free);
      _intBufErase(&fmixer->emitters_3d, _AAX_EMITTER, free);
      _intBufErase(&fmixer->play_ringbuffers, _AAX_RINGBUFFER,
                   _aaxRingBufferFree);
      _intBufErase(&fmixer->frame_ringbuffers, _AAX_RINGBUFFER,
                   _aaxRingBufferFree);

      /* frees both EQUALIZER_LF and EQUALIZER_HF */
      if (handle->filter) {
         free(handle->filter[EQUALIZER_LF].data);
      }

      /* safeguard against using already destroyed handles */
      handle->id = FADEDBAD;
      free(handle);
      handle = 0;
   }
   else {
      put_frame(frame);
   }

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameSetMatrix(aaxFrame frame, const aaxMtx4f mtx)
{
   _frame_t *handle = get_frame(frame, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx || detect_nan_mtx4((const float(*)[4])mtx)) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxAudioFrame* fmixer = handle->submix;
      _handle_t *parent = handle->handle;

      if (parent && parent->id == HANDLE_ID)
      {
         const _intBufferData* dptr;
         dptr = _intBufGet(parent->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            mtx4fCopy(&fmixer->props3d->m_dprops3d->matrix,
                      &sensor->mixer->props3d->m_dprops3d->matrix);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
      else if (parent && parent->id == AUDIOFRAME_ID)
      {
         _frame_t *parent = (_frame_t*)handle->handle;
         mtx4fCopy(&fmixer->props3d->m_dprops3d->matrix,
                   &parent->submix->props3d->m_dprops3d->matrix);
      }

      mtx4fFill(fmixer->props3d->dprops3d->matrix.m4, mtx);
      if (_IS_RELATIVE(handle))
      {
         fmixer->props3d->dprops3d->matrix.m4[LOCATION][3] = 0.0f;
         fmixer->props3d->dprops3d->velocity.m4[VELOCITY][3] = 0.0f;
      }
      else
      {
         fmixer->props3d->dprops3d->matrix.m4[LOCATION][3] = 1.0f;
         fmixer->props3d->dprops3d->velocity.m4[VELOCITY][3] = 1.0f;
      }
      _PROP_MTX_SET_CHANGED(fmixer->props3d);
   }
   put_frame(frame);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameGetMatrix(aaxFrame frame, aaxMtx4f mtx)
{
   _frame_t *handle = get_frame(frame, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv) {
      mtx4fFill(mtx, handle->submix->props3d->dprops3d->matrix.m4);
   }
   put_frame(frame);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameSetVelocity(aaxFrame frame, const aaxVec3f velocity)
{
   _frame_t *handle = get_frame(frame, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!velocity || detect_nan_vec3(velocity)) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxDelayed3dProps *dp3d;

      dp3d = handle->submix->props3d->dprops3d;
      vec3fFill(dp3d->velocity.m4[VELOCITY], velocity);
      _PROP_SPEED_SET_CHANGED(handle->submix->props3d);
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   put_frame(frame);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameGetVelocity(aaxFrame frame, aaxVec3f velocity)
{
   _frame_t *handle = get_frame(frame, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!velocity) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxDelayed3dProps *dp3d;

      dp3d = handle->submix->props3d->dprops3d;
      vec3fFill(velocity, dp3d->velocity.m4[VELOCITY]);
   }
   put_frame(frame);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameSetSetup(UNUSED(aaxFrame frame), UNUSED(enum aaxSetupType type), UNUSED(unsigned int setup))
{
   int rv = AAX_FALSE;
   return rv;
}

AAX_API unsigned int AAX_APIENTRY
aaxAudioFrameGetSetup(const aaxFrame frame, enum aaxSetupType type)
{
   _frame_t *handle = get_frame(frame, __func__);
   unsigned int track = type & 0x3F;
   unsigned int rv = __release_mode;

   if (!rv)
   {
      if (track >= _AAX_MAX_SPEAKERS) {
         _aaxErrorSet(AAX_INVALID_ENUM);
      } else {
         rv = AAX_TRUE; 
      }
   }
   
   if (rv)
   {
      if (type & AAX_COMPRESSION_VALUE)
      {
         _aaxAudioFrame* fmixer = handle->submix;
         _aaxRingBufferLFOData *lfo;

         lfo = _FILTER_GET2D_DATA(fmixer, DYNAMIC_GAIN_FILTER);
         if (lfo) {
            rv = 256*32768*lfo->compression[track];
         }
      }
      else if (type & AAX_GATE_ENABLED)
      {
         _aaxAudioFrame* fmixer = handle->submix;
         _aaxRingBufferLFOData *lfo;

         lfo = _FILTER_GET2D_DATA(fmixer, DYNAMIC_GAIN_FILTER);
         if (lfo && (lfo->average[track] <= lfo->gate_threshold)) {
            rv = AAX_TRUE;
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   put_frame(frame);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameSetFilter(aaxFrame frame, aaxFilter f)
{
   _frame_t *handle = get_frame(frame, __func__);
   _filter_t* filter = get_filter(f);
   int rv = __release_mode;

   if (!rv) 
   {
      if (!filter) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      int type = filter->pos;
      switch (filter->type)
      {
//    case AAX_GRAPHIC_EQUALIZER:
      case AAX_EQUALIZER:
      {
         if (!handle->filter) {		/* EQUALIZER_LF & EQUALIZER_HF */
            handle->filter = calloc(2, sizeof(_aaxFilterInfo));
         }

         if (handle->filter)
         {
            type = EQUALIZER_HF;
            _FILTER_SET(handle, type, 0, _FILTER_GET_SLOT(filter, 1, 0));
            _FILTER_SET(handle, type, 1, _FILTER_GET_SLOT(filter, 1, 1));
            _FILTER_SET(handle, type, 2, _FILTER_GET_SLOT(filter, 1, 2));
            _FILTER_SET(handle, type, 3, _FILTER_GET_SLOT(filter, 1, 3));
            _FILTER_SWAP_SLOT_DATA(handle, EQUALIZER_HF, filter, 1);

            type = EQUALIZER_LF;
            _FILTER_SET(handle, type, 0, _FILTER_GET_SLOT(filter, 0, 0));
            _FILTER_SET(handle, type, 1, _FILTER_GET_SLOT(filter, 0, 1));
            _FILTER_SET(handle, type, 2, _FILTER_GET_SLOT(filter, 0, 2));
            _FILTER_SET(handle, type, 3, _FILTER_GET_SLOT(filter, 0, 3));
            _FILTER_SET_STATE(handle, type, _FILTER_GET_SLOT_STATE(filter));
            _FILTER_SWAP_SLOT_DATA(handle, type, filter, 0);
            break;
         }
         else {
            _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         }
      }
      case AAX_FREQUENCY_FILTER:
      case AAX_DYNAMIC_GAIN_FILTER:
      case AAX_VOLUME_FILTER:
      case AAX_TIMED_GAIN_FILTER:
      case AAX_COMPRESSOR:
      {
         _aax2dProps *p2d = handle->submix->props2d;
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
         break;
      }
      case AAX_DISTANCE_FILTER:
      case AAX_ANGULAR_FILTER:
      {
         _aax3dProps *p3d = handle->submix->props3d;
         _FILTER_SET(p3d, type, 0, _FILTER_GET_SLOT(filter, 0, 0));
         _FILTER_SET(p3d, type, 1, _FILTER_GET_SLOT(filter, 0, 1));
         _FILTER_SET(p3d, type, 2, _FILTER_GET_SLOT(filter, 0, 2));
         _FILTER_SET(p3d, type, 3, _FILTER_GET_SLOT(filter, 0, 3));
         _FILTER_SET_STATE(p3d, type, _FILTER_GET_SLOT_STATE(filter));
         _FILTER_SWAP_SLOT_DATA(p3d, type, filter, 0);
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
         rv = AAX_FALSE;
      }
   }
   put_frame(frame);

   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxAudioFrameGetFilter(aaxFrame frame, enum aaxFilterType type)
{
   _frame_t *handle = get_frame(frame, __func__);
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
         rv = new_filter_handle(frame, type, submix->props2d, submix->props3d);
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   put_frame(frame);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameSetEffect(aaxFrame frame, aaxEffect e)
{
   _frame_t *handle = get_frame(frame, __func__);
   _effect_t* effect = get_effect(e);
   int rv = __release_mode;

   if (!rv)
   {
      if (!effect) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxAudioFrame* fmixer = handle->submix;
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
         _aax2dProps *p2d = fmixer->props2d;
         _EFFECT_SET(p2d, type, 0, _EFFECT_GET_SLOT(effect, 0, 0));
         _EFFECT_SET(p2d, type, 1, _EFFECT_GET_SLOT(effect, 0, 1));
         _EFFECT_SET(p2d, type, 2, _EFFECT_GET_SLOT(effect, 0, 2));
         _EFFECT_SET(p2d, type, 3, _EFFECT_GET_SLOT(effect, 0, 3));
         _EFFECT_SET_STATE(p2d, type, _EFFECT_GET_SLOT_STATE(effect));
         _EFFECT_SWAP_SLOT_DATA(p2d, type, effect, 0);
         if ((enum aaxEffectType)effect->type == AAX_DYNAMIC_PITCH_EFFECT) {
            p2d->final.pitch_lfo = 1.0f;
         }
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
         rv = AAX_FALSE;
      }
   }
   put_frame(handle);

   return rv;
}

AAX_API aaxEffect AAX_APIENTRY
aaxAudioFrameGetEffect(aaxFrame frame, enum aaxEffectType type)
{
   _frame_t *handle = get_frame(frame, __func__);
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
         _aaxAudioFrame* fmixer = handle->submix;
         rv = new_effect_handle(frame, type, fmixer->props2d, fmixer->props3d);
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
      put_frame(handle);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameSetMode(aaxFrame frame, enum aaxModeType type, int mode)
{
   _frame_t *handle = get_frame(frame, __func__);
   int rv =AAX_TRUE;

   if (rv)
   {
      _aaxAudioFrame* fmixer = handle->submix;
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
               fmixer->props3d->dprops3d->matrix.m4[LOCATION][3] = 0.0f;
            } else {
               fmixer->props3d->dprops3d->matrix.m4[LOCATION][3] = 1.0f;
            }
         }
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
         rv = AAX_FALSE;
      }
   }
   put_frame(handle);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameGetMode(UNUSED(const aaxFrame frame), UNUSED(enum aaxModeType type))
{
   int rv = AAX_FALSE;
   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameRegisterSensor(const aaxFrame frame, const aaxConfig sensor)
{
   _handle_t* ssr_config = get_read_handle(sensor, __func__);
   _frame_t* handle = get_frame(frame, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!ssr_config) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (ssr_config->thread.started) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else if (ssr_config->mixer_pos < UINT_MAX) {
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxAudioFrame* fmixer = handle->submix;
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
         _aaxErrorSet(AAX_ERROR_NONE);
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
            _aax3dProps *mp3d, *sp3d;
            _aaxAudioFrame *smixer;
            _aaxRingBuffer *rb;

            smixer = sensor->mixer;

            mp3d = fmixer->props3d;
            sp3d = smixer->props3d;

            smixer->info->frequency = fmixer->info->frequency;
            while (smixer->info->frequency > 48000.0f) {
               smixer->info->frequency /= 2.0f;
            }
            smixer->info->period_rate = fmixer->info->period_rate;
            smixer->info->refresh_rate = fmixer->info->refresh_rate;
            smixer->info->update_rate = fmixer->info->update_rate;
            if (_FILTER_GET_DATA(sp3d, DISTANCE_FILTER) == NULL)
            {
               _FILTER_COPY_STATE(sp3d, mp3d, DISTANCE_FILTER);
               _FILTER_COPY_DATA(sp3d, mp3d, DISTANCE_FILTER);
            }
            _aaxAudioFrameResetDistDelay(smixer, fmixer);

            if (_EFFECT_GET_DATA(sp3d, VELOCITY_EFFECT) == NULL)
            {
               _EFFECT_COPY(sp3d,mp3d,VELOCITY_EFFECT,AAX_SOUND_VELOCITY);
               _EFFECT_COPY(sp3d,mp3d,VELOCITY_EFFECT,AAX_DOPPLER_FACTOR);
               _EFFECT_COPY_DATA(sp3d, mp3d, VELOCITY_EFFECT);
            }

            ssr_config->handle = handle;
            ssr_config->mixer_pos = pos;
            smixer->refcount++;

            if (!smixer->ringbuffer)
            {
               _handle_t *driver = get_driver_handle(frame);
               const _aaxDriverBackend *be = driver->backend.ptr;
               enum aaxRenderMode mode = driver->info->mode;
               float dt = DELAY_EFFECTS_TIME;

               smixer->ringbuffer = be->get_ringbuffer(dt, mode);
            }

            rb = smixer->ringbuffer;
            if (rb)
            {
               _aaxMixerInfo* info = smixer->info;
               float delay_sec = 1.0f / info->period_rate;

               rb->set_format(rb, AAX_PCM24S, AAX_TRUE);
               rb->set_paramf(rb, RB_FREQUENCY, info->frequency);
               rb->set_parami(rb, RB_NO_TRACKS, 2);

               /* create a ringbuffer with a but of overrun space */
               rb->set_paramf(rb, RB_DURATION_SEC, delay_sec*1.0f);
               rb->init(rb, AAX_TRUE);

               /* 
                * Now set the actual duration, this will not alter the
                * allocated space since it is lower that the initial
                * duration.
                */
               rb->set_paramf(rb, RB_DURATION_SEC, delay_sec);
               rb->set_state(rb, RB_STARTED);
            }

            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
      else
      {
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         rv = AAX_FALSE;
      }
   }
   put_frame(handle);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameDeregisterSensor(const aaxFrame frame, const aaxConfig sensor)
{
   _handle_t* ssr_config = get_handle(sensor, __func__);
   _frame_t* handle = get_frame(frame, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!ssr_config || ssr_config->mixer_pos == UINT_MAX) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _intBuffers *hd = handle->submix->devices;
      _handle_t* ptr;

      ptr = _intBufRemove(hd, _AAX_DEVICE, ssr_config->mixer_pos, AAX_FALSE);
      if (ptr)
      {
         _intBufferData *dptr;

         handle->submix->no_registered--;
         dptr = _intBufGet(ssr_config->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            sensor->mixer->refcount--;
            ssr_config->mixer_pos = UINT_MAX;
            ssr_config->handle = NULL;

            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
   }
   put_frame(handle);

   return rv;
}


AAX_API int AAX_APIENTRY
aaxAudioFrameRegisterEmitter(const aaxFrame frame, const aaxEmitter em)
{
   _emitter_t* emitter = get_emitter_unregistered(em, __func__);
   _frame_t* handle = get_frame(frame, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!emitter || emitter->handle || emitter->mixer_pos < UINT_MAX) {
          _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxAudioFrame* fmixer = handle->submix;
      _aaxEmitter *src = emitter->source;
      unsigned int pos = UINT_MAX;
      unsigned int positional;
      _intBuffers *he;

      positional = _IS_POSITIONAL(src->props3d);
      if (!positional) {
         he = fmixer->emitters_2d;
      } else {
         he = fmixer->emitters_3d;
      }

      if (fmixer->no_registered < fmixer->info->max_registered)
      {
         if (_aaxGetEmitter())
         {
            pos = _intBufAddData(he, _AAX_EMITTER, emitter);
            fmixer->no_registered++;
         }
      }

      if (pos != UINT_MAX)
      {
         _aaxEmitter *src = emitter->source;
         emitter->handle = handle;
         emitter->mixer_pos = pos;

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

         src->info = fmixer->info;
         if (src->update_rate == 0) {
            src->update_rate = fmixer->info->update_rate;
         }
         src->update_ctr = src->update_rate;

         if (positional)
         {
            _aax3dProps *ep3d = src->props3d;
            _aax3dProps *mp3d = fmixer->props3d;

            if (_FILTER_GET_DATA(ep3d, DISTANCE_FILTER) == NULL)
            {
               _FILTER_COPY_STATE(ep3d, mp3d, DISTANCE_FILTER);
               _FILTER_COPY_DATA(ep3d, mp3d, DISTANCE_FILTER);
            }
            _aaxEMitterResetDistDelay(src, fmixer);

            if (_EFFECT_GET_DATA(ep3d, VELOCITY_EFFECT) == NULL)
            {
               _EFFECT_COPY(ep3d, mp3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);
               _EFFECT_COPY(ep3d, mp3d, VELOCITY_EFFECT, AAX_DOPPLER_FACTOR);
               _EFFECT_COPY_DATA(ep3d, mp3d, VELOCITY_EFFECT);
            }
         }
      }
      else
      {
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         rv = AAX_FALSE;
      }
   }
   put_frame(handle);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameDeregisterEmitter(const aaxFrame frame, const aaxEmitter em)
{
   _emitter_t* emitter = get_emitter(em, __func__);
   _frame_t* handle = get_frame(frame, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!emitter || emitter->mixer_pos == UINT_MAX) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxAudioFrame* fmixer = handle->submix;
      _aaxEmitter *src = emitter->source;
      _intBuffers *he;

      if (_IS_POSITIONAL(src->props3d))
      {
         he = fmixer->emitters_3d;
         _PROP_DISTQUEUE_CLEAR_DEFINED(src->props3d);
      } else {
         he = fmixer->emitters_2d;
      }

      /* Unlock the frame again to make sure locking is done in the  */
      /* proper order by _intBufRemove                               */
      _intBufRelease(he, _AAX_EMITTER, emitter->mixer_pos);
      _intBufRemove(he, _AAX_EMITTER, emitter->mixer_pos, AAX_FALSE);
      _aaxPutEmitter();
      fmixer->no_registered--;
      emitter->mixer_pos = UINT_MAX;
      emitter->handle = NULL;
   }
   put_frame(frame);
   put_emitter(emitter);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameRegisterAudioFrame(const aaxFrame frame, const aaxFrame subframe)
{
   _frame_t* sframe = get_frame(subframe, __func__);
   _frame_t* handle = get_frame(frame, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!sframe) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (sframe->handle) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else if (sframe->mixer_pos < UINT_MAX) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxAudioFrame *fmixer =  handle->submix;
      _intBuffers *hf = fmixer->frames;
      unsigned int pos = UINT_MAX;

      if (hf == NULL)
      {
         unsigned int res;

         res = _intBufCreate(&fmixer->frames, _AAX_FRAME);
         if (res != UINT_MAX) {
            hf = fmixer->frames;
         }
      }

      if (hf && (fmixer->no_registered < fmixer->info->max_registered))
      {
         aaxBuffer buf; /* clear the frames buffer queue */
         while ((buf = aaxAudioFrameGetBuffer(sframe)) != NULL) {
            aaxBufferDestroy(buf);
         }
         _aaxErrorSet(AAX_ERROR_NONE);
         pos = _intBufAddData(hf, _AAX_FRAME, sframe);
         fmixer->no_registered++;
      }
      else {
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      }

      if (pos != UINT_MAX)
      {
         _aax3dProps *mp3d, *fp3d;
         _aaxAudioFrame *submix = sframe->submix;
         const _aaxDriverBackend *be = NULL;
         unsigned int be_pos;

         be = _aaxGetDriverBackendLoopback(&be_pos);
         if (be)
         {
            mp3d = fmixer->props3d;
            fp3d = submix->props3d;

            if (_FILTER_GET_DATA(fp3d, DISTANCE_FILTER) == NULL)
            {
               _FILTER_COPY_STATE(fp3d, mp3d, DISTANCE_FILTER);
               _FILTER_COPY_DATA(fp3d, mp3d, DISTANCE_FILTER);
            }
            _aaxAudioFrameResetDistDelay(submix, fmixer);

            if (_EFFECT_GET_DATA(fp3d, VELOCITY_EFFECT) == NULL)
            {
               _EFFECT_COPY(fp3d,mp3d,VELOCITY_EFFECT,AAX_SOUND_VELOCITY);
               _EFFECT_COPY(fp3d,mp3d,VELOCITY_EFFECT,AAX_DOPPLER_FACTOR);
               _EFFECT_COPY_DATA(fp3d, mp3d, VELOCITY_EFFECT);
            }

            submix->refcount++;
            sframe->handle = handle;
            sframe->mixer_pos = pos;
         }
         else
         {
            _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            rv = AAX_FALSE;
         }
      }
      else
      {
         if (sframe->handle) put_frame(sframe);
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         rv = AAX_FALSE;
      }
   }
   put_frame(handle);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameDeregisterAudioFrame(const aaxFrame frame, const aaxFrame subframe)
{
   _frame_t* sframe = get_frame(subframe, __func__);
   _frame_t* handle = get_frame(frame, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!sframe || sframe->mixer_pos == UINT_MAX) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _intBuffers *hf = handle->submix->frames;

      /* Unlock the frame again to make sure locking is done in the proper */
      /* order by _intBufRemove                                            */
      _intBufRelease(hf, _AAX_FRAME, sframe->mixer_pos);
      _intBufRemove(hf, _AAX_FRAME, sframe->mixer_pos, AAX_FALSE);
      sframe->submix->refcount--;
      sframe->mixer_pos = UINT_MAX;
      sframe->handle = NULL;

      handle->submix->no_registered--;
   }
   put_frame(handle);
   put_frame(sframe);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameSetState(aaxFrame frame, enum aaxState state)
{
   _frame_t* handle = get_frame(frame, __func__);
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
         handle = get_frame(frame, __func__);
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
   put_frame(frame);
   return rv;
}

AAX_API int AAX_APIENTRY 
aaxAudioFrameGetState(UNUSED(const aaxFrame frame))
{  
   enum aaxState ret = AAX_STATE_NONE;
   return ret;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameWaitForBuffer(const aaxFrame frame, float timeout)
{
   _frame_t* handle = get_frame(frame, __func__);
   int rv = AAX_FALSE;
   if (handle)
   {
      float duration = 0.0f, refrate = handle->submix->info->period_rate;
      unsigned int sleep_ms;
      _aaxAudioFrame* fmixer;
      int nbuf;

      fmixer = handle->submix;
      put_frame(frame);

     sleep_ms = (unsigned int)_MAX(100.0f/refrate, 1.0f);
     do
     {
         nbuf = 0;
         duration += sleep_ms*0.001f;
         if (duration >= timeout) break;

         nbuf = _intBufGetNumNoLock(fmixer->play_ringbuffers, _AAX_RINGBUFFER);
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
   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameAddBuffer(aaxFrame frame, aaxBuffer buf)
{
   _frame_t* handle = get_frame(frame, __func__);
   _buffer_t* buffer = get_buffer(buf, __func__);
   int rv = __release_mode;

   if (!rv && handle)
   {
      if (!buffer) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
      else if (!buffer->aaxs) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else {
         rv = AAX_TRUE;
      }
   }
   put_frame(frame);

   if (rv) {
     rv = _frameCreateEFFromAAXS(buffer, buffer->aaxs);
   }
   return rv;
}

AAX_API aaxBuffer AAX_APIENTRY
aaxAudioFrameGetBuffer(const aaxFrame frame)
{
   _frame_t* handle = get_frame(frame, __func__);
   aaxBuffer buffer = NULL;
   if (handle)
   {
      _aaxAudioFrame* fmixer = handle->submix;
      _intBuffers *ringbuffers = fmixer->play_ringbuffers;
      _intBufferData *rbuf;

      rbuf = _intBufPop(ringbuffers, _AAX_RINGBUFFER);
      if (rbuf)
      {
         _aaxRingBuffer *rb = _intBufGetDataPtr(rbuf);
         _buffer_t *buf = calloc(1, sizeof(_buffer_t));
         if (buf)
         {
            buf->id = BUFFER_ID;
            buf->ref_counter = 1;

            buf->blocksize = 1;
            buf->pos = 0;
            buf->no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
            buf->no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
            buf->format = rb->get_parami(rb, RB_FORMAT);
            buf->frequency = rb->get_paramf(rb, RB_FREQUENCY);

            buf->mipmap = AAX_FALSE;

            buf->info = &_info;
            rb->set_parami(rb, RB_IS_MIXER_BUFFER, AAX_FALSE);
            buf->ringbuffer = rb;

            buffer = (aaxBuffer)buf;
         }
         else {
            _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         }
         _intBufDestroyDataNoLock(rbuf);
      }
      else {
         _aaxErrorSet(AAX_INVALID_REFERENCE);
      }
   }
   put_frame(frame);

   return buffer;
}

/* -------------------------------------------------------------------------- */

void
_aaxAudioFrameFree(void *handle)
{
   aaxAudioFrameDestroy(handle);
}

_frame_t*
get_frame(aaxFrame f, const char *func)
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
            _aaxAudioFrame* smixer = sensor->mixer;
            _intBufferData *dptr_frame;
            _intBuffers *hf;

            hf = smixer->frames;

            dptr_frame = _intBufGet(hf, _AAX_FRAME, frame->mixer_pos);
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
         dptr_frame = _intBufGet(hf, _AAX_FRAME, frame->mixer_pos);
         frame = _intBufGetDataPtr(dptr_frame);
      }
      rv = frame;
   }
   else if (frame && frame->id == FADEDBAD) {
      __aaxErrorSet(AAX_DESTROYED_HANDLE, func);
   }
   else {
       __aaxErrorSet(AAX_INVALID_HANDLE, func);
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
            _aaxAudioFrame* smixer = sensor->mixer;
            _intBuffers *hf;

            hf = smixer->frames;
            _intBufRelease(hf, _AAX_FRAME, frame->mixer_pos);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
      else if (handle && handle->id == AUDIOFRAME_ID)
      {
         _frame_t *handle = (_frame_t*)frame->handle;
         _intBuffers *hf;

         hf =  handle->submix->frames;
         _intBufRelease(hf, _AAX_FRAME, frame->mixer_pos);
      }
   }
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
      _aax3dProps *pp3d = mixer->props3d;
      _aaxDelayed3dProps *pdp3d_m = pp3d->m_dprops3d;
      _aax3dProps *fp3d = frame->props3d;
      _aaxDelayed3dProps *fdp3d_m = fp3d->m_dprops3d;
      _aaxDelayed3dProps *fdp3d = fp3d->dprops3d;
      _aax2dProps *fp2d = frame->props2d;
      float dist, vs;

      vs = _EFFECT_GET(pp3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);

      /**
       * Align the modified frame matrix with the sensor by multiplying 
       * the frame matrix by the modified parent matrix.
       */
      mtx4fMul(&fdp3d_m->matrix, &pdp3d_m->matrix, &fdp3d->matrix);
      dist = vec3fMagnitude((vec3f_t*)&fdp3d_m->matrix.s4x4[LOCATION]);
      fp2d->dist_delay_sec = dist / vs;

#if 0
 printf("# frame parent:\t\t\t\tframe:\n");
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
      frame->submix->capturing = AAX_TRUE;
      rv = AAX_TRUE;
   }
   else if _IS_STANDBY(frame) {
      rv = AAX_TRUE;
   }
   return rv;
}

int
_aaxAudioFrameStop(UNUSED(_frame_t *frame))
{
   int rv = AAX_TRUE;
   return rv;
}

static int
_aaxAudioFrameUpdate(UNUSED(_frame_t *frame))
{
   int rv = AAX_TRUE;
   return rv;
}

static int
_frameCreateEFFromAAXS(aaxFrame frame, const char *aaxs)
{
   _frame_t* handle = get_frame(frame, __func__);
   aaxConfig config = handle->handle;
   int rv = AAX_TRUE;
   void *xid;

   put_frame(handle);

   xid = xmlInitBuffer(aaxs, strlen(aaxs));
   if (xid)
   {
      void *xmid = xmlNodeGet(xid, "aeonwave/audioframe");
      if (xmid)
      {
         int clear = xmlAttributeCompareString(xmid, "mode", "append");
         unsigned int i, num = xmlNodeGetNum(xmid, "filter");
         void *xeid, *xfid = xmlMarkId(xmid);

         if (clear) {
            _aaxSetDefault2dProps(handle->submix->props2d);
         }
         for (i=0; i<num; i++)
         {
            if (xmlNodeGetPos(xmid, xfid, "filter", i) != 0)
            {
               char src[65];
               int slen;

               slen = xmlAttributeCopyString(xfid, "type", src, 64);
               if (slen)
               {
                  enum aaxFilterType ftype;
                  aaxFilter flt;

                  src[slen] = 0;
                  ftype = aaxFilterGetByName(config, src);
                  flt = aaxFilterCreate(config, ftype);
                  if (flt)
                  {
                     enum aaxWaveformType state = AAX_CONSTANT_VALUE;
                     unsigned int s, num = xmlNodeGetNum(xfid, "slot");
                     void *xsid = xmlMarkId(xfid);
                     for (s=0; s<num; s++)
                     {
                        if (xmlNodeGetPos(xfid, xsid, "slot", s) != 0)
                        {
                           enum aaxType type = AAX_LINEAR;
                           aaxVec4f params;
                           long int n;

                           n = xmlAttributeGetInt(xsid, "n");
                           if (n == XML_NONE) n = s;

                           params[0] = xmlNodeGetDouble(xsid, "p1");
                           params[1] = xmlNodeGetDouble(xsid, "p2");
                           params[2] = xmlNodeGetDouble(xsid, "p3");
                           params[3] = xmlNodeGetDouble(xsid, "p4");

                           slen = xmlAttributeCopyString(xsid, "type", src, 64);
                           if (slen)
                           {
                              src[slen] = 0;
                              type = aaxGetTypeByName(src);
                           }
                           aaxFilterSetSlotParams(flt, n, type, params);
                        }
                     }

                     slen = xmlAttributeCopyString(xfid, "type", src, 64);
                     if (slen)
                     {
                        src[slen] = 0;
                        state = aaxGetWaveformTypeByName(src);
                     }
                     aaxFilterSetState(flt, state);

                     aaxAudioFrameSetFilter(frame, flt);
                     aaxFilterDestroy(flt);
                     xmlFree(xsid);
                  }
               }
            }
         }
         xmlFree(xfid);

         xeid = xmlMarkId(xmid);
         num = xmlNodeGetNum(xmid, "effect");
         for (i=0; i<num; i++)
         {
            if (xmlNodeGetPos(xmid, xeid, "effect", i) != 0)
            {
               char src[64];
               int slen;

               slen = xmlAttributeCopyString(xeid, "type", src, 64);
               if (slen)
               {
                  enum aaxEffectType ftype;
                  aaxEffect eff;

                  src[slen] = 0;
                  ftype = aaxEffectGetByName(config, src);
                  eff = aaxEffectCreate(config, ftype);
                  if (eff)
                  {
                     enum aaxWaveformType state = AAX_CONSTANT_VALUE;
                     unsigned int s, num = xmlNodeGetNum(xeid, "slot");
                     void *xsid = xmlMarkId(xeid);
                     for (s=0; s<num; s++)
                     {
                        if (xmlNodeGetPos(xeid, xsid, "slot", s) != 0)
                        {
                           aaxVec4f params;
                           long int n;

                           n = xmlAttributeGetInt(xsid, "n");
                           if (n == XML_NONE) n = s;

                           params[0] = xmlNodeGetDouble(xsid, "p1");
                           params[1] = xmlNodeGetDouble(xsid, "p2");
                           params[2] = xmlNodeGetDouble(xsid, "p3");
                           params[3] = xmlNodeGetDouble(xsid, "p4");
                           aaxEffectSetSlotParams(eff, n, AAX_LINEAR, params);
                        }
                     }

                     slen = xmlAttributeCopyString(xeid, "src", src, 64);
                     if (slen)
                     {
                        src[slen] = 0;
                        state = aaxGetWaveformTypeByName(src);
                     }
                     aaxEffectSetState(eff, state);
                     aaxAudioFrameSetEffect(frame, eff);
                     aaxEffectDestroy(eff);
                     xmlFree(xsid);
                  }
               }
            }
         }
         xmlFree(xeid);
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
