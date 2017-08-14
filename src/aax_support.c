/*
 * Copyright 2007-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#if HAVE_STRINGS_H
# include <strings.h>	/* strcasecmp */
#endif

#include <aax/aax.h>

#include <dsp/filters.h>
#include <dsp/effects.h>

#include "api.h"
#include "copyright.h"
#include "arch.h"
#include "software/audio.h"

#define _AAX_MAX_ERROR		12

typedef struct {
   char supported_lite;
   const char *name;
} ef_type; 

const char *_aax_id_s[_AAX_MAX_ID];
const char *_aaxErrorStrings[_AAX_MAX_ERROR];

static const char* __aaxErrorSetFunctionOrBackendString(const char*);

AAX_API void AAX_APIENTRY
aaxFree(void *mm)
{
   free(mm);
}

AAX_API unsigned AAX_APIENTRY
aaxGetMajorVersion()
{
   return AAX_MAJOR_VERSION;
}

AAX_API unsigned AAX_APIENTRY
aaxGetMinorVersion()
{
   return AAX_MINOR_VERSION;
}

AAX_API unsigned int AAX_APIENTRY
aaxGetPatchLevel()
{
   return AAX_PATCH_LEVEL;
}

AAX_API const char* AAX_APIENTRY
aaxGetCopyrightString()
{
   return (const char*)COPYING;
}

AAX_API const char* AAX_APIENTRY
aaxGetVersionString(aaxConfig cfg)
{
   _handle_t *handle = (_handle_t*)cfg;
   static const char* _version;
 
   if (handle && VALID_HANDLE(handle)) {
      _version = AAX_LIBRARY_STR" "AAX_VERSION_STR;
   } else {
      _version = AAX_LIBRARY_STR_LT" "AAX_VERSION_STR;
   }
   return _version;
}

AAX_API enum aaxFilterType AAX_APIENTRY
aaxMaxFilter(void)
{
   return AAX_FILTER_MAX;
}

AAX_API enum aaxEffectType AAX_APIENTRY
aaxMaxEffect(void)
{
   return AAX_EFFECT_MAX;
}

AAX_API int AAX_APIENTRY
aaxIsFilterSupported(aaxConfig cfg, const char *filter)
{
   _handle_t* handle = (_handle_t*)cfg;
   int rv = AAX_FALSE;
   if (handle)
   {
      if (filter)
      {
         int i;
         for(i=0; i<AAX_FILTER_MAX-1; i++)
         {
            char *p = strchr(filter, '.');
            while (p > filter && *p != '_') --p;
            if (p && !strncasecmp(filter, _aaxFilters[i]->name, p-filter))
            {
               float version = atof(p+1);
               float v = _aaxFilters[i]->version - version;
               if (v >= 0 && v < 1)
               {
                  if (_aaxFilters[i]->lite || VALID_HANDLE(handle)) {
                     rv = AAX_TRUE;
                  }
               }
               break;
            }
            else if (!strncasecmp(filter, _aaxFilters[i]->name, strlen(filter)))
            {
               if (_aaxFilters[i]->lite || VALID_HANDLE(handle)) {
                  rv = AAX_TRUE;
               }
               break;
            }
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxIsEffectSupported(aaxConfig cfg, const char *effect)
{
   _handle_t* handle = (_handle_t*)cfg;
   int rv = AAX_FALSE;
   if (handle)
   {
      if (effect)
      {
         int i;
         for(i=0; i<AAX_EFFECT_MAX-1; i++)
         {
            char *p = strchr(effect, '.');
            while (p > effect && *p != '_') --p;
            if (p && !strncasecmp(effect, _aaxEffects[i]->name, p-effect))
            {
               float version = atof(p+1);
               float v = version - _aaxEffects[i]->version;
               if (v >= 0 && v < 1)
               {
                  if (_aaxEffects[i]->lite || VALID_HANDLE(handle)) {
                     rv = AAX_TRUE;
                  }  
               }  
               break;
            }  
            else if (!strncasecmp(effect, _aaxEffects[i]->name, strlen(effect)))
            {
               if (_aaxEffects[i]->lite || VALID_HANDLE(handle)) {
                  rv = AAX_TRUE;
               }
               break;
            }
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API enum aaxErrorType AAX_APIENTRY
aaxGetErrorNo()
{
   return __aaxErrorSet(AAX_ERROR_NONE, NULL);
}

AAX_API const char* AAX_APIENTRY
aaxGetErrorString(enum aaxErrorType err)
{
   static char str[255] = "None";
   const char* rv = (const char*)&str;

   if (err != AAX_ERROR_NONE)
   {
      if (err != AAX_BACKEND_ERROR)
      {
         int param = err & 0xF;
         int pos = err >> 4;
         if ((err >= AAX_INVALID_DEVICE) && (err < AAX_ERROR_MAX))
         {
            if (((err & ~0xF) == AAX_INVALID_PARAMETER) && param) {
               snprintf(str, 255, "%s: %s for parameter %i",
                                     __aaxErrorSetFunctionOrBackendString(NULL),
                                     _aaxErrorStrings[pos], param);
            } else {
               snprintf(str, 255, "%s: %s",
                                     __aaxErrorSetFunctionOrBackendString(NULL),
                                     _aaxErrorStrings[pos]);
            }
         }
         else {
            rv = "AAX: Unknown error condition";
         }
      }
      else { /* err != AAX_BACKEND_ERROR */
         rv = __aaxErrorSetFunctionOrBackendString(NULL);
      }
   }
   else {
      snprintf(str, 255, "%s: %s", __aaxErrorSetFunctionOrBackendString(NULL),
                                   _aaxErrorStrings[0]);
   }
   return rv;
}

AAX_API unsigned AAX_APIENTRY
aaxGetBytesPerSample(enum aaxFormat format)
{
   unsigned int native_fmt = format & AAX_FORMAT_NATIVE;
   unsigned rv = 0;

   switch(native_fmt)
   {
   case AAX_PCM8S:
   case AAX_MULAW:
   case AAX_ALAW:
      rv = 1;
      break;
   case AAX_PCM16S:
   case AAX_IMA4_ADPCM:		/* gets decoded before use */
      rv = 2;
      break;
   case AAX_PCM24S:
   case AAX_PCM32S:
   case AAX_FLOAT:
      rv = 4;
      break;
   case AAX_DOUBLE:
      rv = 8;
      break;
   default:
      __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
   }

   return rv;
}

AAX_API unsigned AAX_APIENTRY
aaxGetBitsPerSample(enum aaxFormat format)
{
   unsigned rv = 0;

   switch (format & AAX_FORMAT_NATIVE)
   {
   case AAX_PCM8S:
   case AAX_MULAW:
   case AAX_ALAW:
      rv = 8;
      break;
   case AAX_PCM16S:
      rv = 16;
      break;
   case AAX_PCM24S:
   case AAX_PCM32S:
   case AAX_FLOAT:
      rv = 32;
      break;
   case AAX_DOUBLE:
      rv = 64;
      break;
   case AAX_IMA4_ADPCM:
      rv = 4;
      break;
   default:
      break;
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxIsValid(const void* handle, enum aaxHandleType type)
{
   enum aaxErrorType rv = AAX_FALSE;
   if (handle)
   {
      switch(type)
      {
      case AAX_CONFIG:
      {
         const _handle_t* ptr = (const _handle_t*)handle;
         if (ptr->id == HANDLE_ID) rv = AAX_TRUE;
         break;
      }
      case AAX_CONFIG_HD:
      {
          const _handle_t* ptr = (const _handle_t*)handle;
          if (ptr->id == HANDLE_ID && VALID_HANDLE(ptr)) rv = AAX_TRUE;
         break;
      }
      case AAX_BUFFER:
      {  
         const _buffer_t* ptr = (const _buffer_t*)handle;
         if (ptr->id == BUFFER_ID) rv = AAX_TRUE;
         break;
      }
      case AAX_EMITTER:
      {  
         const _emitter_t* ptr = (const _emitter_t*)handle;
         if (ptr->id == EMITTER_ID) rv = AAX_TRUE;
         break;
      }
      case AAX_AUDIOFRAME:
      {  
         const _frame_t* ptr = (const _frame_t*)handle;
         if (ptr->id == AUDIOFRAME_ID) rv = AAX_TRUE;
         break;
      }
      case AAX_FILTER:
      {
         const _filter_t* ptr = (const _filter_t*)handle;
         if (ptr->id == FILTER_ID) rv = AAX_TRUE;
         break;
      }
      case AAX_EFFECT:
      {
         const _filter_t* ptr = (const _filter_t*)handle;
         if (ptr->id == EFFECT_ID) rv = AAX_TRUE;
         break;
      }
      default:
         __aaxErrorSet(AAX_INVALID_ENUM, __func__);
      }
   }
   return rv;
}

AAX_API unsigned AAX_APIENTRY
aaxGetNoCores(aaxConfig cfg)
{
   unsigned rv = 1;

   if (aaxIsValid(cfg, AAX_CONFIG_HD))
   {
      _handle_t* handle = (_handle_t*)cfg;
      rv = handle->info->no_cores;
   }

   return rv;
}

AAX_API enum aaxType AAX_APIENTRY
aaxGetTypeByName(const char *name)
{
   enum aaxType rv = AAX_TYPE_NONE;
   if (name)
   {
      if (!strncmp(name, "AAX_", 4)) {
         name += 4;
      }

      if (!strcasecmp(name, "log") || !strcasecmp(name, "db") ||
          !strcasecmp(name, "logarithmic")) { 
         rv = AAX_LOGARITHMIC;
      } else if (!strcasecmp(name, "rad") || !strcasecmp(name, "radians")) {
         rv = AAX_RADIANS;
      } else if (!strcasecmp(name, "deg") || !strcasecmp(name, "degrees")) {
         rv = AAX_DEGREES;
      } else if (!strcasecmp(name, "bytes")) {
         rv = AAX_BYTES;
      } else if (!strcasecmp(name, "frames")) {
         rv = AAX_FRAMES;
      } else if (!strcasecmp(name, "samples")) {
         rv = AAX_SAMPLES;
      } else if (!strcasecmp(name, "usec") || !strcasecmp(name, "μsec") ||
                 !strcasecmp(name, "microseconds")) {
         rv = AAX_MICROSECONDS;
      } else {
         rv = AAX_LINEAR;
      } 
   }
   return rv;
}

AAX_API enum aaxWaveformType AAX_APIENTRY
aaxGetWaveformTypeByName(const char *name)
{
   enum aaxWaveformType rv = AAX_WAVE_NONE;
   if (name)
   {
      size_t len;
      char *end;

      if (!strncmp(name, "AAX_", 4)) {
         name += 4;
      }
      end = strrchr(name, '_');
      if (end)
      {
         if (!strcmp(end, "_VALUE") || !strcmp(end, "_WAVE") ||
             !strcmp(end, "_FOLLOW")) {
            len = end-name;
         }
         else {
            len = strlen(name);
         }
      } else {
         len = strlen(name);
      }

      if (!strncasecmp(name, "triangle", len)) {
         rv = AAX_TRIANGLE_WAVE;
      } else if (!strncasecmp(name, "sine", len)) {
         rv = AAX_SINE_WAVE;
      } else if (!strncasecmp(name, "square", len)) {
         rv = AAX_SQUARE_WAVE;
      } else if (!strncasecmp(name, "sawtooth", len)) {
         rv = AAX_SAWTOOTH_WAVE;
      } else if (!strncasecmp(name, "envelope", len)) {
         rv = AAX_ENVELOPE_FOLLOW;
      }
      else if (!strncasecmp(name, "inverse_triangle", len)) {
         rv = AAX_INVERSE_TRIANGLE_WAVE;
      } else if (!strncasecmp(name, "inverse_sine", len)) {
         rv = AAX_INVERSE_SINE_WAVE;
      } else if (!strncasecmp(name, "inverse_square", len)) {
         rv = AAX_INVERSE_SQUARE_WAVE;
      } else if (!strncasecmp(name, "inverse_sawtooth", len)) {
         rv = AAX_INVERSE_SAWTOOTH_WAVE;
      } else if (!strncasecmp(name, "inverse_envelope", len)) {
         rv = AAX_INVERSE_ENVELOPE_FOLLOW;
      }
      else {
         rv = AAX_CONSTANT_VALUE;
      }
   }
   return rv;
}


/* -------------------------------------------------------------------------- */

const char *_aax_id_s[_AAX_MAX_ID] =
{
   "NONE",
   "_AAX_BACKEND",
   "_AAX_DEVICE",
   "_AAX_BUFFER",
   "_AAX_EMITTER",
   "_AAX_EMITTER_BUFFER",
   "_AAX_SENSOR",
   "_AAX_FRAME",
   "_AAX_RINGBUFFER",
   "_AAX_EXTENSION"
};

const char* _aaxErrorStrings[_AAX_MAX_ERROR] =
{
   "No Error",
   "Invalid device",
   "Invalid device configuration",
   "Invalid or NULL handle type",
   "Invalid internal state",
   "Invalid enumerated type",
   "Invalid parameter value",
   "Invalid type reference",
   "Insufficient resources",
   "Event timed out",
   "Backend error",
   "Handle was already destroyed"
};

static const char*
__aaxErrorSetFunctionOrBackendString(const char* fnname)
{
   static const char* __error_fn = "AAX";
   const char* rv = __error_fn;
   if (fnname) {
      __error_fn = fnname;
   }
   return rv;
}

enum aaxErrorType
__aaxErrorSet(enum aaxErrorType err, const char* fnname)
{
   static enum aaxErrorType _err = AAX_ERROR_NONE;
   enum aaxErrorType rv = _err;

   if (err == AAX_ERROR_NONE && _err == AAX_ERROR_NONE) {
      __aaxErrorSetFunctionOrBackendString("AAX");
   } else if (err != AAX_ERROR_NONE) {
      __aaxErrorSetFunctionOrBackendString(fnname);
   }
   _err = err;

   return rv;
}

