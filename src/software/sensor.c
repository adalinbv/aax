/*
 * Copyright 2005-2012 by Erik Hofman.
 * Copyright 2009-2012 by Adalin B.V.
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

#include <math.h>		/* for floor, and rint */
#include <errno.h>		/* for ETIMEDOUT */
#include <assert.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>		/* for struct time */
#endif

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>
#include <base/threads.h>
#include <base/types.h>		/* for msecSleep */

#include "audio.h"


void*
_aaxSoftwareMixerReadFrame(void *rb, const void* backend, void *handle, float *dt, float pos_sec)
{
   const _aaxDriverBackend* be = (const _aaxDriverBackend*)backend;
   _oalRingBuffer *dest_rb = (_oalRingBuffer*)rb;
   _oalRingBufferSample *rbd;
   int32_t **scratch;
   void *rv = rb;

   /*
    * dest_rb is thread specific and does not need a lock
    * Note:
    *  - The ringbuffer is singed 24-bit PCM, stereo or mono
    *  - capture functions should return the data in signed 24-bit
    */
   assert(dest_rb->sample);

   rbd = dest_rb->sample;
   scratch = (int32_t**)rbd->scratch;
   if (scratch)
   {
      unsigned int bps = rbd->bytes_sample;
      unsigned int ds = rbd->dde_samples;
      size_t frames, nframes;
      int res;

      nframes = frames = _oalRingBufferGetNoSamples(dest_rb);
      res = be->capture(handle, rbd->track, 0, &nframes,
                                scratch[0]-ds, ds+frames);
      if (res && nframes)
      {
//       float pitch = (float)(nframes)/(float)(frames);
         unsigned int t, tracks;
         _oalRingBuffer *nrb;

//       dest_rb->pitch_norm = pitch;
         nrb = _oalRingBufferDuplicate(dest_rb, AAX_FALSE, AAX_FALSE);

         ds -= frames-nframes;
#if 0
printf("ds: %i\n", ds);
#endif
// TODO: properly handle dde buffer from old to new ringbuffer
         tracks = rbd->no_tracks;
         for (t=0; t<tracks; t++)
         {
            int32_t *ptr = nrb->sample->track[t];
            int32_t *optr = rbd->track[t];
            _aax_memcpy(ptr-ds, optr-ds+nframes, ds*bps);
         }
         rv = nrb;
      }
      else {
         _oalRingBufferClear(dest_rb);
      }
      if (res <= 0) *dt = 0.0f;
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

               nbuf = _intBufGetNumNoLock(ringbuffers,_AAX_RINGBUFFER);
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
                                               NULL, 1.0f, 1.0f, 0, 0);
                     _intBufReleaseData(buf, _AAX_RINGBUFFER);

                     if (rv) /* always streaming */
                     {
                        void **ptr;

                        ptr =_intBufShiftIndex(ringbuffers,_AAX_RINGBUFFER,0,1);
                        if (ptr)
                        {
                           _oalRingBufferDelete(ptr[0]);
                           free(ptr);
                        }

                        if (--nbuf)
                        {
                           buf = _intBufGet(ringbuffers, _AAX_RINGBUFFER, 0);
                           src_rb = _intBufGetDataPtr(buf);
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

unsigned int
_aaxSoftwareMixerMixSensors(void *dest, const void *sensors, void *props2d)
{
   _oalRingBuffer *dest_rb = (_oalRingBuffer *)dest;
   unsigned int i, num = 0;
   if (sensors)
   {
      _intBuffers *hs = (_intBuffers *)sensors;

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
               const _intBufferData* sptr_rb;
               _oalRingBuffer *src_rb;
               _aaxAudioFrame *smixer;
               _sensor_t* sensor;
               _intBuffers *srbs;
               void *rv;
               float dt;

               sensor = _intBufGetDataPtr(dptr_sensor);
               smixer = sensor->mixer;
               src_rb = smixer->ringbuffer;
               dt = 1.0f / smixer->info->refresh_rate;
               srbs = smixer->ringbuffers;
               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

               rv = _aaxSoftwareMixerReadFrame(src_rb, be, be_handle, &dt,
                                               smixer->curr_pos_sec);
               if (dt == 0.0f)
               {
                  _SET_STOPPED(config);
                  _SET_PROCESSED(config); 
               }

               dptr_sensor = _intBufGet(config->sensors, _AAX_SENSOR, 0);
               if (dptr_sensor)
               {
                  if (rv != src_rb)
                  {
                     /**
                      * Add the new buffer to the buffer queue and pop the
                      * first buffer from the queue when needed (below).
                      * This way pitch effects (< 1.0) can be processed safely.
                      */
                     _oalRingBufferSetFrequency(src_rb,config->info->frequency);
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
                        _oalRingBuffer2dProps *p2d;
                        _oalRingBufferLFOInfo *lfo;

                        p2d = (_oalRingBuffer2dProps*)props2d;
                        lfo = _EFFECT_GET_DATA(p2d, DYNAMIC_PITCH_EFFECT);
                        if (lfo) {
                           p2d->final.pitch_lfo = lfo->get(lfo, NULL, 0, 0);
                        }
                        lfo = _FILTER_GET_DATA(p2d, DYNAMIC_GAIN_FILTER);
                        if (lfo && !lfo->envelope) {
                           p2d->final.gain_lfo = lfo->get(lfo, NULL, 0, 0);
                        }
                        rv = be->mix2d(be_handle, dest_rb, ssr_rb,
                                    smixer->props2d, props2d, 1.0f, 1.0f, 0, 0);
                        _intBufReleaseData(sptr_rb, _AAX_RINGBUFFER);

                        if (rv)	/* always streaming */
                        {
                           unsigned int nbuf;

                           nbuf = _intBufGetNumNoLock(srbs, _AAX_RINGBUFFER);
                           if (nbuf)
                           {
                              void **ptr;
                              ptr = _intBufShiftIndex(srbs,_AAX_RINGBUFFER,0,1);
                              if (ptr)
                              {
                                 nbuf--;
                                 _oalRingBufferDelete(ptr[0]);
                                 free(ptr);
                              }
                           }

                           if (nbuf)
                           {
                              sptr_rb = _intBufGet(srbs,_AAX_RINGBUFFER,0);
                              ssr_rb = _intBufGetDataPtr(sptr_rb);
                              /* since rv == AAX_TRUE this will be unlocked 
                                 after be->mix2d */
                           }
                           else rv = 0;
                        }
                     }
                     while(rv);
                  }
                  _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
               }
            }
            _intBufReleaseData(dptr, _AAX_DEVICE);
         }
      }
      _intBufReleaseNum(hs, _AAX_DEVICE);
   }
   return num;
}

