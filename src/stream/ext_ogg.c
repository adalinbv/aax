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

#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <assert.h>

#include <arch.h>

#include "extension.h"
#include "format.h"
#include "ext_ogg.h"

typedef struct
{
   void *id;

   _fmt_t *fmt;

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
   enum oggFormat ogg_format;

   /* page header */
   char header_type;
   uint64_t granule_position;
   uint32_t bitstream_serial_no;
   uint32_t page_sequence_no;
   size_t segment_size;
   /* page header */

   void *oggptr;
   uint32_t *oggBuffer;
   size_t oggBufSize;
   size_t oggBufPos;

   uint32_t *crc_table;
   size_t datasize;

} _driver_t;


static _fmt_type_t _getFmtFromOGGFormat(enum oggFormat);
static int _aaxFormatDriverReadHeader(_driver_t*, size_t*);
static int _getOggPageHeader(_driver_t*, size_t);
static int _getOggIdentification(_driver_t*, unsigned char*, size_t);
static int _getOggComment(_driver_t*, unsigned char*, size_t);


#define OGG_HEADER_SIZE		8
static const uint32_t _aaxDefaultOggHeader[OGG_HEADER_SIZE];

#include "ext_ogg_crc.c"


int
_ogg_detect(_ext_t *ext, int mode)
{
   return AAX_TRUE;
}

int
_ogg_setup(_ext_t *ext, int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
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
         handle->blocksize = tracks*bits_sample/8;
         handle->frequency = freq;
         handle->no_tracks = tracks;
         handle->format = format;
         handle->bitrate = bitrate;
         handle->no_samples = no_samples;
         handle->max_samples = 0;

         if (handle->capturing)
         {
            handle->no_samples = UINT_MAX;
            *bufsize = 4096;
         }
         else /* playback */
         {
            *bufsize = 0;
         }
         ext->id = handle;
         rv = AAX_TRUE;
      }
      else {
         _AAX_FILEDRVLOG("OGG: Insufficient memory");
      }
   }
   else {
      _AAX_FILEDRVLOG("OGG: Unsupported format");
   }

   return rv;
}

void*
_ogg_open(_ext_t *ext, void_ptr buf, size_t *bufsize, size_t fsize)
{
   _driver_t *handle = ext->id;
   void *rv = NULL;

   assert(bufsize);

   if (handle)
   {
      if (!handle->capturing)   /* write */
      {
      }
			/* read: handle->capturing */
      else if (1) // (!handle->fmt) || !handle->fmt->open)
      {
         if (!handle->oggptr)
         {
            char *ptr = 0;

            handle->oggBufPos = 0;
            handle->oggBufSize = 16384;
            handle->oggptr = _aax_malloc(&ptr, handle->oggBufSize);
            handle->oggBuffer = (uint32_t*)ptr;
         }

         if (handle->oggptr)
         {
            size_t step, datapos, datasize = *bufsize, size = *bufsize;
            size_t avail = handle->oggBufSize-handle->oggBufPos;
            _fmt_type_t fmt;
            int res;

            avail = _MIN(size, avail);
            if (!avail) return NULL;

            memcpy((char*)handle->oggBuffer+handle->oggBufPos, buf, avail);
            handle->oggBufPos += avail;
            size -= avail;

            /*
             * read the file information and set the file-pointer to
             * the start of the data section
             */
            datapos = 0;
            do
            {
               step = 0;
               while ((res = _aaxFormatDriverReadHeader(handle,&step)) != __F_EOF)
               {
                  if (step > 0)
                  {
                     datapos += step;
                     datasize -= step;
                     handle->oggBufPos -= step;
                     memmove(handle->oggBuffer, (char*)handle->oggBuffer+step,
                             handle->oggBufPos);
                  }
                  if (res <= 0) break;
               }

               // The size of 'buf' may have been larger than the size of
               // handle->oggBuffer and there's still some data left.
               // Copy the next chunk and process it.
               if (res > 0 && size)
               {
                  avail = handle->oggBufSize-handle->oggBufPos;
                  if (!avail) break;

                  avail = _MIN(size, avail);

                  datapos = 0;
                  datasize = avail;
                  size -= avail;
                  memcpy((char*)handle->oggBuffer+handle->oggBufPos,
                         buf, avail);
                  handle->oggBufPos += avail;
               }
            }
            while (res > 0);

            if (!handle->fmt)
            {
               char *dataptr;

               fmt = _getFmtFromOGGFormat(handle->ogg_format);
               handle->fmt = _fmt_create(fmt, handle->mode);
               if (!handle->fmt) {
                  return rv;
               }

               if (!handle->fmt->setup(handle->fmt, fmt, handle->format))
               {
                  handle->fmt = _fmt_free(handle->fmt);
                  return rv;
               }

               handle->fmt->set(handle->fmt, __F_FREQ, handle->frequency);
               handle->fmt->set(handle->fmt, __F_RATE, handle->bitrate);
               handle->fmt->set(handle->fmt, __F_TRACKS, handle->no_tracks);
               handle->fmt->set(handle->fmt,__F_SAMPLES, handle->no_samples);
               handle->fmt->set(handle->fmt, __F_BITS, handle->bits_sample);
               handle->fmt->set(handle->fmt, __F_BLOCK, handle->blocksize);
//             handle->fmt->set(handle->fmt, __F_POSITION,
//                                              handle->blockbufpos);
               dataptr = (char*)buf + datapos;
               rv = handle->fmt->open(handle->fmt, dataptr, &datasize,
                                      handle->datasize);
            }

            if (res < 0)
            {
               if (res == __F_PROCESS) {
                  return buf;
               }
               else if (size)
               {
                  _AAX_FILEDRVLOG("OGG: Incorrect format");
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
            _AAX_FILEDRVLOG("OGG: Incorrect format");
            return rv;
         }
      }
        /* Format requires more data to process it's own header */
      else if (handle->fmt && handle->fmt->open) {
          rv = handle->fmt->open(handle->fmt, buf, bufsize,
                                 handle->datasize);
      }
      else _AAX_FILEDRVLOG("OGG: Unknown opening error");
   }
   else {
      _AAX_FILEDRVLOG("OGG: Internal error: handle id equals 0");
   }

   return rv;
}

int
_ogg_close(_ext_t *ext)
{
   _driver_t *handle = ext->id;
   int res = AAX_TRUE;

   if (handle)
   {
      _aax_free(handle->oggptr);
      if (handle->fmt)
      {
         handle->fmt->close(handle->fmt);
         _fmt_free(handle->fmt);
      }
      free(handle->crc_table);
      free(handle);
   }

   return res;
}

void*
_ogg_update(_ext_t *ext, size_t *offs, size_t *size, char close)
{
   return NULL;
}

size_t
_ogg_fill(_ext_t *ext, void_ptr sptr, size_t *num)
{
   _driver_t *handle = ext->id;

// TODO: handle packet header
   return handle->fmt->fill(handle->fmt, sptr, num);
}

size_t
_ogg_copy(_ext_t *ext, int32_ptr dptr, size_t offs, size_t *num)
{
   _driver_t *handle = ext->id;
   return handle->fmt->copy(handle->fmt, dptr, offs, num);
}

size_t
_ogg_cvt_from_intl(_ext_t *ext, int32_ptrptr dptr, size_t offset, size_t *num)
{
   _driver_t *handle = ext->id;
   return handle->fmt->cvt_from_intl(handle->fmt, dptr, offset, num);
}

size_t
_ogg_cvt_to_intl(_ext_t *ext, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = ext->id;
   size_t rv;

   rv = handle->fmt->cvt_to_intl(handle->fmt, dptr, sptr, offs, num,
                                 scratch, scratchlen);
   handle->no_samples += *num;
// handle->update_dt += (float)*num/handle->frequency;

   return rv;
}

char*
_ogg_name(_ext_t *ext, enum _aaxStreamParam param)
{
   _driver_t *handle = ext->id;
   char *rv = handle->fmt->name(handle->fmt, param);
#if 0
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
#endif
   return rv;
}

char*
_ogg_interfaces(int ext, int mode)
{
   static const char *rd[2] = { "*.ogg\0", "*.ogg\0" };
   return (char *)rd[mode];
}

int
_ogg_extension(char *ext)
{
   int rv = _EXT_NONE;

   if (ext) {
      if (!strcasecmp(ext, "ogg") || !strcasecmp(ext, "oga") ||
          !strcasecmp(ext, "ogx") || !strcasecmp(ext, "spx") ||
          !strcasecmp(ext, "opus")) {
         rv = _EXT_OGG;
      }
   }
   return rv;
}

off_t
_ogg_get(_ext_t *ext, int type)
{
   _driver_t *handle = ext->id;
   return handle->fmt->get(handle->fmt, type);
}

off_t
_ogg_set(_ext_t *ext, int type, off_t value)
{
   _driver_t *handle = ext->id;
   return handle->fmt->set(handle->fmt, type, value);
}

/* -------------------------------------------------------------------------- */

static const uint32_t _aaxDefaultOggHeader[OGG_HEADER_SIZE] =
{
    0x5367674f,			/* 'OggS'                                    */
    0x00000200,			/* granule[1,0], type & version              */
    0x00000000,			/* granule[3,2,5,4]                          */
    0x00000000,			/* granule[7,6] & serial[1,0]                */
    0x00000000,			/* serial[3,2] & sequence[1,0]               */
    0x00000000,			/* sequence[3,2] & CRC[1,0]                  */
    0x00000000,			/* CRC[3,2] & seg_table[0], segments         */
    0x00000000			/* segment table                             */
};



// https://www.ietf.org/rfc/rfc3533.txt
static int
_getOggPageHeader(_driver_t *handle, size_t size)
{
   uint32_t *header = handle->oggBuffer;
   size_t bufsize = handle->oggBufPos;
   int32_t curr;
   int rv = -1;

   if (bufsize < OGG_HEADER_SIZE) {
      return __F_PROCESS;
   }

   curr = header[0];
   if (curr == 0x5367674f)		/* OggS */
   {
      unsigned int version, header_size;

      curr = (header[6] >> 16) & 0xFF;
      header_size = 27 + curr;
      if (bufsize < header_size) {
         return -1;
      }

      version = header[1] & 0xFF;
      handle->header_type = (header[1] >> 8) & 0xFF;
      if (version == 0x0)
      {
         curr = header[1] && 0xFFFF0000;
         handle->granule_position = (uint64_t)curr << 32;

         curr = ((uint64_t)header[2] << 16) | (header[3] & 0xFFFF);
         handle->granule_position |= curr;

         curr = (header[3] >> 16) | (header[4] << 16);
         if (!handle->bitstream_serial_no || curr==handle->bitstream_serial_no)
         {
            handle->bitstream_serial_no = curr;

            curr = (header[4] >> 16) | (header[5] << 16);
            if (!handle->page_sequence_no || curr > handle->page_sequence_no)
            {
               unsigned char *ch = (unsigned char*)header;
               unsigned int i, no_segments;

               handle->page_sequence_no = curr;

               no_segments = (header[6] >> 16) & 0xFF;
               if (no_segments > 0)
               {
                  handle->segment_size = 0;
                  for (i=0; i<no_segments; ++i) {
                     handle->segment_size += ch[27+i];
                  }
#if 0
{
                  uint32_t crc32, i32;

                  // CRC check, must be last
                  i32 = (header[5] >> 16) | (header[6] << 16);
                  header[5] = (header[5] & 0x0000FFFF);
                  header[6] = (header[6] & 0xFFFF0000);
                  crc32 = crc32_calculcate(ch, header_size+handle->segment_size);
}
#endif

#if 0
{
   char *ch = (char*)header;
   uint64_t i64;
   uint32_t i32;
   int i;

   printf("Read Header:\n");

   printf("0: %08x (Magic number: \"%c%c%c%c\"\n", header[0], ch[0], ch[1], ch[2], ch[3]);

   printf("1: %08x (Version: %i | Type: %x)\n", header[1], ch[4], ch[5]);

   i64 = ((uint64_t)(header[1] && 0xFFFF0000) << 32) | ((uint64_t)header[2] << 16) | (header[3] & 0xFFFF);
   printf("2: %08x (Granule position: %li)\n", header[2], i64);

   i32 = (header[3] >> 16) | (header[4] << 16);
   printf("3: %08x (Serial number: %08x)\n", header[3], i32);

   i32 = (header[4] >> 16) | (header[5] << 16);
   printf("4: %08x (Sequence number: %08x)\n", header[4], i32);

   i32 = (header[5] >> 16) | (header[6] << 16);
   printf("5: %08x (CRC cehcksum: %08x)\n", header[5], i32);

   i32 = (header[6] >> 16) & 0xFF;

   i64 = 0;
   for (i=0; i<i32; ++i) {
      i64 += (uint8_t)ch[27+i];
   }
   printf("6: %08x (Page segments: %i, Total segment size: %li)\n", header[6], i32, i64);
}
#endif
                  rv = header_size;
                  if (rv <= bufsize)
                  {
                     handle->oggBufPos -= rv;
                     if (handle->oggBufPos > 0) {
                        memmove(ch, ch+rv, bufsize);
                     }
                  }
                  else {
                     rv = __F_PROCESS;
                  }
               }
               else {
                  rv = __F_EOF;
               }
            }
         }
         else	// SKIP page due to incorrect serial number
         {
            unsigned char *ch = (unsigned char*)header;
            unsigned int i, no_segments = (header[6] >> 16) & 0xFF;

            rv = header_size;
            for (i=0; i<no_segments; ++i) {
               rv += ch[27+i];
            }

            if (rv <= bufsize)
            {
               handle->oggBufPos -= rv;
               if (handle->oggBufPos > 0) {
                  memmove(ch, ch+rv, handle->oggBufPos);
               }
            }
            else {
               rv = __F_PROCESS;
            }
         }
      }
      else {
         rv = __F_EOF;
      }
   }
   else {
      rv = 0;
   }

   return rv;
}

int
_aaxFormatDriverReadHeader(_driver_t *handle, size_t *step)
{
   uint32_t *header = handle->oggBuffer;
   size_t bufsize = handle->oggBufPos;
   int rv;

   rv = _getOggPageHeader(handle, bufsize);
   if (rv >= 0)
   {
     unsigned int page_size = 0;

     /*
      * The packets must occur in the order of identification,
      * comment, setup. 
      */
      if ((handle->segment_size > 0) && (bufsize > handle->segment_size))
      {
         unsigned char *segment = (unsigned char*)header;
         size_t segment_size = handle->segment_size;

         /*
          * https://tools.ietf.org/html/rfc3533.html#section-6
          * As Ogg pages have a maximum size of about 64 kBytes, sometimes a
          * packet has to be distributed over several pages.  To simplify that
          * process, Ogg divides each packet into 255 byte long chunks plus a
          * final shorter chunk.  These chunks are called "Ogg Segments".
          *
          * They are only a logical construct and do not have a header for
          * themselves.
          */

         if (bufsize >= segment_size)
         {
            switch(segment[0])	// type
            {
            case HEADER_IDENTIFICATION:
               rv = _getOggIdentification(handle, segment, segment_size);
               break;
            case HEADER_COMMENT:
               rv = _getOggComment(handle, segment, segment_size);
               segment_size = rv;
               break;
            default:
               rv = handle->fmt->open(handle->fmt, header, bufsize,
                                      handle->datasize);
               break;
            }

            if (rv > 0)
            {
               page_size += segment_size;
               *step = page_size;
               rv = page_size;
            }
         }
         else {
            *step = 0;
            rv = __F_PROCESS;
         }
      }
      else
      {
         *step = 0;
         rv = __F_PROCESS;
      }
   }

   return rv;
}

static _fmt_type_t
_getFmtFromOGGFormat(enum oggFormat fmt)
{
   _fmt_type_t rv = _FMT_PCM;
   return rv;
}

// https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-630004.2.2
static int
_aaxFormatDriverReadVorbisHeader(_driver_t *handle, char *h, size_t len)
{
   int rv = -1;

   if (len >= VORBIS_ID_HEADER_SIZE)
   {
      uint32_t *header = (uint32_t*)h; 
      int type = h[0];

      if (type == HEADER_IDENTIFICATION)
      {
#if 1
   printf("\n--Vorbis Identification Header:\n");
   printf("  0: %08x (Type: %x)\n", header[0], h[0]);
   printf("  1: %08x (Codec identifier \"%c%c%c%c%c%c\")\n", header[1], h[1], h[2], h[3], h[4], h[5], h[6]);
   printf("  2: %08x (version %i, channels: %i)\n", header[2], (header[2] << 8) | (header[3] >> 24), header[2] >> 24);
   printf("  3: %08x (Sample rate: %i)\n", header[3], header[3]);
   printf("  4: %08x (Max. bitrate: %i)\n", header[4], (int32_t)header[4]);
   printf("  5: %08x (Nom. bitrate: %i)\n", header[5], (int32_t)header[5]);
   printf("  6: %08x (Min. bitrate: %i)\n", header[6], (int32_t)header[6]);
   printf("  7: %08x (block size: %i - %i, framing: %i)\n", header[7], 1 << (header[7] >> 28), 1 << ((header[7] >> 24) & 0xF), (header[7] >> 16) & 0x1);
#endif 

         uint32_t version = (header[2] << 8) | (header[3] >> 24);
         if (version == 0x0)
         {
            int blocksize1;

            handle->format = AAX_FLOAT;
            handle->no_tracks = header[2] >> 24;
            handle->frequency = header[3];
            handle->blocksize = 1 << (header[7] >> 28);
//          handle->blocksize = 4*handle->no_tracks;
            blocksize1 = 1 << ((header[7] >> 24) & 0xF);

            handle->bitrate = header[5]; // nominal bitrate
            if (handle->bitrate <= 0) {
               handle->bitrate = header[6]; // minimum bitrate
            }
            if (handle->bitrate <= 0) {
               handle->bitrate = header[4]; // maximum bitrate
            }

            if (handle->no_tracks <= 0 || handle->frequency <= 0 ||
                (handle->blocksize > blocksize1) ||
                (((header[7] >> 16) & 0x1) == 0))
            {
               // Failure to meet any of these conditions renders a
               // stream undecodable.
//             _AAX_FILEDRVLOG("OGG: Invalid Vorbis stream");
               return __F_EOF;
            }

            rv = VORBIS_ID_HEADER_SIZE;
            len -= VORBIS_ID_HEADER_SIZE;
         }
      }
   }

   return rv;
}

// https://wiki.xiph.org/OggPCM#Main_Header_Packet
static int
_aaxFormatDriverReadPCMHeader(_driver_t *handle, char *h, size_t len)
{
   int rv = -1;

   if (len >= 16)
   {
      uint32_t *header = (uint32_t*)h;
      uint32_t pcm_format = header[3];

#if 0
   printf("\n  oggPCM Header:\n");
   printf("0-1: %08x %08x (Codec identifier \"%c%c%c%c%c%c%c%c\"\n", header[0], header[1], h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7]);
   printf("  2: %08x (version %04x - %04x)\n", header[2], (header[2] & 0xFF), (header[2] >> 16));
   printf("  3: %08x (Format)\n",  header[3]);
   printf("  4: %08x (Sample Rate: %i)\n", header[4], header[4]);
   printf("  5: %08x (No. significant bits: %i)\n", header[5], (header[5] >> 24));
   printf("           No. channels: %i\n", (header[6] >> 16) & 0xFF);
   printf("           Max. frames per packet: %i)\n", header[7] & 0xFFFF);
   printf("  6: %08x (No. extra header packets: %i)\n", header[6], header[6]);
#endif

      if (header[2] == 0x0) // vorbis version
      {
         rv = len;
         handle->frequency = header[4];
         handle->no_tracks = (header[6] >> 16) & 0xFF;
         handle->bits_sample = header[5] >> 24;

         switch (pcm_format)
         {
         case OGGPCM_FMT_S8:
            handle->format = AAX_PCM8S;
            handle->blocksize = handle->no_tracks;
            break;
         case OGGPCM_FMT_U8:
            handle->format = AAX_PCM8U;
            handle->blocksize = handle->no_tracks;
            break;
         case OGGPCM_FMT_S16_LE:
            handle->format = AAX_PCM16S_LE;
            handle->blocksize = 2*handle->no_tracks;
            break;
         case OGGPCM_FMT_S16_BE:
            handle->format = AAX_PCM16S_BE;
            handle->blocksize = 2*handle->no_tracks;
            break;
         case OGGPCM_FMT_S24_LE:
            handle->format = AAX_PCM24S_LE;
            handle->blocksize = 3*handle->no_tracks;
            break;
         case OGGPCM_FMT_S24_BE:
            handle->format = AAX_PCM24S_BE;
            handle->blocksize = 3*handle->no_tracks;
            break;
         case OGGPCM_FMT_S32_LE:
            handle->format = AAX_PCM32S_LE;
            handle->blocksize = 4*handle->no_tracks;
            break;
         case OGGPCM_FMT_S32_BE:
            handle->format = AAX_PCM32S_BE;
            handle->blocksize = 4*handle->no_tracks;
            break;
         case OGGPCM_FMT_ULAW:
            handle->format = AAX_MULAW;
            break;
         case OGGPCM_FMT_ALAW:
            handle->format  = AAX_ALAW;
            break;
         case OGGPCM_FMT_FLT32_LE:
            handle->format = AAX_FLOAT_LE;
            handle->blocksize = 4*handle->no_tracks;
            break;
         case OGGPCM_FMT_FLT32_BE:
            handle->format = AAX_FLOAT_BE;
            handle->blocksize = 4*handle->no_tracks;
            break;
         case OGGPCM_FMT_FLT64_LE:
            handle->format = AAX_DOUBLE_LE;
            handle->blocksize = 8*handle->no_tracks;
            break;
         case OGGPCM_FMT_FLT64_BE:
            handle->format = AAX_DOUBLE_BE;
            handle->blocksize = 8*handle->no_tracks;
            break;
         default:
            break;
         }
      }
   }
   return rv;
}

// https://tools.ietf.org/html/rfc5334
static int
_getOggIdentification(_driver_t *handle, unsigned char *ch, size_t len)
{
   char *h = (char*)ch;
   int rv = -1;

   if ((len > 5) && !strncmp(h, "\177FLAC", 5))
   {
      handle->ogg_format = FLAC_OGG_FILE;
      handle->format = AAX_PCM16S;
   }
   else if ((len > 7) && !strncmp(h, "\x01vorbis", 7))
   {
      handle->ogg_format = VORBIS_OGG_FILE;
      rv = _aaxFormatDriverReadVorbisHeader(handle, h, len);
   }
   else if ((len > 8) && !strncmp(h, "PCM     ", 8))
   {
      handle->ogg_format = PCM_OGG_FILE;
      rv = _aaxFormatDriverReadPCMHeader(handle, h, len);
   }
   else if ((len > 8) && !strncmp(h, "Speex   ", 8))
   {
      handle->ogg_format = SPEEX_OGG_FILE;
      handle->format = AAX_PCM16S;
   }
   else if ((len > 8) && !strncmp(h, "OpusHead", 8))
   {
      handle->ogg_format = OPUS_OGG_FILE;
      handle->format = AAX_PCM16S;
   }

   return rv;
}

// https://xiph.org/vorbis/doc/v-comment.html
#define COMMENT_SIZE	1024
static int
_getOggComment(_driver_t *handle, unsigned char *ch, size_t len)
{
   int32_t *header = (int32_t*)ch;
   unsigned char *ptr;
   size_t i, size;
   int rv = len;

   i = 11 + ((header[1] >> 24) | (header[2] << 8));
   ptr = ch+i;
   size = *(uint32_t*)ptr;
   ptr += 4;
   for (i=0; i<size; i++)
   {
      size_t slen = *(uint32_t*)ptr;
      ptr += 4 + slen;

      if ((ptr-ch) > len) {
          return __F_PROCESS;
      }
   }
   ptr++;
   rv = ptr-ch;

#if 0
   char s[COMMENT_SIZE+1];

   printf("\n--Vorbis Comment Header:\n");
   printf("  0: %08x (Type: %x)\n", header[0], ch[0]);
   printf("  1: %08x (Codec identifier \"%c%c%c%c%c%c\")\n", header[1], ch[1], ch[2], ch[3], ch[4], ch[5], ch[6]);

   size = (header[1] >> 24) | (header[2] << 8);
   snprintf(s, _MIN(size+1, COMMENT_SIZE), "%s\0", ch+11);
   printf("  2: %08x Vendor: '%s'\n", header[2], s);

   i = 11+size;
   ptr = ch+i;
   size = *(uint32_t*)ptr;
// printf("User comment list length: %i\n", size);

   ptr += 4;
   for (i=0; i<size; i++)
   {
      size_t slen = *(uint32_t*)ptr;
      ptr += 4;
      snprintf(s, _MIN(slen+1, COMMENT_SIZE), "%s\0", ptr);
      printf("\t'%s'\n", s);
      ptr += slen;
   }
   ptr++;
   printf("framing: %i\n", *ptr);
#endif

   return rv;
}

