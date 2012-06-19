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
#include <errno.h>
#if HAVE_STRINGS_H
# include <strings.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif

#include <base/types.h>
#include <devices.h>

#include "filetype.h"

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
static _get_param_fn _aaxWavFileGetFrameSize;
static _get_param_fn _aaxWavFileGetBlockSize;

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
      rv->get_frame_size = (_get_param_fn*)&_aaxWavFileGetFrameSize;
      rv->get_block_size = (_get_param_fn*)&_aaxWavFileGetBlockSize;
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

enum wavFormat
{
   UNSUPPORTED = 0,
   PCM_WAVE = 1,
   FLOAT_WAVE = 3,
   ALAW_WAVE = 6,
   MULAW_WAVE = 7,
   IMA4_ADPCM_WAVE = 17
};

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
   int capturing;
   char *name;

   int frequency;
   int no_tracks;
   int frame_size;
   unsigned int no_frames;
   unsigned bits_sample;
   unsigned blocksize;
   enum aaxFormat format;
   size_t size_bytes;
   float update_dt;

   uint32_t header[WAVE_EXT_HEADER_SIZE]; 

} _driver_t;

static enum aaxFormat getFormatFromFileFormat(unsigned int, int);
static unsigned int getFileFormatFromFormat(enum aaxFormat);
static int _aaxFileDriverUpdateHeader(_driver_t *);
static int _aaxFileDriverReadHeader(_driver_t *);
#ifndef HAVE_STRDUP
char *strdup(const char *);
#endif


static int
_aaxWavFileDetect(int mode) {
   return AAX_TRUE;
}

static int
_aaxWavFileOpen(void *id, const char* fname)
{
   _driver_t *handle = (_driver_t *)id;
   int res = AAX_FALSE;

   if (handle && fname)
   {
      handle->name = strdup(fname);
      handle->fd = open(handle->name, handle->mode, 0644);
      if (handle->fd >= 0)
      {
         if (!handle->capturing)
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
   }

   return (res >= 0) ? AAX_TRUE : AAX_FALSE;
}

static int
_aaxWavFileClose(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int ret = AAX_TRUE;

   if (handle)
   {
      free(handle->name);
      if (!handle->capturing) {
         ret = _aaxFileDriverUpdateHeader(handle);
      }
      close(handle->fd);
      free(handle);
   }

   return ret;
}

static void*
_aaxWavFileSetup(int mode, int freq, int tracks, int format, int blocksz)
{
   _driver_t *handle = NULL;
   int bits_sample = 0;

   switch(format)
   {
   case AAX_PCM8S:
      bits_sample = 8;
      break;
   case AAX_PCM16S:
      bits_sample = 16;
      break;
   default:
      break;
   }

   if (bits_sample)
   {
      handle = calloc(1, sizeof(_driver_t));
      if (handle)
      {
         static const int _mode[] = {
            O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,
            O_RDONLY|O_BINARY
         };
         handle->size_bytes = 0;
         handle->capturing = (mode > 0) ? 0 : 1;
         handle->mode = _mode[handle->capturing];
         handle->bits_sample = bits_sample;
         handle->blocksize = blocksz;
         handle->frequency = freq;
         handle->no_tracks = tracks;
         handle->format = format;
         handle->frame_size = (handle->bits_sample * handle->no_tracks)/8;
      }
   }

   return (void*)handle;
}

static int
_aaxWavFileReadWrite(void *id, void *data, unsigned int no_frames)
{
   _driver_t *handle = (_driver_t *)id;
   size_t buflen = no_frames * handle->frame_size;
   int rv = AAX_FALSE;

   if (!handle->capturing)
   {
      rv = write(handle->fd, data, buflen);
      if (rv >= 0)
      {
         handle->update_dt += (float)no_frames/(float)handle->frequency;
         if (handle->update_dt >= 1.0f)
         {
            _aaxFileDriverUpdateHeader(handle);
            handle->update_dt -= 1.0f;
         }
      }
   } else {
      rv = read(handle->fd, data, buflen);
   }

   if (rv >= 0) {
       handle->size_bytes += rv;
   } else {
      rv = AAX_FALSE;
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
    "/tmp/"AAX_NAME_STR"In.wav\0\0",
    "~/"AAX_NAME_STR"Out.wav\0/tmp/"AAX_NAME_STR"Out.wav\0\0"
   };
   return (char *)rd[mode];
}

static unsigned int
_aaxWavFileGetFormat(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   return handle->format;
}

static unsigned int
_aaxWavFileGetNoTracks(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   return handle->no_tracks;
}

static unsigned int
_aaxWavFileGetFrequency(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   return handle->frequency;
}

static unsigned int
_aaxWavFileGetFrameSize(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   return handle->frame_size;
}

static unsigned int
_aaxWavFileGetBlockSize(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   return handle->blocksize;
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

static enum aaxFormat
getFormatFromFileFormat(unsigned int format, int  bits_sample)
{
   enum aaxFormat rv = AAX_FORMAT_NONE;
   int big_endian = is_bigendian();
   switch (format)
   {
   case PCM_WAVE:
      if (bits_sample == 8) rv = AAX_PCM8U;
      else if (bits_sample == 16 && big_endian) rv = AAX_PCM16S_LE;
      else if (bits_sample == 16) rv = AAX_PCM16S;
      else if (bits_sample == 32 && big_endian) rv = AAX_PCM32S_LE;
      else if (bits_sample == 32) rv = AAX_PCM32S;
      break;
   case FLOAT_WAVE:
      if (bits_sample == 32 && big_endian) rv = AAX_FLOAT_LE;
      else if (bits_sample == 32) rv = AAX_FLOAT;
      else if (bits_sample == 64 && big_endian) rv = AAX_DOUBLE_LE;
      else if (bits_sample == 64) rv = AAX_DOUBLE;
      break;
   case ALAW_WAVE:
      rv = AAX_ALAW;
      break;
   case MULAW_WAVE:
      rv = AAX_MULAW;
      break;
   case IMA4_ADPCM_WAVE:
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
   case AAX_PCM32S_LE:
      rv = PCM_WAVE;
      break;
   case AAX_PCM16S:
   case AAX_PCM32S:
      if (!big_endian) rv = PCM_WAVE;
      break;
   case AAX_FLOAT_LE:
   case AAX_DOUBLE_LE:
      rv = FLOAT_WAVE;
      break;
   case AAX_FLOAT:
   case AAX_DOUBLE:
      if (!big_endian) rv = FLOAT_WAVE;
      break;
   case AAX_ALAW:
      rv = ALAW_WAVE;
      break;
   case AAX_MULAW:
      rv = MULAW_WAVE;
      break;
   case AAX_IMA4_ADPCM:
      rv = IMA4_ADPCM_WAVE;
      break;
   default:
      break;
   }
   return rv;
}
