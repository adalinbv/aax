/*
 * Copyright 2012-2015 by Erik Hofman.
 * Copyright 2012-2015 by Adalin B.V.
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


#define _AAX_FILEDRVLOG(a)          _aaxStreamDriverLog(NULL, 0, 0, a);
_aaxDriverLog _aaxStreamDriverLog;


enum wavFormat
{
   UNSUPPORTED = 0,
   PCM_WAVE_FILE = 1,
   MSADPCM_WAVE_FILE = 2,
   FLOAT_WAVE_FILE = 3,
   ALAW_WAVE_FILE = 6,
   MULAW_WAVE_FILE = 7,
   IMA4_ADPCM_WAVE_FILE = 17,
   MP3_WAVE_FILE = 85,

   EXTENSIBLE_WAVE_FORMAT = 0xFFFE
};
enum aaxFormat getFormatFromWAVFormat(unsigned int, int);

/* I/O related: file, socket, etc */
typedef enum
{
   PROTOCOL_UnSUPPORTED = -1,
   PROTOCOL_FILE = 0,
   PROTOCOL_HTTP
} _protocol_t;

typedef int _open_fn(const char*, int, ...);
typedef int _close_fn(int);
typedef ssize_t _read_fn(int, void*, size_t);
typedef ssize_t _write_fn(int, const void*, size_t);
typedef off_t _seek_fn(int, off_t, int);
typedef int _stat_fn(int, struct stat*);

int _socket_open(const char*, int, ...);
int _socket_close(int);
ssize_t _socket_read(int, void*, size_t);
ssize_t _socket_write(int, const void*, size_t);
off_t _socket_seek(int, off_t, int);
int _socket_stat(int, struct stat*);

typedef struct
{
   _open_fn *open;
   _close_fn *close;
   _read_fn *read;
   _write_fn *write;
   _seek_fn *seek;
   _stat_fn *stat;

   _protocol_t protocol;
} _io_t;


#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* __STREAM_AUDIO_H */


