/*
 * Copyright 2005-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
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

#include <base/logging.h>

#include "stream/file.h"
#include "dmedia/device.h"
#include "alsa/device.h"
#include "oss/device.h"
#ifdef HAVE_WINDOWS_H
# include "windows/wasapi.h"
#endif
#include "api.h"
#include "arch.h"
#include "devices.h"
#include "ringbuffer.h"
#include "filters/effects.h"

static _intBuffers *_aaxIntDriverGetBackends();
static char* _aaxDriverDetectConfigConnector(char*, char**, char*, char*);
static void _aaxDriverOldBackendReadConfigSettings(void*, char**, _aaxConfig*, const char*, int);

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

void *
_aaxRemoveDriverBackends(_intBuffers **be)
{
   _intBufErase(be, _AAX_BACKEND, free);
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
         config->backend.driver = _aax_strdup(be->driver);
      }
   }

   return (long)rv; 
}

void
_aaxDriverBackendReadConfigSettings(void *xid, char **devname, _aaxConfig *config, const char *path, int m)
{
   void *xcid = xmlNodeGet(xid, "/configuration");

   if (xcid != NULL && config != NULL)
   {
      unsigned int n, num;
      void *xoid, *xiid;

      if (!m)
      {
         xiid = xmlNodeGet(xcid, "input");	/* global input section */
         if (xiid)
         {
            char *dev = xmlNodeGetString(xiid, "device");
            if (dev)
            {
               free(config->node[n].devname);
               config->node[n].devname = _aax_strdup(dev);
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

         if (n < _AAX_MAX_SLAVES)
         {
            unsigned int i;
            char *setup;
            float f;

            if (m)
            {
               dev = xmlNodeGetString(xoid, "device");
               if (dev)
               {
                  free(config->node[n].devname);
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
      else if (v < AAX_NEW_CONFIG_VERSION)
      {
         xmlFree(xcid);
         _aaxDriverOldBackendReadConfigSettings(xid, devname, config, path, m);
         return;
      }
   }

   /* Config file version 2.0 */
   if (xcid != NULL && config != NULL)
   {
      unsigned int dev, dev_num;
      char device_name[255];
      int curlevel;
      void *xdid;

      /*
       * find a mathcing backend
       * level == 0, not fout
       * level == 1, defaul device found
       * level == 2, requested device found
       * level == 3, requested device found with requested output port
       */
      curlevel = 0;
      xdid = xmlMarkId(xcid);
      dev_num = xmlNodeGetNum(xdid, "device");
      snprintf((char*)device_name, 255, "%s", config->backend.driver);
      for (dev=0; dev<dev_num; dev++)
      {
         unsigned int con, con_num;
         int level;
         void *xiid;
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

               tmp = strstr(ptr, " on ");
               if (tmp) q = tmp-ptr;
               else q = strlen(ptr);

               l = ++q + strlen(rr[0]) + strlen(rr[1]) + strlen(" on : \0");
               config->backend.driver = malloc(l);

               // copy the backend name (might be a part of the renderer string)
               snprintf(config->backend.driver, q, "%s", ptr);	
               strcat(config->backend.driver, " on ");
               strcat(config->backend.driver, rr[0]);	/* device name    */
               strcat(config->backend.driver, ": ");
               strcat(config->backend.driver, rr[1]);	/* interface name */
               free(ptr);

               if (m)
               {
                  xmlFree(config->backend.output);
                  config->backend.output = xmlNodeCopyPos(xdid, xiid, "connector", con);
               }
               else
               {
                  xmlFree(config->backend.input);
                  config->backend.input = xmlNodeCopyPos(xdid, xiid, "connector", con);
               }

               i = xmlNodeGetInt(xiid, "bitrate");
               if (i) config->node[0].bitrate = i;

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
                  void *xsid;

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
                     char s[10];
                     size_t len;
                     void *ptr;

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

                     xmlFree(config->node[0].speaker[index]);
                     ptr = xmlNodeCopyPos(xiid, xsid, "speaker", q);
                     config->node[0].speaker[index] = ptr;
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
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxFileDriverBackend);
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxLoopbackDriverBackend);
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxOSSDriverBackend);
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxALSADriverBackend);
       _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxLinuxDriverBackend);
#ifdef HAVE_WINDOWS_H
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxWASAPIDriverBackend);
#endif
      _intBufAddData(_aaxIntBackends, _AAX_BACKEND, &_aaxDMediaDriverBackend);

      atexit(_aaxIntDriverRemoveBackends);
   }

   return _aaxIntBackends;
}

static char*
_aaxDriverDetectConfigConnector(char *xid, char **devname, char *l, char *cl)
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

/* Config file version 1.x */
static char*
_aaxDriverOldDetectConfigRenderer(char *xid, char **devname, char *l, char *cl)
{
   size_t size;
   char curlevel = *cl;
   char level = 0;
   char rr[255];

   *rr = '\0';
   size = xmlNodeCopyString(xid, "renderer", (char*)&rr, 255);
   if (size)
   {
      if (devname[1])
      {
         if (strlen(devname[1]) < size) size = strlen(devname[1]);

         if (!strcasecmp(rr, "default") && level < 2) level = 1;
         if (!strncasecmp(devname[1], rr, size))
         {
            char *dptr = strstr(devname[1], ": ");
            char *rrptr = strstr(rr, ": ");
            if ((dptr-devname[1]) == (rrptr-rr)) level = 3;
            else level = 2;
         }
         else {
            xid = 0;
         }
      }                 /* no renderer is requested and default is defined */
      else if (!strcasecmp(rr, "default")) level = 3;
   }                    /* no renderer defined but default is requested */
   else if (!devname[1] || !strcasecmp(devname[1], "default")) level = 2;

   *l = level;
   *cl = curlevel;

   return xid;
}

static void
_aaxDriverOldBackendReadConfigSettings(void *xid, char **devname, _aaxConfig *config, const char *path, int m)
{
   void *xcid = xmlNodeGet(xid, "/configuration");

   if (xcid != NULL && config != NULL)
   {
      char driver_name[255];
      unsigned int be, be_num;
      char curlevel;
      void *xbid;

      /*
       * find a mathcing backend
       * level == 0, not fout
       * level == 1, defaul device found
       * level == 2, requested device found
       * level == 3, requested device found with requested output port
       */
      curlevel = 0;
      xbid = xmlMarkId(xcid);
      be_num = xmlNodeGetNum(xbid, "backend");
      snprintf((char*)driver_name, 255, "%s", config->backend.driver);
      for (be=0; be<be_num; be++)
      {
         char level;

         xmlNodeGetPos(xcid, xbid, "backend", be);
         if (xmlNodeCompareString(xbid, "name", driver_name)) {
            continue;
         }

         if (m)
         {
            char *output = xmlNodeCopy(xbid, "output");
            level = 0;

            if (output)
            {
               _aaxDriverOldDetectConfigRenderer(output, devname,
                                                (char *)&level,
                                                (char *)&curlevel);
               if (level > curlevel)
               {
                  ssize_t q, i, l, index = -1;
                  char *ptr, *tmp;
                  char rr[255];
                  void *xsid;
                  float f;

                  curlevel = level;

                  xmlNodeCopyString(output, "renderer", (char*)&rr, 255);
                  ptr = config->backend.driver;

                  tmp = strstr(ptr, " on ");
                  if (tmp) q = tmp-ptr;
                  else q = strlen(ptr);

                  l = ++q + strlen(rr) + strlen(" on \0");
                  config->backend.driver = malloc(l);
                  snprintf(config->backend.driver, q, "%s", ptr);
                  strcat(config->backend.driver, " on ");
                  strcat(config->backend.driver, rr);
                  free(ptr);

                  xmlFree(config->backend.output);
                  config->backend.output = output;

                  f = (float)xmlNodeGetDouble(output, "frequency-hz");
                  if (f) config->node[0].frequency = f;

                  f = (float)xmlNodeGetDouble(output, "interval-hz");
                  if (f) config->node[0].interval = f;

                  i = xmlNodeGetInt(output, "channels");
                  if (i > _AAX_MAX_SPEAKERS) i = _AAX_MAX_SPEAKERS;
                  config->node[0].no_speakers = i;

                  ptr = xmlNodeGetString(output, "setup");
                  if (ptr)
                  {
                     free(config->node[0].setup);
                     config->node[0].setup = _aax_strdup(ptr);
                     xmlFree(ptr);
                  }

                  /* setup speakers */
                  xsid = xmlMarkId(output);
                  i = xmlNodeGetNum(xsid, "speaker");
                  if (i > _AAX_MAX_SPEAKERS) i = _AAX_MAX_SPEAKERS;
                  for (q=0; q<i; q++)
                  {
                     char s[10];
                     size_t len;
                     void *ptr;

                     xmlNodeGetPos(output, xsid, "speaker", q);
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

                     xmlFree(config->node[0].speaker[index]);
                     ptr = xmlNodeCopyPos(output, xsid, "speaker", q);
                     config->node[0].speaker[index] = ptr;
                  }
                  xmlFree(xsid);
               }
               else {
                  xmlFree(output);
               }
            }
         }
         else /* m == AAAX_MODE_READ */
         {
            char *input = xmlNodeCopy(xbid, "input");

            level = 0;
            if (input)
            {
               _aaxDriverOldDetectConfigRenderer(input, devname,
                                                (char *)&level,
                                                (char *)&curlevel);
               if (level >= curlevel)
               {
                  size_t q, l;
                  char *ptr, *tmp;
                  char rr[255];
                  float f;

                  curlevel = level;

                  xmlNodeCopyString(input, "renderer", (char*)&rr, 255);
                  ptr = config->backend.driver;

                  tmp = strstr(ptr, " on ");
                  if (tmp) q = tmp-ptr;
                  else q = strlen(ptr);

                  l = ++q + strlen(rr) + strlen(" on \0");
                  config->backend.driver = malloc(l);
                  snprintf(config->backend.driver, q, "%s", ptr);
                  strcat(config->backend.driver, " on ");
                  strcat(config->backend.driver, rr);
                  free(ptr);

                  xmlFree(config->backend.input);
                  config->backend.input = input;

                  f = (float)xmlNodeGetDouble(input, "frequency-hz");
                  if (f) config->node[0].frequency = f;

                  f = (float)xmlNodeGetDouble(input, "interval-hz");
                  if (f) config->node[0].interval = f;

                  ptr = xmlNodeGetString(input, "setup");
                  if (ptr)
                  {
                     free(config->node[0].setup);
                     config->node[0].setup = _aax_strdup(ptr);
                     xmlFree(ptr);
                  }
               }
               else {
                  xmlFree(input);
               }
            }
         }
      }
      xmlFree(xbid);
      xmlFree(xcid);
   }
}

