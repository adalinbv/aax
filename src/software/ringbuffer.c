/*
 * Copyright 2005-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
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
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <malloc.h>
# if HAVE_STRINGS_H
#  include <strings.h>   /* strcasecmp */
# endif
#endif
#include <math.h>
#ifndef NDEBUG
# include <stdio.h>
#endif

#include <api.h>

#include <base/types.h>
#include <base/logging.h>

#include "arch.h"
#include "ringbuffer.h"
#include "audio.h"

#ifndef _DEBUG
# define _DEBUG		0
#endif

static unsigned int _oalGetSetMonoSources(unsigned int, int);
static void _oalRingBufferIMA4ToPCM16(int32_t **__restrict,const void *__restrict,int,int,unsigned int);

int
_oalRingBufferIsValid(_oalRingBuffer *rb)
{
   int rv = 0;
   if (rb && rb->sample && rb->sample->track) {
      rv = -1;
   }
   return rv;
}


_oalRingBuffer *
_oalRingBufferCreate(float dde)
{
   _oalRingBuffer *rb;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   rb = (_oalRingBuffer *)calloc(1, sizeof(_oalRingBuffer));
   if (rb)
   {
      _oalRingBufferSample *rbd;

      rbd = (_oalRingBufferSample *)calloc(1, sizeof(_oalRingBufferSample));
      if (rbd)
      {
         float ddesamps;
         int format;
         /*
          * fill in the defaults
          */
         rb->sample = rbd;
         rbd->ref_counter = 1;

         rb->playing = 0;
         rb->stopped = 1;
         rb->streaming = 0;
         rb->dde_sec = dde;
         rb->gain_agc = 1.0f;
         rb->pitch_norm = 1.0f;
         rb->volume_min = 0.0f;
         rb->volume_max = 1.0f;
         rb->format = AAX_PCM16S;

         format = rb->format;
         rbd->no_tracks = 1;
         rbd->frequency_hz = 44100.0f;
         rbd->codec = _oalRingBufferCodecs[format];
         rbd->bytes_sample = _oalRingBufferFormat[format].bits/8;

         ddesamps = ceilf(dde * rbd->frequency_hz);
         rbd->dde_samples = (unsigned int)ddesamps;
         rbd->scratch = NULL;
      }
      else
      {
         free(rb);
         rb = NULL;
      }
   }

   return rb;
}

static void
_oalRingBufferInitTracks(_oalRingBuffer *rb)
{
   _oalRingBufferSample *rbd;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != NULL);

   rbd = rb->sample;

   if (!rbd->track)
   {
      unsigned int tracks, no_samples, tracksize, dde_bytes;
      char *ptr, *ptr2;
      char bps;

      bps = rbd->bytes_sample;
      no_samples = rbd->no_samples_avail;
      dde_bytes = TEST_FOR_TRUE(rb->dde_sec) ? (rbd->dde_samples * bps) : 0;
      if (dde_bytes & 0xF)
      {
         dde_bytes |= 0xF;
         dde_bytes++;
      }
      tracksize = dde_bytes + (no_samples + 0xF) * bps;

      /*
       * Create one buffer that can hold the data for all channels.
       * The first bytes are reserved for the track pointers
       */
#if BYTE_ALIGN
      /* 16-byte align every buffer */
      if (tracksize & 0xF)
      {
         tracksize |= 0xF;
         tracksize++;
      }

      tracks = rbd->no_tracks;
      ptr2 = (char*)(tracks * sizeof(void*));
      ptr = _aax_calloc(&ptr2, tracks, sizeof(void*) + tracksize);
#else
      ptr = ptr2 = calloc(tracks, sizeof(void*) + tracksize);
#endif
      if (ptr)
      {
         unsigned int i;

         rbd->track_len_bytes = no_samples * bps;
         rbd->duration_sec = (float)no_samples / rbd->frequency_hz;

         rbd->loop_start_sec = 0.0f;
         rbd->loop_end_sec = rbd->duration_sec;

         free(rbd->track);
         rbd->track = (void **)ptr;
         for (i=0; i<rbd->no_tracks; i++)
         {
            rbd->track[i] = ptr2 + dde_bytes;
            ptr2 += tracksize;
         }
      }
   }
}

void
_oalRingBufferInit(_oalRingBuffer *rb, char add_scratchbuf)
{
   _oalRingBufferSample *rbd;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != NULL);

   rbd = rb->sample;
   _oalRingBufferInitTracks(rb);
   rbd->no_samples = rbd->no_samples_avail;

   if (add_scratchbuf && rbd->scratch == NULL)
   {
      unsigned int i, tracks, no_samples, tracksize, dde_bytes;
      char *ptr, *ptr2;
      char bps;

      bps = rbd->bytes_sample;
      no_samples = rbd->no_samples_avail;
      dde_bytes = rbd->dde_samples*bps;
      if (dde_bytes & 0xF)
      {
         dde_bytes |= 0xF;
         dde_bytes++;
      }
      tracksize = 2*dde_bytes + no_samples*bps;
      tracks = MAX_SCRATCH_BUFFERS;

#if BYTE_ALIGN
      /* 16-byte align every buffer */
      if (tracksize & 0xF)
      {
         tracksize |= 0xF;
         tracksize++;
      }
      ptr2 = (char*)(tracks * sizeof(void*));
      ptr = _aax_calloc(&ptr2, tracks, sizeof(void*) + tracksize);
#else
      ptr = ptr2 = calloc(tracks, sizeof(void*) + tracksize);
#endif
      if (ptr)
      {
         rbd->scratch = (void **)ptr;
         for (i=0; i<tracks; i++)
         {
            rbd->scratch[i] = ptr2 + dde_bytes;
            ptr2 += tracksize;
         }
      }
   }
}


_oalRingBuffer *
_oalRingBufferReference(_oalRingBuffer *ringbuffer)
{
   _oalRingBuffer *rb;

   assert(ringbuffer != 0);

   rb = malloc(sizeof(_oalRingBuffer));
   if (rb)
   {
      memcpy(rb, ringbuffer, sizeof(_oalRingBuffer));
      rb->sample->ref_counter++;
      // rb->looping = 0;
      rb->playing = 0;
      rb->stopped = 1;
      rb->streaming = 0;
      rb->elapsed_sec = 0.0f;
      rb->curr_pos_sec = 0.0f;
      rb->curr_sample = 0;
// TODO: Is this really what we want? Looks suspiceous to me
      rb->sample->scratch = NULL;
   }

   return rb;
}

_oalRingBuffer *
_oalRingBufferDuplicate(_oalRingBuffer *ringbuffer, char copy, char dde)
{
   _oalRingBuffer *srb = ringbuffer;
   _oalRingBuffer *drb;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(ringbuffer != 0);

   drb = _oalRingBufferCreate(srb->dde_sec);
   if (drb)
   {
      _oalRingBufferSample *srbd, *drbd;
      char add_scratchbuf = AAX_FALSE;
      void *ptr;

      srbd = srb->sample;
      drbd = drb->sample;

      _aax_memcpy(drb, srb, sizeof(_oalRingBuffer));
      drb->sample = drbd;

      ptr = drbd->track;
      _aax_memcpy(drbd, srbd, sizeof(_oalRingBufferSample));
      drbd->track = ptr;
      drbd->scratch = NULL;

      if (srbd->scratch)
      {
         if (!copy)
         {
            drbd->scratch = srbd->scratch;
            srbd->scratch = NULL;
         }
         else {
            add_scratchbuf = AAX_TRUE;
         }
      }
      _oalRingBufferInit(drb, add_scratchbuf);

      if (copy || dde)
      {
         unsigned int t, ds, tracksize;

         tracksize = copy ? _oalRingBufferGetParami(srb, RB_NO_SAMPLES) : 0;
         tracksize *= _oalRingBufferGetParami(srb, RB_BYTES_SAMPLE);
         ds = dde ? srbd->dde_samples : 0;
         for (t=0; t<drbd->no_tracks; t++)
         {
            char *s, *d;

            s = (char *)srbd->track[t];
            d = (char *)drbd->track[t];
            _aax_memcpy(d-ds, s-ds, tracksize+ds);
         }
      }
   }

   return drb;
}

void
_oalRingBufferFillNonInterleaved(_oalRingBuffer *rb, const void *data, unsigned blocksize, char looping)
{
   unsigned int t, tracksize;
   _oalRingBufferSample *rbd;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);
   assert(rb->sample != 0);

   rbd = rb->sample;
   rb->looping = looping;

   tracksize = rbd->no_samples * rbd->bytes_sample;
   switch (rb->format)
   {
   case AAX_IMA4_ADPCM:
   {
#if 1
      printf("WARNING: AAX_IMA4_ADPCM not implemented for _oalRingBufferFillNonInterleaved\n");
      exit(-1);
#else
      int t;
      for (t=0; t<no_tracks; t++)
      {
         int32_t **track = &tracks[t];
         _oalRingBufferIMA4ToPCM16(track, data, 1, blocksize, no_samples);
      }
      rb->format = AAX_PCM16S;
#endif
      break;
   }
   default:
      for (t=0; t<rbd->no_tracks; t++)
      {
         char *s, *d;

         s = (char *)data + t*tracksize;
         d = (char *)rbd->track[t];
         _aax_memcpy(d, s, tracksize);
      }
   }

#if 0
   if (looping) {
      _oalRingBufferAddLooping(rb);
   }
#endif

   rb->playing = 0;
   rb->stopped = 1;
   rb->streaming = 0;
   rb->elapsed_sec = 0.0f;
   rb->curr_pos_sec = 0.0f;
   rb->curr_sample = 0;
   rb->looping = looping;
}

void
_oalRingBufferFillInterleaved(_oalRingBuffer *rb, const void *data, unsigned blocksize, char looping)
{
   unsigned int bps, no_samples, no_tracks, tracksize;
   _oalRingBufferSample *rbd;
   int32_t **tracks;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);
   assert(rb->sample != 0);
   assert(data != 0);

   rbd = rb->sample;
   rb->looping = looping;

   bps = rbd->bytes_sample;
   no_tracks = rbd->no_tracks;
   no_samples = rbd->no_samples;
   tracksize = no_samples * bps;
   tracks = (int32_t **)rbd->track;

   switch (rb->format)
   {
   case AAX_IMA4_ADPCM:
      _oalRingBufferIMA4ToPCM16(tracks, data, no_tracks, blocksize, no_samples);
      break;
   case AAX_PCM32S:
      _batch_cvt24_32_intl(tracks, data, 0, no_tracks, no_samples);
      
      break;
   case AAX_FLOAT:
      _batch_cvt24_ps_intl(tracks, data, 0, no_tracks, no_samples);
      break;
   case AAX_DOUBLE:
      _batch_cvt24_pd_intl(tracks, data, 0, no_tracks, no_samples);
      break;
   default:
      if (rbd->no_tracks == 1) {
         _aax_memcpy(rbd->track[0], data, tracksize);
      }
      else /* stereo */
      {
         unsigned int frame_size = no_tracks * bps;
         unsigned int t;

         for (t=0; t<no_tracks; t++)
         {
            char *sptr, *dptr;
            unsigned int i;

            sptr = (char *)data + t*bps;
            dptr = (char *)rbd->track[t];
            i = no_samples;
            do
            {
               memcpy(dptr, sptr, bps);
               sptr += frame_size;
               dptr += bps;
            }
            while (--i);
         }
      }
   } /* switch */

#if 0
   if (looping) {
      _oalRingBufferAddLooping(rb);
   }
#endif

   rb->playing = 0;
   rb->stopped = 1;
   rb->streaming = 0;
   rb->elapsed_sec = 0.0f;
   rb->curr_pos_sec = 0.0f;
   rb->curr_sample = 0;
   rb->looping = looping;
}


void
_oalRingBufferGetDataInterleaved(_oalRingBuffer *rb, void* data, unsigned int samples, int tracks, float fact)
{
   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);
   assert(rb->sample != 0);

   if (data)
   {
      _oalRingBufferSample *rbd = rb->sample;
      unsigned int t, no_tracks = rbd->no_tracks;
      unsigned int no_samples = rbd->no_samples;
      unsigned char bps = rbd->bytes_sample;
      void **ptr, **track = rbd->track;

      assert(samples >= (unsigned int)(fact*no_samples));

      if (no_tracks > tracks) no_tracks = tracks;

      fact = 1.0f/fact;
      ptr = track;
      if (fact != 1.0f)
      {
         unsigned int size = samples*bps;
         char *p;
         
         if (bps == sizeof(int32_t))
         {
            p = (char*)(no_tracks*sizeof(void*));
            track = (void**)_aax_malloc(&p, no_tracks*(sizeof(void*) + size));
            for (t=0; t<no_tracks; t++)
            {
               track[t] = p;
               _aaxProcessResample(track[t], ptr[t], 0, samples, 0, fact);
               p += size;
            }
         }
         else
         {
            unsigned int scratch_size;
            int32_t **scratch;
            char *sptr;

            scratch_size = 2*sizeof(int32_t*);
            sptr = (char*)scratch_size;
           
            scratch_size += (no_samples+samples)*sizeof(int32_t);
            scratch = (int32_t**)_aax_malloc(&sptr, scratch_size);
            scratch[0] = (int32_t*)sptr;
            scratch[1] = (int32_t*)(sptr + no_samples*sizeof(int32_t));

            p = (char*)(no_tracks*sizeof(void*));
            track = (void**)_aax_malloc(&p, no_tracks*(sizeof(void*) + size));
            for (t=0; t<no_tracks; t++)
            {
               track[t] = p;
               bufConvertDataToPCM24S(scratch[0], ptr[t], no_samples,
                                     rb->format);
               _aaxProcessResample(scratch[1], scratch[0], 0, samples, 0, fact);
               bufConvertDataFromPCM24S(track[t], scratch[1], 1, samples, 
                                        rb->format, 1);
               p += size;
            }
            free(scratch);
         }
      }

      if (no_tracks == 1) {
         _aax_memcpy(data, track[0], samples*bps);
      }
      else
      {
         for (t=0; t<no_tracks; t++)
         {
            uint8_t *s = (uint8_t*)track[t];
            uint8_t *d = (uint8_t *)data + t*bps;
            unsigned int i =  samples;
            do
            {
               memcpy(d, s, bps);
               d += no_tracks*bps;
               s += bps;
            }
            while (--i);
         }
      }

      if (ptr != track) free(track);
   }
}

void*
_oalRingBufferGetDataInterleavedMalloc(_oalRingBuffer *rb, int tracks, float fact)
{
   unsigned int samples;
   void *data;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);
   assert(rb->sample != 0);
  
   samples = (unsigned int)(fact*rb->sample->no_samples);
   data = malloc(tracks * samples*rb->sample->bytes_sample);
   if (data) {
      _oalRingBufferGetDataInterleaved(rb, data, samples, tracks, fact);
   }

   return data;
}

void
_oalRingBufferGetDataNonInterleaved(_oalRingBuffer *rb, void *data, unsigned int samples, int tracks, float fact)
{
   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);
   assert(rb->sample != 0);

   if (data)
   {
      _oalRingBufferSample *rbd = rb->sample;
      unsigned int t, no_tracks = rbd->no_tracks;
      unsigned int no_samples = rbd->no_samples;
      unsigned char bps = rbd->bytes_sample;
      void **ptr, **track = rbd->track;
      char *p = data;

      assert(samples >= (unsigned int)(fact*no_samples));

      memset(data, 0, no_samples*no_tracks*bps);
      if (no_tracks > tracks) no_tracks = tracks;

      fact = 1.0f/fact;
      ptr = track;
      if (fact != 1.0f)
      {
         unsigned int size = samples*bps;
         char *p;

         if (bps == sizeof(int32_t))
         {
            p = (char*)(no_tracks*sizeof(void*));
            track = (void**)_aax_malloc(&p, no_tracks*(sizeof(void*) + size));
            for (t=0; t<no_tracks; t++)
            {
               track[t] = p;
               _aaxProcessResample(track[t], ptr[t], 0, samples, 0, fact);
               p += size;
            }
         }
         else
         {
            unsigned int scratch_size;
            int32_t **scratch;
            char *sptr;

            scratch_size = 2*sizeof(int32_t*);
            sptr = (char*)scratch_size;

            scratch_size += (no_samples+samples)*sizeof(int32_t);
            scratch = (int32_t**)_aax_malloc(&sptr, scratch_size);
            scratch[0] = (int32_t*)sptr;
            scratch[1] = (int32_t*)(sptr + no_samples*sizeof(int32_t));

            p = (char*)(no_tracks*sizeof(void*));
            track = (void**)_aax_malloc(&p, no_tracks*(sizeof(void*) + size));
            for (t=0; t<no_tracks; t++)
            {
               track[t] = p;
               bufConvertDataToPCM24S(scratch[0], ptr[t], no_samples,
                                     rb->format);
               _aaxProcessResample(scratch[1], scratch[0], 0, samples, 0, fact);
               bufConvertDataFromPCM24S(track[t], scratch[1], 1, samples,
                                        rb->format, 1);
               p += size;
            }
            free(scratch);
         }
      }

      p = data;
      for (t=0; t<no_tracks; t++)
      {
         _aax_memcpy(p, rbd->track[t], rbd->track_len_bytes);
         p += rbd->track_len_bytes;
      }

      if (ptr != track) free(track);
   }
}

void*
_oalRingBufferGetDataNonInterleavedMalloc(_oalRingBuffer *rb, int tracks, float fact)
{
   unsigned int samples;
   void *data;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);
   assert(rb->sample != 0);

   samples = (unsigned int)(fact*rb->sample->no_samples);
   data = malloc(tracks * samples*rb->sample->bytes_sample);
   if (data) {
      _oalRingBufferGetDataNonInterleaved(rb, data, samples, tracks, fact);
   }

   return data;
}

void
_oalRingBufferClear(_oalRingBuffer *rb)
{
   _oalRingBufferSample *rbd;
   unsigned int i;

   /* _AAX_LOG(LOG_DEBUG, __FUNCTION__); */

   assert(rb != 0);

   rbd = rb->sample;
   assert(rbd != 0);
   assert(rbd->track);

   for (i=0; i<rbd->no_tracks; i++) {
      memset((void *)rbd->track[i], 0, rbd->track_len_bytes);
   }

// rb->sample = rbd;
// rb->reverb = reverb;

   rb->elapsed_sec = 0.0f;
   rb->pitch_norm = 1.0f;

   rb->curr_pos_sec = 0.0f;
   rb->curr_sample = 0;

// rb->format = fmt;
   rb->playing = 0;
   rb->stopped = 1;
   rb->looping = 0;
   rb->streaming = 0;
// rb->dde_sec = dde;
}

void
_oalRingBufferDelete(void *rbuf)
{
   _oalRingBuffer *rb = (_oalRingBuffer*)rbuf;
   _oalRingBufferSample *rbd;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);
   assert(rb->sample != 0);

   rbd = rb->sample;
   if (rbd && rbd->ref_counter > 0)
   {
      if (--rbd->ref_counter == 0)
      {
         free(rbd->track);
         rbd->track = NULL;

         free(rbd->scratch);
         rbd->scratch = NULL;

         free(rb->sample);
         rb->sample = NULL;
      }
      free(rb);
   }
}

void
_oalRingBufferRewind(_oalRingBuffer *rb)
{
   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);

   rb->curr_pos_sec = 0.0f;
   rb->curr_sample = 0;
}


void
_oalRingBufferForward(_oalRingBuffer *rb)
{
   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);

   rb->curr_pos_sec = rb->sample->duration_sec;
   rb->curr_sample = rb->sample->no_samples;
}


void
_oalRingBufferStart(_oalRingBuffer *rb)
{
   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);

// rb->playing = 1;
   rb->stopped = 0;
   rb->streaming = 0;
}


void
_oalRingBufferStop(_oalRingBuffer *rb)
{
   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);

// rb->playing = 0;
   rb->stopped = 1;
   rb->streaming = 0;
}


void
_oalRingBufferStartStreaming(_oalRingBuffer *rb)
{
   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);

// rb->playing = 1;
   rb->stopped = 0;
   rb->streaming = 1;
}

int
_oalRingBufferSetParamf(_oalRingBuffer *rb, enum _oalRingBufferParam param, float fval)
{
   _oalRingBufferSample *rbd = rb->sample;
   int rv = AAX_TRUE;

   switch(param)
   {
   case RB_VOLUME:
      rb->volume_norm = fval;
      break;
   case RB_VOLUME_MIN:
      rb->volume_min = fval;
      break;
   case RB_VOLUME_MAX:
      rb->volume_max = fval;
      break;
   case RB_FREQUENCY:
      rbd->frequency_hz = fval;
      rbd->duration_sec = rbd->no_samples / fval;
      rbd->dde_samples = (unsigned int)ceilf(fval * rb->dde_sec);

      fval = rbd->frequency_hz / fval;
      rbd->loop_start_sec *= fval;
      rbd->loop_end_sec *= fval;
      break;
   case RB_DURATION_SEC:
      rbd->duration_sec = fval;
      fval *= rbd->frequency_hz;
      rv = _oalRingBufferSetParami(rb, RB_NO_SAMPLES, rintf(fval));
      break;
   case RB_LOOPPOINT_START:
      if (fval < rbd->loop_end_sec)
      {
         rbd->loop_start_sec = fval;
//       _oalRingBufferAddLooping(rb);
      }
      break;
   case RB_LOOPPOINT_END:
      if ((rbd->loop_start_sec < fval) && (fval <= rbd->duration_sec))
      {
         rbd->loop_end_sec = fval;
//       _oalRingBufferAddLooping(rb);
      }
      break;
   case RB_OFFSET_SEC:
      if (fval > rbd->duration_sec) {
         fval = rbd->duration_sec;
      }
      rb->curr_pos_sec = fval;
      rb->curr_sample = rintf(fval*rbd->frequency_hz);
      break;
   default:
#ifndef NDEBUG
      printf("UNKNOWN PARAMETER %i at line %i\n", param, __LINE__);
#endif
      break;
   }

   return rv;
}

int
_oalRingBufferSetParami(_oalRingBuffer *rb, enum _oalRingBufferParam param, unsigned int val)
{
   _oalRingBufferSample *rbd = rb->sample;
   int rv = AAX_TRUE;

   switch(param)
   {
   case RB_BYTES_SAMPLE:
      if (rbd->track == NULL) {
         rbd->bytes_sample = val;
      }
#ifndef NDEBUG
      else printf("Unable set bytes/sample when rbd->track == NULL\n");
      break;
#endif
   case RB_NO_TRACKS:
      if ((rbd->track == NULL) || (val <= rbd->no_tracks)) {
         rbd->no_tracks = val;
      }
#ifndef NDEBUG
      else printf("Unable set the no. tracks rbd->track == NULL\n");
#endif
      break;
   case RB_LOOPING:
      rb->looping = val ? AAX_TRUE : AAX_FALSE;
      rb->loop_max = (val > AAX_TRUE) ? val : 0;
#if 0
      if (loops) {
         _oalRingBufferAddLooping(rb);
      }
#endif
      break;
   case RB_LOOPPOINT_START:
   {
      float fval = val/rbd->frequency_hz;
      if (fval < rbd->loop_end_sec)
      {
         rbd->loop_start_sec = fval;
//       _oalRingBufferAddLooping(rb);
      }
      break;
   }
   case RB_LOOPPOINT_END:
   {
      float fval = val/rbd->frequency_hz;
      if ((rbd->loop_start_sec < fval) && (val <= rbd->no_samples))
      {
         rbd->loop_end_sec = fval;
//       _oalRingBufferAddLooping(rb);
      }
      break;
   }
   case RB_OFFSET_SAMPLES:
      if (val > rbd->no_samples) {
         val = rbd->no_samples;
      }
      rb->curr_sample = val;
      rb->curr_pos_sec = (float)val / rbd->frequency_hz;
      break;
   case RB_TRACKSIZE:
      val /= rbd->bytes_sample;
      /* no break needed */
   case RB_NO_SAMPLES:
      if (rbd->track == NULL)
      {
         rbd->no_samples_avail = val;
         rbd->duration_sec = (float)val / rbd->frequency_hz;
         rv = AAX_TRUE;
      }
      else if (val <= rbd->no_samples_avail)
      {
         /**
          * Note:
          * Sensors rely in the fact that resizing to a smaller bufferr-size
          * does not alter the actual buffer size, so it can overrun if required
          */
         rbd->no_samples = val;
         rbd->duration_sec = (float)val / rbd->frequency_hz;
         rv = AAX_TRUE;
      }
      else if (val > rbd->no_samples_avail)
      {
         rbd->no_samples_avail = val;
         _oalRingBufferInitTracks(rb);
         rv = AAX_TRUE;
      }
#ifndef NDEBUG
      else if (rbd->track == NULL) {
         printf("Unable to set no. tracks when rbd->track != NULL");
      } else {
         printf("%s: Unknown error\n", __FUNCTION__);
      }
#endif
      break;
   case RB_FORMAT:
   default:
#ifndef NDEBUG
      printf("UNKNOWN PARAMETER %i at line %i\n", param, __LINE__);
#endif
      rv = AAX_FALSE;
      break;
   }

   return rv;
}

float
_oalRingBufferGetParamf(const _oalRingBuffer *rb, enum _oalRingBufferParam param)
{
// _oalRingBufferSample *rbd = rb->sample;
   float rv = AAX_NONE;

   switch(param)
   {
   case RB_VOLUME:
      rv = rb->volume_norm;
      break;
   case RB_VOLUME_MIN:
      rv = rb->volume_min;
      break;
   case RB_VOLUME_MAX:
      rv = rb->volume_max;
      break;
   case RB_FREQUENCY:
      rv = rb->sample->frequency_hz;
      break;
   case RB_DURATION_SEC:
      rv = rb->sample->duration_sec;
      break;
   case RB_OFFSET_SEC:
      rv = rb->curr_pos_sec;
      break;
   default:
#ifndef NDEBUG
      printf("UNKNOWN PARAMETER %i at line %i\n", param, __LINE__);
#endif
      break;
   }

   return rv;
}

unsigned int
_oalRingBufferGetParami(const _oalRingBuffer *rb, enum _oalRingBufferParam param)
{
   _oalRingBufferSample *rbd = rb->sample;
   unsigned int rv = -1;
   switch(param)
   {
   case RB_NO_TRACKS:
      rv = rb->sample->no_tracks;
      break;
   case RB_NO_SAMPLES:
      rv = rb->sample->no_samples;
      break;
   case RB_TRACKSIZE:
      rv = rb->sample->track_len_bytes;
      break;
   case RB_BYTES_SAMPLE:
      rv = rb->sample->bytes_sample;
      break;
   case RB_FORMAT:
      rv = _oalRingBufferFormat[rb->format].format;
      break;
   case RB_LOOPING:
      rv = rb->loop_max ? rb->loop_max : rb->looping;
      break;
   case RB_LOOPPOINT_START:
      rv = (unsigned int)(rbd->loop_start_sec * rbd->frequency_hz);
      break;
   case RB_LOOPPOINT_END:
      rv = (unsigned int)(rbd->loop_end_sec * rbd->frequency_hz);
      break;
   case RB_OFFSET_SAMPLES:
      rv = rb->curr_sample;
      break;
   case RB_IS_PLAYING:
      rv = !(rb->playing == 0 && rb->stopped == 1);
      break;
   default:
#ifndef NDEBUG
      printf("UNKNOWN PARAMETER %i at line %i\n", param, __LINE__);
#endif
      break;
   }

   return rv;
}

int
_oalRingBufferSetFormat(_oalRingBuffer *rb, _aaxCodec **codecs, enum aaxFormat format)
{
   _oalRingBufferSample *rbd;
   int rv = AAX_TRUE;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);
   assert(format < AAX_FORMAT_MAX);

   rbd = rb->sample;
   if (rbd->track == NULL)
   {
      if (codecs == 0) {
         codecs = _oalRingBufferCodecs;
      }
      rb->format = format;
      rbd->codec = codecs[format];
      rbd->bytes_sample = _oalRingBufferFormat[format].bits/8;
   }
#ifndef NDEBUG
   else printf("%s: Can't set value when rbd->track != NULL\n", __FUNCTION__);
#endif

   return rv;
}

unsigned int
_oalRingBufferGetNoSources()
{
   int num = _oalGetSetMonoSources(0, 0);
   if (num > _AAX_MAX_MIXER_REGISTERED) num = _AAX_MAX_MIXER_REGISTERED;
   return num;
}

unsigned int
_oalRingBufferSetNoSources(unsigned int max)
{
   return _oalGetSetMonoSources(max, 0);
}

unsigned int
_oalRingBufferGetSource() {
   return _oalGetSetMonoSources(0, 1);
}

unsigned int
_oalRingBufferPutSource() {
   return _oalGetSetMonoSources(0, -1);
}

/* -------------------------------------------------------------------------- */

_oalFormat_t _oalRingBufferFormat[AAX_FORMAT_MAX] =
{
  {  8, AAX_PCM8S },	/* 8-bit  */
  { 16, AAX_PCM16S },	/* 16-bit */
  { 32, AAX_PCM24S },	/* 24-bit */
  { 32, AAX_PCM24S },	/* 32-bit gets converted to 24-bit */
  { 32, AAX_PCM24S },	/* float gets converted to 24-bit  */
  { 32, AAX_PCM24S },	/* double gets converted to 24-bit */
  {  8, AAX_MULAW },	/* mu-law  */
  {  8, AAX_ALAW },	/* a-law */
  { 16, AAX_PCM16S }	/* IMA4-ADPCM gets converted to 16-bit */
};

static unsigned int
_oalGetSetMonoSources(unsigned int max, int num)
{
   static unsigned int _max_sources = _AAX_MAX_SOURCES_AVAIL;
   static unsigned int _sources = _AAX_MAX_SOURCES_AVAIL;
   unsigned int abs_num = abs(num);
   unsigned int ret = _sources;

   if (max)
   {
      if (max > _AAX_MAX_SOURCES_AVAIL) max = _AAX_MAX_SOURCES_AVAIL;
      _max_sources = max;
      _sources = max;
      ret = max;
   }

   if (abs_num && (abs_num < _AAX_MAX_MIXER_REGISTERED))
   {
      unsigned int _src = _sources - num;
      if ((_sources >= (unsigned int)num) && (_src < _max_sources))
      {
         _sources = _src;
         ret = abs_num;
      }
   }

   return ret;
}

/*
 * Convert 4-bit IMA to 16-bit PCM
 */
static void
_oalRingBufferIMA4ToPCM16(int32_t **__restrict dst, const void *__restrict src, int tracks, int blocksize, unsigned int no_samples)
{
   unsigned int i, blocks, block_smp;
   int16_t *d[_AAX_MAX_SPEAKERS];
   uint8_t *s = (uint8_t *)src;
   int t;

   if (tracks > _AAX_MAX_SPEAKERS)
      return;

   /* copy buffer pointers */
   for(t=0; t<tracks; t++) {
      d[t] = (int16_t*)dst[t];
   }

   block_smp = BLOCKSIZE_TO_SMP(blocksize);
   blocks = no_samples / block_smp;
   i = blocks-1;
   do
   {
      for (t=0; t<tracks; t++)
      {
         _sw_bufcpy_ima_adpcm(d[t], s, 1, block_smp);
         d[t] += block_smp;
         s += blocksize;
      }
   }
   while (--i);

   no_samples -= blocks*block_smp;
   if (no_samples)
   {
      int t;
      for (t=0; t<tracks; t++)
      {
         _sw_bufcpy_ima_adpcm(d[t], s, 1, no_samples);
         s += blocksize;
      }
   }
}

