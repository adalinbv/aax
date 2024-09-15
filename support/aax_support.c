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

#define SRC_ADD(p, l, m, s) { \
    size_t sl = strlen(s); \
    if (m && l) *p++ = '|'; \
    if (l > sl) { \
        memcpy(p, s, sl); \
        p += sl; l -= sl; m = 1; *p= 0; \
    } \
}

const char *_aax_id_s[_AAX_MAX_ID];
const char *_aaxErrorStrings[_AAX_MAX_ERROR];

static const char* __aaxErrorSetFunctionOrBackendString(const char*);

AAX_API void AAX_APIENTRY
aaxFree(void *mm)
{
   free(mm);
}

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

AAX_API bool AAX_APIENTRY
aaxPlaySoundLogo(const char *devname)
{
   bool rv = false;
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
           if (aaxBufferSetData(buffer, __aax_sound_logo) == false)
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
       else rv = false;

       aaxMixerSetState(config, AAX_STOPPED);
       aaxBufferDestroy(buffer);

       aaxDriverClose(config);
       aaxDriverDestroy(config);
   }

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxIsFilterSupported(aaxConfig cfg, const char *filter)
{
   _handle_t* handle = (_handle_t*)cfg;
   bool rv = false;
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
                  if (VALID_HANDLE(handle)) {
                     rv = true;
                  }
               }
               break;
            }
            else if (!strncasecmp(filter, _aaxFilters[i]->name, strlen(filter)))
            {
               if (VALID_HANDLE(handle)) {
                  rv = true;
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

AAX_API bool AAX_APIENTRY
aaxIsEffectSupported(aaxConfig cfg, const char *effect)
{
   _handle_t* handle = (_handle_t*)cfg;
   bool rv = false;
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
                  if (VALID_HANDLE(handle)) {
                     rv = true;
                  }
               }
               break;
            }
            else if (!strncasecmp(effect, _aaxEffects[i]->name, strlen(effect)))
            {
               if (VALID_HANDLE(handle)) {
                  rv = true;
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

AAX_API bool AAX_APIENTRY
aaxIsValid(const void* handle, enum aaxHandleType type)
{
   bool rv = false;
   if (handle)
   {
      switch(type)
      {
      case AAX_CONFIG:
      {
          const _handle_t* ptr = (const _handle_t*)handle;
          if (ptr->id == HANDLE_ID && VALID_HANDLE(ptr)) rv = true;
         break;
      }
      case AAX_BUFFER:
      {
         const _buffer_t* ptr = (const _buffer_t*)handle;
         if (ptr->id == BUFFER_ID) rv = true;
         break;
      }
      case AAX_EMITTER:
      {
         const _emitter_t* ptr = (const _emitter_t*)handle;
         if (ptr->id == EMITTER_ID) rv = true;
         break;
      }
      case AAX_AUDIOFRAME:
      {
         const _frame_t* ptr = (const _frame_t*)handle;
         if (ptr->id == AUDIOFRAME_ID) rv = true;
         break;
      }
      case AAX_FILTER:
      {
         const _filter_t* ptr = (const _filter_t*)handle;
         if (ptr->id == FILTER_ID) rv = true;
         break;
      }
      case AAX_EFFECT:
      {
         const _filter_t* ptr = (const _filter_t*)handle;
         if (ptr->id == EFFECT_ID) rv = true;
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
_aaxGetTypeByName(const char *name)
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

static enum aaxProcessingType
_aaxGetProcessingType(const char *type)
{
   enum aaxProcessingType rv = AAX_PROCESSING_NONE;

   if (type)
   {
      char *name = (char *)type;
      size_t len = strlen(name);

      if (len > 4 && !strncasecmp(name, "AAX_", 4)) {
         name += 4;
      }

      if (!strcasecmp(name, "overwrite")) {
        rv = AAX_OVERWRITE;
      } else if (!strcasecmp(name, "add")) {
        rv = AAX_ADD;
      } else if (!strcasecmp(name, "mix")) {
        rv = AAX_MIX;
      } else if (!strcasecmp(name, "modulate")) {
        rv = AAX_RINGMODULATE;
      } else if (!strcasecmp(name, "append")) {
        rv = AAX_APPEND;
      }
   }

   return rv;
}

static enum aaxMIDIModulationMode
_aaxGetMIDIModulationTypeByName(const char *mod)
{
   enum aaxMIDIModulationMode rv = mod ? atoi(mod) : 0;
   if (!rv && mod)
   {
      static const char *skip_str = "AAX_MIDI_";
      int skip = strlen(skip_str);
      char *name = (char *)mod;
      char *end, *last;
      size_t len;

      last = strchr(name, '|');
      if (!last) last = name+strlen(name);

      do
      {
         len = last - name;
         if (len > skip && !strncasecmp(name, skip_str, skip)) {
            name += skip;
         }

         end = last;
         while(end > name && *end != '_' && *end != '-') --end;
         if (end > name)
         {
            if (!strcasecmp(end+1, "CONTROL") || !strcasecmp(end+1, "DEPTH")) {
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
            if (!strncasecmp(name, "gain", len)) {
               rv |= AAX_MIDI_GAIN_CONTROL;
            } else if (!strncasecmp(name, "pitch", len)) {
               rv |= AAX_MIDI_PITCH_CONTROL;
            } else if (!strncasecmp(name, "filter", len) ||
                       !strncasecmp(name, "frequency", len)) {
               rv |= AAX_MIDI_FILTER_CONTROL;
            } else if (!strncasecmp(name, "chorus", len)) {
               rv |= AAX_MIDI_CHORUS_CONTROL;
            } else if (!strncasecmp(name, "tremolo", len)) {
               rv |= AAX_MIDI_LFO_GAIN_DEPTH;
            } else if (!strncasecmp(name, "vibrato", len)) {
               rv |= AAX_MIDI_LFO_PITCH_DEPTH;
            } else if (!strncasecmp(name, "wah", len)) {
               rv |= AAX_MIDI_LFO_FILTER_DEPTH;
            }
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

static char*
aaxGetMIDIModulationNameByType(enum aaxMIDIModulationMode mode)
{
   char rv[1024] = "none";
   int l = 1024;
   char *p = rv;
   char m = 0;

   if (mode & AAX_MIDI_PITCH_CONTROL) {
      SRC_ADD(p, l, m, "gain");
   }
   if (mode & AAX_MIDI_GAIN_CONTROL) {
      SRC_ADD(p, l, m, "pitch");
   }
   if (mode & AAX_MIDI_FILTER_CONTROL) {
      SRC_ADD(p, l, m, "filter");
   }
   if (mode & AAX_MIDI_CHORUS_CONTROL) {
      SRC_ADD(p, l, m, "chorus");
   }
   if (mode & AAX_MIDI_LFO_GAIN_DEPTH) {
      SRC_ADD(p, l, m, "tremolo");
   }
   if (mode & AAX_MIDI_LFO_PITCH_DEPTH) {
      SRC_ADD(p, l, m, "vibrato");
   }
   if (mode & AAX_MIDI_LFO_FILTER_DEPTH) {
      SRC_ADD(p, l, m, "wah");
   }

   if (!strcmp(rv, "none")) return NULL;
   return strdup(rv);
}

static enum aaxSourceType
_aaxGetSourceTypeByName(const char *wave, enum aaxTypeName type)
{
   enum aaxSourceType rv = AAX_WAVE_NONE;

   if (wave)
   {
      char *name = (char *)wave;
      size_t len, invlen;
      char *end, *last;

      last = strchr(name, '|');
      if (!last) last = name+strlen(name);

      do
      {
         len = last - name;
         if (len > 4 && !strncasecmp(name, "AAX_", 4)) {
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

            invlen = strlen("pure");
            if (!strncasecmp(name, "pure", invlen) &&
                (len > ++invlen))
            {
               name += invlen;
               len -= invlen;
               rv |= AAX_PURE_WAVEFORM;
            }

            if (!strncasecmp(name, "bessel", len)) {
               rv |= AAX_BESSEL;
            }

            if (!strncasecmp(name, "triangle", len)) {
               rv |= AAX_TRIANGLE;
            } else if (!strncasecmp(name, "sine", len)) {
               rv |= AAX_SINE;
            } else if (!strncasecmp(name, "square", len)) {
               rv |= AAX_SQUARE;
            } else if (!strncasecmp(name, "impulse", len)) {
               rv |= AAX_IMPULSE;
            } else if (!strncasecmp(name, "sawtooth", len)) {
               rv |= AAX_SAWTOOTH;
            } else if (!strncasecmp(name, "cycloid", len)) {
               rv |= AAX_CYCLOID;
            } else if (!strncasecmp(name, "random", len)) {
               rv |= AAX_RANDOM_SELECT;
            } else if (!strncasecmp(name, "randomness", len)) {
               rv |= AAX_RANDOMNESS;
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
                       !strncasecmp(name, "log", len)) {
               rv |= AAX_LOGARITHMIC_CURVE|AAX_LFO_EXPONENTIAL;
            } else if (!strncasecmp(name, "square-root", len) ||
                       !strncasecmp(name, "sqrt", len)) {
               rv |= AAX_SQUARE_ROOT_CURVE;
            } else if (!strncasecmp(name, "exponential", len) ||
                       !strncasecmp(name, "exp", len)) {
               rv |= AAX_EXPONENTIAL_CURVE|AAX_LFO_EXPONENTIAL;
            } else if (!strncasecmp(name, "linear", len) ||
                       !strncasecmp(name, "lin", len)) {
               rv |= AAX_LINEAR_CURVE;
            } else if (!strncasecmp(name, "1st-order", len)) {
               rv |= AAX_EFFECT_1ST_ORDER;
            } else if (!strncasecmp(name, "2nd-order", len)) {
               rv |= AAX_EFFECT_2ND_ORDER;
            } else if (!strncasecmp(name, "true", len) ||
                       !strncasecmp(name, "constant", len)) {
               rv |= AAX_CONSTANT;
            } else if (!strncasecmp(name, "inverse", len)) {
               rv |= AAX_CONSTANT|AAX_INVERSE;
            /* reverb */
            } else if (!strncasecmp(name, "empty", len)) {
               rv |= AAX_EMPTY_ROOM;
            } else if (!strncasecmp(name, "open", len)) {
               rv |= AAX_OPEN_ROOM;
            } else if (!strncasecmp(name, "sparse", len)) {
               rv |= AAX_SPARSE_ROOM;
            } else if (!strncasecmp(name, "average", len)) {
               rv |= AAX_AVERAGE_ROOM;
            } else if (!strncasecmp(name, "filled", len)) {
               rv |= AAX_FILLED_ROOM;
            } else if (!strncasecmp(name, "packed", len)) {
               rv |= AAX_PACKED_ROOM;
            } else if (!strncasecmp(name, "dense", len)) {
               rv |= AAX_DENSE_ROOM;
            } else if (!strncasecmp(name, "damped", len)) {
               rv |= AAX_DAMPED_ROOM;
            }
            else /* frequency filter, delay effects */
            {
               if (len >= 5 && (!strncasecmp(name+len-5, "ORDER", 5) ||
                                !strncasecmp(name+len-5, "STAGE", 5)))
               {
                  if (len >= 6 && (*(name+len-6) == '_' ||
                                   *(name+len-6) == '-')) {
                     len -= 6;
                  }
               }
               else if (len >= 3 && !strncasecmp(name+len-3, "OCT", 3))
               {
                  if (len >= 4 && (*(name+len-4) == '_' ||
                                   *(name+len-4) == '/')) {
                     len -= 4;
                  }
               }

               if (!strncasecmp(name, "resonance", len) ||
                   !strncasecmp(name, "Q", len))
               {
                  rv |= AAX_RESONANCE_FACTOR;
               }
               else if (!strncasecmp(name, "1", len) ||
                        !strncasecmp(name, "1st", len) ||
                        !strncasecmp(name, "6db", len))
               {
                  rv |= AAX_1ST_ORDER;
               }
               else if (!strncasecmp(name, "2", len) ||
                        !strncasecmp(name, "2nd", len) ||
                        !strncasecmp(name, "12db", len))
               {
                  rv |= AAX_2ND_ORDER;
               }
               else if (!strncasecmp(name, "3", len) ||
                        !strncasecmp(name, "3rd", len))
               {
                  rv |= AAX_3RD_ORDER;
               }
               else if (!strncasecmp(name, "4", len) ||
                        !strncasecmp(name, "4th", len) ||
                        !strncasecmp(name, "24db", len))
               {
                  rv |= AAX_4TH_ORDER;
               }
               else if (!strncasecmp(name, "5", len) ||
                        !strncasecmp(name, "5th", len))
               {
                  rv |= AAX_5TH_ORDER;
               }
               else if (!strncasecmp(name, "6", len) ||
                        !strncasecmp(name, "6th", len) ||
                        !strncasecmp(name, "36db", len))
               {
                  rv |= AAX_6TH_ORDER;
               }
               else if (!strncasecmp(name, "7", len) ||
                        !strncasecmp(name, "7th", len))
               {
                  rv |= AAX_7TH_ORDER;
               }
               else if (!strncasecmp(name, "8", len) ||
                        !strncasecmp(name, "8th", len) ||
                        !strncasecmp(name, "48db", len))
               {
                  rv |= AAX_8TH_ORDER;
               }
            } /* frequency filter, delay effects */
         }
         else {
            rv = AAX_CONSTANT;
         }

         if (last == name+strlen(name)) break;
         name = ++last;
         last = strchr(name, '|');
         if (!last) last = name+strlen(name);
      }
      while(last);
   }

   /* if only exponential is defined, assume exponential envelope following */
   if ((rv & AAX_LFO_EXPONENTIAL) && (rv & AAX_SOURCE_MASK) == 0) {
      rv |= AAX_ENVELOPE_FOLLOW;
   }

   return rv;
}

static char*
_aaxGetSourceNameByType(enum aaxSourceType type, enum aaxTypeName name)
{
   enum aaxSourceType ntype = type & AAX_NOISE_MASK;
   enum aaxSourceType stype = type & AAX_WAVEFORM_MASK;
   char rv[1024] = "none";
   int l = 1024;
   char *p = rv;
   char m = 0;
   int order;

   if (type & AAX_INVERSE) {
      SRC_ADD(p, l, m, "inverse-");
   }

   if (type & AAX_PURE_WAVEFORM) {
      SRC_ADD(p, l, m, "pure-");
   }

   m = 0;
   /* AAX_CONSTANT, AAX_IMPULSE and noises are different for
      frequency filters. */
   switch(stype)
   {
   case AAX_SAWTOOTH:
      SRC_ADD(p, l, m, "sawtooth");
      break;
   case AAX_SQUARE:
      SRC_ADD(p, l, m, "square");
      break;
   case AAX_TRIANGLE:
      SRC_ADD(p, l, m, "triangle");
      break;
   case AAX_SINE:
      SRC_ADD(p, l, m, "sine");
      break;
   case AAX_CYCLOID:
      SRC_ADD(p, l, m, "cycloid");
      break;
   case AAX_ENVELOPE_FOLLOW:
      SRC_ADD(p, l, m, "envelope");
      break;
   case AAX_TIMED_TRANSITION:
      SRC_ADD(p, l, m, "timed");
      break;
   case AAX_RANDOMNESS:
      SRC_ADD(p, l, m, "randomness");
      break;
   case AAX_RANDOM_SELECT:
      SRC_ADD(p, l, m, "random");
      break;
   case AAX_WAVE_NONE:
   default:
      break;
   }

   switch(name)
   {
   case AAX_DELAY_EFFECT_NAME:
      order = type & AAX_ORDER_MASK;
      if (type & AAX_EFFECT_1ST_ORDER) {
         SRC_ADD(p, l, m, "1st-order");
      } else if (type & AAX_EFFECT_2ND_ORDER) {
         SRC_ADD(p, l, m, "2nd-order");
      }

      if (order == AAX_1STAGE) {
         SRC_ADD(p, l, m, "1-stage");
      } else if (order == AAX_2STAGE) {
         SRC_ADD(p, l, m, "2-stage");
      } else if (order == AAX_3STAGE) {
         SRC_ADD(p, l, m, "3-stage");
      } else if (order == AAX_4STAGE) {
         SRC_ADD(p, l, m, "4-stage");
      } else if (order == AAX_5STAGE) {
         SRC_ADD(p, l, m, "5-stage");
      } else if (order == AAX_6STAGE) {
         SRC_ADD(p, l, m, "6-stage");
      } else if (order == AAX_7STAGE) {
         SRC_ADD(p, l, m, "7-stage");
      } else if (order == AAX_8STAGE) {
         SRC_ADD(p, l, m, "8-stage");
      }
      break;
   case AAX_TIMED_GAIN_FILTER_NAME:
   {
       int num = type & AAX_REPEAT_MASK; // max: 4095 (0x0FFF)
       char snum[9];

       snprintf(snum, 8, "%i", num);
       if (type & AAX_DSP_STATE_MASK)
       {
          if (type & AAX_RELEASE_FACTOR)
          {
             SRC_ADD(p, l, m, "release-factor");
             SRC_ADD(p, l, m, snum);
          }
          else if (type & AAX_REPEAT)
          {
             SRC_ADD(p, l, m, "repeat");
             if (num == AAX_MAX_REPEAT) {
                SRC_ADD(p, l, m, "max");
             } else {
                SRC_ADD(p, l, m, snum);
             }
          }
       }
       break;
   }
   case AAX_VOLUME_NAME:
   {
      int curve = type & AAX_DSP_STATE_MASK;

      if (curve == AAX_LOGARITHMIC_CURVE) {
          SRC_ADD(p, l, m, "logarithmic");
      } else if (curve == AAX_SQUARE_ROOT_CURVE) {
          SRC_ADD(p, l, m, "square-root");
      } else if (curve == AAX_EXPONENTIAL_CURVE) {
          SRC_ADD(p, l, m, "exponential");
      } else if (curve == AAX_LINEAR_CURVE) {
          SRC_ADD(p, l, m, "linear");
      }
      break;
   }
   case AAX_FREQUENCY_FILTER_NAME:
      order = type & AAX_ORDER_MASK;
      if (order == AAX_1ST_ORDER) {
         SRC_ADD(p, l, m, "6db");
      } else if (order == AAX_2ND_ORDER) {
         SRC_ADD(p, l, m, "12db");
      } else if (order == AAX_4TH_ORDER) {
         SRC_ADD(p, l, m, "24db");
      } else if (order == AAX_6TH_ORDER) {
         SRC_ADD(p, l, m, "36db");
      } else if (order == AAX_8TH_ORDER) {
         SRC_ADD(p, l, m, "48db");
      } else if (order == AAX_RESONANCE_FACTOR) {
         SRC_ADD(p, l, m, "Q");
      }

      if (type & AAX_BESSEL) {
         SRC_ADD(p, l, m, "bessel");
      }

      if (type & AAX_LFO_EXPONENTIAL) {
         SRC_ADD(p, l, m, "logarithmic");
      }
      break;
   case AAX_REVERB_NAME:
      order = type & AAX_ROOM_MASK;
      if (type & AAX_EFFECT_1ST_ORDER) {
         SRC_ADD(p, l, m, "1st-order");
      } else if (type & AAX_EFFECT_2ND_ORDER) {
         SRC_ADD(p, l, m, "2nd-order");
      }

      if (order == AAX_EMPTY_ROOM) {
          SRC_ADD(p, l, m, "empty");
      } else if (order == AAX_OPEN_ROOM) {
          SRC_ADD(p, l, m, "open");
      } else if (order == AAX_SPARSE_ROOM) {
          SRC_ADD(p, l, m, "sparse");
      } else if (order == AAX_AVERAGE_ROOM) {
          SRC_ADD(p, l, m, "average");
      } else if (order == AAX_FILLED_ROOM) {
          SRC_ADD(p, l, m, "filled");
      } else if (order == AAX_PACKED_ROOM) {
          SRC_ADD(p, l, m, "packed");
      } else if (order == AAX_DENSE_ROOM) {
          SRC_ADD(p, l, m, "dense");
      } else if (order == AAX_DAMPED_ROOM) {
          SRC_ADD(p, l, m, "damped");
      }
      break;
   default:
      order = type & AAX_ORDER_MASK;
      if (type & AAX_EFFECT_1ST_ORDER) {
         SRC_ADD(p, l, m, "1st-order");
      } else if (type & AAX_EFFECT_2ND_ORDER) {
         SRC_ADD(p, l, m, "2nd-order");
      }

      if (order == AAX_1ST_ORDER) {
         SRC_ADD(p, l, m, "1st-order");
      } else if (order == AAX_2ND_ORDER) {
         SRC_ADD(p, l, m, "2nd-order");
      } else if (order == AAX_3RD_ORDER) {
         SRC_ADD(p, l, m, "3rd-order");
      } else if (order == AAX_4TH_ORDER) {
         SRC_ADD(p, l, m, "4th-order");
      } else if (order == AAX_5TH_ORDER) {
         SRC_ADD(p, l, m, "5th-order");
      } else if (order == AAX_6TH_ORDER) {
         SRC_ADD(p, l, m, "6th-order");
      } else if (order == AAX_7TH_ORDER) {
         SRC_ADD(p, l, m, "7th-order");
      } else if (order == AAX_8TH_ORDER) {
         SRC_ADD(p, l, m, "8th-order");
      }

      switch(stype)
      {
      case AAX_CONSTANT:
         SRC_ADD(p, l, m, "true");
         break;
      case AAX_IMPULSE:
         SRC_ADD(p, l, m, "impulse");
         break;
      default:
         break;
      }

      switch(ntype)
      {
      case AAX_WHITE_NOISE:
         SRC_ADD(p, l, m, "white-noise");
         break;
      case AAX_PINK_NOISE:
         SRC_ADD(p, l, m, "pink-noise");
         break;
      case AAX_BROWNIAN_NOISE:
         SRC_ADD(p, l, m, "brownian-noise");
         break;
      default:
         break;
      }

      if (type & AAX_LFO_EXPONENTIAL) {
         SRC_ADD(p, l, m, "exponential");
      }
      break;
   }

   if (!strcmp(rv, "none")) return NULL;
   return strdup(rv);
}


static enum aaxDistanceModel
_aaxGetDistanceModelByName(const char *name)
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

      if (len > 7 && !strncasecmp(name, "AAX_AL_", 7)) {
         name += 7;
      } else if (len > 4 && !strncasecmp(name, "AAX_", 4)) {
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

static enum aaxFilterType
_aaxFilterGetByName(const char *name)
{
   enum aaxFilterType rv = AAX_FILTER_NONE;
   char type[256];
   int i, slen;
   char *end;

   slen = strlen(name);
   if (slen > 4 && !strncasecmp(name, "AAX_", 4)) {
      name += 4;
   }

   strlcpy(type, name, 256);
   name = type;

   slen = strlen(name);
   for (i=0; i<slen; ++i) {
      if (type[i] == '-') type[i] = '_';
   }

   end = strchr(name, '.');
   if (end)
   {
      while (end > name && *end != '_') --end;
      if (end) type[end-name] = 0;
   }

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
   else if (!strncasecmp(name, "timed_layer", slen)) {
       rv = AAX_TIMED_LAYER_FILTER;
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
_aaxEffectGetByName(const char *name)
{
   enum aaxEffectType rv = AAX_EFFECT_NONE;
   char type[256];
   int i, slen;
   char *end;

   slen = strlen(name);
   if (slen > 4 && !strncasecmp(name, "AAX_", 4)) {
      name += 4;
   }

   strlcpy(type, name, 256);
   name = type;

   slen = strlen(name);
   for (i=0; i<slen; ++i) {
      if (type[i] == '-') type[i] = '_';
   }

   end = strchr(name, '.');
   if (end)
   {
      while (end > name && *end != '_') --end;
      if (end) type[end-name] = 0;
   }

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
   else if (!strncasecmp(name, "flanging", slen) ||
            !strncasecmp(name, "flanger", slen)) {
      rv = AAX_FLANGING_EFFECT;
   }
   else if (!strncasecmp(name, "delay", slen) ||
            !strncasecmp(name, "delay-line", slen)) {
      rv = AAX_DELAY_EFFECT;
   }
   else if (!strncasecmp(name, "wavefold", slen)) {
      rv = AAX_WAVEFOLD_EFFECT;
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

AAX_API int64_t AAX_APIENTRY
aaxGetByName(const char* name, enum aaxTypeName type)
{
   int64_t rv = AAX_NONE;
   switch (type)
   {
   case AAX_ALL:
      rv = _aaxGetSourceTypeByName(name, type);
      if (!rv) rv = _aaxGetProcessingType(name);
      if (!rv) rv = _aaxFilterGetByName(name);
      if (!rv) rv = _aaxEffectGetByName(name);
      if (!rv) rv = _aaxGetTypeByName(name);
      if (!rv) rv = _aaxGetDistanceModelByName(name);
      break;
   case AAX_PROCESSING_NAME:
      rv = _aaxGetProcessingType(name);
      break;
   case AAX_FILTER_NAME:
      rv = _aaxFilterGetByName(name);
      break;
   case AAX_EFFECT_NAME:
   case AAX_DELAY_EFFECT_NAME:
      rv = _aaxEffectGetByName(name);
      break;
   case AAX_DISTANCE_MODEL_NAME:
      rv = _aaxGetDistanceModelByName(name);
      break;
   case AAX_TYPE_NAME:
      rv = _aaxGetTypeByName(name);
      break;
   case AAX_MODULATION_NAME:
      rv = _aaxGetMIDIModulationTypeByName(name);
      break;
   default:
      rv = _aaxGetSourceTypeByName(name, type);
      break;
   }

   return rv;
}

AAX_API char* AAX_APIENTRY
aaxGetStringByType(int type, enum aaxTypeName name)
{
   char *rv = NULL;
   switch(name)
   {
   case AAX_SOURCE_NAME:
      rv = _aaxGetSourceNameByType(type, name);
      break;
   case AAX_FREQUENCY_FILTER_NAME:
      rv = _aaxGetSourceNameByType(type, name);
      break;
   case AAX_DELAY_EFFECT_NAME:
      rv = _aaxGetSourceNameByType(type, name);
      break;
   case AAX_REVERB_NAME:
      rv = _aaxGetSourceNameByType(type, name);
      break;
   case AAX_FILTER_NAME:
      switch (type)
      {
      case AAX_EQUALIZER:
         rv = "equalizer";
         break;
      case AAX_VOLUME_FILTER:
         rv = "volume";
         break;
      case AAX_DYNAMIC_GAIN_FILTER:
         rv = "dynamic-gain";
         break;
      case AAX_TIMED_GAIN_FILTER:
         rv = "timed-gain";
         break;
      case AAX_DIRECTIONAL_FILTER:
         rv = "directional";
         break;
      case AAX_DISTANCE_FILTER:
         rv = "distance";
         break;
      case AAX_FREQUENCY_FILTER:
         rv = "frequency";
         break;
      case AAX_BITCRUSHER_FILTER:
         rv = "bitcrusher";
         break;
      case AAX_GRAPHIC_EQUALIZER:
         rv = "graphic-equalizer";
         break;
      case AAX_COMPRESSOR:
         rv = "compressor";
         break;
      case AAX_DYNAMIC_LAYER_FILTER:
         rv = "dynamic-layer";
         break;
      case AAX_TIMED_LAYER_FILTER:
         rv = "timed-layer";
         break;
      case AAX_FILTER_NONE:
      case AAX_FILTER_MAX:
      default:
         rv = "none";
         break;
      }
      break;
   case AAX_EFFECT_NAME:
      switch (type)
      {
      case AAX_PITCH_EFFECT:
         rv = "pitch";
         break;
      case AAX_DYNAMIC_PITCH_EFFECT:
         rv = "dynamic-pitch";
         break;
      case AAX_TIMED_PITCH_EFFECT:
         rv = "timed-pitch";
         break;
      case AAX_DISTORTION_EFFECT:
         rv = "distortion";
         break;
      case AAX_PHASING_EFFECT:
         rv = "phasing";
         break;
      case AAX_CHORUS_EFFECT:
         rv = "chorus";
         break;
      case AAX_FLANGING_EFFECT:
         rv = "flanging";
         break;
      case AAX_VELOCITY_EFFECT:
         rv = "velocity";
         break;
      case AAX_REVERB_EFFECT:
         rv = "reverb";
         break;
      case AAX_CONVOLUTION_EFFECT:
         rv = "convolution";
         break;
      case AAX_RINGMODULATOR_EFFECT:
         rv = "ringmodulator";
         break;
      case AAX_DELAY_EFFECT:
         rv = "delay";
         break;
      case AAX_WAVEFOLD_EFFECT:
         rv = "wavefold";
         break;
      case AAX_EFFECT_NONE:
      case AAX_EFFECT_MAX:
      default:
         rv = "none";
         break;
      }
      break;
   case AAX_DISTANCE_MODEL_NAME:
      switch (type)
      {
      case AAX_EXPONENTIAL_DISTANCE:
         rv = "exponential-distance";
         break;
      case AAX_ISO9613_DISTANCE:
         rv = "iso9613-distance";
         break;
      case AAX_DISTANCE_DELAY:
         rv = "distance-delay";
         break;
      case AAX_EXPONENTIAL_DISTANCE_DELAY:
         rv = "exponential-distance-delay";
         break;
      case AAX_ISO9613_DISTANCE_DELAY:
         rv = "iso9613-distance-delay";
         break;
      case AAX_AL_INVERSE_DISTANCE:
         rv = "al-inverse-distance";
         break;
      case AAX_AL_INVERSE_DISTANCE_CLAMPED:
         rv = "al-inverse-distance-clamped";
         break;
      case AAX_AL_LINEAR_DISTANCE:
         rv = "al-linear-distance";
         break;
      case AAX_AL_LINEAR_DISTANCE_CLAMPED:
         rv = "al-linear-distance-clamped";
         break;
      case AAX_AL_EXPONENT_DISTANCE:
         rv = "al-exponential-distance";
         break;
      case AAX_AL_EXPONENT_DISTANCE_CLAMPED:
         rv = "al-exponential-distance-clamped";
         break;
      case AAX_DISTANCE_MODEL_MAX:
      case AAX_AL_DISTANCE_MODEL_MAX:
      default:
         rv = "none";
         break;
      }
      break;
   case AAX_TYPE_NAME:
      switch (type)
      {
//    case AAX_KELVIN:
//    case AAX_KILOPASCAL:
      case AAX_LINEAR:
         rv = "linear";
         break;
//    case AAX_LOGARITHMIC:
      case AAX_DECIBEL:
         rv = "db";
         break;
      case AAX_RADIANS:
         rv = "rad";
         break;
      case AAX_DEGREES:
         rv = "deg";
         break;
      case AAX_BYTES:
         rv = "bytes";
         break;
      case AAX_FRAMES:
         rv = "frames";
         break;
      case AAX_SAMPLES:
         rv = "samples";
         break;
      case AAX_MICROSECONDS:
         rv = "usec";
         break;
      case AAX_DEGREES_CELSIUS:
         rv = "degC";
         break;
      case AAX_DEGREES_FAHRENHEIT:
         rv = "degF";
         break;
      case AAX_ATMOSPHERE:
         rv = "atm";
         break;
      case AAX_BAR:
         rv = "bar";
         break;
      case AAX_POUNDS_PER_SQUARE_INCH:
//    case AAX_PSI:
         rv = "psi";
         break;
      case AAX_BITS_PER_SAMPLE:
//    case AAX_BPS:
         rv = "bps";
         break;
      case AAX_MILLISECONDS:
         rv = "msec";
         break;
      case AAX_SECONDS:
         rv = "sec";
         break;
      case AAX_TYPE_MAX:
      default:
         rv = "none";
         break;
      }
      break;
   case AAX_MODULATION_NAME:
      rv = aaxGetMIDIModulationNameByType(type);
      break;
   default:
      break;
   }

   return rv ? strdup(rv) : rv;
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

