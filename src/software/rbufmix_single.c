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
                              _oalRingBuffer2dProps *p2d,
                              _oalRingBuffer2dProps *mix_p2d,
                              float gain, unsigned char ch, unsigned char ctr,
                              unsigned int n)
{
   unsigned int track, offs, dno_samples;
   _oalRingBufferLFOInfo *lfo;
   _oalRingBufferSample *rbd;
   float svol, evol;
   float pitch, max;
   int32_t **sptr;
   void *env;
   int ret = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(src != 0);
   assert(dest != 0);
   assert(src->sample != 0);
   assert(dest->sample != 0);
   assert(p2d != 0);

   /** Pitch */
   pitch = p2d->final.pitch; /* Doppler effect */
   pitch *= _EFFECT_GET(p2d, PITCH_EFFECT, AAX_PITCH);
   lfo = _EFFECT_GET_DATA(p2d, DYNAMIC_PITCH_EFFECT);
   if (lfo) {
      pitch *= lfo->get(lfo, NULL, 0, 0);
   }

   if (mix_p2d)
   {
      float lfo = mix_p2d->final.pitch_lfo-0.5f;
      pitch *= _EFFECT_GET(mix_p2d, PITCH_EFFECT, AAX_PITCH);
      pitch = 1.0f+((pitch-1.0f)*lfo);
   }

   env = _EFFECT_GET_DATA(p2d, TIMED_PITCH_EFFECT);
   pitch *= _oalRingBufferEnvelopeGet(env, src->stopped);

   max = _EFFECT_GET(p2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch, 0.0f, max);

   /** Resample */
   offs = 0;
   sptr = _aaxProcessMixer(dest, src, p2d, pitch, &offs, &dno_samples, ctr, n);
   if (sptr == NULL) {
      return ret;
   }

   /** Volume */
   env = _FILTER_GET_DATA(p2d, TIMED_GAIN_FILTER);
   if (src->playing == 0 && src->stopped == 1) {
      ret = -1;
   }
   else if (!env && src->stopped == 1)
   {
      p2d->delay_sec -= dest->sample->duration_sec;
      if (p2d->delay_sec <= 0.0f) {
         ret = -1;
      }
   }

   gain *= _oalRingBufferEnvelopeGet(env, src->stopped);
   if (gain < -1e-3f) {
      ret = -1;
   }

   max = mix_p2d->final.gain_lfo;
   gain *= p2d->final.gain;
   lfo = _FILTER_GET_DATA(p2d, DYNAMIC_GAIN_FILTER);
   if (lfo)
   {
      if (!lfo->envelope)
      {
         float g = lfo->get(lfo, sptr[ch]+offs, 0, dno_samples);
         if (lfo->inv) g = 1.0f/g;
      } else {
         max *= lfo->get(lfo, NULL, 0, 0);
      }
   }
   if (max != 1.0f) {
      gain *= 1.0f - max/2.0f;
   }

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
   for (track=0; track<rbd->no_tracks; track++)
   {
      int32_t *t = (int32_t *)rbd->track[track];
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
      dir_fact = _MIN(0.8776f + p2d->pos[track][DIR_RIGHT], 1.0f);
      ch_volume = gain * dir_fact;
      do
      {
         float vstart, vend, vstep;
         int32_t *ptr = sptr[ch]+offs;
         int32_t *dptr = t + offs;

         vstart = gain * svol * p2d->prev_gain[track];
         vend = gain * evol * ch_volume;
         vstep = (vend - vstart) / dno_samples;

         assert(dptr+dno_samples <= t+rbd->no_samples);
         if (dptr+dno_samples > t+rbd->no_samples)
             dno_samples = t+rbd->no_samples-dptr;

//       DBG_MEMCLR(!offs, rbd->track[track], rbd->no_samples, sizeof(int32_t));
         _batch_fmadd(dptr, ptr, dno_samples, vstart, vstep);
         p2d->prev_gain[track] = ch_volume;
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
                                _oalRingBuffer2dProps *p2d,
                                _oalRingBuffer2dProps *mix_p2d,
                                float gain, unsigned char ch, unsigned char ctr,
                                unsigned int n)
{
   unsigned int track, offs, dno_samples;
   _oalRingBufferLFOInfo *lfo;
   _oalRingBufferSample *rbd;
   float svol, evol;
   float pitch, max;
   int32_t **sptr;
   void *env;
   int ret = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(src != 0);
   assert(dest != 0);
   assert(src->sample != 0);
   assert(dest->sample != 0);
   assert(p2d != 0);

   /** Pitch */
   pitch = p2d->final.pitch; /* Doppler effect */
   pitch *= _EFFECT_GET(p2d, PITCH_EFFECT, AAX_PITCH);
   lfo = _EFFECT_GET_DATA(p2d, DYNAMIC_PITCH_EFFECT);
   if (lfo) {
      pitch *= lfo->get(lfo, NULL, 0, 0);
   }

   if (mix_p2d)
   {
      float lfo = mix_p2d->final.pitch_lfo-0.5f;
      pitch *= _EFFECT_GET(mix_p2d, PITCH_EFFECT, AAX_PITCH);
      pitch = 1.0f+((pitch-1.0f)*lfo);
   }

   env = _EFFECT_GET_DATA(p2d, TIMED_PITCH_EFFECT);
   pitch *= _oalRingBufferEnvelopeGet(env, src->stopped);

   max = _EFFECT_GET(p2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch, 0.0f, max);

   /** Resample */
   offs = 0;
   sptr = _aaxProcessMixer(dest, src, p2d, pitch, &offs, &dno_samples, ctr, n);
   if (sptr == NULL) {
      return ret;
   }

   /** Volume */
   env = _FILTER_GET_DATA(p2d, TIMED_GAIN_FILTER);
   if (!env && src->playing == 0 && src->stopped == 1) {
      ret = -1;
   }
   else if (!env && src->stopped == 1)
   {
      p2d->delay_sec -= dest->sample->duration_sec;
      if (p2d->delay_sec <= 0.0f) {
         ret = -1;
      }
   }

   gain *= _oalRingBufferEnvelopeGet(env, src->stopped);
   if (gain < -1e-3f) {
      ret = -1;
   }

   max = mix_p2d->final.gain_lfo;
   gain *= p2d->final.gain;
   lfo = _FILTER_GET_DATA(p2d, DYNAMIC_GAIN_FILTER);
   if (lfo)
   {
      if (!lfo->envelope) 
      {
         float g = lfo->get(lfo, sptr[ch]+offs, 0, dno_samples);
         if (lfo->inv) g = 1.0f/g;
      } else {
         max *= lfo->get(lfo, NULL, 0, 0);
      }
   }
   if (max != 1.0f) {
      gain *= 1.0f - max/2.0f;
   }

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
   for (track=0; track<rbd->no_tracks; track++)
   {
      int32_t *t = (int32_t *)rbd->track[track];
      float ch_volume, dir_fact;
      float hrtf_volume[3];

      /**
       * vertical positioning
       **/
#if 1
      dir_fact = p2d->pos[DIR_UPWD+3*track][0];
      hrtf_volume[DIR_UPWD] = gain * 0.25f;
#else
      dir_fact = (p2d->pos[DIR_UPWD+3*track][0]);
      hrtf_volume[DIR_UPWD] = gain * (-0.25 +  0.75f*dir_fact);
#endif

      /**
       * horizontal positioning, back-front
       **/
#if 1
      dir_fact = p2d->pos[DIR_BACK+3*track][0];
      hrtf_volume[DIR_BACK] = gain * (0.25f + 0.5f*dir_fact);
#else
      dir_fact = _MIN(0.33f - p2d->pos[DIR_BACK+3*track][0], 1.0f);
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
      dir_fact = _MIN(0.8776f + p2d->pos[track][DIR_RIGHT], 1.0f);
      ch_volume = gain * dir_fact;
      do
      {
         float vstart, vend, vstep;
         int32_t *ptr = sptr[ch]+offs;
         int32_t *dptr = t + offs;
         int j;

         vstart = gain * svol * p2d->prev_gain[track];
         vend = gain * evol * ch_volume;
         vstep = (vend - vstart) / dno_samples;

         assert(dptr+dno_samples <= t+rbd->no_samples);
         if (dptr+dno_samples > t+rbd->no_samples)
             dno_samples = t+rbd->no_samples-dptr;

//       DBG_MEMCLR(!offs, rbd->track[track], rbd->no_samples, sizeof(int32_t));
         _batch_fmadd(dptr, ptr, dno_samples, vstart, vstep);
         p2d->prev_gain[track] = ch_volume;

         for (j=1; j<3; j++) /* skip left-right delays */
         {
            int diff = (int)p2d->hrtf[track][j];
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
                               _oalRingBuffer2dProps *p2d,
                               _oalRingBuffer2dProps *mix_p2d,
                               float gain, unsigned char ch, unsigned char ctr,
                               unsigned int n)
{
   unsigned int track, offs, dno_samples;
   _oalRingBufferLFOInfo *lfo;
   _oalRingBufferSample *rbd;
   float svol, evol;
   float pitch, max;
   int32_t **sptr;
   void *env;
   int ret = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(src != 0);
   assert(dest != 0);
   assert(src->sample != 0);
   assert(dest->sample != 0);
   assert(p2d != 0);

   /** Pitch */
   pitch = p2d->final.pitch; /* Doppler effect */
   pitch *= _EFFECT_GET(p2d, PITCH_EFFECT, AAX_PITCH);
   lfo = _EFFECT_GET_DATA(p2d, DYNAMIC_PITCH_EFFECT);
   if (lfo) {
      pitch *= lfo->get(lfo, NULL, 0, 0);
   }

   if (mix_p2d)
   {
      float lfo = mix_p2d->final.pitch_lfo-0.5f;
      pitch *= _EFFECT_GET(mix_p2d, PITCH_EFFECT, AAX_PITCH);
      pitch = 1.0f+((pitch-1.0f)*lfo);
   }

   env = _EFFECT_GET_DATA(p2d, TIMED_PITCH_EFFECT);
   pitch *= _oalRingBufferEnvelopeGet(env, src->stopped);

   max = _EFFECT_GET(p2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch, 0.0f, max);

   /** Resample */
   offs = 0;
   sptr = _aaxProcessMixer(dest, src, p2d, pitch, &offs, &dno_samples, ctr, n);
   if (sptr == NULL) {
      return ret;
   }

   /** Volume */
   env = _FILTER_GET_DATA(p2d, TIMED_GAIN_FILTER);
   if (src->playing == 0 && src->stopped == 1) {
      ret = -1;
   }
   else if (!env && src->stopped == 1)
   {
      p2d->delay_sec -= dest->sample->duration_sec;
      if (p2d->delay_sec <= 0.0f) {
         ret = -1;
      }
   }

   gain *= _oalRingBufferEnvelopeGet(env, src->stopped);
   if (gain < -1e-3f) {
      ret = -1;
   }

   max = mix_p2d->final.gain_lfo;
   gain *= p2d->final.gain;
   lfo = _FILTER_GET_DATA(p2d, DYNAMIC_GAIN_FILTER);
   if (lfo)
   {
      if (!lfo->envelope) 
      {
         float g = lfo->get(lfo, sptr[ch]+offs, 0, dno_samples);
         if (lfo->inv) g = 1.0f/g;
      } else {
         max *= lfo->get(lfo, NULL, 0, 0);
      }
   }
   if (max != 1.0f) {
      gain *= 1.0f - max/2.0f;
   }

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
   for (track=0; track<rbd->no_tracks; track++)
   {
      int32_t *dptr = (int32_t *)rbd->track[track] + offs;
      float vstart, vend, vstep;
      float dir_fact;

      dir_fact = p2d->pos[track][DIR_RIGHT];
      vstart = svol * gain * dir_fact * p2d->prev_gain[track];
      vend   = evol * gain * dir_fact * gain;
      vstep  = (vend - vstart) / dno_samples;

//    DBG_MEMCLR(!offs, rbd->track[track], rbd->no_samples, sizeof(int32_t));
      _batch_fmadd(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);

      p2d->prev_gain[track] = gain;
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
#define IDT_FB_DEVIDER	(p2d->head[0] / p2d->head[1])
#define IDT_UD_DEVIDER	(p2d->head[0] / p2d->head[2])
#define IDT_UD_OFFSET	p2d->head[3]

int
_oalRingBufferMixMono16HRTF(_oalRingBuffer *dest, _oalRingBuffer *src,
                            _oalRingBuffer2dProps *p2d,
                            _oalRingBuffer2dProps *mix_p2d,
                            float gain, unsigned char ch, unsigned char ctr,
                            unsigned int n)
{
   unsigned int track, offs, dno_samples, ddesamps;
   _oalRingBufferLFOInfo *lfo;
   _oalRingBufferSample *rbd;
   float svol, evol;
   float pitch, max;
   int32_t **sptr;
   void *env;
   int ret = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(src != 0);
   assert(dest != 0);
   assert(src->sample != 0);
   assert(dest->sample != 0);
   assert(p2d != 0);

   /** Pitch */
   pitch = p2d->final.pitch; /* Doppler effect */
   pitch *= _EFFECT_GET(p2d, PITCH_EFFECT, AAX_PITCH);
   lfo = _EFFECT_GET_DATA(p2d, DYNAMIC_PITCH_EFFECT);
   if (lfo) {
      pitch *= lfo->get(lfo, NULL, 0, 0);
   }

   if (mix_p2d)
   {
      float lfo = mix_p2d->final.pitch_lfo-0.5f;
      pitch *= _EFFECT_GET(mix_p2d, PITCH_EFFECT, AAX_PITCH);
      pitch = 1.0f+((pitch-1.0f)*lfo);
   }

   env = _EFFECT_GET_DATA(p2d, TIMED_PITCH_EFFECT);
   pitch *= _oalRingBufferEnvelopeGet(env, src->stopped);

   max = _EFFECT_GET(p2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch, 0.0f, max);

   /** Resample */
   // make sure the returned buffer has at least ddesamps prior to sptr[track]
   rbd = dest->sample;
   offs = ddesamps = rbd->dde_samples;
   sptr = _aaxProcessMixer(dest, src, p2d, pitch, &offs, &dno_samples, ctr, n);
   if (sptr == NULL) {
      return ret;
   }

   /** Volume */
   env = _FILTER_GET_DATA(p2d, TIMED_GAIN_FILTER);
   if (src->playing == 0 && src->stopped == 1) {
      ret = -1;
   }
   else if (!env && src->stopped == 1)
   {
      p2d->delay_sec -= dest->sample->duration_sec;
      if (p2d->delay_sec <= 0.0f) {
         ret = -1;
      }
   }

   gain *= _oalRingBufferEnvelopeGet(env, src->stopped);
   if (gain < -1e-3f) {
      ret = -1;
   }

   max = mix_p2d->final.gain_lfo;
   gain *= p2d->final.gain;
   lfo = _FILTER_GET_DATA(p2d, DYNAMIC_GAIN_FILTER);
   if (lfo)
   {
      if (!lfo->envelope) 
      {
         float g = lfo->get(lfo, sptr[ch]+offs, 0, dno_samples);
         if (lfo->inv) g = 1.0f/g;
      } else {
         max *= lfo->get(lfo, NULL, 0, 0);
      }
   }
   if (max != 1.0f) {
      gain *= 1.0f - max/2.0f;
   }

   /** Automatic volume ramping to avoid clicking */
   svol = evol = 1.0f;
   if (!env && (src->playing == src->stopped))
   {
      svol = (src->stopped || offs) ? 1.0f : 0.0f;
      evol = (src->stopped) ? 0.0f : 1.0f;
      src->playing = !src->stopped;
   }

   for (track=0; track<rbd->no_tracks; track++)
   {
      int32_t *t = (int32_t *)rbd->track[track];
      float vstart, vend, dir_fact; //, vstep;
      int32_t *dptr, *ptr;
      float hrtf_volume[3];
      int j;

      vstart = gain * svol * p2d->prev_gain[track];
      vend = gain * evol * gain;
      p2d->prev_gain[track] = vend;

      /*
       * IID; Interaural Intensitive Differenc
       * TODO: Implement 'head shadow' _frequency_ filter
       */

      /**
       * horizontal positioning, left-right
       **/
      dir_fact = p2d->pos[DIR_RIGHT+3*track][0];
      hrtf_volume[DIR_RIGHT] = 0.5f + 0.75f*dir_fact*vend;
// printf("l-r: %i, volume %f, delay: %f, dir_fact: %f\n", track, hrtf_volume[DIR_RIGHT], p2d->hrtf[track][DIR_RIGHT]/48000.0, dir_fact);

      /**
       * vertical positioning
       **/
      dir_fact = (p2d->pos[DIR_UPWD+3*track][0]);
      hrtf_volume[DIR_UPWD] = (0.25f + dir_fact)*vend;
// printf("u-d: %i, volume %f, delay: %f, dir_fact: %f\n", track, hrtf_volume[DIR_UPWD], p2d->hrtf[track][DIR_UPWD]/48000.0, dir_fact);

      /**
       * horizontal positioning, back-front
       **/
      dir_fact = (p2d->pos[DIR_BACK+3*track][0]);
      hrtf_volume[DIR_BACK] = (0.25f + 0.5f*dir_fact)*vend;
// printf("f-b: %i, volume %f, delay: %f, dir_fact: %f\n", track, hrtf_volume[DIR_BACK], p2d->hrtf[track][DIR_BACK]/48000.0, dir_fact);

      dptr = t+offs;
      ptr = sptr[ch]+offs;
      // vstep = (vend - vstart) / dno_samples;
      for (j=0; j<3; j++)
      {
         int diff = (int)p2d->hrtf[track][j];
         float v_start, v_step;

         assert(diff < (int)ddesamps);
         assert(diff > -(int)dno_samples);
         diff = _MINMAX(diff, -(int)dno_samples, (int)rbd->dde_samples);
 
         v_start = vstart * hrtf_volume[j];
         v_step = 0.0f; // vstep * hrtf_volume[j];

//       DBG_MEMCLR(!offs, rbd->track[track], rbd->no_samples, sizeof(int32_t));
         _batch_fmadd(dptr, ptr-diff, dno_samples, v_start, v_step);
      }
   }

   return ret;
}

void
_oalRingBufferPrepare3d(_oalRingBuffer3dProps* sprops3d, _oalRingBuffer3dProps* fprops3d, const void* mix_info, const _oalRingBuffer2dProps* sprops2d, void* source)
{
   _oalRingBufferPitchShiftFunc* dopplerfn;
   _oalRingBufferDistFunc* distfn;
   _oalRingBuffer2dProps *eprops2d;
   _oalRingBuffer3dProps *eprops3d;
   const _aaxMixerInfo* info;
   _aaxEmitter *src;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(source);

   info = (const _aaxMixerInfo*)mix_info;
   src = (_aaxEmitter *)source;

   eprops3d = src->props3d;
   eprops2d = src->props2d;

   distfn = _FILTER_GET_DATA(eprops3d, DISTANCE_FILTER);
   dopplerfn = _EFFECT_GET_DATA(eprops3d, VELOCITY_EFFECT);
   assert(dopplerfn);
   assert(distfn);

   if (_PROP_MTX_HAS_CHANGED(eprops3d) || _PROP_MTX_HAS_CHANGED(sprops3d) ||
       (fprops3d && _PROP_MTX_HAS_CHANGED(fprops3d)))
   {
      mtx4_t mtx;
      vec4_t epos;
      float esv, ssv, ss;
      float dist, gain;

//    _PROP_PITCH_CLEAR_CHANGED(eprops3d);
//    _PROP_PITCH_CLEAR_CHANGED(sprops3d);

      if (fprops3d) /* frame */
      {
         mtx4_t fmatrix;

//       _PROP_PITCH_CLEAR_CHANGED(fprops3d);

         mtx4Mul(fmatrix, sprops3d->matrix, fprops3d->matrix);
         mtx4Mul(mtx, fmatrix, eprops3d->matrix);
#if 0
 printf("sensor:\t\t\t\tframe:\n");
 PRINT_MATRICES(sprops3d->matrix, fprops3d->matrix);
 printf("sensor-frame:\t\t\temitter:\n");
 PRINT_MATRICES(fmatrix, eprops3d->matrix);
 printf("modified frame-emitter:\n");
 PRINT_MATRIX(mtx);
#endif
      }
      else {
         mtx4Mul(mtx, sprops3d->matrix, eprops3d->matrix);
#if 0
 printf("sensor:\t\t\t\temitter:\n");
 PRINT_MATRICES(sprops3d->matrix, eprops3d->matrix);
 printf("modified emitter\n");
 PRINT_MATRIX(mtx);
#endif
      }
      dist = vec3Normalize(epos, mtx[LOCATION]);

      /* calculate the sound velocity inbetween the emitter and the sensor */
      esv = eprops3d->effect[VELOCITY_EFFECT].param[AAX_SOUND_VELOCITY];
      if (fprops3d) {
         ssv = fprops3d->effect[VELOCITY_EFFECT].param[AAX_SOUND_VELOCITY];
      } else {
         ssv = sprops3d->effect[VELOCITY_EFFECT].param[AAX_SOUND_VELOCITY];
      }
      ss = (esv+ssv) / 2.0f;

      /* distance delay effect */
//    if (!src->stopped)
      {
         if (_PROP_DISTDELAY_IS_DEFINED(eprops3d)
             && _PROP_MTX_HAS_CHANGED(eprops3d))
         {
            eprops2d->delay_sec = dist / ss;
         } else {
            eprops2d->delay_sec = 0.0f;
         }
      }

      /*
       * Doppler
       */
      eprops2d->final.pitch = 1.0f;
      if (dist > 1.0f)
      {
         float ve, vs, de, df;
         vec4_t sv, ev;

         /* align velocity vectors with the modified emitter position
          * relative to the sensor
          */
         vec4Matrix4(sv, sprops3d->velocity, sprops3d->matrix);
         if (fprops3d)
         {
            vec4_t fv;
            vec4Copy(fv, fprops3d->velocity);
            vec4Add(fv, eprops3d->velocity);
            vec4Matrix4(ev, fv, sprops3d->matrix);
            
         }
         else {
            vec4Matrix4(ev, eprops3d->velocity, sprops3d->matrix);
         }
         vs = vec3DotProduct(sv, epos);
         ve = vec3DotProduct(ev, epos);
         de = sprops3d->effect[VELOCITY_EFFECT].param[AAX_DOPPLER_FACTOR];
         df = dopplerfn(vs, ve, ss/de);

         eprops2d->final.pitch = df;

#if 0
         if (_PROP_DISTDELAY_IS_DEFINED(eprops3d))
         {
            float vd = _MAX((vs+ve-ss) / ss, 0.0f);
            eprops2d->delay_sec += vd;
         }
#endif
      }

      /*
       * Distance queues for every speaker (volume)
       */
      if (_PROP_MTX_HAS_CHANGED(eprops3d) || _PROP_MTX_HAS_CHANGED(sprops3d)
          || (fprops3d && _PROP_MTX_HAS_CHANGED(fprops3d)))
      {
         float dist_fact, cone_volume = 1.0f;
         float refdist, maxdist, rolloff;
         float gain_min, gain_max;
         unsigned int i;

         _PROP_MTX_CLEAR_CHANGED(eprops3d);
         _PROP_MTX_CLEAR_CHANGED(sprops3d);
         if (fprops3d) _PROP_MTX_CLEAR_CHANGED(fprops3d);

         refdist = _FILTER_GET3D(src, DISTANCE_FILTER, AAX_REF_DISTANCE);
         maxdist = _FILTER_GET3D(src, DISTANCE_FILTER, AAX_MAX_DISTANCE);
         rolloff = _FILTER_GET3D(src, DISTANCE_FILTER, AAX_ROLLOFF_FACTOR);
         dist_fact = _MIN(dist/refdist, 1.0f);

         switch (info->mode)
         {
         case AAX_MODE_WRITE_HRTF:
         {
            unsigned int t, tracks = info->no_tracks;
            for (t=0; t<tracks; t++)
            {
               for (i=0; i<3; i++)
               {
                  float dp = vec3DotProduct(sprops2d->pos[3*t+i], epos);
                  float offs, fact;

                  eprops2d->pos[3*t+i][0] = dp * dist_fact;  /* -1 .. +1 */

                  dp = 0.5f+dp/2.0f;  /* 0 .. +1 */
                  if (i == DIR_BACK) dp *= dp;
                  if (i == DIR_UPWD) dp = 0.25f*(5.0f*dp - dp*dp);

                  offs = info->hrtf[HRTF_OFFSET][i];
                  fact = info->hrtf[HRTF_FACTOR][i];
                  eprops2d->hrtf[t][i] = info->hrtf[HRTF_OFFSET][i];
                  eprops2d->hrtf[t][i] = _MAX(offs+dp*fact, 0.0f);
               }
            }
            break;
         }
         case AAX_MODE_WRITE_SPATIAL:
            for (i=0; i<info->no_tracks; i++)
            {
               float dp = vec3DotProduct(sprops2d->pos[i], epos);
               eprops2d->pos[i][0] = 0.5f + dp * dist_fact;
            }
            break;
         case AAX_MODE_WRITE_SURROUND:
         {
            unsigned int t, tracks = info->no_tracks;
            for (t=0; t<tracks; t++)
            {
               for (i=1; i<3; i++) /* skip left-right */
               {
                  float dp = vec3DotProduct(sprops2d->pos[3*t+i], epos);
                  float offs, fact;

                  dp = 0.5f+dp/2.0f;  /* 0 .. +1 */
                  offs = info->hrtf[HRTF_OFFSET][i];
                  fact = info->hrtf[HRTF_FACTOR][i];
                  eprops2d->hrtf[t][i] = info->hrtf[HRTF_OFFSET][i];
                  eprops2d->hrtf[t][i] = _MAX(offs+dp*fact, 0.0f);
               }
            }
            /* break not needed */
         }
         default: /* AAX_MODE_WRITE_STEREO */
            for (i=0; i<info->no_tracks; i++)
            {
               vec4Mulvec4(eprops2d->pos[i], sprops2d->pos[i], epos);
               vec4ScalarMul(eprops2d->pos[i], dist_fact);
            }
         }

         gain = _FILTER_GET2D(src, VOLUME_FILTER, AAX_GAIN);
         gain *= distfn(dist, refdist, maxdist, rolloff, ss, 1.0f);

         /*
          * audio cone recalculaion
          */
         if (_PROP_CONE_IS_DEFINED(eprops3d))
         {
            float inner_vec, tmp = -mtx[DIR_BACK][2];

            inner_vec = _FILTER_GET3D(src, ANGULAR_FILTER, AAX_INNER_ANGLE);
            if (tmp < inner_vec)
            {
               float outer_vec, outer_gain;
               outer_vec = _FILTER_GET3D(src, ANGULAR_FILTER, AAX_OUTER_ANGLE);
               outer_gain = _FILTER_GET3D(src, ANGULAR_FILTER, AAX_OUTER_GAIN);
               if (outer_vec < tmp)
               {
                  tmp -= inner_vec;
                  tmp *= (outer_gain - 1.0f);
                  tmp /= (outer_vec - inner_vec);
                  cone_volume = (1.0f + tmp);
               } else {
                  cone_volume = outer_gain;
               }
            }
         }

         gain_min = _FILTER_GET2D(src, VOLUME_FILTER, AAX_MIN_GAIN);
         gain_max = _FILTER_GET2D(src, VOLUME_FILTER, AAX_MAX_GAIN);
         gain *= cone_volume;

         eprops2d->final.gain = _MINMAX(gain, gain_min, gain_max);
         eprops2d->final.gain *= _FILTER_GET(sprops2d, VOLUME_FILTER, AAX_GAIN);
      }
   }
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
_oalRingBufferDopplerShift(float vs, float ve, float ss)
{
   float vss, ves;
   vss = ss - _MIN(vs, ss);
   ves = _MAX(ss - _MIN(ve, ss), 1.0f);
   return vss/ves;
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
