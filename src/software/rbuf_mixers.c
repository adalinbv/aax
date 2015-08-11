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

#include <math.h>       /* rintf */
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
_aaxRingBufferProcessMixer(_aaxRingBuffer *drb, _aaxRingBuffer *srb, _aax2dProps *p2d, float pitch_norm, size_t *start, size_t *no_samples, unsigned char ctr, int32_t history[_AAX_MAX_SPEAKERS][CUBIC_SAMPS])
{
   _aaxRingBufferData *drbi, *srbi;
   _aaxRingBufferSample *srbd, *drbd;
   float dfreq, dadvance, dduration, drb_pos_sec, fact, dremain;
   float sfreq, sduration, srb_pos_sec, new_srb_pos_sec;
   size_t ddesamps = *start;
   MIX_T **track_ptr;
   char src_loops;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(no_samples);
   *no_samples = 0;

   assert(drb);
   assert(srb);
   drbi = drb->handle;
   srbi = srb->handle;

   srbd = srbi->sample;
   drbd = drbi->sample;
   track_ptr = (MIX_T**)drbd->scratch;

   assert(drbd->bytes_sample == 4);
   assert(srbd->no_tracks >= 1);
   assert(drbd->no_tracks >= 1);

   /* destination position and duration */
   drb_pos_sec = drb->get_paramf(drb, RB_OFFSET_SEC);
   dduration = drb->get_paramf(drb, RB_DURATION_SEC) - drb_pos_sec;
   if (dduration == 0)
   {
      _AAX_SYSLOG("remaining duration of the destination buffer = 0.0.");
      return NULL;
   }

   /* source position and duration        */
   /* get srb_pos_sec before fast forward */
   srb_pos_sec = srb->get_paramf(srb, RB_OFFSET_SEC);
   sduration = srb->get_paramf(srb, RB_DURATION_SEC);

   /* source fast forward */
   pitch_norm *= srbi->pitch_norm;
   srb->set_paramf(srb, RB_FORWARD_SEC, dduration*pitch_norm);
   if (pitch_norm < 0.01f) return NULL;

   /* source time offset */
   sfreq = srb->get_paramf(srb, RB_FREQUENCY);
   new_srb_pos_sec = srb_pos_sec + dduration*pitch_norm;
   src_loops = (srbi->looping && !srbi->streaming);

   /* destination time offset */
   dfreq = drb->get_paramf(drb, RB_FREQUENCY);
   dadvance = (sduration - srb_pos_sec)/pitch_norm;
   drb->set_paramf(drb, RB_FORWARD_SEC, dadvance);

   /* destination time remaining after fast forwarding */
   dremain = drb->get_paramf(drb, RB_DURATION_SEC);
   dremain -= drb->get_paramf(drb, RB_OFFSET_SEC);
   if (!srbi->streaming || dremain == 0.0f) {
      drb->set_state(drb, RB_REWINDED);
   }

   /* sample conversion factor */
   fact = _MAX((sfreq * pitch_norm)/dfreq, 0.001f);

   /*
    * Test if the remaining start delay is smaller than the duration of the
    * destination buffer (distance delay).
    */
   if (new_srb_pos_sec >= -dduration)
   {
      _aaxRingBufferDelayEffectData* delay_effect;
      _aaxRingBufferFreqFilterData* freq_filter;
      size_t dest_pos, dno_samples, dend;
      size_t src_pos, sstart, sno_samples;
      size_t rdesamps, cno_samples;
      unsigned int sno_tracks;
      unsigned char sbps;
      int dist_state;

      delay_effect = _EFFECT_GET_DATA(p2d, DELAY_EFFECT);  // phasing, etc.
      freq_filter = _FILTER_GET_DATA(p2d, FREQUENCY_FILTER);
      dist_state = _EFFECT_GET_STATE(p2d, DISTORTION_EFFECT);

      /* source */
      sstart = 0;
      src_pos = floorf(srb_pos_sec*sfreq);
      sbps = srb->get_parami(srb, RB_BYTES_SAMPLE);
      sno_tracks = srb->get_parami(srb, RB_NO_TRACKS);
      sno_samples = srb->get_parami(srb, RB_NO_SAMPLES);
      if (src_loops)
      {
         size_t loop_start = srb->get_parami(srb, RB_LOOPPOINT_START);
         if (src_pos >= loop_start) {
            sstart = loop_start;
         }
         sno_samples = srb->get_parami(srb, RB_LOOPPOINT_END);
      }

      /* delay effects buffer */
      if (srb_pos_sec >= 0) {
         dest_pos = rintf(drb_pos_sec * dfreq);
      } else {		/* distance delay ended */
         dest_pos = rintf((drb_pos_sec + srb_pos_sec)*dfreq);
      }

      rdesamps = 0;
      if (ddesamps || delay_effect)
      {
         float dde = DELAY_EFFECTS_TIME*dfreq;

         ddesamps = (size_t)floorf(dde);
         if (drb->get_parami(drb, RB_DDE_SAMPLES) < ddesamps) {
            ddesamps = drb->get_parami(drb, RB_DDE_SAMPLES);
         }
         rdesamps = (size_t)floorf(dde*fact);
      }

      /* number of samples */
      dend = drb->get_parami(drb, RB_NO_SAMPLES);
      dno_samples = dend - dest_pos;

      cno_samples = rintf(dno_samples*fact);
      if (!src_loops && (cno_samples > (sno_samples-src_pos)))
      {
         size_t new_dno_samples;

         cno_samples = sno_samples-src_pos;
         new_dno_samples = rintf(sfreq*(sduration-srb_pos_sec)/fact);
         if (new_dno_samples < dno_samples)
         {
            dno_samples = new_dno_samples;
            cno_samples = rintf(dno_samples*fact);
         }
      }

      if (delay_effect) {
         cno_samples += CUBIC_SAMPS;
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
      if (track_ptr && dno_samples)
      {
         char eff = (freq_filter || delay_effect || dist_state) ? 1 : 0;
         MIX_T *scratch0 = track_ptr[SCRATCH_BUFFER0];
         MIX_T *scratch1 = track_ptr[SCRATCH_BUFFER1];
         void *env, *distortion_effect = NULL;
         unsigned int track;
         float smu;

         env = _FILTER_GET_DATA(p2d, TIMED_GAIN_FILTER);
         if (dist_state) {
            distortion_effect = &p2d->effect[DISTORTION_EFFECT];
         }

         smu = (srb_pos_sec*sfreq) - (float)src_pos;
         for (track=0; track<sno_tracks; track++)
         {
            MIX_T *sptr = (MIX_T*)srbd->track[track];
            MIX_T *dst, *dptr = track_ptr[track];

            /* short-cut for automatic file streaming with registered sensors */
            if (srbd->mixer_fmt) {
                scratch0 = sptr+src_pos; // -1;
            }
            else
            {
               size_t samples = cno_samples+CUBIC_SAMPS;
               size_t send = sno_samples;
               MIX_T *ptr = scratch0;

               if (srbi->streaming) {
                  send += CUBIC_SAMPS;
               }

               DBG_MEMCLR(1, scratch0, dend, sizeof(int32_t));
               srbi->codec((int32_t*)ptr, sptr, srbd->codec,
                            src_pos, sstart, send, 0, samples,
                            sbps, src_loops);
#if RB_FLOAT_DATA
               // convert from int32_t to float32
               _batch_cvtps24_24(ptr, ptr, samples);
               DBG_TESTNAN(ptr, samples);
#endif
            }

            /* update the history */
            if (!delay_effect && history)
            {
               size_t size = CUBIC_SAMPS*sizeof(MIX_T);
               MIX_T *ptr = scratch0-CUBIC_SAMPS;

               _aax_memcpy(ptr, history[track], size);
               _aax_memcpy(history[track], ptr+cno_samples, size);
            }

            dst = eff ? scratch1 : dptr;
            DBG_MEMCLR(1, dst-ddesamps, ddesamps+dend, sizeof(MIX_T));

            drbd->resample(dst-ddesamps, scratch0-rdesamps,
                           dest_pos, dest_pos+dno_samples+ddesamps, smu, fact);
#if RB_FLOAT_DATA
            DBG_TESTNAN(dst-ddesamps+dest_pos, dno_samples+ddesamps);
#endif

            if (eff)
            {
#if 0
memcpy(dptr+dest_pos, dst+dest_pos, dno_samples*sizeof(MIX_T));
#else
               DBG_MEMCLR(1, dptr-ddesamps, ddesamps+dend, sizeof(MIX_T));
               srbi->effects(srbi->sample, dptr, dst, scratch0,
                             dest_pos, dend, dno_samples, ddesamps, track, ctr,
                             freq_filter, delay_effect, distortion_effect, env);
#endif
#if RB_FLOAT_DATA
               DBG_TESTNAN(dptr-ddesamps+dest_pos, dno_samples+ddesamps);
#endif
            }
         }
      }

      *start = dest_pos;
      *no_samples = dno_samples;
   }

   return (CONST_MIX_PTRPTR_T)track_ptr;
}

