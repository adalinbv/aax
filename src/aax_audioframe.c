/*
 * SPDX-FileCopyrightText: Copyright © 2011-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2011-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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

#include <base/timer.h>		/* for msecSleep */
#include <dsp/filters.h>
#include <dsp/effects.h>
#include <dsp/common.h>
#include <dsp/lfo.h>

#include "ringbuffer.h"
#include "arch.h"
#include "api.h"

#define ENABLE_MULTI_REGISTER	0

static bool _aaxAudioFrameStart(_frame_t*);
static bool _aaxAudioFrameStop(_frame_t*);
static bool _aaxAudioFrameUpdate(_frame_t*);
static bool _frameCreateEFFromAAXS(aaxFrame, _buffer_t*);

AAX_API aaxFrame AAX_APIENTRY
aaxAudioFrameCreate(aaxConfig config)
{
   _handle_t *handle = get_handle(config, __func__);
   aaxFrame rv = NULL;

   if (handle && VALID_HANDLE(handle))
   {
      size_t offs, size;
      char* ptr2;
      void* ptr1;

      offs = sizeof(_frame_t) + sizeof(_aaxAudioFrame);
      size = sizeof(_aax2dProps);
      ptr1 = _aax_calloc(&ptr2, offs, 1, size);
      if (ptr1)
      {
         _frame_t* frame = (_frame_t*)ptr1;
         _aaxAudioFrame* submix;
         unsigned int res;

         frame->id = AUDIOFRAME_ID;
         frame->handle = handle->handle;
         frame->root = handle->root;
         frame->mixer_pos[0] = UINT_MAX;
         frame->max_emitters = UINT_MAX;
         if (!RENDER_NORMAL(handle->info->midi_mode))
         {
            if (handle->info->midi_mode == AAX_RENDER_ARCADE) {
               frame->max_emitters = 3;
            } else {
               frame->max_emitters = 6;
            }
         }

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

//             _FILTER_COPYD3D_DATA(submix, smixer, DISTANCE_FILTER);
               memcpy(submix->props3d->filter[DISTANCE_FILTER].data,
                      smixer->props3d->filter[DISTANCE_FILTER].data,
                      sizeof(_aaxRingBufferDistanceData));

               _EFFECT_COPYD3D(submix, smixer, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);
               _EFFECT_COPYD3D(submix, smixer, VELOCITY_EFFECT, AAX_DOPPLER_FACTOR);
               _EFFECT_COPYD3D(submix, smixer, VELOCITY_EFFECT, AAX_LIGHT_VELOCITY);
//             _EFFECT_COPYD3D_DATA(submix, smixer, VELOCITY_EFFECT);
                memcpy(submix->props3d->effect[VELOCITY_EFFECT].data,
                       smixer->props3d->effect[VELOCITY_EFFECT].data,
                       sizeof(_aaxRingBufferVelocityEffectData));

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

AAX_API bool AAX_APIENTRY
aaxAudioFrameDestroy(aaxFrame frame)
{
   _frame_t* handle;
   bool rv = __release_mode;

   aaxAudioFrameSetState(frame, AAX_STOPPED);

   handle = get_frame(frame, _LOCK, __func__);
   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (handle->parent[0]) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      _aaxAudioFrame* fmixer = handle->submix;
      int i;

      _aaxMutexLock(handle->mutex);
      _FILTER_FREE_DATA(fmixer, EQUALIZER_LF);
      _aaxMutexDestroy(handle->mutex);

      for (i=0; i<MAX_STEREO_FILTER; ++i) {
         _FILTER_FREE2D_DATA(fmixer, i);
      }
      for (i=0; i<MAX_STEREO_EFFECT; ++i) {
         _EFFECT_FREE2D_DATA(fmixer, i);
      }

      _intBufErase(&fmixer->p3dq, _AAX_DELAYED3D, _aax_aligned_free);
      _aax3dPropsDestory(fmixer->props3d);

      /* handle->ringbuffer gets removed by the frame thread */
      /* be->destroy_ringbuffer(handle->ringbuffer); */
      _intBufErase(&fmixer->frames, _AAX_FRAME, _aaxAudioFrameFree);
      _intBufErase(&fmixer->devices, _AAX_DEVICE, _aaxDriverFree);
      _intBufErase(&fmixer->emitters_2d, _AAX_EMITTER, free);
      _intBufErase(&fmixer->emitters_3d, _AAX_EMITTER, free);
      _intBufErase(&fmixer->play_ringbuffers, _AAX_RINGBUFFER,
                   _aaxRingBufferFree);
      _intBufErase(&fmixer->frame_ringbuffers, _AAX_RINGBUFFER,
                   _aaxRingBufferFree);
      if (fmixer->ringbuffer) {
         _aaxRingBufferFree(fmixer->ringbuffer);
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

AAX_API bool AAX_APIENTRY
aaxAudioFrameSetMatrix64(aaxFrame frame, aaxMtx4d mtx64)
{
   _frame_t *handle = get_frame(frame, _LOCK, __func__);
   bool rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!mtx64 || detect_nan_mtx4d(mtx64)) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      _aaxAudioFrame* fmixer = handle->submix;
      _aax3dProps *fp3d = fmixer->props3d;
      _aaxDelayed3dProps *fdp3d = fp3d->dprops3d;

      mtx4dFill(fdp3d->matrix.m4, mtx64);

      if (_IS_RELATIVE(fp3d) &&
          handle->parent[0] && (handle->parent[0] == handle->root))
      {
         fdp3d->matrix.m4[LOCATION][3] = 0.0;
      } else {
         fdp3d->matrix.m4[LOCATION][3] = 1.0;
      }
      _PROP_MTX_SET_CHANGED(fp3d);
      handle->mtx_set = true;

      if (_IS_RELATIVE(fp3d))
      {
         _frame_t *parent = handle->parent[0];

         // Walk back to the lowest parent frame which is registered
         // at the sensor and mark it changed.
         if (parent)
         {
            while (handle->root && (void*)parent != handle->root)
            {
               handle = parent;
               parent = handle->parent[0];
            }
         }
         _PROP_MTX_SET_CHANGED(handle->submix->props3d);
      }
   }
   put_frame(frame);

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxAudioFrameGetMatrix64(aaxFrame frame, aaxMtx4d mtx64)
{
   _frame_t *handle = get_frame(frame, _NOLOCK, __func__);
   bool rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!mtx64) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = true;
      }
   }

   if (rv) {
      mtx4dFill(mtx64, handle->submix->props3d->dprops3d->matrix.m4);
   }

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxAudioFrameSetVelocity(aaxFrame frame, aaxVec3f velocity)
{
   _frame_t *handle = get_frame(frame, _LOCK, __func__);
   bool rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!velocity || detect_nan_vec3(velocity)) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      _aaxAudioFrame* fmixer = handle->submix;
      _aax3dProps *fp3d = fmixer->props3d;
      _aaxDelayed3dProps *fdp3d = fp3d->dprops3d;

      vec3fFill(fdp3d->velocity.m4[VELOCITY], velocity);
      if (_IS_RELATIVE(fp3d) &&
          handle->parent[0] && (handle->parent[0] == handle->root))
      {
         fdp3d->velocity.m4[LOCATION][3] = 0.0f;
      } else {
         fdp3d->velocity.m4[LOCATION][3] = 1.0f;
      }
      _PROP_SPEED_SET_CHANGED(fp3d);

      if (_IS_RELATIVE(fp3d))
      {
         _frame_t *parent = handle->parent[0];

         // Walk back to the lowest parent frame which is registered
         // at the sensor and mark it changed.
         if (parent)
         {
            while ((void*)parent != handle->root)
            {
               handle = parent;
               parent = handle->parent[0];

            }
         }
         _PROP_SPEED_SET_CHANGED(handle->submix->props3d);
      }
   }
   put_frame(frame);

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxAudioFrameGetVelocity(aaxFrame frame, aaxVec3f velocity)
{
   _frame_t *handle = get_frame(frame, _NOLOCK, __func__);
   bool rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!velocity) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      _aaxDelayed3dProps *dp3d;

      dp3d = handle->submix->props3d->dprops3d;
      vec3fFill(velocity, dp3d->velocity.m4[VELOCITY]);
   }

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxAudioFrameSetSetup(aaxFrame frame, enum aaxSetupType type, int64_t setup)
{
   bool rv = false;

   if (frame == NULL)
   {
      switch(type)
      {
      case AAX_STEREO_EMITTERS:
         setup *= 2;
         // intentional fallthrough
      case AAX_MONO_EMITTERS:
         rv = (setup <= _aaxGetNoEmitters(NULL)) ? true : false;
         break;
      default:
         break;
      }
   }
   else
   {
      _frame_t *handle = get_frame(frame, _NOLOCK, __func__);
      if (handle)
      {
         switch(type)
         {
         case AAX_STEREO_EMITTERS:
            setup *= 2;
            // intentional fallthrough
         case AAX_MONO_EMITTERS:
            handle->max_emitters = setup;
            rv = true;
            break;
         default:
            break;
         }
      }
   }
   return rv;
}

AAX_API int64_t AAX_APIENTRY
aaxAudioFrameGetSetup(const aaxFrame frame, enum aaxSetupType type)
{
   _frame_t *handle = get_frame(frame, _NOLOCK, __func__);
   int track = type & 0x3F;
   int64_t rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (track >= _AAX_MAX_SPEAKERS) {
         _aaxErrorSet(AAX_INVALID_ENUM);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      switch(type)
      {
      case AAX_MONO_EMITTERS:
         rv = handle->max_emitters;
         break;
      case AAX_STEREO_EMITTERS:
         rv = handle->max_emitters/2;
         break;
      default:
         if (type & AAX_COMPRESSION_VALUE)
         {
            _aaxAudioFrame* fmixer = handle->submix;
            _aaxLFOData *lfo;

            lfo = _FILTER_GET2D_DATA(fmixer, DYNAMIC_GAIN_FILTER);
            if (lfo) {
               rv = AAX_TO_INT(lfo->compression[track]);
            }
         }
         else if (type & AAX_GATE_ENABLED)
         {
            _aaxAudioFrame* fmixer = handle->submix;
            _aaxLFOData *lfo;

            lfo = _FILTER_GET2D_DATA(fmixer, DYNAMIC_GAIN_FILTER);
            if (lfo && (lfo->average[track] <= lfo->gate_threshold)) {
               rv = true;
            }
         }
         else {
            _aaxErrorSet(AAX_INVALID_ENUM);
         }
         break;
      }
   }

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxAudioFrameSetFilter(aaxFrame frame, aaxFilter f)
{
   _frame_t *handle = get_frame(frame, _LOCK, __func__);
   _filter_t* filter = get_filter(f);
   bool rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!filter) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      _aax2dProps *p2d = handle->submix->props2d;
      _aax3dProps *p3d = handle->submix->props3d;
      int type = filter->pos;
      switch (filter->type)
      {
      case AAX_COMPRESSOR:
      case AAX_DYNAMIC_GAIN_FILTER:
         p2d->final.gain_lfo = 1.0f;
         // intentional fallthrough
      case AAX_FREQUENCY_FILTER:
      case AAX_BITCRUSHER_FILTER:
         _FILTER_SWAP_SLOT(p2d, type, filter, 0);
         break;
//    case AAX_GRAPHIC_EQUALIZER:
      case AAX_EQUALIZER:
         if (!handle->mutex) handle->mutex = _aaxMutexCreate(NULL);
         _aaxMutexLock(handle->mutex);
         _FILTER_SWAP_SLOT(handle->submix, EQUALIZER_LF, filter, 0);
         _FILTER_SWAP_SLOT(handle->submix, EQUALIZER_LMF, filter, 1);
         _FILTER_SWAP_SLOT(handle->submix, EQUALIZER_HMF, filter, 2);
         _FILTER_SWAP_SLOT(handle->submix, EQUALIZER_HF, filter, 3);
         _aaxMutexUnLock(handle->mutex);
         break;
      case AAX_DISTANCE_FILTER:
         _FILTER_SWAP_SLOT(p3d, type, filter, 0);
         if (_EFFECT_GET_UPDATED(p3d, VELOCITY_EFFECT) == false)
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
         float inner_vec = _FILTER_GET_SLOT(filter, 0, AAX_INNER_ANGLE);
         float outer_gain = _FILTER_GET_SLOT(filter, 0, AAX_OUTER_GAIN);
         if ((inner_vec >= 0.995f) || (outer_gain >= 0.99f)) {
            _PROP_CONE_CLEAR_DEFINED(p3d);
         } else {
            _PROP_CONE_SET_DEFINED(p3d);
         }
         _FILTER_SWAP_SLOT(p3d, type, filter, 0);
         break;
      }
      case AAX_VOLUME_FILTER:
         _FILTER_SWAP_SLOT(p2d, type, filter, 0);
         _FILTER_SWAP_SLOT(p3d, type, filter, 1);
         _FILTER_COPY_DATA(p3d, p2d, type);
         if (_FILTER_GET_DATA(p3d, type)) {
            _PROP_OCCLUSION_SET_DEFINED(p3d);
         }
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
         rv = false;
      }
   }
   put_frame(frame);

   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxAudioFrameGetFilter(aaxFrame frame, enum aaxFilterType type)
{
   _frame_t *handle = get_frame(frame, _NOLOCK, __func__);
   aaxFilter rv = false;
   if (handle)
   {
      _aaxAudioFrame *submix = handle->submix;
      switch(type)
      {
      case AAX_EQUALIZER:
         rv = _aaxFilterCreateHandle(submix->info, type, _MAX_PARAM_EQ, 0);
         if (rv)
         {
            _filter_t *flt = (_filter_t*)rv;
            _aax_dsp_copy(flt->slot[0], &submix->filter[EQUALIZER_LF]);
            _aax_dsp_copy(flt->slot[1], &submix->filter[EQUALIZER_LMF]);
            _aax_dsp_copy(flt->slot[2], &submix->filter[EQUALIZER_HMF]);
            _aax_dsp_copy(flt->slot[3], &submix->filter[EQUALIZER_HF]);
            flt->state = submix->filter[EQUALIZER_LF].state;
            flt->slot[0]->destroy = _freqfilter_destroy;
            flt->slot[0]->swap = _equalizer_swap;
         }
         break;
      case AAX_FREQUENCY_FILTER:
      case AAX_DYNAMIC_GAIN_FILTER:
      case AAX_BITCRUSHER_FILTER:
      case AAX_VOLUME_FILTER:
      case AAX_DISTANCE_FILTER:
      case AAX_DIRECTIONAL_FILTER:
      case AAX_COMPRESSOR:
         rv = new_filter_handle(frame, type, submix->props2d, submix->props3d);
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   return rv;
}

AAX_API bool AAX_APIENTRY
aaxAudioFrameSetEffect(aaxFrame frame, aaxEffect e)
{
   _frame_t *handle = get_frame(frame, _LOCK, __func__);
   _effect_t* effect = get_effect(e);
   bool rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!effect) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      _aaxAudioFrame* fmixer = handle->submix;
      _aax2dProps *p2d = fmixer->props2d;
      _aax3dProps *p3d = fmixer->props3d;
      int type = effect->pos;
      switch (effect->type)
      {
      case AAX_DYNAMIC_PITCH_EFFECT:
         p2d->final.pitch_lfo = 1.0f;
         // intentional fallthrough
      case AAX_PITCH_EFFECT:
         _PROP_PITCH_SET_CHANGED(p3d);
         // intentional fallthrough
      case AAX_DISTORTION_EFFECT:
      case AAX_PHASING_EFFECT:
      case AAX_CHORUS_EFFECT:
      case AAX_FLANGING_EFFECT:
      case AAX_FREQUENCY_SHIFT_EFFECT:
      case AAX_RINGMODULATOR_EFFECT:
      case AAX_DELAY_EFFECT:
         _EFFECT_SWAP_SLOT(p2d, type, effect, 0);
         break;
      case AAX_REVERB_EFFECT:
      {
         _aaxRingBufferReverbData *reverb;

         _EFFECT_SWAP_SLOT(p2d, type, effect, 0);
         _EFFECT_SWAP_SLOT(p3d, type, effect, 1);
         _EFFECT_COPY_DATA(p3d, p2d, type);
         if ((reverb = _EFFECT_GET_DATA(p2d, type)) != NULL)
         {
            if (_EFFECT_GET_STATE(p2d, REVERB_EFFECT) == false) {
               fmixer->reverb_time = fmixer->reverb_dt = 0.0f;
            }
            else
            {
               float decay_level = _EFFECT_GET_SLOT(effect, 0, AAX_DECAY_LEVEL);
               fmixer->reverb_time = decay_level_to_reverb_time(decay_level);
            }
            _PROP_OCCLUSION_SET_DEFINED(p3d);
         }
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
         rv = false;
      }
   }
   put_frame(handle);

   return rv;
}

AAX_API aaxEffect AAX_APIENTRY
aaxAudioFrameGetEffect(aaxFrame frame, enum aaxEffectType type)
{
   _frame_t *handle = get_frame(frame, _NOLOCK, __func__);
   aaxEffect rv = false;
   if (handle)
   {
      switch(type)
      {
      case AAX_PITCH_EFFECT:
      case AAX_DYNAMIC_PITCH_EFFECT:
      case AAX_DISTORTION_EFFECT:
      case AAX_PHASING_EFFECT:
      case AAX_CHORUS_EFFECT:
      case AAX_FLANGING_EFFECT:
      case AAX_REVERB_EFFECT:
      case AAX_FREQUENCY_SHIFT_EFFECT:
      case AAX_RINGMODULATOR_EFFECT:
      case AAX_DELAY_EFFECT:
      {
         _aaxAudioFrame* fmixer = handle->submix;
         rv = new_effect_handle(frame, type, fmixer->props2d, fmixer->props3d);
         break;
      }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   return rv;
}

AAX_API bool AAX_APIENTRY
aaxAudioFrameSetMode(aaxFrame frame, enum aaxModeType type, int mode)
{
   _frame_t *handle = get_frame(frame, _LOCK, __func__);
   int m, rv = true;

   if (rv)
   {
      _aax3dProps *fp3d = handle->submix->props3d;
      switch(type)
      {
      case AAX_POSITION:
         if (mode & AAX_INDOOR)
         {
            _PROP_INDOOR_SET_DEFINED(fp3d);
            if (mode == AAX_INDOOR) mode = AAX_ABSOLUTE;
            else mode &= ~AAX_INDOOR;
         }
         else
         {
            _PROP_INDOOR_CLEAR_DEFINED(fp3d);
            _PROP_MONO_CLEAR_DEFINED(fp3d);
         }

         m = (mode != AAX_STEREO) ? true : false;
         _TAS_POSITIONAL(fp3d, m);
         if TEST_FOR_TRUE(m)
         {
            m = (mode == AAX_RELATIVE) ? true : false;
            _TAS_RELATIVE(fp3d, m);
            if (_IS_RELATIVE(fp3d))
            {
               _aaxDelayed3dProps *fdp3d = fp3d->dprops3d;
               if (handle->parent[0] && (handle->parent[0] == handle->root))
               {
                  fdp3d->matrix.m4[LOCATION][3] = 0.0;
                  fdp3d->velocity.m4[LOCATION][3] = 0.0;
               }
               else
               {
                  fdp3d->matrix.m4[LOCATION][3] = 1.0;
                  fdp3d->velocity.m4[LOCATION][3] = 1.0;
               }
            }
         }
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
         rv = false;
      }
   }
   put_frame(handle);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameGetMode(const aaxFrame frame, enum aaxModeType type)
{
   _frame_t *handle = get_frame(frame, _NOLOCK, __func__);
   int rv = AAX_STEREO;

   switch(type)
   {
   case AAX_POSITION:
   {
      _aax3dProps *fp3d = handle->submix->props3d;
      if (_IS_POSITIONAL(fp3d)) {
         rv = _IS_RELATIVE(fp3d) ? AAX_RELATIVE : AAX_ABSOLUTE;
      }
      if (_PROP_INDOOR_IS_DEFINED(fp3d)) rv |= AAX_INDOOR;
      break;
   }
   default:
      _aaxErrorSet(AAX_INVALID_ENUM);
   }

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxAudioFrameRegisterSensor(const aaxFrame frame, const aaxConfig sensor)
{
   _handle_t* ssr_config = get_read_handle(sensor, __func__);
   _frame_t* handle = get_frame(frame, _LOCK, __func__);
   bool rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!ssr_config) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (ssr_config->thread.started) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else if (ssr_config->mixer_pos < UINT_MAX) {
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      } else {
         rv = true;
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
            smixer->info->unit_m = fmixer->info->unit_m;
            if (_FILTER_GET_STATE(sp3d, DISTANCE_FILTER) == false)
            {
               _FILTER_COPY_STATE(sp3d, mp3d, DISTANCE_FILTER);
//             _FILTER_COPY_DATA(sp3d, mp3d, DISTANCE_FILTER);
               memcpy(sp3d->filter[DISTANCE_FILTER].data,
                      mp3d->filter[DISTANCE_FILTER].data,
                      sizeof(_aaxRingBufferDistanceData));
            }
            _aaxAudioFrameResetDistDelay(smixer, fmixer);

            if (_EFFECT_GET_DATA(sp3d, VELOCITY_EFFECT) == NULL)
            {
               _EFFECT_COPY(sp3d, mp3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);
               _EFFECT_COPY(sp3d, mp3d, VELOCITY_EFFECT, AAX_DOPPLER_FACTOR);
               _EFFECT_COPY_DATA(sp3d, mp3d, VELOCITY_EFFECT);
            }
            _EFFECT_COPY(sp3d, mp3d, VELOCITY_EFFECT, AAX_LIGHT_VELOCITY);

            ssr_config->handle = handle->handle;
            ssr_config->root = handle->root;
            ssr_config->parent = handle;
            ssr_config->mixer_pos = pos;
            smixer->refctr++;

            if (!smixer->ringbuffer)
            {
               _handle_t *driver = get_driver_handle(frame);
               const _aaxDriverBackend *be = driver->backend.ptr;
               enum aaxRenderMode mode = driver->info->mode;
               float dt = MAX_EFFECTS_TIME;

               smixer->ringbuffer = be->get_ringbuffer(dt, mode);
            }

            rb = smixer->ringbuffer;
            if (rb)
            {
               _aaxMixerInfo* info = smixer->info;
               float delay_sec = 1.0f / info->period_rate;

               rb->set_format(rb, AAX_PCM24S, true);
               rb->set_paramf(rb, RB_FREQUENCY, info->frequency);
               rb->set_parami(rb, RB_NO_TRACKS, 2);

               /* create a ringbuffer with a but of overrun space */
               rb->set_paramf(rb, RB_DURATION_SEC, delay_sec*1.0f);
               rb->init(rb, true);

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
         rv = false;
      }
   }
   put_frame(handle);

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxAudioFrameDeregisterSensor(const aaxFrame frame, const aaxConfig sensor)
{
   _handle_t* ssr_config = get_handle(sensor, __func__);
   _frame_t* handle = get_frame(frame, _NOLOCK, __func__);
   bool rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!ssr_config || ssr_config->mixer_pos == UINT_MAX) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (ssr_config->parent != handle) {
         _aaxErrorSet(AAX_INVALID_STATE+1);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      _intBuffers *hd = handle->submix->devices;
      _handle_t* ptr;

      ptr = _intBufRemove(hd, _AAX_DEVICE, ssr_config->mixer_pos, false);
      if (ptr)
      {
         _intBufferData *dptr;

         handle->submix->no_registered--;
         dptr = _intBufGet(ssr_config->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            sensor->mixer->refctr--;
            ssr_config->mixer_pos = UINT_MAX;
            ssr_config->parent = NULL;
            ssr_config->root = NULL;

            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
   }

   return rv;
}


AAX_API bool AAX_APIENTRY
aaxAudioFrameRegisterEmitter(const aaxFrame frame, const aaxEmitter em)
{
   _emitter_t* emitter = get_emitter_unregistered(em, __func__);
   _frame_t* handle = get_frame(frame, _LOCK, __func__);
   bool rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!emitter) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (emitter->parent || emitter->mixer_pos < UINT_MAX) {
          _aaxErrorSet(AAX_INVALID_STATE+1);
      } else {
         rv = true;
      }
   }

   if (rv && emitter)
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
         _handle_t *driver = get_driver_handle(frame);
         const _aaxDriverBackend *be = driver->backend.ptr;
         if (_aaxIncreaseEmitterCounter(be))
         {
            unsigned int max_emitters = _intBufGetMaxNum(he, _AAX_EMITTER);
            unsigned int no_emitters = _intBufGetNumNoLock(he, _AAX_EMITTER);
            if (no_emitters > handle->max_emitters)
            {
               unsigned int i, num = 0, pos = 0;
               float lowest_gain = 10.0f;
               _emitter_t *ptr;

               for (i=0; i<max_emitters; ++i)
               {
                  _intBufferData *dptr_src = _intBufGet(he, _AAX_EMITTER, i);
                  if (dptr_src != NULL)
                  {
                     _emitter_t *emitter = _intBufGetDataPtr(dptr_src);
                     _aaxEmitter *src = emitter->source;
                     _aax2dProps *ep2d = src->props2d;

                     // first replace an already processed emitter
                     if (_IS_PROCESSED(src->props3d))
                     {
                        _intBufReleaseData(dptr_src, _AAX_EMITTER);
                        pos = emitter->mixer_pos;
                        break;
                     }

                     // second replace the emitter with the lowest gain
                     if (ep2d->final.gain < lowest_gain)
                     {
                        lowest_gain = ep2d->final.gain;
                        pos = emitter->mixer_pos;
                     }
                     _intBufReleaseData(dptr_src, _AAX_EMITTER);
                     if (++num == no_emitters) break;
                  }
               }
               _intBufReleaseNum(he, _AAX_EMITTER);

               ptr = _intBufRemove(he, _AAX_EMITTER, pos, false);
               if (ptr)
               {
                   fmixer->no_registered--;
                   ptr->mixer_pos = UINT_MAX;
                   ptr->parent = NULL;
                   ptr->root = NULL;
               }
            }
            else {
               _intBufReleaseNum(he, _AAX_EMITTER);
            }

            pos = _intBufAddData(he, _AAX_EMITTER, emitter);
            fmixer->no_registered++;
         }
      }

      if (pos != UINT_MAX)
      {
         _aaxEmitter *src = emitter->source;
         emitter->handle = handle->handle;
         emitter->root = handle->root;
         emitter->parent = handle;
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
            aaxFilterDestroy(f);
         }

         src->info = fmixer->info;
         if (emitter->midi.mode == AAX_RENDER_DEFAULT) {
            emitter->midi.mode = fmixer->info->midi_mode;
         }

         if (positional)
         {
            _aax3dProps *ep3d = src->props3d;
            _aax3dProps *mp3d = fmixer->props3d;

            if (_FILTER_GET_STATE(ep3d, DISTANCE_FILTER) == false)
            {
               _FILTER_COPY_STATE(ep3d, mp3d, DISTANCE_FILTER);
//             _FILTER_COPY_DATA(ep3d, mp3d, DISTANCE_FILTER);
               memcpy(ep3d->filter[DISTANCE_FILTER].data,
                      mp3d->filter[DISTANCE_FILTER].data,
                      sizeof(_aaxRingBufferDistanceData));
            }
            _aaxEMitterResetDistDelay(src, fmixer);

            if (_EFFECT_GET_DATA(ep3d, VELOCITY_EFFECT) == NULL)
            {
               _EFFECT_COPY(ep3d, mp3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);
               _EFFECT_COPY(ep3d, mp3d, VELOCITY_EFFECT, AAX_DOPPLER_FACTOR);
               _EFFECT_COPY_DATA(ep3d, mp3d, VELOCITY_EFFECT);
            }
            _EFFECT_COPY(ep3d, mp3d, VELOCITY_EFFECT, AAX_LIGHT_VELOCITY);

            // TODO: add _aaxRingBufferReflectionData to the emitter
         }
      }
      else
      {
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         rv = false;
      }
   }
   put_frame(handle);

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxAudioFrameDeregisterEmitter(const aaxFrame frame, const aaxEmitter em)
{
   _emitter_t* emitter = get_emitter(em, _LOCK, __func__);
   _frame_t* handle = get_frame(frame, _LOCK, __func__);
   bool rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!emitter) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (emitter->mixer_pos == UINT_MAX || emitter->parent != handle) {
         _aaxErrorSet(AAX_INVALID_STATE+1);
      }
      else
      {
         _intBuffers *he;
         if (_IS_POSITIONAL(emitter->source->props3d)) {
            he = handle->submix->emitters_3d;
         } else {
            he = handle->submix->emitters_2d;
         }

         if (_intBufGetNumNoLock(he, _AAX_EMITTER) == 0) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         } else {
            rv = true;
         }
      }
   }

   if (rv)
   {
      _handle_t *driver = get_driver_handle(frame);
       const _aaxDriverBackend *be = driver->backend.ptr;
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
      _intBufRemove(he, _AAX_EMITTER, emitter->mixer_pos, false);
      _aaxDecreaseEmitterCounter(be);
      fmixer->no_registered--;
      emitter->mixer_pos = UINT_MAX;
      emitter->parent = NULL;
      emitter->root = NULL;
   }
   put_frame(frame);
   put_emitter(emitter);

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxAudioFrameRegisterAudioFrame(const aaxFrame frame, const aaxFrame subframe)
{
   _frame_t* sframe = get_frame(subframe, _LOCK, __func__);
   _frame_t* handle = get_frame(frame, _LOCK, __func__);
   bool rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!sframe) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
#if ENABLE_MULTI_REGISTER
      } else if (sframe->parent[E]) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else if (sframe->mixer_pos[0] < UINT_MAX) {
         _aaxErrorSet(AAX_INVALID_STATE);
#endif
      } else {
         rv = true;
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
         _aaxAudioFrame *submix = sframe->submix;
         if (!submix->refctr)
         { /* first time the frame gets registered */
            aaxBuffer buf; /* clear the frame's buffer queue */
            while ((buf = aaxAudioFrameGetBuffer(sframe)) != NULL) {
               aaxBufferDestroy(buf);
            }
            _aaxErrorSet(AAX_ERROR_NONE);
         }

         pos = _intBufAddData(hf, _AAX_FRAME, sframe);
      }
      else {
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      }

      if (pos != UINT_MAX)
      {
         _aaxAudioFrame *submix = sframe->submix;
         _aax2dProps *mp2d = fmixer->props2d;
         _aax2dProps *fp2d = submix->props2d;
         _aax3dProps *mp3d = fmixer->props3d;
         _aax3dProps *fp3d = submix->props3d;

         fmixer->no_registered++;

         fp2d->parent = mp2d;
         fp3d->parent = mp3d;
         if (_FILTER_GET_STATE(fp3d, DISTANCE_FILTER) == false)
         {
            _FILTER_COPY_STATE(fp3d, mp3d, DISTANCE_FILTER);
//          _FILTER_COPY_DATA(fp3d, mp3d, DISTANCE_FILTER);
            memcpy(fp3d->filter[DISTANCE_FILTER].data,
                   mp3d->filter[DISTANCE_FILTER].data,
                   sizeof(_aaxRingBufferDistanceData));
         }

         if (_EFFECT_GET_DATA(fp3d, VELOCITY_EFFECT) == NULL)
         {
            _EFFECT_COPY(fp3d, mp3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);
            _EFFECT_COPY(fp3d, mp3d, VELOCITY_EFFECT, AAX_DOPPLER_FACTOR);
            _EFFECT_COPY_DATA(fp3d, mp3d, VELOCITY_EFFECT);
         }
         _EFFECT_COPY(fp3d, mp3d, VELOCITY_EFFECT, AAX_LIGHT_VELOCITY);

         if (_PROP_INDOOR_IS_DEFINED(mp3d)) {
            _PROP_MONO_SET_DEFINED(fp3d);
         }

         if (submix->refctr++ == 0)
         {
            sframe->parent[0] = handle;
            sframe->mixer_pos[0] = pos;
         }

         _aaxAudioFrameResetDistDelay(submix, fmixer);
      }
      else
      {
         if (sframe->parent[0]) put_frame(sframe);
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         rv = false;
      }
   }
   put_frame(handle);

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxAudioFrameDeregisterAudioFrame(const aaxFrame frame, const aaxFrame subframe)
{
   _frame_t* sframe = get_frame(subframe, _LOCK, __func__);
   _frame_t* handle = get_frame(frame, _LOCK, __func__);
   bool rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!sframe || sframe->mixer_pos[0] == UINT_MAX) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (sframe->parent[0] != handle) {
         _aaxErrorSet(AAX_INVALID_STATE+1);
      } else if (_intBufGetNumNoLock(handle->submix->frames, _AAX_FRAME) == 0) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      _intBuffers *hf = handle->submix->frames;
      _aaxAudioFrame *submix = sframe->submix;

      /* Unlock the frame again to make sure locking is done in the proper */
      /* order by _intBufRemove                                            */
      _intBufRelease(hf, _AAX_FRAME, sframe->mixer_pos[0]);
      _intBufRemove(hf, _AAX_FRAME, sframe->mixer_pos[0], false);
      if (--submix->refctr == 0)
      {
         sframe->mixer_pos[0] = UINT_MAX;
         sframe->parent[0] = NULL;
         sframe->root = NULL;
      }

      handle->submix->no_registered--;
   }
   put_frame(handle);
   put_frame(sframe);

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxAudioFrameSetState(aaxFrame frame, enum aaxState state)
{
   _frame_t* handle = get_frame(frame, _LOCK, __func__);
   bool rv = false;
   if (handle)
   {
      _aax3dProps *fp3d = handle->submix->props3d;
      switch (state)
      {
      case AAX_UPDATE:
         rv = _aaxAudioFrameUpdate(handle);
         break;
      case AAX_SUSPENDED:
         _SET_PAUSED(fp3d);
         rv = true;
         break;
      case AAX_STANDBY:
         _SET_STANDBY(fp3d);
         rv = true;
         break;
      case AAX_PLAYING:
         put_frame(frame);
         rv = _aaxAudioFrameStart(handle);
         handle = get_frame(frame, _LOCK, __func__);
         if (rv) _SET_PLAYING(fp3d);
         break;
      case AAX_PROCESSED:
      case AAX_STOPPED:
         rv = _aaxAudioFrameStop(handle);
         if (rv) _SET_PROCESSED(fp3d);
         break;
      case AAX_INITIALIZED:
         handle->mtx_set = false;
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   put_frame(frame);
   return rv;
}

AAX_API enum aaxState AAX_APIENTRY
aaxAudioFrameGetState(UNUSED(const aaxFrame frame))
{
   enum aaxState ret = AAX_STATE_NONE;
   return ret;
}

AAX_API bool AAX_APIENTRY
aaxAudioFrameWaitForBuffer(const aaxFrame frame, float timeout)
{
   _frame_t* handle = get_frame(frame, _LOCK, __func__);
   bool rv = false;
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

      if (nbuf) rv = true;
      else _aaxErrorSet(AAX_TIMEOUT);
   }
   return rv;
}

AAX_API bool AAX_APIENTRY
aaxAudioFrameAddBuffer(aaxFrame frame, aaxBuffer buf)
{
   _frame_t* handle = get_frame(frame, _NOLOCK, __func__);
   _buffer_t* buffer = get_buffer(buf, __func__);
   bool rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!buffer) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
      else if (!buffer->aaxs) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      _aaxAudioFrame* fmixer = handle->submix;
      if (RENDER_NORMAL(fmixer->info->midi_mode)) {
         rv = _frameCreateEFFromAAXS(handle, buffer);
      }
      if (!buffer->root)
      {
         buffer->handle = handle->handle;
         buffer->root = handle->root;
      }
   }
   return rv;
}

AAX_API aaxBuffer AAX_APIENTRY
aaxAudioFrameGetBuffer(const aaxFrame frame)
{
   _frame_t* handle = get_frame(frame, _LOCK, __func__);
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

            buf->info.blocksize = 1;
            buf->pos = 0;
            buf->info.no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
            buf->info.no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
            buf->info.fmt = rb->get_parami(rb, RB_FORMAT);
            buf->info.rate = rb->get_paramf(rb, RB_FREQUENCY);

            buf->mipmap = false;

            buf->mixer_info = &_info;
            rb->set_parami(rb, RB_IS_MIXER_BUFFER, false);
            buf->ringbuffer[0] = rb;

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
get_frame(aaxFrame f, int lock, const char *func)
{
   _frame_t* frame = (_frame_t*)f;
   _frame_t* rv = NULL;

   if (frame && (frame->id == AUDIOFRAME_ID))
   {
      if (frame->parent[0])
      {
         if (frame->parent[0] == frame->root)
         {
            _handle_t *handle = frame->root;
            _intBufferData *dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {			/* frame is registered at the final mixer */
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               _aaxAudioFrame* smixer = sensor->mixer;
               _intBufferData *dptr_frame;
               _intBuffers *hf;

               hf = smixer->frames;

               if (lock) {
                  dptr_frame = _intBufGet(hf, _AAX_FRAME, frame->mixer_pos[0]);
               } else {
                  dptr_frame =_intBufGetNoLock(hf,_AAX_FRAME, frame->mixer_pos[0]);
               }
               frame = _intBufGetDataPtr(dptr_frame);
               _intBufReleaseData(dptr, _AAX_SENSOR);
            }
         }
         else			/* subframe is registered at another frame */
         {
            _frame_t *handle = frame->parent[0];
            _intBufferData *dptr_frame;
            _intBuffers *hf;

            hf =  handle->submix->frames;
            if (lock) {
               dptr_frame = _intBufGet(hf, _AAX_FRAME, frame->mixer_pos[0]);
            } else {
               dptr_frame = _intBufGetNoLock(hf, _AAX_FRAME, frame->mixer_pos[0]);
            }
            frame = _intBufGetDataPtr(dptr_frame);
         }
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
      if (frame->parent[0])
      {
         if (frame->parent[0] == frame->root)
         {
            _handle_t *handle = frame->root;
            _intBufferData *dptr =_intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               _aaxAudioFrame* smixer = sensor->mixer;
               _intBuffers *hf;

               hf = smixer->frames;
               _intBufRelease(hf, _AAX_FRAME, frame->mixer_pos[0]);
               _intBufReleaseData(dptr, _AAX_SENSOR);
            }
         }
         else
         {
            _frame_t *handle = frame->parent[0];
            _intBuffers *hf;

            hf =  handle->submix->frames;
            _intBufRelease(hf, _AAX_FRAME, frame->mixer_pos[0]);
         }
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
      double dist;
      float vs;

      vs = _EFFECT_GET(pp3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);

      /**
       * Align the modified frame matrix with the sensor by multiplying
       * the frame matrix by the modified parent matrix.
       */
      mtx4dMul(&fdp3d_m->matrix, &pdp3d_m->matrix, &fdp3d->matrix);
      dist = vec3dMagnitude(&fdp3d_m->matrix.v34[LOCATION]);
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


static bool
_aaxAudioFrameStart(_frame_t *frame)
{
   _aaxAudioFrame* fmixer = frame->submix;
   _aax3dProps *fp3d;
   bool rv = false;

   assert(frame);

   fp3d = fmixer->props3d;
   if (_IS_INITIAL(fp3d) || _IS_PROCESSED(fp3d))
   {
      fmixer->reverb_dt = 0.0f;
      fmixer->capturing = false;
      rv = true;
   }
   else if _IS_STANDBY(fp3d) {
      rv = true;
   }
   return rv;
}

static bool
_aaxAudioFrameStop(UNUSED(_frame_t *frame))
{
   bool rv = true;
   return rv;
}

static bool
_aaxAudioFrameUpdate(UNUSED(_frame_t *frame))
{
   bool rv = true;
   return rv;
}

static bool
_frameCreateBodyFromAAXS(aaxFrame frame, _frame_t* handle, _buffer_t *buffer, xmlId *xmid)
{
   float freq = buffer->info.base_frequency;
   aaxConfig config = handle->root;
   int clear = false;
   _aaxAudioFrame* fmixer;
   xmlId *xeid, *xfid;
   int i, num;

   if (xmlAttributeExists(xmid, "mode")) {
      clear = xmlAttributeCompareString(xmid, "mode", "append");
   }
#if 0
   if (xmlAttributeExists(xmid, "stereo") &&
       xmlAttributeGetBool(xmid, "stereo") == 0)
   {
      _aax3dProps *fp3d = handle->submix->props3d;
      _PROP_MONO_SET_DEFINED(fp3d);
      _PROP_INDOOR_SET_DEFINED(fp3d);
   }
#endif

   if (!handle->mtx_set)
   {
      float pan = xmlAttributeGetDouble(xmid, "pan");
      if (fabsf(pan) > 0.01f)
      {
          static aaxVec3f _at = { 0.0f, 0.0f, -1.0f };
          aaxMtx4d mtx641, mtx642;
          aaxVec3f at, up;
          aaxVec3d pos;

          // Note: If there is no position offset this will only rotate
          aaxAudioFrameGetMatrix64(frame, mtx641);
          aaxMatrix64GetOrientation(mtx641, pos, at, up);

          aaxMatrix64SetIdentityMatrix(mtx641);
          aaxMatrix64SetOrientation(mtx641, pos, _at, up);

          aaxMatrix64SetIdentityMatrix(mtx642);
          aaxMatrix64Rotate(mtx642, -GMATH_PI_4*pan, 0.0, 1.0, 0.0);

          aaxMatrix64Multiply(mtx642, mtx641);
          aaxAudioFrameSetMatrix64(frame, mtx642);
          handle->mtx_set = true;
      }
   }

   if (xmlAttributeExists(xmid, "gain"))
   {
      _aax2dProps *p2d = handle->submix->props2d;
      p2d->final.gain = xmlAttributeGetDouble(xmid, "gain");
   }

   if (xmlAttributeExists(xmid, "max-emitters"))
   {
      handle->max_emitters = xmlAttributeGetInt(xmid, "max-emitters");
      handle->max_emitters = _MAX(handle->max_emitters, 1);
   }

   fmixer = handle->submix;
   if (clear) {
      _aaxSetDefault2dFiltersEffects(fmixer->props2d);
   }

   // No filters and effects for audio-frames if not normal rendering
   if (RENDER_NORMAL(fmixer->info->midi_mode))
   {
      xfid = xmlMarkId(xmid);
      num = xmlNodeGetNum(xmid, "filter");
      for (i=0; i<num; i++)
      {
         if (xmlNodeGetPos(xmid, xfid, "filter", i) != 0)
         {
            aaxFilter f;
            f = _aaxGetFilterFromAAXS(config, xfid, freq, 0.0f, 0.0f, NULL);
            if (f)
            {
               aaxAudioFrameSetFilter(frame, f);
               aaxFilterDestroy(f);
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
            aaxEffect eff = _aaxGetEffectFromAAXS(config, xeid, freq, 0.0f, 0.0f, NULL);
            if (eff)
            {
               aaxAudioFrameSetEffect(frame, eff);
               aaxEffectDestroy(eff);
            }
         }
      }
      xmlFree(xeid);
   }
   else if (fmixer->info->midi_mode == AAX_RENDER_SYNTHESIZER)
   {
      aaxEffect eff = aaxEffectCreate(config, AAX_PHASING_EFFECT);
      if (eff)
      {
         aaxEffectSetSlot(eff, 0, AAX_LINEAR, 0.7f, 0.1f, 0.5f, 0.9f);
         aaxEffectSetState(eff, true);
         aaxAudioFrameSetEffect(frame, eff);
         aaxEffectDestroy(eff);
      }
   }

   return true;
}

static bool
_frameCreateEFFromAAXS(aaxFrame frame, _buffer_t *buffer)
{
   const char *aaxs = buffer->aaxs;
   _frame_t* handle = get_frame(frame, _NOLOCK, __func__);
   bool rv = true;
   xmlId *xid;

   xid = xmlInitBuffer(aaxs, strlen(aaxs));
   if (xid)
   {
      int polyphony = buffer->info.polyphony;
      xmlId *xmid;

      if (polyphony)
      {
         // number of simultaneous keys for an instrument
         unsigned int min = 12;
         unsigned int max = 88;
         handle->max_emitters = polyphony;
         handle->max_emitters = _MINMAX(handle->max_emitters, min, max);
      }

      xmid = xmlNodeGet(xid, "aeonwave/audioframe");
      if (xmid)
      {
         if (xmlAttributeExists(xmid, "include"))
         {
            char file[1024];
            int len = xmlAttributeCopyString(xmid, "include", file, 1024);
            if (len < 1024-strlen(".aaxs"))
            {
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
                     xmlId *xamid = xmlNodeGet(xid, "aeonwave/audioframe");
                     if (xamid)
                     {
                        rv = _frameCreateBodyFromAAXS(frame, handle, buffer, xamid);
                        xmlFree(xamid);
                     }
                     xmlClose(xid);
                  }
                  free(data);
               }
               free(url);
            }
         }
         rv = _frameCreateBodyFromAAXS(frame, handle, buffer, xmid);
         xmlFree(xmid);
      }
      xmlClose(xid);
   }
   else
   {
      _aaxErrorSet(AAX_INVALID_STATE);
      rv = false;
   }
   return rv;
}
