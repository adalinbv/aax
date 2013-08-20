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

#include <stdio.h>

#include <math.h>       /* floorf */
#include <assert.h>

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>
#include <base/types.h>
#include <base/geometry.h>
#include <base/logging.h>

#include "arch_simd.h"
#include "audio.h"


/*
 * Note: The NDEBUG tests in this file may fail for registered sensors but will
 *       work in release mode
 */
#define NDEBUG 1

#define CUBIC_TRESHOLD		0.25f

_aaxDriverCompress _aaxProcessCompression = bufCompressElectronic;


/**
 * returns a buffer containing pointers to the playback position, but reserves
 * room prior to the playback position for delay effects (a maximum of
 * DELAY_EFFECTS_TIME in samples for the mixer frequency)
 */
int32_t **
_aaxProcessMixer(_oalRingBuffer *drb, _oalRingBuffer *srb, _oalRingBuffer2dProps *p2d, float pitch_norm, unsigned int *start, unsigned int *no_samples, unsigned char ctr, unsigned int nbuf)
{
   _oalRingBufferSample *srbd, *drbd;
   float sfreq, sduration, srb_pos_sec, new_srb_pos_sec;
   float dfreq, dduration, drb_pos_sec, fact, eps;
   unsigned int ddesamps = *start;
   int32_t **track_ptr;
   char src_loops;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(no_samples);
   *no_samples = 0;

   srbd = srb->sample;
   drbd = drb->sample;
   track_ptr = (int32_t**)drbd->scratch;

   assert(drbd->bytes_sample == 4);
   assert(srbd->no_tracks >= 1);
   assert(drbd->no_tracks >= 1);

   if (pitch_norm < 0.01f)
   {
      srb->curr_pos_sec += drbd->duration_sec;
      if (srb->curr_pos_sec > srbd->duration_sec)
      {
         srb->curr_pos_sec = srbd->duration_sec;
         srb->playing = 0;
         srb->stopped = 1;
      }
      return NULL;
   }

   srb_pos_sec = srb->curr_pos_sec;
   src_loops = (srb->looping && !srb->streaming);
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
      srb->curr_pos_sec = srbd->duration_sec;
      srb->playing = 0;
      srb->stopped = 1;
      return NULL;
   }

   /* destination */
   dfreq = drbd->frequency_hz;
   drb_pos_sec = drb->curr_pos_sec;
   dduration = drbd->duration_sec - drb_pos_sec;
   if (dduration < (1.1f/dfreq))
   {
      _AAX_SYSLOG("remaining duration of the destination buffer = 0.0.");
      return NULL;
   }

   /* conversion */
   fact = (sfreq * pitch_norm*srb->pitch_norm) / dfreq;
   if (fact < 0.01f) fact = 0.01f;
// else if (fact > 2.0f) fact = 2.0f;

   eps = 1.1f/sfreq;
   new_srb_pos_sec = srb_pos_sec + dduration*pitch_norm;
   if (new_srb_pos_sec >= (srbd->loop_end_sec-eps))
   {
      if (src_loops)
      {
         srb->loop_no++;
         if (srb->loop_max && (srb->loop_no >= srb->loop_max)) {
            srb->looping = AAX_FALSE;
         }
         else
         {
            // new_srb_pos_sec = fmodf(new_srb_pos_sec, sduration);
            float loop_length_sec = srbd->loop_end_sec - srbd->loop_start_sec;
            new_srb_pos_sec -= srbd->loop_start_sec;
            new_srb_pos_sec = fmodf(new_srb_pos_sec, loop_length_sec);
            new_srb_pos_sec += srbd->loop_start_sec;
            drb->curr_pos_sec = 0.0f;
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
         srb->playing = 0;
         srb->stopped = 1;
         new_srb_pos_sec = sduration;

         drb->curr_pos_sec += dt;
         if (drb->curr_pos_sec >= (drbd->duration_sec-(1.1f/dfreq)))
         {
            drb->curr_pos_sec = 0.0f;
            drb->playing = 0;
            drb->stopped = 1;
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
      _oalRingBufferDelayEffectData* delay_effect;
      _oalRingBufferFreqFilterInfo* freq_filter;
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
      if (!(srb->streaming && (sno_samples < dend)))
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

         src_pos = srb->curr_sample;
         cdesamps = (unsigned int)floorf(ddesamps*fact);
         cno_samples = (unsigned int)ceilf(dno_samples*fact);
         if ((cno_samples > sno_samples) && !src_loops)
         {
            cno_samples = sno_samples;
            cdesamps = CUBIC_SAMPS;
         }

         if (!freq_filter && !delay_effect && !dist_state)
         {
            int32_t *scratch0 = track_ptr[SCRATCH_BUFFER0];
            int offs = (fact < CUBIC_TRESHOLD) ? 1 : 0;

            for (track=0; track<sno_tracks; track++)
            {
               char *sptr = (char*)srbd->track[track];
               int32_t *dptr = track_ptr[track];

               /* needed for automatic file streaming with registered sensors */
               if (!nbuf)
               {
                  sptr -= CUBIC_SAMPS*sbps;
                  sno_samples += CUBIC_SAMPS;
               }

               DBG_MEMCLR(1, scratch0-ddesamps, ddesamps+dend, sizeof(int32_t));
               _aaxProcessCodec(scratch0, sptr, srbd->codec, src_pos,
                                sstart, sno_samples, cdesamps, cno_samples,
                                sbps, src_loops);

               DBG_MEMCLR(1, dptr-ddesamps, ddesamps+dend, sizeof(int32_t));
               _aaxProcessResample(dptr-ddesamps, scratch0-cdesamps-offs,
                                   dest_pos, dest_pos+dno_samples+ddesamps,
                                   smu, fact);
            }
         }
         else
         {
            int32_t *scratch0 = track_ptr[SCRATCH_BUFFER0];
            int32_t *scratch1 = track_ptr[SCRATCH_BUFFER1];
            void* distortion_effect = NULL;
            int offs = (fact < CUBIC_TRESHOLD) ? 1 : 0;
            
            if (dist_state) {
                distortion_effect = &p2d->effect[DISTORTION_EFFECT];
            }

            for (track=0; track<sno_tracks; track++)
            {
               char *sptr = (char*)srbd->track[track];
               int32_t *dptr = track_ptr[track];

               /* needed for automatic file streaming with registered sensors */
               if (!nbuf)
               {
                  sptr -= CUBIC_SAMPS*sbps;
                  sno_samples += CUBIC_SAMPS;
               }

               DBG_MEMCLR(1, scratch0-ddesamps, ddesamps+dend, sizeof(int32_t));
               _aaxProcessCodec(scratch0, sptr, srbd->codec, src_pos,
                                sstart, sno_samples, cdesamps, cno_samples,
                                sbps, src_loops);

               DBG_MEMCLR(1, scratch1-ddesamps, ddesamps+dend, sizeof(int32_t));
               _aaxProcessResample(scratch1-ddesamps, scratch0-cdesamps-offs,
                                   dest_pos, dest_pos+dno_samples+ddesamps,
                                   smu, fact);

               DBG_MEMCLR(1, dptr-ddesamps, ddesamps+dend, sizeof(int32_t));
               bufEffectsApply(dptr, scratch1, scratch0, dest_pos, dend,
                               dno_samples, ddesamps, track, ctr,
                               freq_filter, delay_effect, distortion_effect);
            }
         }
      }

      srb->curr_sample = (unsigned int)floorf(new_srb_pos_sec * sfreq);
      srb->curr_pos_sec = new_srb_pos_sec;
   }

   return track_ptr;
}

/* -------------------------------------------------------------------------- */

void
_aaxProcessResample(int32_ptr d, const int32_ptr s, unsigned int dmin, unsigned int dmax, float smu, float fact)
{
   _batch_resample_proc resamplefn;

   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      resamplefn = _aaxBufResampleCubic;
   }
   else if (fact < 1.0f) {
      resamplefn = _aaxBufResampleLinear;
   }
   else if (fact > 1.0f) {
      resamplefn = _aaxBufResampleSkip;
   } else {
      resamplefn = _aaxBufResampleNearest;
   }

   resamplefn(d, s, dmin, dmax, 0, smu, fact);
}

void
bufCompressElectronic(void *d, unsigned int *dmin, unsigned int *dmax)
{
   bufCompress(d, dmin, dmax, 0.5f, 0.0f);
}

void
bufCompressDigital(void *d, unsigned int *dmin, unsigned int *dmax)
{
   bufCompress(d, dmin, dmax, 0.9f, 0.0f);
}

void
bufCompressValve(void *d, unsigned int *dmin, unsigned int *dmax)
{
   bufCompress(d, dmin, dmax, 0.2f, 0.9f);
}

#define BITS		11
#define SHIFT		(31-BITS)
#define START		((1<<SHIFT)-1)
#define FACT		(float)(23-SHIFT)/(float)(1<<(31-SHIFT))
#if 1
void
bufCompress(void *d, unsigned int *dmin, unsigned int *dmax, float clip, float asym)
{
   static const float df = (float)(int32_t)0x7FFFFFFF;
   static const float rf = 1.0f/(65536.0f*12.0f);
   int32_t *ptr = (int32_t*)d;
   float osamp, imix, mix;
   unsigned int j, max;
   int32_t iasym;
   float peak;
   double sum;

   osamp = 0.0f;
   sum = peak = 0;
   mix = _MINMAX(clip, 0.0f, 1.0f);
   imix = (1.0f - mix);
   j = max = *dmax - *dmin;
   iasym = asym*16*(1<<SHIFT);
   do
   {
      float val, fact1, fact2, sdf, rise;
      int32_t asamp, samp;
      unsigned int pos;

      samp = *ptr;
      val = (float)samp*samp;	// RMS
      sum += val;
      if (val > peak) peak = val;

      asamp = (samp < 0) ? abs(samp-iasym) : abs(samp);
      pos = (asamp >> SHIFT);
      sdf = _MINMAX(asamp*df, 0.0f, 1.0f);

      rise = _MINMAX((osamp-samp)*rf, 0.3f, 303.3f);
      pos = (unsigned int)_MINMAX(pos+asym*rise, 1, ((1<<BITS)));
      osamp = samp;

      fact1 = (1.0f-sdf)*_compress_tbl[0][pos-1];
      fact1 += sdf*_compress_tbl[0][pos];

      fact2 = (1.0f-sdf)*_compress_tbl[1][pos-1];
      fact2 += sdf*_compress_tbl[1][pos];

      *ptr++ = (int32_t)((imix*fact1 + mix*fact2)*samp);
   }
   while (--j);
 
   *dmax = (unsigned int)sqrtf(peak);
   *dmin = (unsigned int)sqrt(sum/max);
}

#else
/* arctan */
void
bufCompress(void *d, unsigned int dmin, unsigned int dmax, float clip, float asym)
{
   static const float df = 1.0f/2147483648.0f;
   int32_t *ptr = (int32_t*)d;
   unsigned int j;
   float mix;

   mix = 256.0f; // * _MINMAX(clip, 0.0, 1.0);
   j = dmax-dmin;
   do
   {
      float samp = atan(*ptr*df*mix)*GMATH_1_PI_2;
      *ptr++ = samp*8388608;
   }
   while (--j);
}
#endif
