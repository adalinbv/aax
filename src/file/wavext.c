/*
 * Copyright 2005-2013 by Erik Hofman.
 * Copyright 2009-2013 by Adalin B.V.
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
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>   /* strcasecmp */
# endif
#endif
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

static _detect_fn _aaxWavDetect;

static _new_hanle_fn _aaxWavSetup;
static _open_fn _aaxWavOpen;
static _close_fn _aaxWavClose;
static _update_fn _aaxWavUpdate;

static _cvt_to_fn _aaxWavCvtToIntl;
static _cvt_from_fn _aaxWavCvtFromIntl;
static _cvt_fn _aaxWavCvtEndianness;
static _cvt_fn _aaxWavCvtToSigned;
static _cvt_fn _aaxWavCvtFromSigned;

static _default_fname_fn _aaxWavInterfaces;
static _extension_fn _aaxWavExtension;

static _get_param_fn _aaxWavGetParam;

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

      rv->cvt_from_intl = _aaxWavCvtFromIntl;
      rv->cvt_to_intl = _aaxWavCvtToIntl;
      rv->cvt_endianness = _aaxWavCvtEndianness;
      rv->cvt_from_signed = _aaxWavCvtFromSigned;
      rv->cvt_to_signed = _aaxWavCvtToSigned;

      rv->supported = _aaxWavExtension;
      rv->interfaces = _aaxWavInterfaces;

      rv->get_param = _aaxWavGetParam;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

#define WAVE_HEADER_SIZE        	(3+8)
#define WAVE_EXT_HEADER_SIZE    	(3+14)
#define WAVE_FACT_CHUNK_SIZE		3
#define DEFAULT_OUTPUT_RATE		22050
#define MSBLOCKSIZE_TO_SMP(b, t)	(((b)-4*(t))*2)/(t)
#define SMP_TO_MSBLOCKSIZE(s, t)	(((s)*(t)/2)+4*(t))

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
    0x61746164,                 /* 15. "data"                                */
    0
};

static const uint32_t _aaxDefaultWaveFactCHunck[3] =
{
    0x74636166,			/*  0. "fact"                                */
    4,				/*  1. chunk size                            */
    0				/*  2. no. samples                           */
};

typedef struct
{
   int capturing;

   int frequency;
   enum aaxFormat format;
   int blocksize;
   int no_tracks;
   int bits_sample;

   union
   {
      struct
      {
         uint32_t *header;
         unsigned int header_size;
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
         int16_t format;
      } read;
   } io;

   _batch_cvt_proc cvt_to_signed;
   _batch_cvt_proc cvt_from_signed;
   _batch_cvt_proc cvt_endianness;
   _batch_cvt_to_intl_proc cvt_to_intl;
   _batch_cvt_from_intl_proc cvt_from_intl;

} _driver_t;

static int _aaxFileDriverReadHeader(_driver_t*, void*, unsigned int *);
static void* _aaxFileDriverUpdateHeader(_driver_t*, unsigned int *);
static unsigned int getFileFormatFromFormat(enum aaxFormat);

unsigned int _batch_cvt24_adpcm_intl(void*, int32_ptrptr, const_void_ptr, int, unsigned int, unsigned int);
void _batch_cvt24_alaw_intl(int32_ptrptr, const_void_ptr, int, unsigned int, unsigned int);
void _batch_cvt24_mulaw_intl(int32_ptrptr, const_void_ptr, int, unsigned int, unsigned int);

static int
_aaxWavDetect(int mode) {
   return AAX_TRUE;
}

static void*
_aaxWavOpen(void *id, void *buf, unsigned int *bufsize)
{
   _driver_t *handle = (_driver_t *)id;
   void *rv = NULL;

   assert(bufsize);

   *bufsize = 0;
   if (handle)
   {
      int need_endian_swap, need_sign_swap;

      if (!handle->capturing)
      {
         char extfmt = AAX_FALSE;
         unsigned int size;

         if (handle->bits_sample> 16) extfmt = AAX_TRUE;
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

               s = 0; // getMSChannelMask(handle->no_tracks);
               handle->io.write.header[10] = s;

               s = handle->io.write.format;
               handle->io.write.header[11] = s;
            }

            if (is_bigendian())
            {
               int i;
               for (i=0; i<handle->io.write.header_size; i++)
               {
                  uint32_t tmp = _bswap32(handle->io.write.header[i]);
                  handle->io.write.header[i] = tmp;
               }

               handle->io.write.header[5]=_bswap32(handle->io.write.header[5]);
               handle->io.write.header[6]=_bswap32(handle->io.write.header[6]);
               handle->io.write.header[7]=_bswap32(handle->io.write.header[7]);
               handle->io.write.header[8]=_bswap32(handle->io.write.header[8]);
            }
            _aaxFileDriverUpdateHeader(handle, bufsize);

            *bufsize = size;
            rv = handle->io.write.header;
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
         _aaxFileDriverReadHeader(handle, buf, bufsize);
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
         assert(handle->io.read.blockbuf != NULL);
         handle->io.read.blockstart_smp = -1;
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
         free(handle->io.read.blockbuf);
      }
      else {
         free(handle->io.write.header);
      }
      free(handle);
   }

   return res;
}

static void*
_aaxWavSetup(int mode, unsigned int *bufsize, int freq, int tracks, int format, int no_samples, int bitrate)
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

         if (!handle->capturing)
         {
            handle->io.write.format = getFileFormatFromFormat(format);
            *bufsize = 0;
         }
         else {
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
_aaxWavUpdate(void *id, unsigned int *offs, unsigned int *size, char close)
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

static int
_aaxWavCvtFromIntl(void *id, int32_ptrptr dptr, const_void_ptr sptr, int offset, unsigned int tracks, unsigned int num)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = __F_EOF;

   if (handle->format == AAX_IMA4_ADPCM) {
      rv = _batch_cvt24_adpcm_intl(id, dptr, sptr, offset, tracks, num);
   }
   else if (sptr)
   {
      if (handle->cvt_from_intl)
      {
         handle->cvt_from_intl(dptr, sptr, offset, tracks, num);
         rv = num;
      }
   }
   else {
      rv = 0;
   }

   return rv;
}

static int
_aaxWavCvtToIntl(void *id, void_ptr dptr, const_int32_ptrptr sptr, int offset, unsigned int tracks, unsigned int num, void *scratch)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = 0;
   if (handle->cvt_to_intl)
   {
      unsigned int bytes = (num*tracks*handle->bits_sample)/8;

      handle->cvt_to_intl(dptr, sptr, offset, tracks, num);
      handle->io.write.update_dt += (float)num/handle->frequency;
      handle->io.write.size_bytes += bytes;

      rv = bytes;
   }
   return rv;
}

static void
_aaxWavCvtEndianness(void *id, void_ptr dptr, unsigned int num)
{
   _driver_t *handle = (_driver_t *)id;
   if (dptr && handle->cvt_endianness) {
      handle->cvt_endianness(dptr, num);
   }
}

static void
_aaxWavCvtToSigned(void *id, void_ptr dptr, unsigned int num)
{
   _driver_t *handle = (_driver_t *)id;
   if (dptr && handle->cvt_to_signed) {
      handle->cvt_to_signed(dptr, num);
   }
}

static void
_aaxWavCvtFromSigned(void *id, void_ptr dptr, unsigned int num)
{
   _driver_t *handle = (_driver_t *)id;
   if (dptr && handle->cvt_from_signed) {
      handle->cvt_from_signed(dptr, num);
   }
}

static int
_aaxWavExtension(char *ext) {
   return !strcasecmp(ext, "wav");
}

static char*
_aaxWavInterfaces(int mode)
{
   static const char *rd[2] = { "*.wav\0", "*.wav\0" };
   return (char *)rd[mode];
}

static unsigned int
_aaxWavGetParam(void *id, int type)
{
   _driver_t *handle = (_driver_t *)id;

   unsigned int rv = 0;

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
   default:
      break;
   }
   return rv;
}

int
_aaxFileDriverReadHeader(_driver_t *handle, void *buffer, unsigned int *offs)
{
   uint32_t *header = (uint32_t*)buffer;
   unsigned int size, bufsize = *offs;
   int fmt, bits, res = -1;
   char extfmt, *ptr;

   extfmt = (header[5] & 0xFFFF) == EXTENSIBLE_WAVE_FORMAT;
   size = 4*sizeof(int32_t) + header[4];

   *offs = 0;
   if (is_bigendian())
   {
      int i;
      for (i=0; i<size/sizeof(int32_t); i++) {
         header[i] = _bswap32(header[i]);
      }
   }

#if 1
   printf("Read %s Header:\n", extfmt ? "Extnesible" : "Canonical");
   printf(" 0: %08x (ChunkID \"RIFF\")\n", header[0]);
   printf(" 1: %08x (ChunkSize: %i)\n", header[1], header[1]);
   printf(" 2: %08x (Format \"WAVE\")\n", header[2]);
   printf(" 3: %08x (Subchunk1ID \"fmt \")\n", header[3]);
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
#endif

   handle->frequency = header[6];
   handle->no_tracks = header[5] >> 16;
   handle->bits_sample = extfmt ? (header[9] >> 16) : (header[8] >> 16);
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
      free(handle->io.read.blockbuf);
      handle->io.read.blockbuf = malloc(handle->blocksize);
      break;
   default: 
      break;
   }

   /* search for the data chunk */
   ptr = (char*)buffer;
   ptr += size;
   bufsize -= size;

   if (ptr && (bufsize > 0))
   {
      unsigned int i = bufsize;
      while (i-- > 4)
      {
         int32_t *q = (int32_t*)ptr;
         if (*q == 0x61746164)		/* "data" */
         {
            q += 2;
            res = ((char*)q - (char*)buffer);
            *offs = res;
            break;
         }
         else if (*q == 0x74636166)	/* "fact */
         {
            unsigned int chunk_size = 8 + *(++q);
            ptr += chunk_size;
            i -= chunk_size;
         }
         else ptr++;
      }
      if (!i) res = __F_EOF;
   }
   else {
      res = __F_EOF;
   }

   return res;
}

static void*
_aaxFileDriverUpdateHeader(_driver_t *handle, unsigned int *bufsize)
{
   void *res = NULL;

   if (handle->io.write.size_bytes != 0)
   {
      char extfmt = (handle->io.write.header_size == WAVE_HEADER_SIZE) ? 0 : 1;
      unsigned int size = handle->io.write.size_bytes;
      uint32_t s;

      s =  4*handle->io.write.header_size + size - 8;
      handle->io.write.header[1] = s;

      s = size;
      if (extfmt) {
         handle->io.write.header[16] = s;
      }
      else {
         handle->io.write.header[10] = s;
      }

      if (is_bigendian())
      {
         handle->io.write.header[1] = _bswap32(handle->io.write.header[1]);
         if (extfmt) {
            handle->io.write.header[16] = _bswap32(handle->io.write.header[16]);
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
      printf("15: %08x (SubChunk2ID \"data\")\n", handle->io.write.header[15]);
      printf("16: %08x (Subchunk2Size: %i)\n", handle->io.write.header[16], handle->io.write.header[16]);
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

static unsigned int
getFileFormatFromFormat(enum aaxFormat format)
{
   unsigned int rv = UNSUPPORTED;
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
_aaxMSADPCM_IMA4(void *data, size_t bufsize, int tracks, int *blocksize)
{
   *blocksize /= tracks;
   if (tracks > 1)
   {
      int32_t *buf = malloc(blocksize);
      if (buf)
      {
         int32_t* dptr = (int32_t*)data;
         unsigned int numBlocks, numChunks;
         unsigned int blockNum;

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

static unsigned int
_aaxWavMSADPCMBlockDecode(void *id, int32_t **dptr, unsigned int smp_offs, unsigned int num, unsigned int offset)
{
   _driver_t *handle = (_driver_t*)id;
   unsigned int offs_smp, rv = 0;

   if (num)
   {
      int32_t *src = handle->io.read.blockbuf;
      unsigned t, tracks = handle->no_tracks;

      for (t=0; t<tracks; t++)
      {
         int16_t predictor = handle->io.read.predictor[t];
         uint8_t index = handle->io.read.index[t];
         unsigned int l, ctr = 4;
         int32_t *d = dptr[t]+offset;
         int32_t *s = src+t;
         int32_t samp;
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
            unsigned int offs_chunks = offs_smp/8;
            int offs_bytes;

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
               samp = adpcm2linear(nibble >> 4, &predictor, &index);
               *d++ = samp << 8;
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
               samp = adpcm2linear(nibble & 0xF, &predictor, &index);
               *d++ = samp << 8;
               samp = adpcm2linear(nibble >> 4, &predictor, &index);
               *d++ = samp << 8;
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
            samp = adpcm2linear(nibble & 0xF, &predictor, &index);
            *d++ = samp << 8;
            l--;
         }

         handle->io.read.predictor[t] = predictor;
         handle->io.read.index[t] = index;

         rv = num-l;
      }
   }

   return rv;
}

unsigned int
_batch_cvt24_adpcm_intl(void *id, int32_ptrptr dptr, const_void_ptr sptr, int offs, unsigned int tracks, unsigned int num)
{
   _driver_t *handle = (_driver_t*)id;
   int rv = 0;

   if (sptr)
   {
      unsigned int blocksize = SMP_TO_MSBLOCKSIZE(num, tracks);
      if (blocksize >= handle->blocksize)
      {
         _aax_memcpy(handle->io.read.blockbuf, sptr, handle->blocksize);
         handle->io.read.blockstart_smp = 0;
         rv = __F_PROCESS;
      }
      else {
         rv = __F_EOF;
      }
   }
   else if (num && handle->io.read.blockstart_smp != -1)
   {
      unsigned int block_smp, decode_smp, offs_smp;

      offs_smp = handle->io.read.blockstart_smp;
      block_smp = MSBLOCKSIZE_TO_SMP(handle->blocksize, handle->no_tracks);
      decode_smp = _MIN(block_smp-offs_smp, num);

      rv = _aaxWavMSADPCMBlockDecode(handle, dptr, offs_smp, decode_smp, offs);

      handle->io.read.blockstart_smp += rv;
   }

   return rv;
}

void
_batch_cvt24_mulaw_intl(int32_ptrptr dptr, const_void_ptr sptr, int offset, unsigned int tracks, unsigned int num)
{
   if (num)
   {
      unsigned int t;
      for (t=0; t<tracks; t++)
      {
         int8_t *s = (int8_t *)sptr + t;
         int32_t *d = dptr[t] + offset;
         unsigned int i = num;

         do
         {
            *d++ = _mulaw2linear_table[*s] << 8;
            s += tracks;
         }
         while (--i);
      }
   }
}

void
_batch_cvt24_alaw_intl(int32_ptrptr dptr, const_void_ptr sptr, int offset, unsigned int tracks, unsigned int num)
{
   if (num)
   {
      unsigned int t;
      for (t=0; t<tracks; t++)
      {
         int8_t *s = (int8_t *)sptr + t;
         int32_t *d = dptr[t] + offset;
         unsigned int i = num;

         do
         {
            *d++ = _alaw2linear_table[*s] << 8;
            s += tracks;
         }
         while (--i);
      }
   }
}


