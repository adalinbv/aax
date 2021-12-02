/*
 * Copyright 2012-2021 by Erik Hofman.
 * Copyright 2012-2021 by Adalin B.V.
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

#define MAX_ID_STRLEN			128

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

   __F_PATCH_LEVEL,
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

#include "io.h"
#include "format.h"
#include "extension.h"
#include "protocol.h"

/* MIDI support */
float note2freq(uint8_t);

float cents2pitch(float, float);
float cents2modulation(float, float);

/* ID3 support */
void _aaxFormatDriverReadID3Header(pdmp3_handle*, struct _meta_t*);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* __STREAM_AUDIO_H */


