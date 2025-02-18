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

#include <assert.h>

#include <objects.h>
#include <api.h>

#include <dsp/filters.h>
#include <dsp/effects.h>
#include <dsp/dsp.h>

#include "ringbuffer.h"
#include "rbuf_int.h"
#include "renderer.h"
#include "audio.h"


/**
 * The following code renders all emitters attached to an audio-frame object
 * (which includes the final mixer) into the ringbuffer of the audio frame.
 *
 * fp2d -> frames 2d properties structure
 * ssv -> sensor sound velocity
 * sdf -> sensor doppler factor
 */


/*
 * Threaded emitter rendering code using a thread pool with worker threads,
 * one thread for every physical CPU core.
 */
bool
_aaxEmittersProcess(_aaxRingBuffer *drb, const _aaxMixerInfo *info,
                    float ssv, float sdf,
                    _aax2dProps *fp2d, _aax3dProps *fp3d,
                    _intBuffers *e2d, _intBuffers *e3d,
                    const _aaxDriverBackend* be, void *be_handle)
{
   _aaxRenderer *render = be->render(be_handle);
   _aaxRendererData data;

   data.mode = THREAD_PROCESS_EMITTER;

   data.drb = drb;
   data.info = info;
   data.fp3d = fp3d;
   data.fp2d = fp2d;
   data.e2d = e2d;
   data.e3d = e3d;
   data.be = be;
   data.be_handle = be_handle;

   data.ssv = ssv;
   data.sdf = sdf;
   data.dt = drb->get_paramf(drb, RB_DURATION_SEC);

   data.callback = _aaxProcessEmitter;

   return render->process(render, &data);
}

int
_aaxProcessEmitter(_aaxRingBuffer *drb, _aaxRendererData *data, _intBufferData *dptr_src, unsigned int stage)
{
   _emitter_t *emitter;
   _aaxEmitter *src;
   int rv = false;

   drb->set_state(drb, RB_REWINDED);

   emitter = _intBufGetDataPtr(dptr_src);
   src = emitter->source;
   if (_IS_PLAYING(src->props3d))
   {
      _intBufferData *dptr_sbuf;
      unsigned int nbuf;
      int streaming;

      rv = true;
      nbuf = _intBufGetNum(src->buffers, _AAX_EMITTER_BUFFER);
      assert(nbuf > 0);

      streaming = (nbuf > 1) ? true : false;
      dptr_sbuf = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER,
                                           src->buffer_pos);
      if (dptr_sbuf)
      {
         _embuffer_t *embuf = _intBufGetDataPtr(dptr_sbuf);
         _aaxRingBuffer *srb = embuf->ringbuffer;
         float buffer_gain = embuf->buffer->gain;
         int res = 0;

         if (stage == 2) {
            data->be->prepare3d(src, data);
         }

         do
         {
            _aax2dProps *ep2d = src->props2d;

            if (_IS_STOPPED(src->props3d)) {
               srb->set_state(srb, RB_STOPPED);
            }
            else if (srb->get_parami(srb, RB_IS_PLAYING) == 0)
            {
               if (streaming) {
                  srb->set_state(srb, RB_STARTED_STREAMING);
               } else {
                  srb->set_state(srb, RB_STARTED);
               }
            }

            ep2d->curr_pos_sec = src->curr_pos_sec;
             
            /* 3d mixing */
            if (stage == 2)
            {
               assert(_IS_POSITIONAL(src->props3d));

               res = false;
               if (ep2d->curr_pos_sec >= ep2d->dist_delay_sec) {
                  res = drb->mix3d(drb, srb, ep2d, data, emitter->track,
                                             buffer_gain, src->history);
//                if (ep2d->final.silence) rv = false;
               }
            }
            else
            {
               assert(!_IS_POSITIONAL(src->props3d));
               res = drb->mix2d(drb, srb, data, ep2d, buffer_gain,src->history);
            }

            /*
             * The current buffer of the source has finished playing.
             * Decide what to do next.
             */
            if (res)
            {
               if (streaming)
               {
                  /* is there another buffer ready to play? */
                  if (++src->buffer_pos == nbuf)
                  {
                     /*
                      * The last buffer was processed, return to the
                      * first buffer or stop? 
                      */
                     if TEST_FOR_TRUE(emitter->looping) {
                        src->buffer_pos = 0;
                     }
                     else
                     {
                        _SET_STOPPED(src->props3d);
                        _SET_PROCESSED(src->props3d);
                        break;
                     }
                  }

                  res &= drb->get_parami(drb, RB_IS_PLAYING);
                  if (res)
                  {
                     _intBufReleaseData(dptr_sbuf,_AAX_EMITTER_BUFFER);
                     dptr_sbuf = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER,
                                            src->buffer_pos);
                     embuf = _intBufGetDataPtr(dptr_sbuf);
                     srb = embuf->ringbuffer;
                  }
               }
               else /* !streaming */
               {
                  _SET_PROCESSED(src->props3d);
                  break;
               }
            }
         }
         while (res);
         src->curr_pos_sec += data->dt;
         _intBufReleaseData(dptr_sbuf, _AAX_EMITTER_BUFFER);
      }
      _intBufReleaseNum(src->buffers, _AAX_EMITTER_BUFFER);
      drb->set_state(drb, RB_STARTED);
   }
   _intBufReleaseData(dptr_src, _AAX_EMITTER);

   return rv;
}

/**
 * ssv:     sensor velocity vector
 * de:      sensor doppler factor
 * speaker: parent frame p2d->speaker position
 * fdp3d_m: frame dp3d->dprops3d
 */
void
_aaxEmitterPrepare3d(_aaxEmitter *src, const void *data)
{
   const _aaxRendererData *renderer = (const _aaxRendererData*)data;
   const _aaxMixerInfo* info = renderer->info;
   const vec4f_ptr speaker = (const vec4f_ptr)info->speaker;
   const _aax3dProps *fp3d = renderer->fp3d;
   float ssv = renderer->ssv;
   float sdf = renderer->sdf;
   _aaxDelayed3dProps *sdp3d_m, *fdp3d_m;
   _aaxDelayed3dProps *edp3d, *edp3d_m;
   _aax3dProps *ep3d;
   _aax2dProps *ep2d;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(src);
   assert(info);

   fdp3d_m = fp3d->m_dprops3d;
   sdp3d_m = fp3d->root->m_dprops3d;

   ep3d = src->props3d;
   edp3d = ep3d->dprops3d;
   edp3d_m = ep3d->m_dprops3d;
   ep2d = src->props2d;

   /* aplly pitch and gain before it is pushed to the delay queue */
   edp3d->pitch = 1.0f; // _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_PITCH);
   edp3d->gain = 1.0f; // _FILTER_GET(ep2d, VOLUME_FILTER, AAX_GAIN);
   edp3d_m->state3d = edp3d->state3d;

   // _aaxEmitterProcessDelayQueue
   if (_PROP3D_DISTQUEUE_IS_DEFINED(edp3d))
   {
      _aaxDelayed3dProps *ndp3d = NULL;
      _intBufferData *buf3dq;
      float pos3dq;

      assert(src->p3dq);

      _intBufAddData(src->p3dq, _AAX_DELAYED3D, edp3d);
      if (src->curr_pos_sec >= ep2d->dist_delay_sec)
      {
         pos3dq = ep2d->bufpos3dq;
         ep2d->bufpos3dq += ep3d->buf3dq_step;
         if (pos3dq > 0.0f)
         {
            do
            {
               buf3dq = _intBufPop(src->p3dq, _AAX_DELAYED3D);
               if (buf3dq)
               {
                  ndp3d = _intBufGetDataPtr(buf3dq);
                  _intBufDestroyDataNoLock(buf3dq);
               }
               --ep2d->bufpos3dq;
            }
            while (ep2d->bufpos3dq > 1.0f);
         }
      }

      if (!ndp3d) {                  // TODO: get from buffer cache
         ndp3d = _aaxDelayed3dPropsDup(edp3d);
      }
      ep3d->dprops3d = ndp3d;
      edp3d = ep3d->dprops3d;
   }

   /* only update when the matrix and/or the velocity vector has changed */
   if (_PROP3D_MTXSPEED_HAS_CHANGED(edp3d) ||
       _PROP3D_MTXSPEED_HAS_CHANGED(fdp3d_m))
   {
      _aaxRingBufferVelocityEffectData *velocity;
      vec3f_t epos, tmp;
      float esv, vs;
      float dist_ef;
      float gain;
      float df;
      FLOAT pitch;

      pitch = (FLOAT)1.0f;
      gain = edp3d->gain;

      _PROP3D_SPEED_CLEAR_CHANGED(edp3d);
      _PROP3D_MTX_CLEAR_CHANGED(edp3d);

      /**
       * align the emitter with the parent frame.
       * (compensate for the parents direction offset)
       */
      if (_IS_RELATIVE(ep3d)) {
         mtx4dMul(&edp3d_m->matrix, &fdp3d_m->matrix, &edp3d->matrix);
      } else {
         mtx4dMul(&edp3d_m->matrix, &sdp3d_m->matrix, &edp3d->matrix);
      }
      vec3fFilld(tmp.v3, edp3d_m->matrix.v34[LOCATION].v3);
      dist_ef = vec3fNormalize(&epos, &tmp);

#if 0
 printf("# modified parent frame:\t\temitter:\n");
 PRINT_MATRICES(fdp3d_m->matrix, edp3d->matrix);
 printf(" modified emitter:\n");
 PRINT_MATRIX(edp3d_m->matrix);
 printf("# dist: %5.1f\n", dist_ef);
#endif

      /* calculate the sound velocity inbetween the emitter and the sensor */
      velocity = _EFFECT_GET_DATA(ep3d, VELOCITY_EFFECT);
      esv = _EFFECT_GET(ep3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);
      vs = (esv+ssv) / 2.0f;
      df = velocity->prepare(ep3d, edp3d, edp3d_m, fdp3d_m, &epos, dist_ef, vs, sdf);
      pitch *= df;

      /* distance attenuation and audio-cone support */
      gain *= _directional_prepare(ep3d, edp3d_m, fdp3d_m);
      gain *= _distance_prepare(ep2d, ep3d, fdp3d_m, &epos, dist_ef, speaker, info);

      // Only do distance attenuation frequency filtering if the emitter is
      // registered at the mixer or when the parent-frame is defined indoor.
      assert(info->unit_m > 0.0f);

      ep2d->final.k = 1.0f;
      if (fdp3d_m == sdp3d_m || _PROP3D_INDOOR_IS_DEFINED(fdp3d_m))
      {
         float dist_km;
         float fc;

         df = _MAX(df , 1.0f);
         gain /= df;

         dist_km = _MIN(df*dist_ef * info->unit_m / 5000.0f, 1.0f);
         fc = 22050.0f - (22050.0f-1000.0f)*sqrtf(dist_km);
         ep2d->final.k = _aax_movingaverage_compute(fc, info->frequency);
      }

      /*
       * Volume filter/Reverb effect: Occlusion at the the parent frame
       */
      do
      {
         _aaxRingBufferReverbData *reverb;

         reverb = _EFFECT_GET_DATA(fp3d, REVERB_EFFECT);
         if (reverb) {
            reverb->prepare(src, fp3d, reverb);
         }
         else
         {
            _aaxRingBufferOcclusionData *occlusion;

            occlusion = _FILTER_GET_DATA(fp3d, VOLUME_FILTER);
            if (occlusion) {
               occlusion->prepare(src, fp3d, occlusion);
            }
         }
      }
      while (0);

      ep2d->final.gain_min = _FILTER_GET(ep2d, VOLUME_FILTER, AAX_MIN_GAIN);
      ep2d->final.gain_max = _FILTER_GET(ep2d, VOLUME_FILTER, AAX_MAX_GAIN);
      ep2d->final.gain_3d = gain;
      ep2d->final.pitch = pitch;
   }
}

