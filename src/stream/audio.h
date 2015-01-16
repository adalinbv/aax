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

#define KSDATAFORMAT_SUBTYPE1           0x00000010
#define KSDATAFORMAT_SUBTYPE2           0x800000aa
#define KSDATAFORMAT_SUBTYPE3           0x00389b71


#define _AAX_FILEDRVLOG(a)          _aaxFileDriverLog(NULL, 0, 0, a);
_aaxDriverLog _aaxFileDriverLog;


enum wavFormat
{
   UNSUPPORTED = 0,
   PCM_WAVE_FILE = 1,
   MSADPCM_WAVE_FILE = 2,
   FLOAT_WAVE_FILE = 3,
   ALAW_WAVE_FILE = 6,
   MULAW_WAVE_FILE = 7,
   IMA4_ADPCM_WAVE_FILE = 17,

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

_protocol_t _url_split(const char*, char**, char**, char**, int*);

typedef int _open_fn(const char*, int, ...);
typedef int _close_fn(int);
typedef ssize_t _read_fn(int, void*, size_t);
typedef ssize_t _write_fn(int, const void*, size_t);
typedef off_t _seek_fn(int, off_t, int);
typedef int _stat_fn(int, void*);

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

/* HTTP */
int http_open(_io_t*, const char*, const char*, int*);
void http_close(_io_t*, int);
int http_send_request(_io_t*, int, const char*, const char*, const char*);
int http_get_response(_io_t*, int, char*, int);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* __STREAM_AUDIO_H */


