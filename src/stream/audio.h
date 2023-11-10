/*
 * SPDX-FileCopyrightText: Copyright © 2012-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2012-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef __STREAM_AUDIO_H
#define __STREAM_AUDIO_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/stat.h>

#include <base/dlsym.h>
#include <backends/driver.h>
#include <3rdparty/pdmp3.h>
#include "api.h"

#define MAX_ID_STRLEN			128
#define COMMENT_SIZE			1024

#ifdef WIN32
// TODO: Needs fixing # define WINXP
#endif

#define EVEN(n)                     (((n) & 0x1) ? ((n)+1) : (n))
#define STRCMP(a, b)                strncasecmp((a), (b), strlen(b))
#define _AAX_FILEDRVLOG(a)          _aaxStreamDriverLog(NULL, 0, 0, a);
_aaxDriverLog _aaxStreamDriverLog;

enum _aaxStreamParam
{
   __F_PROCESS = -3,            /* get */
   __F_NEED_MORE = -2,		/* need more data */
   __F_EOF = -1,
   __F_FMT = 0,
   __F_TRACKS,
   __F_FREQUENCY,
   __F_BITS_PER_SAMPLE,
   __F_BLOCK_SIZE,
   __F_BLOCK_SAMPLES,
   __F_NO_SAMPLES,
   __F_NO_BYTES,
   __F_BITRATE,
   __F_CHANNEL_MASK,

   __F_MIP_LEVEL,
   __F_NO_PATCHES,
   __F_LOOP_COUNT,
   __F_LOOP_START,
   __F_LOOP_END,
   __F_SAMPLED_RELEASE,
   __F_BASE_FREQUENCY,
   __F_LOW_FREQUENCY,
   __F_HIGH_FREQUENCY,
   __F_PITCH_FRACTION,
   __F_TREMOLO_RATE,
   __F_TREMOLO_DEPTH,
   __F_TREMOLO_SWEEP,
   __F_VIBRATO_RATE,
   __F_VIBRATO_DEPTH,
   __F_VIBRATO_SWEEP,

   __F_RATE,
   __F_PORT,
   __F_EXTENSION,
   __F_TIMEOUT,
   __F_FLAGS,
   __F_MODE,

   __F_NAME_CHANGED = 0x0400, /* set if the name changed since the last get */
   __F_IMAGE = 0x0800,          /* get info name strings */
   __F_ARTIST,
   __F_GENRE,
   __F_TITLE,
   __F_TRACKNO,
   __F_ALBUM,
   __F_DATE,
   __F_COPYRIGHT,
   __F_COMMENT,
   __F_COMPOSER,
   __F_ORIGINAL,
   __F_WEBSITE,

   __F_IS_STREAM = 0x1000,      /* set */
   __F_COPY_DATA,
   __F_POSITION,

   __F_ENVELOPE_LEVEL     = 0x2000,
   __F_ENVELOPE_LEVEL_MAX = 0x2010,
   __F_ENVELOPE_RATE       = 0x2010,
   __F_ENVELOPE_RATE_MAX   = 0x2020,
   __F_ENVELOPE_SUSTAIN,
   __F_FAST_RELEASE
};

#include "format.h"
#include "extension.h"

/* MIDI support */
float note2freq(uint8_t);

float cents2pitch(float, float);
float cents2modulation(float, float);

/* ID3 support */
int _aaxFormatDriverReadID3Header(pdmp3_handle*, struct _meta_t*);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* __STREAM_AUDIO_H */


