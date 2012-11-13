/*
 * Copyright 2007-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef AAX_API_H
#define AAX_API_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include <aax/aax.h>

#include <driver.h>
#include <objects.h>
#include <ringbuffer.h>


#define TEST_FOR_TRUE(x)	(x != AAX_FALSE)
#define TEST_FOR_FALSE(x)	(x == AAX_FALSE)

#define INFO_ID			0xFEDCBA98
#define EBF_VALID(a)		(a->info && a->info->id == INFO_ID)


/* --- Error support -- */
#ifndef __FUNCTION__
# define __FUNCTION__	__func__
#endif
#define _aaxErrorSet(a)		__aaxErrorSet(a,__FUNCTION__)
enum aaxErrorType __aaxErrorSet(enum aaxErrorType, const char*);


/* --- Sensor --- */
#define CAPTURE_ID	0x8FB82DEF

typedef struct
{
   _aaxAudioFrame *mixer;

   size_t count;
   size_t no_speakers;

   /* parametric equalizer, located at _handle_t **/
   _oalRingBufferFilterInfo *filter;

} _sensor_t;

void _aaxSensorsProcess(_oalRingBuffer*, const _intBuffers*,
                        _oalRingBuffer2dProps*);
void *_aaxSensorCapture(_oalRingBuffer*, const _aaxDriverBackend*, void*,
                        float*, float);

/* --- Driver --- */
/* Note: several validation mechanisms rely on this value.
 *       be sure before changing.
 */
#define HANDLE_ID	0xFF2701E0

#define VALID_HANDLE(handle)	(handle && (handle->valid & ~AAX_TRUE) == HANDLE_ID)

struct backend_t
{
   char *driver;
   void *handle;
   const _aaxDriverBackend *ptr;
};

struct threat_t
{
   void *ptr;
   void *mutex;
   void *condition;
   char started;
};

typedef struct
{
   unsigned int id;

   int pos;
   int valid;
   int state;
   unsigned int be_pos;
   void* handle;		/* assigned when registered to a (sub)mixer */

   char *devname[2];
   _aaxMixerInfo *info;
   _intBuffers *sensors;		/* locked sensor and scene properies */
   _intBuffers *backends;
   struct backend_t backend;
   struct threat_t thread;

   /* destination ringbuffer */
   _oalRingBuffer *ringbuffer;
 
   /* parametric equalizer **/
   _oalRingBufferFilterInfo filter[EQUALIZER_MAX];

} _handle_t;

_handle_t* new_handle();
_handle_t* get_handle(aaxConfig);
_handle_t* get_valid_handle(aaxConfig);
_handle_t* get_read_handle(aaxConfig);
_handle_t* get_write_handle(aaxConfig);

/* --- AudioFrame --- */
#define AUDIOFRAME_ID   0x3137ABFF

typedef struct
{
   int id;

   int state;
   unsigned int pos;
   void* handle;	/* assigned when registered to a (sub)mixer */

   _aaxAudioFrame *submix;
   _oalRingBuffer *ringbuffer;

   struct threat_t thread;

} _frame_t;

_frame_t* get_frame(aaxFrame);
void put_frame(aaxFrame);
int _aaxAudioFrameStop(_frame_t*);
void* _aaxAudioFrameThread(void*);

void _aaxAudioFrameProcessFrame(_handle_t*, _frame_t*, _aaxAudioFrame*, _aaxAudioFrame*, _aaxAudioFrame*, const _aaxDriverBackend*);
void _aaxAudioFrameMix(_oalRingBuffer*, _intBuffers *, unsigned char*, _oalRingBuffer2dProps*, const _aaxDriverBackend*, void*);

/* --- Instrument --- */
#define INSTRUMENT_ID	0x0EB9A645

typedef struct
{
    aaxBuffer buffer;
    float pitch;
} _inst_sound_t;

typedef struct
{
    aaxEmitter emitter;
    float pitchbend;
    float velocity;
    float aftertouch;
    float displacement;
//  float sustain;
//  float soften;
} _inst_note_t;

typedef struct
{
    int id;
    void *handle;
    enum aaxFormat format;
    unsigned int frequency_hz;

    unsigned int sound_min;
    unsigned int sound_max;
    unsigned int sound_step;
    _inst_sound_t **sound;

    float soften;
    float sustain;
    unsigned int polyphony;
    _inst_note_t **note;

    aaxFilter filter[AAX_FILTER_MAX];
    aaxEffect effect[AAX_EFFECT_MAX];
    aaxFrame frame;

} _instrument_t;

_instrument_t* get_instrument(aaxInstrument);
_instrument_t* get_valid_instrument(aaxInstrument);

/* --- Buffer --- */
#define BUFFER_ID       0x81ACFE07

typedef struct
{
   unsigned int id;	/* always first */
   unsigned int ref_counter;

   int blocksize;
   unsigned int pos;
   enum aaxFormat format;
   float frequency;

   char mipmap;

   _oalRingBuffer *ringbuffer;
   _aaxMixerInfo *info;
   _aaxCodec** codecs;
} _buffer_t;

_buffer_t* new_buffer(_handle_t*, unsigned int, enum aaxFormat, unsigned);
_buffer_t* get_buffer(aaxBuffer);
int free_buffer(_buffer_t*);


void _bufferMixWhiteNoise(void**, unsigned int, char, int, float, float, unsigned char);
void _bufferMixPinkNoise(void**, unsigned int, char, int, float, float, float, unsigned char);
void _bufferMixBrownianNoise(void**, unsigned int, char, int, float, float, float, unsigned char);
void _bufferMixSineWave(void**, float, char, unsigned int, int, float, float);
void _bufferMixSquareWave(void**, float, char, unsigned int, int, float, float);
void _bufferMixTriangleWave(void**, float, char, unsigned int, int, float, float);
void _bufferMixSawtooth(void**, float, char, unsigned int, int, float, float);
void _bufferMixImpulse(void**, float, char, unsigned int, int, float, float);


/* --- Emitter --- */
#define EMITTER_ID	0x17F533AA
#define EMBUFFER_ID	0xABD82641

typedef struct
{
   unsigned int id;	/* always first */

   unsigned int pos;
   unsigned char track;	/* specifies which track to use from the buffer */
   char looping;

   _aaxEmitter *source;
   void *handle;	/* assigned when registered to the mixer */

} _emitter_t;

typedef struct
{
   unsigned int id;	/* always first */

   _buffer_t *buffer;
   _oalRingBuffer *ringbuffer;
} _embuffer_t;

_emitter_t* get_emitter(aaxEmitter);
_emitter_t* get_emitter_unregistered(aaxEmitter);
void put_emitter(aaxEmitter);
int destory_emitter(aaxEmitter);
void emitter_remove_buffer(_aaxEmitter *);

char _aaxEmittersProcess(_oalRingBuffer*, _aaxMixerInfo*,
                         _oalRingBuffer2dProps*, _oalRingBuffer3dProps*,
                         _oalRingBuffer2dProps*, _oalRingBuffer3dProps*,
                         _intBuffers*, _intBuffers*,
                         const _aaxDriverBackend*, void*);


/* -- Filters and Effects -- */

#define FILTER_ID	0x887AFE21
#define EFFECT_ID	0x21EFA788

typedef struct
{
   int id;
   int pos;
   int state;
   enum aaxFilterType type;
   _oalRingBufferFilterInfo* slot[_MAX_SLOTS];
   _aaxMixerInfo* info;
} _filter_t;

_filter_t* new_filter(_aaxMixerInfo*, enum aaxFilterType);
_filter_t* new_filter_handle(_aaxMixerInfo*, enum aaxFilterType, _oalRingBuffer2dProps*, _oalRingBuffer3dProps*);
_filter_t* get_filter(aaxFilter);

_filter_t* new_effect(_aaxMixerInfo*, enum aaxEffectType);
_filter_t* new_effect_handle(_aaxMixerInfo*, enum aaxEffectType, _oalRingBuffer2dProps*, _oalRingBuffer3dProps*);
_filter_t* get_effect(aaxEffect);
_filter_t* get_effect(aaxEffect);

/* --- WaveForms --- */
void *_bufferWaveCreateWhiteNoise(float, int, float, unsigned int*);
void *_bufferWaveCreatePinkNoise(float, int, float, unsigned int*);
void *_bufferWaveCreateSine(float, float, int, float, unsigned int*);
void *_bufferWaveCreateTriangle(float, float, int, float, unsigned int*);
void *_bufferWaveCreateSquare(float, float, int, float, unsigned int*);
void *_bufferWaveCreateSawtooth(float, float, int, float, unsigned int*);
void *_bufferWaveCreateImpulse(float, float, int, float, float, unsigned int*);


/* --- Logging --- */
# include <base/logging.h>

enum
{
    _AAX_NONE = 0,
    _AAX_BACKEND,
    _AAX_DEVICE,
    _AAX_BUFFER,
    _AAX_EMITTER,
    _AAX_EMITTER_BUFFER,
    _AAX_SENSOR,
    _AAX_FRAME,
    _AAX_RINGBUFFER,
    _AAX_EXTENSION,

    _AAX_MAX_ID
};

extern const char* _aax_id_s[_AAX_MAX_ID];

#ifndef NDEBUG
# define LOG_LEVEL	LOG_ERR
# define _AAX_LOG(a, c)	__oal_log((a),0,(const char*)(c), _aax_id_s, LOG_LEVEL)
# define _AAX_SYSLOG(c) __oal_log(LOG_SYSLOG, 0, (c), _aax_id_s, LOG_SYSLOG)
#else
# include <stdlib.h>
# include <string.h>
# define _AAX_LOG(a, c)
# define _AAX_SYSLOG(c) __oal_log(LOG_SYSLOG, 0, (c), _aax_id_s, LOG_SYSLOG)
#endif

/* --- Config Files --- */
const char* userHomeDir();
char* systemConfigFile();
char* userConfigFile();

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif

