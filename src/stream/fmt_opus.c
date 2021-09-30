/*
 * Copyright 2005-2020 by Erik Hofman.
 * Copyright 2009-2020 by Adalin B.V.
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

#define FRAME_SIZE		960
#define OPUS_BUFFER_SIZE	16384
#define MAX_FRAME_SIZE		(6*FRAME_SIZE)
#define MAX_PACKET_SIZE		(2*3*1276)

#if 1
#define MAX_FLOATBUFSIZE	OPUS_BUFFER_SIZE
#else
#define MAX_OPUSBUFSIZE		(MAX_FRAME_SIZE*handle->no_tracks*sizeof(float))
#define MAX_FLOATBUFSIZE	(2*MAX_OPUSBUFSIZE)
#endif

DECL_FUNCTION(opus_decoder_create);
DECL_FUNCTION(opus_decoder_destroy);
DECL_FUNCTION(opus_decoder_ctl);
DECL_FUNCTION(opus_decode_float);

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
   float gain;

   _data_t *opusBuffer;
   _data_t *pcmBuffer;

   size_t preSkip;
   int channel_mapping;

   /* The rest is only used if channel_mapping != 0 */
   int nb_streams;
   int nb_coupled;
   unsigned char stream_map[255];


} _driver_t;

static void *audio = NULL;

static int _aaxReadOpusHeader(_driver_t*);

int
_opus_detect(UNUSED(_fmt_t *fmt), int mode)
{
   int rv = AAX_FALSE;

   audio = _aaxIsLibraryPresent("opus", "0");
   if (!audio) {
      audio = _aaxIsLibraryPresent("libopus", "0");
   }

   if (audio)
   {
      char *error;

      _aaxGetSymError(0);

      if (!mode) // capture
      {
         TIE_FUNCTION(opus_decoder_create);
         if (popus_decoder_create)
         {
            TIE_FUNCTION(opus_decoder_destroy);
            TIE_FUNCTION(opus_decoder_ctl);
            TIE_FUNCTION(opus_decode_float);
         }
      }
      else // playback
      {
         TIE_FUNCTION(opus_encoder_create);
         if (popus_encoder_create)
         {
            TIE_FUNCTION(opus_encoder_destroy);
            TIE_FUNCTION(opus_encoder_ctl);
            TIE_FUNCTION(opus_encode);
         }
      }

      error = _aaxGetSymError(0);
      if (!error)
      {
         /* not required but useful */
         TIE_FUNCTION(opus_strerror);
         TIE_FUNCTION(opus_get_version_string);

         rv = AAX_TRUE;
      }
   }

   return rv;
}

void*
_opus_open(_fmt_t *fmt, int mode, void *buf, ssize_t *bufsize, UNUSED(size_t fsize))
{
   _driver_t *handle = fmt->id;
   void *rv = NULL;

   if (bufsize && *bufsize == 0 && handle && handle->id)
   {
      *bufsize = -1;
      return rv;
   }

   if (!handle)
   {
      handle = fmt->id = calloc(1, sizeof(_driver_t));
      if (fmt->id)
      {
         handle->mode = mode;
         handle->frequency = 48000;
         handle->format = AAX_PCM24S;
         handle->bits_sample = aaxGetBitsPerSample(handle->format);
         handle->capturing = (mode == 0) ? 1 : 0;
         handle->blocksize = FRAME_SIZE;
      }
      else {
         _AAX_FILEDRVLOG("OPUS: Insufficient memory");
      }
   }

   if (!handle->opusBuffer) {
      handle->opusBuffer = _aaxDataCreate(OPUS_BUFFER_SIZE, 1);
   }

   if (!handle->pcmBuffer) {
      handle->pcmBuffer = _aaxDataCreate(MAX_FLOATBUFSIZE, 1);
   }

   if (handle && handle->opusBuffer && handle->pcmBuffer)
   {
      if (handle->capturing)
      {
         if (handle && buf && bufsize)
         {
            int res, err = OPUS_OK;

            res = _opus_fill(fmt, buf, bufsize);
            if (res <= 0)
            {
//             *bufsize = res;
               if (!handle->id)
               {
                  int tracks = handle->no_tracks;
                  int32_t freq = handle->frequency;

                  handle->id = popus_decoder_create(freq, tracks, &err);
                  if (handle->id)
                  {
                     res = _aaxReadOpusHeader(handle);
                     if (res)
                     {
                        res = _aaxDataMove(handle->opusBuffer, NULL, res);
                        // Note: _getOggOpusComment is handled in ext_ogg
                     }
                  }
                  else
                  {
                     *bufsize = 0;
                     switch (err)
                     {
                     case OPUS_BAD_ARG:
                        _AAX_FILEDRVLOG("OPUS: argument invalid or out of range");
                        break;
                     case OPUS_BUFFER_TOO_SMALL:
                        _AAX_FILEDRVLOG("OPUS: invalid mode struct");
                        break;
                     case OPUS_INTERNAL_ERROR:
                        _AAX_FILEDRVLOG("OPUS: internal error");
                        break;
                     case OPUS_INVALID_PACKET:
                        _AAX_FILEDRVLOG("OPUS: corrupted compressed data");
                        break;
                     case OPUS_UNIMPLEMENTED:
                        _AAX_FILEDRVLOG("OPUS: unimplemented request");
                        break;
                     case OPUS_INVALID_STATE:
                        _AAX_FILEDRVLOG("OPUS: id is invalid or already freed");
                        break;
                     case OPUS_ALLOC_FAIL:
                        _AAX_FILEDRVLOG("OPUS: insufficient memory");
                        break;
                     default:
                        _AAX_FILEDRVLOG(popus_strerror(err));
                        break;
                     }
                  }
               } // !handle->id
            } // _buf_fill() != 0
         } // handle && buf && bufsize
      } // handle->capturing
   }
   else if (handle && (!handle->opusBuffer || !handle->pcmBuffer))
   {
      _AAX_FILEDRVLOG("OPUS: Unable to allocate the audio buffer");
      rv = buf; // try again
   }
   else {
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
_opus_fill(_fmt_t *fmt, void_ptr sptr, ssize_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t rv = __F_PROCESS;

   if (_aaxDataAdd(handle->opusBuffer, sptr, *bytes) == 0) {
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

   floats = (float*)_aaxDataGetData(handle->pcmBuffer);
   do
   {
      size_t avail = _aaxDataGetDataAvail(handle->pcmBuffer);
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
         size_t bufsize  = _MIN(packet_sz, _aaxDataGetDataAvail(handle->opusBuffer));
         if (bufsize == packet_sz)
         {
            size_t floatsmp = _aaxDataGetSize(handle->pcmBuffer)/framesize;
            unsigned char *buf = _aaxDataGetData(handle->opusBuffer);

            n = popus_decode_float(handle->id, buf, bufsize, floats, floatsmp,
                                   0);
            if (n <= 0) break;

            _aaxDataSetOffset(handle->pcmBuffer, n*framesize);
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
   unsigned char *pcmBuf;
   size_t pcmBufavail;
   size_t rv = 0;
   int ret;

   req = *num;
   tracks = handle->no_tracks;
   *num = 0;

   pcmBuf = _aaxDataGetData(handle->pcmBuffer);
   pcmBufavail = _aaxDataGetDataAvail(handle->pcmBuffer);

   /* there is still data left in the buffer from the previous run */
   if (pcmBufavail)
   {
      unsigned int max = _MIN(req, pcmBufavail/sizeof(float));

      _batch_cvt24_ps_intl(dptr, pcmBuf, dptr_offs, tracks, max);
      _aaxDataMove(handle->pcmBuffer, NULL, max*sizeof(float));
      handle->no_samples += max;
      dptr_offs += max;
      req -= max;
      *num = max;
   }

   if (req > 0)
   {
      unsigned char *opusBuf;
      int32_t opusBufAvail;
      int32_t pcmBufFree;
      int32_t pcmBufOffs;
      size_t packetSize;
      int frameSpace;

      packetSize = handle->blocksize;
      opusBufAvail = _aaxDataGetDataAvail(handle->opusBuffer);
      if (opusBufAvail < packetSize) {
         return __F_NEED_MORE;
      }

      opusBuf = _aaxDataGetData(handle->opusBuffer);
      pcmBufFree = _aaxDataGetFreeSpace(handle->pcmBuffer);
      pcmBufOffs = _aaxDataGetOffset(handle->pcmBuffer);
      frameSpace = pcmBufFree/(tracks*sizeof(float));

      // store the next chunk into the pcmBuffer
      ret = popus_decode_float(handle->id, opusBuf, packetSize,
                               (float*)(pcmBuf+pcmBufOffs), frameSpace, 0);
      if (ret >= 0)
      {
         if (handle->preSkip > (size_t)ret)
         {
            handle->preSkip -= ret;
//          rv = __F_NEED_MORE;
         }
         else
         {
            ret -= handle->preSkip;
            handle->preSkip = 0;
         }

         if (!handle->preSkip)
         {
            size_t pcmBufAvail;
            unsigned int max;

            rv += _aaxDataMove(handle->opusBuffer, NULL, packetSize);

            _aaxDataIncreaseOffset(handle->pcmBuffer, ret*sizeof(float));

            pcmBufAvail = _aaxDataGetDataAvail(handle->pcmBuffer);
            assert(pcmBufAvail <= _aaxDataGetSize(handle->pcmBuffer));

            max = _MIN(req, pcmBufAvail/sizeof(float));
            _batch_cvt24_ps_intl(dptr, pcmBuf, dptr_offs, tracks, max);

            _aaxDataMove(handle->pcmBuffer, NULL, max*sizeof(float));
            handle->no_samples += max;
            dptr_offs += max;
            req -= max;
            *num += max;
         }
      }

      if (ret <= 0)
      {
         _AAX_FILEDRVLOG(popus_strerror(ret));
         rv = __F_NEED_MORE;
      }
   }

// printf("  opus_cvt_from: %li\n", rv);
   return rv;
}

size_t
_opus_cvt_to_intl(_fmt_t *fmt, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = fmt->id;
   size_t res = 0;

   if (num)
   {
      size_t size = _aaxDataGetDataAvail(handle->opusBuffer);
      void *ptr = _aaxDataGetData(handle->opusBuffer);

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
      res = popus_encode(handle->id, scratch, *num, ptr, size);
      _aax_memcpy(dptr, handle->opusBuffer, res);
   }

   return res;
}

int
_opus_set_name(_fmt_t *fmt, enum _aaxStreamParam param, const char *desc)
{
   _driver_t *handle = fmt->id;
   int rv = AAX_FALSE;

   switch(param)
   {
   case __F_ARTIST:
      handle->artist = (char*)desc;
      rv = AAX_TRUE;
      break;
   case __F_TITLE:
      handle->title = (char*)desc;
      rv = AAX_TRUE;
      break;
   case __F_GENRE:
      handle->genre = (char*)desc;
      rv = AAX_TRUE;
      break;
   case __F_TRACKNO:
      handle->trackno = (char*)desc;
      rv = AAX_TRUE;
      break;
   case __F_ALBUM:
      handle->album = (char*)desc;
      rv = AAX_TRUE;
      break;
   case __F_DATE:
      handle->date = (char*)desc;
      rv = AAX_TRUE;
      break;
   case __F_COMPOSER:
      handle->composer = (char*)desc;
      rv = AAX_TRUE;
      break;
   case __F_COMMENT:
      handle->comments = (char*)desc;
      rv = AAX_TRUE;
      break;
   case __F_COPYRIGHT:
      handle->copyright = (char*)desc;
      rv = AAX_TRUE;
      break;
   case __F_ORIGINAL:
      handle->original = (char*)desc;
      rv = AAX_TRUE;
      break;
   case __F_WEBSITE:
      handle->website = (char*)desc;
      rv = AAX_TRUE;
      break;
   default:
      break;
   }
   return rv;
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
   case __F_BITRATE:
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

/* -------------------------------------------------------------------------- */// https://tools.ietf.org/html/rfc7845.html#page-12
#define OPUS_ID_HEADER_SIZE	(4*5-1)

static int
_aaxReadOpusHeader(_driver_t *handle)
{
   char *h = (char*)_aaxDataGetData(handle->opusBuffer);
   size_t len = _aaxDataGetDataAvail(handle->opusBuffer);
   int32_t *x = (int32_t*)h;
   int rv = __F_EOF;

   //                                       'Opus'                'Head'
   if (len >= OPUS_ID_HEADER_SIZE && x[0] == 0x7375704f && x[1] == 0x64616548)
   {
      int version = h[8];
      if (version == 1)
      {
         unsigned char mapping_family;
         int gain;

         handle->format = AAX_FLOAT;
         handle->no_tracks = (unsigned char)h[9];
         handle->nb_streams = 1;
         handle->nb_coupled = handle->no_tracks/2;
         handle->frequency = *((uint32_t*)h+3);
         handle->preSkip = (unsigned)h[10] << 8 | h[11];
//       handle->no_samples = -handle->preSkip;

         gain = (int)h[16] << 8 | h[17];
         handle->gain = pow(10, (float)gain/(20.0f*256.0f));

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
#if 1
  printf("\n-- Opus Identification Header:\n");
  printf("  0: %08x %08x ('%c%c%c%c%c%c%c%c')\n", x[0], x[1], h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7]);
  printf("  2: %08x (Version: %i, Tracks: %i, Pre Skip: %i)\n", x[2], h[8], (unsigned char)h[9], (unsigned)h[10] << 8 | h[11]);
  printf("  3: %08x (Original Sample Rate: %i)\n", x[3],  x[3]);
  if (mapping_family == 1) {
    printf("  4: %08x (Replay gain: %f, Mapping Family: %i)\n", x[4], handle->gain, h[18]);
    printf("  5: %08x (Stream Count: %i, Coupled Count: %i)\n", x[5], handle->nb_streams, handle->nb_coupled);
  } else {
    uint32_t i = (int)h[16] << 16 | h[17] << 8 || h[18];
    printf("  4: %08x (Replay gain: %f, Mapping Family: %i)\n", i, handle->gain, h[18]);
  }
  printf(" rv: %i\n", rv);
#endif
      }
   }

   return rv;
}
