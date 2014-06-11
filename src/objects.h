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

#ifndef _AAX_OBJECT_H
#define _AAX_OBJECT_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <base/geometry.h>
// #include <base/threads.h>

#include <filters/effects.h>


#define WRITE_BUFFER_TO_FILE(dptr, bufsz) \
 do { \
   int16_t q, *data = malloc((bufsz)*2); \
   for(q=0; q<(bufsz); q++) *((dptr)+q) *= 0.5; \
   _batch_cvt16_24(data, (dptr), (bufsz)); \
   _aaxFileDriverWrite("/tmp/resample.wav", AAX_OVERWRITE, data, (bufsz), 48000, 1, AAX_PCM16S); \
   free(data); \
   exit(-1); \
 } while (0);


/*
 * Beware: state can be both PLAYING and STOPPED, meaning the emitter is
 * ordered to stop playback but needs to handle additional samples before
 * it actually stops playback.
 */
enum
{
    _STATE_PAUSED	= 0x0001,
    _STATE_PROCESSED	= 0x0002,
    _STATE_STOPPED	= 0x0004,
    _STATE_INITIAL	= (_STATE_PAUSED|_STATE_PROCESSED|_STATE_STOPPED),
    _STATE_LOOPING	= 0x0008,
    _STATE_RELATIVE	= 0x0010,
    _STATE_POSITIONAL	= 0x0020,

    _STATE_PLAYING_MASK	= (_STATE_PAUSED|_STATE_PROCESSED)
};

#define _STATE_TAS(q,r,s)    ((r) ? ((q) |= (s)) : ((q) &= ~(s)))

#define _IS_PLAYING(q)       (((q)->state & _STATE_PLAYING_MASK) == 0)
#define _IS_INITIAL(q)       (((q)->state & _STATE_INITIAL) == _STATE_INITIAL)
#define _IS_STANDBY(q)       (((q)->state & _STATE_INITIAL) == _STATE_INITIAL)
#define _IS_STOPPED(q)       (((q)->state & _STATE_INITIAL) == _STATE_STOPPED)
#define _IS_PAUSED(q)        (((q)->state & _STATE_INITIAL) == _STATE_PAUSED)
#define _IS_PROCESSED(q)     (((q)->state & _STATE_PLAYING_MASK) == _STATE_PROCESSED)
#define _IS_LOOPING(q)       ((q)->state & _STATE_LOOPING)
#define _IS_RELATIVE(q)      ((q)->state & _STATE_RELATIVE)
#define _IS_POSITIONAL(q)    ((q)->state & _STATE_POSITIONAL)

#define _SET_INITIAL(q)      ((q)->state |= _STATE_INITIAL)
#define _SET_STANDBY(q)      ((q)->state |= _STATE_INITIAL)
#define _SET_PLAYING(q)      ((q)->state &= ~_STATE_INITIAL)
#define _SET_STOPPED(q)      ((q)->state |= _STATE_STOPPED)
#define _SET_PAUSED(q)       ((q)->state |= _STATE_PAUSED)
#define _SET_LOOPING(q)      ((q)->state |= _STATE_LOOPING)
#define _SET_PROCESSED(q)    ((q)->state |= _STATE_PROCESSED)
#define _SET_RELATIVE(q)     ((q)->state |= _STATE_RELATIVE)
#define _SET_POSITIONAL(q)   ((q)->state |= _STATE_POSITIONAL)

#define _TAS_PLAYING(q,r)     _STATE_TAS((q)->state, (r), _STATE_INITIAL)
#define _TAS_PAUSED(q,r)      _STATE_TAS((q)->state, (r), _STATE_PAUSED)
#define _TAS_LOOPING(q,r)     _STATE_TAS((q)->state, (r), _STATE_LOOPING)
#define _TAS_PROCESSED(q,r)   _STATE_TAS((q)->state, (r), _STATE_PROCESSED)
#define _TAS_RELATIVE(q,r)    _STATE_TAS((q)->state, (r), _STATE_RELATIVE)
#define _TAS_POSITIONAL(q,r)  _STATE_TAS((q)->state, (r), _STATE_POSITIONAL)

/* 3d properties */

enum
{
    MODEL_MTX = 0,		/* 4x4 model view matrix*/
    DIR_RIGHT = MODEL_MTX,	/* vec4*/
    DIR_UPWD,			/* vec4*/
    DIR_BACK,			/* vec4*/
    LOCATION,			/* vec4*/
    OFFSET = LOCATION,
    VELOCITY = LOCATION,
    GAIN = LOCATION,

    MAX_OBJECT
};

enum
{
    PITCH_CHANGED          = 0x00000001,
    GAIN_CHANGED           = 0x00000002,
    DIST_CHANGED           = 0x00000004,
    MTX_CHANGED            = 0x00000008,
    SPEED_CHANGED          = 0x00000010,

    CONE_DEFINED           = 0x00010000,
    DYNAMIC_PITCH_DEFINED  = 0x00020000,

    /* SCENE*/
    DISTQUEUE_CHANGED      = 0x08000000,
    SCENE_CHANGED          = 0x10000000,
    REVERB_CHANGED         = 0x20000000,
    DISTDELAY_CHANGED      = 0x40000000,
    WIND_CHANGED           = 0x80000000
};
#define PITCH_CHANGE            (PITCH_CHANGED | DYNAMIC_PITCH_DEFINED)

#define _PROP3D_CLEAR(q)                ((q)->state3d &= (CONE_DEFINED|DYNAMIC_PITCH_DEFINED))
#define _PROP3D_PITCH_HAS_CHANGED(q)    ((q)->state3d & PITCH_CHANGE)
#define _PROP3D_GAIN_HAS_CHANGED(q)     ((q)->state3d & GAIN_CHANGED)
#define _PROP3D_DIST_HAS_CHANGED(q)     ((q)->state3d & DIST_CHANGED)
#define _PROP3D_MTX_HAS_CHANGED(q)      ((q)->state3d & MTX_CHANGED)
#define _PROP3D_SPEED_HAS_CHANGED(q)    ((q)->state3d & SPEED_CHANGED)
#define _PROP3D_MTXSPEED_HAS_CHANGED(q) ((q)->state3d & (SPEED_CHANGED|MTX_CHANGED))
#define _PROP3D_CONE_IS_DEFINED(q)      ((q)->state3d & CONE_DEFINED)

#define _PROP3D_PITCH_SET_CHANGED(q)    ((q)->state3d |= PITCH_CHANGED)
#define _PROP3D_GAIN_SET_CHANGED(q)     ((q)->state3d |= GAIN_CHANGED)
#define _PROP3D_DIST_SET_CHANGED(q)     ((q)->state3d |= DIST_CHANGED)
#define _PROP3D_MTX_SET_CHANGED(q)      ((q)->state3d |= MTX_CHANGED)
#define _PROP3D_SPEED_SET_CHANGED(q)    ((q)->state3d |= SPEED_CHANGED)
#define _PROP3D_CONE_SET_DEFINED(q)     ((q)->state3d |= CONE_DEFINED)
#define _PROP3D_DYNAMIC_PITCH_SET_DEFINED(q) ((q)->state3d |= DYNAMIC_PITCH_DEFINED)

#define _PROP3D_PITCH_CLEAR_CHANGED(q)  ((q)->state3d &= ~PITCH_CHANGED)
#define _PROP3D_GAIN_CLEAR_CHANGED(q)   ((q)->state3d &= ~GAIN_CHANGED)
#define _PROP3D_DIST_CLEAR_CHANGED(q)   ((q)->state3d &= ~DIST_CHANGED)
#define _PROP3D_MTX_CLEAR_CHANGED(q)    ((q)->state3d &= ~MTX_CHANGED)
#define _PROP3D_SPEED_CLEAR_CHANGED(q)  ((q)->state3d &= ~SPEED_CHANGED)
#define _PROP3D_CONE_CLEAR_DEFINED(q)   ((q)->state3d &= ~CONE_DEFINED)
#define _PROP3D_DYNAMIC_PITCH_CLEAR_DEFINED(q) ((q)->state3d &= ~DYNAMIC_PITCH_DEFINED)

/* 3d properties: AAX Scene extension*/
#define _PROP3D_SCENE_IS_DEFINED(q)     ((q)->state3d & SCENE_CHANGED)
#define _PROP3D_REVERB_IS_DEFINED(q)    ((q)->state3d & REVERB_CHANGED)
#define _PROP3D_DISTDELAY_IS_DEFINED(q) ((q)->state3d & DISTDELAY_CHANGED)
#define _PROP3D_DISTQUEUE_IS_DEFINED(q) ((q)->state3d & DISTQUEUE_CHANGED)
#define _PROP3D_WIND_IS_DEFINED(q)      ((q)->state3d & WIND_CHANGED)

#define _PROP3D_SCENE_SET_CHANGED(q)    ((q)->state3d |= SCENE_CHANGED)
#define _PROP3D_REVERB_SET_CHANGED(q)   ((q)->state3d |= REVERB_CHANGED)
#define _PROP3D_DISTDELAY_SET_DEFINED(q) ((q)->state3d |= DISTDELAY_CHANGED)
#define _PROP3D_DISTQUEUE_SET_DEFINED(q) ((q)->state3d |= (DISTQUEUE_CHANGED|DISTDELAY_CHANGED))
#define _PROP3D_WIND_SET_CHANGED(q)     ((q)->state3d |= WIND_CHANGED)

#define _PROP3D_SCENE_CLEAR_CHANGED(q)  ((q)->state3d &= ~SCENE_CHANGED)
#define _PROP3D_REVERB_CLEAR_CHANGED(q) ((q)->state3d &= ~REVERB_CHANGED)
#define _PROP3D_DISTDELAY_CLEAR_DEFINED(q) ((q)->state3d &= ~DISTDELAY_CHANGED)
#define _PROP3D_DISTQUEUE_CLEAR_DEFINED(q) ((q)->state3d &= ~(DISTQUEUE_CHANGED|DISTDELAY_CHANGED))
#define _PROP3D_WIND_CLEAR_CHANGED(q)   ((q)->state3d &= ~WIND_CHANGED)

/* delayed 3d properties */
#define _PROP_CLEAR(q)                  _PROP3D_CLEAR((q)->dprops3d)
#define _PROP_PITCH_HAS_CHANGED(q)      _PROP3D_PITCH_HAS_CHANGED((q)->dprops3d)
#define _PROP_GAIN_HAS_CHANGED(q)       _PROP3D_GAIN_HAS_CHANGED((q)->dprops3d)
#define _PROP_DIST_HAS_CHANGED(q)       _PROP3D_DIST_HAS_CHANGED((q)->dprops3d)
#define _PROP_MTX_HAS_CHANGED(q)        _PROP3D_MTX_HAS_CHANGED((q)->dprops3d)
#define _PROP_SPEED_HAS_CHANGED(q)      _PROP3D_SPEED_HAS_CHANGED((q)->dprops3d)
#define _PROP_CONE_IS_DEFINED(q)        _PROP3D_CONE_IS_DEFINED((q)->dprops3d)

#define _PROP_PITCH_SET_CHANGED(q)      _PROP3D_PITCH_SET_CHANGED((q)->dprops3d)
#define _PROP_GAIN_SET_CHANGED(q)       _PROP3D_GAIN_SET_CHANGED((q)->dprops3d)
#define _PROP_DIST_SET_CHANGED(q)       _PROP3D_DIST_SET_CHANGED((q)->dprops3d)
#define _PROP_MTX_SET_CHANGED(q)        _PROP3D_MTX_SET_CHANGED((q)->dprops3d)
#define _PROP_SPEED_SET_CHANGED(q)      _PROP3D_SPEED_SET_CHANGED((q)->dprops3d)
#define _PROP_CONE_SET_DEFINED(q)       _PROP3D_CONE_SET_DEFINED((q)->dprops3d)
#define _PROP_DYNAMIC_PITCH_SET_DEFINED(q) _PROP3D_DYNAMIC_PITCH_SET_DEFINED((q)->dprops3d)

#define _PROP_PITCH_CLEAR_CHANGED(q)    _PROP3D_PITCH_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_GAIN_CLEAR_CHANGED(q)     _PROP3D_GAIN_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_DIST_CLEAR_CHANGED(q)     _PROP3D_DIST_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_MTX_CLEAR_CHANGED(q)      _PROP3D_MTX_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_SPEED_CLEAR_CHANGED(q)    _PROP3D_SPEED_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_CONE_CLEAR_DEFINED(q)     _PROP3D_CONE_CLEAR_DEFINED((q)->dprops3d)
#define _PROP_DYNAMIC_PITCH_CLEAR_DEFINED(q) _PROP3D_DYNAMIC_PITCH_CLEAR_DEFINED((q)->dprops3d)

/* delayed 3d properties: AAX Scene extension*/
#define _PROP_SCENE_IS_DEFINED(q)       _PROP3D_SCENE_IS_DEFINED((q)->dprops3d)
#define _PROP_REVERB_IS_DEFINED(q)      _PROP3D_REVERB_IS_DEFINED((q)->dprops3d)
#define _PROP_DISTDELAY_IS_DEFINED(q)   _PROP3D_DISTDELAY_IS_DEFINED((q)->dprops3d)
#define _PROP_DISTQUEUE_IS_DEFINED(q)   _PROP3D_DISTQUEUE_IS_DEFINED((q)->dprops3d)
#define _PROP_WIND_IS_DEFINED(q)        _PROP3D_WIND_IS_DEFINED((q)->dprops3d)

#define _PROP_SCENE_SET_CHANGED(q)      _PROP3D_SCENE_SET_CHANGED((q)->dprops3d)
#define _PROP_REVERB_SET_CHANGED(q)     _PROP3D_REVERB_SET_CHANGED((q)->dprops3d)
#define _PROP_DISTDELAY_SET_DEFINED(q)  _PROP3D_DISTDELAY_SET_DEFINED((q)->dprops3d)
#define _PROP_DISTQUEUE_SET_DEFINED(q)  _PROP3D_DISTQUEUE_SET_DEFINED((q)->dprops3d)
#define _PROP_WIND_SET_CHANGED(q)       _PROP3D_WIND_SET_CHANGED((q)->dprops3d)

#define _PROP_SCENE_CLEAR_CHANGED(q)    _PROP3D_SCENE_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_REVERB_CLEAR_CHANGED(q)   _PROP3D_REVERB_CLEAR_CHANGED(((q)->dprops3d)
#define _PROP_DISTDELAY_CLEAR_DEFINED(q) _PROP3D_DISTDELAY_CLEAR_DEFINED((q)->dprops3d)
#define _PROP_DISTQUEUE_CLEAR_DEFINED(q) _PROP3D_DISTQUEUE_CLEAR_DEFINED((q)->dprops3d)
#define _PROP_WIND_CLEAR_CHANGED(q)     _PROP3D_WIND_CLEAR_CHANGED((q)->dprops3d


enum
{
   HRTF_FACTOR = 0,
   HRTF_OFFSET
};

/* warning:
 * need to update the pre defined structure in objects.c ehwn changing
 * something here
 */
typedef ALIGN16 struct
{
   vec4_t hrtf[2];
   vec4_t speaker[2*_AAX_MAX_SPEAKERS];
   vec4_t *delay;

   char router[_AAX_MAX_SPEAKERS];
   unsigned no_tracks;
   int bitrate;
   int track;

   float pitch;
   float frequency;
   float refresh_rate;
   enum aaxFormat format;
   enum aaxRenderMode mode;
   unsigned int max_emitters;		/* total */
   unsigned int max_registered;		/* per (sub)mixer */

   uint8_t update_rate;	/* how many frames get processed before an update */

   unsigned int id;
   void *backend;

} _aaxMixerInfo ALIGN16C;

typedef ALIGN16 struct
{
   /* modelview matrix and velocity */
   mtx4_t matrix;
   mtx4_t velocity;

   float pitch, gain;
   int state3d;

} _aaxDelayed3dProps ALIGN16C; 

typedef struct
{

   _aaxDelayed3dProps* dprops3d;        /* current   */
   _aaxDelayed3dProps* m_dprops3d;      /* modiefied */
   float buf3dq_step;
   int state;

   /* 3d filters and effects */
   _aaxFilterInfo filter[MAX_3D_FILTER];
   _aaxFilterInfo effect[MAX_3D_EFFECT];

} _aax3dProps;

typedef ALIGN16 struct
{
      /* pos[0] position; -1.0 left,  0.0 center, 1.0 right */
      /* pos[1] position; -1.0 down,  0.0 center, 1.0 up    */
      /* pos[2] position; -1.0 front, 0.0 center, 1.0 back  */
   vec4_t speaker[2*_AAX_MAX_SPEAKERS];

      /* head[0] side delay sec    */
      /* head[1] up delay sec      */
      /* head[2] forward delay sec */
      /* head[3] up offset sec     */
   vec4_t head;
   vec4_t hrtf[2];
   vec4_t hrtf_prev[2];

   /* stereo filters */
   _aaxFilterInfo filter[MAX_STEREO_FILTER];
   _aaxEffectInfo effect[MAX_STEREO_EFFECT];

   float prev_gain[_AAX_MAX_SPEAKERS];
   float prev_freq_fact;

   float dist_delay_sec;        /* time to keep playing after a stop request */
   float bufpos3dq;             /* distance delay queue buffer position      */

   float curr_pos_sec;		/* MIDI */
   struct {
      float velocity;
      float pressure;
   } note;

   struct {
      float pitch_lfo;
      float pitch;
      float gain_lfo;
      float gain;
   } final;

} _aax2dProps ALIGN16C;

typedef struct
{
   _aaxMixerInfo *info;

   _aax2dProps *props2d;
   _aax3dProps *props3d;

   _intBuffers *emitters_2d;	/* plain stereo emitters		*/
   _intBuffers *emitters_3d;	/* emitters with positional information	*/
   _intBuffers *frames;		/* other audio frames			*/
   _intBuffers *devices;	/* registered input devices		*/
   _intBuffers *p3dq;		/* 3d properties delay queue            */

   void *ringbuffer;
   _intBuffers *frame_ringbuffers;	/* for audio frame rendering */
   _intBuffers *play_ringbuffers;		/* for loopback capture */

   unsigned int no_registered;
   float curr_pos_sec;

   unsigned char refcount;

   unsigned char capturing;
 
} _aaxAudioFrame;

typedef struct
{
   _aaxMixerInfo *info;

   _aax2dProps *props2d;		/* 16 byte aligned */
   _aax3dProps *props3d;

   _intBuffers *p3dq;			/* 3d properties delay queue     */
   _intBuffers *buffers;		/* audio buffer queue            */
   int buffer_pos;			/* audio buffer queue pos        */

   int state3d;	/* backup of parent's state needed inbetween update_ctr  */
   int8_t update_rate;
   int8_t update_ctr;

   float curr_pos_sec;

#if 0
   /* delay effects */
   struct {
      char enabled;
      float pre_delay_time;
      float reflection_time;
      float reflection_factor;
      float decay_time;
      float decay_time_hf;
   } reverb;
#endif

} _aaxEmitter;


extern vec4_t _aaxContextDefaultHead[2];
extern vec4_t _aaxContextDefaultSpeakersVolume[_AAX_MAX_SPEAKERS];
extern vec4_t _aaxContextDefaultSpeakersDelay[_AAX_MAX_SPEAKERS];
extern vec4_t _aaxContextDefaultHRTFVolume[_AAX_MAX_SPEAKERS];
extern vec4_t _aaxContextDefaultHRTFDelay[_AAX_MAX_SPEAKERS];
extern char _aaxContextDefaultRouter[_AAX_MAX_SPEAKERS];

void _aaxFreeSource(void *);
void _aaxProcessSource(void *, _aaxEmitter *, unsigned int);

void _aaxSetDefaultInfo(_aaxMixerInfo *, void *);

void _aaxSetDefault2dProps(_aax2dProps *);
_aax3dProps *_aax3dPropsCreate();
_aaxDelayed3dProps *_aaxDelayed3dPropsDup(_aaxDelayed3dProps*);
void _aaxSetDefaultDelayed3dProps(_aaxDelayed3dProps *);

unsigned int _aaxGetNoEmitters();
unsigned int _aaxSetNoEmitters(unsigned int);
unsigned int _aaxGetEmitter();
unsigned int _aaxPutEmitter();

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_OBJECT_H */

