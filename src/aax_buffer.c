/*
 * SPDX-FileCopyrightText: Copyright © 2007-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#include <strings.h>
#endif
#include <stdio.h>              /* for NULL */
#include <math.h>		/* for floorf */
#include <assert.h>

#include <xml.h>

#include <software/audio.h>
#include <software/rbuf_int.h>
#include <stream/device.h>
#include <dsp/common.h>
#include <dsp/filters.h>
#include <dsp/effects.h>

#include <base/random.h>
#include <base/memory.h>

#include <3rdparty/MurmurHash3.h>

// #include "analyze.h"
#include "arch.h"
#include "api.h"

#define MAX_BUFFER_SIZE		(768*1024*1024)

static void _bufInitInfo(_buffer_info_t*);
static _aaxRingBuffer* _bufGetRingBuffer(_buffer_t*, _handle_t*, unsigned char);
static _aaxRingBuffer* _bufDestroyRingBuffer(_buffer_t*, unsigned char);
static bool _bufProcessWaveform(aaxBuffer, int, float, float, float, float, float, unsigned char, int, float, enum aaxSourceType, float, enum aaxProcessingType, limitType, float);
static _aaxRingBuffer* _bufSetDataInterleaved(_buffer_t*, _aaxRingBuffer*, const void*, unsigned);
static _aaxRingBuffer* _bufConvertDataToMixerFormat(_buffer_t*, _aaxRingBuffer*);
static void** _bufGetDataPitchLevels(_buffer_t*);
static void _bufGetDataInterleaved(_aaxRingBuffer*, void*, unsigned int, unsigned int, float);
static void _bufConvertDataToPCM24S(void*, void*, unsigned int, enum aaxFormat);
static void _bufConvertDataFromPCM24S(void*, void*, unsigned int, unsigned int, enum aaxFormat, unsigned int);
static int _bufCreateFromAAXS(_buffer_t*, const void*, float);
static int _bufSetDataFromAAXS(_buffer_t*, char*, int);
// static char** _bufCreateAAXS(_buffer_t*, void**, unsigned int);

static unsigned char  _aaxFormatsBPS[AAX_FORMAT_MAX];

AAX_API aaxBuffer AAX_APIENTRY
aaxBufferCreate(aaxConfig config, unsigned int samples, unsigned tracks,
                                   enum aaxFormat format)
{
   _handle_t* handle = (_handle_t*)config;
   unsigned int native_fmt = format & AAX_FORMAT_NATIVE;
   bool rv = __release_mode;
   _buffer_t *buf = NULL;

   if (!rv)
   {
      if (native_fmt >= AAX_FORMAT_MAX) {
         _aaxErrorSet(AAX_INVALID_ENUM);
      } else if (samples*tracks == 0)  {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      buf = calloc(1, sizeof(_buffer_t));
      if (buf == NULL) {
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      }
   }

   if (buf)
   {
      int blocksize;
      char *env;

      switch(native_fmt)
      {
      case AAX_IMA4_ADPCM:
         blocksize = DEFAULT_IMA4_BLOCKSIZE;
         break;
      default:
         blocksize = 1;
      }

      buf->id = BUFFER_ID;
      buf->root = buf->handle = handle;
      buf->mixer_info = VALID_HANDLE(handle) ? &handle->info : &_info;

      buf->midi_mode = AAX_RENDER_NORMAL;
      buf->to_mixer = false;
      buf->mipmap = false;
      buf->ref_counter = 1;
      buf->mip_levels = 1;
      buf->gain = 1.0f;
      buf->pos = 0;

      _bufInitInfo(&buf->info);
      buf->info.fmt = format;
      buf->info.no_tracks = tracks;
      buf->info.no_samples = samples;
      buf->info.blocksize = blocksize;
      buf->ringbuffer[0] = _bufGetRingBuffer(buf, handle, 0);

      /* explicit request not to convert */
      env = getenv("AAX_USE_MIXER_FMT");
      if (env && !_aax_getbool(env)) {
         buf->to_mixer = false;
      }

      /* sound is not mono */
      else if (tracks != 1) {
         buf->to_mixer = false;
      }

      /* more than 500Mb free memory is available, convert */
      else if (_aax_get_free_memory() > (500*1024*1024)) {
         buf->to_mixer = true;
      }
   }
   return (aaxBuffer)buf;
}

AAX_API bool AAX_APIENTRY
aaxBufferSetSetup(aaxBuffer buffer, enum aaxSetupType type, int64_t setup)
{
   _buffer_t* handle = get_buffer(buffer, __func__);
   bool rv = __release_mode;

   if (!rv && handle)
   {
      _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
      if (!rb) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
      unsigned int tmp;

      switch(type)
      {
      case AAX_POSITION: // overlaps with AAX_NAME_STRING, not a problem
         if (setup <= handle->info.no_samples)
         {
            handle->pos = setup;
            rv = true;
         }
         else  _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      case AAX_FREQUENCY:
         if ((setup >= 1000) && (setup <= 96000))
         {
            if (rb && !handle->info.rate) {
               rb->set_paramf(rb, RB_FREQUENCY, (float)setup);
            }
            handle->info.rate = (float)setup;
            rv = true;
         }
         else _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      case AAX_TRACKS:
         if ((setup > 0) && (setup <= _AAX_MAX_SPEAKERS))
         {
            if (rb)
            {
               rv = rb->set_parami(rb, RB_NO_TRACKS, setup);
               if (!rv) {
                  _aaxErrorSet(AAX_INVALID_PARAMETER);
               }
            }
            handle->info.no_tracks = setup;
            rv = true;
         }
         break;
      case AAX_FORMAT:
      {
         enum aaxFormat native_fmt = setup & AAX_FORMAT_NATIVE;
         if (native_fmt < AAX_FORMAT_MAX)
         {
            handle->info.fmt = setup;
            switch(native_fmt)
            {
            case AAX_IMA4_ADPCM:
               handle->info.blocksize = DEFAULT_IMA4_BLOCKSIZE;
               break;
            default:
               handle->info.blocksize = 1;
               break;
            }
            rv = true;
         }
         else _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      }
      case AAX_TRACK_SIZE:
         if (rb)
         {
            rv = rb->set_parami(rb, RB_TRACKSIZE, setup);
            if (!rv) {
               _aaxErrorSet(AAX_INVALID_PARAMETER);
            }
         }
         tmp = handle->info.no_tracks * aaxGetBitsPerSample(handle->info.fmt);
         handle->info.no_samples = setup*8/tmp;
         rv = true;
         break;
      case AAX_LOOP_START:
         if (setup < handle->info.no_samples)
         {
            if (rb)
            {
               int looping = (setup < handle->info.loop_end) ? true : false;
               rb->set_parami(rb, RB_LOOPPOINT_START, setup);
               rb->set_parami(rb, RB_LOOPING, looping);
            }
            handle->info.loop_start = setup;
            rv = true;
         }
         else _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      case AAX_LOOP_END:
         if (setup < handle->info.no_samples)
         {
            if (rb)
            {
               int looping = (handle->info.loop_start < setup) ? true : false;
               rb->set_parami(rb, RB_LOOPPOINT_END, setup);
               rb->set_parami(rb, RB_LOOPING, looping);
            }
            handle->info.loop_end = setup;
            rv = true;
         }
         else _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      case AAX_BLOCK_ALIGNMENT:
         if (setup > 1)
         {
            if (handle->info.fmt == AAX_IMA4_ADPCM)
            {
               handle->info.blocksize = setup;
               rb->set_parami(rb, RB_BLOCK_SIZE, setup);
               rv = true;
            }
         }
         else if (handle->info.fmt != AAX_IMA4_ADPCM)
         {
            handle->info.blocksize = setup;
            rb->set_parami(rb, RB_BLOCK_SIZE, setup);
            rv = true;
         }
         break;
      case AAX_SAMPLED_RELEASE:
         handle->info.sampled_release = setup ? true : false;
         if (rb) {
            rb->set_parami(rb, RB_SAMPLED_RELEASE,handle->info.sampled_release);
         }
         rv = true;
         break;
      case AAX_CAPABILITIES:
         switch(setup)
         {
         case AAX_RENDER_NORMAL:
         case AAX_RENDER_SYNTHESIZER:
         case AAX_RENDER_ARCADE:
         case AAX_RENDER_DEFAULT:
            handle->midi_mode = setup;
            break;
         default:
            _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         }
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   return rv;
}

AAX_API int64_t AAX_APIENTRY
aaxBufferGetSetup(const aaxBuffer buffer, enum aaxSetupType type)
{
   _buffer_t* handle = get_buffer(buffer, __func__);
   int64_t rv = __release_mode;

   if (!rv && handle)
   {
      _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
      if (!rb) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
      switch(type)
      {
      case AAX_SAMPLE_RATE:
         if (handle->aaxs) rv = handle->info.base_frequency;
         else rv = (unsigned int)roundf(handle->info.rate);
         break;
      case AAX_TRACKS:
         rv = handle->info.no_tracks;
         break;
      case AAX_FORMAT:
         rv = handle->info.fmt;
         break;
      case AAX_TRACK_SIZE:
         if (handle->info.fmt & AAX_AAXS)
         {
            if (handle->aaxs) {
               rv = strlen(handle->aaxs);
            }
         }
         else if (handle->info.rate)
         {
            float fact = 1.0f;
            if (rb) {
               fact = handle->info.rate / rb->get_paramf(rb, RB_FREQUENCY);
            }
            rv = handle->info.no_samples - handle->pos;
            rv *= (unsigned int)(fact*aaxGetBitsPerSample(handle->info.fmt));
            rv /= 8;
         }
         else _aaxErrorSet(AAX_INVALID_STATE);
         break;
      case AAX_NO_SAMPLES:
         if (handle->info.rate && !(handle->info.fmt & AAX_AAXS))
         {
            float fact = 1.0f;
            if (rb) {
               fact = handle->info.rate / rb->get_paramf(rb, RB_FREQUENCY);
            }
            rv = (unsigned int)(fact*(handle->info.no_samples - handle->pos));
         } else {
            _aaxErrorSet(AAX_INVALID_STATE);
         }
         break;
      case AAX_BLOCK_ALIGNMENT:
         rv = handle->info.blocksize;
         break;
      case AAX_POSITION:
         rv = handle->pos;
         break;
      case AAX_BALANCE:
         rv = AAX_TO_INT(handle->info.pan);
         break;
      case AAX_COMPRESSION_VALUE:
         rv = AAX_TO_INT(handle->gain);
         break;
      case AAX_PEAK_VALUE:
      case AAX_AVERAGE_VALUE:
         if (rb->get_state(rb, RB_IS_VALID))
         {
            if (handle->rms == 0.0)
            {
               MIX_T **track = (MIX_T**)rb->get_tracks_ptr(rb, RB_READ);
               size_t num = rb->get_parami(rb, RB_NO_SAMPLES);
               unsigned int rb_format = rb->get_parami(rb, RB_FORMAT);
               if (rb_format != AAX_PCM24S)
               {
                  void *data = _aax_aligned_alloc(num*sizeof(int32_t));
                  if (data)
                  {
                     _bufConvertDataToPCM24S(data, track[0], num, rb_format);
                     _batch_cvtps24_24(data, data, num);
                     _batch_get_average_rms(data, num, &handle->rms, &handle->peak);
                     _aax_aligned_free(data);
                  }
               }
               else
               {
                  _batch_cvtps24_24(track[0], track[0], num);
                  _batch_get_average_rms(track[0], num, &handle->rms, &handle->peak);
                  _batch_cvt24_ps24(track[0], track[0], num);
               }
               rb->release_tracks_ptr(rb);
            }
            rv = (type == AAX_AVERAGE_VALUE) ? handle->rms : handle->peak;
         } else {
            _aaxErrorSet(AAX_INVALID_STATE);
         }
         break;
      case AAX_LOOP_COUNT:
         rv = (unsigned int)handle->info.loop_count;
         break;
      case AAX_LOOP_START:
         rv = roundf(handle->info.loop_start);
         break;
      case AAX_LOOP_END:
         rv = roundf(handle->info.loop_end);
         break;
      case AAX_BASE_FREQUENCY:
         rv = (unsigned int)roundf(handle->info.base_frequency);
         break;
      case AAX_LOW_FREQUENCY:
         rv = (unsigned int)roundf(handle->info.low_frequency);
         break;
      case AAX_HIGH_FREQUENCY:
         rv = (unsigned int)roundf(handle->info.high_frequency);
         break;
      case AAX_PITCH_FRACTION:
         rv = AAX_TO_INT(handle->info.pitch_fraction);
         break;
      case AAX_TREMOLO_RATE:
         rv = AAX_TO_INT(handle->info.tremolo.rate);
         break;
      case AAX_TREMOLO_DEPTH:
         rv = AAX_TO_INT(handle->info.tremolo.depth);
         break;
      case AAX_TREMOLO_SWEEP:
         rv = AAX_TO_INT(handle->info.tremolo.sweep);
         break;
      case AAX_VIBRATO_RATE:
         rv = AAX_TO_INT(handle->info.vibrato.rate);
         break;
      case AAX_VIBRATO_DEPTH:
         rv = AAX_TO_INT(handle->info.vibrato.depth);
         break;
      case AAX_VIBRATO_SWEEP:
         rv = AAX_TO_INT(handle->info.vibrato.sweep);
         break;
      case AAX_ENVELOPE_LEVEL0:
      case AAX_ENVELOPE_RATE0:
      case AAX_ENVELOPE_LEVEL1:
      case AAX_ENVELOPE_RATE1:
      case AAX_ENVELOPE_LEVEL2:
      case AAX_ENVELOPE_RATE2:
      case AAX_ENVELOPE_LEVEL3:
      case AAX_ENVELOPE_RATE3:
      case AAX_ENVELOPE_LEVEL4:
      case AAX_ENVELOPE_RATE4:
      case AAX_ENVELOPE_LEVEL5:
      case AAX_ENVELOPE_RATE5:
      case AAX_ENVELOPE_LEVEL6:
      case AAX_ENVELOPE_RATE6:
         rv =AAX_TO_INT(handle->info.volume_envelope[type-AAX_ENVELOPE_LEVEL0]);
         break;
      case AAX_ENVELOPE_SUSTAIN:
         rv = handle->info.envelope_sustain;
         break;
      case AAX_SAMPLED_RELEASE:
         rv = handle->info.sampled_release;
         break;
      case AAX_FAST_RELEASE:
         rv = handle->info.fast_release;
         break;
      case AAX_POLYPHONY:
         rv = handle->info.polyphony;
         break;
      case AAX_MAX_PATCHES:
         rv = handle->info.no_patches;
         break;
      case AAX_MIDI_PRESSURE_MODE:
         rv = handle->info.pressure.mode;
         break;
      case AAX_MIDI_RELEASE_VELOCITY_FACTOR:
         rv = 100.0f*handle->info.pressure.factor;
         break;
      case AAX_MIDI_MODULATION_MODE:
          rv = handle->info.modulation.mode;
          break;
      case AAX_MIDI_MODULATION_FACTOR:
          rv = 100.0f*handle->info.modulation.factor;
          break;
      case AAX_MIDI_MODULATION_RATE:
          rv = 100.0f*handle->info.modulation.rate;
          break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   return rv;
}

AAX_API bool AAX_APIENTRY
aaxBufferSetData(aaxBuffer buffer, const void* d)
{
   _buffer_t* handle = get_buffer(buffer, __func__);
   bool rv = __release_mode;

   if (!rv && handle)
   {
      if (!d) {
          _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
      else if ((handle->info.fmt & AAX_SPECIAL) == 0)
      {
         _aaxRingBuffer *rb = _bufGetRingBuffer(handle, NULL, 0);
         if (!rb || rb->get_parami(rb, RB_NO_SAMPLES) == 0) {
            _aaxErrorSet(AAX_INVALID_STATE);
         } else {
            rv = true;
         }
      }
      else {
         rv = true;
      }
   }

   if (rv && (handle->info.fmt & AAX_SPECIAL))
   {				/* the data in *d isn't actual raw sound data */
      unsigned int format = handle->info.fmt;
      switch(format)
      {
      case AAX_AAXS16S:
      case AAX_AAXS24S:
         rv = _bufCreateFromAAXS(handle, d, 0);
         break;
      default:					/* should never happen */
         break;
      }
   }
   else if (rv)
   {
      _aaxRingBuffer *rb = _bufGetRingBuffer(handle, NULL, 0);
      unsigned int tracks, no_samples, buf_samples;
      unsigned blocksize =  handle->info.blocksize;
      unsigned int format = handle->info.fmt;
      void *data = (void*)d, *ptr = NULL;
      unsigned int native_fmt;
      char fmt_bps;

      rb->init(rb, false);
      tracks = rb->get_parami(rb, RB_NO_TRACKS);
      no_samples = rb->get_parami(rb, RB_NO_SAMPLES);

      buf_samples = tracks*no_samples;

				/* do we need to convert to native format? */
      native_fmt = format & AAX_FORMAT_NATIVE;
      if (format & ~AAX_FORMAT_NATIVE)
      {
         fmt_bps = _aaxFormatsBPS[native_fmt];
					/* first convert to native endianness */
         if ( ((format & AAX_FORMAT_LE) && is_bigendian()) ||
              ((format & AAX_FORMAT_BE) && !is_bigendian()) )
         {
            if (!ptr)
            {
               ptr = (void**)_aax_aligned_alloc(buf_samples*fmt_bps);
               if (!ptr)
               {
                  _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
                  return rv;
               }

               memcpy(ptr, data, buf_samples*fmt_bps);
               data = ptr;
            }

            switch (native_fmt)
            {
            case AAX_PCM16S:
               _batch_endianswap16(data, buf_samples);
               break;
            case AAX_PCM24S_PACKED:
               _batch_endianswap24(data, buf_samples);
               break;
            case AAX_PCM24S:
            case AAX_PCM32S:
            case AAX_FLOAT:
               _batch_endianswap32(data, buf_samples);
               break;
            case AAX_DOUBLE:
               _batch_endianswap64(data, buf_samples);
               break;
            default:
               break;
            }
         }
					/* then convert to proper signedness */
         if (format & AAX_FORMAT_UNSIGNED)
         {
            if (!ptr)
            {
               ptr = (void**)_aax_aligned_alloc(buf_samples*fmt_bps);
               if (!ptr)
               {
                  _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
                  return rv;
               }

               memcpy(ptr, data, buf_samples*fmt_bps);
               data = ptr;
            }

            switch (native_fmt)
            {
            case AAX_PCM8S:
               _batch_cvt8u_8s(data, buf_samples);
               break;
            case AAX_PCM16S:
               _batch_cvt16u_16s(data, buf_samples);
               break;
            case AAX_PCM24S:
               _batch_cvt24u_24s(data, buf_samples);
               break;
            case AAX_PCM32S:
               _batch_cvt32u_32s(data, buf_samples);
               break;
            default:
               break;
            }
         }
      }

      rb = _bufSetDataInterleaved(handle, rb, data, blocksize);
      handle->ringbuffer[0] = rb;

      rv = true;
      if (ptr) _aax_aligned_free(ptr);
   }
   return rv;
}

AAX_API void** AAX_APIENTRY
aaxBufferGetData(const aaxBuffer buffer)
{
   _buffer_t* handle = get_buffer(buffer, __func__);
   enum aaxFormat user_format;
   bool rv = __release_mode;
   void** data = NULL;

   if (!rv && handle)
   {
      user_format = handle->info.fmt;
      if (user_format != AAX_AAXS16S && user_format != AAX_AAXS24S)
      {
         if (!handle->info.rate) {
            _aaxErrorSet(AAX_INVALID_STATE);
         }
         else
         {
            _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
            if (!rb) {
               _aaxErrorSet(AAX_INVALID_STATE);
            } else {
               rv = true;
            }
         }
      }
      else {
         rv = true;
      }
   }

   user_format = handle->info.fmt;
   if (rv && (user_format == AAX_AAXS16S || user_format == AAX_AAXS24S))
   {
      if (handle->aaxs)
      {
         size_t len = strlen(handle->aaxs);

         data = malloc(len + sizeof(void*));
         if (data == NULL)
         {
            _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            return data;
         }

         data[0] = &data[1];
         memcpy(data+1, handle->aaxs, len);
         rv = false; /* we're done */
      }
      else {
         user_format = AAX_FLOAT;
      }
   }

   if (rv) {
      data = _bufGetDataPitchLevels(handle);
   }

   if (rv && !data)
   {
      unsigned int buf_samples, no_samples, tracks;
      unsigned int native_fmt, rb_format, pos;
      _aaxRingBuffer *rb;
      char *ptr, bps;
      float fact;

      rb = _bufGetRingBuffer(handle, NULL, 0);
      fact = handle->info.rate / rb->get_paramf(rb, RB_FREQUENCY);
      pos = (unsigned int)(fact*handle->pos);

      no_samples = (unsigned int)(fact*rb->get_parami(rb, RB_NO_SAMPLES) - pos);
      bps = rb->get_parami(rb, RB_BYTES_SAMPLE);
      tracks = rb->get_parami(rb, RB_NO_TRACKS);
      buf_samples = tracks*no_samples;

      data = (void**)_aax_malloc(&ptr, 3*sizeof(void*), no_samples*tracks*bps);
      if (data == NULL)
      {
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         return data;
      }

      _bufGetDataInterleaved(rb, ptr, no_samples, tracks, fact);
      data[0] = (void*)(ptr + pos*tracks*bps);
      data[1] = data[2] = NULL;

      native_fmt = user_format & AAX_FORMAT_NATIVE;
      rb_format = rb->get_parami(rb, RB_FORMAT);
      if (rb_format != native_fmt)
      {
         if (rb_format != AAX_PCM24S) 	/* first convert to signed 24-bit */
         {
            void **ndata;
            char *ptr;

            ndata = (void**)_aax_malloc(&ptr, sizeof(void*), buf_samples*sizeof(int32_t));
            if (ndata)
            {
               *ndata = (void*)ptr;
               _bufConvertDataToPCM24S(*ndata, *data, buf_samples, rb_format);
               free(data);
               data = ndata;
            }
         }

         if (native_fmt != AAX_PCM24S)	/* then convert to requested format */
         {
            int new_bps = aaxGetBytesPerSample(native_fmt);
            int block_smp = IMA4_BLOCKSIZE_TO_SMP(handle->info.blocksize);
            int new_samples = ((no_samples/block_smp)+1)*block_smp;
            void** ndata;
            char *ptr;

            ndata = (void**)_aax_malloc(&ptr, sizeof(void*), tracks*new_samples*new_bps);
            if (ndata)
            {
               *ndata = (void*)ptr;
               _bufConvertDataFromPCM24S(*ndata, *data, tracks, no_samples,
                                         native_fmt, handle->info.blocksize);
               free(data);
               data = ndata;
            }
         }
      } /* rb_format != native_fmt */

			/* do we need to convert to non-native format? */
      if (user_format & ~AAX_FORMAT_NATIVE)
      {
					/* do we need to change signedness? */
         if (user_format & AAX_FORMAT_UNSIGNED)
         {
            int signed_format = user_format & ~(AAX_FORMAT_LE | AAX_FORMAT_BE);
            switch (signed_format)
            {
            case AAX_PCM8U:
               _batch_cvt8s_8u(*data, buf_samples);
               break;
            case AAX_PCM16U:
               _batch_cvt16s_16u(*data, buf_samples);
               break;
            case AAX_PCM24U:
               _batch_cvt24s_24u(*data, buf_samples);
               break;
            case AAX_PCM32U:
               _batch_cvt32s_32u(*data, buf_samples);
               break;
            default:
               break;
            }
         }
					/* convert to requested endianness */
         if ( ((user_format & AAX_FORMAT_LE) && is_bigendian()) ||
              ((user_format & AAX_FORMAT_BE) && !is_bigendian()) )
         {
            switch (native_fmt)
            {
            case AAX_PCM16S:
               _batch_endianswap16(*data, buf_samples);
               break;
            case AAX_PCM24S_PACKED:
               _batch_endianswap24(data, buf_samples);
               break;
            case AAX_PCM24S:
            case AAX_PCM32S:
            case AAX_FLOAT:
               _batch_endianswap32(*data, buf_samples);
               break;
            case AAX_DOUBLE:
               _batch_endianswap64(*data, buf_samples);
               break;
            default:
               break;
            }
         }
      }

#if 0
      if (handle->info.fmt == AAX_AAXS16S || handle->info.fmt == AAX_AAXS24S) {
         data = (void**)_bufCreateAAXS(handle, data, buf_samples);
      }
#endif
   }

   return data;
}

AAX_API bool AAX_APIENTRY
aaxBufferDestroy(aaxBuffer buffer)
{
   _buffer_t* handle = get_buffer(buffer, __func__);
   bool rv = false;
   if (handle) {
     rv = free_buffer(handle);
   }
   return rv;
}

/**
 * This creates a buffer from an audio file indicated by an URL
 */
AAX_API aaxBuffer AAX_APIENTRY
aaxBufferReadFromStream(aaxConfig config, const char *url)
{
   _handle_t *handle = (_handle_t*)config;
   bool rv = __release_mode;
   _buffer_t* buf = NULL;

   if (!rv && handle) {
      rv = true;
   }

   if (rv)
   {
      _buffer_info_t info;
      char **ptr;

      ptr = _bufGetDataFromStream(handle, url, &info, handle->info);
      if (ptr)
      {
         buf = aaxBufferCreate(config, info.no_samples, info.no_tracks, info.fmt);
         if (buf)
         {
             _aaxRingBuffer* rb = _bufGetRingBuffer(buf, NULL, 0);

             if (buf->url) free(buf->url);
             buf->url = strdup(url);

             buf->info = info;
             if (rb)
             {
                _buffer_info_t *info = &buf->info;

                rb->set_paramf(rb, RB_FREQUENCY, info->rate);
                rb->set_format(rb, info->fmt & AAX_FORMAT_NATIVE, false);
                rb->set_parami(rb, RB_NO_TRACKS, info->no_tracks);

                // RB_BLOCK_SIZE must be set before RB_TRACKSIZE
                rb->set_parami(rb, RB_BLOCK_SIZE, info->blocksize);
                rb->set_parami(rb, RB_TRACKSIZE, info->no_bytes);
                rb->set_parami(rb, RB_NO_SAMPLES, info->no_samples);

                rb->set_paramf(rb, RB_LOOPPOINT_END, info->loop_end/info->rate);
                rb->set_paramf(rb, RB_LOOPPOINT_START, info->loop_start/info->rate);
                rb->set_parami(rb, RB_LOOPING, info->loop_count);
                rb->set_parami(rb, RB_ENVELOPE_SUSTAIN, info->envelope_sustain);
                rb->set_parami(rb, RB_SAMPLED_RELEASE, info->sampled_release);
                rb->set_parami(rb, RB_FAST_RELEASE, info->fast_release);
             }

#if 0
 printf("Stream Captured data:\n");
 printf(" url: '%s'\n", url);
 printf(" format: %x\n", info.fmt);
 printf(" no. tracks: %i\n", info.no_tracks);
 printf(" rate: %5.1f\n", info.rate);
 printf(" blocksize: %i\n", info.blocksize);
 printf(" data size: %i\n", info.no_bytes);
 printf(" samples: %li\n\n", info.no_samples);
# if 0
 _aaxFileDriverWrite("/tmp/test.wav", AAX_OVERWRITE, ptr[0], info.no_samples, info.rate, info.no_tracks, info.fmt);
# endif
#endif

             if ((aaxBufferSetData(buf, ptr[0])) != false)
             {
               _aaxRingBuffer* rb = _bufGetRingBuffer(buf, NULL, 0);
                _buffer_info_t *info = &buf->info;
                int i;

                for (i=0; i<_MAX_ENVELOPE_STAGES; ++i)
                {
                   float level, rate;

                   level = info->volume_envelope[2*i];
                   rate = info->volume_envelope[2*i+1];

                   rb->set_paramf(rb, RB_ENVELOPE_LEVEL+i, level);
                   rb->set_paramf(rb, RB_ENVELOPE_RATE+i, rate);
                }

                rb->set_paramf(rb, RB_TREMOLO_RATE, info->tremolo.rate);
                rb->set_paramf(rb, RB_TREMOLO_DEPTH, info->tremolo.depth);
                rb->set_paramf(rb, RB_TREMOLO_SWEEP, info->tremolo.sweep);
                rb->set_paramf(rb, RB_VIBRATO_RATE, info->vibrato.rate);
                rb->set_paramf(rb, RB_VIBRATO_DEPTH, info->vibrato.depth);
                rb->set_paramf(rb, RB_VIBRATO_SWEEP, info->vibrato.sweep);
             }
             else
             {
                aaxBufferDestroy(buf);
                buf = NULL;
             }
#if 0
_aaxFileDriverWrite("/tmp/test.wav", AAX_OVERWRITE, ptr, no_samples, freq, tracks, fmt);
#endif
         }
         free(ptr);
      }
      else {
         _aaxErrorSet(AAX_INVALID_REFERENCE);
      }
   }

   return buf;
}

AAX_API bool AAX_APIENTRY
aaxBufferWriteToFile(aaxBuffer buffer, const char *file, enum aaxProcessingType type)
{
   bool rv = false;
   if (aaxIsValid(buffer, AAX_BUFFER))
   {
      enum aaxFormat format = aaxBufferGetSetup(buffer, AAX_FORMAT);
      unsigned int samples = aaxBufferGetSetup(buffer, AAX_NO_SAMPLES);
      unsigned int freq = aaxBufferGetSetup(buffer, AAX_FREQUENCY);
      char tracks = aaxBufferGetSetup(buffer, AAX_TRACKS);

#if 0
      _buffer_t* handle = get_buffer(buffer, __func__);
      _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
      _aaxRingBufferData *rbi = rb->root;
      _aaxRingBufferSample *rbd = rbi->sample;
      const int32_t **data;

      data = (const int32_t**)rbd->track;
      _aaxFileDriverWrite(file, type, data, samples, freq, tracks, format);
#else
      void **data = aaxBufferGetData(buffer);
      _buffer_t *handle = (_buffer_t*)buffer;

      format = handle->info.fmt;
      freq = handle->info.rate;
      _aaxFileDriverWrite(file, type, *data, samples, freq, tracks, format);
      free(data);
#endif
      rv = true;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

static void _bufApplyBitCrusherFilter(_buffer_t*, _filter_t*, int);
static void _bufApplyFrequencyFilter(_buffer_t*, _filter_t*, int);
static void _bufApplyDistortionEffect(_buffer_t*, _effect_t*, int);
static void _bufApplyWaveFoldEffect(_buffer_t*, _effect_t*, int);
static void _bufApplyEqualizer(_buffer_t*, _filter_t*, int);


static unsigned char  _aaxFormatsBPS[AAX_FORMAT_MAX] =
{
  1,    /* 8-bit          */
  2,    /* 16-bit         */
  4,    /* 24-bit         */
  4,    /* 32-bit         */
  4,    /* 32-bit floats  */
  8,    /* 64-bit doubles */
  1,    /* mu-law         */
  1,    /* a-law          */
  1,    /* IMA4-ADPCM     */
  3 	/* AAX_PCM24S_PACKED */
};

int _getMaxMipLevels(int n)
{
   int rv = 0;
   int x = 1;

   while (x < n) {
      x <<= 1;
      rv++;
   }

   return rv;
}

_buffer_t*
get_buffer(aaxBuffer buffer, const char *func)
{
   _buffer_t *handle = (_buffer_t *)buffer;
   _buffer_t* rv  = NULL;

   if (handle && handle->id == BUFFER_ID) {
      rv = handle;
   }
   else if (handle && handle->id == FADEDBAD) {
      __aaxErrorSet(AAX_DESTROYED_HANDLE, func);
   }
   else {
      __aaxErrorSet(AAX_INVALID_HANDLE, func);
   }

   return rv;
}

int
free_buffer(_buffer_t* handle)
{
   bool rv = false;
   if (handle)
   {
      if (--handle->ref_counter == 0)
      {
         unsigned char b;
         for (b=0; b<handle->mip_levels; ++b) {
            handle->ringbuffer[b] = _bufDestroyRingBuffer(handle, b);
         }
         if (handle->aaxs) free(handle->aaxs);
         if (handle->url) free(handle->url);

         /* safeguard against using already destroyed handles */
         handle->id = FADEDBAD;
         free(handle);
      }
      rv = true;
   }
   return rv;
}

static void
_bufInitInfo(_buffer_info_t *info)
{
   memset(info, 0, sizeof(_buffer_info_t));
   info->fmt = AAX_PCM16S;
   info->no_tracks = 1;
   info->blocksize = 2;
   info->pitch_fraction = 1.0f;
   info->polyphony = 88;

   info->modulation.mode = AAX_MIDI_LFO_PITCH_DEPTH;
   info->modulation.factor = 1.0f;
   info->modulation.rate = 5.54f;

   info->pressure.mode = AAX_MIDI_GAIN_CONTROL;
   info->pressure.factor = 1.0f;
}

static _aaxRingBuffer*
_bufGetRingBuffer(_buffer_t* buf, _handle_t *handle, unsigned char pos)
{
   _aaxRingBuffer *rb = buf->ringbuffer[pos];
   if (!rb && handle && (VALID_HANDLE(handle) ||
                        (buf->mixer_info && *buf->mixer_info &&
                        VALID_HANDLE((_handle_t*)((*buf->mixer_info)->backend)))
                        )
      )
   {
      enum aaxRenderMode mode = handle->info->mode;
      const _aaxDriverBackend *be;

      if VALID_HANDLE(handle) {
         be = handle->backend.ptr;
      } else {
         be = ((_handle_t*)((*buf->mixer_info)->backend))->backend.ptr;
      }

      rb = be->get_ringbuffer(0.0f, mode);
      if (rb)
      {
         _buffer_info_t *info = &buf->info;

         /* initialize the ringbuffer in native format only */
         rb->set_paramf(rb, RB_FREQUENCY, info->rate);
         rb->set_format(rb, info->fmt & AAX_FORMAT_NATIVE, false);
         rb->set_parami(rb, RB_NO_TRACKS, info->no_tracks);

         // RB_BLOCK_SIZE must be set before RB_TRACKSIZE
         rb->set_parami(rb, RB_BLOCK_SIZE, info->blocksize);
//       rb->set_parami(rb, RB_TRACKSIZE, info->no_bytes);
         rb->set_parami(rb, RB_NO_SAMPLES, info->no_samples);

         rb->set_paramf(rb, RB_LOOPPOINT_END, info->loop_end/info->rate);
         rb->set_paramf(rb, RB_LOOPPOINT_START, info->loop_start/info->rate);
         rb->set_parami(rb, RB_LOOPING, info->loop_count);
         rb->set_parami(rb, RB_ENVELOPE_SUSTAIN, info->envelope_sustain);
         rb->set_parami(rb, RB_SAMPLED_RELEASE, info->sampled_release);
         rb->set_parami(rb, RB_FAST_RELEASE, info->fast_release);
         /* Postpone until aaxBufferSetData gets called
          * rb->init(rb, false);
          */
      }
   }
   return rb;
}

static _aaxRingBuffer*
_bufDestroyRingBuffer(_buffer_t* buf, unsigned char pos)
{
   _aaxRingBuffer *rb = buf->ringbuffer[pos];
   if (rb && (buf->mixer_info && *buf->mixer_info &&
              VALID_HANDLE((_handle_t*)((*buf->mixer_info)->backend)))
      )
   {
      _handle_t *handle = (_handle_t*)(*buf->mixer_info)->backend;
      const _aaxDriverBackend *be = handle->backend.ptr;

      be->destroy_ringbuffer(rb);
      rb = NULL;
   }
   return rb;
}

char**
_bufGetDataFromStream(_handle_t *handle, const char *url, _buffer_info_t *info, _aaxMixerInfo *_info)
{
   const _aaxDriverBackend *stream = &_aaxStreamDriverBackend;
   char **ptr = NULL;

   _bufInitInfo(info);
   if (stream)
   {
      static const char *xcfg = "<?xml?><"COPY_TO_BUFFER">1</"COPY_TO_BUFFER">";
      void *id = stream->new_handle(AAX_MODE_READ);
      xmlId *xid = xmlInitBuffer(xcfg, strlen(xcfg));

      // xid makes the stream return sound data in file format when capturing
      // At least the PAT extension supports adding "?level=<n>" to the URL
      // to define which patch number, of possible many, to process.
      id = stream->connect(NULL, id, xid, url, AAX_MODE_READ);
      xmlClose(xid);

      if (id)
      {
         float refrate = _info->refresh_rate;
         float periodrate = _info->period_rate;
         int brate = _info->bitrate;
         int res;

         info->no_tracks = _info->no_tracks;
         info->rate = _info->frequency;
         info->fmt = _info->format;

         res = stream->setup(id, &refrate, &info->fmt, &info->no_tracks,
                                 &info->rate, &brate, false, periodrate);
         if (TEST_FOR_TRUE(res) &&
             (info->no_tracks >= 1 && info->no_tracks <= RB_MAX_TRACKS) &&
             (info->rate >= 4000 && info->rate <= _AAX_MAX_MIXER_FREQUENCY))
         {
            size_t no_bytes, datasize, bits;
            char *ptr2;

            bits = aaxGetBitsPerSample(info->fmt);
            info->no_bytes = stream->param(id, DRIVER_NO_BYTES);
            info->blocksize = stream->param(id, DRIVER_BLOCK_SIZE);
            info->no_samples = stream->param(id, DRIVER_MAX_SAMPLES);
            info->rate = stream->param(id, DRIVER_FREQUENCY);
            no_bytes = (info->no_samples)*bits/8;

            if (info->blocksize) {
               no_bytes = ((no_bytes/info->blocksize)+1)*info->blocksize;
            }

            datasize = SIZE_ALIGNED(info->no_tracks*no_bytes);
            if (datasize < MAX_BUFFER_SIZE)	// sanity check
            {
               size_t offs = 2 * sizeof(void*);
               ptr = (char**)_aax_calloc(&ptr2, offs, 2, datasize);
            }

            if (ptr)
            {
               void **dst = (void **)ptr;
               ssize_t res; // offset = 0;
               ssize_t offs_packets = 0;
//             size_t size;
               int i;

               dst[0] = ptr2;
               ptr2 += datasize;
               dst[1] = ptr2;

               // capture now returns native file format instead of PCM24S
               // in batched capturing mode
//             size = 0;
               do
               {
                  ssize_t offs = offs_packets;
                  size_t packets = info->no_samples;

                  assert(packets <= datasize*8/(info->no_tracks*bits));

                  res = stream->capture(id, dst, &offs, &packets,
                                            dst[1], datasize, 1.0f, true);
//                if (res > 0) size += res;
                  offs_packets += packets;
//                if (res > 0) {
//                   offset += res*8/(bits*info->no_tracks);
//                }
               }
               while (res >= 0);

               // read the last information chunks, if any
               stream->flush(id);

               if (handle)
               {
                  _aax_free_meta(&handle->meta);
                  handle->meta.artist = stream->name(id, AAX_MUSIC_PERFORMER_STRING);
                  handle->meta.original = stream->name(id, AAX_ORIGINAL_PERFORMER_STRING);
                  handle->meta.title = stream->name(id, AAX_TRACK_TITLE_STRING);
                  handle->meta.album = stream->name(id, AAX_ALBUM_NAME_STRING);
                  handle->meta.trackno = stream->name(id, AAX_TRACK_NUMBER_STRING);
                  handle->meta.date = stream->name(id, AAX_RELEASE_DATE_STRING);
                  handle->meta.genre = stream->name(id, AAX_MUSIC_GENRE_STRING);
                  handle->meta.composer = stream->name(id, AAX_SONG_COMPOSER_STRING);
                  handle->meta.comments = stream->name(id, AAX_SONG_COMMENT_STRING);
                  handle->meta.copyright = stream->name(id, AAX_SONG_COPYRIGHT_STRING);
                  handle->meta.contact = stream->name(id, AAX_CONTACT_STRING);
                  handle->meta.website = stream->name(id, AAX_WEBSITE_STRING);
                  handle->meta.image = stream->name(id, AAX_COVER_IMAGE_DATA);
#if 0
 printf("artist: %s\n", handle->meta.artist);
 printf("title: %s\n", handle->meta.title);
 printf("genre: %s\n", handle->meta.genre);
 printf("track no.: %s\n", handle->meta.trackno);
 printf("album: %s\n", handle->meta.album);
 printf("sate: %s\n", handle->meta.date);
 printf("composer: %s\n", handle->meta.composer);
 printf("copyright: %s\n", handle->meta.copyright);
 printf("comments: %s\n", handle->meta.comments);
 printf("contact: %s\n", handle->meta.contact);
#endif
               }

               // get the actual number of samples
               info->rate = stream->param(id, DRIVER_FREQUENCY);
               info->no_samples = stream->param(id, DRIVER_MAX_SAMPLES);
               info->loop_count = stream->param(id, DRIVER_LOOP_COUNT);
               info->loop_start = stream->param(id, DRIVER_LOOP_START);
               info->loop_end = stream->param(id, DRIVER_LOOP_END);
               info->sampled_release = stream->param(id, DRIVER_SAMPLED_RELEASE);
               info->tremolo.rate = stream->param(id, DRIVER_TREMOLO_RATE);
               info->tremolo.depth = stream->param(id, DRIVER_TREMOLO_DEPTH);
               info->tremolo.sweep = stream->param(id, DRIVER_TREMOLO_SWEEP);
               info->vibrato.rate = stream->param(id, DRIVER_VIBRATO_RATE);
               info->vibrato.depth = stream->param(id, DRIVER_VIBRATO_DEPTH);
               info->vibrato.sweep = stream->param(id, DRIVER_VIBRATO_SWEEP);
               info->pitch_fraction = stream->param(id, DRIVER_PITCH_FRACTION);
               if (info->pitch_fraction < FLT_EPSILON) {
                  info->pitch_fraction = 1.0f;
               }
               // These could have been set by an AAXS file
               if (info->base_frequency < FLT_EPSILON) {
                  info->base_frequency = stream->param(id, DRIVER_BASE_FREQUENCY);
               }
               if (info->low_frequency < FLT_EPSILON) {
                  info->low_frequency = stream->param(id, DRIVER_LOW_FREQUENCY);
               }
               if (info->high_frequency < FLT_EPSILON) {
                  info->high_frequency = stream->param(id, DRIVER_HIGH_FREQUENCY);
               }

               for (i=0; i<_MAX_ENVELOPE_STAGES; ++i)
               {
                  off_t level, rate;

                  level = stream->param(id, DRIVER_ENVELOPE_LEVEL+i);
                  rate = stream->param(id, DRIVER_ENVELOPE_RATE+i);

                  if (rate == OFF_T_MAX) {
                     info->volume_envelope[2*i+1] = AAX_FPINFINITE;
                  } else {
                     info->volume_envelope[2*i+1] = rate;
                  }
                  info->volume_envelope[2*i] = level;
               }
               info->envelope_sustain = stream->param(id, DRIVER_ENVELOPE_SUSTAIN);
               info->sampled_release = stream->param(id, DRIVER_SAMPLED_RELEASE);
               info->fast_release = stream->param(id, DRIVER_FAST_RELEASE);
               info->no_patches = stream->param(id, DRIVER_NO_PATCHES);

#if 0
 printf("no. samples:\t\t%lu\n", info->no_samples);
 if (info->loop_count == OFF_T_MAX)
  printf("no. loops:\t\tinf\n");
 else
  printf("no. loops:\t\t%lu\n", info->loop_count);
 printf("loop start:\t\t%g\n", info->loop_start);
 printf("loop end:\t\t%g\n", info->loop_end);
 printf("sampled release:\t%s\n", info->sampled_release ? "yes" : "no");
 printf("base frequency:\t\t%g Hz\n", info->base_frequency);
 printf("low frequency:\t\t%g Hz\n", info->low_frequency);
 printf("high frequency:\t\t%g Hz\n", info->high_frequency);
 printf("pitch fraction:\t\t%g\n", info->pitch_fraction);
 printf("tremolo rate:\t\t%g Hz\n", info->tremolo.rate);
 printf("tremolo depth:\t\t%g\n", info->tremolo.depth);
 printf("tremolo sweep:\t\t%g Hz\n", info->tremolo.sweep);
 printf("vibrato rate:\t\t%g Hz\n", info->vibrato.rate);
 printf("vibrato depth:\t\t%g\n", info->vibrato.depth);
 printf("vibrato sweep:\t\t%g Hz\n", info->vibrato.sweep);
 printf("Envelope rates:\n");
 for (i=0; i<_MAX_ENVELOPE_STAGES; ++i) {
   float rate = stream->param(id, DRIVER_ENVELOPE_RATE+i);
   if (rate < 1.0f) printf("%4.2fms ", 1e3f*rate);
   else printf("%4.2fs ", rate);
 }
 printf("\nEnvelope offsets:\n");
 for (i=0; i<_MAX_ENVELOPE_STAGES; ++i)
   printf("%4.2f ", stream->param(id, DRIVER_ENVELOPE_LEVEL+i));
 printf("\n");
 printf("Envelope sustain: %s\n", info->envelope_sustain ? "yes" : "no");
 printf("Fast release: %s\n", info->fast_release ? "yes" : "no");
#endif
            }
         }
         stream->disconnect(id);
      }
   }

   return ptr;
}

int
_bufSetDataFromAAXS(_buffer_t *buffer, char *file, int level)
{
   _buffer_info_t *info = &buffer->info;
   char *s, *url, **data = NULL;
   int rv = 0;

   url = _aaxURLConstruct(buffer->url, file);
   s = strrchr(url, '.');
   if (!s || strcasecmp(s, ".aaxs"))
   {
      data = _bufGetDataFromStream(NULL, url, info, *buffer->mixer_info);
#if 0
 printf("url: '%s'\n\tfmt: %x, tracks: %i, freq: %4.1f, samples: %li, blocksize: %i\n", url, info->fmt, info->no_tracks, info->rate, info->no_samples, info->blocksize);
#endif
   }
   free(url);

   if (data)
   {
      _aaxRingBuffer* rb = _bufGetRingBuffer(buffer, buffer->root, level);
      if (rb)
      {
         /* the internal buffer format for .aaxs files is signed 24-bit */
         char **ndata;
         char *ptr;

         ndata = (char**)_aax_malloc(&ptr, sizeof(void*), info->no_samples*sizeof(int32_t));
         if (ndata)
         {
            *ndata = (void*)ptr;
            _bufConvertDataToPCM24S(*ndata, *data, info->no_samples, info->fmt);
            free(data);
            data = ndata;
         }

         info->fmt = AAX_PCM24S;
//       rb->set_format(rb, fmt, false);
         rb->set_parami(rb, RB_NO_LAYERS, info->no_tracks);
         rb->set_parami(rb, RB_NO_TRACKS, info->no_tracks);
         rb->set_paramf(rb, RB_FREQUENCY, info->rate);
         rb->set_parami(rb, RB_NO_SAMPLES, info->no_samples);

         rv = aaxBufferSetData(buffer, data[0]);
         free(data);

         rb->set_paramf(rb, RB_LOOPPOINT_END, info->loop_end/info->rate);
         rb->set_paramf(rb, RB_LOOPPOINT_START, info->loop_start/info->rate);
         rb->set_parami(rb, RB_SAMPLED_RELEASE, info->sampled_release);
         rb->set_parami(rb, RB_LOOPING, info->loop_count);
      }
   }

   return rv;
}

static bool
_bufCreateWaveformFromAAXS(_buffer_t* handle, const xmlId *xwid, int track, float ratio_factor, float pitch_factor, float freq, unsigned int pitch_level, int voices, float spread, limitType limiter, float version)
{
   enum aaxProcessingType ptype = AAX_OVERWRITE;
   enum aaxSourceType wtype = AAX_NONE;
   float staticity = 0.0f;
   float random = 0.0f;
   float phase = 0.0f;
   float pitch = 1.0f;
   float ratio = 0.0f;
   int midi_mode;

   midi_mode = handle->midi_mode;
   if (RENDER_NORMAL(midi_mode))
   {
      if (xmlAttributeExists(xwid, "voices"))
      {
         voices = _MINMAX(xmlAttributeGetInt(xwid, "voices"), 1, 11);
         if (xmlAttributeExists(xwid, "spread")) {
            spread = _MAX(xmlNodeGetDouble(xwid, "spread"), 0.01f);
            if (xmlAttributeGetBool(xwid, "phasing")) spread = -spread;
         }
      }
   }

   if (xmlAttributeExists(xwid, "ratio")) {
      ratio = xmlAttributeGetDouble(xwid, "ratio");
   }
   if (xmlAttributeExists(xwid, "pitch")) {
      pitch = xmlAttributeGetDouble(xwid, "pitch");
   }
   if (xmlAttributeExists(xwid, "phase")) {
      phase = xmlAttributeGetDouble(xwid, "phase");
   }
   if (RENDER_NORMAL(midi_mode) && xmlAttributeExists(xwid, "staticity")) {
      staticity = xmlAttributeGetDouble(xwid, "staticity");
   }
   if (xmlAttributeExists(xwid, "random")) {
      random = xmlAttributeGetDouble(xwid, "random");
   }

   if (!xmlAttributeCompareString(xwid, "src", "sawtooth")) {
      wtype = AAX_SAWTOOTH;
   } else if (!xmlAttributeCompareString(xwid, "src", "square")) {
      wtype = AAX_SQUARE;
   } else if (!xmlAttributeCompareString(xwid, "src", "triangle")) {
      wtype = AAX_TRIANGLE;
   } else if (!xmlAttributeCompareString(xwid, "src", "sine")) {
      wtype = AAX_SINE;
   } else if (!xmlAttributeCompareString(xwid, "src", "cycloid")) {
      wtype = AAX_CYCLOID;
   } else if (!xmlAttributeCompareString(xwid, "src", "impulse")) {
      wtype = AAX_IMPULSE;
   } else if (!xmlAttributeCompareString(xwid, "src", "true") ||
              !xmlAttributeCompareString(xwid, "src", "constant")) {
      wtype = AAX_CONSTANT;
   }
   else if (!xmlAttributeCompareString(xwid, "src","white-noise"))
   {
      wtype = AAX_WHITE_NOISE;
      if (!RENDER_NORMAL(midi_mode)) pitch = 1.0f;
   }
   else if (!xmlAttributeCompareString(xwid, "src","pink-noise"))
   {
      wtype = AAX_PINK_NOISE;
      if (!RENDER_NORMAL(midi_mode)) pitch = 1.0f;
   }
   else if (!xmlAttributeCompareString(xwid, "src", "brownian-noise"))
   {
      wtype = AAX_BROWNIAN_NOISE;
      if (!RENDER_NORMAL(midi_mode)) pitch = 1.0f;
   }
   else if (!xmlAttributeCompareString(xwid, "src", "pure-sawtooth")) {
      wtype = AAX_PURE_SAWTOOTH;
   } else if (!xmlAttributeCompareString(xwid, "src", "pure-square")) {
      wtype = AAX_PURE_SQUARE;
   } else if (!xmlAttributeCompareString(xwid, "src", "pure-triangle")) {
      wtype = AAX_PURE_TRIANGLE;
   } else if (!xmlAttributeCompareString(xwid, "src", "pure-sine")) {
      wtype = AAX_PURE_SINE;
   } else if (!xmlAttributeCompareString(xwid, "src", "pure-cycloid")) {
      wtype = AAX_PURE_CYCLOID;
   } else if (!xmlAttributeCompareString(xwid, "src", "pure-impulse")) {
      wtype = AAX_PURE_IMPULSE;
   }else {
      wtype = AAX_WAVE_NONE;
   }

   if (xmlAttributeExists(xwid, "processing"))
   {
      if (!xmlAttributeCompareString(xwid, "processing", "add"))
      {
         ptype = AAX_ADD;
         if (!ratio) ratio = 1.0f;
         if (!pitch) pitch = 1.0f;
         pitch *= pitch_factor;
         ratio *= ratio_factor;
      }
      else if (!xmlAttributeCompareString(xwid, "processing", "modulate"))
      {
         ptype = AAX_RINGMODULATE;
         if (!ratio) ratio = 1.0f;
         if (!pitch) pitch = 1.0f;
      }
      else if (!xmlAttributeCompareString(xwid, "processing", "mix"))
      {
         ptype = AAX_MIX;
         if (!ratio) ratio = 0.5f;
         if (!pitch) pitch = 1.0f;
         pitch *= pitch_factor;
         ratio *= ratio_factor;
      }
      else /* (!xmlAttributeCompareString(xwid, "processing", "overwrite")) */
      {
         ptype = AAX_OVERWRITE;
         if (!ratio) ratio = 1.0f;
         if (!pitch) pitch = 1.0f;
         pitch *= pitch_factor;
         ratio *= ratio_factor;
      }
   }
   else
   {
      ptype = AAX_OVERWRITE;
      if (!ratio) ratio = 1.0f;
      if (!pitch) pitch = 1.0f;
      pitch *= pitch_factor;
      ratio *= ratio_factor;
   }

   spread = spread*_log2lin(_lin2log(freq)/3.3f);
   if (ptype == AAX_RINGMODULATE) voices = 1;
   return _bufProcessWaveform(handle, track, freq, phase, pitch,
                              staticity, random, pitch_level, voices, spread,
                              wtype, ratio, ptype, limiter, version);
}

static int
_bufCreateFilterFromAAXS(_buffer_t* handle, const xmlId *xfid, int layer, float frequency)
{
   aaxFilter flt;
   _midi_t midi;

   midi.mode = handle->midi_mode;
   flt = _aaxGetFilterFromAAXS(handle->root, xfid, frequency, handle->info.low_frequency, handle->info.high_frequency, &midi);
   if (flt)
   {
      _filter_t* filter = get_filter(flt);
      switch (filter->type)
      {
      case AAX_BITCRUSHER_FILTER:
         _bufApplyBitCrusherFilter(handle, filter, layer);
         break;
      case AAX_FREQUENCY_FILTER:
      {
         int state = filter->state & ~(AAX_BUTTERWORTH|AAX_BESSEL);
         if (state == true     ||
             state == AAX_6DB_OCT  ||
             state == AAX_12DB_OCT ||
             state == AAX_24DB_OCT ||
             state == AAX_36DB_OCT ||
             state == AAX_48DB_OCT)
         {
            _bufApplyFrequencyFilter(handle, filter, layer);
         }
         break;
      }
      case AAX_EQUALIZER:
      case AAX_GRAPHIC_EQUALIZER:
         _bufApplyEqualizer(handle, filter, layer);
         break;
      default:
         break;
      }
      aaxFilterDestroy(flt);
   }
   return true;
}

static int
_bufCreateEffectFromAAXS(_buffer_t* handle, const xmlId *xeid, int layer, float frequency)
{
   float min = handle->info.low_frequency;
   float max = handle->info.high_frequency;
   aaxEffect eff;
   _midi_t midi;

   midi.mode = handle->midi_mode;
   eff = _aaxGetEffectFromAAXS(handle->root, xeid, frequency, min, max, &midi);
   if (eff)
   {
      _effect_t* effect = get_effect(eff);
      switch (effect->type)
      {
      case AAX_DISTORTION_EFFECT:
         if (effect->state == true) {
            _bufApplyDistortionEffect(handle, effect, layer);
         }
         break;
      case AAX_WAVEFOLD_EFFECT:
         _bufApplyWaveFoldEffect(handle, effect, layer);
         break;
      default:
         break;
      }
      aaxEffectDestroy(eff);
   }
   return true;
}

#define MUL     (65536.0f*256.0f)
#define IMUL    (1.0f/MUL)

static inline float fast_atanf(float x) {
  return GMATH_PI_4*x + 0.273f*x * (1.0f -fabsf(x));
}

static void
_bufLimit(_aaxRingBuffer* rb)
{
   _aaxRingBufferData *rbi = rb->handle;
   _aaxRingBufferSample *rbd = rbi->sample;
   unsigned int track, no_tracks = rbd->no_tracks;
   size_t no_samples = rbd->no_samples;
   int32_t **tracks;

   tracks = (int32_t**)rbd->track;
   for (track=0; track<no_tracks; track++)
   {
      int32_t *dptr = tracks[track];
      size_t i = no_samples;

      do
      {
         float samp = *dptr * IMUL;
         samp = _MINMAX(samp, -1.94139795f, 1.94139795f);
         samp = fast_atanf(samp)*(MUL*GMATH_1_PI_2);
         *dptr++ = samp;
      } while (--i);
   }
}

static float
_bufNormalize(_aaxRingBuffer* rb, float gain)
{
   static const float norm = (float)(1<<24);
   _aaxRingBufferData *rbi = rb->handle;
   _aaxRingBufferSample *rbd = rbi->sample;
   size_t j, no_samples = rbd->no_samples;
   int32_t *dptr = ((int32_t**)rbd->track)[0];
   double rms_total = 0.0;
   float rv, rms, peak = 0;

   j = no_samples;
   do
   {
      float samp = abs(*dptr++);
      rms_total += (double)samp*samp;
      if (samp > peak) peak = samp;
   }
   while (--j);

   peak /= norm;
   rms = sqrt(rms_total/no_samples)/norm;
   rv = gain*_db2lin(-21.0f - _lin2db(rms));

   return rv;
}

static inline float note2freq(uint8_t d) {
   return 440.0f*powf(2.0f, ((float)d-69.0f)/12.0f);
}

#if 0
static int
_bufAAXSThreadReadFromCache(_buffer_aax_t *aax_buf, const char *fname, size_t fsize)
{
   _buffer_t* handle = aax_buf->parent;
   bool rv = false;
   FILE *infile;

   infile = fopen(fname, "r");
   if (infile)
   {
      void **data = malloc(fsize);
      if (data)
      {
         char *end = (char*)data + fsize;
         int b = 0, mip_levels = 0;
         size_t *d = (size_t*)data;
         size_t size;

         size = fread(data, fsize, 1, infile);
         if (size == 1)
         {
            while(*d != 0)
            {
               size_t offs = *d++;

               // make sure the level-offsets are in ascending order
               if (((char*)d >= end) || (*d && (offs >= *d)))
               {
                  b = -1;
                  break;
               }
               mip_levels++;
            }

            // make sure the file-size matches the included data size
            if (b >= 0)
            {
               *d++ = fsize;		// was 0, to be used below.
               if ((char*)d < end)
               {
                  size = *d;
                  if (size != fsize) b = -1;
               }
            }

            if (b >= 0)
            {
                d = (size_t*)data;
                if (mip_levels > MAX_MIP_LEVELS) {
                   mip_levels = MAX_MIP_LEVELS;
                }
                handle->mip_levels = mip_levels;

                for (b=0; b<mip_levels; ++b)
                {
                   unsigned int no_samples;
                   _aaxRingBuffer *rb;
                   void **tracks;
                   char *ptr;

                   rb = _bufGetRingBuffer(handle, handle->root, b);

                   ptr = (char*)data + d[b];
                   size = d[b+1] - d[b];
                   no_samples = size/sizeof(float);

                   if (rb->get_state(rb, RB_IS_VALID) == false)
                   {
                      rb->set_parami(rb, RB_NO_SAMPLES, no_samples);
                      rb->init(rb, false);
                      handle->ringbuffer[b] = rb;
                   }
                   assert(size <= rb->get_parami(rb, RB_TRACKSIZE));

                   tracks = (void**)rb->get_tracks_ptr(rb, RB_WRITE);
                   memcpy(tracks[0], ptr, size);
                   rb->release_tracks_ptr(rb);
                }
                rv = true;
            }
         }
         free(data);
      }
   }

   return rv;
}
#endif

static bool
_bufCreateResonatorFromAAXS(_buffer_t* handle, xmlId *xsid, float version)
{
   float high_frequency = handle->info.high_frequency;
   float freq = handle->info.base_frequency;
   int b, layer, no_layers;
   double duration = 1.0f;
   limitType limiter;
   float spread = 0;
   int voices = 1;
   int bits = 24;
   int midi_mode;
   xmlId *xlid;
   char *env;
   bool rv = false;

   limiter = WAVEFORM_LIMIT_NORMAL;
   env = getenv("AAX_INSTRUMENT_MODE");
   if (env) {
      limiter = atoi(env);
   }

   if (xmlAttributeExists(xsid, "gain")) {
      handle->gain = xmlAttributeGetDouble(xsid, "gain");
   } else if (xmlAttributeExists(xsid, "fixed-gain")) {
      handle->gain = xmlAttributeGetDouble(xsid, "fixed-gain");
   }

   if (!freq && xmlAttributeExists(xsid, "frequency"))
   {
      freq = xmlAttributeGetDouble(xsid, "frequency");
      handle->info.base_frequency = freq;
      if (handle->info.pitch_fraction < 1.0f)
      {
         handle->info.low_frequency = freq*(1.0f-handle->info.pitch_fraction);
         handle->info.high_frequency = freq*(1.0f+handle->info.pitch_fraction);
         high_frequency = handle->info.high_frequency;
      }
      if (high_frequency > FLT_EPSILON)
      {
         int pitch = ceilf(high_frequency/freq);
         handle->mip_levels = _getMaxMipLevels(pitch);
         if (handle->mip_levels > MAX_MIP_LEVELS) {
            handle->mip_levels = MAX_MIP_LEVELS;
         }
      }
   }
   if (!handle->info.base_frequency) {
      handle->info.base_frequency = freq;
   }

   if (xmlAttributeExists(xsid, "duration"))
   {
      duration = xmlAttributeGetDouble(xsid, "duration");
      if (duration < 0.1f) {
         duration = 0.1f;
      }
   }

   if (xmlAttributeExists(xsid, "bits"))
   {
      bits = xmlAttributeGetInt(xsid, "bits");
      if (bits != 16) bits = 24;
   }

   midi_mode = handle->midi_mode;
   if (!env && RENDER_NORMAL(handle->midi_mode)) {
      limiter = xmlAttributeGetInt(xsid, "mode");
   }

   if (RENDER_NORMAL(midi_mode))
   {
      if (xmlAttributeExists(xsid, "voices")) {
         voices = _MINMAX(xmlAttributeGetInt(xsid, "voices"), 1, 11);
      }
      if (xmlAttributeExists(xsid, "spread")) {
         spread = _MAX(xmlAttributeGetDouble(xsid, "spread"), 0.01f);
         if (xmlAttributeGetBool(xsid, "phasing")) spread = -spread;
      }
   }

   // layers, declare one if none are defined
   no_layers = xmlNodeGetNum(xsid, "layer");
   if (no_layers > 0) {
      xlid = xmlMarkId(xsid);
   }
   else
   {
      no_layers = 1;
      xlid = xsid;
   }
   if (!RENDER_NORMAL(midi_mode)) {
      no_layers = 1;
   }
   handle->info.no_tracks = no_layers;

   for (b=0; b<handle->mip_levels; ++b)
   {
      _aaxRingBuffer *rb = _bufGetRingBuffer(handle, handle->root, b);
      if (rb)
      {
         float mul = (float)(1 << b);
         float pitch_fact = 1.0f/mul;

         if (duration >= 0.099f)
         {
            float f = pitch_fact*rb->get_paramf(rb, RB_FREQUENCY);
            size_t no_samples = SIZE_ALIGNED((size_t)rintf(duration*f));

            rb->set_parami(rb, RB_NO_SAMPLES, no_samples);
            rb->set_parami(rb, RB_NO_TRACKS, handle->info.no_tracks);
            rb->set_parami(rb, RB_NO_LAYERS, handle->info.no_tracks);
            handle->ringbuffer[b] = rb;
         }
      }

      for (layer=0; layer<no_layers; ++layer)
      {
         float mul = (float)(1 << b);
         float frequency = mul*freq;
         float pitch = 1.0f;
         float ratio = 1.0f;
         int num, waves;
         xmlId *xwid;

         if (xlid != xsid) {
            if (!xmlNodeGetPos(xsid, xlid, "layer", layer)) continue;
         }

         if (xmlAttributeExists(xlid, "ratio")) {
            ratio = xmlAttributeGetDouble(xlid, "ratio");
         }
         if (xmlAttributeExists(xlid, "pitch")) {
            pitch = xmlAttributeGetDouble(xlid, "pitch");
         }

         if (xmlAttributeExists(xlid, "voices")) {
            voices = _MINMAX(xmlAttributeGetInt(xlid, "voices"), 1, 11);
         }
         if (xmlAttributeExists(xlid, "spread")) {
            spread = _MAX(xmlAttributeGetDouble(xlid, "spread"), 0.01f);
            if (xmlAttributeGetBool(xlid, "phasing")) spread = -spread;
         }

         num = xmlNodeGetNum(xlid, "*");
         if (midi_mode == AAX_RENDER_ARCADE) waves = _MIN(3, num);
         else if (midi_mode == AAX_RENDER_SYNTHESIZER) waves = _MIN(3, num);
         else waves = num;

         xwid = xmlMarkId(xlid);
         if (num && xwid)
         {
            int i = _MAX(num-waves-1, 0);
            for (; i<num; i++)
            {
               if (!xmlNodeGetPos(xlid, xwid, "*", i)) continue;

               if (!xmlNodeCompareName(xwid, "waveform"))
               {
                  if (waves)
                  {
                     // mode:
                     // 0: do limiting in _bufferMixWaveform for every sample
                     // 1: do limiting here for all waveforms at once
                     // 2: do both
                     int mode = limiter & 1; // mode 0 or 1 only
                     rv = _bufCreateWaveformFromAAXS(handle, xwid, layer,
                                                     ratio, pitch, frequency,
                                                     b, voices, spread, mode,
                                                     version);
                     waves--;
                  }
               }
               else
               {
                  if (!xmlNodeCompareName(xwid, "filter")) {
                     rv = _bufCreateFilterFromAAXS(handle, xwid, layer, frequency);
                  } else if (!xmlNodeCompareName(xwid, "effect")) {
                     rv = _bufCreateEffectFromAAXS(handle, xwid, layer, frequency);
                  }
               }

               if (rv == false) break;
            }
         }
         if (xwid) xmlFree(xwid);
      } // layer

      if (midi_mode)
      {
         if (!b && rb->get_state(rb, RB_IS_VALID))
         {
            float gain = handle->gain;
            switch (handle->midi_mode)
            {
            case AAX_RENDER_ARCADE:
               gain *= 0.6f*0.6f;
               break;
            case AAX_RENDER_SYNTHESIZER:
               gain *= 0.6f;
               break;
            case AAX_RENDER_NORMAL:
            case AAX_RENDER_DEFAULT:
            default:
               break;
            }
            handle->gain = _bufNormalize(rb, gain);
         }
      }
      else if (limiter) {
         _bufLimit(rb);
      }

      if (0 && handle->to_mixer) {
         _bufConvertDataToMixerFormat(handle, rb);
      }
      else if (bits == 16)
      {
         _aaxRingBufferData *rbi = rb->handle;
         _aaxRingBufferSample *rbd = rbi->sample;
         unsigned int no_samples;

         no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
         for (layer=0; layer<no_layers; ++layer)
         {
            void *dptr = rbd->track[layer];
            _batch_cvt16_24(dptr, dptr, no_samples);
         }
         rb->set_parami(rb, RB_FORMAT, AAX_PCM16S);
         handle->info.fmt = AAX_PCM16S;
      }
   } // mip-level

   if (xlid != xsid) {
      xmlFree(xlid);
   }

   return rv;
}

static bool
_bufAAXSThreadCreateWaveform(_buffer_aax_t *aax_buf, xmlId *xid)
{
   _buffer_t* handle = aax_buf->parent;
   float low_frequency = 0.0f;
   float high_frequency = 0.0f;
   float sound_version = 1.0f;
   int midi_mode;
   bool rv = false;
   xmlId *xaid, *xiid;
   xmlId *xsid;

   xaid = xmlNodeGet(xid, "aeonwave");
   if (!xaid) return rv;

   midi_mode = handle->midi_mode;
   if (midi_mode == AAX_RENDER_NORMAL && handle->mixer_info) {
      midi_mode = handle->midi_mode = (*handle->mixer_info)->midi_mode;
   }

   xsid = NULL;
   if (!RENDER_NORMAL(midi_mode) && xmlNodeGet(xaid, "fm")) {
      xsid = xmlNodeGet(xaid, "fm");
   }
   if (!xsid)
   {
      xsid = xmlNodeGet(xaid, "resonator");
      if (!xsid) {
         xsid = xmlNodeGet(xaid, "sound");
      }
   }
   if (!xsid)
   {
      xmlFree(xaid);
      return rv;
   }

   xiid = xmlNodeGet(xaid, "info");
   if (xiid)
   {
      char s[1024] = "";
      xmlId *xnid;
      int res;

      /*
       * Read the info block version number:
       * 0.0: First implementation, AeonWave prior to version 4.0
       * 0.1: Use the volume matched gains table when generating waveforms.
       */
      xnid = xmlNodeGet(xiid, "sound");
      if (xnid)
      {
         if (xmlAttributeExists(xnid, "version")) {
            sound_version = xmlAttributeGetDouble(xnid, "version");
         }
         xmlFree(xnid);
      }

      do
      {
         char *bank, *program;

         res = xmlAttributeCopyString(xiid, "name", s, 1024);
         if (res) aax_buf->meta.title = strdup(s);

         bank = xmlAttributeGetString(xiid, "bank");
         program = xmlAttributeGetString(xiid, "program");

         if (bank && program) {
            snprintf(s, 1024, "%s/%s", bank, program);
         } else if (program) {
            snprintf(s, 1024, "%s", program);
         }
         aax_buf->meta.trackno = strdup(s);

         xmlFree(program);
         xmlFree(bank);
      }
      while(0);

      xnid = xmlNodeGet(xiid, "copyright");
      if (xnid)
      {
         res = xmlAttributeCopyString(xnid, "by", s, 1024);
         if (res) aax_buf->meta.copyright = strdup(s);

         res = xmlAttributeCopyString(xnid, "from", s, 1024);
         if (res) aax_buf->meta.date = strdup(s);

         xmlFree(xnid);
      }

      xnid = xmlNodeGet(xiid, "contact");
      if (xnid)
      {
         res = xmlAttributeCopyString(xnid, "author", s, 1024);
         if (res) aax_buf->meta.composer = strdup(s);

         res = xmlAttributeCopyString(xnid, "website", s, 1024);
         if (res) aax_buf->meta.contact = strdup(s);

         xmlFree(xnid);
      }

      xnid = xmlNodeGet(xiid, "note");
      if (xnid)
      {
         if (xmlAttributeExists(xnid, "min")) {
            low_frequency = note2freq(xmlAttributeGetDouble(xnid, "min"));
         }
         if (xmlAttributeExists(xnid, "max")) {
            high_frequency = note2freq(_MIN(xmlAttributeGetDouble(xnid, "max"), 128));
         }

         if (xmlAttributeExists(xnid, "pitch-fraction")) {
            handle->info.pitch_fraction = xmlAttributeGetDouble(xnid, "pitch-fraction");
         }

         handle->info.polyphony = xmlAttributeGetInt(xnid, "polyphony");
         if (handle->info.polyphony && handle->info.pitch_fraction) {
            snprintf(s, 1024, "polyphony: %u, pitch fraction: %3.1f",
                      handle->info.polyphony, handle->info.pitch_fraction);
         } else if (handle->info.polyphony) {
            snprintf(s, 1024, "polyphony: %u", handle->info.polyphony);
         } else if (handle->info.pitch_fraction) {
            snprintf(s, 1024, "pitch fraction: %3.1f", handle->info.pitch_fraction);
         }
         aax_buf->meta.comments = strdup(s);

         xmlFree(xnid);
      }

      xmlFree(xiid);
   }

   if (handle->info.base_frequency < FLT_EPSILON) {
      handle->info.base_frequency = aax_buf->frequency;
   }
   if (handle->info.low_frequency < FLT_EPSILON) {
      handle->info.low_frequency = low_frequency;
   }
   if (handle->info.high_frequency < FLT_EPSILON) {
      handle->info.high_frequency = high_frequency;
   }

   if (xmlAttributeExists(xsid, "include"))
   {
      char file[1024];
      int len = xmlAttributeCopyString(xsid, "include", file, 1024);
      if (len < 1024-strlen(".aaxs"))
      {
         _buffer_info_t *info = &handle->info;
         char **data, *url;

         strcat(file, ".aaxs");
         url = _aaxURLConstruct(handle->url, file);

         data = _bufGetDataFromStream(NULL, url, info, *handle->mixer_info);
         if (data)
         {
            xmlId *xid = xmlInitBuffer(data[0], strlen(data[0]));
            if (xid)
            {
               xmlId *xasid = xmlNodeGet(xid, "aeonwave/resonator");
               if (!xasid) {
                  xasid = xmlNodeGet(xid, "aeonwave/sound");
               }
               if (xasid)
               {
                  rv =_bufCreateResonatorFromAAXS(handle, xasid, sound_version);
                  xmlFree(xasid);
               }
               xmlClose(xid);
            }
            free(data);
         }

         free(url);
      }
   }
   else if (xmlAttributeExists(xsid, "file"))
   {
      char *file = xmlAttributeGetString(xsid, "file");

      rv = _bufSetDataFromAAXS(handle, file, 0);
      if (!rv) {
         aax_buf->error = AAX_INVALID_REFERENCE;
      }
      xmlFree(file);

      if (!handle->info.loop_end) // loop was not defined in the file
      {
         _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
         float loop_start, loop_end;

         loop_start = xmlAttributeGetDouble(xsid, "loop-start");
         if (xmlAttributeExists(xsid, "loop-end")) {
            loop_end = xmlAttributeGetDouble(xsid, "loop-end");
         } else {
            loop_end = handle->info.no_samples;
         }
         if (loop_end > loop_start) {
            handle->info.loop_count = INT_MAX;
         }
         handle->info.loop_start = loop_start;
         handle->info.loop_end = loop_end;

         rb->set_paramf(rb, RB_LOOPPOINT_END, loop_end/handle->info.rate);
         rb->set_paramf(rb, RB_LOOPPOINT_START,loop_start/handle->info.rate);
         rb->set_parami(rb, RB_SAMPLED_RELEASE,handle->info.sampled_release);
         rb->set_parami(rb, RB_LOOPING, handle->info.loop_count);
      }

      handle->mip_levels = 0;
      rv = _bufCreateResonatorFromAAXS(handle, xsid, sound_version);
   }
   else {
      rv = _bufCreateResonatorFromAAXS(handle, xsid, sound_version);
   }

   xmlFree(xsid);
   xmlFree(xaid);

   return rv;
}

static int
_bufAAXSThread(void *d)
{
   _buffer_aax_t *aax_buf = (_buffer_aax_t*)d;
   _buffer_t* handle = aax_buf->parent;
   const void *aaxs =  aax_buf->aaxs;
   bool rv = false;
   xmlId *xid, *xiid;

   assert(handle);
   assert(aaxs);

   if (handle->aaxs) free(handle->aaxs);
   handle->aaxs = strdup(aaxs);
   if (!handle->aaxs) {
      return false;
   }

   xid = xmlInitBuffer(handle->aaxs, strlen(handle->aaxs));
   if (xid)
   {
      char *a = xmlNodeGetString(xid, "aeonwave");
      uint32_t hash[4] = {0, 0, 0, 0};
      char have_hash = 0;
      char hstr[64];

      if (a)
      {
          char *s = strcasestr(a, "<sound");
          if (s)
          {
              char *e = strcasestr(s, "</sound>");
              if (e)
              {
                  e += strlen("</sound>");
                  MurmurHash3_x64_128(s, e-s, 0x27918072, &hash);
                  snprintf(hstr, 64, "%08x%08x%08x%08x", hash[0], hash[1],
                                                         hash[2], hash[3]);
                  have_hash = 1;
              }
          }
          xmlFree(a);
      }

      handle->info.pressure.factor = 1.0f;
      xiid = xmlNodeGet(xid, "aeonwave/info/aftertouch");
      if (xiid)
      {
         if (xmlAttributeExists(xiid, "mode"))
         {
            char *s = xmlAttributeGetString(xiid, "mode");
            if (s)
            {
               handle->info.pressure.mode = aaxGetByName(s,AAX_MODULATION_NAME);
               xmlFree(s);
            }
         }
         if (xmlAttributeExists(xiid, "sensitivity")) {
            handle->info.pressure.factor = xmlAttributeGetDouble(xiid, "sensitivity");
         }
         xmlFree(xiid);
      }

      handle->info.modulation.factor = 1.0f;
      handle->info.modulation.rate = 5.54f; // Hz
      xiid = xmlNodeGet(xid, "aeonwave/info/modulation");
      if (xiid)
      {
         char *s = xmlAttributeGetString(xiid, "mode");
         if (s)
         {
            handle->info.modulation.mode = aaxGetByName(s, AAX_MODULATION_NAME);
            xmlFree(s);
         }
         if (xmlAttributeExists(xiid, "depth")) {
            handle->info.modulation.factor = xmlAttributeGetDouble(xiid, "depth");
         }
         if (xmlAttributeExists(xiid, "rate")) {
            handle->info.modulation.rate = xmlAttributeGetDouble(xiid, "rate");
         }
         xmlFree(xiid);
      }

      if (handle->info.pan == 0.0f)
      {
         xiid = xmlNodeGet(xid, "aeonwave/emitter");
         if (xiid)
         {
            float pan = xmlAttributeGetDouble(xiid, "pan");
            if (pan != 0.0f) handle->info.pan = pan;
            xmlFree(xiid);
         }
      }
      if (handle->info.pan == 0.0f)
      {
         xiid = xmlNodeGet(xid, "aeonwave/audioframe");
         if (xiid)
         {
            float pan = xmlAttributeGetDouble(xiid, "pan");
            if (pan != 0.0f) handle->info.pan = pan;
            xmlFree(xiid);
         }
      }

      if (have_hash)
      {
         char *fname;

         fname = userCacheFile(hstr);
         if (fname)
         {
#if 0
            size_t fsize = getFileSize(fname);
            if (fsize) {
               rv = _bufAAXSThreadReadFromCache(aax_buf, fname, fsize);
            }
#endif
            if (!rv)
            {
               rv = _bufAAXSThreadCreateWaveform(aax_buf, xid);
#if 0
               if (rv)
               {
                  // create the cache file
                  void **data = _bufGetDataPitchLevels(handle);
                  if (data)
                  {
                     FILE *output = fopen(fname, "w");
                     if (output)
                     {
                        size_t *d = (size_t*)data;
                        char *s = (char*)data;
                        size_t datasize;

                        while (*d)
                        {
                           char *c = (char*)*d;
                           *d++ = (size_t)(c - s); // make offsets relative
                        }
                        datasize = *(d+1);

                        fwrite(data, datasize, 1, output);
                        fclose(output);
                     }
                     free(data);
                  }
               }
#endif
            }
            free(fname);
         }
      }
      else {
         rv = _bufAAXSThreadCreateWaveform(aax_buf, xid);
      }
      xmlClose(xid);
   }
   else {
      aax_buf->error = AAX_INVALID_PARAMETER+2;
   }

   return rv;
}

static int
_bufCreateFromAAXS(_buffer_t* buffer, const void *aaxs, float freq)
{
   _handle_t *handle = buffer->root;
   _buffer_aax_t data;
   bool rv = false;

   memset(&data, 0, sizeof(_buffer_aax_t));

   data.parent = buffer;
   data.aaxs = aaxs;
   data.frequency = freq;
   data.error = AAX_ERROR_NONE;

   // Using a thread here might spawn wavweform generation on another CPU
   // core freeing the current (possibly busy) CPU core from doing the work.
   if (!handle->buffer_thread.ptr) {
      handle->buffer_thread.ptr = _aaxThreadCreate();
   }

   if (handle->buffer_thread.ptr)
   {
      int r = _aaxThreadStart(handle->buffer_thread.ptr, _bufAAXSThread, &data,
		              0, "aaxBufferAAXS");
      if (r == thrd_success) {
         _aaxThreadJoin(handle->buffer_thread.ptr);
         if (r == thrd_success) rv = true;
      }
      else {
         _bufAAXSThread(&data);
      }
   }
   else {
      _bufAAXSThread(&data);
   }

   _aax_free_meta(&handle->meta);
   handle->meta = data.meta;

   if (data.error) {
      _aaxErrorSet(data.error);
   } else {
      rv = true;
   }

   return rv;
}

#if 0
static char**
_bufCreateAAXS(_buffer_t *handle, void **data, unsigned int samples)
{
   _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
   float fs, **freqs;
   char **rv;

   rv = malloc(100);

   fs = rb->get_paramf(rb, RB_FREQUENCY);
   freqs = _aax_analyze_waveforms(data, samples, fs);

   free(freqs);

   return rv;
}
#endif

static bool
_bufProcessWaveform(aaxBuffer buffer, int track, float freq, float phase, float pitch, float staticity, float random, unsigned char pitch_level, int voices, float spread, enum aaxSourceType wtype, float ratio, enum aaxProcessingType ptype, limitType limiter, float version)
{
   enum aaxSourceType wave = wtype & (AAX_ALL_SOURCE_MASK & ~AAX_PURE_WAVEFORM);
   _buffer_t* handle = get_buffer(buffer, __func__);
   bool rv = __release_mode;

   if (!rv)
   {
      if (wave < AAX_1ST_WAVE || wave > AAX_LAST_NOISE ||
          (wave > AAX_LAST_WAVE && wave < AAX_1ST_NOISE))
      {
         _aaxErrorSet(AAX_INVALID_PARAMETER + 3);
      } else if ((ptype == AAX_MIX) && (ratio > 1.0f || ratio < -1.0f)) {
         _aaxErrorSet(AAX_INVALID_PARAMETER + 4);
      } else if (ptype >= AAX_PROCESSING_MAX) {
         _aaxErrorSet(AAX_INVALID_PARAMETER + 5);
      }
      else if (handle && handle->mixer_info && (*handle->mixer_info && ((*handle->mixer_info)->id == INFO_ID)))
         rv = true;
   }

   if (rv)
   {
      _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, pitch_level);
      float samps_period, fs, fw, fs_mixer, rate;
      _data_t *scratch;
      int no_samples;
      int q, hvoices;
      uint64_t seed;
      unsigned char skip;
      bool modulate;
      bool phasing;

      fs = rb->get_paramf(rb, RB_FREQUENCY);
      fs_mixer = _info->frequency;
      if (handle->mixer_info && *handle->mixer_info) {
         fs_mixer = (*handle->mixer_info)->frequency;
      }

      modulate = false;
      rate = freq * pitch;
      fw = FNMINMAX(rate, 1.0f, 22050.0f);
      seed = (FNMINMAX((double)random, 0.0, 1.0) * (double)UINT64_MAX);

      staticity = _MINMAX(staticity*fs/fs_mixer, 0.0f, 1.0f);
      skip = (unsigned char)(1.0f + 99.0f*staticity);

      phase *= GMATH_PI;
//    if (ratio < 0.0f) phase = GMATH_PI - phase;

      fs = rb->get_paramf(rb, RB_FREQUENCY);
      no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
      samps_period = fs/fw;
      hvoices = voices >> 1;
      voices |= 0x1;

      if (rb->get_state(rb, RB_IS_VALID) == false)
      {
         no_samples = floorf((no_samples/samps_period)+1)*samps_period;
         rb->set_parami(rb, RB_NO_SAMPLES, no_samples);
         rb->init(rb, false);
      }

      switch (ptype)
      {
      case AAX_OVERWRITE:
         rb->data_clear(rb, track);
         break;
      case AAX_MIX:
      {
         float ratio_orig = FNMINMAX(1.0f-ratio, 0.0f, 1.0f);

         ratio = 2.0f*(1.0f - ratio_orig);
         if ((wave >= AAX_1ST_WAVE && wave <= AAX_LAST_WAVE) ||
             (wave >= AAX_1ST_NOISE && wave <= AAX_LAST_NOISE))
         {
            ratio /= 2;
         }

         rb->data_multiply(rb, 0, 0, ratio_orig);
         break;
      }
      case AAX_RINGMODULATE:
         modulate = true;
         break;
      case AAX_ADD:
      default:
         break;
      }

      phasing = (spread < 0.0f) ? true : false;
      spread = fabsf(spread);
      scratch = _aaxDataCreate(2, no_samples+NOISE_PADDING, sizeof(float));
      if (scratch)
      {
         enum aaxSourceType noise = wtype & AAX_NOISE_MASK;
         if (wave >= AAX_1ST_WAVE && wave <= AAX_LAST_WAVE)
         {
            bool v1 = (version <= 1.0f) ? true : false;
            for (q=0; q<voices; ++q)
            {
               float ffact, nfw, nphase, nratio;

               nfw = (fw - hvoices*spread);
               if (phasing) nfw += (float)q*spread;
               samps_period = fs/nfw;
               ffact = (float)no_samples/(float)samps_period;
               nfw = nfw*ceilf(ffact)/ffact;
               nphase = phase + q*GMATH_2PI/voices;
               nratio = (q == hvoices) ? 0.8f*ratio : 0.6f*ratio;
               rv = rb->data_mix_waveform(rb, scratch, wtype, track, nfw,
                                         nratio, nphase, modulate, v1, limiter);
            }
         }

         if (noise >= AAX_1ST_NOISE && noise <= AAX_LAST_NOISE)
         {
            rv = rb->data_mix_noise(rb, scratch, noise, track, fs_mixer, pitch,
                                    ratio, seed, skip, modulate, limiter);
         }
         _aaxDataDestroy(scratch);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

/*
 * Convert the buffer to 24-bit
 */
static void
_aaxMuLaw2Linear(int32_t*ndata, uint8_t* data, unsigned int i)
{
   do {
      *ndata++ = _mulaw2linear(*data++) << 8;
   } while (--i);
}

static void
_aaxALaw2Linear(int32_t*ndata, uint8_t* data, unsigned int i)
{
   do {
      *ndata++ = _alaw2linear(*data++) << 8;
   } while (--i);
}

static void
_bufConvertDataToPCM24S(void *ndata, void *data, unsigned int samples, enum aaxFormat format)
{
   if (ndata)
   {
      int native_fmt = format & AAX_FORMAT_NATIVE;

      if (format != native_fmt)
      {
                                /* first convert to requested endianness */
         if ( ((format & AAX_FORMAT_LE) && is_bigendian()) ||
              ((format & AAX_FORMAT_BE) && !is_bigendian()) )
         {
            switch (native_fmt)
            {
            case AAX_PCM16S:
               _batch_endianswap16(data, samples);
               break;
            case AAX_PCM24S_PACKED:
               _batch_endianswap24(data, samples);
               break;
            case AAX_PCM24S:
            case AAX_PCM32S:
            case AAX_FLOAT:
               _batch_endianswap32(data, samples);
               break;
            case AAX_DOUBLE:
               _batch_endianswap64(data, samples);
               break;
            default:
               break;
            }
         }
                                /* then convert to proper signedness */
         if (format & AAX_FORMAT_UNSIGNED)
         {
            switch (native_fmt)
            {
            case AAX_PCM8S:
               _batch_cvt8u_8s(data, samples);
               break;
            case AAX_PCM16S:
               _batch_cvt16u_16s(data, samples);
               break;
            case AAX_PCM24S:
               _batch_cvt24u_24s(data, samples);
               break;
            case AAX_PCM32S:
               _batch_cvt32u_32s(data, samples);
               break;
            default:
               break;
            }
         }
      }

      switch(native_fmt)
      {
      case AAX_PCM8S:
         _batch_cvt24_8(ndata, data, samples);
         break;
      case AAX_IMA4_ADPCM:
         /* the ringbuffer uses AAX_PCM16S internally for AAX_IMA4_ADPCM */
      case AAX_PCM16S:
         _batch_cvt24_16(ndata, data, samples);
         break;
      case AAX_PCM24S_PACKED:
         _batch_cvt24_24_3(ndata, data, samples);
         break;
      case AAX_PCM32S:
         _batch_cvt24_32(ndata, data, samples);
         break;
      case AAX_FLOAT:
        _batch_cvt24_ps(ndata, data, samples);
         break;
      case AAX_DOUBLE:
         _batch_cvt24_pd(ndata, data, samples);
         break;
      case AAX_MULAW:
         _aaxMuLaw2Linear(ndata, data, samples);
         break;
      case AAX_ALAW:
         _aaxALaw2Linear(ndata, data, samples);
         break;
      default:
         break;
      }
   } /* ndata */
}

static _aaxRingBuffer*
_bufConvertDataToMixerFormat(_buffer_t *buf, _aaxRingBuffer *rb)
{
   _aaxRingBuffer *nrb = rb;
   unsigned int no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
   unsigned int tracks = rb->get_parami(rb, RB_NO_TRACKS);
   _handle_t *handle = buf->root;

   if (handle)
   {
      enum aaxRenderMode mode = handle->info->mode;
      const _aaxDriverBackend *be;

      if VALID_HANDLE(handle) {
         be = handle->backend.ptr;
      } else {
         be = ((_handle_t*)((*buf->mixer_info)->backend))->backend.ptr;
      }

      nrb = be->get_ringbuffer(0.0f, mode);
      if (nrb)
      {
         int32_t **src, **dst;
         unsigned int t, fmt;

         nrb->set_format(nrb, AAX_PCM24S, false);
         nrb->set_parami(nrb, RB_NO_TRACKS, buf->info.no_tracks);
         nrb->set_parami(nrb, RB_NO_SAMPLES, buf->info.no_samples);
         nrb->set_paramf(nrb, RB_FREQUENCY, buf->info.rate);
         nrb->init(nrb, false);

         nrb->set_paramf(nrb, RB_LOOPPOINT_END, buf->info.loop_end/buf->info.rate);
         nrb->set_paramf(nrb, RB_LOOPPOINT_START, buf->info.loop_start/buf->info.rate);

         nrb->set_parami(nrb, RB_SAMPLED_RELEASE, buf->info.sampled_release);
         nrb->set_parami(nrb, RB_LOOPING, buf->info.loop_count);

         fmt = rb->get_parami(rb, RB_FORMAT);
         src = (int32_t**)rb->get_tracks_ptr(rb, RB_READ);
         dst = (int32_t**)nrb->get_tracks_ptr(nrb, RB_WRITE);
         for (t=0; t<tracks; ++t) {
            _bufConvertDataToPCM24S(dst[t], src[t], no_samples, fmt);
         }
         nrb->release_tracks_ptr(nrb);
      }
      else {
         nrb = rb;
      }
   }

   return nrb;
}

static void
_aaxLinear2MuLaw(uint8_t* ndata, int32_t* data, unsigned int i)
{
   do {
      *ndata++ = _linear2mulaw(*data++ >> 8);
   } while (--i);
}

static void
_aaxLinear2ALaw(uint8_t* ndata, int32_t* data, unsigned int i)
{
   do {
      *ndata++ = _linear2alaw(*data++ >> 8);
   } while (--i);
}

/*
 * Incompatible with MS-IMA which specifies a different way of interleaving.
 */
void
_pcm_cvt_lin_to_ima4_block(uint8_t* ndata, int32_t* data,
                                unsigned block_smp, int16_t* sample,
                                uint8_t* index, short step)
{
   unsigned int i, size = (block_smp/2)*2;
   int16_t header;
   uint8_t nibble;

   header = *sample;
   *ndata++ = header & 0xFF;
   *ndata++ = header >> 8;
   *ndata++ = *index;
   *ndata++ = 0;

   for (i=0; i<size; i += 2)
   {
      int16_t nsample;

      nsample = *data >> 8;
      _linear2adpcm(sample, nsample, &nibble, index);
      data += step;
      *ndata = nibble;

      nsample = *data >> 8;
      _linear2adpcm(sample, nsample, &nibble, index);
      data += step;
      *ndata++ |= nibble << 4;
   }

   if (i < block_smp) *ndata++ = 0;
}

static void
_aaxLinear2IMA4(uint8_t* ndata, int32_t* data, unsigned int samples, unsigned block_smp, unsigned tracks)
{
   unsigned int i, no_blocks, blocksize;
   int16_t sample = 0;
   uint8_t index = 0;

   no_blocks = samples/block_smp;
   blocksize = IMA4_SMP_TO_BLOCKSIZE(block_smp);

   for(i=0; i<no_blocks; i++)
   {
      _pcm_cvt_lin_to_ima4_block(ndata, data, block_smp, &sample, &index, tracks);
      ndata += blocksize*tracks;
      data += block_smp*tracks;
   }

   if (no_blocks*block_smp < samples)
   {
      unsigned int rest = (no_blocks+1)*block_smp - samples;

      samples = block_smp - rest;
      _pcm_cvt_lin_to_ima4_block(ndata, data, samples, &sample, &index, tracks);

      ndata += IMA4_SMP_TO_BLOCKSIZE(samples);
      memset(ndata, 0, rest/2);
   }
}

static void
_bufConvertDataFromPCM24S(void *ndata, void *data, unsigned int tracks, unsigned int no_samples, enum aaxFormat format, unsigned int blocksize)
{
   if (ndata)
   {
      int native_fmt = format & AAX_FORMAT_NATIVE;
      unsigned int samples = tracks*no_samples;

      if (format != native_fmt)
      {
                                /* first convert to requested endianness */
         if ( ((format & AAX_FORMAT_LE) && is_bigendian()) ||
              ((format & AAX_FORMAT_BE) && !is_bigendian()) )
         {
            switch (native_fmt)
            {
            case AAX_PCM16S:
               _batch_endianswap16(data, samples);
               break;
            case AAX_PCM24S_PACKED:
               _batch_endianswap24(data, samples);
               break;
            case AAX_PCM24S:
            case AAX_PCM32S:
            case AAX_FLOAT:
               _batch_endianswap32(data, samples);
               break;
            case AAX_DOUBLE:
               _batch_endianswap64(data, samples);
               break;
            default:
               break;
            }
         }
                                /* then convert to proper signedness */
         if (format & AAX_FORMAT_UNSIGNED)
         {
            switch (native_fmt)
            {
            case AAX_PCM8S:
               _batch_cvt8u_8s(data, samples);
               break;
            case AAX_PCM16S:
               _batch_cvt16u_16s(data, samples);
               break;
            case AAX_PCM24S:
               _batch_cvt24u_24s(data, samples);
               break;
            case AAX_PCM32S:
               _batch_cvt32u_32s(data, samples);
               break;
            default:
               break;
            }
         }
      }

      // convert to the requested format
      switch(native_fmt)
      {
      case AAX_PCM8S:
         _batch_cvt8_24(ndata, data, samples);
         break;
      case AAX_PCM16S:
         _batch_cvt16_24(ndata, data, samples);
         break;
      case AAX_PCM24S_PACKED:
         _batch_cvt24_3_24(ndata, data, samples);
         break;
      case AAX_PCM32S:
         _batch_cvt32_24(ndata, data, samples);
         break;
      case AAX_FLOAT:
         _batch_cvtps_24(ndata, data, samples);
         break;
      case AAX_DOUBLE:
         _batch_cvtpd_24(ndata, data, samples);
         break;
      case AAX_MULAW:
         _aaxLinear2MuLaw(ndata, data, samples);
         break;
      case AAX_ALAW:
         _aaxLinear2ALaw(ndata, data, samples);
         break;
      case AAX_IMA4_ADPCM:
      {
         int block_smp = IMA4_BLOCKSIZE_TO_SMP(blocksize);
         unsigned t;
         for (t=0; t<tracks; t++)
         {
            uint8_t *dst = (uint8_t *)ndata + t*blocksize;
            int32_t *src = (int32_t *)data + t;
            _aaxLinear2IMA4(dst, src, no_samples, block_smp, tracks);
         }
         break;
      }
      default:
         break;
      }
   } /* ndata */
}

/*
 * Convert 4-bit IMA to 16-bit PCM
 *
 * IMA4 coding of one block is as follows
 * +--+-+--+-+----------------+----------------+----------------+----- ...
 * |P1|I|P2|I| Data           | Data           | Data           | Data
 * +--+-+--+-+----------------+----------------+----------------+----- ...
 * | T1 | T2 | T1             | T2             | T1             | T2
 * P: Predictor - 2 bytes
 * I: Index - 1 byte
 * Data: 4*4 bytes of 4-bit nibbles: delta offset per sample
 * T: Track no.
 */
static void
_aaxRingBufferIMA4ToPCM16(int32_t **__restrict dptr, const void *__restrict sptr, int no_tracks, int blocksize, unsigned int no_blocks)
{
   int16_t *d[_AAX_MAX_SPEAKERS];
   uint8_t *s = (uint8_t *)sptr;
   int b, t;

   if (no_tracks > _AAX_MAX_SPEAKERS) {
      return;
   }

   /* copy buffer pointers */
   for(t=0; t<no_tracks; ++t) {
      d[t] = (int16_t*)dptr[t];
   }

   for (b=0; b<no_blocks; ++b)
   {
      uint8_t nibble, index[_AAX_MAX_SPEAKERS];
      int16_t predictor[_AAX_MAX_SPEAKERS];
      int j;

      // block header
      for (t=0; t<no_tracks; ++t)
      {
         predictor[t] = *s++;
         predictor[t] |= *s++ << 8;

         index[t] = *s++;
         s++;
      }

      // block data
      for (j=no_tracks*4; j<blocksize; j += no_tracks*4)
      {
         for (t=0; t<no_tracks; ++t)
         {
            int q;
            for (q=0; q<4; ++q)
            {
               nibble = *s & 0xf;
               *d[t]++ = _adpcm2linear(nibble, &predictor[t], &index[t]);

               nibble = *s++ >> 4;
               *d[t]++ = _adpcm2linear(nibble, &predictor[t], &index[t]);
            }
         }
      }
   }
}

static _aaxRingBuffer*
_bufSetDataInterleaved(_buffer_t *buf, _aaxRingBuffer *rb, const void *dbuf, unsigned blocksize)
{
   unsigned int no_blocks, no_samples, no_tracks;
   unsigned int rb_format, bps, tracksize;
   _aaxRingBuffer *rv = rb;
   const void* data;
   int32_t **tracks;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(rb != 0);
   assert(dbuf != 0);

   data = dbuf;

   // _bufCreateFromAAXS sets part of the ringbuffer information struct
   // so do not clear the ringbuffer.
   // rb->set_state(rb, RB_CLEARED);

   rb_format = rb->get_parami(rb, RB_FORMAT);
   bps = rb->get_parami(rb, RB_BYTES_SAMPLE);
   no_blocks = rb->get_parami(rb, RB_NO_BLOCKS);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
   tracksize = no_samples * bps;
#if 0
 printf("format: %x, bps: %i\n", rb_format, bps);
 printf("no_blocks:  %i\n", no_blocks);
 printf("no_samples: %i\n", no_samples);
 printf("no_tracks:  %i\n", no_tracks);
 printf("tracksize:  %i\n", tracksize);
#endif

   switch (rb_format)
   {
   case AAX_IMA4_ADPCM:
      tracks = rb->get_tracks_ptr(rb, RB_WRITE);
      _aaxRingBufferIMA4ToPCM16(tracks, data, no_tracks, blocksize, no_blocks);
      rb->release_tracks_ptr(rb);
      break;
   case AAX_PCM24S:
      tracks = rb->get_tracks_ptr(rb, RB_WRITE);
      _batch_cvt24_24_intl(tracks, data, 0, no_tracks, no_samples);
      rb->release_tracks_ptr(rb);
      rb->set_parami(rb, RB_FORMAT, AAX_PCM24S);
      break;
   case AAX_PCM32S:
      tracks = rb->get_tracks_ptr(rb, RB_WRITE);
      _batch_cvt24_32_intl(tracks, data, 0, no_tracks, no_samples);
      rb->release_tracks_ptr(rb);
      rb->set_parami(rb, RB_FORMAT, AAX_PCM24S);
      break;
   case AAX_FLOAT:
      tracks = rb->get_tracks_ptr(rb, RB_WRITE);
      _batch_cvt24_ps_intl(tracks, data, 0, no_tracks, no_samples);
      rb->release_tracks_ptr(rb);
      rb->set_parami(rb, RB_FORMAT, AAX_PCM24S);
      break;
   case AAX_DOUBLE:
      tracks = rb->get_tracks_ptr(rb, RB_WRITE);
      _batch_cvt24_pd_intl(tracks, data, 0, no_tracks, no_samples);
      rb->release_tracks_ptr(rb);
      rb->set_parami(rb, RB_FORMAT, AAX_PCM24S);
      break;
   case AAX_PCM24S_PACKED:
      tracks = rb->get_tracks_ptr(rb, RB_WRITE);
      _batch_cvt24_24_3intl(tracks, data, 0, no_tracks, no_samples);
      rb->release_tracks_ptr(rb);
      rb->set_parami(rb, RB_FORMAT, AAX_PCM24S);
      break;
   default:
      tracks = rb->get_tracks_ptr(rb, RB_WRITE);
      if (no_tracks == 1) {
         memcpy(tracks[0], data, tracksize);
      }
      else /* multi-channel */
      {
         unsigned int frame_size = no_tracks * bps;
         unsigned int t;

         for (t=0; t<no_tracks; t++)
         {
            char *sptr, *dptr;
            unsigned int i;

            sptr = (char *)data + t*bps;
            dptr = (char *)tracks[t];
            i = no_samples;
            do
            {
               memcpy(dptr, sptr, bps);
               sptr += frame_size;
               dptr += bps;
            }
            while (--i);
         }
      }
      rb->release_tracks_ptr(rb);
      if (buf->to_mixer) rv = _bufConvertDataToMixerFormat(buf, rb);
   } /* switch */

   return rv;
}

static void**
_bufGetDataPitchLevels(_buffer_t *handle)
{
   _aaxRingBuffer *rb = _bufGetRingBuffer(handle, NULL, 0);
// _aaxRingBufferData *rbi = rb->handle;
// _aaxRingBufferSample *rbd = rbi->sample;
   unsigned int no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
   int format = handle->info.fmt & ~AAX_SPECIAL;
   uint32_t b, offs, size;
   void **data = NULL;
   void **tracks;
   size_t *d;
   char *ptr;

   if (handle->info.no_tracks != 1) return data;
   if (handle->mip_levels <= 1) return data;
   if (format != AAX_PCM24S && format != AAX_FLOAT) return data;
   if (rb->get_parami(rb, RB_FORMAT) != AAX_PCM24S) return data;
   /**
    * format:
    * 1. an array of offsets to the nth buffer followd by a value of zero.
    * 2. the full size of the buffer as a size_t type.
    * 3. followed by the data for all pitch levels.
    *    - the data format is AAX_PCM24S
    */
   offs = (handle->mip_levels+2)*sizeof(void*);
   size = 2*no_samples*sizeof(int32_t);
   data = (void**)_aax_malloc(&ptr, offs, size);
   if (data == NULL)
   {
      _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      return data;
   }


   d = (size_t*)data;
   for (b=0; b<handle->mip_levels; ++b)
   {
      uint32_t len;

      d[b] = (size_t)ptr;

      rb = handle->ringbuffer[b];
      if (!rb) break;

      no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
      len = no_samples*sizeof(int32_t);
      offs += len;

      assert(offs <= size);

      tracks = (void**)rb->get_tracks_ptr(rb, RB_READ);
      memcpy(ptr, tracks[0], len);
      rb->release_tracks_ptr(rb);

      if (fabsf(handle->gain - 1.0f) > 0.05f) {
         _batch_imul_value(ptr, ptr, sizeof(int32_t), no_samples, handle->gain);
      }
      ptr += len;
   }
   d[b++] = 0;
   d[b] = offs;

   return data;
}

void
_bufGetDataInterleaved(_aaxRingBuffer *rb, void* data, unsigned int samples, unsigned int channels, float fact)
{
   unsigned int fmt, bps, no_samples, t, no_tracks;
   void **ptr, **tracks;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(rb != 0);
   assert(data != 0);

   fmt = rb->get_parami(rb, RB_FORMAT);
   bps = rb->get_parami(rb, RB_BYTES_SAMPLE);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
   if (no_tracks > channels) no_tracks = channels;

   assert(samples >= (unsigned int)(fact*no_samples));

   // do not alter the data, we convert to and from float24 when required
   tracks = (void**)rb->get_tracks_ptr(rb, RB_NONE);

   fact = 1.0f/fact;
   ptr = (void**)tracks;
   if (fact != 1.0f)
   {
      unsigned int size = samples*bps;
      char *p;

      if (fmt == AAX_PCM24S)
      {
         size_t offs = no_tracks*sizeof(void*);
         tracks = (void**)_aax_malloc(&p, offs, no_tracks*size);
         for (t=0; t<no_tracks; t++)
         {
            tracks[t] = p;
            if (rb->get_parami(rb, RB_IS_MIXER_BUFFER) == false) {
               _batch_cvtps24_24(ptr[t], ptr[t], no_samples);
            }
            _batch_resample_float(tracks[t], ptr[t], 0, samples, 0, fact);
            _batch_cvt24_ps24(tracks[t], tracks[t], samples);
            p += size;
         }
      }
      else
      {
         size_t scratch_size, offs, len;
         MIX_T **scratch;
         char *sptr;

         offs = 2*sizeof(MIX_T*);
         scratch_size = SIZE_ALIGNED(samples*sizeof(MIX_T));
         len = SIZE_ALIGNED(no_samples*sizeof(MIX_T));
         scratch_size += len;

         scratch = (MIX_T**)_aax_malloc(&sptr, offs, scratch_size);
         scratch[0] = (MIX_T*)sptr;
         scratch[1] = (MIX_T*)(sptr + len);

         offs = no_tracks*sizeof(void*);
         tracks = (void**)_aax_malloc(&p, offs, no_tracks*size);
         for (t=0; t<no_tracks; t++)
         {
            tracks[t] = p;
            _bufConvertDataToPCM24S(scratch[0], ptr[t], no_samples, fmt);

            _batch_cvtps24_24(scratch[0], scratch[0], no_samples);
            _batch_resample_float(scratch[1], scratch[0], 0, samples, 0, fact);
            _batch_cvt24_ps24(scratch[1], scratch[1], samples);
            _bufConvertDataFromPCM24S(tracks[t], scratch[1], 1, samples, fmt,1);
            p += size;
         }
         free(scratch);
      }
   }

   if (no_tracks == 1) {
      memcpy(data, tracks[0], samples*bps);
   }
   else
   {
      for (t=0; t<no_tracks; t++)
      {
         uint8_t *s = (uint8_t*)tracks[t];
         uint8_t *d = (uint8_t *)data + t*bps;
         unsigned int i =  samples;
         do
         {
            memcpy(d, s, bps);
            d += no_tracks*bps;
            s += bps;
         }
         while (--i);
      }
   }
   rb->release_tracks_ptr(rb);

   if (ptr != tracks) free(tracks);
}

static void
_bufApplyBitCrusherFilter(_buffer_t* handle, _filter_t *filter, int layer)
{
   _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
   int32_t **sbuf = (int32_t**)rb->get_tracks_ptr(rb, RB_RW);
   _aaxFilterInfo* slot = filter->slot[0];
   float ratio = slot->param[AAX_NOISE_LEVEL];
   float level = slot->param[AAX_LFO_OFFSET];
   unsigned int bps, no_samples;
   int32_t *dptr = sbuf[layer];

   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
   bps = sizeof(float); // rb->get_parami(rb, RB_BYTES_SAMPLE);
   if (dptr)
   {
      if (level > 0.01f)
      {
         level = powf(2.0f, 8+sqrtf(level)*13.5f);      // (24-bits/sample)
         _batch_imul_value(dptr, dptr, bps, no_samples, 1.0f/level);
         _batch_imul_value(dptr, dptr, bps, no_samples, level);
      }

      if (ratio > 0.01f)
      {
         unsigned int i;

         ratio *= ((0.25 * (double)AAX_PEAK_MAX)/(double)UINT64_MAX);
         for (i=0; i<no_samples; ++i) {
            dptr[i] += ratio*xoroshiro128plus();
         }
      }
   }
   rb->release_tracks_ptr(rb);
}



static void
_bufApplyFrequencyFilter(_buffer_t* handle, _filter_t *filter, int layer)
{
   _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
   _aaxRingBufferData *rbi = rb->handle;
   _aaxRingBufferSample *rbd = rbi->sample;
   unsigned int bps, no_samples;
   float *dptr = rbd->track[layer];
   float *sptr, *tmp;

   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
   bps = rb->get_parami(rb, RB_BYTES_SAMPLE);

   // AAXS defined buffers have just one track, but could have multiple layers
   assert(rb->get_parami(rb, RB_NO_TRACKS) == 1);
   assert(bps == 4);

   sptr = _aax_aligned_alloc(4*no_samples*bps);
   if (sptr)
   {
      _aaxRingBufferFreqFilterData *data = filter->slot[0]->data;
      _aaxRingBufferFreqFilterData *data_hf = filter->slot[1]->data;

      _batch_cvtps24_24(sptr, dptr, no_samples);

      tmp = sptr+no_samples;
      memcpy(tmp, sptr, no_samples*bps);

      if (!data_hf) /* frequency filter */
      {
         rbd->freqfilter(sptr, sptr, 0, 2*no_samples, data);
         if (data->state && (data->low_gain > LEVEL_128DB))	// Bessel
         {
            _batch_cvtps24_24(dptr, dptr, no_samples);
            rbd->add(tmp, dptr, no_samples, data->low_gain, 0.0f);
         }
      }
      _batch_cvt24_ps24(dptr, tmp, no_samples);

      _aax_aligned_free(sptr);
   }
}

static void
_bufApplyEqualizer(_buffer_t* handle, _filter_t *filter, int layer)
{
   _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
   _aaxRingBufferData *rbi = rb->handle;
   _aaxRingBufferSample *rbd = rbi->sample;
   unsigned int bps, no_samples;
   MIX_T *dptr = rbd->track[layer];
   MIX_T *sptr, *tmp;

   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
   bps = rb->get_parami(rb, RB_BYTES_SAMPLE);

   // AAXS defined buffers have just one track, but could have multiple layers
   assert(rb->get_parami(rb, RB_NO_TRACKS) == 1);
   assert(bps == 4);

   sptr = _aax_aligned_alloc(4*no_samples*bps);
   if (sptr)
   {
      _aaxRingBufferFreqFilterData *data[_MAX_PARAM_EQ];
      int s;

      for (s=0; s<_MAX_PARAM_EQ; ++s) {
         data[s] = filter->slot[s]->data;
      }

      _batch_cvtps24_24(sptr, dptr, no_samples);

      tmp = sptr+no_samples;
      memcpy(tmp, sptr, no_samples*bps);

      _equalizer_run(rbd, sptr, NULL, 0, 2*no_samples, 0, data);

      _batch_cvt24_ps24(dptr, tmp, no_samples);

      _aax_aligned_free(sptr);
   }
}

static void
_bufApplyDistortionEffect(_buffer_t* handle, _effect_t *effect, int layer)
{
   _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
   int32_t **sbuf = (int32_t**)rb->get_tracks_ptr(rb, RB_RW);
   _aaxRingBufferData *rbi = rb->handle;
   _aaxRingBufferSample *rbd = rbi->sample;
   _aaxEffectInfo* slot = effect->slot[layer];
   float fact = slot->param[AAX_DISTORTION_FACTOR];
   float clip = slot->param[AAX_CLIPPING_FACTOR];
   float mix  = slot->param[AAX_MIX_FACTOR];
   float asym = slot->param[AAX_ASYMMETRY];
   unsigned int bps, no_samples;
   void *dptr = sbuf[0];
   void *sptr;

   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
   bps = sizeof(float); // rb->get_parami(rb, RB_BYTES_SAMPLE);
   sptr = _aax_aligned_alloc(no_samples*bps);
   if (sptr)
   {
      if (mix > 0.01f)
      {
         float mix_factor;

         _batch_cvtps24_24(dptr, dptr, no_samples);
         memcpy(sptr, dptr, no_samples*bps);

         /* make dptr the wet signal */
         if (fact > 0.0013f) {
            rbd->multiply(dptr, dptr, bps, no_samples, 1.0f+64.f*fact);
         }

         if ((fact > 0.01f) || (asym > 0.01f)) {
            _aaxRingBufferLimiter(dptr, no_samples, clip, 4*asym);
         }

         /* mix with the dry signal */
         mix_factor = mix/(0.5f+powf(fact, 0.25f));
         rbd->multiply(dptr, dptr, bps, no_samples, mix_factor);
         if (mix < 0.99f) {
            rbd->add(dptr, sptr, no_samples, 1.0f-mix, 0.0f);
         }

         _batch_cvt24_ps24(dptr, dptr, no_samples);
      }
      _aax_aligned_free(sptr);
   }
   rb->release_tracks_ptr(rb);
}

static void
_bufApplyWaveFoldEffect(_buffer_t* handle, _effect_t *effect, int layer)
{
   _aaxRingBufferWaveFoldData *wavefold = effect->slot[0]->data;
   _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
   _aaxRingBufferData *rbi = rb->handle;
   _aaxRingBufferSample *rbd = rbi->sample;
   MIX_T *dptr = rbd->track[layer];
   unsigned int no_samples;

   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);

   _batch_cvtps24_24(dptr, dptr, no_samples);
   wavefold->run(dptr, 0, no_samples, wavefold, NULL, 0);
   _batch_cvt24_ps24(dptr, dptr, no_samples);
}
