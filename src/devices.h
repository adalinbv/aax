/*
 * Copyright 2005-2013 by Erik Hofman.
 * Copyright 2009-2013 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_DEVICES_H
#define _AAX_DEVICES_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <base/buffers.h>

#include "driver.h"

typedef struct
{
   unsigned int no_nodes;
   unsigned int no_backends;
// unsigned int backend_pos;
   struct {
      char *devname;
      char *setup;
      void *hrtf;
      int no_speakers;
      int no_emitters;
      void *speaker[_AAX_MAX_SPEAKERS];
      float frequency;
      float interval;
      float update;
   } node[_AAX_MAX_SLAVES];
   struct {
      char *driver;
      void *output;
      void *input;
   } backend; // [_AAX_MAX_BACKENDS];
} _aaxConfig;


char is_bigendian();
_intBuffers *_aaxGetDriverBackends();
_aaxDriverBackend *_aaxGetDriverBackendLoopback(unsigned int *);
void *_aaxRemoveDriverBackends(_intBuffers **);

_aaxDriverBackend *_aaxGetDriverBackendDefault(_intBuffers *, unsigned int *);
_aaxDriverBackend *_aaxGetDriverBackendDefaultCapture(_intBuffers *, unsigned int *);
_aaxDriverBackend *_aaxGetDriverBackendByName(_intBuffers *, const char *, unsigned int *);
_aaxDriverBackend *_aaxGetDriverBackendByPos(_intBuffers *, unsigned int);
const char *_aaxGetDriverBackendName(const _aaxDriverBackend *);

long _aaxDriverBackendSetConfigSettings(_intBuffers *,char**, _aaxConfig *);
void _aaxDriverBackendReadConfigSettings(void *, char**, _aaxConfig *, const char *, int);
void _aaxDriverBackendClearConfigSettings(_aaxConfig *);
char _aaxGetDriverBackendExtensionSupport(const _aaxDriverBackend *, const char *);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_DEVICES_H */

