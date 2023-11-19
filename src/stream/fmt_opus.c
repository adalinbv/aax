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

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif

#include <xml.h>

#include <base/databuffer.h>
#include <base/dlsym.h>

#include <api.h>
#include <arch.h>

#include "audio.h"
#include "fmt_opus.h"

#define FRAME_SIZE		960
#define MAX_FRAME_SIZE		(6*FRAME_SIZE)
#define MAX_PACKET_SIZE		(3*1276)
#define OPUS_BUFFER_SIZE	(2*MAX_PACKET_SIZE)
// #define OPUS_BUFFER_SIZE	16384

#define MAX_PCMBUFSIZE	(MAX_FRAME_SIZE*_AAX_MAX_SPEAKERS*sizeof(int16_t))

DECL_FUNCTION(opus_decoder_create);
DECL_FUNCTION(opus_decoder_destroy);
DECL_FUNCTION(opus_decoder_ctl);
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

   struct _meta_t meta;

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
   size_t packetSize;

   int channel_mapping;

   /* The rest is only used if channel_mapping != 0 */
   int nb_streams;
   int nb_coupled;
   unsigned char stream_map[255];


} _driver_t;

static void *audio = NULL;

int
_opus_detect(UNUSED(_fmt_t *fmt), int mode)
{
   int rv = false;
return rv;

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
            TIE_FUNCTION(opus_decode);
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

         rv = true;
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
         handle->format = AAX_PCM16S;
         handle->bits_sample = aaxGetBitsPerSample(handle->format);
         handle->capturing = (mode == 0) ? 1 : 0;
         handle->blocksize = FRAME_SIZE;
      }
      else {
         _AAX_FILEDRVLOG("OPUS: Insufficient memory");
      }
   }

   if (!handle->opusBuffer) {
      handle->opusBuffer = _aaxDataCreate(1, OPUS_BUFFER_SIZE, 1);
   }

   if (!handle->pcmBuffer) {
      handle->pcmBuffer = _aaxDataCreate(1, MAX_PCMBUFSIZE, 1);
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
                  if (!handle->id)
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

      _aax_free_meta(&handle->meta);
      free(handle);
   }
}

int
_opus_setup(UNUSED(_fmt_t *fmt), UNUSED(_fmt_type_t pcm_fmt), UNUSED(enum aaxFormat aax_fmt))
{
   return true;
}

size_t
_opus_fill(_fmt_t *fmt, void_ptr sptr, ssize_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t rv = __F_PROCESS;

   if (_aaxDataAdd(handle->opusBuffer, 0, sptr, *bytes) == 0) {
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
   int16_t *pcmBuffer;
   int n;

   req = *num;
   tracks = handle->no_tracks;
   bits = handle->bits_sample;
   framesize = tracks*bits/8;
   packet_sz = FRAME_SIZE;
   *num = 0;

   pcmBuffer = (int16_t*)_aaxDataGetData(handle->pcmBuffer, 0);
   do
   {
      size_t avail = _aaxDataGetDataAvail(handle->pcmBuffer, 0);
      if (avail > 0)
      {
         unsigned int max = _MIN(req, avail/framesize);
         if (max)
         {
            _batch_cvt24_ps(dptr+dptr_offs, pcmBuffer, max*tracks);
            _aaxDataMove(handle->pcmBuffer, 0, NULL, max*framesize);

            dptr_offs += max;
            handle->no_samples += max;
            req -= max;
            *num = max;
         }
      }

      if (req > 0)
      {
         size_t bufsize  = _MIN(packet_sz, _aaxDataGetDataAvail(handle->opusBuffer, 0));
         if (bufsize == packet_sz)
         {
            size_t pcmsmp = _aaxDataGetSize(handle->pcmBuffer)/framesize;
            unsigned char *buf = _aaxDataGetData(handle->opusBuffer, 0);

            n = popus_decode(handle->id, buf, bufsize, pcmBuffer, pcmsmp,
                                   0);
            if (n <= 0) break;

            _aaxDataSetOffset(handle->pcmBuffer, 0, n*framesize);
            rv += _aaxDataMove(handle->opusBuffer, 0, NULL, bufsize);
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
   unsigned int req, bps, tracks;
   uint8_t *opusBuf;
   int16_t *pcmBuf;
   size_t pcmBufAvail;
   size_t rv = 0;
   int ret;

   req = *num;
   tracks = handle->no_tracks;
   bps = tracks*sizeof(int16_t);

   // there is still data left in the buffer from the previous run
   *num = 0;
   while (req && (pcmBufAvail = _aaxDataGetDataAvail(handle->pcmBuffer, 0)) > 0)
   {
      unsigned int max = _MIN(req, pcmBufAvail/bps);

      pcmBuf = _aaxDataGetData(handle->pcmBuffer, 0);
      _batch_cvt24_16_intl(dptr, pcmBuf, dptr_offs, tracks, max);

      _aaxDataMove(handle->pcmBuffer, 0, NULL, max*bps);
      handle->no_samples += max;
      dptr_offs += max;
      req -= max;
      *num += max;
   }

   if (req > 0)
   {
      int32_t packetSize = handle->blocksize;
      if (packetSize)
      {
         int32_t pcmBufFree;
         size_t frameSpace;

         pcmBuf = _aaxDataGetPtr(handle->pcmBuffer, 0);
         pcmBufFree = _aaxDataGetFreeSpace(handle->pcmBuffer, 0);
         frameSpace = pcmBufFree/bps;

         opusBuf = _aaxDataGetData(handle->opusBuffer, 0);

         if (handle->recover) {
            popus_decoder_ctl(handle->id,
                              OPUS_GET_LAST_PACKET_DURATION(&packetSize));
         }


         // store the next chunk into the pcmBuffer
         ret = popus_decode(handle->id,
                            handle->recover ? NULL : opusBuf, packetSize,
                            pcmBuf, frameSpace, 0);
         if (ret > 0)
         {
            rv += _aaxDataMove(handle->opusBuffer, 0, NULL, packetSize);

            handle->recover = false;
            _aaxDataIncreaseOffset(handle->pcmBuffer, 0, ret*bps);

            // copy the remaining samples
            {
               pcmBufAvail = _aaxDataGetDataAvail(handle->pcmBuffer, 0);
               if (pcmBufAvail)
               {
                  unsigned int max = _MIN(req, pcmBufAvail/bps);

                  pcmBuf = _aaxDataGetData(handle->pcmBuffer, 0);
                  _batch_cvt24_16_intl(dptr, pcmBuf, dptr_offs, tracks, max);

                  _aaxDataMove(handle->pcmBuffer, 0, NULL, max*bps);
                  handle->no_samples += max;
                  dptr_offs += max;
                  req -= max;
                  *num += max;
               }
            }

            if (req > 0) {
               rv = __F_NEED_MORE;
            }
         }
         else if (ret < 0)
         {
            handle->recover = true;
            _AAX_FILEDRVLOG(popus_strerror(ret));
            rv = __F_NEED_MORE;
         }
      }
      else {
         rv = __F_NEED_MORE;
      }
   }

   return rv;
}

size_t
_opus_cvt_to_intl(_fmt_t *fmt, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = fmt->id;
   size_t res = 0;

   if (num)
   {
      size_t size = _aaxDataGetDataAvail(handle->opusBuffer, 0);
      void *ptr = _aaxDataGetData(handle->opusBuffer, 0);

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
   int rv = false;

   switch(param)
   {
   case __F_ARTIST:
      handle->meta.artist = (char*)desc;
      rv = true;
      break;
   case __F_TITLE:
      handle->meta.title = (char*)desc;
      rv = true;
      break;
   case __F_GENRE:
      handle->meta.genre = (char*)desc;
      rv = true;
      break;
   case __F_TRACKNO:
      handle->meta.trackno = (char*)desc;
      rv = true;
      break;
   case __F_ALBUM:
      handle->meta.album = (char*)desc;
      rv = true;
      break;
   case __F_DATE:
      handle->meta.date = (char*)desc;
      rv = true;
      break;
   case __F_COMPOSER:
      handle->meta.composer = (char*)desc;
      rv = true;
      break;
   case __F_COMMENT:
      handle->meta.comments = (char*)desc;
      rv = true;
      break;
   case __F_COPYRIGHT:
      handle->meta.copyright = (char*)desc;
      rv = true;
      break;
   case __F_ORIGINAL:
      handle->meta.original = (char*)desc;
      rv = true;
      break;
   case __F_WEBSITE:
      handle->meta.website = (char*)desc;
      rv = true;
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
      rv = handle->meta.artist;
      break;
   case __F_TITLE:
      rv = handle->meta.title;
      break;
   case __F_COMPOSER:
      rv = handle->meta.composer;
      break;
   case __F_GENRE:
      rv = handle->meta.genre;
      break;
   case __F_TRACKNO:
      rv = handle->meta.trackno;
      break;
   case __F_ALBUM:
      rv = handle->meta.album;
      break;
   case __F_DATE:
      rv = handle->meta.date;
      break;
   case __F_COMMENT:
      rv = handle->meta.comments;
      break;
   case __F_COPYRIGHT:
      rv = handle->meta.copyright;
      break;
   case __F_ORIGINAL:
      rv = handle->meta.original;
      break;
   case __F_WEBSITE:
      rv = handle->meta.website;
      break;
   case __F_IMAGE:
      rv = handle->meta.image;
      break;
   default:
      break;
   }
   return rv;
}

float
_opus_get(_fmt_t *fmt, int type)
{
   _driver_t *handle = fmt->id;
   float rv = 0.0f;

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

float
_opus_set(_fmt_t *fmt, int type, float value)
{
   _driver_t *handle = fmt->id;
   float rv = 0.0f;

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
