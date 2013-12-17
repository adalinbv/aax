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

#include "software/arch.h"
#include "software/ringbuffer.h"
#include "software/audio.h"

#ifndef _DEBUG
# define _DEBUG		0
#endif

static void _aaxRingBufferInitFunctions(_aaxRingBuffer*);
static int _aaxRingBufferClear(_aaxRingBufferData*);

static _aaxFormat_t _aaxRingBufferFormat[AAX_FORMAT_MAX];

_aaxRingBuffer *
_aaxRingBufferCreate(float dde)
{
   _aaxRingBuffer *rb;
   unsigned int size;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   size = sizeof(_aaxRingBuffer) + sizeof(_aaxRingBufferData);
   rb = (_aaxRingBuffer *)calloc(1, size);
   if (rb)
   {
      _aaxRingBufferData *rbi;
      _aaxRingBufferSample *rbd;
      char *ptr;

      ptr = (char*)rb;
      rbi = (_aaxRingBufferData*)(ptr + sizeof(_aaxRingBuffer));
      rb->id = rbi;
#ifndef NDEBUG
      rb->id->parent = rb;
#endif

      rbd = (_aaxRingBufferSample *)calloc(1, sizeof(_aaxRingBufferSample));
      if (rbd)
      {
         float ddesamps;
         int format;
         /*
          * fill in the defaults
          */
         rbi->sample = rbd;
         rbd->ref_counter = 1;

         rbi->playing = 0;
         rbi->stopped = 1;
         rbi->streaming = 0;
         rbi->dde_sec = dde;
         rbi->gain_agc = 1.0f;
         rbi->pitch_norm = 1.0f;
         rbi->volume_min = 0.0f;
         rbi->volume_max = 1.0f;
         rbi->format = AAX_PCM16S;

         format = rbi->format;
         rbd->no_tracks = 1;
         rbd->frequency_hz = 44100.0f;
         rbd->codec = _aaxRingBufferCodecs[format];
         rbd->bytes_sample = _aaxRingBufferFormat[format].bits/8;

         ddesamps = ceilf(dde * rbd->frequency_hz);
         rbd->dde_samples = (unsigned int)ddesamps;
         rbd->scratch = NULL;

         _aaxRingBufferInitFunctions(rb);
      }
      else
      {
         free(rb);
         rb = NULL;
      }
   }

   return rb;
}

void
_aaxRingBufferDestroy(void *rbuf)
{
   _aaxRingBuffer *rb = (_aaxRingBuffer*)rbuf;
   _aaxRingBufferSample *rbd;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);
   assert(rb->id->sample != 0);

   rbd = rb->id->sample;

   if (rbd && rbd->ref_counter > 0)
   {
      if (--rbd->ref_counter == 0)
      {
         free(rbd->track);
         rbd->track = NULL;

         free(rbd->scratch);
         rbd->scratch = NULL;

         free(rb->id->sample);
         rb->id->sample = NULL;
      }
      free(rb);
   }
}

static void
_aaxRingBufferInitTracks(_aaxRingBufferData *rbi)
{
   _aaxRingBufferSample *rbd;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rbi != NULL);
   assert(rbi->parent == (char*)rbi-sizeof(_aaxRingBuffer));

   rbd = rbi->sample;

   if (!rbd->track)
   {
      unsigned int tracks, no_samples, tracksize, dde_bytes;
      char *ptr, *ptr2;
      char bps;

      bps = rbd->bytes_sample;
      no_samples = rbd->no_samples_avail;
      dde_bytes = TEST_FOR_TRUE(rbi->dde_sec) ? (rbd->dde_samples * bps) : 0;
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
_aaxRingBufferInit(_aaxRingBuffer *rb, char add_scratchbuf)
{
   _aaxRingBufferSample *rbd;
   _aaxRingBufferData *rbi;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != NULL);

   rbi = rb->id;
   assert(rbi != NULL);
   assert(rbi->parent == (char*)rbi-sizeof(_aaxRingBuffer));

   rbd = rbi->sample;
   _aaxRingBufferInitTracks(rbi);
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


_aaxRingBuffer *
_aaxRingBufferReference(_aaxRingBuffer *ringbuffer)
{
   _aaxRingBuffer *rb;

   assert(ringbuffer != 0);

   rb = malloc(sizeof(_aaxRingBuffer) + sizeof(_aaxRingBufferData));
   if (rb)
   {
      _aaxRingBufferData *rbi;
      char *ptr = (char*)rb;

      rb->id = (_aaxRingBufferData*)(ptr + sizeof(_aaxRingBuffer));
      rbi = rb->id;
#ifndef NDEBUG
      rb->id->parent = rb;
#endif

      memcpy(rbi, ringbuffer->id, sizeof(_aaxRingBuffer));
      rbi->sample->ref_counter++;
      // rb->looping = 0;
      rbi->playing = 0;
      rbi->stopped = 1;
      rbi->streaming = 0;
      rbi->elapsed_sec = 0.0f;
      rbi->curr_pos_sec = 0.0f;
      rbi->curr_sample = 0;
// TODO: Is this really what we want? Looks suspiceous to me
      rbi->sample->scratch = NULL;

      _aaxRingBufferInitFunctions(rb);
   }

   return rb;
}

_aaxRingBuffer *
_aaxRingBufferDuplicate(_aaxRingBuffer *ringbuffer, char copy, char dde)
{
   _aaxRingBuffer *srb = ringbuffer;
   _aaxRingBuffer *drb;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(ringbuffer != 0);

   drb = _aaxRingBufferCreate(srb->id->dde_sec);
   if (drb)
   {
      _aaxRingBufferSample *srbd, *drbd;
      char add_scratchbuf = AAX_FALSE;
      void *ptr;

      srbd = srb->id->sample;
      drbd = drb->id->sample;

      _aax_memcpy(drb->id, srb->id, sizeof(_aaxRingBuffer));
      drb->id->sample = drbd;
#ifndef NDEBUG
      drb->id->parent = drb;
#endif

      ptr = drbd->track;
      _aax_memcpy(drbd, srbd, sizeof(_aaxRingBufferSample));
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
      _aaxRingBufferInit(drb, add_scratchbuf);

      if (copy || dde)
      {
         unsigned int t, ds, tracksize;

         tracksize = copy ? srb->get_parami(srb, RB_NO_SAMPLES) : 0;
         tracksize *= srb->get_parami(srb, RB_BYTES_SAMPLE);
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

int32_t**
_aaxRingBufferGetTracksPtr(_aaxRingBufferData *rbi, enum _aaxRingBufferMode mode)
{
   _aaxRingBufferSample *rbd;
   int32_t **rv = NULL;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rbi != 0);
   assert(rbi->sample != 0);
   assert(rbi->parent == (char*)rbi-sizeof(_aaxRingBuffer));

   rbd = rbi->sample;
   if (rbd) {
      rv  = (int32_t**)rbd->track;
   }
   return rv;
}

int
_aaxRingBufferReleaseTracksPtr(_aaxRingBufferData *rbi)
{
   return AAX_TRUE;
}

void**
_aaxRingBufferGetScratchBufferPtr(_aaxRingBufferData *rbi)
{
   _aaxRingBufferSample *rbd;
   void **rv = NULL;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rbi != 0);
   assert(rbi->sample != 0);
   assert(rbi->parent == (char*)rbi-sizeof(_aaxRingBuffer));

   rbd = rbi->sample;
   if (rbd) {
      rv  = (void**)rbd->scratch;
   }
   return rv;
}

void
_aaxRingBufferSetState(_aaxRingBuffer *rb, enum _aaxRingBufferState state)
{
   _aaxRingBufferData *rbi;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != NULL);

   rbi = rb->id;
   assert(rbi != 0);
   assert(rbi->sample != 0);
   assert(rbi->parent == rb);

   switch (state)
   {
   case RB_CLEARED:
      _aaxRingBufferClear(rbi);

      rbi->elapsed_sec = 0.0f;
      rbi->pitch_norm = 1.0f;
      rbi->curr_pos_sec = 0.0f;
      rbi->curr_sample = 0;

      rbi->playing = 0;
      rbi->stopped = 1;
      rbi->looping = 0;
      rbi->streaming = 0;
      break;
   case RB_REWINDED:
      rbi->curr_pos_sec = 0.0f;
      rbi->curr_sample = 0;
      break;
   case RB_FORWARDED:
      rbi->curr_pos_sec = rbi->sample->duration_sec;
      rbi->curr_sample = rbi->sample->no_samples;
      break;
   case RB_STARTED:
//    rbi->playing = 1;
      rbi->stopped = 0;
      rbi->streaming = 0;
      break;
   case RB_STOPPED:
//    rbi->playing = 0;
      rbi->stopped = 1;
      rbi->streaming = 0;
      break;
   case RB_STARTED_STREAMING:
//    rbi->playing = 1;
      rbi->stopped = 0;
      rbi->streaming = 1;
      break;
   default:
     break;
   }
}

int
_aaxRingBufferGetState(_aaxRingBuffer *rb, enum _aaxRingBufferState state)
{
// _aaxRingBufferSample *rbd;
   _aaxRingBufferData *rbi;
   int rv = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != NULL);

   rbi = rb->id;
   assert(rbi != 0);
   assert(rbi->sample != 0);
   assert(rbi->parent == rb);

// rbd = rbi->sample;

   switch (state)
   {
   case RB_IS_VALID:
      if (rb && rbi->sample && rbi->sample->track) {
         rv = -1;
      }
      break;
   default:
      break;
   }
   return rv;
}

int
_aaxRingBufferSetParamf(_aaxRingBuffer *rb, enum _aaxRingBufferParam param, float fval)
{
   _aaxRingBufferSample *rbd;
   _aaxRingBufferData *rbi;
   int rv = AAX_TRUE;

   assert(rb != NULL);

   rbi = rb->id;
   assert(rbi != NULL);
   assert(rbi->sample != 0);
   assert(rbi->parent == rb);

   rbd = rbi->sample;
   switch(param)
   {
   case RB_VOLUME:
      rbi->volume_norm = fval;
      break;
   case RB_VOLUME_MIN:
      rbi->volume_min = fval;
      break;
   case RB_VOLUME_MAX:
      rbi->volume_max = fval;
      break;
   case RB_FREQUENCY:
      rbd->frequency_hz = fval;
      rbd->duration_sec = rbd->no_samples / fval;
      rbd->dde_samples = (unsigned int)ceilf(fval * rbi->dde_sec);

      fval = rbd->frequency_hz / fval;
      rbd->loop_start_sec *= fval;
      rbd->loop_end_sec *= fval;
      break;
   case RB_DURATION_SEC:
   {
      int val;

      rbd->duration_sec = fval;
      fval *= rbd->frequency_hz;
      val = rintf(fval);

      // same code as for _aaxRingBufferSetParami with RN_NO_SAMPLES
      if (rbd->track == NULL)
      {
         rbd->no_samples_avail = val;
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
         rv = AAX_TRUE;
      }
      else if (val > rbd->no_samples_avail)
      {
         rbd->no_samples_avail = val;
         _aaxRingBufferInitTracks(rbi);
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
   }
   case RB_LOOPPOINT_START:
      if (fval < rbd->loop_end_sec)
      {
         rbd->loop_start_sec = fval;
//       _aaxRingBufferAddLooping(rb);
      }
      break;
   case RB_LOOPPOINT_END:
      if ((rbd->loop_start_sec < fval) && (fval <= rbd->duration_sec))
      {
         rbd->loop_end_sec = fval;
//       _aaxRingBufferAddLooping(rb);
      }
      break;
   case RB_OFFSET_SEC:
      if (fval > rbd->duration_sec) {
         fval = rbd->duration_sec;
      }
      rbi->curr_pos_sec = fval;
      rbi->curr_sample = rintf(fval*rbd->frequency_hz);
      break;
   default:
      if ((param >= RB_PEAK_VALUE) &&
          (param <= RB_PEAK_VALUE+_AAX_MAX_SPEAKERS))
      {
         rbi->peak[param-RB_PEAK_VALUE] = fval;
      }
      else if ((param >= RB_AVERAGE_VALUE) &&
               (param <= RB_AVERAGE_VALUE+_AAX_MAX_SPEAKERS))
      {
         rbi->average[param-RB_AVERAGE_VALUE] = fval;
      }
#ifndef NDEBUG
      else {
         printf("UNKNOWN PARAMETER %x at line %i\n", param, __LINE__);
      }
#endif
      break;
   }

   return rv;
}

int
_aaxRingBufferSetParami(_aaxRingBuffer *rb, enum _aaxRingBufferParam param, unsigned int val)
{
   _aaxRingBufferSample *rbd;
   _aaxRingBufferData *rbi;
   int rv = AAX_TRUE;

   assert(rb != NULL);

   rbi = rb->id;
   assert(rbi != NULL);
   assert(rbi->sample != 0);
   assert(rbi->parent == (char*)rbi-sizeof(_aaxRingBuffer));

   rbd = rbi->sample;
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
      rbi->looping = val ? AAX_TRUE : AAX_FALSE;
      rbi->loop_max = (val > AAX_TRUE) ? val : 0;
#if 0
      if (loops) {
         _aaxRingBufferAddLooping(rb);
      }
#endif
      break;
   case RB_LOOPPOINT_START:
   {
      float fval = val/rbd->frequency_hz;
      if (fval < rbd->loop_end_sec)
      {
         rbd->loop_start_sec = fval;
//       _aaxRingBufferAddLooping(rb);
      }
      break;
   }
   case RB_LOOPPOINT_END:
   {
      float fval = val/rbd->frequency_hz;
      if ((rbd->loop_start_sec < fval) && (val <= rbd->no_samples))
      {
         rbd->loop_end_sec = fval;
//       _aaxRingBufferAddLooping(rb);
      }
      break;
   }
   case RB_OFFSET_SAMPLES:
      if (val > rbd->no_samples) {
         val = rbd->no_samples;
      }
      rbi->curr_sample = val;
      rbi->curr_pos_sec = (float)val / rbd->frequency_hz;
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
         _aaxRingBufferInitTracks(rbi);
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
   case RB_STATE:
      _aaxRingBufferSetState(rb, val);
      rv = AAX_TRUE;
      break;
   case RB_FORMAT:
   default:
#ifndef NDEBUG
      printf("UNKNOWN PARAMETER %x at line %i\n", param, __LINE__);
#endif
      rv = AAX_FALSE;
      break;
   }

   return rv;
}

float
_aaxRingBufferGetParamf(const _aaxRingBuffer *rb, enum _aaxRingBufferParam param)
{
// _aaxRingBufferSample *rbd = rbi->sample;
   _aaxRingBufferData *rbi;
   float rv = AAX_NONE;

   assert(rb != NULL);

   rbi = rb->id;
   assert(rbi != NULL);
   assert(rbi->parent == rb);

   switch(param)
   {
   case RB_VOLUME:
      rv = rbi->volume_norm;
      break;
   case RB_VOLUME_MIN:
      rv = rbi->volume_min;
      break;
   case RB_VOLUME_MAX:
      rv = rbi->volume_max;
      break;
   case RB_AGC_VALUE:
      rv = rbi->gain_agc;
      break;
   case RB_FREQUENCY:
      rv = rbi->sample->frequency_hz;
      break;
   case RB_DURATION_SEC:
      rv = rbi->sample->duration_sec;
      break;
   case RB_OFFSET_SEC:
      rv = rbi->curr_pos_sec;
      break;
   default:
      if ((param >= RB_PEAK_VALUE) && (param < RB_PEAK_VALUE+_AAX_MAX_SPEAKERS))
      {
         rv = rbi->peak[param-RB_PEAK_VALUE];
      }
      else if ((param >= RB_AVERAGE_VALUE) &&
               (param < RB_AVERAGE_VALUE+_AAX_MAX_SPEAKERS))
      {
         rv = rbi->average[param-RB_AVERAGE_VALUE];
      }
#ifndef NDEBUG
      else {
         printf("UNKNOWN PARAMETER %x at line %x\n", param, __LINE__);
      }
#endif
      break;
   }

   return rv;
}

unsigned int
_aaxRingBufferGetParami(const _aaxRingBuffer *rb, enum _aaxRingBufferParam param)
{
   _aaxRingBufferSample *rbd;
   _aaxRingBufferData *rbi;
   unsigned int rv = -1;

   assert(rb != NULL);

   rbi = rb->id;
   assert(rbi != NULL);
   assert(rbi->sample != 0);
   assert(rbi->parent == rb);

   rbd = rbi->sample;
   switch(param)
   {
   case RB_NO_TRACKS:
      rv = rbi->sample->no_tracks;
      break;
   case RB_NO_SAMPLES:
      rv = rbi->sample->no_samples;
      break;
   case RB_NO_SAMPLES_AVAIL:
      rv = rbi->sample->no_samples_avail;
      break;
   case RB_TRACKSIZE:
      rv = rbi->sample->track_len_bytes;
      break;
   case RB_BYTES_SAMPLE:
      rv = rbi->sample->bytes_sample;
      break;
   case RB_INTERNAL_FORMAT:
      rv = _aaxRingBufferFormat[rbi->format].format;
      break;
   case RB_FORMAT:
      rv = rbi->format;
      break;
   case RB_LOOPING:
      rv = rbi->loop_max ? rbi->loop_max : rbi->looping;
      break;
   case RB_LOOPPOINT_START:
      rv = (unsigned int)(rbd->loop_start_sec * rbd->frequency_hz);
      break;
   case RB_LOOPPOINT_END:
      rv = (unsigned int)(rbd->loop_end_sec * rbd->frequency_hz);
      break;
   case RB_OFFSET_SAMPLES:
      rv = rbi->curr_sample;
      break;
   case RB_DDE_SAMPLES:
      rv = rbd->dde_samples;
      break;
   case RB_IS_PLAYING:
      rv = !(rbi->playing == 0 && rbi->stopped == 1);
      break;
   default:
#ifndef NDEBUG
      printf("UNKNOWN PARAMETER %x at line %i\n", param, __LINE__);
#endif
      break;
   }

   return rv;
}

int
_aaxRingBufferSetFormat(_aaxRingBuffer *rb, _aaxCodec **codecs, enum aaxFormat format)
{
   _aaxRingBufferSample *rbd;
   _aaxRingBufferData *rbi;
   int rv = AAX_TRUE;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != NULL);

   rbi = rb->id;
   assert(rbi != 0);
   assert(rbi->sample != 0);
   assert(format < AAX_FORMAT_MAX);
   assert(rbi->parent == rb);

   rbd = rbi->sample;
   if (rbd->track == NULL)
   {
      if (codecs == 0) {
         codecs = _aaxRingBufferCodecs;
      }
      rbi->format = format;
      rbd->codec = codecs[format];
      rbd->bytes_sample = _aaxRingBufferFormat[format].bits/8;
   }
#ifndef NDEBUG
   else printf("%s: Can't set value when rbd->track != NULL\n", __FUNCTION__);
#endif

   return rv;
}

int
_aaxRingBufferDataMixWaveform(_aaxRingBuffer *rb, enum aaxWaveformType type, float f, float ratio, float phase)
{
   unsigned int tracks, no_samples;
   unsigned char bps;
   int rv = AAX_FALSE;
   void *data;

   bps = rb->get_parami(rb, RB_BYTES_SAMPLE);
   tracks = rb->get_parami(rb, RB_NO_TRACKS);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);

   data = rb->id->sample->track;
   switch (type)
   {
   case AAX_SINE_WAVE:
      _bufferMixSineWave(data, f, bps, no_samples, tracks, ratio, phase);
      rv = AAX_TRUE;
      break;
   case AAX_SQUARE_WAVE:
      _bufferMixSquareWave(data, f, bps, no_samples, tracks, ratio, phase);
      rv = AAX_TRUE;
      break;
   case AAX_TRIANGLE_WAVE:
      _bufferMixTriangleWave(data, f, bps, no_samples, tracks, ratio, phase);
      rv = AAX_TRUE;
      break;
   case AAX_SAWTOOTH_WAVE:
      _bufferMixSawtooth(data, f, bps, no_samples, tracks, ratio, phase);
      rv = AAX_TRUE;
      break;
   case AAX_IMPULSE_WAVE:
      _bufferMixImpulse(data, f, bps, no_samples, tracks, ratio, phase);
      rv = AAX_TRUE;
      break;
   default:
      break;
   }
   return rv;
}

int
_aaxRingBufferDataMixNoise(_aaxRingBuffer *rb, enum aaxWaveformType type, float f, float ratio, float dc, char skip)
{
   unsigned int tracks, no_samples;
   unsigned char bps;
   int rv = AAX_FALSE;
   void *data;

   bps = rb->get_parami(rb, RB_BYTES_SAMPLE);
   tracks = rb->get_parami(rb, RB_NO_TRACKS);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);

   data = rb->id->sample->track;
   switch (type)
   {
   case AAX_WHITE_NOISE:
      _bufferMixWhiteNoise(data, no_samples, bps, tracks, ratio, dc, skip);
      rv = AAX_TRUE;
      break;
   case AAX_PINK_NOISE:
      if (f) 
      {
         _bufferMixPinkNoise(data, no_samples, bps, tracks, ratio, f, dc, skip);
         rv = AAX_TRUE;
      }
      break;
   case AAX_BROWNIAN_NOISE:
     if (f)
     {
         _bufferMixBrownianNoise(data, no_samples, bps, tracks, ratio, f, dc, skip);
         rv = AAX_TRUE;
      }
      break;
   default:
      break;
   }
   return rv;
}

int
_aaxRingBufferDataMultiply(_aaxRingBuffer *rb, size_t offs, size_t no_samples, float ratio_orig)
{
   _aaxRingBufferData *rbi = rb->id;
   unsigned int t, tracks;
   unsigned char bps;
   int32_t *data;

   bps = rb->get_parami(rb, RB_BYTES_SAMPLE);
   tracks = rb->get_parami(rb, RB_NO_TRACKS);
   if (!no_samples)
   {
      no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
      offs = 0;
   }

   for (t=0; t<tracks; t++)
   {
      data = rbi->sample->track[t];
      _batch_mul_value(data+offs, bps, no_samples, ratio_orig);
   }
   return AAX_TRUE;
}

int
_aaxRingBufferDataMixData(_aaxRingBuffer *drb, _aaxRingBuffer *srb, _aaxRingBufferLFOInfo *lfo)
{
   unsigned char track, tracks;
   unsigned int dno_samples;
   float g = 1.0f;

   dno_samples =  drb->get_parami(drb, RB_NO_SAMPLES);
   tracks =  drb->get_parami(drb, RB_NO_TRACKS);

   if (lfo && lfo->envelope)
   {
       g = 0.0f;
       for (track=0; track<tracks; track++)
       {
           int32_t *sptr = srb->id->sample->track[track];
           float gain =  lfo->get(lfo, sptr, track, dno_samples);

           if (lfo->inv) gain = 1.0f/g;
           g += gain;
       }
       g /= tracks;
   }

   for (track=0; track<tracks; track++)
   {
      int32_t *dptr = drb->id->sample->track[track];
      int32_t *sptr = srb->id->sample->track[track];
      float gstep = 0.0f;

      _batch_fmadd(dptr, sptr, dno_samples, g, gstep);
   }
   return AAX_TRUE;
}

int
_aaxRingBufferDataClear(_aaxRingBuffer*rb) {
   return _aaxRingBufferClear(rb->id);
}

void
_aaxRingBufferCopyDelyEffectsData(_aaxRingBufferData *di, const _aaxRingBufferData *si)
{
   _aaxRingBufferSample *srbd, *drbd;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(si);
   assert(di);
   assert(si->parent == (char*)si-sizeof(_aaxRingBuffer));
   assert(di->parent == (char*)di-sizeof(_aaxRingBuffer));

   srbd = si->sample;
   drbd = di->sample;

   if (srbd->bytes_sample == drbd->bytes_sample)
   {
      unsigned int t, tracks, ds, bps;

      ds = srbd->dde_samples;
      if (ds > drbd->dde_samples) {
         ds = drbd->dde_samples;
      }

      tracks = srbd->no_tracks;
      if (tracks > drbd->no_tracks) {
         tracks = drbd->no_tracks;
      }

      bps = srbd->bytes_sample;
      for (t=0; t<tracks; t++)
      {
         int32_t *sptr = srbd->track[t];
         int32_t *dptr = drbd->track[t];
         _aax_memcpy(dptr-ds, sptr-ds, ds*bps);
      }
   }
}


/* -------------------------------------------------------------------------- */

static _aaxFormat_t _aaxRingBufferFormat[AAX_FORMAT_MAX] =
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

_aaxRingBufferMixMNFn _aaxRingBufferMixMulti16;
_aaxRingBufferMix1NFn _aaxRingBufferMixMono16;

static int
_aaxRingBufferClear(_aaxRingBufferData *rbi)
{
   _aaxRingBufferSample *rbd;
   unsigned int i;

   assert(rbi->parent == (char*)rbi-sizeof(_aaxRingBuffer));

   rbd = rbi->sample;
   for (i=0; i<rbd->no_tracks; i++) {
      memset((void *)rbd->track[i], 0, rbd->track_len_bytes);
   }

   return AAX_TRUE;
}

static void
_aaxRingBufferInitFunctions(_aaxRingBuffer *rb)
{
   rb->init = _aaxRingBufferInit;
   rb->reference = _aaxRingBufferReference;
   rb->duplicate = _aaxRingBufferDuplicate;

   rb->set_state = _aaxRingBufferSetState;
   rb->get_state = _aaxRingBufferGetState;
   rb->set_format = _aaxRingBufferSetFormat;

   rb->set_paramf = _aaxRingBufferSetParamf;
   rb->set_parami = _aaxRingBufferSetParami;
   rb->get_paramf = _aaxRingBufferGetParamf;
   rb->get_parami = _aaxRingBufferGetParami;

   rb->mix2d = _aaxRingBufferMixMulti16;
   rb->mix3d = _aaxRingBufferMixMono16;

   rb->get_scratch = _aaxRingBufferGetScratchBufferPtr;

   rb->get_tracks_ptr = _aaxRingBufferGetTracksPtr;
   rb->release_tracks_ptr = _aaxRingBufferReleaseTracksPtr;

   rb->data_clear = _aaxRingBufferDataClear;
   rb->data_multiply = _aaxRingBufferDataMultiply;
   rb->data_mix_waveform = _aaxRingBufferDataMixWaveform;
   rb->data_mix_noise = _aaxRingBufferDataMixNoise;
   rb->data_mix = _aaxRingBufferDataMixData;

   rb->copy_effectsdata = _aaxRingBufferCopyDelyEffectsData;
}
