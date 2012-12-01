/*
 * Copyright 2012 by Erik Hofman.
 * Copyright 2012 by Adalin B.V.
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
#include <assert.h>

#include <base/timer.h>		/* for gettimeofday */
#include <base/threads.h>
#include <base/buffers.h>
#include <ringbuffer.h>
#include <objects.h>
#include <arch.h>
#include <api.h>

#include "audio.h"

static void*
_aaxAudioFrameProcessThreadedFrame(_handle_t*, void*, _aaxAudioFrame*,
                                   _aaxAudioFrame*, _aaxAudioFrame*,
                                   const _aaxDriverBackend*);

void*
_aaxAudioFrameThread(void* config)
{
   _frame_t *frame = (_frame_t *)config;
   _aaxAudioFrame *smixer, *fmixer, *mixer;
   const _aaxDriverBackend *be;
   _oalRingBuffer *dest_rb;
   _handle_t* handle;
   unsigned int pos;
   struct timespec ts;
   float dt, delay_sec;
   float elapsed;
   int res = 0;

   if (!frame || !frame->submix->info->no_tracks) {
      return NULL;
   }
   handle = frame->handle;

   dest_rb = _oalRingBufferCreate(DELAY_EFFECTS_TIME);
   if (!dest_rb) {
      return NULL;
   }

   fmixer = NULL;
   smixer = frame->submix;
   delay_sec = 1.0f / smixer->info->refresh_rate;

   be = _aaxGetDriverBackendLoopback(&pos);     /* be = handle->backend.ptr */
   if (be)
   {
//    _oalRingBuffer *nrb;
      _aaxMixerInfo* info;
      int tracks;

      info = smixer->info;

      /* unregistered frames that are positional are mono */
      tracks = (!handle && _IS_POSITIONAL(frame)) ? 1 : info->no_tracks;
      _oalRingBufferSetNoTracks(dest_rb, tracks);

      _oalRingBufferSetFormat(dest_rb, be->codecs, AAX_PCM24S);
      _oalRingBufferSetFrequency(dest_rb, info->frequency);
      _oalRingBufferSetDuration(dest_rb, delay_sec);
      _oalRingBufferInit(dest_rb, AAX_TRUE);
      _oalRingBufferStart(dest_rb);
      frame->ringbuffer = dest_rb;
//    nrb = _oalRingBufferDuplicate(dest_rb, AAX_FALSE, AAX_FALSE);
//    _intBufAddData(smixer->ringbuffers, _AAX_RINGBUFFER, nrb);

   }

   mixer = smixer;
   if (handle)  /* frame is registered */
   {
      assert (handle->id == HANDLE_ID);

      if (handle->id == HANDLE_ID)
      {
         _intBufferData *dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            smixer = sensor->mixer;
            fmixer = frame->submix;
            mixer = fmixer;
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
   }
#if 0
   else /* frame is not registered */
   {
      mixer = smixer = fmixer = frame->submix;
      memcpy(&sp2d, smixer->props2d, sizeof(_oalRingBuffer2dProps));
      memcpy(&sp2d.pos, smixer->info->speaker,
                                     _AAX_MAX_SPEAKERS*sizeof(vec4_t));
      memcpy(&sp2d.hrtf, smixer->info->hrtf, 2*sizeof(vec4_t));
      memcpy(&sp3d, smixer->props3d, sizeof(_oalRingBuffer3dProps));
   }
#endif

   /* get real duration, it might have been altered for better performance */
   dest_rb = frame->ringbuffer;
   delay_sec = _oalRingBufferGetDuration(dest_rb);

   dt = 0.0f;
   elapsed = 0.0f;
   _aaxMutexLock(frame->thread.mutex);
   do
   {
#if 1
      float delay = delay_sec;                  /* twice as slow when standby */

//    if (_IS_STANDBY(frame)) delay *= 2;
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
         dt *= 1000000.0f;	/* to usec */
         dt += now.tv_usec;
         ts.tv_nsec = (long)rintf(dt*1000.0f);
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
         ts.tv_nsec = (long)rintf(dt*1e9f);
      }
      if (ts.tv_nsec >= 1000000000L)
      {
         ts.tv_sec++;
         ts.tv_nsec -= 1000000000L;
      }
#else
      clock_gettime(CLOCK_REALTIME, &ts);
#endif

      if TEST_FOR_FALSE(frame->thread.started) {
         break;
      }

      if (_IS_PLAYING(frame) || _IS_STANDBY(frame))
      {
         if (mixer->emitters_3d || mixer->emitters_2d || mixer->frames)
         {
            if (_IS_PLAYING(frame) && be->is_available(NULL))
            {
               _aaxAudioFrameProcessThreadedFrame(handle, frame->ringbuffer,
                                                  mixer, smixer, fmixer, be);
            }
            else { /* if (_IS_STANDBY(frame)) */
               _aaxNoneDriverProcessFrame(mixer);
            }
         }
      }

      /**
       * _aaxSoftwareMixerSignalFrames uses _aaxConditionSignal to let the
       * frame procede in advance, before the main thread starts mixing so
       * threads will be finished soon after the main thread.
       * As a result _aaxConditionWaitTimed may return 0 instead, which is
       * not a problem since earlier in the loop there is a test to see if
       * the thread really is finished and then breaks the loop.
       *
       * Note: the thread will not be signaled to start mixing if there's
       *       already a buffer in it's buffer queue.
       */
      res = _aaxConditionWaitTimed(frame->thread.condition,
                                   frame->thread.mutex, &ts);
   }
   while ((res == ETIMEDOUT) || (res == 0));

   _aaxMutexUnLock(frame->thread.mutex);
   _oalRingBufferStop(frame->ringbuffer);
   _oalRingBufferDelete(frame->ringbuffer);
   frame->ringbuffer = 0;

   return frame;
}

void
_aaxAudioFrameMix(_oalRingBuffer *dest_rb, _intBuffers *ringbuffers,
                  unsigned char *capturing, _oalRingBuffer2dProps *fp2d,
                  const _aaxDriverBackend *be, void *be_handle)
{
   _intBufferData *buf;

   _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);
   buf = _intBufPopData(ringbuffers, _AAX_RINGBUFFER);
   _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);

   if (buf)
   {
      _oalRingBufferLFOInfo *lfo;
      unsigned char track, tracks;
      unsigned int dno_samples;
      _oalRingBuffer *src_rb;
      float g = 1.0f;

      dno_samples = _oalRingBufferGetNoSamples(dest_rb);
      tracks = _oalRingBufferGetNoTracks(dest_rb);
      src_rb = _intBufGetDataPtr(buf);

      lfo = _FILTER_GET_DATA(fp2d, DYNAMIC_GAIN_FILTER);
      if (lfo && lfo->envelope)
      {
          g = 0.0f;
          for (track=0; track<tracks; track++)
          {
              int32_t *sptr = src_rb->sample->track[track];
              float gain =  lfo->get(lfo, sptr, track, dno_samples);

              if (lfo->inv) gain = 1.0f/g;
              g += gain;
          }
          g /= tracks;
      }

      for (track=0; track<tracks; track++)
      {
         int32_t *dptr = dest_rb->sample->track[track];
         int32_t *sptr = src_rb->sample->track[track];
         float gstep = 0.0f;

         _batch_fmadd(dptr, sptr, dno_samples, g, gstep);
         *capturing = 1;
      }

      /*
       * push the ringbuffer to the back of the stack so it can
       * be used without the need to delete this one now and 
       * create a new ringbuffer later on.
       */
      _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);
      _intBufPushData(ringbuffers, _AAX_RINGBUFFER, buf);
      _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);
   }
}

/* -------------------------------------------------------------------------- */

static void *
_aaxAudioFrameSwapBuffers(void *rb, _intBuffers *ringbuffers, unsigned char *signal, char dde)
{
   _oalRingBuffer *nrb;
   unsigned int nbuf;

   nbuf = _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);
   if (nbuf == 0)
   {
      nrb = _oalRingBufferDuplicate(rb, AAX_TRUE, dde);
      _intBufAddData(ringbuffers, _AAX_RINGBUFFER, rb);
   }
   else
   {	 /* switch ringbuffers */
      _intBufferData *buf = _intBufPopData(ringbuffers, _AAX_RINGBUFFER);

      if (buf)
      {
         nrb = _intBufSetDataPtr(buf, rb);
         _intBufPushData(ringbuffers, _AAX_RINGBUFFER, buf);

         if (dde) {
            _oalRingBufferCopyDelyEffectsData(nrb, rb);
         }
      }
      else
      {
         nrb = _oalRingBufferDuplicate(rb, AAX_TRUE, dde);
         _intBufAddData(ringbuffers, _AAX_RINGBUFFER, rb);
      }
   }

   rb = nrb;
   assert(rb != NULL);
   _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);

#if USE_CONDITION
   if (signal) {
      _aaxConditionSignal(signal);
   }
#else
   *signal = *signal + 1;
#endif

   return rb;
}

static char
_aaxAudioFrameProcess(_oalRingBuffer *dest_rb, _aaxAudioFrame *fmixer,
                      _oalRingBuffer2dProps *sp2d, _oalRingBuffer3dProps *sp3d,
                      _oalRingBuffer2dProps *pp2d, _oalRingBuffer3dProps *pp3d,
                      _oalRingBuffer2dProps *fp2d, _oalRingBuffer3dProps *fp3d,
                      const _aaxDriverBackend *be, void *be_handle)
{
   char process;

   /** process possible registered emitters */
   process = _aaxEmittersProcess(dest_rb, fmixer->info, sp2d, sp3d, pp2d, pp3d,
                                 fmixer->emitters_2d, fmixer->emitters_3d,
                                 be, be_handle);

   /** process registered devices */
   if (fmixer->devices)
   {
      _aaxSensorsProcess(dest_rb, fmixer->devices, fp2d);
      process = AAX_TRUE;
   }

   /** process registered sub-frames */
   if (fmixer->frames)
   {
      _oalRingBuffer *frame_rb = fmixer->ringbuffer;

      /*
       * Make sure there's a ringbuffer when at least one subframe is
       * registered. All subframes use this ringbuffer for rendering.
       */
      if (!frame_rb)
      {
         frame_rb = _oalRingBufferCreate(DELAY_EFFECTS_TIME);
         if (frame_rb)
         {
            _aaxMixerInfo* info = fmixer->info;

            _oalRingBufferSetNoTracks(frame_rb, info->no_tracks);
            _oalRingBufferSetFormat(frame_rb, be->codecs, AAX_PCM24S);
            _oalRingBufferSetFrequency(frame_rb, info->frequency);
            _oalRingBufferSetDuration(frame_rb, 1.0f / info->refresh_rate);
            _oalRingBufferInit(frame_rb, AAX_TRUE);
            fmixer->ringbuffer = frame_rb;
         }
      }

      /* process registered (non threaded) sub-frames */
      if (frame_rb)
      {
         _intBuffers *hf = fmixer->frames;
         _oalRingBuffer2dProps sfp2d;
         _oalRingBuffer3dProps sfp3d;
         unsigned int i, max, cnt;

         max = _intBufGetMaxNum(hf, _AAX_FRAME);
         cnt = _intBufGetNumNoLock(hf, _AAX_FRAME);
         for (i=0; i<max; i++)
         {
            _aaxAudioFrame *sfmixer;
            _intBufferData *dptr;
            _frame_t* subframe;
            char res = 0;

            /* clear the buffer for use by the subframe */
            _oalRingBufferClear(frame_rb);
            _oalRingBufferStart(frame_rb);

            /* process the subframe */
            dptr = _intBufGet(hf, _AAX_FRAME, i);
            if (!dptr) break;

            /* copy to prevent locking while walking the tree */
            subframe = _intBufGetDataPtr(dptr);
            sfmixer = subframe->submix;
            memcpy(&sfp3d, sfmixer->props3d, sizeof(_oalRingBuffer3dProps));
            memcpy(&sfp2d, sfmixer->props2d, sizeof(_oalRingBuffer2dProps));
            _intBufReleaseData(dptr, _AAX_FRAME);

            /*
             * frames render in the ringbuffer of their parent and mix with
             * dest_rb, this could potentialy save a lot of ringbuffers
             */
            res = _aaxAudioFrameProcess(frame_rb, sfmixer, sp2d, sp3d,
                                       fp2d, fp3d, &sfp2d, &sfp3d,
                                       be, be_handle);
            /* if the subframe actually did render something, mix the data */
            if (res)
            {
               char dde = (_EFFECT_GET2D_DATA(sfmixer, DELAY_EFFECT) != NULL);
               frame_rb = _aaxAudioFrameSwapBuffers(frame_rb,
                                                    sfmixer->frame_ringbuffers,
#if USE_CONDITION
                                                    NULL,
#else
                                                    &sfmixer->capturing,
#endif
                                                    dde);
               fmixer->ringbuffer = frame_rb;

               /* finally mix the data with dest_rb */
               _aaxAudioFrameMix(dest_rb, sfmixer->frame_ringbuffers,
                                 &sfmixer->capturing, &sfp2d, be, be_handle);

               process = AAX_TRUE;
      
               if (--cnt == 0) break;
            }
         }
         _intBufReleaseNum(hf, _AAX_FRAME);
      }
   }

   if (process)
   {
      be->effects(be_handle, dest_rb, fp2d);
      be->postprocess(be_handle, dest_rb, NULL);
   }

   return process;
}

static void*
_aaxAudioFrameProcessThreadedFrame(_handle_t* handle, void *frame_rb,
          _aaxAudioFrame *mixer, _aaxAudioFrame *smixer, _aaxAudioFrame *fmixer,
          const _aaxDriverBackend *be)
{
   void *be_handle = NULL;
   _oalRingBuffer2dProps sp2d;
   _oalRingBuffer3dProps sp3d;
   _oalRingBuffer2dProps fp2d;
   _oalRingBuffer3dProps fp3d;
   _intBufferData *dptr;
   char dde;

   assert(handle);
   assert(mixer); /* equals to fmixer for registered frames: always for now */
   assert(smixer); /* sensor mixer */
   assert(fmixer); /* frame submixer */

   be_handle = handle->backend.handle;

   /* copying prevents locking the listener the whole time */
   /* it's used for just one time-frame anyhow             */
   dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
   if (dptr)
   {
      memcpy(&sp3d, smixer->props3d, sizeof(_oalRingBuffer3dProps));
      memcpy(&sp2d, smixer->props2d, sizeof(_oalRingBuffer2dProps));
      memcpy(&sp2d.pos, smixer->info->speaker,_AAX_MAX_SPEAKERS*sizeof(vec4_t));
      memcpy(&sp2d.hrtf, smixer->info->hrtf, 2*sizeof(vec4_t));
      _intBufReleaseData(dptr, _AAX_SENSOR);
   }

   memcpy(&fp3d, fmixer->props3d, sizeof(_oalRingBuffer3dProps));
   memcpy(&fp2d, fmixer->props2d, sizeof(_oalRingBuffer2dProps));

   /* clear the buffer for use by the subframe */
   _oalRingBufferClear(frame_rb);
   _oalRingBufferStart(frame_rb);

   _aaxAudioFrameProcess(frame_rb, mixer, &sp2d, &sp3d, NULL, NULL,
                                    &fp2d, &fp3d, be, be_handle);

   dde = (_EFFECT_GET2D_DATA(fmixer, DELAY_EFFECT) != NULL);
   return _aaxAudioFrameSwapBuffers(frame_rb, fmixer->ringbuffers,
#if USE_CONDITION
                                              fmixer->frame_ready,
#else
                                              &fmixer->capturing,
#endif
                                              dde);
}

