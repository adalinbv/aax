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
#include "ringbuffer.h"

void
_aaxRingBufferMixMono16Stereo(_aaxRingBufferSample *drbd, const int32_ptrptr sptr, _aax2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   /** Mix */
   for (t=0; t<drbd->no_tracks; t++)
   {
      int32_t *dptr = (int32_t *)drbd->track[t] + offs;
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
#if RB_FLOAT_DATA
      _batch_fmadd(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);
#else
      _batch_imadd(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);
#endif

      ep2d->prev_gain[t] = gain;
   }
}

void
_aaxRingBufferMixMono16Surround(_aaxRingBufferSample *drbd, const int32_ptrptr sptr, _aax2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   /** Mix */
   for (t=0; t<drbd->no_tracks; t++)
   {
      int32_t *dptr = (int32_t *)drbd->track[t] + offs;
      float vstart, vend, vstep;
      float dir_fact;
      float hrtf_volume[3];
      int j;

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

      hrtf_volume[DIR_RIGHT] = dir_fact*vend;
#if RB_FLOAT_DATA
      _batch_fmadd(dptr, sptr[ch]+offs, dno_samples, dir_fact*vstart, vstep);
#else
      _batch_imadd(dptr, sptr[ch]+offs, dno_samples, dir_fact*vstart, vstep);
#endif

      ep2d->prev_gain[t] = vend;

      /**
       * vertical positioning
       **/
      dir_fact = ep2d->speaker[t][DIR_UPWD];
      hrtf_volume[DIR_UPWD] = (0.25f + dir_fact)*vend;

      /**
       * horizontal positioning, back-front
       **/
      dir_fact = ep2d->speaker[t][DIR_BACK];
      hrtf_volume[DIR_BACK] = gain * (0.25f + 0.5f*dir_fact)*vend;

      for (j=1; j<2; j++)	/* skip left-right and back-front */
      {
         int diff = (int)ep2d->hrtf[t][j];
         float v_start, v_step;

         assert(diff < (int)drbd->dde_samples);
         assert(diff > -(int)dno_samples);
         diff = _MINMAX(diff, -(int)dno_samples, (int)drbd->dde_samples);

         v_start = vstart * hrtf_volume[j];
         v_step = 0.0f; // vstep * hrtf_volume[j];

//       DBG_MEMCLR(!offs, drbd->track[t], drbd->no_samples, sizeof(int32_t));
#if RB_FLOAT_DATA
         _batch_fmadd(dptr, sptr[ch]+offs-diff, dno_samples, v_start, v_step);
#else
         _batch_imadd(dptr, sptr[ch]+offs-diff, dno_samples, v_start, v_step);
#endif
      }
   }
}

void
_aaxRingBufferMixMono16Spatial(_aaxRingBufferSample *drbd, int32_t **sptr, _aax2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);
   /** Mix */
   for (t=0; t<drbd->no_tracks; t++)
   {
      int32_t *dptr = (int32_t *)drbd->track[t] + offs;
      float vstart, vend, vstep;
      float dir_fact;

      dir_fact = ep2d->speaker[t][DIR_RIGHT];
      vstart = dir_fact * svol * ep2d->prev_gain[t];
      vend   = dir_fact * evol * gain;
      vstep  = (vend - vstart) / dno_samples;

//    DBG_MEMCLR(!offs, drbd->track[t], drbd->no_samples, sizeof(int32_t));
#if RB_FLOAT_DATA
      _batch_fmadd(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);
#else
      _batch_imadd(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);
#endif

      ep2d->prev_gain[t] = gain;
   }
}

/* the time delay from ear to ear, in seconds */
#define HEAD_DELAY	p2d->head[0]
#define IDT_FB_DEVIDER	(ep2d->head[0] / ep2d->head[1])
#define IDT_UD_DEVIDER	(ep2d->head[0] / ep2d->head[2])
#define IDT_UD_OFFSET	p2d->head[3]

void
_aaxRingBufferMixMono16HRTF(_aaxRingBufferSample *drbd, int32_t **sptr, _aax2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   for (t=0; t<drbd->no_tracks; t++)
   {
      int32_t *track = (int32_t *)drbd->track[t];
      float vstart, vend, dir_fact; //, vstep;
      int32_t *dptr, *ptr;
      float hrtf_volume[3];
      int j;

      vstart = svol * ep2d->prev_gain[t];
      vend = gain * evol;
      ep2d->prev_gain[t] = vend;

      /*
       * IID; Interaural Intensitive Differenc
       * TODO: Implement 'head shadow' _frequency_ filter
       */

      /**
       * horizontal positioning, left-right
       **/
      dir_fact = ep2d->speaker[t][DIR_RIGHT];
      hrtf_volume[DIR_RIGHT] = 0.5f + dir_fact*vend;
#if 0
 printf("l-r: %i, volume %f, delay: %f, dir_fact: %f, f_gain: %f\n", t, hrtf_volume[DIR_RIGHT], ep2d->hrtf[t][DIR_RIGHT]/48000.0, dir_fact, vend);
#endif

      /**
       * vertical positioning
       **/
      dir_fact = (ep2d->speaker[t][DIR_UPWD]);
      hrtf_volume[DIR_UPWD] = (0.25f + dir_fact)*vend;
#if 0
 printf("u-d: %i, volume %f, delay: %f, dir_fact: %f\n", t, hrtf_volume[DIR_UPWD], ep2d->hrtf[t][DIR_UPWD]/48000.0, dir_fact);
#endif

      /**
       * horizontal positioning, back-front
       **/
      dir_fact = (ep2d->speaker[t][DIR_BACK]);
      hrtf_volume[DIR_BACK] = (0.25f + 0.5f*dir_fact)*vend;
#if 0
 printf("f-b: %i, volume %f, delay: %f, dir_fact: %f\n", t, hrtf_volume[DIR_BACK], ep2d->hrtf[t][DIR_BACK]/48000.0, dir_fact);
#endif

      dptr = track+offs;
      ptr = sptr[ch]+offs;
      // vstep = (vend - vstart) / dno_samples;
      for (j=0; j<3; j++)
      {
         int diff = (int)ep2d->hrtf[t][j];
         float v_start, v_step;

         assert(diff < (int)drbd->dde_samples);
         assert(diff > -(int)dno_samples);
         diff = _MINMAX(diff, -(int)dno_samples, (int)drbd->dde_samples);
 
         v_start = vstart * hrtf_volume[j];
         v_step = 0.0f; // vstep * hrtf_volume[j];

//       DBG_MEMCLR(!offs, drbd->track[t], drbd->no_samples, sizeof(int32_t));
#if RB_FLOAT_DATA
         _batch_fmadd(dptr, ptr-diff, dno_samples, v_start, v_step);
#else
         _batch_imadd(dptr, ptr-diff, dno_samples, v_start, v_step);
#endif
      }
   }
}

