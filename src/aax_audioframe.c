/*
 * Copyright 2011-2019 by Erik Hofman.
 * Copyright 2011-2019 by Adalin B.V.
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
#include <dsp/lfo.h>

#include "ringbuffer.h"
#include "arch.h"
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
         frame->root = handle->root;
         frame->mixer_pos = UINT_MAX;
         frame->max_emitters = get_low_resource() ? 32 : 256;
         if (handle->info->midi_mode)
         {
            if (handle->info->midi_mode == AAX_RENDER_ARCADE) {
               frame->max_emitters = 2;
            } else {
               frame->max_emitters = 8;
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

               _FILTER_COPYD3D_DATA(submix, smixer, DISTANCE_FILTER);
               _EFFECT_COPYD3D(submix, smixer, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);
               _EFFECT_COPYD3D(submix, smixer, VELOCITY_EFFECT, AAX_DOPPLER_FACTOR);
               _EFFECT_COPYD3D(submix, smixer, VELOCITY_EFFECT, AAX_LIGHT_VELOCITY);
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

   handle = get_frame(frame, _LOCK, __func__);
   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (handle->parent) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      int i;

      _aaxAudioFrame* fmixer = handle->submix;

      _aaxMutexLock(fmixer->props2d->mutex);
      for (i=0; i<MAX_STEREO_FILTER; ++i) {
         _FILTER_FREE2D_DATA(fmixer, i);
      }

      for (i=0; i<MAX_STEREO_EFFECT; ++i) {
         _EFFECT_FREE2D_DATA(fmixer, i);
      }
      _aaxMutexDestroy(fmixer->props2d->mutex);

      _aaxMutexLock(fmixer->props3d->mutex);
      _FILTER_FREE3D_DATA(fmixer, DISTANCE_FILTER);
      _aaxMutexDestroy(fmixer->props3d->mutex);

      _intBufErase(&fmixer->p3dq, _AAX_DELAYED3D, _aax_aligned_free);
      _aax_aligned_free(fmixer->props3d->dprops3d);
      free(fmixer->props3d);

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

      /* frees both EQUALIZER_LF and EQUALIZER_HF */
      if (handle->filter)
      {
         if (handle->filter[EQUALIZER_LF].data) {
            _aaxMutexDestroy(handle->mutex);
         }
         _FILTER_FREE_DATA(handle, EQUALIZER_LF);
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
aaxAudioFrameSetMatrix64(aaxFrame frame, aaxMtx4d mtx64)
{
   _frame_t *handle = get_frame(frame, _LOCK, __func__);
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
      _aaxAudioFrame* fmixer = handle->submix;
      _aax3dProps *fp3d = fmixer->props3d;
      _aaxDelayed3dProps *fdp3d = fp3d->dprops3d;

#ifdef ARCH32
      mtx4fFilld(fdp3d->matrix.m4, mtx64);
#else
      mtx4dFill(fdp3d->matrix.m4, mtx64);
#endif

      if (_IS_RELATIVE(fp3d) &&
          handle->parent && (handle->parent == handle->root))
      {
         fdp3d->matrix.m4[LOCATION][3] = 0.0;
      } else {
         fdp3d->matrix.m4[LOCATION][3] = 1.0;
      }
      _PROP_MTX_SET_CHANGED(fp3d);
      handle->mtx_set = AAX_TRUE;

      if (_IS_RELATIVE(fp3d))
      {
         _frame_t *parent = handle->parent;

         // Walk back to the lowest parent frame which is registered
         // at the sensor and mark it changed.
         if (parent)
         {
            while ((void*)parent != handle->root)
            {
               handle = parent;
               parent = handle->parent;
            }
         }
         _PROP_MTX_SET_CHANGED(handle->submix->props3d);
      }
   }
   put_frame(frame);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameGetMatrix64(aaxFrame frame, aaxMtx4d mtx64)
{
   _frame_t *handle = get_frame(frame, _NOLOCK, __func__);
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

   if (rv) {
#ifdef ARCH32
      mtx4dFillf(mtx64, handle->submix->props3d->dprops3d->matrix.m4);
#else
      mtx4dFill(mtx64, handle->submix->props3d->dprops3d->matrix.m4);
#endif
   }

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameSetVelocity(aaxFrame frame, aaxVec3f velocity)
{
   _frame_t *handle = get_frame(frame, _LOCK, __func__);
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
      _aaxAudioFrame* fmixer = handle->submix;
      _aax3dProps *fp3d = fmixer->props3d;
      _aaxDelayed3dProps *fdp3d = fp3d->dprops3d;

      vec3fFill(fdp3d->velocity.m4[VELOCITY], velocity);
      if (_IS_RELATIVE(fp3d) &&
          handle->parent && (handle->parent == handle->root))
      {
         fdp3d->velocity.m4[LOCATION][3] = 0.0f;
      } else {
         fdp3d->velocity.m4[LOCATION][3] = 1.0f;
      }
      _PROP_SPEED_SET_CHANGED(fp3d);

      if (_IS_RELATIVE(fp3d))
      {
         _frame_t *parent = handle->parent;

         // Walk back to the lowest parent frame which is registered
         // at the sensor and mark it changed.
         if (parent)
         {
            while ((void*)parent != handle->root)
            {
               handle = parent;
               parent = handle->parent;

            }
         }
         _PROP_SPEED_SET_CHANGED(handle->submix->props3d);
      }
   }
   put_frame(frame);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameGetVelocity(aaxFrame frame, aaxVec3f velocity)
{
   _frame_t *handle = get_frame(frame, _NOLOCK, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!velocity) {
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

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameSetSetup(UNUSED(aaxFrame frame), UNUSED(enum aaxSetupType type), UNUSED(unsigned int setup))
{
   _frame_t *handle = get_frame(frame, _NOLOCK, __func__);
   int rv = AAX_FALSE;

   switch(type)
   {
   case AAX_MONO_EMITTERS:
       rv = handle->max_emitters;
      break;
   case AAX_STEREO_EMITTERS:
      rv = handle->max_emitters/2;
      break;
   default:
      break;
   }
   return rv;
}

AAX_API unsigned int AAX_APIENTRY
aaxAudioFrameGetSetup(const aaxFrame frame, enum aaxSetupType type)
{
   _frame_t *handle = get_frame(frame, _NOLOCK, __func__);
   unsigned int track = type & 0x3F;
   unsigned int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (track >= _AAX_MAX_SPEAKERS) {
         _aaxErrorSet(AAX_INVALID_ENUM);
      } else {
         rv = AAX_TRUE;
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
               rv = 256*32768*lfo->compression[track];
            }
         }
         else if (type & AAX_GATE_ENABLED)
         {
            _aaxAudioFrame* fmixer = handle->submix;
            _aaxLFOData *lfo;

            lfo = _FILTER_GET2D_DATA(fmixer, DYNAMIC_GAIN_FILTER);
            if (lfo && (lfo->average[track] <= lfo->gate_threshold)) {
               rv = AAX_TRUE;
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

AAX_API int AAX_APIENTRY
aaxAudioFrameSetFilter(aaxFrame frame, aaxFilter f)
{
   _frame_t *handle = get_frame(frame, _LOCK, __func__);
   _filter_t* filter = get_filter(f);
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!filter) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aax2dProps *p2d = handle->submix->props2d;
      _aax3dProps *p3d = handle->submix->props3d;
      int type = filter->pos;
      switch (filter->type)
      {
//    case AAX_GRAPHIC_EQUALIZER:
      case AAX_EQUALIZER:
         if (!handle->filter)  		/* EQUALIZER_LF & EQUALIZER_HF */
         {
            handle->mutex = _aaxMutexCreate(NULL);
            handle->filter = calloc(2, sizeof(_aaxFilterInfo));
         }

         if (handle->filter)
         {
            _FILTER_SWAP_SLOT(handle, EQUALIZER_HF, filter, 1);
            _FILTER_SWAP_SLOT(handle, EQUALIZER_LF, filter, 0);
            break;
         }
         else {
            _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         }
         break;
      case AAX_FREQUENCY_FILTER:
      case AAX_DYNAMIC_GAIN_FILTER:
      case AAX_BITCRUSHER_FILTER:
      case AAX_COMPRESSOR:
         _FILTER_SWAP_SLOT(p2d, type, filter, 0);
         if (filter->type == AAX_DYNAMIC_GAIN_FILTER ||
             filter->type == AAX_COMPRESSOR) {
            p2d->final.gain_lfo = 1.0f;
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
         if (filter->type == AAX_DYNAMIC_GAIN_FILTER ||
             filter->type == AAX_COMPRESSOR) {
            p2d->final.gain_lfo = 1.0f;
         }

         _FILTER_SWAP_SLOT(p3d, type, filter, 1);
         _FILTER_COPY_DATA(p3d, p2d, type);
         if (_FILTER_GET_DATA(p3d, type)) {
            _PROP_OCCLUSION_SET_DEFINED(p3d);
         }
         break;
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
   _frame_t *handle = get_frame(frame, _NOLOCK, __func__);
   aaxFilter rv = AAX_FALSE;
   if (handle)
   {
      switch(type)
      {
      case AAX_EQUALIZER:
      case AAX_FREQUENCY_FILTER:
      case AAX_DYNAMIC_GAIN_FILTER:
      case AAX_BITCRUSHER_FILTER:
      case AAX_VOLUME_FILTER:
      case AAX_DISTANCE_FILTER:
      case AAX_DIRECTIONAL_FILTER:
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
   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameSetEffect(aaxFrame frame, aaxEffect e)
{
   _frame_t *handle = get_frame(frame, _LOCK, __func__);
   _effect_t* effect = get_effect(e);
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!effect) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
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
      case AAX_PITCH_EFFECT:
      case AAX_DYNAMIC_PITCH_EFFECT:
         _PROP_PITCH_SET_CHANGED(p3d);
         if ((enum aaxEffectType)effect->type == AAX_DYNAMIC_PITCH_EFFECT) {
            p2d->final.pitch_lfo = 1.0f;
         }
         // intentional fallthrough
      case AAX_DISTORTION_EFFECT:
      case AAX_RINGMODULATOR_EFFECT:
      case AAX_FLANGING_EFFECT:
      case AAX_PHASING_EFFECT:
      case AAX_CHORUS_EFFECT:
         _EFFECT_SWAP_SLOT(p2d, type, effect, 0);
         break;
      case AAX_REVERB_EFFECT:
         _EFFECT_SWAP_SLOT(p2d, type, effect, 0);
         _EFFECT_SWAP_SLOT(p3d, type, effect, 1);
         _EFFECT_COPY_DATA(p3d, p2d, type);
         if (_EFFECT_GET_DATA(p2d, type)) {
            _PROP_OCCLUSION_SET_DEFINED(p3d);
         }

         // TODO: add _aaxRingBufferReflectionData to all registered emitters
         break;
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
   _frame_t *handle = get_frame(frame, _NOLOCK, __func__);
   aaxEffect rv = AAX_FALSE;
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
      case AAX_RINGMODULATOR_EFFECT:
      case AAX_REVERB_EFFECT:
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

AAX_API int AAX_APIENTRY
aaxAudioFrameSetMode(aaxFrame frame, enum aaxModeType type, int mode)
{
   _frame_t *handle = get_frame(frame, _LOCK, __func__);
   int m, rv = AAX_TRUE;

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

         m = (mode != AAX_STEREO) ? AAX_TRUE : AAX_FALSE;
         _TAS_POSITIONAL(fp3d, m);
         if TEST_FOR_TRUE(m)
         {
            m = (mode == AAX_RELATIVE) ? AAX_TRUE : AAX_FALSE;
            _TAS_RELATIVE(fp3d, m);
            if (_IS_RELATIVE(fp3d))
            {
               _aaxDelayed3dProps *fdp3d = fp3d->dprops3d;
               if (handle->parent && (handle->parent == handle->root))
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
         rv = AAX_FALSE;
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

AAX_API int AAX_APIENTRY
aaxAudioFrameRegisterSensor(const aaxFrame frame, const aaxConfig sensor)
{
   _handle_t* ssr_config = get_read_handle(sensor, __func__);
   _frame_t* handle = get_frame(frame, _LOCK, __func__);
   int rv = __release_mode;

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
            smixer->info->unit_m = fmixer->info->unit_m;
            if (_FILTER_GET_STATE(sp3d, DISTANCE_FILTER) == AAX_FALSE)
            {
               _FILTER_COPY_STATE(sp3d, mp3d, DISTANCE_FILTER);
               _FILTER_COPY_DATA(sp3d, mp3d, DISTANCE_FILTER);
            }
            _aaxAudioFrameResetDistDelay(smixer, fmixer);

            if (_EFFECT_GET_DATA(sp3d, VELOCITY_EFFECT) == NULL)
            {
               _EFFECT_COPY(sp3d, mp3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);
               _EFFECT_COPY(sp3d, mp3d, VELOCITY_EFFECT, AAX_DOPPLER_FACTOR);
               _EFFECT_COPY_DATA(sp3d, mp3d, VELOCITY_EFFECT);
            }
            _EFFECT_COPY(sp3d, mp3d, VELOCITY_EFFECT, AAX_LIGHT_VELOCITY);

            ssr_config->root = handle->root;
            ssr_config->parent = handle;
            ssr_config->mixer_pos = pos;
            smixer->refcount++;

            if (!smixer->ringbuffer)
            {
               _handle_t *driver = get_driver_handle(frame);
               const _aaxDriverBackend *be = driver->backend.ptr;
               enum aaxRenderMode mode = driver->info->mode;
               float dt = FRAME_REVERB_EFFECTS_TIME;

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
   _frame_t* handle = get_frame(frame, _NOLOCK, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!ssr_config || ssr_config->mixer_pos == UINT_MAX) {
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
            ssr_config->parent = NULL;
            ssr_config->root = NULL;

            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
   }

   return rv;
}


AAX_API int AAX_APIENTRY
aaxAudioFrameRegisterEmitter(const aaxFrame frame, const aaxEmitter em)
{
   _emitter_t* emitter = get_emitter_unregistered(em, __func__);
   _frame_t* handle = get_frame(frame, _LOCK, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!emitter || emitter->parent || emitter->mixer_pos < UINT_MAX) {
          _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
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

               ptr = _intBufRemove(he, _AAX_EMITTER, pos, AAX_FALSE);
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
         }

         src->info = fmixer->info;
         if (!emitter->midi.mode) {
            emitter->midi.mode = src->info->midi_mode;
         }
         if (src->update_rate == 0) {
            src->update_rate = fmixer->info->update_rate;
         }
         src->update_ctr = 1;

         if (positional)
         {
            _aax3dProps *ep3d = src->props3d;
            _aax3dProps *mp3d = fmixer->props3d;

            if (_FILTER_GET_STATE(ep3d, DISTANCE_FILTER) == AAX_FALSE)
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
            _EFFECT_COPY(ep3d, mp3d, VELOCITY_EFFECT, AAX_LIGHT_VELOCITY);

            // TODO: add _aaxRingBufferReflectionData to the emitter
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
   _emitter_t* emitter = get_emitter(em, _LOCK, __func__);
   _frame_t* handle = get_frame(frame, _LOCK, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!emitter || emitter->mixer_pos == UINT_MAX) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
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
            rv = AAX_TRUE;
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
      _intBufRemove(he, _AAX_EMITTER, emitter->mixer_pos, AAX_FALSE);
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

AAX_API int AAX_APIENTRY
aaxAudioFrameRegisterAudioFrame(const aaxFrame frame, const aaxFrame subframe)
{
   _frame_t* sframe = get_frame(subframe, _LOCK, __func__);
   _frame_t* handle = get_frame(frame, _LOCK, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!sframe) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (sframe->parent) {
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
         _aaxAudioFrame *submix = sframe->submix;
         _aax3dProps *mp3d = fmixer->props3d;
         _aax3dProps *fp3d = submix->props3d;

         fp3d->parent = mp3d;
         if (_FILTER_GET_STATE(fp3d, DISTANCE_FILTER) == AAX_FALSE)
         {
            _FILTER_COPY_STATE(fp3d, mp3d, DISTANCE_FILTER);
            _FILTER_COPY_DATA(fp3d, mp3d, DISTANCE_FILTER);
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

         submix->refcount++;
         sframe->parent = handle;
         sframe->mixer_pos = pos;

         _aaxAudioFrameResetDistDelay(submix, fmixer);
      }
      else
      {
         if (sframe->parent) put_frame(sframe);
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
   _frame_t* sframe = get_frame(subframe, _LOCK, __func__);
   _frame_t* handle = get_frame(frame, _LOCK, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!sframe || sframe->mixer_pos == UINT_MAX) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (_intBufGetNumNoLock(handle->submix->frames, _AAX_FRAME) == 0) {
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
      sframe->parent = NULL;
      sframe->root = NULL;

      handle->submix->no_registered--;
   }
   put_frame(handle);
   put_frame(sframe);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxAudioFrameSetState(aaxFrame frame, enum aaxState state)
{
   _frame_t* handle = get_frame(frame, _LOCK, __func__);
   int rv = AAX_FALSE;
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
         rv = AAX_TRUE;
         break;
      case AAX_STANDBY:
         _SET_STANDBY(fp3d);
         rv = AAX_TRUE;
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
         handle->mtx_set = AAX_FALSE;
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
   _frame_t* handle = get_frame(frame, _LOCK, __func__);
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
   _frame_t* handle = get_frame(frame, _NOLOCK, __func__);
   _buffer_t* buffer = get_buffer(buf, __func__);
   int rv = __release_mode;

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
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxAudioFrame* fmixer = handle->submix;
      if (!fmixer->info->midi_mode) {
         rv = _frameCreateEFFromAAXS(handle, buffer->aaxs);
      }
      if (!buffer->root) {
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
            buf->info.tracks = rb->get_parami(rb, RB_NO_TRACKS);
            buf->info.no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
            buf->info.fmt = rb->get_parami(rb, RB_FORMAT);
            buf->info.freq = rb->get_paramf(rb, RB_FREQUENCY);

            buf->mipmap = AAX_FALSE;

            buf->mixer_info = &_info;
            rb->set_parami(rb, RB_IS_MIXER_BUFFER, AAX_FALSE);
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
      if (frame->parent)
      {
         if (frame->parent == frame->root)
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
                  dptr_frame = _intBufGet(hf, _AAX_FRAME, frame->mixer_pos);
               } else {
                  dptr_frame =_intBufGetNoLock(hf,_AAX_FRAME, frame->mixer_pos);
               }
               frame = _intBufGetDataPtr(dptr_frame);
               _intBufReleaseData(dptr, _AAX_SENSOR);
            }
         }
         else			/* subframe is registered at another frame */
         {
            _frame_t *handle = frame->parent;
            _intBufferData *dptr_frame;
            _intBuffers *hf;

            hf =  handle->submix->frames;
            if (lock) {
               dptr_frame = _intBufGet(hf, _AAX_FRAME, frame->mixer_pos);
            } else {
               dptr_frame = _intBufGetNoLock(hf, _AAX_FRAME, frame->mixer_pos);
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
      if (frame->parent)
      {
         if (frame->parent == frame->root)
         {
            _handle_t *handle = frame->root;
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
         else
         {
            _frame_t *handle = frame->parent;
            _intBuffers *hf;

            hf =  handle->submix->frames;
            _intBufRelease(hf, _AAX_FRAME, frame->mixer_pos);
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
#ifdef ARCH32
      mtx4fMul(&fdp3d_m->matrix, &pdp3d_m->matrix, &fdp3d->matrix);
      dist = vec3fMagnitude(&fdp3d_m->matrix.v34[LOCATION]);
#else
      mtx4dMul(&fdp3d_m->matrix, &pdp3d_m->matrix, &fdp3d->matrix);
      dist = vec3dMagnitude(&fdp3d_m->matrix.v34[LOCATION]);
#endif
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
   _aax3dProps *fp3d;
   int rv = AAX_FALSE;

   assert(frame);

   fp3d = frame->submix->props3d;
   if (_IS_INITIAL(fp3d) || _IS_PROCESSED(fp3d))
   {
      frame->submix->capturing = AAX_TRUE;
      rv = AAX_TRUE;
   }
   else if _IS_STANDBY(fp3d) {
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
   _frame_t* handle = get_frame(frame, _NOLOCK, __func__);
   aaxConfig config = handle->root;
   int rv = AAX_TRUE;
   void *xid;

   xid = xmlInitBuffer(aaxs, strlen(aaxs));
   if (xid)
   {
      void *xmid = xmlNodeGet(xid, "aeonwave/info");
      float freq = 0.0f;

      if (xmid)
      {
         void *xnid = xmlNodeGet(xmid, "note");
         if (xnid)
         {
            if (xmlAttributeExists(xnid, "polyphony"))
            {
               unsigned int min = get_low_resource() ? 8 : 12;
               unsigned int max = get_low_resource() ? 24 : 88;
               handle->max_emitters = xmlAttributeGetInt(xnid, "polyphony");
               handle->max_emitters = _MINMAX(handle->max_emitters, min, max);
            }
            xmlFree(xnid);
         }
         xmlFree(xmid);
      }

      xmid = xmlNodeGet(xid, "aeonwave/sound");
      if (xmid)
      {
         freq = xmlAttributeGetDouble(xmid, "frequency");
         xmlFree(xmid);
      }

      xmid = xmlNodeGet(xid, "aeonwave/audioframe");
      if (xmid)
      {
         int clear = xmlAttributeCompareString(xmid, "mode", "append");
         unsigned int i, num = xmlNodeGetNum(xmid, "filter");
         void *xeid, *xfid = xmlMarkId(xmid);

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

                aaxAudioFrameGetMatrix64(frame, mtx641);
                aaxMatrix64GetOrientation(mtx641, pos, at, up);

                aaxMatrix64SetIdentityMatrix(mtx641);
                aaxMatrix64SetOrientation(mtx641, pos, _at, up);
                
                aaxMatrix64SetIdentityMatrix(mtx642);
                aaxMatrix64Rotate(mtx642, -1.57*pan, 0.0, 1.0, 0.0);
                
                aaxMatrix64Multiply(mtx642, mtx641);
                aaxAudioFrameSetMatrix64(frame, mtx642);
                handle->mtx_set = AAX_TRUE;
            }
         }

         if (xmlAttributeExists(xmid, "max-emitters"))
         {
            unsigned int max = get_low_resource() ? 32 : 256;
            handle->max_emitters = xmlAttributeGetInt(xmid, "max-emitters");
            handle->max_emitters = _MINMAX(handle->max_emitters, 1, max);
         }

         if (clear)
         {
            _aaxAudioFrame* fmixer = handle->submix;
            _aaxSetDefault2dFiltersEffects(fmixer->props2d);
         }

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
                  aaxFilter flt = _aaxGetFilterFromAAXS(config, xfid, freq, 0.0f, 0.0f, NULL);
                  if (flt)
                  {
                     aaxAudioFrameSetFilter(frame, flt);
                     aaxFilterDestroy(flt);
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
            {int non_optional = AAX_TRUE;
               if (xmlAttributeExists(xeid, "optional")) {
                  non_optional = !xmlAttributeGetBool(xeid, "optional");
               }
               if (non_optional || !get_low_resource())
               {
                  aaxEffect eff = _aaxGetEffectFromAAXS(config, xeid, freq, 0.0f, 0.0f, NULL);
                  if (eff)
                  {
                     aaxAudioFrameSetEffect(frame, eff);
                     aaxEffectDestroy(eff);
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
