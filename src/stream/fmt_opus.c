/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#include "audio.h"
#include "fmt_opus.h"

// https://android.googlesource.com/platform/external/libopus/+/refs/heads/master-soong/doc/trivial_example.c
#define FRAME_SIZE		960
#define MAX_FRAME_SIZE		(6*FRAME_SIZE)
#define MAX_PACKET_SIZE		(2*3*1276)
#define OPUS_SAMPLE_RATE	48000

#define MAX_OPUSBUFSIZE		(MAX_FRAME_SIZE*handle->no_tracks*sizeof(float))
#define MAX_FLOATBUFSIZE	(2*MAX_OPUSBUFSIZE)

DECL_FUNCTION(opus_multistream_decoder_create);
DECL_FUNCTION(opus_multistream_decoder_destroy);
DECL_FUNCTION(opus_multistream_decoder_ctl);
DECL_FUNCTION(opus_multistream_decode_float);
DECL_FUNCTION(opus_multistream_decode);

DECL_FUNCTION(opus_decoder_create);
DECL_FUNCTION(opus_decoder_destroy);
DECL_FUNCTION(opus_decoder_ctl);
DECL_FUNCTION(opus_decode_float);
DECL_FUNCTION(opus_decode);

DECL_FUNCTION(opus_encoder_create);
DECL_FUNCTION(opus_encoder_destroy);
DECL_FUNCTION(opus_encoder_ctl);
DECL_FUNCTION(opus_encode);

DECL_FUNCTION(opus_strerror);
DECL_FUNCTION(opus_get_version_string);

typedef struct
{
   void *id;

   char artist_changed;
   char title_changed;

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

   int mode;

   char capturing;
   char recover;

   uint8_t no_tracks;
   uint8_t bits_sample;
   unsigned int frequency;
   unsigned int bitrate;
   unsigned int blocksize;
   enum aaxFormat format;
   size_t no_samples;
   size_t max_samples;
   size_t pre_skip;
   float gain;

   _data_t *opusBuffer;
   _data_t *pcmBuffer;

   int channel_mapping;

   /* The rest is only used if channel_mapping != 0 */
   int nb_streams;
   int nb_coupled;
   unsigned char stream_map[255];

} _driver_t;

static int _aaxReadOpusHeader(_driver_t*);
// static int _aaxReadOpusComment(_driver_t *);

int
_opus_detect(UNUSED(_fmt_t *fmt), UNUSED(int mode))
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

         TIE_FUNCTION(opus_decoder_create);
         TIE_FUNCTION(opus_decoder_destroy);
         TIE_FUNCTION(opus_decoder_ctl);
         TIE_FUNCTION(opus_decode_float);
         TIE_FUNCTION(opus_decode);

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

            rv = AAX_TRUE;
         }
      }
   }

   return rv;
}

void*
_opus_open(_fmt_t *fmt, int mode, void *buf, size_t *bufsize, UNUSED(size_t fsize))
{
   _driver_t *handle = fmt->id;
   void *rv = NULL;

   if (!handle)
   {
      handle = fmt->id = calloc(1, sizeof(_driver_t));
      if (fmt->id)
      {
         handle->mode = mode;
         handle->capturing = (mode == 0) ? 1 : 0;
         handle->blocksize = FRAME_SIZE;
      }
      else {
         _AAX_FILEDRVLOG("OPUS: Insufficient memory");
      }
   }

   if (handle && buf && bufsize)
   {
      if (!handle->opusBuffer) {
         handle->opusBuffer = _aaxDataCreate(MAX_OPUSBUFSIZE, 1);
      }

      if (!handle->pcmBuffer) {
         handle->pcmBuffer = _aaxDataCreate(MAX_FLOATBUFSIZE, 1);
      }

      if (handle->opusBuffer && handle->pcmBuffer)
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

               handle->id = popus_decoder_create(freq, tracks, &err);
               if (handle->id)
               {
                  size_t size = _MIN(*bufsize, MAX_OPUSBUFSIZE);
                  int res;

                  res = _aaxDataAdd(handle->opusBuffer, buf, size);
                  *bufsize = res;

                  res = _aaxReadOpusHeader(handle);
                  if (res) {
                     res = _aaxDataMove(handle->opusBuffer, NULL, res);
                  }
               }
               else
               {
                  char s[1025];
                  rv = buf;
                  if (popus_strerror) {
                     snprintf(s, 1024, "OPUS: Unable to create a handle: %s",
                                     popus_strerror(err));
                  } else {
                     snprintf(s, 1024, "OPUS: Unable to create a handle: %i",
                                     err);
                  }
                  s[1024] = 0;
                  _aaxStreamDriverLog(NULL, 0, 0, s);
               }
            }
            else
            {
               size_t res = _aaxDataAdd(handle->opusBuffer, buf, *bufsize);
               *bufsize = res;
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
      _aaxDataDestroy(handle->pcmBuffer);

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

   floats = (float*)handle->pcmBuffer->data;
   do
   {
      size_t avail = handle->pcmBuffer->avail;
      if (avail > 0)
      {
         unsigned int max = _MIN(req, avail/framesize);
         if (max)
         {
            _batch_cvt24_ps(dptr+dptr_offs, floats, max*tracks);
            _aaxDataMove(handle->pcmBuffer, NULL, max*framesize);

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
            size_t floatsmp = handle->pcmBuffer->size/framesize;
            unsigned char *buf = handle->opusBuffer->data;

            n = popus_decode_float(handle->id, buf, bufsize,
                                                           floats, floatsmp, 0);
            if (n <= 0) break;

            handle->pcmBuffer->avail = n*framesize;
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
   unsigned int req, tracks;
   unsigned char *pcmbuf;
   size_t pcmbufavail;
   size_t rv = 0;
   int ret;

   req = *num;
   tracks = handle->no_tracks;
   *num = 0;

   pcmbuf = handle->pcmBuffer->data;
   pcmbufavail = handle->pcmBuffer->avail;

   /* there is still data left in the buffer from the previous run */
   if (pcmbufavail)
   {
      unsigned int max = _MIN(req, pcmbufavail/sizeof(float));

      _batch_cvt24_ps_intl(dptr, pcmbuf, dptr_offs, tracks, max);
      _aaxDataMove(handle->pcmBuffer, NULL, max*sizeof(float));
      handle->no_samples += max;
      dptr_offs += max;
      req -= max;
      *num = max;
   }

   while (req > 0)
   {
      unsigned char *opusbuf;
      int32_t packet_size;
      int32_t pcmbufremain;
      int32_t pcmbufoffs;
      int frame_space;

      packet_size = handle->blocksize;
      if (handle->opusBuffer->avail < packet_size) {
         rv = __F_NEED_MORE;
         break;
      }

      pcmbufoffs = handle->pcmBuffer->avail;
      pcmbufremain = handle->pcmBuffer->size - pcmbufoffs;
      frame_space = pcmbufremain/(tracks*sizeof(float));

      // store the next chunk into the pcmBuffer;
      opusbuf = handle->opusBuffer->data;
      ret = popus_decode_float(handle->id, opusbuf, packet_size,
                              (float*)(pcmbuf+pcmbufoffs), frame_space, 0);
printf("popus_decode_float: %i, packet size: %i\n", ret, packet_size);
      if (ret > 0)
      {
         unsigned int max;

         rv += _aaxDataMove(handle->opusBuffer, NULL, packet_size);

         handle->pcmBuffer->avail += ret*sizeof(float);
         assert(handle->pcmBuffer->avail <= handle->pcmBuffer->size);

         max = _MIN(req, handle->pcmBuffer->avail/sizeof(float));

         _batch_cvt24_ps_intl(dptr, pcmbuf, dptr_offs, tracks, max);
         _aaxDataMove(handle->pcmBuffer, NULL, max*sizeof(float));
         handle->no_samples += max;
         dptr_offs += max;
         req -= max;
         *num += max;
      }
      else
      {
         rv = __F_NEED_MORE;
         break;
      }
   }

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
      handle->blocksize = rv = value;
      break;
   case __F_FREQUENCY:
      handle->frequency = rv = value;
      break;
   case __F_RATE:
      handle->bitrate = rv = value;
      break;
   case __F_TRACKS:
      handle->no_tracks = rv = value;
      break;
   case __F_NO_SAMPLES:
      handle->no_samples = value;
      handle->max_samples = rv = value;
      break;
   case __F_BITS_PER_SAMPLE:
      handle->bits_sample = rv = value;
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
_aaxReadOpusHeader(_driver_t *handle)
{
   char *h = (char*)handle->opusBuffer->data;
   size_t len = handle->opusBuffer->avail;
   int rv = __F_EOF;

   if (len >= OPUS_ID_HEADER_SIZE && !strncasecmp(h, "OpusHead", 8))
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


#if 0
// https://xiph.org/vorbis/doc/v-comment.html
// https://wiki.xiph.org/OggOpus#Content_Type
// https://wiki.xiph.org/OggPCM#Comment_packet
#define COMMENT_SIZE    1024

static int
_aaxReadOpusComment(_driver_t *handle)
{
   unsigned char *ch = (unsigned char*)handle->opusBuffer->data;
   size_t len = handle->opusBuffer->avail;
   uint32_t *header = (uint32_t*)ch;
   char field[COMMENT_SIZE+1];
   unsigned char *ptr;
   size_t i, size;
   int rv = 0;

   if (len > 12 && !strncasecmp((char*)ch, "OpusTags", 8))
   {
#if 0
      printf("\n--Opus Comment Header:\n");
      printf("  0: %08x %08x (\"%c%c%c%c%c%c%c%c\")\n", header[0], header[1], ch[0], ch[1], ch[2], ch[3], ch[4], ch[5], ch[6], ch[7]);

      size = header[2];
      snprintf(field, _MIN(size+1, COMMENT_SIZE), "%s", ch+12);
      printf("  2: %08x Vendor: '%s'\n", header[2], field);

      i = 12+size;
      ptr = ch+i;
      size = *(uint32_t*)ptr;
// printf("User comment list length: %i\n", size);

      ptr += 4;
      for (i=0; i<size; i++)
      {
         size_t slen = *(uint32_t*)ptr;
         ptr += 4;
         snprintf(field, _MIN(slen+1, COMMENT_SIZE), "%s", ptr);
         printf("\t'%s'\n", field);
         ptr += slen;
      }
#endif

      field[COMMENT_SIZE] = 0;

      size = header[2];
      snprintf(field, _MIN(size+1, COMMENT_SIZE), "%s", ch+12);
//    handle->vendor = strdup(field);

      i = 12+size;
      ptr = ch+i;
      size = *(uint32_t*)ptr;

      ptr += 4;
      for (i=0; i<size; i++)
      {
         uint32_t slen = *(uint32_t*)ptr;

         ptr += sizeof(uint32_t);
         if ((size_t)(ptr+slen-ch) > len) {
             return __F_NEED_MORE;
         }

         snprintf(field, _MIN(slen+1, COMMENT_SIZE), "%s", ptr);
         ptr += slen;

         if (!STRCMP(field, "TITLE"))
         {
             handle->title = stradd(handle->title, field+strlen("TITLE="));
             handle->title_changed = AAX_TRUE;
         }
         else if (!STRCMP(field, "ARTIST")) {
             handle->artist = stradd(handle->artist, field+strlen("ARTIST="));
             handle->artist_changed = AAX_TRUE;
         }
//       else if (!STRCMP(field, "PERFORMER"))
//       {
//           handle->artist = stradd(handle->artist, field+strlen("PERFORMER="));
//           handle->artist_changed = AAX_TRUE;
   //       }
         else if (!STRCMP(field, "ALBUM")) {
             handle->album = stradd(handle->album, field+strlen("ALBUM="));
         } else if (!STRCMP(field, "TRACKNUMBER")) {
             handle->trackno = stradd(handle->trackno, field+strlen("TRACKNUMBER="));
         } else if (!STRCMP(field, "COPYRIGHT")) {
             handle->copyright = stradd(handle->copyright, field+strlen("COPYRIGHT="));
         } else if (!STRCMP(field, "GENRE")) {
             handle->genre = stradd(handle->genre, field+strlen("GENRE="));
         } else if (!STRCMP(field, "DATE")) {
             handle->date = stradd(handle->date, field+strlen("DATE="));
         } else if (!STRCMP(field, "CONTACT")) {
             handle->website = stradd(handle->website, field+strlen("CONTACT="));
         } else if (!STRCMP(field, "DESCRIPTION")) {
             handle->comments = stradd(handle->comments, field+strlen("DESCRIPTION="));
         }
         else if (!STRCMP(field, "R128_TRACK_GAIN"))
         {
             int gain = atoi(field+strlen("R128_TRACK_GAIN="));
             handle->gain = pow(10, (float)gain/(20.0f*256.0f));
         }
      }
      rv = ptr-ch;
   }

   return rv;
}
#endif
