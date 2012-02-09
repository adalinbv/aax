/*
 * Copyright 2007-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
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

#ifdef HAVE_LIBIO_H
#include <libio.h>              /* for NULL */
#endif
#include <string.h>		/* for calloc */
#include <assert.h>
#include <math.h>		/* for ceif */

#include <sys/time.h> 
#include <signal.h>
#ifdef SOLARIS /* needed with at least Solaris 8 */
# include <siginfo.h>
#endif


#include <arch.h>
#include <ringbuffer.h>

#include <base/gmath.h>
#include <base/threads.h>

#include "api.h"

static int _aaxMixerInit(_handle_t*);
static int _aaxMixerStart(_handle_t*);
static int _aaxMixerStop(_handle_t*);
static int _aaxMixerSignal(_handle_t*);

int
aaxMixerSetSetup(aaxConfig config, enum aaxSetupType type, unsigned int setup)
{
   int rv = AAX_FALSE;

   if (config == NULL)
   {
      switch(type)
      {
      case AAX_STEREO_EMITTERS:
         setup *= 2;
         /* break not needed */
      case AAX_MONO_EMITTERS:
         rv = (setup <= _oalRingBufferGetNoSources()) ? AAX_TRUE : AAX_FALSE;
         break;
      default:
         break;
      }
   }
   else
   {
      _handle_t *handle = get_handle(config);
      if (handle)
      {
         _aaxMixerInfo* info = handle->info;
         switch(type)
         {
         case AAX_STEREO_EMITTERS:
            setup *= 2;
            /* break not needed */
         case AAX_MONO_EMITTERS:
            rv = (setup <= info->max_emitters) ? AAX_TRUE : AAX_FALSE;
            break;
         case AAX_FREQUENCY:
            if ((setup > 1000) && (setup < 96000))
            {
               float iv = info->refresh_rate;
               iv = setup / (float)get_pow2((unsigned int)ceilf(setup / iv));
               info->frequency = (float)setup;
               info->refresh_rate = iv;
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_REFRESHRATE:
            if ((setup > 5) && (setup < 200))
            {
               float fq = info->frequency;
               float iv = (float)setup;
               if (iv <= 5.0f) iv = 5.0f;

               iv = fq / (float)get_pow2((unsigned)ceilf(fq / iv));
               info->refresh_rate = iv;
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_TRACKSIZE:
            {
               float fq = info->frequency;
               float iv = fq*sizeof(int32_t)/setup;
               if (iv <= 5.0f) iv = 5.0f;

               iv = fq / (float)get_pow2((unsigned)ceilf(fq / iv));
               info->refresh_rate = iv;
               rv = AAX_TRUE;
            }
            break;
         case AAX_TRACKS:
            if (setup < _AAX_MAX_SPEAKERS)
            {
               info->no_tracks = setup;
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_FORMAT:
            if (setup < AAX_FORMAT_MAX)
            {
               info->format = setup;
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         default:
            _aaxErrorSet(AAX_INVALID_ENUM);
            break;
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   return rv;
}

unsigned int
aaxMixerGetSetup(const aaxConfig config, enum aaxSetupType type)
{
   unsigned int rv = AAX_FALSE;

   if (config == NULL)
   {
      switch(type)
      {
      case AAX_MONO_EMITTERS:
         rv = _oalRingBufferGetNoSources();
         break;
      case AAX_STEREO_EMITTERS:
         rv = _oalRingBufferGetNoSources()/2;
         break;
      case AAX_AUDIO_FRAMES:
         rv = 0;
         break;
      default:
         break;
      }
   }
   else
   {
      _handle_t *handle = get_handle(config);
      if (handle && (type < AAX_SETUP_TYPE_MAX))
      {
         _aaxMixerInfo* info = handle->info;
         switch(type)
         {
         case AAX_MONO_EMITTERS:
            rv = info->max_emitters;
            break;
         case AAX_STEREO_EMITTERS:
            rv = info->max_emitters/2;
            break;
         case AAX_AUDIO_FRAMES:
            rv = VALID_HANDLE(handle) ? 256 : 0;
            break;
         case AAX_FREQUENCY:
            rv = info->frequency;
            break;
         case AAX_REFRESHRATE:
            rv = info->refresh_rate;
            break;     
         case AAX_TRACKSIZE:
            rv = info->frequency*sizeof(int32_t)/info->refresh_rate;
            break;
         case AAX_TRACKS:
            rv = info->no_tracks;
            break;
         case AAX_FORMAT:
            rv = info->format;
            break;
         default:
            _aaxErrorSet(AAX_INVALID_ENUM);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   return rv;
}

int
aaxMixerSetFilter(aaxConfig config, aaxFilter f)
{
   _handle_t* handle = get_handle(config);
   int rv = AAX_FALSE;
   if (handle)
   {
      _filter_t* filter = get_filter(f);
      if (filter)
      {
         const _intBufferData* dptr;
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            int type = filter->pos;
            switch (filter->type)
            {
            case AAX_VOLUME_FILTER:
            {
               _aaxAudioFrame* mixer = sensor->mixer;
               _oalRingBuffer2dProps *p2d = mixer->props2d;
               _FILTER_SET(p2d, type, 0, _FILTER_GET_SLOT(filter, 0, 0));
                   /* gain min and gain max are fixed values for the mixer   */
               /* _FILTER_SET(p2d, type, 1, _FILTER_GET_SLOT(filter, 0, 1)); */
               /* _FILTER_SET(p2d, type, 2, _FILTER_GET_SLOT(filter, 0, 2)); */
               _FILTER_SET_STATE(p2d, type, _FILTER_GET_SLOT_STATE(filter));
               _FILTER_SWAP_SLOT_DATA(p2d, type, filter, 0);
               rv = AAX_TRUE;
               break;
            }
            case AAX_DYNAMIC_GAIN_FILTER:
            case AAX_TIMED_GAIN_FILTER:
            {
               _aaxAudioFrame* mixer = sensor->mixer;
               _oalRingBuffer2dProps *p2d = mixer->props2d;
               _FILTER_SET(p2d, type, 0, _FILTER_GET_SLOT(filter, 0, 0));
               _FILTER_SET(p2d, type, 1, _FILTER_GET_SLOT(filter, 0, 1));
               _FILTER_SET(p2d, type, 2, _FILTER_GET_SLOT(filter, 0, 2));
               _FILTER_SET_STATE(p2d, type, _FILTER_GET_SLOT_STATE(filter));
               _FILTER_SWAP_SLOT_DATA(p2d, type, filter, 0);
               rv = AAX_TRUE;
               break;
            }
            case AAX_EQUALIZER:
            case AAX_GRAPHIC_EQUALIZER:
               type = EQUALIZER_HF;
               _FILTER_SET(sensor, type, 0, _FILTER_GET_SLOT(filter, 1, 0));
               _FILTER_SET(sensor, type, 1, _FILTER_GET_SLOT(filter, 1, 1));
               _FILTER_SET(sensor, type, 2, _FILTER_GET_SLOT(filter, 1, 2));
               _FILTER_SWAP_SLOT_DATA(sensor, type, filter, 1);

               type = EQUALIZER_LF;
               /* frees both EQUALIZER_LF and EQUALIZER_HF */
               _FILTER_SET(sensor, type, 0, _FILTER_GET_SLOT(filter, 0, 0));
               _FILTER_SET(sensor, type, 1, _FILTER_GET_SLOT(filter, 0, 1));
               _FILTER_SET(sensor, type, 2, _FILTER_GET_SLOT(filter, 0, 2));
               _FILTER_SET_STATE(sensor, type, _FILTER_GET_SLOT_STATE(filter));
               _FILTER_SWAP_SLOT_DATA(sensor, type, filter, 0);
               rv = AAX_TRUE;
               break;
            default:
               _aaxErrorSet(AAX_INVALID_ENUM);
            }
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

const aaxFilter
aaxMixerGetFilter(const aaxConfig config, enum aaxFilterType type)
{
   _handle_t* handle = get_handle(config);
   aaxFilter rv = AAX_FALSE;
   if (handle)
   {
      const _intBufferData* dptr;
      switch(type)
      {
      case AAX_VOLUME_FILTER:
      case AAX_TIMED_GAIN_FILTER:
      case AAX_DYNAMIC_GAIN_FILTER:
      case AAX_EQUALIZER:
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = sensor->mixer;
            rv = new_filter_handle(handle->info, type, mixer->props2d,
                                                       mixer->props3d);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}


int
aaxMixerSetEffect(aaxConfig config, aaxEffect e)
{
   _handle_t* handle = get_handle(config);
   int rv = AAX_FALSE;
   if (handle)
   {
      _filter_t* effect = get_effect(e);
      if (effect)
      {
         const _intBufferData* dptr;
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = sensor->mixer;
            int type = effect->pos;
            switch (effect->type)
            {
            case AAX_PITCH_EFFECT:
            case AAX_DYNAMIC_PITCH_EFFECT:
            case AAX_DISTORTION_EFFECT:
            case AAX_PHASING_EFFECT:
            case AAX_CHORUS_EFFECT:
            case AAX_FLANGING_EFFECT:
            {
               _oalRingBuffer2dProps *p2d = mixer->props2d;
               _EFFECT_SET(p2d, type, 0, _EFFECT_GET_SLOT(effect, 0, 0));
               _EFFECT_SET(p2d, type, 1, _EFFECT_GET_SLOT(effect, 0, 1));
               _EFFECT_SET(p2d, type, 2, _EFFECT_GET_SLOT(effect, 0, 2));
               _EFFECT_SET_STATE(p2d, type, _EFFECT_GET_SLOT_STATE(effect));
               _EFFECT_SWAP_SLOT_DATA(p2d, type, effect, 0);
               rv = AAX_TRUE;
               break;
            }
            default:
               _aaxErrorSet(AAX_INVALID_ENUM);
            }
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

const aaxEffect
aaxMixerGetEffect(const aaxConfig config, enum aaxEffectType type)
{
   _handle_t* handle = get_handle(config);
   aaxEffect rv = AAX_FALSE;
   if (handle)
   {
      const _intBufferData* dptr;
      switch(type)
      {
      case AAX_DYNAMIC_PITCH_EFFECT:
      case AAX_DISTORTION_EFFECT:
      case AAX_PHASING_EFFECT:
      case AAX_CHORUS_EFFECT:
//    case AAX_FLANGING_EFFECT:
      case AAX_PITCH_EFFECT:
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = sensor->mixer;
            rv = new_effect_handle(handle->info, type, mixer->props2d,
                                                       mixer->props3d);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

int
aaxMixerSetState(aaxConfig config, enum aaxState state)
{
   int rv = AAX_FALSE;
   _handle_t* handle;

   if (state == AAX_INITIALIZED) {
      handle = get_handle(config);
   } else {
      handle = get_valid_handle(config);
   }

   if (handle)
   {
      switch (state)
      {
      case AAX_UPDATE:
         rv = _aaxMixerSignal(handle);
         break;
      case AAX_SUSPENDED:
         _SET_PAUSED(handle);
         rv = AAX_TRUE;
         break;
      case AAX_STANDBY:
         _SET_STANDBY(handle);
         rv = AAX_TRUE;
         break;
      case AAX_INITIALIZED:
         rv = _aaxMixerInit(handle);
         break;
      case AAX_PLAYING:
         rv = _aaxMixerStart(handle);
         if (rv) _SET_PLAYING(handle);
         break;
      case AAX_STOPPED:
         rv = _aaxMixerStop(handle);
         if (rv) _SET_PROCESSED(handle);
         break;
      default:
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

int
aaxMixerRegisterEmitter(const aaxConfig config, const aaxEmitter em)
{
   _handle_t* handle = get_handle(config);
   int rv = AAX_FALSE;
   if (handle)
   {
      _emitter_t* emitter = get_emitter_unregistered(em);
      if (emitter && emitter->pos == UINT_MAX)
      {
         _aaxEmitter *src = emitter->source;
         unsigned int pos, positional;
         _intBufferData *dptr;

         positional = _IS_POSITIONAL(src);

         pos = UINT_MAX;
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = sensor->mixer;
            _intBuffers *he;

            if (!positional) {
               he = mixer->emitters_2d;
            } else {
               he = mixer->emitters_3d;
            }

            if ((mixer->no_emitters+1) <= mixer->info->max_emitters)
            {
               mixer->no_emitters++;
               pos = _intBufAddData(he, _AAX_EMITTER, emitter);
            }
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }

         if (pos != UINT_MAX)
         {
            _aaxEmitter *src = emitter->source;
            _intBufferData *dptr;

            emitter->handle = handle;
            emitter->pos = pos;

            /*
             * The mixer frequency is not know by the frequency filter
             * (which needs it to properly compute the filter coefficients)
             * until it has been registered to the mixer.
             * So we need to update the filter coefficients one more time.
             */
            if (_FILTER_GET2D_DATA(emitter->source, FREQUENCY_FILTER)) 
            {
               aaxFilter f = aaxEmitterGetFilter(emitter, AAX_FREQUENCY_FILTER);
               _filter_t *filter = (_filter_t *)f;
               aaxFilterSetState(f, filter->slot[0]->state);
               aaxEmitterSetFilter(emitter, f);
            }

            src->info = handle->info;
            if (src->update_rate == 0) {
               src->update_rate = handle->info->update_rate;
            }
            src->update_ctr = 0;

            dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _oalRingBuffer3dProps *mp3d, *ep3d = src->props3d;
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               _aaxAudioFrame* mixer = sensor->mixer;

               if (mixer->ringbuffer) {
                  src->props2d->delay_sec
                                  =_oalRingBufferGetDuration(mixer->ringbuffer);
               }

               if (positional)
               {
                  mp3d = mixer->props3d;
                  if (mixer->dist_delaying) {
                     _PROP_DISTDELAY_SET_DEFINED(src->props3d);
                  }

                  if (_FILTER_GET_DATA(ep3d, DISTANCE_FILTER) == NULL) {
                     _FILTER_COPY_DATA(ep3d, mp3d, DISTANCE_FILTER);
                  }

                  if (_EFFECT_GET_DATA(ep3d, VELOCITY_EFFECT) == NULL)
                  {
                     _EFFECT_COPY(ep3d,mp3d,VELOCITY_EFFECT,AAX_SOUND_VELOCITY);
                     _EFFECT_COPY(ep3d,mp3d,VELOCITY_EFFECT,AAX_DOPPLER_FACTOR);
                     _EFFECT_COPY_DATA(ep3d, mp3d, VELOCITY_EFFECT);
                  }
               }
               _intBufReleaseData(dptr, _AAX_SENSOR);
            }
            rv = AAX_TRUE;
         }
         else {
            _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

int
aaxMixerDeregisterEmitter(const aaxConfig config, const aaxEmitter em)
{
   _handle_t* handle = get_handle(config);
   int rv = AAX_FALSE;
   if (handle)
   {
      _emitter_t* emitter = get_emitter(em);
      if (emitter && emitter->pos != UINT_MAX)
      {
         _aaxEmitter *src = emitter->source;
         _intBufferData *dptr;

         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = sensor->mixer;
            _intBuffers *he;
            void *ptr;

            if (_IS_POSITIONAL(src))
            {
               he = mixer->emitters_3d;
               _PROP_DISTDELAY_CLEAR_DEFINED(src->props3d);
            }
            else {
               he = mixer->emitters_2d;
            }

            ptr = _intBufRemove(he, _AAX_EMITTER, emitter->pos, AAX_FALSE);
            if (ptr)
            {
               mixer->no_emitters--;
               emitter->handle = NULL;
               emitter->pos = UINT_MAX;
               rv = AAX_TRUE;
            }
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
      put_emitter(emitter);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

int
aaxMixerRegisterAudioFrame(const aaxConfig config, const aaxFrame f)
{
   _handle_t* handle = get_handle(config);
   int rv = AAX_FALSE;
   if (handle && VALID_HANDLE(handle))
   {
      _frame_t* frame = get_frame(f);
      if (frame && !frame->handle && !frame->thread.started)
      {
         if (frame->pos == UINT_MAX)
         {
            unsigned int pos = UINT_MAX;
            _intBufferData *dptr;

            dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               _aaxAudioFrame* mixer = sensor->mixer;
               _intBuffers *hf = mixer->frames;

               if (hf == NULL)
               {
                  unsigned int res = _intBufCreate(&mixer->frames, _AAX_FRAME);
                  if (res != UINT_MAX) {
                     hf = mixer->frames;
                  }
               }

               if (hf)
               {
                  aaxBuffer buf; /* clear the frames buffer queue */
                  while ((buf = aaxAudioFrameGetBuffer(frame)) != NULL) {
                     aaxBufferDestroy(buf);
                  }
                  pos = _intBufAddData(hf, _AAX_FRAME, frame);
               }
               else {
                  _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
               }
               _intBufReleaseData(dptr, _AAX_SENSOR);
            }
            
            if (dptr && pos != UINT_MAX)
            {
               _oalRingBuffer3dProps *mp3d, *fp3d;
               _aaxAudioFrame* mixer, *submix;
               _sensor_t* sensor;

               dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
               sensor = _intBufGetDataPtr(dptr);
               mixer = sensor->mixer;
               submix = frame->submix;

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
               _intBufReleaseData(dptr, _AAX_SENSOR);
               rv = AAX_TRUE;

               frame->submix->refcount++;
               frame->submix->thread = AAX_TRUE;
               frame->handle = handle;
               frame->pos = pos;
            }
            else {
               _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            }
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }

         /* No need to put the frame since it was not registered yet.
          * This means there is no lock to unlock 
          * put_frame(frame);
          */
      }
      else
      {
         if (frame->handle) put_frame(frame);
         _aaxErrorSet(AAX_INVALID_STATE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

int
aaxMixerDeregisterAudioFrame(const aaxConfig config, const aaxFrame f)
{
   _handle_t* handle = get_handle(config);
   int rv = AAX_FALSE;
   if (handle)
   {
      _frame_t* frame = get_frame(f);
      if (frame && frame->pos != UINT_MAX)
      {
         _intBufferData *dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = sensor->mixer;
            _intBuffers *hf = mixer->frames;
            _intBufRemove(hf, _AAX_FRAME, frame->pos, AAX_FALSE);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }

         frame->submix->refcount--;
         frame->submix->thread = AAX_FALSE;
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


/* -------------------------------------------------------------------------- */

static int
_aaxMixerInit(_handle_t *handle)
{
   int res = AAX_FALSE;
   const _aaxDriverBackend *be = handle->backend.ptr;
   _aaxMixerInfo* info = handle->info;
   float refrate = info->refresh_rate;
   enum aaxFormat fmt = info->format;			/* playback format */
   unsigned ch = info->no_tracks;
   float freq = info->frequency;
   size_t bufsz = 0;

   assert(be != 0);

   if (bufsz == 0)
   {
      float no_samples = freq / refrate;
      bufsz = ch * no_samples * _oalRingBufferFormatsBPS[fmt];
   }

   res = be->setup(handle->backend.handle, &bufsz, fmt, &ch, &freq);
   if TEST_FOR_TRUE(res)
   {
      float iv;

      handle->valid |= AAX_TRUE;
      info->pitch = info->frequency/freq;
      info->no_tracks = ch;

      iv = info->refresh_rate;
      iv = freq / (float)get_pow2((unsigned int)ceilf(freq / iv));
      info->frequency = freq;
      info->refresh_rate = iv;
   }
   else {
      __aaxErrorSet(AAX_INVALID_SETUP, "aaxMixerSetState");
   }
   return res;
}

static int
_aaxMixerStart(_handle_t *handle)
{
   int rv = AAX_FALSE;

   if (handle && TEST_FOR_FALSE(handle->thread.started))
   {
      int r;

      handle->thread.update = 1;
      handle->thread.ptr = _aaxThreadCreate();
      assert(handle->thread.ptr != 0);

      handle->thread.condition = _aaxConditionCreate();
      assert(handle->thread.condition != 0);

      handle->thread.mutex = _aaxMutexCreate(0);
      assert(handle->thread.mutex != 0);

      handle->thread.started = AAX_TRUE;
      r = _aaxThreadStart(handle->thread.ptr, handle->backend.ptr->thread, handle);
      if (r == 0)
      {
         int p = 0;
         do
         {
            static const struct timespec sleept = {0, 100000};
            _intBufferData *dptr_sensor;

            nanosleep(&sleept, 0);

            dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr_sensor)
            {
               _sensor_t* sensor = _intBufGetDataPtr(dptr_sensor);
               r = (sensor->mixer->ringbuffer != 0);
               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
            }

            if (p++ > 500) break;
         }
         while (r == 0);

         if (r == 0)
         {
            _aaxErrorSet(AAX_TIMEOUT);
            handle->thread.started = AAX_FALSE;
         }
         else {
            rv = AAX_TRUE;
         }
      }   
      else {
         _aaxErrorSet(AAX_INVALID_STATE);
      }
   }
   else if (_IS_STANDBY(handle)) {
      rv = AAX_TRUE;
   }
   return rv;
}

static int
_aaxMixerStop(_handle_t *handle)
{
   int rv = AAX_FALSE;
   if TEST_FOR_TRUE(handle->thread.started)
   {
      handle->thread.started = AAX_FALSE;
      _aaxConditionSignal(handle->thread.condition);
      _aaxThreadJoin(handle->thread.ptr);

      _aaxConditionDestroy(handle->thread.condition);
      _aaxMutexDestroy(handle->thread.mutex);
      _aaxThreadDestroy(handle->thread.ptr);

      rv = AAX_TRUE;
   }

   return rv;
}

static int
_aaxMixerSignal(_handle_t *handle)
{
   int rv = AAX_FALSE;
   if TEST_FOR_TRUE(handle->thread.started)
   {
      handle->thread.update = 1;
      _aaxConditionSignal(handle->thread.condition);
      rv = AAX_TRUE;
   }

   return rv;
}


