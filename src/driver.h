/*
 * Copyright 2005-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_DRIVER_H
#define _AAX_DRIVER_H 1

#if HAVE_CONFIG_H
#include "config.h"
#endif

#define AAX_LIBRARY_STR			"AeonWave-HD"
#define AAX_LIBRARY_STR_LT		"AeonWave-Lite"

#define AAX_MIN_CONFIG_VERSION		0.0f
#define AAX_MAX_CONFIG_VERSION		1.1f

#define _AAX_MAX_MIXER_FREQUENCY	192000
#define _AAX_MAX_MIXER_FREQUENCY_LT	44100

#define _AAX_MAX_MIXER_REFRESH_RATE	1000
#define _AAX_MAX_MIXER_REFRESH_RATE_LT	100

#define _AAX_MAX_SOURCES_AVAIL		UINT_MAX
#define _AAX_MAX_SOURCES_AVAIL_LT	64

#define _AAX_MAX_MIXER_SOURCES		256
#define _AAX_MAX_MIXER_SOURCES_LT	64

#define AAX_NAME_STR			PACKAGE
#define AAX_VENDOR_STR			"Adalin B.V."
#define _AAX_MAX_SLAVES			(_AAX_MAX_SPEAKERS/2)
#define _AAX_MIN_MIXER_FREQUENCY	2000
#define _AAX_MIN_MIXER_REFRESH_RATE	1
#define _AAX_MAX_BACKENDS		6
#define _AAX_MAX_SPEAKERS		8


#include <aax.h>
#include <base/types.h>
#include <base/buffers.h>

#define AAX_DO_MKSTR(X)		#X
#define AAX_MKSTR(X)		AAX_DO_MKSTR(X)

#define AAX_VERSION_STR		AAX_MKSTR(AAX_MAJOR_VERSION)"." \
				AAX_MKSTR(AAX_MINOR_VERSION)"." \
				AAX_MKSTR(AAX_MICRO_VERSION)"-" \
				AAX_MKSTR(AAX_PATCH_LEVEL)

typedef void _aaxCodec(void*, const void*, unsigned char, unsigned int);

typedef int _aaxDriverDetect();
typedef char *_aaxDriverGetDevices(const void*, int mode);
typedef char *_aaxDriverGetInterfaces(const void*, const char*, int mode);

typedef void *_aaxDriverConnect(const void*, void*, const char*, enum aaxRenderMode);
typedef int _aaxDriverDisconnect(void*);
typedef char *_aaxDriverGetName(const void*, int);
typedef int _aaxDriverSetup(const void*, size_t*, int, unsigned int*, float*);
typedef int _aaxDriverState(const void*);

typedef int _aaxDriverCallback(const void*, void*, void*, float, float);
typedef int _aaxDriverRecordCallback(const void*, void*, size_t*, float, float);

typedef int _aaxDriver2dMixerCB(const void*, void*, void*, void*, void*, float, float);
typedef int _aaxDriver3dMixerCB(const void*, void*, void*, void*, void*, int);
typedef void _aaxDriverPrepare3d(void*, void*, const void*, const void*, void*);
typedef void _aaxDriverPostProcess(const void*, void*, const void*);
typedef void _aaxDriverPrepare(const void*, void*, const void*);
typedef void (*_aaxDriverCompress)(void*, unsigned int, unsigned int);

typedef void *_aaxDriverThread(void*);


typedef struct
{
    float gain;
    int format;
    unsigned int rate;
    unsigned int tracks;

    const char *version;
    const char *driver;
    const char *vendor;
    char *renderer;

    _aaxCodec **codecs;

    _aaxDriverDetect *detect;
    _aaxDriverGetDevices *get_devices;
    _aaxDriverGetInterfaces * get_interfaces;
 
    _aaxDriverGetName *name;
    _aaxDriverThread *thread;

    _aaxDriverConnect *connect;
    _aaxDriverDisconnect *disconnect;
    _aaxDriverSetup *setup;
    _aaxDriverState *pause;
    _aaxDriverState *resume;
    _aaxDriverRecordCallback *record;
    _aaxDriverCallback *play;
    _aaxDriver2dMixerCB *mix2d;
    _aaxDriver3dMixerCB *mix3d;
    _aaxDriverPrepare3d *prepare3d;
    _aaxDriverPostProcess *postprocess;
    _aaxDriverPrepare *effects;

    _aaxDriverState *support_playback;
    _aaxDriverState *support_recording;
    _aaxDriverState *is_available;

} _aaxDriverBackend;


/* ---  software device helper functions --- */

extern _aaxDriverPostProcess _aaxSoftwareDriverPostProcess;
extern _aaxDriverPrepare _aaxSoftwareDriverApplyEffects;
extern _aaxDriver3dMixerCB _aaxSoftwareDriver3dMixer;
extern _aaxDriver2dMixerCB _aaxSoftwareDriverStereoMixer;
extern _aaxDriverPrepare3d _aaxSoftwareDriver3dPrepare;
extern _aaxDriverThread _aaxSoftwareMixerThread;

void _aaxNoneDriverProcessFrame(void*);

unsigned int _aaxSoftwareMixerSignalFrames(void*);
void* _aaxSoftwareMixerReadFrame(void*, const void*, void*, float*);
int _aaxSoftwareMixerPlayFrame(void*, const void*, void*, void*);
void _aaxSoftwareMixerProcessFrame(void*, void*, void*, void*, void*, void*, void*, void*, const void*, void*);

void* _aaxSoftwareMixerThread(void*);
int _aaxSoftwareMixerThreadUpdate(void*, void*);
void _aaxSoftwareMixerPostProcess(const void *, void *, const void *);
void _aaxSoftwareMixerApplyEffects(const void *, void *, const void *);
unsigned int _aaxSoftwareMixerMixFrames(void*, _intBuffers*);

#endif /* !_AAX_DRIVER_H */

