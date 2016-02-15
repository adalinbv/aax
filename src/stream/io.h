/*
 * Copyright 2012-2016 by Erik Hofman.
 * Copyright 2012-2016 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_IO_H
#define _AAX_IO_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "protocol.h"

#if defined(__cplusplus)
}  /* extern "C" */
#endif

struct _io_st;
struct stat;

/* I/O related: file, socket, etc */
typedef int _open_fn(struct _io_st*, const char*);
typedef int _close_fn(struct _io_st*);
typedef ssize_t _read_fn(struct _io_st*, void*, size_t);
typedef ssize_t _write_fn(struct _io_st*, const void*, size_t);
typedef off_t _seek_fn(struct _io_st*, off_t, int);
typedef int _stat_fn(struct _io_st*, struct stat*);
typedef int _set_fn(struct _io_st*, int, int);

enum {
   _IO_FILE_FLAGS = 1,
   _IO_FILE_MODE = 2,
   _IO_PARAM_MAX,

   _IO_SOCKET_RATE = 0,
   _IO_SOCKET_PORT = 1,
   _IO_SOCKET_TIMEOUT = 2
};

struct _io_st
{
   _open_fn *open;
   _close_fn *close;
   _read_fn *read;
   _write_fn *write;
   _seek_fn *seek;
   _stat_fn *stat;
   _set_fn *set;

   _protocol_t protocol;
   int param[_IO_PARAM_MAX];
   int fd;

};
typedef struct _io_st _io_t;

_io_t* _io_create(_protocol_t);
void* _io_free(_io_t*);

/* file */
int _file_open(_io_t*, const char*);
int _file_close(_io_t*);
ssize_t _file_read(_io_t*, void*, size_t);
ssize_t _file_write(_io_t*, const void*, size_t);
off_t _file_seek(_io_t*, off_t, int);
int _file_stat(_io_t*, struct stat*);
int _file_set(_io_t*, int, int);

/* socket */
int _socket_open(_io_t*, const char*);
int _socket_close(_io_t*);
ssize_t _socket_read(_io_t*, void*, size_t);
ssize_t _socket_write(_io_t*, const void*, size_t);
off_t _socket_seek(_io_t*, off_t, int);
int _socket_stat(_io_t*, struct stat*);
int _socket_set(_io_t*, int, int);


#endif /* !_AAX_IO_H */

