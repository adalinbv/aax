/*
 * Copyright 2007-2011 by Adalin B.V.
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
# define AAX_API __attribute__((dllexport))
#else
# define AAX_APIENTRY
# if __GNUC__ >= 4
#  define AAX_API __attribute__((visibility("default")))
# else
#  define AAX_API extern
# endif
#endif

#if defined(TARGET_OS_MAC) && TARGET_OS_MAC
# pragma export on
#endif


#define AAX_MAJOR_VERSION	2
#define AAX_MINOR_VERSION	1
#define AAX_PATCH_LEVEL		111010

enum aaxHandleType
{
    AAX_NONE = 0,
    AAX_CONFIG,
    AAX_BUFFER,
    AAX_EMITTER,
    AAX_AUDIOFRAME,
    AAX_FILTER,
    AAX_EFFECT,
    AAX_HANDLE_TYPE_MAX
};

enum aaxFormat
{
   AAX_FORMAT_NONE = (unsigned int)-1,

   /* native endianness */
   AAX_FORMAT_PCM8S = 0,		/* signed, 8-bits per sample  */
   AAX_FORMAT_PCM16S,			/* signed, 16-bits per sample   */
   AAX_FORMAT_PCM24S,			/* signed, 24-bits per sample   */
   AAX_FORMAT_PCM32S,			/* signed, 32-bits per sample   */
   AAX_FORMAT_FLOAT,		/* 32-bit floating point, -1.0 to 1.0   */
   AAX_FORMAT_DOUBLE,		/* 64-bit floating point, -1.0 to 1.0   */
   AAX_FORMAT_MULAW,
   AAX_FORMAT_ALAW,
   AAX_FORMAT_IMA4_ADPCM,
   AAX_FORMAT_MAX,
	
   AAX_FORMAT_NATIVE = 0x07F,

   			/* the following formats get converted before use */
   AAX_FORMAT_UNSIGNED = 0x080,
   AAX_FORMAT_PCM8U = 0x080,		/* unsigned, 8-bits per sample    */
   AAX_FORMAT_PCM16U,			/* unsigned, 16-bits per sample */
   AAX_FORMAT_PCM24U,			/* unsigned, 24-bits per sample */
   AAX_FORMAT_PCM32U,			/* unsigned, 32-bits per sample */

   /* little endian */
   AAX_FORMAT_LE = 0x100,
   AAX_FORMAT_PCM16S_LE,
   AAX_FORMAT_PCM24S_LE,
   AAX_FORMAT_PCM32S_LE,
   AAX_FORMAT_FLOAT_LE,
   AAX_FORMAT_DOUBLE_LE,
 
   AAX_FORMAT_LE_UNSIGNED = (AAX_FORMAT_LE | AAX_FORMAT_UNSIGNED),
   AAX_FORMAT_PCM16U_LE,
   AAX_FORMAT_PCM24U_LE,
   AAX_FORMAT_PCM32U_LE,

   /* big endian */
   AAX_FORMAT_BE = 0x200,
   AAX_FORMAT_PCM16S_BE,
   AAX_FORMAT_PCM24S_BE,
   AAX_FORMAT_PCM32S_BE,
   AAX_FORMAT_FLOAT_BE,
   AAX_FORMAT_DOUBLE_BE,

   AAX_FORMAT_BE_UNSIGNED = (AAX_FORMAT_BE | AAX_FORMAT_UNSIGNED),
   AAX_FORMAT_PCM16U_BE,
   AAX_FORMAT_PCM24U_BE,
   AAX_FORMAT_PCM32U_BE,
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

enum aaxSetupType
{
   AAX_SETUP_TYPE_NONE = 0,
   AAX_NAME_STRING,
   AAX_VERSION_STRING,
   AAX_RENDERER_STRING,
   AAX_VENDOR_STRING,
   AAX_FREQUENCY,
   AAX_TRACKS,
   AAX_FORMAT,
   AAX_REFRESHRATE,
   AAX_TRACKSIZE,
   AAX_NO_SAMPLES,
   AAX_LOOP_START,
   AAX_LOOP_END,
   AAX_MONO_SOURCES,
   AAX_STEREO_SOURCES,
   AAX_BLOCK_ALIGNMENT,
   AAX_SETUP_TYPE_MAX
};

enum aaxErrorType
{
   AAX_ERROR_NONE = 0,
   AAX_INVALID_DEVICE         = 0x10,
   AAX_INVALID_SETUP          = 0x20,
   AAX_INVALID_HANDLE         = 0x30,
   AAX_INVALID_STATE          = 0x40,
   AAX_INVALID_ENUM           = 0x50,
   AAX_INVALID_PARAMETER      = 0x60,
   AAX_INVALID_REFERENCE      = 0x70,
   AAX_INSUFFICIENT_RESOURCES = 0x80,
   AAX_TIMEOUT                = 0x90,
   AAX_ERROR_MAX              = 0xA0
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

   AAX_EXPONENTIAL_DISTANCE_DELAY = AAX_EXPONENTIAL_DISTANCE | 0x80,

   AAX_AL_INVERSE_DISTANCE = 1000,
   AAX_AL_INVERSE_DISTANCE_CLAMPED,
   AAX_AL_LINEAR_DISTANCE,
   AAX_AL_LINEAR_DISTANCE_CLAMPED,
   AAX_AL_EXPONENT_DISTANCE,
   AAX_AL_EXPONENT_DISTANCE_CLAMPED,

   AAX_AL_DISTANCE_MODEL_MAX
};

enum aaxFilterType
{
   AAX_FILTER_NONE = 0,
   AAX_EQUALIZER,
   AAX_VOLUME_FILTER,
   AAX_TREMOLO_FILTER,
   AAX_TIMED_GAIN_FILTER,
   AAX_ANGULAR_FILTER,
   AAX_DISTANCE_FILTER,
   AAX_FREQUENCY_FILTER,
   AAX_FILTER_MAX
};

enum aaxEffectType
{
   AAX_EFFECT_NONE = 0,
   AAX_PITCH_EFFECT,
   AAX_VIBRATO_EFFECT,
   AAX_TIMED_PITCH_EFFECT,
   AAX_DISTORTION_EFFECT,
   AAX_PHASING_EFFECT,
   AAX_CHORUS_EFFECT,
   AAX_FLANGING_EFFECT,
   AAX_VELOCITY_EFFECT,
   AAX_EFFECT_MAX
};

enum aaxParameter {	/* Filter & Effect parameter number definitions */
   /* AAX_VOLUME_FILTER */
   AAX_GAIN = 0, AAX_MIN_GAIN, AAX_MAX_GAIN,
   /* AAX_ANGULAR_FILTER */
   AAX_INNER_ANGLE = 0, AAX_OUTER_ANGLE, AAX_OUTER_GAIN,
   /* AAX_DISTANCE_FILTER */
   AAX_REF_DISTANCE = 0, AAX_MAX_DISTANCE, AAX_ROLLOFF_FACTOR,
   /* AAX_FREQUENCY_FILTER */
   /* AAX_EQUALIZER        */
   AAX_CUTOFF_FREQUENCY = 0, AAX_LF_GAIN, AAX_HF_GAIN,
   /* AAX_EQUALIZER        */
   AAX_CUTOFF_FREQUENCY_HF = 0x10, AAX_LF_GAIN_HF, AAX_HF_GAIN_HF,

   /* AAX_TIMED_GAIN_FILTER */
   /* AAX_TIMED_PITCH_EFFECT */
   AAX_LEVEL0 = 0x00, AAX_TIME0, AAX_LEVEL1, AAX_TIME1,
   AAX_LEVEL2 = 0x10, AAX_TIME2, AAX_LEVEL3, AAX_TIME3,
   AAX_LEVEL4 = 0x20, AAX_TIME4, AAX_LEVEL5, AAX_TIME5,
   AAX_LEVEL6 = 0x30, AAX_TIME6, AAX_LEVEL7, AAX_TIME7,

   /* AAX_TREMOLO_FILTER, ignores AAX_DELAY_GAIN */
   /* AAX_VIBRATO_EFFECT, ignores AAX_DELAY_GAIN */
   /* AAX_PHASING_EFFECT  */
   /* AAX_CHORUS_EFFECT   */
   /* AAX_FLANGING_EFFECT */
   AAX_DELAY_GAIN = 0, AAX_LFO_FREQUENCY, AAX_LFO_DEPTH, AAX_LFO_OFFSET,

   /* AAX_PITCH_EFFECT */
   AAX_PITCH = 0, AAX_MAX_PITCH, AAX_SPEEDUP,
   /* AAX_DISTORTION_EFFECT*/
   AAX_DISTORTION_FACTOR = 0, CLIPPING_FACTOR, AAX_MIX_FACTOR,
   /* AAX_VELOCITY_EFFECT */
   AAX_SOUND_VELOCITY = 0, AAX_DOPPLER_FACTOR
};


enum aaxWaveformType
{
   AAX_WAVE_NONE 	= 0,

   AAX_TRIANGLE_WAVE    = 0x0001,
   AAX_SINE_WAVE	= 0x0002,
   AAX_SQUARE_WAVE	= 0x0004,
   AAX_SAWTOOTH_WAVE	= 0x0008,
   AAX_IMPULSE_WAVE	= 0x0010,
   AAX_WHITE_NOISE	= 0x0020,
   AAX_PINK_NOISE	= 0x0040,
   AAX_BROWNIAN_NOISE	= 0x0080,
   AAX_LAST_WAVEFORM	= AAX_BROWNIAN_NOISE,

   AAX_MAX_WAVE		= 8
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

enum aaxInstrumentParameter
{
   AAX_NOTEPARAM_NONE = 0,

   AAX_NOTE_PITCHBEND,
   AAX_NOTE_VELOCITY,
   AAX_NOTE_AFTERTOUCH,
   AAX_NOTE_DISPLACEMENT,
   AAX_NOTE_SUSTAIN,
   AAX_NOTE_SOFTEN,

   AAX_NOTEPARAM_MAX
};

#define AAX_FALSE	0
#define AAX_TRUE	1
#define AAX_FPNONE	(0.0f/0.0f)
#define AAX_FPINFINITE	(1.0f/0.0f)

typedef void* aaxConfig;
typedef void* aaxBuffer;
typedef void* aaxEmitter;
typedef void* aaxFilter;
typedef void* aaxEffect;
typedef void* aaxFrame;
typedef void* aaxInstrument;

typedef float aaxVec3f[3];
typedef float aaxVec4f[4];
typedef float aaxMtx4f[4][4];

/*
 * Version and Support information
 */
AAX_API unsigned AAX_APIENTRY aaxGetMajorVersion();
AAX_API unsigned AAX_APIENTRY aaxGetMinorVersion();
AAX_API unsigned int AAX_APIENTRY aaxGetPatchLevel();
AAX_API const char* AAX_APIENTRY aaxGetVersionString(aaxConfig);

AAX_API enum aaxErrorType AAX_APIENTRY aaxGetErrorNo();
AAX_API const char* AAX_APIENTRY aaxGetErrorString(enum aaxErrorType);

AAX_API unsigned AAX_APIENTRY aaxGetBytesPerSample(enum aaxFormat);
AAX_API int AAX_APIENTRY aaxIsValid(const void*, enum aaxHandleType);

AAX_API int AAX_APIENTRY aaxIsFilterSupported(aaxConfig, const char*);
AAX_API int AAX_APIENTRY aaxIsEffectSupported(aaxConfig, const char*);

/*
 * Driver information
 */
AAX_API aaxConfig AAX_APIENTRY aaxDriverGetByName(const char*, enum aaxRenderMode mode);
AAX_API unsigned AAX_APIENTRY aaxDriverGetCount(enum aaxRenderMode mode);
AAX_API aaxConfig AAX_APIENTRY aaxDriverGetByPos(unsigned, enum aaxRenderMode mode);
AAX_API int AAX_APIENTRY aaxDriverDestroy(aaxConfig);

AAX_API unsigned AAX_APIENTRY aaxDriverGetDeviceCount(const aaxConfig, enum aaxRenderMode mode);
AAX_API const char* AAX_APIENTRY aaxDriverGetDeviceNameByPos(const aaxConfig, unsigned, enum aaxRenderMode mode);
AAX_API unsigned AAX_APIENTRY aaxDriverGetInterfaceCount(const aaxConfig, const char*, enum aaxRenderMode mode);
AAX_API const char* AAX_APIENTRY aaxDriverGetInterfaceNameByPos(const aaxConfig, const char*, unsigned, enum aaxRenderMode mode);
AAX_API const char* AAX_APIENTRY aaxDriverGetSetup(const aaxConfig, enum aaxSetupType);
AAX_API int AAX_APIENTRY aaxDriverGetSupport(const aaxConfig, enum aaxRenderMode);

/*
 * Driver functions
 */
AAX_API aaxConfig AAX_APIENTRY aaxDriverOpenDefault(enum aaxRenderMode);
AAX_API aaxConfig AAX_APIENTRY aaxDriverOpenByName(const char*, enum aaxRenderMode);
AAX_API aaxConfig AAX_APIENTRY aaxDriverOpen(aaxConfig);
AAX_API int AAX_APIENTRY aaxDriverClose(aaxConfig);

/*
 * Mixer/Playback setup
 */
AAX_API int AAX_APIENTRY aaxMixerSetSetup(aaxConfig, enum aaxSetupType, unsigned int);
AAX_API int AAX_APIENTRY aaxMixerSetFilter(aaxConfig, aaxFilter);
AAX_API int AAX_APIENTRY aaxMixerSetEffect(aaxConfig, aaxEffect);
AAX_API int AAX_APIENTRY aaxMixerSetState(aaxConfig, enum aaxState);
AAX_API int AAX_APIENTRY aaxMixerRegisterEmitter(const aaxConfig, const aaxEmitter);
AAX_API int AAX_APIENTRY aaxMixerDeregisterEmitter(const aaxConfig, const aaxEmitter);
AAX_API int AAX_APIENTRY aaxMixerRegisterAudioFrame(const aaxConfig, const aaxFrame);
AAX_API int AAX_APIENTRY aaxMixerDeregisterAudioFrame(const aaxConfig, const aaxFrame);

AAX_API unsigned int aaxMixerGetSetup(const aaxConfig, enum aaxSetupType);
AAX_API const aaxFilter AAX_APIENTRY aaxMixerGetFilter(const aaxConfig, enum aaxFilterType);
AAX_API const aaxEffect AAX_APIENTRY aaxMixerGetEffect(const aaxConfig, enum aaxEffectType);

/*
 * Scenery setup
 */
AAX_API int AAX_APIENTRY aaxScenerySetFilter(aaxConfig, aaxFilter);
AAX_API int AAX_APIENTRY aaxScenerySetEffect(aaxConfig, aaxEffect);

AAX_API const aaxFilter AAX_APIENTRY aaxSceneryGetFilter(const aaxConfig, enum aaxFilterType);
AAX_API const aaxEffect AAX_APIENTRY aaxSceneryGetEffect(const aaxConfig, enum aaxEffectType);

/*
 * Sound buffer related
 */
AAX_API aaxBuffer AAX_APIENTRY aaxBufferCreate(aaxConfig, unsigned int, unsigned, enum aaxFormat);
AAX_API int AAX_APIENTRY aaxBufferDestroy(aaxBuffer);
AAX_API int AAX_APIENTRY aaxBufferSetSetup(aaxBuffer, enum aaxSetupType, unsigned int);
AAX_API int AAX_APIENTRY aaxBufferSetData(aaxBuffer, const void*);
AAX_API int AAX_APIENTRY aaxBufferProcessWaveform(aaxBuffer, float, enum aaxWaveformType, float, enum aaxProcessingType);

AAX_API void** AAX_APIENTRY aaxBufferGetData(const aaxBuffer);
AAX_API unsigned int AAX_APIENTRY aaxBufferGetSetup(const aaxBuffer, enum aaxSetupType);
AAX_API int AAX_APIENTRY aaxBufferWriteToFile(aaxBuffer, const char*, enum aaxProcessingType);

/*
 * Source/Emitter manipulation
 */
AAX_API aaxEmitter AAX_APIENTRY aaxEmitterCreate();
AAX_API int AAX_APIENTRY aaxEmitterAddBuffer(aaxEmitter, aaxBuffer);
AAX_API int AAX_APIENTRY aaxEmitterRemoveBuffer(aaxEmitter);
AAX_API int AAX_APIENTRY aaxEmitterDestroy(aaxEmitter);
AAX_API int AAX_APIENTRY aaxEmitterSetMode(aaxEmitter, enum aaxModeType, int);
AAX_API int AAX_APIENTRY aaxEmitterSetFilter(aaxEmitter, aaxFilter);
AAX_API int AAX_APIENTRY aaxEmitterSetEffect(aaxEmitter, aaxEffect);
AAX_API int AAX_APIENTRY aaxEmitterSetState(aaxEmitter, enum aaxState);
AAX_API int AAX_APIENTRY aaxEmitterSetOffset(aaxEmitter, unsigned long, enum aaxType);
AAX_API int AAX_APIENTRY aaxEmitterSetOffsetSec(aaxEmitter, float);
AAX_API int AAX_APIENTRY aaxEmitterSetMatrix(aaxConfig, aaxMtx4f);
AAX_API int AAX_APIENTRY aaxEmitterSetVelocity(aaxEmitter, const aaxVec3f);

AAX_API int AAX_APIENTRY aaxEmitterGetMode(const aaxEmitter, enum aaxModeType);
AAX_API int AAX_APIENTRY aaxEmitterGetState(const aaxEmitter);
AAX_API const aaxFilter AAX_APIENTRY aaxEmitterGetFilter(const aaxEmitter, enum aaxFilterType);
AAX_API const aaxEffect AAX_APIENTRY aaxEmitterGetEffect(const aaxEmitter, enum aaxEffectType);
AAX_API unsigned int AAX_APIENTRY aaxEmitterGetNoBuffers(const aaxEmitter, enum aaxState);
AAX_API const aaxBuffer AAX_APIENTRY aaxEmitterGetBufferByPos(const aaxEmitter, unsigned int, int);
AAX_API float AAX_APIENTRY aaxEmitterGetOffsetSec(const aaxEmitter);
AAX_API unsigned long  AAX_APIENTRY aaxEmitterGetOffset(const aaxEmitter, enum aaxType);
AAX_API unsigned int AAX_APIENTRY aaxEmitterSetSetup(const aaxEmitter, enum aaxSetupType);
AAX_API int AAX_APIENTRY aaxEmitterGetMatrix(aaxConfig, aaxMtx4f);
AAX_API int AAX_APIENTRY aaxEmitterGetVelocity(const aaxEmitter, aaxVec3f);

/*
 * Listener/Sensor manipulation
 */
AAX_API int AAX_APIENTRY aaxSensorSetMatrix(aaxConfig, aaxMtx4f);
AAX_API int AAX_APIENTRY aaxSensorSetVelocity(aaxConfig, const aaxVec3f);

AAX_API int AAX_APIENTRY aaxSensorGetMatrix(const aaxConfig, aaxMtx4f);
AAX_API int AAX_APIENTRY aaxSensorGetVelocity(const aaxConfig, aaxVec3f);

AAX_API int AAX_APIENTRY aaxSensorSetState(aaxConfig, enum aaxState);
AAX_API int AAX_APIENTRY aaxSensorWaitForBuffer(const aaxConfig, float);
AAX_API aaxBuffer AAX_APIENTRY aaxSensorGetBuffer(const aaxConfig);
AAX_API unsigned long AAX_APIENTRY aaxSensorGetOffset(const aaxConfig, enum aaxType);

/*
 * Filter support
 */
typedef int (*aaxFilterFn)(void*, aaxFilter);
AAX_API aaxFilter AAX_APIENTRY aaxFilterCreate(aaxConfig config, enum aaxFilterType);
AAX_API int AAX_APIENTRY aaxFilterSetParam(aaxFilter, int, int, float);
AAX_API aaxFilter AAX_APIENTRY aaxFilterSetSlotParams(aaxFilter, unsigned, int, aaxVec4f);
AAX_API aaxFilter AAX_APIENTRY aaxFilterSetSlot(aaxFilter, unsigned, int, float, float, float, float);
AAX_API aaxFilter AAX_APIENTRY aaxFilterSetState(aaxFilter, int);
AAX_API float AAX_APIENTRY aaxFilterGetParam(const aaxFilter, int, int);
AAX_API aaxFilter AAX_APIENTRY aaxFilterGetSlotParams(const aaxFilter, unsigned, int, aaxVec4f);
AAX_API aaxFilter AAX_APIENTRY aaxFilterGetSlot(const aaxFilter, unsigned, int, float*, float*, float*, float*);
AAX_API int AAX_APIENTRY aaxFilterDestroy(aaxFilter);
AAX_API float AAX_APIENTRY aaxFilterApplyParam(const aaxFilter, int, int, int);
AAX_API aaxFilter AAX_APIENTRY aaxFilterApply(aaxFilterFn, void *, aaxFilter);

/*
 * Effect support
 */
typedef int (*aaxEffectFn)(void*, aaxEffect);
AAX_API aaxEffect AAX_APIENTRY aaxEffectCreate(aaxConfig config, enum aaxEffectType);
AAX_API int AAX_APIENTRY aaxEffectSetParam(aaxEffect, int, int, float);
AAX_API aaxEffect AAX_APIENTRY aaxEffectSetSlotParams(aaxEffect, unsigned, int, aaxVec4f);
AAX_API aaxEffect AAX_APIENTRY aaxEffectSetSlot(aaxEffect, unsigned, int, float, float, float, float);
AAX_API aaxEffect AAX_APIENTRY aaxEffectSetState(aaxEffect, int);
AAX_API float AAX_APIENTRY aaxEffectGetParam(const aaxEffect, int , int);
AAX_API aaxEffect AAX_APIENTRY aaxEffectGetSlotParams(const aaxEffect, unsigned, int, aaxVec4f);
AAX_API aaxEffect AAX_APIENTRY aaxEffectGetSlot(const aaxEffect, unsigned, int, float*, float*, float*, float*);
AAX_API int AAX_APIENTRY aaxEffectDestroy(aaxEffect);
AAX_API float AAX_APIENTRY aaxEffectApplyParam(const aaxEffect, int, int, int);
AAX_API aaxEffect AAX_APIENTRY aaxEffectApply(aaxEffectFn, void *, aaxEffect);

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


/*
 * AudioFrame support (version 2.0 and later)
 */
AAX_API aaxFrame AAX_APIENTRY aaxAudioFrameCreate(aaxConfig);
AAX_API int AAX_APIENTRY aaxAudioFrameDestroy(aaxFrame);
AAX_API int AAX_APIENTRY aaxAudioFrameSetMode(aaxFrame, enum aaxModeType, int);
AAX_API int AAX_APIENTRY aaxAudioFrameRegisterEmitter(const aaxFrame, const aaxEmitter);
AAX_API int AAX_APIENTRY aaxAudioFrameDeregisterEmitter(const aaxFrame, const aaxEmitter);
AAX_API int AAX_APIENTRY aaxAudioFrameSetMatrix(aaxFrame, aaxMtx4f);
AAX_API int AAX_APIENTRY aaxAudioFrameSetVelocity(aaxFrame, const aaxVec3f);
AAX_API int AAX_APIENTRY aaxAudioFrameSetFilter(aaxFrame, aaxFilter);
AAX_API int AAX_APIENTRY aaxAudioFrameSetEffect(aaxFrame, aaxEffect);
AAX_API int AAX_APIENTRY aaxAudioFrameSetState(aaxFrame, enum aaxState);

AAX_API int AAX_APIENTRY aaxAudioFrameGetMatrix(aaxFrame, aaxMtx4f);
AAX_API int AAX_APIENTRY aaxAudioFrameGetVelocity(aaxFrame, aaxVec3f);
AAX_API int AAX_APIENTRY aaxAudioFrameWaitForBuffer(aaxFrame, float);
AAX_API aaxBuffer AAX_APIENTRY aaxAudioFrameGetBuffer(const aaxFrame);
AAX_API const aaxFilter AAX_APIENTRY aaxAudioFrameGetFilter(const aaxFrame, enum aaxFilterType);
AAX_API const aaxEffect AAX_APIENTRY aaxAudioFrameGetEffect(const aaxFrame, enum aaxEffectType);


/*
 * Instrument support (version 2.2 and later)
 */
#if AAX_PATCH_LEVEL > 111101
AAX_API aaxInstrument AAX_APIENTRY aaxInstrumentCeate(aaxConfig);
AAX_API int AAX_APIENTRY aaxInstrumentDestroy(aaxInstrument);
AAX_API int AAX_APIENTRY aaxInstrumentLoad(aaxInstrument, const char*, unsigned int, unsigned int);
AAX_API int AAX_APIENTRY aaxInstrumentLoadByName(aaxInstrument, const char*, const char*);
AAX_API int AAX_APIENTRY aaxInstrumentSetSetup(aaxInstrument, enum aaxSetupType, int);
AAX_API int AAX_APIENTRY aaxInstrumentRegister(aaxInstrument);
AAX_API int AAX_APIENTRY aaxInstrumentDeregister(aaxInstrument);
AAX_API int AAX_APIENTRY aaxInstrumentSetParam(aaxInstrument, enum aaxInstrumentParameter, float);
AAX_API int AAX_APIENTRY aaxInstrumentNoteOn(aaxInstrument, unsigned int);
AAX_API int AAX_APIENTRY aaxInstrumentNoteOff(aaxInstrument, unsigned int);
AAX_API int AAX_APIENTRY aaxInstrumentNoteSetParam(aaxInstrument, unsigned int, enum aaxInstrumentParameter, float);

AAX_API int AAX_APIENTRY aaxInstrumentGetSetup(aaxInstrument, enum aaxSetupType);
AAX_API float AAX_APIENTRY aaxInstrumentGetParam(aaxInstrument, enum aaxInstrumentParameter);
AAX_API float AAX_APIENTRY aaxInstrumentNoteGetParam(aaxInstrument, unsigned int, enum aaxInstrumentParameter);
#endif


#if defined(TARGET_OS_MAC) && TARGET_OS_MAC
# pragma export off
#endif

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif

