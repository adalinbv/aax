/*
 * SPDX-FileCopyrightText: Copyright © 2012-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2012-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef _AAX_PROTOCOL_H
#define _AAX_PROTOCOL_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <base/databuffer.h>

#include <api.h>
#include "audio.h"

typedef enum
{
   PROTOCOL_UNSUPPORTED = -1,
   PROTOCOL_DIRECT = 0,
   PROTOCOL_HTTP,
   PROTOCOL_HTTPS
} _protocol_t;

struct _io_st;
struct _prot_st;

typedef ssize_t _prot_connect_fn(struct _prot_st*, _data_t*, struct _io_st*, char**, const char*, const char*);
typedef void _prot_disconnect_fn(struct _prot_st*);
typedef int _prot_process_fn(struct _prot_st*, _data_t*);
typedef int _prot_set_param_fn(struct _prot_st*, enum _aaxStreamParam, ssize_t);
typedef int _prot_get_param_fn(struct _prot_st*, enum _aaxStreamParam);
typedef char* _prot_name_fn(struct _prot_st*, enum _aaxStreamParam);

struct _prot_st
{
   _prot_connect_fn *connect;
   _prot_disconnect_fn *disconnect;
   _prot_process_fn *process;
   _prot_set_param_fn *set_param;
   _prot_get_param_fn *get_param;
   _prot_name_fn *name;
   
   int protocol;
   size_t no_bytes;
   size_t meta_interval;
   size_t meta_offset;

   bool metadata_changed;
   struct _meta_t meta;
};
typedef struct _prot_st _prot_t;

char* _url_get_param(char*, const char*, size_t*);
_protocol_t _url_split(char*, char**, char**, char**, char**, int*);
_prot_t* _prot_create(_protocol_t);
void* _prot_free(_prot_t*);

/* http protocol */
ssize_t _http_connect(_prot_t*, _data_t*, struct _io_st*, char**, const char*, const char*);
void _http_disconnect(_prot_t*);
int _http_process(struct _prot_st*, _data_t*);
int _http_set(_prot_t*, enum _aaxStreamParam, ssize_t);
int _http_get(_prot_t*, enum _aaxStreamParam);
char* _http_name(_prot_t*, enum _aaxStreamParam);

# define SSL_get_cipher(s)	SSL_CIPHER_get_name(SSL_get_current_cipher(s))

/* direct protocol */
ssize_t _direct_connect(_prot_t*, _data_t*, struct _io_st*, char**, const char*, const char*);
void _direct_disconnect(_prot_t*);
int _direct_process(struct _prot_st*, _data_t*);
int _direct_set(_prot_t*, enum _aaxStreamParam, ssize_t);
int _direct_get(_prot_t*, enum _aaxStreamParam);
char* _direct_name(_prot_t*, enum _aaxStreamParam);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_PROTOCOL_H */

