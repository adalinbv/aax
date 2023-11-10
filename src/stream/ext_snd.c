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

#include <stdio.h>
#include <fcntl.h>	/* SEEK_*, O_* */
#include <errno.h>
#include <assert.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#endif

#include <base/databuffer.h>
#include <base/memory.h>

#include "device.h"
#include "audio.h"
#include "api.h"

// Spec:
// https://en.wikipedia.org/wiki/Au_file_format
// All fields are stored in big-endian format, including the sample data. 
enum sndEncoding
{
   UNSUPPORTED = 0,
   MULAW_SND_FILE,
   PCM8S_SND_FILE,
   PCM16S_SND_FILE,
   PCM24S_SND_FILE,
   PCM32S_SND_FILE,
   FLOAT32_SND_FILE,
   FLOAT64_SND_FILE,

   IMA4_SND_FILE = 23,
   ALAW_SND_FILE = 27
};

typedef struct
{
   _fmt_t *fmt;

   char *annotation;

   int mode;
   int bitrate;
   int bits_sample;
   size_t max_samples;
   _buffer_info_t info;

   enum sndEncoding encoding;
   char copy_to_buffer;
   char capturing;

   union
   {
      struct
      {
         float update_dt;
         uint32_t fact_chunk_offs;
         uint32_t data_chunk_offs;
      } write;

      struct
      {
         size_t datasize; // combined size of all tracks
         ssize_t size; // size of the file: headers + data
         uint32_t blockbufpos;
         uint32_t last_tag;
         uint32_t channel_mask;
      } read;
   } io;

   _data_t *sndBuffer;

} _driver_t;

static enum aaxFormat _getAAXFormatFromSNDFormat(unsigned int);
static int _aaxFormatDriverReadHeader(_driver_t*, uint8_t*, size_t, size_t*);

#define SND_HEADER_SIZE		6
#define DEFAULT_OUTPUT_RATE	8000

int
_snd_detect(UNUSED(_ext_t *ext), int mode)
{
   // only capturing (for now)
   return (mode == 0) ? AAX_TRUE : AAX_FALSE;
}

int
_snd_setup(_ext_t *ext, int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
{
   int bits_sample = aaxGetBitsPerSample(format);
   int rv = AAX_FALSE;

   assert(ext != NULL);
   assert(ext->id == NULL);

   if (bits_sample)
   {
      _driver_t *handle = calloc(1, sizeof(_driver_t));
      if (handle)
      {
         handle->mode = mode;
         handle->capturing = (mode > 0) ? 0 : 1;
         handle->encoding = MULAW_SND_FILE;
         handle->bits_sample = bits_sample;
         handle->info.blocksize = tracks*bits_sample/8;
         handle->info.rate = freq;
         handle->info.no_tracks = tracks;
         handle->info.fmt = format;
         handle->info.no_samples = no_samples;
         handle->bitrate = bitrate;
         handle->max_samples = 0;

         if (handle->capturing)
         {
            handle->info.no_samples = 0;
            handle->max_samples = 0;
            *bufsize = 4096;
         }
         else /* playback */
         {
         }
         ext->id = handle;
         rv = AAX_TRUE;
      }
      else {
         _AAX_FILEDRVLOG("SND: Insufficient memory");
      }
   }
   else {
      _AAX_FILEDRVLOG("SND: Unsupported format");
   }

   return rv;
}

void*
_snd_open(_ext_t *ext, void_ptr buf, ssize_t *bufsize, size_t fsize)
{
   _driver_t *handle = ext->id;
   void *rv = NULL;
   if (handle)
   {
      if (!handle->sndBuffer) {
         handle->sndBuffer = _aaxDataCreate(1, 16384, 0);
      }

      if (!handle->capturing) { // write
      }
      else if (buf && bufsize && *bufsize > SND_HEADER_SIZE)
      {
         int res = _aaxFormatDriverReadHeader(handle, buf, *bufsize, &fsize);
         if (res > 0)
         {
            _fmt_type_t fmt = _FMT_PCM;

            buf = ((uint8_t*)buf+res);
            *bufsize -= res;

            handle->fmt = _fmt_create(fmt, handle->mode);
            if (handle->fmt)
            {
               handle->fmt->open(handle->fmt, handle->mode, NULL, NULL, 0);
               handle->fmt->set(handle->fmt, __F_TRACKS, handle->info.no_tracks);
               handle->fmt->set(handle->fmt, __F_COPY_DATA, handle->copy_to_buffer);
               if (handle->fmt->setup(handle->fmt, fmt, handle->info.fmt))
               {
                  handle->fmt->set(handle->fmt, __F_FREQUENCY, handle->info.rate);
                  handle->fmt->set(handle->fmt, __F_BITRATE, handle->bitrate);
                  handle->fmt->set(handle->fmt,__F_NO_SAMPLES, handle->info.no_samples);
                  handle->fmt->set(handle->fmt, __F_BITS_PER_SAMPLE, handle->bits_sample);

                  if (handle->info.fmt == AAX_IMA4_ADPCM)
                  {
                     handle->fmt->set(handle->fmt, __F_BLOCK_SIZE,
                                 handle->info.blocksize/handle->info.no_tracks);
                     handle->fmt->set(handle->fmt, __F_BLOCK_SAMPLES,
                                  MSIMA_BLOCKSIZE_TO_SMP(handle->info.blocksize,
                                                       handle->info.no_tracks));
                  } else {
                     handle->fmt->set(handle->fmt, __F_BLOCK_SIZE,
                                                   handle->info.blocksize);
                  }
                  handle->fmt->set(handle->fmt, __F_POSITION,
                                                handle->io.read.blockbufpos);
#if 0
 printf("format: 0x%x\n", handle->info.fmt);
 printf("sample rate: %5.1f\n", handle->info.rate);
 printf("no. tracks : %i\n", handle->info.no_tracks);
 printf("bits/sample: %i\n", handle->bits_sample);
 printf("bitrate: %i\n", handle->bitrate);
 printf("blocksize:  %i\n", handle->info.blocksize);
#endif
                  rv = handle->fmt->open(handle->fmt, handle->mode,
                                        buf, bufsize, handle->io.read.datasize);
                  if (!rv && *bufsize) {
                     _snd_fill(ext, buf, bufsize);
                  }
               }
               else
               {
                  *bufsize = 0;
                  handle->fmt = _fmt_free(handle->fmt);
               }
            }
         }
      }
   }
   else {
      _AAX_FILEDRVLOG("SND: Internal error: handle id equals 0");
   }

   return rv;

}

int
_snd_close(_ext_t *ext)
{
   _driver_t *handle = ext->id;
   int res = AAX_TRUE;

   if (handle)
   {
      _aaxDataDestroy(handle->sndBuffer);
      if (handle->fmt)
      {
         handle->fmt->close(handle->fmt);
         _fmt_free(handle->fmt);
      }

      if (handle->annotation) free(handle->annotation);
      free(handle);
   }

   return res;
}

int
_snd_flush(_ext_t *ext)
{
   return 0;
}

void*
_snd_update(UNUSED(_ext_t *ext), UNUSED(size_t *offs), UNUSED(ssize_t *size), UNUSED(char close))
{
// _driver_t *handle = ext->id;
   void *rv = NULL;

   return rv;
}

size_t
_snd_fill(_ext_t *ext, void_ptr sptr, ssize_t *bytes)
{
   _driver_t *handle = ext->id;
   size_t res, rv = __F_PROCESS;

   *bytes = res = _aaxDataAdd(handle->sndBuffer, 0, sptr, *bytes);
   if (res > 0)
   {
      void *data = _aaxDataGetData(handle->sndBuffer, 0);
      ssize_t avail = _aaxDataGetDataAvail(handle->sndBuffer, 0);

      if (avail)
      {
          handle->fmt->fill(handle->fmt, data, &avail);
         _aaxDataMove(handle->sndBuffer, 0, NULL, avail);
      }
   }

   return rv;
}

size_t
_snd_copy(_ext_t *ext, int32_ptr dptr, size_t offset, size_t *num)
{
   _driver_t *handle = ext->id;
   ssize_t rv = handle->fmt->copy(handle->fmt, dptr, offset, num);
   if (rv > 0) {
      handle->io.read.datasize -= rv;
   }
   if (handle->io.read.datasize == 0) {
      rv = __F_EOF;
   }
   return rv;
}

size_t
_snd_cvt_from_intl(_ext_t *ext, int32_ptrptr dptr, size_t offset, size_t *num)
{
   _driver_t *handle = ext->id;
   ssize_t rv = handle->fmt->cvt_from_intl(handle->fmt, dptr, offset, num);
   if (rv > 0) {
      handle->io.read.datasize -= rv;
   }
   if (handle->io.read.datasize == 0) {
      rv = __F_EOF;
   }
   return rv;
}

size_t
_snd_cvt_to_intl(_ext_t *ext, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = ext->id;
   size_t rv;

   rv = handle->fmt->cvt_to_intl(handle->fmt, dptr, sptr, offs, num,
                                 scratch, scratchlen);
   handle->info.no_samples += *num;
   handle->io.write.update_dt += (float)*num/handle->info.rate;

   return rv;
}

int
_snd_set_name(_ext_t *ext, enum _aaxStreamParam param, const char *desc)
{
   _driver_t *handle = ext->id;
   int rv = handle->fmt->set_name(handle->fmt, param, desc);
   return rv;
}

char*
_snd_name(_ext_t *ext, enum _aaxStreamParam param)
{
   _driver_t *handle = ext->id;
   char *rv = handle->fmt->name(handle->fmt, param);

   if (!rv)
   {
      switch(param)
      {
      case __F_COMMENT:
         rv = handle->annotation;
         break;
      default:
         break;
      }
   }

   return rv;
}

char*
_snd_interfaces(UNUSED(int ext), int mode)
{
   static const char *rd[2] = { "*.snd\0", "\0" };
   return (char *)rd[mode];
}

int
_snd_extension(char *ext)
{
   return (ext && (!strcasecmp(ext, "snd") || !strcasecmp(ext, "au"))) ? 1 : 0;
}

float
_snd_get(_ext_t *ext, int type)
{
   _driver_t *handle = ext->id;
   float rv = 0.0f;

   switch (type)
   {
   case __F_FMT:
      rv = handle->info.fmt;
      break;
   case __F_NO_BYTES: // size per track
      rv = handle->io.read.datasize/handle->info.no_tracks;
      break;
   case __F_BLOCK_SIZE:
      rv = handle->info.blocksize;
      break;
   case __F_CHANNEL_MASK:
      rv = handle->io.read.channel_mask;
      break;
   case __F_LOOP_COUNT:
      rv = handle->info.loop_count;
      break;
   case __F_LOOP_START:
      rv = handle->info.loop_start;
      break;
   case __F_LOOP_END:
      rv = handle->info.loop_end;
      break;
   case __F_BASE_FREQUENCY:
      rv = handle->info.base_frequency;
      break;
   case __F_LOW_FREQUENCY:
      rv = handle->info.low_frequency;
      break;
   case __F_HIGH_FREQUENCY:
      rv = handle->info.high_frequency;
      break;
   case __F_PITCH_FRACTION:
      rv = handle->info.pitch_fraction;
      break;
   case __F_TREMOLO_RATE:
      rv = handle->info.tremolo.rate;
      break;
   case __F_TREMOLO_DEPTH:
      rv = handle->info.tremolo.depth;
      break;
   case __F_TREMOLO_SWEEP:
      rv = handle->info.tremolo.sweep;
      break;
   case __F_VIBRATO_RATE:
      rv = handle->info.vibrato.rate;
      break;
   case __F_VIBRATO_DEPTH:
      rv = handle->info.vibrato.depth;
      break;
   case __F_VIBRATO_SWEEP:
      rv = handle->info.vibrato.sweep;
      break;
   default:
      if (handle->fmt) {
         rv = handle->fmt->get(handle->fmt, type);
      }
      break;
   }
   return rv;
}

float
_snd_set(_ext_t *ext, int type, float value)
{
   _driver_t *handle = ext->id;
   float rv = 0.0f;

   switch (type)
   {
   case __F_COPY_DATA:
      handle->copy_to_buffer = value;
      break;
   default:
      if (handle->fmt) {
         rv = handle->fmt->set(handle->fmt, type, value);
      }
      break;
   }
   return rv;
}


/* -------------------------------------------------------------------------- */
int
_aaxFormatDriverReadHeader(_driver_t *handle, uint8_t *buf, size_t bufsize, size_t *step)
{
   size_t fsize = *step;
   uint8_t *ch = buf;
   int rv = __F_EOF;
   uint32_t curr;

   curr = read32be(&ch, &bufsize);
   if (curr == 0x2e736e64)	// .snd
   {
      curr = read32be(&ch, &bufsize); // offset to the data in bytes
      handle->io.read.size = curr;
      if (handle->io.read.size <= bufsize)
      {
         char field[COMMENT_SIZE+1];
         uint32_t clen;

         *step = rv = handle->io.read.size;

         curr = read32be(&ch, &bufsize);
         if (curr != -1) { // AUDIO_UNKNOWN_SIZE
            handle->io.read.datasize = curr;
         } else {
            handle->io.read.datasize = fsize - handle->io.read.size;
         }

         curr = read32be(&ch, &bufsize);
         handle->encoding = curr;

         curr = read32be(&ch, &bufsize);
         handle->info.rate = curr;

         curr = read32be(&ch, &bufsize);
         handle->info.no_tracks = curr;

         handle->io.read.size -= 24;
         clen = _MIN(COMMENT_SIZE, handle->io.read.size);
         readstr(&ch, field, clen, &bufsize);
         handle->annotation = strdup(field);

         handle->info.fmt = _getAAXFormatFromSNDFormat(handle->encoding);
         handle->bits_sample = aaxGetBitsPerSample(handle->info.fmt);
         handle->info.no_samples = handle->io.read.datasize/(handle->info.no_tracks*handle->bits_sample/8);
         handle->info.blocksize = handle->bits_sample*handle->info.no_tracks/8;
         handle->bitrate = handle->info.rate*handle->bits_sample*handle->info.no_tracks;

         if (handle->info.fmt == AAX_FORMAT_NONE) {
            rv = __F_EOF;
         }
#if 0
 printf("format: 0x%x, bits/sample: %i, rate: %f, tracks: %i\n", 
         handle->info.fmt, handle->bits_sample, handle->info.rate, handle->info.no_tracks);
#endif
      }
   }
   return rv;
}

static enum aaxFormat
_getAAXFormatFromSNDFormat(unsigned int format)
{
   enum aaxFormat rv = AAX_FORMAT_NONE;

   switch (format)
   {
   case MULAW_SND_FILE:
      rv = AAX_MULAW;
      break;
   case PCM8S_SND_FILE:
      rv = AAX_PCM8S;
      break;
   case PCM16S_SND_FILE:
      rv = AAX_PCM16S_BE;
      break;
   case PCM24S_SND_FILE:
      rv = AAX_PCM24S_PACKED_BE;
      break;
   case PCM32S_SND_FILE:
      rv = AAX_PCM32S_BE;
      break;
   case FLOAT32_SND_FILE:
      rv = AAX_FLOAT_BE;
      break;
   case FLOAT64_SND_FILE:
      rv = AAX_DOUBLE_BE;
      break;
   case IMA4_SND_FILE:
      rv = AAX_IMA4_ADPCM;
      break;
   case ALAW_SND_FILE:
      rv = AAX_ALAW;
      break;
   default:
      break;
   }
   return rv;
}

