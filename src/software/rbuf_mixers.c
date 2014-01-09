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

#include <ringbuffer.h>
#include <arch.h>

#include "audio.h"
#include "rbuf_int.h"
#include "cpu/arch2d_simd.h"


/*
 * Note: The NDEBUG tests in this file may fail for registered sensors but will
 *       work in release mode
 */
#define NDEBUG 1


/**
 * returns a buffer containing pointers to the playback position, but reserves
 * room prior to the playback position for delay effects (a maximum of
 * DELAY_EFFECTS_TIME in samples for the mixer frequency)
 */

CONST_MIX_PTRPTR_T
_aaxRingBufferProcessMixer(_aaxRingBufferData *drbi, _aaxRingBufferData *srbi, _aax2dProps *p2d, float pitch_norm, unsigned int *start, unsigned int *no_samples, unsigned char ctr, unsigned int nbuf)
{
   _aaxRingBufferSample *srbd, *drbd;
   float sfreq, sduration, srb_pos_sec, new_srb_pos_sec;
   float dfreq, dduration, drb_pos_sec, fact, eps;
   unsigned int ddesamps = *start;
   MIX_T **track_ptr;
   char src_loops;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

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
      srbi->curr_pos_sec = srbd->duration_sec;
      srbi->playing = 0;
      srbi->stopped = 1;
      return NULL;
   }

   /* destination */
   dfreq = drbd->frequency_hz;
   drb_pos_sec = drbi->curr_pos_sec;
   dduration = drbd->duration_sec - drb_pos_sec;
   if (dduration < (1.1f/dfreq))
   {
      _AAX_SYSLOG("remaining duration of the destination buffer = 0.0.");
      return NULL;
   }

   /* conversion */
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
            float loop_length_sec = srbd->loop_end_sec - srbd->loop_start_sec;
            new_srb_pos_sec -= srbd->loop_start_sec;
            new_srb_pos_sec = fmodf(new_srb_pos_sec, loop_length_sec);
            new_srb_pos_sec += srbd->loop_start_sec;
            drbi->curr_pos_sec = 0.0f;
         }
      }  
      else if (new_srb_pos_sec >= (srbd->duration_sec-eps))	/* streaming */
      {  
         float dt = (sduration - srb_pos_sec)/pitch_norm;

#ifndef NDEBUG
         /*
          * Note: This may happen for a registered sensor but it will work in
          *       non debugging mode.
          * TODO: Fix this behaviour
          */
         if (dt < (1.1f/dfreq) )
         {
            _AAX_SYSLOG("Remaining duration of the buffer is too small.");
            return NULL;
         }
#endif
         srbi->playing = 0;
         srbi->stopped = 1;
         new_srb_pos_sec = sduration;

         drbi->curr_pos_sec += dt;
         if (drbi->curr_pos_sec >= (drbd->duration_sec-(1.1f/dfreq)))
         {
            drbi->curr_pos_sec = 0.0f;
            drbi->playing = 0;
            drbi->stopped = 1;
         }
         else dduration = dt;
      }
   }

   /*
    * Test if the remaining start delay is smaller than the duration of the
    * destination buffer.
    */
   if (new_srb_pos_sec >= -dduration)
   {
      _aaxRingBufferDelayEffectData* delay_effect;
      _aaxRingBufferFreqFilterData* freq_filter;
      unsigned int sno_samples, sstart, sno_tracks;
      unsigned int dest_pos, dno_samples, dend;
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
      } else {						/* distance delay */
         dest_pos = rintf((drb_pos_sec + srb_pos_sec)*dfreq);
      }

      *start = dest_pos;
      *no_samples = dno_samples; // dend - dest_pos;
      if (!(srbi->streaming && (sno_samples < dend)))
      {
#if 0
         if (dest_pos)
         {
            // TODO: Why??
            *no_samples += 1; //  = dno_samples - (dest_pos-1);
            *start -= 1; // = (dest_pos-1);
         }
#endif
         if (!ddesamps) {
            ddesamps = (unsigned int)ceilf(CUBIC_SAMPS/fact);
         }
      }

      delay_effect = _EFFECT_GET_DATA(p2d, DELAY_EFFECT);	// phasing, etc.
      freq_filter = _FILTER_GET_DATA(p2d, FREQUENCY_FILTER);
      dist_state = _EFFECT_GET_STATE(p2d, DISTORTION_EFFECT);
      if (delay_effect)
      {
         /*
          * can not use drbd->dde_samples since it's 10 times as big for the
          * fial mixer to accomodate for reverb
          */
         // ddesamps = drbd->dde_samples;
         ddesamps = (unsigned int)ceilf(DELAY_EFFECTS_TIME*dfreq);
      }

      if (track_ptr)
      {
         unsigned int cdesamps, cno_samples;
         unsigned int track, smin, src_pos;
         float ftmp, smu;

         ftmp = srb_pos_sec * sfreq;
         smin = (unsigned int)floorf(ftmp);
         smu = ftmp - smin;

         src_pos = srbi->curr_sample;
         cdesamps = (unsigned int)floorf(ddesamps*fact);
         cno_samples = (unsigned int)ceilf(dno_samples*fact);
         if ((cno_samples > sno_samples) && !src_loops)
         {
            cno_samples = sno_samples;
            cdesamps = CUBIC_SAMPS;
         }

         if (!freq_filter && !delay_effect && !dist_state)
         {
            MIX_T *scratch0 = track_ptr[SCRATCH_BUFFER0];
            int offs = (fact < CUBIC_TRESHOLD) ? 1 : 0;

            for (track=0; track<sno_tracks; track++)
            {
               char *sptr = (char*)srbd->track[track];
               MIX_T *dptr = track_ptr[track];

               /* needed for automatic file streaming with registered sensors */
               if (!nbuf)
               {
                  sptr -= CUBIC_SAMPS*sbps;
                  sno_samples += CUBIC_SAMPS;
               }

               DBG_MEMCLR(1, scratch0-ddesamps, ddesamps+dend, sizeof(int32_t));
               srbi->codec((int32_t*)scratch0, sptr, srbd->codec, src_pos,
                                sstart, sno_samples, cdesamps, cno_samples,
                                sbps, src_loops);

#if RB_FLOAT_DATA
               if (drbd->mixer) {
                  _batch_cvtps24_24(scratch0, scratch0, cno_samples);
               }
#endif

               DBG_MEMCLR(1, dptr-ddesamps, ddesamps+dend, sizeof(MIX_T));
               drbd->resample(dptr-ddesamps, scratch0-cdesamps-offs,
                                   dest_pos, dest_pos+dno_samples+ddesamps,
                                   smu, fact);
            }
         }
         else
         {
            MIX_T *scratch0 = track_ptr[SCRATCH_BUFFER0];
            MIX_T *scratch1 = track_ptr[SCRATCH_BUFFER1];
            void* distortion_effect = NULL;
            int offs = (fact < CUBIC_TRESHOLD) ? 1 : 0;
            
            if (dist_state) {
                distortion_effect = &p2d->effect[DISTORTION_EFFECT];
            }

            for (track=0; track<sno_tracks; track++)
            {
               char *sptr = (char*)srbd->track[track];
               MIX_T *dptr = track_ptr[track];

               /* needed for automatic file streaming with registered sensors */
               if (!nbuf)
               {
                  sptr -= CUBIC_SAMPS*sbps;
                  sno_samples += CUBIC_SAMPS;
               }

               DBG_MEMCLR(1, scratch0-ddesamps, ddesamps+dend, sizeof(int32_t));
               srbi->codec((int32_t*)scratch0, sptr, srbd->codec, src_pos,
                                sstart, sno_samples, cdesamps, cno_samples,
                                sbps, src_loops);

#if RB_FLOAT_DATA
               if (drbd->mixer) {
                  _batch_cvtps24_24(scratch0, scratch0, cno_samples);
               }
#endif

               DBG_MEMCLR(1, scratch1-ddesamps, ddesamps+dend, sizeof(MIX_T));
               drbd->resample(scratch1-ddesamps, scratch0-cdesamps-offs,
                                   dest_pos, dest_pos+dno_samples+ddesamps,
                                   smu, fact);

               DBG_MEMCLR(1, dptr-ddesamps, ddesamps+dend, sizeof(MIX_T));
               srbi->effects(srbi->sample, dptr, scratch1, scratch0,
                             dest_pos, dend, dno_samples, ddesamps, track, ctr,
                             freq_filter, delay_effect, distortion_effect);
            }
         }
      }

      srbi->curr_sample = (unsigned int)floorf(new_srb_pos_sec * sfreq);
      srbi->curr_pos_sec = new_srb_pos_sec;
   }

   return (CONST_MIX_PTRPTR_T)track_ptr;
}

