/*
 * SPDX-FileCopyrightText: Copyright © 2018-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2018-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef _SDL_AUDIO_H
#define _SDL_AUDIO_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "base/types.h"

// http://wiki.libsdl.org/CategoryAudio

#define SDL_INIT_AUDIO 0x10u

#define SDL_AUDIO_MASK_BITSIZE	(0xFF)
#define SDL_AUDIO_MASK_DATATYPE (1<<8)
#define SDL_AUDIO_MASK_ENDIAN (1<<12)
#define SDL_AUDIO_MASK_SIGNED (1<<15)
#define SDL_AUDIO_BITSIZE(x) (x & SDL_AUDIO_MASK_BITSIZE)
#define SDL_AUDIO_ISFLOAT(x) (x & SDL_AUDIO_MASK_DATATYPE)
#define SDL_AUDIO_ISBIGENDIAN(x) (x & SDL_AUDIO_MASK_ENDIAN)
#define SDL_AUDIO_ISSIGNED(x) (x & SDL_AUDIO_MASK_SIGNED)
#define SDL_AUDIO_ISINT(x) (!SDL_AUDIO_ISFLOAT(x))
#define SDL_AUDIO_ISLITTLEENDIAN(x) (!SDL_AUDIO_ISBIGENDIAN(x))
#define SDL_AUDIO_ISUNSIGNED(x) (!SDL_AUDIO_ISSIGNED(x))

#define SDL_AUDIO_ALLOW_FREQUENCY_CHANGE 0x1
#define SDL_AUDIO_ALLOW_FORMAT_CHANGE 0x2
#define SDL_AUDIO_ALLOW_CHANNELS_CHANGE	0x4
#define SDL_AUDIO_ALLOW_ANY_CHANGE (SDL_AUDIO_ALLOW_FREQUENCY_CHANGE|SDL_AUDIO_ALLOW_FORMAT_CHANGE|SDL_AUDIO_ALLOW_CHANNELS_CHANGE)

#define AUDIO_S16LSB	0x8010
#define AUDIO_S16MSB	0x9010

typedef struct {
   int freq;
   uint16_t format;
   uint8_t channels;
   uint8_t silence;
   uint16_t samples;
   uint16_t padding;
   uint32_t size;
   void (*callback)(void*, uint8_t*, int);
   void *userdata;

} SDL_AudioSpec;

typedef struct {
   uint8_t major;
   uint8_t minor;
   uint8_t patch;

} SDL_version;

typedef void (*SDL_GetVersion_proc)(SDL_version*);

typedef int (*SDL_GetNumAudioDrivers_proc)(void);
typedef const char* (*SDL_GetAudioDriver_proc)(int);

typedef const char* (*SDL_GetError_proc)(void);
typedef void (*SDL_ClearError_proc)(void);

typedef int (*SDL_GetNumAudioDevices_proc)(int);
typedef const char* (*SDL_GetAudioDeviceName_proc)(int, int);

typedef int (*SDL_InitSubSystem_proc)(uint32_t);
typedef void (*SDL_QuitSubSystem_proc)(uint32_t);
typedef int (*SDL_AudioInit_proc)(const char*);
typedef void (*SDL_AudioQuit_proc)(void);

typedef uint32_t (*SDL_OpenAudioDevice_proc)(const char*, int, const SDL_AudioSpec*, SDL_AudioSpec*, int);
typedef void (*SDL_CloseAudioDevice_proc)(uint32_t);
typedef void (*SDL_PauseAudioDevice_proc)(uint32_t, int);

typedef int (*SDL_QueueAudio_proc)(uint32_t, const void*, uint32_t);
typedef int (*SDL_DequeueAudio_proc)(uint32_t, void*, uint32_t);
typedef uint32_t (*SDL_GetQueuedAudioSize_proc)(uint32_t);
typedef void (*SDL_ClearQueuedAudio_proc)(uint32_t);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* _SDL_AUDIO_H */

