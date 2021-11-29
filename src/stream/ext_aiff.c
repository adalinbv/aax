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

   char *artist;
   char *title;
   char *album;
   char *trackno;
   char *date;
   char *genre;
   char *copyright;
   char *comments;

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
static int _aaxFormatDriverReadHeader(_driver_t*, size_t*);
static void* _aaxFormatDriverUpdateHeader(_driver_t*, ssize_t *);

#define COMMENT_SIZE		1024
#define AIFF_HEADER_SIZE	(4+4)

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
         handle->aiff_format = PCM_AIFF_FILE;
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
            uint32_t *header;
            uint8_t *ch;

            header = (uint32_t*)_aaxDataGetData(handle->aiffBuffer);
            ch = (uint8_t*)header;

            writestr(&ch, "FORM", 4, &size);			// [0]
            write32be(&ch, 0, &size);				// [1]
            writestr(&ch, "AIFF", 4, &size);			// [2]
            writestr(&ch, "COMM", 4, &size);			// [3]
            // TODO
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
               size_t step;
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
      if (handle->artist) free(handle->artist);
      if (handle->title) free(handle->title);
      if (handle->album) free(handle->album);
      if (handle->trackno) free(handle->trackno);
      if (handle->date) free(handle->date);
      if (handle->genre) free(handle->genre);
      if (handle->copyright) free(handle->copyright);
      if (handle->comments) free(handle->comments);

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
      case __F_COMMENT:
         handle->comments = (char*)desc;
         rv = AAX_TRUE;
         break;
      case __F_COPYRIGHT:
         handle->copyright = (char*)desc;
         rv = AAX_TRUE;
         break;
      case __F_COMPOSER:
      case __F_ORIGINAL:
      case __F_WEBSITE:
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
         rv = handle->artist;
         break;
      case __F_TITLE:
         rv = handle->title;
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

#define EVEN(n)		(((n) % 2) ? ((n)+1) : (n))

int
_aaxFormatDriverReadHeader(_driver_t *handle, size_t *step)
{
   size_t bufsize = _aaxDataGetDataAvail(handle->aiffBuffer);
   uint8_t *buf = _aaxDataGetData(handle->aiffBuffer);
   uint32_t *header = (uint32_t*)buf;
   uint32_t curr, init_tag;
   uint8_t *ch = buf;
   int rv = __F_EOF;

   *step = 0;

   init_tag = curr = handle->io.read.last_tag;
   if (curr == 0) {
#if 0
 printf("%08x: '%c%c%c%c'\n", header[0], ch[0], ch[1], ch[2], ch[3]);
#endif
      curr = read32be(&ch, &bufsize);
   }
   handle->io.read.last_tag = 0;

#if 0
if (curr == 0x464f524d) // FORM
{
   ssize_t samples = 0;
   uint32_t *head = header;
   uint8_t *h = (uint8_t*)header;
   printf("Read Header:\n");
   PRINT("(ChunkID FORM: \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
   PRINT("(ChunkSize: %i)\n", _aax_bswap32(*head));
   PRINT("(Format AIFF: \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
   if (*header == 0x34364652) {
      PRINT("(Subchunk1ID fmt: \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
      PRINT("(Subchunk1Size): %i\n", _aax_bswap32(*head));
      head += 7; h += 7*4;
   }
   PRINT("(Subchunk1ID COMM: \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
   PRINT("(Subchunk1Size): %i\n", _aax_bswap32(*head));
   printf("%2li: %08x ", head-header, *head);  printf("(numChannels): %i\n", _aax_bswap32(*head) >> 16); h += 2;
   printf("%2li: %08x ", head-header+1, _aax_bswap32(*head));
    printf("(numSampleFrames: %i", (_aax_bswap32(*head) & 0xFFF)<<16|(_aax_bswap32(*(head+1)) >> 16)); head++; h += 4;
    printf("| sampleSize: %i)\n", _aax_bswap32(*head) & 0xFFFF); head++; h += 2;
}
#endif

   /* Is it a AIFF file? */
   switch(curr)
   {
   case 0x464f524d: // FORM
      bufsize = _aaxDataGetDataAvail(handle->aiffBuffer);
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
         char field[COMMENT_SIZE+1];
         size_t clen;

         curr = read32be(&ch, &bufsize); // compressionType
         handle->aiff_format = curr;

         // read the pascal string length first
         clen = _MAX(read8(&ch, &bufsize), COMMENT_SIZE);
         readstr(&ch, field, curr, &clen); // compressionName
      }

#if 0
 printf("no_samples: %li, bits/sample: %i, rate: %f, tracks: %i\n", handle->info.no_samples, handle->bits_sample, handle->info.rate, handle->info.no_tracks);
#endif

      if (handle->bits_sample >= 4 && handle->bits_sample <= 64)
      {
         handle->info.blocksize = handle->bits_sample*handle->info.no_tracks/8;
         handle->bitrate = handle->info.rate*handle->info.blocksize;
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
      *step = rv = EVEN(ch-buf);
      handle->io.read.size -= rv + handle->io.read.datasize;

      curr = read32be(&ch, &bufsize); // offset
      curr = read32be(&ch, &bufsize); // blocksize
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
   // TODO: more chunk types
   case 0x5045414b: // PEAK
   case 0x414e4e4f: // ANNO
      curr = read32be(&ch, &bufsize); // size
      *step = rv = ch-buf + EVEN(curr);
      handle->io.read.size -= rv;
      break;
   default:
      break;
   }

   // sanity check
   bufsize = _aaxDataGetDataAvail(handle->aiffBuffer);
   if ((*step == 0) || (*step > bufsize))
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
      uint32_t *header = _aaxDataGetData(handle->aiffBuffer);
//    char extensible = (handle->aiffBufSize == AIFF_HEADER_SIZE) ? 0 : 1;
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

      *bufsize = 4*handle->aiffBufSize;
      res = _aaxDataGetData(handle->aiffBuffer);

#if 0
{
   uint32_t *head = header;
   char *h = (char*)header;
   extensible = ((header[5] & 0xffff) == EXTENSIBLE_AIFF_FORMAT) ? 1 : 0;
   printf("Write %s Header:\n", extensible ? "Extensible" : "Canonical");
   printf(" 0: %08x (ChunkID FORM: \"%c%c%c%c\")\n", *head, h[0], h[1], h[2], h[3]); head++;
   printf(" 1: %08x (ChunkSize: %i)\n", *head, *head); head++;
   printf(" 2: %08x (Format AIFF: \"%c%c%c%c\")\n", *head, h[8], h[9], h[10], h[11]); head++;
   printf(" 3: %08x (Subchunk1ID fmt: \"%c%c%c%c\")\n", *head, h[12], h[13], h[14], h[15]); head++;
   printf(" 4: %08x (Subchunk1Size): %i\n", *head, *head); head++;
   printf(" 5: %08x (NumChannels: %i | AudioFormat: %i/0x%x)\n", *head, *head >> 16, *head & 0xffff, *head & 0xffff); head++;
   printf(" 6: %08x (SampleRate: %i)\n", *head, *head); head++;
   printf(" 7: %08x (ByteRate: %i)\n", *head, *head); head++;
   printf(" 8: %08x (BitsPerSample: %i | BlockAlign: %i)\n", *head, *head >> 16, *head & 0xffff); head++;
   if (extensible)
   {
      printf(" 9: %08x (size: %i, nValidBits: %i)\n", *head, *head & 0xFFFF, *head >> 16); head++;
      printf("10: %08x (dwChannelMask: %i)\n", *head, *head); head++;
      printf("11: %08x (GUID0)\n", *head++);
      printf("12: %08x (GUID1)\n", *head++);
      printf("13: %08x (GUID2)\n", *head++);
      printf("14: %08x (GUID3)\n", *head++);
   }
   else if (header[4] > 16)
   {
      int i, size = *head & 0xffff;
      printf(" 9: %08x (xFormatBytes: %i) ", *head, size);
      head = (uint32_t*)((char*)head+EVEN(size)+2);
      for(i=0; i<size; ++i) printf("0x%1x ", (unsigned)(unsigned char)h[38+i]);
      printf("\n");
   }
   if (*head == 0x74636166)     /* fact */
   {
      printf("%2li: %08x (SubChunk2ID \"fact\")\n", head-header, *head); head++;
      printf("%2li: %08x (Subchunk2Size: %i)\n", head-header, *head, *head); head++;
      printf("%2li: %08x (nSamples: %i)\n", head-header, *head, *head); head++;
   }
   if (*head == 0x61746164) /* data */
   {
      printf("%2li: %08x (SubChunk2ID \"data\")\n", head-header, *head); head++;
      printf("%2li: %08x (Subchunk2Size: %i)\n", head-header, *head, *head);
   }
}
#endif
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
