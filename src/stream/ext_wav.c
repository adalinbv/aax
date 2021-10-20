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

// https://tools.ietf.org/html/rfc2361
// http://svn.tribler.org/vlc/trunk/include/vlc_codecs.h
enum wavFormat
{
   UNSUPPORTED          = 0x0000,
   PCM_WAVE_FILE        = 0x0001,
   MSADPCM_WAVE_FILE    = 0x0002,
   FLOAT_WAVE_FILE      = 0x0003,
   ALAW_WAVE_FILE       = 0x0006,
   MULAW_WAVE_FILE      = 0x0007,
   IMA4_ADPCM_WAVE_FILE = 0x0011, //    17
   MP3_WAVE_FILE        = 0x0055, //    85
   VORBIS_WAVE_FILE     = 0x6771,
   SPEEX_WAVE_FILE      = 0xa109, // 41225

   EXTENSIBLE_WAVE_FORMAT = 0xFFFE
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

   enum wavFormat wav_format;
   char copy_to_buffer;

   union
   {
      struct
      {
         float update_dt;
      } write;

      struct
      {
         size_t datasize;
         size_t blockbufpos;
         uint32_t last_tag;
      } read;
   } io;

   _data_t *wavBuffer;
   size_t wavBufSize;

} _driver_t;

static enum aaxFormat _getAAXFormatFromWAVFormat(unsigned int, int);
static enum wavFormat _getWAVFormatFromAAXFormat(enum aaxFormat);
static _fmt_type_t _getFmtFromWAVFormat(enum wavFormat);
static int _aaxFormatDriverReadHeader(_driver_t*, size_t*);
static void* _aaxFormatDriverUpdateHeader(_driver_t*, ssize_t *);

#define COMMENT_SIZE		1024
#define WAVE_HEADER_SIZE	(3+8)
#define WAVE_EXT_HEADER_SIZE	(3+20)
static const uint32_t _aaxDefaultWaveHeader[WAVE_HEADER_SIZE];
static const uint32_t _aaxDefaultExtWaveHeader[WAVE_EXT_HEADER_SIZE];


int
_wav_detect(UNUSED(_ext_t *ext), UNUSED(int mode))
{
   return AAX_TRUE;
}

int
_wav_setup(_ext_t *ext, int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
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
         handle->bits_sample = bits_sample;
         handle->info.blocksize = tracks*bits_sample/8;
         handle->info.freq = freq;
         handle->info.tracks = tracks;
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
            handle->wav_format = _getWAVFormatFromAAXFormat(format);
            *bufsize = 0;
         }
         ext->id = handle;
         rv = AAX_TRUE;
      }
      else {
         _AAX_FILEDRVLOG("WAV: Insufficient memory");
      }
   }
   else {
      _AAX_FILEDRVLOG("WAV: Unsupported format");
   }

   return rv;
}

void*
_wav_open(_ext_t *ext, void_ptr buf, ssize_t *bufsize, size_t fsize)
{
   _driver_t *handle = ext->id;
   void *rv = NULL;

   if (handle)
   {
      if (!handle->capturing)	/* write */
      {
         char extfmt = AAX_FALSE;
         _fmt_type_t fmt;
         size_t size;

         if (handle->bits_sample > 16) extfmt = AAX_TRUE;
         else if (handle->info.tracks > 2) extfmt = AAX_TRUE;
         else if (handle->bits_sample < 8) extfmt = AAX_TRUE;

         fmt = _getFmtFromWAVFormat(handle->wav_format);

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

         handle->fmt->set(handle->fmt, __F_FREQUENCY, handle->info.freq);
         handle->fmt->set(handle->fmt, __F_BITRATE, handle->bitrate);
         handle->fmt->set(handle->fmt, __F_TRACKS, handle->info.tracks);
         handle->fmt->set(handle->fmt, __F_NO_SAMPLES, handle->info.no_samples);
         handle->fmt->set(handle->fmt, __F_BITS_PER_SAMPLE, handle->bits_sample);
         handle->fmt->set(handle->fmt, __F_BLOCK_SIZE, handle->info.blocksize);
         handle->fmt->set(handle->fmt, __F_BLOCK_SAMPLES,
           MSIMA_BLOCKSIZE_TO_SMP(handle->info.blocksize, handle->info.tracks));
         rv = handle->fmt->open(handle->fmt, handle->mode, buf, bufsize, fsize);


         if (extfmt) {
            handle->wavBufSize = WAVE_EXT_HEADER_SIZE;
         } else {
            handle->wavBufSize = WAVE_HEADER_SIZE;
         }
         size = 4*handle->wavBufSize;
         handle->wavBuffer = _aaxDataCreate(size, 0);

         if (handle->wavBuffer)
         {
            uint32_t s, *header;

            header = (uint32_t*)_aaxDataGetData(handle->wavBuffer);
            if (extfmt)
            {
               _aaxDataAdd(handle->wavBuffer, _aaxDefaultExtWaveHeader, size);
               s = (handle->info.tracks << 16) | EXTENSIBLE_WAVE_FORMAT;
               header[5] = s;
            }
            else
            {
               _aaxDataAdd(handle->wavBuffer, _aaxDefaultWaveHeader, size);
               s = (handle->info.tracks << 16) | handle->wav_format;
               header[5] = s;
            }

            s = (uint32_t)handle->info.freq;
            header[6] = s;

            s = (s * handle->info.tracks * handle->bits_sample)/8;
            header[7] = s;

            s = handle->info.blocksize;
            s |= handle->bits_sample << 16;
            header[8] = s;

            if (extfmt)
            {
               s = handle->bits_sample;
               header[9] = s << 16 | 22;

               s = getMSChannelMask(handle->info.tracks);
               header[10] = s;

               s = handle->wav_format;
               header[11] = s;
            }

            if (is_bigendian())
            {
               size_t i;
               for (i=0; i<handle->wavBufSize; i++)
               {
                  uint32_t tmp = _aax_bswap32(header[i]);
                  header[i] = tmp;
               }

               header[5] = _aax_bswap32(header[5]);
               header[6] = _aax_bswap32(header[6]);
               header[7] = _aax_bswap32(header[7]);
               header[8] = _aax_bswap32(header[8]);
               if (extfmt)
               {
                  header[9] = _aax_bswap32(header[9]);
                  header[10] = _aax_bswap32(header[10]);
                  header[11] = _aax_bswap32(header[11]);
               }
            }
            _aaxFormatDriverUpdateHeader(handle, bufsize);

            *bufsize = size;

            rv = _aaxDataGetData(handle->wavBuffer);
         }
         else {
            _AAX_FILEDRVLOG("WAV: Insufficient memory");
         }
      }
			/* read: handle->capturing */
      else if (!handle->fmt || !handle->fmt->open)
      {
         if (!handle->wavBuffer) {
            handle->wavBuffer = _aaxDataCreate(16384, 0);
         }

         if (handle->wavBuffer)
         {
            ssize_t datasize = *bufsize, size = *bufsize;
            size_t datapos;
            int res;

            res = _aaxDataAdd(handle->wavBuffer, buf, size);
            if (!res) return NULL;

            /*
             * read the file information and set the file-pointer to
             * the start of the data section
             */
            datapos = 0;
            do
            {
               size_t step;
               while ((res = _aaxFormatDriverReadHeader(handle, &step)) != __F_EOF)
               {
                  datapos += step;
                  datasize -= step;

                  _aaxDataMove(handle->wavBuffer, NULL, step);
                  if (res <= 0) break;
               }

               // The size of 'buf' may have been larger than the size of
               // handle->wavBuffer and there's still some data left.
               // Copy the next chunk and process it.
               if (size)
               {
                  size_t avail = _aaxDataAdd(handle->wavBuffer, buf, size);
                  if (!avail) break;

                  datapos = 0;
                  datasize = avail;
                  size -= avail;
               }
            }
            while (res > 0);

            if (!handle->fmt)
            {
               _fmt_type_t fmt;
               char *dataptr;

               fmt = _getFmtFromWAVFormat(handle->wav_format);
               handle->fmt = _fmt_create(fmt, handle->mode);
               if (!handle->fmt) {
                  *bufsize = 0;
                  return rv;
               }

               handle->fmt->open(handle->fmt, handle->mode, NULL, NULL, 0);
               handle->fmt->set(handle->fmt, __F_TRACKS, handle->info.tracks);
               handle->fmt->set(handle->fmt, __F_COPY_DATA, handle->copy_to_buffer);
               if (!handle->fmt->setup(handle->fmt, fmt, handle->info.fmt))
               {
                  *bufsize = 0;
                  handle->fmt = _fmt_free(handle->fmt);
                  return rv;
               }

               handle->fmt->set(handle->fmt, __F_FREQUENCY, handle->info.freq);
               handle->fmt->set(handle->fmt, __F_BITRATE, handle->bitrate);
               handle->fmt->set(handle->fmt, __F_TRACKS, handle->info.tracks);
               handle->fmt->set(handle->fmt,__F_NO_SAMPLES, handle->info.no_samples);
               handle->fmt->set(handle->fmt, __F_BITS_PER_SAMPLE, handle->bits_sample);
               if (handle->info.fmt == AAX_IMA4_ADPCM) {
                  handle->fmt->set(handle->fmt, __F_BLOCK_SIZE,
                                   handle->info.blocksize/handle->info.tracks);
                  handle->fmt->set(handle->fmt, __F_BLOCK_SAMPLES,
                                  MSIMA_BLOCKSIZE_TO_SMP(handle->info.blocksize,
                                                         handle->info.tracks));
               } else {
                  handle->fmt->set(handle->fmt, __F_BLOCK_SIZE, handle->info.blocksize);
               }
               handle->fmt->set(handle->fmt, __F_POSITION,
                                                handle->io.read.blockbufpos);
               dataptr = (char*)buf + datapos;
               rv = handle->fmt->open(handle->fmt, handle->mode,
                                      dataptr, &datasize,
                                      handle->io.read.datasize);
               if (!rv)
               {
                  if (datasize)
                  {
                      _aaxDataClear(handle->wavBuffer);
                      _wav_fill(ext, dataptr, &datasize);
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
                  _AAX_FILEDRVLOG("WAV: Incorrect format");
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
            _AAX_FILEDRVLOG("WAV: Incorrect format");
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
            _aaxDataClear(handle->wavBuffer);
            _wav_fill(ext, buf, bufsize);
         }
      }
      else _AAX_FILEDRVLOG("WAV: Unknown opening error");
   }
   else {
      _AAX_FILEDRVLOG("WAV: Internal error: handle id equals 0");
   }

   return rv;

}

int
_wav_close(_ext_t *ext)
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

      _aaxDataDestroy(handle->wavBuffer);
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
_wav_update(_ext_t *ext, size_t *offs, ssize_t *size, char close)
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
_wav_fill(_ext_t *ext, void_ptr sptr, ssize_t *bytes)
{
   _driver_t *handle = ext->id;
   unsigned tracks = handle->info.tracks;
   size_t rv = __F_PROCESS;

   if (handle->wav_format == IMA4_ADPCM_WAVE_FILE ||
       handle->wav_format == MSADPCM_WAVE_FILE)
   {
      if (handle->wav_format == IMA4_ADPCM_WAVE_FILE && tracks > 1)
      {
         size_t blocksize = handle->info.blocksize;
         size_t avail = (*bytes/blocksize)*blocksize;
         if (avail)
         {
             if (*bytes > avail) {
                *bytes = avail;
             }

             _aaxDataAdd(handle->wavBuffer, sptr, *bytes);
         }
         else {
            rv = *bytes = 0;
         }
      }
   }
   else {
      rv = handle->fmt->fill(handle->fmt, sptr, bytes);
   }

   return rv;
}

size_t
_wav_copy(_ext_t *ext, int32_ptr dptr, size_t offset, size_t *num)
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
_wav_cvt_from_intl(_ext_t *ext, int32_ptrptr dptr, size_t offset, size_t *num)
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
_wav_cvt_to_intl(_ext_t *ext, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = ext->id;
   size_t rv;

   rv = handle->fmt->cvt_to_intl(handle->fmt, dptr, sptr, offs, num,
                                 scratch, scratchlen);
   handle->info.no_samples += *num;
   handle->io.write.update_dt += (float)*num/handle->info.freq;

   return rv;
}

int
_wav_set_name(_ext_t *ext, enum _aaxStreamParam param, const char *desc)
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
_wav_name(_ext_t *ext, enum _aaxStreamParam param)
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
_wav_interfaces(UNUSED(int ext), int mode)
{
   static const char *rd[2] = { "*.wav\0", "*.wav\0" };
   return (char *)rd[mode];
}

int
_wav_extension(char *ext)
{
   return (ext && !strcasecmp(ext, "wav")) ? 1 : 0;
}

off_t
_wav_get(_ext_t *ext, int type)
{
   _driver_t *handle = ext->id;
   off_t rv;

   switch (type)
   {
   case __F_FMT:
      rv = handle->info.fmt;
      break;
   case __F_NO_BYTES:
      rv = handle->io.read.datasize;
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
_wav_set(_ext_t *ext, int type, off_t value)
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
#define WAVE_FACT_CHUNK_SIZE		3
#define DEFAULT_OUTPUT_RATE		22050

#define EVEN(n)		((n % 1) ? (n+1) : n)
#define __COPY(p, c, h) if ((p = malloc(c)) != NULL) memcpy(p, h, c)

static const uint32_t _aaxDefaultWaveHeader[WAVE_HEADER_SIZE] =
{
    0x46464952,                 /*  0. "RIFF"                                */
    0x00000024,                 /*  1. (file_length - 8)                     */
    0x45564157,                 /*  2. "WAVE"                                */

    0x20746d66,                 /*  3. "fmt "                                */
    0x00000010,                 /*  4. fmt chunk size                        */
    0x00020001,                 /*  5. PCM & stereo                          */
    DEFAULT_OUTPUT_RATE,        /*  6.                                       */
    0x0001f400,                 /*  7. (sample_rate*channels*bits_sample/8)  */
    0x0010000F,                 /*  8. (channels*bits_sample/8)              *
                                 *     & 16 bits per sample                  */
    0x61746164,                 /*  9. "data"                                */
    0                           /* 10. size of data block                    *
                                 *     (sampels*channels*bits_sample/8)      */
};

static const uint32_t _aaxDefaultExtWaveHeader[WAVE_EXT_HEADER_SIZE] =
{
    0x46464952,                 /*  0. "RIFF"                                */
    0x00000024,                 /*  1. (file_length - 8)                     */
    0x45564157,                 /*  2. "WAVE"                                */

    0x20746d66,                 /*  3. "fmt "                                */
    0x00000028,                 /*  4. fmt chunk size                        */
    0x0002fffe,                 /*  5. PCM & stereo                          */
    DEFAULT_OUTPUT_RATE,        /*  6.                                       */
    0x0001f400,                 /*  7. (sample_rate*channels*bits_sample/8)  */
    0x0010000F,                 /*  8. (channels*bits_sample/8)              *
                                 *     & 16 bits per sample                  */
    0x00100022,                 /*  9. extension size & valid bits           */
    0,                          /* 10. speaker mask                          */
        /* sub-format */
    PCM_WAVE_FILE,              /* 11-14 GUID                                */
    KSDATAFORMAT_SUBTYPE1,
    KSDATAFORMAT_SUBTYPE2,
    KSDATAFORMAT_SUBTYPE3,

    0x74636166,                 /* 15. "fact"                                */
    4,                          /* 16. chunk size                            */
    0,                          /* 17. no. samples per track                 */

    0x61746164,                 /* 18. "data"                                */
    0,                          /* 19. chunk size in bytes following thsi    */
};

// http://wiki.audacityteam.org/wiki/WAV
// https://docs.fileformat.com/audio/wav/
// https://sites.google.com/site/musicgapi/technical-documents/wav-file-format
int
_aaxFormatDriverReadHeader(_driver_t *handle, size_t *step)
{
   size_t bufsize = _aaxDataGetDataAvail(handle->wavBuffer);
   uint8_t *buf = _aaxDataGetData(handle->wavBuffer);
   uint32_t *header = (uint32_t*)buf;
   uint32_t curr, init_tag;
   uint8_t *ch = buf;
   int rv = __F_EOF;
   char extfmt;

   *step = 0;

   init_tag = curr = handle->io.read.last_tag;
   if (curr == 0) {
#if 0
 printf("%08x: '%c%c%c%c'\n", header[0], ch[0], ch[1], ch[2], ch[3]);
#endif
      curr = read32(&ch);

   }
   handle->io.read.last_tag = 0;

   /* Is it a RIFF file? */
   if (curr == 0x46464952)              // header[0]: ChunkID: RIFF
   {
      if (bufsize < WAVE_HEADER_SIZE) {
         return __F_EOF;
      }

#if 0
{
   char *h = (char*)header;
   extfmt = ((header[5] & 0xffff) == EXTENSIBLE_WAVE_FORMAT) ? 1 : 0;
   printf("Read %s Header:\n", extfmt ? "Extensible" : "Canonical");
   printf(" 0: %08x (ChunkID RIFF: \"%c%c%c%c\")\n", header[0], h[0], h[1], h[2], h[3]);
   printf(" 1: %08x (ChunkSize: %i)\n", header[1], header[1]);
   printf(" 2: %08x (Format WAVE: \"%c%c%c%c\")\n", header[2], h[8], h[9], h[10], h[11]);
   printf(" 3: %08x (Subchunk1ID fmt: \"%c%c%c%c\")\n", header[3], h[12], h[13], h[14], h[15]);
   printf(" 4: %08x (Subchunk1Size): %i\n", header[4], header[4]);
   printf(" 5: %08x (NumChannels: %i | AudioFormat: %i/0x%x)\n", header[5], header[5] >> 16, header[5] & 0xffff, header[5] & 0xffff);
   printf(" 6: %08x (SampleRate: %i)\n", header[6], header[6]);
   printf(" 7: %08x (ByteRate: %i)\n", header[7], header[7]);
   printf(" 8: %08x (BitsPerSample: %i | BlockAlign: %i)\n", header[8], header[8] >> 16, header[8] & 0xffff);
   if ((header[5] & 0xffff) > PCM_WAVE_FILE) {
      printf(" 9: %08x (xFormatBytes: %i)\n", header[9], header[9] & 0xffff);
   }
}
#endif

// Notice: 'fmt ' is not garuenteed to follow 'RIFF",
// it could be later.

      curr = read32(&ch); 			// header[1]: ChunkSize
      curr = read32(&ch);
      if (curr == 0x45564157)			// header[2]: Format: WAVE
      {
         curr = read32(&ch);
         if (curr == 0x20746d66) 		// header[3]: Subchunk1ID: fmt
         {
            curr = read32(&ch);			// header[4]: Subchunk1Size
            *step = rv = ch-buf+EVEN(curr);

            handle->wav_format = read16(&ch);	// header[5]: AudioFormat
            extfmt = (handle->wav_format == EXTENSIBLE_WAVE_FORMAT) ? 1 : 0;
            if (extfmt && (bufsize < WAVE_EXT_HEADER_SIZE)) {
                return __F_EOF;
            }

            handle->info.tracks = read16(&ch);
            handle->info.freq = read32(&ch);		// header[6]: SampleRate

            curr = read32(&ch);				// header[7]: ByteRate
            handle->info.blocksize = read16(&ch);	// header[8]: BlockAlign
            handle->bits_sample = read16(&ch);		//         BitsPerSample
            if (handle->wav_format > PCM_WAVE_FILE) {
               curr = read16(&ch);				// xFormat bytes
            }

            if (extfmt)
            {
               curr = read16(&ch);		// size
               curr = read16(&ch);		// header[9]: ValidBitsPerSample
               handle->bits_sample = curr;
               curr = read32(&ch);		// header[10]: ChannelMask
               handle->wav_format = read32(&ch);// header[11]: SubFormat
            }

            switch(handle->wav_format)
            {
            case MP3_WAVE_FILE:
               handle->bits_sample = 16;
               break;
            default:
               break;
            }

#if 0
 printf("bits/sample: %i, rate: %f, tracks: %i\n", handle->bits_sample, handle->info.freq, handle->info.tracks);
#endif

            if ((handle->bits_sample >= 4 && handle->bits_sample <= 64) &&
                (handle->info.freq >= 4000 && handle->info.freq <= 256000) &&
                (handle->info.tracks >= 1 && handle->info.tracks <= _AAX_MAX_SPEAKERS))
            {
               handle->info.blocksize = handle->bits_sample*handle->info.tracks/8;
               handle->bitrate = handle->info.freq*handle->info.blocksize;

               handle->info.fmt = _getAAXFormatFromWAVFormat(handle->wav_format,
                                                           handle->bits_sample);
               if (handle->info.fmt == AAX_FORMAT_NONE) {
                  return __F_EOF;
               }
#if 0
{
   char *h = (char*)header;
   printf("Read %s Header:\n", extfmt ? "Extensible" : "Canonical");
   printf(" 0: %08x (ChunkID RIFF: \"%c%c%c%c\")\n", header[0], h[0], h[1], h[2], h[3]);
   printf(" 1: %08x (ChunkSize: %i)\n", header[1], header[1]);
   printf(" 2: %08x (Format WAVE: \"%c%c%c%c\")\n", header[2], h[8], h[9], h[10], h[11]);
   printf(" 3: %08x (Subchunk1ID fmt: \"%c%c%c%c\")\n", header[3], h[12], h[13], h[14], h[15]);
   printf(" 4: %08x (Subchunk1Size): %i\n", header[4], header[4]);
   printf(" 5: %08x (NumChannels: %i | AudioFormat: %i)\n", header[5], handle->info.tracks, handle->wav_format);
   printf(" 6: %08x (SampleRate: %5.1f)\n", header[6], handle->info.freq);
   printf(" 7: %08x (ByteRate: %i)\n", header[7], header[7]);
   printf(" 8: %08x (BitsPerSample: %i | BlockAlign: %i)\n", header[8], handle->bits_sample, handle->info.blocksize);
   if (header[4] == 0x10)
   {
      printf(" 9: %08x (SubChunk2ID \"data\")\n", header[9]);
      printf("10: %08x (Subchunk2Size: %i)\n", header[10], header[10]);
   }
   else if (extfmt)
   {
      printf(" 9: %08x (size: %i, nValidBits: %i)\n", header[9], header[9] & 0xFFFF, header[9] >> 16);
      printf("10: %08x (dwChannelMask: %i)\n", header[10], header[10]);
      printf("11: %08x (GUID0)\n", header[11]);
      printf("12: %08x (GUID1)\n", header[12]);
      printf("13: %08x (GUID2)\n", header[13]);
      printf("14: %08x (GUID3)\n", header[14]);
      printf("15: %08x (SubChunk2ID \"data\")\n", header[15]);
      printf("16: %08x (Subchunk2Size: %i)\n", header[16], header[16]);
   }
   else if (header[10] == 0x74636166)	/* fact */
   {
      printf(" 9: %08x (xFormatBytes: %i)\n", header[9], header[9]);
      printf("10: %08x (SubChunk2ID \"fact\")\n", header[10]);
      printf("11: %08x (Subchunk2Size: %i)\n", header[11], header[11]);
      printf("12: %08x (nSamples: %i)\n", header[12], header[12]);
   }
   printf("AAX format: %x\n", handle->info.fmt);
   printf("rv: %i, step: %li, processed: %lu\n", rv, *step, ch-(uint8_t*)header);
}
#endif
            }
            else {
               return -1;
            }
         }
         else {
            return -1;
         }
      }
      else {
         return -1;
      }
   }
   else if (curr == 0x74636166)         /* fact */
   {
      curr = read32(&ch); // header[1]: size
      *step = rv = ch-buf+EVEN(curr);

      curr = read32(&ch); // header[2]: data
      handle->info.no_samples = curr;
      handle->max_samples = curr;
   }
   else if (curr == 0x61746164)         /* data */
   {
      curr = read32(&ch);
      handle->io.read.datasize = curr;
      if (handle->max_samples == 0)
      {
         curr = curr*8/(handle->info.tracks*handle->bits_sample);
         handle->info.no_samples = curr;
         handle->max_samples = curr;
      }
      *step = rv = ch-buf;
      if (!handle->io.read.datasize) rv = __F_EOF;
   }
   else if (curr == 0x5453494c)         /* LIST */
   {                            // http://www.daubnet.com/en/file-format-riff
      ssize_t size = bufsize;

      *step = 0;
      if (!init_tag)
      {
         curr = read32(&ch);		// header[1]: size
         handle->io.read.blockbufpos = curr;
         size = ch-buf;
         size = _MIN(size, bufsize);
         *step = rv = size;
      }

      /*
       * if handle->io.read.last_tag != 0 we know this is an INFO tag because
       * the last run couldn't finish due to lack of data and we did set
       * handle->io.read.last_tag to "LIST" ourselves.
       */
      curr = read32(&ch);
      if (init_tag || curr == 0x4f464e49)   /* INFO */
      {
         char field[COMMENT_SIZE+1];

         field[COMMENT_SIZE] = 0;
         do
         {
#if 0
 printf("LIST: %c%c%c%C\n", ch[0], ch[1], ch[2], ch[3]);
#endif
            int32_t head = read32(&ch); // header[0];
            switch(head)
            {
            case 0x54524149:    /* IART: Artist              */
            case 0x4d414e49:    /* INAM: Track Title         */
            case 0x44525049:    /* IPRD: Album Title/Product */
            case 0x4b525449:    /* ITRK: Track Number        */
            case 0x44524349:    /* ICRD: Date Created        */
            case 0x524e4749:    /* IGNR: Genre               */
            case 0x504f4349:    /* ICOP: Copyright           */
            case 0x544d4349:    /* ISFT: Comments            */
            case 0x54465349:    /* ICMT: Software            */
            case 0x59454b49:    /* IKEY: Subject             */
            case 0x4a425349:    /* ISBJ	 Keywords            */
            case 0x48435449:    /* ITCH: Engineer            */
            case 0x474e4549:    /* IENG: Technician          */
            case 0x524e4547:    /* GENR: Genre               */
               curr = read32(&ch); // header[1];
               size -= 2*sizeof(int32_t) + EVEN(curr);
               if (size < 0) break;

               rv += 2*sizeof(int32_t)+EVEN(curr);
               *step = rv;

               readstr(&ch, field, curr, COMMENT_SIZE);
               switch(head)
               {
               case 0x54524149: /* IART: Artist              */
                  handle->artist = stradd(handle->artist, field);
                  break;
               case 0x4d414e49: /* INAM: Track Title         */
                  handle->title = stradd(handle->title, field);
                  break;
               case 0x44525049: /* IPRD: Album Title/Product */
                  handle->album = stradd(handle->album, field);
                  break;
               case 0x4b525449: /* ITRK: Track Number        */
                  handle->trackno = stradd(handle->trackno, field);
                  break;
               case 0x44524349: /* ICRD: Date Created        */
                  handle->date = stradd(handle->date, field);
                  break;
               case 0x524e4749: /* IGNR: Genre               */
                  handle->genre = stradd(handle->genre, field);
                  break;
               case 0x504f4349: /* ICOP: Copyright           */
                  handle->copyright = stradd(handle->copyright, field);
                  break;
               case 0x544d4349: /* ICMT: Comments            */
                  handle->comments = stradd(handle->comments, field);
                  break;
               case 0x54465349: /* ISFT: Software            */
               default:
                  handle->comments = stradd(handle->comments, field);
                  break;
               }
               break;
            default:            // we're done
               size = 0;
               if (head != 0x61746164) {         /* data */
                  rv = __F_EOF;
               }
               break;
            }
         }
         while (size > 0);

         if (size < 0)
         {
            handle->io.read.last_tag = 0x5453494c; /* LIST */
            rv = __F_NEED_MORE;
         }
         else {
            handle->io.read.blockbufpos = 0;
         }
      }
      else if (curr == 0x6c746461)         /* adtl */
      {
#if 0
 printf("2 %x: '%c%c%c%c'\n", header[0], ch[0], ch[1], ch[2], ch[3]);
#endif
         curr = read32(&ch);
         if (curr == 0x6e6f7465)	/* note */
         {
            curr = read32(&ch);
            rv += 3*sizeof(int32_t)+EVEN(curr);
            *step = rv;
         }
         else if (curr == 0x6c61626c)	/* labl */
         {
            curr = read32(&ch);
            rv += 3*sizeof(int32_t)+EVEN(curr);
            *step = rv;
         }
         else if (curr == 0x6c747874)	/* ltxt */
         {
            curr = read32(&ch);
            rv += 3*sizeof(int32_t)+EVEN(curr);
            *step = rv;
         }
         else				/* unknown */
         {
            curr = read32(&ch);
            rv += 3*sizeof(int32_t)+EVEN(curr);
            *step = rv;
         }
      }
   }
   else if (curr == 0x6c706d73)		/* smpl */
   {
// https://sites.google.com/site/musicgapi/technical-documents/wav-file-format#smpl
      curr = read32(&ch);
      *step = rv = ch-buf+EVEN(curr);

      curr = BSWAP(header[9]);
      if (curr && *step >= (17*4))
      {
         float cents = 100.0f*header[6]/(float)0xFFFFFFFF;

         handle->info.base_frequency = note2freq((uint8_t)header[5]);
         handle->info.pitch_fraction = cents2pitch(cents, 1.0f);
         handle->info.loop_start = 8*header[13]/handle->bits_sample;
         handle->info.loop_end = 8*header[14]/handle->bits_sample;
         handle->info.loop_count = header[16];
#if 0
   printf("Base Frequency: %f\n", handle->info.base_frequency);
   printf("Pitch Fraction: %f\n", handle->info.pitch_fraction);
   printf("Looping: %s\n", handle->info.loop_count ? "yes" : "no");
   if (handle->info.loop_count)
   {
      printf(" - Loop Count: %li\n", handle->info.loop_count);
      printf(" - Loop Start: %lu\n", handle->info.loop_start);
      printf(" - Loop End:   %lu\n", handle->info.loop_end);
   }
#endif
      }
   }
   else if (curr == 0x74736e69)		/* inst */
   {
// https://sites.google.com/site/musicgapi/technical-documents/wav-file-format#inst
      curr = read32(&ch);
      *step = rv = ch-buf+EVEN(curr);

      handle->info.base_frequency = note2freq((uint8_t)header[2]);
      handle->info.pitch_fraction = cents2pitch((int)header[3], 0.5f);
//    handle->info.gain = _db2lin((float)header[4]);
      handle->info.low_frequency = note2freq((uint8_t)header[5]);
      handle->info.high_frequency = note2freq((uint8_t)header[6]);
//    handle->info.low_velocity = (uint8_t)header[5];
//    handle->info.high_velocity = (uint8_t)header[6];
#if 0
   printf("Base Frequency: %f\n", handle->info.base_frequency);
   printf("Low Frequency:  %f\n", handle->info.low_frequency);
   printf("High Frequency: %f\n", handle->info.high_frequency);
   printf("Pitch Fraction: %f\n", handle->info.pitch_fraction);
#endif
   }
   else if (curr == 0x4b414550)         /* peak */
   { // https://web.archive.org/web/20081201144551/http://music.calarts.edu/~tre/PeakChunk.html
      curr = read32(&ch); // header[1]: size
      *step = rv = ch-buf+EVEN(curr);
   }
   else if (curr == 0x20657563 ||	/* cue  */
            curr == 0x74786562 ||	/* bext */
            curr == 0x4b4e554a)		/* junk */
   {
      curr = read32(&ch);
      *step = rv = ch-buf+EVEN(curr);
   }

   return rv;
}

static void*
_aaxFormatDriverUpdateHeader(_driver_t *handle, ssize_t *bufsize)
{
   void *res = NULL;

   if (handle->info.no_samples != 0)
   {
      char extfmt = (handle->wavBufSize == WAVE_HEADER_SIZE) ? 0 : 1;
      int32_t *header = _aaxDataGetData(handle->wavBuffer);
      size_t size;
      uint32_t s;

      size=(handle->info.no_samples*handle->info.tracks*handle->bits_sample)/8;
      s = 4*handle->wavBufSize + size - 8;
      header[1] = s;

      s = size;
      if (extfmt)
      {
         header[17] = handle->info.no_samples;
         header[19] = s;
      }
      else {
         header[10] = s;
      }

      if (is_bigendian())
      {
         header[1] = _aax_bswap32(header[1]);
         if (extfmt)
         {
            header[17] = _aax_bswap32(header[17]);
            header[19] = _aax_bswap32(header[19]);
         }
         else {
            header[10] = _aax_bswap32(header[10]);
         }
      }

      *bufsize = 4*handle->wavBufSize;
      res = _aaxDataGetData(handle->wavBuffer);

#if 0
   printf("Write %s Header:\n", extfmt ? "Extensible" : "Canonical");
   printf(" 0: %08x (ChunkID \"RIFF\")\n", header[0]);
   printf(" 1: %08x (ChunkSize: %i)\n", header[1], header[1]);
   printf(" 2: %08x (Format \"WAVE\")\n", header[2]);
   printf(" 3: %08x (Subchunk1ID \"fmt \")\n", header[3]);
   printf(" 4: %08x (Subchunk1Size): %i\n", header[4], header[4]);
   printf(" 5: %08x (NumChannels: %i | AudioFormat: %i)\n", header[5], header[5] >> 16, header[5] & 0xFFFF);
   printf(" 6: %08x (SampleRate: %i)\n", header[6], header[6]);
   printf(" 7: %08x (ByteRate: %i)\n", header[7], header[7]);
   printf(" 8: %08x (BitsPerSample: %i | BlockAlign: %i)\n", header[8], header[8] >> 16, header[8] & 0xFFFF);
   if (!extfmt)
   {
      printf(" 9: %08x (SubChunk2ID \"data\")\n", header[9]);
      printf("10: %08x (Subchunk2Size: %i)\n", header[10], header[10]);
   }
   else
   {
      printf(" 9: %08x (size: %i, nValidBits: %i)\n", header[9], header[9] >> 16, header[9] & 0xFFFF);
      printf("10: %08x (dwChannelMask: %i)\n", header[10], header[10]);
      printf("11: %08x (GUID0)\n", header[11]);
      printf("12: %08x (GUID1)\n", header[12]);
      printf("13: %08x (GUID2)\n", header[13]);
      printf("14: %08x (GUID3)\n", header[14]);

      printf("15: %08x (SubChunk2ID \"fact\")\n", header[15]);
      printf("16: %08x (Subchunk2Size: %i)\n", header[16], header[16]);
      printf("17: %08x (NumSamplesPerChannel: %i)\n", header[17], header[17]);

      printf("18: %08x (SubChunk3ID \"data\")\n", header[18]);
      printf("19: %08x (Subchunk3Size: %i)\n", header[19], header[19]);
   }
#endif
   }

   return res;
}

uint32_t
getMSChannelMask(uint16_t nChannels)
{
   uint32_t rv = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;

   /* for now without mode */
   switch (nChannels)
   {
   case 1:
      rv = SPEAKER_FRONT_CENTER;
      break;
   case 8:
      rv |= SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
      // intentional fallthrough
   case 6:
      rv |= SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY;
      // intentional fallthrough
   case 4:
      rv |= SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
      // intentional fallthrough
   case 2:
      break;
   default:
      rv = SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
      break;
   }
   return rv;
}

static enum aaxFormat
_getAAXFormatFromWAVFormat(unsigned int format, int bits_sample)
{
   enum aaxFormat rv = AAX_FORMAT_NONE;

   switch (format)
   {
   case PCM_WAVE_FILE:
      if (bits_sample == 8) rv = AAX_PCM8U;
      else if (bits_sample == 16) rv = AAX_PCM16S_LE;
      else if (bits_sample == 24) rv = AAX_PCM24S_PACKED_LE;
      else if (bits_sample == 32) rv = AAX_PCM32S_LE;
      break;
   case FLOAT_WAVE_FILE:
      if (bits_sample == 32) rv = AAX_FLOAT_LE;
      else if (bits_sample == 64) rv = AAX_DOUBLE_LE;
      break;
   case ALAW_WAVE_FILE:
      rv = AAX_ALAW;
      break;
   case MULAW_WAVE_FILE:
      rv = AAX_MULAW;
      break;
   case MSADPCM_WAVE_FILE:
   case IMA4_ADPCM_WAVE_FILE:
      rv = AAX_IMA4_ADPCM;
      break;
   case MP3_WAVE_FILE:
      rv = AAX_PCM16S;
      break;
   case VORBIS_WAVE_FILE:
      rv = AAX_PCM24S;
      break;
   default:
      break;
   }
   return rv;
}

static enum wavFormat
_getWAVFormatFromAAXFormat(enum aaxFormat format)
{
   enum wavFormat rv = UNSUPPORTED;
   switch (format & AAX_FORMAT_NATIVE)
   {
   case AAX_PCM8U:
   case AAX_PCM16S:
   case AAX_PCM24S_PACKED:
   case AAX_PCM32S:
      rv = PCM_WAVE_FILE;
      break;
   case AAX_FLOAT:
   case AAX_DOUBLE:
      rv = FLOAT_WAVE_FILE;
      break;
   case AAX_ALAW:
      rv = ALAW_WAVE_FILE;
      break;
   case AAX_MULAW:
      rv = MULAW_WAVE_FILE;
      break;
   case AAX_IMA4_ADPCM:
      rv = IMA4_ADPCM_WAVE_FILE;
      break;
   default:
      break;
   }
   return rv;
}

static _fmt_type_t
_getFmtFromWAVFormat(enum wavFormat fmt)
{
   _fmt_type_t rv = _FMT_NONE;

   switch(fmt)
   {
   case MP3_WAVE_FILE:
      rv = _FMT_MP3;
      break;
   case VORBIS_WAVE_FILE:
      rv = _FMT_VORBIS;
      break;
   case PCM_WAVE_FILE:
   case FLOAT_WAVE_FILE:
   case ALAW_WAVE_FILE:
   case MULAW_WAVE_FILE:
   case MSADPCM_WAVE_FILE:
   case IMA4_ADPCM_WAVE_FILE:
      rv = _FMT_PCM;
      break;
   default:
      break;
   }
   return rv;
}

/**
 * Shuffle the WAV based ADPCM interleaved channel blocks to
 * IMA4 expected interleaved blocks for every channel.
 *
 * http://wiki.multimedia.cx/index.php?title=Microsoft_IMA_ADPCM
 * WAV uses:     [track0[0],track1[0]..|track0[1],track1[1]..|... ]
 *               in 32-bit chunks (8 samples per track at a time)
 *
 * https://wiki.multimedia.cx/index.php/Apple_QuickTime_IMA_ADPCM
 * IMA4 expects: [track0[0]..track0[n]|track1[0]..track1[n]|... ]
 */
void
_wav_cvt_msadpcm_to_ima4(void *data, size_t bufsize, unsigned int tracks, size_t *size)
{
   size_t blocksize = *size;

   *size /= tracks;
   if (tracks > 1)
   {
      int32_t *buf = (int32_t*)malloc(blocksize);
      if (buf)
      {
         int32_t* dptr = (int32_t*)data;
         size_t blockBytes, numChunks;
         size_t blockNum, numBlocks;

         numBlocks = bufsize/blocksize;
         blockBytes = blocksize/tracks;
         numChunks = blockBytes/sizeof(int32_t);

         for (blockNum=0; blockNum<numBlocks; blockNum++)
         {
            unsigned int t, i;

            /* block shuffle */
            memcpy(buf, dptr, blocksize);
            for (t=0; t<tracks; t++)
            {
               int32_t *src = (int32_t*)buf + t;
               for (i=0; i < numChunks; i++)
               {
                  *dptr++ = *src;
                  src += tracks;
               }
            }
         }
         free(buf);
      }
   }
}

/**
 * Write a canonical WAVE file from memory to a file.
 *
 * @param a pointer to the exact ascii file location
 * @param no_samples number of samples per audio track
 * @param fs sample frequency of the audio tracks
 * @param no_tracks number of audio tracks in the buffer
 * @param format audio format
 */
void
_aaxFileDriverWrite(const char *file, enum aaxProcessingType type,
                          void *data, size_t no_samples,
                          size_t freq, char no_tracks,
                          enum aaxFormat format)
{
   _ext_t* ext;
   ssize_t req;
   size_t size;
   int fd, oflag;
   int res, mode;
   char *buf;

   mode = AAX_MODE_WRITE_STEREO;
   ext = _ext_create(_EXT_WAV);
   res = _wav_setup(ext, mode, &size, freq, no_tracks, format, no_samples, 0);
   if (!res)
   {
      printf("Error: Unable to setup the file stream handler.\n");
      return;
   }
// ext->set_param(ext, __F_BLOCK_SIZE, 512); // blocksize);

   oflag = O_CREAT|O_WRONLY|O_BINARY;
   if (type == AAX_OVERWRITE) oflag |= O_TRUNC;
   fd = open(file, oflag, 0644);
   if (fd < 0)
   {
      printf("Error: Unable to write to file.\n");
      return;
   }

   req = size;
   buf = _wav_open(ext, NULL, &req, 0);
   size = req;

   res = write(fd, buf, size);
   if (res == -1) {
      _AAX_FILEDRVLOG(strerror(errno));
   }

   if (format == AAX_IMA4_ADPCM) {
      size = no_samples;
   } else {
      size = no_samples * ext->get_param(ext, __F_BLOCK_SIZE);
   }
   res = write(fd, data, size);	// write the header

   close(fd);
   _wav_close(ext);
   _ext_free(ext);
}
