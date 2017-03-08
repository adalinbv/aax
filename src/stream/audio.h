/*
 * Copyright 2012-2017 by Erik Hofman.
 * Copyright 2012-2017 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
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
   __F_PROCESS = -2,            /* get */
   __F_EOF = -1,
   __F_FMT = 0,
   __F_TRACKS,
   __F_FREQ,
   __F_BITS,
   __F_BLOCK,
   __F_BLOCK_SAMPLES,
   __F_SAMPLES,
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
   __F_POSITION
};


#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* __STREAM_AUDIO_H */


