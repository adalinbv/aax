/*
 * Copyright 2007-2023 by Erik Hofman.
 * Copyright 2009-2023 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_STRINGS_H
# include <strings.h>	/* strcasecmp */
#endif

#include <aax/aax.h>

#include <base/memory.h>

#include <dsp/filters.h>
#include <dsp/effects.h>

#include "api.h"
#include "copyright.h"
#include "sound_logo.h"
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

/* deprecated functions */

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
   return (const char*)COPYING_v3;
}

AAX_API const char* AAX_APIENTRY
aaxGetVersionString(UNUSED(aaxConfig cfg))
{
   return AAX_LIBRARY_STR" "AAX_VERSION_STR;
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

/* end of deprecated functions list */

#if AAX_MAJOR_VERSION > 3
// Windows DLL API changes would break backwards compatibility
AAX_API const char* AAX_APIENTRY
aaxGetString(enum aaxSetupType type)
{
   const char *rv = NULL;

   switch(type)
   {
   case AAX_NAME_STRING:
      rv = AAX_LIBRARY_STR;
      break;
   case AAX_VERSION_STRING:
      rv = AAX_VERSION_STR;
      break;
   case AAX_VENDOR_STRING:
      rv = AAX_VENDOR_STR;
      break;
   case AAX_RENDERER_STRING:
      rv = AAX_LIBRARY_STR" "AAX_VERSION_STR;
      break;
   case AAX_COPYRIGHT_STRING:
      rv = (const char*)COPYING_v3;
      break;
   case AAX_WEBSITE_STRING:
      rv = AAX_WEBSITE_STR;
      break;
   case AAX_SHARED_DATA_DIR:
      rv = APP_DATA_DIR;
      break;
   default:
      break;
   }

   return rv;
}

AAX_API unsigned int AAX_APIENTRY
aaxGetByType(enum aaxSetupType type)
{
   unsigned int rv = 0;

   switch (type)
   {
   case AAX_VERSION_MAJOR:
      rv = AAX_MAJOR_VERSION;
      break;
   case AAX_VERSION_MINOR:
      rv = AAX_MINOR_VERSION;
      break;
    case AAX_VERSION_MICRO:
      rv = AAX_MICRO_VERSION;
      break;
   case AAX_RELEASE_NUMBER:
      rv = AAX_PATCH_LEVEL;
      break;
   case AAX_ERROR_NUMBER:
      rv = __aaxErrorSet(AAX_ERROR_NONE, NULL);
      break;
   case AAX_MAX_FILTER:
      rv = AAX_FILTER_MAX;
      break;
   case AAX_MAX_EFFECT:
      rv = AAX_EFFECT_MAX;
      break;
   default:
      break;
   }

   return rv;
}

AAX_API const char* AAX_APIENTRY
aaxGetFormatString(enum aaxFormat format)
{
   static const char* _format_s[AAX_FORMAT_MAX] = {
      "signed, 8-bits per sample",
      "signed, 16-bits per sample",
      "signed, 24-bits per sample, 32-bit encoded",
      "signed, 32-bits per sample",
      "32-bit floating point, range: -1.0 to 1.0",
      "64-bit floating point, range: -1.0 to 1.0",
      "mulaw, 16-bit with 2:1 compression",
      "alaw, 16-bit with 2:1 compression",
      "IMA4 ADPCM, 16-bit with 4:1 compression",
      "signed, 24-bits per sample, 24-bit encoded"
   };
   static const char* _format_us[] = {
      "unsigned, 8-bits per sample",
      "unsigned, 16-bits per sample",
      "unsigned, 24-bits per sample, 32-bit encoded",
      "unsigned, 32-bits per sample"
   };
   int pos = format & AAX_FORMAT_NATIVE;
   const char *rv = "";

   if (pos < AAX_FORMAT_MAX)
   {
      if (format & AAX_FORMAT_UNSIGNED && pos <= AAX_PCM32S) {
         rv = _format_us[pos];
      } else {
         rv = _format_s[pos];
      }
   }

   return rv;
}
#endif

int AAX_APIENTRY
aaxPlaySoundLogo(const char *devname)
{
   int rv = AAX_FALSE;
   aaxConfig config;

   config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_SPATIAL);
   if(config)
   {
       aaxBuffer buffer = NULL;
       unsigned int frequency;

       rv = aaxMixerSetState(config, AAX_INITIALIZED);
       if (rv) rv = aaxMixerSetState(config, AAX_PLAYING);

       frequency = aaxMixerGetSetup(config, AAX_FREQUENCY);
       if (rv) buffer = aaxBufferCreate(config, frequency, 1, AAX_AAXS24S);
       if (buffer)
       {
           rv = aaxBufferSetSetup(buffer, AAX_FREQUENCY, frequency);
           if (rv) rv = aaxBufferSetSetup(buffer, AAX_BLOCK_ALIGNMENT, 1);
           if (aaxBufferSetData(buffer, __aax_sound_logo) == AAX_FALSE)
           {
              rv = aaxBufferDestroy(buffer);
              buffer = NULL;
           }
       }

       if (buffer)
       {
           aaxEmitter emitter = NULL;
           aaxFrame frame;
           int state;

           frame = aaxAudioFrameCreate(config);
           if (frame)
           {
              rv = aaxMixerRegisterAudioFrame(config, frame);
              if (rv) rv = aaxAudioFrameSetState(frame, AAX_PLAYING);
              if (rv) rv = aaxAudioFrameAddBuffer(frame, buffer);

              emitter = aaxEmitterCreate();
              if (emitter)
              {
                 rv = aaxEmitterAddBuffer(emitter, buffer);
                 if (rv) rv = aaxAudioFrameRegisterEmitter(frame, emitter);
                 if (rv) rv = aaxEmitterSetState(emitter, AAX_PLAYING);
              }
           }

           if (frame && emitter)
           {
              do
              {
                  msecSleep(50);
                  state = aaxEmitterGetState(emitter);
              }
              while (state == AAX_PLAYING);

              aaxAudioFrameDeregisterEmitter(frame, emitter);
           }
           if (frame)
            {
              aaxMixerDeregisterAudioFrame(config, frame);
              aaxAudioFrameDestroy(frame);
           }
           if (emitter) aaxEmitterDestroy(emitter);
       }
       else rv = AAX_FALSE;

       aaxMixerSetState(config, AAX_STOPPED);
       aaxBufferDestroy(buffer);

       aaxDriverClose(config);
       aaxDriverDestroy(config);
   }

   return rv;
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
            if (param) {
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
   case AAX_PCM24S_PACKED:
      rv = 3;
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
   case AAX_PCM24S_PACKED:
      rv = 24;
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
aaxGetNoCores(UNUSED(aaxConfig cfg))
{
   return _aaxGetNoCores();
}

static enum aaxType
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
      } else if (!strcasecmp(name, "radians") || !strcasecmp(name, "rad")) {
         rv = AAX_RADIANS;
      } else if (!strcasecmp(name, "degrees") || !strcasecmp(name, "deg")) {
         rv = AAX_DEGREES;
      } else if (!strcasecmp(name, "bytes")) {
         rv = AAX_BYTES;
      } else if (!strcasecmp(name, "frames")) {
         rv = AAX_FRAMES;
      } else if (!strcasecmp(name, "samples")) {
         rv = AAX_SAMPLES;
      } else if (!strcasecmp(name, "bits-per-sample")
                 || !strcasecmp(name, "bps")) {
         rv = AAX_BITS_PER_SAMPLE;
      } else if (!strcasecmp(name, "seconds") || !strcasecmp(name, "sec")
                 || !strcasecmp(name, "s")) {
         rv = AAX_SECONDS;
      } else if (!strcasecmp(name, "milliseconds") || !strcasecmp(name, "msec")
                || !strcasecmp(name, "ms")) {
         rv = AAX_MILLISECONDS;
      } else if (!strcasecmp(name, "microseconds")
                 || !strcasecmp(name, "usec") || !strcasecmp(name, "μsec")
                 || !strcasecmp(name, "us") || !strcasecmp(name, "μs")) {
         rv = AAX_MICROSECONDS;
      } else if (!strcasecmp(name, "degree-celsius")
                || !strcasecmp(name, "degC")) {
         rv = AAX_DEGREES_CELSIUS;
      } else if (!strcasecmp(name, "degree-fahrenheit")
                 || !strcasecmp(name, "degF")) {
         rv = AAX_DEGREES_FAHRENHEIT;
      } else if (!strcasecmp(name, "kelvin") || !strcasecmp(name, "K")) {
         rv = AAX_KELVIN;
      } else if (!strcasecmp(name, "atmosphere") || !strcasecmp(name, "atm")) {
         rv = AAX_ATMOSPHERE;
      } else if (!strcasecmp(name, "bar")) {
         rv = AAX_BAR;
      } else if (!strcasecmp(name, "pounds-per-square-inch")
                 || !strcasecmp(name, "psi")) {
         rv = AAX_PSI;
      } else if (!strcasecmp(name, "kilopascal") || !strcasecmp(name, "kPa")) {
         rv = AAX_KPA;
      } else {
         rv = AAX_LINEAR;
      } 
   }
   return rv;
}

static enum aaxWaveformType
aaxGetWaveformTypeByName(const char *wave)
{
   enum aaxWaveformType rv = AAX_WAVE_NONE;
   if (wave)
   {
      char *name = (char *)wave;
      size_t len, invlen;
      char *end, *last;

      last = strchr(name, '|');
      if (!last) last = name+strlen(name);

      do
      {
         if (!strncasecmp(name, "AAX_", 4)) {
            name += 4;
         }

         end = last;
         while(end > name && *end != '_' && *end != '-') --end;
         if (end > name)
         {
            if (!strcasecmp(end+1, "VALUE") || !strcasecmp(end+1, "WAVE") ||
                !strcasecmp(end+1, "FOLLOW")) {
               len = end-name;
            }
            else {
               len = last-name;
            }
         } else {
            len = last-name;
         }

         if (len)
         {
            invlen = strlen("inverse");
            if (!strncasecmp(name, "inverse", invlen) &&
                (len > ++invlen))
            {
               name += invlen;
               len -= invlen;
               rv |= AAX_INVERSE;
            }

            if (!strncasecmp(name, "triangle", len)) {
               rv |= AAX_TRIANGLE_WAVE;
            } else if (!strncasecmp(name, "sine", len)) {
               rv |= AAX_SINE_WAVE;
            } else if (!strncasecmp(name, "square", len)) {
               rv |= AAX_SQUARE_WAVE;
            } else if (!strncasecmp(name, "impulse", len)) {
               rv |= AAX_IMPULSE_WAVE;
            } else if (!strncasecmp(name, "sawtooth", len)) {
               rv |= AAX_SAWTOOTH_WAVE;
            } else if (!strncasecmp(name, "random", len)) {
               rv |= AAX_RANDOM_SELECT;
            } else if (!strncasecmp(name, "randomness", len)) {
               rv |= AAX_RANDOMNESS;
            } else if (!strncasecmp(name, "cycloid", len)) {
               rv |= AAX_CYCLOID_WAVE;
            } else if (!strncasecmp(name, "white-noise", len)) {
               rv |= AAX_WHITE_NOISE;
            } else if (!strncasecmp(name, "pink-noise", len)) {
               rv |= AAX_PINK_NOISE;
            } else if (!strncasecmp(name, "brownian-noise", len)) {
               rv |= AAX_BROWNIAN_NOISE;
            } else if (!strncasecmp(name, "timed", len)) {
               rv |= AAX_TIMED_TRANSITION;
            } else if (!strncasecmp(name, "envelope", len)) {
               rv |= AAX_ENVELOPE_FOLLOW;
            } else if (!strncasecmp(name, "logarithmic", len) ||
                       !strncasecmp(name, "exponential", len) ||
                       !strncasecmp(name, "log", len) ||
                       !strncasecmp(name, "exp", len))
            {
               rv |= AAX_ENVELOPE_FOLLOW_LOG;
            } else if (!strncasecmp(name, "1st-order", len)) {
                rv |= AAX_EFFECT_1ST_ORDER;
            } else if (!strncasecmp(name, "2nd-order", len)) {
                rv |= AAX_EFFECT_2ND_ORDER;
            } else if (!strncasecmp(name, "true", len) ||
                       !strncasecmp(name, "constant", len)) {
               rv |= AAX_CONSTANT_VALUE;
            } else if (!strncasecmp(name, "inverse", len)) {
               rv |= AAX_CONSTANT_VALUE|AAX_INVERSE;
            }
         }
         else {
            rv = AAX_CONSTANT_VALUE;
         }

         if (last == name+strlen(name)) break;
         name = ++last;
         last = strchr(name, '|');
         if (!last) last = name+strlen(name);
      }
      while(last);
   }
   return rv;
}


static enum aaxDistanceModel
aaxGetDistanceModelByName(const char *name)
{
   enum aaxDistanceModel rv = AAX_DISTANCE_MODEL_NONE;
   if (name)
   {
      char type[256];
      size_t i, len;

      strlcpy(type, name, 255);
      name = type;

      len = strlen(name);
      for (i=0; i<len; ++i) {
         if (type[i] == '-') type[i] = '_';
      }

      if (!strncasecmp(name, "AAX_AL_", 7)) {
         name += 7;
      } else if (!strncasecmp(name, "AAX_", 4)) {
         name += 4;
      }
      len = strlen(name);

      if (!strncasecmp(name, "exponential_distance", len)) {
         rv = AAX_EXPONENTIAL_DISTANCE;
      } else if (!strncasecmp(name, "exponential_distance_delay", len)) {
         rv = AAX_EXPONENTIAL_DISTANCE_DELAY;
      }

      else if (!strncasecmp(name, "inverse_distance", len)) {
         rv = AAX_AL_INVERSE_DISTANCE;
      } else if (!strncasecmp(name, "inverse_distance_clamped", len)) {
         rv = AAX_AL_INVERSE_DISTANCE_CLAMPED;
      } else if (!strncasecmp(name, "linear_distance", len)) {
         rv = AAX_AL_LINEAR_DISTANCE;
      } else if (!strncasecmp(name, "linear_distance_clamped", len)) {
         rv = AAX_AL_LINEAR_DISTANCE_CLAMPED;
      } else if (!strncasecmp(name, "exponent_distance", len)) {
         rv = AAX_AL_EXPONENT_DISTANCE;
      } else if (!strncasecmp(name, "exponent_distance_clamped", len)) {
         rv = AAX_AL_EXPONENT_DISTANCE_CLAMPED;
      }
   }

   return rv;
}

static enum aaxFrequencyFilterType
aaxGetFrequencyFilterTypeByName(const char *type)
{
   int rv = aaxGetWaveformTypeByName(type);

   /* Prevent a clash with reverb 1st-order and 2nd-order */
   if (rv & AAX_EFFECT_1ST_ORDER) rv &= ~AAX_EFFECT_1ST_ORDER;
   else if (rv & AAX_EFFECT_2ND_ORDER) rv &= ~AAX_EFFECT_2ND_ORDER;

   if (type)
   {
      char *last, *name = (char*)type;
      size_t len, slen;

      slen = strlen(name);
      last = strchr(name, '|');
      if (!last) last = name+slen;

      do
      {
         if (!strncasecmp(name, "AAX_", 4)) {
            name += 4;
         }

         len = last-name;
         if (!strncasecmp(name, "bessel", len)) {
            rv |= AAX_BESSEL;
         }

         if (len >= 5 && !strncasecmp(name+len-5, "ORDER", 5))
         {
            if (len >= 6 && (*(name+len-6) == '_' || *(name+len-6) == '-')) {
               len -= 6;
            }
         }
         else if (len >= 3 && !strncasecmp(name+len-3, "OCT", 3))
         {
            if (len >= 4 && (*(name+len-4) == '_' || *(name+len-4) == '/')) {
               len -= 4;
            }
         }

         if (!strncasecmp(name, "resonance", len) ||
             !strncasecmp(name, "Q", len)) {
            rv |= AAX_RESONANCE_FACTOR;
         }
         else if (!strncasecmp(name, "1st", len) ||
                  !strncasecmp(name, "6db", len)) {
            rv |= AAX_1ST_ORDER;
         }
         else if (!strncasecmp(name, "2nd", len) ||
                  !strncasecmp(name, "12db", len)) {
            rv |= AAX_2ND_ORDER;
         }
         else if (!strncasecmp(name, "4th", len) ||
                  !strncasecmp(name, "24db", len)) {
            rv |= AAX_4TH_ORDER;
         }
         else if (!strncasecmp(name, "6th", len) ||
                  !strncasecmp(name, "36db", len)) {
            rv |= AAX_6TH_ORDER;
         }
         else if (!strncasecmp(name, "8th", len) ||
                  !strncasecmp(name, "48db", len)) {
            rv |= AAX_8TH_ORDER;
         }

         if (last == name+strlen(name)) break;
         name = ++last;
         last = strchr(name, '|');
         if (!last) last = name+strlen(name);
      }
      while(last);
   }

   return rv;
}

static enum aaxFilterType
aaxFilterGetByName(const char *name)
{
   enum aaxFilterType rv = AAX_FILTER_NONE;
   char type[256];
   int i, slen;
   char *end;

   if (!strncasecmp(name, "AAX_", 4)) {
      name += 4;
   }

   strlcpy(type, name, 256);
   name = type;

   slen = strlen(name);
   for (i=0; i<slen; ++i) {
      if (type[i] == '-') type[i] = '_';
   }

   end = strchr(name, '.');
   while (end > name && *end != '_') --end;
   if (end) type[end-name] = 0;

   end = strrchr(name, '_');
   if (end && !strcasecmp(end+1, "FILTER")) {
      type[end-name] = 0;
   }
   slen = strlen(name);

   if (!strncasecmp(name, "equalizer", slen)) {
      rv = AAX_EQUALIZER;
   }
   else if (!strncasecmp(name, "volume", slen)) {
      rv = AAX_VOLUME_FILTER;
   }
   else if (!strncasecmp(name, "dynamic_gain", slen) ||
            !strncasecmp(name, "tremolo", slen)) {
       rv = AAX_DYNAMIC_GAIN_FILTER;
   }
   else if (!strncasecmp(name, "bitcrusher", slen)) {
       rv = AAX_BITCRUSHER_FILTER;
   }
   else if (!strncasecmp(name, "timed_gain", slen) ||
            !strncasecmp(name, "envelope", slen)) {
      rv = AAX_TIMED_GAIN_FILTER;
   }
   else if (!strncasecmp(name, "frequency", slen)) {
      rv = AAX_FREQUENCY_FILTER;
   }
   else if (!strncasecmp(name, "graphic_equalizer", slen)) {
      rv = AAX_GRAPHIC_EQUALIZER;
   }
   else if (!strncasecmp(name, "compressor", slen)) {
      rv = AAX_COMPRESSOR;
   }
   else if (!strncasecmp(name, "dynamic_layer", slen)) {
       rv = AAX_DYNAMIC_LAYER_FILTER;
   }

   else if (!strncasecmp(name, "directional", slen) ||
            !strncasecmp(name, "angular", slen)) {
      rv = AAX_DIRECTIONAL_FILTER;
   }
   else if (!strncasecmp(name, "distance", slen)) {
      rv = AAX_DISTANCE_FILTER;
   }

   return rv;
}

static enum aaxEffectType
aaxEffectGetByName(const char *name)
{
   enum aaxEffectType rv = AAX_EFFECT_NONE;
   char type[256];
   int i, slen;
   char *end;

   if (!strncasecmp(name, "AAX_", 4)) {
      name += 4;
   }

   strlcpy(type, name, 256);
   name = type;

   slen = strlen(name);
   for (i=0; i<slen; ++i) {
      if (type[i] == '-') type[i] = '_';
   }

   end = strchr(name, '.');
   while (end > name && *end != '_') --end;
   if (end) type[end-name] = 0;

   end = strrchr(name, '_');
   if (end && !strcasecmp(end+1, "EFFECT")) {
      type[end-name] = 0;
   }
   slen = strlen(name);

   if (!strncasecmp(name, "pitch", slen)) {
      rv = AAX_PITCH_EFFECT;
   }
   else if (!strncasecmp(name, "dynamic_pitch", slen) ||
            !strncasecmp(name, "vibrato", slen)) {
      rv = AAX_DYNAMIC_PITCH_EFFECT;
   }
   else if (!strncasecmp(name, "timed_pitch", slen) ||
            !strncasecmp(name, "envelope", slen)) {
      rv = AAX_TIMED_PITCH_EFFECT;
   }
   else if (!strncasecmp(name, "ringmodulator", slen)) {
      rv = AAX_RINGMODULATOR_EFFECT;
   }
   else if (!strncasecmp(name, "distortion", slen)) {
      rv = AAX_DISTORTION_EFFECT;
   }
   else if (!strncasecmp(name, "phasing", slen)) {
      rv = AAX_PHASING_EFFECT;
   }
   else if (!strncasecmp(name, "chorus", slen)) {
      rv = AAX_CHORUS_EFFECT;
   }
   else if (!strncasecmp(name, "flanging", slen)) {
      rv = AAX_FLANGING_EFFECT;
   }
   else if (!strncasecmp(name, "delay", slen)) {
      rv = AAX_DELAY_EFFECT;
   }
   else if (!strncasecmp(name, "reverb", slen)) {
      rv = AAX_REVERB_EFFECT;
   }
   else if (!strncasecmp(name, "convolution", slen)) {
      rv = AAX_CONVOLUTION_EFFECT;
   }

   else if (!strncasecmp(name, "velocity", slen)) {
      rv = AAX_VELOCITY_EFFECT;
   }

   return rv;
}

AAX_API int AAX_APIENTRY
aaxGetByName(const char* type)
{
   int rv = aaxGetWaveformTypeByName(type);
   if (!rv) rv = aaxFilterGetByName(type);
   if (!rv) rv = aaxEffectGetByName(type);
   if (!rv) rv = aaxGetFrequencyFilterTypeByName(type);
   if (!rv) rv = aaxGetTypeByName(type);
   if (!rv) rv = aaxGetDistanceModelByName(type);

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

