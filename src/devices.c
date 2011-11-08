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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <strings.h>	/* strncasecmp */
#include <time.h>	/* time */

#include <xml.h>

#include <api.h>
#include <arch.h>
#include <devices.h>
#include <ringbuffer.h>
#include <base/logging.h>
#include <software/device.h>
#include <dmedia/device.h>
#include <alsasoft/device.h>
#include <alsa/device.h>
#include <oss/device.h>

char is_bigendian()
{
   static char __big_endian = 0;
   static char detect = 0;
   if (!detect)
   {
      unsigned int _t = 1;
      __big_endian = (*(char *)&_t == 0);
      detect = 1;
   }
   return __big_endian;
}

_intBuffers *
_aaxGetDriverBackends()
{
   _intBuffers *backends = NULL;
   unsigned int r;

   _AAX_LOG(LOG_WARNING, __FUNCTION__);

   r = _intBufCreate(&backends, _AAX_BACKEND);
   if (r != UINT_MAX)
   {
      _intBuffers *dbe = &_aaxIntDriverBackends;
      unsigned int i, num;

      num = _intBufGetNumNoLock(dbe, _AAX_BACKEND);
      for (i=0; i<num; i++)
      {
         _intBufferData *dptr;
         _aaxDriverBackend *be;

         dptr = _intBufGetNoLock(dbe, _AAX_BACKEND, i);
         assert (dptr);

         be = _intBufGetDataPtr(dptr);
         assert(be->detect);
#if 0
         printf("Backend #%i: '%s'\n", i, be->driver);
#endif
         if (be->detect())
         {
            unsigned int r;

            r = _intBufAddReference(backends, _AAX_BACKEND, dbe, i);
            if (r == UINT_MAX) break;
         }
      }
   }

   return backends;
}

void *
_aaxRemoveDriverBackends(_intBuffers **be)
{
   _intBufErase(be, _AAX_BACKEND, 0, 0);
   return 0;
}

_aaxDriverBackend *
_aaxGetDriverBackendByName(const _intBuffers *bs, const char *name)
{
   _aaxDriverBackend *be = 0;

   assert(bs != 0);

   if (name)
   {
      _aaxDriverBackend *b;
      unsigned int i, num;

      num = _intBufGetNumNoLock(bs, _AAX_BACKEND);
      for (i=0; i<num; i++)
      {
         _intBufferData *dptr;
         int res;

         dptr = _intBufGetNoLock(bs, _AAX_BACKEND, i);
         b = _intBufGetDataPtr(dptr);

         {
            res = strcasecmp(b->driver, name);
            if (!res)
            {
               be = (_aaxDriverBackend *)b;
               break;
            }
         }
      }

      if (i == num) be = 0;
   }
   else
      be = _aaxGetDriverBackendDefault(bs);

   return be;
}

_aaxDriverBackend *
_aaxGetDriverBackendDefault(const _intBuffers *bs)
{
   _aaxDriverBackend *be = 0;
   int i;

   assert (bs != 0);

   i = _intBufGetNumNoLock(bs, _AAX_BACKEND);
   do
   {
      _intBufferData *dptr;

      dptr = _intBufGetNoLock(bs, _AAX_BACKEND, --i);
      be = _intBufGetDataPtr(dptr);
      if (be->detect() && be->support_playback(NULL)) break;
   }
   while (i);

   return be;
}

_aaxDriverBackend *
_aaxGetDriverBackendLoopback()
{
   const _intBuffers *dbe = &_aaxIntDriverBackends;
   const char *name = "AeonWave Loopback";
   _aaxDriverBackend *be = 0;
   unsigned int i, num;

   num = _intBufGetNumNoLock(dbe, _AAX_BACKEND);
   for (i=0; i<num; i++)
   {
      _aaxDriverBackend *found_be;
      _intBufferData *dptr;

      dptr = _intBufGetNoLock(dbe, _AAX_BACKEND, i);
      found_be = _intBufGetDataPtr(dptr);

      if (!strcasecmp(found_be->driver, name))
      {
         be = found_be;
         break;
      }
   }

   return be;
}

_aaxDriverBackend *
_aaxGetDriverBackendDefaultCapture(const _intBuffers *bs)
{
   _aaxDriverBackend *be = 0;
   int i;

   assert (bs != 0);

   i = _intBufGetNumNoLock(bs, _AAX_BACKEND);
   do
   {
      _intBufferData *dptr;

      dptr = _intBufGetNoLock(bs, _AAX_BACKEND, --i);
      be = _intBufGetDataPtr(dptr);
      if (be->detect() && be->support_recording(NULL)) break;
   }
   while (i);

   return be;
}

_aaxDriverBackend *
_aaxGetDriverBackendByPos(const _intBuffers *bs, unsigned int pos)
{
   _aaxDriverBackend *be = 0;
   int num;

   assert(bs != 0);

   num = _intBufGetNumNoLock(bs, _AAX_BACKEND);
   if (num > 0 && pos < num)
   {
      _intBufferData *dptr;
      dptr = _intBufGetNoLock(bs, _AAX_BACKEND, num-pos-1);
      be = _intBufGetDataPtr(dptr);
   }
   return be;
}

const char *
_aaxGetDriverBackendName(const _aaxDriverBackend *be)
{
   return be->driver;
}

long
_aaxDriverBackendSetConfigSettings(const _intBuffers *bs, _aaxConfig *config)
{
   _intBuffers *dbe = (_intBuffers *)&_aaxIntDriverBackends;
   long rv = time(NULL);
   _aaxDriverBackend *be;
   unsigned int i, num;

   memset(config, 0, sizeof(_aaxConfig));
   if (dbe)
   {
      num = _intBufGetNumNoLock(dbe, _AAX_BACKEND);
      config->no_backends = num;
      for (i=0; i<num; i++)
      {
         _intBufferData *dptr;
         dptr = _intBufGetNoLock(dbe, _AAX_BACKEND, i);
         be = _intBufGetDataPtr(dptr);
         config->backend[i].driver = _aax_strdup(be->driver);
         config->backend[i].input = 0;
         config->backend[i].output = 0;
      }

      be = _aaxGetDriverBackendDefault(bs);
      config->no_nodes = 1;
      config->node[0].devname = _aax_strdup(be->driver);
      config->node[0].setup = _aax_strdup("stereo");
      config->node[0].frequency = (float)be->rate;
      config->node[0].interval = 66;
      config->node[0].update = 20;
      config->node[0].hrtf = 0;
      config->node[0].no_speakers = 2;
   }
   return rv; 
}

void
_aaxDriverBackendClearConfigSettings(_aaxConfig *config)
{
   unsigned int i;
   for (i=0; i<config->no_nodes; i++)
   {
      int q;
      free(config->node[i].setup);
      free(config->node[i].devname);
      xmlFree(config->node[i].hrtf);
      for (q=0; q<config->node[i].no_speakers; q++) {
         xmlFree(config->node[i].speaker[q]);
      }
   }

   for (i=0; i<config->no_backends; i++)
   {
      free(config->backend[i].driver);
      xmlFree(config->backend[i].input);
      xmlFree(config->backend[i].output);
   }

   free(config);
}

/* -------------------------------------------------------------------------- */

/**
 * backends
 * _AAX_MAX_BACKENDS is defines in driver.h
 */
_intBufferData _aaxBackends[_AAX_MAX_BACKENDS] =
{
   {0, 1, (void *)&_aaxNoneDriverBackend},
   {0, 1, (void *)&_aaxSoftwareDriverBackend},
   {0, 1, (void *)&_aaxLoopbackDriverBackend},
   {0, 1, (void *)&_aaxOSSDriverBackend},
   {0, 1, (void *)&_aaxALSASoftDriverBackend},
// {0, 1, (void *)&_aaxALSADriverBackend},
   {0, 1, (void *)&_aaxDMediaDriverBackend}
};

void *_aaxBackendsPtr[_AAX_MAX_BACKENDS] =
{ 
   (void *)&_aaxBackends[0],
   (void *)&_aaxBackends[1],
   (void *)&_aaxBackends[2],
   (void *)&_aaxBackends[3],
   (void *)&_aaxBackends[4],
   (void *)&_aaxBackends[5]
// (void *)&_aaxBackends[6]
};

_intBuffers _aaxIntDriverBackends =
{
   0,
   _AAX_BACKEND,
   _AAX_MAX_BACKENDS,
   _AAX_MAX_BACKENDS,
   _AAX_MAX_BACKENDS,
   (void *)&_aaxBackendsPtr
};

