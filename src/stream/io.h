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

#ifndef _AAX_IO_H
#define _AAX_IO_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "protocol.h"

typedef enum {
   _IO_FILE_FLAGS = 1,
   _IO_FILE_MODE = 2,
   _IO_PARAM_MAX,

   _IO_SOCKET_RATE = 0,
   _IO_SOCKET_PORT = 1,
   _IO_SOCKET_TIMEOUT = 2
} _io_type_t;

struct _io_st;
struct stat;

/* I/O related: file, socket, etc */
typedef int _io_open_fn(struct _io_st*, const char*);
typedef int _io_close_fn(struct _io_st*);
typedef ssize_t _io_read_fn(struct _io_st*, void*, size_t);
typedef ssize_t _io_write_fn(struct _io_st*, const void*, size_t);
typedef int _io_set_fn(struct _io_st*, enum _aaxStreamParam, ssize_t);
typedef ssize_t _io_get_fn(struct _io_st*, enum _aaxStreamParam);

struct _io_st
{
   _io_open_fn *open;
   _io_close_fn *close;
   _io_read_fn *read;
   _io_write_fn *write;
   _io_get_fn *get;
   _io_set_fn *set;

   int protocol;
   int param[_IO_PARAM_MAX];
   int fd;

};
typedef struct _io_st _io_t;

_io_t* _io_create(int);
void* _io_free(_io_t*);

/* file */
int _file_open(_io_t*, const char*);
int _file_close(_io_t*);
ssize_t _file_read(_io_t*, void*, size_t);
ssize_t _file_write(_io_t*, const void*, size_t);
int _file_set(_io_t*, int, ssize_t);
ssize_t _file_get(_io_t*, int);

/* socket */
int _socket_open(_io_t*, const char*);
int _socket_close(_io_t*);
ssize_t _socket_read(_io_t*, void*, size_t);
ssize_t _socket_write(_io_t*, const void*, size_t);
int _socket_set(_io_t*, int, ssize_t);
ssize_t _socket_get(_io_t*, int);


#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_IO_H */

