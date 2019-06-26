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
#include "fmt_flac.h"

#define FRAME_SIZE	960
#define MAX_FRAME_SIZE	(6*960)
#define MAX_PACKET_SIZE	(3*1276)
#define MAX_FLACBUFSIZE	(2*DR_FLAC_BUFFER_SIZE)
#define MAX_PCMBUFSIZE	4096

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

   int mode;

   char capturing;

   uint8_t no_tracks;
   uint8_t bits_sample;
   int frequency;
   int bitrate;
   int blocksize;
   enum aaxFormat format;
   size_t no_samples;
   size_t max_samples;

   _data_t *pcmBuffer;
   _data_t *flacBuffer;
   size_t rv;

} _driver_t;

static size_t _flac_callback_read(void*, void*, size_t);
static drflac_bool32 _flac_callback_seek(void*, int, drflac_seek_origin);


int
_flac_detect(UNUSED(_fmt_t *fmt), UNUSED(int mode))
{
   int rv = AAX_FALSE;

#if 0
   if (mode == 0) {
      rv = AAX_TRUE;
   }
#endif

   return rv;
}

void*
_flac_open(_fmt_t *fmt, int mode, void *buf, size_t *bufsize, UNUSED(size_t fsize))
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
//       handle->blocksize = 4096;
         handle->blocksize = sizeof(int32_t);
         handle->format = AAX_PCM32S;
         handle->bits_sample = aaxGetBitsPerSample(handle->format);
      }
      else {
         _AAX_FILEDRVLOG("FLAC: Insufficient memory");
      }
   }

   if (handle && buf && bufsize)
   {
      if (!handle->id)
      {
         if (!handle->flacBuffer) {
            handle->flacBuffer = _aaxDataCreate(MAX_FLACBUFSIZE, 1);
         }

         if (!handle->pcmBuffer) {
            handle->pcmBuffer = _aaxDataCreate(MAX_PCMBUFSIZE, 1);
         }

         if (handle->flacBuffer && handle->pcmBuffer)
         {
            size_t size = *bufsize;
            int res;

            if (size > MAX_FLACBUFSIZE) {
               size = MAX_FLACBUFSIZE;
            }

            res = _aaxDataAdd(handle->flacBuffer, buf, size);
            *bufsize -= res;

            if (!handle->id)
            {
               drflac_container type = drflac_container_native;
                // or drflac_container_ogg
               handle->id = drflac_open_relaxed(_flac_callback_read,
                                                _flac_callback_seek,
                                                 type, handle);
            }

            handle->rv = 0;
            if (!handle->id) {
               rv = buf;
            }
         }
         else
         {
            _AAX_FILEDRVLOG("FLAC: Unable to allocate the audio buffer");
             rv = buf;   // try again
         }
      }
   }
   else {
      _AAX_FILEDRVLOG("FLAC: Internal error: handle id equals 0");
   }

   return rv;
}

void
_flac_close(_fmt_t *fmt)
{
   _driver_t *handle = fmt->id;

   if (handle)
   {
      drflac_close(handle->id);
      handle->id = NULL;

      _aaxDataDestroy(handle->pcmBuffer);
      _aaxDataDestroy(handle->flacBuffer);

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
_flac_setup(UNUSED(_fmt_t *fmt), UNUSED(_fmt_type_t pcm_fmt), UNUSED(enum aaxFormat aax_fmt))
{
   return AAX_TRUE;
}

size_t
_flac_fill(_fmt_t *fmt, void_ptr sptr, size_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t rv = __F_PROCESS;
   size_t res;

   res = _aaxDataAdd(handle->flacBuffer, sptr, *bytes);
   if (res == 0) {
      *bytes = 0;
   }

   return rv;
}

size_t
_flac_copy(_fmt_t *fmt, int32_ptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bufsize, rv = __F_NEED_MORE;

   bufsize = handle->flacBuffer->avail;
   if (*num && bufsize)
   {
      size_t bytes = *num * handle->blocksize;
      if (bytes <= bufsize)
      {
         size_t offs = (dptr_offs/bytes)*bytes;
         rv = _aaxDataMove(handle->flacBuffer, (char*)dptr+offs, bytes);
         handle->no_samples += *num;
      }
      else {
         *num = 0;
      }
   }
   else {
      *num = 0;
   }

   return rv;
}

size_t
_flac_cvt_from_intl(_fmt_t *fmt, int32_ptrptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bufsize, rv = __F_NEED_MORE;
   unsigned char *buf;
   unsigned int req, ret;
   int tracks;

   req = *num;
   tracks = handle->no_tracks;
   *num = 0;

   buf = handle->pcmBuffer->data;
   bufsize = handle->pcmBuffer->avail;

   /* there is still data left in the buffer from the previous run */
   if (bufsize)
   {
      unsigned int max = _MIN(req, bufsize/sizeof(int32_t));

      _batch_cvt24_32_intl(dptr, buf, dptr_offs, tracks, max);
      _aaxDataMove(handle->pcmBuffer, NULL, max*sizeof(int32_t));
      handle->no_samples += max;
      dptr_offs += max;
      req -= max;
      *num = max;
   }

   if (req > 0 && handle->flacBuffer->avail > 0)
   {
      size_t frame_size = tracks*sizeof(int32_t);
      size_t avail = handle->pcmBuffer->avail;
      size_t bufsize = handle->pcmBuffer->size - avail;
      size_t no_frames = bufsize/frame_size;
      int32_t *pcmbuf = (int32_t*)(buf+avail);

      // store the next chunk into the pcmBuffer;
      ret = drflac_read_pcm_frames_s32(handle->id, no_frames, pcmbuf);
      handle->pcmBuffer->avail += ret*frame_size;

      assert(handle->pcmBuffer->avail <= handle->pcmBuffer->size);

      if (handle->rv)
      {
         rv = handle->rv;
         handle->rv = 0;
      }

      if (handle->pcmBuffer->avail)
      {
         unsigned int max = _MIN(ret, handle->pcmBuffer->avail/sizeof(int32_t));

         _batch_cvt24_32_intl(dptr, buf, dptr_offs, tracks, max);
         _aaxDataMove(handle->pcmBuffer, NULL, max*sizeof(int32_t));
         handle->no_samples += max;
         req -= max;
         *num += max;
      }
   }

   return rv;
}

size_t
_flac_cvt_to_intl(UNUSED(_fmt_t *fmt), UNUSED(void_ptr dptr), UNUSED(const_int32_ptrptr sptr), UNUSED(size_t offs), UNUSED(size_t *num), UNUSED(void_ptr scratch), UNUSED(size_t scratchlen))
{
   int res = 0;
   return res;
}

char*
_flac_name(UNUSED(_fmt_t *fmt), UNUSED(enum _aaxStreamParam param))
{
   return NULL;
}

off_t
_flac_get(_fmt_t *fmt, int type)
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
_flac_set(_fmt_t *fmt, int type, off_t value)
{
   _driver_t *handle = fmt->id;
   off_t rv = 0;

   switch(type)
   {
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
static size_t
_flac_callback_read(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
   _driver_t *handle = (_driver_t *)pUserData;
   int tries = 10240;
   size_t rv;

   do
   {
      rv =  _aaxDataMove(handle->flacBuffer, pBufferOut, bytesToRead);
      handle->rv += rv;
      if (rv > 0) break;
      msecSleep(1);
   }
   while (--tries);

   return rv;
}

static drflac_bool32
_flac_callback_seek(UNUSED(void* pUserData), UNUSED(int offset), UNUSED(drflac_seek_origin origin))
{
// _driver_t *handle = (_driver_t *)pUserData;
   return 0;
}
