/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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
#ifdef HAVE_LIBIO_H
# include <libio.h>		/* for NULL */
#endif
#include <math.h>		/* for MAXFLOAT */
#include <xml.h>

#include <software/gpu/gpu.h>
#include <dsp/filters.h>
#include <dsp/effects.h>
#include "objects.h"
#include "arch.h"
#include "api.h"

void
_aaxSetDefaultInfo(_aaxMixerInfo *info, void *handle)
{
// const char *opencl;
   unsigned int size;

   size = 2*sizeof(vec4f_t); 
   _aax_memcpy(&info->hrtf, &_aaxContextDefaultHead, size);

   size = _AAX_MAX_SPEAKERS * sizeof(vec4f_t);
   _aax_memcpy(&info->speaker, &_aaxContextDefaultSpeakersVolume, size);

   info->delay = &info->speaker[_AAX_MAX_SPEAKERS];
   _aax_memcpy(info->delay, &_aaxContextDefaultSpeakersDelay, size);

   size = _AAX_MAX_SPEAKERS-1;
   do {
      info->router[size] = size;
   } while (size--);

   info->no_tracks = 2;
   info->bitrate = 320;
   info->track = AAX_TRACK_ALL;

   info->pitch = 1.0f;
   info->frequency = 48000.0f;
   info->period_rate = 20.0f;
   info->refresh_rate = 20.0f;
   info->format = AAX_PCM16S;
   info->mode = AAX_MODE_WRITE_STEREO;
   info->max_emitters = _AAX_MAX_MIXER_REGISTERED;
   info->max_registered = 0;

   info->update_rate = 0;
   info->sse_level = _aaxGetSIMDSupportLevel();
   info->no_cores = _aaxGetNoCores();

   info->id = INFO_ID;
   info->backend = handle;

#if 0
   opencl = getenv("AAX_USE_OPENCL");
   if ((!opencl || _aax_getbool(opencl)) && _aaxOpenCLDetect()) {
      info->gpu = _aaxOpenCLCreate();
   }
#endif
}

void
_aaxSetDefault2dProps(_aax2dProps *p2d)
{
   unsigned int pos, size;

   assert (p2d);

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

   /* stereo filters */
   for (pos=0; pos<MAX_STEREO_FILTER; pos++) {
      _aaxSetDefaultFilter2d(&p2d->filter[pos], pos);
   }
   for (pos=0; pos<MAX_STEREO_EFFECT; pos++) {
      _aaxSetDefaultEffect2d(&p2d->effect[pos], pos);
   }

   /* previous gains */
   size = _AAX_MAX_SPEAKERS*sizeof(float);
   memset(&p2d->prev_gain, 0, size);
   p2d->prev_freq_fact = 0.0f;
   p2d->dist_delay_sec = 0.0f;
   p2d->bufpos3dq = 0.0f;

   p2d->curr_pos_sec = 0.0f;		/* MIDI */
   p2d->note.velocity = 1.0f;
   p2d->note.pressure = 1.0f;


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
   mtx4dSetIdentity(dp3d->matrix.m4);

   /* velocity     */
   mtx4fSetIdentity(dp3d->velocity.m4);

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
   size_t size;

   size = sizeof(_aax3dProps);
   ptr2 = (char*)size;
   size += sizeof(_aaxDelayed3dProps);
   ptr1 = _aax_calloc(&ptr2, 1, size);
   if (ptr1)
   {
      unsigned int pos;

      rv = (_aax3dProps*)ptr1;
      rv->m_dprops3d = (_aaxDelayed3dProps*)ptr2;

      rv->dprops3d = _aax_aligned_alloc(sizeof(_aaxDelayed3dProps));
      if (rv->dprops3d)
      {
         _aaxSetDefaultDelayed3dProps(rv->dprops3d);
         _aaxSetDefaultDelayed3dProps(rv->m_dprops3d);

         for (pos=0; pos<MAX_3D_FILTER; pos++) {
            _aaxSetDefaultFilter3d(&rv->filter[pos], pos);
         }
         for (pos=0; pos<MAX_3D_EFFECT; pos++) {
            _aaxSetDefaultEffect3d(&rv->effect[pos], pos);
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
 * horizontal plane. Front-back istinctions are not uniquely determined
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
float _aaxContextDefaultHead[2][4] = 
{
//     RIGHT     UP        BACK
   { 0.000640f,-0.000110f, 0.000120f, 0.0f },	/* head delay factors */
   { 0.000000f, 0.000200f, 0.000010f, 0.0f }	/* head delay offsets */
};

float _aaxContextDefaultHRTFVolume[_AAX_MAX_SPEAKERS][4] =
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

float _aaxContextDefaultHRTFDelay[_AAX_MAX_SPEAKERS][4] =
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

float _aaxContextDefaultSpeakersVolume[_AAX_MAX_SPEAKERS][4] =
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

float _aaxContextDefaultSpeakersDelay[_AAX_MAX_SPEAKERS][4] =
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


static unsigned int
_aaxGetSetMonoSources(unsigned int max, int num)
{
   static unsigned int _max_sources = _AAX_MAX_SOURCES_AVAIL;
   static unsigned int _sources = _AAX_MAX_SOURCES_AVAIL;
   unsigned int abs_num = abs(num);
   unsigned int ret = _sources;

   if (max)
   {
      if (max > _AAX_MAX_SOURCES_AVAIL) max = _AAX_MAX_SOURCES_AVAIL;
      _max_sources = max;
      _sources = max;
      ret = max;
   }

   if (abs_num && (abs_num < _AAX_MAX_MIXER_REGISTERED))
   {
      unsigned int _src = _sources - num;
      if ((_sources >= (unsigned int)num) && (_src < _max_sources))
      {
         _sources = _src;
         ret = abs_num;
      }
   }

   return ret;
}

unsigned int
_aaxGetNoEmitters() {
   int rv = _aaxGetSetMonoSources(0, 0);
   if (rv > _AAX_MAX_MIXER_REGISTERED) rv = _AAX_MAX_MIXER_REGISTERED;
   return rv;
}

unsigned int
_aaxSetNoEmitters(unsigned int max) {
   return _aaxGetSetMonoSources(max, 0);
}

unsigned int
_aaxIncreaseEmitterCounter() {
   return _aaxGetSetMonoSources(0, 1);
}

unsigned int
_aaxDecreaseEmitterCounter() {
   return _aaxGetSetMonoSources(0, -1);
}

static void
_aaxSetSlotFromAAXSOld(const char *xid, int (*setSlotFn)(void*, unsigned, int, aaxVec4f), void *id)
{
   unsigned int s, snum = xmlNodeGetNum(xid, "slot");
   void *xsid = xmlMarkId(xid);

   for (s=0; s<snum; s++)
   {
      if (xmlNodeGetPos(xid, xsid, "slot", s) != 0)
      {
         if (xmlNodeGetPos(xid, xsid, "slot", s) != 0)
         {
            enum aaxType type = AAX_LINEAR;
            aaxVec4f params;
            unsigned int slen;
            char src[65];
            long int n;

            n = xmlAttributeGetInt(xsid, "n");
            if (n == XML_NONE) n = s;

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
_aaxSetSlotFromAAXS(const char *xid, int (*setParamFn)(void*, int, int, float), void *id, float freq)
{
   unsigned int s, snum = xmlNodeGetNum(xid, "slot");
   void *xsid = xmlMarkId(xid);
   int rv = AAX_FALSE;

   for (s=0; s<snum; s++)
   {
      if (xmlNodeGetPos(xid, xsid, "slot", s) != 0)
      {
         unsigned int p, pnum = xmlNodeGetNum(xsid, "param");
         if (pnum)
         {
            enum aaxType type = AAX_LINEAR;
            void *xpid = xmlMarkId(xsid);
            unsigned int slen;
            char src[65];
            long int sn;

            sn = xmlAttributeGetInt(xsid, "n");
            if (sn == XML_NONE) sn = s;

            slen = xmlAttributeCopyString(xsid, "type", src, 64);
            if (slen)
            {
               src[slen] = 0; 
               type = aaxGetTypeByName(src);
            }

            for (p=0; p<pnum; p++)
            {  
               if (xmlNodeGetPos(xsid, xpid, "param", p) != 0)
               {
                  int slotnum[_MAX_FE_SLOTS] = { 0x00, 0x10, 0x20, 0x30 };
                  double value = xmlGetDouble(xpid);
                  long int pn = xmlAttributeGetInt(xpid, "n");
                  if (pn == XML_NONE) pn = p;

                  if (freq != 0.0f)
                  {
                     float pitch = xmlAttributeGetDouble(xpid, "pitch");
                     if (pitch != 0.0f) {
                        value = pitch*freq;
                     }
                  }

                  slen = xmlAttributeCopyString(xpid, "type", src, 64);
                  if (slen)
                  {
                     src[slen] = 0;
                     type = aaxGetTypeByName(src);
                  }
            
                  pn |= slotnum[sn];
                  setParamFn(id, pn, type, value);
               }
            }
            xmlFree(xpid);
            rv = AAX_TRUE;
         }
      }
   }
   xmlFree(xsid);

   return rv;
}

aaxFilter
_aaxGetFilterFromAAXS(aaxConfig config, const char *xid, float freq)
{
   aaxFilter rv = NULL;
   char src[65];
   int slen;

   slen = xmlAttributeCopyString(xid, "type", src, 64);
   if (slen)
   {
      enum aaxWaveformType state = AAX_CONSTANT_VALUE;
      enum aaxFilterType ftype;
      aaxFilter flt;

      src[slen] = 0;
      ftype = aaxFilterGetByName(config, src);
      flt = aaxFilterCreate(config, ftype);
      if (flt)
      {
         if (!_aaxSetSlotFromAAXS(xid, aaxFilterSetParam, flt, freq)) {
            _aaxSetSlotFromAAXSOld(xid, aaxFilterSetSlotParams, flt);
         }

         if ((ftype == AAX_TIMED_GAIN_FILTER) &&
              xmlAttributeExists(xid, "repeat"))
         {
            state = xmlAttributeGetInt(xid, "repeat");
            if (state < 0) state = 0;
            if (state >= AAX_REPEAT) state = AAX_REPEAT-1;
            state |= AAX_REPEAT;
         }
         else
         {
            slen = xmlAttributeCopyString(xid, "src", src, 64);
            if (slen)
            {
               src[slen] = 0;
               if (ftype == AAX_DISTANCE_FILTER) {
                  state = aaxGetDistanceModelByName(src);
               } else if (ftype == AAX_FREQUENCY_FILTER) {
                  state = aaxGetFrequencyFilterTypeByName(src);
               } else {
                  state = aaxGetWaveformTypeByName(src);
               }
            }
         }
         aaxFilterSetState(flt, state);
         rv = flt;
      }
   }

   return rv;
}

aaxEffect
_aaxGetEffectFromAAXS(aaxConfig config, const char *xid, float freq)
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
      eff = aaxEffectCreate(config, etype);
      if (eff)
      {
         if (!_aaxSetSlotFromAAXS(xid, aaxEffectSetParam, eff, freq)) {
            _aaxSetSlotFromAAXSOld(xid, aaxEffectSetSlotParams, eff);
         }

         slen = xmlAttributeCopyString(xid, "src", src, 64);
         if (slen)
         {
            src[slen] = 0;
            state = aaxGetWaveformTypeByName(src);
         }
         aaxEffectSetState(eff, state);
         rv = eff;
      }
   }

   return rv;
}
