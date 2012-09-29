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
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>		/* for struct time */
#endif
#include <assert.h>
#include <math.h>

#include <aax/aax.h>

#include <base/threads.h>
#include <base/types.h>		/* for msecSleep */
#include "api.h"
#include "arch.h"
#include "driver.h"

void
_aaxAudioFramePlayFrame(void* frame, const void* backend, void* sensor, void* be_handle)
{
   const _aaxDriverBackend* be = (const _aaxDriverBackend*)backend;
   _aaxAudioFrame* mixer = (_aaxAudioFrame*)frame;
   _oalRingBuffer *dest_rb = mixer->ringbuffer;
   _oalRingBuffer2dProps sp2d;
   _oalRingBuffer3dProps sp3d;

   /* copying here prevents locking the listener the whole time */
   /* it's used for just one time-frame anyhow                  */
   memcpy(&sp2d, mixer->props2d, sizeof(_oalRingBuffer2dProps));
   memcpy(&sp2d.pos, mixer->info->speaker, _AAX_MAX_SPEAKERS*sizeof(vec4_t));
   memcpy(&sp2d.hrtf, mixer->info->hrtf, 2*sizeof(vec4_t));
   memcpy(&sp3d, mixer->props3d, sizeof(_oalRingBuffer3dProps));

   /** process registered devices */
   if (mixer->devices) {
      _aaxSoftwareMixerMixSensors(dest_rb, mixer->devices, &sp2d);
   }

   /* postprocess registered (non threaded) audio frames */
   if (mixer->frames)
   {
      unsigned int i, num;
      _intBuffers *hf;

      hf = mixer->frames;
      num = _intBufGetMaxNum(hf, _AAX_FRAME);
      for (i=0; i<num; i++)
      {
         _intBufferData *dptr = _intBufGet(hf, _AAX_FRAME, i);
         if (dptr)
         {
            _frame_t* subframe = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *fmixer = subframe->submix;

            _intBufReleaseData(dptr, _AAX_FRAME);

            _aaxSoftwareMixerProcessFrame(dest_rb, mixer->info, &sp2d, &sp3d,
                                          fmixer->props2d, fmixer->props3d,
                                          fmixer->emitters_2d,
                                          fmixer->emitters_3d,
                                          be, NULL);

#if 0
            _aaxAudioFramePlayFrame(mixer, be, NULL, be_handle);
#else
            /** process registered audio-frames and sensors */
            if (fmixer->devices) {
               _aaxSoftwareMixerMixSensors(dest_rb, fmixer->devices, fmixer->props2d);
            }
            be->effects(be_handle, dest_rb, fmixer->props2d);
#endif
         }
      }
      _intBufReleaseNum(hf, _AAX_FRAME);
      _aaxSoftwareMixerMixFrames(dest_rb, mixer->frames);
   }

   be->effects(be_handle, dest_rb, mixer->props2d);
   be->postprocess(be_handle, dest_rb, sensor);

   if TEST_FOR_TRUE(mixer->capturing)
   {
      char dde = (_EFFECT_GET2D_DATA(mixer, DELAY_EFFECT) != NULL);
      _intBuffers *ringbuffers = mixer->ringbuffers;
      unsigned int nbuf; 
 
      nbuf = _intBufGetNumNoLock(ringbuffers, _AAX_RINGBUFFER);
      if (nbuf == 0)
      {
         _oalRingBuffer *nrb = _oalRingBufferDuplicate(dest_rb, AAX_TRUE, dde);
         _intBufAddData(ringbuffers, _AAX_RINGBUFFER, dest_rb);
         dest_rb = nrb;
      }
      else
      {
         _intBufferData *buf;
         _oalRingBuffer *nrb;

         _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);
         buf = _intBufPopData(ringbuffers, _AAX_RINGBUFFER);

         /* switch ringbuffers */
         nrb = _intBufSetDataPtr(buf, dest_rb);
         _intBufPushData(ringbuffers, _AAX_RINGBUFFER, buf);
         _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);

         if (dde) {
            _oalRingBufferCopyDelyEffectsData(nrb, dest_rb);
         }
         dest_rb = nrb;
      }
      mixer->ringbuffer = dest_rb;
      mixer->capturing++;
   }

   _oalRingBufferClear(dest_rb);
   _oalRingBufferStart(dest_rb);
}


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
   float delay_sec;
   float elapsed;
   float dt = 0.0f;
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

   be = _aaxGetDriverBackendLoopback(&pos);	/* be = handle->backend.ptr */
   if (be)
   {
      _aaxMixerInfo* info = smixer->info;
      int tracks;

      /* unregistered frames that are positional are mono */
      tracks = (!handle && _IS_POSITIONAL(frame)) ? 1 : info->no_tracks;
      _oalRingBufferSetNoTracks(dest_rb, tracks);

      _oalRingBufferSetFormat(dest_rb, be->codecs, AAX_PCM24S);
      _oalRingBufferSetFrequency(dest_rb, info->frequency);
      _oalRingBufferSetDuration(dest_rb, delay_sec);
      _oalRingBufferInit(dest_rb, AAX_TRUE);
      _oalRingBufferStart(dest_rb);
   }

   mixer = smixer;
   if (handle)	/* frame is registered */
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
   mixer->ringbuffer = dest_rb;
   delay_sec = _oalRingBufferGetDuration(dest_rb);

   elapsed = 0.0f;
   _aaxMutexLock(frame->thread.mutex);
   do
   {
      float delay = delay_sec;			/* twice as slow when standby */

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
         dt += now.tv_usec*1e-6f;
         ts.tv_nsec = (long)(dt*1e9f);
         if (ts.tv_nsec >= 1000000000)
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

      if TEST_FOR_FALSE(frame->thread.started) {
         break;
      }

      if (_IS_PLAYING(frame) || _IS_STANDBY(frame))
      {
         if (mixer->emitters_3d || mixer->emitters_2d || mixer->frames)
         {
            if (_IS_PLAYING(frame) && be->is_available(NULL)) {
               _aaxAudioFrameProcessFrame(handle,frame,mixer,smixer,fmixer,be);
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
   _oalRingBufferStop(mixer->ringbuffer);
   _oalRingBufferDelete(mixer->ringbuffer);
   mixer->ringbuffer = 0;

   return frame;
}

void
_aaxAudioFrameProcessFrame(_handle_t* handle, _frame_t *frame,
          _aaxAudioFrame *mixer, _aaxAudioFrame *smixer, _aaxAudioFrame *fmixer,
          const _aaxDriverBackend *be)
{
   void* be_handle = NULL;
   _oalRingBuffer2dProps sp2d;
   _oalRingBuffer3dProps sp3d;
   _oalRingBuffer2dProps fp2d;
   _oalRingBuffer3dProps fp3d;

   if (handle) /* frame is registered */
   {
      _handle_t* handle = frame->handle;
      _intBufferData *dptr;

       /* copying prevents locking the listener the whole time */
       /* it's used for just one time-frame anyhow             */
       dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
       if (dptr)
       {
          memcpy(&sp2d, smixer->props2d, sizeof(_oalRingBuffer2dProps));
          memcpy(&sp2d.pos, smixer->info->speaker, _AAX_MAX_SPEAKERS*sizeof(vec4_t));
          memcpy(&sp2d.hrtf, smixer->info->hrtf, 2*sizeof(vec4_t));
          memcpy(&sp3d, smixer->props3d, sizeof(_oalRingBuffer3dProps));
          _intBufReleaseData(dptr, _AAX_SENSOR);

       }
       be_handle = handle->backend.handle; 
   }
   memcpy(&fp3d, fmixer->props3d, sizeof(_oalRingBuffer3dProps));
   memcpy(&fp2d, fmixer->props2d, sizeof(_oalRingBuffer2dProps));
   memcpy(&fp2d.pos, fmixer->info->speaker, _AAX_MAX_SPEAKERS*sizeof(vec4_t));
   memcpy(&fp2d.hrtf, fmixer->info->hrtf, 2*sizeof(vec4_t));

   /** process threaded frames */
   _aaxSoftwareMixerProcessFrame(mixer->ringbuffer, mixer->info,
                                              &sp2d, &sp3d, &fp2d, &fp3d,
                                              mixer->emitters_2d,
                                              mixer->emitters_3d,
                                              be, be_handle);

   /** process registered audio-frames and sensors */
   _aaxAudioFramePlayFrame(mixer, be, NULL, be_handle);
}
