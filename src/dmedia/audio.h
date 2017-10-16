/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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

#ifndef _DMEDIA_AUDIO_H
#define _DMEDIA_AUDIO_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "base/types.h"

/* SGI dmedia defines */
#define AL_INVALID_VALUE_SGIS			0xfffffffe

#define AL_SAMPFMT_TWOSCOMP			0x00000001
#define AL_SAMPFMT_FLOAT			0x00000020
#define AL_SAMPFMT_DOUBLE			0x00000040

#define AL_VERSION_SGIS				0x1000002b
#define AL_CHANNELS_SGIS			0x10000004
#define AL_GAIN_SGIS				0x10000019

#define AL_DEFAULT_OUTPUT			0x0000000a
#define AL_CHANNEL_MODE				0x0000000e
#define AL_SAMPLE_8				0x00000001
#define AL_SAMPLE_16				0x00000002
#define AL_SAMPLE_24				0x00000004
#define AL_SYSTEM				0x0000000B
#define AL_OUTPUT_RATE				0x00000004
#define AL_INPUT_RATE				0x00000003
#define AL_INTERFACE				0x10000002
#define AL_INTERFACE_TYPE			0x20000007
#define AL_DEVICE_TYPE				0x20000002
#define AL_RATE					0x1000000d
#define AL_MCLK_TYPE				0x20000009
#define AL_MASTER_CLOCK				0x10000011

#define AL_STRING_VAL				0x00000004
#define AL_NAME					0x10000013

typedef	void * ALport;
typedef void * ALconfig;

typedef uint64_t stamp_t;

/* From: 'man alSetParams' */
typedef union {
   int i;            /* 32-bit integer values */
   long long ll;     /* 64-bit integer and fixed-point values */
   void* ptr;        /* pointer values */
} ALvalue;

typedef struct {
   int     param;    /* parameter */
   ALvalue value;    /* value */
   short   sizeIn;   /* size in -- 1st dimension */
   short   size2In;  /* size out -- 2nd dimension */
   short   sizeOut;  /* size out */
   short   size2Out; /* size out -- 2nd dimension */
} ALpv;

/* From: 'man alGetParamInfo' */
typedef struct {
   int resource;     /* the resource */
   int param;        /* the parameter */
   int valueType;    /* type of the whole value (scalar,vector,set...) */
   int maxElems;     /* maximum number of elements */
   int maxElems2;    /* maximum number of elements (2nd dimension) */
   int elementType;  /* type of each element (enum, fixed, resource ...) */
   char name[32];    /* name of the parameter */
   ALvalue initial;  /* initial value */
   ALvalue min;      /* maximum value (range parameters only) */
   ALvalue max;      /* maximum value (range parameters only) */
   ALvalue minDelta; /* maximum delta between values (range parameters only) */
   ALvalue maxDelta; /* maximum delta between values (range parameters only) */
   int specialVals;  /* special values not between min & max (range parms only) */
   int operations;   /* supported operations */
} ALparamInfo;

typedef int (*alSetParams_proc)(int, ALpv *, int);
typedef int (*alGetResourceByName_proc)(int, char *, int);
typedef int (*alSetConfig_proc)(ALport, ALconfig);
typedef int (*alSetDevice_proc)(ALconfig, int);
typedef ALport (*alOpenPort_proc)(char *, char *, ALconfig);
typedef int (*alSetWidth_proc)(ALconfig, int);
typedef int (*alSetSampFmt_proc)(ALconfig, int);
typedef int (*alSetQueueSize_proc)(ALconfig, const int);
typedef int (*alSetChannels_proc)(ALconfig, int);
typedef ALconfig (*alNewConfig_proc)(void);
typedef int (*alFreeConfig_proc)(ALconfig);
typedef int (*alClosePort_proc)(ALport);
typedef int (*alGetParams_proc)(int, ALpv *, int);
typedef int (*alGetFrameTime_proc)(const ALport, stamp_t *, stamp_t *);
typedef int (*alGetFrameNumber_proc)(const ALport, stamp_t *);
typedef int (*alDiscardFrames_proc)(ALport, int);
typedef int (*alGetParamInfo_proc)(int, int, ALparamInfo *);
typedef int (*alWriteFrames_proc)(const ALport, void *, const int);
typedef int (*alReadFrames_proc)(const ALport, void *, const int);
typedef char *(*alGetErrorString_proc)(int);


#define DM_FAILURE         			-1

typedef const int *(*dmDVIAudioDecode_proc)(void *, unsigned char *, short *, int);
typedef int (*dmDVIAudioDecoderCreate_proc)(void **);
typedef void *(*dmG711MulawDecode_proc)(unsigned char *, short *, int);
typedef void *(*dmG711AlawDecode_proc)(unsigned char *, short *, int);
typedef char *(*dmGetError_proc)(int *, char *);

dmDVIAudioDecode_proc dmDVIAudioDecode;
dmDVIAudioDecoderCreate_proc dmDVIAudioDecoderCreate;
dmG711MulawDecode_proc dmG711MulawDecode;
dmG711AlawDecode_proc dmG711AlawDecode;
dmGetError_proc dmGetError;

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* _DMEDIA_AUDIO_H */

