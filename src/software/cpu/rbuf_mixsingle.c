/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

/*
 * 1:N ringbuffer mixer functions.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _DEBUG
# define _DEBUG		0
#endif

#include <assert.h>

#include <api.h>
#include <ringbuffer.h>
#include <dsp/dsp.h>
#include <dsp/filters.h>

#include "software/rbuf_int.h"

void
_aaxRingBufferMixMono16Mono(_aaxRingBufferSample *drbd, CONST_MIX_PTRPTR_T sptr, UNUSED(const unsigned char *router), _aax2dProps *ep2d, unsigned char ch, size_t offs, size_t dno_samples, UNUSED(float fs), float gain, float svol, float evol)
{
   MIX_T *dptr = (MIX_T*)drbd->track[0] + offs;
   float vstart, vend, vstep;

   vstart = ep2d->prev_gain[0] * svol;
   vend   = gain * evol;
   vstep  = (vend - vstart) / dno_samples;

// DBG_MEMCLR(!offs, drbd->track[t], drbd->no_samples, sizeof(int32_t));
   drbd->add(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);

   ep2d->prev_gain[0] = vend;
}

void
_aaxRingBufferMixMono16Stereo(_aaxRingBufferSample *drbd, CONST_MIX_PTRPTR_T sptr, const unsigned char *router, _aax2dProps *ep2d, unsigned char ch, size_t offs, size_t dno_samples, UNUSED(float fs), float gain, float svol, float evol)
{
   int t;

   _AAX_LOG(LOG_DEBUG, __func__);

   /** Mix */
   for (t=0; t<drbd->no_tracks; t++)
   {
      MIX_T *dptr = (MIX_T*)drbd->track[router[t]] + offs;
      float vstart, vend, vstep;
      float dir_fact;

      /*
       * dir_fact is speaker and source position dependent.
       * If the source is at the speaker's side of the listener it will
       * always be 1.0 (normal volume) for this speaker, but if it's located
       * at the opposite side of the speaker (the listener is between the
       * speaker and the source) it gradually silences until the source is
       * directly opposite of the speaker, relative to the listener.
       *
       * 0.8776 = cosf(0.5)
       */
      dir_fact = _MINMAX(0.5f+ep2d->speaker[t].v4[DIR_RIGHT], 0.0f, 1.0f);
      vstart = ep2d->prev_gain[t] * svol;
      vend   = gain * dir_fact * evol;
      vstep  = (vend - vstart) / dno_samples;

//    DBG_MEMCLR(!offs, drbd->track[t], drbd->no_samples, sizeof(int32_t));
      drbd->add(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);

      ep2d->prev_gain[t] = vend;
   }
}

void
_aaxRingBufferMixMono16Surround(_aaxRingBufferSample *drbd, CONST_MIX_PTRPTR_T sptr, const unsigned char *router, _aax2dProps *ep2d, unsigned char ch, size_t offs, size_t dno_samples, UNUSED(float fs), float gain, float svol, float evol)
{
   int t;

   _AAX_LOG(LOG_DEBUG, __func__);

   /** Mix
    * 1. Copy all channels to LFE and aplly a 40Hz-200Hz (80Hz default)
    *    low pass filter. (It’s not that 100 Hz is really that easy to localize,
    *    but that frequencies a bit above it are.)
    * 2. Apply a high-pass filter with the same cut off frequency to all tracks
    *    but the mains (front left and right) and the LFE.
    *
    * http://www.hometheaterhifi.com/volume_9_3/feature-article-multiple-crossovers-9-2002.html
    * If you want consistent bass response from each channel of your 5.1 system,
    * in our opinion, you're best to set all speakers to "Small", set them all
    * to the same crossover point, and set that point no lower than what you
    * are comfortable throwing away from the LFE channel.  If your main left
    * and right speakers are genuinely full range (be honest now!), then you
    * are better off running them full range as opposed to high-passing them
    * at a ridiculously low frequency.
    *
    *http://www.hometheaterhifi.com/volume_12_2/feature-article-slope-troubles-6-2005.html
    * The up front solution lies in getting receiver and processor manufacturers
    * to start giving us a bass management scheme that caters to "full-range"
    * speakers.  Taking the THX Linkwitz/Riley scheme as a good place to start,
    * all they need to do is provide a choice of high-pass:  2nd order for THX
    * speakers (and other true satellites), and 4th order for all others.  With
    * such a crossover, the main speakers need only be reasonably flat to an
    * octave below the chosen frequency, which, in the case of the common 80 Hz
    * crossover, means being flat to 40 Hz.  Virtually all "full-range" speakers
    * - even most of the smaller bookshelf models - qualify.
    *
    * This is applied in src/software/mixer.c: _aaxSoftwareMixerPostProcess()
    */
   for (t=0; t<drbd->no_tracks; t++)
   {
      MIX_T *dptr = (MIX_T*)drbd->track[router[t]] + offs;
      float vstart, vend, vstep;
      float dir_fact;

      /**
       * horizontal positioning, left-right
       **/
      dir_fact = _MINMAX(0.5f+ep2d->speaker[t].v4[DIR_RIGHT], 0.0f, 1.0f);
      vstart = ep2d->prev_gain[t] * svol;
      vend = gain * dir_fact * evol;
      vstep = (vend - vstart) / dno_samples;

      drbd->add(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);

      ep2d->prev_gain[t] = vend;
   }
}

void
_aaxRingBufferMixMono16SpatialSurround(_aaxRingBufferSample *drbd, CONST_MIX_PTRPTR_T sptr, const unsigned char *router, _aax2dProps *ep2d, unsigned char ch, size_t offs, size_t dno_samples, UNUSED(float fs), float gain, float svol, float evol)
{
   int t;

   _AAX_LOG(LOG_DEBUG, __func__);

   for (t=0; t<drbd->no_tracks; t++)
   {
      MIX_T *dptr = (MIX_T*)drbd->track[router[t]] + offs;
      float vstart, vend, vstep;
      float dir_fact;
      float hrtf_volume[3];
      int i;

      /**
       * horizontal positioning, left-right
       **/
      dir_fact = ep2d->speaker[t].v4[DIR_RIGHT];
      vstart = ep2d->prev_gain[t] * svol;
      vend = gain * dir_fact * evol;
      vstep = (vend - vstart) / dno_samples;

      drbd->add(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);

      ep2d->prev_gain[t] = vend;

      /**
       * vertical positioning
       **/
      dir_fact = ep2d->speaker[t].v4[DIR_UPWD];
      hrtf_volume[DIR_UPWD] = _MAX(-0.125f*dir_fact, 0.1f);
      gain *= 0.76923f;                 /* 1.0f/0.3f */

      i = DIR_UPWD;                     /* skip left-right and back-front */
      do
      {
         ssize_t diff = (ssize_t)ep2d->hrtf[t].v4[i];
         float v_start, v_step;

         assert(diff < (ssize_t)drbd->dde_samples);
         assert(diff > -(ssize_t)dno_samples);
         diff = _MINMAX(diff, -(ssize_t)dno_samples,(ssize_t)drbd->dde_samples);

         v_start = vstart * hrtf_volume[i];
         v_step = ((vend - vstart) * hrtf_volume[i])/dno_samples;

//       DBG_MEMCLR(!offs, drbd->track[t], drbd->no_samples, sizeof(int32_t));

// TODO: add HF filtered version of sptr[ch] if (t != AAX_TRACK_LFE)
// TODO: or add LF filtered verson of sptr[ch] if (t == AAX_TRACK_LFE)
         drbd->add(dptr, sptr[ch]+offs-diff, dno_samples, v_start, v_step);
      }
      while(0);
   }
}

void
_aaxRingBufferMixMono16Spatial(_aaxRingBufferSample *drbd, CONST_MIX_PTRPTR_T sptr, const unsigned char *router, _aax2dProps *ep2d, unsigned char ch, size_t offs, size_t dno_samples, UNUSED(float fs), float gain, float svol, float evol)
{
   int t;

   _AAX_LOG(LOG_DEBUG, __func__);

   /** Mix */
   for (t=0; t<drbd->no_tracks; t++)
   {
      MIX_T *dptr = (MIX_T*)drbd->track[router[t]] + offs;
      float vstart, vend, vstep;
      float dir_fact;

      dir_fact = ep2d->speaker[t].v4[DIR_RIGHT];
      vstart = ep2d->prev_gain[t] * svol;
      vend   = gain * dir_fact * evol;
      vstep  = (vend - vstart) / dno_samples;

//    DBG_MEMCLR(!offs, drbd->track[t], drbd->no_samples, sizeof(int32_t));
      drbd->add(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);

      ep2d->prev_gain[t] = vend;
   }
}

// https://www.sfu.ca/sonic-studio-webdav/handbook/Binaural_Hearing.html
// https://web.archive.org/web/20190216004141/https://www.sfu.ca/sonic-studio-webdav/handbook/Binaural_Hearing.html
void
_aaxRingBufferMixMono16HRTF(_aaxRingBufferSample *drbd, CONST_MIX_PTRPTR_T sptr, const unsigned char *router, _aax2dProps *ep2d, unsigned char ch, size_t offs, size_t dno_samples, float fs, float gain, float svol, float evol)
{
   int t;

   _AAX_LOG(LOG_DEBUG, __func__);

   // compensate for a combined gain of 1.45 below
// gain *= 0.69f;

   /** Mix */
   for (t=0; t<drbd->no_tracks; t++)
   {
      MIX_T *dptr = (MIX_T*)drbd->track[router[t]] + offs;
      float vstart, vend, vstep;
      float dir_fact;
      int diff, dir;
#if 0
 printf("t: %i, lr: %5.4f ms, ud: %5.4f ms, bf: %5.4f ms\n", t,
              1000*ep2d->hrtf[t].v4[0]/44100.0f,
              1000*ep2d->hrtf[t].v4[1]/44100.0f,
              1000*ep2d->hrtf[t].v4[2]/44100.0f);
#endif

      /* left-right */
      dir = DIR_RIGHT;
      dir_fact = 0.4f + 0.4f*ep2d->speaker[t].v4[dir];
      vstart = ep2d->prev_gain[3*t+dir] * svol;
      vend   = gain * dir_fact * evol;
      vstep  = (vend - vstart) / dno_samples;
      diff = (ssize_t)ep2d->hrtf[t].v4[dir];
      drbd->add(dptr, sptr[ch]+offs-diff, dno_samples, vstart, vstep);
      ep2d->prev_gain[3*t+dir] = vend;

      /* down-up */
      dir = DIR_UPWD;
      dir_fact = _MAX(0.175f + 0.25f*ep2d->speaker[t].v4[dir], 0.1f);
      vstart = ep2d->prev_gain[3*t+dir] * svol;
      vend   = gain * dir_fact * evol;
      vstep  = (vend - vstart) / dno_samples;

      diff = (ssize_t)ep2d->hrtf[t].v4[dir];
      drbd->add(dptr, sptr[ch]+offs-diff, dno_samples, vstart, vstep);
      ep2d->prev_gain[3*t+dir] = vend;

      /* front-back */
      dir = DIR_BACK;
      dir_fact = _MAX(0.175f + 0.25f*ep2d->speaker[t].v4[dir], 0.1f);
      vstart = ep2d->prev_gain[3*t+dir] * svol;
      vend   = gain * dir_fact * evol;
      vstep  = (vend - vstart) / dno_samples;

      diff = (ssize_t)ep2d->hrtf[t].v4[dir];
      drbd->add(dptr, sptr[ch]+offs-diff, dno_samples, vstart, vstep);
      ep2d->prev_gain[3*t+dir] = vend;

      /*
       * IID; Interaural Intensitive Difference
       * HEAD shadow frequency filter
       */
      dir_fact = ep2d->speaker[t].v4[DIR_RIGHT];
      if (dir_fact < 0.0f)
      {
// http://www.cns.nyu.edu/~david/courses/perception/lecturenotes/localization/localization-slides/Slide18.jpg
         // dir_fact = 0.0f: 20kHz, dir_fact = -1.0f: 250Hz
         // log10(MAX_CUTOFF - 1000) = 4.2787541
         float fc = MAX_CUTOFF - _log2lin(-4.278754f*dir_fact);
         ep2d->k = _aax_movingaverage_compute(fc, fs);

         if (ep2d->k <= 0.8f)
         {
            float *hist = &ep2d->freqfilter_history[t];
            _batch_movingaverage_float(dptr, dptr, dno_samples, hist, ep2d->k);
         }
      }
   }
}

