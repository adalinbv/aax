 /*
 * Copyright 2007-2011 by Erik Hofman.
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

#ifdef HAVE_LIBIO_H
#include <libio.h>              /* for NULL */
#endif

#include <math.h>		/* for floorf */
#include <string.h>		/* for memset */

#include <software/audio.h>
#include "api.h"
#include "arch.h"

static unsigned char  _oalFormatsBPS[AAX_FORMAT_MAX];

aaxBuffer
aaxBufferCreate(aaxConfig config, unsigned int samples, unsigned channels,
                                   enum aaxFormat format)
{
   unsigned int native_fmt = format & AAX_FORMAT_NATIVE;
   aaxBuffer rv = NULL;

   if ((native_fmt < AAX_FORMAT_MAX) && (samples*channels > 0))
   {
      _buffer_t* buf = calloc(1, sizeof(_buffer_t));
      if (buf)
      {
         _oalRingBuffer *rb = _oalRingBufferCreate(AAX_FALSE);
         if (rb)
         {
            _handle_t* handle = (_handle_t*)config;
            buf->codecs = _oalRingBufferCodecs;
            if (handle)
            {
               if VALID_HANDLE(handle) buf->info = handle->info;
               buf->codecs = handle->backend.ptr->codecs;
            }
            buf->mipmap = AAX_FALSE;
            buf->ref_counter = 1;
            buf->format = format;
            buf->id = BUFFER_ID;
            buf->frequency = 0.0f;

            /* initialize the ringbuffer in native format only */
            _oalRingBufferSetFormat(rb, buf->codecs, native_fmt);
            _oalRingBufferSetNoSamples(rb, samples);
            _oalRingBufferSetNoTracks(rb, channels);
            /* Postpone until aaxBufferSetData gets called
             * _oalRingBufferInit(rb, AAX_FALSE);
            */
            buf->ringbuffer = rb;

            switch(native_fmt)
            {
            case AAX_IMA4_ADPCM:
               buf->blocksize = DEFAULT_IMA4_BLOCKSIZE;
               break;
            default:
               buf->blocksize = 1;
            }

            rv = (aaxBuffer)buf;
         }
         else
         {
            free(buf);
            buf = NULL;
         }
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

int
aaxBufferSetSetup(aaxBuffer buffer, enum aaxSetupType type, unsigned int setup)
{
   _buffer_t* buf = get_buffer(buffer);
   int rv = AAX_FALSE;
   if (buf)
   {
      _oalRingBuffer* rb = buf->ringbuffer;
      switch(type)
      {
      case AAX_FREQUENCY:
         if ((setup > 1000) && (setup < 96000))
         {
            if (!buf->frequency) {
               _oalRingBufferSetFrequency(rb, (float)setup);
            }
            buf->frequency = (float)setup;
            rv = AAX_TRUE;
         }
         else _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      case AAX_TRACKS:
         if (setup < _AAX_MAX_SPEAKERS)
         {
            _oalRingBufferSetNoTracks(rb, setup);
            rv = AAX_TRUE;
         }
         else _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      case AAX_FORMAT:
      {
         enum aaxFormat native_fmt = setup & AAX_FORMAT_NATIVE;
         if (native_fmt < AAX_FORMAT_MAX)
         {
            buf->format = setup;
            switch(native_fmt)
            {
            case AAX_IMA4_ADPCM:
               buf->blocksize = DEFAULT_IMA4_BLOCKSIZE;
               break;
            default:
               buf->blocksize = 1;
               break;
            }
            rv = AAX_TRUE;
         } else _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      }
      case AAX_TRACKSIZE:
         rv = _oalRingBufferSetTrackSize(rb, setup);
         break;
      case AAX_LOOP_START:
         if (setup < _oalRingBufferGetNoSamples(rb))
         {
            unsigned int start, end;
            _oalRingBufferGetLoopPoints(rb, &start, &end);
            _oalRingBufferSetLoopPoints(rb, setup, end);
            _oalRingBufferSetLooping(rb, (setup < end) ? AAX_TRUE : AAX_FALSE);
            rv = AAX_TRUE;
         }
         else _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      case AAX_LOOP_END:
         if (setup < _oalRingBufferGetNoSamples(rb))
         {
            unsigned int start, end;
            _oalRingBufferGetLoopPoints(rb, &start, &end);
            _oalRingBufferSetLoopPoints(rb, start, setup);
            _oalRingBufferSetLooping(rb,(start < setup) ? AAX_TRUE : AAX_FALSE);
            rv = AAX_TRUE;
         }
         else _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      case AAX_BLOCK_ALIGNMENT:
         if (setup > 1)
         {
            if (buf->format == AAX_IMA4_ADPCM)
            {
               buf->blocksize = setup;
               rv = AAX_TRUE;
            }
         }
         else if (buf->format != AAX_IMA4_ADPCM)
         {
            buf->blocksize = setup;
            rv = AAX_TRUE;
         }
         break;
      case AAX_POSITION:
         if (setup <= _oalRingBufferGetNoSamples(rb)) {
            buf->pos = setup;
            rv = AAX_TRUE;
         }
         else  _aaxErrorSet(AAX_INVALID_PARAMETER);
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

unsigned int
aaxBufferGetSetup(const aaxBuffer buffer, enum aaxSetupType type)
{
   _buffer_t* buf = get_buffer(buffer);
   unsigned int rv = AAX_FALSE;
   if (buf)
   {
      _oalRingBuffer* rb = buf->ringbuffer;
      unsigned int tmp;
      switch(type)
      {
      case AAX_FREQUENCY:
         rv = buf->frequency;
         break;
      case AAX_TRACKS:
         rv = _oalRingBufferGetNoTracks(rb);
         break;
      case AAX_FORMAT:
         rv = _oalRingBufferGetFormat(rb);
         break;
      case AAX_TRACKSIZE:
         if (buf->frequency)
         {
            float fact = buf->frequency / _oalRingBufferGetFrequency(rb);
            rv = _oalRingBufferGetNoSamples(rb) - buf->pos;
            rv *= fact*_oalRingBufferGetBytesPerSample(rb);
         }
         else _aaxErrorSet(AAX_INVALID_STATE);
         break;
      case AAX_NO_SAMPLES:
          if (buf->frequency)
         {
            float fact = buf->frequency / _oalRingBufferGetFrequency(rb);
            rv = fact*(_oalRingBufferGetNoSamples(rb) - buf->pos);
         }
         else _aaxErrorSet(AAX_INVALID_STATE);
         break;
      case AAX_LOOP_START:
         _oalRingBufferGetLoopPoints(rb, &rv, &tmp);
         break;
      case AAX_LOOP_END:
         _oalRingBufferGetLoopPoints(rb, &tmp, &rv);
         break;
      case AAX_BLOCK_ALIGNMENT:
         rv = buf->blocksize;
         break;
      case AAX_POSITION:
         rv = buf->pos;
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

int
aaxBufferSetData(aaxBuffer buffer, const void* d)
{
   _buffer_t* buf = get_buffer(buffer);
   int rv = AAX_FALSE;
   if (buf)
   {
      unsigned int tracks, no_samples, buf_samples;

      _oalRingBufferInit(buf->ringbuffer, AAX_FALSE);
      tracks = _oalRingBufferGetNoTracks(buf->ringbuffer);
      no_samples = _oalRingBufferGetNoSamples(buf->ringbuffer);

      buf_samples = tracks*no_samples;
      if (d && (buf_samples > 0))
      {
         unsigned blocksize =  buf->blocksize;
         unsigned int format = buf->format;
         void *data = (void*)d, *m = NULL;
         unsigned int native_fmt;
         char fmt_bps;

				/* do we need to convert to native format? */
         native_fmt = format & AAX_FORMAT_NATIVE;
         if (format & ~AAX_FORMAT_NATIVE)
         {
            fmt_bps = _oalFormatsBPS[native_fmt];
            m = malloc(buf_samples*fmt_bps);
            if (!m)
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
         _oalRingBufferFillInterleaved(buf->ringbuffer, data, blocksize, 0);
         rv = AAX_TRUE;
         free(m);
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

int
aaxBufferProcessWaveform(aaxBuffer buffer, float rate, enum aaxWaveformType type, float ratio, enum aaxProcessingType ptype)
{
   _buffer_t* buf = get_buffer(buffer);
   int rv = AAX_FALSE;
   if (buf && EBF_VALID(buf))
   {
      float phase = (ratio < 0.0f) ? GMATH_PI : 0.0f;
      ratio = fabsf(ratio);
      if (type <= AAX_LAST_WAVEFORM && ptype < AAX_PROCESSING_MAX)
      {
         _oalRingBuffer* rb = buf->ringbuffer;
         unsigned int no_samples, tracks, i, bit = 1;
         unsigned char bps, skip;
         float f, fs, fw;
         void** data;

         fw = FNMINMAX(rate, 1.0f, 22050.0f);
         skip = (unsigned char)(1.0f + 99.0f*_MINMAX(rate, 0.0f, 1.0f));

         fs = _oalRingBufferGetFrequency(rb);
         bps = _oalRingBufferGetBytesPerSample(rb);
         tracks = _oalRingBufferGetNoTracks(rb);
         no_samples = _oalRingBufferGetNoSamples(rb);

         if (_oalRingBufferIsValid(rb))
         {
            float dt = no_samples/fs;
            fw = floorf((fw*dt)+0.5f)/dt;
         }
         else
         {
            float dt = rb->sample->no_samples_avail/fs;
            float duration = floorf((fw*dt)+0.5f)/fw;

            no_samples = ceilf(duration*fs);
            _oalRingBufferSetNoSamples(rb, no_samples);
            _oalRingBufferInit(rb, AAX_FALSE);
        }
        f = fs/fw;

         data = rb->sample->track;
         switch (ptype)
         {
         case AAX_OVERWRITE:
            for (i=0; i<tracks; i++) {
               memset(data[i], 0, bps*no_samples);
            }
            break;
         case AAX_MIX:
         {
            float ratio_orig = FNMINMAX(1.0f-ratio, 0.0f, 1.0f);

            ratio = 2.0f*(1.0f - ratio_orig);
            if (type & AAX_SINE_WAVE) ratio /= 2;
            if (type & AAX_SQUARE_WAVE) ratio /= 2;
            if (type & AAX_TRIANGLE_WAVE) ratio /= 2;
            if (type & AAX_SAWTOOTH_WAVE) ratio /= 2;
            if (type & AAX_IMPULSE_WAVE) ratio /= 2;
            if (type & AAX_WHITE_NOISE) ratio /= 2;
            if (type & AAX_PINK_NOISE) ratio /= 2;
            if (type & AAX_BROWNIAN_NOISE) ratio /= 2;

            for (i=0; i<tracks; i++) {
               _batch_mul_value(data[i], bps, no_samples, ratio_orig);
            }
            break;
         }
         case AAX_RINGMODULATE:
            ratio = -ratio;
            break;
         default:
            break;
         }

         rv = AAX_TRUE;
         for (i=0; i<AAX_MAX_WAVE; i++)
         {
            float dc = 1.0; /* duty cicle for noise */
            switch (type & bit)
            {
            case AAX_SINE_WAVE:
               _bufferMixSineWave(data, f, bps, no_samples, tracks,
                                  ratio, phase);
               break;
            case AAX_SQUARE_WAVE:
               _bufferMixSquareWave(data, f, bps, no_samples, tracks,
                                    ratio, phase);
               break;
            case AAX_TRIANGLE_WAVE:
               _bufferMixTriangleWave(data, f, bps, no_samples, tracks,
                                      ratio, phase);
               break;
            case AAX_SAWTOOTH_WAVE:
               _bufferMixSawtooth(data, f, bps, no_samples, tracks,
                                  ratio, phase);
               break;
            case AAX_IMPULSE_WAVE:
               _bufferMixImpulse(data, f, bps, no_samples, tracks,
                                 ratio, phase);
               break;
            case AAX_WHITE_NOISE:
               _bufferMixWhiteNoise(data, no_samples, bps, tracks,
                                    ratio, dc, skip);
               break;
            case AAX_PINK_NOISE:
               if (buf->info) {
                  _bufferMixPinkNoise(data, no_samples, bps, tracks,
                                      ratio, buf->info->frequency, dc, skip);
               } else rv = AAX_FALSE;
               break;
            case AAX_BROWNIAN_NOISE:
               if (buf->info) {
                  _bufferMixBrownianNoise(data, no_samples, bps, tracks,
                                         ratio, buf->info->frequency, dc, skip);
               } else rv = AAX_FALSE;
               break;
            default:
               break;
            }
            bit <<= 1;
         }
#if 0
_aaxSoftwareDriverWriteFile("/tmp/wave.wav", AAX_OVERWRITE, *data, no_samples,
                            _oalRingBufferGetFrequency(rb) , tracks,
                            aaxBufferGetSetup(buffer, AAX_FORMAT));
#endif
      }
      else
      {
         if (ptype >= AAX_PROCESSING_MAX) {
            _aaxErrorSet(AAX_INVALID_PARAMETER + 5);
         } else {
            _aaxErrorSet(AAX_INVALID_PARAMETER + 3);
         }
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

void**
aaxBufferGetData(const aaxBuffer buffer)
{
   _buffer_t* buf = get_buffer(buffer);
   void** data = NULL;
   if (buf && buf->frequency)
   {
      unsigned int buf_samples, no_samples, tracks;
      unsigned int native_fmt, rb_format, pos;
      enum aaxFormat user_format;
      _oalRingBuffer *rb;
      char *ptr, bps;
      float fact;

      rb = buf->ringbuffer;
      fact = buf->frequency / _oalRingBufferGetFrequency(rb);
      pos = fact*buf->pos;

      no_samples = fact*_oalRingBufferGetNoSamples(rb) - pos;
      bps = _oalRingBufferGetBytesPerSample(rb);
      tracks = _oalRingBufferGetNoTracks(rb);
      buf_samples = tracks*no_samples;

      ptr = (char*)sizeof(void*);
      data = (void**)_aax_malloc(&ptr, no_samples*tracks*bps);
      if (data == NULL) 
      {
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         return data;
      }

      _oalRingBufferGetDataInterleaved(rb, ptr, no_samples, fact);
      *data = (void*)(ptr + pos*tracks*bps);

      user_format = buf->format;
      native_fmt = user_format & AAX_FORMAT_NATIVE;
      rb_format = _oalRingBufferGetFormat(rb);
      if (rb_format != native_fmt)
      {
         if (rb_format != AAX_PCM24S) 	/* first convert to signed 24-bit */
         {
            void **ndata;
            char *ptr;

            ptr = (char*)sizeof(void*);
            ndata = (void**)_aax_malloc(&ptr, buf_samples*sizeof(int32_t));
            if (ndata)
            {
               *ndata = (void*)ptr;
               bufConvertDataToPCM24S(*ndata, *data, buf_samples, rb_format);
               free(data);
               data = ndata;
            }
         } 

         if (native_fmt != AAX_PCM24S)	/* then convert to requested format */
         {
            int new_bps = aaxGetBytesPerSample(native_fmt);
            int block_smp = BLOCKSIZE_TO_SMP(buf->blocksize);
            int new_samples = ((no_samples/block_smp)+1)*block_smp;
            void** ndata;
            char *ptr;

            ptr = (char*)sizeof(void*);
            ndata = (void**)_aax_malloc(&ptr, tracks*new_samples*new_bps);
            if (ndata)
            {
                *ndata = (void*)ptr;
                bufConvertDataFromPCM24S(*ndata, *data, tracks, buf_samples,
                                        native_fmt, no_samples, buf->blocksize);
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
   }
   else if (buf) {	/* buf->frequency is not set */
      _aaxErrorSet(AAX_INVALID_STATE);
   } else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }

   return data;
}

int
aaxBufferDestroy(aaxBuffer buffer)
{
   _buffer_t* buf = get_buffer(buffer);
   int rv = AAX_FALSE;
   if (buf) {
     rv = free_buffer(buf);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

int
aaxBufferWriteToFile(aaxBuffer buffer, const char *file, enum aaxProcessingType type)
{
   int rv = AAX_FALSE;
   if (aaxIsValid(buffer, AAX_BUFFER))
   {
      enum aaxFormat format = aaxBufferGetSetup(buffer, AAX_FORMAT);
      unsigned int no_samples = aaxBufferGetSetup(buffer, AAX_NO_SAMPLES);
      unsigned int freq = aaxBufferGetSetup(buffer, AAX_FREQUENCY);
      char no_tracks = aaxBufferGetSetup(buffer, AAX_TRACKS);
      void **data = aaxBufferGetData(buffer);

      _aaxSoftwareDriverWriteFile(file, type, *data,
                                  no_samples, freq, no_tracks, format);
      free(data);

      rv = AAX_TRUE;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

static unsigned char  _oalFormatsBPS[AAX_FORMAT_MAX] =
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
get_buffer(aaxBuffer buffer)
{
   _buffer_t *handle = (_buffer_t *)buffer;
   _buffer_t* rv  = NULL;
   if (handle && handle->id == BUFFER_ID) {
      rv = handle;
   }
   return rv;
}

int
free_buffer(_buffer_t* buf)
{
   int rv = AAX_FALSE;
   if (buf)
   {
      if (--buf->ref_counter == 0)
      {
         _oalRingBufferDelete(buf->ringbuffer);

         /* safeguard against using already destroyed handles */
         buf->id = 0xdeadbeef;
         free(buf);
      }
      rv = AAX_TRUE;
   }
   return rv;
}

