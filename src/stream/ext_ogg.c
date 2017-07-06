/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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
#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif


#include <base/memory.h>
#include <arch.h>
#include <api.h>

#include "extension.h"
#include "format.h"
#include "ext_ogg.h"


// https://wiki.xiph.org/index.php/Main_Page
// https://xiph.org/vorbis/doc/v-comment.html
// https://xiph.org/flac/ogg_mapping.html
// https://wiki.xiph.org/OggOpus (superseded by RFC-7845)
//  -- https://tools.ietf.org/html/rfc7845.html
// https://wiki.xiph.org/OggPCM (listed under abandonware)


typedef struct
{
   void *id;

   char artist_changed;
   char title_changed;

   char *artist;
   char *title;
   char *original;
   char *album;
   char *trackno;
   char *date;
   char *genre;
   char *composer;
   char *comments;
   char *copyright;
   char *website;

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

   _fmt_type_t format_type;
   enum oggFormat ogg_format;

   /*
    * Indicate whether the data presented to the format code needs the Ogg
    * page header or not. If not remove it from the data stream.
    */
   char keep_header;

   /* page header information */
   char header_type;
   uint64_t granule_position;
   uint32_t bitstream_serial_no;
   uint32_t page_sequence_no;

   unsigned int header_size;
   unsigned int page_size;
   unsigned int segment_size;

   unsigned short packet_no;
   unsigned short no_packets;
   unsigned short packet_offset[256];
   /* page header */

   _data_t *oggBuffer;
   size_t datasize;

   /* Opus */
   size_t pre_skip;
   float gain;

} _driver_t;


static int _aaxFormatDriverReadHeader(_driver_t*);
static int _getOggPageHeader(_driver_t*, uint32_t*, size_t);
static int _aaxOggInitFormat(_driver_t*, unsigned char*, size_t*);
static void _aaxOggFreeInfo(_driver_t*);
static void crc32_init(void);

/*
 * This handler does peek into the Ogg page header to determine it's serial
 * number and remove any Ogg package which does not match. If the serial
 * numbers do match however we send the complete Ogg page to the format
 * handler (currently handled by stb_vorbis and rb_flac).
 *
 * The Vorbis code always expects an Ogg stream.
 * The FLAC code automatically detects if it is an Ogg stream or a raw stream.
 */
#define OGG_HEADER_SIZE		8


int
_ogg_detect(VOID(_ext_t *ext), VOID(int mode))
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

         crc32_init();
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
         *bufsize = 0;
      }
			/* read: handle->capturing */
      else if (!handle->fmt || !handle->fmt->open)
      {
         if (!handle->oggBuffer) {
            handle->oggBuffer = _aaxDataCreate(200*1024, 1);
         }

         if (handle->oggBuffer)
         {
            size_t size = *bufsize;
            int res;

            handle->datasize = fsize;
            res = _aaxDataAdd(handle->oggBuffer, buf, size);
            *bufsize = res;

            res = _aaxFormatDriverReadHeader(handle);
            if (res <= 0) {
               rv = buf;
            }
         }
         else
         {
            _AAX_FILEDRVLOG("OGG: Incorrect format");
            return rv;
         }
      }
        /* Format requires more data to process it's own header */
      else if (handle->fmt && handle->fmt->open)
      {
         size_t size = *bufsize;
         int res;

         res = _aaxDataAdd(handle->oggBuffer, buf, size);
         *bufsize = res;

         res = _aaxFormatDriverReadHeader(handle);
         if (res <= 0) {
            rv = buf;
         }
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
      _aaxDataDestroy(handle->oggBuffer);
      if (handle->fmt)
      {
         handle->fmt->close(handle->fmt);
         _fmt_free(handle->fmt);
      }

      _aaxOggFreeInfo(handle);
      free(handle);
   }

   return res;
}

void*
_ogg_update(VOID(_ext_t *ext), VOID(size_t *offs), VOID(size_t *size), VOID(char close))
{
   return NULL;
}

size_t
_ogg_fill(_ext_t *ext, void_ptr sptr, size_t *bytes)
{
   _driver_t *handle = ext->id;
   int res, rv = __F_PROCESS;
   unsigned char *header;
   size_t avail;

   header = (unsigned char*)handle->oggBuffer->data;

   res = _aaxDataAdd(handle->oggBuffer, sptr, *bytes);
   *bytes = res;

   do
   {
      avail = _MIN(handle->page_size, handle->oggBuffer->avail);
      if (avail && handle->page_sequence_no >= 2 && handle->fmt)
      {
         rv = handle->fmt->fill(handle->fmt, header, &avail);
         if (avail)
         {
            _aaxDataMove(handle->oggBuffer, NULL, avail);
            handle->page_size -= avail;
         }
      }

      if (!handle->page_size)
      {
         uint32_t curr = header[5];
         if (curr == PACKET_FIRST_PAGE || handle->page_sequence_no < 2)
         {
            if (curr == PACKET_FIRST_PAGE)
            {
               handle->bitstream_serial_no = 0;
               handle->page_sequence_no = 0;

               _aaxOggFreeInfo(handle);

               handle->fmt->close(handle->fmt);
               _fmt_free(handle->fmt);
               handle->fmt = NULL;
            }
            rv = _aaxFormatDriverReadHeader(handle);
         }
         else
         {
            avail = handle->oggBuffer->avail;
            res = _getOggPageHeader(handle, (uint32_t*)header, avail);
            if (res)
            {
               if (!handle->keep_header)
               {
                  header += res;
                  avail = _MIN(avail, handle->segment_size);
               }
               else {
                  avail = _MIN(avail, handle->page_size);
               }
               rv = handle->fmt->fill(handle->fmt, header, &avail);
               if (avail)
               {
                  _aaxDataMove(handle->oggBuffer, NULL, avail);
                  handle->page_size -= avail;
               }
            }
            else {
               break;
            }
         }
      }
      else {
         rv = __F_PROCESS;
      }
   }
   while (rv > 0 && avail && handle->oggBuffer->avail);

   return rv;
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
   size_t rv = __F_EOF;

   handle->fmt->set(handle->fmt, __F_BLOCK_SIZE, handle->packet_offset[0]);
   rv = handle->fmt->cvt_from_intl(handle->fmt, dptr, offset, num);

   return rv;
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

   if (!rv)
   {
      switch(param)
      {
      case __F_ARTIST:
         rv = handle->artist;
         handle->artist_changed = AAX_FALSE;
         break;
      case __F_TITLE:
         rv = handle->title;
         handle->title_changed = AAX_FALSE;
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
      case __F_COMPOSER:
         rv = handle->composer;
         break;
      case __F_COMMENT:
         rv = handle->comments;
         break;
      case __F_COPYRIGHT:
         rv = handle->copyright;
         break;
      case __F_ORIGINAL:
         rv = handle->original;
         break;
      case __F_WEBSITE:
         rv = handle->website;
         break;
      default:
         if (param & __F_NAME_CHANGED)
         {
            switch (param & ~__F_NAME_CHANGED)
            {
            case __F_ARTIST:
               if (handle->artist_changed)
               {
                  rv = handle->artist;
                  handle->artist_changed = AAX_FALSE;
               }
               break;
            case __F_TITLE:
               if (handle->title_changed)
               {
                  rv = handle->title;
                  handle->title_changed = AAX_FALSE;
               }
               break;
            default:
               break;
            }
         }
         break;
      }
   }

   return rv;
}

char*
_ogg_interfaces(int ext, int mode)
{
   static const char *raw_exts[_EXT_MAX - _EXT_PCM] = {
      "*.ogg *.oga", "*.opus"
   };
   static char *rd[2][_EXT_PCM - _EXT_OGG] = {
      { NULL, NULL },
      { NULL, NULL }
   };
   char *rv = NULL;

   if (!mode && ext >= _EXT_OGG && ext < _EXT_PCM)
   {
      int m = mode > 0 ? 1 : 0;
      int pos = ext - _EXT_OGG;

      if (rd[m][pos] == NULL)
      {
         int format = _FMT_MAX;

         switch(ext)
         {
         case _EXT_OGG:
            format = _FMT_VORBIS;
            break;
         case _EXT_OPUS:
            format = _FMT_OPUS;
            break; 
         default:
            break;
         }

         _fmt_t *fmt = _fmt_create(format, m);
         if (fmt)
         {
            _fmt_free(fmt);
            rd[m][pos] = (char*)raw_exts[pos];
         }
      }
      rv = rd[mode][pos];
   }


   return rv;
}

int
_ogg_extension(char *ext)
{
   int rv = _EXT_NONE;

   if (ext) {
      if (!strcasecmp(ext, "ogg") || !strcasecmp(ext, "oga")
//        || !strcasecmp(ext, "ogx") || !strcasecmp(ext, "spx")
          || !strcasecmp(ext, "opus")
         )
      {
         rv = _EXT_OGG;
      }
      else if (!strcasecmp(ext, "flac")) {
         rv = _EXT_FLAC;
      }
   }
   return rv;
}

off_t
_ogg_get(_ext_t *ext, int type)
{
   _driver_t *handle = ext->id;
   off_t rv = 0;
   if (handle->fmt) {
      rv = handle->fmt->get(handle->fmt, type);
   }
   return rv;
}

off_t
_ogg_set(_ext_t *ext, int type, off_t value)
{
   _driver_t *handle = ext->id;
   off_t rv = 0;
   if (handle->fmt) {
      rv = handle->fmt->set(handle->fmt, type, value);
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

static int _getOggOpusComment(_driver_t *handle, unsigned char *ch, size_t len);
static int _getOggVorbisComment(_driver_t *handle, unsigned char *ch, size_t len);

#define CRC32_POLY		0x04c11db7   // from spec
#define CRC32_UPDATE(c,b)	c = ((c) << 8) ^ crc_table[(b) ^ ((c) >> 24)]
static uint32_t crc_table[256];

static void
crc32_init(void)
{
   int i,j;
   uint32_t s;
   for(i=0; i < 256; i++) {
      for (s=(uint32_t) i << 24, j=0; j < 8; ++j)
         s = (s << 1) ^ (s >= (1U<<31) ? CRC32_POLY : 0);
      crc_table[i] = s;
   }
}

static void
_aaxOggFreeInfo(_driver_t *handle)
{
   free(handle->trackno);
   handle->trackno = NULL;
   free(handle->artist);
   handle->artist = NULL;
   free(handle->title);
   handle->title = NULL;
   free(handle->album);
   handle->album = NULL;
   free(handle->date);
   handle->date = NULL;
   free(handle->genre);
   handle->genre = NULL;
   free(handle->composer);
   handle->composer = NULL;
   free(handle->comments);
   handle->comments = NULL;
   free(handle->copyright);
   handle->copyright = NULL;
   free(handle->original);
   handle->original = NULL;
   free(handle->website);
   handle->website = NULL;
}

static int
_aaxOggInitFormat(_driver_t *handle, unsigned char *oggbuf, size_t *bufsize)
{
   int rv = 0;
   if (!handle->fmt)
   {
      _fmt_type_t fmt = handle->format_type;

      handle->fmt = _fmt_create(handle->format_type, handle->mode);
      if (!handle->fmt)
      {
         *bufsize = 0;
         return rv;
      }

      if (!handle->fmt->setup(handle->fmt, fmt, handle->format))
      {
         handle->fmt = _fmt_free(handle->fmt);
         *bufsize = 0;
         return rv;
      }

      handle->fmt->set(handle->fmt, __F_FREQUENCY, handle->frequency);
      handle->fmt->set(handle->fmt, __F_RATE, handle->bitrate);
      handle->fmt->set(handle->fmt, __F_TRACKS, handle->no_tracks);
      handle->fmt->set(handle->fmt, __F_NO_SAMPLES, handle->no_samples);
      handle->fmt->set(handle->fmt, __F_BITS_PER_SAMPLE, handle->bits_sample);

      handle->fmt->set(handle->fmt, __F_BLOCK_SIZE, handle->blocksize);
//    handle->fmt->set(handle->fmt, __F_POSITION,
//                                     handle->blockbufpos);
   }

   if (handle->fmt) {
      handle->fmt->open(handle->fmt, oggbuf, bufsize, handle->datasize);
   }

   return rv;
}

// https://www.ietf.org/rfc/rfc3533.txt
static int
_getOggPageHeader(_driver_t *handle, uint32_t *header, size_t size)
{
   ssize_t bufsize = handle->oggBuffer->avail;
   uint32_t curr;
   int rv = 0;

   if (bufsize < OGG_HEADER_SIZE || size < OGG_HEADER_SIZE) {
      return __F_NEED_MORE;
   }

#if 0
{
   char *ch = (char*)header;
   unsigned int i;
   uint64_t i64;
   uint32_t i32;

   printf("Read Header:\n");

   printf("0: %08x (Magic number: \"%c%c%c%c\"\n", header[0], ch[0], ch[1], ch[2], ch[3]);

   printf("1: %08x (Version: %i | Type: 0x%x)\n", header[1], ch[4], ch[5]);

   i64 = (uint64_t)(header[1] >> 16);
   i64 |= ((uint64_t)header[2] << 16);
   i64 |= ((uint64_t)header[3] << 48);
   printf("2: %08x (Granule position: %zu)\n", header[2], i64);

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
   printf("6: %08x (Page segments: %i, Total segment size: %zi)\n", header[6], i32, i64);
}
#endif

   curr = header[0];
   if (curr != 0x5367674f)		/* OggS */
   {
      char *c = strncasestr(header, "OggS", size);
      if (!c) printf("OggS header not found\n");
      else
      {
         printf("Found OggS header at offset: %i\n", c-(char*)header);
         header = (uint32_t*)c;
         curr = header[0];
      }
   }

   if (curr == 0x5367674f)		/* OggS */
   {
      unsigned int version;

      curr = header[1] & 0xFF;
      version = curr;

      curr = (header[6] >> 16) & 0xFF;
      handle->header_size = 27 + curr;

      if ((bufsize >= handle->header_size) && (version == 0x0))
      {
         curr = (header[3] >> 16) | (header[4] << 16);
         if (!handle->bitstream_serial_no) {
            handle->bitstream_serial_no = curr;
         }

         if (curr == handle->bitstream_serial_no)
         {
            curr = (header[4] >> 16) | (header[5] << 16);
            if (!handle->page_sequence_no || 
                curr > handle->page_sequence_no)
            {
               unsigned char *ch = (unsigned char*)header;
               unsigned int i, no_segments;

               handle->page_sequence_no = curr;

#if 0
{
   char *ch = (char*)header;
   unsigned int i;
   uint64_t i64;
   uint32_t i32;
      
   printf("Read Header:\n");
      
   printf("0: %08x (Magic number: \"%c%c%c%c\"\n", header[0], ch[0], ch[1], ch[2], ch[3]);
   
   printf("1: %08x (Version: %i | Type: 0x%x)\n", header[1], ch[4], ch[5]);

   i64 = (uint64_t)(header[1] >> 16);
   i64 |= ((uint64_t)header[2] << 16);
   i64 |= ((uint64_t)header[3] << 48);
   printf("2: %08x (Granule position: %zu)\n", header[2], i64);

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
   printf("6: %08x (Page segments: %i, Total segment size: %zi)\n", header[6], i32, i64);
}
#endif
               curr = (header[1] >> 8) & 0xFF;
               handle->header_type = curr;

               handle->granule_position  = ((uint64_t)header[1] >> 16);
               handle->granule_position |= ((uint64_t)header[2] << 16);
               handle->granule_position |= ((uint64_t)header[3] << 48);

               no_segments = (header[6] >> 16) & 0xFF;
               if (no_segments > 0)
               {
                  unsigned int p = 0;

                  handle->segment_size = 0;
                  handle->packet_offset[p] = 0;
                  for (i=0; i<no_segments; ++i)
                  {
                     unsigned char ps = ch[27+i];

                     handle->segment_size += ps;
                     handle->packet_offset[p] += ps;
                     if (ps < 0xFF) {
                        handle->packet_offset[++p] = handle->segment_size;
                     }
                  }
                  handle->no_packets = p-1;
                  handle->page_size = handle->header_size+handle->segment_size;
#if 0
  printf("no. packets: %i\n", handle->no_packets);
  for(i=0; i<handle->no_packets; i++)
  printf(" %i: %u\n", i, handle->packet_offset[i+1] - handle->packet_offset[i]);
#endif
                  if (handle->page_size <= bufsize)
                  {
                     uint32_t crc32 = 0;

                     header[5] = (header[5] & 0x0000FFFF);
                     header[6] = (header[6] & 0xFFFF0000);
                     for (i=0; i<handle->page_size; i++) {
                        CRC32_UPDATE(crc32, ch[i]);
                     }
                  }
                  else {
                     handle->page_sequence_no--;
                  }

                  rv = handle->header_size;
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

            rv = 0;
            for (i=0; i<no_segments; ++i) {
               rv += ch[27+i];
            }

            if (rv <= bufsize)
            {
               _aaxDataMove(handle->oggBuffer, NULL, rv);
               rv = 0;
            }
            else {
               rv = __F_NEED_MORE;
            }
         }
      }
      else if (bufsize < handle->header_size) {
         rv = __F_NEED_MORE;
      } else {
         rv = __F_EOF;
      }
   }
   else
   {
      // TODO: search next header in the stream
      _AAX_FILEDRVLOG("OGG: wrong header");
      rv = 0;
   }

   return rv;
}

// https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-630004.2.2
#define VORBIS_ID_HEADER_SIZE	(4*7-1)
static int
_aaxFormatDriverReadVorbisHeader(_driver_t *handle, char *h, size_t len)
{
   int rv = __F_EOF;

   if (len >= VORBIS_ID_HEADER_SIZE)
   {
      uint32_t *header = (uint32_t*)h;
      int type = h[0];

      if (type == HEADER_IDENTIFICATION)
      {
#if 0
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
            unsigned int blocksize1;

            handle->format = AAX_PCM24S;
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
         }
      }
   }

   return rv;
}

// https://tools.ietf.org/html/rfc7845.html#page-12
#define OPUS_ID_HEADER_SIZE	(4*5-1)
static int
_aaxFormatDriverReadOpusHeader(_driver_t *handle, char *h, size_t len)
{
   int rv = __F_EOF;

   if (len >= OPUS_ID_HEADER_SIZE)
   {
      int version = h[8];
      if (version == 1)
      {
         unsigned char mapping_family;
         int gain;

         handle->format = AAX_FLOAT;
         handle->no_tracks = (unsigned char)h[9];
         handle->frequency = *((uint32_t*)h+3);
         handle->pre_skip = (unsigned)h[10] << 8 | h[11];
         handle->no_samples = -handle->pre_skip;

         gain = (int)h[16] << 8 | h[17];
         handle->gain = pow(10, (float)gain/(20.0f*256.0f));

         mapping_family = h[18];
         if ((mapping_family == 0 || mapping_family == 1) &&
             (handle->no_tracks > 1) && (handle->no_tracks <= 8))
         {
             /*
              * The 'channel mapping table' MUST be omitted when the channel
              * mapping family s 0, but is REQUIRED otherwise.
              */
             if (mapping_family == 1)
             {
                 int stream_count = h[19];
                 int coupled_count = h[20];
                 if ((stream_count > 0) && (stream_count <= coupled_count))
                 {
                    // what follows is 'no_tracks' bytes for the channel mapping
                    rv = OPUS_ID_HEADER_SIZE + handle->no_tracks + 2;
                    if (rv <= (int)len) {
                       rv = __F_NEED_MORE;
                    }
                 }
             }
             else {
                rv = OPUS_ID_HEADER_SIZE;
             }
         }
      }

#if 0
{
  uint32_t *header = (uint32_t*)h;
  float gain = (int)h[16] << 8 | h[17];
  printf("\n-- Opus Identification Header:\n");
  printf("  0: %08x %08x ('%c%c%c%c%c%c%c%c')\n", header[0], header[1], h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7]);
  printf("  2: %08x (Version: %i, Tracks: %i, Pre Skip: %i)\n", header[2], h[8], (unsigned char)h[9], (unsigned)h[10] << 8 | h[11]);
  printf("  3: %08x (Original Sample Rate: %i)\n", header[3],  header[3]);
  printf("  4: %08x (Replay gain: %f, Mapping Family: %i)\n", header[4], pow(10, (float)gain/(20.0f*256.0f)), h[18]);
  if (h[18] == 1) {
    printf("  5: %08x (Stream Count: %i, Coupled Count: %i)\n", header[5], h[19], h[20]);
  }
}
#endif
   }

   return rv;
}

#if 0
// https://wiki.xiph.org/OggPCM#Main_Header_Packet
static int
_aaxFormatDriverReadPCMHeader(_driver_t *handle, char *h, size_t len)
{

   int rv = __F_EOF;

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
#endif

// https://tools.ietf.org/html/rfc5334
static int
_getOggIdentification(_driver_t *handle, unsigned char *ch, size_t len)
{
   char *h = (char*)ch;
   int rv = __F_NEED_MORE;

#if 0
  printf("  Codec identifier \"%c%c%c%c%c%c%c\"\n", ch[0], ch[1], ch[2], ch[3], ch[4], ch[5], ch[6]);
#endif

   if ((len > 5) && !strncmp(h, "\177FLAC", 5))
   {
      handle->keep_header = AAX_FALSE;
      handle->ogg_format = FLAC_OGG_FILE;
      handle->format_type = _FMT_FLAC;
      handle->format = AAX_PCM16S;
      rv = 0;
   }
   else if ((len > 7) && !strncmp(h, "\x01vorbis", 7))
   {
      handle->keep_header = AAX_TRUE;
      handle->format_type = _FMT_VORBIS;
      handle->ogg_format = VORBIS_OGG_FILE;
      rv = _aaxFormatDriverReadVorbisHeader(handle, h, len);
   }
   else if ((len > 8) && !strncmp(h, "OpusHead", 8))
   {
      handle->keep_header = AAX_FALSE;
      handle->format_type = _FMT_OPUS;
      handle->ogg_format = OPUS_OGG_FILE;
      rv = _aaxFormatDriverReadOpusHeader(handle, h, len);
   }
#if 0
   else if ((len > 8) && !strncmp(h, "PCM     ", 8))
   {
      handle->keep_header = AAX_FALSE;
      handle->format_type = _FMT_PCM;
      handle->ogg_format = PCM_OGG_FILE;
      rv = _aaxFormatDriverReadPCMHeader(handle, h, len);
   }
   else if ((len > 8) && !strncmp(h, "Speex   ", 8))
   {
      handle->keep_header = AAX_FALSE;
      handle->format_type = _FMT_SPEEX;
      handle->ogg_format = SPEEX_OGG_FILE;
      handle->format = AAX_PCM16S;
      rv = 0;
   }
#endif

   return rv;
}

// https://xiph.org/vorbis/doc/v-comment.html
// https://wiki.xiph.org/OggOpus#Content_Type
// https://wiki.xiph.org/OggPCM#Comment_packet
#define COMMENT_SIZE    1024

static int
_getOggOpusComment(_driver_t *handle, unsigned char *ch, size_t len)
{
   uint32_t *header = (uint32_t*)ch;
   char field[COMMENT_SIZE+1];
   unsigned char *ptr;
   size_t i, size;
   int rv = len;

#if 0
   printf("\n--Opus Comment Header:\n");
   printf("  0: %08x %08x (\"%c%c%c%c%c%c%c%c\")\n", header[0], header[1], ch[0], ch[1], ch[2], ch[3], ch[4], ch[5], ch[6], ch[7]);

   size = header[2];
   snprintf(field, _MIN(size+1, COMMENT_SIZE), "%s", ch+12);
   printf("  2: %08x Vendor: '%s'\n", header[2], field);

   i = 12+size;
   ptr = ch+i;
   size = *(uint32_t*)ptr;
// printf("User comment list length: %i\n", size);

   ptr += 4;
   for (i=0; i<size; i++)
   {
      size_t slen = *(uint32_t*)ptr;
      ptr += 4;
      snprintf(field, _MIN(slen+1, COMMENT_SIZE), "%s", ptr);
      printf("\t'%s'\n", field);
      ptr += slen;
   }
#endif

   field[COMMENT_SIZE] = 0;

   size = header[2];
   snprintf(field, _MIN(size+1, COMMENT_SIZE), "%s", ch+12);
// handle->vendor = strdup(field);

   i = 12+size;
   ptr = ch+i;
   size = *(uint32_t*)ptr;

   ptr += 4;
   for (i=0; i<size; i++)
   {
      uint32_t slen = *(uint32_t*)ptr;

      ptr += sizeof(uint32_t);
      if ((size_t)(ptr+slen-ch) > len) {
          return __F_NEED_MORE;
      }

      snprintf(field, _MIN(slen+1, COMMENT_SIZE), "%s", ptr);
      ptr += slen;

      if (!STRCMP(field, "TITLE"))
      {
          handle->title = stradd(handle->title, field+strlen("TITLE="));
          handle->title_changed = AAX_TRUE;
      }
      else if (!STRCMP(field, "ARTIST")) {
          handle->artist = stradd(handle->artist, field+strlen("ARTIST="));
          handle->artist_changed = AAX_TRUE;
      }
//    else if (!STRCMP(field, "PERFORMER"))
//    {
//        handle->artist = stradd(handle->artist, field+strlen("PERFORMER="));
//        handle->artist_changed = AAX_TRUE;
//    }
      else if (!STRCMP(field, "ALBUM")) {
          handle->album = stradd(handle->album, field+strlen("ALBUM="));
      } else if (!STRCMP(field, "TRACKNUMBER")) {
          handle->trackno = stradd(handle->trackno, field+strlen("TRACKNUMBER="));
      } else if (!STRCMP(field, "COPYRIGHT")) {
          handle->copyright = stradd(handle->copyright, field+strlen("COPYRIGHT="));
      } else if (!STRCMP(field, "GENRE")) {
          handle->genre = stradd(handle->genre, field+strlen("GENRE="));
      } else if (!STRCMP(field, "DATE")) {
          handle->date = stradd(handle->date, field+strlen("DATE="));
      } else if (!STRCMP(field, "CONTACT")) {
          handle->website = stradd(handle->website, field+strlen("CONTACT="));
      } else if (!STRCMP(field, "DESCRIPTION")) {
          handle->comments = stradd(handle->comments, field+strlen("DESCRIPTION="));
      }
      else if (!STRCMP(field, "R128_TRACK_GAIN"))
      {
          int gain = atoi(field+strlen("R128_TRACK_GAIN="));
          handle->gain = pow(10, (float)gain/(20.0f*256.0f));
      }
   }
   rv = ptr-ch;

   return rv;
}

static int
_getOggVorbisComment(_driver_t *handle, unsigned char *ch, size_t len)
{
   uint32_t *header = (uint32_t*)ch;
   char field[COMMENT_SIZE+1];
   unsigned char *ptr;
   size_t i, size;
   int rv = len;

#if 0
   printf("\n--Vorbis Comment Header:\n");
   printf("  0: %08x %08x (\"%c%c%c%c%c%c%c%c\")\n", header[0], header[1], ch[0], ch[1], ch[2], ch[3], ch[4], ch[5], ch[6], ch[7]);

   size = (header[1] >> 24) | (header[2] << 8);
   snprintf(field, _MIN(size+1, COMMENT_SIZE), "%s", ch+11);
   printf("  2: %08x Vendor: '%s'\n", header[2], field);

   i = 11+size;
   ptr = ch+i;
   size = *(uint32_t*)ptr;
// printf("User comment list length: %i\n", size);

   ptr += 4;
   for (i=0; i<size; i++)
   {
      size_t slen = *(uint32_t*)ptr;
      ptr += 4;
      snprintf(field, _MIN(slen+1, COMMENT_SIZE), "%s", ptr);
      printf("\t'%s'\n", field);
      ptr += slen;
   }
   ptr++;
   printf("framing: %i\n", *ptr);
#endif

   field[COMMENT_SIZE] = 0;

   size = (header[1] >> 24) | (header[2] << 8);
   snprintf(field, _MIN(size+1, COMMENT_SIZE), "%s", ch+11);
// handle->vendor = strdup(field);

   i = 11+size;
   ptr = ch+i;
   size = *(uint32_t*)ptr;

   ptr += 4;
   for (i=0; i<size; i++)
   {
      uint32_t slen = *(uint32_t*)ptr;

      ptr += sizeof(uint32_t);
      if ((size_t)(ptr+slen-ch) > len) {
          return __F_NEED_MORE;
      }

      snprintf(field, _MIN(slen+1, COMMENT_SIZE), "%s", ptr);
      ptr += slen;

      if (!STRCMP(field, "TITLE"))
      {
          handle->title = stradd(handle->title, field+strlen("TITLE="));
          handle->title_changed = AAX_TRUE;
      }
      else if (!STRCMP(field, "ARTIST")) {
         handle->artist = stradd(handle->artist, field+strlen("ARTIST="));
         handle->artist_changed = AAX_TRUE;
      }
//    else if (!STRCMP(field, "PERFORMER"))
//    {
//        handle->artist = stradd(handle->artist, field+strlen("PERFORMER="));
//        handle->artist_changed = AAX_TRUE;
//    }
      else if (!STRCMP(field, "ALBUM")) {
          handle->album = stradd(handle->album, field+strlen("ALBUM="));
      } else if (!STRCMP(field, "TRACKNUMBER")) {
          handle->trackno = stradd(handle->trackno, field+strlen("TRACKNUMBER="));
      } else if (!STRCMP(field, "TRACK")) {
          handle->trackno = stradd(handle->trackno, field+strlen("TRACK="));
      } else if (!STRCMP(field, "COPYRIGHT")) {
          handle->copyright = stradd(handle->copyright, field+strlen("COPYRIGHT="));
      } else if (!STRCMP(field, "GENRE")) {
          handle->genre = stradd(handle->genre, field+strlen("GENRE="));
      } else if (!STRCMP(field, "DATE")) {
          handle->date = stradd(handle->date, field+strlen("DATE="));
      } else if (!STRCMP(field, "CONTACT")) {
          handle->website = stradd(handle->website, field+strlen("CONTACT="));
      } else if (!STRCMP(field, "DESCRIPTION")) {
          handle->comments = stradd(handle->comments, field+strlen("DESCRIPTION="));
      }
      // REPLAYGAIN_TRACK_PEAK
      // REPLAYGAIN_ALBUM_GAIN
      // REPLAYGAIN_ALBUM_PEAK
      else if (!STRCMP(field, "REPLAYGAIN_TRACK_GAIN"))
      {
          float gain_db = atof(field+strlen("REPLAYGAIN_TRACK_GAIN="));
          handle->gain = _db2lin(gain_db);
      }
   }

   ptr++;
   rv = ptr-ch;

   return rv;
}

static int
_aaxFormatDriverReadHeader(_driver_t *handle)
{
   unsigned char *header;
   size_t bufsize;
   int rv = 0;

   header = (unsigned char*)handle->oggBuffer->data;
   bufsize = handle->oggBuffer->avail;

   if (handle->page_sequence_no < 2)
   {
      do
      {
         rv = _getOggPageHeader(handle, (uint32_t*)header, bufsize);
         if ((rv >= 0) && (handle->segment_size > 0) &&
             (handle->page_sequence_no < 2))
         {
            unsigned char *segment = (unsigned char*)header + rv;
            size_t segment_size = handle->segment_size;
            size_t page_size = handle->page_size;

            /*
             * https://tools.ietf.org/html/rfc3533.html#section-6
             * As Ogg pages have a maximum size of about 64 kBytes, sometimes a
             * packet has to be distributed over several pages. To simplify that
             * process, Ogg divides each packet into 255 byte long chunks plus a
             * final shorter chunk.  These chunks are called "Ogg Segments".
             *
             * They are only a logical construct and do not have a header for
             * themselves.
             */
            if (bufsize >= page_size)
            {
               /*
                * The packets must occur in the order of identification,
                * comment (and setup for Vorbis).
                */
               switch(handle->page_sequence_no)
               {
               case 0: // HEADER_IDENTIFICATION
                  rv = _getOggIdentification(handle, segment, segment_size);

                  if (rv > 0)
                  {
                     if (handle->keep_header) {
                        _aaxOggInitFormat(handle, header, &page_size);
                     } else {
                        _aaxOggInitFormat(handle, segment, &segment_size);
                        if (!segment_size) page_size = 0;
                     }

                     // remove the page from the stream
                     if (page_size)
                     {
                        _aaxDataMove(handle->oggBuffer, NULL, page_size);
                        handle->page_size -= page_size;
                        bufsize -= page_size;
                     }
                     else {
                        rv = __F_NEED_MORE;
                     }
                  }
                  else {
                     handle->page_size = 0;
                  }
                  break;
               case 1: // HEADER_COMMENT
                  switch(handle->format_type)
                  {
                     case _FMT_VORBIS:
                     rv = _getOggVorbisComment(handle, segment, segment_size);
                     break;
                  case _FMT_OPUS:
                     rv = _getOggOpusComment(handle, segment, segment_size);
                     break;
                  default:
                     rv = __F_EOF;
                     break;
                  }

                  if (rv > 0)
                  {
                     if (handle->keep_header) {
                        handle->fmt->open(handle->fmt, header, &page_size,
                                          handle->datasize);
                     } else {
                        handle->fmt->open(handle->fmt, segment, &segment_size,
                                          handle->datasize);
                        if (!segment_size) page_size = 0;
                     }

                     // remove the page from the stream
                     if (page_size)
                     {
                        _aaxDataMove(handle->oggBuffer, NULL, page_size);
                        handle->page_size -= page_size;
                        bufsize -= page_size;
                     }
                     else {
                        rv = __F_NEED_MORE;
                     }
                  }
                  else {
                     handle->page_size = 0;
                  }
                  break;
               default:
                  rv = __F_EOF;
                  break;
               }
            }
            else { /* (bufsize >= segment_size) */
               rv = __F_NEED_MORE;
               handle->page_size = 0;
            }
         }
         else if (handle->segment_size == 0) {
            rv = __F_EOF;
         }
         else {
            rv = __F_NEED_MORE;
         }
      }
      while ((rv > 0) && (handle->page_sequence_no < 1));
   }

   return rv;
}

