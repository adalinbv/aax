/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif

#include <xml.h>

#include <base/dlsym.h>

#include <api.h>
#include <arch.h>

#include "extension.h"
#include "format.h"
#include "fmt_opus.h"
#include "audio.h"

// https://android.googlesource.com/platform/external/libopus/+/refs/heads/master-soong/doc/trivial_example.c
#define FRAME_SIZE 960
#define MAX_FRAME_SIZE (6*FRAME_SIZE)
#define MAX_PACKET_SIZE (2*3*1276)
#define OPUS_SAMPLE_RATE 48000

DECL_FUNCTION(opus_multistream_decoder_create);
DECL_FUNCTION(opus_multistream_decoder_destroy);
DECL_FUNCTION(opus_multistream_decoder_ctl);
DECL_FUNCTION(opus_multistream_decode_float);
DECL_FUNCTION(opus_multistream_decode);

DECL_FUNCTION(opus_encoder_create);
DECL_FUNCTION(opus_encoder_destroy);
DECL_FUNCTION(opus_encoder_ctl);
DECL_FUNCTION(opus_encode);

DECL_FUNCTION(opus_strerror);
DECL_FUNCTION(opus_get_version_string);

typedef struct
{
   void *id;
   char *artist;
   char *original;
   char *title;
   char *album;
   char *trackno;
   char *date;
   char *genre;
   char *composer;
   char *comments;
   char *copyright;
   char *website;
   char *image;

   void *audio;
   int mode;

   char capturing;

   uint8_t no_tracks;
   uint8_t bits_sample;
   unsigned int frequency;
   unsigned int bitrate;
   unsigned int blocksize;
   enum aaxFormat format;
   size_t no_samples;
   size_t max_samples;

   _data_t *opusBuffer;
   _data_t *floatBuffer;

   int channel_mapping;

   /* The rest is only used if channel_mapping != 0 */
   int nb_streams;
   int nb_coupled;
   unsigned char stream_map[255];

} _driver_t;

static int _aaxReadOpusHeader(_driver_t*, char*, size_t);

int
_opus_detect(_fmt_t *fmt, int mode)
{
   void *audio = NULL;
   int rv = AAX_FALSE;

   audio = _aaxIsLibraryPresent("opus", "0");
   if (!audio) {
      audio = _aaxIsLibraryPresent("libopus", "0");
   }

   if (audio)
   {
      char *error;

      _aaxGetSymError(0);

      TIE_FUNCTION(opus_multistream_decoder_create);
      if (popus_multistream_decoder_create)
      {
         TIE_FUNCTION(opus_multistream_decoder_destroy);
         TIE_FUNCTION(opus_multistream_decoder_ctl);
         TIE_FUNCTION(opus_multistream_decode_float);
         TIE_FUNCTION(opus_multistream_decode);

         TIE_FUNCTION(opus_encoder_create);
         TIE_FUNCTION(opus_encoder_destroy);
         TIE_FUNCTION(opus_encoder_ctl);
         TIE_FUNCTION(opus_encode);

         error = _aaxGetSymError(0);
         if (!error)
         {
            /* not required but useful */
            TIE_FUNCTION(opus_strerror);
            TIE_FUNCTION(opus_get_version_string);

            fmt->id = calloc(1, sizeof(_driver_t));
            if (fmt->id)
            {
               _driver_t *handle = fmt->id;

               handle->audio = audio;
               handle->mode = mode;
               handle->capturing = (mode == 0) ? 1 : 0;
               handle->blocksize = FRAME_SIZE;

               rv = AAX_TRUE;
            }             
            else {
               _AAX_FILEDRVLOG("OPUS: Insufficient memory");
            }
         }
      }
   }

   return rv;
}

void*
_opus_open(_fmt_t *fmt, void *buf, size_t *bufsize, UNUSED(size_t fsize))
{
   _driver_t *handle = fmt->id;
   void *rv = NULL;

   assert(bufsize);

   if (handle)
   {
      if (!handle->opusBuffer)
      {
         unsigned int bufsize = MAX_FRAME_SIZE*handle->no_tracks*sizeof(float);
         handle->opusBuffer = _aaxDataCreate(bufsize, 1);
      }

      if (!handle->floatBuffer)
      {
         unsigned int bufsize = MAX_PACKET_SIZE*handle->no_tracks*sizeof(float);
         handle->floatBuffer = _aaxDataCreate(2*bufsize, 1);
      }

      if (handle->opusBuffer && handle->floatBuffer)
      {
         if (handle->capturing)
         {
            if (!handle->id)
            {
               int err, tracks = handle->no_tracks;
               int32_t freq = OPUS_SAMPLE_RATE;

               /* https://tools.ietf.org/html/rfc7845.html#page-12 */
               handle->frequency = freq;
               handle->blocksize = FRAME_SIZE;
               handle->format = AAX_PCM24S;
               handle->bits_sample = aaxGetBitsPerSample(handle->format);

               if (_aaxReadOpusHeader(handle, buf, *bufsize) > 0)
               {
                  handle->id = popus_multistream_decoder_create(freq, tracks,
                                                             handle->nb_streams,
                                                             handle->nb_coupled,
                                                             handle->stream_map,
                                                             &err);
                  if (!handle->id && popus_strerror)
                  {
                     char s[1025];
                     snprintf(s, 1024, "OPUS: Unable to create a handle: %s",
                                        popus_strerror(err));
                     s[1024] = 0;
                     _aaxStreamDriverLog(NULL, 0, 0, s);
                  }
               }
            }
         }
      }
   }
   else
   {
      _AAX_FILEDRVLOG("OPUS: Internal error: handle id equals 0");
   }

   return rv;
}

void
_opus_close(_fmt_t *fmt)
{
   _driver_t *handle = fmt->id;

   if (handle)
   {
      popus_encoder_destroy(handle->id);
      handle->id = NULL;

      _aaxDataDestroy(handle->opusBuffer);
      _aax_aligned_free(handle->floatBuffer->data);

      free(handle->trackno);
      free(handle->artist);
      free(handle->title);
      free(handle->album);
      free(handle->date);
      free(handle->genre);
      free(handle->comments);
      free(handle->composer);
      free(handle->copyright);
      free(handle->original);
      free(handle->website);
      free(handle->image);
      free(handle);
   }
}

int
_opus_setup(UNUSED(_fmt_t *fmt), UNUSED(_fmt_type_t pcm_fmt), UNUSED(enum aaxFormat aax_fmt))
{
   return AAX_TRUE;
}

size_t
_opus_fill(_fmt_t *fmt, void_ptr sptr, size_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t rv = __F_PROCESS;

   if (_aaxDataAdd(handle->opusBuffer, sptr, *bytes)  == 0) {
      *bytes = 0;
   }

   return rv;
}

size_t
_opus_copy(_fmt_t *fmt, int32_ptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   unsigned int bits, tracks, framesize, packet_sz;
   size_t req, rv = 0;
   float *floats;
   int n;

   req = *num;
   tracks = handle->no_tracks;
   bits = handle->bits_sample;
   framesize = tracks*bits/8;
   packet_sz = handle->blocksize;
   *num = 0;

   floats = (float*)handle->floatBuffer->data;
   do
   {
      size_t avail = handle->floatBuffer->avail;
      if (avail > 0)
      {
         unsigned int max = _MIN(req, avail/framesize);
         if (max)
         {
            _batch_cvt24_ps(dptr+dptr_offs, floats, max*tracks);
            _aaxDataMove(handle->floatBuffer, NULL, max*framesize);

            dptr_offs += max;
            handle->no_samples += max;
            req -= max;
            *num = max;
         }
      }

      if (req > 0)
      {
         size_t bufsize  = _MIN(packet_sz, handle->opusBuffer->avail);
         if (bufsize == packet_sz)
         {
            size_t floatsmp = handle->floatBuffer->size/framesize;
            unsigned char *buf = handle->opusBuffer->data;

            n = popus_multistream_decode_float(handle->id, buf, bufsize,
                                                           floats, floatsmp, 0);
            if (n <= 0) break;

            handle->floatBuffer->avail = n*framesize;
            rv += _aaxDataMove(handle->opusBuffer, NULL, bufsize);
         }
         else {
            break;
         }
      }
   }
   while (req > 0);

   return rv;
}

size_t
_opus_cvt_from_intl(_fmt_t *fmt, int32_ptrptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   unsigned int bits, tracks, framesize, packet_sz;
   size_t req, rv = __F_NEED_MORE;
   float *floats;
   int n;

   req = *num;
   tracks = handle->no_tracks;
   bits = handle->bits_sample;
   framesize = tracks*bits/8;
   packet_sz = handle->blocksize;
   *num = 0;

   floats = (float*)handle->floatBuffer->data;
   do
   {
      size_t avail = handle->floatBuffer->avail;
      if (avail > 0)
      {
         unsigned int max = _MIN(req, avail/framesize);
         if (max)
         {
            _batch_cvt24_ps_intl(dptr, floats, dptr_offs, tracks, max);
            _aaxDataMove(handle->floatBuffer, NULL, max*framesize);

            dptr_offs += max;
            handle->no_samples += max;
            *num += max;
            req -= max;
         }
      }

      if (req > 0)
      {
         size_t bufsize  = _MIN(packet_sz, handle->opusBuffer->avail);
         if (bufsize == packet_sz)
         {
            size_t floatsmp = handle->floatBuffer->size/framesize;
            unsigned char *buf = handle->opusBuffer->data;

            n = popus_multistream_decode_float(handle->id, buf, bufsize,
                                                           floats, floatsmp, 0);
            if (n <= 0) {
               *num = req;
               rv = __F_NEED_MORE;
               break;
            }

            handle->floatBuffer->avail = n*framesize;
            rv += _aaxDataMove(handle->opusBuffer, NULL, bufsize);
         }
         else {
            break;
         }
      }
   }
   while (req > 0);

   return rv;
}

size_t
_opus_cvt_to_intl(_fmt_t *fmt, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = fmt->id;
   int res;

   assert(scratchlen >= *num*handle->no_tracks*sizeof(int32_t));

   handle->no_samples += *num;
   _batch_cvt16_intl_24(scratch, sptr, offs, handle->no_tracks, *num);

   /*
    * -- about *num --
    * Number of samples per channel in the input signal. This must be an Opus
    * frame size for the encoder's sampling rate. For example, at 48 kHz the
    * permitted values are 120, 240, 480, 960, 1920, and 2880. Passing in a
    * duration of less than 10 ms (480 samples at 48 kHz) will prevent the
    * encoder from using the LPC or hybrid modes. 
    */
   res = popus_encode(handle->id, scratch, *num,
                      handle->opusBuffer->data, handle->opusBuffer->avail);
   _aax_memcpy(dptr, handle->opusBuffer, res);

   return res;
}

char*
_opus_name(_fmt_t *fmt, enum _aaxStreamParam param)
{
   _driver_t *handle = fmt->id;
   char *rv = NULL;

   switch(param)
   {
   case __F_ARTIST:
      rv = handle->artist;
      break;
   case __F_TITLE:
      rv = handle->title;
      break;
   case __F_COMPOSER:
      rv = handle->composer;
      break;
   case __F_GENRE:
      rv = handle->genre;
      break;
   case __F_TRACKNO:
      rv = handle->trackno;
      break;
   case __F_ALBUM:
      rv = handle->album;
      break;
   case __F_DATE:
      rv = handle->date;
      break;
   case __F_COMMENT:
      rv = handle->comments;
      break;
   case __F_COPYRIGHT:
      rv = handle->copyright;
      break;
   case __F_ORIGINAL:
      rv = handle->original;
      break;
   case __F_WEBSITE:
      rv = handle->website;
      break;
   case __F_IMAGE:
      rv = handle->image;
      break;
   default:
      break;
   }
   return rv;
}

off_t
_opus_get(_fmt_t *fmt, int type)
{
   _driver_t *handle = fmt->id;
   off_t rv = 0;

   switch(type)
   {
   case __F_FMT:
      rv = handle->format;
      break;
   case __F_TRACKS:
      rv = handle->no_tracks;
      break;
   case __F_FREQUENCY:
      rv = handle->frequency;
      break;
   case __F_BITS_PER_SAMPLE:
      rv = handle->bits_sample;
      break;
   case __F_BLOCK_SIZE:
      rv = handle->blocksize;
      break;
   case __F_NO_SAMPLES:
      rv = handle->max_samples;
      break;
   default:
      if (type & __F_NAME_CHANGED)
      {
         switch (type & ~__F_NAME_CHANGED)
         {
         default:
            break;
         }
      }
      break;
   }
   return rv;
}

off_t
_opus_set(_fmt_t *fmt, int type, off_t value)
{
   _driver_t *handle = fmt->id;
   off_t rv = 0;

   switch(type)
   {
   case __F_BLOCK_SIZE:
      handle->blocksize = value;
      break;
   case __F_FREQUENCY:
      handle->frequency = value;
      break;
   case __F_RATE:
      handle->bitrate = value;
      break;
   case __F_TRACKS:
      handle->no_tracks = value;
      break;
   case __F_NO_SAMPLES:
      handle->no_samples = value;
      handle->max_samples = value;
      break;
   case __F_BITS_PER_SAMPLE:
      handle->bits_sample = value;
      break;
   case __F_IS_STREAM:
      break;
   case __F_POSITION:
      break;
   default:
      break;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

// https://tools.ietf.org/html/rfc7845.html#page-12
#define OPUS_ID_HEADER_SIZE	(4*5-1)
static int
_aaxReadOpusHeader(_driver_t *handle, char *h, size_t len)
{
   int rv = __F_EOF;

   if (len >= OPUS_ID_HEADER_SIZE)
   {
      int version = h[8];
      if (version == 1)
      {
         unsigned char mapping_family;
//       int gain;

         handle->format = AAX_FLOAT;
         handle->no_tracks = (unsigned char)h[9];
         handle->nb_streams = 1;
         handle->nb_coupled = handle->no_tracks/2;
         handle->frequency = *((uint32_t*)h+3);
//       handle->pre_skip = (unsigned)h[10] << 8 | h[11];
//       handle->no_samples = -handle->pre_skip;

//       gain = (int)h[16] << 8 | h[17];
//       handle->gain = pow(10, (float)gain/(20.0f*256.0f));

         mapping_family = h[18];
         if ((mapping_family == 0 || mapping_family == 1) &&
             (handle->no_tracks > 1) && (handle->no_tracks <= 8))
         {
             /*
              * The 'channel mapping table' MUST be omitted when the channel
              * mapping family s 0, but is REQUIRED otherwise.
              */
             if (mapping_family == 1)
             {
                 handle->nb_streams = h[19];
                 handle->nb_coupled = h[20];
                 if ((handle->nb_streams > 0) &&
                     (handle->nb_streams <= handle->nb_coupled))
                 {
                    // what follows is 'no_tracks' bytes for the channel mapping
                    rv = OPUS_ID_HEADER_SIZE + handle->no_tracks + 2;
                    if (rv <= (int)len) {
                       rv = __F_NEED_MORE;
                    }
                 }
             }
             else {
                rv = OPUS_ID_HEADER_SIZE;
             }
         }
      }

#if 0
{
  uint32_t *header = (uint32_t*)h;
  float gain = (int)h[16] << 8 | h[17];
  printf("\n-- Opus Identification Header:\n");
  printf("  0: %08x %08x ('%c%c%c%c%c%c%c%c')\n", header[0], header[1], h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7]);
  printf("  2: %08x (Version: %i, Tracks: %i, Pre Skip: %i)\n", header[2], h[8], (unsigned char)h[9], (unsigned)h[10] << 8 | h[11]);
  printf("  3: %08x (Original Sample Rate: %i)\n", header[3],  header[3]);
  printf("  4: %08x (Replay gain: %f, Mapping Family: %i)\n", header[4], pow(10, (float)gain/(20.0f*256.0f)), h[18]);
  printf("  5: %08x (Stream Count: %i, Coupled Count: %i)\n", header[5], handle->nb_streams, handle->nb_coupled);
}
#endif
   }

   return rv;
}
