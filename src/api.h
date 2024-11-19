/*
 * SPDX-FileCopyrightText: Copyright © 2007-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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

#include <base/xthreads.h>
#include <base/gmath.h>

#include "ringbuffer.h"
#include "objects.h"

#define	FADEDBAD			0xfadedbad

#if _WIN32
# ifndef WIN32
#  pragma waring _WIN32 defined but not WIN32
#  define WIN32			_WIN32
# endif
#endif

#define TEST_FOR_TRUE(x)	(x != false)
#define TEST_FOR_FALSE(x)	(x == false)

#define RENDER_NORMAL(a)	((a)==AAX_RENDER_NORMAL || (a)==AAX_RENDER_DEFAULT)

#define INFO_ID			0xFEDCBA98
//#define EBF_VALID(a)		((a)->info && ((a)->info)->id == INFO_ID)
#define EBF_VALID(a)		((a)->info && VALID_HANDLE((_handle_t*)((a)->info)->backend))


#define _READ			false
#define _WRITE			true

#define _NOLOCK			false
#define _LOCK			true

#if defined(_MSC_VER)
# ifdef _WIN64
#  define AAX_SET_FLUSH_ZERO_ON  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON)
# else
#  define AAX_SET_FLUSH_ZERO_ON
# endif
#elif defined(__x86_64__) || defined(__i386__)
# define AAX_SET_FLUSH_ZERO_ON  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON)
#elif defined(__ARM_NEON)
# define AAX_SET_FLUSH_ZERO_ON
#endif


/* --- Error support -- */
#define _aaxErrorSet(a)		__aaxDriverErrorSet(handle,(a),__func__)
enum aaxErrorType __aaxDriverErrorSet(void*,enum aaxErrorType, const char*);
enum aaxErrorType __aaxErrorSet(enum aaxErrorType, const char*);

unsigned long long _aax_get_free_memory(void);

/* --- Sensor --- */
#define CAPTURE_ID	0x8FB82DEF

typedef struct
{
   _aaxAudioFrame *mixer;

   /* band-pass filter between 20 Hz and 20kHz **/
   _aaxRingBufferFreqFilterData *filter[2];
   float rms[RB_MAX_TRACKS][_AAX_MAX_EQBANDS];
   float peak[RB_MAX_TRACKS][_AAX_MAX_EQBANDS];
   void *mutex;

} _sensor_t;


/* --- Driver --- */
/* Note: several validation mechanisms rely on this value.
 *       be sure before changing.
 */
#define HANDLE_ID	0xFF2701E0

#define VALID_HANDLE(h)		((h) && ((h)->valid & ~true) == HANDLE_ID)
#define VALID_MIXER(h)		(VALID_HANDLE(h) && ((h)->valid & true))
#define INTERVAL(a)		(rintf((a)/64)*64)

extern bool __release_mode;
extern _aaxMixerInfo* _info;
extern _aaxMixerInfo __info;

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
   bool started;
   bool initialized;
};

struct _meta_t
{
   bool id3_found;

   bool artist_changed;
   bool title_changed;

   char *artist;
   char *original;
   char *title;
   char *album;
   char *trackno;
   char *date;
   char *genre;
   char *composer;
   char *comments;
   char *copyright;
   char *contact;
   char *website;
   char *image;
};

void _aax_free_meta(struct _meta_t*);

typedef struct aax_handle_t
{
   unsigned int id;

   unsigned int valid;
   int state;
   enum aaxErrorType error;
   unsigned int be_pos;
   unsigned int mixer_pos;
   int registered_sensors;
   void *parent;		/* assigned when registered to a (sub)mixer  */
   struct aax_handle_t *root;	/* reference to the mixer object             */
   struct aax_handle_t *handle;

   char *devname[2];
   char *renderer;
   char *data_dir;
   _aaxMixerInfo *info;
   _intBuffers *sensors;		/* locked sensor and scene properies */
   _intBuffers *backends;
   struct backend_t backend;
   struct backend_t file;		/* file recording backend */
   struct _meta_t meta;

   struct threat_t thread;
   _aaxSignal buffer_ready;
   _aaxSemaphore *batch_finished;	/* released after a rendering period */

   /* destination ringbuffer */
   _aaxRingBuffer *ringbuffer;
   float dt_ms;

   /* timing */
   _aaxTimer *timer;
   float elapsed;

   /* buffer for AAXS defined filters and effects */
   aaxBuffer buffer;

   /* emitter thread for AAXS defined waveform generation */
   struct threat_t buffer_thread;

   /* thread for AAXS defined waveform generation */
   struct threat_t emitter_thread;

} _handle_t;

_handle_t* new_handle(void);
_handle_t* get_handle(aaxConfig, const char*);
_handle_t* get_valid_handle(aaxConfig, const char*);
_handle_t* get_read_handle(aaxConfig, const char*);
_handle_t* get_write_handle(aaxConfig, const char*);
void _aaxDriverFree(void*);
void _aaxMixerSetRendering(_handle_t*);

/* --- AudioFrame --- */
#define AUDIOFRAME_ID   0x8AE7A22F
#define AUDIOFRAME_MAX_PARENTS	4

typedef struct aax_frame_t
{
   unsigned int id;

   unsigned int max_emitters;
   unsigned int num_parents;
   unsigned int mixer_pos[AUDIOFRAME_MAX_PARENTS];

   /* assigned when registered to a (sub)mixer */
   void *parent[AUDIOFRAME_MAX_PARENTS];
   _handle_t *root;		/* reference to the mixer object            */
   _handle_t *handle;

   _aaxAudioFrame *submix;

   bool registered_at_mixer;
   bool mtx_set;

   /* parametric and graphic equalizer **/
   void *mutex;

} _frame_t;

_frame_t* get_frame(aaxFrame, int, const char*);
void put_frame(aaxFrame);
_handle_t *get_driver_handle(const void*);
void _aaxAudioFrameResetDistDelay(_aaxAudioFrame*, _aaxAudioFrame*);
void _aaxAudioFrameFree(void*);

/* --- Buffer --- */
#define BUFFER_ID       0x81ACFE07

#define DEFAULT_IMA4_BLOCKSIZE		36
#define IMA4_SMP_TO_BLOCKSIZE(a)	(((a)/2)+4)
#define IMA4_BLOCKSIZE_TO_SMP(a)	((a) > 1) ? (((a)-4)*2) : 1
#define MAX_MIP_LEVELS			8

typedef enum
{
   WAVEFORM_LIMIT_NORMAL  = 0x00,
   WAVEFORM_LIMIT_DELAYED = 0x01,
   WAVEFORM_LIMIT_STRONG  = 0x02
} limitType;

typedef struct
{
   enum aaxFormat fmt; 
   unsigned int no_tracks;
   unsigned int blocksize;
   size_t no_blocks;
   size_t no_bytes;
   size_t no_samples;
   off_t loop_count;
   float loop_start;
   float loop_end;

   float rate;
   float base_frequency;
   float low_frequency;
   float high_frequency;
   float pitch_fraction;
   float pan;

   struct {
      float rate;
      float depth;
      float sweep;
   } tremolo, vibrato;

   float volume_envelope[2*_MAX_ENVELOPE_STAGES];
   bool envelope_sustain;
   bool sampled_release;
   bool fast_release;

   char no_patches;

   struct {
      char mode;
      float factor;
   } pressure;

   struct {
      char mode;
      float factor;
      float rate;
   } modulation;

   int polyphony;

} _buffer_info_t;

typedef struct aax_buffer_t
{
   unsigned int id;	/* always first */
   unsigned int ref_counter;

   _buffer_info_t info;
   unsigned int sample_num;

   unsigned int pos;
   float rms, peak, gain;

   enum aaxCapabilities midi_mode;
   bool to_mixer;
   bool mipmap;

   char mip_levels;
   _aaxRingBuffer *ringbuffer[MAX_MIP_LEVELS];
   _aaxMixerInfo **mixer_info;
   _handle_t *root;		/* reference to the mixer object */
   _handle_t *handle;
   void *aaxs;
   void *url;

} _buffer_t;

typedef struct
{
   _buffer_t* parent;
   const void *aaxs;
   float frequency;
   enum aaxErrorType error;

   struct _meta_t meta;

} _buffer_aax_t;

_buffer_t* new_buffer(_handle_t*, unsigned int, enum aaxFormat, unsigned);
_buffer_t* get_buffer(aaxBuffer, const char*);
int free_buffer(_buffer_t*);

int _getMaxMipLevels(int);
char** _bufGetDataFromStream(_handle_t*, const char*, _buffer_info_t*, _aaxMixerInfo*);
void _aaxFileDriverWrite(const char*, enum aaxProcessingType, void*, size_t, size_t, char, enum aaxFormat);

/* --- Emitter --- */
#define EMITTER_ID	0x17F533AA
#define EMBUFFER_ID	0xABD82641

typedef struct aax_emitter_t
{
   unsigned int id;	/* always first */

   unsigned int mixer_pos;	/* rigsitered emitter position            */
   unsigned char track;		/* which track to use from the buffer     */
   signed char looping;
   bool sampled_release;
   bool mtx_set;

   _aaxEmitter *source;

   void *parent;		/* assigned when registered to a (sub)mixer */
   _handle_t *root;		/* reference to the mixer                   */
   _handle_t *handle;

   _midi_t midi;

} _emitter_t;

typedef struct aax_embuffer_t
{
   unsigned int id;	/* always first */

   void *aaxs;
   _buffer_t *buffer;
   _aaxRingBuffer *ringbuffer;
} _embuffer_t;

_emitter_t* get_emitter(aaxEmitter, int, const char*);
_emitter_t* get_emitter_unregistered(aaxEmitter, const char*);
void put_emitter(aaxEmitter);
int destory_emitter(aaxEmitter);
void emitter_remove_buffer(_aaxEmitter *);

void _aaxEMitterResetDistDelay(_aaxEmitter*, _aaxAudioFrame*);

/* -- Filters and Effects -- */

#define FILTER_ID	0x88F172E1
#define EFFECT_ID	0x21EFFEC8

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
# define DBG_TESTZERO(a, b)		do { unsigned int i,z=(b); for (i=0;i<(b);i++) if ((a)[i] == 0) --z; if (!z) printf("%s line %i\n\tbuffer is all zero's\n", __FILE__, __LINE__ ); } while(0);
# define DBG_TESTNAN(a, b)		do { unsigned int i; for (i=0;i<(b);i++) if (is_nan((a)[i])) { printf("%s line %i\n\tNaN detetced at pos %i\n", __FILE__, __LINE__, i); exit(-1); } } while(0);
# define DBG_MEMCLR(a, b, c, d)         if (a) memset((void*)(b), 0, (c)*(d))
# define WRITE(a, b, dptr, ds, no_samples) \
   if (a) { static int ct = 0; if (++ct > (b)) { \
             WRITE_BUFFER_TO_FILE(dptr-ds, ds+no_samples); } }
#else
# define DBG_TESTZERO(a, b)
# define DBG_TESTNAN(a, b)
# define DBG_MEMCLR(a, b, c, d)
# define WRITE(a, b, dptr, ds, no_samples) \
        printf("Need to turn on debugging to use the WRITE macro\n")
#endif


/* --- System Specific & Config file related  --- */

void _aaxBackendDriverToDeviceConnecttor(char **, char **);
void _aaxDeviceConnecttorToBackendDriver(char **, char **);
void _aaxConnectorDeviceToDeviceConnector(char *);
void _aaxURLSplit(char*, char**, char**, char**, char**, int*);
char *_aaxURLConstruct(char*, char*);

int mkDir(const char*);
size_t getFileSize(const char*);

int isSafeDir(const char*, char);

char* systemLanguage(char**);

const char* tmpDir(void);
const char* userHomeDir(void);
char* systemDataFile(const char*);
char* systemConfigFile(const char*);
char* userConfigFile(void);
char* userCacheFile(const char*);

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

