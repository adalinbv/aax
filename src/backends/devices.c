/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# if HAVE_STRINGS_H
#  include <strings.h>   /* strcasecmp */
# endif
#endif
#include <time.h>	/* time */

#include <xml.h>

#include <base/memory.h>
#include <base/logging.h>

#include <stream/device.h>
#include <backends/software/device.h>
#include <backends/linux/device.h>
#include <backends/oss/device.h>
#include <backends/sdl/device.h>
#ifdef HAVE_WINDOWS_H
# include <backends/windows/wasapi.h>
#endif
#include <dsp/filters.h>
#include <dsp/effects.h>
#include "api.h"
#include "arch.h"
#include "devices.h"
#include "ringbuffer.h"


static _intBuffers *_aaxIntDriverGetBackends();
static xmlId* _aaxDriverDetectConfigConnector(xmlId*, char**, char*, char*);

_intBuffers *
_aaxGetDriverBackends()
{
   _intBuffers *backends = NULL;
   unsigned int r;

   _AAX_LOG(LOG_WARNING, __func__);

   r = _intBufCreate(&backends, _AAX_BACKEND);
   if (r != UINT_MAX)
   {
      _intBuffers *dbe = _aaxIntDriverGetBackends();
      unsigned int i, num;

      num = _intBufGetNumNoLock(dbe, _AAX_BACKEND);
      for (i=0; i<num; i++)
      {
         _intBufferData *dptr;
         _aaxDriverBackend *be;

         dptr = _intBufGetNoLock(dbe, _AAX_BACKEND, i);
         assert (dptr);

         be = _intBufGetDataPtr(dptr);
         if (be)
         {
            assert(be->detect);
#if 0
            printf("Backend #%i: '%s'\n", i, be->driver);
#endif
            if (be->detect(AAX_MODE_WRITE_STEREO) || be->detect(AAX_MODE_READ))
            {
               unsigned int r;

               r = _intBufAddReference(backends, _AAX_BACKEND, dbe, i);
               if (r == UINT_MAX) break;
            }
         }
      }
   }

   return backends;
}

static void
_aax_free_backend(void *p)
{
   _aaxDriverBackend *be = (_aaxDriverBackend*)p;
   be->free_handle(NULL);
   free(be);
}

void *
_aaxRemoveDriverBackends(_intBuffers **be)
{
   _intBufErase(be, _AAX_BACKEND, _aax_free_backend);
   return 0;
}

_aaxDriverBackend *
_aaxGetDriverBackendByName(_intBuffers *bs, const char *name, unsigned int *pos)
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
               *pos = i;
               be = (_aaxDriverBackend *)b;
               break;
            }
         }
      }

      if (i == num)
      {
         be = 0;
         *pos = UINT_MAX;
      }
   }
   else {
      be = _aaxGetDriverBackendDefault(bs, pos);
   }

   return be;
}

_aaxDriverBackend *
_aaxGetDriverBackendDefault(_intBuffers *bs, unsigned int *pos)
{
   _aaxDriverBackend *be = 0;
   int i;

   assert (bs != 0);

   i = _intBufGetNumNoLock(bs, _AAX_BACKEND);
   while (i--)
   {
      _intBufferData *dptr;

      dptr = _intBufGetNoLock(bs, _AAX_BACKEND, i);
      be = _intBufGetDataPtr(dptr);
      if (be->detect(AAX_MODE_WRITE_STEREO) &&
          be->state(NULL, DRIVER_SUPPORTS_PLAYBACK))
      {
         *pos = i;
         break;
      }
   }

   return be;
}

_aaxDriverBackend *
_aaxGetDriverBackendLoopback(unsigned int *pos)
{
   _intBuffers *dbe = _aaxIntDriverGetBackends();
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
      if (found_be)
      {
         if (!strcasecmp(found_be->driver, name))
         {
            *pos = i;
            be = found_be;
            break;
         }
      }
   }

   return be;
}

_aaxDriverBackend *
_aaxGetDriverBackendDefaultCapture(_intBuffers *bs, unsigned int *pos)
{
   _aaxDriverBackend *be = 0;
   int i;

   assert (bs != 0);

   i = _intBufGetNumNoLock(bs, _AAX_BACKEND);
   while(i--)
   {
      _intBufferData *dptr;

      dptr = _intBufGetNoLock(bs, _AAX_BACKEND, i);
      be = _intBufGetDataPtr(dptr);
      if (be->detect(AAX_MODE_READ) &&
          be->state(NULL, DRIVER_SUPPORTS_CAPTURE))
      {
         *pos = i;
         break;
      }
   }

   return be;
}

_aaxDriverBackend *
_aaxGetDriverBackendByPos(_intBuffers *bs, unsigned int pos)
{
   _aaxDriverBackend *be = 0;
   unsigned int num;

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
_aaxDriverBackendSetConfigSettings(_intBuffers *bs, char** devname, _aaxConfig *config)
{
   _intBuffers *dbe = _aaxIntDriverGetBackends();
   time_t rv = time(NULL);
   _aaxDriverBackend *be;
   unsigned int i, num;

   memset(config, 0, sizeof(_aaxConfig));
   if (dbe)
   {
      unsigned int pos = 2;

      if (devname[0])
      {
         num = _intBufGetNumNoLock(dbe, _AAX_BACKEND);
         config->no_backends = num;
         for (i=0; i<num; i++)
         {
            _intBufferData *dptr;
            dptr = _intBufGetNoLock(dbe, _AAX_BACKEND, i);
            be = _intBufGetDataPtr(dptr);
            if (be)
            {
               if (!strcasecmp(devname[0], be->driver))
               {
                  config->backend.driver = _aax_strdup(be->driver);
                  config->backend.input = 0;
                  config->backend.output = 0;
               }
            }
         }
      }

      config->node[0].devname = NULL;
      config->node[0].frequency = 44100;
      config->node[0].setup = _aax_strdup("stereo");
      config->no_nodes = 1;
      config->node[0].interval = 66;
      config->node[0].update = 50;
      config->node[0].hrtf = 0;
      config->node[0].no_speakers = 2;

      if (!devname[0])
      {
         be = _aaxGetDriverBackendDefault(bs, &pos);
         if (be) {
            config->backend.driver = _aax_strdup(be->driver);
         }
      }
   }

   return (long)rv; 
}

void
_aaxDriverBackendReadConfigSettings(const xmlId *xid, char **devname, _aaxConfig *config, char **path, int m)
{
   xmlId *xcid = xmlNodeGet(xid, "/configuration");

   assert(devname);
   assert(config);

   if (xcid != NULL && config != NULL)
   {
      unsigned int n, num;
      xmlId *xoid, *xiid;

      if (path) {
         *path = xmlNodeGetString(xcid, "data-dir");
      }

      if (!m)
      {
         xiid = xmlNodeGet(xcid, "input");	/* global input section */
         if (xiid)
         {
            char *dev = xmlNodeGetString(xiid, "device");
            if (dev)
            {
               free(config->node[0].devname);
               config->node[0].devname = _aax_strdup(dev);
               xmlFree(dev);
            }
            xmlFree(xiid);
         }
      }

      xoid = xmlMarkId(xcid);
      num = xmlNodeGetNum(xoid, "output");      /* global output sections */
      config->no_nodes = num;
      for (n=0; n<num; n++)
      {
         char *dev;

         xmlNodeGetPos(xcid, xoid, "output", n);
         if (n < _AAX_MAX_OUTPUTS)
         {
            unsigned int i;
            char *setup;
            float f;

            if (m)
            {
               dev = xmlNodeGetString(xoid, "device");
               if (dev)
               {
                  if (config->node[n].devname) free(config->node[n].devname);
                  config->node[n].devname = _aax_strdup(dev);
                  xmlFree(dev);
               }
            }

#if 0
            setup = getenv("AAX_TUBE_COMPRESSOR");
            if (setup) {
               i = _aax_getbool(setup);
            } else {
               i = xmlNodeGetBool(xoid, "tube-compressor");
            }
            if (i) {
               
               _aaxProcessCompression = bufCompressValve;
            } else {
               _aaxProcessCompression = bufCompressElectronic;
            }
#endif

            setup = xmlNodeGetString(xoid, "setup");
            if (setup)
            {
               free(config->node[n].setup);
               config->node[n].setup = _aax_strdup(setup);
               xmlFree(setup);
            }

            config->node[n].hrtf = xmlNodeCopy(xoid, "head");

            f = (float)xmlNodeGetDouble(xoid, "frequency-hz");
            if (f) config->node[n].frequency = f;

            f = (float)xmlNodeGetDouble(xoid, "interval-hz");
            if (f) config->node[n].interval = f;

            f = (float)xmlNodeGetDouble(xoid, "update-hz");
            if (f) config->node[n].update = f;

            i = xmlNodeGetInt(xoid, "max-emitters");
            if (i) config->node[n].no_emitters = i;
         }
      }
      xmlFree(xoid);
   }

   if (xcid)
   {
      float v = (float)xmlNodeGetDouble(xcid, "version");
      if (v < AAX_MIN_CONFIG_VERSION || v > AAX_MAX_CONFIG_VERSION)
      {
         xmlFree(xcid);
         fprintf(stderr, "AAX: incompattible configuration file version, skipping.\n");
         return;
      }
   }

   /* Config file version 2.0 */
   if (xcid != NULL && config != NULL)
   {
      unsigned int dev, dev_num;
      char device_name[255];
      int curlevel;
      xmlId *xdid;

      /*
       * find a matching backend
       * level == 0, not fout
       * level == 1, defaul device found
       * level == 2, requested device found
       * level == 3, requested device found with requested output port
       */
      curlevel = -1;
      xdid = xmlMarkId(xcid);
      dev_num = xmlNodeGetNum(xdid, "device");
      snprintf((char*)device_name, 255, "%s", config->backend.driver);
      for (dev=0; dev<dev_num; dev++)
      {
         unsigned int con, con_num;
         int level;
         xmlId *xiid;
         char *ptr;

         xmlNodeGetPos(xcid, xdid, "device", dev);
         if (xmlAttributeCompareString(xdid, "backend", device_name)) {
            continue;
         }

         if (devname[1])
         {
            ptr = strstr(devname[1], ": ");
            if (ptr) *ptr = 0;
            level = xmlAttributeCompareString(xdid, "name", devname[1]);
            if (ptr) *ptr = ':';
            if (level) {
               continue;
            }
         }

         xiid = xmlMarkId(xdid);
         con_num = xmlNodeGetNum(xiid, "connector");
         for (con=0; con<con_num; con++)
         {
            level = 0;

            xmlNodeGetPos(xdid, xiid, "connector", con);
            if ((m && xmlAttributeCompareString(xiid, "type", "out")) ||
                (!m && xmlAttributeCompareString(xiid, "type", "in")))
            {
               continue;
            }

            _aaxDriverDetectConfigConnector(xiid, devname,
                                             (char *)&level,
                                             (char *)&curlevel);
            if (level > curlevel)
            {
               ssize_t q, i, l, index = -1;
               char *ptr, *tmp;
               char rr[2][255];
               float f;

               curlevel = level;

               xmlAttributeCopyString(xdid, "name", (char*)rr[0], 255);
               xmlAttributeCopyString(xiid, "name", (char*)rr[1], 255);

               ptr = config->backend.driver;
               if (ptr)
               {
                  tmp = strstr(ptr, " on ");
                  if (tmp) q = tmp-ptr;
                  else q = strlen(ptr);

                  l = ++q + strlen(rr[0]) + strlen(rr[1]) + strlen(" on : \0");
                  config->backend.driver = malloc(l);

                  ///copy the backend name (might be a part of the
                  // renderer string)
                  snprintf(config->backend.driver, q, "%s", ptr);	
                  strcat(config->backend.driver, " on ");
                  strcat(config->backend.driver, rr[0]);   /* device name    */
                  strcat(config->backend.driver, ": ");
                  strcat(config->backend.driver, rr[1]);   /* interface name */
                  free(ptr);
               }

               xmlNodeGetPos(xdid, xiid, "connector", con);
               if ((ptr = xmlGetString(xiid)) != NULL)
               {
                  if (m)
                  {
                     xmlFree(config->backend.output);
                     config->backend.output = ptr;
                  }
                  else
                  {
                     xmlFree(config->backend.input);
                     config->backend.input = ptr;
                  }
               }

               i = xmlNodeGetInt(xiid, "bitrate");
               if (i) config->node[0].bitrate = i*1000;

               f = (float)xmlNodeGetDouble(xiid, "frequency-hz");
               if (f) config->node[0].frequency = f;

               f = (float)xmlNodeGetDouble(xiid, "interval-hz");
               if (f) config->node[0].interval = f;

               ptr = xmlNodeGetString(xiid, "setup");
               if (ptr)
               {
                  free(config->node[0].setup);
                  config->node[0].setup = _aax_strdup(ptr);
                  xmlFree(ptr);
               }

               if (m)
               {
                  xmlId *xsid;

                  i = xmlNodeGetInt(xiid, "channels");
                  if (i > _AAX_MAX_SPEAKERS) i = _AAX_MAX_SPEAKERS;
                  config->node[0].no_speakers = i;

                  if (xmlNodeTest(xiid, "hrtf"))
                  {
                      xmlFree(config->node[0].hrtf);
                      config->node[0].hrtf = xmlNodeCopy(xiid, "hrtf");
                  }

                  /* setup speakers */
                  xsid = xmlMarkId(xiid);
                  i = xmlNodeGetNum(xiid, "speaker");
                  if (i > _AAX_MAX_SPEAKERS) i = _AAX_MAX_SPEAKERS;
                  for (q=0; q<i; q++)
                  {
                     size_t len;
                     char s[10];
                     char *ptr;

                     xmlNodeGetPos(xiid, xsid, "speaker", q);
                     len = xmlAttributeCopyString(xsid, "n", (char*)&s, 10);
                     if (len)
                     {
                        char *eptr = (char *)&s + len;
                        index = strtol(s, &eptr, 10);
                     }
                     else index++;
                     if (index >= _AAX_MAX_SPEAKERS) {
                        index = _AAX_MAX_SPEAKERS;
                     }

                     xmlNodeGetPos(xiid, xsid, "speaker", q);
                     if ((ptr = xmlGetString(xsid)) != NULL)
                     {
                        xmlFree(config->node[0].speaker[index]);
                        config->node[0].speaker[index] = ptr;
                     }
                  }
                  xmlFree(xsid);
               }
            }
         } // for
         xmlFree(xiid);
      }
      xmlFree(xdid);
      xmlFree(xcid);
   }

#if 0
 if (config)
 {
   int q;
   printf("Config: %s\n", m ? "Write" : "Read");
   printf(" no. nodes: %i\n", config->no_nodes);
   printf(" no_backends: %i\n", config->no_backends);
   for(q=0; q<config->no_nodes; ++q) {
     printf(" Node: %i\n", q+1);
     printf("  devname: '%s'\n", config->node[q].devname);
     printf("  setup: '%s'\n", config->node[q].setup);
     printf("  no. speakers: %i\n", config->node[q].no_speakers);
     printf("  bitrate: %i\n", config->node[q].bitrate);
     printf("  frequency: %7.1f\n", config->node[q].frequency);
     printf("  interval: %4.1f\n", config->node[q].interval);
     printf("  update: %4.1f\n", config->node[q].update);
     printf("  no. emitters: %i\n", config->node[q].no_emitters);
   }
   printf(" Backend:\n");
   printf("  driver: '%s'\n", config->backend.driver);
 }
 else printf("config == NULL\n");
#endif
}

void
_aaxDriverBackendClearConfigSettings(_aaxConfig *config)
{
   unsigned int i;
   for (i=0; i<config->no_nodes; i++)
   {
      int q;
      if (config->node[i].setup) free(config->node[i].setup);
      if (config->node[i].devname) free(config->node[i].devname);
      if (config->node[i].hrtf) xmlFree(config->node[i].hrtf);
      for (q=0; q<config->node[i].no_speakers; q++) {
         if (config->node[i].speaker[q]) xmlFree(config->node[i].speaker[q]);
      }
   }

   if (config->backend.driver) free(config->backend.driver);
   if (config->backend.input) xmlFree(config->backend.input);
   if (config->backend.output) xmlFree(config->backend.output);

   free(config);
}

/* -------------------------------------------------------------------------- */

static _intBuffers *_aaxIntBackends = NULL;

static void
_aaxIntDriverRemoveBackends()
{
   if (_aaxIntBackends)
   {
      _intBufErase(&_aaxIntBackends, _AAX_BACKEND, 0);
      _aaxIntBackends = NULL;
   }
}

static _intBuffers *
_aaxIntDriverGetBackends()
{
   if (_aaxIntBackends == NULL)
   {
      unsigned int r = _intBufCreate(&_aaxIntBackends, _AAX_BACKEND);
      if (r == UINT_MAX) return NULL;

      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxNoneDriverBackend);
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxStreamDriverBackend);
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxLoopbackDriverBackend);
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxSDLDriverBackend);
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxOSS3DriverBackend);
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxOSS4DriverBackend);
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxALSADriverBackend);
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxLinuxDriverBackend);
#ifdef HAVE_PULSEAUDIO_H
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxPulseAudioDriverBackend);
#endif
#if HAVE_PIPEWIRE_H
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxPipeWireDriverBackend);
#endif
#ifdef HAVE_WINDOWS_H
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxWASAPIDriverBackend);
#endif

      atexit(_aaxIntDriverRemoveBackends);
   }

   return _aaxIntBackends;
}

static xmlId*
_aaxDriverDetectConfigConnector(xmlId *xid, char **devname, char *l, char *cl)
{
   size_t size;
   char curlevel = *cl;
   char level = 0;
   char rr[255];

   *rr = '\0';
   size = xmlAttributeCopyString(xid, "name", (char*)&rr, 255);
   if (size)
   {
      if (devname[1])
      {
         char *dptr = strstr(devname[1], ": ");

         if (strlen(devname[1]) < size) size = strlen(devname[1]);

         if (!strcasecmp(rr, "default") && level < 2) level = 1;
         if (dptr)
         {
            dptr += strlen(": ");
            if (!strcasecmp(rr, dptr)) level = 3;
            else xid = 0;
         }
         else {
            level = 2;
         }
      }                 /* no renderer is requested and default is defined */
      else if (!strcasecmp(rr, "default")) level = 3;
   }                    /* no renderer defined but default is requested */
   else if (!devname[1] || !strcasecmp(devname[1], "default")) level = 2;

   *l = level;
   *cl = curlevel;

   return xid;
}
