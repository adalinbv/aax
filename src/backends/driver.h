/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#ifndef _AAX_DRIVER_H
#define _AAX_DRIVER_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#define AAX_LIBRARY_STR		"AeonWave-HD 64"

#define AAX_MIN_CONFIG_VERSION		0.0f
#define AAX_NEW_CONFIG_VERSION		2.0f
#define AAX_MAX_CONFIG_VERSION		2.9f

#define _AAX_MAX_MIXER_FREQUENCY	192000.0f

#define _AAX_MAX_MIXER_REFRESH_RATE	750

#define _AAX_MAX_SOURCES_AVAIL		UINT_MAX

#define _AAX_MAX_MIXER_REGISTERED	256

#define AAX_NAME_STR			PACKAGE_NAME
#define AAX_VENDOR_STR			"Adalin B.V., The Netherlands"
#define _AAX_MAX_SLAVES			(_AAX_MAX_SPEAKERS/2)
#define _AAX_MIN_MIXER_FREQUENCY	2000.0f
#define _AAX_MIN_MIXER_REFRESH_RATE	1.0f
#define _AAX_MAX_BACKENDS		7
#define _AAX_MAX_SPEAKERS		8


#include <aax/aax.h>
#include <base/types.h>
#include <base/buffers.h>

#define AAX_DO_MKSTR(X)		#X
#define AAX_MKSTR(X)		AAX_DO_MKSTR(X)

#define AAX_VERSION_STR		AAX_MKSTR(AAX_MAJOR_VERSION)"." \
				AAX_MKSTR(AAX_MINOR_VERSION)"." \
				AAX_MKSTR(AAX_MICRO_VERSION)"-" \
				AAX_MKSTR(AAX_PATCH_LEVEL)

enum _aaxDriverParam {
   /* float */
   DRIVER_LATENCY = 0,
   DRIVER_MIN_VOLUME,
   DRIVER_MAX_VOLUME,
   DRIVER_VOLUME,
   DRIVER_AGC_LEVEL,
   DRIVER_BLOCK_SIZE,
   DRIVER_FREQUENCY,

   /* int */
   DRIVER_MIN_FREQUENCY = 0x100,
   DRIVER_MAX_FREQUENCY,
   DRIVER_MIN_TRACKS,
   DRIVER_MAX_TRACKS,
   DRIVER_MIN_PERIODS,
   DRIVER_MAX_PERIODS,
   DRIVER_MAX_SOURCES,
   DRIVER_MAX_SAMPLES,	/* no. samples in the file or UINT_MAX */
   DRIVER_SAMPLE_DELAY,	/* no samples to go before the next sample is played */

   /* boolean */
   DRIVER_SHARED_MODE = 0x200,
   DRIVER_TIMER_MODE,
   DRIVER_BATCHED_MODE,
   DRIVER_SEEKABLE_SUPPORT
   
};

enum _aaxDriverState {
   DRIVER_AVAILABLE = 0,
   DRIVER_PAUSE,
   DRIVER_RESUME,
   DRIVER_SUPPORTS_PLAYBACK,
   DRIVER_SUPPORTS_CAPTURE,
   DRIVER_SHARED_MIXER,
   DRIVER_NEED_REINIT
};

/* forward declaration */
struct _aaxRenderer_t;

typedef char *_aaxDriverLog(const void*, int, int, const char *);

typedef int _aaxDriverDetect(int mode);
typedef void *_aaxDriverNewHandle(enum aaxRenderMode);
typedef void *_aaxDriverConnect(void*, const void*, void*, const char*, enum aaxRenderMode);
typedef int _aaxDriverDisconnect(void*);
typedef int _aaxDriverSetup(const void*, float*, int*, unsigned int*, float*, int*, int, float);

typedef char *_aaxDriverGetName(const void*, int);
typedef int _aaxDriverState(const void*, enum _aaxDriverState);
typedef float _aaxDriverParam(const void*, enum _aaxDriverParam);
typedef int _aaxDriverSetPosition(const void*, off_t);

typedef char *_aaxDriverGetDevices(const void*, int mode);
typedef char *_aaxDriverGetInterfaces(const void*, const char*, int mode);

typedef size_t _aaxDriverPlaybackCallback(const void*, void*, float, float, char);
typedef ssize_t _aaxDriverCaptureCallback(const void*, void**, ssize_t*, size_t*, void*, size_t, float, char);

typedef void _aaxDriverPrepare3d(void*, const void*, float, float, void*, void*);
typedef void _aaxDriverPostProcess(const void*, const void*, void*, const void*, const void*, void*);
typedef void _aaxDriverPrepare(const void*, const void*, void*, const void*, char, char);

typedef unsigned int _aaxDriverGetSetSources(unsigned int, int);

typedef void *_aaxDriverRingBufferCreate(float, enum aaxRenderMode);
typedef void _aaxDriverRingBufferDestroy(void*);


typedef struct _aaxRenderer_t *_aaxDriverRender(const void*);
typedef void *_aaxDriverThread(void*);


typedef struct
{
    const char *version;
    const char *driver;
    const char *vendor;
    char *renderer;

    _aaxDriverRingBufferCreate *get_ringbuffer;
    _aaxDriverRingBufferDestroy *destroy_ringbuffer;

    _aaxDriverDetect *detect;
    _aaxDriverNewHandle *new_handle;
    _aaxDriverGetDevices *get_devices;
    _aaxDriverGetInterfaces *get_interfaces;
 
    _aaxDriverGetName *name;
    _aaxDriverRender *render;
    _aaxDriverThread *thread;

    _aaxDriverConnect *connect;
    _aaxDriverDisconnect *disconnect;
    _aaxDriverSetup *setup;
    _aaxDriverCaptureCallback *capture;
    _aaxDriverPlaybackCallback *play;

    _aaxDriverPrepare3d *prepare3d;
    _aaxDriverPostProcess *postprocess;
    _aaxDriverPrepare *effects;
    _aaxDriverSetPosition *set_position;

    _aaxDriverGetSetSources *getset_sources;

    _aaxDriverState *state;
    _aaxDriverParam *param;
    _aaxDriverLog *log;

} _aaxDriverBackend;


/* ---  software device helper functions --- */


_aaxDriverPostProcess _aaxSoftwareDriverPostProcess;
_aaxDriverPrepare _aaxSoftwareDriverApplyEffects;
_aaxDriverPrepare3d _aaxSoftwareDriver3dPrepare;
_aaxDriverThread _aaxSoftwareMixerThread;
_aaxDriverGetSetSources _aaxSoftwareDriverGetSetSources;

void _aaxNoneDriverProcessFrame(void*);

void* _aaxSoftwareMixerThread(void*);
int _aaxSoftwareMixerThreadUpdate(void*, void*);
void _aaxSoftwareMixerPostProcess(const void *, const void *, void *, const void *, const void*, void*);
void _aaxSoftwareMixerApplyEffects(const void *, const void *, void *, const void *, char, char);


uint32_t getMSChannelMask(uint16_t);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_DRIVER_H */

