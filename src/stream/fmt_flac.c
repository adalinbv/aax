/*
 * Copyright 2005-2023 by Erik Hofman.
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
#include "fmt_flac.h"

typedef struct
{
   void *id;

   struct _meta_t meta;

   int mode;
   char capturing;

   uint8_t no_tracks;
   uint8_t bits_sample;
   int frequency;
   int bitrate;
   int blocksize;
   int blocksmp;
   enum aaxFormat format;
   size_t no_samples;
   size_t max_samples;

   _data_t *flacBuffer;
   size_t rv;

   int32_t output[MAX_PCMBUFSIZE];
   size_t out_size;
   size_t out_pos;

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
_flac_open(_fmt_t *fmt, int mode, void *buf, ssize_t *bufsize, size_t fsize)
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
         handle->blocksmp = 1;
         handle->blocksize = sizeof(int32_t);
         handle->out_size = MAX_PCMBUFSIZE;
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
            handle->flacBuffer = _aaxDataCreate(1, MAX_FLACBUFSIZE, 1);
         }

         if (handle->flacBuffer)
         {
            if (_flac_fill(fmt, buf, bufsize) > 0)
            {
               if (!handle->id)
               {			// or drflac_container_ogg
                  drflac_container type = drflac_container_native;

                  if (fsize > 0) {	// file
                     handle->id = drflac_open(_flac_callback_read,
                                              _flac_callback_seek,
                                              handle, NULL);
                  } else {		// internet stream
                     handle->id = drflac_open_relaxed(_flac_callback_read,
                                                      _flac_callback_seek,
                                                      type, handle, NULL);
                  }

                  if (handle->id)
                  {
                     drflac *ret = (drflac *)handle->id;

                     handle->frequency = ret->sampleRate;
                     handle->no_tracks = ret->channels;
                     handle->format = AAX_PCM32S;
                     handle->bits_sample = aaxGetBitsPerSample(handle->format);
                     handle->blocksize = handle->no_tracks*handle->bits_sample/8;
#if 0
 printf("freq: %i, tracks: %i, bits/sample: %i\n", handle->frequency, handle->no_tracks, handle->bits_sample);
#endif
                  }
               }

               if (!handle->id) {
                  rv = buf;
               }
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
      if (handle->id) {
         drflac_close(handle->id);
      }
      handle->id = NULL;

      _aaxDataDestroy(handle->flacBuffer);

      _aax_free_meta(&handle->meta);
      free(handle);
   }
}

int
_flac_setup(UNUSED(_fmt_t *fmt), UNUSED(_fmt_type_t pcm_fmt), UNUSED(enum aaxFormat aax_fmt))
{
   return AAX_TRUE;
}

size_t
_flac_fill(_fmt_t *fmt, void_ptr sptr, ssize_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t rv = __F_PROCESS;

   if (_aaxDataAdd(handle->flacBuffer, 0, sptr, *bytes) == 0) {
      *bytes = 0;
   }

   return rv;
}

size_t
_flac_copy(_fmt_t *fmt, int32_ptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bufsize, rv = __F_NEED_MORE;

   bufsize = _aaxDataGetDataAvail(handle->flacBuffer, 0);
   if (bufsize)
   {
      unsigned int bufsize = _aaxDataGetDataAvail(handle->flacBuffer, 0);
      unsigned int blocksize = handle->blocksize;
      unsigned int blocksmp = handle->blocksmp;
      size_t offs, bytes, n;

      if ((*num + handle->no_samples) > handle->max_samples) {
         *num = handle->max_samples - handle->no_samples;
      }

      if (*num)
      {
         n = *num/blocksmp;
         bytes = n*blocksize;
         if (bytes > bufsize)
         {
            n = (bufsize/blocksize);
            bytes = n*blocksize;
         }
         *num = n*blocksmp;

         offs = (dptr_offs/blocksmp)*blocksize;
         rv = _aaxDataMove(handle->flacBuffer, 0, (char*)dptr+offs, bytes);
         handle->no_samples += *num;

         if (handle->no_samples >= handle->max_samples) {
            rv = __F_EOF;
         }
      }
#if 0
      size_t bytes = *num * handle->blocksize;
      if (bytes <= bufsize)
      {
         size_t offs = (dptr_offs/bytes)*bytes;
         rv = _aaxDataMove(handle->flacBuffer, 0, (char*)dptr+offs, bytes);
         handle->no_samples += *num;
      }
#endif
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
   size_t rv = __F_NEED_MORE;
   unsigned int req, n;
   int tracks;

   req = *num;
   tracks = handle->no_tracks;
   *num = 0;

   /* there is still data left in the buffer from the previous run */
   if (handle->out_pos > 0)
   {
      unsigned int pos = handle->out_pos;
      unsigned int max = _MIN(req, handle->out_size - pos);

      _batch_cvt24_32_intl(dptr, handle->output, dptr_offs, tracks, max);

      dptr_offs += max;
      handle->out_pos += max;
      handle->no_samples += max;
      if (handle->out_pos == handle->out_size) {
         handle->out_pos = 0;
      }
      req -= max;
      *num = max;
   }

   while (req > 0)
   {
      size_t no_frames = MAX_PCMBUFSIZE/handle->blocksize;

      n = drflac_read_pcm_frames_s32(handle->id, no_frames, handle->output);

      if (handle->rv)
      {
         rv = handle->rv;
         handle->rv = 0;
      }

      if (n > 0)
      {
         if (n > (int)req)
         {
            handle->out_size = n;
            handle->out_pos = req;
            n = req;
            req = 0;
         }
         else
         {
            assert(handle->out_pos == 0);
            req -= n;
         }

         *num += n;
         handle->no_samples += n;

         _batch_cvt24_32_intl(dptr, handle->output, dptr_offs, tracks, n);

         dptr_offs += n;
      }
      else {
         break;
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

int
_flac_set_name(_fmt_t *fmt, enum _aaxStreamParam param, const char *desc)
{
   return AAX_FALSE;
}

char*
_flac_name(UNUSED(_fmt_t *fmt), UNUSED(enum _aaxStreamParam param))
{
   return NULL;
}

float
_flac_get(_fmt_t *fmt, int type)
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
_flac_set(_fmt_t *fmt, int type, float value)
{
   _driver_t *handle = fmt->id;
   float rv = 0.0f;

   switch(type)
   {
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
  case __F_BLOCK_SIZE:
      handle->blocksize = rv = value;
      break;
   case __F_BLOCK_SAMPLES:
      handle->blocksmp = rv = value;
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
   size_t rv;

   rv =  _aaxDataMove(handle->flacBuffer, 0, pBufferOut, bytesToRead);
   if (rv > 0) handle->rv += rv;

   return rv;
}

static drflac_bool32
_flac_callback_seek(UNUSED(void* pUserData), UNUSED(int offset), UNUSED(drflac_seek_origin origin))
{
// _driver_t *handle = (_driver_t *)pUserData;
   return 0;
}
