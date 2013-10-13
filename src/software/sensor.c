/*
 * Copyright 2005-2013 by Erik Hofman.
 * Copyright 2009-2013 by Adalin B.V.
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

#include <ringbuffer.h>
#include <arch.h>
#include <api.h>

void
_aaxSensorsProcess(_oalRingBuffer *dest_rb, const _intBuffers *devices,
                   _oalRingBuffer2dProps *props2d, int dest_track)
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
         _oalRingBuffer2dProps *p2d = (_oalRingBuffer2dProps*)props2d;
         float gain, dt, rr, curr_pos_sec;
         const _intBufferData* sptr_rb;
         _oalRingBuffer *src_rb;
         _aaxAudioFrame *smixer;
         _sensor_t* sensor;
         _intBuffers *srbs;
         void *rv;

         sensor = _intBufGetDataPtr(dptr_sensor);
         smixer = sensor->mixer;
         src_rb = smixer->ringbuffer;
         dt = 1.0f / smixer->info->refresh_rate;
         srbs = smixer->ringbuffers;
         curr_pos_sec = smixer->curr_pos_sec;
         _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

         if (!device->ringbuffer) {
            device->ringbuffer = _oalRingBufferCreate(0.0f);
         }

         gain = _FILTER_GET(smixer->props2d, VOLUME_FILTER, AAX_GAIN);
         rr =_FILTER_GET(smixer->props2d, VOLUME_FILTER, AAX_AGC_RESPONSE_RATE);
         rv = _aaxSensorCapture(src_rb, be, be_handle, &dt, rr, dest_track,
                                curr_pos_sec, gain);
         if (dt == 0.0f)
         {
            _SET_STOPPED(device);
            _SET_PROCESSED(device);
         }

         if (device->ringbuffer)
         {
            _oalRingBuffer *srb = device->ringbuffer;
            _oalRingBuffer *rb = rv;
            int track, tracks;

            tracks = _oalRingBufferGetParami(rb, RB_NO_TRACKS);
            for (track=0; track<tracks; track++)
            {
               srb->average[track] = rb->average[track];
               srb->peak[track] = rb->peak[track];
            }
         }

         dptr_sensor = _intBufGetNoLock(device->sensors, _AAX_SENSOR, 0);
         if (rv != src_rb)
         {
            /**
             * Add the new buffer to the buffer queue and pop the
             * first buffer from the queue when needed (below).
             * This way pitch effects (< 1.0) can be processed safely.
             */
            _oalRingBufferSetParamf(src_rb, RB_FREQUENCY, device->info->frequency);
            _oalRingBufferStart(src_rb);
            _oalRingBufferRewind(src_rb);

            _intBufAddData(srbs, _AAX_RINGBUFFER, src_rb);
            smixer->ringbuffer = rv;
         }
         smixer->curr_pos_sec += dt;

         sptr_rb = _intBufGet(srbs, _AAX_RINGBUFFER, 0);
         if (sptr_rb)
         {
            _oalRingBuffer *ssr_rb = _intBufGetDataPtr(sptr_rb);
            unsigned int rv = 0;

            do
            {
               _oalRingBufferLFOInfo *lfo;

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
               rv = be->mix2d(be_handle, dest_rb, ssr_rb,
                              smixer->props2d, props2d, 0, 0);
               _intBufReleaseData(sptr_rb, _AAX_RINGBUFFER);

               if (rv) /* always streaming */
               {
                  _intBufferData *rbuf;

                  rbuf = _intBufPop(srbs, _AAX_RINGBUFFER);
                  if (rbuf)
                  {
                     _oalRingBuffer *rb = _intBufGetDataPtr(rbuf);

                     _oalRingBufferDelete(rb);
                     _intBufDestroyDataNoLock(rbuf);
                  }

                  if ((sptr_rb = _intBufGet(srbs, _AAX_RINGBUFFER, 0)) != NULL)
                  {
                     ssr_rb = _intBufGetDataPtr(sptr_rb);
                     /* since rv == AAX_TRUE this will be unlocked 
                        after be->mix2d */
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
_aaxSensorCapture(_oalRingBuffer *dest_rb, const _aaxDriverBackend* be,
                  void *be_handle, float *delay, float agc_rr, int dest_track,
                  float pos_sec, float gain)
{
   _oalRingBufferSample *rbd;
   void *rv = dest_rb;
   int32_t **scratch;

   /*
    * dest_rb is thread specific and does not need a lock
    * Note:
    *  - The ringbuffer is singed 24-bit PCM, stereo or mono
    *  - capture functions should return the data in signed 24-bit
    */
   assert(dest_rb->sample);
   assert(delay);

   rbd = dest_rb->sample;
   scratch = (int32_t**)rbd->scratch;
   if (scratch)
   {
      unsigned int bps = rbd->bytes_sample;
      unsigned int ds = rbd->dde_samples;
      float dt = GMATH_E1 * *delay;
      size_t frames, nframes;
      int res;

      if (agc_rr > 0.0f)
      {
         agc_rr = _MINMAX(dt/agc_rr, 0.0f, 1.0f);
         gain *= -dest_rb->gain_agc;
      }

      nframes = frames = _oalRingBufferGetParami(dest_rb, RB_NO_SAMPLES);
      res = be->capture(be_handle, rbd->track, 0, &nframes,
                        scratch[SCRATCH_BUFFER0]-ds, ds+frames, gain);
      if (res && nframes)
      {
         float peak, rms, rms_rr, max, maxrms, maxpeak;
         unsigned int track, tracks;
         int32_t **tptr, **otptr;
         _oalRingBuffer *nrb;
         double sum;

         nrb = _oalRingBufferDuplicate(dest_rb, AAX_FALSE, AAX_FALSE);
         assert(nrb != 0);

         tptr = (int32_t **)nrb->sample->track;
         otptr = (int32_t **)rbd->track;

         rms_rr = _MINMAX(dt/0.2f, 0.0f, 1.0f);		// 200 ms RMS average
         maxrms = maxpeak = 0;
         tracks = rbd->no_tracks;
         for (track=0; track<tracks; track++)
         {
            int32_t *optr = otptr[track];

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
               _batch_fmadd(otptr[0], optr, frames, 1.0f, 0.0f);
            }
         }

         /* if downmix requested devide track0 by the number of tracks */
         if ((dest_track == AAX_TRACK_MIX) && tracks)
         {
            float fact = 1.0f/tracks;
            _batch_mul_value(otptr[0], sizeof(int32_t), frames, fact);
            dest_track = 0;
         }

         for (track=0; track<tracks; track++)
         {
            int32_t *ptr = tptr[track];
            int32_t *optr = otptr[track];
            unsigned int j;

            /* single channel requested, copy to the other channels */
            if ((dest_track != AAX_TRACK_ALL) && (track != dest_track)) {
               _aax_memcpy(optr, otptr[dest_track], frames*sizeof(int32_t));
            }

            /* copy the delay effects buffer */
            _aax_memcpy(ptr-ds, optr-ds+nframes, ds*bps);

            /** average RMS and peak values */
            sum = peak = 0;
            j = nframes;
            do
            {
               int32_t val = *optr++;
               float samp = (float)val*val;	// RMS

               sum += samp;
               if (samp > peak) peak = samp;
            }
            while (--j);

            rms = sqrt(sum/nframes);
            peak = sqrtf(peak);

            if (maxrms < rms) maxrms = rms;
            if (maxpeak < peak) maxpeak = peak;

            nrb->average[track] = (rms_rr*dest_rb->average[track]
                                + (1.0f-rms_rr)*rms);
            nrb->peak[track] = peak;
         }
         nrb->average[_AAX_MAX_SPEAKERS] = maxrms;
         nrb->peak[_AAX_MAX_SPEAKERS] = maxpeak;

         /** Automatic Gain Control */
         max = 0.0f;
         if (maxrms > 256) {
            max = _MIN(0.707f*8388607.0f/maxrms, 128.0f);
         }

         if (max < dest_rb->gain_agc) {
            nrb->gain_agc = 0.2f*dest_rb->gain_agc + 0.8f*max;
         } else {
            nrb->gain_agc = (1.0f-agc_rr)*dest_rb->gain_agc + (agc_rr)*max;
         }
         if (nrb->gain_agc < 0.01f) nrb->gain_agc = 0.01f;

         rv = nrb;
      }
      else {
         _oalRingBufferClear(dest_rb);
      }
      if (res <= 0) *delay = 0.0f;
   }

   return rv;
}

/*-------------------------------------------------------------------------- */

unsigned int
_aaxSoftwareMixerMixSensorsThreaded(void *dest, _intBuffers *hs)
{
   _oalRingBuffer *dest_rb = (_oalRingBuffer *)dest;
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
            const _aaxDriverBackend* be;
            void* be_handle;

            be = config->backend.ptr;
            be_handle = config->backend.handle;

            dptr_sensor = _intBufGet(config->sensors, _AAX_SENSOR, 0);
            if (dptr_sensor)
            {
               _intBuffers *ringbuffers;
               _aaxAudioFrame *mixer;
               _sensor_t* sensor;
               unsigned int nbuf;

               sensor = _intBufGetDataPtr(dptr_sensor);
               mixer = sensor->mixer;
               ringbuffers = mixer->ringbuffers;
               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

               nbuf = _intBufGetNumNoLock(ringbuffers, _AAX_RINGBUFFER);
               if (nbuf)
               {
                  _intBufferData *buf;
                  _oalRingBuffer *src_rb;
                  unsigned int rv = 0;

                  buf = _intBufGet(ringbuffers, _AAX_RINGBUFFER, 0);
                  src_rb = _intBufGetDataPtr(buf);
                  do
                  {
                     rv = be->mix2d(be_handle, dest_rb, src_rb, mixer->props2d,
                                               NULL, 0, 0);
                     _intBufReleaseData(buf, _AAX_RINGBUFFER);

                     if (rv) /* always streaming */
                     {
                        buf = _intBufPop(ringbuffers, _AAX_RINGBUFFER);
                        _oalRingBufferDelete(src_rb);
                        _intBufDestroyDataNoLock(buf);

                        buf = _intBufGet(ringbuffers, _AAX_RINGBUFFER, 0);
                        if (buf)
                        {
                           src_rb = _intBufGetDataPtr(buf);
                           /* since rv == AAX_TRUE this will be unlocked 
                              after be->mix2d */
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

