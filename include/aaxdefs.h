/*
 * Copyright 2005-2011 by Erik Hofman.
 * Copyright 2007-2011 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef AAXDEFS_H
#define AAXDEFS_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include <aax.h>

#define AAX_DEFINITIONS	1

/*
 * Filter and Effect support
 *
 * PTYPE: parameter type (like AAX_LINEAR)
 * EFL:   either Effect or Filter
 * TYPE:  EffectType or FilterType style parameter
 * GRP:   Emitter, Scenery, Mixer or AudioFrame
 */
#define __intSetParam1(a,PTYPE,b,EFL,TYPE,GRP)				\
  aax##EFL##Destroy(aax##EFL##Apply(aax##GRP##Set##EFL,a,		\
    aax##EFL##SetSlot(aax##GRP##Get##EFL(a,TYPE),0,PTYPE,		\
				       b,AAX_FPNONE,AAX_FPNONE,AAX_FPNONE)))

#define __intSetParam2(a,PTYPE,b,EFL,TYPE,GRP)				\
  aax##EFL##Destroy(aax##EFL##Apply(aax##GRP##Set##EFL,a,		\
    aax##EFL##SetSlot(aax##GRP##Get##EFL(a,TYPE),0,PTYPE,		\
				       AAX_FPNONE,b,AAX_FPNONE,AAX_FPNONE)))

#define __intSetParam3(a,PTYPE,b,EFL,TYPE,GRP)				\
  aax##EFL##Destroy(aax##EFL##Apply(aax##GRP##Set##EFL,a,		\
    aax##EFL##SetSlot(aax##GRP##Get##EFL(a,TYPE),0,PTYPE,		\
				       AAX_FPNONE,AAX_FPNONE,b,AAX_FPNONE)))

#define __intSetAll(a,PTYPE,c,d,e,EFL,TYPE,GRP)				\
  aax##EFL##Destroy(aax##EFL##Apply(aax##GRP##Set##EFL,a,		\
    aax##EFL##SetSlot(aax##GRP##Get##EFL(a,TYPE),0,PTYPE,c,d,e,AAX_FPNONE)))

#define __intSetState(a,b,EFL,TYPE,GRP)					\
  aax##EFL##Destroy(aax##EFL##Apply(aax##GRP##Set##EFL,a,		\
    aax##EFL##SetState(aax##GRP##Get##EFL(a,TYPE),b)))

#define __intGetParam1(a,b,EFL,TYPE,GRP)				\
  aax##EFL##ApplyParam(aax##GRP##Get##EFL(a,TYPE),0,0,b)
 
#define __intGetParam2(a,b,EFL,TYPE,GRP)				\
  aax##EFL##ApplyParam(aax##GRP##Get##EFL(a,TYPE),0,1,b)

#define __intGetParam3(a,b,EFL,TYPE,GRP)				\
  aa##EFL##ApplyParam(aax##GRP##Get##EFL(a,TYPE),0,2,b)

#define __intGetAll(a,b,c,d,e,EFL,FLTR,GRP)				\
  aax##EFL##Destroy(aaxFilterGetSlot(aax##GRP##Get##EFL(a,FLTR),0,b,c,d,e,NULL))

	/* filters */
#define __aaxFilterSetParam1(a,PTYPE,b,FLT,GRP)			 	\
	__intSetParam1(a,PTYPE,b,Filter,AAX_##FLT##_FILTER,GRP)
#define __aaxFilterSetParam2(a,PTYPE,b,FLT,GRP)			 	\
	__intSetParam2(a,PTYPE,b,Filter,AAX_##FLT##_FILTER,GRP)
#define __aaxFilterSetParam3(a,PTYPE,b,FLT,GRP)			 	\
	__intSetParam3(a,PTYPE,b,Filter,AAX_##FLT##_FILTER,GRP)
#define __aaxFilterSet(a,PTYPE,c,d,e,FLT,GRP)				\
	__intSetAll(a,PTYPE,c,d,e,Filter,AAX_##FLT##_FILTER,GRP)
#define __aaxFilterSetState(a,b,FLT,GRP)				\
	__intSetState(a,b,Filter,AAX_##FLT##_FILTER,GRP)

#define __aaxFilterGetParam1(a,b,FLT,GRP)				\
	__intGetParam1(a,b,Filter,AAX_##FLT##_FILTER,GRP)
#define __aaxFilterGetParam2(a,b,FLT,GRP)				\
	__intGetParam2(a,b,Filter,AAX_##FLT##_FILTER,GRP)
#define __aaxFilterGetParam3(a,b,FLT,GRP)				\
	__intGetParam3(a,b,Filter,AAX_##FLT##_FILTER,GRP)
#define __aaxFilterGet(a,b,c,d,e,FLT,GRP)				\
	__intGetAll(a,b,c,d,e,Filter,AAX_##FLT##_FILTER,GRP)

	/* effects */
#define __aaxEffectSetParam1(a,PTYPE,b,EFF,GRP)				\
	__intSetParam1(a,PTYPE,b,Effect,AAX_##EFF##_EFFECT,GRP)
#define __aaxEffectSetParam2(a,PTYPE,b,EFF,GRP)				\
	__intSetParam2(a,PTYPE,b,Effect,AAX_##EFF##_EFFECT,GRP)
#define __aaxEffectSetParam3(a,PTYPE,b,EFF,GRP)				\
	 __intSetParam3(a,PTYPE,b,Effect,AAX_##EFF##_EFFECT,GRP)
#define __aaxEffectSet(a,PTYPE,c,d,e,EFF,GRP)				\
	__intSetAll(a,PTYPE,c,d,e,Effect,AAX_##EFF##_EFFECT,GRP)
#define __aaxEffectSetState(a,b,EFF,GRP)				\
	__intSetState(a,b,Effect,AAX_##EFF##_EFFECT,GRP)

#define __aaxEffectGetParam1(a,b,EFF,GRP)				\
	__intGetParam1(a,b,Effect,AAX_##EFF##_EFFECT,GRP)
#define __aaxEffectGetParam2(a,b,EFF,GRP)				\
	__intGetParam2(a,b,Effect,AAX_##EFF##_EFFECT,GRP)
#define __aaxEffectGetParam3(a,b,EFF,GRP)				\
	__intGetParam3(a,b,Effect,AAX_##EFF##_EFFECT,GRP)
#define __aaxEffectGet(a,b,c,d,e,EFF,GRP)				\
	__intGetAll(a,b,c,d,e,Effect,AAX_##EFF##_EFFECT,GRP)


AAX_API aaxMtx4f aaxIdentityMatrix;

/*
 * Driver
 */
#define aaxDriverIsValid(a)						\
	aaxIsValid((a), AAX_CONFIG)
#define aaxDriverGetDefault(a)						\
	aaxDriverGetByName(NULL,(a))
#define aaxDriverGetName(a)						\
	aaxDriverGetSetup((a),AAX_NAME_STRING)
#define aaxDriverGetVersion(a)						\
	aaxDriverGetSetup((a),AAX_VERSION_STRING)
#define aaxDriverGetRenderer(a)						\
	aaxDriverGetSetup((a),AAX_RENDERER_STRING)
#define aaxDriverGetDriver(a)						\
	aaxDriverGetSetup((a),AAX_DRIVER_STRING)
#define aaxDriverGetVendor(a)						\
	aaxDriverGetSetup((a),AAX_VENDOR_STRING)

/*
 * Mixer/Playback setup and configuration
 */
#define aaxMixerInit(a)							\
	aaxMixerSetState((a),AAX_INITIALIZED)
#define aaxMixerSetNoTracks(a,b)					\
	aaxMixerSetSetup((a),AAX_TRACKS,(b))
#define aaxMixerSetFrequency(a,b)					\
	aaxMixerSetSetup((a),AAX_FREQUENCY,(b))
#define aaxMixerSetRefreshRate(a,b)					\
	aaxMixerSetSetup((a),AAX_REFRESHRATE,(b))
#define aaxMixerSetFormat(a,b)						\
	aaxMixerSetSetup((a),AAX_FORMAT,(b))
#define aaxMixerSetMonoSources(a)					\
	aaxMixerSetSetup(NULL,AAX_MONO_EMITTERS,(a))
#define aaxMixerSetStereoSources(a)					\
	aaxMixerSetSetup(NULL,AAX_STEREO_EMITTERS,(a))
#define aaxMixerSetup(a,b,c,d,e)					\
        (!e) ? AAX_FALSE : \
	aaxMixerSetSetup((a),AAX_FREQUENCY,(b))?			\
	(aaxMixerSetSetup((a),AAX_TRACKS,(c))?				\
	 (aaxMixerSetSetup((a),AAX_FORMAT,(d))?				\
	  (aaxMixerSetSetup((a),AAX_REFRESHRATE,			\
	(unsigned int)((b)*((float)((c)*aaxGetBytesPerSample(d)))/(float)(e)))?\
	   (aaxMixerSetState((a),AAX_INITIALIZED)):AAX_FALSE		\
          ):AAX_FALSE							\
	 ):AAX_FALSE							\
	):AAX_FALSE
#define aaxMixerStart(a)						\
	aaxMixerSetState((a),AAX_PLAYING)
#define aaxMixerStop(a)			 				\
	aaxMixerSetState((a),AAX_STOPPED)
#define aaxMixerSuspend(a)						\
	aaxMixerSetState((a),AAX_SUSPENDED)
#define aaxMixerResume(a)						\
	aaxMixerSetState((a),AAX_PLAYING)
#define aaxMixerUpdate(a)						\
        aaxMixerSetState((a),AAX_UPDATE)
#define aaxMixerSetGain(a,b)						\
	__aaxFilterSetParam1((a),AAX_LINEAR,(b),VOLUME,Mixer)
#define aaxMixerSetPitch(a,b)						\
	__aaxEffectSetParam1((a),AAX_LINEAR,(b),PITCH,Mixer)
#define aaxMixerGetFrequency(a)						\
	aaxMixerGetSetup((a),AAX_FREQUENCY)
#define aaxMixerGetRefreshRate(a)					\
	aaxMixerGetSetup((a),AAX_REFRESHRATE)
#define aaxMixerGetNoMonoSources()					\
	aaxMixerGetSetup(NULL,AAX_MONO_EMITTERS)
#define aaxMixerGetNoStereoSources()					\
	aaxMixerGetSetup(NULL,AAX_STEREO_EMITTERS)
#define aaxMixerGetGain(a)						\
	__aaxFilterGetParam1((a),AAX_LINEAR,VOLUME,Mixer)
#define aaxMixerGetPitch(a)						\
	__aaxEffectGetParam1((a),AAX_LINEAR,PITCH,Mixer)

/*
 * Scenery
 */
#define aaxScenerySetFrequencyFilter(a,b,c,d)				\
	__aaxFilterSet((a),AAX_LINEAR,(b),(c),(d),FREQUENCY,Scenery)
#define aaxSceneryEnableFrequencyFilter(a,b)				\
	__aaxFilterSetState(a,b,FREQUENCY,Scenery)
#define aaxScenerySetDistanceModel(a,b)					\
	__aaxFilterSetState(a,b,DISTANCE,Scenery)
#define aaxScenerySetSoundVelocity(a,b)					\
	__aaxEffectSetParam1((a),AAX_LINEAR,(b),VELOCITY,Scenery)
#define aaxScenerySetDopplerFactor(a,b)					\
	__aaxEffectSetParam2((a),AAX_LINEAR,(b),VELOCITY,Scenery)
#define aaxSceneryGetSoundVelocity(a)					\
	__aaxEffectGetParam1((a)(b),VELOCITY,Scenery)
#define aaxSceneryGetDopplerFactor(a,b)					\
	__aaxEffectGetParam2((a)(b),VELOCITY,Scenery)

/*
 * Buffer
 */
#define aaxBufferIsValid(a)						\
	aaxIsValid((a), AAX_BUFFER)
#define aaxBufferSetFrequency(a,b)					\
	aaxBufferSetSetup((a),AAX_FREQUENCY,(b))
#define aaxBufferSetLoopPoints(a,b,c)					\
	aaxBufferSetSetup((a),AAX_LOOP_START,(b))?			\
	aaxBufferSetSetup((a),AAX_LOOP_END,(c)):AAX_FALSE
#define aaxBufferSetWaveform(a,b,c)					\
	aaxBufferProcessWaveform(a,b,c,1.0f,AAX_OVERWRITE)
#define aaxBufferAddWaveform(a,b,c,d)					\
	aaxBufferProcessWaveform(a,b,c,d,AAX_ADD)
#define aaxBufferMixWaveform(a,b,c,d)					\
	aaxBufferProcessWaveform(a,b,c,d,AAX_MIX)
#define aaxBufferRingmodulateWaveform(a,b,c,d)				\
	aaxBufferProcessWaveform(a,b,c,d,AAX_RINGMODULATE)
#define aaxBufferGetNoTracks(a)						\
	aaxBufferGetSetup((a),AAX_TRACKS)
#define aaxBufferGetFrequency(a)					\
	aaxBufferGetSetup((a),AAX_FREQUENCY)
#define aaxBufferGetFormat(a)						\
	aaxBufferGetSetup((a),AAX_FORMAT)
#define aaxBufferGetSize(a)						\
	aaxBufferGetSetup((a),AAX_NO_SAMPLES)				\
	* aaxBufferGetSetup((a),AAX_TRACKS)				\
	* aaxGetBytesPerSample(aaxBufferGetSetup((a),AAX_FORMAT))
#define aaxBufferGetNoSamples(a)					\
	aaxBufferGetSetup((a),AAX_NO_SAMPLES)
#define aaxBufferGetBytesPerSample(a)					\
	aaxGetBytesPerSample(aaxBufferGetSetup((a),AAX_FORMAT))

/*
 * Source/Emitter manipulation
 */
#define aaxEmitterIsValid(a)						\
	aaxIsValid((a), AAX_EMITTER)
#define aaxEmitterStart(a)						\
	aaxEmitterSetState((a),AAX_PLAYING)
#define aaxEmitterStop(a)						\
	aaxEmitterSetState((a),AAX_STOPPED)
#define aaxEmitterSuspend(a)						\
	aaxEmitterSetState((a),AAX_SUSPENDED)
#define aaxEmitterResume(a)				 		\
	aaxEmitterSetState((a),AAX_PLAYING)
#define aaxEmitterRewind(a)				 		\
	aaxEmitterSetState((a),AAX_INITIALIZED)
#define aaxEmitterSetGain(a,b)						\
	__aaxFilterSetParam1((a),AAX_LINEAR,(b),VOLUME,Emitter)
#define aaxEmitterSetPitch(a,b)						\
	__aaxEffectSetParam1((a),AAX_LINEAR,(b),PITCH,Emitter)
#define aaxEmitterSetGainMinMax(a,b,c)					\
	__aaxFilterSet((a),AAX_LINEAR,AAX_FPNONE,(b),(c),VOLUME,Emitter)
#define aaxEmitterSetOffsetSamples(a,b)	 				\
	aaxEmitterSetOffset((a),(b),AAX_SAMPLES)
#define aaxEmitterSetOffsetFrames(a,b)					\
	aaxEmitterSetOffset((a),( b),AAX_FRAMES)
#define aaxEmitterSetOffsetBytes(a,b)		 			\
	aaxEmitterSetOffset((a),(b),AAX_BYTES)
#define aaxEmitterSetIdentityMatrix(a)					\
	aaxEmitterSetMatrix(a,aaxIdentityMatrix)
#define aaxEmitterSetReferenceDistance(a,b)				\
	__aaxFilterSetParam1((a),AAX_LINEAR,(b),DISTANCE,Emitter)
#define aaxEmitterSetMaxDistance(a,b)					\
	__aaxFilterSetParam2((a),AAX_LINEAR,(b),DISTANCE,Emitter)
#define aaxEmitterSetRolloffFactor(a,b)					\
	__aaxFilterSetParam3((a),AAX_LINEAR,(b),DISTANCE,Emitter)
#define aaxEmitterSetDistanceModel(a,b)					\
	__aaxFilterSetState(a,b,DISTANCE,Emitter)
#define aaxEmitterSetAudioCone(a,b,c,d)					\
	__aaxFilterSet((a),AAX_RADIANS,(b),(c),(d),ANGULAR,Emitter)
#define aaxEmitterSetFrequencyFilter(a,b,c,d)				\
	__aaxFilterSet((a),AAX_LINEAR,(b),(c),(d),FREQUENCY,Emitter)
#define aaxEmitterEnableFrequencyFilter(a,b)				\
	__aaxFilterSetState(a,b,FREQUENCY,Emitter)
#define aaxEmitterSetBufferTrack(a, b)					\
	aaxEmitterSetMode(a, AAX_BUFFER_TRACK, b)
#define aaxEmitterSetLooping(a,b)					\
	aaxEmitterSetMode(a, AAX_LOOPING, b)
#define aaxEmitterGetSetup(a,b)						\
	aaxBufferGetSetup(aaxEmitterGetBufferByPos(a,0,AAX_FALSE),(b))
#define aaxEmitterGetBuffer(a)						\
	aaxEmitterGetBufferByPos((a),0,AAX_FALSE)
#define aaxEmitterGetNoTracks(a)					\
	aaxEmitterGetSetup((a),AAX_TRACKS)
#define aaxEmitterGetFormat(a)						\
	aaxEmitterGetSetup((a),AAX_FORMAT)
#define aaxEmitterGetGain(a)						\
	__aaxFilterGetParam1((a),AAX_LINEAR,VOLUME,Emitter)
#define aaxEmitterGetPitch(a)						\
	__aaxEffectGetParam1((a),AAX_LINEAR,PITCH,Emitter)
#define aaxEmitterGetGainMinMax(a,b,c)				\
	__aaxFilterGet((a),AAX_LINEAR,NULL,(b),(c),VOLUME,Emitter)
#define aaxEmitterGetReferenceDistance(a)				\
	__aaxFilterGetParam1((a),AAX_LINEAR,DISTANCE,Emitter)
#define aaxEmitterGetMaxDistance(a)					\
	__aaxFilterGetParam2((a),AAX_LINEAR,DISTANCE,Emitter)
#define aaxEmitterGetRolloffFactor(a)					\
	__aaxFilterGetParam2((a),AAX_LINEAR,DISTANCE,Emitter)
#define aaxEmitterGetAudioCone(a,b,c,d)					\
	__aaxFilterGet((a),AAX_RADIANS,(b),(c),(d),ANGULAR,Emitter)
#define aaxEmitterGetOffsetSamples(a)		 			\
	aaxEmitterGetOffset((a),AAX_SAMPLES)
#define aaxEmitterGetOffsetFrames(a)					\
	aaxEmitterGetOffset((a),AAX_FRAMES)
#define aaxEmitterGetOffsetBytes(a)			 		\
	aaxEmitterGetOffset((a),AAX_BYTES)
#define aaxEmitterGetLooping(a)						\
	aaxEmitterGetMode(a, AAX_LOOPING)

/*
 * AudioFrame setup and configuration
 */
#define aaxAudioFrameIsValid(a)					\
        aaxIsValid((a), AAX_AUDIOFRAME)
#define aaxAudioFrameStart(a)						\
	aaxAudioFrameSetState((a),AAX_PLAYING)
#define aaxAudioFrameStop(a)						\
	aaxAudioFrameSetState((a),AAX_STOPPED)
#define aaxAudioFrameSuspend(a)						\
	aaxAudioFrameSetState((a),AAX_SUSPENDED)
#define aaxAudioFrameResume(a)						\
	aaxAudioFrameSetState((a),AAX_PLAYING)
#define aaxAudioFrameUpdate(a)						\
	aaxAudioFrameSetState((a),AAX_UPDATE)
#define aaxAudioFrameSetGain(a,b)					\
	__aaxFilterSetParam1((a),AAX_LINEAR,(b),VOLUME,AudioFrame)
#define aaxAudioFrameSetPitch(a,b)					\
	__aaxEffectSetParam1((a),AAX_LINEAR,(b),PITCH,AudioFrame)
#define aaxAudioFrameSetFrequencyFilter(a,b,c,d)			\
	__aaxFilterSet((a),AAX_LINEAR,(b),(c),(d),FREQUENCY,AudioFrame)
#define aaxAudioFrameEnableFrequencyFilter(a,b)				\
	__aaxFilterSetState(a,b,FREQUENCY,AudioFrame)
#define aaxAudioFrameSetIdentityMatrix(a)				\
	aaxAudioFrameSetMatrix(a,aaxIdentityMatrix)
#define aaxAudioFrameSetDistanceModel(a,b)				\
	__aaxFilterSetState(a,b,DISTANCE,AudioFrame)
#define aaxAudioFrameSetSoundVelocity(a,b)				\
	__aaxEffectSetParam1((a),AAX_LINEAR,(b),VELOCITY,AudioFrame)
#define aaxAudioFrameSetDopplerFactor(a,b)				\
	__aaxEffectSetParam2((a),AAX_LINEAR,(b),VELOCITY,AudioFrame)
#define aaxAudioFrameGetGain(a)					\
	__aaxFilterGetParam1((a),AAX_LINEAR,VOLUME,AudioFrame)
#define aaxAudioFrameGetPitch(a)					\
	__aaxEffectGetParam1((a),AAX_LINEAR,PITCH,AudioFrame)
#define aaxAudioFrameGetSoundVelocity(a)				\
	__aaxEffectGetParam1((a)(b),VELOCITY,AudioFrame)
#define aaxAudioFrameGetDopplerFactor(a,b)				\
	__aaxEffectGetParam2((a)(b),VELOCITY,AudioFrame)
	

/*
 * Listener/Sensor manipulation
 */
#define aaxSensorCaptureStart(a)					\
	aaxSensorSetState((a),AAX_RECORDING)
#define aaxSensorCaptureStop(a)			 			\
	aaxSensorSetState((a),AAX_STOPPED)
#define aaxSensorGetOffsetSamples(a)					\
	aaxSensorGetOffset((a),AAX_SAMPLES)
#define aaxSensorGetOffsetFrames(a)					\
	aaxSensorGetOffset((a),AAX_FRAMES)
#define aaxSensorGetOffsetBytes(a)					\
	aaxSensorGetOffset((a),AAX_BYTES)
#define aaxSensorSetIdentityMatrix(a)					\
        aaxSensorSetMatrix(a,aaxIdentityMatrix)

/*
 * Filter & Effect
 */
#define aaxFilterIsValid(a)						\
        aaxIsValid((a), AAX_FILTER)
#define aaxEffectIsValid(a)						\
	aaxIsValid((a), AAX_EFFECT)

/*
 * Instrument
 */
#define aaxInstrumentSetKeyVelocity(a, b, c)				\
	aaxInstrumentKeySetParam((a), (b), AAX_KEY_VELOCITY, (c))
#define aaxInstrumentSetKeyAftertouch(a, b, c)				\
	aaxInstrumentKeySetParam((a), (b), AAX_KEY_AFTERTOUCH, (c))
#define aaxInstrumentSetKeyDisplacement(a, b, c)			\
	aaxInstrumentKeySetParam((a), (b), AAX_KEY_DISPLACEMENT, (c))
#define aaxInstrumentSetKeyPitchBend(a, b, c)				\
	aaxInstrumentKeySetParam((a), (b), AAX_KEY_PITCHBEND, (c))

#endif

#if defined(__cplusplus)
}  /* extern "C" */
#endif

