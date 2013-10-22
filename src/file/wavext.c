/*
 * Copyright 2005-2012 by Erik Hofman.
 * Copyright 2009-2012 by Adalin B.V.
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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>   /* strcasecmp */
# endif
#endif
#include <fcntl.h>		/* SEEK_*, O_* */
#include <assert.h>		/* assert */
#include <errno.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif

#include <base/types.h>
#include <software/audio.h>
#include <ringbuffer.h>
#include <devices.h>
#include <arch.h>

#include "filetype.h"
#include "audio.h"

static _detect_fn _aaxWavFileDetect;

static _new_hanle_fn _aaxWavFileSetup;
static _open_fn _aaxWavFileOpen;
static _close_fn _aaxWavFileClose;
static _update_fn _aaxWavFileReadWrite;

static _default_fname_fn _aaxWavFileInterfaces;
static _extension_fn _aaxWavFileExtension;

static _get_param_fn _aaxWavFileGetFormat;
static _get_param_fn _aaxWavFileGetNoTracks;
static _get_param_fn _aaxWavFileGetFrequency;
static _get_param_fn _aaxWavFileGetBitsPerSample;

_aaxFileHandle*
_aaxDetectWavFile()
{
   _aaxFileHandle* rv = calloc(1, sizeof(_aaxFileHandle));
   if (rv)
   {
      rv->detect = (_detect_fn*)&_aaxWavFileDetect;
      rv->setup = (_new_hanle_fn*)&_aaxWavFileSetup;
      rv->open = (_open_fn*)&_aaxWavFileOpen;
      rv->close = (_close_fn*)&_aaxWavFileClose;
      rv->update = (_update_fn*)&_aaxWavFileReadWrite;

      rv->supported = (_extension_fn*)&_aaxWavFileExtension;
      rv->interfaces = (_default_fname_fn*)&_aaxWavFileInterfaces;

      rv->get_format = (_get_param_fn*)&_aaxWavFileGetFormat;
      rv->get_no_tracks = (_get_param_fn*)&_aaxWavFileGetNoTracks;
      rv->get_frequency = (_get_param_fn*)&_aaxWavFileGetFrequency;
      rv->get_bits_per_sample = (_get_param_fn*)&_aaxWavFileGetBitsPerSample;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

#define WAVE_HEADER_SIZE        	11
#define WAVE_EXT_HEADER_SIZE    	17
#define DEFAULT_OUTPUT_RATE		22050
#define MSBLOCKSIZE_TO_SMP(b, t)	(((b)-4*(t))*2)/(t)


static const uint32_t _aaxDefaultWaveHeader[WAVE_EXT_HEADER_SIZE] =
{
    0x46464952,                 /*  0. "RIFF"                                */
    0x00000024,                 /*  1. (file_length - 8)                     */
    0x45564157,                 /*  2. "WAVE"                                */

    0x20746d66,                 /*  3. "fmt "                                */
    0x00000010,                 /*  4.                                       */
    0x00020001,                 /*  5. PCM & stereo                          */
    DEFAULT_OUTPUT_RATE,        /*  6.                                       */
    0x0001f400,                 /*  7. (sample_rate*channels*bits_sample/8)  */
    0x00100004,                 /*  8. (channels*bits_sample/8)              *
                                 *     & 16 bits per sample                  */
	/* used for both the extensible data section and data section */
    0x61746164,                 /*  9. "data"                                */
    0,                          /* 10. length of the data block              *
                                 *     (sampels*channels*bits_sample/8)      */
    0, 0,
	/* data section starts here in case of the extensible format */
    0x61746164,                 /* 15. "data"                                */
    0
};

typedef struct
{
   char *name;

   int fd;
   int mode;
   int capturing;

   int frequency;
   enum aaxFormat format;
   uint16_t blocksize;
   uint8_t no_tracks;
   uint8_t bits_sample;

   union
   {
      struct
      {
         uint32_t *header;
         size_t size_bytes;
         float update_dt;
         int16_t format;
      } write;

      struct 
      {
         void *blockbuf;
         unsigned int blockstart_smp;
         int16_t predictor[_AAX_MAX_SPEAKERS];
         uint8_t index[_AAX_MAX_SPEAKERS];
      } read;
   } io;

} _handle_t;

static int _aaxFileDriverReadHeader(_handle_t *);
static int _aaxFileDriverUpdateHeader(_handle_t *);
static unsigned int getFileFormatFromFormat(enum aaxFormat);
static int _aaxWavFileReadIMA4(void*, int16_t *, unsigned int);


static int
_aaxWavFileDetect(int mode) {
   return AAX_TRUE;
}

static int
_aaxWavFileOpen(void *id, const char* fname)
{
   _handle_t *handle = (_handle_t *)id;
   int res = -1;

   if (handle && fname)
   {
      handle->fd = open(fname, handle->mode, 0644);
      if (handle->fd >= 0)
      {
         handle->name = _aax_strdup(fname);
         if (!handle->capturing)
         {
            unsigned int size = 4*WAVE_EXT_HEADER_SIZE;
            handle->io.write.header = malloc(size);
            if (handle->io.write.header)
            {
               memcpy(handle->io.write.header, _aaxDefaultWaveHeader, size);
               if (is_bigendian())
               {
                  int i;
                  for (i=0; i<WAVE_EXT_HEADER_SIZE; i++)
                  {
                     uint32_t tmp = _bswap32(handle->io.write.header[i]);
                     handle->io.write.header[i] = tmp;
                  }
               }
               _aaxFileDriverUpdateHeader(handle);
               res = write(handle->fd, handle->io.write.header, size);
            }
            else {
               _AAX_FILEDRVLOG("WAVFile: Insufficient memory");
            }
         }
         else
         {
            /*
             * read the file information and set the file-pointer to
             * the start of the data section
             */
            res = _aaxFileDriverReadHeader(handle);
         }
      }
      else {
         _AAX_FILEDRVLOG("WAVFile: file not found");
      }
   }
   else
   {
      if (!fname) {
         _AAX_FILEDRVLOG("WAVFile: No filename prvided");
      } else {
         _AAX_FILEDRVLOG("WAVFile: Internal error: handle id equals 0");
      }
   }

   return (res >= 0) ? AAX_TRUE : AAX_FALSE;
}

static int
_aaxWavFileClose(void *id)
{
   _handle_t *handle = (_handle_t *)id;
   int ret = AAX_TRUE;

   if (handle)
   {
      if (handle->capturing) {
         free(handle->io.read.blockbuf);
      }
      else
      {
         ret = _aaxFileDriverUpdateHeader(handle);
         free(handle->io.write.header);
      }
      close(handle->fd);
      free(handle->name);
      free(handle);
   }

   return ret;
}

static void*
_aaxWavFileSetup(int mode, int freq, int tracks, int format)
{
   int bits_sample = aaxGetBitsPerSample(format);
   _handle_t *handle = NULL;

   if (bits_sample)
   {
      handle = calloc(1, sizeof(_handle_t));
      if (handle)
      {
         static const int _mode[] = {
            O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,
            O_RDONLY|O_BINARY
         };
         handle->capturing = (mode > 0) ? 0 : 1;
         handle->mode = _mode[handle->capturing];
         handle->bits_sample = bits_sample;
         handle->blocksize = 1;
         handle->frequency = freq;
         handle->no_tracks = tracks;
         handle->format = format;
         if (!handle->capturing) {
            handle->io.write.format = getFileFormatFromFormat(format);
         }
      }
      else {
         _AAX_FILEDRVLOG("WAVFile: Insufficient memory");
      }
   }
   else {
      _AAX_FILEDRVLOG("WAVFile: Unsupported format");
   }

   return (void*)handle;
}

static int
_aaxWavFileReadWrite(void *id, void *data, unsigned int no_samples)
{
   _handle_t *handle = (_handle_t *)id;
   unsigned int no_tracks = handle->no_tracks;
   size_t bufsize = (no_tracks * no_samples * handle->bits_sample)/8;
   int rv = 0;

   if (!handle->capturing)
   {
      rv = write(handle->fd, data, bufsize);
      if (rv > 0)
      {				/* update the file header once a second */
         handle->io.write.update_dt += (float)no_samples/handle->frequency;
         if (handle->io.write.update_dt >= 1.0f)
         {
            _aaxFileDriverUpdateHeader(handle);
            handle->io.write.update_dt -= 1.0f;
         }
         handle->io.write.size_bytes += rv;
      }
      else {
          _AAX_FILEDRVLOG("WAVFile: error writing data");
      }
   }
   else		/* capturing */
   {
      if (handle->format == AAX_IMA4_ADPCM) {
         rv = _aaxWavFileReadIMA4(handle, data, no_samples);
      } else {
         rv = read(handle->fd, data, bufsize);
      }
   }

   if (rv <= 0) {
      rv = -1;
   }

   return rv;
}

static int
_aaxWavFileExtension(char *ext) {
   return !strcasecmp(ext, "wav");
}

static char*
_aaxWavFileInterfaces(int mode)
{
   static const char *rd[2] = {
    "*.wav\0",
    "*.wav\0"
   };
   return (char *)rd[mode];
}

static unsigned int
_aaxWavFileGetFormat(void *id)
{
   _handle_t *handle = (_handle_t *)id;
   return handle->format;
}

static unsigned int
_aaxWavFileGetNoTracks(void *id)
{
   _handle_t *handle = (_handle_t *)id;
   return handle->no_tracks;
}

static unsigned int
_aaxWavFileGetFrequency(void *id)
{
   _handle_t *handle = (_handle_t *)id;
   return handle->frequency;
}

static unsigned int
_aaxWavFileGetBitsPerSample(void *id)
{
   _handle_t *handle = (_handle_t *)id;
   return handle->bits_sample;
}

int
_aaxFileDriverReadHeader(_handle_t *handle)
{
   uint32_t header[WAVE_EXT_HEADER_SIZE];
   size_t bufsize;
   char buf[4];
   int fmt;
   int res;

   lseek(handle->fd, 0L, SEEK_SET);
   res = read(handle->fd, &header, WAVE_EXT_HEADER_SIZE*4);
   if (res <= 0) return res;

   if (is_bigendian())
   {
      int i;
      for (i=0; i<WAVE_EXT_HEADER_SIZE; i++) {
         header[i] = _bswap32(header[i]);
      }
   }

   handle->frequency = header[6];
   handle->no_tracks = header[5] >> 16;
   handle->bits_sample = header[8] >> 16;
   handle->blocksize = header[8] & 0xFFFF;

   fmt = header[5] & 0xFFFF;
   handle->format = getFormatFromWAVFileFormat(fmt, handle->bits_sample);
   if (handle->format == AAX_FORMAT_NONE) {
      return -1;
   }

   /* search for the data chunk */
   bufsize = 0;
   if (lseek(handle->fd, 32L, SEEK_SET) > 0)
   {
      unsigned int i = 4096;
      do
      {
         res = read(handle->fd, buf, 1);
         if ((res > 0) && buf[0] == 'd')
         {
            res = read(handle->fd, buf+1, 3);
            if ((res > 0) && 
                (buf[0] == 'd' && buf[1] == 'a' &&
                 buf[2] == 't' && buf[3] == 'a'))
            {
               res = read(handle->fd, &bufsize, 4); /* chunk size */
               if (is_bigendian()) bufsize = _bswap32(bufsize);
               break;
            }
         }
      }
      while ((res > 0) && --i);
      if (!i) res = -1;
   }
   else {
      res = -1;
   }

   return res;
}

static int
_aaxFileDriverUpdateHeader(_handle_t *handle)
{
   int res = 0;

   if (handle->io.write.size_bytes != 0)
   {
      unsigned int size = handle->io.write.size_bytes;
      uint32_t s;
      off_t floc;

      s =  WAVE_HEADER_SIZE*4 - 8 + size;
      handle->io.write.header[1] = s;

      s = (handle->no_tracks << 16) | handle->io.write.format;
      handle->io.write.header[5] = s;

      s = (uint32_t)handle->frequency;
      handle->io.write.header[6] = s;

      s *= handle->no_tracks * handle->bits_sample;
      handle->io.write.header[7] = s;

      s = size;
      handle->io.write.header[10] = s;

      if (is_bigendian())
      {
         handle->io.write.header[1] = _bswap32(handle->io.write.header[1]);
         handle->io.write.header[5] = _bswap32(handle->io.write.header[5]);
         handle->io.write.header[6] = _bswap32(handle->io.write.header[6]);
         handle->io.write.header[7] = _bswap32(handle->io.write.header[7]);
         handle->io.write.header[10] = _bswap32(handle->io.write.header[10]);
      }

      floc = lseek(handle->fd, 0L, SEEK_CUR);
      lseek(handle->fd, 0L, SEEK_SET);
      res = write(handle->fd, handle->io.write.header, WAVE_HEADER_SIZE*4);
      lseek(handle->fd, floc, SEEK_SET);
#if 0
      if (res == -1) {
         _AAX_SYSLOG(strerror(errno));
      }
#endif

#if 0
// printf("Write:\n");
// printf(" 0: %08x\n", handle->io.write.header[0]);
// printf(" 1: %08x\n", handle->io.write.header[1]);
// printf(" 2: %08x\n", handle->io.write.header[2]);
// printf(" 3: %08x\n", handle->io.write.header[3]);
// printf(" 4: %08x\n", handle->io.write.header[4]);
// printf(" 5: %08x\n", handle->io.write.header[5]);
// printf(" 6: %08x\n", handle->io.write.header[6]);
// printf(" 7: %08x\n", handle->io.write.header[7]);
// printf(" 8: %08x\n", handle->io.write.header[8]);
// printf(" 9: %08x\n", handle->io.write.header[9]);
// printf("10: %08x\n", handle->io.write.header[10]);
#endif
   }

   return res;
}

enum aaxFormat
getFormatFromWAVFileFormat(unsigned int format, int  bits_sample)
{
   enum aaxFormat rv = AAX_FORMAT_NONE;
   int big_endian = is_bigendian();
   switch (format)
   {
   case PCM_WAVE_FILE:
      if (bits_sample == 8) rv = AAX_PCM8U;
      else if (bits_sample == 16 && big_endian) rv = AAX_PCM16S_LE;
      else if (bits_sample == 16) rv = AAX_PCM16S;
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
   case IMA4_ADPCM_WAVE_FILE:
      rv = AAX_IMA4_ADPCM;
      break;
   default:
      break;
   }
   return rv;
}

static unsigned int
getFileFormatFromFormat(enum aaxFormat format)
{
   int big_endian = is_bigendian();
   unsigned int rv = UNSUPPORTED;
   switch (format)
   {
   case AAX_PCM8U:
   case AAX_PCM16S_LE:
   case AAX_PCM24S_LE:
   case AAX_PCM32S_LE:
      rv = PCM_WAVE_FILE;
      break;
   case AAX_PCM16S:
   case AAX_PCM24S:
   case AAX_PCM32S:
      if (!big_endian) rv = PCM_WAVE_FILE;
      break;
   case AAX_FLOAT_LE:
   case AAX_DOUBLE_LE:
      rv = FLOAT_WAVE_FILE;
      break;
   case AAX_FLOAT:
   case AAX_DOUBLE:
      if (!big_endian) rv = FLOAT_WAVE_FILE;
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

static int16_t*
_aaxWavFileMSIMADecode(void *id, int16_t *dst, unsigned int no_samples, unsigned int offs)
{
   _handle_t *handle = (_handle_t*)id;
   int32_t *src = handle->io.read.blockbuf;
   unsigned t, tracks = handle->no_tracks;
   int16_t *d = dst;

   if (!no_samples) return dst;

   for (t=0; t<tracks; t++)
   {
      int16_t predictor = handle->io.read.predictor[t];
      uint8_t index = handle->io.read.index[t];
      unsigned int offs_smp = offs;
      unsigned int l, ctr = 4;
      uint8_t *sptr;
      int32_t *s;

      d = dst+t;
      s = src+t;
      l = no_samples;

      if (!offs_smp)
      {
         sptr = (uint8_t*)s;			/* read the block header */
         predictor = *sptr++;
         predictor |= *sptr++ << 8;
         index = *sptr;

         s += tracks;				/* skip the header      */
         sptr = (uint8_t*)s;
      }
      else
      {
         /* 8 samples per chunk of 4 bytes (int32_t) */
         unsigned int offs_chunks = offs_smp/8;
         int offs_bytes;

         s += tracks;				/* skip the header      */
         s += tracks*offs_chunks;		/* skip the data chunks */
         sptr = (uint8_t*)s;

         offs_smp -= offs_chunks*8;
         offs_bytes = offs_smp/2;		/* two samples per byte */

         sptr += offs_bytes;			/* add remaining offset */
         ctr -= offs_bytes;

         offs_smp -= offs_bytes*2;		/* skip two-samples (bytes) */
         offs_bytes = offs_smp/2;

         sptr += offs_bytes;
         if (offs_smp)				/* offset is an odd number */
         {
            uint8_t nibble = *sptr++;
            *d = ima2linear(nibble >> 4, &predictor, &index);
            d += tracks;
            if (--ctr == 0)
             {
                ctr = 4;
                s += tracks;
                sptr = (uint8_t*)s;
             }
         }
      }

      while(l >= 2)			/* decode the (rest of the) blocks  */
      {
         uint8_t nibble = *sptr++;
         *d = ima2linear(nibble & 0xF, &predictor, &index);
         d += tracks;
         *d = ima2linear(nibble >> 4, &predictor, &index);
         d += tracks;
         if (--ctr == 0)
         {
            ctr = 4;
            s += tracks;
            sptr = (uint8_t*)s;
         }
         l -= 2;
      }

      if (l)				/* no. samples was an odd number */
      {
         uint8_t nibble = *sptr;
         *d = ima2linear(nibble & 0xF, &predictor, &index);
         d += tracks;
      }

      handle->io.read.predictor[t] = predictor;
      handle->io.read.index[t] = index;
   }

   return d;
}

static int
_aaxWavFileReadIMA4(void *id, int16_t *dst, unsigned int no_samples)
{
   _handle_t *handle = (_handle_t*)id;
   int rv = 0;

   if (handle->no_tracks > _AAX_MAX_SPEAKERS) {
      return -1;
   }

   if (no_samples)
   {
      unsigned int block_smp, offs_smp;
      unsigned blocksize;
      int tracks, res;

      tracks = handle->no_tracks;
      blocksize = handle->blocksize;
      block_smp = MSBLOCKSIZE_TO_SMP(blocksize, tracks);

      if (handle->io.read.blockbuf == 0) {
         handle->io.read.blockbuf = malloc(blocksize);
      }

      offs_smp = handle->io.read.blockstart_smp;
      if (offs_smp)	/* there is data from a previous run to process */
      {
         unsigned int decode_smp = block_smp - offs_smp;

         if (no_samples < decode_smp) {
            decode_smp = no_samples;
         }

         dst = _aaxWavFileMSIMADecode(handle, dst, decode_smp, offs_smp);
         rv += tracks*decode_smp/2;

         handle->io.read.blockstart_smp += decode_smp;
         if (handle->io.read.blockstart_smp >= block_smp) {
            handle->io.read.blockstart_smp = 0;
         }
         no_samples -= decode_smp;
      }

      if (no_samples)				/* process new blocks */
      {
         unsigned int blocks = no_samples/block_smp;

         no_samples -= blocks*block_smp;
         while (blocks--)
         {
            res = read(handle->fd, handle->io.read.blockbuf, blocksize);
            if (res > 0)
            {
               dst = _aaxWavFileMSIMADecode(handle, dst, block_smp, 0);
               rv += tracks*block_smp/2;
            }
            else
            {
               no_samples = 0;
               rv = res;
               break;
            }
         }

         handle->io.read.blockstart_smp = no_samples;
         if (no_samples)		/* one more block is required */
         {
            res = read(handle->fd, handle->io.read.blockbuf, blocksize);
            if (res > 0)
            {
               _aaxWavFileMSIMADecode(handle, dst, no_samples, 0);
               rv += tracks*no_samples/2;
            }
            else
            {
               handle->io.read.blockstart_smp = 0;
               rv = res;
            }
         }
      }
   }

   return rv;
}
