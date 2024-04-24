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

#ifndef _AAX_OBJECT_H
#define _AAX_OBJECT_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <xml.h>

#include <base/geometry.h>
#include <dsp/common.h>

#define WRITE_BUFFER_TO_FILE(dptr, bufsz) \
 do { \
   int16_t q, *data = malloc((bufsz)*2); \
   for(q=0; q<(bufsz); q++) *((dptr)+q) *= 0.5; \
   _batch_cvt16_24(data, (dptr), (bufsz)); \
   _aaxFileDriverWrite("/tmp/resample.wav", AAX_OVERWRITE, data, (bufsz), 48000, 1, AAX_PCM16S); \
   free(data); \
   exit(-1); \
 } while (0);

// CUBIC_SAMPS was increased to 16 from 4 to handle HRTF properly
#define CUBIC_SAMPS		4
#define HISTORY_SAMPS		(4*CUBIC_SAMPS)

typedef int32_t	_history_t[_AAX_MAX_SPEAKERS][HISTORY_SAMPS];

/*
 * Beware: state can be both PLAYING and STOPPED, meaning the emitter is
 * ordered to stop playback but needs to handle additional samples before
 * it actually stops playback.
 */
enum
{
    //  playing = (_STATE_PAUSED|_STATE_PROCESSED) == 0
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

#define PITCH_CHANGED			0x00000001
#define GAIN_CHANGED			0x00000002
#define DIST_CHANGED			0x00000004
#define MTX_CHANGED			0x00000008
#define SPEED_CHANGED			0x00000010
#define PITCH_CHANGE			(PITCH_CHANGED | DYNAMIC_PITCH_DEFINED)

#define CONE_DEFINED			0x00010000
#define INDOOR_DEFINED			0x00020000
#define OCCLUSION_DEFINED		0x00040000
#define TIMED_GAIN_DEFINED		0x00080000
#define DYNAMIC_PITCH_DEFINED		0x00100000
#define MONO_RENDERING			0x00200000
#define ALL_DEFINED			(CONE_DEFINED|INDOOR_DEFINED|OCCLUSION_DEFINED|TIMED_GAIN_DEFINED|DYNAMIC_PITCH_DEFINED|MONO_RENDERING)

    /* SCENE */
#define DISTQUEUE_CHANGED		0x10000000
#define REVERB_CHANGED			0x20000000
#define WIND_CHANGED			0x80000000

#define _PROP3D_CLEAR(q)                ((q)->state3d &= ALL_DEFINED)
#define _PROP3D_PITCH_HAS_CHANGED(q)    ((q)->state3d & PITCH_CHANGE)
#define _PROP3D_GAIN_HAS_CHANGED(q)     ((q)->state3d & GAIN_CHANGED)
#define _PROP3D_DIST_HAS_CHANGED(q)     ((q)->state3d & DIST_CHANGED)
#define _PROP3D_MTX_HAS_CHANGED(q)      ((q)->state3d & MTX_CHANGED)
#define _PROP3D_SPEED_HAS_CHANGED(q)    ((q)->state3d & SPEED_CHANGED)
#define _PROP3D_MTXSPEED_HAS_CHANGED(q) ((q)->state3d & (SPEED_CHANGED|MTX_CHANGED))

#define _PROP3D_PITCH_SET_CHANGED(q)    ((q)->state3d |= PITCH_CHANGED)
#define _PROP3D_GAIN_SET_CHANGED(q)     ((q)->state3d |= GAIN_CHANGED)
#define _PROP3D_DIST_SET_CHANGED(q)     ((q)->state3d |= DIST_CHANGED)
#define _PROP3D_MTX_SET_CHANGED(q)      ((q)->state3d |= MTX_CHANGED)
#define _PROP3D_SPEED_SET_CHANGED(q)    ((q)->state3d |= SPEED_CHANGED)

#define _PROP3D_PITCH_CLEAR_CHANGED(q)  ((q)->state3d &= ~PITCH_CHANGED)
#define _PROP3D_GAIN_CLEAR_CHANGED(q)   ((q)->state3d &= ~GAIN_CHANGED)
#define _PROP3D_DIST_CLEAR_CHANGED(q)   ((q)->state3d &= ~DIST_CHANGED)
#define _PROP3D_MTX_CLEAR_CHANGED(q)    ((q)->state3d &= ~MTX_CHANGED)
#define _PROP3D_SPEED_CLEAR_CHANGED(q)  ((q)->state3d &= ~SPEED_CHANGED)
#define _PROP3D_MTXSPEED_CLEAR_CHANGED(q) ((q)->state3d &= ~(SPEED_CHANGED|MTX_CHANGED))

#define _PROP3D_CONE_IS_DEFINED(q)      ((q)->state3d & CONE_DEFINED)
#define _PROP3D_INDOOR_IS_DEFINED(q)    ((q)->state3d & INDOOR_DEFINED)
#define _PROP3D_OCCLUSION_IS_DEFINED(q) ((q)->state3d & OCCLUSION_DEFINED)
#define _PROP3D_DYNAMIC_PITCHIS_DEFINED(q) ((q)->state3d & DYNAMIC_PITCH_DEFINED)

#define _PROP3D_CONE_SET_DEFINED(q)     ((q)->state3d |= CONE_DEFINED)
#define _PROP3D_INDOOR_SET_DEFINED(q)   ((q)->state3d |= INDOOR_DEFINED)
#define _PROP3D_OCCLUSION_SET_DEFINED(q) ((q)->state3d |= OCCLUSION_DEFINED)
#define _PROP3D_DYNAMIC_PITCH_SET_DEFINED(q) ((q)->state3d |= DYNAMIC_PITCH_DEFINED)

#define _PROP3D_CONE_CLEAR_DEFINED(q)   ((q)->state3d &= ~CONE_DEFINED)
#define _PROP3D_INDOOR_CLEAR_DEFINED(q) ((q)->state3d &= ~INDOOR_DEFINED)
#define _PROP3D_OCCLUSION_CLEAR_DEFINED(q) ((q)->state3d &= ~OCCLUSION_DEFINED)
#define _PROP3D_DYNAMIC_PITCH_CLEAR_DEFINED(q) ((q)->state3d &= ~DYNAMIC_PITCH_DEFINED)

/* 3d properties: AAX Scene extension*/
#define _PROP3D_MONO_IS_DEFINED(q)     ((q)->state3d & MONO_RENDERING)
#define _PROP3D_REVERB_IS_DEFINED(q)    ((q)->state3d & REVERB_CHANGED)
#define _PROP3D_TIMED_GAIN_IS_DEFINED(q) ((q)->state3d & TIMED_GAIN_DEFINED)
#define _PROP3D_DISTQUEUE_IS_DEFINED(q) ((q)->state3d & DISTQUEUE_CHANGED)
#define _PROP3D_WIND_IS_DEFINED(q)      ((q)->state3d & WIND_CHANGED)

#define _PROP3D_MONO_SET_DEFINED(q)    ((q)->state3d |= MONO_RENDERING)
#define _PROP3D_REVERB_SET_CHANGED(q)   ((q)->state3d |= REVERB_CHANGED)
#define _PROP3D_TIMED_GAIN_SET_DEFINED(q) ((q)->state3d |= TIMED_GAIN_DEFINED)
#define _PROP3D_DISTQUEUE_SET_DEFINED(q) ((q)->state3d |= (DISTQUEUE_CHANGED|TIMED_GAIN_DEFINED))
#define _PROP3D_WIND_SET_CHANGED(q)     ((q)->state3d |= WIND_CHANGED)

#define _PROP3D_MONO_CLEAR_DEFINED(q)  ((q)->state3d &= ~MONO_RENDERING)
#define _PROP3D_REVERB_CLEAR_CHANGED(q) ((q)->state3d &= ~REVERB_CHANGED)
#define _PROP3D_TIMED_GAIN_CLEAR_DEFINED(q) ((q)->state3d &= ~TIMED_GAIN_DEFINED)
#define _PROP3D_DISTQUEUE_CLEAR_DEFINED(q) ((q)->state3d &= ~(DISTQUEUE_CHANGED|TIMED_GAIN_DEFINED))
#define _PROP3D_WIND_CLEAR_CHANGED(q)   ((q)->state3d &= ~WIND_CHANGED)

/* delayed 3d properties */
#define _PROP_CLEAR(q)                  _PROP3D_CLEAR((q)->dprops3d)
#define _PROP_PITCH_HAS_CHANGED(q)      _PROP3D_PITCH_HAS_CHANGED((q)->dprops3d)
#define _PROP_GAIN_HAS_CHANGED(q)       _PROP3D_GAIN_HAS_CHANGED((q)->dprops3d)
#define _PROP_DIST_HAS_CHANGED(q)       _PROP3D_DIST_HAS_CHANGED((q)->dprops3d)
#define _PROP_MTX_HAS_CHANGED(q)        _PROP3D_MTX_HAS_CHANGED((q)->dprops3d)
#define _PROP_SPEED_HAS_CHANGED(q)      _PROP3D_SPEED_HAS_CHANGED((q)->dprops3d)
#define _PROP_CONE_IS_DEFINED(q)        _PROP3D_CONE_IS_DEFINED((q)->dprops3d)
#define _PROP_INDOOR_IS_DEFINED(q)	_PROP3D_INDOOR_IS_DEFINED((q)->dprops3d)
#define _PROP_OCCLUSION_IS_DEFINED(q)   _PROP3D_OCCLUSION_IS_DEFINED((q)->dprops3d)

#define _PROP_PITCH_SET_CHANGED(q)      _PROP3D_PITCH_SET_CHANGED((q)->dprops3d)
#define _PROP_GAIN_SET_CHANGED(q)       _PROP3D_GAIN_SET_CHANGED((q)->dprops3d)
#define _PROP_DIST_SET_CHANGED(q)       _PROP3D_DIST_SET_CHANGED((q)->dprops3d)
#define _PROP_MTX_SET_CHANGED(q)        _PROP3D_MTX_SET_CHANGED((q)->dprops3d)
#define _PROP_SPEED_SET_CHANGED(q)      _PROP3D_SPEED_SET_CHANGED((q)->dprops3d)
#define _PROP_CONE_SET_DEFINED(q)       _PROP3D_CONE_SET_DEFINED((q)->dprops3d)
#define _PROP_INDOOR_SET_DEFINED(q)	_PROP3D_INDOOR_SET_DEFINED((q)->dprops3d)
#define _PROP_OCCLUSION_SET_DEFINED(q)  _PROP3D_OCCLUSION_SET_DEFINED((q)->dprops3d)
#define _PROP_DYNAMIC_PITCH_SET_DEFINED(q) _PROP3D_DYNAMIC_PITCH_SET_DEFINED((q)->dprops3d)

#define _PROP_PITCH_CLEAR_CHANGED(q)    _PROP3D_PITCH_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_GAIN_CLEAR_CHANGED(q)     _PROP3D_GAIN_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_DIST_CLEAR_CHANGED(q)     _PROP3D_DIST_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_MTX_CLEAR_CHANGED(q)      _PROP3D_MTX_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_SPEED_CLEAR_CHANGED(q)    _PROP3D_SPEED_CLEAR_CHANGED((q)->dprops3d)
#define _PROP_CONE_CLEAR_DEFINED(q)     _PROP3D_CONE_CLEAR_DEFINED((q)->dprops3d)
#define _PROP_INDOOR_CLEAR_DEFINED(q) _PROP3D_INDOOR_CLEAR_DEFINED((q)->dprops3d)
#define _PROP_OCCLUSION_CLEAR_DEFINED(q) _PROP3D_OCCLUSION_CLEAR_DEFINED((q)->dprops3d)
#define _PROP_DYNAMIC_PITCH_CLEAR_DEFINED(q) _PROP3D_DYNAMIC_PITCH_CLEAR_DEFINED((q)->dprops3d)

/* delayed 3d properties: AAX Scene extension*/
#define _PROP_MONO_IS_DEFINED(q)       _PROP3D_MONO_IS_DEFINED((q)->dprops3d)
#define _PROP_REVERB_IS_DEFINED(q)      _PROP3D_REVERB_IS_DEFINED((q)->dprops3d)
#define _PROP_TIMED_GAIN_IS_DEFINED(q)   _PROP3D_TIMED_GAIN_IS_DEFINED((q)->dprops3d)
#define _PROP_DISTQUEUE_IS_DEFINED(q)   _PROP3D_DISTQUEUE_IS_DEFINED((q)->dprops3d)
#define _PROP_WIND_IS_DEFINED(q)        _PROP3D_WIND_IS_DEFINED((q)->dprops3d)

#define _PROP_MONO_SET_DEFINED(q)      _PROP3D_MONO_SET_DEFINED((q)->dprops3d)
#define _PROP_REVERB_SET_CHANGED(q)     _PROP3D_REVERB_SET_CHANGED((q)->dprops3d)
#define _PROP_TIMED_GAIN_SET_DEFINED(q)  _PROP3D_TIMED_GAIN_SET_DEFINED((q)->dprops3d)
#define _PROP_DISTQUEUE_SET_DEFINED(q)  _PROP3D_DISTQUEUE_SET_DEFINED((q)->dprops3d)
#define _PROP_WIND_SET_CHANGED(q)       _PROP3D_WIND_SET_CHANGED((q)->dprops3d)

#define _PROP_MONO_CLEAR_DEFINED(q)    _PROP3D_MONO_CLEAR_DEFINED((q)->dprops3d)
#define _PROP_REVERB_CLEAR_CHANGED(q)   _PROP3D_REVERB_CLEAR_CHANGED(((q)->dprops3d)
#define _PROP_TIMED_GAIN_CLEAR_DEFINED(q) _PROP3D_TIMED_GAIN_CLEAR_DEFINED((q)->dprops3d)
#define _PROP_DISTQUEUE_CLEAR_DEFINED(q) _PROP3D_DISTQUEUE_CLEAR_DEFINED((q)->dprops3d)
#define _PROP_WIND_CLEAR_CHANGED(q)     _PROP3D_WIND_CLEAR_CHANGED((q)->dprops3d


enum
{
   HRTF_FACTOR = 0,
   HRTF_OFFSET
};

/* --- MIDI --- */
typedef struct
{
    /* midi support */
   float attack_factor;
   float release_factor;
   float decay_factor;

   int mode;

} _midi_t;

typedef struct {
   float velocity;			/* attack */
   float release;			/* velocity */
   float pressure;
   float soft;
} _note_t;

typedef ALIGN16  struct {
   vec3f_t box;				/* bounding box */
   float radius_sq;			/* bounding radius squared */
} bounding_t ALIGN16C;

/* warning:
 * need to update the pre defined structure in objects.c when changing
 * something here
 */
typedef ALIGN16 struct
{
   vec4f_t hrtf[2];
   vec4f_t speaker[2*_AAX_MAX_SPEAKERS];

   vec4f_t *delay;

   unsigned char router[_AAX_MAX_SPEAKERS];
   unsigned int no_samples;
   unsigned int no_tracks;
   int bitrate;
   int track;

   float pitch;
   float balance;
   float frequency;
   float period_rate;
   float refresh_rate;			/* defines the latency */
   float unit_m;			/* unit factor to convert to meters */
   enum aaxFormat format;
   enum aaxRenderMode mode;
   enum aaxCapabilities midi_mode;
   unsigned int max_emitters;		/* total */
   unsigned int max_registered;		/* per (sub)mixer */

   int capabilities;			/* CPU capabilities */
   uint8_t update_rate;	/* how many frames get processed before an update */

   unsigned int id;
   void *backend;

} _aaxMixerInfo ALIGN16C;

typedef ALIGN16 struct
{
   /* modelview matrix and velocity */
   MTX4_t matrix, imatrix;
   mtx4f_t velocity;
   vec4f_t occlusion;

   float pitch, gain;
   int state3d;

} _aaxDelayed3dProps ALIGN16C; 

typedef struct _aax3dProps_s
{
   float buf3dq_step;
   int state;

   struct _aax3dProps_s *root;
   struct _aax3dProps_s *parent;
   _aaxDelayed3dProps* dprops3d;        /* current   */
   _aaxDelayed3dProps* m_dprops3d;      /* modiefied */

   /* 3d filters and effects */
   void *mutex;
   _aaxFilterInfo filter[MAX_3D_FILTER];
   _aaxEffectInfo effect[MAX_3D_EFFECT];

} _aax3dProps;

typedef ALIGN16 struct _aax2dProps_s
{
      /* pos[0] position; -1.0 left,  0.0 center, 1.0 right */
      /* pos[1] position; -1.0 down,  0.0 center, 1.0 up    */
      /* pos[2] position; -1.0 front, 0.0 center, 1.0 back  */
   vec4f_t speaker[2*_AAX_MAX_SPEAKERS];

      /* head[0] side delay sec    */
      /* head[1] up delay sec      */
      /* head[2] forward delay sec */
      /* head[3] up offset sec     */
   vec4f_t hrtf[2];
   vec4f_t hrtf_prev[2];

   /* HRTF head shadow */
   float freqfilter_history[_AAX_MAX_SPEAKERS];
   float k;

   struct _aax2dProps_s *parent;

   /* stereo filters */
   void *mutex;
   _aaxFilterInfo filter[MAX_STEREO_FILTER];
   _aaxEffectInfo effect[MAX_STEREO_EFFECT];

   /* smooth transition between volume and pitch values */
   float prev_gain[_AAX_MAX_SPEAKERS];
   float prev_freq_fact;

   float dist_delay_sec;        /* time to keep playing after a stop request */
   float bufpos3dq;             /* distance delay queue buffer position      */

   float curr_pos_sec;
   float pitch_factor;
   int mip_levels;

   _note_t note;

   struct {
      FLOAT pitch;
      float pitch_lfo;
      float gain_lfo;
      float gain, gain_min, gain_max;
      float occlusion;		/* occlusion factor 0.0..1.0 (being hidden) */

      float k;			/* attenuation frequency filter */
      float freqfilter_history[_AAX_MAX_SPEAKERS][2];

      char silence;
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
   _intBuffers *frame_ringbuffers;	/* for audio frame rendering    */
   _intBuffers *play_ringbuffers;	/* for loopback capture         */

   unsigned int no_registered;
   int64_t curr_sample;
   float curr_pos_sec;

   unsigned char refctr;

   unsigned char capturing;

   /* parametric and graphic equalizer **/
   _aaxFilterInfo filter[EQUALIZER_MAX];
 
} _aaxAudioFrame;


typedef struct
{
   _aaxMixerInfo *info;

   _aax2dProps *props2d;		/* 16 byte aligned */
   _aax3dProps *props3d;

   _intBuffers *p3dq;			/* 3d properties delay queue     */
   _intBuffers *buffers;		/* audio buffer queue            */
   unsigned int buffer_pos;		/* audio buffer queue pos        */

   uint8_t update_rate;
   uint8_t update_ctr;

   float curr_pos_sec;

   _history_t history;

} _aaxEmitter;

struct aax_emitter_t;
struct aax_embuffer_t;

extern float _aaxDefaultHead[2][4];
extern float _aaxDefaultSpeakersVolume[_AAX_MAX_SPEAKERS][4];
extern float _aaxDefaultSpeakersDelay[_AAX_MAX_SPEAKERS][4];
extern float _aaxDefaultHRTFVolume[_AAX_MAX_SPEAKERS][4];
extern float _aaxDefaultHRTFDelay[_AAX_MAX_SPEAKERS][4];

void _aaxFreeSource(void*);
void _aaxProcessSource(void*, _aaxEmitter*, unsigned int);
int _emitterCreateEFFromRingbuffer(struct aax_emitter_t*, struct aax_embuffer_t*);
int _emitterCreateEFFromAAXS(struct aax_emitter_t*, struct aax_embuffer_t*, const char*);

void _aaxSetDefaultInfo(_aaxMixerInfo**, void*);

void _aaxSetDefault2dProps(_aax2dProps*);
void _aaxSetDefault2dFiltersEffects(_aax2dProps*);

_aax3dProps* _aax3dPropsCreate();
_aaxDelayed3dProps* _aaxDelayed3dPropsDup(_aaxDelayed3dProps*);
void _aaxSetDefaultDelayed3dProps(_aaxDelayed3dProps*);

void _aaxSetupSpeakersFromDistanceVector(vec3f_ptr, float, vec4f_ptr, _aax2dProps*, const _aaxMixerInfo*);

unsigned int _aaxGetNoMonoEmitters();
unsigned int _aaxGetNoEmitters(const _aaxDriverBackend*);
unsigned int _aaxSetNoEmitters(const _aaxDriverBackend*, unsigned int);
unsigned int _aaxIncreaseEmitterCounter(const _aaxDriverBackend*);
unsigned int _aaxDecreaseEmitterCounter(const _aaxDriverBackend*);

aaxFilter _aaxGetFilterFromAAXS(aaxConfig, const xmlId*, float, float, float, _midi_t*);
aaxEffect _aaxGetEffectFromAAXS(aaxConfig, const xmlId*, float, float, float, _midi_t*);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_OBJECT_H */

