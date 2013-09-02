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

#include "objects.h"
#include "arch.h"
#include "api.h"

void
_aaxSetDefaultInfo(_aaxMixerInfo *info, void *handle)
{
   unsigned int size;

   size = 2*sizeof(vec4_t); 
   _aax_memcpy(&info->hrtf, &_aaxContextDefaultHead, size);

   size = _AAX_MAX_SPEAKERS * sizeof(vec4_t);
   _aax_memcpy(&info->speaker, &_aaxContextDefaultSpeakers, size);

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

   info->update_rate = 0;

   info->id = INFO_ID;
   info->backend = handle;
}

void
_aaxSetDefault2dProps(_oalRingBuffer2dProps *p2d)
{
   unsigned int pos, size;

   assert (p2d);

   /* normalized  directions */
   size = _AAX_MAX_SPEAKERS*sizeof(vec4_t);
   memset(p2d->speaker, 0, size);

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
   p2d->final.pitch_lfo = 1.0f;
   p2d->final.pitch = 1.0f;
   p2d->final.gain_lfo = 1.0f;
   p2d->final.gain = 0.0f;
}

void
_aaxSetDefaultDelayed3dProps(_oalRingBufferDelayed3dProps *dp3d)
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

_oalRingBuffer3dProps *
_aax3dPropsCreate()
{
   _oalRingBuffer3dProps *rv = NULL;
   unsigned int size;
   char *ptr1, *ptr2;

   size = sizeof(_oalRingBuffer3dProps);
   ptr2 = (char*)size;
   size += sizeof(_oalRingBufferDelayed3dProps);
   ptr1 = _aax_calloc(&ptr2, 1, size);
   if (ptr1)
   {
      unsigned int pos;

      rv = (_oalRingBuffer3dProps*)ptr1;
      rv->m_dprops3d = (_oalRingBufferDelayed3dProps*)ptr2;

      rv->dprops3d = _aax_aligned_alloc16(sizeof(_oalRingBufferDelayed3dProps));
      _aaxSetDefaultDelayed3dProps(rv->dprops3d);
      _aaxSetDefaultDelayed3dProps(rv->m_dprops3d);

      for (pos=0; pos<MAX_3D_FILTER; pos++) {
         _aaxSetDefaultFilter3d(&rv->filter[pos], pos);
      }
      for (pos=0; pos<MAX_3D_EFFECT; pos++) {
         _aaxSetDefaultEffect3d(&rv->effect[pos], pos);
      }
   }
   return rv;
}

_oalRingBufferDelayed3dProps *
_aaxDelayed3dPropsDup(_oalRingBufferDelayed3dProps *dp3d)
{
   _oalRingBufferDelayed3dProps *rv;

   rv = _aax_aligned_alloc16(sizeof(_oalRingBufferDelayed3dProps));
   if (rv)
   {
      _aaxSetDefaultDelayed3dProps(rv);

      rv->state3d = dp3d->state3d;
      rv->pitch = dp3d->pitch;
      rv->gain = dp3d->gain;
   }
   return rv;
}

void
removeDelayed3dQueueByPos(void *queue, unsigned int pos)
{
   _intBuffers *p3dq = (_intBuffers*)queue;
   _oalRingBufferDelayed3dProps *d3dp;

   d3dp = _intBufRemove(p3dq, _AAX_DELAYED3D, pos, AAX_FALSE);
   if (d3dp) {
      _aax_aligned_free(d3dp);
   }
}

void
_aaxSetDefaultFilter2d(_oalRingBufferFilterInfo *filter, unsigned int type)
{
   assert(type < MAX_STEREO_FILTER);

   memset(filter, 0, sizeof(_oalRingBufferFilterInfo));
   switch(type)
   {
   case VOLUME_FILTER:
      filter->param[AAX_GAIN] = 1.0f;
      filter->param[AAX_MAX_GAIN] = 1.0f;
      filter->state = AAX_TRUE;
      break;
   case FREQUENCY_FILTER:
      filter->param[AAX_CUTOFF_FREQUENCY] = 22050.0f;
      filter->param[AAX_LF_GAIN] = 1.0f;
      filter->param[AAX_HF_GAIN] = 1.0f;
      filter->param[AAX_RESONANCE] = 1.0f;
      break;
   default:
      break;
   }
}

void
_aaxSetDefaultEffect2d(_oalRingBufferFilterInfo *effect, unsigned int type)
{
   assert(type < MAX_STEREO_EFFECT);

   memset(effect, 0, sizeof(_oalRingBufferFilterInfo));
   switch(type)
   {
   case PITCH_EFFECT:
      effect->param[AAX_PITCH] = 1.0f;
      effect->param[AAX_MAX_PITCH] = 4.0f;
      effect->state = AAX_TRUE;
      break;
   case TIMED_PITCH_EFFECT:
      effect->param[AAX_LEVEL0] = 1.0f;
      effect->param[AAX_LEVEL1] = 1.0f;
      break;
   case DISTORTION_EFFECT:
      effect->param[AAX_CLIPPING_FACTOR] = 0.3f;
      effect->param[AAX_ASYMMETRY] = 0.7f;
      break;
   case AAX_REVERB_EFFECT:
      effect->param[AAX_CUTOFF_FREQUENCY] = 10000.0f;
      effect->param[AAX_DELAY_DEPTH] = 0.27f;
      effect->param[AAX_DECAY_LEVEL] = 0.3f;
      effect->param[AAX_DECAY_DEPTH] = 0.7f;
      break;
   default:
      break;
   }
}

void
_aaxSetDefaultFilter3d(_oalRingBufferFilterInfo *filter, unsigned int type)
{
   assert(type < MAX_3D_FILTER);

   memset(filter, 0, sizeof(_oalRingBufferFilterInfo));
   switch(type)
   {
   case DISTANCE_FILTER:
      filter->param[AAX_REF_DISTANCE] = 1.0f;
      filter->param[AAX_MAX_DISTANCE] = MAXFLOAT;
      filter->param[AAX_ROLLOFF_FACTOR] = 1.0f;
      filter->state = AAX_EXPONENTIAL_DISTANCE;
      break;
   case ANGULAR_FILTER:
      filter->param[AAX_INNER_ANGLE] = 1.0f;
      filter->param[AAX_OUTER_ANGLE] = 1.0f;
      filter->param[AAX_OUTER_GAIN] = 1.0f;
      filter->state = AAX_TRUE;
      break;
   default:
      break;
   }
}

void
_aaxSetDefaultEffect3d(_oalRingBufferFilterInfo *effect, unsigned int type)
{
   assert(type < MAX_3D_EFFECT);

   memset(effect, 0, sizeof(_oalRingBufferFilterInfo));
   switch(type)
   {
   case VELOCITY_EFFECT:
      effect->param[AAX_SOUND_VELOCITY] = 343.0f;
      effect->param[AAX_DOPPLER_FACTOR] = 1.0f;
      effect->state = AAX_TRUE;
   default:
      break;
   }
}

/* -------------------------------------------------------------------------- */

char _aaxContextDefaultRouter[_AAX_MAX_SPEAKERS] =
 { 0, 1, 2, 3, 4, 5, 6, 7 };

vec4_t _aaxContextDefaultSpeakersHRTF[_AAX_MAX_SPEAKERS] =
{
   /* left headphone shell */
   {-1.0f, 0.0f, 0.0f, 1.0f },     /* left-right            */
   { 0.0f,-1.0f, 0.0f, 1.0f },     /* up-down               */
   { 0.0f, 0.0f, 1.0f, 1.0f },     /* back-front            */
   /* right headphone shell */
   { 1.0f, 0.0f, 0.0f, 1.0f },     /* left-right            */
   { 0.0f,-1.0f, 0.0f, 1.0f },     /* up-down               */
   { 0.0f, 0.0f, 1.0f, 1.0f },     /* back-front            */
   /* unused */
   { 0.0f, 0.0f, 0.0f, 0.0f },
   { 0.0f, 0.0f, 0.0f, 0.0f }
};

vec4_t _aaxContextDefaultHead[2] = 
{
   {-0.00128f,-0.00025f, 0.00009f, 0.000f },	/* head delay factors */
   { 0.00064f, 0.00035f, 0.00000f, 0.000f }	/* head delay offsets */
};

vec4_t _aaxContextDefaultSpeakers[_AAX_MAX_SPEAKERS] =
{
   { 1.0f, 0.0f, 1.0f, 1.0f },		/* front left speaker    */
   {-1.0f, 0.0f, 1.0f, 1.0f },		/* front right speaker   */
   { 1.0f, 0.0f,-1.0f, 1.0f },		/* rear left speaker     */
   {-1.0f, 0.0f,-1.0f, 1.0f },		/* rear right speaker    */
   { 0.0f, 0.0f, 1.0f, 1.0f },		 /* front center speaker  */
   { 0.0f, 0.0f, 1.0f, 1.0f },		/* low frequency emitter */
   { 1.0f, 0.0f, 0.0f, 1.0f },		/* left side speaker     */
   {-1.0f, 0.0f, 0.0f, 1.0f }		/* right side speaker    */
};

