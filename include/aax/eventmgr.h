/*
 * Copyright 2013-2017 by Erik Hofman.
 * Copyright 2013-2017 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(TARGET_OS_MAC) && TARGET_OS_MAC
# pragma export on
#endif

#include <aax/aax.h>

enum aaxEventType
{
    AAX_EVENT_NONE = 0,
    AAX_EVENT_START,
    AAX_EVENT_STOP,
    AAX_EVENT_PROCESS,
    AAX_EVENT_LOOP,
    AAX_EVENT_BUFFER_AVAIL,
 
    AAX_EVENT_WITHIN_RANGE,
    AAX_EVENT_OUT_OF_RANGE,

    AAX_EVENT_MAX,

    AAX_EMITTER_EVENTS = 0x100,
    AAX_SENSOR_EVENTS = 0x200,
    AAX_FRAME_EVENTS = 0x400,
    AAX_BUFFER_EVENTS = 0x800,
    AAX_SPECIAL_EVENTS = 0x1000,

    AAX_SYSTEM_EVENTS = 0x80000000
};

typedef void* aaxEvent;
typedef void (*aaxEventCallbackFn)(void*);

/*
 * Event Manager Setup
 */
AAX_API int AAX_APIENTRY aaxEventManagerCreate(aaxConfig);
AAX_API int AAX_APIENTRY aaxEventManagerDestory(aaxConfig);
AAX_API int AAX_APIENTRY aaxEventManagerSetState(aaxConfig, enum aaxState);

/*
 * Emitter handling.
 * Emtters are always associated with an AudioFrame.
 *
 * The EventManager keeps track of the created emitters and it's associated
 * audio buffers. If an emitter is destroyed using the event manager it will
 * take the appropriate actions like deregistering and buffer destroying
 * and freeeing any other allocated memory.
 * The event manager also keeps a list of loaded files. If two or more emitters
 * request the same audio file only one copy will be kept in memory and the
 * data will remeain in memory until the last referencing emitter is destroyed.
 */
AAX_API aaxFrame AAX_APIENTRY aaxEventManagerCreateAudioFrame(aaxConfig);
AAX_API aaxEmitter AAX_APIENTRY aaxEventManagerRegisterEmitter(aaxFrame, const char*);
AAX_API int AAX_APIENTRY aaxEventManagerDestoryEmitter(aaxFrame, aaxEmitter);

/*
 * Event setup
 */
AAX_API aaxEvent AAX_APIENTRY aaxEventManagerRegisterEvent(aaxConfig, aaxEmitter, enum aaxEventType, aaxEventCallbackFn, void*);
AAX_API int AAX_APIENTRY aaxEventManagerDeregisterEvent(aaxConfig, aaxEvent);

/*
 * Single shot events
 */
AAX_API aaxEmitter AAX_APIENTRY aaxEventManagerPlayTone(aaxConfig, enum aaxWaveformType, float, float, float, float);
AAX_API aaxEmitter AAX_APIENTRY aaxEventManagerPlayFile(aaxConfig, const char*, float, float);


#if defined(TARGET_OS_MAC) && TARGET_OS_MAC
# pragma export off
#endif

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif

