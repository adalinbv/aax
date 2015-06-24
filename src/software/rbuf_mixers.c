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

#include <stdio.h>

#include <math.h>       /* floorf */
#include <assert.h>

#include <api.h>

#include <base/types.h>
#include <base/geometry.h>
#include <base/logging.h>

#include <dsp/filters.h>
#include <dsp/effects.h>
#include <ringbuffer.h>
#include <arch.h>

#include "audio.h"
#include "rbuf_int.h"
#include "cpu/arch2d_simd.h"


/**
 * returns a buffer containing pointers to the playback position, but reserves
 * room prior to the playback position for delay effects (a maximum of
 * DELAY_EFFECTS_TIME in samples for the mixer frequency)
 */

CONST_MIX_PTRPTR_T
_aaxRingBufferProcessMixer(_aaxRingBufferData *drbi, _aaxRingBufferData *srbi, _aax2dProps *p2d, float pitch_norm, size_t *start, size_t *no_samples, unsigned char ctr, unsigned int streaming)
{
   _aaxRingBufferSample *srbd, *drbd;
   float dfreq, dduration, drb_pos_sec, new_drb_pos_sec, fact;
   float sfreq, sduration, srb_pos_sec, new_srb_pos_sec, eps;
   size_t ddesamps = *start;
   MIX_T **track_ptr;
   char src_loops;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(no_samples);
   *no_samples = 0;

   srbd = srbi->sample;
   drbd = drbi->sample;
   track_ptr = (MIX_T**)drbd->scratch;

   assert(drbd->bytes_sample == 4);
   assert(srbd->no_tracks >= 1);
   assert(drbd->no_tracks >= 1);

   if (pitch_norm < 0.01f)
   {
      srbi->curr_pos_sec += drbd->duration_sec;
      if (srbi->curr_pos_sec > srbd->duration_sec)
      {
         srbi->curr_pos_sec = srbd->duration_sec;
         srbi->playing = 0;
         srbi->stopped = 1;
      }
      return NULL;
   }

   srb_pos_sec = srbi->curr_pos_sec;
   src_loops = (srbi->looping && !srbi->streaming);
#ifndef NDEBUG
   /*
    * Note: This may happen for a registered sensor but it will work in
    *       non debugging mode.
    * TODO: Fix this behaviour
    */
   if ((srb_pos_sec > srbd->duration_sec) && !src_loops)
   {
      _AAX_SYSLOG("Sound should have stopped playing by now.");
      return NULL;
   }
#endif

   /* source */
   sfreq = srbd->frequency_hz;
   sduration = srbd->duration_sec;
   if (srb_pos_sec >= sduration)
   {
      // This can (only) happen if there's just one buffer in the stream-queue
      srbi->curr_pos_sec = srbd->duration_sec;
      srbi->playing = 0;
      srbi->stopped = 1;
      return NULL;
   }

   /* destination */
   dfreq = drbd->frequency_hz;
   drb_pos_sec = drbi->curr_pos_sec;
   new_drb_pos_sec = drb_pos_sec;
   dduration = drbd->duration_sec - drb_pos_sec;
   if (dduration == 0)
   {
      _AAX_SYSLOG("remaining duration of the destination buffer = 0.0.");
      return NULL;
   }

   /* sample conversion factor */
   fact = (sfreq * pitch_norm*srbi->pitch_norm) / dfreq;
   if (fact < 0.01f) fact = 0.01f;
// else if (fact > 2.0f) fact = 2.0f;

   eps = 1.1f/sfreq;
   new_srb_pos_sec = srb_pos_sec + dduration*pitch_norm;
   if (new_srb_pos_sec >= (srbd->loop_end_sec-eps))
   {
      if (src_loops)
      {
         srbi->loop_no++;
         if (srbi->loop_max && (srbi->loop_no >= srbi->loop_max)) {
            srbi->looping = AAX_FALSE;
         }
         else
         {
            // new_srb_pos_sec = fmodf(new_srb_pos_sec, sduration);
            float loop_start_sec = srbd->loop_start_sec;
            float loop_length_sec = srbd->loop_end_sec - loop_start_sec;
            new_srb_pos_sec -= loop_start_sec;
            new_srb_pos_sec = fmodf(new_srb_pos_sec+eps, loop_length_sec);
            new_srb_pos_sec += loop_start_sec;
            new_drb_pos_sec = 0.0f;
         }
      }  
      else if (new_srb_pos_sec >= (srbd->duration_sec-eps))	/* streaming */
      {  
         float dt = (sduration - srb_pos_sec)/pitch_norm;

         srbi->playing = 0;
         srbi->stopped = 1;
         new_srb_pos_sec = sduration;

         new_drb_pos_sec = drbi->curr_pos_sec + dt;
         if (new_drb_pos_sec >= (drbd->duration_sec-(1.1f/dfreq)))
         {
            new_drb_pos_sec = 0.0f;
            drbi->playing = 0;
            drbi->stopped = 1;
         }
         else {
            dduration = dt;
         }
      }
   }
   else {
      new_drb_pos_sec += (sduration - srb_pos_sec)/pitch_norm;
   }

   /*
    * Test if the remaining start delay is smaller than the duration of the
    * destination buffer.
    */
   if (new_srb_pos_sec >= -dduration)
   {
      _aaxRingBufferDelayEffectData* delay_effect;
      _aaxRingBufferFreqFilterData* freq_filter;
      size_t dest_pos, dno_samples, dend;
      size_t sno_samples, sstart;
      unsigned int sno_tracks;
      unsigned char sbps;
      int dist_state;

      /* source */
      sstart = 0;
      sbps = srbd->bytes_sample;
      sno_tracks = srbd->no_tracks;
      sno_samples = srbd->no_samples;
      if (src_loops)
      {
         if (srb_pos_sec > srbd->loop_start_sec) {
            sstart = rintf(srbd->loop_start_sec*sfreq);
         }
         sno_samples = rintf(srbd->loop_end_sec*sfreq);
      }

      /* destination */
      dend = drbd->no_samples;
      dno_samples = rintf(dduration*dfreq);
      if (srb_pos_sec >= 0) {
         dest_pos = rintf(drb_pos_sec * dfreq);
      } else {					/* distance delay ended */
         dest_pos = rintf((drb_pos_sec + srb_pos_sec)*dfreq);
      }

      if (!(srbi->streaming && (sno_samples < dend)))
      {
         if (!ddesamps) {
            ddesamps = (size_t)ceilf(CUBIC_SAMPS/fact);
         }
      }

      delay_effect = _EFFECT_GET_DATA(p2d, DELAY_EFFECT);	// phasing, etc.
      freq_filter = _FILTER_GET_DATA(p2d, FREQUENCY_FILTER);
      dist_state = _EFFECT_GET_STATE(p2d, DISTORTION_EFFECT);
      if (delay_effect)
      {
         /*
          * can not use drbd->dde_samples since it's 10 times too big for the
          * final mixer to accomodate for reverb
          */
         // ddesamps = drbd->dde_samples;
         ddesamps = (size_t)ceilf(DELAY_EFFECTS_TIME*dfreq);
         if (drbd->dde_samples < ddesamps) ddesamps = drbd->dde_samples;
      }

#ifdef NDEBUG
#if 0
      if (srbd->mixer_fmt && (fact > 0.99f && fact < 1.01f) &&
          !freq_filter && !delay_effect && !dist_state)
      {
            track_ptr = (MIX_T**)srbd->track;
      }
      else
#endif
#endif

      if (track_ptr)
      {
         char eff = (freq_filter || delay_effect || dist_state) ? 1 : 0;
         MIX_T *scratch0 = track_ptr[SCRATCH_BUFFER0];
         MIX_T *scratch1 = track_ptr[SCRATCH_BUFFER1];
         void *env, *distortion_effect = NULL;
         size_t src_pos, offs, cno_samples, cdesamps = 0;
         unsigned int track;
         float smu;

         src_pos = srbi->curr_sample;
         smu = _MAX((srb_pos_sec*sfreq) - src_pos, 0.0f);

         cdesamps = (size_t)floorf(ddesamps*fact);
         cno_samples = (size_t)ceilf(dno_samples*fact);
         if (!src_loops && (cno_samples > (sno_samples-src_pos)))
         {
            size_t new_dno_samples;

            cno_samples = sno_samples-src_pos;
            new_dno_samples = rintf(cno_samples/fact);
            if (new_dno_samples < dno_samples) {
//             dno_samples = new_dno_samples;
            }
            if (!src_pos) {
               cdesamps = CUBIC_SAMPS;
            }
         }

         env = _FILTER_GET_DATA(p2d, TIMED_GAIN_FILTER);
         if (dist_state) {
             distortion_effect = &p2d->effect[DISTORTION_EFFECT];
         }

         offs = (fact < CUBIC_TRESHOLD) ? 1 : 0;
         if (dest_pos) {
            ddesamps = cdesamps = offs = 0;
         }

         for (track=0; track<sno_tracks; track++)
         {
            MIX_T *sptr = (MIX_T*)srbd->track[track];
            MIX_T *dst, *dptr = track_ptr[track];

            /* needed for automatic file streaming with registered sensors */
            if (!srbd->mixer_fmt)
            {
               if (!streaming)
               {
                  sptr -= CUBIC_SAMPS;
                  sno_samples += CUBIC_SAMPS;
               }

               DBG_MEMCLR(1,scratch0-ddesamps, ddesamps+dend, sizeof(MIX_T));
               srbi->codec((int32_t*)scratch0-cdesamps, sptr, srbd->codec, src_pos,
                                sstart, sno_samples, cdesamps, cno_samples,
                                sbps, src_loops);
#if RB_FLOAT_DATA
               // rbd->resample needs a few samples more for low fact values.
               _batch_cvtps24_24(scratch0-cdesamps-offs-CUBIC_SAMPS,
                                 scratch0-cdesamps-offs-CUBIC_SAMPS,
                                 cno_samples+cdesamps+2*CUBIC_SAMPS);
#endif
            } else {
               scratch0 = sptr+src_pos-1;
            }

#if RB_FLOAT_DATA
            DBG_TESTNAN(scratch0, cno_samples);
            DBG_TESTNAN(scratch0-cdesamps-offs, cno_samples+cdesamps);
#endif

            dst = eff ? scratch1 : dptr;
            DBG_MEMCLR(1, dst-ddesamps, ddesamps+dend, sizeof(MIX_T));
            drbd->resample(dst-ddesamps, scratch0-cdesamps-offs,
                           dest_pos, dest_pos+dno_samples+ddesamps, smu, fact);

#if RB_FLOAT_DATA
            DBG_TESTNAN(dst-ddesamps+dest_pos, dno_samples+ddesamps);
#endif

            if (eff)
            {
               DBG_MEMCLR(1, dptr-ddesamps, ddesamps+dend, sizeof(MIX_T));
               srbi->effects(srbi->sample, dptr, dst, scratch0,
                             dest_pos, dend, dno_samples, ddesamps, track, ctr,
                             freq_filter, delay_effect, distortion_effect, env);
#if RB_FLOAT_DATA
               DBG_TESTNAN(dptr-ddesamps+dest_pos, dno_samples+ddesamps);
#endif
            }
         }
         drbi->curr_pos_sec += dno_samples/dfreq;
      }

      if (new_srb_pos_sec >= (sduration-eps))
      {
         new_srb_pos_sec = sduration;
         srbi->playing = 0;
         srbi->stopped = 1;
      }
      srbi->curr_pos_sec = new_srb_pos_sec;
      srbi->curr_sample = floorf(new_srb_pos_sec * sfreq);

      *start = dest_pos;
      *no_samples = dno_samples;
   }

   return (CONST_MIX_PTRPTR_T)track_ptr;
}

