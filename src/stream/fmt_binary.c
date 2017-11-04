/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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


typedef struct
{
   void *id;

   char capturing;
   int mode;

   uint8_t no_tracks;
   uint8_t bits_sample;
   int frequency;
   int bitrate;
   int blocksize;
   enum aaxFormat format;
   size_t no_samples;
   size_t max_samples;

   _data_t *rawBuffer;

// float **outputs;
// unsigned int out_pos;
// unsigned int out_size;

} _driver_t;


int
_binary_detect(UNUSED(_fmt_t *fmt), int mode)
{
   int rv = AAX_FALSE;

   if (!mode) {
      rv = AAX_TRUE;
   }

   return rv;
}

void*
_binary_open(_fmt_t *fmt, int mode, void *buf, size_t *bufsize, UNUSED(size_t fsize))
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

         handle->no_tracks = 1;
         handle->bits_sample = 8;
         handle->frequency = 44100;
         handle->bitrate = 0;
         handle->blocksize = 1;
         handle->format = AAX_PCM16S;
         handle->no_samples = 0;
         handle->max_samples = 0;
      }
      else {
         _AAX_FILEDRVLOG("RAW format: Insufficient memory");
      }
   }

   if (handle && buf && bufsize)
   {
      if (!handle->rawBuffer) {
         handle->rawBuffer = _aaxDataCreate(16384, 1);
      }

      if (handle->rawBuffer)
      {
         if (handle->capturing) {
            if (_binary_fill(fmt, buf, bufsize) <= 0) {
               rv = buf;
            }
         }
      }
      else {
         _AAX_FILEDRVLOG("RAW format: Unable to allocate the audio buffer");
      }
   }
   else
   {
      _AAX_FILEDRVLOG("RAW format: Internal error: handle id equals 0");
   }

   return rv;
}

void
_binary_close(_fmt_t *fmt)
{
   _driver_t *handle = fmt->id;

   if (handle)
   {
      handle->id = NULL;
      _aaxDataDestroy(handle->rawBuffer);
      free(handle);
   }
}

int
_binary_setup(_fmt_t *fmt, _fmt_type_t pcm_fmt, UNUSED(enum aaxFormat aax_fmt))
{
   _driver_t *handle = fmt->id;
   switch(pcm_fmt)
   {
   case _FMT_AAXS:
      handle->format = AAX_AAXS24S;
      handle->max_samples = handle->frequency;
      break;
   default:
      break;
   }
   return AAX_TRUE;
}

size_t
_binary_fill(_fmt_t *fmt, void_ptr sptr, size_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t rv = __F_PROCESS;

   if (_aaxDataAdd(handle->rawBuffer, sptr, *bytes) == 0) {
      *bytes = 0;
   }

   return rv;
}

size_t
_binary_copy(_fmt_t *fmt, int32_ptr dptr, size_t offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t rv = __F_NEED_MORE;
   if (handle->rawBuffer->avail) {
      rv = _aaxDataMove(handle->rawBuffer, (char*)dptr+offs, *num); 
      *num = rv;
   }
   else {
      *num = 0;
   }
   return rv;
}

size_t
_binary_cvt_from_intl(UNUSED(_fmt_t *fmt), UNUSED(int32_ptrptr dptr), UNUSED(size_t dptr_offs), UNUSED(size_t *num))
{
   size_t rv = __F_NEED_MORE;
   return rv;
}

size_t
_binary_cvt_to_intl(UNUSED(_fmt_t *fmt), UNUSED(void_ptr dptr), UNUSED(const_int32_ptrptr sptr), UNUSED(size_t offs), UNUSED(size_t *num), UNUSED(void_ptr scratch), UNUSED(size_t scratchlen))
{
   int rv = __F_EOF;
   return rv;
}

char*
_binary_name(UNUSED(_fmt_t *fmt), UNUSED(enum _aaxStreamParam param))
{
   return NULL;
}

off_t
_binary_get(_fmt_t *fmt, int type)
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
_binary_set(_fmt_t *fmt, int type, off_t value)
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
//    handle->no_tracks = rv = value;
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

