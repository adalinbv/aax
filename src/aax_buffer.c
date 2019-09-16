 /*
 * Copyright 2007-2019 by Erik Hofman.
 * Copyright 2009-2019 by Adalin B.V.
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

static _aaxRingBuffer* _bufGetRingBuffer(_buffer_t*, _handle_t*, unsigned char);
static _aaxRingBuffer* _bufDestroyRingBuffer(_buffer_t*, unsigned char);
static int _bufProcessWaveform(aaxBuffer, float, float, float, float, unsigned char, int, float, enum aaxWaveformType, float, enum aaxProcessingType, limitType);
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
   aaxBuffer rv = NULL;

   if ((native_fmt < AAX_FORMAT_MAX) && (samples*tracks > 0))
   {
      _buffer_t* buf = calloc(1, sizeof(_buffer_t));
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
         buf->ref_counter = 1;
         buf->pitch_levels = 1;

         buf->info.tracks = tracks;
         buf->info.no_samples = samples;
         buf->info.blocksize = blocksize;
         buf->mipmap = AAX_FALSE;
         buf->pos = 0;
         buf->info.fmt = format;
         buf->info.freq = 0.0f;
         buf->gain = 1.0f;
         buf->mixer_info = VALID_HANDLE(handle) ? &handle->info : &_info;
         buf->root = handle;
         buf->ringbuffer[0] = _bufGetRingBuffer(buf, handle, 0);
         buf->to_mixer = AAX_FALSE;

         /* explicit request not to convert */
         env = getenv("AAX_USE_MIXER_FMT");
         if (env && !_aax_getbool(env)) {
            buf->to_mixer = AAX_FALSE;
         }

         /* sound is not mono */
         else if (tracks != 1) {
            buf->to_mixer = AAX_FALSE;
         }

         /* more than 500Mb free memory is available, convert */
         else if (_aax_get_free_memory() > (500*1024*1024)) {
            buf->to_mixer = AAX_TRUE;
         }

         rv = (aaxBuffer)buf;
      }
      if (buf == NULL) {
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      }
   }
   else
   {
      if (native_fmt >= AAX_FORMAT_MAX) {
         _aaxErrorSet(AAX_INVALID_ENUM);
      } else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxBufferSetSetup(aaxBuffer buffer, enum aaxSetupType type, unsigned int setup)
{
   _buffer_t* handle = get_buffer(buffer, __func__);
   int rv = __release_mode;

   if (!rv && handle)
   {
      _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
      if (!rb) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else {
         rv = AAX_TRUE;
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
            rv = AAX_TRUE;
         }
         else  _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      case AAX_FREQUENCY:
         if ((setup >= 1000) && (setup <= 96000))
         {
            if (rb && !handle->info.freq) {
               rb->set_paramf(rb, RB_FREQUENCY, (float)setup);
            }
            handle->info.freq = (float)setup;
            rv = AAX_TRUE;
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
            handle->info.tracks = setup;
            rv = AAX_TRUE;
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
            rv = AAX_TRUE;
         }
         else _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      }
      case AAX_TRACKSIZE:
         if (rb)
         {
            rv = rb->set_parami(rb, RB_TRACKSIZE, setup);
            if (!rv) {
               _aaxErrorSet(AAX_INVALID_PARAMETER);
            }
         }
         tmp = handle->info.tracks * aaxGetBitsPerSample(handle->info.fmt);
         handle->info.no_samples = setup*8/tmp;
         rv = AAX_TRUE;
         break;
      case AAX_LOOP_START:
         if (setup < handle->info.no_samples)
         {
            if (rb)
            {
               int looping = (setup < handle->info.loop_end) ? AAX_TRUE : AAX_FALSE;
               rb->set_parami(rb, RB_LOOPPOINT_START, setup);
               rb->set_parami(rb, RB_LOOPING, looping);
            }
            handle->info.loop_start = setup;
            rv = AAX_TRUE;
         }
         else _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      case AAX_LOOP_END:
         if (setup < handle->info.no_samples)
         {
            if (rb)
            {
               int looping = (handle->info.loop_start < setup) ? AAX_TRUE : AAX_FALSE;
               rb->set_parami(rb, RB_LOOPPOINT_END, setup);
               rb->set_parami(rb, RB_LOOPING, looping);
            }
            handle->info.loop_end = setup;
            rv = AAX_TRUE;
         }
         else _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      case AAX_BLOCK_ALIGNMENT:
         if (setup > 1)
         {
            if (handle->info.fmt == AAX_IMA4_ADPCM)
            {
               handle->info.blocksize = setup;
               rv = AAX_TRUE;
            }
         }
         else if (handle->info.fmt != AAX_IMA4_ADPCM)
         {
            handle->info.blocksize = setup;
            rv = AAX_TRUE;
         }
         break;
      case AAX_SAMPLED_RELEASE:
         handle->info.sampled_release = setup ? AAX_TRUE : AAX_FALSE;
         rv = AAX_TRUE;
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   return rv;
}

AAX_API unsigned int AAX_APIENTRY
aaxBufferGetSetup(const aaxBuffer buffer, enum aaxSetupType type)
{
   _buffer_t* handle = get_buffer(buffer, __func__);
   int rv = __release_mode;

   if (!rv && handle)
   {
      _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
      if (!rb) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
      switch(type)
      {
      case AAX_FREQUENCY:
         rv = (unsigned int)handle->info.freq;
         break;
      case AAX_UPDATE_RATE:
         rv = (unsigned int)handle->info.base_frequency;
         break;
      case AAX_TRACKS:
         rv = handle->info.tracks;
         break;
      case AAX_FORMAT:
         rv = handle->info.fmt;
         break;
      case AAX_TRACKSIZE:
         if (handle->info.fmt & AAX_AAXS)
         {
            if (handle->aaxs) {
               rv = strlen(handle->aaxs);
            }
         }
         else if (handle->info.freq)
         {
            float fact = 1.0f;
            if (rb) {
               fact = handle->info.freq / rb->get_paramf(rb, RB_FREQUENCY);
            }
            rv = handle->info.no_samples - handle->pos;
            rv *= (unsigned int)(fact*aaxGetBitsPerSample(handle->info.fmt));
            rv /= 8;
         }
         else _aaxErrorSet(AAX_INVALID_STATE);
         break;
      case AAX_NO_SAMPLES:
         if (handle->info.freq && !(handle->info.fmt & AAX_AAXS))
         {
            float fact = 1.0f;
            if (rb) {
               fact = handle->info.freq / rb->get_paramf(rb, RB_FREQUENCY);
            }
            rv = (unsigned int)(fact*(handle->info.no_samples - handle->pos));
         } else {
            _aaxErrorSet(AAX_INVALID_STATE);
         }
         break;
      case AAX_LOOP_START:
         rv = handle->info.loop_start;
         break;
      case AAX_LOOP_END:
         rv = handle->info.loop_end;
         break;
      case AAX_BLOCK_ALIGNMENT:
         rv = handle->info.blocksize;
         break;
      case AAX_POSITION:
         rv = handle->pos;
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
      case AAX_PRESSURE_MODE:
         rv = handle->pressure_mode;
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxBufferSetData(aaxBuffer buffer, const void* d)
{
   _buffer_t* handle = get_buffer(buffer, __func__);
   int rv = __release_mode;

   if (!rv && handle)
   {
      if (!d) {
          _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
      else if ((handle->info.fmt & AAX_SPECIAL) != 0)
      {
         _aaxRingBuffer *rb = _bufGetRingBuffer(handle, NULL, 0);
         if (!rb || rb->get_parami(rb, RB_NO_SAMPLES) == 0) {
            _aaxErrorSet(AAX_INVALID_STATE);
         } else {
            rv = AAX_TRUE;
         }
      }
      else {
         rv = AAX_TRUE;
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
      char fmt_bps, *m;

      rb->init(rb, AAX_FALSE);
      tracks = rb->get_parami(rb, RB_NO_TRACKS);
      no_samples = rb->get_parami(rb, RB_NO_SAMPLES);

      buf_samples = tracks*no_samples;

				/* do we need to convert to native format? */
      native_fmt = format & AAX_FORMAT_NATIVE;
      if (format & ~AAX_FORMAT_NATIVE)
      {
         fmt_bps = _aaxFormatsBPS[native_fmt];
         ptr = (void**)_aax_malloc(&m, 0, buf_samples*fmt_bps);
         if (!ptr)
         {
            _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            return rv;
         }

         _aax_memcpy(m, data, buf_samples*fmt_bps);
         data = m;
					/* first convert to native endianness */
         if ( ((format & AAX_FORMAT_LE) && is_bigendian()) ||
              ((format & AAX_FORMAT_BE) && !is_bigendian()) )
         {
            switch (native_fmt)
            {
            case AAX_PCM16S:
               _batch_endianswap16(data, buf_samples);
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

      rv = AAX_TRUE;
      if (ptr) _aax_free(ptr);
   }
   return rv;
}

AAX_API int AAX_APIENTRY aaxBufferProcessWaveform(aaxBuffer buffer, float rate, enum aaxWaveformType wtype, float ratio, enum aaxProcessingType ptype)
{
   return _bufProcessWaveform(buffer, rate, 0.0f, 1.0f, rate, 0, 1, 0.0f, wtype, ratio, ptype, 0);
}

AAX_API void** AAX_APIENTRY
aaxBufferGetData(const aaxBuffer buffer)
{
   _buffer_t* handle = get_buffer(buffer, __func__);
   enum aaxFormat user_format;
   int rv = __release_mode;
   void** data = NULL;

   if (!rv && handle)
   {
      user_format = handle->info.fmt;
      if (user_format != AAX_AAXS16S && user_format != AAX_AAXS24S)
      {
         if (!handle->info.freq) {
            _aaxErrorSet(AAX_INVALID_STATE);
         }
         else
         {
            _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
            if (!rb) {
               _aaxErrorSet(AAX_INVALID_STATE);
            } else {
               rv = AAX_TRUE;
            }
         }
      }
      else {
         rv = AAX_TRUE;
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
         rv = AAX_FALSE; /* we're done */
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
      fact = handle->info.freq / rb->get_paramf(rb, RB_FREQUENCY);
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

AAX_API int AAX_APIENTRY
aaxBufferDestroy(aaxBuffer buffer)
{
   _buffer_t* handle = get_buffer(buffer, __func__);
   int rv = AAX_FALSE;
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
   int rv = __release_mode;
   _buffer_t* buf = NULL;

   if (!rv && handle) {
      rv = AAX_TRUE;
   }

   if (rv)
   {
      _buffer_info_t info;
      char **ptr;

      ptr = _bufGetDataFromStream(url, &info);
      if (ptr)
      {
         buf = aaxBufferCreate(config, info.no_samples, info.tracks, info.fmt);
         if (buf)
         {
             _aaxRingBuffer* rb = _bufGetRingBuffer(buf, NULL, 0);

             if(buf->url) free(buf->url);
             buf->url = strdup(url);

             memcpy(&buf->info, &info, sizeof(_buffer_info_t));

             if (rb)
             {
                rb->set_paramf(rb, RB_FREQUENCY, buf->info.freq);
                rb->set_parami(rb, RB_LOOPPOINT_END, buf->info.loop_end);
                rb->set_parami(rb, RB_LOOPPOINT_START, buf->info.loop_start);
                rb->set_parami(rb, RB_LOOPING, buf->info.loop_count);
             }


             if ((aaxBufferSetData(buf, ptr[0])) == AAX_FALSE) {
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

AAX_API int AAX_APIENTRY
aaxBufferWriteToFile(aaxBuffer buffer, const char *file, enum aaxProcessingType type)
{
   int rv = AAX_FALSE;
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
      freq = handle->info.freq;
      _aaxFileDriverWrite(file, type, *data, samples, freq, tracks, format);
      free(data);
#endif
      rv = AAX_TRUE;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

static void _bufApplyBitCrusherFilter(_buffer_t*, _filter_t*);
static void _bufApplyFrequencyFilter(_buffer_t*, _filter_t*);
static void _bufApplyDistortionEffect(_buffer_t*, _effect_t*);


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
  1     /* IMA4-ADPCM     */
};

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
   int rv = AAX_FALSE;
   if (handle)
   {
      if (--handle->ref_counter == 0)
      {
         unsigned char b;
         for (b=0; b<handle->pitch_levels; ++b) {
            handle->ringbuffer[b] = _bufDestroyRingBuffer(handle, b);
         }
         if (handle->aaxs) free(handle->aaxs);
         if (handle->url) free(handle->url);

         /* safeguard against using already destroyed handles */
         handle->id = FADEDBAD;
         free(handle);
      }
      rv = AAX_TRUE;
   }
   return rv;
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
         /* initialize the ringbuffer in native format only */
         rb->set_format(rb, buf->info.fmt & AAX_FORMAT_NATIVE, AAX_FALSE);
         rb->set_parami(rb, RB_NO_TRACKS, buf->info.tracks);
         rb->set_parami(rb, RB_NO_SAMPLES, buf->info.no_samples);
         rb->set_parami(rb, RB_LOOPPOINT_END, buf->info.loop_end);
         rb->set_parami(rb, RB_LOOPPOINT_START, buf->info.loop_start);
         rb->set_paramf(rb, RB_FREQUENCY, buf->info.freq);
//       rb->set_paramf(rb, RB_BLOCKSIZE, buf->info.blocksize);
         /* Postpone until aaxBufferSetData gets called
          * rb->init(rb, AAX_FALSE);
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
_bufGetDataFromStream(const char *url, _buffer_info_t *info)
{
   const _aaxDriverBackend *stream = &_aaxStreamDriverBackend;
   char **ptr = NULL;

   info->no_samples = 0;
   if (stream)
   {
      static const char *xcfg = "<?xml?><"COPY_TO_BUFFER">1</"COPY_TO_BUFFER">";
      void *id = stream->new_handle(AAX_MODE_READ);
      void *xid = xmlInitBuffer(xcfg, strlen(xcfg));

      // xid makes the stream return sound data in file format when capturing
      id = stream->connect(NULL, id, xid, url, AAX_MODE_READ);
      xmlClose(xid);

      if (id)
      {
         float refrate = _info->refresh_rate;
         float periodrate = _info->period_rate;
         int brate = _info->bitrate;
         int res;

         info->tracks = _info->no_tracks;
         info->freq = _info->frequency;
         info->fmt = _info->format;

         res = stream->setup(id, &refrate,
                                 &info->fmt, &info->tracks, &info->freq,
                                 &brate, AAX_FALSE, periodrate);
         if (res)
         {
            size_t no_bytes, datasize, bits;
            char *ptr2;

            bits = aaxGetBitsPerSample(info->fmt);
            info->blocksize = stream->param(id, DRIVER_BLOCK_SIZE);
            info->no_samples = stream->param(id, DRIVER_MAX_SAMPLES);
            no_bytes = (info->no_samples)*bits/8;

            no_bytes = ((no_bytes/info->blocksize)+1)*info->blocksize;
            datasize =  SIZE_ALIGNED(info->tracks*no_bytes);

            if (datasize < 64*1024*1024) // sanity check
            {
               size_t offs = 2 * sizeof(void*);
               ptr = (char**)_aax_calloc(&ptr2, offs, 2, datasize);
            }

            if (ptr)
            {
               void **dst = (void **)ptr;
               ssize_t res, offset = 0;
               ssize_t offs_packets = 0;

               dst[0] = ptr2;
               ptr2 += datasize;
               dst[1] = ptr2;

               // capture now returns native file format instead of PCM24S
               // in batched capturing mode
               do
               {
                  ssize_t offs = offs_packets;
                  size_t packets = info->no_samples;

                  assert(packets <= datasize*8/(info->tracks*bits));

                  res = stream->capture(id, dst, &offs, &packets,
                                            dst[1], datasize, 1.0f, AAX_TRUE);
                  offs_packets += packets;
                  if (res > 0) {
                     offset += res*8/(bits*info->tracks);
                  }
               }
               while (res >= 0);

               // get the actual number of samples
               info->no_samples = stream->param(id, DRIVER_MAX_SAMPLES);
               info->loop_count = stream->param(id, DRIVER_LOOP_COUNT);
               info->loop_start = stream->param(id, DRIVER_LOOP_START);
               info->loop_end = stream->param(id, DRIVER_LOOP_END);
               info->sampled_release =stream->param(id, DRIVER_SAMPLED_RELEASE);
               info->base_frequency = stream->param(id, DRIVER_BASE_FREQUENCY);
               info->low_frequency = stream->param(id, DRIVER_LOW_FREQUENCY);
               info->high_frequency = stream->param(id, DRIVER_HIGH_FREQUENCY);
               info->pitch_fraction = stream->param(id, DRIVER_PITCH_FRACTION);
               info->tremolo_rate = stream->param(id, DRIVER_TREMOLO_RATE);
               info->tremolo_depth = stream->param(id, DRIVER_TREMOLO_DEPTH);
               info->tremolo_sweep = stream->param(id, DRIVER_TREMOLO_SWEEP);
               info->vibrato_rate = stream->param(id, DRIVER_VIBRATO_RATE);
               info->vibrato_depth = stream->param(id, DRIVER_VIBRATO_DEPTH);
               info->vibrato_sweep = stream->param(id, DRIVER_VIBRATO_SWEEP);
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
   char *s, *u, *url, **data = NULL;
   int rv = 0;

   u = strdup(buffer->url);
   url = _aaxURLConstruct(u, file);
   free(u);

   s = strrchr(url, '.');
   if (!s || strcasecmp(s, ".aaxs")) {
      data = _bufGetDataFromStream(url, info);
#if 0
 printf("url: '%s'\n\tfmt: %x, tracks: %i, freq: %4.1f, samples: %li, blocksize: %li\n", url, info->fmt, info->tracks, info->freq, info->no_samples, info->blocksize);
#endif
   }
   free(url);

   if (data)
   {
      _aaxRingBuffer* rb = _bufGetRingBuffer(buffer, buffer->root, level);

      /* the internal buffer format for .aaxs files is signed 24-bit */
      {
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
         _batch_imul_value(*data, *data, sizeof(int32_t), info->no_samples, 256.0f);
      }

      info->fmt = AAX_PCM24S;
//    rb->set_format(rb, fmt, AAX_FALSE);
      rb->set_parami(rb, RB_NO_TRACKS, info->tracks);
      rb->set_paramf(rb, RB_FREQUENCY, info->freq);
      rb->set_parami(rb, RB_NO_SAMPLES, info->no_samples);

      rv = aaxBufferSetData(buffer, data[0]);
      free(data);

      rb->set_paramf(rb, RB_LOOPPOINT_END, info->loop_end/info->freq);
      rb->set_paramf(rb, RB_LOOPPOINT_START, info->loop_start/info->freq);
      rb->set_parami(rb, RB_LOOPING, info->loop_count);
   }

   return rv;
}

static int
_bufCreateWaveformFromAAXS(_buffer_t* handle, const void *xwid, float freq, unsigned int pitch_level, int voices, float spread, limitType limiter)
{
   enum aaxProcessingType ptype = AAX_OVERWRITE;
   enum aaxWaveformType wtype = AAX_SINE_WAVE;
   float phase, pitch, ratio, staticity;

   if (xmlAttributeExists(xwid, "voices"))
   {
      voices = _MINMAX(xmlAttributeGetInt(xwid, "voices"), 1, 11);
      if (xmlAttributeExists(xwid, "spread")) {
         spread = _MAX(xmlNodeGetDouble(xwid, "spread"), 0.01f);
         if (xmlAttributeGetBool(xwid, "phasing")) spread = -spread;
      }
   }

   if (xmlAttributeExists(xwid, "ratio")) {
      ratio = xmlAttributeGetDouble(xwid, "ratio");
   } else {
      ratio = xmlNodeGetDouble(xwid, "ratio");
   }
   if (xmlAttributeExists(xwid, "pitch")) {
      pitch = xmlAttributeGetDouble(xwid, "pitch");
   } else {
      pitch = xmlNodeGetDouble(xwid, "pitch");
   }
   if (xmlAttributeExists(xwid, "phase")) {
      phase = xmlAttributeGetDouble(xwid, "phase");
   } else {
      phase = xmlNodeGetDouble(xwid, "phase");
   }
   if (xmlAttributeExists(xwid, "staticity")) {
      staticity = xmlAttributeGetDouble(xwid, "staticity");
   } else {
      staticity = xmlNodeGetDouble(xwid, "staticity");
   }

   if (!xmlAttributeCompareString(xwid, "src", "brownian-noise")) {
       wtype = AAX_BROWNIAN_NOISE;
   }
   else if (!xmlAttributeCompareString(xwid, "src","white-noise")){
       wtype = AAX_WHITE_NOISE;
   }
   else if (!xmlAttributeCompareString(xwid, "src","pink-noise")) {
       wtype = AAX_PINK_NOISE;
   }
   else if (!xmlAttributeCompareString(xwid, "src", "square")) {
      wtype = AAX_SQUARE_WAVE;
   }
   else if (!xmlAttributeCompareString(xwid, "src", "triangle")) {
       wtype = AAX_TRIANGLE_WAVE;
   }
   else if (!xmlAttributeCompareString(xwid, "src", "sawtooth")) {
       wtype = AAX_SAWTOOTH_WAVE;
   }
   else if (!xmlAttributeCompareString(xwid, "src", "impulse")) {
       wtype = AAX_IMPULSE_WAVE;
   }
   else {   // !xmlAttributeCompareString(xwid, "src", "sine")
      wtype = AAX_SINE_WAVE;
   }

   if (xmlAttributeExists(xwid, "processing"))
   {
      if (!xmlAttributeCompareString(xwid,"processing","modulate"))
      {
         ptype = AAX_RINGMODULATE;
         if (!ratio) ratio = 1.0f;
         if (!pitch) pitch = 1.0f;
      }
      else if (!xmlAttributeCompareString(xwid,"processing","add"))
      {
         ptype = AAX_ADD;
         if (!ratio) ratio = 1.0f;
         if (!pitch) pitch = 1.0f;
      }
      else if (!xmlAttributeCompareString(xwid,"processing","mix"))
      {
         ptype = AAX_MIX;
         if (!ratio) ratio = 0.5f;
         if (!pitch) pitch = 1.0f;
      }
      else if (!xmlAttributeCompareString(xwid, "processing", "overwrite"))
      {
         ptype = AAX_OVERWRITE;
         if (!ratio) ratio = 1.0f;
         if (!pitch) pitch = 1.0f;
      }
   }
   else
   {
      if (!xmlNodeCompareString(xwid, "processing", "modulate"))
      {
         ptype = AAX_RINGMODULATE;
         if (!ratio) ratio = 1.0f;
         if (!pitch) pitch = 1.0f;
      }
      else if (!xmlNodeCompareString(xwid, "processing", "add"))
      {
         ptype = AAX_ADD;
         if (!ratio) ratio = 1.0f;
         if (!pitch) pitch = 1.0f;
      }
      else if (!xmlNodeCompareString(xwid, "processing", "mix"))
      {
         ptype = AAX_MIX;
         if (!ratio) ratio = 0.5f;
         if (!pitch) pitch = 1.0f;
      }
      else //!xmlNodeCompareString(xwid, "processing", "overwrite")
      {
         ptype = AAX_OVERWRITE;
         if (!ratio) ratio = 1.0f;
         if (!pitch) pitch = 1.0f;
      }
   }

   spread = spread*_log2lin(_lin2log(freq)/3.3f);
   if (ptype == AAX_RINGMODULATE) voices = 1;
   return _bufProcessWaveform(handle, freq, phase, pitch, staticity,
                   pitch_level, voices, spread, wtype, ratio, ptype, limiter);
}

static int
_bufCreateFilterFromAAXS(_buffer_t* handle, const void *xfid, float frequency)
{
   aaxFilter flt = _aaxGetFilterFromAAXS(handle->root, xfid, frequency, handle->info.low_frequency, handle->info.high_frequency, NULL);
   if (flt)
   {
      _filter_t* filter = get_filter(flt);
      switch (filter->type)
      {
      case AAX_BITCRUSHER_FILTER:
         _bufApplyBitCrusherFilter(handle, filter);
         break;
      case AAX_FREQUENCY_FILTER:
      {
         int state = filter->state & ~(AAX_BUTTERWORTH|AAX_BESSEL);
         if (state == AAX_TRUE     ||
             state == AAX_6DB_OCT  ||
             state == AAX_12DB_OCT ||
             state == AAX_24DB_OCT ||
             state == AAX_36DB_OCT ||
             state == AAX_48DB_OCT)
         {
            _bufApplyFrequencyFilter(handle, filter);
         }
         break;
      }
      case AAX_EQUALIZER:
         _bufApplyFrequencyFilter(handle, filter);
         break;
      default:
         break;
      }
      aaxFilterDestroy(flt);
   }
   return AAX_TRUE;
}

static int
_bufCreateEffectFromAAXS(_buffer_t* handle, const void *xeid, float frequency, float min, float max)
{

   aaxEffect eff = _aaxGetEffectFromAAXS(handle->root, xeid, frequency, min, max, NULL);
   if (eff)
   {
      _effect_t* effect = get_effect(eff);
      if (effect->type == AAX_DISTORTION_EFFECT && effect->state == AAX_TRUE) {
         _bufApplyDistortionEffect(handle, effect);
      }
      aaxEffectDestroy(eff);
   }
   return AAX_TRUE;
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
      size_t i;

      for (i=0; i<no_samples; ++i) {
         dptr[i] = (1<<23)*atanf(dptr[i]*GMATH_1_PI_2/(1<<23));
      }
   }
}

static inline float note2freq(uint8_t d) {
   return 440.0f*powf(2.0f, ((float)d-69.0f)/12.0f);
}

static int
_bufAAXSThreadReadFromCache(_buffer_aax_t *aax_buf, const char *fname, size_t fsize)
{
   _buffer_t* handle = aax_buf->parent;
   int rv = AAX_FALSE;
   FILE *infile;

   infile = fopen(fname, "r");
   if (infile)
   {
      void **data = malloc(fsize);
      if (data)
      {
         char *end = (char*)data + fsize;
         int b = 0, pitch_levels = 0;
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
               pitch_levels++;
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
                if (pitch_levels > MAX_PITCH_LEVELS) {
                   pitch_levels = MAX_PITCH_LEVELS;
                }
                handle->pitch_levels = pitch_levels;

                for (b=0; b<pitch_levels; ++b)
                {
                   unsigned int no_samples;
                   _aaxRingBuffer *rb;
                   void **tracks;
                   char *ptr;

                   rb = _bufGetRingBuffer(handle, handle->root, b);

                   ptr = (char*)data + d[b];
                   size = d[b+1] - d[b];
                   no_samples = size/sizeof(float);

                   if (rb->get_state(rb, RB_IS_VALID) == AAX_FALSE)
                   {
                      rb->set_parami(rb, RB_NO_SAMPLES, no_samples);
                      rb->init(rb, AAX_FALSE);
                      handle->ringbuffer[b] = rb;
                   }
                   assert(size <= rb->get_parami(rb, RB_TRACKSIZE));

                   tracks = (void**)rb->get_tracks_ptr(rb, RB_WRITE);
                   _aax_memcpy(tracks[0], ptr, size);
                   rb->release_tracks_ptr(rb);
                }
                rv = AAX_TRUE;
            }
         }
         free(data);
      }
   }

   return rv;
}

static int
_bufAAXSThreadCreateWaveform(_buffer_aax_t *aax_buf, void *xid)
{
   _buffer_t* handle = aax_buf->parent;
   float freq = aax_buf->frequency;
   float low_frequency = 0.0f;
   float high_frequency = 0.0f;
   int s, nsound, rv = AAX_FALSE;
   limitType limiter;
   void *xaid, *xsid;
   char *env;

   env = getenv("AAX_INSTRUMENT_MODE");
   if (env) {
      limiter = atoi(env);
   }

   xaid = xmlNodeGet(xid, "aeonwave/info/note");
   if (xaid)
   {
      if (xmlAttributeExists(xaid, "min")) {
         low_frequency = note2freq(xmlAttributeGetInt(xaid, "min"));
      }
      if (xmlAttributeExists(xaid, "max")) {
         high_frequency = note2freq(xmlAttributeGetInt(xaid, "max"));
      }
      xmlFree(xaid);
   }

   xaid = xmlNodeGet(xid, "aeonwave");
   if (!xaid) xaid = xid;

   nsound = 1; // xmlNodeGetNum(xaid, "sound");
   xsid = xmlMarkId(xaid);
   for (s=0; s<nsound; ++s)
   {
      unsigned int i, num, bits = 24;
      double duration = 1.0f;
      float spread = 0;
      int b, voices = 1;

      if (!xmlNodeGetPos(xaid, xsid, "sound", s)) continue;

      if (xmlAttributeExists(xsid, "gain")) {
         handle->gain = xmlAttributeGetDouble(xsid, "gain");
      } else if (xmlAttributeExists(xsid, "fixed-gain")) {
         handle->gain = xmlAttributeGetDouble(xsid, "fixed-gain");
      }

      if (!freq)
      {
         freq = xmlAttributeGetDouble(xsid, "frequency");
         handle->info.base_frequency = freq;
         if (high_frequency > 0.0f)
         {
            handle->pitch_levels =_MAX(1,1+log2i(ceilf(high_frequency/freq)));
            if (handle->pitch_levels > MAX_PITCH_LEVELS) {
               handle->pitch_levels = MAX_PITCH_LEVELS;
            }
         }
      }

      if (xmlAttributeExists(xsid, "file"))
      {
         char *file = xmlAttributeGetString(xsid, "file");

         rv = _bufSetDataFromAAXS(handle, file, s);
         if (!rv) {
            aax_buf->error = AAX_INVALID_REFERENCE;
         }
         xmlFree(file);

         if (!handle->info.loop_end) // loop was not defined in the file
         {
            _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
            unsigned int loop_start, loop_end;

            loop_start = xmlAttributeGetInt(xsid, "loop-start");
            if (xmlAttributeExists(xsid, "loop-end")) {
               loop_end = xmlAttributeGetInt(xsid, "loop-end");
            } else {
               loop_end = handle->info.no_samples;
            }
            if (loop_end > loop_start) {
               handle->info.loop_count = INT_MAX;
            }

            rb->set_paramf(rb, RB_LOOPPOINT_END, loop_end/handle->info.freq);
            rb->set_paramf(rb, RB_LOOPPOINT_START,loop_start/handle->info.freq);
            rb->set_parami(rb, RB_LOOPING, handle->info.loop_count);

            handle->info.loop_start = loop_start;
            handle->info.loop_end = loop_end;
         }

         handle->pitch_levels = s;

         duration = 0.0f;
      }
      else if (xmlAttributeExists(xsid, "duration"))
      {
         duration = xmlAttributeGetDouble(xsid, "duration");
         if (duration < 0.1f) {
            duration = 0.1f;
         }
      }

      if (!handle->info.base_frequency) {
         handle->info.base_frequency = freq;
      }
      if (!handle->info.low_frequency) {
         handle->info.low_frequency = low_frequency;
      }
      if (!handle->info.high_frequency) {
         handle->info.high_frequency = high_frequency;
      }

      if (xmlAttributeExists(xsid, "bits"))
      {
         bits = xmlAttributeGetInt(xsid, "bits");
         if (bits != 16) bits = 24;
      }

      if (!env) {
         limiter = xmlAttributeGetInt(xsid, "mode");
      }

      if (xmlAttributeExists(xsid, "voices")) {
         voices = _MINMAX(xmlAttributeGetInt(xsid, "voices"), 1, 11);
      }
      if (xmlAttributeExists(xsid, "spread")) {
         spread = _MAX(xmlAttributeGetDouble(xsid, "spread"), 0.01f);
         if (xmlAttributeGetBool(xsid, "phasing")) spread = -spread;
      }

      for (b=0; b<handle->pitch_levels; ++b)
      {
         float mul = (float)(1 << b);
         float frequency = mul*freq;
         float pitch_fact = 1.0f/mul;
         void *xwid;

         if (duration >= 0.099f)
         {
            _aaxRingBuffer* rb = _bufGetRingBuffer(handle, handle->root, b);
            float f = pitch_fact*rb->get_paramf(rb, RB_FREQUENCY);
            size_t no_samples = SIZE_ALIGNED((size_t)rintf(duration*f));
            rb->set_parami(rb, RB_NO_SAMPLES, no_samples);
            handle->ringbuffer[b] = rb;
         }
         xwid = xmlMarkId(xsid);
         if (xwid)
         {
            num = xmlNodeGetNum(xsid, "*");
            for (i=0; i<num; i++)
            {
               if (xmlNodeGetPos(xsid, xwid, "*", i) != 0)
               {
                  char *name = xmlNodeGetName(xwid);
                  if (!strcasecmp(name, "waveform")) {
                     rv = _bufCreateWaveformFromAAXS(handle, xwid, frequency,
                                             b, voices, spread, limiter & 1);
                  } else if (!strcasecmp(name, "filter")) {
                     rv = _bufCreateFilterFromAAXS(handle, xwid, frequency);
                  } else if (!strcasecmp(name, "effect")) {
                     rv = _bufCreateEffectFromAAXS(handle, xwid, frequency,
                                                 low_frequency, high_frequency);
                  }
                  xmlFree(name);
                  if (rv == AAX_FALSE) break;
               }
            }
            xmlFree(xwid);
         }

         if (limiter)
         {
            _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, b);
            _bufLimit(rb);
         }

         if (0) // handle->to_mixer)
         {
            _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, b);
            _bufConvertDataToMixerFormat(handle, rb);
         }
         else if (bits == 16)
         {
            _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, b);
            _aaxRingBufferData *rbi = rb->handle;
            _aaxRingBufferSample *rbd = rbi->sample;
            void *dptr = rbd->track[0];
            unsigned int no_samples;

            // 32-bit aligned 24-bit is smaller than 16-bit
            // TODO: return the freed space,
            //       but how does aligned realloc work?
            no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
            _batch_cvt16_24(dptr, dptr, no_samples);
            rb->set_parami(rb, RB_FORMAT, AAX_PCM16S);
            handle->info.fmt = AAX_PCM16S;
         }
      }
   }
   xmlFree(xsid);
   if (xaid != xid) xmlFree(xaid);

   return rv;
}

static void*
_bufAAXSThread(void *d)
{
   _buffer_aax_t *aax_buf = (_buffer_aax_t*)d;
   _buffer_t* handle = aax_buf->parent;
   const void *aaxs =  aax_buf->aaxs;
   int rv = AAX_FALSE;
   void *xid, *xiid;

   assert(handle);
   assert(aaxs);

   handle->aaxs = strdup(aaxs);
   if (!handle->aaxs) return NULL;

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
#ifdef ARCH32
                  MurmurHash3_x86_32(s, e-s, 0x27918072, &hash);
                  snprintf(hstr, 64, "%08x", hash[0]);
#else
                  MurmurHash3_x64_128(s, e-s, 0x27918072, &hash);
                  snprintf(hstr, 64, "%08x%08x%08x%08x", hash[0], hash[1], hash[2], hash[3]);
#endif
                  have_hash = 1;
              }
          }
          xmlFree(a);
      }

      xiid = xmlNodeGet(xid, "aeonwave/info/aftertouch");
      if (xiid)
      {
         handle->pressure_mode = xmlAttributeGetInt(xiid, "mode");
         xmlFree(xiid);
      }

      if (have_hash)
      {
         char *fname;

         fname = userCacheFile(hstr);
         if (fname)
         {
            size_t fsize = getFileSize(fname);
            if (fsize) {
               rv = _bufAAXSThreadReadFromCache(aax_buf, fname, fsize);
            }

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
   }
   else {
      aax_buf->error = AAX_INVALID_PARAMETER;
   }

   return rv ? d : NULL;
}

static int
_bufCreateFromAAXS(_buffer_t* buffer, const void *aaxs, float freq)
{
   _handle_t *handle = buffer->root;
   _buffer_aax_t data;
   int rv = AAX_FALSE;

   data.parent = buffer;
   data.aaxs = aaxs;
   data.frequency = freq;
   data.error = AAX_ERROR_NONE;

   // Using a thread here might spawn wavweform generation on another CPU
   // core freeing the current (possibly busy) CPU core from doing the work.
   if (!handle->aaxs_thread.ptr) {
      handle->aaxs_thread.ptr = _aaxThreadCreate();
   }

   if (handle->aaxs_thread.ptr)
   {
      rv = _aaxThreadStart(handle->aaxs_thread.ptr, _bufAAXSThread, &data, 0);
      if (!rv) {
         _aaxThreadJoin(handle->aaxs_thread.ptr);
      }
      else {
         _bufAAXSThread(&data);
      }
   }
   else {
      _bufAAXSThread(&data);
   }

   if (data.error) {
      _aaxErrorSet(data.error);
   } else {
      rv = AAX_TRUE;
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

static int
_bufProcessWaveform(aaxBuffer buffer, float freq, float phase, float pitch, float staticity, unsigned char pitch_level, int voices, float spread, enum aaxWaveformType wtype, float ratio, enum aaxProcessingType ptype, limitType limiter)
{
   _buffer_t* handle = get_buffer(buffer, __func__);
   int rv = AAX_FALSE;

   if (wtype > AAX_LAST_WAVEFORM) {
      _aaxErrorSet(AAX_INVALID_PARAMETER + 3);
   } else if ((ptype == AAX_MIX) && (ratio > 1.0f || ratio < -1.0f)) {
      _aaxErrorSet(AAX_INVALID_PARAMETER + 4);
   } else if (ptype >= AAX_PROCESSING_MAX) {
      _aaxErrorSet(AAX_INVALID_PARAMETER + 5);
   }
   else if (handle && handle->mixer_info && (*handle->mixer_info && ((*handle->mixer_info)->id == INFO_ID)))
   {
      _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, pitch_level);
      float samps_period, fs, fw, fs_mixer, rate, *scratch;
      unsigned int no_samples, i, bit = 1;
      int q, hvoices;
      unsigned skip;
      char modulate;
      char phasing;

      modulate = 0;
      rate = freq * pitch;
      fw = FNMINMAX(rate, 1.0f, 22050.0f);
      skip = (unsigned char)(1.0f + 99.0f*_MINMAX(staticity, 0.0f, 1.0f));

      phase *= GMATH_PI;
//    if (ratio < 0.0f) phase = GMATH_PI - phase;

      fs = rb->get_paramf(rb, RB_FREQUENCY);
      no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
      samps_period = fs/fw;
      hvoices = voices >> 1;
      voices |= 0x1;

      if (rb->get_state(rb, RB_IS_VALID) == AAX_FALSE)
      {
         no_samples = floorf((no_samples/samps_period)+1)*samps_period;
         rb->set_parami(rb, RB_NO_SAMPLES, no_samples);
         rb->init(rb, AAX_FALSE);
      }

      switch (ptype)
      {
      case AAX_OVERWRITE:
         rb->set_state(rb, RB_CLEARED);
         break;
      case AAX_MIX:
      {
         float ratio_orig = FNMINMAX(1.0f-ratio, 0.0f, 1.0f);

         ratio = 2.0f*(1.0f - ratio_orig);
         if (wtype & AAX_SINE_WAVE) ratio /= 2;
         if (wtype & AAX_SQUARE_WAVE) ratio /= 2;
         if (wtype & AAX_IMPULSE_WAVE) ratio /= 2;
         if (wtype & AAX_TRIANGLE_WAVE) ratio /= 2;
         if (wtype & AAX_SAWTOOTH_WAVE) ratio /= 2;
         if (wtype & AAX_IMPULSE_WAVE) ratio /= 2;
         if (wtype & AAX_WHITE_NOISE) ratio /= 2;
         if (wtype & AAX_PINK_NOISE) ratio /= 2;
         if (wtype & AAX_BROWNIAN_NOISE) ratio /= 2;

         rb->data_multiply(rb, 0, 0, ratio_orig);
         break;
      }
      case AAX_RINGMODULATE:
         modulate = 1;
         break;
      case AAX_ADD:
      default:
         break;
      }

      rv = AAX_TRUE;
      if (handle->mixer_info && *handle->mixer_info) {
         fs_mixer = (*handle->mixer_info)->frequency;
      } else {
         fs_mixer = 0.0f;
      }

      phasing = (spread <0.0f);
      spread = fabsf(spread);
      scratch = _aax_aligned_alloc(2*(no_samples+NOISE_PADDING)*sizeof(float));
      if (scratch)
      {
         // AAX_CONSTANT_VALUE == 0
         for (i=1; i<AAX_MAX_WAVE_NOISE; i++)
         {
            switch (wtype & bit)
            {
            case AAX_SINE_WAVE:
            case AAX_SQUARE_WAVE:
            case AAX_TRIANGLE_WAVE:
            case AAX_SAWTOOTH_WAVE:
            case AAX_IMPULSE_WAVE:
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
                  rv = rb->data_mix_waveform(rb, scratch, wtype&bit, nfw, nratio, nphase, modulate, limiter);
               }
               break;
            case AAX_WHITE_NOISE:
            case AAX_PINK_NOISE:
            case AAX_BROWNIAN_NOISE:
               rv = rb->data_mix_noise(rb, scratch, wtype & bit, fs_mixer, pitch, ratio, skip, modulate, limiter);
               break;
            default:
               break;
            }
            bit <<= 1;
         }
         _aax_aligned_free(scratch);
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

         nrb->set_format(nrb, AAX_PCM24S, AAX_FALSE);
         nrb->set_parami(nrb, RB_NO_TRACKS, buf->info.tracks);
         nrb->set_parami(nrb, RB_NO_SAMPLES, buf->info.no_samples);
         nrb->set_paramf(nrb, RB_FREQUENCY, buf->info.freq);
         nrb->init(nrb, AAX_FALSE);

         nrb->set_parami(nrb, RB_LOOPPOINT_END, buf->info.loop_end);
         nrb->set_parami(nrb, RB_LOOPPOINT_START, buf->info.loop_start);
         nrb->set_parami(nrb, RB_LOOPING, buf->info.loop_count);

         fmt = rb->get_parami(rb, RB_FORMAT);
         src = (int32_t**)rb->get_tracks_ptr(rb, RB_READ);
         dst = (int32_t**)nrb->get_tracks_ptr(nrb, RB_WRITE);
         for (t=0; t<tracks; ++t) {
            _bufConvertDataToPCM24S(dst[t], src[t], no_samples, fmt);
         }
         nrb->release_tracks_ptr(nrb);
         be->destroy_ringbuffer(rb);
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
 */
static void
_aaxRingBufferIMA4ToPCM16(int32_t **__restrict dst, const void *__restrict src, int tracks, int blocksize, unsigned int no_samples)
{
   unsigned int i, blocks, block_smp;
   int16_t *d[_AAX_MAX_SPEAKERS];
   uint8_t *s = (uint8_t *)src;
   int t;

   if (tracks > _AAX_MAX_SPEAKERS)
      return;

   /* copy buffer pointers */
   for(t=0; t<tracks; t++) {
      d[t] = (int16_t*)dst[t];
   }

   block_smp = IMA4_BLOCKSIZE_TO_SMP(blocksize);
   blocks = no_samples / block_smp;
   i = blocks-1;
   do
   {
      for (t=0; t<tracks; t++)
      {
         _sw_bufcpy_ima_adpcm(d[t], s, block_smp);
         d[t] += block_smp;
         s += blocksize;
      }
   }
   while (--i);

   no_samples -= blocks*block_smp;
   if (no_samples)
   {
      int t;
      for (t=0; t<tracks; t++)
      {
         _sw_bufcpy_ima_adpcm(d[t], s, no_samples);
         s += blocksize;
      }
   }
}

_aaxRingBuffer*
_bufSetDataInterleaved(_buffer_t *buf, _aaxRingBuffer *rb, const void *dbuf, unsigned blocksize)
{
   unsigned int rb_format, bps, no_samples, no_tracks, tracksize;
   _aaxRingBuffer *rv = rb;
   const void* data;
   int32_t **tracks;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(rb != 0);
   assert(dbuf != 0);

   data = dbuf;

   rb->set_state(rb, RB_CLEARED);

   rb_format = rb->get_parami(rb, RB_FORMAT);
   bps = rb->get_parami(rb, RB_BYTES_SAMPLE);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
   tracksize = no_samples * bps;

   switch (rb_format)
   {
   case AAX_IMA4_ADPCM:
      tracks = (int32_t**)rb->get_tracks_ptr(rb, RB_WRITE);
      _aaxRingBufferIMA4ToPCM16(tracks, data, no_tracks, blocksize, no_samples);
      rb->release_tracks_ptr(rb);
      break;
   case AAX_PCM24S:
   case AAX_PCM32S:
      tracks = (int32_t**)rb->get_tracks_ptr(rb, RB_WRITE);
      _batch_cvt24_32_intl(tracks, data, 0, no_tracks, no_samples);
      rb->release_tracks_ptr(rb);
      break;
   case AAX_FLOAT:
      tracks = (int32_t**)rb->get_tracks_ptr(rb, RB_WRITE);
      _batch_cvt24_ps_intl(tracks, data, 0, no_tracks, no_samples);
      rb->release_tracks_ptr(rb);
      break;
   case AAX_DOUBLE:
      tracks = (int32_t**)rb->get_tracks_ptr(rb, RB_WRITE);
      _batch_cvt24_pd_intl(tracks, data, 0, no_tracks, no_samples);
      rb->release_tracks_ptr(rb);
      break;
   case AAX_PCM24S_PACKED:
      tracks = (int32_t**)rb->get_tracks_ptr(rb, RB_WRITE);
      _batch_cvt24_24_3intl(tracks, data, 0, no_tracks, no_samples);
      rb->release_tracks_ptr(rb);
      break;
   default:
      tracks = (int32_t**)rb->get_tracks_ptr(rb, RB_WRITE);
      if (no_tracks == 1) {
         _aax_memcpy(tracks[0], data, tracksize);
      }
      else /* stereo */
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

   if (handle->info.tracks != 1) return data;
   if (handle->pitch_levels <= 1) return data;
   if (format != AAX_PCM24S && format != AAX_FLOAT) return data;
   if (rb->get_parami(rb, RB_FORMAT) != AAX_PCM24S) return data;
   /**
    * format:
    * 1. an array of offsets to the nth buffer followd by a value of zero.
    * 2. the full size of the buffer as a size_t type.
    * 3. followed by the data for all pitch levels.
    *    - the data format is AAX_PCM24S
    */
   offs = (handle->pitch_levels+2)*sizeof(void*);
   size = 2*no_samples*sizeof(int32_t);
   data = (void**)_aax_malloc(&ptr, offs, size);
   if (data == NULL)
   {
      _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      return data;
   }


   d = (size_t*)data;
   for (b=0; b<handle->pitch_levels; ++b)
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
      _aax_memcpy(ptr, tracks[0], len);
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
#if RB_FLOAT_DATA
            if (rb->get_parami(rb, RB_IS_MIXER_BUFFER) == AAX_FALSE) {
               _batch_cvtps24_24(ptr[t], ptr[t], no_samples);
            }
            _batch_resample_float(tracks[t], ptr[t], 0, samples, 0, fact);
            _batch_cvt24_ps24(tracks[t], tracks[t], samples);
#else
            _batch_resample(tracks[t], ptr[t], 0, samples, 0, fact);
#endif
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

#if RB_FLOAT_DATA
            _batch_cvtps24_24(scratch[0], scratch[0], no_samples);
            _batch_resample_float(scratch[1], scratch[0], 0, samples, 0, fact);
            _batch_cvt24_ps24(scratch[1], scratch[1], samples);
#else
            _batch_resample(scratch[1], scratch[0], 0, samples, 0, fact);
#endif
            _bufConvertDataFromPCM24S(tracks[t], scratch[1], 1, samples, fmt,1);
            p += size;
         }
         free(scratch);
      }
   }

   if (no_tracks == 1) {
      _aax_memcpy(data, tracks[0], samples*bps);
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
_bufApplyBitCrusherFilter(_buffer_t* handle, _filter_t *filter)
{
   _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
   int32_t **sbuf = (int32_t**)rb->get_tracks_ptr(rb, RB_RW);
   _aaxFilterInfo* slot = filter->slot[0];
   float ratio = slot->param[AAX_NOISE_LEVEL];
   float level = slot->param[AAX_LFO_OFFSET];
   unsigned int bps, no_samples;
   int32_t *dptr = sbuf[0];

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

         ratio *= (0.25f * 8388608.0f)/UINT64_MAX;
         for (i=0; i<no_samples; ++i) {
            dptr[i] += ratio*xoroshiro128plus();
         }
      }
   }
   rb->release_tracks_ptr(rb);
}



static void
_bufApplyFrequencyFilter(_buffer_t* handle, _filter_t *filter)
{
   _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
   _aaxRingBufferData *rbi = rb->handle;
   _aaxRingBufferSample *rbd = rbi->sample;
   unsigned int bps, no_samples;
   float *dptr = rbd->track[0];
   float *sptr, *tmp;

   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
   bps = rb->get_parami(rb, RB_BYTES_SAMPLE);

   /* AAXS defined buffers have just one track */
   assert(rb->get_parami(rb, RB_NO_TRACKS) == 1);
   assert(bps == 4);

   sptr = _aax_aligned_alloc(4*no_samples*bps);
   if (sptr)
   {
      _aaxRingBufferFreqFilterData *data = filter->slot[0]->data;
      _aaxRingBufferFreqFilterData *data_hf = filter->slot[1]->data;

      _batch_cvtps24_24(dptr, dptr, no_samples);

      tmp = sptr+no_samples;
      _aax_memcpy(sptr, dptr, no_samples*bps);
      _aax_memcpy(tmp, dptr, no_samples*bps);

      if (!data_hf)
      {
         rbd->freqfilter(sptr, sptr, 0, 2*no_samples, data);
         if (data->state && (data->low_gain > LEVEL_128DB)) {
            rbd->add(tmp, dptr, no_samples, data->low_gain, 0.0f);
         }
      }
      else
      {
         if (data->type == LOWPASS && data_hf->type == HIGHPASS)
         {
            float *tmp2 = sptr + 2*no_samples;
            rbd->freqfilter(tmp2, sptr, 0, 2*no_samples, data);
            rbd->freqfilter(sptr, sptr, 0, 2*no_samples, data_hf);
            rbd->add(tmp, tmp2+no_samples, no_samples, 1.0f, 0.0f);
         }
         else
         {
            rbd->freqfilter(sptr, sptr, 0, 2*no_samples, data);
            rbd->freqfilter(sptr, sptr, 0, 2*no_samples, data_hf);
         }
      }
      _aax_memcpy(dptr, tmp, no_samples*bps);

      _aax_aligned_free(sptr);
      _batch_cvt24_ps24(dptr, dptr, no_samples);
   }
}

static void
_bufApplyDistortionEffect(_buffer_t* handle, _effect_t *effect)
{
   _aaxRingBuffer* rb = _bufGetRingBuffer(handle, NULL, 0);
   int32_t **sbuf = (int32_t**)rb->get_tracks_ptr(rb, RB_RW);
   _aaxRingBufferData *rbi = rb->handle;
   _aaxRingBufferSample *rbd = rbi->sample;
   _aaxEffectInfo* slot = effect->slot[0];
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
      _batch_cvtps24_24(dptr, dptr, no_samples);
      _aax_memcpy(sptr, dptr, no_samples*bps);
      if (mix > 0.01f)
      {
         float mix_factor;

         /* make dptr the wet signal */
         if (fact > 0.0013f) {
            rbd->multiply(dptr, dptr, bps, no_samples, 1.0f+64.f*fact);
         }

         if ((fact > 0.01f) || (asym > 0.01f))
         {
            size_t avg = 0;
            size_t peak = no_samples;
            _aaxRingBufferLimiter(dptr, &avg, &peak, clip, 4*asym);
         }

         /* mix with the dry signal */
         mix_factor = mix/(0.5f+powf(fact, 0.25f));
         rbd->multiply(dptr, dptr, bps, no_samples, mix_factor);
         if (mix < 0.99f) {
            rbd->add(dptr, sptr, no_samples, 1.0f-mix, 0.0f);
         }
      }
      _aax_aligned_free(sptr);
      _batch_cvt24_ps24(dptr, dptr, no_samples);
   }
   rb->release_tracks_ptr(rb);
}
