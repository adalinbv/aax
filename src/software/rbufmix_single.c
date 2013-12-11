/*
 * Copyright 2005-2013 by Erik Hofman.
 * Copyright 2009-2013 by Adalin B.V.
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

#include "cpu2d/arch_simd.h"
#include "ringbuffer.h"

typedef void
_aaxRingBufferMixFn(_aaxRingBuffer*, const int32_ptrptr, _aaxRingBuffer2dProps*, unsigned char, unsigned int, unsigned int, float, float, float);


/* Forward declartations */
static _aaxRingBufferDistFn _aaxRingBufferDistNone;
static _aaxRingBufferDistFn _aaxRingBufferDistInvExp;
static _aaxRingBufferPitchShiftFn _aaxRingBufferDopplerShift;

static _aaxRingBufferDistFn _aaxRingBufferALDistInv;
static _aaxRingBufferDistFn _aaxRingBufferALDistInvClamped;
static _aaxRingBufferDistFn _aaxRingBufferALDistLin;
static _aaxRingBufferDistFn _aaxRingBufferALDistLinClamped;
static _aaxRingBufferDistFn _aaxRingBufferALDistExp;
static _aaxRingBufferDistFn _aaxRingBufferALDistExpClamped;

static _aaxRingBufferMixFn _aaxRingBufferMixMono16Stereo;
static _aaxRingBufferMixFn _aaxRingBufferMixMono16Spatial;
static _aaxRingBufferMixFn _aaxRingBufferMixMono16Surround;
static _aaxRingBufferMixFn _aaxRingBufferMixMono16HRTF;


/**
 * Mix a single track source buffer into a multi track destination buffer.
 * The result will be a stereo-mixed multitrack recording.
 *
 * When the source buffer has more tracks than the destination buffer the
 * first remaining source track will be mixed with the first destination
 * buffer and so on.
 *
 * @param dest multi track destination buffer
 * @param src single track source buffer
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
_aaxRingBufferMixMono16(_aaxRingBuffer *dest, _aaxRingBuffer *src, enum aaxRenderMode mode, _aaxRingBuffer2dProps *ep2d, _aaxRingBuffer2dProps *fp2d, unsigned char ch, unsigned char ctr, unsigned int nbuf)
{
   unsigned int offs, dno_samples;
   _aaxRingBufferLFOInfo *lfo;
   float gain, svol, evol;
   float pitch, max;
   int32_t **sptr;
   void *env;
   int ret = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(src != 0);
   assert(dest != 0);
   assert(src->sample != 0);
   assert(dest->sample != 0);
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
   pitch *= _aaxRingBufferEnvelopeGet(env, src->stopped);

   max = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch, 0.0f, max);

   /** Resample */
   offs = (mode == AAX_MODE_WRITE_HRTF) ? dest->sample->dde_samples : 0;
   sptr = _aaxProcessMixer(dest, src, ep2d, pitch, &offs, &dno_samples, ctr, nbuf);
   if (sptr == NULL || dno_samples == 0)
   {
      if (src->playing == 0 && src->stopped == 1) {
         return -1;
      } else {
         return 0;
      }
   }

   /** Volume */
   env = _FILTER_GET_DATA(ep2d, TIMED_GAIN_FILTER);
   if (src->playing == 0 && src->stopped == 1)
   {
      /* the emitter was already flagged as stopped */
      ret = -1;
   }
   else if (!env && src->stopped == 1)
   {
      /*
       * Distance delay induced stopping of playback
       * In the event that distance delay is not active dist_delay_sec equals
       * to 0 so detracting duration_sec instantly turns dist_delay_sec < 0.0
       */
      ep2d->dist_delay_sec -= dest->sample->duration_sec;
      if (ep2d->dist_delay_sec <= 0.0f) {
         ret = -1;
      }
   }

   /* apply envelope filter */
   gain = _aaxRingBufferEnvelopeGet(env, src->stopped);
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
   if (!env && (src->playing == src->stopped))
   {
      svol = (src->stopped || offs) ? 1.0f : 0.0f;
      evol = (src->stopped) ? 0.0f : 1.0f;
      src->playing = !src->stopped;
   }

   /* Mix */
   switch(mode)
   {
   case AAX_MODE_WRITE_SPATIAL:
      _aaxRingBufferMixMono16Spatial(dest, sptr, ep2d,
                                       ch, offs, dno_samples, gain, svol, evol);
      break;
   case AAX_MODE_WRITE_SURROUND:
      _aaxRingBufferMixMono16Surround(dest, sptr, ep2d,
                                       ch, offs, dno_samples, gain, svol, evol);
      break;
   case AAX_MODE_WRITE_HRTF:
      _aaxRingBufferMixMono16HRTF(dest, sptr, ep2d,
                                       ch, offs, dno_samples, gain, svol, evol);
      break;
   case AAX_MODE_WRITE_STEREO:
   default:
      _aaxRingBufferMixMono16Stereo(dest, sptr, ep2d,
                                       ch, offs, dno_samples, gain, svol, evol);
      break;
   }

   return ret;
}

/* -------------------------------------------------------------------------- */

_aaxRingBufferDistFn *_aaxRingBufferDistanceFn[AAX_DISTANCE_MODEL_MAX] =
{
   (_aaxRingBufferDistFn *)&_aaxRingBufferDistNone,
   (_aaxRingBufferDistFn *)&_aaxRingBufferDistInvExp
};

#define AL_DISTANCE_MODEL_MAX AAX_AL_DISTANCE_MODEL_MAX-AAX_AL_INVERSE_DISTANCE
_aaxRingBufferDistFn *_aaxRingBufferALDistanceFn[AL_DISTANCE_MODEL_MAX] =
{
   (_aaxRingBufferDistFn *)&_aaxRingBufferALDistInv,
   (_aaxRingBufferDistFn *)&_aaxRingBufferALDistInvClamped,
   (_aaxRingBufferDistFn *)&_aaxRingBufferALDistLin,
   (_aaxRingBufferDistFn *)&_aaxRingBufferALDistLinClamped,
   (_aaxRingBufferDistFn *)&_aaxRingBufferALDistExp,
   (_aaxRingBufferDistFn *)&_aaxRingBufferALDistExpClamped
};

_aaxRingBufferPitchShiftFn *_aaxRingBufferDopplerFn[] =
{
   (_aaxRingBufferPitchShiftFn *)&_aaxRingBufferDopplerShift
};

static void
_aaxRingBufferMixMono16Stereo(_aaxRingBuffer *dest, const int32_ptrptr sptr, _aaxRingBuffer2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   _aaxRingBufferSample *rbd;
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   /** Mix */
   rbd = dest->sample;
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
      _batch_fmadd(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);

      ep2d->prev_gain[t] = gain;
   }
}

static void
_aaxRingBufferMixMono16Surround(_aaxRingBuffer *dest, const int32_ptrptr sptr, _aaxRingBuffer2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   _aaxRingBufferSample *rbd;
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   /** Mix */
   rbd = dest->sample;
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
      _batch_fmadd(dptr, sptr[ch]+offs, dno_samples, dir_fact*vstart, vstep);

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
         _batch_fmadd(dptr, sptr[ch]+offs-diff, dno_samples, v_start, v_step);
      }
   }
}

static void
_aaxRingBufferMixMono16Spatial(_aaxRingBuffer *dest, int32_t **sptr, _aaxRingBuffer2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   _aaxRingBufferSample *rbd;
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   /** Mix */
   rbd = dest->sample;
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
      _batch_fmadd(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);

      ep2d->prev_gain[t] = gain;
   }
}

/* the time delay from ear to ear, in seconds */
#define HEAD_DELAY	p2d->head[0]
#define IDT_FB_DEVIDER	(ep2d->head[0] / ep2d->head[1])
#define IDT_UD_DEVIDER	(ep2d->head[0] / ep2d->head[2])
#define IDT_UD_OFFSET	p2d->head[3]

static void
_aaxRingBufferMixMono16HRTF(_aaxRingBuffer *dest, int32_t **sptr, _aaxRingBuffer2dProps *ep2d, unsigned char ch, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   _aaxRingBufferSample *rbd;
   unsigned int t;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   rbd = dest->sample;
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
         _batch_fmadd(dptr, ptr-diff, dno_samples, v_start, v_step);
      }
   }
}

static float
_aaxRingBufferDistNone(float dist, float ref_dist, float max_dist, float rolloff, float vsound, float Q)
{
   return 1.0f;
}


/**
 * http://www.engineeringtoolbox.com/outdoor-propagation-sound-d_64.html
 *
 * Lp = Lw + 10 log(Q/(4π r2) + 4/R)  (1b)
 *
 * where
 *
 * Lp = sound pressure level (dB)
 * Lw = sound power level source in decibel (dB)
 * Q = Q coefficient 
 *     1 if uniform spherical
 *     2 if uniform half spherical (single reflecting surface)
 *     4 if uniform radiation over 1/4 sphere (two reflecting surfaces, corner)
 * r = distance from source   (m)
 * R = room constant (m2)
 */
static float
_aaxRingBufferDistInvExp(float dist, float ref_dist, float max_dist, float rolloff, float vsound, float Q)
{
#if 1
   float fraction = 0.0f, gain = 1.0f;
   if (ref_dist) fraction = _MAX(dist, 0.01f) / _MAX(ref_dist, 0.01f);
   if (fraction) gain = powf(fraction, -rolloff);
   return gain;
#else
   return powf(dist/ref_dist, -rolloff);
#endif
}

static float
_aaxRingBufferDopplerShift(float vs, float ve, float vsound)
{
#if 1
   float vse, rv;

   /* relative speed */
   vse = _MIN(ve, vsound) - _MIN(vs, vsound);
   rv =  vsound/_MAX(vsound - vse, 1.0f);

   return rv;
#else
   float vss, ves;
   vss = vsound - _MIN(vs, vsound);
   ves = _MAX(vsound - _MIN(ve, vsound), 1.0f);
   return vss/ves;
#endif
}


/* --- OpenAL support --- */

static float
_aaxRingBufferALDistInv(float dist, float ref_dist, float max_dist, float rolloff, float vsound, float Q)
{
   float gain = 1.0f;
   float denom = ref_dist + rolloff * (dist - ref_dist);
   if (denom) gain = ref_dist/denom;
   return gain;
}

static float
_aaxRingBufferALDistInvClamped(float dist, float ref_dist, float max_dist, float rolloff, float vsound, float Q)
{
   float gain = 1.0f;
   float denom;
   dist = _MAX(dist, ref_dist);
   dist = _MIN(dist, max_dist);
   denom = ref_dist + rolloff * (dist - ref_dist);
   if (denom) gain = ref_dist/denom;
   return gain;
}

static float
_aaxRingBufferALDistLin(float dist, float ref_dist, float max_dist, float rolloff, float vsound, float Q)
{
   float gain = 1.0f;
   float denom = max_dist - ref_dist;
   if (denom) gain = (1-rolloff)*(dist-ref_dist)/denom;
   return gain;
}

static float
_aaxRingBufferALDistLinClamped(float dist, float ref_dist, float max_dist, float rolloff, float vsound, float Q)
{
   float gain = 1.0f;
   float denom = max_dist - ref_dist;
   dist = _MAX(dist, ref_dist);
   dist = _MIN(dist, max_dist);
   if (denom) gain = (1-rolloff)*(dist-ref_dist)/denom;
   return gain;
}

static float
_aaxRingBufferALDistExp(float dist, float ref_dist, float max_dist, float rolloff, float vsound, float Q)
{
   float fraction = 0.0f, gain = 1.0f;
   if (ref_dist) fraction = dist / ref_dist;
   if (fraction) gain = powf(fraction, -rolloff);
   return gain;
}

static float
_aaxRingBufferALDistExpClamped(float dist, float ref_dist, float max_dist, float rolloff, float vsound, float Q)
{
   float fraction = 0.0f, gain = 1.0f;

   dist = _MAX(dist, ref_dist);
   dist = _MIN(dist, max_dist);
   if (ref_dist) fraction = dist / ref_dist;
   if (fraction) gain = powf(fraction, -rolloff);

   return gain;
}
