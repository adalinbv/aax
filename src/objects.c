/*
 * Copyright 2005-2023 by Erik Hofman.
 * Copyright 2009-2023 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_ASSERT_H
# include <assert.h>
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif
#include <stdio.h>		/* for NULL */
#include <xml.h>

#include <base/random.h>

#include <backends/software/device.h>
#include <dsp/filters.h>
#include <dsp/effects.h>
#include "objects.h"
#include "arch.h"
#include "api.h"

void
_aaxSetDefaultInfo(_aaxMixerInfo **inf, void *handle)
{
   char *env = getenv("AAX_RENDER_MODE");
   _aaxMixerInfo *info;
   unsigned int size;

   assert(inf);
   if (!*inf) {
      *inf = &__info;
   }
   info = *inf;

   size = 2*sizeof(vec4f_t);
   _aax_memcpy(&info->hrtf, &_aaxDefaultHead, size);

   size = _AAX_MAX_SPEAKERS * sizeof(vec4f_t);
   _aax_memcpy(&info->speaker, &_aaxDefaultSpeakersVolume, size);

   info->delay = &info->speaker[_AAX_MAX_SPEAKERS];
   _aax_memcpy(info->delay, &_aaxDefaultSpeakersDelay, size);

   size = _AAX_MAX_SPEAKERS-1;
   do {
      info->router[size] = size;
   } while (size--);

   info->no_tracks = 2;
   info->bitrate = 0; // variable bitrate
   info->track = AAX_TRACK_ALL;

   info->pitch = 1.0f;
   info->frequency = 48000.0f;
   info->period_rate = 20.0f;
   info->refresh_rate = 20.0f;
   info->format = AAX_PCM16S;
   info->mode = AAX_MODE_WRITE_STEREO;
   info->max_emitters = _AAX_MAX_MIXER_REGISTERED;
   info->max_registered = 0;
   info->unit_m = 1.0f;

   info->update_rate = 0;
   info->capabilities = _aaxGetCapabilities(NULL);

   if (env)
   {
       if (!strcasecmp(env, "synthesizer")) {
          info->midi_mode = AAX_RENDER_SYNTHESIZER;
       } else if (!strcasecmp(env, "arcade")) {
          info->midi_mode = AAX_RENDER_ARCADE;
       }
   }

   info->id = INFO_ID;
   info->backend = handle;
}

void
_aaxSetDefault2dFiltersEffects(_aax2dProps *p2d)
{
   unsigned int pos;

   /* Note: skips the volume filter */
   for (pos=DYNAMIC_GAIN_FILTER; pos<MAX_STEREO_FILTER; pos++) {
      _aaxSetDefaultFilter2d(&p2d->filter[pos], pos, 0);
   }

   /* Note: skips the pitch effect */
   for (pos=REVERB_EFFECT; pos<MAX_STEREO_EFFECT; pos++) {
      _aaxSetDefaultEffect2d(&p2d->effect[pos], pos, 0);
   }
}

void
_aaxSetDefault2dProps(_aax2dProps *p2d)
{
   unsigned int size;

   assert (p2d);

   p2d->mutex = _aaxMutexCreate(NULL);
   _aaxSetDefaultFilter2d(&p2d->filter[0], VOLUME_FILTER, 0);
   _aaxSetDefaultEffect2d(&p2d->effect[0], PITCH_EFFECT, 0);
   _aaxSetDefault2dFiltersEffects(p2d);

   /* normalized  directions */
   size = _AAX_MAX_SPEAKERS*sizeof(vec4f_t);
   memset(p2d->speaker, 0, 2*size);

   /* HRTF sample offsets */
   size = 2*sizeof(vec4f_t);
   memset(p2d->hrtf, 0, size);
   memset(p2d->hrtf_prev, 0, size);

   /* HRTF head shadow */
   size = _AAX_MAX_SPEAKERS*sizeof(float);
   memset(p2d->freqfilter_history, 0, size);
   p2d->k = 0.0f;

   /* previous gains */
   size = _AAX_MAX_SPEAKERS*sizeof(float);
   memset(&p2d->prev_gain, 0, size);
   p2d->prev_freq_fact = 0.0f;
   p2d->dist_delay_sec = 0.0f;
   p2d->bufpos3dq = 0.0f;

   p2d->curr_pos_sec = 0.0f;
   p2d->pitch_factor = 1.0f;

   p2d->note.velocity = 1.0f;		/* MIDI */
   p2d->note.release = 1.0f;
   p2d->note.pressure = 1.0f;
   p2d->note.soft = 1.0f;

   p2d->final.pitch_lfo = 1.0f;		/* LFO */
   p2d->final.pitch = 1.0f;
   p2d->final.gain_lfo = 1.0f;
   p2d->final.gain = 1.0f;
}

void
_aaxSetDefaultDelayed3dProps(_aaxDelayed3dProps *dp3d)
{
   assert(dp3d);

   /* modelview matrix */
#ifdef ARCH32
   mtx4fSetIdentity(dp3d->matrix.m4);
   mtx4fSetIdentity(dp3d->imatrix.m4);
#else
   mtx4dSetIdentity(dp3d->matrix.m4);
   mtx4dSetIdentity(dp3d->imatrix.m4);
#endif
   _PROP3D_MTX_SET_CHANGED(dp3d);

   /* velocity     */
   mtx4fSetIdentity(dp3d->velocity.m4);
   _PROP3D_SPEED_SET_CHANGED(dp3d);

   /* occlusion */
   memset(dp3d->occlusion.v4, 0, sizeof(vec4f_t));

   /* status */
   dp3d->state3d = 0;
   dp3d->pitch = 1.0f;
   dp3d->gain = 1.0f;
}

_aax3dProps *
_aax3dPropsCreate()
{
   _aax3dProps *rv = NULL;
   char *ptr1, *ptr2;
   size_t offs, size;

   offs = sizeof(_aax3dProps);
   size = sizeof(_aaxDelayed3dProps);
   ptr1 = _aax_calloc(&ptr2, offs, 1, size);
   if (ptr1)
   {
      unsigned int pos;

      rv = (_aax3dProps*)ptr1;
      rv->m_dprops3d = (_aaxDelayed3dProps*)ptr2;

      rv->dprops3d = _aax_aligned_alloc(sizeof(_aaxDelayed3dProps));
      if (rv->dprops3d)
      {
         _SET_INITIAL(rv);

         _aaxSetDefaultDelayed3dProps(rv->dprops3d);
         _aaxSetDefaultDelayed3dProps(rv->m_dprops3d);

         rv->mutex = _aaxMutexCreate(NULL);
         for (pos=0; pos<MAX_3D_FILTER; pos++) {
            _aaxSetDefaultFilter3d(&rv->filter[pos], pos, 0);
         }
         for (pos=0; pos<MAX_3D_EFFECT; pos++) {
            _aaxSetDefaultEffect3d(&rv->effect[pos], pos, 0);
         }
      }
      else
      {
         free(rv);
         rv = NULL;
      }
   }
   return rv;
}

_aaxDelayed3dProps *
_aaxDelayed3dPropsDup(_aaxDelayed3dProps *dp3d)
{
   _aaxDelayed3dProps *rv;

   rv = _aax_aligned_alloc(sizeof(_aaxDelayed3dProps));
   if (rv) {
      _aax_memcpy(rv, dp3d, sizeof(_aaxDelayed3dProps));
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

/* HRTF
 *
 * Left-right time difference (delay):
 * Angle from ahead (azimuth, front = 0deg): (ear-distance)
 *     0 deg =  0.00 ms,                                -- ahead      --
 *    90 deg =  0.64 ms,				-- right/left --
 *   180 deg =  0.00 ms 				-- back       --
 *
 * The outer pinna rim which is important in determining elevation
 * in the vertical plane:
 * Angle from above (0deg = below, 180deg = above): (outer pinna rim)
 *     0 deg = 0.325 ms,                                -- below  --
 *    90 deg = 0.175 ms,                                -- center --
 *   180 deg = 0.100 ms                                 -- above  --
 *
 * The inner pinna ridge which determine front-back directions in the
 * horizontal plane. Front-back distinctions are not uniquely determined
 * by time differences:
 * Angle from right (azimuth, front = 0deg): (inner pinna ridge)
 *     0 deg =  0.080 ms,				-- ahead  --
 *    90 deg =  0.015 ms,				-- center --
 *   135+deg =  0.000 ms				-- back   --
 *
 *        RIGHT   UP        BACK
 * ahead: 0.00ms, 0.175 ms, 0.080 ms
 * left:  0.64ms, 0.175 ms, 0.015 ms
 * right: 0.64ms, 0.175 ms, 0.015 ms
 * back:  0.00ms, 0.175 ms, 0.000 ms
 * up:    0.00ms, 0.100 ms, 0.015 ms
 * down:  0.00ms, 0.325 ms, 0.015 ms
 */
float _aaxDefaultHead[2][4] =
{
//     RIGHT     UP        BACK
   { 0.000120f, 0.000125f, 0.000640f, 0.0f },   /* head delay factors */
   {-0.000030f, 0.000225f, 0.000000f, 0.0f }    /* head delay offsets */
};

float _aaxDefaultHRTFVolume[_AAX_MAX_SPEAKERS][4] =
{
   /* left headphone shell (volume)                          --- */
   { 1.00f, 0.00f, 0.00f, 1.0f }, 	 /* left-right           */
   { 0.00f,-1.00f, 0.00f, 1.0f }, 	 /* up-down              */
   { 0.00f, 0.00f, 1.00f, 1.0f }, 	 /* back-front           */
   /* right headphone shell (volume)                         --- */
   {-1.00f, 0.00f, 0.00f, 1.0f }, 	 /* left-right           */
   { 0.00f,-1.00f, 0.00f, 1.0f }, 	 /* up-down              */
   { 0.00f, 0.00f, 1.00f, 1.0f }, 	 /* back-front           */
   /* unused                                                     */
   { 0.00f, 0.00f, 0.00f, 0.0f },
   { 0.00f, 0.00f, 0.000, 0.0f }
};

float _aaxDefaultHRTFDelay[_AAX_MAX_SPEAKERS][4] =
{
   /* left headphone shell (delay)                           --- */
   {-1.00f, 0.00f, 0.00f, 0.0f },        /* left-right           */
   { 0.00f,-1.00f, 0.00f, 0.0f },        /* up-down              */
   { 0.00f, 0.00f, 1.00f, 0.0f },        /* back-front           */
   /* right headphone shell (delay)                          --- */
   { 1.00f, 0.00f, 0.00f, 0.0f },        /* left-right           */
   { 0.00f,-1.00f, 0.00f, 0.0f },        /* up-down              */
   { 0.00f, 0.00f, 1.00f, 0.0f },        /* back-front           */
   /* unused                                                     */
   { 0.00f, 0.00f, 0.00f, 0.0f },
   { 0.00f, 0.00f, 0.000, 0.0f }
};

float _aaxDefaultSpeakersVolume[_AAX_MAX_SPEAKERS][4] =
{
   { 1.00f, 0.00f, 1.00f, 1.0f },	/* front left speaker    */
   {-1.00f, 0.00f, 1.00f, 1.0f },	/* front right speaker   */
   { 1.00f, 0.00f,-1.00f, 1.0f },	/* rear left speaker     */
   {-1.00f, 0.00f,-1.00f, 1.0f },	/* rear right speaker    */
   { 0.00f, 0.00f, 1.00f, 1.0f },	/* front center speaker  */
   { 0.00f, 0.00f, 1.00f, 1.0f },	/* low frequency emitter */
   { 1.00f, 0.00f, 0.00f, 1.0f },	/* left side speaker     */
   {-1.00f, 0.00f, 0.00f, 1.0f }	/* right side speaker    */
};

float _aaxDefaultSpeakersDelay[_AAX_MAX_SPEAKERS][4] =
{
   { 0.00f, 0.00f, 0.00f, 1.0f },       /* front left speaker    */
   { 0.00f, 0.00f, 0.00f, 1.0f },       /* front right speaker   */
   { 0.00f, 0.00f, 0.00f, 1.0f },       /* rear left speaker     */
   { 0.00f, 0.00f, 0.00f, 1.0f },       /* rear right speaker    */
   { 0.00f, 0.00f, 0.00f, 1.0f },       /* front center speaker  */
   { 0.00f, 0.00f, 0.00f, 1.0f },       /* low frequency emitter */
   { 0.00f, 0.00f, 0.00f, 1.0f },       /* left side speaker     */
   { 0.00f, 0.00f, 0.00f, 1.0f }        /* right side speaker    */
};

unsigned int
_aaxGetNoEmitters(const _aaxDriverBackend *be) {
   if (!be) be = &_aaxLoopbackDriverBackend;
   return _MAX(be->getset_sources(0, 0), _AAX_MAX_MIXER_REGISTERED);
}

unsigned int
_aaxSetNoEmitters(const _aaxDriverBackend *be, unsigned int max) {
   if (!be) be = &_aaxLoopbackDriverBackend;
   return be->getset_sources(max, 0);
}

unsigned int
_aaxIncreaseEmitterCounter(const _aaxDriverBackend *be) {
   if (!be) be = &_aaxLoopbackDriverBackend;
   return be->getset_sources(0, 1);
}

unsigned int
_aaxDecreaseEmitterCounter(const _aaxDriverBackend *be) {
   if (!be) be = &_aaxLoopbackDriverBackend;
   return be->getset_sources(0, -1);
}

static void
_aaxSetSlotFromAAXSOld(const xmlId *xid, int (*setSlotFn)(void*, unsigned, int, aaxVec4f), void *id)
{
   unsigned int s, snum = xmlNodeGetNum(xid, "slot");
   xmlId *xsid;

   xsid = xmlMarkId(xid);
   if (!xsid) return;

   for (s=0; s<snum; s++)
   {
      if (xmlNodeGetPos(xid, xsid, "slot", s) != 0)
      {
         if (xmlNodeGetPos(xid, xsid, "slot", s) != 0)
         {
            enum aaxType type = AAX_LINEAR;
            aaxVec4f params;
            unsigned int slen;
            long int n = s;
            char src[65];

            if (xmlAttributeExists(xsid, "n")) {
               n = xmlAttributeGetInt(xsid, "n");
            }

            params[0] = xmlNodeGetDouble(xsid, "p0");
            params[1] = xmlNodeGetDouble(xsid, "p1");
            params[2] = xmlNodeGetDouble(xsid, "p2");
            params[3] = xmlNodeGetDouble(xsid, "p3");

            slen = xmlAttributeCopyString(xsid, "type", src, 64);
            if (slen)
            {
               src[slen] = 0;
               type = aaxGetTypeByName(src);
            }
            setSlotFn(id, n, type, params);
         }
      }
   }
   xmlFree(xsid);
}

static int
_aaxSetFilterSlotState(const aaxFilter f, int slot, int state)
{
   _filter_t* filter = get_filter(f);

   filter->slot[slot]->src = state;

   return AAX_TRUE;
}

static int
_aaxSetEffectSlotState(const aaxEffect e, int slot, int state)
{
   _effect_t* effect = get_effect(e);

   effect->slot[slot]->src = state;

   return AAX_TRUE;
}

static int
_aaxSetSlotFromAAXS(const xmlId *xid, int (*setStateFn)(void*, int, int), int (*setParamFn)(void*, int, int, float), void *id, float freq, float min, float max, _midi_t *midi)
{
   unsigned int s, snum = xmlNodeGetNum(xid, "slot");
   int rv = AAX_FALSE;
   xmlId *xsid;

   xsid = xmlMarkId(xid);
   if (!xsid) return rv;

   if (min != 0.0f && freq < min) freq = min;
   if (max != 0.0f && freq > max) freq = max;

   for (s=0; s<snum; s++)
   {
      if (xmlNodeGetPos(xid, xsid, "slot", s) != 0)
      {
         unsigned int p, pnum = xmlNodeGetNum(xsid, "param");
         unsigned int slen;
         char src[65];

         slen = xmlAttributeCopyString(xsid, "src", src, 64);
         if (slen)
         {
            int state = aaxGetFrequencyFilterTypeByName(src);
            setStateFn(id, s, state);
         }

         if (pnum)
         {
            enum aaxType type = AAX_LINEAR;
            long int sn = s;
            xmlId *xpid;

            if (xmlAttributeExists(xsid, "n")) {
               sn = xmlAttributeGetInt(xsid, "n");
            }

            slen = xmlAttributeCopyString(xsid, "type", src, 64);
            if (slen)
            {
               src[slen] = 0;
               type = aaxGetTypeByName(src);
            }

            xpid = xmlMarkId(xsid);
            if (xpid)
            {
               for (p=0; p<pnum; p++)
               {
                  if (xmlNodeGetPos(xsid, xpid, "param", p) != 0)
                  {
                     int slotnum[_MAX_FE_SLOTS] = { 0x00, 0x10, 0x20, 0x30 };
                     double value = xmlGetDouble(xpid);
                     long int pn = p;

                     if (xmlAttributeExists(xpid, "n")) {
                        pn = xmlAttributeGetInt(xpid, "n");
                     }

                     if (freq != 0.0f)
                     {
                        float pitch, adjust, random;

                        pitch = xmlAttributeGetDouble(xpid, "pitch");
                        if (pitch != 0.0f) {
                           value = pitch*freq;
                        }

                        random = xmlAttributeGetDouble(xpid, "random");
                        if (random != 0.0f) {
                           value += random*2.0f*(_aax_random()-0.5f);
                        }

                        adjust = xmlAttributeGetDouble(xpid, "auto");
                        if (adjust == 0.0f) {
                           adjust = xmlAttributeGetDouble(xpid, "auto-sustain");
                        }
                        if (adjust != 0.0f)
                        {
                           float min = xmlAttributeGetDouble(xpid, "min");
                           float max = xmlAttributeGetDouble(xpid, "max");
                           if (min <= 0.0f) min = 0.01f;
                           if (max <= min) max = FLT_MAX;
                           value=_MINMAX(value-adjust*_lin2log(freq), min, max);
                        }
                     }

                     slen = xmlAttributeCopyString(xpid, "type", src, 64);
                     if (slen == 0) {
                        type = AAX_LINEAR;
                     }
                     else
                     {
                        src[slen] = 0;
                        type = aaxGetTypeByName(src);
                     }

                     pn |= slotnum[sn];
                     if (midi)
                     {
                         if (pn ==  AAX_TIME0) value *= midi->attack_factor;
                         if (pn ==  AAX_TIME1) value *= midi->release_factor;
                     }
                     setParamFn(id, pn, type, value);
                  }
               }
               xmlFree(xpid);
            }
            rv = AAX_TRUE;
         }
      }
   }
   xmlFree(xsid);

   return rv;
}

aaxFilter
_aaxGetFilterFromAAXS(aaxConfig config, const xmlId *xid, float freq, float min, float max, _midi_t *midi)
{
   aaxFilter rv = NULL;
   char src[65];
   int slen;

   slen = xmlAttributeCopyString(xid, "type", src, 64);
   if (slen)
   {
      int state = AAX_CONSTANT_VALUE;
      enum aaxFilterType ftype;
      aaxFilter flt;

      src[slen] = 0;
      ftype = aaxFilterGetByName(config, src);
      // frequency filter and dynmaic gain filter are always supported
      if (midi && midi->mode != AAX_RENDER_NORMAL &&
          (ftype != AAX_FREQUENCY_FILTER && ftype != AAX_DYNAMIC_GAIN_FILTER))
      {
         // as is the timed-gain filter in synthesizer mode
         if (midi->mode == AAX_RENDER_SYNTHESIZER)
         {
            if (ftype != AAX_TIMED_GAIN_FILTER) {
               return rv;
            }
         }
         else { // AAX_RENDER_ARCADE
            return rv;
         }
      }

      if (ftype != AAX_TIMED_GAIN_FILTER) {
          midi = NULL;
      }
      flt = aaxFilterCreate(config, ftype);
      if (flt)
      {
         if (!_aaxSetSlotFromAAXS(xid, _aaxSetFilterSlotState, aaxFilterSetParam, flt, freq, min, max, midi)) {
            _aaxSetSlotFromAAXSOld(xid, aaxFilterSetSlotParams, flt);
         }

         if (ftype == AAX_TIMED_GAIN_FILTER)
         {
            float release_factor = midi ? midi->release_factor : 1.0f;
            int repeat = 0;

            if (xmlAttributeExists(xid, "repeat"))
            {
               state = 0;
               if (!xmlAttributeCompareString(xid, "repeat", "inf") ||
                   !xmlAttributeCompareString(xid, "repeat", "max"))
               {
                  repeat = AAX_MAX_REPEAT;
               }
               else
               {
                  repeat = xmlAttributeGetInt(xid, "repeat");
                  repeat = _MINMAX(repeat, 0, AAX_MAX_REPEAT);
               }
               repeat |= AAX_REPEAT;
            }
            else if (xmlAttributeExists(xid, "release-factor")) {
               release_factor *= xmlAttributeGetDouble(xid, "release-factor");
            }
            else if (xmlAttributeExists(xid, "release-time")) {
               float dt = 2.5f*xmlAttributeGetDouble(xid, "release-time");
               _handle_t *handle = get_driver_handle(config);
               if (handle)
               {
                  float period = handle->info->period_rate;
                  release_factor *= dt*period/86.132812f;
               }
            }

            if (release_factor != 1.0f)
            {
               state = _MAX(10.0f*release_factor, 1);
               if (state >= AAX_ENVELOPE_FOLLOW_LOG) {
                  state = AAX_ENVELOPE_FOLLOW_LOG-1;
               } else if (state < 1) {
                  state = 0;
               }
               state |= AAX_RELEASE_FACTOR;
            }
            slen = xmlAttributeCopyString(xid, "src", src, 64);
            if (slen)
            {
               int s = aaxGetWaveformTypeByName(src);
               if (s == AAX_INVERSE_ENVELOPE_FOLLOW) {
                  state |= AAX_INVERSE_ENVELOPE_FOLLOW;
               } else if (s == AAX_ENVELOPE_FOLLOW) {
                  state |= AAX_ENVELOPE_FOLLOW;
               } else {
                  state = s;
               }
            }
            state |= repeat;
         }
         else
         {
            slen = xmlAttributeCopyString(xid, "src", src, 64);
            if (slen)
            {
               src[slen] = 0;
               switch(ftype)
               {
               case AAX_DISTANCE_FILTER:
                  state = aaxGetDistanceModelByName(src);
                  break;
               case AAX_FREQUENCY_FILTER:
                  state = aaxGetFrequencyFilterTypeByName(src);
                  break;
               case AAX_EQUALIZER:
                  state = xmlAttributeGetBool(xid, "src");
                  break;
               default:
                  state = aaxGetWaveformTypeByName(src);
                  break;
               }
            }

            if (xmlAttributeExists(xid, "stereo") &&
                xmlAttributeGetBool(xid, "stereo")) {
               state |= AAX_LFO_STEREO;
            }
         }

         aaxFilterSetState(flt, state);
         rv = flt;
      }
   }

   return rv;
}

aaxEffect
_aaxGetEffectFromAAXS(aaxConfig config, const xmlId *xid, float freq, float min, float max, _midi_t *midi)
{
   aaxEffect rv = NULL;
   char src[65];
   int slen;

   slen = xmlAttributeCopyString(xid, "type", src, 64);
   if (slen)
   {
      enum aaxWaveformType state = AAX_CONSTANT_VALUE;
      enum aaxEffectType etype;
      aaxEffect eff;

      src[slen] = 0;
      etype = aaxEffectGetByName(config, src);
      // dynamic pitch effect is alwasy supported
      if (midi && midi->mode != AAX_RENDER_NORMAL &&
          etype != AAX_DYNAMIC_PITCH_EFFECT)
      {
         return rv;
      }

      if (etype != AAX_TIMED_PITCH_EFFECT) {
          midi = NULL;
      }
      eff = aaxEffectCreate(config, etype);
      if (eff)
      {
         if (!_aaxSetSlotFromAAXS(xid, _aaxSetEffectSlotState, aaxEffectSetParam, eff, freq, min, max, midi)) {
            _aaxSetSlotFromAAXSOld(xid, aaxEffectSetSlotParams, eff);
         }

         slen = xmlAttributeCopyString(xid, "src", src, 64);
         if (slen)
         {
            src[slen] = 0;
            state = aaxGetWaveformTypeByName(src);
         }

         if (xmlAttributeExists(xid, "stereo") &&
             xmlAttributeGetBool(xid, "stereo")) {
            state |= AAX_LFO_STEREO;
         }

         aaxEffectSetState(eff, state);
         rv = eff;
      }
   }

   return rv;
}
