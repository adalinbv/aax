/*
 * Copyright 2007-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <aax/aax.h>
#include <aax/eventmgr.h>
#include <aax/instrument.h>

#include <base/threads.h>
#include <base/gmath.h>

#include "ringbuffer.h"
#include "objects.h"

#ifndef RELEASE
#define USE_MIDI			AAX_FALSE
#define USE_EVENTMGR			AAX_TRUE
#else
#define USE_MIDI			AAX_FALSE
#define USE_EVENTMGR			AAX_FALSE
#endif
#define USE_SPATIAL_FOR_SURROUND	AAX_TRUE
#define	FADEDBAD			0xfadedbad

#if _WIN32
# ifndef WIN32
#  pragma waring _WIN32 defined but not WIN32
#  define WIN32			_WIN32
# endif
#endif

#define TEST_FOR_TRUE(x)	(x != AAX_FALSE)
#define TEST_FOR_FALSE(x)	(x == AAX_FALSE)

#define INFO_ID			0xFEDCBA98
//#define EBF_VALID(a)		((a)->info && ((a)->info)->id == INFO_ID)
#define EBF_VALID(a)		((a)->info && VALID_HANDLE((_handle_t*)((a)->info)->backend))


/* --- Error support -- */
#define _aaxErrorSet(a)		__aaxDriverErrorSet(handle,(a),__func__)
enum aaxErrorType __aaxDriverErrorSet(aaxConfig,enum aaxErrorType, const char*);
enum aaxErrorType __aaxErrorSet(enum aaxErrorType, const char*);


/* --- Sensor --- */
#define CAPTURE_ID	0x8FB82DEF

typedef struct
{
   _aaxAudioFrame *mixer;

   size_t count;
   size_t no_speakers;

   /* parametric equalizer, located at _handle_t **/
   _aaxFilterInfo *filter;

} _sensor_t;

/* --- Driver --- */
/* Note: several validation mechanisms rely on this value.
 *       be sure before changing.
 */
#define HANDLE_ID	0xFF2701E0
#define LITE_HANDLE_ID	0xFF270800

#define VALID_LITE_HANDLE(h)	((h) && (((h)->valid & ~AAX_TRUE)==LITE_HANDLE_ID || ((h)->valid & ~AAX_TRUE) == HANDLE_ID))
#define VALID_HANDLE(h)		((h) && ((h)->valid & ~AAX_TRUE) == HANDLE_ID)
#define VALID_MIXER(h)		(VALID_HANDLE(h) && ((h)->valid & AAX_TRUE))
#define INTERVAL(a)		(rintf((a)/64)*64)

extern int __release_mode;
extern _aaxMixerInfo* _info;

struct backend_t
{
   char *driver;
   void *handle;
   const _aaxDriverBackend *ptr;
};

struct threat_t
{
   void *ptr;
   _aaxSignal signal;
   char started;
   char initialized;
};

typedef struct
{
   unsigned int id;

   int valid;
   int state;
   enum aaxErrorType error;
   unsigned int be_pos;
   unsigned int mixer_pos;
   int registered_sensors;
   void* handle;		/* assigned when registered to a (sub)mixer */

   char *devname[2];
   _aaxMixerInfo *info;
   _intBuffers *sensors;		/* locked sensor and scene properies */
   _intBuffers *backends;
   struct backend_t backend;
   struct backend_t file;		/* file recording backend */
   struct threat_t thread;
   _aaxSignal buffer_ready;
   _aaxSemaphore *finished;		/* released after a rendering period */

   /* destination ringbuffer */
   _aaxRingBuffer *ringbuffer;
   float dt_ms;

   /* timing */
   _aaxTimer *timer;
   float elapsed;
 
   /* parametric equalizer **/
   _aaxFilterInfo filter[EQUALIZER_MAX];

   void *eventmgr;

} _handle_t;

_handle_t* new_handle();
_handle_t* get_handle(aaxConfig, const char*);
_handle_t* get_valid_handle(aaxConfig, const char*);
_handle_t* get_read_handle(aaxConfig, const char*);
_handle_t* get_write_handle(aaxConfig, const char*);
void _aaxDriverFree(void*);

/* --- AudioFrame --- */
#define AUDIOFRAME_ID   0x3137ABFF

typedef struct
{
   int id;

   int state;
   unsigned int mixer_pos;
   unsigned int cache_pos;	/* position in the eventmgr emitter cache   */
   void* handle;		/* assigned when registered to a (sub)mixer */

   _aaxAudioFrame *submix;

   /* parametric equalizer **/
   _aaxFilterInfo *filter;

} _frame_t;

_frame_t* get_frame(aaxFrame, const char*);
void put_frame(aaxFrame);
_handle_t *get_driver_handle(aaxFrame);
int _aaxAudioFrameStop(_frame_t*);
void* _aaxAudioFrameThread(void*);
void* _aaxAudioFrameProcessThreadedFrame(_handle_t*, void*, _aaxAudioFrame*, _aaxAudioFrame*, _aaxAudioFrame*, const _aaxDriverBackend*);
void _aaxAudioFrameProcessFrame(_handle_t*, _frame_t*, _aaxAudioFrame*, _aaxAudioFrame*, _aaxAudioFrame*, const _aaxDriverBackend*);
char _aaxAudioFrameProcess(_aaxRingBuffer*, _frame_t*, void*, _aaxAudioFrame*, float, float, _aax2dProps*, _aaxDelayed3dProps*, _aax2dProps*, _aaxDelayed3dProps*, _aaxDelayed3dProps*, const _aaxDriverBackend*, void*, char, char);
char _aaxAudioFrameRender(_aaxRingBuffer *, _aaxAudioFrame *, _aax2dProps*, _aaxDelayed3dProps *, _intBuffers *, unsigned int, float, float, const _aaxDriverBackend*, void*, char);
void _aaxAudioFrameProcessDelayQueue(_aaxAudioFrame *);
void _aaxAudioFrameResetDistDelay(_aaxAudioFrame*, _aaxAudioFrame*);
void _aaxAudioFrameMix(_aaxRingBuffer*, _intBuffers *, _aax2dProps*, const _aaxDriverBackend*, void*);
void _aaxAudioFrameFree(void*);

/* --- Instrument --- */
#define INSTRUMENT_ID	0x0EB9A645

typedef struct
{
    aaxBuffer buffer;
    struct {
       unsigned int min;
       unsigned int max;
    } note;
} _timbre_t;

typedef struct
{
    aaxEmitter emitter;
    float pitchbend;
    float velocity;
    float aftertouch;
    float displacement;
//  float sustain;
//  float soften;
} _note_t;

typedef struct
{
    int id;
    void *handle;
    aaxFrame frame;

    float soften;
    float sustain;
    unsigned int polyphony;
    _intBuffers *note;
//  enum aaxFormat format;
//  unsigned int frequency_hz;
//  aaxFilter filter[AAX_FILTER_MAX];
//  aaxEffect effect[AAX_EFFECT_MAX];
} _controller_t;

typedef struct
{
    _intBuffers *timbres;
    _intBuffers *controllers;
} _soundbank_t;

_controller_t *get_controller(aaxController);
_controller_t *get_valid_controller(aaxController);

/* --- Buffer --- */
#define BUFFER_ID       0x81ACFE07

#define DEFAULT_IMA4_BLOCKSIZE		36
#define IMA4_SMP_TO_BLOCKSIZE(a)	(((a)/2)+4)
#define IMA4_BLOCKSIZE_TO_SMP(a)	((a) > 1) ? (((a)-4)*2) : 1

typedef struct
{
   unsigned int id;	/* always first */
   unsigned int ref_counter;

   int blocksize;
   unsigned int pos;
   unsigned int no_tracks, no_samples;
   unsigned int loop_start, loop_end;
   enum aaxFormat format;
   float frequency;

   char mipmap;

   _aaxRingBuffer *ringbuffer;
   _aaxMixerInfo **info;
   void *handle;
} _buffer_t;

_buffer_t* new_buffer(_handle_t*, unsigned int, enum aaxFormat, unsigned);
_buffer_t* get_buffer(aaxBuffer, const char*);
int free_buffer(_buffer_t*);


int _aaxFileDriverWrite(const char*, enum aaxProcessingType, void*, unsigned int, unsigned int, char, enum aaxFormat);

/* --- Emitter --- */
#define EMITTER_ID	0x17F533AA
#define EMBUFFER_ID	0xABD82641

typedef struct
{
   unsigned int id;	/* always first */

   unsigned int mixer_pos;	/* rigsitered emitter position            */
   unsigned int cache_pos;	/* position in the eventmgr emitter cache */
   unsigned char track;		/* which track to use from the buffer     */
   char looping;

   _aaxEmitter *source;
   void *handle;	/* assigned when registered to the mixer */

} _emitter_t;

typedef struct
{
   unsigned int id;	/* always first */

   _buffer_t *buffer;
   _aaxRingBuffer *ringbuffer;
} _embuffer_t;

_emitter_t* get_emitter(aaxEmitter, const char*);
_emitter_t* get_emitter_unregistered(aaxEmitter, const char*);
void put_emitter(aaxEmitter);
int destory_emitter(aaxEmitter);
void emitter_remove_buffer(_aaxEmitter *);

void _aaxEMitterResetDistDelay(_aaxEmitter*, _aaxAudioFrame*);

/* -- Filters and Effects -- */

#define FILTER_ID	0x887AFE21
#define EFFECT_ID	0x21EFA788

/* --- Events --- */
#define EVENT_ID	0x9173652A

typedef struct
{
   int id;
   unsigned int emitter_pos;
   enum aaxEventType event;
   void *data;
   aaxEventCallbackFn callback;
   void *user_data;
} _event_t;

typedef struct
{
   void *handle;
   _intBuffers *buffers;
   _intBuffers *emitters;
   _intBuffers *frames;

   struct threat_t thread;

} _aaxEventMgr;

void* _aaxEventThread(void*);

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
    _AAX_DELAYED3D,
    _AAX_BUFFER_CACHE,
    _AAX_EMITTER_CACHE,
    _AAX_FRAME_CACHE,
    _AAX_EVENT_QUEUE,

    _AAX_NOTE,

    _AAX_MAX_ID
};

extern const char* _aax_id_s[_AAX_MAX_ID];

#ifndef NDEBUG
# define LOG_LEVEL	LOG_ERR
# define _AAX_LOG(a, c)	__aax_log((a),0,(const char*)(c), _aax_id_s, LOG_LEVEL)
# define _AAX_SYSLOG(c) __aax_log(LOG_SYSLOG, 0, (c), _aax_id_s, LOG_SYSLOG)
#else
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <string.h>
#endif
# define _AAX_LOG(a, c)
# define _AAX_SYSLOG(c) __aax_log(LOG_SYSLOG, 0, (c), _aax_id_s, LOG_SYSLOG)
#endif

#ifndef NDEBUG
# define DBG_TESTNAN(a, b)		do { int i; for (i=0;i<(b);i++) if (is_nan((a)[i])) { printf("%s line %i\n\tNaN detetced at pos %i\n", __FILE__, __LINE__, i); exit(-1); } } while(0);
# define DBG_MEMCLR(a, b, c, d)         if (a) memset((void*)(b), 0, (c)*(d))
# define WRITE(a, b, dptr, ds, no_samples) \
   if (a) { static int ct = 0; if (++ct > (b)) { \
             WRITE_BUFFER_TO_FILE(dptr-ds, ds+no_samples); } }
#else
# define DBG_TESTNAN(a, b)
# define DBG_MEMCLR(a, b, c, d)
# define WRITE(a, b, dptr, ds, no_samples) \
        printf("Need to turn on debugging to use the WRITE macro\n")
#endif


/* --- System Specific & Config file related  --- */

void _aaxBackendDriverToDeviceConnecttor(char **, char **);
void _aaxDeviceConnecttorToBackendDriver(char **, char **);
void _aaxConnectorDeviceToDeviceConnector(char *);

const char* userHomeDir();
char* systemConfigFile(const char*);
char* userConfigFile();
char* systemLanguage(char**);

#ifdef WIN32
char* _aaxGetEnv(const char*);
int _aaxSetEnv(const char*, const char*, int);
int _aaxUnsetEnv(const char*);
#else
# define _aaxGetEnv(a)			getenv(a)
# define _aaxSetEnv(a,b,c)		setenv((a),(b),(c))
# define _aaxUnsetEnv(a)		unsetenv(a)
#endif

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif

