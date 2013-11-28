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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif
#include <errno.h>		/* for ETIMEDOUT */
#include <assert.h>

#include <base/threads.h>
#include <ringbuffer.h>
#include <arch.h>
#include <api.h>


void
_aaxSoftwareMixerApplyEffects(const void *id, const void *hid, void *drb, const void *props2d)
{
   _aaxDriverBackend *be = (_aaxDriverBackend*)id;
   _oalRingBuffer2dProps *p2d = (_oalRingBuffer2dProps*)props2d;
   _oalRingBuffer *rb = (_oalRingBuffer *)drb;
   _oalRingBufferDelayEffectData* delay_effect;
   _oalRingBufferFreqFilterInfo* freq_filter;
   _oalRingBufferSample *rbd;
   float maxgain, gain;
   int dist_state;

   assert(rb != 0);
   assert(rb->sample != 0);

   rbd = rb->sample;
   assert(rbd->bytes_sample == sizeof(int32_t));

   delay_effect = _EFFECT_GET_DATA(p2d, DELAY_EFFECT);
   freq_filter = _FILTER_GET_DATA(p2d, FREQUENCY_FILTER);
   dist_state = _EFFECT_GET_STATE(p2d, DISTORTION_EFFECT);
   if (delay_effect || freq_filter || dist_state)
   {
      int32_t *scratch0 = rbd->scratch[SCRATCH_BUFFER0];
      int32_t *scratch1 = rbd->scratch[SCRATCH_BUFFER1];
      void* distortion_effect = NULL;
      unsigned int no_samples, ddesamps = 0;
      unsigned int track, tracks, bps;

      if (dist_state) {
         distortion_effect = &p2d->effect[DISTORTION_EFFECT];
      }

      if (delay_effect)
      {
         /*
          * can not use drbd->dde_samples since it's 10 times as big for the
          * fial mixer to accomodate for reverb
          */
         // ddesamps = drbd->dde_samples;
         ddesamps = (unsigned int)ceilf(DELAY_EFFECTS_TIME*rbd->frequency_hz);
      }

      bps = rbd->bytes_sample;
      tracks = rbd->no_tracks;
      no_samples = rbd->no_samples;
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
         bufEffectsApply(scratch0, dptr, scratch1,
                         0, no_samples, no_samples, ddesamps, track, 0,
                         freq_filter, delay_effect, distortion_effect);

         /* copy the unmodified next effects buffer back */
         DBG_MEMCLR(1, dptr-ddesamps, no_samples+ddesamps, bps);
         _aax_memcpy(ddeptr, scratch1+no_samples, ddesamps*bps);

         /* copy the data back from scratch0 to dptr */
         _aax_memcpy(dptr, scratch0, no_samples*bps);
      }
   }

   /*
    * If the requested gain is larger than the maximum capabilities of
    * hardware volume support, adjust the difference here (before the
    * compressor/limiter)
    */
   maxgain = be->param(hid, DRIVER_MAX_VOLUME);
   gain = _FILTER_GET(p2d, VOLUME_FILTER, AAX_GAIN);
   if (gain > maxgain) 
   {
      unsigned int track, tracks = rbd->no_tracks;
      unsigned int no_samples = rbd->no_samples;
      for (track=0; track<tracks; track++)
      {
         int32_t *dptr = rbd->track[track];
         _batch_mul_value(dptr, sizeof(int32_t), no_samples, gain/maxgain);
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
   unsigned int peak, maxpeak;
   unsigned int rms, maxrms;
   _oalRingBufferSample *rbd;
   char parametric, graphic;
   float dt, rms_rr;
   void *ptr = 0;
   char *p;

   assert(rb != 0);
   assert(rb->sample != 0);

   rbd = rb->sample;
   dt = GMATH_E1 * _oalRingBufferGetParamf(rb, RB_DURATION_SEC);
   rms_rr = _MINMAX(dt/0.3f, 0.0f, 1.0f);	// 300 ms average

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
   maxrms = maxpeak = 0;
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
         rms = 0;
         peak = dmax;
         _aaxProcessCompression(d1, &rms, &peak);
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

         eq = _FILTER_GET_DATA(sensor, EQUALIZER_HF);
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

      rms = 0;
      peak = dmax;
      _aaxProcessCompression(d1, &rms, &peak);
      rb->average[track] = (rms_rr*rb->average[track] + (1.0f-rms_rr)*rms);
      rb->peak[track] = peak;

      if (maxrms < rms) maxrms = rms;
      if (maxpeak < peak) maxpeak = peak;
   }
   free(ptr);

   rb->average[_AAX_MAX_SPEAKERS] = maxrms;
   rb->peak[_AAX_MAX_SPEAKERS] = maxpeak;
}

void*
_aaxSoftwareMixerThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   _intBufferData *dptr_sensor;
   const _aaxDriverBackend *be;
   _oalRingBuffer *dest_rb;
   _aaxAudioFrame *smixer;
   _aaxTimer *timer;
   int state, tracks;
   float delay_sec;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return NULL;
   }

   be = handle->backend.ptr;
   delay_sec = 1.0f/handle->info->refresh_rate;

   tracks = 2;
   smixer = NULL;
   dest_rb = _oalRingBufferCreate(REVERB_EFFECTS_TIME);
   if (dest_rb)
   {
      dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
      if (dptr_sensor)
      {
         _aaxMixerInfo* info;
         _sensor_t* sensor;

         sensor = _intBufGetDataPtr(dptr_sensor);
         smixer = sensor->mixer;
         info = smixer->info;

         tracks = info->no_tracks;
         _oalRingBufferSetParami(dest_rb, RB_NO_TRACKS, tracks);
         _oalRingBufferSetFormat(dest_rb, be->codecs, AAX_PCM24S);
         _oalRingBufferSetParamf(dest_rb, RB_FREQUENCY, info->frequency);
         _oalRingBufferSetParamf(dest_rb, RB_DURATION_SEC, delay_sec);
         _oalRingBufferInit(dest_rb, AAX_TRUE);
         _oalRingBufferStart(dest_rb);

         handle->ringbuffer = dest_rb;
         _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
      }
   }

   dest_rb = handle->ringbuffer;
   if (!dest_rb) {
      return NULL;
   }

   /* get real duration, it might have been altered for better performance */
   delay_sec = _oalRingBufferGetParamf(dest_rb, RB_DURATION_SEC);

   be->state(handle->backend.handle, DRIVER_PAUSE);
   state = AAX_SUSPENDED;

   timer = _aaxTimerCreate();
   _aaxTimerStartRepeatable(timer, delay_sec);

   _aaxMutexLock(handle->thread.mutex);
   do
   {
      if TEST_FOR_FALSE(handle->thread.started) {
         break;
      }

      if (state != handle->state)
      {
         if (_IS_PAUSED(handle) || (!_IS_PLAYING(handle) && _IS_STANDBY(handle))) {
            be->state(handle->backend.handle, DRIVER_PAUSE);
         }
         else if (_IS_PLAYING(handle) || _IS_STANDBY(handle)) {
            be->state(handle->backend.handle, DRIVER_RESUME);
         }
         state = handle->state;
      }

      /* do all the mixing */
      _aaxSoftwareMixerThreadUpdate(handle, dest_rb);
   }
   while (_aaxTimerWait(timer, handle->thread.mutex) == AAX_TIMEOUT);

   _aaxTimerDestroy(timer);
   _aaxMutexUnLock(handle->thread.mutex);

   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      _oalRingBufferDelete(handle->ringbuffer);
      handle->ringbuffer = NULL;
   }

   return handle;
}

unsigned int
_aaxSoftwareMixerSignalFrames(void *frames, float refresh_rate)
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
            _aaxAudioFrame* fmixer = frame->submix;

            if TEST_FOR_TRUE(fmixer->capturing)
            {
               unsigned int nbuf;
               nbuf = _intBufGetNumNoLock(fmixer->play_ringbuffers, _AAX_RINGBUFFER);
               if (nbuf < 2 && frame->thread.condition)  {
                  _aaxConditionSignal(frame->thread.condition);
               }
            }
//          _intBufReleaseData(dptr, _AAX_FRAME);
         }
      }
      _intBufReleaseNum(hf, _AAX_FRAME);

      /* give the remainder of the threads time slice to other threads */
//    _aaxThreadSwitch();
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
      num = _intBufGetMaxNum(hf, _AAX_FRAME);
      for (i=0; i<num; i++)
      {
         _intBufferData *dptr = _intBufGet(hf, _AAX_FRAME, i);
         if (dptr)
         {
            _frame_t* frame = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *fmixer = frame->submix;

            /*
             * fmixer->thread  = -1: mixer
             * fmixer->thread  =  1: threaded frame
             * fmixer->thread  =  0: non threaded frame, call update ourselves.
             */
            if (fmixer->thread)
            {
//             unsigned int dt = 1.5f*1000.0f/fmixer->info->refresh_rate; // ms
               unsigned int dt = 5000;
               int p = 0;

               /*
                * Wait for the frame's buffer te be available.
                * Can't call aaxAudioFrameWaitForBuffer because of a dead-lock
                */
               while ((fmixer->capturing == 1) && (++p < dt)) // 3ms is enough
               {
                  _intBufReleaseData(dptr, _AAX_FRAME);
                  _intBufReleaseNum(hf, _AAX_FRAME);

                  msecSleep(1);

                  _intBufGetMaxNum(hf, _AAX_FRAME);
                  dptr = _intBufGet(hf, _AAX_FRAME, i);
                  if (!dptr) break;

                  frame = _intBufGetDataPtr(dptr);
               }
            } /* fmixer->thread */
//          else { // non registered frames }

            if (dptr && fmixer->capturing > 1)
            {
                _handle_t *handle = frame->handle;
                const _aaxDriverBackend *be = handle->backend.ptr;
                void *be_handle = handle->backend.handle;
                _oalRingBuffer2dProps *p2d = fmixer->props2d;

                _aaxAudioFrameMix(dest_rb, fmixer->play_ringbuffers,
                                  p2d, be, be_handle);
                fmixer->capturing = 1;
            }

            /*
             * dptr could be zero if it was removed while waiting for a new
             * buffer
             */
            if (dptr) _intBufReleaseData(dptr, _AAX_FRAME);
         }
      }
      _intBufReleaseNum(hf, _AAX_FRAME);
   }
   return num;
}

int
_aaxSoftwareMixerPlay(void* rb, const void* devices, const void* ringbuffers, const void* frames, void* props2d, char capturing, const void* sensor, const void* backend, const void* be_handle, const void* fbackend, const void* fbe_handle)
{
   const _aaxDriverBackend* be = (const _aaxDriverBackend*)backend;
   const _aaxDriverBackend* fbe = (const _aaxDriverBackend*)fbackend;
   _oalRingBuffer2dProps *p2d = (_oalRingBuffer2dProps*)props2d;
   _oalRingBuffer *dest_rb = (_oalRingBuffer *)rb;
   float gain;
   int res;

   /* mix all threaded frame ringbuffers to the final mixer ringbuffer */
# if THREADED_FRAMES
   if (frames)
   {
      _intBuffers *mixer_frames = (_intBuffers*)frames;
      _aaxSoftwareMixerMixFrames(dest_rb, mixer_frames);
   }
   be->effects(be, be_handle, dest_rb, props2d);
   be->postprocess(be_handle, dest_rb, sensor);
#endif

   /** play back all mixed audio */
   gain = _FILTER_GET(p2d, VOLUME_FILTER, AAX_GAIN);
   if TEST_FOR_TRUE(capturing)
   {
      _intBuffers *mixer_ringbuffers = (_intBuffers*)ringbuffers;
      _oalRingBuffer *new_rb;

      new_rb = _oalRingBufferDuplicate(dest_rb, AAX_TRUE, AAX_FALSE);

      _oalRingBufferRewind(new_rb);
      _intBufAddData(mixer_ringbuffers, _AAX_RINGBUFFER, new_rb);

      dest_rb = new_rb;
   }

   res = be->play(be_handle, dest_rb, 1.0f, gain);
   if (fbe) {	/* slaved file-out backend */
      fbe->play(fbe_handle, dest_rb, 1.0f, gain);
   }

   return res;
}

int
_aaxSoftwareMixerThreadUpdate(void *config, void *dest_rb)
{
   _handle_t *handle = (_handle_t *)config;
   const _aaxDriverBackend *be, *fbe = NULL;
   _intBufferData *dptr_sensor;
   int res = 0;

   assert(handle);
   assert(handle->sensors);
   assert(handle->backend.ptr);
   assert(handle->info->no_tracks);

   be = handle->backend.ptr;
   if (handle->file.driver && _IS_PLAYING((_handle_t*)handle->file.driver)) {
      fbe = handle->file.ptr;
   }
   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor && (_IS_PLAYING(handle) || _IS_STANDBY(handle)))
   {
      void* be_handle = handle->backend.handle;
      void* fbe_handle = handle->file.handle;
      _aaxAudioFrame *smixer = NULL;

      if (_IS_PLAYING(handle))
      {
         dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
         if (dptr_sensor)
         {
            _sensor_t *sensor = _intBufGetDataPtr(dptr_sensor);

            smixer = sensor->mixer;
            if (handle->info->mode == AAX_MODE_READ)
            {
               float gain, rr, dt = 1.0f/smixer->info->refresh_rate;
               void *rv, *rb = dest_rb;

               gain = _FILTER_GET(smixer->props2d, VOLUME_FILTER, AAX_GAIN);
               rr = _FILTER_GET(smixer->props2d, VOLUME_FILTER, AAX_AGC_RESPONSE_RATE);
               rv = _aaxSensorCapture(rb, be, be_handle, &dt, rr,
                                      smixer->info->track,
                                      smixer->curr_pos_sec, gain);
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
                     _intBufAddData(smixer->play_ringbuffers, _AAX_RINGBUFFER, rb);
                     handle->ringbuffer = rv;
                  }
                  smixer->curr_pos_sec += dt;
                  _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
               }
            }
            else if (smixer->emitters_3d || smixer->emitters_2d || smixer->frames)
            {
               _oalRingBufferDelayed3dProps *sdp3d, *sdp3d_m;
               _oalRingBuffer2dProps sp2d;
               char fprocess = AAX_TRUE;
               unsigned int size;
               float ssv = 343.3f;
               float sdf = 1.0f;

               size = sizeof(_oalRingBufferDelayed3dProps);
               sdp3d = _aax_aligned_alloc16(size);
               sdp3d_m = _aax_aligned_alloc16(size);

               /**
                * copying here prevents locking the listener the whole time
                * it's used for just one time-frame anyhow
                * Note: modifications here should also be made to
                *       _aaxAudioFrameProcessThreadedFrame
                */
               dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
               if (dptr_sensor)
               {
                  _aaxAudioFrameProcessDelayQueue(smixer);
                  ssv =_EFFECT_GETD3D(smixer,VELOCITY_EFFECT,AAX_SOUND_VELOCITY);
                  sdf =_EFFECT_GETD3D(smixer,VELOCITY_EFFECT,AAX_DOPPLER_FACTOR);

                  _aax_memcpy(&sp2d, smixer->props2d,
                                     sizeof(_oalRingBuffer2dProps));
                  _aax_memcpy(sdp3d, smixer->props3d->dprops3d,
                                      sizeof(_oalRingBufferDelayed3dProps));
                  sdp3d_m->state3d = sdp3d->state3d;
                  sdp3d_m->pitch = sdp3d->pitch;
                  sdp3d_m->gain = sdp3d->gain;
                  _PROP_CLEAR(smixer->props3d);
                  _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
               }

               /* read-only data */
               _aax_memcpy(&sp2d.speaker, handle->info->speaker,
                                      _AAX_MAX_SPEAKERS*sizeof(vec4_t));
               _aax_memcpy(&sp2d.hrtf, handle->info->hrtf, 2*sizeof(vec4_t));

               /* update the modified properties */
               mtx4Copy(sdp3d_m->matrix, sdp3d->matrix);
               mtx4Mul(sdp3d_m->velocity, sdp3d->matrix, sdp3d->velocity);
#if 0
 if (_PROP3D_MTXSPEED_HAS_CHANGED(sdp3d_m)) {
 printf("matrix:\t\t\t\tvelocity\n");
 PRINT_MATRICES(sdp3d->matrix, sdp3d->velocity);
 printf("modified velocity\n");
 PRINT_MATRIX(sdp3d_m->velocity);
 }
#endif
               /* clear the buffer for use by the subframe */
               _oalRingBufferClear(dest_rb);
               _oalRingBufferStart(dest_rb);

               /** signal threaded frames to update (if necessary) */
               /* thread == -1: mixer; attached frames are threads */
               /* thread >=  0: frame; call updates manually       */
               if (smixer->thread < 0)
               {
                  _aaxSoftwareMixerSignalFrames(smixer->frames,
                                                smixer->info->refresh_rate);
                  fprocess = AAX_FALSE;
               }

               /* process emitters and registered sensors */
               res = _aaxAudioFrameProcess(dest_rb, sensor, smixer, ssv, sdf,
                                           NULL, NULL, &sp2d, sdp3d, sdp3d_m,
                                           be, be_handle, fprocess);
               /*
                * if the final mixer actually did render something,
                * mix the data.
                */
               res = _aaxSoftwareMixerPlay(dest_rb, smixer->devices,
                                           smixer->play_ringbuffers,
                                           smixer->frames, &sp2d,
                                           smixer->capturing, sensor,
                                           be, be_handle, fbe, fbe_handle);
               _aax_aligned_free(sdp3d);
               _aax_aligned_free(sdp3d_m);
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
               smixer = sensor->mixer;

               if (smixer->emitters_3d || smixer->emitters_2d) {
                  _aaxNoneDriverProcessFrame(smixer);
               }
               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
            }
         }
      }
   }

   return res;
}

