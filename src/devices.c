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
#include <strings.h>	/* strncasecmp, strstr */
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
_aaxDriverBackendSetConfigSettings(const _intBuffers *bs, char** devname, _aaxConfig *config)
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
         if (!strcasecmp(devname[0], be->driver))
         {
            config->backend.driver = _aax_strdup(be->driver);
            config->backend.input = 0;
            config->backend.output = 0;
         }
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
_aaxDriverBackendReadConfigSettings(void *xid, char **devname, _aaxConfig *config, const char *path)
{
   void *xcid = xmlNodeGet(xid, "/configuration");

   if (xcid != NULL && config != NULL)
   {
      unsigned int n, num;
      void *xoid;
      float v;

      v = xmlNodeGetDouble(xcid, "version");
      if ((v < AAX_MIN_CONFIG_VERSION) || (v > AAX_MAX_CONFIG_VERSION))
      {
         xmlFree(xcid);
         fprintf(stderr, "AAX: incompattible configuration file version, skipping.\n");
         return;
      }

      xoid = xmlMarkId(xcid);
      num = xmlNodeGetNum(xoid, "output");      /* global output sections */
      config->no_nodes = num;
      for (n=0; n<num; n++)
      {
         unsigned int be, be_num;
         char *dev;

         xmlNodeGetPos(xcid, xoid, "output", n);

         if (n < _AAX_MAX_SLAVES)
         {
            unsigned int i, q;
            char *setup;
            void *xbid;
            float f;

            dev = xmlNodeGetString(xoid, "device");
            if (dev)
            {
               free(config->node[n].devname);
               config->node[n].devname = strdup(dev);
               xmlFree(dev);
            }

            setup = getenv("AAX_TUBE_COMPRESSOR");
            if (setup) {
               q = _oal_getbool(setup);
            } else {
               q = xmlNodeGetBool(xoid, "tube-compressor");
            }
            if (q) {
               _aaxProcessCompression = bufCompressValve;
            } else {
               _aaxProcessCompression = bufCompressElectronic;
            }

            setup = xmlNodeGetString(xoid, "setup");
            if (setup)
            {
               free(config->node[n].setup);
               config->node[n].setup = strdup(setup);
               xmlFree(setup);
            }

            config->node[n].hrtf = xmlNodeCopy(xoid, "head");

            f = xmlNodeGetDouble(xoid, "frequency-hz");
            if (f) config->node[n].frequency = f;

            f = xmlNodeGetDouble(xoid, "interval-hz");
            if (f) config->node[n].interval = f;

            f = xmlNodeGetDouble(xoid, "update-hz");
            if (f) config->node[n].update = f;

            i = xmlNodeGetInt(xoid, "max-emitters");
            if (i) config->node[n].no_emitters = i;


            /*
             * find a mathcing backend
             * level == 0, not fout
             * level == 1, defaul device found
             * level == 2, requested device found
             * level == 3, requested device found with requested output port
             */
            xbid = xmlMarkId(xcid);
            be_num = xmlNodeGetNum(xbid, "backend");
            for (be=0; be<be_num; be++)
            {
               static char curlevel = 0;
               char *input, *output;
               char rr[255];
               char level;

               xmlNodeGetPos(xcid, xbid, "backend", be);
               if (xmlNodeCompareString(xbid, "name", config->backend.driver)) {
                  continue;
               }

               level = 0;
               output = xmlNodeCopy(xbid, "output");
               if (output)
               {
                  unsigned int size;

                  *rr = '\0';
                  size = xmlNodeCopyString(output, "renderer", (char*)&rr, 255);
                  if (size && devname[1])
                  {
                     if (strlen(devname[1]) < size) size = strlen(devname[1]);

                     if (!strcasecmp(rr, "default") && level < 2) level = 1;
                     else if (!strncasecmp(devname[1], rr, size))
                     {
                        char *dptr = strstr(devname[1], ": ");
                        char *rrptr = strstr(rr, ": ");
                        if ((dptr-devname[1]) == (rrptr-rr)) level = 3;
                        else level = 2;
                     }
                     else
                     {
                        xmlFree(output);
                        output = 0;
                        continue;
                     }
                  }
                  else {		/* no renderer specified or requested */
                     level = 1;
                  }
               }

               if (level > curlevel)
               {
                  unsigned int q, i, l, index = -1;
                  void *xsid;
                  char *ptr;

                  curlevel = level;

                  ptr = config->backend.driver;
                  l = strlen(config->backend.driver) + strlen(rr) + 5;
                  config->backend.driver = malloc(l);
                  strcpy(config->backend.driver, ptr);
                  strcat(config->backend.driver, " on ");
                  strcat(config->backend.driver, rr);
                  free(ptr);

                  xmlFree(config->backend.output);
                  config->backend.output = output;

                  /* setup speakers */
                  xsid = xmlMarkId(output);

                  i = xmlNodeGetInt(xsid, "channels");
                  if (i > _AAX_MAX_SPEAKERS) i = _AAX_MAX_SPEAKERS;
                  config->node[n].no_speakers = i;

                  i = xmlNodeGetNum(xsid, "speaker");
                  if (i > _AAX_MAX_SPEAKERS) i = _AAX_MAX_SPEAKERS;

                  ptr = xmlNodeGetString(xsid, "setup");
                  if (ptr)
                  {
                     free(config->node[0].setup);
                     config->node[0].setup = strdup(ptr);
                     xmlFree(ptr);
                  }

                  for (q=0; q<i; q++)
                  {
                     char attrib[10];
                     void *ptr;

                     if (xmlAttributeCopyString(output, "n", (char*)&attrib, 9))
                     {
                        char *pe = (char *)&attrib + index;
                        index = strtol(attrib, &pe, 10);
                     }
                     else index++;
                     if (index >= _AAX_MAX_SPEAKERS) {
                        index = _AAX_MAX_SPEAKERS;
                     }

                     xmlFree(config->node[n].speaker[index]);
                     ptr = xmlNodeCopyPos(output, xsid, "speaker", q);
                     config->node[n].speaker[index] = ptr;
                  }
                  xmlFree(xsid);
               }

               input = xmlNodeCopy(xbid, "input");
               if (input)
               {
                  xmlFree(config->backend.input);
                  config->backend.input = input;
               }
            }
            xmlFree(xbid);
         }
      }
      xmlFree(xoid);
      xmlFree(xcid);
   }
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

   free(config->backend.driver);
   xmlFree(config->backend.input);
   xmlFree(config->backend.output);

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

