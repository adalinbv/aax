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

#ifdef HAVE_VALUES_H
#include <values.h>             /* for MAXFLOAT */
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <assert.h>
#include <math.h>		/* for INFINITY */
#include <errno.h>		/* for ETIMEDOUT */
#include <sys/time.h>		/* for struct timeval */

#include <base/threads.h>
#include <base/gmath.h>

#include "api.h"

static int _aaxSensorCreateRingBuffer(_handle_t *);
static int _aaxSensorCaptureStart(_handle_t *);
static int _aaxSensorCaptureStop(_handle_t *);

int
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
            _aaxAudioFrame* mixer = sensor->mixer;
            mtx4Copy(mixer->props3d->matrix, mtx);
            _PROP_MTX_SET_CHANGED(mixer->props3d);
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

int
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
            _aaxAudioFrame* mixer = sensor->mixer;
             mtx4Copy(mtx, mixer->props3d->matrix);
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

int
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
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = sensor->mixer;
            vec3Copy(mixer->props3d->velocity, velocity);
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

int
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
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = sensor->mixer;
            vec3Copy(velocity, mixer->props3d->velocity);
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

unsigned long
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
         _aaxAudioFrame* mixer = sensor->mixer;
         const _intBufferData* dptr_rb;

         dptr_rb = _intBufGet(mixer->ringbuffers, _AAX_RINGBUFFER, 0);
         if (dptr_rb)
         {
            _oalRingBuffer *rb = _intBufGetDataPtr(dptr_rb);
            switch (type)
            {
            case AAX_FRAMES:
            case AAX_SAMPLES:
               rv = _intBufGetNumNoLock(mixer->ringbuffers, _AAX_RINGBUFFER);
               rv *= _oalRingBufferGetNoSamples(rb);
               rv += _oalRingBufferGetOffsetSamples(rb);
               break;
            case AAX_BYTES:
            {
               rv = _intBufGetNumNoLock(mixer->ringbuffers, _AAX_RINGBUFFER);
               rv *= _oalRingBufferGetNoSamples(rb);
               rv += _oalRingBufferGetOffsetSamples(rb);
               rv *= _oalRingBufferGetBytesPerSample(rb);
               break;
            }
            case AAX_MICROSECONDS:
               rv = sensor->mixer->curr_pos_sec*1e6f;
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

aaxBuffer
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
         _aaxAudioFrame* mixer = sensor->mixer;
         _intBuffers *dptr_rb = mixer->ringbuffers;
         unsigned int nbuf;

         nbuf = _intBufGetNum(dptr_rb, _AAX_RINGBUFFER);
         if (nbuf > 0)
         {
            void **ptr = _intBufShiftIndex(dptr_rb, _AAX_RINGBUFFER, 0, 1);
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
            _intBufReleaseNum(dptr_rb, _AAX_RINGBUFFER);
         }
         _intBufReleaseData(dptr, _AAX_SENSOR);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return buffer;
}

int
aaxSensorWaitForBuffer(aaxConfig config, float timeout)
{
   _handle_t *handle = get_handle(config);
   int rv = AAX_FALSE;
   if (handle)
   {
      static struct timespec sleept = {0, 1000};
      const _intBufferData* dptr;
      float sleep, duration = 0.0f;
      unsigned int nbuf;

      sleep = 1.0 / (handle->info->refresh_rate * 10.0);
      sleept.tv_nsec = sleep * 1e9f;
      do
      {
         nbuf = 0;
         duration += sleep;
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _intBuffers *ringbuffers = sensor->mixer->ringbuffers;
            nbuf=_intBufGetNumNoLock(ringbuffers, _AAX_RINGBUFFER);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
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

int
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
         else if (handle->handle)
         {
            _aaxSensorCreateRingBuffer(handle);
            _SET_PLAYING(handle);
            rv = AAX_TRUE;
         }
         else
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
//          _aaxErrorSet(AAX_INVALID_STATE);
         }
         break;
//    case AAX_PROCESSED:
      case AAX_STOPPED:
         if ((handle->info->mode == AAX_MODE_READ) && !handle->handle) {
            rv = _aaxSensorCaptureStop(handle);
         }
         else if (handle->handle)
         {
            _aaxSensorCreateRingBuffer(handle);
            _SET_STOPPED(handle);
            rv = AAX_TRUE;
         }
         else
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

int
_aaxSensorCreateRingBuffer(_handle_t *handle)
{
   _intBufferData *dptr;
   int rv = AAX_FALSE;

   assert(handle);
   assert(handle->info->mode == AAX_MODE_READ);
   assert(handle->thread.started == AAX_FALSE);

   dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
   if (dptr)
   {
      _sensor_t *sensor = _intBufGetDataPtr(dptr);
      _aaxAudioFrame *submix = sensor->mixer;
      _oalRingBuffer *rb;

      if (!submix->ringbuffer)
      {
         submix->ringbuffer = _oalRingBufferCreate(AAX_TRUE);
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
             * Now set the actual duration, this will not alter the allocated
             * space since it is lower that the initial duration.
             */
            _oalRingBufferSetDuration(rb, delay_sec);
            _oalRingBufferStart(rb);
         }
      }
      _intBufReleaseData(dptr, _AAX_SENSOR);
   }

   return rv;
}

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
         if (thread)
         {
            const _aaxDriverBackend *be = handle->backend.ptr;

            if (be->thread)
            {
               int r;

               handle->thread.ptr = _aaxThreadCreate();
               assert(handle->thread.ptr != 0);

               handle->thread.condition = _aaxConditionCreate();
               assert(handle->thread.condition != 0);

               handle->thread.mutex = _aaxMutexCreate(0);
               assert(handle->thread.mutex != 0);

               handle->thread.started = AAX_TRUE;
#if 0
               r = _aaxThreadStart(handle->thread.ptr,
                                   handle->backend.ptr->thread, handle);
#else
               r = _aaxThreadStart(handle->thread.ptr, _aaxSoftwareMixerThread, handle);
#endif
               if (r == 0)
               {
                  int p = 0;
                  do
                  {
                     static const struct timespec sleept = {0, 100000};

                     nanosleep(&sleept, 0);

                     dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
                     if (dptr)
                     {
                        sensor = _intBufGetDataPtr(dptr);
                        r = (sensor->mixer->ringbuffer != 0);
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
