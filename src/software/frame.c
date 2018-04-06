/*
 * Copyright 2012-2018 by Erik Hofman.
 * Copyright 2012-2018 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>		/* for ETIMEDOUT */
#include <assert.h>

#include <base/threads.h>
#include <base/buffers.h>
#include <dsp/filters.h>
#include <dsp/effects.h>
#include <dsp/lfo.h>

#include <objects.h>
#include <devices.h>
#include <api.h>

#include "arch.h"
#include "ringbuffer.h"
#include "rbuf_int.h"
#include "audio.h"

static char _aaxAudioFrameRender(_aaxRingBuffer*, _aaxAudioFrame*, _aax2dProps*, _aax3dProps*, _intBuffers*, unsigned int, float, float, const _aaxDriverBackend*,  void*, char);
static void* _aaxAudioFrameSwapBuffers(void*, _intBuffers*, char);

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
_aaxAudioFrameProcess(_aaxRingBuffer *dest_rb, _frame_t *subframe,
                     void *sensor,  _aaxAudioFrame *fmixer,
                     float ssv, float sdf, _aax2dProps *fp2d, _aax3dProps *fp3d,
                     _aaxDelayed3dProps *fdp3d,
                     const _aaxDriverBackend *be, void *be_handle,
                     char fprocess, char batched, char mono)
{
   _aaxDelayed3dProps *sdp3d_m = fp3d->root->m_dprops3d;
   _aaxDelayed3dProps *fdp3d_m = fp3d->m_dprops3d;
   _aaxLFOData *lfo;
   char process;

   /* Update the model-view matrix based on our own and that of out parent. */
   /* fp3d->parent == NULL means this is the sensor frame so no math there. */
   if (fp3d->parent)
   {
      _aaxDelayed3dProps *pdp3d_m = fp3d->parent->m_dprops3d;
      if (_PROP3D_MTX_HAS_CHANGED(fdp3d) ||
          _PROP3D_MTX_HAS_CHANGED(pdp3d_m))
      {
#ifdef ARCH32
         if (_IS_RELATIVE(fp3d)) {
            mtx4fMul(&fdp3d_m->matrix, &pdp3d_m->matrix, &fdp3d->matrix);
         } else {
            mtx4fMul(&fdp3d_m->matrix, &sdp3d_m->matrix, &fdp3d->matrix);
         }
         if (_PROP3D_OCCLUSION_IS_DEFINED(fdp3d_m)) {
            mtx4fInverseSimple(&fdp3d_m->imatrix, &fdp3d_m->matrix);
         }
#else
         if (_IS_RELATIVE(fp3d)) {
            mtx4dMul(&fdp3d_m->matrix, &pdp3d_m->matrix, &fdp3d->matrix);
         } else {
            mtx4dMul(&fdp3d_m->matrix, &sdp3d_m->matrix, &fdp3d->matrix);
         }
         if (_PROP3D_OCCLUSION_IS_DEFINED(fdp3d_m)) {
            mtx4dInverseSimple(&fdp3d_m->imatrix, &fdp3d_m->matrix);
         }
#endif
         _PROP3D_MTX_SET_CHANGED(fdp3d_m);
#if 0
 printf("!  modifed parent frame:\tframe:\n");
 PRINT_MATRICES(pdp3d_m->matrix, fdp3d->matrix);
 printf("!  modified frame:\n");
 PRINT_MATRIX(fdp3d_m->matrix);
#endif
      }

      if (_PROP3D_MTXSPEED_HAS_CHANGED(fdp3d) ||
          _PROP3D_MTXSPEED_HAS_CHANGED(pdp3d_m))
      {
          mtx4fMul(&fdp3d_m->velocity, &pdp3d_m->velocity, &fdp3d->velocity);
         _PROP3D_SPEED_SET_CHANGED(fdp3d_m);
#if 0
 printf("parent velocity:\t\t\tframe velocity:\n");
 PRINT_MATRICES(pdp3d_m->velocity, fdp3d->velocity);
 printf("modified frame velocity\n");
 PRINT_MATRIX(fdp3d_m->velocity);
#endif
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
   process = _aaxEmittersProcess(dest_rb, fmixer->info, ssv, sdf, fp2d, fp3d,
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

         frame_rb = be->get_ringbuffer(FRAME_REVERB_EFFECTS_TIME, info->mode);
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
            process = _aaxAudioFrameRender(dest_rb, fmixer,fp2d, fp3d, hf, i,
                                           ssv, sdf, be, be_handle, batched);
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
      _aaxSensorsProcess(dest_rb, fmixer->devices, fp2d, info->track, batched);
      process = AAX_TRUE;
   }

   if (fprocess && process)
   {
      be->effects(be, be_handle, dest_rb, fp2d, mono);
      be->postprocess(be, be_handle, dest_rb, sensor, subframe, fmixer->info);
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

      if (!tdp3d) {			// TODO: get from a buffer cache
         tdp3d = _aaxDelayed3dPropsDup(fdp3d);
      }
      fp3d->dprops3d = tdp3d;
   }
}

/* -------------------------------------------------------------------------- */

static void
_aaxAudioFrameMix(_aaxRingBuffer *dest_rb, _intBuffers *ringbuffers,
                  _aax2dProps *fp2d, char mono)
{
   _intBufferData *buf;

   _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);

   buf = _intBufPopNormal(ringbuffers, _AAX_RINGBUFFER, AAX_TRUE);
   if (buf)
   {
      _aaxRingBuffer *src_rb  = _intBufGetDataPtr(buf);
      _aaxLFOData *lfo;

      lfo = _FILTER_GET_DATA(fp2d, DYNAMIC_GAIN_FILTER);
      dest_rb->data_mix(dest_rb, src_rb, lfo, mono ? 1 : AAX_TRACK_ALL);

      /*
       * push the ringbuffer to the back of the stack so it can
       * be used without the need to delete this one now and 
       * create a new ringbuffer later on.
       */
      _intBufPushNormal(ringbuffers, _AAX_RINGBUFFER, buf, AAX_TRUE);
   }

   _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);
}

static void
_aaxAudioFrameMix3D(_aaxRingBuffer *dest_rb, _intBuffers *ringbuffers,
                    _aax2dProps *fp2d, vec3f_ptr sftmp, vec4f_ptr speaker,
                    const _aaxMixerInfo *info)
{
   vec3f_t sfpos;
   _intBufferData *buf;
   float dfact;

   dfact = _MIN(vec3fNormalize(&sfpos, sftmp), 1.0f);
   _aaxSetupSpeakersFromDistanceVector(&sfpos, dfact, speaker, fp2d, info);

   _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);
   buf = _intBufPopNormal(ringbuffers, _AAX_RINGBUFFER, AAX_TRUE);
   if (buf)
   {
      _aaxRingBuffer *src_rb  = _intBufGetDataPtr(buf);
      _aaxRingBufferData *srbi, *drbi;
      _aaxRingBufferSample *srbd, *drbd;
      CONST_MIX_PTRPTR_T sptr;
      size_t t, no_samples;

      srbi = src_rb->handle;
      srbd = srbi->sample;
      sptr = (CONST_MIX_PTRPTR_T)srbd->track;
      no_samples = dest_rb->get_parami(dest_rb, RB_NO_SAMPLES);

      drbi = dest_rb->handle;
      drbd = drbi->sample;

      for (t=0; t<drbd->no_tracks; t++) {
         fp2d->prev_gain[t] = 1.0f;
      }
      drbd->mix1n(drbd, sptr, info->router, fp2d, 0, 0, no_samples,
                  info->frequency, 1.0f, 1.0f, 1.0f, 0);
      /*
       * push the ringbuffer to the back of the stack so it can
       * be used without the need to delete this one now and 
       * create a new ringbuffer later on.
       */
      _intBufPushNormal(ringbuffers, _AAX_RINGBUFFER, buf, AAX_TRUE);
   }

   _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);
}

static char
_aaxAudioFrameRender(_aaxRingBuffer *dest_rb, _aaxAudioFrame *fmixer,
                     _aax2dProps *fp2d, _aax3dProps *fp3d,
                     _intBuffers *hf, unsigned int pos, float ssv, float sdf,
                     const _aaxDriverBackend *be, void *be_handle, char batched)
{
   char process = AAX_FALSE;
   _intBufferData *dptr;

   dptr = _intBufGet(hf, _AAX_FRAME, pos);
   if (dptr)
   {
      _aaxRingBuffer *frame_rb = fmixer->ringbuffer;
      _frame_t* subframe = _intBufGetDataPtr(dptr);
      _aaxAudioFrame *sfmixer = subframe->submix;
      _aaxDelayed3dProps *fdp3d_m = fp3d->m_dprops3d;
      _aaxDelayed3dProps sfdp3d, *sfdp3d_m;
      _aax2dProps sfp2d;
      _aax3dProps sfp3d;
      char mono, indoor;
      int res;

      _aaxAudioFrameProcessDelayQueue(sfmixer);

      _aax_memcpy(&sfp2d, sfmixer->props2d, sizeof(_aax2dProps));
      _aax_memcpy(&sfp3d, sfmixer->props3d, sizeof(_aax3dProps));
      _aax_memcpy(&sfdp3d, sfmixer->props3d->dprops3d,
                           sizeof(_aaxDelayed3dProps));
      sfdp3d_m = sfmixer->props3d->m_dprops3d;
      if (_PROP3D_MTX_HAS_CHANGED(sfmixer->props3d->dprops3d)) {
         _aax_memcpy(sfdp3d_m, sfmixer->props3d->dprops3d,
                              sizeof(_aaxDelayed3dProps));
      }
      sfp3d.root = fp3d->root;
      sfp3d.parent = fp3d;

      mono = _PROP3D_MONO_IS_DEFINED(fdp3d_m) ? AAX_TRUE : AAX_FALSE;
      indoor = _PROP3D_INDOOR_IS_DEFINED(fdp3d_m) ? AAX_TRUE : AAX_FALSE;

      _PROP_CLEAR(sfmixer->props3d);
      _intBufReleaseData(dptr, _AAX_FRAME);


      /* read-only data */
      _aax_memcpy(&sfp2d.speaker, fp2d->speaker,
                                  2*_AAX_MAX_SPEAKERS*sizeof(vec4f_t));
      _aax_memcpy(&sfp2d.hrtf, fp2d->hrtf, 2*sizeof(vec4f_t));

      /* clear the buffer for use by the subframe */
      dest_rb->set_state(frame_rb, RB_CLEARED);
      dest_rb->set_state(frame_rb, RB_STARTED);

      /*
       * frames render in the ringbuffer of their parent and mix with
       * dest_rb, this could potentialy save a lot of ringbuffers
       */
      res = _aaxAudioFrameProcess(frame_rb, subframe, NULL, sfmixer, ssv, sdf,
                                  &sfp2d, &sfp3d, &sfdp3d,
                                  be, be_handle, AAX_TRUE, batched, mono);
      _PROP3D_CLEAR(sfmixer->props3d->m_dprops3d);

      /* if the subframe actually did render something, mix the data */
      if (res)
      {
         char dde;

         dde = _EFFECT_GET2D_DATA(sfmixer, DELAY_EFFECT) ? AAX_TRUE : AAX_FALSE;
         fmixer->ringbuffer = _aaxAudioFrameSwapBuffers(frame_rb,
                                              sfmixer->frame_ringbuffers, dde);

         /* finally mix the data with dest_rb */
         if (indoor && !mono)
         {
            vec3f_t tmp;

#ifdef ARCH32
            vec3fCopy(&tmp, &sfdp3d_m->matrix.v34[LOCATION]);
#else
            vec3fFilld(tmp.v3, sfdp3d_m->matrix.v34[LOCATION].v3);
#endif
#if 0
 PRINT_VEC3(tmp);
#endif

            _aaxAudioFrameMix3D(dest_rb, sfmixer->frame_ringbuffers,
                                &sfp2d, &tmp, fp2d->speaker, fmixer->info);
         } else {
            _aaxAudioFrameMix(dest_rb, sfmixer->frame_ringbuffers,
                              &sfp2d, mono);
         }
         sfmixer->capturing = 1;

         process = AAX_TRUE;
      }
   }

   return process;
}

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
      nrb = rb->duplicate(rb, AAX_TRUE, AAX_TRUE);
      _intBufAddDataNormal(ringbuffers, _AAX_RINGBUFFER, rb, AAX_TRUE);
   }

   rb = nrb; 
   assert(rb != NULL);
   _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);

   return rb;
}

