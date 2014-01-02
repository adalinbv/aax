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

#include <math.h>
#include <assert.h>

#include <api.h>
#include <objects.h>

#include <base/geometry.h>
#include <base/logging.h>

#include "software/ringbuffer.h"
#include "arch2d_simd.h"
#include "ringbuffer.h"

/**
 * Mix a single track source buffer into a multi track destination buffer.
 * The result will be a stereo-mixed multitrack recording.
 *
 * When the source buffer has more tracks than the destination buffer the
 * first remaining source track will be mixed with the first destination
 * buffer and so on.
 *
 * @param drbi multi track destination buffer
 * @param srbi single track source buffer
 * @param mode requested mixing mode
 * @param ep2d 3d positioning information structure of the source
 * @param fp2f 3d positioning information structure of the parents frame
 * @param ch channel to use from the source buffer if it is multi-channel
 * #param ctr update-rate counter:
 *     - Rendering to the destination buffer is done every frame at the
 *       interval rate. Updating of 3d properties and the like is done
 *       once every 'ctr' frame updates. so if ctr == 1, updates are
 *       done every frame.
 * @param nbuf number of buffers in the source queue (>1 means streaming)
 */
int
_aaxRingBufferMixMono16(_aaxRingBuffer *drb, _aaxRingBuffer *srb, _aax2dProps *ep2d, _aax2dProps *fp2d, unsigned char ch, unsigned char ctr, unsigned int nbuf)
{
   _aaxRingBufferData *drbi, *srbi;
   unsigned int offs, dno_samples;
   _aaxRingBufferLFOData *lfo;
   float gain, svol, evol;
   float pitch, max;
   int32_t **sptr;
   void *env;
   int ret = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(drb != NULL);
   assert(srb != NULL);

   drbi = drb->handle;
   srbi = srb->handle;
   assert(srbi != 0);
   assert(drbi != 0);
   assert(srbi->sample != 0);
   assert(drbi->sample != 0);
   assert(ep2d != 0);

   /** Pitch */
   pitch = ep2d->final.pitch; /* Doppler effect */
   pitch *= _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_PITCH);
   lfo = _EFFECT_GET_DATA(ep2d, DYNAMIC_PITCH_EFFECT);
   if (lfo) {
      pitch *= lfo->get(lfo, NULL, 0, 0);
   }

   if (fp2d)
   {
      float lfo = fp2d->final.pitch_lfo-0.5f;
      pitch *= _EFFECT_GET(fp2d, PITCH_EFFECT, AAX_PITCH);
      pitch = 1.0f+((pitch-1.0f)*lfo);
   }

   env = _EFFECT_GET_DATA(ep2d, TIMED_PITCH_EFFECT);
   pitch *= _aaxRingBufferEnvelopeGet(env, srbi->stopped);

   max = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch, 0.0f, max);

   /** Resample */
   offs = (drbi->mode == AAX_MODE_WRITE_HRTF) ? drbi->sample->dde_samples : 0;
   sptr = drbi->mix(drbi, srbi, ep2d, pitch, &offs, &dno_samples, ctr, nbuf);
   if (sptr == NULL || dno_samples == 0)
   {
      if (srbi->playing == 0 && srbi->stopped == 1) {
         return -1;
      } else {
         return 0;
      }
   }

   /** Volume */
   env = _FILTER_GET_DATA(ep2d, TIMED_GAIN_FILTER);
   if (srbi->playing == 0 && srbi->stopped == 1)
   {
      /* the emitter was already flagged as stopped */
      ret = -1;
   }
   else if (!env && srbi->stopped == 1)
   {
      /*
       * Distance delay induced stopping of playback
       * In the event that distance delay is not active dist_delay_sec equals
       * to 0 so detracting duration_sec instantly turns dist_delay_sec < 0.0
       */
      ep2d->dist_delay_sec -= drbi->sample->duration_sec;
      if (ep2d->dist_delay_sec <= 0.0f) {
         ret = -1;
      }
   }

   /* apply envelope filter */
   gain = _aaxRingBufferEnvelopeGet(env, srbi->stopped);
   if (gain < -1e-3f) {
      ret = -1;
   }

   /* 3d: distance and audio-cone related gain */
   gain *= ep2d->final.gain;

   /* apply the parent mixer/audio-frame volume and tremolo-gain */
   max = 1.0f;
   if (fp2d)
   {
      gain *= _FILTER_GET(fp2d, VOLUME_FILTER, AAX_GAIN);
      max *= fp2d->final.gain_lfo;
   }

   /* tremolo and envelope following gain filter */
   lfo = _FILTER_GET_DATA(ep2d, DYNAMIC_GAIN_FILTER);
   if (lfo)
   {
      if (lfo->envelope)
      {
         float g = lfo->get(lfo, sptr[ch]+offs, 0, dno_samples);
         if (lfo->inv) g = 1.0f/g;
         gain *= g;
      }
      else {
         max *= lfo->get(lfo, NULL, 0, 0);
      }
   }

   /* tremolo was defined */
   if (max != 1.0f) {
      gain *= 1.0f - max/2.0f;
   }

   /* final emitter volume */
   gain *= _FILTER_GET(ep2d, VOLUME_FILTER, AAX_GAIN);

   /** Automatic volume ramping to avoid clicking */
   svol = evol = 1.0f;
   if (!env && (srbi->playing == srbi->stopped))
   {
      svol = (srbi->stopped || offs) ? 1.0f : 0.0f;
      evol = (srbi->stopped) ? 0.0f : 1.0f;
      srbi->playing = !srbi->stopped;
   }

   /* Mix */
   drbi->mix1n(drb, sptr, ep2d, ch, offs, dno_samples, gain, svol, evol);

   return ret;
}

/* -------------------------------------------------------------------------- */

void
_aaxRingBufferMixMono16Stereo(_aaxRingBuffer *drb, const int32_ptrptr sptr, _aax2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   _aaxRingBufferData *drbi;
   _aaxRingBufferSample *rbd;
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   /** Mix */
   drbi = drb->handle;
   rbd = drbi->sample;
   for (t=0; t<rbd->no_tracks; t++)
   {
      int32_t *dptr = (int32_t *)rbd->track[t] + offs;
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

//    DBG_MEMCLR(!offs, rbd->track[t], rbd->no_samples, sizeof(int32_t));
#if RB_FLOAT_DATA
      _batch_fmadd(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);
#else
      _batch_imadd(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);
#endif

      ep2d->prev_gain[t] = gain;
   }
}

void
_aaxRingBufferMixMono16Surround(_aaxRingBuffer *drb, const int32_ptrptr sptr, _aax2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   _aaxRingBufferData *drbi;
   _aaxRingBufferSample *rbd;
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   /** Mix */
   drbi = drb->handle;
   rbd = drbi->sample;
   for (t=0; t<rbd->no_tracks; t++)
   {
      int32_t *dptr = (int32_t *)rbd->track[t] + offs;
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

         assert(diff < (int)rbd->dde_samples);
         assert(diff > -(int)dno_samples);
         diff = _MINMAX(diff, -(int)dno_samples, (int)rbd->dde_samples);

         v_start = vstart * hrtf_volume[j];
         v_step = 0.0f; // vstep * hrtf_volume[j];

//       DBG_MEMCLR(!offs, rbd->track[t], rbd->no_samples, sizeof(int32_t));
#if RB_FLOAT_DATA
         _batch_fmadd(dptr, sptr[ch]+offs-diff, dno_samples, v_start, v_step);
#else
         _batch_imadd(dptr, sptr[ch]+offs-diff, dno_samples, v_start, v_step);
#endif
      }
   }
}

void
_aaxRingBufferMixMono16Spatial(_aaxRingBuffer *drb, int32_t **sptr, _aax2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   _aaxRingBufferData *drbi;
   _aaxRingBufferSample *rbd;
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);
   /** Mix */
   drbi = drb->handle;
   rbd = drbi->sample;
   for (t=0; t<rbd->no_tracks; t++)
   {
      int32_t *dptr = (int32_t *)rbd->track[t] + offs;
      float vstart, vend, vstep;
      float dir_fact;

      dir_fact = ep2d->speaker[t][DIR_RIGHT];
      vstart = dir_fact * svol * ep2d->prev_gain[t];
      vend   = dir_fact * evol * gain;
      vstep  = (vend - vstart) / dno_samples;

//    DBG_MEMCLR(!offs, rbd->track[t], rbd->no_samples, sizeof(int32_t));
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
_aaxRingBufferMixMono16HRTF(_aaxRingBuffer *drb, int32_t **sptr, _aax2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   _aaxRingBufferData *drbi;
   _aaxRingBufferSample *rbd;
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   drbi = drb->handle;
   rbd = drbi->sample;
   for (t=0; t<rbd->no_tracks; t++)
   {
      int32_t *track = (int32_t *)rbd->track[t];
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

         assert(diff < (int)rbd->dde_samples);
         assert(diff > -(int)dno_samples);
         diff = _MINMAX(diff, -(int)dno_samples, (int)rbd->dde_samples);
 
         v_start = vstart * hrtf_volume[j];
         v_step = 0.0f; // vstep * hrtf_volume[j];

//       DBG_MEMCLR(!offs, rbd->track[t], rbd->no_samples, sizeof(int32_t));
#if RB_FLOAT_DATA
         _batch_fmadd(dptr, ptr-diff, dno_samples, v_start, v_step);
#else
         _batch_imadd(dptr, ptr-diff, dno_samples, v_start, v_step);
#endif
      }
   }
}

