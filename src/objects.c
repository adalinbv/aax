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

#include "api.h"
#include "objects.h"
#include "arch.h"

void
_aaxSetDefaultInfo(_aaxMixerInfo *info, void *handle)
{
   unsigned int size;

   size = 2*sizeof(vec4_t); 
   _aax_memcpy(&info->hrtf, &_aaxContextDefaultHead, size);

   size = _AAX_MAX_SPEAKERS * sizeof(vec4_t);
   _aax_memcpy(&info->speaker, &_aaxContextDefaultSpeakersVolume, size);

   info->delay = &info->speaker[_AAX_MAX_SPEAKERS];
   _aax_memcpy(info->delay, &_aaxContextDefaultSpeakersDelay, size);

   size = _AAX_MAX_SPEAKERS;
   memcpy(&info->router, &_aaxContextDefaultRouter, size);

   info->no_tracks = 2;
   info->track = AAX_TRACK_ALL;

   info->pitch = 1.0f;
   info->frequency = 48000.0f;
   info->refresh_rate = 20.0f;
   info->format = AAX_PCM16S;
   info->mode = AAX_MODE_WRITE_STEREO;
   info->max_emitters = _AAX_MAX_MIXER_REGISTERED;
   info->max_registered = 0;

   info->bitrate = 320;
   info->update_rate = 0;

   info->id = INFO_ID;
   info->backend = handle;
}

void
_aaxSetDefault2dProps(_aax2dProps *p2d)
{
   unsigned int pos, size;

   assert (p2d);

   /* normalized  directions */
   size = _AAX_MAX_SPEAKERS*sizeof(vec4_t);
   memset(p2d->speaker, 0, 2*size);

   /* heade setup, unused for emitters */
   size = sizeof(vec4_t);
   memset(p2d->head, 0, size);

   /* hrtf sample offsets */
   size = 2*sizeof(vec4_t);
   memset(p2d->hrtf, 0, size);
   memset(p2d->hrtf_prev, 0, size);

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
   mtx4Copy(dp3d->matrix, aaxIdentityMatrix);

   /* velocity     */
   mtx4Copy(dp3d->velocity, aaxIdentityMatrix);

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

      rv->dprops3d = _aax_aligned_alloc16(sizeof(_aaxDelayed3dProps));
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

   rv = _aax_aligned_alloc16(sizeof(_aaxDelayed3dProps));
   if (rv) {
      _aax_memcpy(rv, dp3d, sizeof(_aaxDelayed3dProps));
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

char _aaxContextDefaultRouter[_AAX_MAX_SPEAKERS] =
 { 0, 1, 2, 3, 4, 5, 6, 7 };

/* HRTF
 *
 * The inner pinna ridge which determine front-back directions
 * in the horizontal plane:
 * Angle from right (azimuth, front = 0deg): (inner pinna ridge)
 *     0 deg =  0.080 ms,					-- ahead --
 *    90 deg =  0.015 ms,
 *   135+deg =  0.000 ms (back)
 *
 * The outer pinna rim which is important in determining elevation
 * in the vertical plane
 * Angle from above (0deg = below, 180deg = above): (outer pinna rim)
 *     0 deg = 0.325 ms,				-- below --
 *    90 deg = 0.175 ms,				-- ahead --
 *   180 deg = 0.100 ms (above)
 *
 * Front-back distinctions are not uniquely determined by time differences
 * Angle from ahead (azimuth, front = 0deg): (frequency comb filter)
 *     0 deg =  0.00 ms,                                -- ahead --
 *    90 deg =  0.64 ms,
 *   180 deg =  0.00 ms (back)
 *
 * ahead: 0.080 ms, 0.175 ms, 0.00 ms
 * left:  0.015 ms, 0.175 ms, 0.64 ms
 * right: 0.015 ms, 0.175 ms, 0.64 ms
 * back:  0.000 ms, 0.175 ms, 0.00 ms
 * up:    0.015 ms, 0.100 ms, 0.64 ms
 * down:  0.015 ms, 0.325 ms, 0.64 ms
 */
vec4_t _aaxContextDefaultHead[2] = 
{
//     RIGHT     UP        BACK	    GAIN	/* samples at 48kHz   */
   { 0.00064f, 0.00025f, 0.00009f, 0.000f },	/* head delay factors */
   { 0.00000f, 0.00010f, 0.000000, 0.000f }	/* head delay offsets */
};

vec4_t _aaxContextDefaultHRTFVolume[_AAX_MAX_SPEAKERS] =
{
   /* left headphone shell (volume)                          --- */
   {-1.20f, 0.00f, 0.00f, 1.0f }, 	 /* left-right           */
   { 0.00f,-1.00f, 0.00f, 1.0f }, 	 /* up-down              */
   {-0.98f, 0.00f, 0.20f, 1.0f }, 	 /* back-front           */
   /* right headphone shell (volume)                         --- */
   { 1.20f, 0.00f, 0.00f, 1.0f }, 	 /* left-right           */
   { 0.00f,-1.00f, 0.00f, 1.0f }, 	 /* up-down              */
   { 0.98f, 0.00f, 0.20f, 1.0f }, 	 /* back-front           */
   /* unused                                                     */
   { 0.00f, 0.00f, 0.00f, 0.0f },
   { 0.00f, 0.00f, 0.000, 0.0f }
};

vec4_t _aaxContextDefaultHRTFDelay[_AAX_MAX_SPEAKERS] =
{
   /* left headphone shell (delay)                           --- */
   {-0.20f, 0.00f, 0.98f, 0.0f },        /* left-right           */
   { 0.00f,-1.00f, 0.00f, 1.0f },        /* up-down              */
   {-1.00f, 0.00f, 0.00f, 1.0f },        /* back-front           */
   /* right headphone shell (delay)                          --- */
   { 0.20f, 0.00f, 0.98f, 0.0f },        /* left-right           */
   { 0.00f,-1.00f, 0.00f, 1.0f },        /* up-down              */
   { 1.00f, 0.00f, 0.00f, 1.0f },        /* back-front           */
   /* unused                                                     */
   { 0.00f, 0.00f, 0.00f, 0.0f },        
   { 0.00f, 0.00f, 0.000, 0.0f }
};

vec4_t _aaxContextDefaultSpeakersVolume[_AAX_MAX_SPEAKERS] =
{
   {-1.00f, 0.00f,-1.00f, 1.0f },	/* front left speaker    */
   { 1.00f, 0.00f,-1.00f, 1.0f },	/* front right speaker   */
   {-1.00f, 0.00f, 1.00f, 1.0f },	/* rear left speaker     */
   { 1.00f, 0.00f, 1.00f, 1.0f },	/* rear right speaker    */
   { 0.00f, 0.00f,-1.00f, 1.0f },	/* front center speaker  */
   { 0.00f, 0.00f,-1.00f, 1.0f },	/* low frequency emitter */
   {-1.00f, 0.00f, 0.00f, 1.0f },	/* left side speaker     */
   { 1.00f, 0.00f, 0.00f, 1.0f }	/* right side speaker    */
};

vec4_t _aaxContextDefaultSpeakersDelay[_AAX_MAX_SPEAKERS] =
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
_aaxGetEmitter() {
   return _aaxGetSetMonoSources(0, 1);
}

unsigned int
_aaxPutEmitter() {
   return _aaxGetSetMonoSources(0, -1);
}

