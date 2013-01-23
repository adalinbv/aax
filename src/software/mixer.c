/*
 * Copyright 2005-2012 by Erik Hofman.
 * Copyright 2009-2012 by Adalin B.V.
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

#include <string.h>		/* for memset */
#include <errno.h>		/* for ETIMEDOUT */
#include <assert.h>

#include <base/timer.h>		/* for msecSleep, gettimeofday */
#include <base/threads.h>
#include <ringbuffer.h>
#include <arch.h>
#include <api.h>


void
_aaxSoftwareMixerApplyEffects(const void *id, void *drb, const void *props2d)
{
   _oalRingBuffer2dProps *p2d = (_oalRingBuffer2dProps*)props2d;
   _oalRingBuffer *rb = (_oalRingBuffer *)drb;
   _oalRingBufferFreqFilterInfo* freq_filter;
   _oalRingBufferDelayEffectData* delay;
   _oalRingBufferSample *rbd;
   int dist_state;
   float gain;

   assert(rb != 0);
   assert(rb->sample != 0);

   rbd = rb->sample;

   gain = _FILTER_GET(p2d, VOLUME_FILTER, AAX_GAIN);

   delay = _EFFECT_GET_DATA(p2d, DELAY_EFFECT);
   freq_filter = _FILTER_GET_DATA(p2d, FREQUENCY_FILTER);
   dist_state = _EFFECT_GET_STATE(p2d, DISTORTION_EFFECT);
   if ((gain > 1.01f || gain < 0.99f) || (delay || freq_filter || dist_state))
   {
      int32_t *scratch0 = rbd->scratch[SCRATCH_BUFFER0];
      int32_t *scratch1 = rbd->scratch[SCRATCH_BUFFER1];
      unsigned int bps, no_samples, ddesamps;
      unsigned int track, tracks;
      void* distortion = NULL;

      bps = rbd->bytes_sample;
      ddesamps = rbd->dde_samples;
      no_samples = rbd->no_samples;

      if (dist_state) {
         distortion = &_EFFECT_GET(p2d, DISTORTION_EFFECT, 0);
      }

      tracks = rbd->no_tracks;
      for (track=0; track<tracks; track++)
      {
         int32_t *dptr = rbd->track[track];
         int32_t *ddeptr = dptr - ddesamps;

         /* save the unmodified next effects buffer for later use          */
         /* (scratch buffers have a leading and a trailing effects buffer) */
         DBG_MEMCLR(1, scratch1-ddesamps, no_samples+2*ddesamps, bps);
         _aax_memcpy(scratch1+no_samples, ddeptr+no_samples, ddesamps*bps);

         /* mix the buffer and the delay buffer */
         DBG_MEMCLR(1, scratch0-ddesamps, no_samples+2*ddesamps, bps);
         bufEffectsApply(scratch0, dptr, scratch1, 0, no_samples, no_samples,
                         ddesamps, track, 0, freq_filter, delay, distortion);

         /* copy the unmodified next effects buffer back */
         DBG_MEMCLR(1, dptr-ddesamps, no_samples+ddesamps, bps);
         _aax_memcpy(ddeptr, scratch1+no_samples, ddesamps*bps);

         /* copy the data back from scratch0 to dptr */
         _aax_memcpy(dptr, scratch0, no_samples*bps);

         if (gain > 1.01f || gain < 0.99f) {
            _batch_mul_value(dptr, sizeof(int32_t), no_samples, gain);
         }
      }
   }
}

void
_aaxSoftwareMixerPostProcess(const void *id, void *d, const void *s)
{
   _oalRingBuffer *rb = (_oalRingBuffer*)d;
   _sensor_t *sensor = (_sensor_t*)s;
   _oalRingBufferReverbData *reverb;
   unsigned int track, tracks;
   unsigned int average, peak;
   _oalRingBufferSample *rbd;
   char parametric, graphic;
   void *ptr = 0;
   float dt;
   char *p;

   assert(rb != 0);
   assert(rb->sample != 0);

   rbd = rb->sample;

   reverb = 0;
   parametric = graphic = 0;
   if (sensor)
   {
      reverb = _EFFECT_GET_DATA(sensor->mixer->props2d, REVERB_EFFECT);
      parametric = graphic = (_FILTER_GET_DATA(sensor, EQUALIZER_HF) != NULL);
      parametric &= (_FILTER_GET_DATA(sensor, EQUALIZER_LF) != NULL);
      graphic    &= (_FILTER_GET_DATA(sensor, EQUALIZER_LF) == NULL);

      if (parametric || graphic || reverb)
      {
         unsigned int size = 2*rbd->track_len_bytes;
         if (reverb) size += rbd->dde_samples*rbd->bytes_sample;
         p = 0;
         ptr = _aax_malloc(&p, size);
         // TODO: create only once
      }
   }

   /* set up this way because we always need to apply compression */
   tracks = rbd->no_tracks;
   for (track=0; track<tracks; track++)
   {
      int32_t *d1 = (int32_t *)rbd->track[track];
      unsigned int dmax = rbd->no_samples;

      if (ptr && reverb)
      {
         unsigned int ds = rbd->dde_samples;
         int32_t *sbuf = (int32_t *)p + ds;
         int32_t *sbuf2 = sbuf + dmax;

         /* level out previous filters and effects */
         average = 0;
         peak = dmax;
         _aaxProcessCompression(d1, &average, &peak);
         bufEffectReflections(d1, sbuf, sbuf2, 0, dmax, ds, track, reverb);
         bufEffectReverb(d1, 0, dmax, ds, track, reverb);
      }

      if (ptr && parametric)
      {
         _oalRingBufferFreqFilterInfo* filter;
         int32_t *d2 = (int32_t *)p;
         int32_t *d3 = d2 + dmax;

         _aax_memcpy(d3, d1, rbd->track_len_bytes);
         filter = _FILTER_GET_DATA(sensor, EQUALIZER_LF);
         bufFilterFrequency(d1, d3, 0, dmax, 0, track, filter, 0);

         filter = _FILTER_GET_DATA(sensor, EQUALIZER_HF);
         bufFilterFrequency(d2, d3, 0, dmax, 0, track, filter, 0);
         _batch_fmadd(d1, d2, dmax, 1.0, 0.0);
      }
      else if (ptr && graphic)
      {
         _oalRingBufferFreqFilterInfo* filter;
         _oalRingBufferEqualizerInfo *eq;
         int32_t *d2 = (int32_t *)p;
         int32_t *d3 = d2 + dmax;
         int b = 6;

         eq=_FILTER_GET_DATA(sensor, EQUALIZER_HF);
         filter = &eq->band[b--];
         _aax_memcpy(d3, d1, rbd->track_len_bytes);
         bufFilterFrequency(d1, d3,  0, dmax, 0, track, filter, 0);
         do
         {
            filter = &eq->band[b--];
            if (filter->lf_gain || filter->hf_gain)
            {
               bufFilterFrequency(d2, d3, 0, dmax, 0, track, filter, 0);
               _batch_fmadd(d1, d2, rbd->no_samples, 1.0f, 0.0f);
            }

            filter = &eq->band[b--];
            if (filter->lf_gain || filter->hf_gain) 
            {
               bufFilterFrequency(d2, d3, 0, dmax, 0, track, filter, 0);
               _batch_fmadd(d1, d2, rbd->no_samples, 1.0f, 0.0f);
            }

            filter = &eq->band[b--];
            if (filter->lf_gain || filter->hf_gain) 
            {
               bufFilterFrequency(d2, d3, 0, dmax, 0, track, filter, 0);
               _batch_fmadd(d1, d2, rbd->no_samples, 1.0f, 0.0f);
            }
         }
         while (b > 0);
      }

      average = 0;
      peak = dmax;
      _aaxProcessCompression(d1, &average, &peak);

      dt = _oalRingBufferGetDuration(rb)*2.0f;	/* half a second average */
      rb->average[track] = ((1.0f-dt)*rb->average[track] + dt*average);
      rb->peak[track] = ((1.0f-dt)*rb->peak[track] + dt*peak);
   }
   free(ptr);
}

void*
_aaxSoftwareMixerThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   _intBufferData *dptr_sensor;
   const _aaxDriverBackend *be;
   _oalRingBuffer *dest_rb;
   _aaxAudioFrame *mixer;
   unsigned int bufsz;
   struct timespec ts;
   float delay_sec;
   float dt = 0.0;
   float elapsed;
   int state;
   int tracks;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return NULL;
   }

   be = handle->backend.ptr;
   delay_sec = 1.0f/handle->info->refresh_rate;

   tracks = 2;
   mixer = NULL;
   dest_rb = _oalRingBufferCreate(REVERB_EFFECTS_TIME);
   if (dest_rb)
   {
      dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
      if (dptr_sensor)
      {
//       _oalRingBuffer *nrb;
         _aaxMixerInfo* info;
         _sensor_t* sensor;

         sensor = _intBufGetDataPtr(dptr_sensor);
         mixer = sensor->mixer;
         info = mixer->info;

         tracks = info->no_tracks;
         _oalRingBufferSetNoTracks(dest_rb, tracks);
         _oalRingBufferSetFormat(dest_rb, be->codecs, AAX_PCM24S);
         _oalRingBufferSetFrequency(dest_rb, info->frequency);
         _oalRingBufferSetDuration(dest_rb, delay_sec);
         _oalRingBufferInit(dest_rb, AAX_TRUE);
         _oalRingBufferStart(dest_rb);

         handle->ringbuffer = dest_rb;
//       nrb = _oalRingBufferDuplicate(dest_rb, AAX_FALSE, AAX_FALSE);
//       _intBufAddData(mixer->ringbuffers, _AAX_RINGBUFFER, nrb);
         _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
      }
   }

   dest_rb = handle->ringbuffer;
   if (!dest_rb) {
      return NULL;
   }

   /* get real duration, it might have been altered for better performance */
   bufsz = _oalRingBufferGetNoSamples(dest_rb);
   delay_sec = _oalRingBufferGetDuration(dest_rb);

   be->pause(handle->backend.handle);
   state = AAX_SUSPENDED;

   elapsed = 0.0f;
   _aaxMutexLock(handle->thread.mutex);
   do
   {
      static int res = 0;
      float time_fact = 1.0f -(float)res/(float)bufsz;
#if 1
      float delay = delay_sec*time_fact;

      /* once per second when standby */
//    if (_IS_STANDBY(handle)) delay = 1.0f;

      elapsed -= delay;
      if (elapsed <= 0.0f)
      {
         struct timeval now;
         float fdt;

         elapsed += 60.0f;               /* resync the time every 60 seconds */

         dt = delay;
         fdt = floorf(dt);

         gettimeofday(&now, 0);
         ts.tv_sec = now.tv_sec + (time_t)fdt;

         dt -= fdt;
         dt += now.tv_usec*1e-6f;
         ts.tv_nsec = (long)(dt*1e9f);
         if (ts.tv_nsec >= 1e9f)
         {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
         }
      }
      else
      {
         dt += delay;
         if (dt >= 1.0f)
         {
            float fdt = floorf(dt);
            ts.tv_sec += (time_t)fdt;
            dt -= fdt;
         }
         ts.tv_nsec = (long)(dt*1e9f);
      }
#else
      clock_gettime(CLOCK_REALTIME, &ts);
#endif

      if TEST_FOR_FALSE(handle->thread.started) {
         break;
      }

      if (state != handle->state)
      {
         if (_IS_PAUSED(handle) || (!_IS_PLAYING(handle) && _IS_STANDBY(handle))) {
            be->pause(handle->backend.handle);
         }
         else if (_IS_PLAYING(handle) || _IS_STANDBY(handle)) {
            be->resume(handle->backend.handle);
         }
         state = handle->state;
      }

      /* do all the mixing */
      res = _aaxSoftwareMixerThreadUpdate(handle, dest_rb);
   }
   while (_aaxConditionWaitTimed(handle->thread.condition, handle->thread.mutex, &ts) == ETIMEDOUT);

   _aaxMutexUnLock(handle->thread.mutex);

   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      _oalRingBufferStop(handle->ringbuffer);
      _oalRingBufferDelete(handle->ringbuffer);
      handle->ringbuffer = NULL;
   }

   return handle;
}

unsigned int
_aaxSoftwareMixerSignalFrames(void *frames)
{
   _intBuffers *hf = (_intBuffers*)frames;
   unsigned int num = 0;

   if (hf)
   {
      unsigned int i;

      num = _intBufGetMaxNum(hf, _AAX_FRAME);
      for (i=0; i<num; i++)
      {
         _intBufferData *dptr = _intBufGetNoLock(hf, _AAX_FRAME, i);
         if (dptr)
         {
            _frame_t* frame = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = frame->submix;

            if TEST_FOR_TRUE(mixer->capturing)
            {
               unsigned int nbuf;
               nbuf = _intBufGetNumNoLock(mixer->ringbuffers, _AAX_RINGBUFFER);
               if (nbuf < 2) {
                  _aaxConditionSignal(frame->thread.condition);
               }
            }
//          _intBufReleaseData(dptr, _AAX_FRAME);
         }
      }
      _intBufReleaseNum(hf, _AAX_FRAME);

      /* give the remainder of the threads time slice to other threads */
//    msecSleep(2);
   }
   return num;
}

/*-------------------------------------------------------------------------- */

unsigned int
_aaxSoftwareMixerMixFrames(void *dest, _intBuffers *hf)
{
   _oalRingBuffer *dest_rb = (_oalRingBuffer *)dest;
   unsigned int i, num = 0;
   if (hf)
   {
#if USE_CONDITION
      void *mutex = _aaxMutexCreate(NULL);
      double dt_ns = 0.8 * _oalRingBufferGetDuration(dest_rb)*1000000000;
      struct timeval tv;
      struct timespec ts;

      gettimeofday(&tv, NULL);
      ts.tv_sec = tv.tv_sec + 0;
      ts.tv_nsec = tv.tv_usec*1000 + (long)dt_ns;
      if (ts.tv_nsec > 1000000000L)
      {
         ts.tv_sec++;
         ts.tv_nsec -= 1000000000L;
      }
#endif

      num = _intBufGetMaxNum(hf, _AAX_FRAME);
      for (i=0; i<num; i++)
      {
         _intBufferData *dptr = _intBufGet(hf, _AAX_FRAME, i);
         if (dptr)
         {
            _frame_t* frame = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *mixer = frame->submix;

            /*
             * mixer->thread  = -1: mixer
             * mixer->thread  =  1: threaded frame
             * mixer->thread  =  0: non threaded frame, call update ourselves.
             */
//          if (!mixer->thread)
//          {
// printf("non threaded frame\n");
//          }
//          else
            {
#if USE_CONDITION
               if (mixer->frame_ready)		// REGISTERED_FRAME;
               {
                  int rv;
                  rv = _aaxConditionWaitTimed(mixer->frame_ready, mutex, &ts);
#if 1
if (rv != 0)
printf("_aaxConditionWaitTimed: %s\n", (rv == ETIMEDOUT) ? "time-out" : "invalid");
#endif
               }
#else
               float refrate = mixer->info->refresh_rate;
               int p = 0;

               /*
                * Can't call aaxAudioFrameWaitForBuffer because of a dead-lock
                */
               while ((mixer->capturing == 1) && (p++ < 5000))
               {
                  _intBufReleaseData(dptr, _AAX_FRAME);

                  msecSleep(1);	 /* special case, see Sleep(0) for windows */

                  dptr = _intBufGet(hf, _AAX_FRAME, i);
                  if (!dptr) break;

                  frame = _intBufGetDataPtr(dptr);
               }
#endif
            } /* mixer->thread */

            if (dptr && mixer->capturing > 1)
            {
                _handle_t *handle = frame->handle;
                const _aaxDriverBackend *be = handle->backend.ptr;
                void *be_handle = handle->backend.handle;
                _oalRingBuffer2dProps *p2d = mixer->props2d;

                _aaxAudioFrameMix(dest_rb, mixer->ringbuffers,
                                  &mixer->capturing, p2d, be, be_handle);
            }

            /*
             * dptr could be zero if it was removed while waiting for a new
             * buffer
             */
            if (dptr) _intBufReleaseData(dptr, _AAX_FRAME);
         }
      }
      _intBufReleaseNum(hf, _AAX_FRAME);

#if USE_CONDITION
      _aaxMutexDestroy(mutex);
#endif
   }
   return num;
}

int
_aaxSoftwareMixerPlayFrame(void** rb, const void* devices, const void* ringbuffers, const void* frames, void* props2d, const void* props3d, char capturing, const void* sensor, const void* backend, const void* be_handle)
{
   const _aaxDriverBackend* be = (const _aaxDriverBackend*)backend;
   _oalRingBuffer *dest_rb = (_oalRingBuffer *)*rb;
   int res;

   if (frames)
   {
      _intBuffers *mixer_frames = (_intBuffers*)frames;
      _aaxSoftwareMixerMixFrames(dest_rb, mixer_frames);
   }

   if (devices) {
      _aaxSensorsProcess(dest_rb, devices, props2d);
   }
   be->effects(be_handle, dest_rb, props2d);
   be->postprocess(be_handle, dest_rb, sensor);

   /** play back all mixed audio */
   res = be->play(be_handle, dest_rb, 1.0, 1.0);

   if TEST_FOR_TRUE(capturing)
   {
      _intBuffers *mixer_ringbuffers = (_intBuffers*)ringbuffers;
      _oalRingBuffer *new_rb;

      new_rb = _oalRingBufferDuplicate(dest_rb, AAX_TRUE, AAX_FALSE);

      _oalRingBufferForward(new_rb);
      _intBufAddData(mixer_ringbuffers, _AAX_RINGBUFFER, new_rb);
   }

   _oalRingBufferClear(dest_rb);
   _oalRingBufferStart(dest_rb);

   return res;
}

int
_aaxSoftwareMixerThreadUpdate(void *config, void *dest)
{
   _handle_t *handle = (_handle_t *)config;
   const _aaxDriverBackend* be;
   _intBufferData *dptr_sensor;
   int res = 0;

   assert(handle);
   assert(handle->sensors);
   assert(handle->backend.ptr);
   assert(handle->info->no_tracks);

   be = handle->backend.ptr;
   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor && (_IS_PLAYING(handle) || _IS_STANDBY(handle)))
   {
      void* be_handle = handle->backend.handle;
      _aaxAudioFrame *mixer = NULL;

      if (_IS_PLAYING(handle) && be->is_available(be_handle))
      {
         dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr_sensor)
         {
            _sensor_t *sensor = _intBufGetDataPtr(dptr_sensor);
            mixer = sensor->mixer;

            if (handle->info->mode == AAX_MODE_READ)
            {
               float dt = 1.0f / mixer->info->refresh_rate;
               void *rv, *rb = dest; // mixer->ringbuffer;

               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
               rv = _aaxSensorCapture(rb, be, be_handle, &dt,
                                               mixer->curr_pos_sec);
               if (dt == 0.0f)
               {
                  _SET_STOPPED(handle);
                  _SET_PROCESSED(handle);
               }

               dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
               if (dptr_sensor)
               {
                  if (rb != rv)
                  {
                     _intBufAddData(mixer->ringbuffers, _AAX_RINGBUFFER, rb);
                     handle->ringbuffer = rv;
                  }
                  mixer->curr_pos_sec += dt;
                  _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
               }
            }
            else if (mixer->emitters_3d || mixer->emitters_2d || mixer->frames)
            {
               _oalRingBuffer2dProps sp2d;
               _oalRingBuffer3dProps sp3d;
               void *new_rb;

               /* copying here prevents locking the listener the whole time */
               /* it's used for just one time-frame anyhow                  */
               memcpy(&sp3d, mixer->props3d, sizeof(_oalRingBuffer3dProps));
               memcpy(&sp2d, mixer->props2d, sizeof(_oalRingBuffer2dProps));
               memcpy(&sp2d.pos, handle->info->speaker,
                                  _AAX_MAX_SPEAKERS*sizeof(vec4_t));
               memcpy(&sp2d.hrtf, handle->info->hrtf, 2*sizeof(vec4_t));

               /** signal threaded frames to update (if necessary) */
               /* thread == -1: mixer; attached frames are threads */
               /* thread >=  0: frame; call updates manually       */
               if (mixer->thread < 0) {
                  _aaxSoftwareMixerSignalFrames(mixer->frames);
               }
               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

               /* main mixer */
               _aaxEmittersProcess(dest, handle->info, &sp2d, &sp3d, NULL, NULL,
                                         mixer->emitters_2d, mixer->emitters_3d,
                                         be, be_handle);
 
               new_rb = handle->ringbuffer;
               res = _aaxSoftwareMixerPlayFrame(&new_rb, mixer->devices,
                                                mixer->ringbuffers,
                                                mixer->frames,
                                                &sp2d, &sp3d, mixer->capturing,
                                                sensor, be, be_handle);

               if (new_rb != handle->ringbuffer)
               {
                  dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
                  if (dptr_sensor)
                  {
                     handle->ringbuffer = new_rb;
                     _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
                  }
               }
            }
         }
      }
      else /* if (_IS_STANDBY(handle) */
      {
         if (handle->info->mode != AAX_MODE_READ)
         {
            dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr_sensor)
            {
               _sensor_t *sensor = _intBufGetDataPtr(dptr_sensor);
               mixer = sensor->mixer;

               if (mixer->emitters_3d || mixer->emitters_2d) {
                  _aaxNoneDriverProcessFrame(mixer);
               }
               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
            }
         }
      }
   }

   return res;
}

