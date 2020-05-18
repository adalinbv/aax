/*
 * Copyright 2012-2020 by Erik Hofman.
 * Copyright 2012-2020 by Adalin B.V.
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

#ifndef _AAX_IO_H
#define _AAX_IO_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#else
#define POLLIN	0
struct pollfd {
   int fd;
   short events;
   short revents;
};
#endif

#include "protocol.h"

typedef enum {
   _IO_FILE_FLAGS = 1,
   _IO_FILE_MODE,

   _IO_SOCKET_SIZE = 0,
   _IO_SOCKET_RATE,
   _IO_SOCKET_PORT,
   _IO_SOCKET_TIMEOUT,

   _IO_PARAM_MAX
} _io_type_t;

struct _io_st;
struct stat;

/* I/O related: file, socket, etc */
typedef int _io_open_fn(struct _io_st*, const char*);
typedef int _io_close_fn(struct _io_st*);
typedef ssize_t _io_read_fn(struct _io_st*, void*, size_t);
typedef ssize_t _io_write_fn(struct _io_st*, const void*, size_t);
typedef int _io_set_param_fn(struct _io_st*, enum _aaxStreamParam, ssize_t);
typedef ssize_t _io_get_param_fn(struct _io_st*, enum _aaxStreamParam);
typedef void _io_wait_fn(struct _io_st*, float);

struct _io_st
{
   _io_open_fn *open;
   _io_close_fn *close;
   _io_read_fn *read;
   _io_write_fn *write;
   _io_get_param_fn *get_param;
   _io_set_param_fn *set_param;
   _io_wait_fn *wait;

   unsigned error_ctr;
   unsigned error_max;

   int protocol;
   int param[_IO_PARAM_MAX];
   struct pollfd fds;

   void *timer;
   float update_dt;

   void *ssl;
   void *ssl_ctx;
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
void _file_wait(_io_t*, float);

/* socket */
int _socket_open(_io_t*, const char*);
int _socket_close(_io_t*);
ssize_t _socket_read(_io_t*, void*, size_t);
ssize_t _socket_write(_io_t*, const void*, size_t);
int _socket_set(_io_t*, int, ssize_t);
ssize_t _socket_get(_io_t*, int);
void _socket_wait(_io_t*, float);

/* https support */
typedef int (*OPENSSL_init_ssl_proc)(uint64_t, const void*);
typedef void* (*SSL_new_proc)(void*);
typedef int (*SSL_free_proc)(void*);
typedef void* (*SSL_CTX_new_proc)(const void*);
typedef void (*SSL_CTX_free_proc)(void*);
typedef int (*SSL_set_fd_proc)(void*, int);
typedef int (*SSL_connect_proc)(void*);
typedef int (*SSL_shutdown_proc)(void*);
typedef int (*SSL_read_proc)(void*, void*, int);
typedef int (*SSL_write_proc)(void*, const void*, int);
typedef const char* (*SSL_CIPHER_get_name_proc)(const void*);
typedef const void* (*SSL_get_current_cipher_proc)(const void*);
typedef int (*SSL_get_error_proc)(const void*, int);
typedef void* (*TLS_client_method_proc)(void);


#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_IO_H */

