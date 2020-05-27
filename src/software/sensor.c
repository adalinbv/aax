/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
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

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>
#include <dsp/filters.h>
#include <dsp/effects.h>
#include <dsp/lfo.h>

#include "cpu/arch2d_simd.h"
#include "rbuf_int.h"
#include "audio.h"

char
_aaxSensorsProcess(_aaxRingBuffer *drb, const _intBuffers *devices,
                   _aax2dProps *props2d, int dest_track, char batched)
{
   _intBuffers *hd = (_intBuffers *)devices;
   char rv = AAX_FALSE;
   size_t i, num;

   assert(devices);

   num = _intBufGetMaxNum(hd, _AAX_DEVICE);
   for (i=0; i<num; i++)
   {
      _intBufferData *dptr;
      _handle_t* device;

      dptr = _intBufGet(hd, _AAX_DEVICE, i);
      if (!dptr) continue;

      device = _intBufGetDataPtr(dptr);
      rv &= _aaxSensorsProcessSensor(device, drb, props2d, dest_track, batched);
      _intBufReleaseData(dptr, _AAX_DEVICE);
   }
   _intBufReleaseNum(hd, _AAX_DEVICE);

   return rv;
}

char
_aaxSensorsProcessSensor(void *id, _aaxRingBuffer *drb, _aax2dProps *p2d, int dest_track, char batched)
{
   _handle_t* device = (_handle_t*)id;
   const _intBufferData *dptr_sensor = NULL;
   _aax2dProps *props2d = p2d;
   const _aaxDriverBackend *be;
   char unregistered_ssr;
   void *be_handle;
   char rv = AAX_FALSE;

   be = device->backend.ptr;
   be_handle = device->backend.handle;
   unregistered_ssr = props2d ? AAX_FALSE : AAX_TRUE;

   // It's tempting to test if the device is processed and then continue
   // but the device might still have several unprocessed ringbuffers
   // in the queue. We have to process them first.

   if (be->state(be_handle, DRIVER_AVAILABLE) != AAX_FALSE) {
      dptr_sensor = _intBufGet(device->sensors, _AAX_SENSOR, 0);
   }

   if (dptr_sensor)
   {
      float gain, dt, rr, curr_pos_sec;
      const _intBufferData* sptr_rb;
      _aaxRingBuffer *srb;
      _aaxAudioFrame *smixer;
      _sensor_t* sensor;
      _intBuffers *srbs;
      ssize_t nsamps;
      void *res;

      sensor = _intBufGetDataPtr(dptr_sensor);
      smixer = sensor->mixer;
      srb = smixer->ringbuffer;
      dt = 1.0f / smixer->info->period_rate;
      srbs = smixer->play_ringbuffers;
      curr_pos_sec = smixer->curr_pos_sec;

      if (unregistered_ssr)
      {
         p2d = smixer->props2d;
         dest_track = smixer->info->track;
         batched = device->finished ? AAX_TRUE : AAX_FALSE;
         srb = drb;
      }
      _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

      if (_IS_PLAYING(device))
      {
         if (!device->ringbuffer) {
            device->ringbuffer = be->get_ringbuffer(0.0f, smixer->info->mode);
         }

         nsamps = 0;
         gain = _FILTER_GET(smixer->props2d, VOLUME_FILTER, AAX_GAIN);
         gain *= (float)_FILTER_GET_STATE(smixer->props2d, VOLUME_FILTER);
         rr = _FILTER_GET(smixer->props2d, VOLUME_FILTER,
                                           AAX_AGC_RESPONSE_RATE);
         res = _aaxSensorCapture(srb, be, be_handle, &dt, rr, dest_track,
                                curr_pos_sec, gain, &nsamps, batched);
         if (dt == 0.0f)
         {
            _SET_STOPPED(device);
            _SET_PROCESSED(device);
            smixer->curr_pos_sec = 0.0f;
            smixer->curr_sample = 0;
            if (be->set_position) {
               be->set_position(be_handle, 0);
            }
         }

         if (device->ringbuffer)
         {
            _aaxRingBuffer *drb = device->ringbuffer;
            _aaxRingBuffer *rb = res;
            int track, tracks;

            tracks = rb->get_parami(rb, RB_NO_TRACKS);
            for (track=0; track<tracks; track++)
            {
               float f = rb->get_paramf(rb, RB_AVERAGE_VALUE+track);
               drb->set_paramf(drb, RB_AVERAGE_VALUE+track, f);

               f = rb->get_paramf(rb, RB_PEAK_VALUE+track);
               drb->set_paramf(drb, RB_PEAK_VALUE+track, f);
            }
         }

         dptr_sensor = _intBufGetNoLock(device->sensors, _AAX_SENSOR, 0);
         if (res != srb)
         {
            /**
             * Add the new buffer to the buffer queue and pop the
             * first buffer from the queue when needed (below).
             * This way pitch effects (< 1.0) can be processed safely.
             */
            srb->set_paramf(srb, RB_FREQUENCY, device->info->frequency);
            srb->set_state(srb, RB_STARTED_STREAMING);
            srb->set_state(srb, RB_REWINDED);

            _intBufAddData(srbs, _AAX_RINGBUFFER, srb);
            if (unregistered_ssr)
            {
               if (smixer->capturing)
               {
                  device->ringbuffer = res;
                  _aaxSignalTrigger(&device->buffer_ready);
               }
            }
            else {
               smixer->ringbuffer = res;
            }
         }
         smixer->curr_pos_sec += dt;
         smixer->curr_sample += nsamps;

         if (unregistered_ssr) {
            sptr_rb = NULL;
         } else {
            sptr_rb = _intBufGet(srbs, _AAX_RINGBUFFER, 0);
         }
         if (sptr_rb)
         {
            _aaxRingBuffer *ssr_rb = _intBufGetDataPtr(sptr_rb);
            size_t res = 0;

            do
            {
               _aaxLFOData *lfo;

               lfo = _EFFECT_GET_DATA(p2d, DYNAMIC_PITCH_EFFECT);
               if (lfo)
               {
                  p2d->final.pitch_lfo = lfo->get(lfo, NULL, NULL, 0, 0);
                  p2d->final.pitch_lfo -= lfo->min;
               } else {
                  p2d->final.pitch_lfo = 1.0f;
               }

               lfo = _FILTER_GET_DATA(p2d, DYNAMIC_GAIN_FILTER);
               if (lfo && !lfo->envelope) {
                  p2d->final.gain_lfo = lfo->get(lfo, NULL, NULL, 0, 0);
               } else {
                  p2d->final.gain_lfo = 1.0f;
               }
               res = drb->mix2d(drb, ssr_rb, smixer->info, smixer->props2d, p2d, 0, 1.0f, NULL);
               _intBufReleaseData(sptr_rb, _AAX_RINGBUFFER);

               if (res) /* true if a new buffer is required */
               {
                  _intBufferData *rbuf;

                  rbuf = _intBufPop(srbs, _AAX_RINGBUFFER);
                  if (rbuf)
                  {
                     _aaxRingBuffer *rb = _intBufGetDataPtr(rbuf);

                     // TODO: store in a reusable ringbuffer queue
                     be->destroy_ringbuffer(rb);
                     _intBufDestroyDataNoLock(rbuf);
                     }

                  if ((sptr_rb =_intBufGet(srbs, _AAX_RINGBUFFER,0)) != NULL)
                  {
                     ssr_rb = _intBufGetDataPtr(sptr_rb);
                     /* since res == AAX_TRUE this will be unlocked 
                        after rb->mix2d */
                  }
                  else {
                     res = 0;
                  }
               }
            }
            while(res);
            rv = AAX_TRUE;
         }
      }
      else if (_IS_PROCESSED(device))
      {
         smixer->curr_pos_sec = 0.0f;
         smixer->curr_sample = 0;
         if (be->set_position) {
            be->set_position(be_handle, 0);
         }
      }
//    _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
   }
   else {
      _SET_PROCESSED(device);
   }

   return rv;
}

void*
_aaxSensorCapture(_aaxRingBuffer *drb, const _aaxDriverBackend* be, void *be_handle, float *delay, float agc_rr, unsigned int dest_track, UNUSED(float pos_sec), float gain, ssize_t *nsamps, char batched)
{
   void *rv = drb;
   int32_t **scratch;

   /*
    * drb is thread specific and does not need a lock
    * Note:
    *  - The ringbuffer is singed 24-bit PCM, stereo or mono
    *  - capture functions should return the data in signed 24-bit
    */
   assert(delay);

   scratch = (int32_t**)drb->get_scratch(drb);
   if (scratch)
   {
      unsigned int track, no_tracks = drb->get_parami(drb, RB_NO_TRACKS);
      unsigned int bps = drb->get_parami(drb, RB_BYTES_SAMPLE);
      size_t ds = drb->get_parami(drb, RB_DDE_SAMPLES);
      float freq, dt = GMATH_E1 * (*delay);
      size_t frames, nframes;
      ssize_t res, offs;
      void **sbuf;

      if (agc_rr > 0.0f)
      {
         agc_rr = _MINMAX(dt/agc_rr, 0.0f, 1.0f);
         gain *= -drb->get_paramf(drb, RB_AGC_VALUE);
      }

      offs = 0;
      nframes = frames = drb->get_parami(drb, RB_NO_SAMPLES);

      sbuf = (void**)drb->get_tracks_ptr(drb, RB_WRITE);
      res = be->capture(be_handle, sbuf, &offs, &nframes,
                        scratch[SCRATCH_BUFFER0]-ds, 2*2*ds+frames, gain,
                        batched);
      drb->release_tracks_ptr(drb);	// convert to mixer format

      // Some formats allow format changes to save bandwidth, which might
      // include changing the playback frequency
      freq = be->param(be_handle, DRIVER_FREQUENCY);
      if (freq != drb->get_paramf(drb, RB_FREQUENCY)) {
         drb->set_paramf(drb, RB_FREQUENCY, freq);
      }

#if RB_FLOAT_DATA
      // be->capture can capture one extra sample to keep synchronised with
      // the capture buffer but it is in int32_t format while the mixer format
      // might be float. Convert this sample to float ourselves.
      if (offs < 0)
      {
         assert (offs == -1);
         for (track=0; track<no_tracks; track++)
         {
            int32_t *iptr = (int32_t*)sbuf[track];
            MIX_T *ptr = (MIX_T*)sbuf[track];
            int32_t s = *(iptr-1);
            *(ptr-1) = (MIX_T)s;
         }
      }
#endif

      if (res >= 0 && nframes)
      {
         _aaxRingBufferData *nrbi, *drbi = drb->handle;
         _aaxRingBufferSample *nrbd, *drbd = drbi->sample;
         float avg, agc, rms_rr, max, maxrms, maxpeak;
         MIX_T **ntptr, **otptr;
         _aaxRingBuffer *nrb;
         float rms, peak;

         nrb = drb->duplicate(drb, AAX_FALSE, AAX_TRUE);
         assert(nrb != 0);

         nrbi = nrb->handle;
         nrbd = nrbi->sample;

         otptr = (MIX_T **)sbuf;
         rms_rr = _MINMAX(dt/0.3f, 0.0f, 1.0f);		// 300 ms RMS average
         maxrms = maxpeak = 0;
         for (track=0; track<no_tracks; track++)
         {
            MIX_T *optr = otptr[track];

#if 0
            if (frames != nframes)
            {
               /*
                * The backend driver may fetch one sample more, or one sample
                * less than requested to synchronize the capture and playback
                * streams. If one extra sample is fetched it replaces the last
                * sample in the delay effects buffer.
                * If one sample less wat fetched the new data starts at the
                * second position and we have to construct the first sample in
                * the buffer ourselves based on the last sample in the delay
                * effects buffer and the first new sample.
                */
               if (frames < nframes) {
                  *optr = (*(optr-1) + *(optr+1))/2;
               }
               else {
                  *(optr-1) = (*(optr-2)*2 + *(optr+1))/3;
                  *optr     = (*(optr-2) + *(optr+1)*2)/3;
               }
            }
#endif

            /* stereo downmix requested, add the tracks to track0 */
            if ((dest_track == AAX_TRACK_MIX) && track) {
               drbd->add(otptr[0], optr, frames, 1.0f, 0.0f);
            }
         }

         /* if downmix requested devide track0 by the number of tracks */
         if ((dest_track == AAX_TRACK_MIX) && no_tracks)
         {
            float fact = 1.0f/no_tracks;
            drbd->multiply(otptr[0], otptr[0], sizeof(MIX_T), frames, fact);
            dest_track = 0;
         }

         ntptr = (MIX_T**)nrbd->track;
         for (track=0; track<no_tracks; track++)
         {
            MIX_T *ptr = ntptr[track];
            MIX_T *optr = otptr[track];

            /* single channel requested, copy to the other channels */
            if ((dest_track != AAX_TRACK_ALL) && (track != dest_track)) {
               _aax_memcpy(optr, otptr[dest_track], frames*sizeof(MIX_T));
            }

            /* copy the delay effects buffer */
            _aax_memcpy(ptr-ds, optr-ds+nframes, ds*bps);

            /** average RMS and peak values */
            _batch_get_average_rms(otptr[track], nframes, &rms, &peak);

            if (maxrms < rms) maxrms = rms;
            if (maxpeak < peak) maxpeak = peak;

            avg = drb->get_paramf(drb, RB_AVERAGE_VALUE+track);
            avg = (rms_rr*avg + (1.0f-rms_rr)*rms);
            nrb->set_paramf(nrb, RB_AVERAGE_VALUE+track, avg);
            nrb->set_paramf(nrb, RB_PEAK_VALUE+track, peak);
         }

         nrb->set_paramf(nrb, RB_AVERAGE_VALUE_MAX, maxrms);
         nrb->set_paramf(nrb, RB_PEAK_VALUE_MAX, maxpeak);

         /** Automatic Gain Control */
         max = 0.0f;
         if (maxrms > 256) {
            max = _MIN(0.707f*AAX_PEAK_MAX/maxrms, 128.0f);
         }

         agc = drb->get_paramf(drb, RB_AGC_VALUE);
         if (max < agc) {
            agc = agc_rr*agc + (1.0f-agc_rr)*max;
         } else {
            agc =  (1.0f-agc_rr)*agc + (agc_rr)*max;
         }
         nrb->set_paramf(nrb, RB_AGC_VALUE, _MINMAX(agc, 0.01f, 9.5f));

         rv = nrb;
      }
      else {
         drb->set_state(drb, RB_CLEARED);
      }

      if (res < 0) *delay = 0.0f;
      *nsamps = nframes;
   }

   return rv;
}

