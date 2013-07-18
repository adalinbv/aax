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
#include <ringbuffer.h>
#include <base/geometry.h>
#include <base/logging.h>

#include "arch_simd.h"


/* Forward declartations */
static _oalRingBufferMix1NFunc _oalRingBufferMixMono16HRTF;
static _oalRingBufferMix1NFunc _oalRingBufferMixMono16Stereo;
static _oalRingBufferMix1NFunc _oalRingBufferMixMono16Spatial;
static _oalRingBufferMix1NFunc _oalRingBufferMixMono16Surround;

static _oalRingBufferDistFunc _oalRingBufferDistNone;
static _oalRingBufferDistFunc _oalRingBufferDistInvExp;
static _oalRingBufferPitchShiftFunc _oalRingBufferDopplerShift;

static _oalRingBufferDistFunc _oalRingBufferALDistInv;
static _oalRingBufferDistFunc _oalRingBufferALDistInvClamped;
static _oalRingBufferDistFunc _oalRingBufferALDistLin;
static _oalRingBufferDistFunc _oalRingBufferALDistLinClamped;
static _oalRingBufferDistFunc _oalRingBufferALDistExp;
static _oalRingBufferDistFunc _oalRingBufferALDistExpClamped;

_oalRingBufferMix1NFunc *__renderer[AAX_MODE_WRITE_MAX] =
{
   0,					/* capture  */
   _oalRingBufferMixMono16Stereo,	/* stereo   */
   _oalRingBufferMixMono16Spatial,	/* spatial  */
   _oalRingBufferMixMono16Surround,	/* surround */
   _oalRingBufferMixMono16HRTF		/* hrtf     */
};

_oalRingBufferMix1NFunc*
_oalRingBufferMixMonoGetRenderer(enum aaxRenderMode mode)
{
    assert(mode >= 0 && mode < AAX_MODE_WRITE_MAX);
   return __renderer[mode];
}

/**
 * Mix a single track source buffer into a multi track destination buffer.
 * The result will be a stereo-mixed multitrack recording.
 *
 * When the source buffer has more tracks than the destination buffer the
 * first remaining source track will be mixed with the first destination
 * buffer and so on.
 *
 * @dest multi track destination buffer
 * @src single track source buffer
 * @p2d 3d positioning information structure
 */
int
_oalRingBufferMixMono16Stereo(_oalRingBuffer *dest, _oalRingBuffer *src,
                              _oalRingBuffer2dProps *ep2d,
                              _oalRingBuffer2dProps *fp2d,
                              unsigned char ch, unsigned char ctr,
                              unsigned int n)
{
   unsigned int t, offs, dno_samples;
   _oalRingBufferLFOInfo *lfo;
   _oalRingBufferSample *rbd;
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
   pitch *= _oalRingBufferEnvelopeGet(env, src->stopped);

   max = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch, 0.0f, max);

   /** Resample */
   offs = 0;
   sptr = _aaxProcessMixer(dest, src, ep2d, pitch, &offs, &dno_samples, ctr, n);
   if (sptr == NULL || dno_samples == 0) {
      return ret;
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
   gain = _oalRingBufferEnvelopeGet(env, src->stopped);
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

   /** Mix */
   rbd = dest->sample;
   for (t=0; t<rbd->no_tracks; t++)
   {
      int32_t *track = (int32_t *)rbd->track[t];
      float ch_volume, dir_fact;

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
      dir_fact = _MIN(0.8776f + ep2d->pos[t][DIR_RIGHT], 1.0f);
      ch_volume = gain * dir_fact;
      do
      {
         float vstart, vend, vstep;
         int32_t *ptr = sptr[ch]+offs;
         int32_t *dptr = track + offs;

         vstart = gain * svol * ep2d->prev_gain[t];
         vend = gain * evol * ch_volume;
         vstep = (vend - vstart) / dno_samples;

         assert(dptr+dno_samples <= track+rbd->no_samples);
         if (dptr+dno_samples > track+rbd->no_samples)
             dno_samples = track+rbd->no_samples-dptr;

//       DBG_MEMCLR(!offs, rbd->track[track], rbd->no_samples, sizeof(int32_t));
         _batch_fmadd(dptr, ptr, dno_samples, vstart, vstep);
         ep2d->prev_gain[t] = ch_volume;
      }
      while (0);
   }

   return ret;
}

/**
 * Mix a single track source buffer into a multi track destination buffer.
 * The result will be a 3d surround sound stereo-mixed multitrack recording.
 *
 * When the source buffer has more tracks than the destination buffer the
 * first remaining source track will be mixed with the first destination
 * buffer and so on.
 *
 * @dest multi track destination buffer
 * @src single track source buffer
 * @p2d 3d positioning information structure
 */
int
_oalRingBufferMixMono16Surround(_oalRingBuffer *dest, _oalRingBuffer *src,
                                _oalRingBuffer2dProps *ep2d,
                                _oalRingBuffer2dProps *fp2d,
                                unsigned char ch, unsigned char ctr,
                                unsigned int n)
{
   unsigned int t, offs, dno_samples;
   _oalRingBufferLFOInfo *lfo;
   _oalRingBufferSample *rbd;
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
   pitch *= _oalRingBufferEnvelopeGet(env, src->stopped);

   max = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch, 0.0f, max);

   /** Resample */
   offs = 0;
   sptr = _aaxProcessMixer(dest, src, ep2d, pitch, &offs, &dno_samples, ctr, n);
   if (sptr == NULL) {
      return ret;
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
   gain = _oalRingBufferEnvelopeGet(env, src->stopped);
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

   /** Mix */
   rbd = dest->sample;
   for (t=0; t<rbd->no_tracks; t++)
   {
      int32_t *track = (int32_t *)rbd->track[t];
      float ch_volume, dir_fact;
      float hrtf_volume[3];

      /**
       * vertical positioning
       **/
#if 1
      dir_fact = ep2d->pos[DIR_UPWD+3*t][0];
      hrtf_volume[DIR_UPWD] = gain * 0.25f;
#else
      dir_fact = (ep2d->pos[DIR_UPWD+3*t][0]);
      hrtf_volume[DIR_UPWD] = gain * (-0.25 +  0.75f*dir_fact);
#endif

      /**
       * horizontal positioning, back-front
       **/
#if 1
      dir_fact = ep2d->pos[DIR_BACK+3*t][0];
      hrtf_volume[DIR_BACK] = gain * (0.25f + 0.5f*dir_fact);
#else
      dir_fact = _MIN(0.33f - ep2d->pos[DIR_BACK+3*t][0], 1.0f);
      hrtf_volume[DIR_BACK] = gain * dir_fact;
#endif

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
      dir_fact = _MIN(0.8776f + ep2d->pos[t][DIR_RIGHT], 1.0f);
      ch_volume = gain * dir_fact;
      do
      {
         int32_t *ptr = sptr[ch]+offs;
         int32_t *dptr = track + offs;
         float vstart, vend, vstep;
         int j;

         vstart = gain * svol * ep2d->prev_gain[t];
         vend = gain * evol * ch_volume;
         vstep = (vend - vstart) / dno_samples;

         assert(dptr+dno_samples <= track+rbd->no_samples);
         if (dptr+dno_samples > track+rbd->no_samples)
             dno_samples = track+rbd->no_samples-dptr;

//       DBG_MEMCLR(!offs, rbd->track[track], rbd->no_samples, sizeof(int32_t));
         _batch_fmadd(dptr, ptr, dno_samples, vstart, vstep);
         ep2d->prev_gain[t] = ch_volume;

         for (j=1; j<3; j++) /* skip left-right delays */
         {
            int diff = (int)ep2d->hrtf[t][j];
            float v_start, v_step;

            assert(diff < (int)rbd->dde_samples);
            assert(diff > -(int)dno_samples);
            diff = _MINMAX(diff, -(int)dno_samples, (int)rbd->dde_samples);

            v_start = vstart * hrtf_volume[j];
            v_step = 0.0f; // vstep * hrtf_volume[j];
            _batch_fmadd(dptr, ptr-diff, dno_samples, v_start, v_step);
         }
      }
      while (0);
   }

   return ret;
}

/**
 * Mix a single track source buffer into a multi track destination buffer.
 * The result will be a 3d spatialised surround sound multitrack recording.
 *
 * When the source buffer has more tracks than the destination buffer the
 * first remaining source track will be mixed with the first destination
 * buffer and so on.
 *
 * @dest multi track destination buffer
 * @src single track source buffer
 * @p2d 3d positioning information structure
 */
int
_oalRingBufferMixMono16Spatial(_oalRingBuffer *dest, _oalRingBuffer *src,
                               _oalRingBuffer2dProps *ep2d,
                               _oalRingBuffer2dProps *fp2d,
                               unsigned char ch, unsigned char ctr,
                               unsigned int n)
{
   unsigned int t, offs, dno_samples;
   _oalRingBufferLFOInfo *lfo;
   _oalRingBufferSample *rbd;
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
   pitch *= _oalRingBufferEnvelopeGet(env, src->stopped);

   max = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch, 0.0f, max);

   /** Resample */
   offs = 0;
   sptr = _aaxProcessMixer(dest, src, ep2d, pitch, &offs, &dno_samples, ctr, n);
   if (sptr == NULL) {
      return ret;
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
   gain = _oalRingBufferEnvelopeGet(env, src->stopped);
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

   /** Mix */
   rbd = dest->sample;
   for (t=0; t<rbd->no_tracks; t++)
   {
      int32_t *dptr = (int32_t *)rbd->track[t] + offs;
      float vstart, vend, vstep;
      float dir_fact;

      dir_fact = ep2d->pos[t][DIR_RIGHT];
      vstart = svol * gain * dir_fact * ep2d->prev_gain[t];
      vend   = evol * gain * dir_fact * gain;
      vstep  = (vend - vstart) / dno_samples;

//    DBG_MEMCLR(!offs, rbd->track[t], rbd->no_samples, sizeof(int32_t));
      _batch_fmadd(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);

      ep2d->prev_gain[t] = gain;
   }

   return ret;
}

/**
 * Mix a single track source buffer into a multi track destination buffer.
 * The result will be a 3d spatialized multitrack recording.
 *
 * When the source buffer has more tracks than the destination buffer the
 * first remaining source track will be mixed with the first destination
 * buffer and so on.
 *
 * @dest multi track destination buffer
 * @src single track source buffer
 * @p2d 3d positioning information structure
 */

/* the time delay from ear to ear, in seconds */
#define HEAD_DELAY	p2d->head[0]
#define IDT_FB_DEVIDER	(ep2d->head[0] / ep2d->head[1])
#define IDT_UD_DEVIDER	(ep2d->head[0] / ep2d->head[2])
#define IDT_UD_OFFSET	p2d->head[3]

int
_oalRingBufferMixMono16HRTF(_oalRingBuffer *dest, _oalRingBuffer *src,
                            _oalRingBuffer2dProps *ep2d,
                            _oalRingBuffer2dProps *fp2d,
                            unsigned char ch, unsigned char ctr,
                            unsigned int n)
{
   unsigned int t, offs, dno_samples, ddesamps;
   _oalRingBufferLFOInfo *lfo;
   _oalRingBufferSample *rbd;
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
   pitch *= _oalRingBufferEnvelopeGet(env, src->stopped);

   max = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch, 0.0f, max);

   /** Resample */
   // make sure the returned buffer has at least ddesamps prior to sptr[t]
   rbd = dest->sample;
   offs = ddesamps = rbd->dde_samples;
   sptr = _aaxProcessMixer(dest, src, ep2d, pitch, &offs, &dno_samples, ctr, n);
   if (sptr == NULL) {
      return ret;
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
   gain = _oalRingBufferEnvelopeGet(env, src->stopped);
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

   for (t=0; t<rbd->no_tracks; t++)
   {
      int32_t *track = (int32_t *)rbd->track[t];
      float vstart, vend, dir_fact; //, vstep;
      int32_t *dptr, *ptr;
      float hrtf_volume[3];
      int j;

      vstart = gain * svol * ep2d->prev_gain[t];
      vend = gain * evol * gain;
      ep2d->prev_gain[t] = vend;

      /*
       * IID; Interaural Intensitive Differenc
       * TODO: Implement 'head shadow' _frequency_ filter
       */

      /**
       * horizontal positioning, left-right
       **/
      dir_fact = ep2d->pos[DIR_RIGHT+3*t][0];
      hrtf_volume[DIR_RIGHT] = 0.5f + 0.75f*dir_fact*vend;
// printf("l-r: %i, volume %f, delay: %f, dir_fact: %f\n", t, hrtf_volume[DIR_RIGHT], ep2d->hrtf[t][DIR_RIGHT]/48000.0, dir_fact);

      /**
       * vertical positioning
       **/
      dir_fact = (ep2d->pos[DIR_UPWD+3*t][0]);
      hrtf_volume[DIR_UPWD] = (0.25f + dir_fact)*vend;
// printf("u-d: %i, volume %f, delay: %f, dir_fact: %f\n", t, hrtf_volume[DIR_UPWD], ep2d->hrtf[t][DIR_UPWD]/48000.0, dir_fact);

      /**
       * horizontal positioning, back-front
       **/
      dir_fact = (ep2d->pos[DIR_BACK+3*t][0]);
      hrtf_volume[DIR_BACK] = (0.25f + 0.5f*dir_fact)*vend;
// printf("f-b: %i, volume %f, delay: %f, dir_fact: %f\n", t, hrtf_volume[DIR_BACK], ep2d->hrtf[t][DIR_BACK]/48000.0, dir_fact);

      dptr = track+offs;
      ptr = sptr[ch]+offs;
      // vstep = (vend - vstart) / dno_samples;
      for (j=0; j<3; j++)
      {
         int diff = (int)ep2d->hrtf[t][j];
         float v_start, v_step;

         assert(diff < (int)ddesamps);
         assert(diff > -(int)dno_samples);
         diff = _MINMAX(diff, -(int)dno_samples, (int)rbd->dde_samples);
 
         v_start = vstart * hrtf_volume[j];
         v_step = 0.0f; // vstep * hrtf_volume[j];

//       DBG_MEMCLR(!offs, rbd->track[t], rbd->no_samples, sizeof(int32_t));
         _batch_fmadd(dptr, ptr-diff, dno_samples, v_start, v_step);
      }
   }

   return ret;
}

/* -------------------------------------------------------------------------- */

_oalRingBufferDistFunc *_oalRingBufferDistanceFunc[AAX_DISTANCE_MODEL_MAX] =
{
   (_oalRingBufferDistFunc *)&_oalRingBufferDistNone,
   (_oalRingBufferDistFunc *)&_oalRingBufferDistInvExp
};

#define AL_DISTANCE_MODEL_MAX AAX_AL_DISTANCE_MODEL_MAX-AAX_AL_INVERSE_DISTANCE
_oalRingBufferDistFunc *_oalRingBufferALDistanceFunc[AL_DISTANCE_MODEL_MAX] =
{
   (_oalRingBufferDistFunc *)&_oalRingBufferALDistInv,
   (_oalRingBufferDistFunc *)&_oalRingBufferALDistInvClamped,
   (_oalRingBufferDistFunc *)&_oalRingBufferALDistLin,
   (_oalRingBufferDistFunc *)&_oalRingBufferALDistLinClamped,
   (_oalRingBufferDistFunc *)&_oalRingBufferALDistExp,
   (_oalRingBufferDistFunc *)&_oalRingBufferALDistExpClamped
};

_oalRingBufferPitchShiftFunc *_oalRingBufferDopplerFunc[] =
{
  (_oalRingBufferPitchShiftFunc *)&_oalRingBufferDopplerShift
};

static float
_oalRingBufferDistNone(float dist, float ref_dist, float max_dist, float rolloff, float sound_velocity, float directivity)
{
   return 1.0f;
}

static float
_oalRingBufferDistInvExp(float dist, float ref_dist, float max_dist, float rolloff, float sound_velocity, float directivity)
{
#if 0
   float p = 0.5f;
   float rv = 1.0f/(1.0f + pow(dist, p)); // p=2 equals natural decay */
   return rv;
#else
   float fraction = 0.0f, gain = 1.0f;
   if (ref_dist) fraction = dist / ref_dist;
   if (fraction) gain = powf(fraction, -rolloff);
   return gain;
#endif
}

static float
_oalRingBufferDopplerShift(float vs, float ve, float vsound)
{
   float vse, rv;

   /* relative speed */
   vse = _MIN(ve, vsound) - _MIN(vs, vsound);
   rv =  vsound/_MAX(vsound - vse, 1.0f);

   return rv;
}


/* --- OpenAL support --- */

static float
_oalRingBufferALDistInv(float dist, float ref_dist, float max_dist, float rolloff, float sound_velocity, float directivity)
{
   float gain = 1.0f;
   float denom = ref_dist + rolloff * (dist - ref_dist);
   if (denom) gain = ref_dist/denom;
   return gain;
}

static float
_oalRingBufferALDistInvClamped(float dist, float ref_dist, float max_dist, float rolloff, float sound_velocity, float directivity)
{
   float gain = 1.0f;
   float denom;
   dist = _MAX(dist, ref_dist);
   dist = _MIN(dist, max_dist);
   denom = ref_dist + rolloff * (dist - ref_dist);
   if (denom) gain =ref_dist/denom;

   return gain;
}

static float
_oalRingBufferALDistLin(float dist, float ref_dist, float max_dist, float rolloff, float sound_velocity, float directivity)
{
   float gain = 1.0f;
   float denom = max_dist - ref_dist;
   if (denom) gain = (1-rolloff)*(dist-ref_dist)/denom;
   return gain;
}

static float
_oalRingBufferALDistLinClamped(float dist, float ref_dist, float max_dist, float rolloff, float sound_velocity, float directivity)
{
   float gain = 1.0f;
   float denom = max_dist - ref_dist;
   dist = _MAX(dist, ref_dist);
   dist = _MIN(dist, max_dist);
   if (denom) gain = (1-rolloff)*(dist-ref_dist)/denom;
   return gain;
}

static float
_oalRingBufferALDistExp(float dist, float ref_dist, float max_dist, float rolloff, float sound_velocity, float directivity)
{
   float fraction = 0.0f, gain = 1.0f;
   if (ref_dist) fraction = dist / ref_dist;
   if (fraction) gain = powf(fraction, -rolloff);
   return gain;
}

static float
_oalRingBufferALDistExpClamped(float dist, float ref_dist, float max_dist, float rolloff, float sound_velocity, float directivity)
{
   float fraction = 0.0f, gain = 1.0f;

   dist = _MAX(dist, ref_dist);
   dist = _MIN(dist, max_dist);
   if (ref_dist) fraction = dist / ref_dist;
   if (fraction) gain = powf(fraction, -rolloff);

   return gain;
}
