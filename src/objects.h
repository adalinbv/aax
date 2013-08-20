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

#include <base/geometry.h>

#include <ringbuffer.h>

#include <software/audio.h>
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
#define _IS_PROCESSED(q)     (((q)->state & _STATE_INITIAL) == _STATE_PROCESSED)
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

typedef struct
{
   _aaxMixerInfo *info;

   _oalRingBuffer2dProps *props2d;
   _oalRingBuffer3dProps *props3d;

   _intBuffers *emitters_2d;	/* plain stereo emitters		*/
   _intBuffers *emitters_3d;	/* emitters with positional information	*/
   _intBuffers *frames;		/* other audio frames			*/
   _intBuffers *devices;	/* registered input devices		*/
   _intBuffers *p3dq;		/* 3d properties delay queue            */

   _oalRingBuffer *ringbuffer;
   _intBuffers *frame_ringbuffers;	/* for audio frame rendering */
   _intBuffers *ringbuffers;		/* for loopback capture */

   unsigned int no_registered;
   float curr_pos_sec;

   unsigned char dist_delaying;
   unsigned char refcount;

   unsigned char capturing;
   signed char thread;
   void *frame_ready;		/* condition for when te frame is reeady */
 
} _aaxAudioFrame;

typedef struct
{
   _aaxMixerInfo *info;

   _oalRingBuffer2dProps *props2d;	/* 16 byte aligned */
   _oalRingBuffer3dProps *props3d;

   _intBuffers *p3dq;			/* 3d properties delay queue     */
   _intBuffers *buffers;		/* audio buffer queue            */
   int pos;				/* audio buffer queue pos        */

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

void _aaxRemoveSourceByPos(void *, unsigned int);
void _aaxRemoveRingBufferByPos(void *, unsigned int);
void _aaxRemoveFrameRingBufferByPos(void *, unsigned int);
void _aaxProcessSource(void *, _aaxEmitter *, unsigned int);

void _aaxSetDefaultInfo(_aaxMixerInfo *, void *);

void _aaxSetDefault2dProps(_oalRingBuffer2dProps *);
_oalRingBuffer3dProps *_aax3dPropsCreate();
_oalRingBufferDelayed3dProps *_aaxDelayed3dPropsDup(_oalRingBufferDelayed3dProps*);
void _aaxSetDefaultDelayed3dProps(_oalRingBufferDelayed3dProps *);
void removeDelayed3dQueueByPos(void *, unsigned int);

void _aaxSetDefaultFilter2d(_oalRingBufferFilterInfo *, unsigned int);
void _aaxSetDefaultFilter3d(_oalRingBufferFilterInfo *, unsigned int);
void _aaxSetDefaultEffect2d(_oalRingBufferFilterInfo *, unsigned int);
void _aaxSetDefaultEffect3d(_oalRingBufferFilterInfo *, unsigned int);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_OBJECT_H */

