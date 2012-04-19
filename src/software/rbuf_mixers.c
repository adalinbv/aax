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


_aaxDriverCompress _aaxProcessCompression = bufCompressElectronic;


/**
 * returns a buffer containing pointers to the playback position, but reserves
 * room prior to the playback position for delay effects (a maximum of
 * DELAY_EFFECTS_TIME in samples for the mixer frequency)
 */
int32_t **
_aaxProcessMixer(_oalRingBuffer *dest, _oalRingBuffer *src, _oalRingBuffer2dProps *p2d, float pitch_norm, unsigned int *start, unsigned int *no_samples, unsigned char ctr)
{
   _oalRingBufferSample *rbd, *rbs;
   float sfreq, sduration, src_pos_sec, new_src_pos_sec;
   float dfreq, dduration, dest_pos_sec, fact, eps;
   unsigned int ddesamps = *start;
   int32_t **track_ptr;
   char src_loops;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   if (pitch_norm < 0.01f) {
      return NULL;
   }

   rbs = src->sample;
   rbd = dest->sample;
   track_ptr = (int32_t**)rbd->scratch;

   assert(rbd->bytes_sample == 4);
   assert(rbs->no_tracks >= 1);
   assert(rbd->no_tracks >= 1);

   src_pos_sec = src->curr_pos_sec;
   src_loops = (src->looping && !src->streaming);
#ifndef NDEBUG
   if ((src_pos_sec > rbs->duration_sec) && !src_loops)
   {
      _AAX_SYSLOG("Sound should have stopped playing by now.");
      return NULL;
   }
#endif

   /* source */
   sfreq = rbs->frequency_hz;
   sduration = rbs->duration_sec;

   /* destination */
   dfreq = rbd->frequency_hz;
   dest_pos_sec = dest->curr_pos_sec;
   dduration = rbd->duration_sec - dest_pos_sec;
   if (dduration < (1.1f/dfreq))
   {
      _AAX_SYSLOG("remaining duration of the destination buffer = 0.0.");
      return NULL;
   }

   /* conversion */
   fact = (sfreq * pitch_norm*src->pitch_norm) / dfreq;
   if (fact < 0.01f) fact = 0.01f;
// else if (fact > 2.0f) fact = 2.0f;

   eps = 1.1f/sfreq;
   new_src_pos_sec = src_pos_sec + dduration*pitch_norm;
   if (new_src_pos_sec >= (rbs->loop_end_sec-eps))
   {
      if (src_loops)
      {
         // new_src_pos_sec = fmodf(new_src_pos_sec, sduration);
         float loop_length_sec = rbs->loop_end_sec - rbs->loop_start_sec;
         new_src_pos_sec -= rbs->loop_start_sec;
         new_src_pos_sec = fmodf(new_src_pos_sec, loop_length_sec);
         new_src_pos_sec += rbs->loop_start_sec;
         dest->curr_pos_sec = 0.0f;
      }  
      else if (new_src_pos_sec >= (rbs->duration_sec-eps))	/* streaming */
      {  
         float dt = (sduration - src_pos_sec)/pitch_norm;

#ifndef NDEBUG
         if (dt < (1.1f/dfreq) )
         {
            _AAX_SYSLOG("Remaining duration of the buffer is too small.");
            return NULL;
         }
#endif
         src->playing = 0;
         src->stopped = 1;
         new_src_pos_sec = sduration;

         dest->curr_pos_sec += dt;
         if (dest->curr_pos_sec >= (rbd->duration_sec-(1.1f/dfreq)))
         {
            dest->curr_pos_sec = 0.0f;
            dest->playing = 0;
            dest->stopped = 1;
         }
         else dduration = dt;
      }
   }

   /*
    * Test if the remaining start delay is smaller than the duration of the
    * destination buffer.
    */
   if (new_src_pos_sec >= -dduration)
   {
      _oalRingBufferDelayEffectData* delay_effect;
      _oalRingBufferFreqFilterInfo* freq_filter;
      unsigned int sno_samples, sstart, sno_tracks;
      unsigned int dest_pos, dno_samples, dend;
      unsigned char sbps;
      int dist_state;

      /* source */
      sstart = 0;
      sbps = rbs->bytes_sample;
      sno_tracks = rbs->no_tracks;
      sno_samples = rbs->no_samples;
      if (src_loops)
      {
         if (src_pos_sec > rbs->loop_start_sec) {
            sstart = rintf(rbs->loop_start_sec*sfreq);
         }
         sno_samples = rintf(rbs->loop_end_sec*sfreq);
      }

      /* destination */
      dend = rbd->no_samples;
      dno_samples = rintf(dduration*dfreq);
      if (src_pos_sec >= 0) {
         dest_pos = rint(dest_pos_sec * dfreq);
      } else {						/* distance delay */
         dest_pos = rintf((dest_pos_sec + src_pos_sec)*dfreq);
      }

      *start = dest_pos;
      *no_samples = dno_samples; // dend - dest_pos;
      if (!(src->streaming && (sno_samples < dend)))
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
            ddesamps = ceilf(CUBIC_SAMPS/fact);
         }
      }

      delay_effect = _EFFECT_GET_DATA(p2d, DELAY_EFFECT);	// phasing, etc.
      freq_filter = _FILTER_GET_DATA(p2d, FREQUENCY_FILTER);
      dist_state = _EFFECT_GET_STATE(p2d, DISTORTION_EFFECT);
      if (delay_effect) {
         ddesamps = rbd->dde_samples;
      }

      if (track_ptr)
      {
         unsigned int cdesamps, cno_samples;
         unsigned int track, smin, src_pos;
         float ftmp, smu;

         ftmp = src_pos_sec * sfreq;
         smin = floorf(ftmp);
         smu = ftmp - smin;

         src_pos = src->curr_sample;
         cdesamps = floorf(ddesamps*fact);
         cno_samples = ceilf(dno_samples*fact);
         if ((cno_samples > sno_samples) && !src_loops)
         {
            cno_samples = sno_samples;
            cdesamps = 0;
         }

         if (!freq_filter && !delay_effect && !dist_state)
         {
            int32_t *scratch0 = track_ptr[SCRATCH_BUFFER0];
            for (track=0; track<sno_tracks; track++)
            {
               int32_t *dptr = track_ptr[track];
               void *sptr = rbs->track[track];

               DBG_MEMCLR(1, scratch0-ddesamps, ddesamps+dend, sizeof(int32_t));
               _aaxProcessCodec(scratch0, sptr, rbs->codec, src_pos,
                                sstart, sno_samples, cdesamps, cno_samples,
                                sbps, src_loops);

               DBG_MEMCLR(1, dptr-ddesamps, ddesamps+dend, sizeof(int32_t));
               _aaxProcessResample(dptr-ddesamps, scratch0-cdesamps, dest_pos,
                                   dest_pos+dno_samples+ddesamps, smu, fact);
            }
         }
         else
         {
            int32_t *scratch0 = track_ptr[SCRATCH_BUFFER0];
            int32_t *scratch1 = track_ptr[SCRATCH_BUFFER1];
            void* distortion_effect = NULL;
            
            if ( dist_state) {
                distortion_effect = &_EFFECT_GET(p2d, DISTORTION_EFFECT, 0);
            }

            for (track=0; track<sno_tracks; track++)
            {
               int32_t *dptr = track_ptr[track];
               void *sptr = rbs->track[track];

               DBG_MEMCLR(1, scratch0-ddesamps, ddesamps+dend, sizeof(int32_t));
               _aaxProcessCodec(scratch0, sptr, rbs->codec, src_pos,
                                sstart, sno_samples, cdesamps, cno_samples,
                                sbps, src_loops);

               DBG_MEMCLR(1, scratch1-ddesamps, ddesamps+dend, sizeof(int32_t));
               _aaxProcessResample(scratch1-ddesamps, scratch0-cdesamps,
                                   dest_pos, dest_pos+dno_samples+ddesamps,
                                   smu, fact);

               DBG_MEMCLR(1, dptr-ddesamps, ddesamps+dend, sizeof(int32_t));
               bufEffectsApply(dptr, scratch1, scratch0, dest_pos, dend,
                               dno_samples, ddesamps, track, ctr,
                               freq_filter, delay_effect, distortion_effect);
            }
         }
      }

      src->curr_sample = floorf(new_src_pos_sec * sfreq);
      src->curr_pos_sec = new_src_pos_sec;
   }

   return track_ptr;
}

/* -------------------------------------------------------------------------- */

void
_aaxProcessResample(int32_ptr d, const int32_ptr s, unsigned int dmin, unsigned int dmax, float smu, float fact)
{
   _batch_resample_proc resamplefn;

   assert(fact > 0.0f);

   if (fact < 0.25f) {
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
bufCompressElectronic(void *d, unsigned int dmin, unsigned int dmax) {
   bufCompress(d, dmin, dmax, 0.5f, 0.0f);
}

void
bufCompressDigital(void *d, unsigned int dmin, unsigned int dmax) {
   bufCompress(d, dmin, dmax, 0.9f, 0.0f);
}

void
bufCompressValve(void *d, unsigned int dmin, unsigned int dmax) {
   bufCompress(d, dmin, dmax, 0.2f, 0.9f);
}

#define BITS		11
#define SHIFT		(31-BITS)
#define START		((1<<SHIFT)-1)
#define FACT		(float)(23-SHIFT)/(float)(1<<(31-SHIFT))
#if 1
void
bufCompress(void *d, unsigned int dmin, unsigned int dmax, float clip, float asym)
{
   int32_t *ptr = (int32_t*)d;
   float osamp = 0.0f;
   float imix, mix;
   unsigned int j;

   mix = _MINMAX(clip, 0.0, 1.0);
   imix = (1.0f - mix);
   j = dmax-dmin;
   asym *= 4096.0f;
   do
   {
      static const float df = 1.0f/(float)0x7FFFFFFF;
      float fact1, fact2, sdf, rise;
      unsigned int pos;
      uint32_t asamp;
      int32_t samp;

      samp = *ptr;
      asamp = abs(samp+asym);

      pos = 1+(asamp >> SHIFT);
      sdf = asamp*df;

      assert(sdf >= 0.0f);
      assert(sdf <= 1.0f);

      rise = _MINMAX(osamp*df-sdf, 0.0f, 1.0f);
      pos = _MINMAX(pos+asym*rise, 0, ((1<<BITS)-1));
      osamp = asamp;

      fact1 = (1.0f-sdf)*_compress_tbl[0][pos-1];
      fact1 += sdf*_compress_tbl[0][pos];

      fact2 = (1.0f-sdf)*_compress_tbl[1][pos-1];
      fact2 += sdf*_compress_tbl[1][pos];

      *ptr++ = (imix*fact1 + mix*fact2)*samp;
   }
   while (--j);
}

#else
/* arctan */
void
bufCompress(void *d, unsigned int dmin, unsigned int dmax, float clip, float asym)
{
   int32_t *ptr = (int32_t*)d;
   unsigned int j;
   float mix;

   mix = 256.0f; //  * _MINMAX(clip, 0.0, 1.0);
   j = dmax-dmin;
   do
   {
      static const float df = 1.0f/(float)0x7FFFFFFF;
      float samp;

      samp = atan(*ptr*df*mix)*GMATH_1_PI_2;
      *ptr++ = samp*(float)0x7FFFFF;
   }
   while (--j);
}
#endif
