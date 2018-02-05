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

#include <assert.h>

#include <objects.h>
#include <api.h>

#include <dsp/filters.h>
#include <dsp/effects.h>

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
char
_aaxEmittersProcess(_aaxRingBuffer *drb, const _aaxMixerInfo *info,
                    float ssv, float sdf, _aaxDelayed3dProps *sdp3d_m,
                    _aax2dProps *fp2d, _aax3dProps *fp3d,
                    _aaxDelayed3dProps *fdp3d_m, _aaxDelayed3dProps *pdp3d_m,
                    _intBuffers *e2d, _intBuffers *e3d,
                    const _aaxDriverBackend* be, void *be_handle)
{
   _aaxRenderer *render = be->render(be_handle);
   _aaxRendererData data;
   char rv = AAX_TRUE;

   data.drb = drb;
   data.info = info;
   data.sdp3d_m = sdp3d_m;
   data.pdp3d_m = pdp3d_m;
   data.fdp3d_m = fdp3d_m;
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

   render->process(render, &data);

   return rv;
}

int
_aaxProcessEmitter(_aaxRingBuffer *drb, _aaxRendererData *data, _intBufferData *dptr_src, unsigned int stage)
{
   _emitter_t *emitter;
   _aaxEmitter *src;
   int rv = AAX_FALSE;

   drb->set_state(drb, RB_REWINDED);

   emitter = _intBufGetDataPtr(dptr_src);
   src = emitter->source;
   if (_IS_PLAYING(src->props3d))
   {
      _intBufferData *dptr_sbuf;
      unsigned int nbuf;
      int streaming;

      rv = AAX_TRUE;
      nbuf = _intBufGetNum(src->buffers, _AAX_EMITTER_BUFFER);
      assert(nbuf > 0);

      streaming = (nbuf > 1) ? AAX_TRUE : AAX_FALSE;
      dptr_sbuf = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER,
                                           src->buffer_pos);
      if (dptr_sbuf)
      {
         _embuffer_t *embuf = _intBufGetDataPtr(dptr_sbuf);
         _aaxRingBuffer *srb = embuf->ringbuffer;
         int res = 0;
         int ctr;

         if (embuf->buffer->aaxs != embuf->aaxs)
         {
            embuf->aaxs = embuf->buffer->aaxs;
            _emitterCreateEFFromAAXS(emitter, embuf, embuf->aaxs);
         }

         ctr = --src->update_ctr;
         // TODO: delayed updates causes emitter/frame status mismatch.
         if (1) // ctr == 0)
         {
            if (stage == 2)
            {
               data->be->prepare3d(src, data->info, data->ssv, data->sdf,
                                   data->fp2d->speaker, data->sdp3d_m,
                                   data->fp3d, data->fdp3d_m, data->pdp3d_m);

            }
            src->update_ctr = src->update_rate;
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

               res = AAX_FALSE;
               if (ep2d->curr_pos_sec >= ep2d->dist_delay_sec) {
                  res = drb->mix3d(drb, srb, ep2d, data, emitter->track, ctr,
                                             src->history);
               }
            }
            else
            {
               assert(!_IS_POSITIONAL(src->props3d));
               res = drb->mix2d(drb, srb, data->info, ep2d, data->fp2d,  ctr,
                                          src->history);
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
_aaxEmitterPrepare3d(_aaxEmitter *src,  const _aaxMixerInfo* info, float ssv, float sdf, vec4f_t *speaker, _aaxDelayed3dProps *sdp3d_m, _aax3dProps *fp3d, _aaxDelayed3dProps *fdp3d_m, _aaxDelayed3dProps *pdp3d_m)
{
   _aaxRingBufferPitchShiftFn* dopplerfn;
   _aaxDelayed3dProps *edp3d, *edp3d_m;
   _aax3dProps *ep3d;
   _aax2dProps *ep2d;
   _aaxRingBufferDistFn* distfn;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(src);
   assert(info);

   ep3d = src->props3d;
   edp3d = ep3d->dprops3d;
   edp3d_m = ep3d->m_dprops3d;
   ep2d = src->props2d;

   /* aplly pitch and gain before it is pushed to the delay queue */
   edp3d->pitch = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_PITCH);
   edp3d->gain = _FILTER_GET(ep2d, VOLUME_FILTER, AAX_GAIN);

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

   *(void**)(&distfn) = _FILTER_GET_DATA(ep3d, DISTANCE_FILTER);
   *(void**)(&dopplerfn) = _EFFECT_GET_DATA(ep3d, VELOCITY_EFFECT);
   assert(dopplerfn);
   assert(distfn);

   /* only update when the matrix and/or the velocity vector has changed */
   if (_PROP3D_MTXSPEED_HAS_CHANGED(edp3d) ||
       _PROP3D_MTXSPEED_HAS_CHANGED(fdp3d_m))
   {
      vec3f_t epos, tmp;
      _aaxRingBufferOcclusionData *direct_path;
      float refdist, maxdist, rolloff;
      float gain, pitch;
      float min, max;
      float esv, vs;
      float dist_ef;

      _PROP3D_SPEED_CLEAR_CHANGED(edp3d);
      _PROP3D_MTX_CLEAR_CHANGED(edp3d);

      /**
       * align the emitter with the parent frame.
       * (compensate for the parents direction offset)
       */
#ifdef ARCH32
      if (_IS_RELATIVE(ep3d)) {
         mtx4fMul(&edp3d_m->matrix, &fdp3d_m->matrix, &edp3d->matrix);
      } else {
         mtx4fMul(&edp3d_m->matrix, &pdp3d_m->matrix, &edp3d->matrix);
      }
      vec3fCopy(&tmp, &edp3d_m->matrix.v34[LOCATION]);
#else
      if (_IS_RELATIVE(ep3d)) {
         mtx4dMul(&edp3d_m->matrix, &fdp3d_m->matrix, &edp3d->matrix);
      } else {
         mtx4dMul(&edp3d_m->matrix, &pdp3d_m->matrix, &edp3d->matrix);
      }
      vec3fFilld(tmp.v3, edp3d_m->matrix.v34[LOCATION].v3);
#endif
      dist_ef = vec3fNormalize(&epos, &tmp);

#if 0
 printf("# modified parent frame:\t\temitter:\n");
 PRINT_MATRICES(fdp3d_m->matrix, edp3d->matrix);
 printf(" modified emitter:\n");
 PRINT_MATRIX(edp3d_m->matrix);
 printf("# dist: %f\n", dist_ef);
#endif

      /* calculate the sound velocity inbetween the emitter and the sensor */
      esv = _EFFECT_GET(ep3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);
      vs = (esv+ssv) / 2.0f;

      /*
       * Doppler
       */
      pitch = 1.0f;
      if (dist_ef > 1.0f)
      {
         float ve, vf, df;

         /* align velocity vectors with the modified emitter position
          * relative to the sensor
          */
         mtx4fMul(&edp3d_m->velocity, &fdp3d_m->velocity, &edp3d->velocity);

         vf = 0.0f;
         ve = vec3fDotProduct(&edp3d_m->velocity.v34[LOCATION], &epos);
         df = dopplerfn(vf, ve, vs/sdf);
#if 0
# if 1
 printf("velocity: %3.2f, %3.2f, %3.2f\n",
            edp3d_m->velocity[LOCATION][0],
            edp3d_m->velocity[LOCATION][1],
            edp3d_m->velocity[LOCATION][2]);
 printf("modified velocity:\t\t\tparent velocity:\n");
 PRINT_MATRICES(edp3d_m->velocity, fdp3d_m->velocity);
 printf("doppler: %f, ve: %f, vs: %f\n\n", df, ve, vs/sdf);
# else
 printf("doppler: %f, ve: %f, vs: %f\n", df, ve, vs/sdf);
# endif
#endif
         pitch *= df;
         ep3d->buf3dq_step = df;
      }
      ep2d->final.pitch = pitch;

      /*
       * Distance queues for every speaker (volume)
       */
      gain = edp3d->gain;

      refdist = _FILTER_GETD3D(src, DISTANCE_FILTER, AAX_REF_DISTANCE);
      maxdist = _FILTER_GETD3D(src, DISTANCE_FILTER, AAX_MAX_DISTANCE);
      rolloff = _FILTER_GETD3D(src, DISTANCE_FILTER, AAX_ROLLOFF_FACTOR);

      // If the parent frame is defined indoor then directional sound
      // propagation goes out the door. Note that the scenery frame is
      // never defined as indoor so emitters registered with the mixer
      // will always be directional.
      if (!_PROP3D_INDOOR_IS_DEFINED(fdp3d_m))
      {
         float dfact = _MIN(dist_ef/refdist, 1.0f);
         _aaxSetupSpeakersFromDistanceVector(epos, dfact, speaker, ep2d, info);
      }

      gain *= distfn(dist_ef, refdist, maxdist, rolloff, vs, 1.0f);

      /*
       * Occlusion
       */
      assert(sdp3d_m != fdp3d_m);
      if (pdp3d_m)
      {
         direct_path = _EFFECT_GET_DATA(fp3d, REVERB_EFFECT);
         if (!direct_path) direct_path = _FILTER_GET_DATA(fp3d, VOLUME_FILTER);
         if (direct_path)
         {
            vec3f_t vres;

#ifdef ARCH32
            vec3f_t fpepos, fpevec, pevec;
            float mag_pev;

            vec3fSub(&fpevec, &fdp3d_m->matrix.v34[LOCATION],
                              &edp3d_m->matrix.v34[LOCATION]);

            vec3fSub(&pevec, &pdp3d_m->matrix.v34[LOCATION],
                             &edp3d_m->matrix.v34[LOCATION]);
            vec3fNormalize(&pevec, &pevec);

            mag_pev = vec3fDotProduct(&fpevec, &pevec);
            vec3fScalarMul(&fpevec, &pevec, mag_pev);

            vec3fAdd(&fpepos, &edp3d_m->matrix.v34[LOCATION], &fpevec);
            vec3fSub(&vres, &fdp3d_m->matrix.v34[LOCATION], &fpepos);
#else
            vec3d_t fpepos, fpevec, pevec;
            double mag_pev;

            // calculate the frame-to-emitter vector.
            // fdp3d_m specifies the absolute position of the frame.
            vec3dSub(&fpevec, &fdp3d_m->matrix.v34[LOCATION],
                              &edp3d_m->matrix.v34[LOCATION]);

            // calculate the sensor-to-emitter unit vector.
            // pdp3d_m specifies the absolute position of the parent_frame.
            // edp3d_m specifies the absolute position of the emitter.
            vec3dSub(&pevec, &pdp3d_m->matrix.v34[LOCATION],
                             &edp3d_m->matrix.v34[LOCATION]);
            vec3dNormalize(&pevec, &pevec);

            // get the projection length of the frame-to-emitter vector on
            // the sensor-to-emitter unit vector.
            mag_pev = vec3dDotProduct(&fpevec, &pevec);

            // scale the sensor-to-emitter unit vector.
            vec3dScalarMul(&fpevec, &pevec, mag_pev);

            // get the vector from the frame position which perpendicular to
            // the sensor-to-emitter vector.
            vec3dAdd(&fpepos, &edp3d_m->matrix.v34[LOCATION], &fpevec);

            vec3dSub(&fpevec, &fdp3d_m->matrix.v34[LOCATION], &fpepos);
            vec3fFilld(vres.v3, fpevec.v3);
#endif
            /*
             * Squared distance between the emiters parent-frame position
             * and the line from the emitter to the parents-frame parent-frame
             * position using Pythagoras.
             */
#if 1
 printf("obstruction dimensions:\t");
 PRINT_VEC3(direct_path->occlusion.v3);
 printf("        sensor-emitter:\t");
 PRINT_VEC3(vres);
#endif

            vec3fAbsolute(&vres, &vres);
            if (vec3fLessThan(&vres, &direct_path->occlusion.v3))
            {
printf("less\n");
            }
else
printf("more\n");
         } /* direct_path != NULL */
      } /* pdp3d_m != NULL */

      /*
       * Audio cone recalculaion.
       * Version 2.6 adds forward gain which allows for donut shaped cones.
       * TODO: Calculate cone relative to the frame position when indoors.
       *       For now it is assumed that indoor reflections render the cone
       *       pretty much useless, at least if the emitter is in another room.
       */
      if (_PROP3D_CONE_IS_DEFINED(edp3d) && !_PROP3D_INDOOR_IS_DEFINED(fdp3d_m))
      {
         float tmp, forward_gain, inner_vec, cone_volume = 1.0f;

         forward_gain = _FILTER_GETD3D(src, ANGULAR_FILTER, AAX_FORWARD_GAIN);
         inner_vec = _FILTER_GETD3D(src, ANGULAR_FILTER, AAX_INNER_ANGLE);
         tmp = -edp3d_m->matrix.m4[DIR_BACK][2];

         if (tmp < inner_vec)
         {
            float outer_vec, outer_gain;

            outer_vec = _FILTER_GETD3D(src, ANGULAR_FILTER, AAX_OUTER_ANGLE);
            outer_gain = _FILTER_GETD3D(src, ANGULAR_FILTER, AAX_OUTER_GAIN);
            if (outer_vec < tmp)
            {
               tmp -= inner_vec;
               tmp *= (outer_gain - 1.0f);
               tmp /= (outer_vec - inner_vec);
               cone_volume = (1.0f + tmp);
            } else {
               cone_volume = outer_gain;
            }
         }
         else if (forward_gain != 1.0f)
         {
            tmp -= inner_vec;

            tmp /= (1.0f - inner_vec);
            cone_volume = (1.0f - tmp) + tmp*forward_gain;
         }
         gain *= cone_volume;
      }

      min = _FILTER_GET2D(src, VOLUME_FILTER, AAX_MIN_GAIN);
      max = _FILTER_GET2D(src, VOLUME_FILTER, AAX_MAX_GAIN);
      ep2d->final.gain = _MINMAX(gain, min, max);
   }
}

