/*
 * Copyright 2005-2016 by Erik Hofman.
 * Copyright 2009-2016 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <assert.h>

#include <devices.h>
#include <arch.h>

#include "extension.h"
#include "format.h"

enum wavFormat
{
   UNSUPPORTED = 0,
   PCM_WAVE_FILE = 1,
   MSADPCM_WAVE_FILE = 2,
   FLOAT_WAVE_FILE = 3,
   ALAW_WAVE_FILE = 6,
   MULAW_WAVE_FILE = 7,
   IMA4_ADPCM_WAVE_FILE = 17,
   MP3_WAVE_FILE = 85,

   EXTENSIBLE_WAVE_FORMAT = 0xFFFE
};

typedef struct
{
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

   int no_tracks;
   int bits_sample;
   int frequency;
   int bitrate;
   enum aaxFormat format;
   size_t blocksize;
   size_t no_samples;
   size_t max_samples;

   enum wavFormat wav_format;

   union
   {
      struct
      {
         float update_dt;
      } write;

      struct
      {
         size_t blockbufpos;
         size_t wavBufPos;
         uint32_t last_tag;
      } read;
   } io;

   void *wavptr;
   uint32_t *wavBuffer;
   size_t wavBufSize;

   _fmt_t *fmt;

} _driver_t;

static enum aaxFormat _getAAXFormatFromWAVFormat(unsigned int, int);
static enum wavFormat _getWAVFormatFromAAXFormat(enum aaxFormat);
static _fmt_type_t _getFmtFromWAVFormat(enum wavFormat);
static int _aaxFormatDriverReadHeader(_driver_t*, size_t*);
static void* _aaxFormatDriverUpdateHeader(_driver_t*, size_t *);

#define WAVE_HEADER_SIZE	(3+8)
#define WAVE_EXT_HEADER_SIZE	(3+20)
static const uint32_t _aaxDefaultWaveHeader[WAVE_HEADER_SIZE];
static const uint32_t _aaxDefaultExtWaveHeader[WAVE_EXT_HEADER_SIZE];


int
_wav_detect(_ext_t *ext, int mode)
{
   return AAX_TRUE;
}

int
_wav_setup(_ext_t *ext, int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
{
   int bits_sample = aaxGetBitsPerSample(format);
   int rv = AAX_FALSE;

   if (bits_sample)
   {
      _driver_t *handle = calloc(1, sizeof(_driver_t));
      if (handle)
      {
         handle->mode = mode;
         handle->capturing = (mode > 0) ? 0 : 1;
         handle->bits_sample = bits_sample;
         handle->blocksize = tracks*bits_sample/8;
         handle->frequency = freq;
         handle->no_tracks = tracks;
         handle->format = format;
         handle->bitrate = bitrate;
         handle->no_samples = no_samples;
         handle->max_samples = 0;

         if (!handle->capturing)
         {
            handle->wav_format = _getWAVFormatFromAAXFormat(format);
            *bufsize = 0;
         }
         else
         {
            handle->max_samples = 0;
            handle->no_samples = UINT_MAX;
            *bufsize = 2*WAVE_EXT_HEADER_SIZE*sizeof(int32_t);
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
_wav_open(_ext_t *ext, void_ptr buf, size_t *bufsize, size_t fsize)
{
   _driver_t *handle = ext->id;
   void *rv = NULL;

   assert(bufsize);

   if (handle)
   {
      if (!handle->capturing) // write
      {
         char *ptr, extfmt = AAX_FALSE;
         _fmt_type_t fmt;
         size_t size;

         if (handle->bits_sample > 16) extfmt = AAX_TRUE;
         else if (handle->no_tracks > 2) extfmt = AAX_TRUE;
         else if (handle->bits_sample < 8) extfmt = AAX_TRUE;

         if (extfmt) {
            handle->wavBufSize = WAVE_EXT_HEADER_SIZE;
         } else {
            handle->wavBufSize = WAVE_HEADER_SIZE;
         }

         fmt = _getFmtFromWAVFormat(handle->wav_format);
         handle->fmt = _fmt_create(fmt, handle->mode);
         if (!handle->fmt) {
            return rv;
         }

         if (handle->fmt->open)
         {
            handle->fmt->set(handle->fmt, __F_FREQ, handle->frequency);
            handle->fmt->set(handle->fmt, __F_RATE, handle->bitrate);
            handle->fmt->set(handle->fmt, __F_TRACKS, handle->no_tracks);
            handle->fmt->set(handle->fmt, __F_SAMPLES, handle->no_samples);
            handle->fmt->set(handle->fmt, __F_BITS, handle->bits_sample);

            return handle->fmt->open(handle->fmt, buf, bufsize, fsize);
         }
         else if (!handle->fmt->setup(handle->fmt, fmt, handle->format))
         {
            handle->fmt = _fmt_free(handle->fmt);
            return rv;
         }

         handle->fmt->set(handle->fmt, __F_BLOCK, handle->blocksize);
         if (handle->capturing) {
            handle->fmt->set(handle->fmt, __F_POSITION, handle->io.read.blockbufpos);
         }

         ptr = 0;
         size = 4*handle->wavBufSize;
         handle->wavptr = _aax_malloc(&ptr, size);
         handle->wavBuffer = (uint32_t*)ptr;
         if (handle->wavBuffer)
         {
            int32_t s;

            if (extfmt)
            {
               memcpy(handle->wavBuffer, _aaxDefaultExtWaveHeader, size);

               s = (handle->no_tracks << 16) | EXTENSIBLE_WAVE_FORMAT;
               handle->wavBuffer[5] = s;
            }
            else
            {
               memcpy(handle->wavBuffer, _aaxDefaultWaveHeader, size);

               s = (handle->no_tracks << 16) | handle->wav_format;
               handle->wavBuffer[5] = s;
            }

            s = (uint32_t)handle->frequency;
            handle->wavBuffer[6] = s;

            s *= handle->no_tracks * handle->bits_sample/8;
            handle->wavBuffer[7] = s;

            s = (handle->no_tracks * handle->bits_sample/8);
            s |= (handle->bits_sample/8)*8 << 16;
            handle->wavBuffer[8] = s;

            if (extfmt)
            {
               s = handle->bits_sample;
               handle->wavBuffer[9] = s << 16 | 22;

               s = getMSChannelMask(handle->no_tracks);
               handle->wavBuffer[10] = s;

               s = handle->wav_format;
               handle->wavBuffer[11] = s;
            }

            if (is_bigendian())
            {
               size_t i;
               for (i=0; i<handle->wavBufSize; i++)
               {
                  uint32_t tmp = _bswap32(handle->wavBuffer[i]);
                  handle->wavBuffer[i] = tmp;
               }

               handle->wavBuffer[5] =_bswap32(handle->wavBuffer[5]);
               handle->wavBuffer[6] =_bswap32(handle->wavBuffer[6]);
               handle->wavBuffer[7] =_bswap32(handle->wavBuffer[7]);
               handle->wavBuffer[8] =_bswap32(handle->wavBuffer[8]);
               if (extfmt)
               {
                  handle->wavBuffer[9] =
                                           _bswap32(handle->wavBuffer[9]);
                  handle->wavBuffer[10] =
                                          _bswap32(handle->wavBuffer[10]);
                  handle->wavBuffer[11] =
                                          _bswap32(handle->wavBuffer[11]);
               }
            }
            _aaxFormatDriverUpdateHeader(handle, bufsize);

            *bufsize = size;
            rv = handle->wavBuffer;
         }
         else {
            _AAX_FILEDRVLOG("WAV: Insufficient memory");
         }
      }
      else /* read: handle->capturing */
      {
         if (!handle->wavptr)
         {
            char *ptr = 0;

            handle->io.read.wavBufPos = 0;
            handle->wavBufSize = 16384;
            handle->wavptr = _aax_malloc(&ptr, handle->wavBufSize);
            handle->wavBuffer = (uint32_t*)ptr;
         }

         if (handle->wavptr)
         {
            size_t step, size = *bufsize;
            size_t avail = handle->wavBufSize-handle->io.read.wavBufPos;
            _fmt_type_t fmt;
            int res;

            avail = _MIN(size, avail);
            if (!avail) return NULL;

            memcpy((void*)handle->wavBuffer+handle->io.read.wavBufPos,
                   buf, avail);
            handle->io.read.wavBufPos += avail;
            size -= avail;

            /*
             * read the file information and set the file-pointer to
             * the start of the data section
             */
            do
            {
               while ((res = _aaxFormatDriverReadHeader(handle,&step)) != __F_EOF)
               {
                  memmove(handle->wavBuffer,
                          (void*)handle->wavBuffer+step,
                          handle->io.read.wavBufPos-step);
                  handle->io.read.wavBufPos -= step;
                  if (res <= 0) break;
               }

               if (size)        // There's still some data left
               {
                  avail = handle->wavBufSize-handle->io.read.wavBufPos;
                  if (!avail) break;

                  avail = _MIN(size, avail);

                  memcpy((void*)handle->wavBuffer+handle->io.read.wavBufPos,
                         buf, avail);
                  handle->io.read.wavBufPos += avail;
                  size -= avail;
               }
            }
            while (res > 0);

            fmt = _getFmtFromWAVFormat(handle->wav_format);
            handle->fmt = _fmt_create(fmt, handle->mode);
            if (handle->fmt)
            {
               if (handle->fmt->open)
               {
                  handle->fmt->set(handle->fmt, __F_FREQ, handle->frequency);
                  handle->fmt->set(handle->fmt, __F_RATE, handle->bitrate);
                  handle->fmt->set(handle->fmt, __F_TRACKS, handle->no_tracks);
                  handle->fmt->set(handle->fmt,__F_SAMPLES, handle->no_samples);
                  handle->fmt->set(handle->fmt, __F_BITS, handle->bits_sample);
                  rv = handle->fmt->open(handle->fmt, buf, bufsize, fsize);
               }
               else if (res < 0)
               {
                  if (res == __F_PROCESS) {
                     return buf;
                  }
                  else if (size)
                  {
                     _AAX_FILEDRVLOG("WAV: Incorrect format");
                     return rv;
                  }

                  // We're done reading the file header
                  // Now set up the data format handler
                   if (!handle->fmt->setup(handle->fmt, fmt, handle->format))
                  {
                     handle->fmt = _fmt_free(handle->fmt);
                     return rv;
                  }

                  handle->fmt->set(handle->fmt, __F_BLOCK, handle->blocksize);
                  if (handle->capturing) {
                     handle->fmt->set(handle->fmt, __F_POSITION,
                                                   handle->io.read.blockbufpos);
                  }
               }
               else if (res > 0)
               {
                  *bufsize = 0;
                   return buf;
               }
               // else we're done decoding, return NULL
            }
         }
         else
         {
            _AAX_FILEDRVLOG("WAV: Incorrect format");
            return rv;
         }
      }
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
      free(handle->artist);
      free(handle->title);
      free(handle->album);
      free(handle->trackno);
      free(handle->date);
      free(handle->genre);
      free(handle->copyright);
      free(handle->comments);

      _aax_free(handle->wavptr);
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
_wav_update(_ext_t *ext, size_t *offs, size_t *size, char close)
{
   _driver_t *handle = ext->id;
   void *rv = NULL;

   *offs = 0;
   *size = 0;
   if (handle && !handle->capturing)
   {
      if ((handle->io.write.update_dt >= 1.0f) || close)
      {
         handle->io.write.update_dt -= 1.0f;
         rv = _aaxFormatDriverUpdateHeader(handle, size);
      }
   }

   return rv;
}

size_t
_wav_copy(_ext_t *ext, int32_ptr dptr, size_t offs, size_t num)
{
   _driver_t *handle = ext->id;
   size_t rv = __F_EOF;
   size_t bufsize, bytes;
   unsigned int tracks;
   char *buf;
   int bits;

   tracks = handle->no_tracks;
   bits = handle->bits_sample;
   buf = (char*)handle->wavBuffer;

   if (handle->no_samples < num) {
      num = handle->no_samples;
   }

   bufsize = handle->io.read.wavBufPos*8/(tracks*bits);
   if (num > bufsize) {
      num = bufsize;
   }

   offs = offs*tracks*bits/8;
   bytes = num*tracks*bits/8;

   bytes = handle->fmt->copy(handle->fmt, dptr, buf, offs, bytes);
   num = bytes*8/(tracks*bits);
   rv = num;

   if (bytes > 0) 
   {
      handle->io.read.wavBufPos -= bytes;
      memmove(buf, buf+bytes, handle->io.read.wavBufPos);
   }

   if (handle->no_samples >= num) {
      handle->no_samples -= num;
   } else {
      handle->no_samples = 0; 
   }

   return rv;
}

size_t
_wav_process(_ext_t *ext, void_ptr sptr, size_t num)
{
   _driver_t *handle = ext->id;
   char *dptr = (char*)handle->wavBuffer;
   unsigned int tracks = handle->no_tracks;
   int bits = handle->bits_sample;
   size_t offset, size, bytes;

   offset = handle->io.read.wavBufPos;
   size = handle->wavBufSize - offset;
   bytes = _MIN(num*tracks*bits/8, size);
// num = bytes*8/(tracks*bits);

   bytes = handle->fmt->process(handle->fmt, dptr, sptr, offset, num, bytes);
   handle->io.read.wavBufPos += bytes;

   return num; // bytes;
}

size_t
_wav_cvt_from_intl(_ext_t *ext, int32_ptrptr dptr, size_t offset, size_t num)
{
   _driver_t *handle = ext->id;
   char *src = (char*)handle->wavBuffer;
   unsigned int tracks, bits;
   size_t pos, size, bytes;
   size_t rv = __F_EOF;

   if (handle->no_samples < num) {
      num = handle->no_samples;
   }

   bits = handle->bits_sample;
   tracks = handle->no_tracks;
   pos = handle->io.read.wavBufPos;
   size = pos*8/(tracks*bits);
   if (num > size) {
      num = size;
   }

   bytes = handle->fmt->cvt_from_intl(handle->fmt, dptr, offset, src, pos,
                                                   tracks, &num);
   if (handle->wav_format == MP3_WAVE_FILE) {
      return bytes;
   }
   if (handle->format == AAX_IMA4_ADPCM)
   {
      rv = num;
      num /= tracks;
   }
   else
   {
      bytes = num*tracks*bits/8;
      rv = num;
   }

   if (bytes > 0 && handle->wav_format != MP3_WAVE_FILE)
   {
      handle->io.read.wavBufPos -= bytes;
      memmove(src, src+bytes, handle->io.read.wavBufPos);
   }

   if (handle->no_samples >= num) {
      handle->no_samples -= num;
   } else {
      handle->no_samples = 0;
   }
   return rv;
}

size_t
_wav_cvt_to_intl(_ext_t *ext, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = ext->id;
   unsigned int tracks;
   size_t bytes, rv;

   tracks = handle->no_tracks;
   rv = handle->fmt->cvt_to_intl(handle->fmt, dptr, sptr, offs, tracks, num,
                                              scratch, scratchlen);
   bytes = (rv*tracks*handle->bits_sample)/8;

   handle->io.write.update_dt += (float)num/handle->frequency;
   handle->no_samples += num;

   return bytes;
}

char*
_wav_name(_ext_t *ext, enum _aaxStreamParam param)
{
   _driver_t *handle = ext->id;
   char *rv = NULL;

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
   return rv;
}

char*
_wav_interfaces(int mode)
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
   off_t rv = 0;

   switch(type)
   {
   case __F_FMT:
      rv = handle->format;
      break;
   case __F_TRACKS:
      rv = handle->no_tracks;
      break;
   case __F_FREQ:
      rv = handle->frequency;
      break;
   case __F_BITS:
      rv = handle->bits_sample;
      break;
   case __F_BLOCK:
      rv = handle->blocksize;
      break;
   case __F_SAMPLES:
      rv = handle->max_samples;
      break;
   case __F_POSITION:
      break;
   default:
      break;
   }
   return rv;
}

off_t
_wav_set(_ext_t *ext, int type, off_t value)
{
// _driver_t *handle = ext->id;
   off_t rv = 0;

   switch(type)
   {
   case __F_POSITION:
      break;
   default:
      break;
   }
   return rv;
}


/* -------------------------------------------------------------------------- */
#define WAVE_FACT_CHUNK_SIZE		3
#define DEFAULT_OUTPUT_RATE		22050

#define BSWAP(a)			is_bigendian() ? _bswap32(a) : (a)
#define BSWAPH(a)			is_bigendian() ? _bswap32h(a) : (a)

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
    0x00100016,                 /*  9. extension size & valid bits           */
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
int
_aaxFormatDriverReadHeader(_driver_t *handle, size_t *step)
{
   uint32_t *header = handle->wavBuffer;
   size_t size, bufsize = handle->io.read.wavBufPos;
   int32_t curr, init_tag;
   int bits, res = -1;
   char extfmt;

   *step = 0;

   init_tag = curr = handle->io.read.last_tag;
   if (curr == 0) {
      curr = BSWAP(header[0]);
   }
   handle->io.read.last_tag = 0;

   /* Is it a RIFF file? */
   if (curr == 0x46464952)              /* RIFF */
   {
      if (bufsize < WAVE_HEADER_SIZE) {
         return res;
      }

      /* normal or extended format header? */
      curr = BSWAPH(header[5] & 0xFFFF);

      extfmt = (curr == EXTENSIBLE_WAVE_FORMAT) ? 1 : 0;
      if (extfmt && (bufsize < WAVE_EXT_HEADER_SIZE)) {
         return res;
      }

      /* actual file size */
      curr = BSWAP(header[4]);

      /* fmt chunk size */
      size = 5*sizeof(int32_t) + curr;
      *step = res = size;
      if (is_bigendian())
      {
         size_t i;
         for (i=0; i<size/sizeof(int32_t); i++) {
            header[i] = _bswap32(header[i]);
         }
      }
#if 0
{
   char *ch = (char*)header;
   printf("Read %s Header:\n", extfmt ? "Extnesible" : "Canonical");
   printf(" 0: %08x (ChunkID RIFF: \"%c%c%c%c\")\n", header[0], ch[0], ch[1], ch[2], ch[3]);
   printf(" 1: %08x (ChunkSize: %i)\n", header[1], header[1]);
   printf(" 2: %08x (Format WAVE: \"%c%c%c%c\")\n", header[2], ch[8], ch[9], ch[10], ch[11]);
   printf(" 3: %08x (Subchunk1ID fmt: \"%c%c%c%c\")\n", header[3], ch[12], ch[13], ch[14], ch[15]);
   printf(" 4: %08x (Subchunk1Size): %i\n", header[4], header[4]);
   printf(" 5: %08x (NumChannels: %i | AudioFormat: %x)\n", header[5], header[5] >> 16, header[5] & 0xFFFF);
   printf(" 6: %08x (SampleRate: %i)\n", header[6], header[6]);
   printf(" 7: %08x (ByteRate: %i)\n", header[7], header[7]);
   printf(" 8: %08x (BitsPerSample: %i | BlockAlign: %i)\n", header[8], header[8] >> 16, header[8] & 0xFFFF);
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
   else if (header[10] == 0x74636166)
   {
      printf(" 9: %08x (xFromat: %i)\n", header[9], header[9]);
      printf("10: %08x (SubChunk2ID \"fact\")\n", header[10]);
      printf("11: %08x (Subchunk2Size: %i)\n", header[11], header[11]);
      printf("12: %08x (nSamples: %i)\n", header[12], header[12]);
   }
}
#endif

      if (header[2] == 0x45564157 &&            /* WAVE */
          header[3] == 0x20746d66)              /* fmt  */
      {
         handle->frequency = header[6];
         handle->no_tracks = header[5] >> 16;
         handle->wav_format = extfmt ? (header[11]) : (header[5] & 0xFFFF);
         switch(handle->wav_format)
         {
         case MP3_WAVE_FILE:
            handle->bits_sample = 16;
            break;
         default:
            handle->bits_sample = extfmt? (header[9] >> 16) : (header[8] >> 16);
            break;
         }

         if ((handle->bits_sample >= 4 && handle->bits_sample <= 64) &&
             (handle->frequency >= 4000 && handle->frequency <= 256000) &&
             (handle->no_tracks >= 1 && handle->no_tracks <= _AAX_MAX_SPEAKERS))
         {
            handle->blocksize = header[8] & 0xFFFF;
            bits = handle->bits_sample;
            handle->format = _getAAXFormatFromWAVFormat(handle->wav_format,bits);
            switch(handle->format)
            {
            case AAX_FORMAT_NONE:
               return __F_EOF;
               break;
            default:
               break;
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
   else if (curr == 0x5453494c)         /* LIST */
   {                            // http://www.daubnet.com/en/file-format-riff
      ssize_t size = bufsize;

      *step = 0;
      if (!init_tag)
      {
         curr = BSWAP(header[1]);
         handle->io.read.blockbufpos = curr;
         size = _MIN(curr, bufsize);
      }

      /*
       * if handle->io.read.last_tag != 0 we know this is an INFO tag because
       * the last run couldn't finish due to lack of data and we did set
       * handle->io.read.last_tag to "LIST" ourselves.
       */
      if (init_tag || BSWAP(header[2]) == 0x4f464e49)   /* INFO */
      {
         if (!init_tag)
         {
            header += 3;
            size -= 3*sizeof(int32_t);
            *step += 2*sizeof(int32_t);
         }
         *step += sizeof(int32_t);
         res = *step + size;

         do
         {
            int32_t head = BSWAP(header[0]);
            switch(head)
            {
            case 0x54524149:    /* IART: Artist              */
            case 0x4d414e49:    /* INAM: Track Title         */
            case 0x44525049:    /* IPRD: Album Title/Product */
            case 0x4b525449:    /* ITRK: Track Number        */
            case 0x44524349:    /* ICRD: Date Created        */
            case 0x524e4749:    /* IGNR: Genre               */
            case 0x504f4349:    /* ICOP: Copyright           */
            case 0x544d4349:    /* ICMT: Comments            */
            case 0x54465349:    /* ISFT: Software            */
               curr = BSWAP(header[1]);
               size -= 2*sizeof(int32_t) + curr;
               if (size < 0) break;

               switch(head)
               {
               case 0x54524149: /* IART: Artist              */
                  __COPY(handle->artist, curr, (char*)&header[2]);
                  break;
               case 0x4d414e49: /* INAM: Track Title         */
                  __COPY(handle->title, curr, (char*)&header[2]);
                  break;
               case 0x44525049: /* IPRD: Album Title/Product */
                  __COPY(handle->album, curr, (char*)&header[2]);
                  break;
               case 0x4b525449: /* ITRK: Track Number        */
                  __COPY(handle->trackno, curr, (char*)&header[2]);
                  break;
               case 0x44524349: /* ICRD: Date Created        */
                  __COPY(handle->date, curr, (char*)&header[2]);
                  break;
               case 0x524e4749: /* IGNR: Genre               */
                  __COPY(handle->genre, curr, (char*)&header[2]);
                  break;
               case 0x504f4349: /* ICOP: Copyright           */
                  __COPY(handle->copyright, curr, (char*)&header[2]);
                  break;
               case 0x544d4349: /* ICMT: Comments            */
                  __COPY(handle->comments, curr, (char*)&header[2]);
                  break;
               case 0x54465349: /* ISFT: Software            */
               default:
                  break;
               }
               *step += 2*sizeof(int32_t) + curr;
               header = (uint32_t*)((char*)header + 2*sizeof(int32_t) + curr);
               break;
            default:            // we're done
               size = 0;
               *step -= sizeof(int32_t);
               if (head == 0x61746164) {         /* data */
                  res = *step;
               } else {
                  res = __F_EOF;
               }
               break;
            }
         }
         while (size > 0);

         if (size < 0)
         {
            handle->io.read.last_tag = 0x5453494c; /* LIST */
            res = __F_PROCESS;
         }
         else {
            handle->io.read.blockbufpos = 0;
         }
      }
   }
   else if (curr == 0x74636166)         /* fact */
   {
      curr = BSWAP(header[2]);
      handle->no_samples = curr;
      handle->max_samples = curr;
      *step = res = 3*sizeof(int32_t);
   }
   else if (curr == 0x61746164)         /* data */
   {
      curr = (8*header[1])/(handle->no_tracks*handle->bits_sample);
      handle->no_samples = curr;
      handle->max_samples = curr;
      *step = res = 2*sizeof(int32_t);
   }

   return res;
}

static void*
_aaxFormatDriverUpdateHeader(_driver_t *handle, size_t *bufsize)
{
   void *res = NULL;

   if (handle->no_samples != 0)
   {
      char extfmt = (handle->wavBufSize == WAVE_HEADER_SIZE) ? 0 : 1;
      size_t size;
      uint32_t s;

      size = (handle->no_samples*handle->no_tracks*handle->bits_sample)/8;
      s =  4*handle->wavBufSize + size - 8;
      handle->wavBuffer[1] = s;

      s = size;
      if (extfmt)
      {
         handle->wavBuffer[17] = handle->no_samples;
         handle->wavBuffer[19] = s;
      }
      else {
         handle->wavBuffer[10] = s;
      }

      if (is_bigendian())
      {
         handle->wavBuffer[1] = _bswap32(handle->wavBuffer[1]);
         if (extfmt)
         {
            handle->wavBuffer[17] = _bswap32(handle->wavBuffer[17]);
            handle->wavBuffer[19] = _bswap32(handle->wavBuffer[19]);
         }
         else {
            handle->wavBuffer[10] = _bswap32(handle->wavBuffer[10]);
         }
      }

      *bufsize = 4*handle->wavBufSize;
      res = handle->wavBuffer;

#if 0
   printf("Write %s Header:\n", extfmt ? "Extnesible" : "Canonical");
   printf(" 0: %08x (ChunkID \"RIFF\")\n", handle->wavBuffer[0]);
   printf(" 1: %08x (ChunkSize: %i)\n", handle->wavBuffer[1], handle->wavBuffer[1]);
   printf(" 2: %08x (Format \"WAVE\")\n", handle->wavBuffer[2]);
   printf(" 3: %08x (Subchunk1ID \"fmt \")\n", handle->wavBuffer[3]);
   printf(" 4: %08x (Subchunk1Size): %i\n", handle->wavBuffer[4], handle->wavBuffer[4]);
   printf(" 5: %08x (NumChannels: %i | AudioFormat: %x)\n", handle->wavBuffer[5], handle->wavBuffer[5] >> 16, handle->wavBuffer[5] & 0xFFFF);
   printf(" 6: %08x (SampleRate: %i)\n", handle->wavBuffer[6], handle->wavBuffer[6]);
   printf(" 7: %08x (ByteRate: %i)\n", handle->wavBuffer[7], handle->wavBuffer[7]);
   printf(" 8: %08x (BitsPerSample: %i | BlockAlign: %i)\n", handle->wavBuffer[8], handle->wavBuffer[8] >> 16, handle->wavBuffer[8] & 0xFFFF);
   if (!extfmt)
   {
      printf(" 9: %08x (SubChunk2ID \"data\")\n", handle->wavBuffer[9]);
      printf("10: %08x (Subchunk2Size: %i)\n", handle->wavBuffer[10], handle->wavBuffer[10]);
   }
   else
   {
      printf(" 9: %08x (size: %i, nValidBits: %i)\n", handle->wavBuffer[9], handle->wavBuffer[9] >> 16, handle->wavBuffer[9] & 0xFFFF);
      printf("10: %08x (dwChannelMask: %i)\n", handle->wavBuffer[10], handle->wavBuffer[10]);
      printf("11: %08x (GUID0)\n", handle->wavBuffer[11]);
      printf("12: %08x (GUID1)\n", handle->wavBuffer[12]);
      printf("13: %08x (GUID2)\n", handle->wavBuffer[13]);
      printf("14: %08x (GUID3)\n", handle->wavBuffer[14]);

      printf("15: %08x (SubChunk2ID \"fact\")\n", handle->wavBuffer[15]);
      printf("16: %08x (Subchunk2Size: %i)\n", handle->wavBuffer[16], handle->wavBuffer[16]);
      printf("17: %08x (NumSamplesPerChannel: %i)\n", handle->wavBuffer[17], handle->wavBuffer[17]);

      printf("18: %08x (SubChunk3ID \"data\")\n", handle->wavBuffer[18]);
      printf("19: %08x (Subchunk3Size: %i)\n", handle->wavBuffer[19], handle->wavBuffer[19]);
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
   case 6:
      rv |= SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY;
   case 4:
      rv |= SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
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
   int big_endian = is_bigendian();

   switch (format)
   {
   case PCM_WAVE_FILE:
      if (bits_sample == 8) rv = AAX_PCM8U;
      else if (bits_sample == 16 && big_endian) rv = AAX_PCM16S_LE;
      else if (bits_sample == 16) rv = AAX_PCM16S;
      else if (bits_sample == 24 && big_endian) rv = AAX_PCM24S_LE;
      else if (bits_sample == 24) rv = AAX_PCM24S;
      else if (bits_sample == 32 && big_endian) rv = AAX_PCM32S_LE;
      else if (bits_sample == 32) rv = AAX_PCM32S;
      break;
   case FLOAT_WAVE_FILE:
      if (bits_sample == 32 && big_endian) rv = AAX_FLOAT_LE;
      else if (bits_sample == 32) rv = AAX_FLOAT;
      else if (bits_sample == 64 && big_endian) rv = AAX_DOUBLE_LE;
      else if (bits_sample == 64) rv = AAX_DOUBLE;
      break;
   case ALAW_WAVE_FILE:
      rv = AAX_ALAW;
      break;
   case MULAW_WAVE_FILE:
      rv = AAX_MULAW;
      break;
// case MSADPCM_WAVE_FILE:
   case IMA4_ADPCM_WAVE_FILE:
      rv = AAX_IMA4_ADPCM;
      break;
   case MP3_WAVE_FILE:
      rv = AAX_PCM16S;
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
   case AAX_PCM24S:
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
   _fmt_type_t rv = _FMT_PCM;

   switch(fmt)
   {
   case MP3_WAVE_FILE:
      rv = _FMT_MP3;
      break;
   case PCM_WAVE_FILE:
   case MSADPCM_WAVE_FILE:
   case FLOAT_WAVE_FILE:
   case ALAW_WAVE_FILE:
   case MULAW_WAVE_FILE:
   case IMA4_ADPCM_WAVE_FILE:
   default:
      break;
   }
   return rv;
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
#include <fcntl.h>              /* SEEK_*, O_* */
void
_aaxFileDriverWrite(const char *file, enum aaxProcessingType type,
                          void *data, size_t no_samples,
                          size_t freq, char no_tracks,
                          enum aaxFormat format)
{
}
