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

#ifndef _AAX_PROTOCOL_H
#define _AAX_PROTOCOL_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "audio.h"
#include "io.h"

typedef enum
{
   PROTOCOL_UNSUPPORTED = -1,
   PROTOCOL_DIRECT = 0,
   PROTOCOL_HTTP
} _protocol_t;

struct _io_st;
struct _prot_st;

typedef size_t _prot_connect_fn(struct _prot_st*, struct _io_st*, const char*, const char*, const char*);
typedef int _prot_process_fn(struct _prot_st*, uint8_t*, size_t, size_t);
typedef int _prot_set_fn(struct _prot_st*, enum _aaxStreamParam, ssize_t);
typedef int _prot_get_fn(struct _prot_st*, enum _aaxStreamParam);
typedef char* _prot_name_fn(struct _prot_st*, enum _aaxStreamParam);

struct _prot_st
{
   _prot_connect_fn *connect;
   _prot_process_fn *process;
   _prot_set_fn *set;
   _prot_get_fn *get;
   _prot_name_fn *name;
   
   int protocol;
   size_t no_bytes;
   size_t meta_interval;
   size_t meta_pos;

   char *path;
   char *station;
   char *description;
   char *genre;
   char *website;
   char *content_type;
   char metadata_changed;
      // artist[0] = AAX_TRUE if changed since the last get
   char artist[MAX_ID_STRLEN+1];
      // title[0] = AAX_TRUE if changed since the last get
   char title[MAX_ID_STRLEN+1];
};
typedef struct _prot_st _prot_t;

_protocol_t _url_split(char*, char**, char**, char**, char**, int*);
_prot_t* _prot_create(_protocol_t);
void* _prot_free(_prot_t*);

/* http protocol */
size_t _http_connect(_prot_t*, struct _io_st*, const char*, const char*, const char*);
int _http_process(_prot_t*, uint8_t*, size_t, size_t);
int _http_set(_prot_t*, enum _aaxStreamParam, ssize_t);
int _http_get(_prot_t*, enum _aaxStreamParam);
char* _http_name(_prot_t*, enum _aaxStreamParam);

/* direct protocol */
size_t _direct_connect(_prot_t*, struct _io_st*, const char*, const char*, const char*);
int _direct_process(_prot_t*, uint8_t*, size_t, size_t);
int _direct_set(_prot_t*, enum _aaxStreamParam, ssize_t);
int _direct_get(_prot_t*, enum _aaxStreamParam);
char* _direct_name(_prot_t*, enum _aaxStreamParam);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_PROTOCOL_H */

