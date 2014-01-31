/*
 * Copyright 2005-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
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

#include <assert.h>

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>

#include "rbuf_int.h"
#include "audio.h"

void
_aaxSensorsProcess(_aaxRingBuffer *drb, const _intBuffers *devices,
                   _aax2dProps *props2d, int dest_track)
{
   _intBuffers *hd = (_intBuffers *)devices;
   unsigned int i, num;

   assert(devices);

   num = _intBufGetMaxNum(hd, _AAX_DEVICE);
   for (i=0; i<num; i++)
   {
      const _intBufferData *dptr_sensor = NULL;
      const _aaxDriverBackend *be;
      _intBufferData *dptr;
      _handle_t* device;
      void *be_handle;

      dptr = _intBufGet(hd, _AAX_DEVICE, i);
      if (!dptr) continue;

      device = _intBufGetDataPtr(dptr);
      be = device->backend.ptr;
      be_handle = device->backend.handle;

      if (be->state(be_handle, DRIVER_AVAILABLE) != AAX_FALSE) {
         dptr_sensor = _intBufGet(device->sensors, _AAX_SENSOR, 0);
      }

      if (dptr_sensor)
      {
         _aax2dProps *p2d = (_aax2dProps*)props2d;
         float gain, dt, rr, curr_pos_sec;
         const _intBufferData* sptr_rb;
         _aaxRingBuffer *srb;
         _aaxAudioFrame *smixer;
         _sensor_t* sensor;
         _intBuffers *srbs;
         void *rv;

         sensor = _intBufGetDataPtr(dptr_sensor);
         smixer = sensor->mixer;
         srb = smixer->ringbuffer;
         dt = 1.0f / smixer->info->refresh_rate;
         srbs = smixer->play_ringbuffers;
         curr_pos_sec = smixer->curr_pos_sec;
         _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

         if (!device->ringbuffer) {
            device->ringbuffer = be->get_ringbuffer(0.0f, smixer->info->mode);
         }

         gain = _FILTER_GET(smixer->props2d, VOLUME_FILTER, AAX_GAIN);
         gain *= (float)_FILTER_GET_STATE(smixer->props2d, VOLUME_FILTER);
         rr =_FILTER_GET(smixer->props2d, VOLUME_FILTER, AAX_AGC_RESPONSE_RATE);
         rv = _aaxSensorCapture(srb, be, be_handle, &dt, rr, dest_track,
                                curr_pos_sec, gain);
         if (dt == 0.0f)
         {
            _SET_STOPPED(device);
            _SET_PROCESSED(device);
         }

         if (device->ringbuffer)
         {
            _aaxRingBuffer *drb = device->ringbuffer;
            _aaxRingBuffer *rb = rv;
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
         if (rv != srb)
         {
            /**
             * Add the new buffer to the buffer queue and pop the
             * first buffer from the queue when needed (below).
             * This way pitch effects (< 1.0) can be processed safely.
             */
            srb->set_paramf(srb, RB_FREQUENCY, device->info->frequency);
            srb->set_state(srb, RB_STARTED);
            srb->set_state(srb, RB_REWINDED);

            _intBufAddData(srbs, _AAX_RINGBUFFER, srb);
            smixer->ringbuffer = rv;
         }
         smixer->curr_pos_sec += dt;

         sptr_rb = _intBufGet(srbs, _AAX_RINGBUFFER, 0);
         if (sptr_rb)
         {
            _aaxRingBuffer *ssr_rb = _intBufGetDataPtr(sptr_rb);
            unsigned int rv = 0;

            do
            {
               _aaxRingBufferLFOData *lfo;

               lfo = _EFFECT_GET_DATA(p2d, DYNAMIC_PITCH_EFFECT);
               if (lfo) {
                  p2d->final.pitch_lfo = lfo->get(lfo, NULL, 0, 0);
               } else {
                  p2d->final.pitch_lfo = 1.0f;
               }
               lfo = _FILTER_GET_DATA(p2d, DYNAMIC_GAIN_FILTER);
               if (lfo && !lfo->envelope) {
                  p2d->final.gain_lfo = lfo->get(lfo, NULL, 0, 0);
               } else {
                  p2d->final.gain_lfo = 1.0f;
               }
               rv = drb->mix2d(drb, ssr_rb, smixer->props2d, props2d, 0, 0);
               _intBufReleaseData(sptr_rb, _AAX_RINGBUFFER);

               if (rv) /* always streaming */
               {
                  _intBufferData *rbuf;

                  rbuf = _intBufPop(srbs, _AAX_RINGBUFFER);
                  if (rbuf)
                  {
                     _aaxRingBuffer *rb = _intBufGetDataPtr(rbuf);

                     // TODO: store in a reasuable ringbuffer queue
                     be->destroy_ringbuffer(rb);
                     _intBufDestroyDataNoLock(rbuf);
                  }

                  if ((sptr_rb = _intBufGet(srbs, _AAX_RINGBUFFER, 0)) != NULL)
                  {
                     ssr_rb = _intBufGetDataPtr(sptr_rb);
                     /* since rv == AAX_TRUE this will be unlocked 
                        after rb->mix2d */
                  }
                  else rv = 0;
               }
            }
            while(rv);
         }
//       _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
      }
      else {
         _SET_PROCESSED(device);
      }

      _intBufReleaseData(dptr, _AAX_DEVICE);
   }
   _intBufReleaseNum(hd, _AAX_DEVICE);
}

void*
_aaxSensorCapture(_aaxRingBuffer *drb, const _aaxDriverBackend* be,
                  void *be_handle, float *delay, float agc_rr, int dest_track,
                  float pos_sec, float gain)
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
      unsigned int bps = drb->get_parami(drb, RB_BYTES_SAMPLE);
      unsigned int ds = drb->get_parami(drb, RB_DDE_SAMPLES);
      float dt = GMATH_E1 * *delay;
      size_t frames, nframes;
      void **sbuf;
      int res;

      if (agc_rr > 0.0f)
      {
         agc_rr = _MINMAX(dt/agc_rr, 0.0f, 1.0f);
         gain *= -drb->get_paramf(drb, RB_AGC_VALUE);
      }

      nframes = frames = drb->get_parami(drb, RB_NO_SAMPLES);

      sbuf = (void**)drb->get_tracks_ptr(drb, RB_WRITE);
      res = be->capture(be_handle, sbuf, 0, &nframes,
                        scratch[SCRATCH_BUFFER0]-ds, 2*2*ds+frames, gain);
      drb->release_tracks_ptr(drb);	// convert to mixer format

      if (res && nframes)
      {
         _aaxRingBufferData *nrbi, *drbi = drb->handle;
         _aaxRingBufferSample *nrbd, *drbd = drbi->sample;
         float avg, agc, rms_rr, max, maxrms, peak, maxpeak;
         unsigned int track, tracks;
         MIX_T **ntptr, **otptr;
         _aaxRingBuffer *nrb;
         double rms;

         nrb = drb->duplicate(drb, AAX_FALSE, AAX_FALSE);
         assert(nrb != 0);

         nrbi = nrb->handle;
         nrbd = nrbi->sample;

         otptr = (MIX_T **)sbuf;
         rms_rr = _MINMAX(dt/0.3f, 0.0f, 1.0f);		// 300 ms RMS average
         maxrms = maxpeak = 0;
         tracks = drb->get_parami(drb, RB_NO_TRACKS);
         for (track=0; track<tracks; track++)
         {
            MIX_T *optr = otptr[track];

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
               if (nframes < nframes) {
                  *optr = (*(optr-1) + *(optr+1))/2;
               }
               else {
                  *(optr-1) = (*(optr-2)*2 + *(optr+1))/3;
                  *optr     = (*(optr-2) + *(optr+1)*2)/3;
               }
            }

            /* stereo downmix requested, add the tracks to track0 */
            if ((dest_track == AAX_TRACK_MIX) && track) {
               drbd->add(otptr[0], optr, frames, 1.0f, 0.0f);
            }
         }

         /* if downmix requested devide track0 by the number of tracks */
         if ((dest_track == AAX_TRACK_MIX) && tracks)
         {
            float fact = 1.0f/tracks;
            drbd->multiply(otptr[0], sizeof(MIX_T), frames, fact);
            dest_track = 0;
         }

         ntptr = (MIX_T**)nrbd->track;
         for (track=0; track<tracks; track++)
         {
            MIX_T *ptr = ntptr[track];
            MIX_T *optr = otptr[track];
            unsigned int j;

            /* single channel requested, copy to the other channels */
            if ((dest_track != AAX_TRACK_ALL) && (track != dest_track)) {
               _aax_memcpy(optr, otptr[dest_track], frames*sizeof(MIX_T));
            }

            /* copy the delay effects buffer */
            _aax_memcpy(ptr-ds, optr-ds+nframes, ds*bps);

            /** average RMS and peak values */
            rms = peak = 0;
            j = nframes;
            do
            {
               float samp = *optr++;		// rms
               float val = samp*samp;
               rms += val;
               if (val > peak) peak = val;
            }
            while (--j);
            rms = sqrt(rms/nframes);
            peak = sqrtf(peak);
//          _batch_get_average_rms(otptr[track], nframes, &rms, &peak);

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
            max = _MIN(0.707f*8388607.0f/maxrms, 128.0f);
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

      if (res <= 0) *delay = 0.0f;
   }

   return rv;
}

/*-------------------------------------------------------------------------- */

unsigned int
_aaxSoftwareMixerMixSensorsThreaded(void *dest, _intBuffers *hs)
{
   _aaxRingBuffer *drb = (_aaxRingBuffer *)dest;
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

            dptr_sensor = _intBufGet(config->sensors, _AAX_SENSOR, 0);
            if (dptr_sensor)
            {
               const _aaxDriverBackend *be = config->backend.ptr;
               _intBuffers *ringbuffers;
               _aaxAudioFrame *smixer;
               _sensor_t* sensor;
               unsigned int nbuf;

               sensor = _intBufGetDataPtr(dptr_sensor);
               smixer = sensor->mixer;
               ringbuffers = smixer->play_ringbuffers;
               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

               nbuf = _intBufGetNumNoLock(ringbuffers, _AAX_RINGBUFFER);
               if (nbuf)
               {
                  _intBufferData *buf;
                  _aaxRingBuffer *srb;
                  unsigned int rv = 0;

                  buf = _intBufGet(ringbuffers, _AAX_RINGBUFFER, 0);
                  srb = _intBufGetDataPtr(buf);
                  do
                  {
                     rv = drb->mix2d(drb, srb, smixer->props2d, NULL, 0, 0);
                     _intBufReleaseData(buf, _AAX_RINGBUFFER);

                     if (rv) /* always streaming */
                     {
                        buf = _intBufPop(ringbuffers, _AAX_RINGBUFFER);
                        be->destroy_ringbuffer(srb);
                        _intBufDestroyDataNoLock(buf);

                        buf = _intBufGet(ringbuffers, _AAX_RINGBUFFER, 0);
                        if (buf)
                        {
                           srb = _intBufGetDataPtr(buf);
                           /* since rv == AAX_TRUE this will be unlocked 
                              after drb->mix2d */
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

