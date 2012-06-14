/*
 * Copyright 2005-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
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

#include <stdlib.h>		/* for malloc */
#include <string.h>		/* for strdup */
#include <fcntl.h>		/* SEEK_*, O_* */
#include <assert.h>		/* assert */
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif

#include <base/types.h>
#include <devices.h>

#include "filetype.h"

static _connect_fn _aaxWavFileOpen;
static _disconnect_fn _aaxWavFileClose;
static _update_fn _aaxWavFileReadWrite;
static _get_extension_fn _aaxWavFileExtension;

_aaxFileHandle*
_aaxDetectWavFile()
{
   _aaxFileHandle* rv = malloc(sizeof(_aaxFileHandle));
   if (rv)
   {
      rv->connect = (_connect_fn*)&_aaxWavFileOpen;
      rv->disconnect = (_disconnect_fn*)&_aaxWavFileClose;
      rv->update = (_update_fn*)&_aaxWavFileReadWrite;
      rv->get_extension = (_get_extension_fn*)&_aaxWavFileExtension;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

#define DEFAULT_OUTPUT_RATE     22050
#define WAVE_HEADER_SIZE        11
#define WAVE_EXT_HEADER_SIZE    17
#ifndef O_BINARY
# define O_BINARY               0
#endif

static uint32_t _aaxDefaultWaveHeader[WAVE_EXT_HEADER_SIZE] =
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
    0,0,
/* data section starts here in case of the extensible format */
    0x61746164,                 /* 15. "data"                                */
    0
};

typedef struct
{
   int fd;
   int mode;
   int capture;
   char *name;

   int frequency;
   int no_tracks;
   unsigned int no_frames;
   unsigned bits_sample;
   unsigned blocksize;
   enum aaxFormat format;
   size_t size_bytes;

   uint32_t header[WAVE_EXT_HEADER_SIZE]; 

} _driver_t;

static enum aaxFormat getFormatFromFileFormat(unsigned int, int);
static int _aaxFileDriverUpdateHeader(_driver_t *);
static int _aaxFileDriverReadHeader(_driver_t *);
#ifndef HAVE_STRDUP
char *strdup(const char *);
#endif


static void*
_aaxWavFileOpen(const char* fname, int mode)
{
   _driver_t *handle = malloc(sizeof(_driver_t));

   if (handle)
   {
      static const int _mode[] = {
         O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,
         O_RDONLY|O_BINARY
      };
      int res = 0;

      handle->size_bytes = 0;
      handle->capture = (mode > 0) ? 0 : 1;
      handle->name = strdup(fname);
      handle->mode = _mode[handle->capture];
      handle->fd = open(handle->name, handle->mode, 0644);
      if (handle->fd >= 0)
      {
         if (mode)
         {
            memcpy(handle->header, _aaxDefaultWaveHeader,
                   4*WAVE_EXT_HEADER_SIZE);
            if (is_bigendian())
            {
               int i;
               for (i=0; i<WAVE_EXT_HEADER_SIZE; i++) {
                  handle->header[i] = _bswap32(handle->header[i]);
               }
            }
            _aaxFileDriverUpdateHeader(handle);
            res = write(handle->fd, handle->header, WAVE_HEADER_SIZE*4);
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

      if ((handle->fd < 0) || (res < 0))
      {
         free(handle->name);
         if (handle->fd) {
            close(handle->fd);
         }
         free(handle);
         handle = 0;
      }
   }

   return handle;
}

static int
_aaxWavFileClose(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int ret = AAX_TRUE;

   if (handle)
   {
      free(handle->name);
      if (!handle->capture) {
         ret = _aaxFileDriverUpdateHeader(handle);
      }
      close(handle->fd);
      free(handle);
   }

   return ret;
}

static int
_aaxWavFileReadWrite(void *id, void *data)
{
   _driver_t *handle = (_driver_t *)id;
   int frame_size = (handle->bits_sample * handle->no_tracks)/8;
   size_t buflen = handle->no_frames * frame_size;
   int rv = AAX_FALSE;

   if (handle->mode) {
      rv = write(handle->fd, data, buflen);
   } else {
      rv = read(handle->fd, data, buflen);
   }

   if (rv > 0) {
       handle->size_bytes += rv;
   }

   return (rv > 0) ? rv : AAX_FALSE;
}

static char* 
_aaxWavFileExtension() {
   return "wav";
}


int
_aaxFileDriverReadHeader(_driver_t *handle)
{
   uint32_t header[WAVE_EXT_HEADER_SIZE];
   size_t buflen;
   char buf[4];
   int i, res;

// lseek(handle->fd, 0L, SEEK_SET);
   res = read(handle->fd, &header, WAVE_EXT_HEADER_SIZE*4);
   if (res <= 0) return -1;

   if (is_bigendian())
   {
      for (i=0; i<WAVE_EXT_HEADER_SIZE; i++) {
         header[i] = _bswap32(header[i]);
      }
   }

   handle->frequency = header[6];
   handle->no_tracks = header[5] >> 16;
   handle->bits_sample = header[8] >> 16;
   handle->blocksize = header[8] & 0xFFFF;

   res = header[5] & 0xFFFF;
   i = handle->bits_sample;
   handle->format = getFormatFromFileFormat(res, i);

   /* search for the data chunk */
   buflen = 0;
   if (lseek(handle->fd, 32L, SEEK_SET) > 0)
   {
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
               res = read(handle->fd, &buflen, 4); /* chunk size */
               if (is_bigendian()) buflen = _bswap32(buflen);
               break;
            }
         }
      }
      while (1);
      handle->no_frames = (buflen*8)/(handle->no_tracks*handle->bits_sample);
   }
   else {
      res = -1;
   }

   return res;
}

static int
_aaxFileDriverUpdateHeader(_driver_t *handle)
{
   int res = 0;

   if (handle->size_bytes != 0)
   {
      unsigned int fmt, size = handle->size_bytes;
      uint32_t s;
      off_t floc;

      s =  WAVE_HEADER_SIZE*4 - 8 + size;
      handle->header[1] = s;

      fmt = handle->header[5] & 0xFFF;
      s = (handle->no_tracks << 16) | fmt;	/* PCM */
      handle->header[5] = s;

      s = (uint32_t)handle->frequency;
      handle->header[6] = s;

      s *= handle->no_tracks * handle->bits_sample;
      handle->header[7] = s;

      s = size;
      handle->header[10] = s;

      if (is_bigendian())
      {
         handle->header[1] = _bswap32(handle->header[1]);
         handle->header[5] = _bswap32(handle->header[5]);
         handle->header[6] = _bswap32(handle->header[6]);
         handle->header[7] = _bswap32(handle->header[7]);
         handle->header[10] = _bswap32(handle->header[10]);
      }

      floc = lseek(handle->fd, 0L, SEEK_CUR);
      lseek(handle->fd, 0L, SEEK_SET);
      res = write(handle->fd, handle->header, WAVE_HEADER_SIZE*4);
      lseek(handle->fd, floc, SEEK_SET);
#if 0
      if (res == -1) {
         _AAX_SYSLOG(strerror(errno));
      }
#endif

#if 0
// printf("Write:\n");
// printf(" 0: %08x\n", handle->header[0]);
// printf(" 1: %08x\n", handle->header[1]);
// printf(" 2: %08x\n", handle->header[2]);
// printf(" 3: %08x\n", handle->header[3]);
// printf(" 4: %08x\n", handle->header[4]);
// printf(" 5: %08x\n", handle->header[5]);
// printf(" 6: %08x\n", handle->header[6]);
// printf(" 7: %08x\n", handle->header[7]);
// printf(" 8: %08x\n", handle->header[8]);
// printf(" 9: %08x\n", handle->header[9]);
// printf("10: %08x\n", handle->header[10]);
#endif
   }

   return res;
}

enum aaxFormat
getFormatFromFileFormat(unsigned int format, int  bits_sample)
{
   enum aaxFormat rv = AAX_FORMAT_NONE;
   switch (format)
   {
   case 1:
      if (bits_sample == 8) rv = AAX_PCM8U;
      else if (bits_sample == 16) rv = AAX_PCM16S_LE;
      else if (bits_sample == 32) rv = AAX_PCM32S_LE;
      break;
   case 3:
      if (bits_sample == 32) rv = AAX_FLOAT_LE;
      else if (bits_sample == 64) rv = AAX_DOUBLE_LE;
      break;
   case 6:
      rv = AAX_ALAW;
      break;
   case 7:
      rv = AAX_MULAW;
      break;
   case 17:
      rv = AAX_IMA4_ADPCM;
      break;
   default:
      break;
   }
   return rv;
}

