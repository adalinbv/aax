/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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
      int bitrate;
      char *speaker[_AAX_MAX_SPEAKERS];
      float frequency;
      float interval;
      float update;
   } node[_AAX_MAX_OUTPUTS];
   struct {
      char *driver;
      char *output;
      char *input;
   } backend; // [_AAX_MAX_BACKENDS];
} _aaxConfig;


_intBuffers *_aaxGetDriverBackends();
_aaxDriverBackend *_aaxGetDriverBackendLoopback(unsigned int *);
void *_aaxRemoveDriverBackends(_intBuffers **);

_aaxDriverBackend *_aaxGetDriverBackendDefault(_intBuffers *, unsigned int *);
_aaxDriverBackend *_aaxGetDriverBackendDefaultCapture(_intBuffers *, unsigned int *);
_aaxDriverBackend *_aaxGetDriverBackendByName(_intBuffers *, const char *, unsigned int *);
_aaxDriverBackend *_aaxGetDriverBackendByPos(_intBuffers *, unsigned int);
const char *_aaxGetDriverBackendName(const _aaxDriverBackend *);

long _aaxDriverBackendSetConfigSettings(_intBuffers *,char**, _aaxConfig *);
void _aaxDriverBackendReadConfigSettings(const xmlId *, char**, _aaxConfig *, char **, int);
void _aaxDriverBackendClearConfigSettings(_aaxConfig *);
char _aaxGetDriverBackendExtensionSupport(const _aaxDriverBackend *, const char *);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_DEVICES_H */

