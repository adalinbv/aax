/*
 * Copyright 2005-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_DEVICES_H
#define _AAX_DEVICES_H 1

#include <base/buffers.h>

#include "driver.h"

typedef struct
{
   unsigned int no_nodes;
   unsigned int no_backends;
   unsigned int backend_pos;
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
   } backend[_AAX_MAX_BACKENDS];
} _aaxConfig;


extern _intBuffers *_aaxDriverBackends;
extern _intBuffers _aaxIntDriverBackends;


char is_bigendian();
_intBuffers *_aaxGetDriverBackends();
_aaxDriverBackend *_aaxGetDriverBackendLoopback();
void *_aaxRemoveDriverBackends(_intBuffers **);

_aaxDriverBackend *_aaxGetDriverBackendDefault(const _intBuffers *);
_aaxDriverBackend *_aaxGetDriverBackendDefaultCapture(const _intBuffers *);
_aaxDriverBackend *_aaxGetDriverBackendByName(const _intBuffers *, const char *);
_aaxDriverBackend *_aaxGetDriverBackendByPos(const _intBuffers *, unsigned int);
const char *_aaxGetDriverBackendName(const _aaxDriverBackend *);
long _aaxDriverBackendSetConfigSettings(const _intBuffers *, _aaxConfig *);
void _aaxDriverBackendClearConfigSettings(_aaxConfig *);
char _aaxGetDriverBackendExtensionSupport(const _aaxDriverBackend *, const char *);

#endif /* !_AAX_DEVICES_H */

