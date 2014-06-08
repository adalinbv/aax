/*
 * Copyright 2007-2014 by Erik Hofman.
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

#ifdef HAVE_LIBIO_H
#include <libio.h>		/* for NULL */
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include <math.h>		/* for INFINITY */
#include <assert.h>
#include <xml.h>

#include <base/gmath.h>
#include <base/logging.h>

#include "api.h"
#include "devices.h"
#include "arch.h"
#include "ringbuffer.h"

static _intBuffers* get_backends();
static _handle_t* _open_handle(aaxConfig);
static _aaxConfig* _aaxReadConfig(_handle_t*, const char*, int);
static void _aaxContextSetupHRTF(void *, unsigned int);
static void _aaxContextSetupSpeakers(void **, unsigned int);
static void _aaxFreeSensor(void *);
static int _aaxCheckKeyValidity(void*);
static int _aaxCheckKeyValidityStr(char*);

static _aaxFilterInfo _aaxMixerDefaultEqualizer[2];
static const char* _aax_default_devname;
static char* _default_renderer = "default";

int _client_release_mode = 0;
_aaxMixerInfo* _info = NULL;
_intBuffers* _backends = NULL;
time_t _tvnow = 0;


AAX_API const char* AAX_APIENTRY
aaxDriverGetSetup(const aaxConfig config, enum aaxSetupType type)
{
   _handle_t *handle = get_handle(config);
   char *rv = NULL;
   if (handle)
   {
      const _aaxDriverBackend *be = handle->backend.ptr;
      switch(type)
      {
      case AAX_DRIVER_STRING:
         if (handle->backend.driver) {
            rv = (char*)be->renderer;
         } else {
            rv = (char*)be->driver;
         }
         break;
      case AAX_RENDERER_STRING:
         if (handle->backend.driver) {
            rv = (char*)handle->backend.driver;
         } else {
            rv = (char*)be->driver;
         }
         break;
      case AAX_VERSION_STRING:
         rv = (char*)be->version;
         break;
      case AAX_VENDOR_STRING:
         rv = (char*)be->vendor;
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return (const char*)rv;
}

AAX_API unsigned AAX_APIENTRY
aaxDriverGetCount(enum aaxRenderMode mode)
{
   unsigned rv = 0;
   if (mode < AAX_MODE_WRITE_MAX)
   {
      _intBuffers* backends = get_backends();
      unsigned count = _intBufGetNumNoLock(backends, _AAX_BACKEND);
      unsigned i, m = (mode == AAX_MODE_READ) ? 0 : 1;
      for (i=0; i<count; i++)
      {
         _aaxDriverBackend *be = _aaxGetDriverBackendByPos(backends, i);
         if (be)
         {
            if (!m && be->state(NULL, DRIVER_SUPPORTS_CAPTURE)) rv++;
            else if (m && be->state(NULL, DRIVER_SUPPORTS_PLAYBACK)) rv++;
         }
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_ENUM);
   }
   return rv;
}

AAX_API aaxConfig AAX_APIENTRY
aaxDriverGetByPos(unsigned pos_req, enum aaxRenderMode mode)
{
   _handle_t *handle = NULL;

   if (mode < AAX_MODE_WRITE_MAX)
   {
      handle = new_handle();
      if (handle)
      {
         unsigned count = _intBufGetNumNoLock(handle->backends, _AAX_BACKEND);
         unsigned i,  m = (mode == AAX_MODE_READ) ? 0 : 1;
         _aaxDriverBackend *be = NULL;
         unsigned p = 0;

         for (i=0; i<count; i++)
         {
            be = _aaxGetDriverBackendByPos(handle->backends, i);
            if (be)
            {
               if ((!m && be->state(NULL, DRIVER_SUPPORTS_CAPTURE)) &&
                      (p++ == pos_req)) {
                  break;
               }
               else if ((m && be->state(NULL, DRIVER_SUPPORTS_PLAYBACK)) &&
                           (p++ == pos_req)) {
                  break;
               }
            }
         }
 
         if (be)
         {
            handle->id = HANDLE_ID;
            handle->backend.ptr = be;
            handle->info->mode = mode;
//          handle->devname[0] = be->driver;
            handle->devname[0] = (char *)_aax_default_devname;

         }
         else
         {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
            aaxDriverDestroy(handle);
            handle = NULL;
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_ENUM);
   }
   return (aaxConfig)handle;
}

AAX_API aaxConfig AAX_APIENTRY
aaxDriverGetByName(const char* name, enum aaxRenderMode mode)
{
   _handle_t *handle = NULL;

   if (mode < AAX_MODE_WRITE_MAX)
   {
      handle = new_handle();
      if (handle)
      {
         handle->info->mode = mode;
         if ((name != NULL) 
#ifdef WIN32
              && strcasecmp(name, "Generic Software")
              && strcasecmp(name, "Generic Hardware")
#endif
            )
         {
            char *ptr;

            handle->devname[0] = _aax_strdup(name);
            ptr = strstr(handle->devname[0], " on ");
            if (ptr)
            {
               *ptr = 0;
               handle->devname[1] = ptr+4;	/* 4 = strlen(" on ") */
            }
            handle->backend.ptr = _aaxGetDriverBackendByName(handle->backends,
                                                            handle->devname[0],
                                                            &handle->be_pos);
         }
         else /* name == NULL */
         {
            const _aaxDriverBackend *be;

            if (mode == AAX_MODE_READ) {
               be = _aaxGetDriverBackendDefaultCapture(handle->backends,
                                                       &handle->be_pos);
            } else {
               be = _aaxGetDriverBackendDefault(handle->backends,
                                                &handle->be_pos);
            }

            handle->backend.ptr = be;
            if (be) { /* be == NULL should never happen */
               handle->devname[0] = _aax_strdup(be->driver);
               handle->devname[1] = be->name(handle->backend.handle, mode);
            }
         }

         if (handle->backend.ptr == NULL)
         {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
            if (handle->devname[0] != _aax_default_devname)
            {
               free(handle->devname[0]);
               handle->devname[0] = (char *)_aax_default_devname;
            }
            aaxDriverDestroy(handle);
            handle = NULL;
         }
      }
      else
      {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_ENUM);
   }
   return handle;
}

AAX_API int AAX_APIENTRY
aaxDriverGetSupport(const aaxConfig config, enum aaxRenderMode mode)
{
   _handle_t *handle = get_handle(config);
   int rv = AAX_FALSE;

   if (handle)
   {
      const _aaxDriverBackend *be = handle->backend.ptr;

      switch (mode)
      {
      case AAX_MODE_READ:
         rv = be->state(NULL, DRIVER_SUPPORTS_CAPTURE);
         break;
      case AAX_MODE_WRITE_STEREO:
      case AAX_MODE_WRITE_SURROUND:
      case AAX_MODE_WRITE_HRTF:
         rv = be->state(NULL, DRIVER_SUPPORTS_PLAYBACK);
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   } 
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API aaxConfig AAX_APIENTRY
aaxDriverOpenDefault(enum aaxRenderMode mode)
{
   aaxConfig config = aaxDriverGetByName(NULL, mode);
   return aaxDriverOpen(config);
}

AAX_API aaxConfig AAX_APIENTRY
aaxDriverOpen(aaxConfig config)
{
   _handle_t *handle = _open_handle(config);
   if (handle)
   {
      enum aaxRenderMode mode = handle->info->mode;
      _aaxConfig *cfg = _aaxReadConfig(handle, NULL, mode);
      const _aaxDriverBackend *be = handle->backend.ptr;
      void *xoid, *nid = 0;

      if (cfg)
      {
         if (handle->info->mode == AAX_MODE_READ) {
            xoid = cfg->backend.input;
         } else {
            xoid = cfg->backend.output;
         }
         if (be)
         {
            const char* name = handle->devname[1];
            char *renderer;

            handle->backend.handle = be->connect(nid, xoid, name, mode);

            if (handle->backend.driver != _default_renderer) {
               free(handle->backend.driver);
            }
            renderer = be->name(handle->backend.handle, mode);
            handle->backend.driver = renderer ? renderer : _default_renderer;
            if (_info == NULL) {
               _info =  handle->info;
            }
         }
         _aaxDriverBackendClearConfigSettings(cfg);
      }
      else {
         _AAX_SYSLOG("invalid personal product key");
      }
   }

   if (!handle || !handle->backend.handle)
   {
      aaxDriverClose(handle);
      aaxDriverDestroy(handle);
      handle = NULL;

      _aaxErrorSet(AAX_INVALID_DEVICE);
   }
   return (aaxConfig)handle;
}

AAX_API aaxConfig AAX_APIENTRY
aaxDriverOpenByName(const char* name, enum aaxRenderMode mode)
{
   _handle_t *handle = NULL;
   if (mode < AAX_MODE_WRITE_MAX)
   {
      if (name != NULL)
      {
         aaxConfig config = aaxDriverGetByName(name, mode);
         handle = _open_handle(config);
         if (handle)
         {
            const _aaxDriverBackend *be = handle->backend.ptr;
            void *xoid, *nid = 0;
            _aaxConfig *cfg;

            if (mode == AAX_MODE_WRITE_STEREO) mode = handle->info->mode;
            else handle->info->mode = mode;

            cfg = _aaxReadConfig(handle, name, mode);
            if (cfg)
            {
               if (mode == AAX_MODE_READ) {
                  xoid = cfg->backend.input;
               } else {
                  xoid = cfg->backend.output;
               }
               if (be)
               {
                  const char* devname = handle->devname[1];
                  char *renderer;

                  handle->backend.handle= be->connect(nid, xoid, devname, mode);

                  if (handle->backend.driver != _default_renderer) {
                     free(handle->backend.driver);
                  }
                  renderer = be->name(handle->backend.handle, mode);
                  handle->backend.driver=renderer? renderer : _default_renderer;
                  if (_info == NULL) {
                     _info =  handle->info;
                  }
               }
               _aaxDriverBackendClearConfigSettings(cfg);
            }
            else {
               _AAX_SYSLOG("invalid personal product key");
            }
         }
   
         if (handle && !handle->backend.handle)
         {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
            aaxDriverClose(handle);
            aaxDriverDestroy(handle);
            handle = NULL;
         }
      }
      else {
         handle = aaxDriverOpenDefault(mode);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_ENUM);
   }
   return (aaxConfig)handle;
}

AAX_API int AAX_APIENTRY
aaxDriverDestroy(aaxConfig config)
{
   _handle_t *handle = get_handle(config);
   int rv = AAX_FALSE;

   aaxMixerSetState(handle, AAX_STOPPED);
   aaxSensorSetState(handle, AAX_STOPPED);
   aaxDriverClose(handle);

   if (handle && !handle->handle)
   {
      assert(handle->backends != NULL);

      _aaxSignalFree(&handle->buffer_ready);

      handle->info->id = 0xdeadbeef;
      if (_info == handle->info) {
         _info = NULL;
      }

      _intBufErase(&handle->sensors, _AAX_SENSOR, _aaxFreeSensor);

      if (handle->devname[0] != _aax_default_devname)
      {
         free(handle->devname[0]);
         handle->devname[0] = (char *)_aax_default_devname;
      }

      if (handle->backend.driver != _default_renderer) {
         free(handle->backend.driver);
      }

      if (handle->ringbuffer) {
         _aaxRingBufferFree(handle->ringbuffer);
      }

      /* safeguard against using already destroyed handles */
      handle->id = 0xdeadbeef;
      free(handle);

      _aaxRemoveDriverBackends(&_backends);
      rv = AAX_TRUE;
   }
   else if (handle) {
       _aaxErrorSet(AAX_INVALID_STATE);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
};

AAX_API int AAX_APIENTRY
aaxDriverClose(aaxConfig config)
{
   _handle_t *handle = get_handle(config);
   int rv = AAX_FALSE;

   if (handle && handle->backend.handle)
   {
      const _aaxDriverBackend *be = handle->backend.ptr;

      if (aaxMixerGetState(config) != AAX_STOPPED) {
         aaxMixerSetState(config, AAX_STOPPED);
      }
      if (be && handle->backend.handle) {
         be->disconnect(handle->backend.handle);
      }
      handle->backend.handle = NULL;
      rv = AAX_TRUE;
   }
   else if (handle) {
       _aaxErrorSet(AAX_INVALID_STATE);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API unsigned AAX_APIENTRY
aaxDriverGetDeviceCount(const aaxConfig config, enum aaxRenderMode mode)
{
   unsigned int num = 0;

   if (mode < AAX_MODE_WRITE_MAX)
   {
      _handle_t *handle = get_handle(config);
      if (handle)
      {
         const _aaxDriverBackend *be = handle->backend.ptr;
         void* be_handle = handle->backend.handle;
         char *ptr;

         ptr = be->get_devices(be_handle, mode);
         if (ptr)
         {
            while (*ptr)
            {
               ptr += strlen(ptr)+1;
               num++;
            }
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_ENUM);
   }
   return num;
}

AAX_API const char* AAX_APIENTRY
aaxDriverGetDeviceNameByPos(const aaxConfig config, unsigned pos, enum aaxRenderMode mode)
{
   char *rv = NULL;

   if (mode < AAX_MODE_WRITE_MAX)
   {
      _handle_t *handle = get_handle(config);
      if (handle)
      {
         const _aaxDriverBackend *be = handle->backend.ptr;
         void* be_handle = handle->backend.handle;
         unsigned int num = 0;
         char *ptr;

         if (handle->be_pos != pos)
         {
            aaxDriverClose(handle);
            handle->be_pos = pos;
            be_handle = NULL;
         }

         if (!be_handle)
         {
            char *renderer;

            be_handle = be->new_handle(mode);
            handle->backend.handle = be_handle;

            if (handle->backend.driver != _default_renderer) {
               free(handle->backend.driver);
            }
            renderer = be->name(handle->backend.handle, mode);
            handle->backend.driver = renderer ? renderer : _default_renderer;
         }

         ptr = be->get_devices(be_handle, mode);
         if (ptr)
         {
            while(*ptr && (pos != num))
            {
               ptr += strlen(ptr)+1;
               num++;
            }
            if (ptr) {
               rv = ptr;
            }
             else {
               _aaxErrorSet(AAX_INVALID_PARAMETER);
            }
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_ENUM);
   }
   return rv;
}

AAX_API unsigned AAX_APIENTRY
aaxDriverGetInterfaceCount(const aaxConfig config, const char* devname, enum aaxRenderMode mode)
{
   unsigned int num = 0;

   if (mode < AAX_MODE_WRITE_MAX)
   {
      _handle_t *handle = get_handle(config);
      if (handle)
      {
         const _aaxDriverBackend *be = handle->backend.ptr;
         void* be_handle = handle->backend.handle;
         char *ptr;

         ptr = be->get_interfaces(be_handle, devname, mode);
         if (ptr)
         {
            while(*ptr)
            {
               ptr += strlen(ptr)+1;
               num++;
            }
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_ENUM);
   }
   return num;
}

AAX_API const char* AAX_APIENTRY
aaxDriverGetInterfaceNameByPos(const aaxConfig config, const char* devname, unsigned pos, enum aaxRenderMode mode)
{
   char *rv = NULL;

   if (mode < AAX_MODE_WRITE_MAX)
   {
      _handle_t *handle = get_handle(config);
      if (handle)
      {
         const _aaxDriverBackend *be = handle->backend.ptr;
         void* be_handle = handle->backend.handle;
         unsigned int num = 0;
         char *ptr;

         ptr = be->get_interfaces(be_handle, devname, mode);
         if (*ptr)
         {
            while(*ptr && (pos != num))
            {
               ptr += strlen(ptr)+1;
               num++;
            }
            if (ptr) {
               rv = ptr;
            } else {
               _aaxErrorSet(AAX_INVALID_PARAMETER);
            }
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_ENUM);
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

static const char* _aax_default_devname = "None";

static _intBuffers*
get_backends()
{
   if (_backends == NULL) {
      _backends = _aaxGetDriverBackends();
   }
   return _backends;
}

_handle_t*
new_handle()
{
   unsigned long size;
   _handle_t *rv = NULL;
   void *ptr1;
   char *ptr2;

   size = sizeof(_handle_t);
   ptr2 = (char*)size;
   size += sizeof(_aaxMixerInfo);

   ptr1 = _aax_calloc(&ptr2, 1, size);
   if (ptr1)
   {
      _handle_t* handle = (_handle_t*)ptr1;

      handle->id = HANDLE_ID;
      handle->backends = get_backends();

      handle->mixer_pos = UINT_MAX;
      handle->be_pos = UINT_MAX;
      _SET_PROCESSED(handle);

      handle->info = (_aaxMixerInfo*)ptr2;
      _aaxSetDefaultInfo(handle->info, handle);

      rv = handle;
   }

   return rv;
}

_handle_t*
get_handle(aaxConfig config)
{
   _handle_t *handle = (_handle_t *)config;
   _handle_t *rv = NULL;

   if (handle && handle->id == HANDLE_ID) {
      rv = handle;
   }

   return rv;
}

_handle_t*
get_valid_handle(aaxConfig config)
{
   _handle_t *handle = (_handle_t *)config;
   _handle_t *rv = NULL;

   if (handle)
   {
      if (handle->id == HANDLE_ID)
      {
         if (handle->valid & ~HANDLE_ID) {
            rv = handle;
         }
      }
      else if (handle->id == AUDIOFRAME_ID) {
         rv = handle;
      }
   }

   return rv;
}

_handle_t*
get_write_handle(aaxConfig config)
{
   _handle_t *handle = (_handle_t *)config;
   _handle_t *rv = NULL;

   if (handle && handle->id == HANDLE_ID)
   {
      assert(handle->info);
      if (handle->info->mode != AAX_MODE_READ) {
         rv = handle;
      }
   }

   return rv;
}

_handle_t*
get_read_handle(aaxConfig config)
{
   _handle_t *handle = (_handle_t *)config;
   _handle_t *rv = NULL;

   if (handle && handle->id == HANDLE_ID)
   {
      assert(handle->info);
      if (handle->info->mode == AAX_MODE_READ) {
         rv = handle;
      }
   }

   return rv;
}

static _handle_t*
_open_handle(aaxConfig config)
{
   _handle_t *handle = (_handle_t *)config;

   if (handle && handle->id == HANDLE_ID)
   {
      assert (handle->backend.ptr != NULL);

      if (handle->sensors == NULL)
      {
         unsigned int res = _intBufCreate(&handle->sensors, _AAX_SENSOR);
         if (res != UINT_MAX)
         {
            unsigned long size;
            char* ptr2;
            void* ptr1;

            size = sizeof(_sensor_t) + sizeof(_aaxAudioFrame);
            ptr2 = (char*)size;

            size += sizeof(_aax2dProps);
            ptr1 = _aax_calloc(&ptr2, 1, size);

            if (ptr1)
            {
               _sensor_t* sensor = ptr1;
               _aaxAudioFrame* smixer;

               sensor->filter = handle->filter;
               _aaxSetDefaultEqualizer(_aaxMixerDefaultEqualizer);

               size = sizeof(_sensor_t);
               smixer = (_aaxAudioFrame*)((char*)sensor + size);

               sensor->mixer = smixer;
               sensor->mixer->info = handle->info;

               assert(((long int)ptr2 & 0xF) == 0);

               smixer->props2d = (_aax2dProps*)ptr2;
               _aaxSetDefault2dProps(smixer->props2d);
               _EFFECT_SET2D(smixer,PITCH_EFFECT,AAX_PITCH,handle->info->pitch);

               smixer->props3d = _aax3dPropsCreate();
               if (smixer->props3d)
               {
                  smixer->props3d->dprops3d->velocity[VELOCITY][3] = 0.0f;
                  _EFFECT_SETD3D_DATA(smixer, VELOCITY_EFFECT,
                                            _aaxRingBufferDopplerFn[0]);
                  _FILTER_SETD3D_DATA(smixer, DISTANCE_FILTER,
                          _aaxRingBufferDistanceFn[AAX_EXPONENTIAL_DISTANCE]);
               }

               res = _intBufCreate(&smixer->emitters_3d, _AAX_EMITTER);
               if (res != UINT_MAX) {
                  res = _intBufCreate(&smixer->emitters_2d, _AAX_EMITTER);
               }
               if (res != UINT_MAX) {
                  res = _intBufCreate(&smixer->play_ringbuffers, _AAX_RINGBUFFER);
               }
               if (res != UINT_MAX)
               {
                  res = _intBufAddData(handle->sensors,_AAX_SENSOR, sensor);
                  if (res != UINT_MAX)
                  {
                     unsigned int num;

                     sensor->count = 1;
#if THREADED_FRAMES
                     sensor->mixer->thread = -1;
#else
                     sensor->mixer->thread = 0;
#endif
                     num = _aaxGetNoEmitters();
                     sensor->mixer->info->max_emitters = num;
                     num = _AAX_MAX_MIXER_REGISTERED;
                     sensor->mixer->info->max_registered = num;

                     _PROP_PITCH_SET_CHANGED(smixer->props3d);
                     _PROP_MTX_SET_CHANGED(smixer->props3d);

                     _aaxSignalInit(&handle->buffer_ready);

                     return handle;
                  }
                  _intBufErase(&smixer->play_ringbuffers, _AAX_RINGBUFFER, free);
               }
               /* creating the sensor failed */
               free(ptr1);
            }
            _intBufErase(&handle->sensors, _AAX_SENSOR, _aaxFreeSensor);
         }
      }
   }
   return NULL;
}

_aaxConfig*
_aaxReadConfig(_handle_t *handle, const char *devname, int mode)
{
   _aaxConfig* config = calloc(1, sizeof(_aaxConfig));

   if (config)
   {
      _intBufferData *dptr;
      long tract_now;
      char *path, *name;
      void *xid, *be;
      float fq, iv;
      int key;

      /* read the default setup */
      key = AAX_TRUE;
      tract_now = _aaxDriverBackendSetConfigSettings(handle->backends,
                                                     handle->devname, config);
      if (!_tvnow) _tvnow = tract_now;

      /* read the system wide configuration file */
      path = systemConfigFile();
      if (path)
      {
         xid = xmlOpen(path);
         if (xid != NULL)
         {
            int m = (mode > 0) ? 1 : 0;        
            key = _aaxCheckKeyValidity(xid);
            _aaxDriverBackendReadConfigSettings(xid, handle->devname, config,
                                                path, m);
            xmlClose(xid);
         }
         free(path);
      }

      /* read the user configurstion file */
      path = userConfigFile();
      if (path)
      {
         xid = xmlOpen(path);
         if (xid)
         {
            int m = (mode > 0) ? 1 : 0;
            int res = _aaxCheckKeyValidity(xid);
            if ((key == AAX_TRUE) && res) key = res;
            _aaxDriverBackendReadConfigSettings(xid, handle->devname,
                                                config, path, m);
            xmlClose(xid);
         }
         free(path);
      }

      /*
       * must be after reading aax's own configuration file to be abke to
       * invalidate the key for other products
       */
      name = _aaxGetEnv("EKYAXA23VBDOLANI");
      if (name) 
      {
         key = _aaxCheckKeyValidityStr(name);
         _aaxUnsetEnv("EKYAXA23VBDOLANI");
      }

      if (key)
      {
         char *ptr;

         name = handle->devname[0];
         if (name == _aax_default_devname)
         {
            if (devname) {
               name = strdup((char *)devname);
            }
            else if (config->node[0].devname) {
               name = strdup(config->node[0].devname);
            }

            handle->devname[0] = name;
            ptr = strstr(name, " on ");
            if (ptr)
            {
               *ptr = 0;
               handle->devname[1] = ptr+4;		/* 4 = strlen(" on ") */
            }
         }

         be = _aaxGetDriverBackendByName(handle->backends, name,
                                         &handle->be_pos);
         if (be || (handle->devname[0] != _aax_default_devname)) {
            handle->backend.ptr = be;
         }
         handle->backend.driver = _aax_strdup(config->backend.driver);

         key ^= 0x21051974;
         if (config->node[0].no_emitters)
         {
            unsigned int emitters = config->node[0].no_emitters;
            unsigned int system_max = _aaxGetNoEmitters();

            handle->info->max_emitters = _MINMAX(emitters, 4, system_max);
            _aaxSetNoEmitters(handle->info->max_emitters);
         }
         else {
            handle->info->max_emitters = _aaxGetNoEmitters();
         }

         ptr = config->node[0].setup;
         if (ptr && handle->info->mode == AAX_MODE_WRITE_STEREO)
         {
            if (!strcasecmp(ptr, "surround")) {
               handle->info->mode = AAX_MODE_WRITE_SURROUND;
            } else if (!strcasecmp(ptr, "hrtf")) {
               handle->info->mode = AAX_MODE_WRITE_HRTF;
            } else if (!strcasecmp(ptr, "spatial")) {
               handle->info->mode = AAX_MODE_WRITE_SPATIAL;
            } else if (!strcasecmp(ptr, "stereo")) {
               handle->info->mode = AAX_MODE_WRITE_STEREO;
            }
         }

         key = _bswap32(key);
         if (config->node[0].no_speakers > 0) {
            handle->info->no_tracks = config->node[0].no_speakers;
         }

         if (config->node[0].bitrate >= 64 && config->node[0].bitrate <= 320) {
            handle->info->bitrate = config->node[0].bitrate;
         }

         fq = config->node[0].frequency;
         iv = config->node[0].interval;

         /* place config info in the syslog, if enabled */
         /* extra validiy time check */
         /* first generated key was at time(NULL) = 1315324133 */
         if (1315324133 <= key && key < _tvnow)
         {
            if (fq < _AAX_MIN_MIXER_FREQUENCY) {
               fq = _AAX_MIN_MIXER_FREQUENCY;
            }
            if (fq > _AAX_MAX_MIXER_FREQUENCY) {
               fq = _AAX_MAX_MIXER_FREQUENCY;
            }
            if (iv < _AAX_MIN_MIXER_REFRESH_RATE) {
               iv = _AAX_MIN_MIXER_REFRESH_RATE;
            }
            if (iv > _AAX_MAX_MIXER_REFRESH_RATE) {
               iv = _AAX_MAX_MIXER_REFRESH_RATE;
            }
            iv = fq / (float)get_pow2((unsigned)ceilf(fq / iv));
            handle->info->refresh_rate = iv;
            handle->info->frequency = fq;
            if (config->node[0].update) {
               handle->info->update_rate = (uint8_t)rintf(iv/config->node[0].update);
            } else {
               handle->info->update_rate = (uint8_t)rintf(iv/50);
            }
            if (handle->info->update_rate < 1) {
               handle->info->update_rate = 1;
            }

            /* key is valid */
            handle->valid = HANDLE_ID;
         } 
         else
         {
            if (fq < _AAX_MIN_MIXER_FREQUENCY) {
               fq = _AAX_MIN_MIXER_FREQUENCY;
            }
            if (fq > _AAX_MAX_MIXER_FREQUENCY_LT) {
               fq = _AAX_MAX_MIXER_FREQUENCY_LT;
            }
            if (iv < _AAX_MIN_MIXER_REFRESH_RATE) {
               iv = _AAX_MIN_MIXER_REFRESH_RATE;
            }
            if (iv > _AAX_MAX_MIXER_REFRESH_RATE_LT) {
               iv = _AAX_MAX_MIXER_REFRESH_RATE_LT;
            }
            iv = fq / (float)get_pow2((unsigned)ceilf(fq / iv));
            handle->info->refresh_rate = iv;
            handle->info->frequency = fq;
            if (config->node[0].update) {
               handle->info->update_rate = (uint8_t)rintf(iv/config->node[0].update);
            } else {
               handle->info->update_rate = (uint8_t)rintf(iv/50);
            }
            if (handle->info->update_rate < 1) {
               handle->info->update_rate = 1;
            }

            if (handle->info->max_emitters > _AAX_MAX_MIXER_REGISTERED_LT) {
                handle->info->max_emitters =  _AAX_MAX_MIXER_REGISTERED_LT;
            }
            handle->info->max_registered = _AAX_MAX_MIXER_REGISTERED_LT;
            _aaxSetNoEmitters(handle->info->max_emitters);

            handle->valid = LITE_HANDLE_ID;
         }

         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxMixerInfo* info = sensor->mixer->info;
            unsigned int size;

            size = _AAX_MAX_SPEAKERS * sizeof(vec4_t);
            if (handle->info->mode == AAX_MODE_WRITE_HRTF)
            {
               handle->info->no_tracks = 2;
               _aax_memcpy(&info->speaker,&_aaxContextDefaultHRTFVolume, size);
               _aax_memcpy(info->delay, &_aaxContextDefaultHRTFDelay, size);

               /*
                * By mulitplying the delays with the sample frequency the delays
                * in seconds get converted into sample offsets.
                */
               _aaxContextSetupHRTF(config->node[0].hrtf, 0);
               vec4Copy(info->hrtf[0], _aaxContextDefaultHead[0]);
               vec4ScalarMul(info->hrtf[0], fq);

               vec4Copy(info->hrtf[1], _aaxContextDefaultHead[1]);
               vec4ScalarMul(info->hrtf[1], fq);
            }
            else
            {
               int t;

               _aaxContextSetupSpeakers(config->node[0].speaker,info->no_tracks);
               for (t=0; t<handle->info->no_tracks; t++)
               {
                  float gain = vec3Normalize(info->speaker[t],
                                           _aaxContextDefaultSpeakersVolume[t]);
                  info->speaker[t][3] = 1.0f/gain;
               }
               _aax_memcpy(info->delay, &_aaxContextDefaultSpeakersDelay, size);
            }
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }

         do
         {
            char buffer[1024], *buf = (char *)&buffer;
            _intBuffers* backends = get_backends();
            unsigned int i, count;

            for (i=0; i<config->no_nodes; i++)
            {
               snprintf(buf,1024,"config file; settings:");
               _AAX_SYSLOG(buf);

               snprintf(buf,1024,"  output[%i]: '%s'\n", i, config->node[i].devname);
              _AAX_SYSLOG(buf);

               snprintf(buf,1024,"  setup: %s\n", (handle->info->mode == AAX_MODE_READ) ? "capture" : config->node[i].setup);
               _AAX_SYSLOG(buf);

               snprintf(buf,1024,"  frequency: %5.1f, interval: %5.1f\n",
                        handle->info->frequency, handle->info->refresh_rate);
               _AAX_SYSLOG(buf);
            }

            count = _intBufGetNumNoLock(backends, _AAX_BACKEND);
            for (i=0; i<count; i++)
            {
               _aaxDriverBackend *be = _aaxGetDriverBackendByPos(backends, i);
               if (be)
               {
                  snprintf(buf,1024,"  backend[%i]: '%s'\n", i, be->driver);
                  _AAX_SYSLOG(buf);
               }
            }
         }
         while (0);
      }
      else
      {
         free(config);
         config = NULL;
      }
   }

   return config;
}

static void
_aaxContextSetupHRTF(void *xid, unsigned int n)
{
   if (xid)
   {
      float f = (float)xmlNodeGetDouble(xid, "gain");
      _aaxContextDefaultHead[HRTF_FACTOR][GAIN] = f;

      f = (float)xmlNodeGetDouble(xid, "side-delay-sec");
      _aaxContextDefaultHead[HRTF_FACTOR][DIR_RIGHT] = f;

      f = (float)xmlNodeGetDouble(xid, "side-offset-sec");
      _aaxContextDefaultHead[HRTF_OFFSET][DIR_RIGHT] = f;

      f = (float)xmlNodeGetDouble(xid, "up-delay-sec");
      _aaxContextDefaultHead[HRTF_FACTOR][DIR_UPWD] = f;

      f = (float)xmlNodeGetDouble(xid, "up-offset-sec");
      _aaxContextDefaultHead[HRTF_OFFSET][DIR_UPWD] = f;

      f = (float)xmlNodeGetDouble(xid, "forward-delay-sec");
      _aaxContextDefaultHead[HRTF_FACTOR][DIR_BACK] = f;

      f = (float)xmlNodeGetDouble(xid, "forward-offset-sec");
      _aaxContextDefaultHead[HRTF_OFFSET][DIR_BACK] = f;
   }
}

static void
_aaxContextSetupSpeakers(void **speaker, unsigned int n)
{
   unsigned int i;

   for (i=0; i<n; i++)
   {
      void *xsid = speaker[i];

      if (xsid)
      {
         unsigned int channel;
         float f;
         vec3_t v;

         channel = xmlNodeGetInt(xsid, "channel");
         if (channel >= n) channel = n-1;
         _aaxContextDefaultRouter[i] = channel;

         f = (float)xmlNodeGetDouble(xsid, "volume-norm");
         if (f) {
            _aaxContextDefaultSpeakersVolume[channel][GAIN] = f;
         } else {
            _aaxContextDefaultSpeakersVolume[channel][GAIN] = 1.0f;
         }

         v[0] = (float)xmlNodeGetDouble(xsid, "pos-x");
         v[1] = -(float)xmlNodeGetDouble(xsid, "pos-y");
         v[2] = (float)xmlNodeGetDouble(xsid, "pos-z");
         vec3Copy(_aaxContextDefaultSpeakersVolume[channel], v);
      }
   }
}


static int
_aaxCheckKeyValidityStr(char *keystr)
{
   int rv = 0;
   if (keystr && (strlen(keystr) == 26))
   {
      union { uint64_t ll; uint32_t i[2]; } tmp;
      int base = strlen(keystr)+1;   /* 27 */
      char *nptr, *eptr;
      uint64_t key;

      nptr = keystr;
      eptr = strchr(nptr, '-');
      key = strtoll(nptr, &eptr, base);

      nptr = eptr+1;
      eptr = strchr(nptr, '-');
      tmp.ll = strtoll(nptr, &eptr, base);
      if (is_bigendian()) {
         key += tmp.i[1]; // *((uint32_t*)(&tmp.ll)+1);
      } else {
         key += tmp.i[0]; // *((uint32_t*)&tmp.ll);
      }
#if 0
// printf("tmp: %llx\n", tmp);
// printf("*(uint32_t*)(&tmp): %x\n((uint32_t*)(&tmp)+1): %x\n", *(uint32_t*)(&tmp), *((uint32_t*)(&tmp)+1));
#endif

      nptr = eptr+1;
      eptr = nptr+strlen(nptr);
      key -= strtoll(nptr, &eptr, base);
      if (((key^HANDLE_ID) % 29723) == (7*strlen(keystr)-5)) {	/* 177 */
         rv = tmp.ll & 0xFFFFFFFF;
      }
   }
   else {
      rv = 0xdeadbeef;
   }
   return rv;
}

static int
_aaxCheckKeyValidity(void *xid)
{
   void *xcid = xmlNodeGet(xid, "/configuration");
   int rv = AAX_FALSE;

   if (xcid)
   {
      char keystr[27];
      xmlNodeCopyString(xcid, "product-key", keystr, 27);
      rv = _aaxCheckKeyValidityStr(keystr);
      xmlFree(xcid);
   }
   return rv;
}

static void
_aaxFreeSensor(void *ssr)
{
   _sensor_t *sensor = (_sensor_t*)ssr;
   _aaxRingBufferDelayEffectData* effect;
   _aaxAudioFrame* smixer = sensor->mixer;

   /* frees both EQUALIZER_LF and EQUALIZER_HF */
   free(sensor->filter[EQUALIZER_LF].data);

   free(_FILTER_GET2D_DATA(smixer, FREQUENCY_FILTER));
   free(_FILTER_GET2D_DATA(smixer, DYNAMIC_GAIN_FILTER));
   free(_FILTER_GET2D_DATA(smixer, TIMED_GAIN_FILTER));
   free(_EFFECT_GET2D_DATA(smixer, DYNAMIC_PITCH_EFFECT));

   effect = _EFFECT_GET2D_DATA(smixer, DELAY_EFFECT);
   if (effect) free(effect->history_ptr);
   free(effect);

   _intBufErase(&smixer->p3dq, _AAX_DELAYED3D, _aax_aligned_free);
   _aax_aligned_free(smixer->props3d->dprops3d);
   free(smixer->props3d);

   if (smixer->ringbuffer) {
      _aaxRingBufferFree(smixer->ringbuffer);
   }
   _intBufErase(&smixer->frames, _AAX_FRAME, free);
   _intBufErase(&smixer->devices, _AAX_DEVICE, free);
   _intBufErase(&smixer->emitters_2d, _AAX_EMITTER, free);
   _intBufErase(&smixer->emitters_3d, _AAX_EMITTER, free);
   _intBufErase(&smixer->play_ringbuffers, _AAX_RINGBUFFER,
                                           _aaxRingBufferFree);
   _intBufErase(&smixer->frame_ringbuffers, _AAX_RINGBUFFER,
                                            _aaxRingBufferFree);
   free(sensor);
}
