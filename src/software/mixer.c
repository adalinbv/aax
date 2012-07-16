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

#include <math.h>		/* for floor, and rint */
#include <errno.h>		/* for ETIMEDOUT */
#include <assert.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>		/* for struct time */
#endif

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>
#include <base/threads.h>
#include <base/types.h>		/* for msecSleep */

#include "audio.h"


void
_aaxSoftwareMixerApplyEffects(const void *id, void *drb, const void *props2d)
{
   _oalRingBuffer2dProps *p2d = (_oalRingBuffer2dProps*)props2d;
   _oalRingBuffer *rb = (_oalRingBuffer *)drb;
   _oalRingBufferDelayEffectData* delay;
   _oalRingBufferFreqFilterInfo* freq_filter;
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
   unsigned int track, tracks;
   _oalRingBufferSample *rbd;
   char parametric, graphic;
   void *ptr = 0;
   char *p;

   assert(rb != 0);
   assert(rb->sample != 0);

   rbd = rb->sample;

   parametric = graphic = 0;
   if (sensor)
   {
      parametric = graphic = (_FILTER_GET_DATA(sensor, EQUALIZER_HF) != NULL);
      parametric &= (_FILTER_GET_DATA(sensor, EQUALIZER_LF) != NULL);
      graphic    &= (_FILTER_GET_DATA(sensor, EQUALIZER_LF) == NULL);

      if (parametric || graphic)
      {
         p = 0;
         ptr = _aax_malloc(&p, 2*rbd->track_len_bytes);
      }
   }

   /* set up this way because we always need to apply compression */
   tracks = rbd->no_tracks;
   for (track=0; track<tracks; track++)
   {
      int32_t *d1 = (int32_t *)rbd->track[track];
      unsigned int dmax = rbd->no_samples;

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
         _oalRingBufferEqualizerInfo *eq=_FILTER_GET_DATA(sensor, EQUALIZER_HF);
         _oalRingBufferFreqFilterInfo* filter;
         int32_t *d2 = (int32_t *)p;
         int32_t *d3 = d2 + dmax;
         int b = 6;

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

         tracks = info->no_tracks;
         _oalRingBufferSetNoTracks(dest_rb, tracks);
         _oalRingBufferSetFormat(dest_rb, be->codecs, AAX_PCM24S);
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
   _oalRingBufferLFOInfo *lfo;
   unsigned int num, stage;
   _intBuffers *he;
   float dt;

   dt = _oalRingBufferGetDuration(dest_rb);
   props2d = fp2d ? (_oalRingBuffer2dProps*)fp2d : (_oalRingBuffer2dProps*)sp2d;
   props3d = fp3d ? (_oalRingBuffer3dProps*)fp2d : (_oalRingBuffer3dProps*)sp2d;

   lfo = _EFFECT_GET_DATA(props2d, DYNAMIC_PITCH_EFFECT);
   if (lfo) {
      props2d->final.pitch_lfo = lfo->get(lfo, NULL, 0, 0);
   }
   lfo = _FILTER_GET_DATA(props2d, DYNAMIC_GAIN_FILTER);
   if (lfo && !lfo->envelope) {
      props2d->final.gain_lfo = lfo->get(lfo, NULL, 0, 0);
   }

   num = 0;
   stage = 0;
   he = e3d;
   do
   {
      unsigned int i, no_emitters;

      no_emitters = _intBufGetMaxNum(he, _AAX_EMITTER);
      num += no_emitters;

      for (i=0; i<no_emitters; i++)
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

                  --src->update_ctr;
                   
                  /* 3d mixing */
                  if (stage == 0)
                  {
                     assert(_IS_POSITIONAL(src));
                     if (!src->update_ctr) {
                        be->prepare3d(sp3d, fp3d, info, props2d, src);
                     }
                     if (src->curr_pos_sec >= props2d->delay_sec) {
                        rv = be->mix3d(be_handle, dest_rb, src_rb, src->props2d,
                                               props2d, emitter->track,
                                               src->update_ctr, nbuf);
                     }
                  }
                  else
                  {
                     assert(!_IS_POSITIONAL(src));
                     rv = be->mix2d(be_handle, dest_rb, src_rb, src->props2d,
                                           props2d, 1.0, 1.0, src->update_ctr,
                                           nbuf);
                  }

                  if (!src->update_ctr) {
                     src->update_ctr = src->update_rate;
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

      /*
       * stage == 0 is 3d positional audio
       * stage == 1 is stereo audio
       */
      if (stage == 0) {
         he = e2d;	/* switch to stereo */
      }
   }
   while (++stage < 2); /* process 3d positional and stereo emitters */

#if 0
   if (num) {
      be->effects(be_handle, dest_rb, props2d);
   }
#endif

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
         _intBufferData *dptr = _intBufGetNoLock(hf, _AAX_FRAME, i);
         if (dptr)
         {
            _frame_t* frame = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = frame->submix;

            if TEST_FOR_TRUE(mixer->capturing)
            {
               unsigned int nbuf;
               nbuf = _intBufGetNumNoLock(mixer->ringbuffers, _AAX_RINGBUFFER);
               if (!nbuf) {
                  _aaxConditionSignal(frame->thread.condition);
               }
            }
//          _intBufReleaseData(dptr, _AAX_FRAME);
         }
      }
      _intBufReleaseNum(hf, _AAX_FRAME);
   }
   return num;
}

void*
_aaxSoftwareMixerReadFrame(void *rb, const void* backend, void *handle, float *dt, float pos_sec)
{
   const _aaxDriverBackend* be = (const _aaxDriverBackend*)backend;
   _oalRingBuffer *dest_rb = (_oalRingBuffer*)rb;
   _oalRingBufferSample *rbd;
   int32_t **scratch;
   void *rv = rb;

   /*
    * dest_rb is thread specific and does not need a lock
    * Note:
    *  - The ringbuffer is singed 24-bit PCM, stereo or mono
    *  - capture functions should return the data in signed 24-bit
    */
   assert(dest_rb->sample);

   rbd = dest_rb->sample;
   scratch = (int32_t**)rbd->scratch;
   if (scratch)
   {
      unsigned int bps = rbd->bytes_sample;
      unsigned int ds = rbd->dde_samples;
      size_t frames, nframes;
      int res;

      nframes = frames = _oalRingBufferGetNoSamples(dest_rb);
      res = be->capture(handle, rbd->track, 0, &nframes,
                                scratch[0]-ds, ds+frames);
      if (res && nframes)
      {
         float pitch = (float)(nframes)/(float)(frames);
         unsigned int t, tracks;
         _oalRingBuffer *nrb;

         dest_rb->pitch_norm = pitch;
         nrb = _oalRingBufferDuplicate(dest_rb, AAX_FALSE, AAX_FALSE);

         tracks = rbd->no_tracks;
         for (t=0; t<tracks; t++)
         {
            int32_t *ptr = nrb->sample->track[t];
            int32_t *optr = rbd->track[t];
            _aax_memcpy(ptr-ds, optr-ds+frames, ds*bps);
         }
         rv = nrb;

         if (res < 0) *dt = 0.0f;
      }
      else {
         _oalRingBufferClear(dest_rb);
      }
   }

   return rv;
}

/*-------------------------------------------------------------------------- */

unsigned int
_aaxSoftwareMixerMixSensorsThreaded(void *dest, _intBuffers *hs)
{
   _oalRingBuffer *dest_rb = (_oalRingBuffer *)dest;
   unsigned int i, num = 0;

   if (hs)
   {
      num = _intBufGetMaxNum(hs, _AAX_DEVICE);
      for (i=0; i<num; i++)
      {
         _intBufferData *dptr = _intBufGet(hs, _AAX_DEVICE, i);
         if (dptr)
         {
            _handle_t* config = _intBufGetDataPtr(dptr);
            const _intBufferData* dptr_sensor;
            const _aaxDriverBackend* be;
            void* be_handle;

            be = config->backend.ptr;
            be_handle = config->backend.handle;

            dptr_sensor = _intBufGet(config->sensors, _AAX_SENSOR, 0);
            if (dptr_sensor)
            {
               _intBuffers *ringbuffers;
               _aaxAudioFrame *mixer;
               _sensor_t* sensor;
               unsigned int nbuf;

               sensor = _intBufGetDataPtr(dptr_sensor);
               mixer = sensor->mixer;
               ringbuffers = mixer->ringbuffers;
               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

               nbuf = _intBufGetNumNoLock(ringbuffers,_AAX_RINGBUFFER);
               if (nbuf)
               {
                  _intBufferData *buf;
                  _oalRingBuffer *src_rb;
                  unsigned int rv = 0;

                  buf = _intBufGet(ringbuffers, _AAX_RINGBUFFER, 0);
                  src_rb = _intBufGetDataPtr(buf);
                  do
                  {
                     rv = be->mix2d(be_handle, dest_rb, src_rb, mixer->props2d,
                                               NULL, 1.0f, 1.0f, 0, 0);
                     _intBufReleaseData(buf, _AAX_RINGBUFFER);

                     if (rv) /* always streaming */
                     {
                        void **ptr;

                        ptr =_intBufShiftIndex(ringbuffers,_AAX_RINGBUFFER,0,1);
                        if (ptr)
                        {
                           _oalRingBufferDelete(ptr[0]);
                           free(ptr);
                        }

                        if (--nbuf)
                        {
                           buf = _intBufGet(ringbuffers, _AAX_RINGBUFFER, 0);
                           src_rb = _intBufGetDataPtr(buf);
                        }
                        else rv = 0;
                     }
                  }
                  while(rv);
               }
            }
            _intBufReleaseData(dptr, _AAX_DEVICE);
         }
      }
      _intBufReleaseNum(hs, _AAX_DEVICE);
   }
   return num;
}

unsigned int
_aaxSoftwareMixerMixSensors(void *dest, const void *sensors, void *props2d)
{
   _oalRingBuffer *dest_rb = (_oalRingBuffer *)dest;
   unsigned int i, num = 0;
   if (sensors)
   {
      _intBuffers *hs = (_intBuffers *)sensors;

      num = _intBufGetMaxNum(hs, _AAX_DEVICE);
      for (i=0; i<num; i++)
      {
         _intBufferData *dptr = _intBufGet(hs, _AAX_DEVICE, i);
         if (dptr)
         {
            _handle_t* config = _intBufGetDataPtr(dptr);
            const _intBufferData* dptr_sensor;
            const _aaxDriverBackend* be;
            void* be_handle;

            be = config->backend.ptr;
            be_handle = config->backend.handle;
            dptr_sensor = _intBufGet(config->sensors, _AAX_SENSOR, 0);
            if (dptr_sensor)
            {
               const _intBufferData* sptr_rb;
               _oalRingBuffer *src_rb;
               _aaxAudioFrame *smixer;
               _sensor_t* sensor;
               _intBuffers *srbs;
               void *rv;
               float dt;

               sensor = _intBufGetDataPtr(dptr_sensor);
               smixer = sensor->mixer;
               src_rb = smixer->ringbuffer;
               dt = 1.0f / smixer->info->refresh_rate;
               srbs = smixer->ringbuffers;
               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

               rv = _aaxSoftwareMixerReadFrame(src_rb, be, be_handle, &dt,
                                               smixer->curr_pos_sec);
               if (dt == 0.0f)
               {
                  _SET_STOPPED(config);
                  _SET_PROCESSED(config); 
               }

               dptr_sensor = _intBufGet(config->sensors, _AAX_SENSOR, 0);
               if (dptr_sensor)
               {
                  if (rv != src_rb)
                  {
                     /**
                      * Add the new buffer to the buffer queue and pop the
                      * first buffer from the queue when needed (below).
                      * This way pitch effects (< 1.0) can be processed safely.
                      */
                     _oalRingBufferSetFrequency(src_rb,config->info->frequency);
                     _intBufAddData(srbs, _AAX_RINGBUFFER, src_rb);
                     smixer->ringbuffer = rv;
                  }
                  smixer->curr_pos_sec += dt;

                  sptr_rb = _intBufGet(srbs, _AAX_RINGBUFFER, 0);
                  if (sptr_rb)
                  {
                     _oalRingBuffer *ssr_rb = _intBufGetDataPtr(sptr_rb);
                     unsigned int rv = 0;

                     do
                     {
                        _oalRingBuffer2dProps *p2d;
                        _oalRingBufferLFOInfo *lfo;

                        p2d = (_oalRingBuffer2dProps*)props2d;
                        lfo = _EFFECT_GET_DATA(p2d, DYNAMIC_PITCH_EFFECT);
                        if (lfo) {
                           p2d->final.pitch_lfo = lfo->get(lfo, NULL, 0, 0);
                        }
                        lfo = _FILTER_GET_DATA(p2d, DYNAMIC_GAIN_FILTER);
                        if (lfo && !lfo->envelope) {
                           p2d->final.gain_lfo = lfo->get(lfo, NULL, 0, 0);
                        }
                        rv = be->mix2d(be_handle, dest_rb, ssr_rb,
                                    smixer->props2d, props2d, 1.0f, 1.0f, 0, 0);
                        _intBufReleaseData(sptr_rb, _AAX_RINGBUFFER);

                        if (rv)	/* always streaming */
                        {
                           unsigned int nbuf;

                           nbuf = _intBufGetNumNoLock(srbs, _AAX_RINGBUFFER);
                           if (nbuf)
                           {
                              void **ptr;
                              ptr = _intBufShiftIndex(srbs,_AAX_RINGBUFFER,0,1);
                              if (ptr)
                              {
                                 nbuf--;
                                 _oalRingBufferDelete(ptr[0]);
                                 free(ptr);
                              }
                           }

                           if (nbuf)
                           {
                              sptr_rb = _intBufGet(srbs,_AAX_RINGBUFFER,0);
                              ssr_rb = _intBufGetDataPtr(sptr_rb);
                              /* since rv == AAX_TRUE this will be unlocked 
                                 after be->mix2d */
                           }
                           else rv = 0;
                        }
                     }
                     while(rv);
                  }
                  _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
               }
            }
            _intBufReleaseData(dptr, _AAX_DEVICE);
         }
      }
      _intBufReleaseNum(hs, _AAX_DEVICE);
   }
   return num;
}

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
               float sleep_ms;
               int p = 0;

               /*
                * Can't call aaxAudioFrameWaitForBuffer because of a dead-lock
                */
               ringbuffers = mixer->ringbuffers;
               sleep_ms = _MAX(100.0f / mixer->info->refresh_rate, 1.0f);
               while ((mixer->capturing == 1) && (p++ < 500))
               {
                  _intBufReleaseData(dptr, _AAX_FRAME);

                  msecSleep(sleep_ms);

                  dptr = _intBufGet(hf, _AAX_FRAME, i);
                  if (!dptr) break;

                  frame = _intBufGetDataPtr(dptr);
               }
            } /* mixer->thread */

            ringbuffers = mixer->ringbuffers;
            if (dptr && mixer->capturing > 1)
            {
               _intBufferData *buf;

               _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);
               buf = _intBufPopData(ringbuffers, _AAX_RINGBUFFER);
               _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);

               if (buf)
               {
                  _oalRingBufferLFOInfo *lfo;
                  unsigned int dno_samples;
                  unsigned char track, tracks;
                  _oalRingBuffer *src_rb;

                  dno_samples = _oalRingBufferGetNoSamples(dest_rb);
                  tracks = _oalRingBufferGetNoTracks(dest_rb);
                  src_rb = _intBufGetDataPtr(buf);

                  lfo = _FILTER_GET_DATA(mixer->props2d,DYNAMIC_GAIN_FILTER);
                  for (track=0; track<tracks; track++)
                  {
                     int32_t *data = dest_rb->sample->track[track];
                     int32_t *sptr = src_rb->sample->track[track];
                     float g = 1.0f, gstep = 0.0f;

                     if (lfo && lfo->envelope) {
                        g = lfo->get(lfo, sptr, track, dno_samples);
                     }
                     _batch_fmadd(data, sptr, dno_samples, g, gstep);
                     mixer->capturing = 1;
                  }

                  /*
                   * push the ringbuffer to the back of the stack so it can
                   * be used without the need to create a new ringbuffer
                   * and delete this one now.
                   */
                  _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);
                  _intBufPushData(ringbuffers, _AAX_RINGBUFFER, buf);
                  _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);
               }
            }

            /*
             * dptr could be zero if it was removed while waiting for a new
             * buffer
             */
            if (dptr) _intBufReleaseData(dptr, _AAX_FRAME);
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
_aaxSoftwareMixerPlayFrame(void** rb, const void* sensors, const void* ringbuffers, const void* frames, void* props2d, const void* props3d, char capturing, const void* sensor, const void* backend, const void* be_handle)
{
   const _aaxDriverBackend* be = (const _aaxDriverBackend*)backend;
   _oalRingBuffer *dest_rb = (_oalRingBuffer *)*rb;
   int res;

   /** postprocess registered sensors and (threaded) audio frames */
   if (sensors) {
      _aaxSoftwareMixerMixSensors(dest_rb, sensors, props2d);
   }
   if (frames)
   {
      _intBuffers *mixer_frames = (_intBuffers*)frames;
      _aaxSoftwareMixerMixFrames(dest_rb, mixer_frames);
   }
   be->effects(be_handle, dest_rb, props2d);
   be->postprocess(be_handle, dest_rb, sensor);

   /** play back all mixed audio */
   res = be->play(be_handle, 0, dest_rb, 1.0, 1.0);

   if TEST_FOR_TRUE(capturing)
   {
      _intBuffers *mixer_ringbuffers = (_intBuffers*)ringbuffers;
      _oalRingBufferForward(dest_rb);
      _intBufAddData(mixer_ringbuffers, _AAX_RINGBUFFER, dest_rb);
      _intBufGetNumNoLock(mixer_ringbuffers, _AAX_RINGBUFFER);
      dest_rb = _oalRingBufferDuplicate(dest_rb, AAX_FALSE, AAX_TRUE);
      *rb = (void*)dest_rb;
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
               void *rv, *rb = mixer->ringbuffer;

               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
               rv = _aaxSoftwareMixerReadFrame(rb, be, be_handle, &dt,
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
                     mixer->ringbuffer = rv;
                  }
                  mixer->curr_pos_sec += dt;
                  _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
               }
            }
            else if (mixer->emitters_3d || mixer->emitters_2d || mixer->frames)
            {
               _oalRingBuffer2dProps sp2d;
               _oalRingBuffer3dProps sp3d;

               /** signal threaded frames to update (if necessary) */
               /* thread == -1: mixer; attached frames are threads */
               /* thread >=  0: frame; call updates manually       */
               if (mixer->thread < 0) {
                  _aaxSoftwareMixerSignalFrames(mixer->frames);
               }

               /* copying here prevents locking the listener the whole time */
               /* it's used for just one time-frame anyhow                  */
               memcpy(&sp2d, mixer->props2d, sizeof(_oalRingBuffer2dProps));
               memcpy(&sp2d.pos, handle->info->speaker,
                                  _AAX_MAX_SPEAKERS*sizeof(vec4_t));
               memcpy(&sp2d.hrtf, handle->info->hrtf, 2*sizeof(vec4_t));
               memcpy(&sp3d, mixer->props3d, sizeof(_oalRingBuffer3dProps));
               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

               /* main mixer */
               _aaxSoftwareMixerProcessFrame(mixer->ringbuffer, handle->info,
                                              &sp2d, &sp3d, NULL, NULL,
                                              mixer->emitters_2d,
                                              mixer->emitters_3d,
                                              be, be_handle);
 
               res = _aaxSoftwareMixerPlayFrame((void**)&mixer->ringbuffer,
                                                mixer->sensors,
                                                mixer->ringbuffers,
                                                mixer->frames,
                                                &sp2d, &sp3d, mixer->capturing,
                                                sensor, be, be_handle);
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

