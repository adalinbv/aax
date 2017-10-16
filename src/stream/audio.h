/*
 * Copyright 2012-2017 by Erik Hofman.
 * Copyright 2012-2017 by Adalin B.V.
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
#include <driver.h>

#define MAX_ID_STRLEN			64

#ifdef WIN32
// TODO: Needs fixing # define WINXP
#endif

#define SPEAKER_FRONT_LEFT              0x1
#define SPEAKER_FRONT_RIGHT             0x2
#define SPEAKER_FRONT_CENTER            0x4
#define SPEAKER_LOW_FREQUENCY           0x8
#define SPEAKER_BACK_LEFT               0x10
#define SPEAKER_BACK_RIGHT              0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER    0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER   0x80
#define SPEAKER_BACK_CENTER             0x100
#define SPEAKER_SIDE_LEFT               0x200
#define SPEAKER_SIDE_RIGHT              0x400
#define SPEAKER_TOP_CENTER              0x800
#define SPEAKER_TOP_FRONT_LEFT          0x1000
#define SPEAKER_TOP_FRONT_CENTER        0x2000
#define SPEAKER_TOP_FRONT_RIGHT         0x4000
#define SPEAKER_TOP_BACK_LEFT           0x8000
#define SPEAKER_TOP_BACK_CENTER         0x10000
#define SPEAKER_TOP_BACK_RIGHT          0x20000

#define KSDATAFORMAT_SUBTYPE1           0x00100000
#define KSDATAFORMAT_SUBTYPE2           0xaa000080
#define KSDATAFORMAT_SUBTYPE3           0x719b3800

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
   __F_POSITION
};

#include "io.h"
#include "format.h"
#include "extension.h"
#include "protocol.h"

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* __STREAM_AUDIO_H */


