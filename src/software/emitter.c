/*
 * Copyright 2012-2017 by Erik Hofman.
 * Copyright 2012-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
#include "renderer.h"
#include "audio.h"

/**
 * sv_m -> modified sensor velocity vector
 * ssv -> sensor sound velocity
 * sdf -> sensor doppler factor
 * pos
 */


/*
 * Threaded emitter rendering code using a thread pool with worker threads,
 * one thread for every physical CPU core.
 */
char
_aaxEmittersProcess(_aaxRingBuffer *drb, const _aaxMixerInfo *info,
                    float ssv, float sdf, _aax2dProps *fp2d,
                    _aaxDelayed3dProps *fdp3d_m,
                    _intBuffers *e2d, _intBuffers *e3d,
                    const _aaxDriverBackend* be, void *be_handle)
{
   _aaxRenderer *render = be->render(be_handle);
   _aaxRendererData data;
   char rv = AAX_TRUE;

   data.drb = drb;
   data.info = info;
   data.fdp3d_m = fdp3d_m;
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

         ctr = --src->update_ctr;
         if (ctr == 0)
         {
            if (stage == 2)
            {
               src->state3d |= data->fdp3d_m->state3d;
               data->be->prepare3d(src, data->info, data->ssv, data->sdf,
                                   data->fp2d->speaker, data->fdp3d_m);
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
               if (ep2d->curr_pos_sec >= ep2d->dist_delay_sec)
               {
                  res = drb->mix3d(drb, srb, data->info, ep2d, data->fp2d,
                                             emitter->track, ctr, src->history);
               }
            }
            else
            {
               assert(!_IS_POSITIONAL(src->props3d));
               res = drb->mix2d(drb, srb, data->info, ep2d, data->fp2d, ctr, src->history);
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
 * fp3d:    parent frame dp3d->dprops3d
 */
void
_aaxEmitterPrepare3d(_aaxEmitter *src,  const _aaxMixerInfo* info, float ssv, float sdf, vec4f_t *speaker, _aaxDelayed3dProps* fdp3d_m)
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

   /* get pitch and gain before it is pushed to the delay queue */
   edp3d->pitch = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_PITCH);
   edp3d->gain = _FILTER_GET(ep2d, VOLUME_FILTER, AAX_GAIN);

   // _aaxEmitterProcessDelayQueue
   if (_PROP3D_DISTQUEUE_IS_DEFINED(edp3d))
   {
      _aaxDelayed3dProps *sdp3d = NULL;
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
                  sdp3d = _intBufGetDataPtr(buf3dq);
                  _intBufDestroyDataNoLock(buf3dq);
               }
               --ep2d->bufpos3dq;
            }
            while (ep2d->bufpos3dq > 1.0f);
         }
      }

      if (!sdp3d) {                  // TODO: get from buffer cache
         sdp3d = _aaxDelayed3dPropsDup(edp3d);
      }

      ep3d->dprops3d = sdp3d;
      edp3d = ep3d->dprops3d;
   }

   *(void**)(&distfn) = _FILTER_GET_DATA(ep3d, DISTANCE_FILTER);
   *(void**)(&dopplerfn) = _EFFECT_GET_DATA(ep3d, VELOCITY_EFFECT);
   assert(dopplerfn);
   assert(distfn);

   /* only update when the matrix and/or the velocity vector has changed */
   if (_PROP3D_MTXSPEED_HAS_CHANGED(edp3d) ||
       _PROP3D_MTXSPEED_HAS_CHANGED(fdp3d_m) || 
       _PROP3D_MTXSPEED_HAS_CHANGED(src))
   {
      vec3f_t epos, tmp;
      float refdist, dist_fact, maxdist, rolloff;
      unsigned int i, t;
      float gain, pitch;
      float min, max;
      float esv, vs;
      float dist;

      _PROP3D_SPEED_CLEAR_CHANGED(edp3d);
      _PROP3D_MTX_CLEAR_CHANGED(edp3d);

      _PROP3D_SPEED_CLEAR_CHANGED(src);
      _PROP3D_MTX_CLEAR_CHANGED(src);

      /**
       * align the emitter with the parent frame.
       * (compensate for the parents direction offset)
       */
      mtx4dMul(&edp3d_m->matrix, &fdp3d_m->matrix, &edp3d->matrix);
      vec3fFilld(&tmp, &edp3d_m->matrix.v34[LOCATION]);
      dist = vec3fNormalize(&epos, &tmp);
#if 0
 printf("# emitter parent:\t\t\t\temitter:\n");
 PRINT_MATRICES(fdp3d_m->matrix, edp3d->matrix);
 printf("# modified emitter\n");
 PRINT_MATRIX(edp3d_m->matrix);
 printf("# dist: %f\n", dist);
#endif

      /* calculate the sound velocity inbetween the emitter and the sensor */
      esv = _EFFECT_GET(ep3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);
      vs = (esv+ssv) / 2.0f;

      /*
       * Doppler
       */
      pitch = 1.0f; // edp3d->pitch --> this is done in mixmult/mixsingle
      if (dist > 1.0f)
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
 printf("parent velocity:\t\t\t\temitter velocity:\n");
 PRINT_MATRICES(fdp3d_m->velocity, edp3d->velocity);
 printf("# modified emitter velocity\n");
 PRINT_MATRIX(edp3d_m->velocity);
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
      dist_fact = _MIN(dist/refdist, 1.0f);

      switch (info->mode)
      {
      case AAX_MODE_WRITE_HRTF:
         for (t=0; t<info->no_tracks; t++)
         {
            for (i=0; i<3; i++)
            {
               float dp, offs, fact;

               dp = vec3fDotProduct(&speaker[3*t+i].v3, &epos) * speaker[t].v4[3];
               ep2d->speaker[t].v4[i] = dp * dist_fact;		/* -1 .. +1 */

               offs = info->hrtf[HRTF_OFFSET].v4[i];
               fact = info->hrtf[HRTF_FACTOR].v4[i];

               dp = vec3fDotProduct(&speaker[_AAX_MAX_SPEAKERS+3*t+i].v3,&epos);
               ep2d->hrtf[t].v4[i] = _MAX(offs + dp*fact, 0.0f);
            }
         }
         break;
      case AAX_MODE_WRITE_SURROUND:
         for (t=0; t<info->no_tracks; t++)
         {
#ifdef USE_SPATIAL_FOR_SURROUND
            float dp = vec3fDotProduct(&speaker[t].v3, &epos)*speaker[t].v4[3];
            ep2d->speaker[t].v4[0] = 0.5f + dp*dist_fact;
#else
            vec4fMulvec4(&ep2d->speaker[t], &speaker[t], &epos);
            vec4fScalarMul(&ep2d->speaker[t], dist_fact);
#endif
            i = DIR_UPWD;
            do				/* skip left-right and back-front */
            {
               float dp, offs, fact;

               offs = info->hrtf[HRTF_OFFSET].v4[i];
               fact = info->hrtf[HRTF_FACTOR].v4[i];
               dp = vec3fDotProduct(&speaker[_AAX_MAX_SPEAKERS+3*t+i].v3,&epos);
               ep2d->hrtf[t].v4[i] = _MAX(offs + dp*fact, 0.0f);
            }
            while(0);
         }
         break;
      case AAX_MODE_WRITE_SPATIAL:
         for (t=0; t<info->no_tracks; t++)
         {                      /* speaker == sensor_pos */
            float dp = vec3fDotProduct(&speaker[t].v3, &epos)*speaker[t].v4[3];
            ep2d->speaker[t].v4[0] = 0.5f + dp*dist_fact;
         }
         break;
      default: /* AAX_MODE_WRITE_STEREO */
         for (t=0; t<info->no_tracks; t++)
         {
            vec3fMulvec3(&ep2d->speaker[t].v3, &speaker[t].v3, &epos);
            vec4fScalarMul(&ep2d->speaker[t], dist_fact);
         }
      }

      gain *= distfn(dist, refdist, maxdist, rolloff, vs, 1.0f);

      /*
       * audio cone recalculaion
       * version 2.6 adds forward gain which allows for donut shaped cones
       */
      if (_PROP3D_CONE_IS_DEFINED(edp3d))
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

