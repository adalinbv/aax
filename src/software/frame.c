/*
 * Copyright 2012-2014 by Erik Hofman.
 * Copyright 2012-2014 by Adalin B.V.
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

#include <base/threads.h>
#include <base/buffers.h>

#include <objects.h>
#include <devices.h>
#include <api.h>

#include "arch.h"
#include "ringbuffer.h"
#include "audio.h"

void
_aaxAudioFrameMix(_aaxRingBuffer *dest_rb, _intBuffers *ringbuffers,
                  _aax2dProps *fp2d, 
                  const _aaxDriverBackend *be, void *be_handle)
{
   _intBufferData *buf;

   _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);

   buf = _intBufPopNormal(ringbuffers, _AAX_RINGBUFFER, AAX_TRUE);
   if (buf)
   {
      _aaxRingBuffer *src_rb  = _intBufGetDataPtr(buf);
      _aaxRingBufferLFOData *lfo;

      lfo = _FILTER_GET_DATA(fp2d, DYNAMIC_GAIN_FILTER);
      dest_rb->data_mix(dest_rb, src_rb, lfo);

      /*
       * push the ringbuffer to the back of the stack so it can
       * be used without the need to delete this one now and 
       * create a new ringbuffer later on.
       */
      _intBufPushNormal(ringbuffers, _AAX_RINGBUFFER, buf, AAX_TRUE);
   }

   _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);
}

/* -------------------------------------------------------------------------- */

static void *
_aaxAudioFrameSwapBuffers(void *rbuf, _intBuffers *ringbuffers, char dde)
{
   _aaxRingBuffer *rb = (_aaxRingBuffer*)rbuf;
   _aaxRingBuffer *nrb;
   _intBufferData *buf;
   
   _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);

   buf = _intBufPopNormal(ringbuffers, _AAX_RINGBUFFER, AAX_TRUE);
   if (buf)
   {
      nrb = _intBufSetDataPtr(buf, rb);
      if (dde) {
// TODO: don't copy dde data but audio-data instead, not only is it
//       most often less data, but it would get rid of this function.
         nrb->copy_effectsdata(nrb, rb);
      }

      _intBufPushNormal(ringbuffers, _AAX_RINGBUFFER, buf, AAX_TRUE);
   }
   else
   {
      nrb = rb->duplicate(rb, AAX_TRUE, dde);
      _intBufAddDataNormal(ringbuffers, _AAX_RINGBUFFER, rb, AAX_TRUE);
   }

   rb = nrb;
   assert(rb != NULL);
   _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);

   return rb;
}


/**
 * ssv: sensor sound velocity
 * sdf: sensor doppler factor
 * pp[23]d: parent object (mixer or audio-frame)
 * fp[23]d: current audio frame
 *
 * NOTE: fp3d must be a copy of the frames dprops3d structure since
 *       this function may alter it's contents.
 */
char
_aaxAudioFrameProcess(_aaxRingBuffer *dest_rb, void *sensor,
                      _aaxAudioFrame *fmixer, float ssv, float sdf,
                      _aax2dProps *pp2d,
                      _aaxDelayed3dProps *pdp3d_m,
                      _aax2dProps *fp2d,
                      _aaxDelayed3dProps *fdp3d,
                      _aaxDelayed3dProps *fdp3d_m,
                      const _aaxDriverBackend *be, void *be_handle,
                      char fprocess)
{
   _aaxRingBufferLFOData *lfo;
   char process;

   /* update the model-view matrix based on our own and that of out parent */
   if (pdp3d_m)
   {
      if (_PROP3D_MTX_HAS_CHANGED(pdp3d_m) || _PROP3D_MTX_HAS_CHANGED(fdp3d_m))
      {
         mtx4Mul(fdp3d_m->matrix, pdp3d_m->matrix, fdp3d->matrix);
#if 0
 printf("!  parent:\t\t\t\tframe:\n");
 PRINT_MATRICES(pdp3d_m->matrix, fdp3d->matrix);
 printf("!  modified frame\n");
 PRINT_MATRIX(fdp3d_m->matrix);
#endif

         _PROP3D_MTX_CLEAR_CHANGED(pdp3d_m);
         _PROP3D_MTX_SET_CHANGED(fdp3d_m);
      }

      if (_PROP3D_MTXSPEED_HAS_CHANGED(pdp3d_m) ||
          _PROP3D_MTXSPEED_HAS_CHANGED(fdp3d_m))
      {
         mtx4Mul(fdp3d_m->velocity, pdp3d_m->velocity, fdp3d->velocity);

#if 0
 printf("parent velocity:\t\t\tframe velocity:\n");
 PRINT_MATRICES(pdp3d_m->velocity, fdp3d->velocity);
 printf("modified frame velocity\n");
 PRINT_MATRIX(fdp3d_m->velocity);
#endif


         _PROP3D_SPEED_CLEAR_CHANGED(pdp3d_m);
         _PROP3D_SPEED_SET_CHANGED(fdp3d_m);
      }
   }

   lfo = _EFFECT_GET_DATA(fp2d, DYNAMIC_PITCH_EFFECT);
   if (lfo)
   {
      fp2d->final.pitch_lfo = lfo->get(lfo, NULL, NULL, 0, 0);
      fp2d->final.pitch_lfo -= lfo->min;
   } else {
      fp2d->final.pitch_lfo = 1.0f;
   }
   lfo = _FILTER_GET_DATA(fp2d, DYNAMIC_GAIN_FILTER);
   if (lfo && !lfo->envelope) {
      fp2d->final.gain_lfo = lfo->get(lfo, NULL, NULL, 0, 0);
   } else {
      fp2d->final.gain_lfo = 1.0f;
   }

   /** process possible registered emitters */
   process = _aaxEmittersProcess(dest_rb, fmixer->info, ssv, sdf, fp2d, fdp3d_m,
                                 fmixer->emitters_2d, fmixer->emitters_3d,
                                 be, be_handle);

   /** process registered sub-frames */
   if (fprocess && fmixer->frames)
   {
      _aaxRingBuffer *frame_rb = fmixer->ringbuffer;

      /*
       * Make sure there's a ringbuffer when at least one subframe is
       * registered. All subframes use this ringbuffer for rendering.
       */
      if (!frame_rb)
      {
         _aaxMixerInfo* info = fmixer->info;

         assert (be == sensor ? ((_sensor_t*)sensor)->mixer->info->backend
                              : fmixer->info->backend);

         frame_rb = be->get_ringbuffer(DELAY_EFFECTS_TIME, info->mode);
         if (frame_rb)
         {
            float dt = 1.0f/info->period_rate;

            dest_rb->set_parami(frame_rb, RB_NO_TRACKS, info->no_tracks);
            dest_rb->set_format(frame_rb, AAX_PCM24S, AAX_TRUE);
            dest_rb->set_paramf(frame_rb, RB_FREQUENCY, info->frequency);
            dest_rb->set_paramf(frame_rb, RB_DURATION_SEC, dt);
            dest_rb->init(frame_rb, AAX_TRUE);
            fmixer->ringbuffer = frame_rb;
         }
      }

      /* process registered (non threaded) sub-frames */
      if (frame_rb)
      {
         _intBuffers *hf = fmixer->frames;
         unsigned int i, max, cnt;

         max = _intBufGetMaxNum(hf, _AAX_FRAME);
         cnt = _intBufGetNumNoLock(hf, _AAX_FRAME);
         for (i=0; i<max; i++)
         {
            process = _aaxAudioFrameRender(dest_rb, fmixer, fp2d, fdp3d_m, hf,
                                           i, ssv, sdf, be, be_handle);
            if (process) --cnt;
            if (cnt == 0) break;
         }
         _intBufReleaseNum(hf, _AAX_FRAME);
      }
   }

   /** process registered devices */
   if (fmixer->devices)
   {
      _aaxMixerInfo* info = fmixer->info;
      _aaxSensorsProcess(dest_rb, fmixer->devices, fp2d, info->track);
      process = AAX_TRUE;
   }

   if (fprocess && process)
   {
      be->effects(be, be_handle, dest_rb, fp2d);
      be->postprocess(be_handle, dest_rb, sensor);
   }

   return process;
}

char
_aaxAudioFrameRender(_aaxRingBuffer *dest_rb, _aaxAudioFrame *fmixer, _aax2dProps *fp2d, _aaxDelayed3dProps *fdp3d_m, _intBuffers *hf, unsigned int i, float ssv, float sdf, const _aaxDriverBackend *be, void *be_handle)
{
   char process = AAX_FALSE;
   _intBufferData *dptr;

   dptr = _intBufGet(hf, _AAX_FRAME, i);
   if (dptr)
   {
      _aaxRingBuffer *frame_rb = fmixer->ringbuffer;
      _frame_t* subframe = _intBufGetDataPtr(dptr);
      _aaxAudioFrame *sfmixer = subframe->submix;
      _aaxDelayed3dProps *sfdp3d_m;
      _aaxDelayed3dProps *sfdp3d;
      _aax2dProps sfp2d;
      size_t size;
      int res;

      size = sizeof(_aaxDelayed3dProps);
      sfdp3d = _aax_aligned_alloc16(size);
      sfdp3d_m = _aax_aligned_alloc16(size);

      _aaxAudioFrameProcessDelayQueue(sfmixer);

      _aax_memcpy(&sfp2d, sfmixer->props2d,
                          sizeof(_aax2dProps));
      _aax_memcpy(sfdp3d, sfmixer->props3d->dprops3d,
                           sizeof(_aaxDelayed3dProps));
      _aax_memcpy(sfdp3d_m, sfmixer->props3d->m_dprops3d,
                             sizeof(_aaxDelayed3dProps));

      sfdp3d_m->state3d = sfdp3d->state3d;
      sfdp3d_m->pitch = sfdp3d->pitch;
      sfdp3d_m->gain = sfdp3d->gain;
      _PROP_CLEAR(sfmixer->props3d);
      _intBufReleaseData(dptr, _AAX_FRAME);

      /* read-only data */           
      _aax_memcpy(&sfp2d.speaker, fp2d->speaker,
                                  2*_AAX_MAX_SPEAKERS*sizeof(vec4_t));
      _aax_memcpy(&sfp2d.hrtf, fp2d->hrtf, 2*sizeof(vec4_t));

      /* clear the buffer for use by the subframe */
      dest_rb->set_state(frame_rb, RB_CLEARED);
      dest_rb->set_state(frame_rb, RB_STARTED);

      /*
       * frames render in the ringbuffer of their parent and mix with
       * dest_rb, this could potentialy save a lot of ringbuffers
       */
      res = _aaxAudioFrameProcess(frame_rb, NULL, sfmixer, ssv, sdf,
                                  fp2d, fdp3d_m, &sfp2d, sfdp3d, sfdp3d_m,
                                  be, be_handle, AAX_TRUE);

      /* if the subframe actually did render something, mix the data */
      if (res)
      {
         char dde = (_EFFECT_GET2D_DATA(sfmixer, DELAY_EFFECT) != NULL);
         _aaxDelayed3dProps *m_sfdp3d;

         fmixer->ringbuffer = _aaxAudioFrameSwapBuffers(frame_rb,
                                              sfmixer->frame_ringbuffers, dde);

         /* copy back the altered sfdp3d matrix and velocity vector */
         /* beware, they might have been altered in the mean time! */
         dptr = _intBufGet(hf, _AAX_FRAME, i);
         m_sfdp3d = sfmixer->props3d->m_dprops3d;
         if (!_PROP_MTX_HAS_CHANGED(sfmixer->props3d)) {
            mtx4Copy(m_sfdp3d->matrix, sfdp3d_m->matrix);
         }
         if (!_PROP_SPEED_HAS_CHANGED(sfmixer->props3d)) {
            mtx4Copy(m_sfdp3d->velocity, sfdp3d_m->velocity);
         }
         _intBufReleaseData(dptr, _AAX_FRAME);

         /* finally mix the data with dest_rb */
         _aaxAudioFrameMix(dest_rb, sfmixer->frame_ringbuffers, &sfp2d, 
                           be, be_handle);
         sfmixer->capturing = 1;

         process = AAX_TRUE;
      }

      _aax_aligned_free(sfdp3d);
      _aax_aligned_free(sfdp3d_m);
   }

   return process;
}

void
_aaxAudioFrameProcessDelayQueue(_aaxAudioFrame *frame)
{
   _aax3dProps* fp3d = frame->props3d;
   _aaxDelayed3dProps* fdp3d = fp3d->dprops3d;

   if (_PROP3D_DISTQUEUE_IS_DEFINED(fdp3d))
   {
      _aax2dProps* fp2d = frame->props2d;
      _aaxDelayed3dProps *tdp3d = NULL;
      _intBufferData *buf3dq;
      float pos3dq;

      _intBufAddData(frame->p3dq, _AAX_DELAYED3D, fdp3d);
      if (frame->curr_pos_sec > fp2d->dist_delay_sec)
      {
         pos3dq = fp2d->bufpos3dq;
         fp2d->bufpos3dq += fp3d->buf3dq_step;
         if (pos3dq > 0.0f)
         {
            do
            {
               buf3dq = _intBufPop(frame->p3dq, _AAX_DELAYED3D);
               if (buf3dq)
               {
                  tdp3d = _intBufGetDataPtr(buf3dq);
                  _aax_aligned_free(tdp3d);
               }
               --fp2d->bufpos3dq;
            }
            while (fp2d->bufpos3dq > 1.0f);
         }
      }

      if (!tdp3d) {			// TODO: get from buffer cache
         tdp3d = _aaxDelayed3dPropsDup(fdp3d);
      }
      fp3d->dprops3d = tdp3d;
   }
}

