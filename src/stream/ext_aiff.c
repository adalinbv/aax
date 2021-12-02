/*
 * Copyright 2005-2021 by Erik Hofman.
 * Copyright 2009-2021 by Adalin B.V.
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

#include <base/memory.h>
#include <arch.h>

#include "device.h"
#include "audio.h"
#include "api.h"

// Spec:
// http://midi.teragonaudio.com/tech/aiff.htm
// http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/AIFF/AIFF.html
enum aiffCompression
{
   UNSUPPORTED                = 0x0000000,
   PCM_AIFF_FILE              = 0x4e4f4e45, // NONE
   RAW_AIFF_FILE              = 0x72617720, // raw
   PCM_AIFF_BYTE_SWAPPED_FILE = 0x736f7774, // sowt
   FLOAT32_AIFF_FILE          = 0x666c3332, // fl32
   XFLOAT32_AIFF_FILE         = 0x464c3332, // FL32
   FLOAT64_AIFF_FILE          = 0x666c3634, // fl64
   XFLOAT64_AIFF_FILE         = 0x464c3634, // FL64
   ALAW_AIFF_FILE             = 0x616c6177, // alaw
   MULAW_AIFF_FILE            = 0x756c6177, // ulaw
   IMA4_AIFF_FILE             = 0x696d6134  // ima4
};

typedef struct
{
   _fmt_t *fmt;

   struct _meta_t meta;

   int capturing;
   int mode;

   int bitrate;
   int bits_sample;
   size_t max_samples;
   _buffer_info_t info;

   uint32_t aifc;
   uint32_t aiff_format;
   char copy_to_buffer;

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

   _data_t *aiffBuffer;
   size_t aiffBufSize;

   int32_t *adpcmBuffer;

} _driver_t;

static enum aaxFormat _getAAXFormatFromAIFFFormat(unsigned int, int);
static enum aiffCompression _getAIFFFormatFromAAXFormat(enum aaxFormat);
static _fmt_type_t _getFmtFromAIFFFormat(enum aiffCompression);
static const char* _getNameFromAIFFFormat(enum aiffCompression);
static int _aaxFormatDriverReadHeader(_driver_t*, size_t*);
static void* _aaxFormatDriverUpdateHeader(_driver_t*, ssize_t *);

#define COMMENT_SIZE		1024
#define AIFF_HEADER_SIZE	(3+3+20+4)

#ifdef PRINT
# undef PRINT
#endif
#define PRINT(...) do { printf("%2li: %08x ", head-header, *head); printf(__VA_ARGS__); head++; h += 4; } while(0)

int
_aiff_detect(UNUSED(_ext_t *ext), UNUSED(int mode))
{
   return AAX_TRUE;
}

int
_aiff_setup(_ext_t *ext, int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
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
         handle->aiff_format = PCM_AIFF_BYTE_SWAPPED_FILE;
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
            handle->aiff_format = _getAIFFFormatFromAAXFormat(format);
            if (format == AAX_IMA4_ADPCM) {
               handle->info.blocksize = 512;
            }
            *bufsize = 0;
         }
         ext->id = handle;
         rv = AAX_TRUE;
      }
      else {
         _AAX_FILEDRVLOG("AIFF: Insufficient memory");
      }
   }
   else {
      _AAX_FILEDRVLOG("AIFF: Unsupported format");
   }

   return rv;
}

void*
_aiff_open(_ext_t *ext, void_ptr buf, ssize_t *bufsize, size_t fsize)
{
   _driver_t *handle = ext->id;
   void *rv = NULL;
   if (handle)
   {
      if (!handle->capturing)	/* write */
      {
         char fact = AAX_FALSE;
         _fmt_type_t fmt;
         size_t size;

         // Fact chunks exist in all aiffe files that are compressed
         if (handle->bits_sample < 8) {
            fact = AAX_TRUE;
         }

         fmt = _getFmtFromAIFFFormat(handle->aiff_format);

         if (!handle->fmt) {
            handle->fmt = _fmt_create(fmt, handle->mode);
         }
         if (!handle->fmt) {
            return rv;
         }

         handle->fmt->open(handle->fmt, handle->mode, NULL, NULL, 0);
         if (!handle->fmt->setup(handle->fmt, fmt, handle->info.fmt))
         {
            handle->fmt = _fmt_free(handle->fmt);
            return rv;
         }

         handle->fmt->set(handle->fmt, __F_FREQUENCY, handle->info.rate);
         handle->fmt->set(handle->fmt, __F_BITRATE, handle->bitrate);
         handle->fmt->set(handle->fmt, __F_TRACKS, handle->info.no_tracks);
         handle->fmt->set(handle->fmt, __F_NO_SAMPLES, handle->info.no_samples);
         handle->fmt->set(handle->fmt, __F_BITS_PER_SAMPLE, handle->bits_sample);
         handle->fmt->set(handle->fmt, __F_BLOCK_SIZE, handle->info.blocksize);
         handle->fmt->set(handle->fmt, __F_BLOCK_SAMPLES,
           MSIMA_BLOCKSIZE_TO_SMP(handle->info.blocksize, handle->info.no_tracks));
         rv = handle->fmt->open(handle->fmt, handle->mode, buf, bufsize, fsize);

         handle->aiffBufSize = AIFF_HEADER_SIZE;
         if (fact) {
            handle->aiffBufSize += 4;
         }
         size = handle->aiffBufSize*sizeof(int32_t);
         handle->aiffBuffer = _aaxDataCreate(size, 0);

         if (handle->aiffBuffer)
         {
            const char *cmp = _getNameFromAIFFFormat(handle->aiff_format);
            uint8_t clen = strlen(cmp);
            uint32_t *header;
            uint8_t *ch;

            header = (uint32_t*)_aaxDataGetData(handle->aiffBuffer);
            ch = (uint8_t*)header;

            writestr(&ch, "FORM", 4, &size);
            write32be(&ch, 0, &size);
            if (handle->aiff_format == PCM_AIFF_FILE) {
               writestr(&ch, "AIFF", 4, &size);
            }
            else
            {
               writestr(&ch, "AIFC", 4, &size);
               writestr(&ch, "FVER", 4, &size);
               write32be(&ch, 4, &size);			// ckDataSize
               write32be(&ch, 2726318400, &size);		// timestamp
            }
            writestr(&ch, "COMM", 4, &size);
            if (handle->aiff_format == PCM_AIFF_FILE) {		// ckDataSize
                write32be(&ch, 36, &size);
            } else {
               write32be(&ch, EVEN(41+clen), &size);
            }
            write16be(&ch, handle->info.no_tracks, &size);
            write32be(&ch, handle->info.no_samples, &size);
            write16be(&ch, handle->bits_sample, &size);
            writefp80be(&ch, handle->info.rate, &size);
            if (handle->aiff_format != PCM_AIFF_FILE)
            {
               write32be(&ch, handle->aiff_format, &size);
               writepstr(&ch, cmp, clen, &size);
            }

            handle->io.write.data_chunk_offs = 3 + (uint32_t*)ch - header;
            writestr(&ch, "SSND", 4, &size);
            write32be(&ch, 0, &size);				// ckDataSize
            write32be(&ch, 0, &size);				// offset
            write32be(&ch, 0, &size);				// blockSize

            _aaxFormatDriverUpdateHeader(handle, bufsize);

            *bufsize = 4*handle->aiffBufSize;

            rv = _aaxDataGetData(handle->aiffBuffer);
         }
         else {
            _AAX_FILEDRVLOG("AIFF: Insufficient memory");
         }
      }
			/* read: handle->capturing */
      else if (!handle->fmt || !handle->fmt->open)
      {
         if (!handle->aiffBuffer) {
            handle->aiffBuffer = _aaxDataCreate(16384, 0);
         }

         if (handle->aiffBuffer)
         {
            ssize_t size = *bufsize;
            int res;

            res = _aaxDataAdd(handle->aiffBuffer, buf, size);
            *bufsize = res;
            if (!res) return NULL;

            /*
             * read the file information and set the file-pointer to
             * the start of the data section
             */
            size -= res;
            do
            {
               size_t step = 0;
               while ((res = _aaxFormatDriverReadHeader(handle, &step)) != __F_EOF)
               {
                  _aaxDataMove(handle->aiffBuffer, NULL, step);
                  if (res <= 0) break;
               }

               // The size of 'buf' may have been larger than the size of
               // handle->aiffBuffer and there's still some data left.
               // Copy the next chunk and process it.
               if (size)
               {
                  size_t avail = _aaxDataAdd(handle->aiffBuffer, buf, size);
                  *bufsize += avail;
                  if (!avail) break;

                  size -= avail;
               }
            }
            while (res > 0);

            if (!handle->fmt)
            {
               _fmt_type_t fmt;

               fmt = _getFmtFromAIFFFormat(handle->aiff_format);
               handle->fmt = _fmt_create(fmt, handle->mode);
               if (!handle->fmt) {
                  *bufsize = 0;
                  return rv;
               }

               handle->fmt->open(handle->fmt, handle->mode, NULL, NULL, 0);
               handle->fmt->set(handle->fmt, __F_TRACKS, handle->info.no_tracks);
               handle->fmt->set(handle->fmt, __F_COPY_DATA, handle->copy_to_buffer);
               if (!handle->fmt->setup(handle->fmt, fmt, handle->info.fmt))
               {
                  *bufsize = 0;
                  handle->fmt = _fmt_free(handle->fmt);
                  return rv;
               }

               handle->fmt->set(handle->fmt, __F_FREQUENCY, handle->info.rate);
               handle->fmt->set(handle->fmt, __F_BITRATE, handle->bitrate);
               handle->fmt->set(handle->fmt, __F_TRACKS, handle->info.no_tracks);
               handle->fmt->set(handle->fmt,__F_NO_SAMPLES, handle->info.no_samples);
               handle->fmt->set(handle->fmt, __F_BITS_PER_SAMPLE, handle->bits_sample);
               if (handle->info.fmt == AAX_IMA4_ADPCM) {
                  handle->fmt->set(handle->fmt, __F_BLOCK_SIZE,
                                   handle->info.blocksize/handle->info.no_tracks);
                  handle->fmt->set(handle->fmt, __F_BLOCK_SAMPLES,
                                  MSIMA_BLOCKSIZE_TO_SMP(handle->info.blocksize,
                                                         handle->info.no_tracks));
               } else {
                  handle->fmt->set(handle->fmt, __F_BLOCK_SIZE, handle->info.blocksize);
               }
               handle->fmt->set(handle->fmt, __F_POSITION,
                                                handle->io.read.blockbufpos);
            }

            if (handle->fmt)
            {
               char *dataptr = _aaxDataGetData(handle->aiffBuffer);
               ssize_t datasize = _aaxDataGetDataAvail(handle->aiffBuffer);
               rv = handle->fmt->open(handle->fmt, handle->mode,
                                      dataptr, &datasize,
                                      handle->io.read.datasize);
               if (!rv)
               {
                  if (datasize)
                  {
                      _aaxDataClear(handle->aiffBuffer);
                      _aiff_fill(ext, dataptr, &datasize);
                  }
                  else {
                     *bufsize = 0;
                  }
               }
            }

            if (res < 0)
            {
               if (res == __F_PROCESS) {
                  return buf;
               }
               else if (size)
               {
                  _AAX_FILEDRVLOG("AIFF: Incorrect format");
                  return rv;
               }
            }
            else if (res > 0)
            {
               *bufsize = 0;
                return buf;
            }
            // else we're done decoding, return NULL
         }
         else
         {
            _AAX_FILEDRVLOG("AIFF: Incorrect format");
            return rv;
         }
      }
	/* Format requires more data to process it's own header */
      else if (handle->fmt && handle->fmt->open)
      {
         rv = handle->fmt->open(handle->fmt, handle->mode, buf, bufsize,
                                handle->io.read.datasize);
         if (!rv && bufsize)
         {
            _aaxDataClear(handle->aiffBuffer);
            _aiff_fill(ext, buf, bufsize);
         }
      }
      else _AAX_FILEDRVLOG("AIFF: Unknown opening error");
   }
   else {
      _AAX_FILEDRVLOG("AIFF: Internal error: handle id equals 0");
   }

   return rv;

}

int
_aiff_close(_ext_t *ext)
{
   _driver_t *handle = ext->id;
   int res = AAX_TRUE;

   if (handle)
   {
      if (handle->meta.artist) free(handle->meta.artist);
      if (handle->meta.title) free(handle->meta.title);
      if (handle->meta.album) free(handle->meta.album);
      if (handle->meta.trackno) free(handle->meta.trackno);
      if (handle->meta.date) free(handle->meta.date);
      if (handle->meta.genre) free(handle->meta.genre);
      if (handle->meta.copyright) free(handle->meta.copyright);
      if (handle->meta.comments) free(handle->meta.comments);
      if (handle->meta.composer) free(handle->meta.composer);
      if (handle->meta.original) free(handle->meta.original);
      if (handle->meta.website) free(handle->meta.website);
      if (handle->meta.image) free(handle->meta.image);

      if (handle->adpcmBuffer) _aax_aligned_free(handle->adpcmBuffer);

      _aaxDataDestroy(handle->aiffBuffer);
      if (handle->fmt)
      {
         handle->fmt->close(handle->fmt);
         _fmt_free(handle->fmt);
      }
      free(handle);
   }

   return res;
}

int
_aiff_flush(_ext_t *ext)
{
   _driver_t *handle = ext->id;
   void *header =  _aaxDataGetData(handle->aiffBuffer);
   size_t size = _aaxDataGetSize(handle->aiffBuffer);
   int res, rv = AAX_TRUE;

   rv = handle->fmt->copy(handle->fmt, header, -1, &size);
   if (size)
   {
      size_t step = -1;
      _aaxDataSetOffset(handle->aiffBuffer, size);
      while ((res = _aaxFormatDriverReadHeader(handle, &step)) != __F_EOF)
      {
         _aaxDataMove(handle->aiffBuffer, NULL, step);
         if (res <= 0) break;
      }
   }

   return rv;
}

void*
_aiff_update(_ext_t *ext, size_t *offs, ssize_t *size, char close)
{
   _driver_t *handle = ext->id;
   void *rv = NULL;

   *offs = 0;
   *size = 0;
   if (handle && !handle->capturing)
   {
      // Update the file header every second when writing to make sure there
      // is only 1 seconds worth of data lost in case of an unexpected
      // program termination.
      if ((handle->io.write.update_dt >= 1.0f) || close)
      {
         handle->io.write.update_dt -= 1.0f;
         rv = _aaxFormatDriverUpdateHeader(handle, size);
      }
   }

   return rv;
}

size_t
_aiff_fill(_ext_t *ext, void_ptr sptr, ssize_t *bytes)
{
   _driver_t *handle = ext->id;
   size_t res, rv = __F_PROCESS;

   *bytes = res = _aaxDataAdd(handle->aiffBuffer, sptr, *bytes);
   if (res > 0)
   {
      void *data = _aaxDataGetData(handle->aiffBuffer);
      ssize_t avail = _aaxDataGetDataAvail(handle->aiffBuffer);

      if (avail)
      {
          handle->fmt->fill(handle->fmt, data, &avail);
         _aaxDataMove(handle->aiffBuffer, NULL, avail);
      }
   }

   return rv;
}

size_t
_aiff_copy(_ext_t *ext, int32_ptr dptr, size_t offset, size_t *num)
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
_aiff_cvt_from_intl(_ext_t *ext, int32_ptrptr dptr, size_t offset, size_t *num)
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
_aiff_cvt_to_intl(_ext_t *ext, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
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
_aiff_set_name(_ext_t *ext, enum _aaxStreamParam param, const char *desc)
{
   _driver_t *handle = ext->id;
   int rv = handle->fmt->set_name(handle->fmt, param, desc);

   if (!rv)
   {
      switch(param)
      {
      case __F_ARTIST:
         handle->meta.artist = (char*)desc;
         rv = AAX_TRUE;
         break;
      case __F_TITLE:
         handle->meta.title = (char*)desc;
         rv = AAX_TRUE;
         break;
      case __F_GENRE:
         handle->meta.genre = (char*)desc;
         rv = AAX_TRUE;
         break;
      case __F_TRACKNO:
         handle->meta.trackno = (char*)desc;
         rv = AAX_TRUE;
         break;
      case __F_ALBUM:
         handle->meta.album = (char*)desc;
         rv = AAX_TRUE;
         break;
      case __F_DATE:
         handle->meta.date = (char*)desc;
         rv = AAX_TRUE;
         break;
      case __F_COMPOSER:
         handle->meta.composer = (char*)desc;
         rv = AAX_TRUE;
         break;
      case __F_COMMENT:
         handle->meta.comments = (char*)desc;
         rv = AAX_TRUE;
         break;
      case __F_COPYRIGHT:
         handle->meta.copyright = (char*)desc;
         rv = AAX_TRUE;
         break;
      case __F_ORIGINAL:
         handle->meta.original = (char*)desc;
         rv = AAX_TRUE;
         break;
      case __F_WEBSITE:
         handle->meta.website = (char*)desc;
         rv = AAX_TRUE;
      default:
         break;
      }
   }
   return rv;
}

char*
_aiff_name(_ext_t *ext, enum _aaxStreamParam param)
{
   _driver_t *handle = ext->id;
   char *rv = handle->fmt->name(handle->fmt, param);

   if (!rv)
   {
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
   }
   return rv;
}

char*
_aiff_interfaces(UNUSED(int ext), int mode)
{
   static const char *rd[2] = { "*.aiff *.aif\0", "*.aiff\0" };
   return (char *)rd[mode];
}

int
_aiff_extension(char *ext)
{
   return (ext && (!strcasecmp(ext, "aiff") || !strcasecmp(ext, "aif"))) ? 1 : 0;
}

off_t
_aiff_get(_ext_t *ext, int type)
{
   _driver_t *handle = ext->id;
   off_t rv;

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
      rv = roundf(handle->info.loop_start * 16.0f);
      break;
   case __F_LOOP_END:
      rv = roundf(handle->info.loop_end * 16.0f);
      break;
   case __F_BASE_FREQUENCY:
      rv = handle->info.base_frequency*(1 << 16);
      break;
   case __F_LOW_FREQUENCY:
      rv = handle->info.low_frequency*(1 << 16);
      break;
   case __F_HIGH_FREQUENCY:
      rv = handle->info.high_frequency*(1 << 16);
      break;
   case __F_PITCH_FRACTION:
      rv = handle->info.pitch_fraction*(1 << 24);
      break;
   case __F_TREMOLO_RATE:
      rv = handle->info.tremolo_rate*(1 << 24);
      break;
   case __F_TREMOLO_DEPTH:
      rv = handle->info.tremolo_depth*(1 << 24);
      break;
   case __F_TREMOLO_SWEEP:
      rv = handle->info.tremolo_sweep*(1 << 24);
      break;
   case __F_VIBRATO_RATE:
      rv = handle->info.vibrato_rate*(1 << 24);
      break;
   case __F_VIBRATO_DEPTH:
      rv = handle->info.vibrato_depth*(1 << 24);
      break;
   case __F_VIBRATO_SWEEP:
      rv = handle->info.vibrato_sweep*(1 << 24);
      break;
   default:
      rv = handle->fmt->get(handle->fmt, type);
      break;
   }
   return rv;
}

off_t
_aiff_set(_ext_t *ext, int type, off_t value)
{
   _driver_t *handle = ext->id;
   off_t rv = 0;

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
#define DEFAULT_OUTPUT_RATE		22050

int
_aaxFormatDriverReadHeader(_driver_t *handle, size_t *step)
{
   size_t bufsize = _aaxDataGetDataAvail(handle->aiffBuffer);
   uint8_t *buf = _aaxDataGetData(handle->aiffBuffer);
   uint32_t *header = (uint32_t*)buf;
   uint8_t *ch = (uint8_t*)header;
   uint32_t clen, curr, init_tag;
   char field[COMMENT_SIZE+1];
   char flush = (*step == -1);
   int rv = __F_EOF;

   *step = 0;

   init_tag = curr = handle->io.read.last_tag;
   if (curr == 0) {
#if 0
 printf("%08x: '%c%c%c%c'\n", _aax_bswap32(header[0]), ch[0], ch[1], ch[2], ch[3]);
#endif
      curr = read32be(&ch, &bufsize);
   }
   handle->io.read.last_tag = 0;

#if 0
if (curr == 0x464f524d) // FORM
{
   ssize_t samples = 0;
   char i, aifc = 0;
   uint32_t *head = header;
   uint8_t *c, *h = (uint8_t*)header;
   printf("Read Header:\n");
   PRINT("(ChunkID FORM: \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
   PRINT("(ChunkSize: %i)\n", _aax_bswap32(*head));
   PRINT("(Format AIFF: \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
   if (*head == 0x52455646) { aifc = 1;
      PRINT("(SubchunkID FVER: \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
      PRINT("(SubchunkSize: %i)\n", _aax_bswap32(*head));
      head++; h += 4; // timestamp
   }
   PRINT("(SubchunkID COMM: \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
   c = 4 + h + EVEN(_aax_bswap32(*head));
   PRINT("(SubchunkSize: %i)\n", _aax_bswap32(*head));
   printf("%2li: %08x ", head-header, *head); printf("(numChannels): %i\n", _aax_bswap32(*head) >> 16); h += 2;
   printf("%2li: %08x ", head-header+1, _aax_bswap32(*head));
   printf("(numSampleFrames: %i", (_aax_bswap32(*head) & 0xFFF)<<16|(_aax_bswap32(*(head+1)) >> 16)); head++; h += 4;
   printf(" | sampleSize: %i)\n", _aax_bswap32(*head) & 0xFFFF); head++; h += 2;
   h += 10; head = (uint32_t*)h; // fp80 sample rate
   if (aifc) {
      PRINT("(compressionType: \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
      printf("%2li: %08x ", head-header, *head); printf("(compressionName: \"");
      i = *h++; while(i--) { printf("%c", *h++); } printf("\")\n");
   }
   head = (uint32_t*)c; samples = bufsize - (head-header);
   while (samples > 0 && *head != 0x444e5353) { /* search for data */
      h = (uint8_t*)(head+2);
      head = (uint32_t*)(h + EVEN(_aax_bswap32(head[1])));
      samples = bufsize - (head-header);
   }
   if (samples > 0 && *head == 0x444e5353) /* SSND */
   {
      h = (uint8_t*)head;
      PRINT("(SubChunkID \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
      PRINT("(SubchunkSize: %i)\n", _aax_bswap32(*head));
   }
}
#endif

   /* Is it a AIFF file? */
   switch(curr)
   {
   case 0x464f524d: // FORM
      if (bufsize >= AIFF_HEADER_SIZE)
      {
         curr = read32be(&ch, &bufsize); // fileSize-8
         handle->io.read.size = curr+8;

         curr = read32be(&ch, &bufsize);
         if (curr == 0x41494646 || // AIFF
             curr == 0x41494643) // AIFC
         {
            *step = rv = EVEN(ch-buf);
            handle->io.read.size -= rv;
         } else {
            rv = __F_EOF;
         }
      }
      else {
         rv  = __F_EOF;
      }
      break;
   case 0x434f4d4d: // COMM
      curr = read32be(&ch, &bufsize); // size
      *step = rv = ch-buf+EVEN(curr);
      handle->io.read.size -= rv;

      handle->info.no_tracks = read16be(&ch, &bufsize);
      handle->info.no_samples = read32be(&ch, &bufsize); // per track
      handle->bits_sample = read16be(&ch, &bufsize);
      handle->info.rate = readfp80be(&ch, &bufsize);

      if (handle->aifc >= 2726318400)
      {
         curr = read32be(&ch, &bufsize); // compressionType
         handle->aiff_format = curr;

         // read the pascal string length first
         clen = COMMENT_SIZE;
         readpstr(&ch, field, clen, &bufsize); // compressionName
      }

#if 1
 printf("rate: %f, no_samples: %li, bits/sample: %i, tracks: %i\n", handle->info.rate, handle->info.no_samples, handle->bits_sample, handle->info.no_tracks);
#endif

      if (handle->bits_sample >= 4 && handle->bits_sample <= 64)
      {
         handle->info.blocksize = handle->bits_sample*handle->info.no_tracks/8;
         handle->bitrate = handle->info.rate*handle->bits_sample*handle->info.no_tracks;
         handle->info.fmt = _getAAXFormatFromAIFFFormat(handle->aiff_format,
                                                        handle->bits_sample);
         if (handle->info.fmt == AAX_FORMAT_NONE) {
            rv = __F_EOF;
         }
      }
      else {
         rv = __F_EOF;
      }
      break;
   case 0x53534e44: // SSND
      curr = read32be(&ch, &bufsize); // size
      handle->io.read.datasize = curr;

      curr = read32be(&ch, &bufsize); // offset
      curr = read32be(&ch, &bufsize); // blocksize
      *step = rv = EVEN(ch-buf);
#if 0
{
   char *c = (char*)&handle->aiff_format;
   printf("final:\n");
   printf(" ChunkSize: %li\n", handle->io.read.size);
   printf(" NumChannels: %i\n", handle->info.no_tracks);
   printf(" AudioFormat: '%c%c%c%c'\n", c[3], c[2], c[1], c[0]);
   printf(" SampleRate: %5.1f\n", handle->info.rate);
   printf(" BitsPerSample: %i\n", handle->bits_sample);
   printf(" BlockAlign: %i\n", handle->info.blocksize);
   printf(" dwChannelMask: %08x\n", handle->io.read.channel_mask);
   printf(" nSamples: %li\n", handle->info.no_samples);
   printf(" Subchunk2Size: %li\n", handle->io.read.datasize);
   printf("Duration: %f\n", (float)handle->info.no_samples/handle->info.rate);
}
#endif

      handle->io.read.size -= rv + handle->io.read.datasize;
      if (!handle->io.read.datasize) rv = __F_EOF;
      else rv = __F_PROCESS;
      break;
   case 0x46564552: // FVER
      curr = read32be(&ch, &bufsize); // size
      *step = rv = ch-buf + EVEN(curr);
      handle->io.read.size -= rv;

      curr = read32be(&ch, &bufsize); // timestamp
      handle->aifc = curr;
      break;
   case 0x4e414d45: // NAME
   case 0x41555448: // AUTH
   case 0x28632920: // (c) 
   case 0x414e4e4f: // ANNO
   {
      uint32_t type = curr;

      curr = read32be(&ch, &bufsize); // ckDataSize
      *step = rv = ch-buf + EVEN(curr);
      handle->io.read.size -= rv;

      // The number of characters is determined by ckDataSize.
      clen = _MIN(EVEN(curr), COMMENT_SIZE);
      readstr(&ch, field, clen, &bufsize);
      switch(type)
      {
      case 0x4e414d45: // NAME
         handle->meta.title = stradd(handle->meta.title, field);
         break;
      case 0x41555448: // AUTH
         handle->meta.composer = stradd(handle->meta.composer, field);
         break;
      case 0x28632920: // (c) 
         handle->meta.copyright = stradd(handle->meta.copyright, field);
         break;
      case 0x414e4e4f: // ANNO
         handle->meta.title = stradd(handle->meta.title, field);
         break;
      default:
         break;
      }
      break;
   }
   case 0x434f4d54: // COMT
   {
      uint16_t i, num;

      curr = read32be(&ch, &bufsize); // size
      *step = rv = ch-buf + EVEN(curr);
      handle->io.read.size -= rv;

      num = read16be(&ch, &bufsize);
      for (i=0; i<num; ++i)
      {
         uint32_t timeStamp = read32be(&ch, &bufsize);
         uint32_t MarkerID = read32be(&ch, &bufsize);

         clen = read16be(&ch, &bufsize);
         readstr(&ch, field, clen, &bufsize);
         handle->meta.comments = stradd(handle->meta.comments, field);
      }
      break;
   }
   case 0x494e5354: // INST
      curr = read32be(&ch, &bufsize); // size
      *step = rv = ch-buf + EVEN(curr);
      handle->io.read.size -= rv;

      curr = read8(&ch, &bufsize);
      handle->info.base_frequency = note2freq(curr);

      curr = read8(&ch, &bufsize);
      handle->info.pitch_fraction = cents2pitch(curr, 0.5f);

      curr = read8(&ch, &bufsize);
      handle->info.low_frequency = note2freq(curr);

      curr = read8(&ch, &bufsize);
      handle->info.high_frequency = note2freq(curr);

      curr = read8(&ch, &bufsize);
//    handle->info.low_velocity = curr;

      curr = read8(&ch, &bufsize);
//    handle->info.high_velocity = curr;

      curr = read8(&ch, &bufsize);
//    handle->info.gain = _db2lin((float)curr));

      // sustainLoop
      curr = read16be(&ch, &bufsize); // playMode
      curr = read32be(&ch, &bufsize); // MarkerId: beginLoop
      curr = read32be(&ch, &bufsize); // MarkerId: endLoop

      // releaseLoop
      curr = read16be(&ch, &bufsize); // playMode
      curr = read32be(&ch, &bufsize); // MarkerId: beginLoop
      curr = read32be(&ch, &bufsize); // MarkerId: endLoop
#if 0
   printf("Base Frequency: %f\n", handle->info.base_frequency);
   printf("Low Frequency:  %f\n", handle->info.low_frequency);
   printf("High Frequency: %f\n", handle->info.high_frequency);
   printf("Pitch Fraction: %f\n", handle->info.pitch_fraction);
#endif
      break;
   case 0x49443320: // ID3 
   {
      pdmp3_handle id;

      curr = read32be(&ch, &bufsize); // size
      *step = rv = ch-buf + EVEN(curr);
      handle->io.read.size -= rv;

      memset(&id, 0, sizeof(id));
      id.iend = _MIN(bufsize, PDMP3_INBUF_SIZE);
      memcpy(id.in, ch, bufsize);
      _aaxFormatDriverReadID3Header(&id, &handle->meta);
      rv = *step;
      break;
   case 0x4d41524b: // MARK
   case 0x5045414b: // PEAK
   case 0x4150504c: // APPL
      curr = read32be(&ch, &bufsize); // size
      *step = rv = ch-buf + EVEN(curr);
      handle->io.read.size -= rv;
      break;
   default:
      break;
   }

   // sanity check
   bufsize = _aaxDataGetDataAvail(handle->aiffBuffer);
   if (!flush && ((*step == 0) || (*step > bufsize)))
   {
      *step = 0;
      rv = __F_EOF;
   }

   return rv;
}

static void*
_aaxFormatDriverUpdateHeader(_driver_t *handle, ssize_t *bufsize)
{
   void *res = NULL;

   if (handle->info.no_samples != 0)
   {
#if 0
      uint32_t *header = _aaxDataGetData(handle->aiffBuffer);
      size_t size;
      uint32_t s;

      size = handle->info.no_samples*handle->info.no_tracks*handle->bits_sample;
      size /= 8;

      s = 4*handle->aiffBufSize + size - 8;
      header[1] = s;

      if (handle->io.write.fact_chunk_offs) {
         header[handle->io.write.fact_chunk_offs] = handle->info.no_samples;
      }

      s = size;
      header[handle->io.write.data_chunk_offs] = s;

      if (is_bigendian())
      {
         header[1] = _aax_bswap32(header[1]);
         s = handle->io.write.fact_chunk_offs;
         if (s) {
            header[s] = _aax_bswap32(header[s]);
         }
         s = handle->io.write.data_chunk_offs;
         header[s] = _aax_bswap32(header[s]);
      }
#endif
      *bufsize = 4*handle->aiffBufSize;
      res = _aaxDataGetData(handle->aiffBuffer);
   }

   return res;
}

static enum aaxFormat
_getAAXFormatFromAIFFFormat(unsigned int format, int bits_sample)
{
   enum aaxFormat rv = AAX_FORMAT_NONE;

   switch (format)
   {
   case PCM_AIFF_FILE:
   case RAW_AIFF_FILE:
      if (bits_sample == 8) rv = (format == RAW_AIFF_FILE) ? AAX_PCM8U : AAX_PCM8S;
      else if (bits_sample == 16) rv = AAX_PCM16S_BE;
      else if (bits_sample == 24) rv = AAX_PCM24S_PACKED_BE;
      else if (bits_sample == 32) rv = AAX_PCM32S_BE;
      break;
   case PCM_AIFF_BYTE_SWAPPED_FILE:
      if (bits_sample == 8) rv = AAX_PCM8S;
      else if (bits_sample == 16) rv = AAX_PCM16S_LE;
      else if (bits_sample == 24) rv = AAX_PCM24S_PACKED_LE;
      else if (bits_sample == 32) rv = AAX_PCM32S_LE;
      break;
   case FLOAT32_AIFF_FILE:
   case XFLOAT32_AIFF_FILE:
      rv = AAX_FLOAT_BE;
      break;
   case FLOAT64_AIFF_FILE:
   case XFLOAT64_AIFF_FILE:
      rv = AAX_DOUBLE_BE;
      break;
   case ALAW_AIFF_FILE:
      rv = AAX_ALAW;
      break;
   case MULAW_AIFF_FILE:
      rv = AAX_MULAW;
      break;
   case IMA4_AIFF_FILE:
      rv = AAX_IMA4_ADPCM;
      break;
   default:
      break;
   }
   return rv;
}

static enum aiffCompression
_getAIFFFormatFromAAXFormat(enum aaxFormat format)
{
   enum aiffCompression rv = UNSUPPORTED;
   switch (format & AAX_FORMAT_NATIVE)
   {
   case AAX_PCM8S:
   case AAX_PCM16S:
   case AAX_PCM24S_PACKED:
   case AAX_PCM32S:
      rv = PCM_AIFF_FILE;
      break;
   case AAX_FLOAT:
      rv = FLOAT32_AIFF_FILE;
      break;
   case AAX_DOUBLE:
      rv = FLOAT64_AIFF_FILE;
      break;
   case AAX_ALAW:
      rv = ALAW_AIFF_FILE;
      break;
   case AAX_MULAW:
      rv = MULAW_AIFF_FILE;
      break;
   case AAX_IMA4_ADPCM:
      rv = IMA4_AIFF_FILE;
      break;
   default:
      break;
   }
   return rv;
}

static _fmt_type_t
_getFmtFromAIFFFormat(enum aiffCompression fmt)
{
   _fmt_type_t rv = _FMT_NONE;

   switch(fmt)
   {
   case PCM_AIFF_FILE:
   case RAW_AIFF_FILE:
   case PCM_AIFF_BYTE_SWAPPED_FILE:
   case FLOAT32_AIFF_FILE:
   case XFLOAT32_AIFF_FILE:
   case FLOAT64_AIFF_FILE:
   case XFLOAT64_AIFF_FILE:
   case ALAW_AIFF_FILE:
   case MULAW_AIFF_FILE:
   case IMA4_AIFF_FILE:
      rv = _FMT_PCM;
      break;
   default:
      break;
   }
   return rv;
}

static const char*
_getNameFromAIFFFormat(enum aiffCompression fmt)
{
   const char *rv = "";
   switch(fmt)
   {
   case PCM_AIFF_BYTE_SWAPPED_FILE:
      rv = "little-endian";
      break;
   case FLOAT32_AIFF_FILE:
   case XFLOAT32_AIFF_FILE:
      rv = "IEEE 32-bit float";
      break;
   case FLOAT64_AIFF_FILE:
   case XFLOAT64_AIFF_FILE:
      rv = "IEEE 64-bit float";
      break;
   case ALAW_AIFF_FILE:
      rv = "Alaw 2:1";
      break;
   case MULAW_AIFF_FILE:
      rv = "μlaw 2:1";
      break;
   case IMA4_AIFF_FILE:
      rv = "IMA 4";
      break;
   default:
      break;
   }
   return rv;
}
