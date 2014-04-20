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

/*
 * 1:N ringbuffer mixer functions.
 */

/*
 * Sources:
 * http://en.wikipedia.org/wiki/Doppler_effect
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

#include "software/rbuf_int.h"

void
_aaxRingBufferMixMono16Stereo(_aaxRingBufferSample *drbd, CONST_MIX_PTRPTR_T sptr, _aax2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   /** Mix */
   for (t=0; t<drbd->no_tracks; t++)
   {
      MIX_T *dptr = (MIX_T*)drbd->track[t] + offs;
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
#ifdef PURE_STEREO
      dir_fact = _MINMAX(ep2d->speaker[t][DIR_RIGHT], 0.0f, 1.0f);
#else
      dir_fact = _MIN(0.8776f + ep2d->speaker[t][DIR_RIGHT], 1.0f);
#endif
      vstart = dir_fact * svol * ep2d->prev_gain[t];
      vend   = dir_fact * evol * gain;
      vstep  = (vend - vstart) / dno_samples;

//    DBG_MEMCLR(!offs, drbd->track[t], drbd->no_samples, sizeof(int32_t));
      drbd->add(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);

      ep2d->prev_gain[t] = gain;
   }
}

void
_aaxRingBufferMixMono16Surround(_aaxRingBufferSample *drbd, CONST_MIX_PTRPTR_T sptr, _aax2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   /** Mix */
   for (t=0; t<drbd->no_tracks; t++)
   {
      MIX_T *dptr = (MIX_T*)drbd->track[t] + offs;
      float vstart, vend, vstep;
      float dir_fact;
      float hrtf_volume[3];
      int i;

      /**
       * horizontal positioning, left-right
       **/
#ifdef USE_SPATIAL_FOR_SURROUND
      dir_fact = ep2d->speaker[t][DIR_RIGHT];
#else
      dir_fact = _MIN(0.8776f + ep2d->speaker[t][DIR_RIGHT], 1.0f);
#endif
      vstart = svol * ep2d->prev_gain[t];
      vend = evol * gain;
      vstep = (vend - vstart) / dno_samples;

      drbd->add(dptr, sptr[ch]+offs, dno_samples, dir_fact*vstart, vstep);

      ep2d->prev_gain[t] = vend;

      /**
       * vertical positioning
       **/
      dir_fact = ep2d->speaker[t][DIR_UPWD];
      hrtf_volume[DIR_UPWD] = 0.3f;
      gain *= 0.76923f; 		/* 1.0f/0.3f */

      i = DIR_UPWD;			/* skip left-right and back-front */
      do
      {
         int diff = (int)ep2d->hrtf[t][i];
         float v_start, v_step;

         if (hrtf_volume[i] < 1e-3f) { // || (i > 0 && diff == 0)) {
            continue;
         }

         assert(diff < (int)drbd->dde_samples);
         assert(diff > -(int)dno_samples);
         diff = _MINMAX(diff, -(int)dno_samples, (int)drbd->dde_samples);

         v_start = vstart * hrtf_volume[i];
         v_step = ((vend - vstart) * hrtf_volume[i])/dno_samples;

//       DBG_MEMCLR(!offs, drbd->track[t], drbd->no_samples, sizeof(int32_t));
         drbd->add(dptr, sptr[ch]+offs-diff, dno_samples, v_start, v_step);
      }
      while(0);
   }
}

void
_aaxRingBufferMixMono16Spatial(_aaxRingBufferSample *drbd, CONST_MIX_PTRPTR_T sptr, _aax2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   /** Mix */
   for (t=0; t<drbd->no_tracks; t++)
   {
      MIX_T *dptr = (MIX_T*)drbd->track[t] + offs;
      float vstart, vend, vstep;
      float dir_fact;

      dir_fact = ep2d->speaker[t][DIR_RIGHT];
      vstart = dir_fact * svol * ep2d->prev_gain[t];
      vend   = dir_fact * evol * gain;
      vstep  = (vend - vstart) / dno_samples;

//    DBG_MEMCLR(!offs, drbd->track[t], drbd->no_samples, sizeof(int32_t));
      drbd->add(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);

      ep2d->prev_gain[t] = gain;
   }
}

void
_aaxRingBufferMixMono16HRTF(_aaxRingBufferSample *drbd, CONST_MIX_PTRPTR_T sptr, _aax2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   for (t=0; t<drbd->no_tracks; t++)
   {
      MIX_T *track = drbd->track[t];
      float dir_fact, hrtf_volume[3];
      const MIX_T *ptr;
      MIX_T *dptr;
      int i;

      /*
       * IID; Interaural Intensitive Differenc
       * TODO: Implement 'head shadow' _frequency_ filter
       */

      /**
       * horizontal positioning, left-right
       **/
      dir_fact = ep2d->speaker[t][DIR_RIGHT];
      hrtf_volume[DIR_RIGHT] = 0.66f + dir_fact;

      /**
       * vertical positioning
       **/
      dir_fact = (ep2d->speaker[t][DIR_UPWD]);
      hrtf_volume[DIR_UPWD] = 0.3f;

      /**
       * horizontal positioning, back-front
       **/
      dir_fact = (ep2d->speaker[t][DIR_BACK]);
      hrtf_volume[DIR_BACK] = 0.33f + dir_fact;

#if 0
 printf("t: %i, lr: %3.2f (%5.4f ms), ud: %3.2f (%5.4f ms), bf: %3.2f (%5.4f ms)\n", t, hrtf_volume[DIR_RIGHT], 1000*ep2d->hrtf[t][0]/44100.0f, hrtf_volume[DIR_UPWD], 1000*ep2d->hrtf[t][1]/44100.0f, hrtf_volume[DIR_BACK], 1000*ep2d->hrtf[t][2]/44100.0f);
#endif
      gain = _MIN(gain, 0.69f);	// compensate for a combined gain of 1.45 below

      dptr = track+offs;
      ptr = sptr[ch]+offs;
      for (i=0; i<3; i++)
      {
         int diff = (int)ep2d->hrtf[t][i];
         float v_start, v_end, v_step;

         if (hrtf_volume[i] < 1e-3f) { // || (i > 0 && diff == 0))
            continue;
         }

         assert(diff < (int)drbd->dde_samples);
         assert(diff > -(int)dno_samples);
         diff = _MINMAX(diff, -(int)dno_samples, (int)drbd->dde_samples);
 
         v_start = ep2d->prev_gain[3*t+i] * svol;
         v_end = hrtf_volume[i] * gain * evol;
         v_step = (v_end - v_start)/dno_samples;

//       DBG_MEMCLR(!offs, drbd->track[t], drbd->no_samples, sizeof(int32_t));
         drbd->add(dptr, ptr-diff, dno_samples, v_start, v_step);

         ep2d->prev_gain[3*t+i] = hrtf_volume[i] * gain;
      }
   }
}

