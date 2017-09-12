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

#include "extension.h"
#include "format.h"
#include "fmt_flac.h"

#define FRAME_SIZE 960
#define MAX_FRAME_SIZE 6*960
#define MAX_PACKET_SIZE (3*1276)

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

   ssize_t flacBufPos;
   size_t flacBufSize;
   int32_t *flacBuffer;
   char *flacptr;

   drflac__memory_stream memoryStream;

} _driver_t;

static void _flac_metafn(void*, drflac_metadata*);


int
_flac_detect(_fmt_t *fmt, int mode)
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
         handle->blocksize = 4096;
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
         char *ptr = 0;

         handle->flacBufPos = 0;
         handle->flacBufSize = MAX_PACKET_SIZE;
         handle->flacptr = _aax_malloc(&ptr, handle->flacBufSize);
         handle->flacBuffer = (int32_t*)ptr;

printf("a\n");
         memcpy(handle->flacBuffer, buf, *bufsize);
         handle->flacBufPos += *bufsize;

         handle->memoryStream.data = (const unsigned char*)handle->flacBuffer;
         handle->memoryStream.dataSize = handle->flacBufPos;
         handle->memoryStream.currentReadPos = 0;
printf("A\n");
         handle->id = drflac_open_with_metadata_relaxed(drflac__on_read_memory,
                                                        drflac__on_seek_memory,
                                                        NULL, // flac_metafn
                                                        drflac_container_native,
                                                (void*)&handle->memoryStream);
printf("B\n");
         if (handle->id) {
            rv = buf;
         }
      }
   }
   else
   {
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

      free(handle->flacptr);

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
   size_t bufpos, bufsize;
   size_t rv = 0;

   bufpos = handle->flacBufPos;
   bufsize = handle->flacBufSize;
   if ((*bytes+bufpos) <= bufsize)
   {
      char *buf = (char *)handle->flacBuffer + bufpos;

      memcpy(buf, sptr, *bytes);
      handle->flacBufPos += *bytes;
      rv = *bytes;
   }

   return rv;
}

size_t
_flac_copy(_fmt_t *fmt, int32_ptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   unsigned int bits, tracks;
   size_t bytes, bufsize;
   size_t rv = __F_EOF;
   char *buf;

   tracks = handle->no_tracks;
   bits = handle->bits_sample;
   bytes = *num*tracks*bits/8;

   buf = (char*)handle->flacBuffer;
   bufsize = handle->flacBufSize;

   if (bytes > bufsize) {
      bytes = bufsize;
   }

   if (bytes)
   {
      memcpy(dptr+dptr_offs, buf+handle->flacBufPos, bytes);
      handle->flacBufPos += bytes;
      rv = bytes;
   }

   return rv;
}

size_t
_flac_cvt_from_intl(_fmt_t *fmt, int32_ptrptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t ret, rv = __F_NEED_MORE;
   unsigned int blocksize, tracks;
   size_t bytes, bufsize;
   int32_t *buf;

   tracks = handle->no_tracks;
   blocksize = handle->blocksize;
   bytes = *num*blocksize;

   buf = handle->flacBuffer;
   bufsize = handle->flacBufSize;

   if (bytes > bufsize) {
      bytes = bufsize;
   }

   // Returns the number of samples actually read.
printf("C\n");
   ret = drflac_read_s32(handle->id, *num, buf);
   if (ret > 0)
   {
      *num = ret/blocksize;
      _batch_cvt24_32_intl(dptr, buf, dptr_offs, tracks, *num);

      handle->no_samples += *num;
      rv = ret;
   }
printf("D\n");

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
#define COMMENT_SIZE	1024

static void
_flac_metafn(void *userData, drflac_metadata *metaData)
{
   _driver_t *handle = (_driver_t *)userData;

   switch(metaData->type)
   {
   case DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO:
      handle->blocksize = metaData->data.streaminfo.maxBlockSize;
      handle->frequency = metaData->data.streaminfo.sampleRate;
      handle->no_tracks = metaData->data.streaminfo.channels;
      handle->bits_sample = metaData->data.streaminfo.bitsPerSample;
      handle->max_samples = metaData->data.streaminfo.totalSampleCount;
      break;
   case DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT:
   {
	// https://xiph.org/vorbis/doc/v-comment.html
      drflac_vorbis_comment_iterator it;
      char s[COMMENT_SIZE+1];
      const char *ptr;
      uint32_t slen;
#if 0
      ptr = metaData->data.vorbis_comment.vendor;
      slen = metaData->data.vorbis_comment.vendorLength;
      snprintf(s, _MIN(slen+1, COMMENT_SIZE), "%s\0", ptr);
#endif
      while ((ptr = drflac_next_vorbis_comment(&it, &slen)) != NULL)
      {
         snprintf(s, _MIN(slen+1, COMMENT_SIZE), "%s", ptr);
         if (!STRCMP(s, "TITLE")) {
            handle->title = strdup(s+strlen("TITLE="));
         } else if (!STRCMP(s, "ALBUM")) {
            handle->album = strdup(s+strlen("ALBUM="));
         } else if (!STRCMP(s, "TRACKNUMBER")) {
            handle->trackno = strdup(s+strlen("TRACKNUMBER="));
         } else if (!STRCMP(s, "ARTIST")) {
            handle->artist = strdup(s+strlen("ARTIST="));
         } else if (!STRCMP(s, "PERFORMER")) {
            handle->artist = strdup(s+strlen("PERFORMER="));
         } else if (!STRCMP(s, "COPYRIGHT")) {
            handle->copyright = strdup(s+strlen("COPYRIGHT="));
         } else if (!STRCMP(s, "GENRE")) {
            handle->genre = strdup(s+strlen("GENRE="));
         } else if (!STRCMP(s, "DATE")) {
            handle->date = strdup(s+strlen("DATE="));
         } else if (!STRCMP(s, "ORGANIZATION")) {
            handle->composer= strdup(s+strlen("ORGANIZATION="));
         } else if (!STRCMP(s, "DESCRIPTION")) {
            handle->comments = strdup(s+strlen("DESCRIPTION="));
         } else if (!STRCMP(s, "CONTACT")) {
            handle->website =  strdup(s+strlen("CONTACT="));
         }
      }
      break;
   }
   case DRFLAC_METADATA_BLOCK_TYPE_APPLICATION:
   case DRFLAC_METADATA_BLOCK_TYPE_SEEKTABLE:
   case DRFLAC_METADATA_BLOCK_TYPE_CUESHEET:
   case DRFLAC_METADATA_BLOCK_TYPE_PICTURE:
   case DRFLAC_METADATA_BLOCK_TYPE_PADDING:
   default:
      break;
   }
}
