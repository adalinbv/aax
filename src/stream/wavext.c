/*
 * Copyright 2005-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
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

#include <stdio.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>		/* strcasecmp */
# endif
#endif
#include <assert.h>		/* assert */
#include <errno.h>
#ifdef HAVE_IO_H
# include <io.h>
#endif

#include <base/types.h>

#include <arch.h>
#include <devices.h>
#include <ringbuffer.h>

#include "filetype.h"
#include "audio.h"
#include "software/audio.h"

#if 0
do             
{              
   char *ch = (char*)&header[2]; 
   printf("%8x: %c%c%c%c\n", header[2], ch[0], ch[1], ch[2], ch[3]);
} while(0);    
#endif


static _file_detect_fn _aaxWavDetect;

static _file_new_handle_fn _aaxWavSetup;
static _file_open_fn _aaxWavOpen;
static _file_close_fn _aaxWavClose;
static _file_update_fn _aaxWavUpdate;
static _file_get_name_fn _aaxWavGetName;

static _file_cvt_to_fn _aaxWavCvtToIntl;
static _file_cvt_from_fn _aaxWavCvtFromIntl;
static _file_cvt_fn _aaxWavCvtEndianness;
static _file_cvt_fn _aaxWavCvtToSigned;
static _file_cvt_fn _aaxWavCvtFromSigned;

static _file_default_fname_fn _aaxWavInterfaces;
static _file_extension_fn _aaxWavExtension;

static _file_get_param_fn _aaxWavGetParam;
static _file_set_param_fn _aaxWavSetParam;

_aaxFmtHandle*
_aaxDetectWavFile()
{
   _aaxFmtHandle* rv = calloc(1, sizeof(_aaxFmtHandle));
   if (rv)
   {
      rv->detect = _aaxWavDetect;
      rv->setup = _aaxWavSetup;
      rv->open = _aaxWavOpen;
      rv->close = _aaxWavClose;
      rv->update = _aaxWavUpdate;
      rv->name = _aaxWavGetName;

      rv->cvt_from_intl = _aaxWavCvtFromIntl;
      rv->cvt_to_intl = _aaxWavCvtToIntl;
      rv->cvt_endianness = _aaxWavCvtEndianness;
      rv->cvt_from_signed = _aaxWavCvtFromSigned;
      rv->cvt_to_signed = _aaxWavCvtToSigned;

      rv->supported = _aaxWavExtension;
      rv->interfaces = _aaxWavInterfaces;

      rv->get_param = _aaxWavGetParam;
      rv->set_param = _aaxWavSetParam;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

#define WAVE_FACT_CHUNK_SIZE		3
#define WAVE_HEADER_SIZE        	(3+8)
#define WAVE_EXT_HEADER_SIZE    	(3+20)
#define DEFAULT_OUTPUT_RATE		22050
#define MSBLOCKSIZE_TO_SMP(b, t)	(((b)-4*(t))*2)/(t)
#define SMP_TO_MSBLOCKSIZE(s, t)	(((s)*(t)/2)+4*(t))

#define BSWAP(a)			is_bigendian() ? _bswap32(a) : (a)
#define BSWAPH(a)			is_bigendian() ? _bswap32h(a) : (a)

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
    PCM_WAVE_FILE,		/* 11-14 GUID                                */
    KSDATAFORMAT_SUBTYPE1,
    KSDATAFORMAT_SUBTYPE2,
    KSDATAFORMAT_SUBTYPE3,

    0x74636166,			/* 15. "fact"                                */
    4,				/* 16. chunk size                            */
    0,				/* 17. no. samples per track                 */

    0x61746164,                 /* 18. "data"                                */
    0,				/* 19. chunk size in bytes following thsi    */
};

typedef struct
{
   char *artist;
   char *title;

   int capturing;

   int no_tracks;
   int bits_sample;
   int frequency;
   enum aaxFormat format;
   size_t blocksize;
   size_t max_samples;

   union
   {
      struct
      {
         int16_t format;
         uint32_t no_samples;

         uint32_t *header;
         size_t header_size;
         float update_dt;
      } write;

      struct 
      {
         int16_t format;
         uint32_t no_samples;

         uint32_t last_tag;
         size_t wavBufSize;
         size_t wavBufPos;
         size_t blockbufpos;
         void *wavBuffer;
         void *wavptr;

         int16_t predictor[_AAX_MAX_SPEAKERS];
         uint8_t index[_AAX_MAX_SPEAKERS];
      } read;
   } io;

   _batch_cvt_proc cvt_to_signed;
   _batch_cvt_proc cvt_from_signed;
   _batch_cvt_proc cvt_endianness;
   _batch_cvt_to_intl_proc cvt_to_intl;
   _batch_cvt_from_intl_proc cvt_from_intl;

} _driver_t;

static int _aaxFileDriverReadHeader(_driver_t*, size_t*);
static void* _aaxFileDriverUpdateHeader(_driver_t*, size_t *);
static size_t getFileFormatFromFormat(enum aaxFormat);

size_t _batch_cvt24_adpcm_intl(void*, int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t*);
void _batch_cvt24_alaw_intl(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_mulaw_intl(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);

static int
_aaxWavDetect(int mode) {
   return AAX_TRUE;
}

static void*
_aaxWavOpen(void *id, void *buf, size_t *bufsize, size_t fsize)
{
   _driver_t *handle = (_driver_t *)id;
   void *rv = NULL;

   assert(bufsize);

   if (handle)
   {
      int need_endian_swap, need_sign_swap;

      if (!handle->capturing)
      {
         char extfmt = AAX_FALSE;
         size_t size;

         if (handle->bits_sample > 16) extfmt = AAX_TRUE;
         else if (handle->no_tracks > 2) extfmt = AAX_TRUE;
         else if (handle->bits_sample < 8) extfmt = AAX_TRUE;

         if (extfmt) {
            handle->io.write.header_size = WAVE_EXT_HEADER_SIZE;
         } else {
            handle->io.write.header_size = WAVE_HEADER_SIZE;
         }

         size = 4*handle->io.write.header_size;
         handle->io.write.header = malloc(size);
         if (handle->io.write.header)
         {
            int32_t s;

            if (extfmt)
            {
               memcpy(handle->io.write.header, _aaxDefaultExtWaveHeader, size);

               s = (handle->no_tracks << 16) | EXTENSIBLE_WAVE_FORMAT;
               handle->io.write.header[5] = s;
            }
            else
            {
               memcpy(handle->io.write.header, _aaxDefaultWaveHeader, size);

               s = (handle->no_tracks << 16) | handle->io.write.format;
               handle->io.write.header[5] = s;
            }

            s = (uint32_t)handle->frequency;
            handle->io.write.header[6] = s;

            s *= handle->no_tracks * handle->bits_sample/8;
            handle->io.write.header[7] = s;

            s = (handle->no_tracks * handle->bits_sample/8);
            s |= (handle->bits_sample/8)*8 << 16;
            handle->io.write.header[8] = s;

            if (extfmt)
            {
               s = handle->bits_sample;
               handle->io.write.header[9] = s << 16 | 22;

               s = getMSChannelMask(handle->no_tracks);
               handle->io.write.header[10] = s;

               s = handle->io.write.format;
               handle->io.write.header[11] = s;
            }

            if (is_bigendian())
            {
               size_t i;
               for (i=0; i<handle->io.write.header_size; i++)
               {
                  uint32_t tmp = _bswap32(handle->io.write.header[i]);
                  handle->io.write.header[i] = tmp;
               }

               handle->io.write.header[5] =_bswap32(handle->io.write.header[5]);
               handle->io.write.header[6] =_bswap32(handle->io.write.header[6]);
               handle->io.write.header[7] =_bswap32(handle->io.write.header[7]);
               handle->io.write.header[8] =_bswap32(handle->io.write.header[8]);
               if (extfmt)
               { 
                  handle->io.write.header[9] =
                                           _bswap32(handle->io.write.header[9]);
                  handle->io.write.header[10] =
                                          _bswap32(handle->io.write.header[10]);
                  handle->io.write.header[11] =
                                          _bswap32(handle->io.write.header[11]);
               }
            }
            _aaxFileDriverUpdateHeader(handle, bufsize);

            *bufsize = size;
            rv = handle->io.write.header;
         }
         else {
            _AAX_FILEDRVLOG("WAVFile: Insufficient memory");
         }
      }
      else /* handle->capturing */
      {
         if (!handle->io.read.wavptr)
         {
            char *ptr = 0;

            handle->io.read.wavBufPos = 0;
            handle->io.read.wavBufSize = 16384;
            handle->io.read.wavptr = _aax_malloc(&ptr, handle->io.read.wavBufSize);
            handle->io.read.wavBuffer = ptr;
         }

         if (handle->io.read.wavptr)
         {
            size_t size = *bufsize;
            size_t avail = handle->io.read.wavBufSize-handle->io.read.wavBufPos;
            size_t step;
            int res;

            avail =  _MIN(size, avail);
            if (!avail) return NULL;

            memcpy(handle->io.read.wavBuffer+handle->io.read.wavBufPos,
                   buf, avail);
            handle->io.read.wavBufPos += avail;
            size -= avail;

            /*
             * read the file information and set the file-pointer to
             * the start of the data section
             */
            
            do
            {
               while ((res = _aaxFileDriverReadHeader(handle,&step)) != __F_EOF)
               {
                  memcpy(handle->io.read.wavBuffer,
                         handle->io.read.wavBuffer+step,
                         handle->io.read.wavBufPos-step);
                  handle->io.read.wavBufPos -= step;
                  if (res <= 0) break;
               }

               if (size)	// There's still some data left
               {
                  avail = handle->io.read.wavBufSize-handle->io.read.wavBufPos;
                  if (!avail) break;

                  avail = _MIN(size, avail);

                  memcpy(handle->io.read.wavBuffer+handle->io.read.wavBufPos,
                         buf, avail);
                  handle->io.read.wavBufPos += avail;
                  size -= avail;
               }
            }
            while (res > 0);

            if (res < 0)
            {
               if (res == __F_PROCESS) {
                  return buf;
               }
               else if (size)
               {
                  _AAX_FILEDRVLOG("WAVFile: Incorrect format");
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
      }

      need_endian_swap = AAX_FALSE;
      if ( ((handle->format & AAX_FORMAT_LE) && is_bigendian()) ||
           ((handle->format & AAX_FORMAT_BE) && !is_bigendian()) )
      {
         need_endian_swap = AAX_TRUE;
      }

      need_sign_swap = AAX_FALSE;
      if (handle->format & AAX_FORMAT_UNSIGNED) {
         need_sign_swap = AAX_TRUE;
      }

      switch (handle->format & AAX_FORMAT_NATIVE)
      {
       case AAX_PCM8S:
         handle->cvt_to_intl = _batch_cvt8_intl_24;
         handle->cvt_from_intl = _batch_cvt24_8_intl;
         if (need_sign_swap)
         {
            handle->cvt_to_signed = _batch_cvt8u_8s;
            handle->cvt_from_signed = _batch_cvt8s_8u;
         }
         break;
      case AAX_PCM16S:
         handle->cvt_to_intl = _batch_cvt16_intl_24;
         handle->cvt_from_intl = _batch_cvt24_16_intl;
         if (need_endian_swap) {
            handle->cvt_endianness = _batch_endianswap16;
         }
         if (need_sign_swap)
         {
            handle->cvt_to_signed = _batch_cvt16u_16s;
            handle->cvt_from_signed = _batch_cvt16s_16u;
         }
         break;
      case AAX_PCM24S:
         handle->cvt_to_intl = _batch_cvt24_3intl_24;
         handle->cvt_from_intl = _batch_cvt24_24_3intl;
         if (need_endian_swap) {
            handle->cvt_endianness = _batch_endianswap32;
         }
         if (need_sign_swap)
         {
            handle->cvt_to_signed = _batch_cvt32u_32s;
            handle->cvt_from_signed = _batch_cvt32s_32u;
         }
         break;
      case AAX_PCM32S:
         handle->cvt_to_intl = _batch_cvt32_intl_24;
         handle->cvt_from_intl = _batch_cvt24_32_intl;
         if (need_sign_swap)
         {
            handle->cvt_to_signed = _batch_cvt32u_32s;
            handle->cvt_from_signed = _batch_cvt32s_32u;
         }
         if (need_endian_swap) {
            handle->cvt_endianness = _batch_endianswap32;
         }
         break;
      case AAX_FLOAT:
         handle->cvt_to_intl = _batch_cvtps_intl_24;
         handle->cvt_from_intl = _batch_cvt24_ps_intl;
         if (need_endian_swap) {
            handle->cvt_endianness = _batch_endianswap32;
         }
         break;
      case AAX_DOUBLE:
         handle->cvt_to_intl = _batch_cvtpd_intl_24;
         handle->cvt_from_intl = _batch_cvt24_pd_intl;
         if (need_endian_swap) {
            handle->cvt_endianness = _batch_endianswap64;
         }
         break;
      case AAX_ALAW:
         handle->cvt_from_intl = _batch_cvt24_alaw_intl;
         break;
      case AAX_MULAW:
         handle->cvt_from_intl = _batch_cvt24_mulaw_intl;
         break;
      case AAX_IMA4_ADPCM:
         break;
      default:
         _AAX_FILEDRVLOG("File: Unsupported format");
         break;
      }
   }
   else
   {
      _AAX_FILEDRVLOG("WAVFile: Internal error: handle id equals 0");
   }

   return rv;
}

static int
_aaxWavClose(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int res = AAX_TRUE;

   if (handle)
   {
      if (handle->capturing) {
         free(handle->io.read.wavptr);
      }
      else {
         free(handle->io.write.header);
      }
      free(handle->artist);
      free(handle->title);
      free(handle);
   }

   return res;
}

static void*
_aaxWavSetup(int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
{
   int bits_sample = aaxGetBitsPerSample(format);
   _driver_t *handle = NULL;

   if (bits_sample)
   {
      handle = calloc(1, sizeof(_driver_t));
      if (handle)
      {
         handle->capturing = (mode > 0) ? 0 : 1;
         handle->bits_sample = bits_sample;
         handle->blocksize = 0;
         handle->frequency = freq;
         handle->no_tracks = tracks;
         handle->format = format;
         handle->max_samples = UINT_MAX;

         if (!handle->capturing)
         {
            handle->io.write.format = getFileFormatFromFormat(format);
            *bufsize = 0;
         }
         else
         {
            handle->max_samples = UINT_MAX;
            handle->io.read.no_samples = UINT_MAX;
            *bufsize = 2*WAVE_EXT_HEADER_SIZE*sizeof(int32_t);
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

static void*
_aaxWavUpdate(void *id, size_t *offs, size_t *size, char close)
{
   _driver_t *handle = (_driver_t *)id;
   void *rv = NULL;

   *offs = 0;
   *size = 0;
   if (!handle->capturing)
   {
      if ((handle->io.write.update_dt >= 1.0f) || close)
      {
         handle->io.write.update_dt -= 1.0f;      
         rv = _aaxFileDriverUpdateHeader(handle, size);
      }
   }

   return rv;
}

static size_t
_aaxWavCvtFromIntl(void *id, int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   _driver_t *handle = (_driver_t *)id;
   size_t bufsize, bytes = 0;
   unsigned char *buf;
   size_t rv = __F_EOF;
   int bits;

   buf = (unsigned char*)handle->io.read.wavBuffer;
   bits = handle->bits_sample;
   if (sptr)
   {
      bufsize = handle->io.read.wavBufSize - handle->io.read.wavBufPos;
      bytes = _MIN(num*tracks*bits/8, bufsize);

      memcpy(handle->io.read.wavBuffer+handle->io.read.wavBufPos, sptr, bytes);
      handle->io.read.wavBufPos += bytes;

      rv = __F_PROCESS;
   }
   else
   {
      if (handle->io.read.no_samples < num) {
         num = handle->io.read.no_samples;
      }

      bufsize = handle->io.read.wavBufPos*8/(tracks*bits);
      if (num > bufsize) {
         num = bufsize;
      }
      if (handle->format == AAX_IMA4_ADPCM) {
         bytes = _batch_cvt24_adpcm_intl(id, dptr, buf, offset, tracks, &num);
         rv = num;
         num /= tracks;
      }
      else
      {
         if (handle->cvt_from_intl)
         {
            handle->cvt_from_intl(dptr, buf, offset, tracks, num);
            bytes = num*tracks*bits/8;
            rv = num;
         }
      }

      if (bytes > 0)
      {
         memcpy(handle->io.read.wavBuffer, handle->io.read.wavBuffer+bytes,
                handle->io.read.wavBufPos - bytes);
         handle->io.read.wavBufPos -= bytes;
      }

      if (handle->io.read.no_samples >= num) {
         handle->io.read.no_samples -= num;
      } else {
         handle->io.read.no_samples = 0;
      }
   }

   return rv;
}

static size_t
_aaxWavCvtToIntl(void *id, void_ptr dptr, const_int32_ptrptr sptr, size_t offset, unsigned int tracks, size_t num, void *scratch, size_t scratchlen)
{
   _driver_t *handle = (_driver_t *)id;
   size_t rv = 0;
   if (handle->cvt_to_intl)
   {
      size_t bytes = (num*tracks*handle->bits_sample)/8;

      handle->cvt_to_intl(dptr, sptr, offset, tracks, num);

      handle->io.write.update_dt += (float)num/handle->frequency;
      handle->io.write.no_samples += num;

      rv = bytes;
   }
   return rv;
}

static void
_aaxWavCvtEndianness(void *id, void_ptr dptr, size_t num)
{
   _driver_t *handle = (_driver_t *)id;
   if (dptr && handle->cvt_endianness) {
      handle->cvt_endianness(dptr, num);
   }
}

static void
_aaxWavCvtToSigned(void *id, void_ptr dptr, size_t num)
{
   _driver_t *handle = (_driver_t *)id;
   if (dptr && handle->cvt_to_signed) {
      handle->cvt_to_signed(dptr, num);
   }
}

static void
_aaxWavCvtFromSigned(void *id, void_ptr dptr, size_t num)
{
   _driver_t *handle = (_driver_t *)id;
   if (dptr && handle->cvt_from_signed) {
      handle->cvt_from_signed(dptr, num);
   }
}

static int
_aaxWavExtension(char *ext) {
   return (ext && !strcasecmp(ext, "wav")) ? 1 : 0;
}

static char*
_aaxWavInterfaces(int mode)
{
   static const char *rd[2] = { "*.wav\0", "*.wav\0" };
   return (char *)rd[mode];
}

static char*
_aaxWavGetName(void *id, enum _aaxFileParam param)
{
   _driver_t *handle = (_driver_t *)id;
   char *rv = NULL;

   switch(param)
   {
   case __F_ARTIST:
      rv = handle->artist;
      break;
   case __F_TITLE:
      rv = handle->title;
      break;
   default:
      break;
   }
   return rv;
}

static off_t
_aaxWavGetParam(void *id, int type)
{
   _driver_t *handle = (_driver_t *)id;
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
   default:
      break;
   }
   return rv;
}

static  off_t
_aaxWavSetParam(void *id, int type, off_t value)
{
   _driver_t *handle = (_driver_t *)id;
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

// http://wiki.audacityteam.org/wiki/WAV
int
_aaxFileDriverReadHeader(_driver_t *handle, size_t *step)
{
   uint32_t *header = handle->io.read.wavBuffer;
   size_t size, bufsize = handle->io.read.wavBufPos;
   int fmt, bits, res = -1;
   int32_t curr, init_tag;
   char extfmt;

   *step = 0;

   init_tag = curr = handle->io.read.last_tag;
   if (curr == 0) {
      curr = BSWAP(header[0]);
   }
   handle->io.read.last_tag = 0;

   /* Is it a RIFF file? */
   if (curr == 0x46464952)		/* RIFF */
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

      if (header[2] == 0x45564157 &&		/* WAVE */
          header[3] == 0x20746d66) 		/* fmt  */
      {
         handle->frequency = header[6];
         handle->no_tracks = header[5] >> 16;
         handle->bits_sample = extfmt ? (header[9] >> 16) : (header[8] >> 16);

         if ((handle->bits_sample >= 4 && handle->bits_sample <= 64) &&
             (handle->frequency >= 4000 && handle->frequency <= 256000) &&
             (handle->no_tracks >= 1 && handle->no_tracks <= _AAX_MAX_SPEAKERS))
         {
            handle->io.read.format = extfmt ? (header[11]) : (header[5] & 0xFFFF);
            handle->blocksize = header[8] & 0xFFFF;
            if (handle->blocksize == (handle->no_tracks*handle->bits_sample/8)) {
               handle->blocksize = 0;
            }

            bits = handle->bits_sample;
            fmt = handle->io.read.format;
            handle->format = getFormatFromWAVFileFormat(fmt, bits);
            switch(handle->format)
            {
            case AAX_FORMAT_NONE:
               return __F_EOF;
               break;
            case AAX_IMA4_ADPCM:
               break;
            default: 
               break;
            }
         }
      }
      else {
         return -1;
      }
   }
   else if (curr == 0x5453494c)		/* LIST */
   {				// http://www.daubnet.com/en/file-format-riff
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
      if (init_tag || BSWAP(header[2]) == 0x4f464e49)	/* INFO */
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
            curr = BSWAP(header[0]);
            switch(curr)
            {
            case 0x54524149:	/* IART: Artist              */
               curr = BSWAP(header[1]);
               size -= 2*sizeof(int32_t) + curr;
               if (size < 0) break;

               handle->artist = malloc(curr);
               if (handle->artist) {
                  memcpy(handle->artist, (char*)&header[2], curr);
               }

               *step += 2*sizeof(int32_t) + curr;
               header = (uint32_t*)((char*)header + 2*sizeof(int32_t) + curr);
               break;
            case 0x4d414e49:    /* INAM: Track Title         */
               curr = BSWAP(header[1]);
               size -= 2*sizeof(int32_t) + curr;
               if (size < 0) break;

               handle->title = malloc(curr);
               if (handle->title) {
                  memcpy(handle->title, (char*)&header[2], curr);
               }

               *step += 2*sizeof(int32_t) + curr;
               header = (uint32_t*)((char*)header + 2*sizeof(int32_t) + curr);
               break;
            case 0x44525049: 	/* IPRD: Album Title/Product */
            case 0x4b525449:	/* ITRK: Track Number        */
            case 0x44524349:	/* ICRD: Date Created        */
            case 0x524e4749:	/* IGNR: Genre               */
            case 0x504f4349:	/* ICOP: Copyright           */
            case 0x54465349:	/* ISFT: Software            */
            case 0x544d4349:	/* ICMT: Comments            */

               curr = BSWAP(header[1]);
               size -= 2*sizeof(int32_t) + curr;
               if (size < 0) break;

               *step += 2*sizeof(int32_t) + curr;
               header = (uint32_t*)((char*)header + 2*sizeof(int32_t) + curr);
               break;
            default:		// we're done
               size = 0;
               *step -= sizeof(int32_t);
               if (curr == 0x61746164) {	 /* data */
                  
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
   else if (curr == 0x74636166)		/* fact */
   {
      curr = BSWAP(header[2]);
      handle->io.read.no_samples = curr;
      handle->max_samples = curr;
      *step = res = 3*sizeof(int32_t);
   }
   else if (curr == 0x61746164)		/* data */
   {
      curr = (8*header[1])/(handle->no_tracks*handle->bits_sample);
      handle->io.read.no_samples = curr;
      handle->max_samples = curr;
      *step = res = 2*sizeof(int32_t);
   }

   return res;
}

static void*
_aaxFileDriverUpdateHeader(_driver_t *handle, size_t *bufsize)
{
   void *res = NULL;

   if (handle->io.write.no_samples != 0)
   {
      char extfmt = (handle->io.write.header_size == WAVE_HEADER_SIZE) ? 0 : 1;
      size_t size;
      uint32_t s;

      size = (handle->io.write.no_samples*handle->no_tracks*handle->bits_sample)/8;
      s =  4*handle->io.write.header_size + size - 8;
      handle->io.write.header[1] = s;

      s = size;
      if (extfmt)
      {
         handle->io.write.header[17] = handle->io.write.no_samples;
         handle->io.write.header[19] = s;
      }
      else {
         handle->io.write.header[10] = s;
      }

      if (is_bigendian())
      {
         handle->io.write.header[1] = _bswap32(handle->io.write.header[1]);
         if (extfmt)
         {
            handle->io.write.header[17] = _bswap32(handle->io.write.header[17]);
            handle->io.write.header[19] = _bswap32(handle->io.write.header[19]);
         }
         else {
            handle->io.write.header[10] = _bswap32(handle->io.write.header[10]);
         }
      }

      *bufsize = 4*handle->io.write.header_size;
      res = handle->io.write.header;

#if 0
   printf("Write %s Header:\n", extfmt ? "Extnesible" : "Canonical");
   printf(" 0: %08x (ChunkID \"RIFF\")\n", handle->io.write.header[0]);
   printf(" 1: %08x (ChunkSize: %i)\n", handle->io.write.header[1], handle->io.write.header[1]);
   printf(" 2: %08x (Format \"WAVE\")\n", handle->io.write.header[2]);
   printf(" 3: %08x (Subchunk1ID \"fmt \")\n", handle->io.write.header[3]);
   printf(" 4: %08x (Subchunk1Size): %i\n", handle->io.write.header[4], handle->io.write.header[4]);
   printf(" 5: %08x (NumChannels: %i | AudioFormat: %x)\n", handle->io.write.header[5], handle->io.write.header[5] >> 16, handle->io.write.header[5] & 0xFFFF);
   printf(" 6: %08x (SampleRate: %i)\n", handle->io.write.header[6], handle->io.write.header[6]);
   printf(" 7: %08x (ByteRate: %i)\n", handle->io.write.header[7], handle->io.write.header[7]);
   printf(" 8: %08x (BitsPerSample: %i | BlockAlign: %i)\n", handle->io.write.header[8], handle->io.write.header[8] >> 16, handle->io.write.header[8] & 0xFFFF);
   if (!extfmt)
   {
      printf(" 9: %08x (SubChunk2ID \"data\")\n", handle->io.write.header[9]);
      printf("10: %08x (Subchunk2Size: %i)\n", handle->io.write.header[10], handle->io.write.header[10]);
   }
   else
   {
      printf(" 9: %08x (size: %i, nValidBits: %i)\n", handle->io.write.header[9], handle->io.write.header[9] >> 16, handle->io.write.header[9] & 0xFFFF);
      printf("10: %08x (dwChannelMask: %i)\n", handle->io.write.header[10], handle->io.write.header[10]);
      printf("11: %08x (GUID0)\n", handle->io.write.header[11]);
      printf("12: %08x (GUID1)\n", handle->io.write.header[12]);
      printf("13: %08x (GUID2)\n", handle->io.write.header[13]);
      printf("14: %08x (GUID3)\n", handle->io.write.header[14]);

      printf("15: %08x (SubChunk2ID \"fact\")\n", handle->io.write.header[15]);
      printf("16: %08x (Subchunk2Size: %i)\n", handle->io.write.header[16], handle->io.write.header[16]);
      printf("17: %08x (NumSamplesPerChannel: %i)\n", handle->io.write.header[17], handle->io.write.header[17]);

      printf("18: %08x (SubChunk3ID \"data\")\n", handle->io.write.header[18]);
      printf("19: %08x (Subchunk3Size: %i)\n", handle->io.write.header[19], handle->io.write.header[19]);
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

enum aaxFormat
getFormatFromWAVFileFormat(unsigned int format, int bits_sample)
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
   default:
      break;
   }
   return rv;
}

static size_t
getFileFormatFromFormat(enum aaxFormat format)
{
   size_t rv = UNSUPPORTED;
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

void *
_aaxMSADPCM_IMA4(void *data, size_t bufsize, int tracks, size_t *size)
{
   size_t blocksize = *size;
   *size /= tracks;
   if (tracks > 1)
   {
      int32_t *buf = (int32_t*)malloc(blocksize);
      if (buf)
      {
         int32_t* dptr = (int32_t*)data;
         size_t numBlocks, numChunks;
         size_t blockNum;

         numBlocks = bufsize/blocksize;
         numChunks = blocksize/4;

         for (blockNum=0; blockNum<numBlocks; blockNum++)
         {
            int t, i;

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
   return data;
}

static size_t
_aaxWavMSADPCMBlockDecode(void *id, int32_t **dptr, size_t smp_offs, size_t num, size_t offset)
{
   _driver_t *handle = (_driver_t*)id;
   size_t offs_smp, rv = 0;

   if (num)
   {
      int32_t *src = handle->io.read.wavBuffer;
      unsigned t, tracks = handle->no_tracks;

      for (t=0; t<tracks; t++)
      {
         int16_t predictor = handle->io.read.predictor[t];
         uint8_t index = handle->io.read.index[t];
         size_t l, ctr = 4;
         int32_t *d = dptr[t]+offset;
         int32_t *s = src+t;
         uint8_t *sptr;

         l = num;
         offs_smp = smp_offs;
         if (!offs_smp)
         {
            sptr = (uint8_t*)s;			/* read the block header */
            predictor = *sptr++;
            predictor |= *sptr++ << 8;
            index = *sptr;

            s += tracks;			/* skip the header      */
            sptr = (uint8_t*)s;
         }
         else
         {
            /* 8 samples per chunk of 4 bytes (int32_t) */
            size_t offs_chunks = offs_smp/8;
            size_t offs_bytes;

            s += tracks;			/* skip the header      */
            s += tracks*offs_chunks;		/* skip the data chunks */
            sptr = (uint8_t*)s;

            offs_smp -= offs_chunks*8;
            offs_bytes = offs_smp/2;		/* two samples per byte */

            sptr += offs_bytes;			/* add remaining offset */
            ctr -= offs_bytes;

            offs_smp -= offs_bytes*2;		/* skip two-samples (bytes) */
            offs_bytes = offs_smp/2;

            sptr += offs_bytes;
            if (offs_smp)			/* offset is an odd number */
            {
               uint8_t nibble = *sptr++;
               *d++ = _adpcm2linear(nibble >> 4, &predictor, &index) << 8;
               if (--ctr == 0)
               {
                  ctr = 4;
                  s += tracks;
                  sptr = (uint8_t*)s;
               }
            }
         }

         if (l >= 2)			/* decode the (rest of the) blocks  */
         {
            do
            {
               uint8_t nibble = *sptr++;
               *d++ = _adpcm2linear(nibble & 0xF, &predictor, &index) << 8;
               *d++ = _adpcm2linear(nibble >> 4, &predictor, &index) << 8;
               if (--ctr == 0)
               {
                  ctr = 4;
                  s += tracks;
                  sptr = (uint8_t*)s;
               }
               l -= 2;
            }
            while (l >= 2);
         }

         if (l)				/* no. samples was an odd number */
         {
            uint8_t nibble = *sptr;
            *d++ = _adpcm2linear(nibble & 0xF, &predictor, &index) << 8;
            l--;
         }

         handle->io.read.predictor[t] = predictor;
         handle->io.read.index[t] = index;

         rv = num-l;
      }
   }

   return rv;
}

size_t
_batch_cvt24_adpcm_intl(void *id, int32_ptrptr dptr, const_void_ptr sptr, size_t offs, unsigned int tracks, size_t *n)
{
   _driver_t *handle = (_driver_t*)id;
   size_t num = *n;
   size_t rv = 0;

   if (handle->io.read.wavBufPos >= handle->blocksize)
   {
      size_t block_smp, decode_smp, offs_smp;
      size_t r;

      offs_smp = handle->io.read.blockbufpos;
      block_smp = MSBLOCKSIZE_TO_SMP(handle->blocksize, handle->no_tracks);
      decode_smp = _MIN(block_smp-offs_smp, num);

      r = _aaxWavMSADPCMBlockDecode(handle, dptr, offs_smp, decode_smp, offs);
      handle->io.read.blockbufpos += r;
      *n = r;

      if (handle->io.read.blockbufpos >= block_smp)
      {
         handle->io.read.blockbufpos = 0;
         rv = handle->blocksize;
      }
   }
   else {
      *n = rv = 0;
   }

   return rv;
}

void
_batch_cvt24_mulaw_intl(int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      size_t t;
      for (t=0; t<tracks; t++)
      {
         int8_t *s = (int8_t *)sptr + t;
         int32_t *d = dptr[t] + offset;
         size_t i = num;

         do
         {
            *d++ = _mulaw2linear(*s) << 8;
            s += tracks;
         }
         while (--i);
      }
   }
}

void
_batch_cvt24_alaw_intl(int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      size_t t;
      for (t=0; t<tracks; t++)
      {
         int8_t *s = (int8_t *)sptr + t;
         int32_t *d = dptr[t] + offset;
         size_t i = num;

         do
         {
            *d++ = _alaw2linear(*s) << 8;
            s += tracks;
         }
         while (--i);
      }
   }
}


#if 1
/**
 * Write a canonical WAVE file from memory to a file.
 *
 * @param a pointer to the exact ascii file location
 * @param no_samples number of samples per audio track
 * @param fs sample frequency of the audio tracks
 * @param no_tracks number of audio tracks in the buffer
 * @param format audio format
 */
#include <fcntl.h>		/* SEEK_*, O_* */
void
_aaxFileDriverWrite(const char *file, enum aaxProcessingType type,
                          void *data, size_t no_samples,
                          size_t freq, char no_tracks,
                          enum aaxFormat format)
{
   _driver_t *handle;
   size_t size;
   int fd, oflag;
   int res, mode;
   char *buf;

   mode = AAX_MODE_WRITE_STEREO;
   handle = _aaxWavSetup(mode, &size, freq, no_tracks, format, no_samples, 0);
   if (!handle)
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

   buf = _aaxWavOpen(handle, NULL, &size, 0);

   res = write(fd, buf, size);
   if (res == -1) {
      _AAX_FILEDRVLOG(strerror(errno));
   }

   if (handle->cvt_from_signed) {
      handle->cvt_from_signed(data, no_tracks*no_samples);
   }
   if (handle->cvt_endianness) {
      handle->cvt_endianness(data, no_tracks*no_samples);
   }

   size = no_samples * no_tracks * handle->bits_sample/8;
   res = write(fd, data, size);
   if (res >= 0)
   {
      size_t offs;

      handle->io.write.no_samples = no_samples * no_tracks;
      buf = _aaxWavUpdate(handle, &offs, &size, AAX_TRUE);
      if (buf)
      {
         lseek(fd, offs, SEEK_SET);
         res = write(fd, buf, size);
      }
   }
   else {
      _AAX_FILEDRVLOG(strerror(errno));
   }

   close(fd);
   _aaxWavClose(handle);
}
#endif

