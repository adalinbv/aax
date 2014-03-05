/*
 * Copyright 2007-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
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

#include <assert.h>
#include <math.h>		/* for INFINITY */
#include <errno.h>		/* for ETIMEDOUT */

#include <base/threads.h>
#include <base/gmath.h>
#include <base/timer.h>		/* for msecSleep */

#include "api.h"
#include "devices.h"

static int _aaxSensorCreateRingBuffer(_handle_t *);
static int _aaxSensorCaptureStart(_handle_t *);
static int _aaxSensorCaptureStop(_handle_t *);

AAX_API int AAX_APIENTRY
aaxSensorSetMatrix(aaxConfig config, aaxMtx4f mtx)
{
   int rv = AAX_FALSE;
   if (mtx && !detect_nan_vec4(mtx[0]) && !detect_nan_vec4(mtx[1]) &&
              !detect_nan_vec4(mtx[2]) && !detect_nan_vec4(mtx[3]))
   {
      _handle_t *handle = get_handle(config);
      if (handle)
      {
         const _intBufferData* dptr;
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* smixer = sensor->mixer;
            mtx4Copy(smixer->props3d->dprops3d->matrix, mtx);
            mtx4Copy(smixer->props3d->m_dprops3d->matrix, mtx);
            _PROP_MTX_SET_CHANGED(smixer->props3d);
            _intBufReleaseData(dptr, _AAX_SENSOR);
            rv = AAX_TRUE;
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxSensorGetMatrix(const aaxConfig config, aaxMtx4f mtx)
{
   int rv = AAX_FALSE;
   if (mtx)
   {
      _handle_t *handle = get_handle(config);
      if (handle)
      {
         const _intBufferData* dptr;
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* smixer = sensor->mixer;
             mtx4Copy(mtx, smixer->props3d->dprops3d->matrix);
             _PROP_MTX_SET_CHANGED(smixer->props3d);
            _intBufReleaseData(dptr, _AAX_SENSOR);
            rv = AAX_TRUE;
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxSensorSetVelocity(aaxConfig config, const aaxVec3f velocity)
{
   int rv = AAX_FALSE;
   if (velocity && !detect_nan_vec3(velocity))
   {
      _handle_t *handle = get_handle(config);
      if (handle)
      {
         const _intBufferData* dptr;
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            mtx4_t mtx;
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxDelayed3dProps *dp3d;

            mtx4Copy(mtx, aaxIdentityMatrix);
            vec3Copy(mtx[VELOCITY], velocity);
            mtx[VELOCITY][3] = 0.0f;

            dp3d = sensor->mixer->props3d->dprops3d;
            mtx4InverseSimple(dp3d->velocity, mtx);
            _PROP_SPEED_SET_CHANGED(sensor->mixer->props3d);
            _intBufReleaseData(dptr, _AAX_SENSOR);
            rv = AAX_TRUE;
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxSensorGetVelocity(const aaxConfig config, aaxVec3f velocity)
{
   int rv = AAX_FALSE;
   if (velocity)
   {
      _handle_t *handle = get_handle(config);
      if (handle)
      {
         const _intBufferData* dptr;
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            mtx4_t mtx;
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxDelayed3dProps *dp3d;

            dp3d = sensor->mixer->props3d->dprops3d;
            mtx4InverseSimple(mtx, dp3d->velocity);
            _intBufReleaseData(dptr, _AAX_SENSOR);

            vec3Copy(velocity, mtx[VELOCITY]);
            rv = AAX_TRUE;
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return rv;
}

AAX_API unsigned long AAX_APIENTRY
aaxSensorGetOffset(const aaxConfig config, enum aaxType type)
{
   _handle_t *handle = get_handle(config);
   unsigned long rv = 0;

   if (handle)
   {
      const _intBufferData* dptr;
      dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
      if (dptr)
      {
         _sensor_t* sensor = _intBufGetDataPtr(dptr);
         _aaxAudioFrame* smixer = sensor->mixer;
         const _intBufferData* dptr_rb;

         dptr_rb = _intBufGet(smixer->play_ringbuffers, _AAX_RINGBUFFER, 0);
         if (dptr_rb)
         {
            _aaxRingBuffer *rb = _intBufGetDataPtr(dptr_rb);
            switch (type)
            {
            case AAX_FRAMES:
            case AAX_SAMPLES:
               rv = _intBufGetNumNoLock(smixer->play_ringbuffers, _AAX_RINGBUFFER);
               rv *= rb->get_parami(rb, RB_NO_SAMPLES);
               rv += rb->get_parami(rb, RB_OFFSET_SAMPLES);
               break;
            case AAX_BYTES:
            {
               rv = _intBufGetNumNoLock(smixer->play_ringbuffers, _AAX_RINGBUFFER);
               rv *= rb->get_parami(rb, RB_NO_SAMPLES);
               rv += rb->get_parami(rb, RB_OFFSET_SAMPLES);
               rv *= rb->get_parami(rb, RB_BYTES_SAMPLE);
               break;
            }
            case AAX_MICROSECONDS:
               rv = (unsigned long)(sensor->mixer->curr_pos_sec*1e6f);
               break;
            default:
               _aaxErrorSet(AAX_INVALID_ENUM);
            }
            _intBufReleaseData(dptr_rb, _AAX_RINGBUFFER);
         }
         _intBufReleaseData(dptr, _AAX_SENSOR);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API aaxBuffer AAX_APIENTRY
aaxSensorGetBuffer(const aaxConfig config)
{
   _handle_t *handle = get_handle(config);
   aaxBuffer buffer = NULL;

   if (handle)
   {
      const _intBufferData* dptr;
      dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
      if (dptr)
      {
         _sensor_t* sensor = _intBufGetDataPtr(dptr);
         _aaxAudioFrame* smixer = sensor->mixer;
         _intBuffers *dptr_rb = smixer->play_ringbuffers;
         _intBufferData *rbuf;

         rbuf = _intBufPop(dptr_rb, _AAX_RINGBUFFER);
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
         _intBufReleaseData(dptr, _AAX_SENSOR);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return buffer;
}

AAX_API int AAX_APIENTRY
aaxSensorWaitForBuffer(aaxConfig config, float timeout)
{
   _handle_t *handle = get_handle(config);
   int rv = AAX_FALSE;
   if (handle)
   {
      float refrate = handle->info->refresh_rate;
      const _intBufferData* dptr;
      float duration = 0.0f;
      unsigned int sleep_ms;
      unsigned int nbuf;

      sleep_ms = (unsigned int)_MAX(1000/(10.0f*refrate), 1);
      do
      {
         nbuf = 0;
         duration += sleep_ms*0.001f;	// ms to sec.
         if (duration >= timeout) break;

         handle = get_handle(config);	/* handle could be invalid by now */
         if (!handle) break;

         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _intBuffers *ringbuffers = sensor->mixer->play_ringbuffers;
            nbuf = _intBufGetNumNoLock(ringbuffers, _AAX_RINGBUFFER);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }

         if (!nbuf)
         {
            int err = msecSleep(sleep_ms);
            if (err < 0) break;
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

AAX_API int AAX_APIENTRY
aaxSensorSetState(aaxConfig config, enum aaxState state)
{
   _handle_t *handle = get_valid_handle(config);
   int rv = AAX_FALSE;
   if (handle)
   {
      switch(state)
      {
      case AAX_CAPTURING:
         if ((handle->info->mode == AAX_MODE_READ) && !handle->handle) {
            rv = _aaxSensorCaptureStart(handle);
         }
         else if (handle->handle)	/* registered sensor */
         {
            _aaxSensorCreateRingBuffer(handle);
            _SET_PLAYING(handle);
            rv = AAX_TRUE;
         }
         else				/* capture buffer on playback */
         {
            const _intBufferData* dptr;
            dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               sensor->mixer->capturing = AAX_TRUE;
               _intBufReleaseData(dptr, _AAX_SENSOR);
               rv = AAX_TRUE;
            }
            else {
               _aaxErrorSet(AAX_INVALID_STATE);
            }
         }
         break;
//    case AAX_PROCESSED:
      case AAX_STOPPED:
         if ((handle->info->mode == AAX_MODE_READ) && !handle->handle) {
            rv = _aaxSensorCaptureStop(handle);
         }
         else if (handle->handle)	/* registered sensor */
         {
            _aaxSensorCreateRingBuffer(handle);
            _SET_STOPPED(handle);
            rv = AAX_TRUE;
         }
         else				/* capture buffer on playback */
         {
            const _intBufferData* dptr;
            dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               sensor->mixer->capturing = AAX_FALSE;
               _intBufReleaseData(dptr, _AAX_SENSOR);
               rv = AAX_TRUE;
            }
            else {
               _aaxErrorSet(AAX_INVALID_STATE);
            }
         }
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

/* -------------------------------------------------------------------------- */

/* capturing only */
int
_aaxSensorCreateRingBuffer(_handle_t *handle)
{
   _intBufferData *dptr;
   int rv = AAX_FALSE;

   assert(handle);

   if ((handle->info->mode != AAX_MODE_READ) ||
       (handle->thread.started != AAX_FALSE)) {
      return rv;
   }

   dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
   if (dptr)
   {
      _sensor_t *sensor = _intBufGetDataPtr(dptr);
      _aaxAudioFrame *submix = sensor->mixer;
      _aaxMixerInfo* info = submix->info;
      _aaxRingBuffer *rb;

      if (!submix->ringbuffer)
      {
         const _aaxDriverBackend *be = handle->backend.ptr;
         enum aaxRenderMode mode = info->mode;
         float dt = DELAY_EFFECTS_TIME;

         submix->ringbuffer = be->get_ringbuffer(dt, mode);
      }

      rb = submix->ringbuffer;
      if (rb)
      {
         float delay_sec = 1.0f / info->refresh_rate;
         const _aaxDriverBackend *be;
         float min, max;

         rb->set_format(rb, AAX_PCM24S, AAX_TRUE);
         rb->set_parami(rb, RB_NO_TRACKS, info->no_tracks);

         be = handle->backend.ptr;
         min = be->param(handle->backend.handle, DRIVER_MIN_VOLUME);
         max = be->param(handle->backend.handle, DRIVER_MAX_VOLUME);

         rb->set_paramf(rb, RB_VOLUME_MIN, min);
         rb->set_paramf(rb, RB_VOLUME_MAX, max);

         /* Do not alter the frequency at this time, it has been set by
          * aaxMixerRegisterSensor and may have changed in the mean time
*/       rb->set_paramf(rb, RB_FREQUENCY, info->frequency);
/*        */

         /* create a ringbuffer with a but of overrun space */
         rb->set_paramf(rb, RB_DURATION_SEC, delay_sec*1.0f);
         rb->init(rb, AAX_TRUE);

         /* 
          * Now set the actual duration, this will not alter the allocated
          * space since it is lower that the initial duration.
          */
         rb->set_paramf(rb, RB_DURATION_SEC, delay_sec);
         rb->set_state(rb, RB_STARTED);
      }
      _intBufReleaseData(dptr, _AAX_SENSOR);
   }

   return rv;
}

/* capturing only */
int
_aaxSensorCaptureStart(_handle_t *handle)
{
   int rv = AAX_FALSE;
   assert(handle);
   assert(handle->info->mode == AAX_MODE_READ);
   assert(handle->thread.started == AAX_FALSE);

   if (_IS_INITIAL(handle) || _IS_PROCESSED(handle))
   {
      _intBufferData *dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
      if (dptr)
      {
         _sensor_t *sensor = _intBufGetDataPtr(dptr);
         char thread = sensor->mixer->thread;
         _intBufReleaseData(dptr, _AAX_SENSOR);
         if (1) // thread)
         {
            const _aaxDriverBackend *be = handle->backend.ptr;
            if (be->thread)
            {
               unsigned int ms;
               int r;

               handle->thread.ptr = _aaxThreadCreate();
               assert(handle->thread.ptr != 0);

               handle->thread.condition = _aaxConditionCreate();
               assert(handle->thread.condition != 0);

               handle->thread.mutex = _aaxMutexCreate(handle->thread.mutex);
               assert(handle->thread.mutex != 0);

               handle->thread.started = AAX_TRUE;
               ms = rintf(1000/handle->info->refresh_rate);
#if 0
               r = _aaxThreadStart(handle->thread.ptr,
                                   handle->backend.ptr->thread, handle, ms);
#else
               r = _aaxThreadStart(handle->thread.ptr, _aaxSoftwareMixerThread,
                                    handle, ms);
#endif
               if (r == 0)
               {
                  int p = 0;
                  do
                  {
                     msecSleep(100);
                     dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
                     if (dptr)
                     {
                        sensor = _intBufGetDataPtr(dptr);
//                      r = (sensor->mixer->ringbuffer != 0);
                        r = (handle->ringbuffer != 0);
                        _intBufReleaseData(dptr, _AAX_SENSOR);
                     }
                     if (p++ > 500) break;
                  }
                  while (r == 0);

                  if (r == 0)
                  {
                     _aaxErrorSet(AAX_TIMEOUT);
                     handle->thread.started = AAX_FALSE;
                  }
                  else
                  {
                     dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
                     if (dptr)
                     {
                        sensor = _intBufGetDataPtr(dptr);
                        sensor->mixer->capturing = AAX_TRUE;
                        _intBufReleaseData(dptr, _AAX_SENSOR);
                     }

                     _SET_PLAYING(handle);
                     rv = AAX_TRUE;
                  }
               }
            }
            else {
               _aaxErrorSet(AAX_INVALID_STATE);
            }
         } /* sensor->mixer->thread */
         else {
            rv = AAX_TRUE;
         }
      }	
   }
   else if (_IS_STANDBY(handle)) {
      rv = AAX_TRUE;
   }
   return rv;
}

int
_aaxSensorCaptureStop(_handle_t *handle)
{
   int rv = AAX_FALSE;
   if TEST_FOR_TRUE(handle->thread.started)
   {
      if (handle->info->mode == AAX_MODE_READ)
      {
         const _intBufferData* dptr;

         handle->thread.started = AAX_FALSE;
         _aaxConditionSignal(handle->thread.condition);
         _aaxThreadJoin(handle->thread.ptr);

         _aaxConditionDestroy(handle->thread.condition);
         _aaxMutexDestroy(handle->thread.mutex);
         _aaxThreadDestroy(handle->thread.ptr);

         dptr = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t *sensor = _intBufGetDataPtr(dptr);
            sensor->mixer->capturing = AAX_FALSE;
/*          Note: _intBufGetNoLock above, no need to unlock
 *          _intBufReleaseData(dptr, _AAX_SENSOR);
 */
         }

         _SET_PROCESSED(handle);
         rv = AAX_TRUE;
      }
   }
   return rv;
}
