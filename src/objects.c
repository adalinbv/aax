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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBIO_H
#include <libio.h>		/* for NULL */
#endif
#include <math.h>		/* for MAXFLOAT */

#include "objects.h"

char _aaxContextDefaultRouter[_AAX_MAX_SPEAKERS] =
 { 0, 1, 2, 3, 4, 5, 6, 7 };

vec4 _aaxContextDefaultSpeakers[_AAX_MAX_SPEAKERS] =
{
   { 1.0f, 0.0f, 1.0f, 1.0f },     /* front left speaker    */
   {-1.0f, 0.0f, 1.0f, 1.0f },     /* front right speaker   */
   { 1.0f, 0.0f,-1.0f, 0.0f },     /* rear left speaker     */
   {-1.0f, 0.0f,-1.0f, 0.0f },     /* rear right speaker    */
   { 0.0f, 0.0f, 1.0f, 0.0f },     /* front center speaker  */
   { 0.0f, 0.0f, 1.0f, 0.0f },     /* low frequency emitter */
   { 1.0f, 0.0f, 0.0f, 0.0f },     /* left side speaker     */
   {-1.0f, 0.0f, 0.0f, 0.0f }      /* right side speaker    */
};

vec4 _aaxContextDefaultSpeakersHRTF[_AAX_MAX_SPEAKERS] =
{
   /* left headphone shell */
   { 1.0f, 0.0f, 0.0f, 1.0f },     /* left-right            */
   { 0.0f,-1.0f, 0.0f, 1.0f },     /* up-down               */
   { 0.0f, 0.0f, 1.0f, 1.0f },     /* back-front            */
   /* right headphone shell */
   {-1.0f, 0.0f, 0.0f, 1.0f },     /* left-right            */
   { 0.0f,-1.0f, 0.0f, 1.0f },     /* up-down               */
   { 0.0f, 0.0f, 1.0f, 1.0f },     /* back-front            */
   /* unused */
   { 0.0f, 0.0f, 0.0f, 0.0f },
   { 0.0f, 0.0f, 0.0f, 0.0f }
};

vec4 _aaxContextDefaultHead[2] = 
{
   { 0.00064f, 0.000090f, 0.00024f, 1.000f },	/* head delay factors */
   { 0.00000f, 0.000100f, 0.00000f, 1.000f }	/* head delay offsets */
};

const _oalRingBuffer2dProps _aaxDefault2dProps =
{
  {				/* normalized  directions */
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f }
  },

  { 0.0f, 0.0f, 0.0f, 0.0f }, /* heade setup, unused for emitters */
  {                           /* hrtf sample offsets */
     { 0.0f, 0.0f, 0.0f, 0.0f },
     { 0.0f, 0.0f, 0.0f, 0.0f }
  },
  {                           /* previous hrtf sample offsets */
     { 0.0f, 0.0f, 0.0f, 0.0f },
     { 0.0f, 0.0f, 0.0f, 0.0f }
  },

  /* filters */
  {
    /* VOLUME_FILTER: volume, min_gain, max_gain / filter data */
    { { 1.0f, 0.0f, 1.0f, 0.0f }, NULL },
    /* TREMOLO_FILTER */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, NULL },
    /* TIMED_GAIN_FILTER: next-volume, transition time */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, NULL },
    /* FREQUENCY_FILTER: cutoff_freq, lf_gain, hf_gain / filter data */
    { { 22050.0f, 1.0f, 1.0f }, NULL }
  },

  /* effects */
  {
    /* PITCH_EFFECT */
    { { 1.0f, 4.0f, 0.0f, 0.0f }, NULL },
    /* VIBRATO_EFFECT */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, NULL },
    /* TIMED_PITCH_EFFECT */
    { { 1.0f, 0.0f, 1.0f, 0.0f }, NULL },
    /* DISTORTION_EFFECT */
    { { 0.0f, 1.0f, 0.0f, 0.0f }, NULL },
    /* DELAY_EFFECT */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, NULL },
  },

	/* previous gains */
  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
  0.0f,			/* previous frequency factor */
  0.0f,			/* (distance) delay */

  {
    1.0f,		/* internal pitch representation */
    1.0f		/* internal gain representation */
  }
};

const _oalRingBuffer3dProps _aaxDefault3dProps =
{
  /* modelview matrix */
  {
    { 1.0f, 0.0f, 0.0f, 0.0f },	/* right vector */
    { 0.0f, 1.0f, 0.0f, 0.0f },	/* up vector    */
    { 0.0f, 0.0f, 1.0f, 0.0f },	/* at vector    */
    { 0.0f, 0.0f, 0.0f, 1.0f }	/* location     */
  },

  { 0.0f, 0.0f, 0.0f, 0.0f },	/* velocity     */

  _STATE_PAUSED,		/* status */

  /* filters */
  {
    /* DISTANCE_FILTER: ref_distance, max_distance, rolloff factor */
    { { 1.0f, MAXFLOAT, 1.0f, 0.0f }, NULL },
    /* ANGULAR_FILTER: inner_vec, outer_vec, outer_gain */
    { { 0.0f, 1.0f, 1.0f, 0.0f }, NULL }
  },

  /* effects */
  {
    /* VELOCITY_EFFECT */
    { { 343.0f, 1.0f, 0.0f, 0.0f }, NULL },
  }
};

const _aaxMixerInfo _aaxDefaultMixerInfo =
{
  /* hrtf setup */
  {
     { 28.224f, 10.584f, 3.969f, 0.000f } ,
     {  0.000f,  4.410f, 0.000f, 0.000f }
  },

  /* speaker setup */
  {
    { 1.0f, 0.0f,-1.0f, 1.0f },		/* front left speaker    */
    {-1.0f, 0.0f,-1.0f, 1.0f },		/* front right speaker   */
    { 0.0f, 0.0f, 1.0f, 0.0f },		/* rear left speaker     */
    { 0.0f, 0.0f, 1.0f, 0.0f },		/* rear right speaker    */
    { 0.0f, 0.0f,-1.0f, 0.0f },		/* front center speaker  */
    { 0.0f, 0.0f,-1.0f, 0.0f },		/* low frequency emitter */
    { 1.0f, 0.0f, 0.0f, 1.0f },		/* left side speaker     */
    {-1.0f, 0.0f, 0.0f, 1.0f }		/* right side speaker    */
  },

  { 0, 1, 0, 1, 0, 1, 0, 1 },	/* speaker router setup */
  2,				/* no. speakers */

  1.0f,				/* pitch */
  48000.0f,			/* frequency */
  20.0f,			/* refresh_rate */
  AAX_FORMAT_PCM16S,		/* format */
  AAX_MODE_WRITE_STEREO,	/* render mode */
  _AAX_MAX_MIXER_SOURCES,	/* max emitters */

  0				/* update counter */
};

