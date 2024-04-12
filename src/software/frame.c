/*
 * SPDX-FileCopyrightText: Copyright © 2012-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2012-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>		/* for ETIMEDOUT */
#include <assert.h>

#include <base/buffers.h>
#include <dsp/filters.h>
#include <dsp/effects.h>
#include <dsp/lfo.h>

#include <backends/driver.h>
#include <dsp/dsp.h>
#include <objects.h>
#include <api.h>

#include "arch.h"
#include "ringbuffer.h"
#include "renderer.h"
#include "rbuf_int.h"
#include "audio.h"

/* processes one single audio-frame */
static bool _aaxAudioFrameRender(_aaxRingBuffer*, _aaxAudioFrame*, _aax2dProps*, _aax3dProps*, _intBuffers*, unsigned int, float, float, const _aaxDriverBackend*,  void*, bool);
static void* _aaxAudioFrameSwapBuffers(void*, _intBuffers*, bool);


/**
 * process registered emitters, audio-frames and sensors
 * ssv: sensor sound velocity
 * sdf: sensor doppler factor
 * pp[23]d: parent object (mixer or audio-frame)
 * fp[23]d: current audio frame
 *
 * NOTE: fp3d must be a copy of the frames dprops3d structure since
 *       this function may alter it's contents.
 */
bool
_aaxAudioFrameProcess(_aaxRingBuffer *dest_rb, _frame_t *subframe,
                     void *sensor,  _aaxAudioFrame *fmixer,
                     float ssv, float sdf, _aax2dProps *fp2d, _aax3dProps *fp3d,
                     _aaxDelayed3dProps *fdp3d,
                     const _aaxDriverBackend *be, void *be_handle,
                     bool batched, bool mono)
{
   _aaxDelayed3dProps *sdp3d_m = fp3d->root->m_dprops3d;
   _aaxDelayed3dProps *fdp3d_m = fp3d->m_dprops3d;
   _aaxDelayed3dProps *pdp3d_m = NULL;
   _aaxMixerInfo *info = fmixer->info;
   _aaxLFOData *lfo;
   bool ssr = true;
   bool process;

   // Bail out if there is nothing to render
   _intBuffers *e2d = fmixer->emitters_2d;
   _intBuffers *e3d = fmixer->emitters_3d;
   bool active_emitters = ((e2d && _intBufGetNumNoLock(e2d, _AAX_EMITTER)) ||
                           (e3d && _intBufGetNumNoLock(e3d, _AAX_EMITTER)));

   _intBuffers *sensors = fmixer->devices;
   bool active_sensors = (sensors && _intBufGetNumNoLock(sensors, _AAX_SENSOR));

   _intBuffers *frames = fmixer->frames;
   bool active_frames = ((frames && _intBufGetNumNoLock(frames, _AAX_FRAME)) ||
                         (fmixer->reverb_dt > 0.0f));

// if (!active_emitters && !active_sensors && !active_frames) return false;

   /* Update the model-view matrix based on our own and that of out parent. */
   /* fp3d->parent == NULL means this is the sensor frame so no math there. */
   if (fp3d->parent)
   {
      ssr = false;
      pdp3d_m = fp3d->parent->m_dprops3d;
      if (_PROP3D_MTX_HAS_CHANGED(fdp3d) ||
          _PROP3D_MTX_HAS_CHANGED(pdp3d_m))
      {
         if (_IS_RELATIVE(fp3d)) {
            mtx4dMul(&fdp3d_m->matrix, &pdp3d_m->matrix, &fdp3d->matrix);
         } else {
            mtx4dMul(&fdp3d_m->matrix, &sdp3d_m->matrix, &fdp3d->matrix);
         }
         if (_PROP3D_OCCLUSION_IS_DEFINED(fdp3d_m)) {
            mtx4dInverseSimple(&fdp3d_m->imatrix, &fdp3d_m->matrix);
         }
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
   if (lfo) {
      fp2d->final.pitch_lfo *= lfo->get(lfo, NULL, NULL, 0, 0);
   }

   lfo = _FILTER_GET_DATA(fp2d, DYNAMIC_GAIN_FILTER);
   if (lfo && !lfo->envelope) {
      fp2d->final.gain_lfo *= lfo->get(lfo, NULL, NULL, 0, 0);
   }

   // Only do distance attenuation frequency filtering if the frame is
   // registered at the mixer or when the parent-frame is defined indoor.
   fp2d->final.k = 1.0f;
   if (info->unit_m > 0.0f && pdp3d_m && !_PROP3D_INDOOR_IS_DEFINED(pdp3d_m))
   {
      float dist_pf = vec3dMagnitude(&fdp3d_m->matrix.v34[LOCATION]);
      float dist_km = _MIN(dist_pf * info->unit_m / 1000.0f, 1.0f);
      float fc = 22050.0f - (22050.0f-1000.0f)*dist_km;
      fp2d->final.k = _aax_movingaverage_compute(fc, info->frequency);
   }

   /** process possible registered emitters */
   process = false;
   if (active_emitters)
   {
      process = _aaxEmittersProcess(dest_rb, info, ssv, sdf, fp2d, fp3d,
                                    fmixer->emitters_2d, fmixer->emitters_3d,
                                    be, be_handle);
   }

   /** process registered devices */
   if (active_sensors)
   {
      _aaxMixerInfo* info = fmixer->info;
      process |= _aaxSensorsProcess(dest_rb, fmixer->devices, fp2d, info->track,
                                    batched);
   }

   /** process registered sub-frames */
   if (active_frames)
   {
      _aaxRingBuffer *frame_rb = fmixer->ringbuffer;

      /*
       * Make sure there's a ringbuffer when at least one subframe is
       * registered. All subframes use this ringbuffer for rendering.
       */
      if (!frame_rb)
      {
         assert (be == sensor ? ((_sensor_t*)sensor)->mixer->info->backend
                              : fmixer->info->backend);

         frame_rb = be->get_ringbuffer(MAX_EFFECTS_TIME, info->mode);
         if (frame_rb)
         {
            float dt = 1.0f/info->period_rate;

            dest_rb->set_parami(frame_rb, RB_NO_TRACKS, info->no_tracks);
            dest_rb->set_format(frame_rb, AAX_PCM24S, true);
            dest_rb->set_paramf(frame_rb, RB_FREQUENCY, info->frequency);
            dest_rb->set_paramf(frame_rb, RB_DURATION_SEC, dt);
            dest_rb->init(frame_rb, true);
            fmixer->ringbuffer = frame_rb;
         }
      }

      /* process registered (non threaded) sub-frames */
      if (frame_rb)
      {
         _intBuffers *hf = fmixer->frames;
         int i, max, cnt;

         max = _intBufGetMaxNum(hf, _AAX_FRAME);
         cnt = _intBufGetNumNoLock(hf, _AAX_FRAME);
         for (i=0; i<max; i++)
         {
            bool res = _aaxAudioFrameRender(dest_rb, fmixer,fp2d, fp3d, hf, i,
                                            ssv, sdf, be, be_handle, batched);
            process |= res;
            if (res && --cnt == 0) break;
         }
         _intBufReleaseNum(hf, _AAX_FRAME);
      }
   }

   if (process)
   {
//    _aaxRenderer *render = be->render(be_handle);
      _aaxRendererData data;

      data.mode = THREAD_PROCESS_AUDIOFRAME;

      data.ssr = ssr;
      data.mono = mono;
      data.subframe = subframe;
      data.sensor = sensor;

      data.drb = dest_rb;
      data.info = fmixer->info;
      data.fp2d = fp2d;
      data.be = be;
      data.be_handle = be_handle;

      be->effects(&data);
      be->postprocess(&data);
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
                  _aax2dProps *fp2d, bool mono)
{
   _intBufferData *buf;

   _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);

   buf = _intBufPopNormal(ringbuffers, _AAX_RINGBUFFER, true);
   if (buf)
   {
      _aaxRingBuffer *src_rb  = _intBufGetDataPtr(buf);

      dest_rb->data_mix(dest_rb, src_rb, fp2d, mono ? 1 : AAX_TRACK_ALL);

      /*
       * push the ringbuffer to the back of the stack so it can
       * be used without the need to delete this one now and 
       * create a new ringbuffer later on.
       */
      _intBufPushNormal(ringbuffers, _AAX_RINGBUFFER, buf, true);
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
   buf = _intBufPopNormal(ringbuffers, _AAX_RINGBUFFER, true);
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

      assert(fp2d->final.k >= 0.9f);

      for (t=0; t<_AAX_MAX_SPEAKERS; t++) {
         fp2d->prev_gain[t] = 1.0f;
      }
      drbd->mix1n(drbd, sptr, info->router, fp2d, 0, 0, no_samples,
                  info->frequency, 1.0f, 1.0f, 1.0f, 0);
      /*
       * push the ringbuffer to the back of the stack so it can
       * be used without the need to delete this one now and 
       * create a new ringbuffer later on.
       */
      _intBufPushNormal(ringbuffers, _AAX_RINGBUFFER, buf, true);
   }

   _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);
}

/* process one single audio-frame, no emitters, no sensors */
static bool
_aaxAudioFrameRender(_aaxRingBuffer *dest_rb, _aaxAudioFrame *fmixer,
                     _aax2dProps *fp2d, _aax3dProps *fp3d,
                     _intBuffers *hf, unsigned int pos, float ssv, float sdf,
                     const _aaxDriverBackend *be, void *be_handle, bool batched)
{
   _aaxDelayed3dProps *fdp3d_m = fp3d->m_dprops3d;
   bool indoor = _PROP3D_INDOOR_IS_DEFINED(fdp3d_m) ? true : false;
   bool mono = _PROP3D_MONO_IS_DEFINED(fdp3d_m) ? true : false;
   bool process = false;
   bool res = false;
   _intBufferData *dptr;
   _aax2dProps sfp2d;

   dptr = _intBufGet(hf, _AAX_FRAME, pos);
   if (dptr)
   {
      _aaxRingBuffer *frame_rb = fmixer->ringbuffer;
      _frame_t* subframe = _intBufGetDataPtr(dptr);
      _aaxAudioFrame *sfmixer = subframe->submix;
      _aaxDelayed3dProps sfdp3d, *sfdp3d_m;
      _aax3dProps sfp3d;

      _aaxAudioFrameProcessDelayQueue(sfmixer);

      _aax_memcpy(&sfp2d, sfmixer->props2d, sizeof(sfp2d));
      _aax_memcpy(&sfp3d, sfmixer->props3d, sizeof(sfp3d));
      _aax_memcpy(&sfdp3d, sfmixer->props3d->dprops3d,
                           sizeof(sfdp3d));
      sfdp3d_m = sfmixer->props3d->m_dprops3d;
      if (_PROP3D_MTX_HAS_CHANGED(sfmixer->props3d->dprops3d)) {
         _aax_memcpy(sfdp3d_m, sfmixer->props3d->dprops3d,
                               sizeof(sfdp3d));
      }
      sfp3d.root = fp3d->root;
      sfp3d.parent = fp3d;

      _PROP_CLEAR(sfmixer->props3d);
      _intBufReleaseData(dptr, _AAX_FRAME);

      /* read-only data */
      _aax_memcpy(&sfp2d.speaker, fp2d->speaker, sizeof(sfp2d.speaker));
      _aax_memcpy(&sfp2d.hrtf, fp2d->hrtf, sizeof(sfp2d.hrtf));

      /* clear the buffer for use by the subframe */
      dest_rb->set_state(frame_rb, RB_CLEARED);
      dest_rb->set_state(frame_rb, RB_STARTED);

      /* update final stages */
      sfp2d.final.gain *= fp2d->final.gain;
      sfp2d.final.pitch *= fp2d->final.pitch;
      sfp2d.final.gain_lfo *= fp2d->final.gain_lfo;
      sfp2d.final.pitch_lfo *= fp2d->final.pitch_lfo;

      /*
       * frames render in the ringbuffer of their parent and mix with
       * dest_rb, this could potentialy save a lot of ringbuffers
       */
      res = _aaxAudioFrameProcess(frame_rb, subframe, NULL, sfmixer, ssv, sdf,
                                  &sfp2d, &sfp3d, &sfdp3d,
                                  be, be_handle, batched, mono);
      if (fmixer->reverb_time > 0.0f)
      {
         if (!res)
         {
            if (fmixer->reverb_dt < fmixer->reverb_time)
            {
               fmixer->reverb_dt += 1.0f/fmixer->info->refresh_rate;
               res = true;
            }
         }
         else if (fmixer->reverb_dt > 0.0f) {
            fmixer->reverb_dt = 0.0f;
         }
      }
      _PROP3D_CLEAR(sfmixer->props3d->m_dprops3d);


      /* if the subframe actually did render something, mix the data */
      if (res)
      {
         bool dde = false;

         if (_EFFECT_GET2D_DATA(sfmixer, DELAY_EFFECT) ||
             _EFFECT_GET2D_DATA(sfmixer, DELAY_LINE_EFFECT) || 
             _EFFECT_GET3D_DATA(sfmixer, REVERB_EFFECT))
         {
            dde = true;
         }
         fmixer->ringbuffer = _aaxAudioFrameSwapBuffers(frame_rb,
                                             sfmixer->frame_ringbuffers, dde);
         process = true;
      }
   }

   if (res)
   {
      dptr = _intBufGet(hf, _AAX_FRAME, pos);
      if (dptr)
      {
         _frame_t* subframe = _intBufGetDataPtr(dptr);
         _aaxAudioFrame *sfmixer = subframe->submix;

         if (!process) {
            _aax_memcpy(&sfp2d, sfmixer->props2d, sizeof(sfp2d));
         }
         _intBufReleaseData(dptr, _AAX_FRAME);

         /* finally mix the data with dest_rb */
         if (indoor && !mono)
         {
            _aaxDelayed3dProps *sfdp3d_m = sfmixer->props3d->m_dprops3d;
            vec3f_t tmp;

            vec3fFilld(tmp.v3, sfdp3d_m->matrix.v34[LOCATION].v3);
#if 0
 PRINT_VEC3(tmp);
#endif

            _aaxAudioFrameMix3D(dest_rb, sfmixer->frame_ringbuffers,
                                &sfp2d, &tmp, fp2d->speaker, fmixer->info);
         } else {
            _aaxAudioFrameMix(dest_rb, sfmixer->frame_ringbuffers,
                              &sfp2d, mono);
         }

         sfmixer->capturing = true; // sfmixer->capturing++
         process = true;
      }
   }

   return process;
}

static void *
_aaxAudioFrameSwapBuffers(void *rbuf, _intBuffers *ringbuffers, bool dde)
{
   _aaxRingBuffer *rb = (_aaxRingBuffer*)rbuf;
   _aaxRingBuffer *nrb;
   _intBufferData *buf;

   _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);

   buf = _intBufPopNormal(ringbuffers, _AAX_RINGBUFFER, true);
   if (buf)
   {
      nrb = _intBufSetDataPtr(buf, rb);
      if (dde) {
// TODO: don't copy dde data but audio-data instead, not only is it
//       most often less data, but it would get rid of this function.
         nrb->copy_effectsdata(nrb, rb);
      }

      _intBufPushNormal(ringbuffers, _AAX_RINGBUFFER, buf, true);
   }
   else
   {
      nrb = rb->duplicate(rb, true, true);
      _intBufAddDataNormal(ringbuffers, _AAX_RINGBUFFER, rb, true);
   }

   rb = nrb; 
   assert(rb != NULL);
   _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);

   return rb;
}

