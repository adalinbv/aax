/*
 * Copyright 2007-2012 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef AAX_H
#define AAX_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if defined _WIN32 || defined __CYGWIN__
# define AAX_APIENTRY __cdecl
# ifdef AAX_BUILD_LIBRARY
#  define AAX_API __declspec(dllexport)
# else
#  define AAX_API __declspec(dllimport)
# endif
#else
# define AAX_APIENTRY
# if (__GNUC__ >= 4) || defined(__TINYC__)
#  define AAX_API __attribute__((visibility("default")))
# else
#  define AAX_API extern
# endif
#endif

#if defined(TARGET_OS_MAC) && TARGET_OS_MAC
# pragma export on
#endif


#define AAX_MAJOR_VERSION	2
#define AAX_MINOR_VERSION	6
#define AAX_MICRO_VERSION	0
#define AAX_PATCH_LEVEL		150805

#define AAX_FALSE		0
#define AAX_TRUE		1

#ifdef _MSC_VER
# include <float.h>
# pragma warning( disable : 4056 )
# define AAX_FPINFINITE		(FLT_MAX+FLT_MAX)
# define AAX_FPNONE		(AAX_FPINFINITE-AAX_FPINFINITE)
#else
# define AAX_FPNONE		(0.0f/0.0f)
# define AAX_FPINFINITE		(1.0f/0.0f)
#endif
#define AAX_INVERSE		0x10000000

enum aaxHandleType
{
    AAX_NONE = 0,
    AAX_CONFIG,
    AAX_BUFFER,
    AAX_EMITTER,
    AAX_AUDIOFRAME,
    AAX_FILTER,
    AAX_EFFECT,
    AAX_CONFIG_HD,
    AAX_HANDLE_TYPE_MAX
};

enum aaxFormat
{
   AAX_FORMAT_NONE = (unsigned int)-1,

   /* native endianness */
   AAX_PCM8S = 0,		/* signed, 8-bits per sample  */
   AAX_PCM16S,			/* signed, 16-bits per sample */
   AAX_PCM24S,			/* signed, 24-bits per sample */
   AAX_PCM32S,			/* signed, 32-bits per sample */
   AAX_FLOAT,		/* 32-bit floating point, -1.0 to 1.0 */
   AAX_DOUBLE,		/* 64-bit floating point, -1.0 to 1.0 */
   AAX_MULAW,
   AAX_ALAW,
   AAX_IMA4_ADPCM,
   AAX_FORMAT_MAX,
	
   AAX_FORMAT_NATIVE = 0x07F,	/* format mask */

   			/* the following formats get converted before use */
   AAX_FORMAT_UNSIGNED = 0x080,
   AAX_PCM8U = 0x080,		/* unsigned, 8-bits per sample  */
   AAX_PCM16U,			/* unsigned, 16-bits per sample */
   AAX_PCM24U,			/* unsigned, 24-bits per sample */
   AAX_PCM32U,			/* unsigned, 32-bits per sample */

   /* little endian */
   AAX_FORMAT_LE = 0x100,
   AAX_PCM16S_LE,
   AAX_PCM24S_LE,
   AAX_PCM32S_LE,
   AAX_FLOAT_LE,
   AAX_DOUBLE_LE,
 
   AAX_FORMAT_LE_UNSIGNED = (AAX_FORMAT_LE | AAX_FORMAT_UNSIGNED),
   AAX_PCM16U_LE,
   AAX_PCM24U_LE,
   AAX_PCM32U_LE,

   /* big endian */
   AAX_FORMAT_BE = 0x200,
   AAX_PCM16S_BE,
   AAX_PCM24S_BE,
   AAX_PCM32S_BE,
   AAX_FLOAT_BE,
   AAX_DOUBLE_BE,

   AAX_FORMAT_BE_UNSIGNED = (AAX_FORMAT_BE | AAX_FORMAT_UNSIGNED),
   AAX_PCM16U_BE,
   AAX_PCM24U_BE,
   AAX_PCM32U_BE,

   /** Unusual audio formats, the last bytes specifies the internal format **/
   AAX_SPECIAL = 0x80000,

   /* AeonWave Auto-generated XML Sound */
   AAX_AAXS = AAX_SPECIAL,
   AAX_AAXS16S = (AAX_AAXS | AAX_PCM16S),
   AAX_AAXS24S = (AAX_AAXS | AAX_PCM24S)
};

enum aaxType
{
   AAX_TYPE_NONE = 0,
   AAX_LINEAR = AAX_TYPE_NONE,
   AAX_LOGARITHMIC,
   AAX_RADIANS,
   AAX_DEGREES,
   AAX_BYTES,
   AAX_FRAMES,
   AAX_SAMPLES,
   AAX_MICROSECONDS,
   AAX_TYPE_MAX
};

enum aaxModeType
{
   AAX_MODE_TYPE_NONE = 0,
   AAX_POSITION,
   AAX_LOOPING,
   AAX_BUFFER_TRACK,
   AAX_MODE_TYPE_MAX
};

enum aaxTrackType
{
   AAX_TRACK_MIX = 0x40,
   AAX_TRACK_ALL = 0x80,

   AAX_TRACK0 = 0,
   AAX_TRACK_LEFT = AAX_TRACK0,
   AAX_TRACK_FRONT_LEFT = AAX_TRACK0,

   AAX_TRACK1 = 1,
   AAX_TRACK_RIGHT = AAX_TRACK1,
   AAX_TRACK_FRONT_RIGHT = AAX_TRACK1,

   AAX_TRACK2 = 2,
   AAX_TRACK_REAR_LEFT = AAX_TRACK2,

   AAX_TRACK3 = 3,
   AAX_TRACK_REAR_RIGHT = AAX_TRACK3,

   AAX_TRACK4 = 4,
   AAX_TRACK_CENTER = AAX_TRACK4,
   AAX_TRACK_CENTER_FRONT = AAX_TRACK4,

   AAX_TRACK5 = 5,
   AAX_TRACK_LFE = AAX_TRACK5,
   AAX_TRACK_SUBWOOFER = AAX_TRACK5,

   AAX_TRACK6 = 6,
   AAX_TRACK_SIDE_LEFT = AAX_TRACK6,

   AAX_TRACK7 = 7,
   AAX_TRACK_SIDE_RIGHT = AAX_TRACK7,

   AAX_TRACK_MAX
};

enum aaxSetupType
{
   AAX_SETUP_TYPE_NONE = 0,
   AAX_NAME_STRING,
   AAX_DRIVER_STRING = AAX_NAME_STRING,
   AAX_VERSION_STRING,
   AAX_RENDERER_STRING,
   AAX_VENDOR_STRING,
   AAX_FREQUENCY,
   AAX_TRACKS,
   AAX_FORMAT,
   AAX_REFRESH_RATE,
   AAX_REFRESHRATE = AAX_REFRESH_RATE,
   AAX_TRACK_SIZE,
   AAX_TRACKSIZE = AAX_TRACK_SIZE,
   AAX_NO_SAMPLES,
   AAX_LOOP_START,
   AAX_LOOP_END,
   AAX_MONO_EMITTERS,
   AAX_MONO_SOURCES = AAX_MONO_EMITTERS,
   AAX_STEREO_EMITTERS,
   AAX_STEREO_SOURCES = AAX_STEREO_EMITTERS,
   AAX_BLOCK_ALIGNMENT,
   AAX_AUDIO_FRAMES,
   AAX_UPDATE_RATE,
   AAX_UPDATERATE = AAX_UPDATE_RATE,
   AAX_LATENCY,
   AAX_TRACK_LAYOUT,
   AAX_BITRATE,
   AAX_FRAME_TIMING,
   AAX_SETUP_TYPE_MAX,

   AAX_PEAK_VALUE             = 0x0100,
   AAX_AVERAGE_VALUE          = 0x0200,
   AAX_COMPRESSION_VALUE      = 0x0400,
   AAX_GATE_ENABLED           = 0x0800,

   /* mixer capabilities */
   AAX_SHARED_MODE            = 0x1000,
   AAX_TIMER_MODE,
   AAX_BATCHED_MODE,
   AAX_SEEKABLE_SUPPORT,

   AAX_TRACKS_MIN             = 0x1200,
   AAX_TRACKS_MAX,
   AAX_PERIODS_MIN,
   AAX_PERIODS_MAX,
   AAX_FREQUENCY_MIN,
   AAX_FREQUENCY_MAX,
   AAX_SAMPLES_MAX,

   /* File and track information */
   AAX_COVER_IMAGE_DATA         = 0x2000,
   AAX_MUSIC_PERFORMER_STRING,
   AAX_MUSIC_GENRE_STRING,
   AAX_TRACK_TITLE_STRING,
   AAX_TRACK_NUMBER_STRING,
   AAX_ALBUM_NAME_STRING,
   AAX_RELEASE_DATE_STRING,
   AAX_SONG_COPYRIGHT_STRING,
   AAX_SONG_COMPOSER_STRING,
   AAX_SONG_COMMENT_STRING,
   AAX_ORIGINAL_PERFORMER_STRING,
   AAX_WEBSITE_STRING,

   AAX_MUSIC_PERFORMER_UPDATE = 0x2080,
   AAX_TRACK_TITLE_UPDATE
};

enum aaxErrorType
{
   AAX_ERROR_NONE = 0,
   AAX_INVALID_DEVICE         = 0x010,
   AAX_INVALID_SETUP          = 0x020,
   AAX_INVALID_HANDLE         = 0x030,
   AAX_INVALID_STATE          = 0x040,
   AAX_INVALID_ENUM           = 0x050,
   AAX_INVALID_PARAMETER      = 0x060,
   AAX_INVALID_REFERENCE      = 0x070,
   AAX_INSUFFICIENT_RESOURCES = 0x080,
   AAX_TIMEOUT                = 0x090,
   AAX_BACKEND_ERROR          = 0x0A0,
   AAX_ERROR_MAX              = 0x0B0
};

enum aaxEmitterMode
{
   AAX_MODE_NONE = 0,
   AAX_STEREO = AAX_MODE_NONE,
   AAX_ABSOLUTE,
   AAX_RELATIVE,
   AAX_EMITTER_MODE_MAX
};

enum aaxState
{
   AAX_STATE_NONE = 0,
   AAX_INITIALIZED,
   AAX_PLAYING,
   AAX_STOPPED,
   AAX_SUSPENDED,
   AAX_CAPTURING,
   AAX_PROCESSED,
   AAX_STANDBY,
   AAX_UPDATE,
   AAX_MAXIMUM,
   AAX_STATE_MAX = AAX_MAXIMUM,
};

enum aaxRenderMode
{
   AAX_MODE_READ = 0,
   AAX_MODE_WRITE_STEREO,
   AAX_MODE_WRITE_SPATIAL,
   AAX_MODE_WRITE_SURROUND,
   AAX_MODE_WRITE_HRTF,
   AAX_MODE_WRITE_MAX
};

enum aaxDistanceModel
{
   AAX_DISTANCE_MODEL_NONE = 0,
   AAX_EXPONENTIAL_DISTANCE,
   AAX_DISTANCE_MODEL_MAX,

   AAX_DISTANCE_DELAY = 0x8000,
   AAX_EXPONENTIAL_DISTANCE_DELAY=(AAX_EXPONENTIAL_DISTANCE|AAX_DISTANCE_DELAY),

   AAX_AL_INVERSE_DISTANCE = 1000,	/* (0x3E8) a mistake we need to keep */
   AAX_AL_INVERSE_DISTANCE_CLAMPED,
   AAX_AL_LINEAR_DISTANCE,
   AAX_AL_LINEAR_DISTANCE_CLAMPED,
   AAX_AL_EXPONENT_DISTANCE,
   AAX_AL_EXPONENT_DISTANCE_CLAMPED,	/*(0x3ED) */

   AAX_AL_DISTANCE_MODEL_MAX
};

enum aaxFilterType
{
   AAX_FILTER_NONE = 0,
   AAX_EQUALIZER,
   AAX_VOLUME_FILTER,
   AAX_DYNAMIC_GAIN_FILTER,
   AAX_TREMOLO_FILTER = AAX_DYNAMIC_GAIN_FILTER,
   AAX_TIMED_GAIN_FILTER,
   AAX_ANGULAR_FILTER,
   AAX_DISTANCE_FILTER,
   AAX_FREQUENCY_FILTER,
   AAX_GRAPHIC_EQUALIZER,
   AAX_COMPRESSOR,
   AAX_FILTER_MAX
};

enum aaxEffectType
{
   AAX_EFFECT_NONE = 0,
   AAX_PITCH_EFFECT,
   AAX_DYNAMIC_PITCH_EFFECT,
   AAX_VIBRATO_EFFECT = AAX_DYNAMIC_PITCH_EFFECT,
   AAX_TIMED_PITCH_EFFECT,
   AAX_DISTORTION_EFFECT,
   AAX_PHASING_EFFECT,
   AAX_CHORUS_EFFECT,
   AAX_FLANGING_EFFECT,
   AAX_VELOCITY_EFFECT,
   AAX_REVERB_EFFECT,
   AAX_EFFECT_MAX
};

enum aaxParameter {	/* Filter & Effect parameter number definitions */
   /* AAX_VOLUME_FILTER */
   AAX_GAIN = 0, AAX_MIN_GAIN, AAX_MAX_GAIN, AAX_AGC_RESPONSE_RATE,
   /* AAX_ANGULAR_FILTER */
   AAX_INNER_ANGLE = 0, AAX_OUTER_ANGLE, AAX_OUTER_GAIN,
   /* AAX_DISTANCE_FILTER */
   AAX_REF_DISTANCE = 0, AAX_MAX_DISTANCE, AAX_ROLLOFF_FACTOR,
   /* AAX_FREQUENCY_FILTER */
   /* AAX_EQUALIZER        */
   AAX_CUTOFF_FREQUENCY = 0, AAX_LF_GAIN, AAX_HF_GAIN, AAX_RESONANCE,
   AAX_CUTOFF_FREQUENCY_HF = 0x10, AAX_LF_GAIN_HF, AAX_HF_GAIN_HF, AAX_RESONANCE_HF,
   /* AAX_FREQUENCY_FILTER */
   AAX_SWEEP_RATE = AAX_RESONANCE_HF,

   /* AAX_GRAPHIC_EQUALIZER */
   AAX_GAIN_BAND0 = 0,    AAX_GAIN_BAND1, AAX_GAIN_BAND2, AAX_GAIN_BAND3,
   AAX_GAIN_BAND4 = 0x10, AAX_GAIN_BAND5, AAX_GAIN_BAND6, AAX_GAIN_BAND7,


   /* AAX_TIMED_GAIN_FILTER */
   /* AAX_TIMED_PITCH_EFFECT */
   AAX_LEVEL0 = 0x00, AAX_TIME0, AAX_LEVEL1, AAX_TIME1,
   AAX_LEVEL2 = 0x10, AAX_TIME2, AAX_LEVEL3, AAX_TIME3,
   AAX_LEVEL4 = 0x20, AAX_TIME4, AAX_LEVEL5, AAX_TIME5,
   AAX_LEVEL6 = 0x30, AAX_TIME6, AAX_LEVEL7, AAX_TIME7,

   /* AAX_DYNAMIC_GAIN_FILTER, ignores AAX_DELAY_GAIN  */
   /* AAX_DYNAMIC_PITCH_EFFECT, ignores AAX_DELAY_GAIN */
   /* AAX_PHASING_EFFECT  */
   /* AAX_CHORUS_EFFECT   */
   /* AAX_FLANGING_EFFECT */
   AAX_DELAY_GAIN = 0, AAX_LFO_FREQUENCY, AAX_LFO_DEPTH, AAX_LFO_OFFSET,

   /* AAX_COMPRESSOR */
   AAX_ATTACK_RATE = 0, AAX_RELEASE_RATE, AAX_COMPRESSION_RATIO, AAX_THRESHOLD,
   AAX_GATE_PERIOD = 0x11, AAX_GATE_THRESHOLD = 0x13,

   /* AAX_PITCH_EFFECT */
   AAX_PITCH = 0, AAX_MAX_PITCH, AAX_SPEEDUP,
   /* AAX_REVERB_EFFECT */
   /* AAX_CUTOFF_FREQUENCY=0, */ AAX_DELAY_DEPTH=1, AAX_DECAY_LEVEL, AAX_DECAY_DEPTH,
   /* AAX_DISTORTION_EFFECT*/
   AAX_DISTORTION_FACTOR=0, AAX_CLIPPING_FACTOR, AAX_MIX_FACTOR, AAX_ASYMMETRY,
   /* AAX_VELOCITY_EFFECT */
   AAX_SOUND_VELOCITY = 0, AAX_DOPPLER_FACTOR
};


enum aaxWaveformType
{
   AAX_WAVE_NONE 	= 0,

   AAX_CONSTANT_VALUE   = 0x0001,	/* equals to AAX_TRUE */
   AAX_TRIANGLE_WAVE    = 0x0002,
   AAX_SINE_WAVE	= 0x0004,
   AAX_SQUARE_WAVE	= 0x0008,
   AAX_SAWTOOTH_WAVE	= 0x0010,
   AAX_IMPULSE_WAVE	= 0x0020,
   AAX_WHITE_NOISE	= 0x0040,
   AAX_PINK_NOISE	= 0x0080,
   AAX_BROWNIAN_NOISE	= 0x0100,
   AAX_LAST_WAVEFORM	= AAX_BROWNIAN_NOISE,

   AAX_INVERSE_TRIANGLE_WAVE	= (AAX_INVERSE | AAX_TRIANGLE_WAVE),
   AAX_INVERSE_SINE_WAVE	= (AAX_INVERSE | AAX_SINE_WAVE),
   AAX_INVERSE_SQUARE_WAVE	= (AAX_INVERSE | AAX_SQUARE_WAVE),
   AAX_INVERSE_SAWTOOTH_WAVE	= (AAX_INVERSE | AAX_SAWTOOTH_WAVE),
   AAX_INVERSE_IMPULSE_WAVE	= (AAX_INVERSE | AAX_IMPULSE_WAVE),

   AAX_ENVELOPE_FOLLOW  = 0x0800,
   AAX_INVERSE_ENVELOPE_FOLLOW 	= (AAX_INVERSE | AAX_ENVELOPE_FOLLOW),

   AAX_MAX_WAVE		= 9
};

enum aaxFrequencyFilterType
{
   AAX_6DB_OCT = AAX_IMPULSE_WAVE,
   AAX_12DB_OCT = AAX_CONSTANT_VALUE,
   AAX_24DB_OCT = AAX_WHITE_NOISE,
   AAX_36DB_OCT = AAX_PINK_NOISE,
   AAX_48DB_OCT = AAX_BROWNIAN_NOISE,

   AAX_1ST_ORDER = AAX_6DB_OCT,
   AAX_2ND_ORDER = AAX_12DB_OCT,
   AAX_4TH_ORDER = AAX_24DB_OCT,
   AAX_6TH_ORDER = AAX_36DB_OCT,
   AAX_8TH_ORDER = AAX_48DB_OCT,

   AAX_BUTTERWORTH = 0,
   AAX_BESSEL = 0x8000000
};

enum aaxProcessingType
{
   AAX_PROCESSING_NONE = 0,

   AAX_OVERWRITE,
   AAX_ADD,
   AAX_MIX,
   AAX_RINGMODULATE,
   AAX_APPEND,
 
   AAX_PROCESSING_MAX
};

typedef void* aaxConfig;
typedef void* aaxBuffer;
typedef void* aaxEmitter;
typedef void* aaxFilter;
typedef void* aaxEffect;
typedef void* aaxFrame;

typedef float aaxVec3f[3];
typedef float aaxVec4f[4];
typedef float aaxMtx4f[4][4];
typedef double aaxMtx4d[4][4];


/*
 * Version and Support information
 */
AAX_API void AAX_APIENTRY aaxFree(void*);

AAX_API unsigned AAX_APIENTRY aaxGetMajorVersion();
AAX_API unsigned AAX_APIENTRY aaxGetMinorVersion();
AAX_API unsigned int AAX_APIENTRY aaxGetPatchLevel();
AAX_API const char* AAX_APIENTRY aaxGetCopyrightString();
AAX_API const char* AAX_APIENTRY aaxGetVersionString(aaxConfig);

AAX_API enum aaxErrorType AAX_APIENTRY aaxGetErrorNo();
AAX_API const char* AAX_APIENTRY aaxGetErrorString(enum aaxErrorType);

AAX_API unsigned AAX_APIENTRY aaxGetNoCores(aaxConfig);
AAX_API unsigned AAX_APIENTRY aaxGetBytesPerSample(enum aaxFormat);
AAX_API unsigned AAX_APIENTRY aaxGetBitsPerSample(enum aaxFormat);
AAX_API int AAX_APIENTRY aaxIsValid(const void*, enum aaxHandleType);

AAX_API int AAX_APIENTRY aaxIsFilterSupported(aaxConfig, const char*);
AAX_API int AAX_APIENTRY aaxIsEffectSupported(aaxConfig, const char*);

/* Pointer-to-function types */
typedef void (AAX_APIENTRY *PFNAAXFREE)(void*);
typedef unsigned (AAX_APIENTRY *PFNAAXGETMAJORVERSION)();
typedef unsigned (AAX_APIENTRY *PFNAAXGETMINORVERSION)();
typedef unsigned int (AAX_APIENTRY *PFNAAXGETPATCHLEVEL)();
typedef const char* (AAX_APIENTRY *PFNAAXGETCOPYRIGHTSTRING)();
typedef const char* (AAX_APIENTRY *PFNAAXGETVERSIONSTRING)(aaxConfig);
typedef enum aaxErrorType (AAX_APIENTRY *PFNAAXGETERRORNO)();
typedef const char* (AAX_APIENTRY *PFNAAXGETERRORSTRING)(enum aaxErrorType);
typedef unsigned (AAX_APIENTRY *PFNAAXGETNOCORES)(aaxConfig);
typedef unsigned (AAX_APIENTRY *PFNAAXGETBYTESPERSAMPLE)(enum aaxFormat);
typedef unsigned (AAX_APIENTRY *PFNAAXGETBITSPERSAMPLE)(enum aaxFormat);
typedef int (AAX_APIENTRY *PFNAAXISVALID)(const void*, enum aaxHandleType);
typedef int (AAX_APIENTRY *PFNAAXISFILTERSUPPORTED)(aaxConfig, const char*);
typedef int (AAX_APIENTRY *PFNAAXISEFFECTSUPPORTED)(aaxConfig, const char*);


/*
 * Driver information
 */
AAX_API aaxConfig AAX_APIENTRY aaxDriverGetByName(const char*, enum aaxRenderMode);
AAX_API unsigned AAX_APIENTRY aaxDriverGetCount(enum aaxRenderMode);
AAX_API aaxConfig AAX_APIENTRY aaxDriverGetByPos(unsigned, enum aaxRenderMode);
AAX_API int AAX_APIENTRY aaxDriverDestroy(aaxConfig);

AAX_API unsigned AAX_APIENTRY aaxDriverGetDeviceCount(const aaxConfig, enum aaxRenderMode);
AAX_API const char* AAX_APIENTRY aaxDriverGetDeviceNameByPos(const aaxConfig, unsigned, enum aaxRenderMode);
AAX_API unsigned AAX_APIENTRY aaxDriverGetInterfaceCount(const aaxConfig, const char*, enum aaxRenderMode);
AAX_API const char* AAX_APIENTRY aaxDriverGetInterfaceNameByPos(const aaxConfig, const char*, unsigned, enum aaxRenderMode);
AAX_API const char* AAX_APIENTRY aaxDriverGetSetup(const aaxConfig, enum aaxSetupType);
AAX_API int AAX_APIENTRY aaxDriverGetSupport(const aaxConfig, enum aaxRenderMode);

/* Pointer-to-function types */
typedef aaxConfig (AAX_APIENTRY *PFNAAXDRIVERGETBYNAME)(const char*, enum aaxRenderMode);
typedef unsigned (AAX_APIENTRY *PFNAAXDRIVERGETCOUNT)(enum aaxRenderMode);
typedef aaxConfig (AAX_APIENTRY *PFNAAXDRIVERGETBYPOS)(unsigned, enum aaxRenderMode);
typedef int (AAX_APIENTRY *PFNAAXDRIVERDESTROY)(aaxConfig);
typedef unsigned (AAX_APIENTRY *PFNAAXDRIVERGETDEVICECOUNT)(const aaxConfig, enum aaxRenderMode);
typedef const char* (AAX_APIENTRY *PFNAAXDRIVERGETDEVICENAMEBYPOS)(const aaxConfig, unsigned, enum aaxRenderMode);
typedef unsigned (AAX_APIENTRY *PFNAAXDRIVERGETINTERFACECOUNT)(const aaxConfig, const char*, enum aaxRenderMode);
typedef const char* (AAX_APIENTRY *PFNAAXDRIVERGETINTERFACENAMEBYPOS)(const aaxConfig, const char*, unsigned, enum aaxRenderMode);
typedef const char* (AAX_APIENTRY *PFNAAXDRIVERGETSETUP)(const aaxConfig, enum aaxSetupType);
typedef int (AAX_APIENTRY *PFNAAXDRIVERGETSUPPORT)(const aaxConfig, enum aaxRenderMode);


/*
 * Driver functions
 */
AAX_API aaxConfig AAX_APIENTRY aaxDriverOpenDefault(enum aaxRenderMode);
AAX_API aaxConfig AAX_APIENTRY aaxDriverOpenByName(const char*, enum aaxRenderMode);
AAX_API aaxConfig AAX_APIENTRY aaxDriverOpen(aaxConfig);
AAX_API int AAX_APIENTRY aaxDriverClose(aaxConfig);

/* Pointer-to-function types */
typedef aaxConfig (AAX_APIENTRY *PFNAAXDRIVEROPENDEFAULT)(enum aaxRenderMode);
typedef aaxConfig (AAX_APIENTRY *PFNAAXDRIVEROPENBYNAME)(const char*, enum aaxRenderMode);
typedef aaxConfig (AAX_APIENTRY *PFNAAXDRIVEROPEN)(aaxConfig);
typedef int (AAX_APIENTRY *PFNAAXDRIVERCLOSE)(aaxConfig);


/*
 * Mixer/Playback setup
 */
AAX_API int AAX_APIENTRY aaxMixerSetState(aaxConfig, enum aaxState);
AAX_API enum aaxState AAX_APIENTRY aaxMixerGetState(const aaxConfig);

AAX_API int AAX_APIENTRY aaxMixerSetMode(aaxConfig, enum aaxModeType, int);
AAX_API int AAX_APIENTRY aaxMixerGetMode(const aaxConfig, enum aaxModeType);

AAX_API int AAX_APIENTRY aaxMixerSetSetup(aaxConfig, enum aaxSetupType, unsigned int);
AAX_API unsigned int AAX_APIENTRY aaxMixerGetSetup(const aaxConfig, enum aaxSetupType);

AAX_API int AAX_APIENTRY aaxMixerSetFilter(aaxConfig, aaxFilter);
AAX_API aaxFilter AAX_APIENTRY aaxMixerGetFilter(const aaxConfig, enum aaxFilterType);

AAX_API int AAX_APIENTRY aaxMixerSetEffect(aaxConfig, aaxEffect);
AAX_API aaxEffect AAX_APIENTRY aaxMixerGetEffect(const aaxConfig, enum aaxEffectType);

AAX_API int AAX_APIENTRY aaxMixerRegisterSensor(const aaxConfig, const aaxConfig);
AAX_API int AAX_APIENTRY aaxMixerDeregisterSensor(const aaxConfig, const aaxConfig);
AAX_API int AAX_APIENTRY aaxMixerRegisterEmitter(const aaxConfig, const aaxEmitter);
AAX_API int AAX_APIENTRY aaxMixerDeregisterEmitter(const aaxConfig, const aaxEmitter);
AAX_API int AAX_APIENTRY aaxMixerRegisterAudioFrame(const aaxConfig, const aaxFrame);
AAX_API int AAX_APIENTRY aaxMixerDeregisterAudioFrame(const aaxConfig, const aaxFrame);

/* Pointer-to-function types */
typedef int (AAX_APIENTRY *PFNAAXMIXERSETSTATE)(aaxConfig, enum aaxState);
typedef enum aaxState (AAX_APIENTRY *PFNAAXMIXERGETSTATE)(aaxConfig);
typedef int (AAX_APIENTRY *PFNAAXMIXERSETMODE)(aaxConfig, enum aaxModeType, int);
typedef int (AAX_APIENTRY *PFNAAXMIXERGETMODE)(const aaxConfig, enum aaxModeType);
typedef int (AAX_APIENTRY *PFNAAXMIXERSETSETUP)(aaxConfig, enum aaxSetupType, unsigned int);
typedef unsigned int (AAX_APIENTRY *PFNAAXMIXERGETSETUP)(const aaxConfig, enum aaxSetupType);
typedef int (AAX_APIENTRY *PFNAAXMIXERSETFILTER)(aaxConfig, aaxFilter);
typedef aaxFilter (AAX_APIENTRY *PFNAAXMIXERGETFILTER)(const aaxConfig, enum aaxFilterType);
typedef int (AAX_APIENTRY *PFNAAXMIXERSETEFFECT)(aaxConfig, aaxEffect);
typedef aaxEffect (AAX_APIENTRY *PFNAAXMIXERGETEFFECT)(const aaxConfig, enum aaxEffectType);
typedef int (AAX_APIENTRY *PFNAAXMIXERREGISTERSENSOR)(const aaxConfig, const aaxConfig);
typedef int (AAX_APIENTRY *PFNAAXMIXERDEREGISTERSENSOR)(const aaxConfig, const aaxConfig);
typedef int (AAX_APIENTRY *PFNAAXMIXERREGISTEREMITTER)(const aaxConfig, const aaxEmitter);
typedef int (AAX_APIENTRY *PFNAAXMIXERDEREGISTEREMITTER)(const aaxConfig, const aaxEmitter);
typedef int (AAX_APIENTRY *PFNAAXMIXERREGISTERAUDIOFRAME)(const aaxConfig, const aaxFrame);
typedef int (AAX_APIENTRY *PFNAAXMIXERDEREGISTERAUDIOFRAME)(const aaxConfig, const aaxFrame);


/*
 * Scenery setup
 */
AAX_API int AAX_APIENTRY aaxScenerySetFilter(aaxConfig, aaxFilter);
AAX_API aaxFilter AAX_APIENTRY aaxSceneryGetFilter(const aaxConfig, enum aaxFilterType);

AAX_API int AAX_APIENTRY aaxScenerySetEffect(aaxConfig, aaxEffect);
AAX_API aaxEffect AAX_APIENTRY aaxSceneryGetEffect(const aaxConfig, enum aaxEffectType);

/* Pointer-to-function types */
typedef int (AAX_APIENTRY *PFNAAXSCENERYSETFILTER)(aaxConfig, aaxFilter);
typedef aaxFilter (AAX_APIENTRY *PFNAAXSCENERYGETFILTER)(const aaxConfig, enum aaxFilterType);
typedef int (AAX_APIENTRY *PFNAAXSCENERYSETEFFECT)(aaxConfig, aaxEffect);
typedef aaxEffect (AAX_APIENTRY *PFNAAXSCENERYGETEFFECT)(const aaxConfig, enum aaxEffectType);


/*
 * Sound buffer related
 */
AAX_API aaxBuffer AAX_APIENTRY aaxBufferCreate(aaxConfig, unsigned int, unsigned, enum aaxFormat);
AAX_API int AAX_APIENTRY aaxBufferDestroy(aaxBuffer);

AAX_API int AAX_APIENTRY aaxBufferSetSetup(aaxBuffer, enum aaxSetupType, unsigned int);
AAX_API unsigned int AAX_APIENTRY aaxBufferGetSetup(const aaxBuffer, enum aaxSetupType);

AAX_API int AAX_APIENTRY aaxBufferSetData(aaxBuffer, const void*);
AAX_API void** AAX_APIENTRY aaxBufferGetData(const aaxBuffer);

AAX_API int AAX_APIENTRY aaxBufferProcessWaveform(aaxBuffer, float, enum aaxWaveformType, float, enum aaxProcessingType);
AAX_API aaxBuffer AAX_APIENTRY aaxBufferReadFromStream(aaxConfig, const char*);
AAX_API int AAX_APIENTRY aaxBufferWriteToFile(aaxBuffer, const char*, enum aaxProcessingType);

/* Pointer-to-function types */
typedef int (AAX_APIENTRY *PFNAAXBUFFERCREATE)(aaxConfig, unsigned int, unsigned, enum aaxFormat);
typedef int (AAX_APIENTRY *PFNAAXBUFFERDESTROY)(aaxBuffer);
typedef int (AAX_APIENTRY *PFNAAXBUFFERSETSETUP)(aaxBuffer, enum aaxSetupType, unsigned int);
typedef unsigned int (AAX_APIENTRY *PFNAAXBUFFERGETSETUP)(const aaxBuffer, enum aaxSetupType);
typedef int (AAX_APIENTRY *PFNAAXBUFFERSETDATA)(aaxBuffer, const void*);
typedef void** (AAX_APIENTRY *PFNAAXBUFFERGETDATA)(const aaxBuffer);
typedef int (AAX_APIENTRY *PFNAAXBUFFERPROCESSWAVEFORM)(aaxBuffer, float, enum aaxWaveformType, float, enum aaxProcessingType);


/*
 * Source/Emitter manipulation
 */
AAX_API aaxEmitter AAX_APIENTRY aaxEmitterCreate();
AAX_API int AAX_APIENTRY aaxEmitterDestroy(aaxEmitter);

AAX_API int AAX_APIENTRY aaxEmitterSetMode(aaxEmitter, enum aaxModeType, int);
AAX_API int AAX_APIENTRY aaxEmitterGetMode(const aaxEmitter, enum aaxModeType);

AAX_API int AAX_APIENTRY aaxEmitterSetState(aaxEmitter, enum aaxState);
AAX_API int AAX_APIENTRY aaxEmitterGetState(const aaxEmitter);

AAX_API int AAX_APIENTRY aaxEmitterSetSetup(aaxEmitter, enum aaxSetupType, unsigned int);
AAX_API unsigned int AAX_APIENTRY aaxEmitterGetSetup(const aaxEmitter, enum aaxSetupType);

AAX_API int AAX_APIENTRY aaxEmitterSetMatrix(aaxConfig, aaxMtx4f);
AAX_API int AAX_APIENTRY aaxEmitterGetMatrix(aaxConfig, aaxMtx4f);

AAX_API int AAX_APIENTRY aaxEmitterSetVelocity(aaxEmitter, const aaxVec3f);
AAX_API int AAX_APIENTRY aaxEmitterGetVelocity(const aaxEmitter, aaxVec3f);

AAX_API int AAX_APIENTRY aaxEmitterSetFilter(aaxEmitter, aaxFilter);
AAX_API aaxFilter AAX_APIENTRY aaxEmitterGetFilter(const aaxEmitter, enum aaxFilterType);

AAX_API int AAX_APIENTRY aaxEmitterSetEffect(aaxEmitter, aaxEffect);
AAX_API aaxEffect AAX_APIENTRY aaxEmitterGetEffect(const aaxEmitter, enum aaxEffectType);

AAX_API int AAX_APIENTRY aaxEmitterAddBuffer(aaxEmitter, aaxBuffer);
AAX_API int AAX_APIENTRY aaxEmitterRemoveBuffer(aaxEmitter);

AAX_API unsigned int AAX_APIENTRY aaxEmitterGetNoBuffers(const aaxEmitter, enum aaxState);
AAX_API aaxBuffer AAX_APIENTRY aaxEmitterGetBufferByPos(const aaxEmitter, unsigned int, int);

AAX_API int AAX_APIENTRY aaxEmitterSetOffset(aaxEmitter, unsigned long, enum aaxType);
AAX_API unsigned long AAX_APIENTRY aaxEmitterGetOffset(const aaxEmitter, enum aaxType);

AAX_API int AAX_APIENTRY aaxEmitterSetOffsetSec(aaxEmitter, float);
AAX_API float AAX_APIENTRY aaxEmitterGetOffsetSec(const aaxEmitter);

/* Pointer-to-function types */
typedef int (AAX_APIENTRY *PFNAAXEMITTERCREATE)();
typedef int (AAX_APIENTRY *PFNAAXEMITTERDESTROY)(aaxEmitter);
typedef int (AAX_APIENTRY *PFNAAXEMITTERSETSTATE)(aaxEmitter, enum aaxState);
typedef int (AAX_APIENTRY *PFNAAXEMITTERGETSTATE)(const aaxEmitter);
typedef int (AAX_APIENTRY *PFNAAXEMITTERSETMODE)(aaxEmitter, enum aaxModeType, int);
typedef int (AAX_APIENTRY *PFNAAXEMITTERGETMODE)(const aaxEmitter, enum aaxModeType);
typedef int (AAX_APIENTRY *PFNAAXEMITTERSETSETUP)(aaxEmitter, enum aaxSetupType, unsigned int);
typedef unsigned int (AAX_APIENTRY *PFNAAXEMITTERGETSETUP)(const aaxEmitter, enum aaxSetupType);
typedef int (AAX_APIENTRY *PFNAAXEMITTERSETMATRIX)(aaxConfig, aaxMtx4f);
typedef int (AAX_APIENTRY *PFNAAXEMITTERGETMATRIX)(aaxConfig, aaxMtx4f);
typedef int (AAX_APIENTRY *PFNAAXEMITTERSETVELOCITY)(aaxEmitter, const aaxVec3f);
typedef int (AAX_APIENTRY *PFNAAXEMITTERGETVELOCITY)(const aaxEmitter, aaxVec3f);
typedef int (AAX_APIENTRY *PFNAAXEMITTERSETFILTER)(aaxEmitter, aaxFilter);
typedef aaxFilter (AAX_APIENTRY *PFNAAXEMITTERGETFILTER)(const aaxEmitter, enum aaxFilterType);
typedef int (AAX_APIENTRY *PFNAAXEMITTERSETEFFECT)(aaxEmitter, aaxEffect);
typedef aaxEffect (AAX_APIENTRY *PFNAAXEMITTERGETEFFECT)(const aaxEmitter, enum aaxEffectType);

typedef int (AAX_APIENTRY *PFNAAXEMITTERADDBUFFER)(aaxEmitter, aaxBuffer);
typedef int (AAX_APIENTRY *PFNAAXEMITTERREMOVEBUFFER)(aaxEmitter);
typedef int (AAX_APIENTRY *PFNAAXEMITTERSETOFFSET)(aaxEmitter, unsigned long, enum aaxType);
typedef int (AAX_APIENTRY *PFNAAXEMITTERSETOFFSETSEC)(aaxEmitter, float);
typedef unsigned int (AAX_APIENTRY *PFNAAXEMITTERGETNOBUFFERS)(const aaxEmitter, enum aaxState);
typedef aaxBuffer (AAX_APIENTRY *PFNAAXEMITTERGETBUFFERBYPOS)(const aaxEmitter, unsigned int, int);
typedef float (AAX_APIENTRY *PFNAAXEMITTERGETOFFSETSEC)(const aaxEmitter);
typedef unsigned long (AAX_APIENTRY *PFNAAXEMITTERGETOFFSET)(const aaxEmitter, enum aaxType);


/*
 * Listener/Sensor manipulation
 */
AAX_API int AAX_APIENTRY aaxSensorSetState(aaxConfig, enum aaxState);
AAX_API enum aaxState AAX_APIENTRY aaxSensorGetState(const aaxConfig);

AAX_API int AAX_APIENTRY aaxSensorSetMode(aaxConfig, enum aaxModeType, int);
AAX_API int AAX_APIENTRY aaxSensorGetMode(const aaxConfig, enum aaxModeType);

AAX_API int AAX_APIENTRY aaxSensorSetSetup(aaxConfig, enum aaxSetupType, unsigned int);
AAX_API unsigned int AAX_APIENTRY aaxSensorGetSetup(const aaxConfig, enum aaxSetupType);

AAX_API int AAX_APIENTRY aaxSensorSetMatrix(aaxConfig, aaxMtx4f);
AAX_API int AAX_APIENTRY aaxSensorGetMatrix(const aaxConfig, aaxMtx4f);

AAX_API int AAX_APIENTRY aaxSensorSetVelocity(aaxConfig, const aaxVec3f);
AAX_API int AAX_APIENTRY aaxSensorGetVelocity(const aaxConfig, aaxVec3f);

AAX_API int AAX_APIENTRY aaxSensorWaitForBuffer(const aaxConfig, float);
AAX_API aaxBuffer AAX_APIENTRY aaxSensorGetBuffer(const aaxConfig);

AAX_API unsigned long AAX_APIENTRY aaxSensorGetOffset(const aaxConfig, enum aaxType);

/* Pointer-to-function types */
typedef int (AAX_APIENTRY *PFNAAXSENSORSETSTATE)(aaxConfig, enum aaxState);
typedef int (AAX_APIENTRY *PFNAAXSENSORGETSTATE)(const aaxConfig);
typedef int (AAX_APIENTRY *PFNAAXSENSORSETMODE)(aaxConfig, enum aaxModeType, int);
typedef int (AAX_APIENTRY *PFNAAXSENSORGETMODE)(const aaxConfig, enum aaxModeType);
typedef int (AAX_APIENTRY *PFNAAXSENSORSETSETUP)(aaxConfig, enum aaxSetupType, unsigned int);
typedef unsigned int (AAX_APIENTRY *PFNAAXSENSORGETSETUP)(const aaxConfig, enum aaxSetupType);
typedef int (AAX_APIENTRY *PFNAAXSENSORSETMATRIX)(aaxConfig, aaxMtx4f);
typedef int (AAX_APIENTRY *PFNAAXSENSORGETMATRIX)(const aaxConfig, aaxMtx4f);
typedef int (AAX_APIENTRY *PFNAAXSENSORSETVELOCITY)(aaxConfig, const aaxVec3f);
typedef int (AAX_APIENTRY *PFNAAXSENSORGETVELOCITY)(const aaxConfig, aaxVec3f);
typedef int (AAX_APIENTRY *PFNAAXSENSORWAITFORBUFFER)(const aaxConfig, float);
typedef aaxBuffer (AAX_APIENTRY *PFNAAXSENSORGETBUFFER)(const aaxConfig);
typedef unsigned long (AAX_APIENTRY *PFNAAXSENSORGETOFFSET)(const aaxConfig, enum aaxType);


/*
 * Filter support
 */
typedef int (*aaxFilterFn)(void*, aaxFilter);
AAX_API aaxFilter AAX_APIENTRY aaxFilterCreate(aaxConfig, enum aaxFilterType);
AAX_API int AAX_APIENTRY aaxFilterDestroy(aaxFilter);
AAX_API const char* AAX_APIENTRY aaxFilterGetNameByType(aaxConfig, enum aaxFilterType);
AAX_API int AAX_APIENTRY aaxFilterSetParam(aaxFilter, int, int, float);
AAX_API aaxFilter AAX_APIENTRY aaxFilterSetSlotParams(aaxFilter, unsigned, int, aaxVec4f);
AAX_API aaxFilter AAX_APIENTRY aaxFilterSetSlot(aaxFilter, unsigned, int, float, float, float, float);
AAX_API aaxFilter AAX_APIENTRY aaxFilterSetState(aaxFilter, int);

AAX_API float AAX_APIENTRY aaxFilterGetParam(const aaxFilter, int, int);
AAX_API aaxFilter AAX_APIENTRY aaxFilterGetSlotParams(const aaxFilter, unsigned, int, aaxVec4f);
AAX_API aaxFilter AAX_APIENTRY aaxFilterGetSlot(const aaxFilter, unsigned, int, float*, float*, float*, float*);
AAX_API int AAX_APIENTRY aaxFilterGetState(aaxFilter);
AAX_API float AAX_APIENTRY aaxFilterApplyParam(const aaxFilter, int, int, int);
AAX_API aaxFilter AAX_APIENTRY aaxFilterApply(aaxFilterFn, void *, aaxFilter);

/* Pointer-to-function types */
typedef aaxFilter (AAX_APIENTRY *PFNAAXFILTERCREATE)(aaxConfig, enum aaxFilterType);
typedef int (AAX_APIENTRY *PFNAAXFILTERDESTROY)(aaxFilter);
typedef const char* (AAX_APIENTRY *PFNAAXFILTERGETNAMEBYTYPE)(aaxConfig, enum aaxFilterType);
typedef int (AAX_APIENTRY *PFNAAXFILTERSETPARAM)(aaxFilter, int, int, float);
typedef aaxFilter (AAX_APIENTRY *PFNAAXFILTERSETSLOTPARAMS)(aaxFilter, unsigned, int, aaxVec4f);
typedef aaxFilter (AAX_APIENTRY *PFNAAXFILTERSETSLOT)(aaxFilter, unsigned, int, float, float, float, float);
typedef aaxFilter (AAX_APIENTRY *PFNAAXFILTERSETSTATE)(aaxFilter, int);
typedef float (AAX_APIENTRY *PFNAAXFILTERGETPARAM)(const aaxFilter, int, int);
typedef aaxFilter (AAX_APIENTRY *PFNAAXFILTERGETSLOTPARAMS)(const aaxFilter, unsigned, int, aaxVec4f);
typedef aaxFilter (AAX_APIENTRY *PFNAAXFILTERGETSLOT)(const aaxFilter, unsigned, int, float*, float*, float*, float*);
typedef int (AAX_APIENTRY *PFNAAXFILTERGETSTATE)(aaxFilter);
typedef aaxFilter (AAX_APIENTRY *PFNAAXFILTERAPPLY)(aaxFilterFn, void *, aaxFilter);

/*
 * Effect support
 */
typedef int (*aaxEffectFn)(void*, aaxEffect);
AAX_API aaxEffect AAX_APIENTRY aaxEffectCreate(aaxConfig, enum aaxEffectType);
AAX_API int AAX_APIENTRY aaxEffectDestroy(aaxEffect);
AAX_API const char* AAX_APIENTRY aaxEffectGetNameByType(aaxConfig, enum aaxEffectType);
AAX_API int AAX_APIENTRY aaxEffectSetParam(aaxEffect, int, int, float);
AAX_API aaxEffect AAX_APIENTRY aaxEffectSetSlotParams(aaxEffect, unsigned, int, aaxVec4f);
AAX_API aaxEffect AAX_APIENTRY aaxEffectSetSlot(aaxEffect, unsigned, int, float, float, float, float);
AAX_API aaxEffect AAX_APIENTRY aaxEffectSetState(aaxEffect, int);
AAX_API float AAX_APIENTRY aaxEffectGetParam(const aaxEffect, int , int);
AAX_API aaxEffect AAX_APIENTRY aaxEffectGetSlotParams(const aaxEffect, unsigned, int, aaxVec4f);
AAX_API aaxEffect AAX_APIENTRY aaxEffectGetSlot(const aaxEffect, unsigned, int, float*, float*, float*, float*);
AAX_API int AAX_APIENTRY aaxEffectGetState(aaxEffect);
AAX_API float AAX_APIENTRY aaxEffectApplyParam(const aaxEffect, int, int, int);
AAX_API aaxEffect AAX_APIENTRY aaxEffectApply(aaxEffectFn, void *, aaxEffect);

/* Pointer-to-function types */
typedef aaxEffect (AAX_APIENTRY *PFNAAXEFFECTCREATE)(aaxConfig, enum aaxEffectType);
typedef int (AAX_APIENTRY *PFNAAXEFFECTDESTROY)(aaxEffect);
typedef const char* (AAX_APIENTRY *APFNAXEFFECTGETNAMEBYTYPE)(aaxConfig, enum aaxEffectType);
typedef int (AAX_APIENTRY *PFNAAXEFFECTSETPARAM)(aaxEffect, int, int, float);
typedef aaxEffect (AAX_APIENTRY *PFNAAXEFFECTSETSLOTPARAMS)(aaxEffect, unsigned, int, aaxVec4f);
typedef aaxEffect (AAX_APIENTRY *PFNAAXEFFECTSETSLOT)(aaxEffect, unsigned, int, float, float, float, float);
typedef aaxEffect (AAX_APIENTRY *PFNAAXEFFECTSETSTATE)(aaxEffect, int);
typedef float (AAX_APIENTRY *PFNAAXEFFECTGETPARAM)(const aaxEffect, int, int);
typedef aaxEffect (AAX_APIENTRY *PFNAAXEFFECTGETSLOTPARAMS)(const aaxEffect, unsigned, int, aaxVec4f);
typedef aaxEffect (AAX_APIENTRY *PFNAAXEFFECTGETSLOT)(const aaxEffect, unsigned, int, float*, float*, float*, float*);
typedef int (AAX_APIENTRY *PFNAAXEFFECTGETSTATE)(aaxEffect);
typedef aaxEffect (AAX_APIENTRY *PFNAAXEFFECTAPPLY)(aaxEffectFn, void *, aaxEffect);

/*
 * Matrix operations
 */

AAX_API int AAX_APIENTRY aaxMatrixSetIdentityMatrix(aaxMtx4f);
AAX_API int AAX_APIENTRY aaxMatrixTranslate(aaxMtx4f, float, float, float);
AAX_API int AAX_APIENTRY aaxMatrixRotate(aaxMtx4f, float, float, float, float);
AAX_API int AAX_APIENTRY aaxMatrixMultiply(aaxMtx4f, aaxMtx4f);
AAX_API int AAX_APIENTRY aaxMatrixInverse(aaxMtx4f);
AAX_API int AAX_APIENTRY aaxMatrixSetDirection(aaxMtx4f, const aaxVec3f, const aaxVec3f);
AAX_API int AAX_APIENTRY aaxMatrixSetOrientation(aaxMtx4f, const aaxVec3f, const aaxVec3f, const aaxVec3f);
AAX_API int AAX_APIENTRY aaxMatrixGetOrientation(aaxMtx4f, aaxVec3f, aaxVec3f, aaxVec3f);

AAX_API int AAX_APIENTRY aaxMatrix64SetIdentityMatrix(aaxMtx4d);
AAX_API int AAX_APIENTRY aaxMatrix64Translate(aaxMtx4d, double, double, double);
AAX_API int AAX_APIENTRY aaxMatrix64Rotate(aaxMtx4d, double, double, double, double);
AAX_API int AAX_APIENTRY aaxMatrix64Multiply(aaxMtx4d, aaxMtx4d);
AAX_API int AAX_APIENTRY aaxMatrix64Inverse(aaxMtx4d);
AAX_API int AAX_APIENTRY aaxMatrix64ToMatrix(aaxMtx4f, aaxMtx4d);

/* Pointer-to-function types */
typedef int (AAX_APIENTRY *PFNAAXMATRIXSETIDENTITYMATRIX)(aaxMtx4f);
typedef int (AAX_APIENTRY *PFNAAXMATRIXTRANSLATE)(aaxMtx4f, float, float, float);
typedef int (AAX_APIENTRY *PFNAAXMATRIXROTATE)(aaxMtx4f, float, float, float, float);
typedef int (AAX_APIENTRY *PFNAAXMATRIXMULTIPLY)(aaxMtx4f, aaxMtx4f);
typedef int (AAX_APIENTRY *PFNAAXMATRIXINVERSE)(aaxMtx4f);
typedef int (AAX_APIENTRY *PFNAAXMATRIXSETDIRECTION)(aaxMtx4f, const aaxVec3f, const aaxVec3f);
typedef int (AAX_APIENTRY *PFNAAXMATRIXSETORIENTATION)(aaxMtx4f, const aaxVec3f, const aaxVec3f, const aaxVec3f);
typedef int (AAX_APIENTRY *PFNAAXMATRIXGETORIENTATION)(aaxMtx4f, aaxVec3f, aaxVec3f, aaxVec3f);
typedef int (AAX_APIENTRY *PFNAAXMATRIX64SETIDENTITYMATRIX)(aaxMtx4d);
typedef int (AAX_APIENTRY *PFNAAXMATRIX64TRANSLATE)(aaxMtx4d, double, double, double);
typedef int (AAX_APIENTRY *PFNAAXMATRIX64ROTATE)(aaxMtx4d, double, double, double, double);
typedef int (AAX_APIENTRY *PFNAAXMATRIX64MULTIPLY)(aaxMtx4d, aaxMtx4d);
typedef int (AAX_APIENTRY *PFNAAXMATRIX64INVERSE)(aaxMtx4d);
typedef int (AAX_APIENTRY *PFNAAXMATRIX64TOMATRIX)(aaxMtx4f, aaxMtx4d);


/*
 * AudioFrame support (version 2.0 and later)
 */
AAX_API aaxFrame AAX_APIENTRY aaxAudioFrameCreate(aaxConfig);
AAX_API int AAX_APIENTRY aaxAudioFrameDestroy(aaxFrame);

AAX_API int AAX_APIENTRY aaxAudioFrameSetState(aaxFrame, enum aaxState);
AAX_API int AAX_APIENTRY aaxAudioFrameGetState(const aaxFrame);

AAX_API int AAX_APIENTRY aaxAudioFrameSetMode(aaxFrame, enum aaxModeType, int);
AAX_API int AAX_APIENTRY aaxAudioFrameGetMode(const aaxFrame, enum aaxModeType);

AAX_API int AAX_APIENTRY aaxAudioFrameSetSetup(aaxFrame, enum aaxSetupType, unsigned int);
AAX_API unsigned int aaxAudioFrameGetSetup(const aaxFrame, enum aaxSetupType);

AAX_API int AAX_APIENTRY aaxAudioFrameSetMatrix(aaxFrame, aaxMtx4f);
AAX_API int AAX_APIENTRY aaxAudioFrameGetMatrix(aaxFrame, aaxMtx4f);

AAX_API int AAX_APIENTRY aaxAudioFrameSetVelocity(aaxFrame, const aaxVec3f);
AAX_API int AAX_APIENTRY aaxAudioFrameGetVelocity(aaxFrame, aaxVec3f);

AAX_API int AAX_APIENTRY aaxAudioFrameSetFilter(aaxFrame, aaxFilter);
AAX_API aaxFilter AAX_APIENTRY aaxAudioFrameGetFilter(const aaxFrame, enum aaxFilterType);

AAX_API int AAX_APIENTRY aaxAudioFrameSetEffect(aaxFrame, aaxEffect);
AAX_API aaxEffect AAX_APIENTRY aaxAudioFrameGetEffect(const aaxFrame, enum aaxEffectType);

AAX_API int AAX_APIENTRY aaxAudioFrameWaitForBuffer(aaxFrame, float);
AAX_API aaxBuffer AAX_APIENTRY aaxAudioFrameGetBuffer(const aaxFrame);

AAX_API int AAX_APIENTRY aaxAudioFrameRegisterSensor(const aaxFrame, const aaxConfig);
AAX_API int AAX_APIENTRY aaxAudioFrameDeregisterSensor(const aaxFrame, const aaxConfig);

AAX_API int AAX_APIENTRY aaxAudioFrameRegisterEmitter(const aaxFrame, const aaxEmitter);
AAX_API int AAX_APIENTRY aaxAudioFrameDeregisterEmitter(const aaxFrame, const aaxEmitter);

AAX_API int AAX_APIENTRY aaxAudioFrameRegisterAudioFrame(const aaxFrame, const aaxFrame);
AAX_API int AAX_APIENTRY aaxAudioFrameDeregisterAudioFrame(const aaxFrame, const aaxFrame);


/* Pointer-to-function types */
typedef aaxFrame (AAX_APIENTRY *PFNAAXAUDIOFRAMECREATE)(aaxConfig);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMEDESTROY)(aaxFrame);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMESETSTATE)(aaxFrame, enum aaxState);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMEGETSTATE)(const aaxFrame);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMESETMODE)(aaxFrame, enum aaxModeType, int);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMEGETMODE)(const aaxFrame, enum aaxModeType);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMESETSETUP)(aaxFrame, enum aaxSetupType, unsigned int);
typedef unsigned int (AAX_APIENTRY *PFNAAXAUDIOFRAMEGETSETUP)(const aaxFrame, enum aaxSetupType);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMESETMATRIX)(aaxFrame, aaxMtx4f);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMEGETMATRIX)(aaxFrame, aaxMtx4f);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMESETVELOCITY)(aaxFrame, const aaxVec3f);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMEGETVELOCITY)(aaxFrame, aaxVec3f);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMESETFILTER)(aaxFrame, aaxFilter);
typedef aaxFilter (AAX_APIENTRY *PFNAAXAUDIOFRAMEGETFILTER)(const aaxFrame, enum aaxFilterType);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMESETEFFECT)(aaxFrame, aaxEffect);
typedef aaxEffect (AAX_APIENTRY *PFNAAXAUDIOFRAMEGETEFFECT)(const aaxFrame, enum aaxEffectType);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMEWAITFORBUFFER)(aaxFrame, float);
typedef aaxBuffer (AAX_APIENTRY *PFNAAXAUDIOFRAMEGETBUFFER)(const aaxFrame);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMEREGISTERSENSOR)(const aaxFrame, const aaxConfig);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMEDEREGISTERSENSOR)(const aaxFrame, const aaxConfig);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMEREGISTEREMITTER)(const aaxFrame, const aaxEmitter);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMEDEREGISTEREMITTER)(const aaxFrame, const aaxEmitter);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMEREGISTERAUDIOFRAME)(const aaxFrame, const aaxFrame);
typedef int (AAX_APIENTRY *PFNAAXAUDIOFRAMEDEREGISTERAUDIOFRAME)(const aaxFrame, const aaxFrame);



#if defined(TARGET_OS_MAC) && TARGET_OS_MAC
# pragma export off
#endif

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif

