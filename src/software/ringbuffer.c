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
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <malloc.h>
#endif
#include <math.h>
#ifndef NDEBUG
# include <stdio.h>
#endif

#include <base/types.h>
#include <base/logging.h>
#include <base/random.h>

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>
#include <dsp/filters.h>
#include <dsp/lfo.h>

#include "audio.h"
#include "rbuf_int.h"
#include "renderer.h"

#ifndef _DEBUG
# define _DEBUG		0
#endif
#define RB_ID		0x81726354

static int _aaxRingBufferClear(_aaxRingBufferData*, char);
static void _aaxRingBufferInitFunctions(_aaxRingBuffer*);

static _aaxFormat_t _aaxRingBufferFormat[AAX_FORMAT_MAX];

_aaxRingBuffer *
_aaxRingBufferAlloc()
{
   _aaxRingBuffer *rb;
   size_t size;

   _AAX_LOG(LOG_DEBUG, __func__);

   size = sizeof(_aaxRingBuffer) + sizeof(_aaxRingBufferData);
   rb = (_aaxRingBuffer *)calloc(1, size);
   if (rb)
   {
      _aaxRingBufferData *rbi;
      _aaxRingBufferSample *rbd;
      char *ptr;

      ptr = (char*)rb;
      rbi = (_aaxRingBufferData*)(ptr + sizeof(_aaxRingBuffer));

      rb->id = RB_ID;
      rb->handle = rbi;

      rbd = (_aaxRingBufferSample *)calloc(1, sizeof(_aaxRingBufferSample));
      rbi->sample = rbd;
      if (!rbd)
      {
         free(rb);
         rb = NULL;
      }
   }

   return rb;
}

_aaxRingBuffer *
_aaxRingBufferCreate(float dde, enum aaxRenderMode mode)
{
   _aaxRingBuffer *rb;

   _AAX_LOG(LOG_DEBUG, __func__);

   rb = _aaxRingBufferAlloc();
   if (rb)
   {
      _aaxRingBufferData *rbi = rb->handle;
      _aaxRingBufferSample *rbd = rbi->sample;
      if (rbd)
      {
         size_t ddesamps;

         /*
          * fill in the defaults
          */
         rbd->ref_counter = 1;

         rbi->playing = 0;
         rbi->stopped = 1;
         rbi->streaming = 0;
         rbi->gain_agc = 1.0f;
         rbi->pitch_norm = 1.0;
         rbi->volume_min = 0.0f;
         rbi->volume_max = 1.0f;
         rbi->codec = _aaxRingBufferProcessCodec;	// always cvt to 24-bit
//       rbi->effects_1st = _aaxRingBufferEffectsApply1st;
         rbi->effects_2nd = _aaxRingBufferEffectsApply2nd;
         rbi->mix = _aaxRingBufferProcessMixer;		// uses the above funcs

         rbi->mode = mode;
         rbi->access = RB_RW_MAX;
#ifndef NDEBUG
         rbi->parent = rb;
#endif

         rbd->dde_sec = dde;
         rbd->no_tracks = 1;
         rbd->frequency_hz = 44100.0f;
         rbd->format = AAX_PCM16S;
         rbd->codec = _aaxRingBufferCodecs[rbd->format];
         rbd->bytes_sample = _aaxRingBufferFormat[rbd->format].bits/8;
#if RB_FLOAT_DATA
         rbd->freqfilter = _batch_freqfilter_float;
         rbd->resample = _batch_resample_float;
         rbd->multiply = _batch_fmul_value;
         rbd->add = _batch_fmadd;
#else
         rbd->freqfilter = _batch_freqfilter;
         rbd->resample = _batch_resample;
         rbd->multiply = _batch_imul_value;
         rbd->add = _batch_imadd;
#endif
         rbd->mix1 = _aaxRingBufferMixMono16Mono;
         rbd->mixmn = _aaxRingBufferMixStereo16;
         switch(rbi->mode)
         {
         case AAX_MODE_WRITE_SPATIAL:
           rbd->mix1n = _aaxRingBufferMixMono16Spatial;
            break;
         case AAX_MODE_WRITE_SURROUND:
            rbd->mix1n = _aaxRingBufferMixMono16Surround;
            break;
         case AAX_MODE_WRITE_HRTF:
            rbd->mix1n = _aaxRingBufferMixMono16HRTF;
            break;
         case AAX_MODE_WRITE_STEREO:
         default:
            rbd->mix1n = _aaxRingBufferMixMono16Stereo;
            break;
         }

         ddesamps = ceilf(dde * rbd->frequency_hz);
         rbd->dde_samples = ddesamps ? ddesamps : HISTORY_SAMPS;
         rbd->scratch = NULL;

         _aaxRingBufferInitFunctions(rb);
      }
   }

   return rb;
}

void
_aaxRingBufferDestroy(_aaxRingBuffer *rb)
{
   _aaxRingBufferSample *rbd;
   _aaxRingBufferData *rbi;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(rb != 0);
   assert(rb->id == RB_ID);

   rbi = rb->handle;
   assert(rbi->sample != 0);

   rbd = rbi->sample;

   if (rbd && rbd->ref_counter > 0)
   {
      if (--rbd->ref_counter == 0)
      {
         if (rbd->track) _aax_free(rbd->track);
         rbd->track = NULL;

         if (rbd->scratch) free(rbd->scratch);
         rbd->scratch = NULL;

         free(rbi->sample);
         rbi->sample = NULL;
      }
      rb->id = FADEDBAD;
      free(rb);
   }
}

void
_aaxRingBufferFree(void *ringbuffer)
{
   _aaxRingBuffer *rb = (_aaxRingBuffer*)ringbuffer;
   rb->destroy(rb);
}

static void
_aaxRingBufferInitTracks(_aaxRingBufferData *rbi)
{
   _aaxRingBufferSample *rbd;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(rbi != NULL);
   assert(rbi->parent == (char*)rbi-sizeof(_aaxRingBuffer));

   rbd = rbi->sample;

   /*
    * When moving playing registered sensors from one output device
    * to another rbd->track != NULL.  calculate it's new size, free
    * the previous buffer and create a new one with the proper size
    */
   if (1) // !rbd->track)
   {
      unsigned int tracks;
      size_t offs, no_samples, tracksize, dde_bytes;
      char *ptr, *ptr2;
      char bps;

      bps = rbd->bytes_sample;
      no_samples = rbd->no_samples_avail;
      dde_bytes = SIZE_ALIGNED(rbd->dde_samples * bps);

      /*
       * Create one buffer that can hold the data for all channels.
       * The first bytes are reserved for the track pointers
       */
      tracksize = dde_bytes + (no_samples + MEMMASK) * bps;

      /* 16-byte align every buffer */
      tracksize = SIZE_ALIGNED(tracksize);

      tracks = rbd->no_tracks;
      offs = tracks * sizeof(void*);
      ptr = _aax_calloc(&ptr2, offs, tracks, tracksize);
      if (ptr)
      {
         size_t i;

         rbd->track_len_bytes = no_samples * bps;
         rbd->duration_sec = (float)no_samples / rbd->frequency_hz;

         rbd->loop_start_sec = 0.0f;
         rbd->loop_end_sec = rbd->duration_sec;

         if (rbd->track) free(rbd->track);
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

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(rb != NULL);

   rbi = rb->handle;
   assert(rbi != NULL);
   assert(rbi->parent == rb);

   rbd = rbi->sample;
   _aaxRingBufferInitTracks(rbi);

   if (add_scratchbuf && rbd->scratch == NULL)
   {
      size_t i, offs, tracks, no_samples, tracksize, dde_bytes;
      char *ptr, *ptr2;
      char bps;

      bps = rbd->bytes_sample;
      no_samples = rbd->no_samples_avail;
      dde_bytes = rbd->dde_samples*bps;
      dde_bytes = SIZE_ALIGNED(dde_bytes);

      // scratch buffers have two delay effects sections
      tracksize = 2*dde_bytes + no_samples*bps;
      tracks = MAX_SCRATCH_BUFFERS;

      /* align every buffer */
      tracksize = SIZE_ALIGNED(tracksize);

      offs = tracks * sizeof(void*);
      ptr = _aax_calloc(&ptr2, offs, tracks, tracksize);
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

      rbi = (_aaxRingBufferData*)(ptr + sizeof(_aaxRingBuffer));

      rb->id = RB_ID;
      rb->handle = rbi;

      memcpy(rbi, ringbuffer->handle, sizeof(_aaxRingBufferData));

      rbi->sample->ref_counter++;
      rbi->playing = 0;
      rbi->stopped = 1;
      rbi->streaming = 0;
      rbi->elapsed_sec = 0.0f;
      rbi->curr_pos_sec = 0.0;
      rbi->curr_sample = 0;
      rbi->sample->scratch = NULL;

#ifndef NDEBUG
      rbi->parent = rb;
#endif

      _aaxRingBufferInitFunctions(rb);
   }

   return rb;
}

_aaxRingBuffer *
_aaxRingBufferDuplicate(_aaxRingBuffer *ringbuffer, char copy, char dde)
{
   _aaxRingBuffer *srb = ringbuffer;
   _aaxRingBufferData *srbi;
   _aaxRingBuffer *drb;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(ringbuffer != 0);

   srbi = srb->handle;
   drb = _aaxRingBufferAlloc();
   if (drb)
   {
      _aaxRingBufferSample *srbd, *drbd;
      _aaxRingBufferData *drbi;
      char add_scratchbuf = AAX_FALSE;
      void *ptr;

      srbd = srbi->sample;

      drbi = drb->handle;
      drbd = drbi->sample;

      _aaxRingBufferInitFunctions(drb);

      _aax_memcpy(drbi, srbi, sizeof(_aaxRingBufferData));
      drbi->access = RB_RW_MAX;
      drbi->sample = drbd;
#ifndef NDEBUG
      drbi->parent = drb;
#endif

      ptr = drbd->track;
      _aax_memcpy(drbd, srbd, sizeof(_aaxRingBufferSample));
      drbd->track = ptr;
      drbd->scratch = NULL;
      if (!dde)
      {
         drbd->dde_sec = 0.0f;
         drbd->dde_samples = HISTORY_SAMPS;
      }

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
         size_t t, ds, tracksize;

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
_aaxRingBufferGetTracksPtr(_aaxRingBuffer *rb, enum _aaxRingBufferMode mode)
{
   _aaxRingBufferData *rbi;
   _aaxRingBufferSample *rbd;
   int32_t **rv = NULL;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(rb != NULL);

   rbi = rb->handle;
   assert(rbi != 0);
   assert(rbi->sample != 0);
   assert(rbi->parent == rb);
   assert(rbi->access == RB_RW_MAX); // track was previously released

   rbd = rbi->sample;
   if (rbd)
   {
      rbi->access = mode;
#if RB_FLOAT_DATA
      if (rbd->mixer_fmt && (rbi->access & RB_READ))
      {
         _aaxRingBufferSample *rbd = rbi->sample;
         unsigned int track, no_tracks = rbd->no_tracks;
         size_t no_samples = rbd->no_samples;
         void **tracks = rbd->track;
         for (track=0; track<no_tracks; track++) {
            _batch_cvt24_ps24(tracks[track], tracks[track], no_samples);
         }
      }
#endif
      rv  = (int32_t**)rbd->track;
   }
   return rv;
}

int
_aaxRingBufferReleaseTracksPtr(_aaxRingBuffer *rb)
{
   _aaxRingBufferData *rbi;
#if RB_FLOAT_DATA
   _aaxRingBufferSample *rbd;
#endif

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(rb != NULL);

   rbi = rb->handle;
   assert(rbi != 0);
   assert(rbi->sample != 0);
   assert(rbi->parent == rb);
   assert(rbi->access != RB_RW_MAX);

#if RB_FLOAT_DATA
   rbd = rbi->sample;
   if (rbd->mixer_fmt && (rbi->access & RB_WRITE))
   {
      _aaxRingBufferSample *rbd = rbi->sample;
      unsigned int track, no_tracks = rbd->no_tracks;
      size_t no_samples = rbd->no_samples;
      void **tracks = rbd->track;
      for (track=0; track<no_tracks; track++) {
         _batch_cvtps24_24(tracks[track], tracks[track], no_samples);
      }
   }
#endif
   rbi->access = RB_RW_MAX;
   return AAX_TRUE;
}

void**
_aaxRingBufferGetScratchBufferPtr(_aaxRingBuffer *rb)
{
   _aaxRingBufferData *rbi;
   _aaxRingBufferSample *rbd;
   void **rv = NULL;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(rb != NULL);

   rbi = rb->handle;
   assert(rbi != 0);
   assert(rbi->sample != 0);
   assert(rbi->parent == rb);

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
   _aaxRingBufferSample *rbd;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(rb != NULL);

   rbi = rb->handle;
   assert(rbi != 0);
   assert(rbi->sample != 0);
   assert(rbi->parent == rb);

   switch (state)
   {
   case RB_CLEARED:
   case RB_CLEARED_DDE:
      _aaxRingBufferClear(rbi, (state == RB_CLEARED) ? AAX_FALSE : AAX_TRUE);

      rbi->elapsed_sec = 0.0f;
      rbi->pitch_norm = 1.0;
      rbi->curr_pos_sec = 0.0;
      rbi->curr_sample = 0;
      rbi->loop_no = 0;

      rbi->playing = 0;
      rbi->stopped = 1;
      rbi->looping = 0;
      rbi->streaming = 0;
      break;
   case RB_REWINDED:
      rbd = rbi->sample;
      rbi->loop_no = 0;
      rbi->looping = rbi->loop_mode;
      if (!rbi->looping || rbd->loop_start_sec ||
          (rbd->loop_end_sec < rbd->duration_sec))
      {
         rbi->curr_pos_sec = 0.0;
         rbi->curr_sample = 0;
         rbi->looping = 0;
      }
      break;
   case RB_FORWARDED:
      rbi->curr_pos_sec = rbi->sample->duration_sec;
      rbi->curr_sample = rbi->sample->no_samples;
      rbi->loop_no = rbi->loop_max;
      rbi->looping = 0;
      break;
   case RB_STARTED:
      rbi->playing = 1;
      rbi->stopped = 0;
      rbi->streaming = 0;
      break;
   case RB_STOPPED:
      if (rbi->sampled_release) {
         rbi->looping = 0;
      }
      else
      {
         rbi->playing = 0;
         rbi->stopped = 1;
         rbi->streaming = 0;
      }
      break;
   case RB_STARTED_STREAMING:
      rbi->playing = 1;
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

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(rb != NULL);

   rbi = rb->handle;
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
_aaxRingBufferSetParamd(_aaxRingBuffer *rb, enum _aaxRingBufferParam param, FLOAT fval)
{
   _aaxRingBufferSample *rbd;
   _aaxRingBufferData *rbi;
   int rv = AAX_TRUE;

   assert(rb != NULL);

   rbi = rb->handle;
   assert(rbi != NULL);
   assert(rbi->sample != 0);
   assert(rbi->parent == rb);

   rbd = rbi->sample;
   switch(param)
   {
   case RB_FORWARD_SEC:
   {
      float eps = 0.0f;

      if (rbi->streaming) {
         eps = 1.1f/rbd->frequency_hz;
      }

      fval += rbi->curr_pos_sec;
      if (rbi->looping && (fval >= rbd->loop_end_sec))
      {
         FLOAT loop_start_sec = rbd->loop_start_sec;
         FLOAT loop_length_sec = rbd->loop_end_sec - loop_start_sec;

         fval -= loop_start_sec;
         rbi->loop_no += floor(fval/loop_length_sec);
         fval = fmod(fval, loop_length_sec);
         fval += loop_start_sec;

         if (rbi->loop_max && (rbi->loop_no >= rbi->loop_max)) {
            rbi->looping = AAX_FALSE;
         }
      }

      if (fval >= (rbd->duration_sec-eps))
      {
         fval = rbd->duration_sec;
//       rbi->playing = 0;
         rbi->stopped = 1;
      }
      rbi->curr_pos_sec = fval;
      rbi->curr_sample = floorf(fval*rbd->frequency_hz);
      rv = AAX_TRUE;
      break;
   }
   default:
#ifndef NDEBUG
      printf("UNKNOWN PARAMETER %x at line %i\n", param, __LINE__);
#endif
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

   rbi = rb->handle;
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
   case RB_AGC_VALUE:
      rbi->gain_agc = fval;
      break;
   case RB_FREQUENCY:
      rbd->frequency_hz = fval;
      rbd->duration_sec = (fval > 0) ? (float)rbd->no_samples/fval : 0.0f;
      rbd->dde_samples = (size_t)ceilf(rbd->dde_sec * rbd->frequency_hz);
      if (!rbd->dde_samples) rbd->dde_samples = HISTORY_SAMPS;
      break;
   case RB_DURATION_SEC:
   {
      unsigned int val;

      rbd->duration_sec = fval;
      fval *= rbd->frequency_hz;
      val = ceilf(fval);

      // same code as for _aaxRingBufferSetParami with RB_NO_SAMPLES
      if (rbd->track == NULL)
      {
         rbd->no_samples_avail = val;
         rbd->no_samples = val;
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
         rbd->no_samples = val;
         _aaxRingBufferInitTracks(rbi);
         rv = AAX_TRUE;
      }
#ifndef NDEBUG
      else if (rbd->track == NULL) {
         printf("Unable to set no. tracks when rbd->track != NULL");
      } else {
         printf("%s: Unknown error\n", __func__);
      }
#endif
      break;
   }
   case RB_LOOPPOINT_START:
      if (fval < rbd->loop_end_sec) {
         rbd->loop_start_sec = fval;
         rv = AAX_TRUE;
      }
      break;
   case RB_LOOPPOINT_END:
      if ((rbd->loop_start_sec < fval) && (fval <= rbd->duration_sec)) {
         rbd->loop_end_sec = fval;
         rv = AAX_TRUE;
      }
      break;
   case RB_OFFSET_SEC:
      if (fval > rbd->duration_sec) {
         fval = rbd->duration_sec;
      }
      rbi->curr_pos_sec = fval;
      rbi->curr_sample = floorf(fval*rbd->frequency_hz);
      rv = AAX_TRUE;
      break;
   default:
      if ((param >= RB_PEAK_VALUE) &&
          (param <= RB_PEAK_VALUE_MAX))
      {
         rbi->peak[param-RB_PEAK_VALUE] = fval;
      }
      else if ((param >= RB_AVERAGE_VALUE) &&
               (param <= RB_AVERAGE_VALUE_MAX))
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
   int rv = AAX_FALSE;

   assert(rb != NULL);

   rbi = rb->handle;
   assert(rbi != NULL);
   assert(rbi->sample != 0);
   assert(rbi->parent == rb);

   rbd = rbi->sample;
   switch(param)
   {
   case RB_IS_MIXER_BUFFER:
#if RB_FLOAT_DATA
      if (rbd->mixer_fmt != val)
      {
         _aaxRingBufferSample *rbd = rbi->sample;
         unsigned int track, no_tracks = rbd->no_tracks;
         size_t no_samples = rbd->no_samples;
         void **tracks = rbd->track;
         if (tracks)
         {
            for (track=0; track<no_tracks; track++)
            {
               if (val) {
                  _batch_cvtps24_24(tracks[track], tracks[track], no_samples);
               } else {
                  _batch_cvt24_ps24(tracks[track], tracks[track], no_samples);
               }
            }
         }
      }
#endif
      rbd->mixer_fmt = (val != 0) ? AAX_TRUE : AAX_FALSE;
      break;
   case RB_BYTES_SAMPLE:
      if (rbd->track == NULL) {
         rbd->bytes_sample = val;
         rv = AAX_TRUE;
      }
#ifndef NDEBUG
      else printf("Unable set bytes/sample when rbd->track == NULL\n");
      break;
#endif
   case RB_NO_TRACKS:
      if ((rbd->track == NULL) || (val <= rbd->no_tracks)) {
         rbd->no_tracks = val;
         rv = AAX_TRUE;
      }
#ifndef NDEBUG
      else printf("Unable set the no. tracks rbd->track == NULL\n");
#endif
      break;
   case RB_LOOPING:
      rbi->looping = rbi->loop_mode = val ? AAX_TRUE : AAX_FALSE;
      rbi->loop_max = (val > AAX_TRUE) ? val : 0;
      rv = AAX_TRUE;
      break;
   case RB_LOOPPOINT_START:
   {
      float fval = val/rbd->frequency_hz;

      if (fval < rbd->loop_end_sec)
      {
         rbd->loop_start_sec = fval;
         rv = AAX_TRUE;
      }
      break;
   }
   case RB_LOOPPOINT_END:
   {
      float fval = val/rbd->frequency_hz;
      if ((rbd->loop_start_sec < fval) && (fval <= rbd->duration_sec))
      {
         rbd->loop_end_sec = fval;
         rv = AAX_TRUE;
      }
      break;
   }
   case RB_SAMPLED_RELEASE:
      rbi->sampled_release = val ? AAX_TRUE : AAX_FALSE;
      break;
   case RB_OFFSET_SAMPLES:
      if (val > rbd->no_samples) {
         val = rbd->no_samples;
      }
      rbi->curr_sample = val;
      rbi->curr_pos_sec = (double)val / rbd->frequency_hz;
      rv = AAX_TRUE;
      break;
   case RB_TRACKSIZE:
      val /= rbd->bytes_sample;
      // intentional fallthrough
   case RB_NO_SAMPLES:
      if (rbd->track == NULL)
      {
         rbd->no_samples_avail = val;
         rbd->no_samples = val;
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
         rbd->no_samples = val;
         _aaxRingBufferInitTracks(rbi);
         rv = AAX_TRUE;
      }
#ifndef NDEBUG
      else if (rbd->track == NULL) {
         printf("Unable to set no. tracks when rbd->track != NULL");
      } else {
         printf("%s: Unknown error\n", __func__);
      }
#endif
      break;
   case RB_STATE:
      _aaxRingBufferSetState(rb, val);
      rv = AAX_TRUE;
      break;
   case RB_FORMAT:
      rbd->format = val;
      rbd->codec = _aaxRingBufferCodecs[rbd->format];
      rbd->bytes_sample = _aaxRingBufferFormat[rbd->format].bits/8;
      break;
   default:
      if ((param >= RB_PEAK_VALUE) &&
          (param <= RB_PEAK_VALUE_MAX))
      {
         rv = rbi->peak[param-RB_PEAK_VALUE];
      }
      else if ((param >= RB_AVERAGE_VALUE) &&
               (param <= RB_AVERAGE_VALUE_MAX))
      {
         rv = rbi->average[param-RB_AVERAGE_VALUE];
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

float
_aaxRingBufferGetParamf(const _aaxRingBuffer *rb, enum _aaxRingBufferParam param)
{
// _aaxRingBufferSample *rbd;
   _aaxRingBufferData *rbi;
   float rv = AAX_NONE;

   assert(rb != NULL);

   rbi = rb->handle;
   assert(rbi != NULL);
   assert(rbi->parent == rb);

// rbd = rbi->sample;
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
      if ((param >= RB_PEAK_VALUE) &&
          (param <= RB_PEAK_VALUE_MAX))
      {
         rv = rbi->peak[param-RB_PEAK_VALUE];
      }
      else if ((param >= RB_AVERAGE_VALUE) &&
               (param <= RB_AVERAGE_VALUE_MAX))
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
   size_t rv = -1;

   assert(rb != NULL);

   rbi = rb->handle;
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
      rv = _aaxRingBufferFormat[rbd->format].format;
      break;
   case RB_LOOPING:
      rv = rbi->loop_max ? rbi->loop_max : rbi->looping;
      break;
   case RB_FORMAT:
      rv = rbd->format;
      break;
   case RB_LOOPPOINT_START:
      rv = (size_t)floorf(rbd->loop_start_sec * rbd->frequency_hz);
      break;
   case RB_LOOPPOINT_END:
      rv = (size_t)ceilf(rbd->loop_end_sec * rbd->frequency_hz);
      break;
   case RB_OFFSET_SAMPLES:
      rv = rbi->curr_sample;
      break;
   case RB_SAMPLED_RELEASE:
      rv = rbi->sampled_release;
      break;
   case RB_DDE_SAMPLES:
      rv = rbd->dde_samples;
      break;
   case RB_IS_PLAYING:
      rv = (rbi->playing == 0 && rbi->stopped == 1) ? AAX_FALSE : AAX_TRUE;
      break;
   case RB_IS_MIXER_BUFFER:
      rv = (rbd->mixer_fmt != AAX_FALSE) ? AAX_TRUE : AAX_FALSE;
      break;
   default:
      if ((param >= RB_PEAK_VALUE) &&
          (param <= RB_PEAK_VALUE_MAX))
      {
         rv = rbi->peak[param-RB_PEAK_VALUE];
      }
      else if ((param >= RB_AVERAGE_VALUE) &&
               (param <= RB_AVERAGE_VALUE_MAX))
      {
         rv = rbi->average[param-RB_AVERAGE_VALUE];
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
_aaxRingBufferSetFormat(_aaxRingBuffer *rb, enum aaxFormat format, int mixer)
{
   _aaxRingBufferSample *rbd;
   _aaxRingBufferData *rbi;
   int rv = AAX_TRUE;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(rb != NULL);

   rbi = rb->handle;
   assert(rbi != 0);
   assert(rbi->sample != 0);
   assert(format < AAX_FORMAT_MAX);
   assert(rbi->parent == rb);

   rbd = rbi->sample;
   rbd->mixer_fmt = mixer;
   if (rbd->track == NULL)
   {
      rbd->format = format;
      rbd->codec = _aaxRingBufferCodecs[rbd->format];
      rbd->bytes_sample = _aaxRingBufferFormat[rbd->format].bits/8;
   }
#ifndef NDEBUG
   else printf("%s: Can't set value when rbd->track != NULL\n", __func__);
#endif

   return rv;
}

int
_aaxRingBufferDataMixWaveform(_aaxRingBuffer *rb, float *scratch, enum aaxWaveformType type, float f, float ratio, float phase, unsigned char modulate, unsigned char limiter)
{
   unsigned int tracks;
   unsigned char bps;
   size_t no_samples;
   int rv = AAX_FALSE;
   void *data;

   bps = rb->get_parami(rb, RB_BYTES_SAMPLE);
   tracks = rb->get_parami(rb, RB_NO_TRACKS);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);

   f = rb->get_paramf(rb, RB_FREQUENCY)/f;

   data = _aaxRingBufferGetTracksPtr(rb, RB_WRITE);
   switch (type)
   {
   case AAX_SINE_WAVE:
      _bufferMixWaveform(data, scratch, _SINE_WAVE, f, bps, no_samples,
                         tracks, ratio, phase, modulate, limiter);
      rv = AAX_TRUE;
      break;
   case AAX_SQUARE_WAVE:
      _bufferMixWaveform(data, scratch, _SQUARE_WAVE, f, bps, no_samples,
                         tracks, ratio, phase, modulate, limiter);
      rv = AAX_TRUE;
      break;
   case AAX_TRIANGLE_WAVE:
      _bufferMixWaveform(data, scratch, _TRIANGLE_WAVE, f, bps, no_samples,
                         tracks, ratio, phase, modulate, limiter);
      rv = AAX_TRUE;
      break;
   case AAX_SAWTOOTH_WAVE:
      _bufferMixWaveform(data, scratch, _SAWTOOTH_WAVE, f, bps, no_samples,
                         tracks, ratio, phase, modulate, limiter);
      rv = AAX_TRUE;
      break;
   case AAX_IMPULSE_WAVE:
      _bufferMixWaveform(data, scratch, _IMPULSE_WAVE, f, bps, no_samples,
                         tracks, ratio, phase, modulate, limiter);
      rv = AAX_TRUE;
      break;
   case AAX_CONSTANT_VALUE:
       _bufferMixWaveform(data, scratch, _CONSTANT_VALUE, f, bps, no_samples,
                         tracks, ratio, phase, modulate, limiter);
      rv = AAX_TRUE;
   default:
      break;
   }
   _aaxRingBufferReleaseTracksPtr(rb);

   return rv;
}

int
_aaxRingBufferDataMixNoise(_aaxRingBuffer *rb, float *scratch, enum aaxWaveformType type, float fs, float rate, float ratio, unsigned int seed, char skip, unsigned char modulate, unsigned char limiter)
{
   unsigned int tracks;
   unsigned char bps;
   size_t no_samples;
   int rv = AAX_FALSE;
   void *data;
   float pitch;

   pitch = _MINMAX(rate, 0.01f, 1.0f);

   bps = rb->get_parami(rb, RB_BYTES_SAMPLE);
   tracks = rb->get_parami(rb, RB_NO_TRACKS);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);

   data = rb->get_tracks_ptr(rb, RB_WRITE);
   switch (type)
   {
   case AAX_WHITE_NOISE:
      _bufferMixWhiteNoise(data, scratch, no_samples, bps, tracks, pitch,
                           ratio, fs, seed, skip, modulate, limiter);
      rv = AAX_TRUE;
      break;
   case AAX_PINK_NOISE:
      _bufferMixPinkNoise(data, scratch, no_samples, bps, tracks, pitch,
                          ratio, fs, seed, skip, modulate, limiter);
      rv = AAX_TRUE;
      break;
   case AAX_BROWNIAN_NOISE:
      _bufferMixBrownianNoise(data, scratch, no_samples, bps, tracks, pitch,
                              ratio, fs, seed, skip, modulate, limiter);
      rv = AAX_TRUE;
      break;
   default:
      break;
   }
   rb->release_tracks_ptr(rb);

   return rv;
}

int
_aaxRingBufferDataMultiply(_aaxRingBuffer *rb, size_t offs, size_t no_samples, float ratio_orig)
{
   if (fabsf(ratio_orig-1.0f) > LEVEL_96DB)
   {
      _aaxRingBufferData *rbi = rb->handle;
      _aaxRingBufferSample *rbd = rbi->sample;
      size_t t, tracks;
      unsigned char bps;
      MIX_T *data;

      bps = rb->get_parami(rb, RB_BYTES_SAMPLE);
      tracks = rb->get_parami(rb, RB_NO_TRACKS);
      if (!no_samples)
      {
         no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
         offs = 0;
      }

      for (t=0; t<tracks; t++)
      {
         data = rbd->track[t];
         rbd->multiply(data+offs, data+offs, bps, no_samples, ratio_orig);
      }
   }
   return AAX_TRUE;
}

int
_aaxRingBufferDataMixData(_aaxRingBuffer *drb, _aaxRingBuffer *srb, _aax2dProps *fp2d, unsigned char tracks)
{
   _aaxRingBufferData *srbi, *drbi;
   _aaxRingBufferSample *drbd;
   _aaxLFOData *lfo;
   unsigned char track;
   size_t dno_samples;
   float g = 1.0f;

   dno_samples =  drb->get_parami(drb, RB_NO_SAMPLES);
   if (tracks == AAX_TRACK_ALL) {
      tracks =  drb->get_parami(drb, RB_NO_TRACKS);
   }

   srbi = srb->handle;
   lfo =  fp2d ? _FILTER_GET_DATA(fp2d, DYNAMIC_GAIN_FILTER) : NULL;
   if (lfo && lfo->envelope)
   {
       g = 0.0f;
       for (track=0; track<tracks; track++)
       {
           MIX_T *sptr = srbi->sample->track[track];
           float gain =  lfo->get(lfo, NULL, sptr, track, dno_samples);

           if (lfo->inv) gain = 1.0f/g;
           g += gain;
       }
       g /= tracks;
   }

   drbi = drb->handle;
   drbd = drbi->sample;
   for (track=0; track<tracks; track++)
   {
      void *sptr = srbi->sample->track[track];
      void *dptr = drbd->track[track];
      float gstep = 0.0f;

      drbd->add(dptr, sptr, dno_samples, g, gstep);

      if (fp2d && fp2d->final.k < 0.9f)
      {
         float *hist = fp2d->final.freqfilter_history[track];
         MIX_PTR_T d = dptr;

#if RB_FLOAT_DATA
         _batch_movingaverage_float(d, d, dno_samples, hist+0, fp2d->final.k);
         _batch_movingaverage_float(d, d, dno_samples, hist+1, fp2d->final.k);
#else
         _batch_movingaverage(d, d, dno_samples, hist+0, fp2d->final.k);
         _batch_movingaverage(d, d, dno_samples, hist+1, fp2d->final.k);
#endif
      }
   }
   return AAX_TRUE;
}

int
_aaxRingBufferDataClear(_aaxRingBuffer *rb)
{
   _aaxRingBufferData *rbi = rb->handle;
   return _aaxRingBufferClear(rbi, AAX_FALSE);
}

void
_aaxRingBufferCopyDelayEffectsData(_aaxRingBuffer *drb, const _aaxRingBuffer *srb)
{
   _aaxRingBufferData *drbi, *srbi;
   _aaxRingBufferSample *srbd, *drbd;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(drb);
   assert(srb);

   drbi = drb->handle;
   srbi = srb->handle;
   assert(srbi);
   assert(drbi);
   assert(srbi->parent == srb);
   assert(drbi->parent == drb);

   srbd = srbi->sample;
   drbd = drbi->sample;

   if (srbd->bytes_sample == drbd->bytes_sample)
   {
      size_t t, tracks, ds, bps;

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
         MIX_T *sptr = srbd->track[t];
         MIX_T *dptr = drbd->track[t];
         _aax_memcpy(dptr-ds, sptr-ds, ds*bps);
      }
   }
}

void
_aaxRingBufferDataLimiter(_aaxRingBuffer *rb, enum _aaxLimiterType type)
{
   static const float _val[RB_LIMITER_MAX][2] =
   {
      { 0.5f, 0.0f },		// Electronic
      { 0.9f, 0.0f }, 		// Digital
      { 0.2f, 0.9f },		// Valve
      { 0.0f, 0.0f }		// Comress
   };

   _aaxRingBufferData *rbi = rb->handle;
   _aaxRingBufferSample *rbd = rbi->sample;
   unsigned int track, no_tracks = rbd->no_tracks;
   size_t no_samples = rbd->no_samples;
   size_t maxpeak, maxrms;
   float peak, rms;
   float dt, rms_rr, avg;
   MIX_T **tracks;

   dt = GMATH_E1 * rbd->duration_sec;
   rms_rr = _MINMAX(dt/0.3f, 0.0f, 1.0f);       // 300 ms average

   maxrms = maxpeak = 0;
   tracks = (MIX_T**)rbd->track;
   for (track=0; track<no_tracks; track++)
   {
      MIX_T *dptr = tracks[track];

      rms = 0;
      peak = no_samples;
      _batch_get_average_rms(dptr, no_samples, &rms, &peak);

      if (type == RB_COMPRESS) {
         _aaxRingBufferCompress(dptr, no_samples, _val[type][0], _val[type][1]);
      } else {
         _aaxRingBufferLimiter(dptr, no_samples, _val[type][0], _val[type][1]);
      }

      avg = rbi->average[track];
      avg = (rms_rr*avg + (1.0f-rms_rr)*rms);
      rbi->average[track] = avg;
      rbi->peak[track] = peak;

      if (maxrms < rms) maxrms = rms;
      if (maxpeak < peak) maxpeak = peak;
   }

   rbi->average[_AAX_MAX_SPEAKERS] = maxrms;
   rbi->peak[_AAX_MAX_SPEAKERS] = maxpeak;
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
  { 16, AAX_PCM16S },	/* IMA4-ADPCM gets converted to 16-bit */
  { 32, AAX_PCM24S }	/* 24-bit packed gets converted to 24-bit */
};

_aaxRingBufferMixStereoFn _aaxRingBufferMixMulti16;
_aaxRingBufferMixMonoFn _aaxRingBufferMixMono16;

static int
_aaxRingBufferClear(_aaxRingBufferData *rbi, char dde)
{
   _aaxRingBufferSample *rbd;
   size_t dde_bytes;
   unsigned int i;

   assert(rbi->parent == (char*)rbi-sizeof(_aaxRingBuffer));

   rbd = rbi->sample;
   dde_bytes = dde ? rbd->dde_samples*rbd->bytes_sample : 0;
   for (i=0; i<rbd->no_tracks; i++) {
      memset((void *)((char*)rbd->track[i]-dde_bytes), 0,
             rbd->track_len_bytes+dde_bytes);
   }

   return AAX_TRUE;
}

static void
_aaxRingBufferInitFunctions(_aaxRingBuffer *rb)
{
   rb->init = _aaxRingBufferInit;
   rb->reference = _aaxRingBufferReference;
   rb->duplicate = _aaxRingBufferDuplicate;
   rb->destroy = _aaxRingBufferDestroy;

   rb->set_state = _aaxRingBufferSetState;
   rb->get_state = _aaxRingBufferGetState;
   rb->set_format = _aaxRingBufferSetFormat;

   rb->set_paramd = _aaxRingBufferSetParamd;
   rb->set_paramf = _aaxRingBufferSetParamf;
   rb->set_parami = _aaxRingBufferSetParami;
   rb->get_paramf = _aaxRingBufferGetParamf;
   rb->get_parami = _aaxRingBufferGetParami;

   rb->get_tracks_ptr = _aaxRingBufferGetTracksPtr;
   rb->release_tracks_ptr = _aaxRingBufferReleaseTracksPtr;

   /* protected */
   rb->data_clear = _aaxRingBufferDataClear;
   rb->data_multiply = _aaxRingBufferDataMultiply;
   rb->data_mix_waveform = _aaxRingBufferDataMixWaveform;
   rb->data_mix_noise = _aaxRingBufferDataMixNoise;
   rb->data_mix = _aaxRingBufferDataMixData;
   rb->limit = _aaxRingBufferDataLimiter;

   /* private */
   rb->mix2d = _aaxRingBufferMixMulti16;
   rb->mix3d = _aaxRingBufferMixMono16;
   rb->get_scratch = _aaxRingBufferGetScratchBufferPtr;
   rb->copy_effectsdata = _aaxRingBufferCopyDelayEffectsData;
}

// size is the number of samples for every track
size_t
_aaxRingBufferCreateHistoryBuffer(_aaxRingBufferHistoryData **data, size_t size, int tracks)
{
   char *ptr, *p;
   size_t offs;
   int i;

   assert(data);
   assert(*data == NULL);

   size *= sizeof(MIX_T);
   offs = sizeof(_aaxRingBufferHistoryData);
   size += offs;
   if (size & MEMMASK)
   {
      size |= MEMMASK;
      size++;
   }

   ptr = _aax_calloc(&p, offs, tracks, size);
   if (ptr)
   {
      _aaxRingBufferHistoryData *history = (_aaxRingBufferHistoryData*)ptr;
      *data = history;
      history->ptr = p;
      for (i=0; i<tracks; ++i)
      {
         history->history[i] = (MIX_T*)p;
         p += size;
      }
   }
   return size;
}
