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
// http://wiki.audacityteam.org/wiki/WAV
// https://docs.fileformat.com/audio/wav/
// https://sites.google.com/site/musicgapi/technical-documents/wav-file-format
// 64-bit RF64 format: https://tech.ebu.ch/docs/tech/tech3306v1_0.pdf

// Codecs
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

#define SPEAKER_FRONT_LEFT              0x1
#define SPEAKER_FRONT_RIGHT             0x2
#define SPEAKER_FRONT_CENTER            0x4
#define SPEAKER_LOW_FREQUENCY           0x8
#define SPEAKER_BACK_LEFT               0x10
#define SPEAKER_BACK_RIGHT              0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER    0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER   0x80
#define SPEAKER_BACK_CENTER             0x100
#define SPEAKER_SIDE_LEFT               0x200
#define SPEAKER_SIDE_RIGHT              0x400
#define SPEAKER_TOP_CENTER              0x800
#define SPEAKER_TOP_FRONT_LEFT          0x1000
#define SPEAKER_TOP_FRONT_CENTER        0x2000
#define SPEAKER_TOP_FRONT_RIGHT         0x4000
#define SPEAKER_TOP_BACK_LEFT           0x8000
#define SPEAKER_TOP_BACK_CENTER         0x10000
#define SPEAKER_TOP_BACK_RIGHT          0x20000
#define SPEAKER_BITSTREAM_1_LEFT	0x800000
#define SPEAKER_BITSTREAM_1_RIGHT	0x1000000
#define SPEAKER_BITSTREAM_2_LEFT	0x2000000
#define SPEAKER_BITSTREAM_2_RIGHT	0x4000000
#define SPEAKER_CONTROLSAMPLE_1		0x8000000
#define SPEAKER_CONTROLSAMPLE_2		0x10000000
#define SPEAKER_STEREO_LEFT		0x20000000
#define SPEAKER_STEREO_RIGHT		0x40000000
#define SPEAKER_ALL			0x80000000

#define KSDATAFORMAT_SUBTYPE1           0x00100000
#define KSDATAFORMAT_SUBTYPE2           0xaa000080
#define KSDATAFORMAT_SUBTYPE3           0x719b3800

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
   char rf64;

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
         size_t filesize; // size of the file
         uint32_t blockbufpos;
         uint32_t last_tag;
         uint32_t channel_mask;
      } read;
   } io;

   _data_t *wavBuffer;
   size_t wavBufSize;

   int32_t *adpcmBuffer;

} _driver_t;

static enum aaxFormat _getAAXFormatFromWAVFormat(unsigned int, int);
static enum wavFormat _getWAVFormatFromAAXFormat(enum aaxFormat);
static _fmt_type_t _getFmtFromWAVFormat(enum wavFormat);
static int _aaxFormatDriverReadHeader(_driver_t*, size_t*);
static void* _aaxFormatDriverUpdateHeader(_driver_t*, ssize_t *);
static uint32_t _aaxRouterFromMSChannelMask(uint32_t, uint8_t);
static void _wav_cvt_msadpcm_to_ima4(_driver_t*, int32_ptr, ssize_t*);

#define COMMENT_SIZE		1024
#define WAVE_HEADER_SIZE	(3+6+2)
#define WAVE_EXT_HEADER_SIZE	(3+20)

#ifdef PRINT
# undef PRINT
#endif
#define PRINT(...) do { printf("%2li: %08x ", head-header, *head); printf(__VA_ARGS__); head++; h += 4; } while(0)

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
            handle->wav_format = _getWAVFormatFromAAXFormat(format);
            if (format == AAX_IMA4_ADPCM) {
               handle->info.blocksize = 512;
            }
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
         char extensible = AAX_FALSE;
         char fact = AAX_FALSE;
         _fmt_type_t fmt;
         size_t size;

         // Extensible headers should be used by any PCM format that has more
         // than 2 channels or more than 48,000 samples per second
         if (handle->info.no_tracks > 2 || handle->info.rate > 48000) {
            extensible = AAX_TRUE;
         }

         // Fact chunks exist in all wave files that are compressed
         if (handle->bits_sample < 8) {
            fact = AAX_TRUE;
         }

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

         handle->fmt->set(handle->fmt, __F_FREQUENCY, handle->info.rate);
         handle->fmt->set(handle->fmt, __F_BITRATE, handle->bitrate);
         handle->fmt->set(handle->fmt, __F_TRACKS, handle->info.no_tracks);
         handle->fmt->set(handle->fmt, __F_NO_SAMPLES, handle->info.no_samples);
         handle->fmt->set(handle->fmt, __F_BITS_PER_SAMPLE, handle->bits_sample);
         handle->fmt->set(handle->fmt, __F_BLOCK_SIZE, handle->info.blocksize);
         handle->fmt->set(handle->fmt, __F_BLOCK_SAMPLES,
           MSIMA_BLOCKSIZE_TO_SMP(handle->info.blocksize, handle->info.no_tracks));
         rv = handle->fmt->open(handle->fmt, handle->mode, buf, bufsize, fsize);

         handle->wavBufSize = WAVE_HEADER_SIZE;
         if (fact) {
            handle->wavBufSize += 4;
         }
         if (extensible) {
            handle->wavBufSize += 7;
         }
         size = handle->wavBufSize*sizeof(int32_t);
         handle->wavBuffer = _aaxDataCreate(size, 0);

         if (handle->wavBuffer)
         {
            uint32_t s, *header;
            uint8_t *ch;

            header = (uint32_t*)_aaxDataGetData(handle->wavBuffer);
            ch = (uint8_t*)header;

            writestr(&ch, "RIFF", 4, &size);			// [0]
            write32le(&ch, 0, &size);				// [1]
            writestr(&ch, "WAVE", 4, &size);			// [2]
            writestr(&ch, "fmt ", 4, &size);			// [3]

            s = 4; 		// canonical header size
            if (fact) {
               s += 1;		// XFormatBytes
            }
            if (extensible) {
               s += fact ? 5 : 6;
            }
            write32le(&ch, s*sizeof(int32_t), &size);		// [4]

            if (extensible) {
               s = EXTENSIBLE_WAVE_FORMAT;
            } else {
               s = handle->wav_format;
            }
            write16le(&ch, s, &size);				// [5]

            write16le(&ch, handle->info.no_tracks, &size);	// [5]

            s = (uint32_t)handle->info.rate;
            write32le(&ch, s, &size);				// [6]

            s = (s * handle->info.no_tracks * handle->bits_sample)/8;
            write32le(&ch, s, &size);				// [7]

            write16le(&ch, handle->info.blocksize, &size);	// [8]
            write16le(&ch, handle->bits_sample, &size);		// [8]

            if (fact) {
               write16le(&ch, 2, &size);  // xFormatBytes	// [9]
               write8(&ch, 0, &size);	// byte-1		// [9]
               write8(&ch, 0, &size);	// byte-2		// [9]
            }
            else if (extensible)
            {
               write16le(&ch, 22, &size);			// [9]
               write16le(&ch, handle->bits_sample, &size);	// [9]

               s = getMSChannelMask(handle->info.no_tracks);
               write32le(&ch, s, &size);			// [10]

               write32le(&ch, PCM_WAVE_FILE, &size);		// [11]
               write32le(&ch, KSDATAFORMAT_SUBTYPE1, &size);	// [12]
               write32le(&ch, KSDATAFORMAT_SUBTYPE2, &size);	// [13]
               write32le(&ch, KSDATAFORMAT_SUBTYPE3, &size);	// [14]
            }

            if (extensible || fact)
            {
               handle->io.write.fact_chunk_offs = 2 + (uint32_t*)ch - header;
               writestr(&ch, "fact", 4, &size);
               write32le(&ch, 4, &size);
               write32le(&ch, handle->info.no_samples, &size);
            }

            handle->io.write.data_chunk_offs = 1 + (uint32_t*)ch - header;
            writestr(&ch, "data", 4, &size);
            write32le(&ch, 0, &size);

            _aaxFormatDriverUpdateHeader(handle, bufsize);

            *bufsize = 4*handle->wavBufSize;

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
            ssize_t size = *bufsize;
            int res;

            res = _aaxDataAdd(handle->wavBuffer, buf, size);
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
                  _aaxDataMove(handle->wavBuffer, NULL, step);
                  if (res <= 0) break;
               }

               // The size of 'buf' may have been larger than the size of
               // handle->wavBuffer and there's still some data left.
               // Copy the next chunk and process it.
               if (size)
               {
                  size_t avail = _aaxDataAdd(handle->wavBuffer, buf, size);
                  *bufsize += avail;
                  if (!avail) break;

                  size -= avail;
               }
            }
            while (res > 0);

            if (!handle->fmt)
            {
               _fmt_type_t fmt;

               fmt = _getFmtFromWAVFormat(handle->wav_format);
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
               char *dataptr = _aaxDataGetData(handle->wavBuffer);
               ssize_t datasize = _aaxDataGetDataAvail(handle->wavBuffer);
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

      if (handle->adpcmBuffer) _aax_aligned_free(handle->adpcmBuffer);

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
   size_t res, rv = __F_PROCESS;

   *bytes = res = _aaxDataAdd(handle->wavBuffer, sptr, *bytes);
   if (res > 0)
   {
      void *data = _aaxDataGetData(handle->wavBuffer);
      ssize_t avail = _aaxDataGetDataAvail(handle->wavBuffer);

      if (handle->wav_format == MSADPCM_WAVE_FILE) {
         _wav_cvt_msadpcm_to_ima4(handle, data, &avail);
      }

      if (avail)
      {
          handle->fmt->fill(handle->fmt, data, &avail);
         _aaxDataMove(handle->wavBuffer, NULL, avail);
      }
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
   handle->io.write.update_dt += (float)*num/handle->info.rate;

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

int
_aaxFormatDriverReadHeader(_driver_t *handle, size_t *step)
{
   size_t bufsize = _aaxDataGetDataAvail(handle->wavBuffer);
   uint8_t *buf = _aaxDataGetData(handle->wavBuffer);
   uint32_t *header = (uint32_t*)buf;
   uint32_t curr, init_tag;
   uint64_t curr64;
   uint8_t *ch = buf;
   int rv = __F_EOF;
   char extensible;

   *step = 0;

   init_tag = curr = handle->io.read.last_tag;
   if (curr == 0) {
#if 0
 printf("%08x: '%c%c%c%c'\n", header[0], ch[0], ch[1], ch[2], ch[3]);
#endif
      curr = read32le(&ch, &bufsize);
   }
   handle->io.read.last_tag = 0;

#if 0
if (curr == 0x46464952 ||	// header[0]: ChunkID: RIFF
    curr == 0x34364652)		// ChunkID: RF64
{
   ssize_t samples = 0;
   uint32_t *head = header;
   uint8_t *h = (uint8_t*)header;
   extensible = ((((*head == 0x46464952) ? header[5] : header[14]) & 0xffff) == EXTENSIBLE_WAVE_FORMAT) ? 1 : 0;
   printf("Read %s Header:\n", extensible ? "Extensible" : "Canonical");
   PRINT("(ChunkID RIFF: \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
   PRINT("(ChunkSize: %i)\n", *head);
   PRINT("(Format WAVE: \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
   if (*header == 0x34364652) {
      PRINT("(Subchunk1ID fmt: \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
      PRINT("(Subchunk1Size): %i\n", *head);
      head += 7; h += 7*4;
   }
   PRINT("(Subchunk1ID fmt: \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
   PRINT("(Subchunk1Size): %i\n", *head);
   PRINT("(NumChannels: %i | AudioFormat: %i)\n", *head >> 16, *head & 0xffff);
   PRINT("(SampleRate: %i)\n", *head);
   PRINT("(ByteRate: %i)\n", *head);
   PRINT("(BitsPerSample: %i | BlockAlign: %i)\n", *head >> 16, *head & 0xFFFF);
   if (extensible)
   {
      PRINT("(size: %i, nValidBits: %i)\n", *head & 0xFFFF, *head >> 16);
      PRINT("(dwChannelMask: %08x)\n", *head);
      PRINT("(GUID0)\n");
      PRINT("(GUID1)\n");
      PRINT("(GUID2)\n");
      PRINT("(GUID3)\n");
   }
   else if (header[4] > 16)
   {
      int i, size = *head & 0xffff;
      PRINT("(xFormatBytes: %i) ", size);
      for(i=0; i<size; ++i) printf("0x%1x ", h[i]);
      printf("\n");
   }
   if (*head == 0x74636166)     /* fact */
   {
      PRINT("(SubChunk2ID \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
      PRINT("(Subchunk2Size: %i)\n",*head);
      samples = *head;
      PRINT("(nSamples: %i)\n", *head);
   }
   samples = bufsize - (head-header);
   while (samples > 0 && *head != 0x61746164) { /* search for data */
      h = (uint8_t*)(head+2);
      head = (uint32_t*)(h + EVEN(head[1]));
      samples = bufsize - (head-header);
   }
   if (samples > 0 && *head == 0x61746164) /* data */
   {
      h = (uint8_t*)head;
      PRINT("(SubChunk2ID \"%c%c%c%c\")\n", h[0], h[1], h[2], h[3]);
      PRINT("(Subchunk2Size: %i)\n", *head);
   }
}
#endif

   /* Is it a RIFF file? */
   switch(curr)
   {
   case 0x46464952: // RIFF
   case 0x34364652: // RF64
      bufsize = _aaxDataGetDataAvail(handle->wavBuffer);
      if (bufsize >= WAVE_HEADER_SIZE)
      {
         curr = read32le(&ch, &bufsize);
         handle->io.read.filesize = curr;

         curr = read32le(&ch, &bufsize);
         if (curr == 0x45564157) { // WAVE
            *step = rv = ch-buf;
         } else {
            rv = __F_EOF;
         }
      }
      else {
         rv  = __F_EOF;
      }
      break;
   case 0x20746d66: // fmt 
      curr = read32le(&ch, &bufsize);		 // header[4]: Subchunk1Size
      *step = rv = ch-buf+EVEN(curr);

      handle->wav_format = read16le(&ch, &bufsize);
      extensible = (handle->wav_format == EXTENSIBLE_WAVE_FORMAT) ? 1 : 0;

      handle->info.no_tracks = read16le(&ch, &bufsize);
      handle->info.rate = read32le(&ch, &bufsize);

      handle->bitrate = read32le(&ch, &bufsize);
      handle->info.blocksize = read16le(&ch, &bufsize);
      handle->bits_sample = read16le(&ch, &bufsize);

      if (extensible)
      {
         curr = read16le(&ch, &bufsize);	// size
         curr = read16le(&ch, &bufsize);	// header[9]: ValidBitsPerSample
         handle->bits_sample = curr;

         curr = read32le(&ch, &bufsize);
         handle->io.read.channel_mask = _aaxRouterFromMSChannelMask(curr,
                                                  handle->info.no_tracks);

         handle->wav_format = read32le(&ch, &bufsize);
      }
      else if (rv > 16)
      {
         curr = read16le(&ch, &bufsize);	// xFormatBytes
         ch += EVEN(curr);
      }
      handle->bitrate *= handle->bits_sample;

      switch(handle->wav_format)
      {
      case MP3_WAVE_FILE:
         handle->bits_sample = 16;
         break;
      default:
         break;
      }

#if 0
 printf("bits/sample: %i, rate: %f, tracks: %i\n", handle->bits_sample, handle->info.rate, handle->info.no_tracks);
#endif

      if ((handle->bits_sample >= 4 && handle->bits_sample <= 64) &&
          (handle->info.rate >= 4000 && handle->info.rate <= 256000) &&
          (handle->info.no_tracks >= 1 && handle->info.no_tracks <= _AAX_MAX_SPEAKERS))
      {
         if (handle->wav_format == PCM_WAVE_FILE)
         {
            handle->info.blocksize = handle->bits_sample*handle->info.no_tracks/8;
            handle->bitrate = handle->info.rate*handle->info.blocksize;
         }
         handle->info.fmt = _getAAXFormatFromWAVFormat(handle->wav_format,
                                                           handle->bits_sample);
         if (handle->info.fmt == AAX_FORMAT_NONE) {
            rv = __F_EOF;
         }
      }
      else {
         rv = __F_EOF;
      }
      break;
   case 0x61746164: // data
      curr = read32le(&ch, &bufsize);
      if (curr != -1) {
         handle->io.read.datasize = curr;
      }
      if (handle->max_samples == 0)
      {
         curr = curr*8/(handle->info.no_tracks*handle->bits_sample);
         handle->info.no_samples = curr;
         handle->max_samples = curr;
      }
#if 0
{
   printf("final:\n");
   printf(" ChunkSize: %li\n", handle->io.read.filesize);
   printf(" NumChannels: %i\n", handle->info.no_tracks);
   printf(" AudioFormat: %i\n", handle->wav_format);
   printf(" SampleRate: %5.1f\n", handle->info.rate);
   printf(" BitsPerSample: %i\n", handle->bits_sample);
   printf(" BlockAlign: %i\n", handle->info.blocksize);
   printf(" dwChannelMask: %08x\n", handle->io.read.channel_mask);
   printf(" nSamples: %li\n", handle->info.no_samples);
   printf(" Subchunk2Size: %li\n", handle->io.read.datasize);
   printf("Duration: %f\n", (float)handle->info.no_samples/handle->info.rate);
}
#endif
      *step = rv = ch-buf;
      if (!handle->io.read.datasize) rv = __F_EOF;
      else rv = __F_PROCESS;
      break;
   case 0x74636166: // fact
      curr = read32le(&ch, &bufsize);
      *step = rv = ch-buf+EVEN(curr);

      curr = read32le(&ch, &bufsize); // no. samples
      if (curr != -1)
      {
         handle->info.no_samples = curr;
         handle->max_samples = curr;
      }
      break;
   case 0x5453494c: // LIST
   {                            // http://www.daubnet.com/en/file-format-riff
      ssize_t size = bufsize;

      *step = 0;
      if (!init_tag)
      {
         curr = read32le(&ch, &bufsize);		// header[1]: size
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
      curr = read32le(&ch, &bufsize);
      if (init_tag || curr == 0x4f464e49)   /* INFO */
      {
         char field[COMMENT_SIZE+1];

         field[COMMENT_SIZE] = 0;
         do
         {
#if 0
 printf("LIST: %c%c%c%c\n", ch[0], ch[1], ch[2], ch[3]);
#endif
            int32_t head = read32le(&ch, &bufsize); // header[0];
            size_t clen;
            switch(head)
            {
            case 0x54524149: // IART
            case 0x4d414e49: // INAM
            case 0x44525049: // IPRD
            case 0x4b525449: // ITRK
            case 0x44524349: // ICRD
            case 0x524e4749: // IGNR
            case 0x504f4349: // ICOP
            case 0x544d4349: // ISFT
            case 0x54465349: // ICMT
            case 0x59454b49: // IKEY
            case 0x4a425349: // ISBJ
            case 0x48435449: // ITCH
            case 0x474e4549: // IENG
            case 0x524e4547: // GENR
               curr = read32le(&ch, &bufsize); // header[1];
               size -= 2*sizeof(int32_t) + EVEN(curr);
               if (size < 0) break;

               rv += 2*sizeof(int32_t)+EVEN(curr);
               *step = rv;

               clen = COMMENT_SIZE;
               readstr(&ch, field, curr, &clen);
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
         curr = read32le(&ch, &bufsize);
         switch(curr)
         {
         case 0x6e6f7465: // note
            curr = read32le(&ch, &bufsize);
            rv += 3*sizeof(int32_t)+EVEN(curr);
            *step = rv;
            break;
         case 0x6c61626c: // labl
            curr = read32le(&ch, &bufsize);
            rv += 3*sizeof(int32_t)+EVEN(curr);
            *step = rv;
            break;
         case 0x6c747874: // ltxt
            curr = read32le(&ch, &bufsize);
            rv += 3*sizeof(int32_t)+EVEN(curr);
            *step = rv;
            break;
         default:
            curr = read32le(&ch, &bufsize);
            rv += 3*sizeof(int32_t)+EVEN(curr);
            *step = rv;
         }
      }
      break;
   }
   case 0x6c706d73: // smpl
// https://sites.google.com/site/musicgapi/technical-documents/wav-file-format#smpl
      curr = read32le(&ch, &bufsize);
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
      break;
   case 0x74736e69: // inst
   case 0x7478746c: // ltxt
// https://sites.google.com/site/musicgapi/technical-documents/wav-file-format#inst
      curr = read32le(&ch, &bufsize);
      *step = rv = ch-buf+EVEN(curr);

      curr = read8(&ch, &bufsize);
      handle->info.base_frequency = note2freq(curr);

      curr = read8(&ch, &bufsize);
      handle->info.pitch_fraction = cents2pitch(curr, 0.5f);

      curr = read8(&ch, &bufsize);
//    handle->info.gain = _db2lin((float)curr));

      curr = read8(&ch, &bufsize);
      handle->info.low_frequency = note2freq(curr);

      curr = read8(&ch, &bufsize);
      handle->info.high_frequency = note2freq(curr);

      curr = read8(&ch, &bufsize);
//    handle->info.low_velocity = curr;

      curr = read8(&ch, &bufsize);
//    handle->info.high_velocity = curr;
#if 0
   printf("Base Frequency: %f\n", handle->info.base_frequency);
   printf("Low Frequency:  %f\n", handle->info.low_frequency);
   printf("High Frequency: %f\n", handle->info.high_frequency);
   printf("Pitch Fraction: %f\n", handle->info.pitch_fraction);
#endif
      break;
   case 0x34367364: // ds64
   {
      int i;

      handle->rf64 = 1;

      curr = read32le(&ch, &bufsize);	// SubchunkSize
      *step = rv = ch-buf+EVEN(curr);

      curr64 = read64le(&ch, &bufsize);	// header[1]: ChunkSize
      handle->io.read.filesize = curr64;

      curr64 = read64le(&ch, &bufsize);	// dataSize
      handle->io.read.datasize = curr64;

      curr64 = read64le(&ch, &bufsize);	// sampleCount
      handle->info.no_samples = curr64;

      curr = read32le(&ch, &bufsize);	// table entries
      for (i=0; i<curr; i++) {
         read64le(&ch, &bufsize);
      }
      break;
   }
   case 0x4b414550: /* peak */
   // https://web.archive.org/web/20081201144551/http://music.calarts.edu/~tre/PeakChunk.html
      curr = read32le(&ch, &bufsize); // header[1]: size
      *step = rv = ch-buf+EVEN(curr);
      break;
   case 0x74786562: // bext
   {
      char field[COMMENT_SIZE+1];
      size_t clen;

      curr = read32le(&ch, &bufsize);
      *step = rv = ch-buf+EVEN(curr);

      clen = 256;
      readstr(&ch, field, curr, &clen);
      handle->comments = stradd(handle->title, field);

      clen = 32;
      readstr(&ch, field, curr, &clen);
      handle->copyright = stradd(handle->artist, field);

      clen = 32;
      readstr(&ch, field, curr, &clen);
      handle->copyright = stradd(handle->copyright, field);

      clen = 10;
      readstr(&ch, field, curr, &clen);
      handle->copyright = stradd(handle->date, field);
      break;
   }
   case 0x20657563: // cue 
   case 0x20444150: // PAD 
   case 0x4b4e554a: // junk
      curr = read32le(&ch, &bufsize);
      *step = rv = ch-buf+EVEN(curr);
      break;
   default:
      break;
   }

   // sanity check
   bufsize = _aaxDataGetDataAvail(handle->wavBuffer);
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
      uint32_t *header = _aaxDataGetData(handle->wavBuffer);
//    char extensible = (handle->wavBufSize == WAVE_HEADER_SIZE) ? 0 : 1;
      size_t size;
      uint32_t s;

      size = handle->info.no_samples*handle->info.no_tracks*handle->bits_sample;
      size /= 8;

      s = 4*handle->wavBufSize + size - 8;
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

      *bufsize = 4*handle->wavBufSize;
      res = _aaxDataGetData(handle->wavBuffer);

#if 0
{
   uint32_t *head = header;
   char *h = (char*)header;
   extensible = ((header[5] & 0xffff) == EXTENSIBLE_WAVE_FORMAT) ? 1 : 0;
   printf("Write %s Header:\n", extensible ? "Extensible" : "Canonical");
   printf(" 0: %08x (ChunkID RIFF: \"%c%c%c%c\")\n", *head, h[0], h[1], h[2], h[3]); head++;
   printf(" 1: %08x (ChunkSize: %i)\n", *head, *head); head++;
   printf(" 2: %08x (Format WAVE: \"%c%c%c%c\")\n", *head, h[8], h[9], h[10], h[11]); head++;
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


/*
 * The drivers channel mask defines a a 32-bit packed nibble-array where every
 * nibble specifies a aaxTrackType for that particular channel.
 * The lowest 4-bits are for channel 0.
 */
uint32_t
_aaxRouterFromMSChannelMask(uint32_t mask, uint8_t no_channels)
{
   uint32_t rv = 0x10101010;

   if (mask)
   {
      switch(no_channels)
      {
      case 4:
         if (mask & (SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT))
         {
            rv = (rv & 0xFFFFF0FF) | (AAX_TRACK_REAR_LEFT << 8);
            rv = (rv & 0xFFFF0FFF) | (AAX_TRACK_REAR_RIGHT << 12);
         }
         if (mask & (SPEAKER_FRONT_CENTER|SPEAKER_BACK_CENTER))
         {
            rv = (rv & 0xFFFFF0FF) | (AAX_TRACK_CENTER_FRONT << 8);
            rv = (rv & 0xFFFF0FFF) | (AAX_TRACK_CENTER_FRONT << 12);
         }
         break;
      case 6:
         if (mask & (SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT))
         {
            rv = (rv & 0xFFFFF0FF) | (AAX_TRACK_REAR_LEFT << 8);
            rv = (rv & 0xFFFF0FFF) | (AAX_TRACK_REAR_RIGHT << 12);
         }
         if (mask & (SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY))
         {
            rv = (rv & 0xFFF0FFFF) | (AAX_TRACK_CENTER_FRONT << 16);
            rv = (rv & 0xFF0FFFFF) | (AAX_TRACK_SUBWOOFER << 20);
         }
         break;
      case 8:
         if (mask & (SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT))
         {
            rv = (rv & 0xFFFFF0FF) | (AAX_TRACK_REAR_LEFT << 8);
            rv = (rv & 0xFFFF0FFF) | (AAX_TRACK_REAR_RIGHT << 12);
         }
         if (mask & (SPEAKER_FRONT_CENTER||SPEAKER_LOW_FREQUENCY))
         {
            rv = (rv & 0xFFF0FFFF) | (AAX_TRACK_CENTER_FRONT << 16);
            rv = (rv & 0xFF0FFFFF) | (AAX_TRACK_SUBWOOFER << 20);
         }
         if (mask & (SPEAKER_SIDE_LEFT|SPEAKER_SIDE_RIGHT))
         {
            rv = (rv & 0xF0FFFFFF) | (AAX_TRACK_SIDE_LEFT << 24);
            rv = (rv & 0x0FFFFFFF) | (AAX_TRACK_SIDE_RIGHT << 28);
         }
         break;
      default:
         break;
      }
   }
   return rv;
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
   case AAX_PCM8S:
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
static void
_wav_cvt_msadpcm_to_ima4(_driver_t *handle, int32_ptr dptr, ssize_t *bufsize)
{
   unsigned int tracks = handle->info.no_tracks;
   size_t blockSize = handle->info.blocksize;
   size_t bufSize = *bufsize;

   *bufsize = bufSize/tracks;
   if (tracks > 1)
   {
      int32_t *buf;

      if (!handle->adpcmBuffer) {
         handle->adpcmBuffer = (int32_t*)_aax_aligned_alloc(blockSize);
      }

      buf = handle->adpcmBuffer;
      if (buf)
      {
         int numBlocks, numChunks;
         int blockNum;

         numBlocks = bufSize/blockSize;
         numChunks = blockSize/sizeof(int32_t);

         for (blockNum=0; blockNum<numBlocks; blockNum++)
         {
            unsigned int t, i;

            /* block shuffle */
            memcpy(buf, dptr, blockSize);
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
   res = write(fd, data, size);

   close(fd);
   _wav_close(ext);
   _ext_free(ext);
}
