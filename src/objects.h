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

#ifndef _AAX_OBJECT_H
#define _AAX_OBJECT_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <base/geometry.h>

#include "software/audio.h"
#include "software/ringbuffer.h"

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

/* filters */
#define _FILTER_GET_SLOT(F, s, p)       F->slot[s]->param[p]
#define _FILTER_GET_SLOT_STATE(F)       F->slot[0]->state
#define _FILTER_GET_SLOT_DATA(F, s)     F->slot[s]->data
#define _FILTER_SET_SLOT(F, s, p, v)    F->slot[s]->param[p] = v
#define _FILTER_SET_SLOT_DATA(F, s, v)  F->slot[s]->data = v

#define _FILTER_GET(P, f, p)            P->filter[f].param[p]
#define _FILTER_GET_STATE(P, f)         P->filter[f].state
#define _FILTER_GET_DATA(P, f)          P->filter[f].data
#define _FILTER_SET(P, f, p, v)         P->filter[f].param[p] = v
#define _FILTER_SET_STATE(P, f, v)      P->filter[f].state = v;
#define _FILTER_SET_DATA(P, f, v)       P->filter[f].data = v
#define _FILTER_COPY(P1, P2, f, p)      P1->filter[f].param[p] = P2->filter[f].param[p]
#define _FILTER_COPY_DATA(P1, P2, f)    P1->filter[f].data = P2->filter[f].data
#define _FILTER_COPY_STATE(P1, P2, f)   P1->filter[f].state = P2->filter[f].state

#define _FILTER_GET2D(G, f, p)          _FILTER_GET(G->props2d, f, p)
#define _FILTER_GET2D_DATA(G, f)        _FILTER_GET_DATA(G->props2d, f)
#define _FILTER_GET3D(G, f, p)          _FILTER_GET(G->dprops3d, f, p)
#define _FILTER_GET3D_DATA(G, f)        _FILTER_GET_DATA(G->dprops3d, f)
#define _FILTER_SET2D(G, f, p, v)       _FILTER_SET(G->props2d, f, p, v)
#define _FILTER_SET2D_DATA(G, f, v)     _FILTER_SET_DATA(G->props2d, f, v)
#define _FILTER_SET3D(G, f, p, v)       _FILTER_SET(G->dprops3d, f, p, v)
#define _FILTER_SET3D_DATA(G, f, v)     _FILTER_SET_DATA(G->dprops3d, f, v)
#define _FILTER_COPY2D_DATA(G1, G2, f)  _FILTER_COPY_DATA(G1->props2d, G2->props2d, f)
#define _FILTER_COPY3D_DATA(G1, G2, f)  _FILTER_COPY_DATA(G1->dprops3d, G2->dprops3d, f)

#define _FILTER_GETD3D(G, f, p)         _FILTER_GET(G->props3d, f, p)
#define _FILTER_SETD3D_DATA(G, f, v)    _FILTER_SET_DATA(G->props3d, f, v)
#define _FILTER_COPYD3D_DATA(G1, G2, f) _FILTER_COPY_DATA(G1->props3d, G2->props3d, f)

#define _FILTER_SWAP_SLOT_DATA(P, f, F, s)                              \
    do { void* ptr = P->filter[f].data;                                 \
    P->filter[f].data = F->slot[s]->data; F->slot[s]->data = ptr;       \
    if (!s) aaxFilterSetState(F, P->filter[f].state); } while (0);

/* effects */
#define _EFFECT_GET_SLOT                _FILTER_GET_SLOT
#define _EFFECT_GET_SLOT_STATE          _FILTER_GET_SLOT_STATE
#define _EFFECT_GET_SLOT_DATA           _FILTER_GET_SLOT_DATA

#define _EFFECT_GET(P, f, p)            P->effect[f].param[p]
#define _EFFECT_GET_STATE(P, f)         P->effect[f].state
#define _EFFECT_GET_DATA(P, f)          P->effect[f].data
#define _EFFECT_SET(P, f, p, v)         P->effect[f].param[p] = v
#define _EFFECT_SET_STATE(P, f, v)      P->effect[f].state = v;
#define _EFFECT_SET_DATA(P, f, v)       P->effect[f].data = v
#define _EFFECT_COPY(P1, P2, f, p)      \
                                P1->effect[f].param[p] = P2->effect[f].param[p]
#define _EFFECT_COPY_DATA(P1, P2, f)    P1->effect[f].data = P2->effect[f].data

#define _EFFECT_GET2D(G, f, p)          _EFFECT_GET(G->props2d, f, p)
#define _EFFECT_GET2D_DATA(G, f)        _EFFECT_GET_DATA(G->props2d, f)
#define _EFFECT_GET3D(G, f, p)          _EFFECT_GET(G->dprops3d, f, p)
#define _EFFECT_GET3D_DATA(G, f)        _EFFECT_GET_DATA(G->dprops3d, f)
#define _EFFECT_SET2D(G, f, p, v)       _EFFECT_SET(G->props2d, f, p, v)
#define _EFFECT_SET2D_DATA(G, f, v)     _EFFECT_SET_DATA(G->props2d, f, v)
#define _EFFECT_SET3D(G, f, p, v)       _EFFECT_SET(G->dprops3d, f, p, v)
#define _EFFECT_SET3D_DATA(G, f, v)     _EFFECT_SET_DATA(G->dprops3d, f, v)
#define _EFFECT_COPY2D(G1, G2, f, p)    _EFFECT_COPY(G1->props2d, G2->props2d, f, p)
#define _EFFECT_COPY3D(G1, G2, f, p)    _EFFECT_COPY(G1->dprops3d, G2->dprops3d, f, p)
#define _EFFECT_COPY2D_DATA(G1, G2, f)  _EFFECT_COPY_DATA(G1->props2d, G2->props2d, f)
#define _EFFECT_COPY3D_DATA(G1, G2, f)  _EFFECT_COPY_DATA(G1->dprops3d, G2->dprops3d, f)

#define _EFFECT_GETD3D(G, f, p)         _EFFECT_GET(G->props3d, f, p)
#define _EFFECT_SETD3D_DATA(G, f, v)    _EFFECT_SET_DATA(G->props3d, f, v)
#define _EFFECT_COPYD3D(G1, G2, f, p)   _EFFECT_COPY(G1->props3d, G2->props3d, f, p)
#define _EFFECT_COPYD3D_DATA(G1, G2, f) _EFFECT_COPY_DATA(G1->props3d, G2->props3d, f)

#define _EFFECT_SWAP_SLOT_DATA(P, f, F, s)                              \
    do { void* ptr = P->effect[f].data;                                 \
    P->effect[f].data = F->slot[s]->data; F->slot[s]->data = ptr;       \
    if (!s) aaxEffectSetState(F, P->effect[f].state); } while (0);


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
   vec4_t speaker[_AAX_MAX_SPEAKERS];

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
   _aaxRingBufferFilterInfo filter[MAX_3D_FILTER];
   _aaxRingBufferFilterInfo effect[MAX_3D_EFFECT];

} _aax3dProps;

typedef struct
{
   _aaxMixerInfo *info;

   _aaxRingBuffer2dProps *props2d;
   _aax3dProps *props3d;

   _intBuffers *emitters_2d;	/* plain stereo emitters		*/
   _intBuffers *emitters_3d;	/* emitters with positional information	*/
   _intBuffers *frames;		/* other audio frames			*/
   _intBuffers *devices;	/* registered input devices		*/
   _intBuffers *p3dq;		/* 3d properties delay queue            */

   _aaxRingBuffer *ringbuffer;
   _intBuffers *frame_ringbuffers;	/* for audio frame rendering */
   _intBuffers *play_ringbuffers;		/* for loopback capture */

   unsigned int no_registered;
   float curr_pos_sec;

   unsigned char refcount;

   unsigned char capturing;
   signed char thread;
   void *frame_ready;		/* condition for when te frame is reeady */
 
} _aaxAudioFrame;

typedef struct
{
   _aaxMixerInfo *info;

   _aaxRingBuffer2dProps *props2d;	/* 16 byte aligned */
   _aax3dProps *props3d;

   _intBuffers *p3dq;			/* 3d properties delay queue     */
   _intBuffers *buffers;		/* audio buffer queue            */
   int buffer_pos;			/* audio buffer queue pos        */

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
extern vec4_t _aaxContextDefaultSpeakers[_AAX_MAX_SPEAKERS];
extern vec4_t _aaxContextDefaultSpeakersHRTF[_AAX_MAX_SPEAKERS];
extern char _aaxContextDefaultRouter[_AAX_MAX_SPEAKERS];

void _aaxFreeSource(void *);
void _aaxProcessSource(void *, _aaxEmitter *, unsigned int);

void _aaxSetDefaultInfo(_aaxMixerInfo *, void *);

void _aaxSetDefault2dProps(_aaxRingBuffer2dProps *);
_aax3dProps *_aax3dPropsCreate();
_aaxDelayed3dProps *_aaxDelayed3dPropsDup(_aaxDelayed3dProps*);
void _aaxSetDefaultDelayed3dProps(_aaxDelayed3dProps *);

void _aaxSetDefaultFilter2d(_aaxRingBufferFilterInfo *, unsigned int);
void _aaxSetDefaultFilter3d(_aaxRingBufferFilterInfo *, unsigned int);
void _aaxSetDefaultEffect2d(_aaxRingBufferFilterInfo *, unsigned int);
void _aaxSetDefaultEffect3d(_aaxRingBufferFilterInfo *, unsigned int);

unsigned int _aaxGetNoEmitters();
unsigned int _aaxSetNoEmitters(unsigned int);
unsigned int _aaxGetEmitter();
unsigned int _aaxPutEmitter();

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_OBJECT_H */

