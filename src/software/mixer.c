/*
 * Copyright 2005-2011 by Erik Hofman.
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

#include <math.h>		/* for floor, and rint */
#include <errno.h>		/* for ETIMEDOUT */
#include <assert.h>
#include <sys/time.h>		/* for struct timeval */

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>
#include <base/threads.h>

#include "device.h"
#include "audio.h"

void
_aaxSoftwareMixerApplyEffects(const void *id, void *drb, const void *props2d)
{
   _oalRingBuffer2dProps *p2d = (_oalRingBuffer2dProps*)props2d;
   _oalRingBuffer *rb = (_oalRingBuffer *)drb;
   _oalRingBufferDelayEffectData* delay_effect;
   _oalRingBufferFreqFilterInfo* freq_filter;
   _oalRingBufferSample *rbd;
   void* distortion_effect;

   assert(rb != 0);
   assert(rb->sample != 0);

   rbd = rb->sample;

   delay_effect = _EFFECT_GET_DATA(p2d, DELAY_EFFECT);
   distortion_effect = _EFFECT_GET_DATA(p2d, DISTORTION_EFFECT);
   freq_filter = _FILTER_GET_DATA(p2d, FREQUENCY_FILTER);
   if (delay_effect || freq_filter || distortion_effect)
   {
      unsigned int dde_bytes, track_len_bytes;
      unsigned int track, tracks, dno_samples;
      unsigned int bps, dde_samps;

      bps = rbd->bytes_sample;
      dde_samps = rbd->dde_samples;
      track_len_bytes = rbd->track_len_bytes;
      dde_bytes = dde_samps * bps;
      dno_samples = rbd->no_samples;

      tracks = rbd->no_tracks;
      for (track=0; track<tracks; track++)
      {
         int32_t *scratch = rbd->scratch[SCRATCH_BUFFER1];
         int32_t *d2 = rbd->scratch[SCRATCH_BUFFER0];
         int32_t *d1 = rbd->track[track];
         char *cd2 = (char*)d2 - dde_bytes;
         char *cd1 = (char*)d1 - dde_bytes;

         /* save the unmodified next dde buffer for later use */
         _aax_memcpy(cd2-dde_bytes, cd1+track_len_bytes, dde_bytes);

         /* copy the data for processing */
         if ( distortion_effect) {
            distortion_effect = &_EFFECT_GET(p2d, DISTORTION_EFFECT, 0);
         }
         _aax_memcpy(cd2, cd1, dde_bytes+track_len_bytes);
         bufEffectsApply(d1, d2, scratch, 0, dno_samples, dde_samps, track,
                         freq_filter, delay_effect, distortion_effect);

         /* restore the unmodified next dde buffer */
         _aax_memcpy(cd1, cd2-dde_bytes, dde_bytes);
#if 0
{
   unsigned int i, diff = 0;
   for (i=0; i<dno_samples; i++) {
      if (d2[i] != d1[i]) diff++;
   }
   printf("no diff samples: %i\n", diff);
}
#endif
      }
   }
}

void
_aaxSoftwareMixerPostProcess(const void *id, void *d, const void *s)
{
   _oalRingBuffer *rb = (_oalRingBuffer*)d;
   _sensor_t *sensor = (_sensor_t*)s;
   unsigned int track, tracks;
   _oalRingBufferSample *rbd;
   void *ptr = 0;
   char *p;

   assert(rb != 0);
   assert(rb->sample != 0);

   rbd = rb->sample;

   if (sensor && _FILTER_GET_DATA(sensor, EQUALIZER_LF)
        && _FILTER_GET_DATA(sensor, EQUALIZER_HF))
   {
      p = 0;
      ptr = _aax_malloc(&p, 2*rbd->track_len_bytes);
   }

   /* set up this way because we always need to apply compression */
   tracks = rbd->no_tracks;
   for (track=0; track<tracks; track++)
   {
      int32_t *d1 = (int32_t *)rbd->track[track];
      unsigned int dmax = rbd->no_samples;

      if (ptr)
      {
         _oalRingBufferFreqFilterInfo* filter;
         int32_t *d2 = (int32_t *)p;
         int32_t *d3 = d2 + dmax;

         _aax_memcpy(d3, d1, rbd->track_len_bytes);
         filter = sensor->filter[EQUALIZER_LF].data;
         bufFilterFrequency(d1, d3, 0, dmax, 0, track, filter);

         filter = sensor->filter[EQUALIZER_HF].data;
         bufFilterFrequency(d2, d3, 0, dmax, 0, track, filter);
         _batch_fmadd(d1, d2, dmax, 1.0, 0.0);
      }
      _aaxProcessCompression(d1, 0, dmax);
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
   dest_rb = _oalRingBufferCreate(AAX_TRUE);
   if (dest_rb)
   {
      dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
      if (dptr_sensor)
      {
         _aaxMixerInfo* info;
         _sensor_t* sensor;

         sensor = _intBufGetDataPtr(dptr_sensor);
         mixer = sensor->mixer;
         info = mixer->info;

         if (info->mode == AAX_MODE_READ) {
            _oalRingBufferSetFormat(dest_rb, be->codecs, info->format);
         } else {
            _oalRingBufferSetFormat(dest_rb, be->codecs, AAX_PCM24S);
         }
         tracks = info->no_tracks;
         _oalRingBufferSetNoTracks(dest_rb, tracks);
         _oalRingBufferSetFrequency(dest_rb, info->frequency);
         _oalRingBufferSetDuration(dest_rb, delay_sec);
         _oalRingBufferInit(dest_rb, AAX_TRUE);
         _oalRingBufferStart(dest_rb);

         mixer->ringbuffer = dest_rb;
         _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
      }
   }

   dest_rb = mixer->ringbuffer;
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
      float delay = delay_sec*time_fact;

if (res != 0 && res != 1) printf("res: %i\n\t\t\t", res);
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
      _oalRingBufferStop(mixer->ringbuffer);
      _oalRingBufferDelete(mixer->ringbuffer);
   }

   return handle;
}

void
_aaxSoftwareMixerProcessFrame(void* rb, void* info, void *sp2d, void *sp3d, void *fp2d, void *fp3d, void *e2d, void *e3d, const void* backend, void* be_handle)
{
   const _aaxDriverBackend* be = (const _aaxDriverBackend*)backend;
   _oalRingBuffer *dest_rb = (_oalRingBuffer*)rb;
   _oalRingBuffer3dProps *props3d;
   _oalRingBuffer2dProps *props2d;
   _intBuffers *he;
   int stage;
   float dt;

   dt = _oalRingBufferGetDuration(dest_rb);
   props2d = fp2d ? (_oalRingBuffer2dProps*)fp2d : (_oalRingBuffer2dProps*)sp2d;
   props3d = fp3d ? (_oalRingBuffer3dProps*)fp2d : (_oalRingBuffer3dProps*)sp2d;

   stage = 0;
   he = e3d;
   do
   {
      unsigned int i, num;

      num = _intBufGetMaxNum(he, _AAX_EMITTER);
      for (i=0; i<num; i++)
      {
         _intBufferData *dptr_src;
         _emitter_t *emitter;
         _aaxEmitter *src;

         dptr_src = _intBufGet(he, _AAX_EMITTER, i);
         if (!dptr_src) continue;

         dest_rb->curr_pos_sec = 0.0f;

         emitter = _intBufGetDataPtr(dptr_src);
         src = emitter->source;
         if (_IS_PLAYING(src))
         {
            _intBufferData *dptr_sbuf;
            unsigned int nbuf, rv = 0;
            int streaming;

            nbuf = _intBufGetNum(src->buffers, _AAX_EMITTER_BUFFER);
            assert(nbuf > 0);

            streaming = (nbuf > 1);
            dptr_sbuf = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, src->pos);
            if (dptr_sbuf)
            {
               _embuffer_t *embuf = _intBufGetDataPtr(dptr_sbuf);
               _oalRingBuffer *src_rb = embuf->ringbuffer;
               do
               {
                  if (_IS_STOPPED(src)) {
                     _oalRingBufferStop(src_rb);
                  }
                  else if (_oalRingBufferTestPlaying(src_rb) == 0)
                  {
                     if (streaming) {
                        _oalRingBufferStartStreaming(src_rb);
                     } else {
                        _oalRingBufferStart(src_rb);
                     }
                  }

                  /* 3d mixing */
                  if (stage == 0)
                  {
                     assert(_IS_POSITIONAL(src));
                     be->prepare3d(sp3d, fp3d, info, props2d, src);
                     if (src->curr_pos_sec >= props2d->delay_sec)
// printf("mix3d\n");
                        rv = be->mix3d(be_handle, dest_rb, src_rb, src->props2d,
                                               props2d, emitter->track);
                  }
                  else
                  {
                     assert(!_IS_POSITIONAL(src));
                     rv = be->mix2d(be_handle, dest_rb, src_rb, src->props2d,
                                           props2d, 1.0, 1.0);
                  }

                  src->curr_pos_sec += dt;

                  /*
                   * The current buffer of the source has finished playing.
                   * Decide what to do next.
                   */
                  if (rv)
                  {
                     if (streaming)
                     {
                        /* is there another buffer ready to play? */
                        if (++src->pos == nbuf)
                        {
                           /*
                            * The last buffer was processed, return to the
                            * first buffer or stop? 
                            */
                           if TEST_FOR_TRUE(emitter->looping) {
                              src->pos = 0;
                           }
                           else
                           {
                              _SET_STOPPED(src);
                              _SET_PROCESSED(src);
                              break;
                           }
                        }

                        rv &= _oalRingBufferTestPlaying(dest_rb);
                        if (rv)
                        {
                           _intBufReleaseData(dptr_sbuf,_AAX_EMITTER_BUFFER);
                           dptr_sbuf = _intBufGet(src->buffers,
                                              _AAX_EMITTER_BUFFER, src->pos);
                           embuf = _intBufGetDataPtr(dptr_sbuf);
                           src_rb = embuf->ringbuffer;
                        }
                     }
                     else /* !streaming */
                     {
                        _SET_PROCESSED(src);
                        break;
                     }
                  }
               }
               while (rv);
               _intBufReleaseData(dptr_sbuf, _AAX_EMITTER_BUFFER);
            }
            _intBufReleaseNum(src->buffers, _AAX_EMITTER_BUFFER);
            _oalRingBufferStart(dest_rb);
         }
         _intBufReleaseData(dptr_src, _AAX_EMITTER);
      }
      _intBufReleaseNum(he, _AAX_EMITTER);

      if (stage == 0)   /* 3d stage */
      {
         /* apply environmental effects */
         if (num > 0) {
            be->effects(be_handle, dest_rb, props2d);
         }
         he = e2d;
      }
   }
   while (++stage < 2); /* positional and stereo */

   _PROP_MTX_CLEAR_CHANGED(props3d);
   _PROP_PITCH_CLEAR_CHANGED(props3d);
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
         _intBufferData *dptr = _intBufGet(hf, _AAX_FRAME, i);
         if (dptr)
         {
            _frame_t* frame = _intBufGetDataPtr(dptr);

            if TEST_FOR_TRUE(frame->submix->capturing)
            {
               frame->thread.update = 1;
               _aaxConditionSignal(frame->thread.condition);
            }
            _intBufReleaseData(dptr, _AAX_FRAME);
         }
      }
      _intBufReleaseNum(hf, _AAX_FRAME);
   }
   return num;
}

float
_aaxSoftwareMixerReadFrame(void *config, const void* backend, void *handle)
{
   const _aaxDriverBackend* be = (const _aaxDriverBackend*)backend;
   _aaxAudioFrame* mixer = (_aaxAudioFrame*)config;
   _oalRingBuffer *dest_rb;
   size_t tracksize;
   float rv = 0;
   char *dbuf;
   int res;

   /*
    * dest_rb is thread specific and does not need a lock
    */
   dest_rb = mixer->ringbuffer;
   dbuf = dest_rb->sample->scratch[0];
   tracksize = _oalRingBufferGetTrackSize(dest_rb);

   res = be->record(handle, dbuf, &tracksize, 1.0, 1.0);
   if TEST_FOR_TRUE(res)
   {
      _oalRingBuffer *nrb;

      _oalRingBufferFillInterleaved(dest_rb, dbuf, 1, AAX_FALSE);
      nrb = _oalRingBufferDuplicate(dest_rb, AAX_FALSE);

       _oalRingBufferForward(dest_rb);
       /*
        * mixer->ringbuffers is shared between threads and needs
        * to be locked
        */
       _intBufAddData(mixer->ringbuffers, _AAX_RINGBUFFER, dest_rb);

       mixer->ringbuffer = nrb;
       rv = 1.0f/mixer->info->refresh_rate;
   }
   return rv;
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
            _aaxAudioFrame *mixer = frame->submix;
            _intBuffers *ringbuffers;

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
               static struct timespec sleept = {0, 1000};
               float sleep = 0.1f / frame->submix->info->refresh_rate;
               int p = 0;

               /*
                * Can't call aaxAudioFrameWaitForBuffer because of a dead-lock
                * mixer->capturing is AAX_FALSE when a buffer is available
                */
               sleept.tv_nsec = sleep * 1e9f;
               ringbuffers = mixer->ringbuffers;
               while ((mixer->capturing == 1) && (p++ < 500))
               {
                  _intBufReleaseData(dptr, _AAX_FRAME);

                  nanosleep(&sleept, 0);

                  dptr = _intBufGet(hf, _AAX_FRAME, i);
                  if (!dptr) break;

                  frame = _intBufGetDataPtr(dptr);
               }
            } /* mixer->thread */

            if (dptr)
            {
               if (mixer->capturing > 1)
               {
                  _intBufferData *buf;
                  _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);
                  buf = _intBufPopData(ringbuffers, _AAX_RINGBUFFER);
                  _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);

                  if (buf)
                  {
                     unsigned int dno_samples;
                     unsigned char track, tracks;
                     _oalRingBuffer *src_rb;

                     dno_samples =_oalRingBufferGetNoSamples(dest_rb);
                     tracks=_oalRingBufferGetNoTracks(dest_rb);
                     src_rb = _intBufGetDataPtr(buf);

                     for (track=0; track<tracks; track++)
                     {
                        int32_t *data = dest_rb->sample->track[track];
                        int32_t *sptr = src_rb->sample->track[track];
                        _batch_fmadd(data, sptr, dno_samples, 1.0, 0.0);
                     }

                     _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);
                     _intBufPushData(ringbuffers, _AAX_RINGBUFFER, buf);
                     _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);
                     mixer->capturing = 1;
                  }
               }
               _intBufReleaseData(dptr, _AAX_FRAME);
            }
         }
#if 0
for (i=0; i<_oalRingBufferGetNoTracks(dest_rb); i++) {
   unsigned int dno_samples =_oalRingBufferGetNoSamples(dest_rb);
   int32_t *data = dest_rb->sample->track[i];
   _batch_saturate24(data, dno_samples);
}
#endif
      }
      _intBufReleaseNum(hf, _AAX_FRAME);
   }
   return num;
}

int
_aaxSoftwareMixerPlayFrame(void* frame, const void* backend, void* sensor, void* be_handle)
{
   const _aaxDriverBackend* be = (const _aaxDriverBackend*)backend;
   _aaxAudioFrame* mixer = (_aaxAudioFrame*)frame;
   _oalRingBuffer *dest_rb = mixer->ringbuffer;
   int res;

   /* postprocess registered audio frames */
   if (mixer->frames) {
      _aaxSoftwareMixerMixFrames(dest_rb, mixer->frames);
   }
   be->postprocess(be_handle, dest_rb, sensor);

   /* play all mixed audio */
   res = be->play(be_handle, 0, dest_rb, 1.0, 1.0);

   if TEST_FOR_TRUE(mixer->capturing)
   {
      unsigned int nbufs;

      _oalRingBufferForward(dest_rb);
      _intBufAddData(mixer->ringbuffers, _AAX_RINGBUFFER, dest_rb);

      nbufs = _intBufGetNum(mixer->ringbuffers, _AAX_RINGBUFFER);
      if (nbufs == 1) {
         dest_rb = _oalRingBufferDuplicate(dest_rb, AAX_FALSE);
      }
      else
      {
         void **ptr;

         ptr = _intBufShiftIndex(mixer->ringbuffers, _AAX_RINGBUFFER, 0, 1);
         assert(ptr != NULL);

         dest_rb = (_oalRingBuffer *)ptr[0];
         free(ptr);
      }
      _intBufReleaseNum(mixer->ringbuffers, _AAX_RINGBUFFER);
      mixer->ringbuffer = dest_rb;
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
   _aaxAudioFrame *mixer;
   _sensor_t* sensor;
   int res = 0;

   assert(handle);
   assert(handle->sensors);
   assert(handle->backend.ptr);
   assert(handle->info->no_tracks);

   dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      sensor = _intBufGetDataPtr(dptr_sensor);
      mixer = sensor->mixer;
      _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
   }
   else {
      return 0;
   }

   be = handle->backend.ptr;
   if (_IS_PLAYING(handle) || _IS_STANDBY(handle))
   {
      void* be_handle = handle->backend.handle;

      if (_IS_PLAYING(handle) && be->is_available(be_handle))
      {
         if (handle->info->mode == AAX_MODE_READ)
         {
            float dt;

            dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            dt = _aaxSoftwareMixerReadFrame(mixer, be, be_handle);
            mixer->curr_pos_sec += dt;
            _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
         }
         else if (mixer->emitters_3d || mixer->emitters_2d || mixer->frames)
         {
            _oalRingBuffer2dProps sp2d;
            _oalRingBuffer3dProps sp3d;

            /* signal frames to update */
            /* thread == -1: mixer; attached frames are threads */
            /* thread >=  0: frame; call updates manually       */
            if (mixer->thread < 0) {
               _aaxSoftwareMixerSignalFrames(mixer->frames);
            }

            /* copying here prevents locking the listener the whole time */
            /* it's used for just one time-frame anyhow                  */
            dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            sensor = _intBufGetDataPtr(dptr_sensor);
            memcpy(&sp2d, mixer->props2d, sizeof(_oalRingBuffer2dProps));
            memcpy(&sp2d.pos, handle->info->speaker,
                               _AAX_MAX_SPEAKERS*sizeof(vec4));
            memcpy(&sp2d.hrtf, handle->info->hrtf, 2*sizeof(vec4));
            memcpy(&sp3d, mixer->props3d, sizeof(_oalRingBuffer3dProps));
            _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

            _aaxSoftwareMixerProcessFrame(mixer->ringbuffer, handle->info,
                                           &sp2d, &sp3d, NULL, NULL,
                                           mixer->emitters_2d,
                                           mixer->emitters_3d,
                                           be, be_handle);

            res = _aaxSoftwareMixerPlayFrame(mixer, be, sensor, be_handle);
         }
      }
      else /* if (_IS_STANDBY(handle) */
      {
         if (handle->info->mode != AAX_MODE_READ)
         {
            if (mixer->emitters_3d || mixer->emitters_2d)
            {
               dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
               _aaxNoneDriverProcessFrame(mixer);
               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
            }
         }
      }
   }

   return res;
}

