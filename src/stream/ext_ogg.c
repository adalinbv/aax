/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <time.h>
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


#include <base/databuffer.h>
#include <base/memory.h>

#include <api.h>

#include "audio.h"
#include "ext_ogg.h"


// https://xiph.org/ogg/
// https://wiki.xiph.org/index.php/Main_Page
// https://xiph.org/vorbis/doc/v-comment.html
// https://xiph.org/flac/ogg_mapping.html
// https://wiki.xiph.org/OggOpus (superseded by RFC-7845)
//  -- https://tools.ietf.org/html/rfc7845.html
// https://wiki.xiph.org/OggPCM (listed under abandonware)

// Comment tags:
// http://web.archive.org/web/20101021085402/http://reallylongword.org/vorbiscomment/
// http://web.archive.org/web/20040401200215/reactor-core.org/ogg-tag-recommendations.html

#define OGG_CALCULATE_CRC	1
#define OGG_WRITE_SAMPLES	1024
#define OGG_WRITE_PACKET_SIZE	(OGG_WRITE_SAMPLES*sizeof(float)*handle->no_tracks)
#define OGG_WRITE_BUFFER_SIZE	(2*OGG_WRITE_PACKET_SIZE)
#define OGG_HEADER_SIZE		8

typedef struct
{
   ogg_stream_state os;
   ogg_page og;
   ogg_packet op;
} _driver_write_t;

typedef struct
{
   void *id;

   struct _meta_t meta;

   _fmt_t *fmt;

   int capturing;
   int mode;

   int no_tracks;
   int bits_sample;
   int frequency;
   int bitrate;
   int bitrate_min, bitrate_max;
   enum aaxFormat format;
   size_t blocksize;
   size_t no_samples;

   _fmt_type_t format_type;
// enum oggFormat ogg_format;

   char need_more; // data

   /*
    * Indicate whether the data presented to the format code needs the Ogg
    * page header or not. If not remove it from the data stream.
    */
   char keep_ogg_header;

   /* page header information */
   char header_type;
   char continued; // packet continues on the next page
   char first_page;
   char last_page;
   uint64_t granule_position;
   uint32_t bitstream_serial_no;
   uint32_t page_sequence_no;

   uint32_t header_size;
   uint32_t page_size;
   uint32_t segment_size;

   unsigned short packet_no;
   unsigned short no_packets;
   unsigned short packet_offset[256];		// reading
   /* page header */

   _data_t *oggBuffer;
   size_t datasize;

   _driver_write_t *out;

   /* Vorbis */
   char framing;

   /* Opus */
   size_t pre_skip;
   float gain;

} _driver_t;


static int _aaxFormatDriverReadHeader(_driver_t*);
static int _getOggPageHeader(_driver_t*, uint8_t*, size_t, char);
static int _aaxOggInitFormat(_driver_t*, unsigned char*, ssize_t*);
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
DECL_FUNCTION(ogg_stream_init);
DECL_FUNCTION(ogg_stream_clear);
DECL_FUNCTION(ogg_stream_packetin);
DECL_FUNCTION(ogg_stream_pageout);
DECL_FUNCTION(ogg_stream_flush);
DECL_FUNCTION(ogg_page_eos);

static void *audio = NULL;

int
_ogg_detect(UNUSED(_ext_t *ext), int mode)
{
   int rv = false;

   if (mode) // write
   {
      audio = _aaxIsLibraryPresent("ogg", "0");
      if (audio)
      {
         char *error;

         _aaxGetSymError(0);

         TIE_FUNCTION(ogg_stream_init);
         if (pogg_stream_init)
         {
            TIE_FUNCTION(ogg_stream_clear);
            TIE_FUNCTION(ogg_stream_packetin);
            TIE_FUNCTION(ogg_stream_pageout);
            TIE_FUNCTION(ogg_stream_flush);
            TIE_FUNCTION(ogg_page_eos);

            error = _aaxGetSymError(0);
            if (!error) rv = true;
         }
      }
   }
   else {
      rv = true;
   }

   return rv;
}

int
_ogg_setup(_ext_t *ext, int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
{
   int bits_sample = aaxGetBitsPerSample(format);
   int rv = false;

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

         if (handle->capturing)
         {
            handle->no_samples = UINT_MAX;
            *bufsize = 4096;
         }
         else /* playback */
         {
            handle->format_type = format;
            handle->out = calloc(1, sizeof(_driver_write_t));
            *bufsize = sizeof(ogg_packet[3]);
         }
         ext->id = handle;

         crc32_init();
         rv = true;
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
_ogg_open(_ext_t *ext, void_ptr buf, ssize_t *bufsize, size_t fsize)
{
   _driver_t *handle = ext->id;
   void *rv = NULL;

   if (handle)
   {
      if (!handle->capturing)   /* write */
      {
         if (!handle->oggBuffer) {
            handle->oggBuffer = _aaxDataCreate(1, OGG_WRITE_BUFFER_SIZE, 1);
         }

         if (handle->oggBuffer)
         {
            ogg_packet header[3];
            ssize_t size = sizeof(ogg_packet[3]);

            if (!handle->fmt) {
               handle->fmt = _fmt_create(handle->format_type, handle->mode);
            }
            if (!handle->fmt) {
               return rv;
            }

            srand(time(NULL));
            pogg_stream_init(&handle->out->os, rand());

            // create the handle
            handle->fmt->open(handle->fmt, handle->mode, NULL, NULL, 0);
            if (!handle->fmt->setup(handle->fmt, handle->format_type, handle->format))
            {
               handle->fmt = _fmt_free(handle->fmt);
               return rv;
            }

            handle->blocksize = handle->no_tracks*sizeof(float);
            handle->fmt->set(handle->fmt, __F_FREQUENCY, handle->frequency);
            handle->fmt->set(handle->fmt, __F_BITRATE, handle->bitrate);
            handle->fmt->set(handle->fmt, __F_TRACKS, handle->no_tracks);
            handle->fmt->set(handle->fmt, __F_NO_SAMPLES, handle->no_samples);
//          handle->fmt->set(handle->fmt, __F_BLOCK_SIZE, handle->blocksize);

            // open the format layer
            rv = handle->fmt->open(handle->fmt, handle->mode, &header, &size,
                                   fsize);
            if (size == sizeof(ogg_packet[3]))
            {
               int res = 1;

               pogg_stream_packetin(&handle->out->os, &header[0]);
               pogg_stream_packetin(&handle->out->os, &header[1]); // comm
               pogg_stream_packetin(&handle->out->os, &header[2]); // code

               size = 0;
               while (pogg_stream_flush(&handle->out->os, &handle->out->og))
               {
                  if (res && handle->out->og.header_len) {
                     res = _aaxDataAdd(handle->oggBuffer, 0,
                                       handle->out->og.header,
                                       handle->out->og.header_len);
                     size += res;
                  }

                  if (res && handle->out->og.body_len) {
                     res = _aaxDataAdd(handle->oggBuffer, 0,
                                       handle->out->og.body,
                                       handle->out->og.body_len);
                     size += res;
                  }

                  if (!res) break;
               }

               if (res) {
                  *bufsize = size;
               }
               else
               {
                  _AAX_FILEDRVLOG("OGG: Insufficient buffer size");
                  *bufsize = 0;
               }
            }
         }
         else
         {
            _AAX_FILEDRVLOG("OGG: Insufficient memory");
            return rv;
         }
      }
			/* read: handle->capturing */
      else if (!handle->fmt || !handle->fmt->open)
      {
         assert(bufsize);

         if (!handle->oggBuffer) {
            handle->oggBuffer = _aaxDataCreate(1, OGG_WRITE_BUFFER_SIZE, 1);
         }

         if (handle->oggBuffer)
         {
            size_t size = *bufsize;
            int res;

            handle->datasize = fsize;
            res = _aaxDataAdd(handle->oggBuffer, 0, buf, size);
            *bufsize = res;

            res = _aaxFormatDriverReadHeader(handle);
            if (res <= 0)
            {
               if (res == __F_EOF) *bufsize = 0;
               rv = buf;
            }
         }
         else
         {
            _AAX_FILEDRVLOG("OGG: Insufficient memory");
            return rv;
         }
      }
        /* Format requires more data to process it's own header */
      else if (handle->fmt && handle->fmt->open)
      {
         size_t size = *bufsize;
         int res;

         res = _aaxDataAdd(handle->oggBuffer, 0, buf, size);
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
   int res = true;

   if (handle)
   {
      _aaxDataDestroy(handle->oggBuffer);
      if (handle->fmt)
      {
         handle->fmt->close(handle->fmt);
         _fmt_free(handle->fmt);
      }

      if (handle->out)
      {
         pogg_stream_clear(&handle->out->os);
         free(handle->out);
      }

      _aax_free_meta(&handle->meta);
      free(handle);
   }

   return res;
}

void*
_ogg_update(UNUSED(_ext_t *ext), UNUSED(size_t *offs), UNUSED(ssize_t *size), UNUSED(char close))
{
   return NULL;
}

size_t
_ogg_fill(_ext_t *ext, void_ptr sptr, ssize_t *bytes)
{
   _driver_t *handle = ext->id;
   int res, rv = __F_PROCESS;
   uint8_t *header;
   ssize_t avail;

   handle->need_more = false;
   if (sptr && bytes)
   {
      res = _aaxDataAdd(handle->oggBuffer, 0, sptr, *bytes);
      *bytes = res;
   }

   // vorbis stream may reset the stream at the start of each song with
   // a packet indicated as a first page followed by a new comment page.
   header = _aaxDataGetData(handle->oggBuffer, 0);
   if (header[5] == PACKET_FIRST_PAGE || handle->page_sequence_no < 2)
   {
      if (!handle->page_size)
      {
         if (header[5] == PACKET_FIRST_PAGE)
         {
            handle->bitstream_serial_no = 0;
            handle->page_sequence_no = 0;

            _aax_free_meta(&handle->meta);

            handle->fmt->close(handle->fmt);
            _fmt_free(handle->fmt);
            handle->fmt = NULL;
         }

         rv = _aaxFormatDriverReadHeader(handle);
         if (rv <= 0)
         {
            if (rv == __F_NEED_MORE) {
               handle->need_more = true;
            }
         }
      }
   }
   else
   {
      header = _aaxDataGetData(handle->oggBuffer, 0);
      avail = _aaxDataGetDataAvail(handle->oggBuffer, 0);
      if (handle->page_size ||
          (rv = _getOggPageHeader(handle, header, avail, true)) > 0)
      {
         handle->fmt->set(handle->fmt, __F_BLOCK_SIZE, handle->page_size);

         avail = _aaxDataGetDataAvail(handle->oggBuffer, 0);
         if (avail >= handle->page_size)
         {
            avail = handle->page_size;
            rv = handle->fmt->fill(handle->fmt, header, &avail);
            if (avail)
            {
               _aaxDataMove(handle->oggBuffer, 0, NULL, avail);
               handle->page_size -= avail;
            }
         }
      }
   }

// printf("ogg_fill: %i\n", rv);
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

   if (handle->need_more)
   {
      *num = 0;
      rv = __F_NEED_MORE;
   }
   else
   {
      if (handle->keep_ogg_header)
      {
         int ret = handle->fmt->cvt_from_intl(handle->fmt, dptr, offset, num);

         if (handle->packet_no != handle->no_packets)
         {
            if (ret > 0) handle->packet_no++;
            assert(handle->packet_no <= handle->no_packets);
         }

         rv = ret;
      }
      else
      {
         int i, ret;

         rv = 0;
         for(i=0; i<handle->no_packets; i++)
         {
            size_t packetSize;

            packetSize = handle->packet_offset[i+1] - handle->packet_offset[i];
            handle->fmt->set(handle->fmt, __F_BLOCK_SIZE, packetSize);

            ret = handle->fmt->cvt_from_intl(handle->fmt, dptr, offset, num);
            rv += ret;
         }
      }
   }

// printf("ogg_cvt_from: %li\n", rv);
   return rv;
}

size_t
_ogg_cvt_to_intl(_ext_t *ext, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = ext->id;
   _driver_write_t *out = handle->out;
   size_t inbytes = *num*handle->blocksize;
   size_t bufsize, outsize = inbytes;
   char *buf = (char*)dptr;
   size_t rv = 0;
   size_t res;

   // main loop: convert from PCM to an OGG/vorbis-stream
   // vorbis analysis buffer
   size_t size = *num;
   res = handle->fmt->cvt_to_intl(handle->fmt, NULL, sptr, offs, &size,
                                  &out->op, sizeof(ogg_packet));
   if (res) {
      pogg_stream_packetin(&out->os, &out->op);
   }

   do
   {
      bufsize = _aaxDataGetDataAvail(handle->oggBuffer, 0);
      res = _MIN(outsize, bufsize);
      if (res)
      {
         _aaxDataMove(handle->oggBuffer, 0, buf, res);
         buf += res;
         outsize -= res;
         rv += res;
      }

      // inner loop
      if (pogg_stream_pageout(&out->os, &out->og))
      {
         _aaxDataAdd(handle->oggBuffer, 0, out->og.header,
                                        out->og.header_len);
         _aaxDataAdd(handle->oggBuffer, 0, out->og.body,
                                        out->og.body_len);
      }

      if (!pogg_page_eos(&out->og))
      {
         // vorbis bitrate flushpacket
         if (handle->fmt->cvt_to_intl(handle->fmt, NULL, NULL, 0, NULL,
                                 &out->op, sizeof(ogg_packet)))
         {
            pogg_stream_packetin(&out->os, &out->op);
         }
         else
         {
            // vorbis analysis blockout
            if (!handle->fmt->cvt_to_intl(handle->fmt, NULL, NULL, 0,
                                                       NULL, NULL, 0))
            {
               break;
            }
         }
      }
      else
      {
         *num = rv/handle->blocksize;
         rv = __F_EOF;
         break;
      }
   }
   while (outsize);

   if (rv >= 0) *num = rv/handle->blocksize;

   return rv;
}

int
_ogg_set_name(_ext_t *ext, enum _aaxStreamParam param, const char *desc)
{
   _driver_t *handle = ext->id;
   int rv = handle->fmt->set_name(handle->fmt, param, desc);

   if (!rv)
   {
      switch(param)
      {
      case __F_ARTIST:
         handle->meta.artist = strreplace(handle->meta.artist, desc);
         rv = true;
         break;
      case __F_TITLE:
         handle->meta.title = strreplace(handle->meta.title, desc);
         rv = true;
         break;
      case __F_GENRE:
         handle->meta.genre = strreplace(handle->meta.genre, desc);
         rv = true;
         break;
      case __F_TRACKNO:
         handle->meta.trackno = strreplace(handle->meta.trackno, desc);
         rv = true;
         break;
      case __F_ALBUM:
         handle->meta.album = strreplace(handle->meta.album, desc);
         rv = true;
         break;
      case __F_DATE:
         handle->meta.date = strreplace(handle->meta.date, desc);
         rv = true;
         break;
      case __F_COMMENT:
         handle->meta.comments = strreplace(handle->meta.comments, desc);
         rv = true;
         break;
      case __F_COPYRIGHT:
         handle->meta.copyright = strreplace(handle->meta.copyright, desc);
         rv = true;
         break;
      case __F_COMPOSER:
         handle->meta.composer = strreplace(handle->meta.composer, desc);
         rv = true;
         break;
      case __F_ORIGINAL:
         handle->meta.original = strreplace(handle->meta.original, desc);
         rv = true;
         break;
      case __F_WEBSITE:
         handle->meta.website = strreplace(handle->meta.website, desc);
         rv = true;
         break;
      default:
         break;
      }
   }
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
         rv = handle->meta.artist;
         handle->meta.artist_changed = false;
         break;
      case __F_TITLE:
         rv = handle->meta.title;
         handle->meta.title_changed = false;
         break;
      case __F_GENRE:
         rv = handle->meta.genre;
         break;
      case __F_TRACKNO:
         rv = handle->meta.trackno;
         break;
      case __F_ALBUM:
         rv = handle->meta.album;
         break;
      case __F_DATE:
         rv = handle->meta.date;
         break;
      case __F_COMPOSER:
         rv = handle->meta.composer;
         break;
      case __F_COMMENT:
         rv = handle->meta.comments;
         break;
      case __F_COPYRIGHT:
         rv = handle->meta.copyright;
         break;
      case __F_ORIGINAL:
         rv = handle->meta.original;
         break;
      case __F_WEBSITE:
         rv = handle->meta.website;
         break;
      default:
         if (param & __F_NAME_CHANGED)
         {
            switch (param & ~__F_NAME_CHANGED)
            {
            case __F_ARTIST:
               if (handle->meta.artist_changed)
               {
                  rv = handle->meta.artist;
                  handle->meta.artist_changed = false;
               }
               break;
            case __F_TITLE:
               if (handle->meta.title_changed)
               {
                  rv = handle->meta.title;
                  handle->meta.title_changed = false;
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
   static const char *ogg_exts[_EXT_PCM - _EXT_OGG] = {
      "*.ogg", "*.opus"
   };
   static char *rd[2][_EXT_PCM - _EXT_OGG] = {
      { NULL, NULL },
      { NULL, NULL }
   };
   char *rv = NULL;

   if (ext >= _EXT_OGG && ext < _EXT_PCM)
   {
      int m = mode > 0 ? 1 : 0;
      int pos = ext - _EXT_OGG;

      if (rd[m][pos] == NULL)
      {
         int format = _FMT_NONE;

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
            rd[m][pos] = (char*)ogg_exts[pos];
         }
      }
      rv = rd[mode][pos];
   }


   return rv;
}

int
_ogg_extension(char *ext)
{
   int rv = _FMT_NONE;

   if (ext)
   {
      if (!strcasecmp(ext, "ogg") || !strcasecmp(ext, "oga")) rv = _FMT_VORBIS;
      else if (!strcasecmp(ext, "opus")) rv = _FMT_OPUS;
//    else if (!strcasecmp(ext, "ogx") || !strcasecmp(ext, "spx"))
   }
   return rv;
}

float
_ogg_get(_ext_t *ext, int type)
{
   _driver_t *handle = ext->id;
   float rv = 0.0f;

   if (handle->fmt) {
      rv = handle->fmt->get(handle->fmt, type);
   }
   if (type == __F_NO_SAMPLES && handle->no_samples == -handle->pre_skip) {
      rv = -1;
   }
   return rv;
}

float
_ogg_set(_ext_t *ext, int type, float value)
{
   _driver_t *handle = ext->id;
   float rv = 0.0f;

   if (handle->fmt) {
      rv = handle->fmt->set(handle->fmt, type, value);
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

static int _getOggOpusComment(_driver_t*, unsigned char*, size_t);
static int _getOggVorbisComment(_driver_t*, unsigned char*, size_t);

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

static int
_aaxOggInitFormat(_driver_t *handle, unsigned char *oggbuf, ssize_t *bufsize)
{
   int rv = 0;
   if (!handle->fmt)
   {
      _fmt_type_t fmt = handle->format_type;

      handle->fmt = _fmt_create(handle->format_type, handle->mode);
      if (!handle->fmt)
      {
         *bufsize = 0;
         return __F_EOF;
      }

      handle->fmt->open(handle->fmt, handle->mode, NULL, NULL, 0);
      if (!handle->fmt->setup(handle->fmt, fmt, handle->format))
      {
         handle->fmt = _fmt_free(handle->fmt);
         *bufsize = 0;
         return __F_EOF;
      }

      handle->fmt->set(handle->fmt, __F_FREQUENCY, handle->frequency);
      handle->fmt->set(handle->fmt, __F_BITRATE, handle->bitrate);
      handle->fmt->set(handle->fmt, __F_TRACKS, handle->no_tracks);
      handle->fmt->set(handle->fmt, __F_NO_SAMPLES, handle->no_samples);
      handle->fmt->set(handle->fmt, __F_BITS_PER_SAMPLE, handle->bits_sample);

//    handle->fmt->set(handle->fmt, __F_BLOCK_SIZE, handle->blocksize);
//    handle->fmt->set(handle->fmt, __F_POSITION,
//                                     handle->blockbufpos);
   }

   if (handle->fmt)
   {
      if (handle->keep_ogg_header) {
         handle->fmt->open(handle->fmt, handle->mode, oggbuf, bufsize,
                           handle->datasize);
      }
      else
      {
         ssize_t size = 0;
         handle->fmt->open(handle->fmt, handle->mode, oggbuf, &size,
                           handle->datasize);
      }
   }

   return rv;
}

// https://www.ietf.org/rfc/rfc3533.txt
static int
_getOggPageHeader(_driver_t *handle, uint8_t *header, size_t size, char remove_header)
{
   size_t bufsize = _aaxDataGetDataAvail(handle->oggBuffer, 0);
   uint8_t *ch = header;
   int rv = 0;

   if (bufsize < OGG_HEADER_SIZE || size < OGG_HEADER_SIZE) {
      return __F_NEED_MORE;
   }

#if 0
{
   unsigned int i;
   uint64_t i64;
   uint32_t i32;

   ch = (uint8_t*)header;
   printf("Read Header:\n");

   printf("0: %08x (Magic number: \"%c%c%c%c\")\n", header[0], ch[0], ch[1], ch[2], ch[3]);

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

   ch = (uint8_t*)header;
   if (ch[0] != 'O' || ch[1] != 'g' || ch[2] != 'g' || ch[3] != 'S')
   {
      char *c = strncasestr((char*)ch, "OggS", size);
      if (!c)
      {
         int i;
         printf("OggS header not found, len: %zu\n", size);
         for (i=0; i<10; ++i) {
            printf("%c ", ch[i]);
         }
         printf("\n");
      }
      else
      {
         printf("Found OggS header at offset: %zi\n", c-(char*)ch);
         ch = (uint8_t*)c;
      }
   }

   if (*ch++ == 'O' && *ch++ == 'g' && *ch++ == 'g' && *ch++ == 'S')
   {
      int32_t version, crc32, serial_no, no_segments, sequence_no;

      version = read8(&ch, &bufsize);

      handle->header_type = read8(&ch, &bufsize);
      handle->first_page = handle->header_type & PACKET_FIRST_PAGE;
      handle->last_page = handle->header_type & PACKET_LAST_PAGE;
      handle->continued = handle->header_type & PACKET_CONTINUED;

      handle->granule_position = read64le(&ch, &bufsize);

      serial_no = read32le(&ch, &bufsize);
      if (!handle->bitstream_serial_no) {
         handle->bitstream_serial_no = serial_no;
      }

      sequence_no = read32le(&ch, &bufsize);
      crc32 = read32le(&ch, &bufsize);

      no_segments = read8(&ch, &bufsize);
      handle->header_size = 27 + no_segments;

      if ((bufsize >= handle->header_size) && (version == 0x0))
      {
         if (serial_no == handle->bitstream_serial_no)
         {
            if (!handle->page_sequence_no ||
                sequence_no > handle->page_sequence_no)
            {
               unsigned int i;

               handle->page_sequence_no = sequence_no;

               if (no_segments > 0)
               {
                  unsigned int p = 0;

                  handle->no_packets = 0;
                  handle->segment_size = 0;
                  handle->packet_offset[p] = 0;
                  for (i=0; i<no_segments; ++i)
                  {
                     unsigned char ps = *ch++;

                     handle->segment_size += ps;
                     handle->packet_offset[p] += ps;
                     if (ps < 0xFF)
                     {
                        handle->packet_offset[++p] = handle->segment_size;
                        handle->no_packets++;
                     }
                  }
                  handle->packet_no = 0;
                  handle->page_size = handle->header_size+handle->segment_size;
#if 0
  printf("no. packets: %i\n", handle->no_packets);
  for(i=0; i<handle->no_packets; i++)
  printf(" %i: %u\n", i, handle->packet_offset[i+1] - handle->packet_offset[i]);
#endif

#if OGG_CALCULATE_CRC
                  if (handle->page_size <= bufsize)
                  {
                     for (i=0; i<handle->page_size; i++) {
                        CRC32_UPDATE(crc32, ch[i]);
                     }
                  }
                  else {
                     handle->page_sequence_no--;
                  }
#else
                  if (handle->page_size > bufsize) {
                     handle->page_sequence_no--;
                  }
#endif
                  if (remove_header && !handle->keep_ogg_header)
                  {
                     size_t hs = handle->header_size;
                     _aaxDataMove(handle->oggBuffer, 0, NULL, hs);
                     handle->page_size -= hs;
                  }

                  rv = handle->header_size;
               }
               else {
                  rv = __F_EOF;
               }
            }
#if 0
{
 static int prev_seq_no = -1;
 if (handle->page_sequence_no != prev_seq_no)
 {
   uint64_t i64;
   unsigned int i;
   ch = (uint8_t*)header;
   printf("Read Header:\n");
   printf("0: %08x (Magic number: \"%c%c%c%c\")\n", header[0], ch[0], ch[1], ch[2], ch[3]);
   printf("1: %08x (Version: %i | Type: 0x%x: sos: %s, eos: %s, continued: %s)\n", header[1], version, handle->header_type,
     handle->first_page ? "true" : "false",
     handle->last_page ? "true" : "false",
     handle->continued ? "true" : "false");
   printf("2: %08x (Granule position: %zu)\n", header[2], handle->granule_position);
   printf("3: %08x (Serial number: %08x)\n", header[3], serial_no);
   printf("4: %08x (Sequence number: %08x)\n", header[4], handle->page_sequence_no);
   printf("5: %08x (CRC cehcksum: %08x)\n", header[5], crc32);

   i64 = 0;
   for (i=0; i<no_segments; ++i) {
      i64 += (uint8_t)ch[27+i];
   }
   printf("6: %08x (Page segments: %i, Total segment size: %zi)\n", header[6], no_segments, i64);
   prev_seq_no = handle->page_sequence_no;
 }
}
#endif
         }
         else	// SKIP page due to incorrect serial number
         {
            unsigned char *ch = (unsigned char*)header;
            unsigned int i, no_segments = (header[6] >> 16) & 0xFF;

            rv = 0;
            for (i=0; i<no_segments; ++i) {
               rv += ch[27+i];
            }

            if (rv <= (int)bufsize)
            {
               _aaxDataMove(handle->oggBuffer, 0, NULL, rv);
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
#define VORBIS_ID_HEADER_SIZE	30
static int
_aaxFormatDriverReadVorbisHeader(_driver_t *handle, unsigned char *h, size_t len)
{
   int rv = __F_EOF;

   if (len >= VORBIS_ID_HEADER_SIZE)
   {
      uint8_t *ch = h;
      int type = read8(&ch, &len);

      if (type == HEADER_IDENTIFICATION && *ch++ == 'v' && *ch++ == 'o' &&
          *ch++ == 'r' && *ch++ == 'b' && *ch++ == 'i' && *ch++ == 's')
      {
         uint32_t version;
#if 0
   uint32_t *header = (uint32_t*)h;
   printf("\n--Vorbis Identification Header:\n");
   printf("  0: %08x (Type: %x)\n", header[0], h[0]);
   printf("  1: %08x (Codec identifier \"%c%c%c%c%c%c\")\n", header[1], h[1], h[2], h[3], h[4], h[5], h[6]);
   printf("  7: %08x (version %i, channels: %u)\n", header[2], (header[2] << 8) | (header[3] >> 24), header[2] >> 24);
   printf(" 12: %08x (Sample rate: %u)\n", header[3], header[3]);
   printf(" 16: %08x (Max. bitrate: %u)\n", header[4], header[4]);
   printf(" 20: %08x (Nom. bitrate: %u)\n", header[5], header[5]);
   printf(" 24: %08x (Min. bitrate: %u)\n", header[6], header[6]);
   printf(" 28: %01x  %01x (block size: %u - %u, framing: %u)\n", h[28], h[29], 1 << (h[28] & 0xF), 1 << (h[28] >> 4), h[29]);
#endif
         version = read32le(&ch, &len);
         if (version == 0x0)
         {
            unsigned int blocksize1;
            uint32_t i32;

            handle->format = AAX_PCM24S;
            handle->no_tracks = read8(&ch, &len);
            handle->frequency = read32le(&ch, &len);
            handle->bitrate_max = read32le(&ch, &len);
            handle->bitrate = read32le(&ch, &len);
            handle->bitrate_min = read32le(&ch, &len);

            i32 = read8(&ch, &len);
            handle->blocksize = 1 << (i32 & 0xF);
            blocksize1 = 1 << (i32 >> 4);
            handle->framing = read8(&ch, &len) & 0x1;

            if (handle->no_tracks <= 0 || handle->frequency <= 0 ||
                (handle->blocksize > blocksize1))
            {
               // Failure to meet any of these conditions renders a
               // stream undecodable.
               _AAX_FILEDRVLOG("OGG: Invalid Vorbis stream");
#if 0
 printf("no_tracks: %i, frequency: %i, blocksize: %li (%i)\n", handle->no_tracks, handle->frequency, handle->blocksize, blocksize1);
 printf("(header[7] >> 16) & 0x1: %i\n", (header[7] >> 16) & 0x1);
#endif
               return __F_EOF;
            }
#if 0
   printf("\n--Vorbis Identification Header:\n");
   printf("  0: %08x (Type: %x)\n", header[0], type);
   printf("  1: %08x (Codec identifier \"%c%c%c%c%c%c\")\n", header[1], h[1], h[2], h[3], h[4], h[5], h[6]);
   printf("  7: %08x (version %i, channels: %u)\n", header[2], version, handle->no_tracks);
   printf(" 12: %08x (Sample rate: %u)\n", header[3], handle->frequency);
   printf(" 16: %08x (Max. bitrate: %u)\n", header[4], handle->bitrate_max);
   printf(" 20: %08x (Nom. bitrate: %u)\n", header[5], handle->bitrate);
   printf(" 24: %08x (Min. bitrate: %u)\n", header[6], handle->bitrate_min);
   printf(" 28: %01x  %01x (block size: %lu - %u, framing: %u)\n", h[28], h[29], handle->blocksize, blocksize1, handle->framing);
#endif
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
   uint8_t *ch = (uint8_t*)h;
   int rv = __F_EOF;

   if (len >= OPUS_ID_HEADER_SIZE && *ch++ == 'O' && *ch++ == 'p' &&
       *ch++ == 'u' && *ch++ == 's' && *ch++ == 'H' && *ch++ == 'e' &&
       *ch++ == 'a' && *ch++ == 'd')
   {
      unsigned char mapping_family = 0;
      int stream_count = 0, gain = 0;
      int coupled_count = 0;
      int version = read8(&ch, &len);
      if (version == 1)
      {
         handle->format = AAX_FLOAT;
         handle->no_tracks = read8(&ch, &len);
         handle->pre_skip = read16le(&ch, &len);
         handle->frequency = read32le(&ch, &len);
         handle->no_samples = -handle->pre_skip;

         gain = read16le(&ch, &len);
         handle->gain = pow(10, (float)gain/(20.0f*256.0f));

         mapping_family = read8(&ch, &len);
         if ((mapping_family == 0 || mapping_family == 1) &&
             (handle->no_tracks > 1) && (handle->no_tracks <= 8))
         {
             /*
              * The 'channel mapping table' MUST be omitted when the channel
              * mapping family s 0, but is REQUIRED otherwise.
              */
             if (mapping_family == 1)
             {
                 stream_count = read8(&ch, &len);
                 coupled_count = read8(&ch, &len);
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
  printf("Opus Header:\n");
  printf("  0: %08x %08x (Magic number: '%c%c%c%c%c%c%c%c')\n", header[0], header[1], h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7]);
  printf("  2: %08x (Version: %i, Tracks: %i, Pre Skip: %li)\n", header[2], version, handle->no_tracks, handle->pre_skip);
  printf("  3: %08x (Original Sample Rate: %i)\n", header[3],  handle->frequency);
  if (h[18] == 1) {
    printf("  4: %08x (Replay gain: %f, Mapping Family: %i)\n", header[4], handle->gain, mapping_family);
    printf("  5: %08x (Stream Count: %i, Coupled Count: %i)\n", header[5], stream_count, coupled_count);
  } else {
    uint32_t i = (int)h[16] << 16 | h[17] << 8 || h[18];
    printf("  4: %08x (Replay gain: %f, Mapping Family: %i)\n", i, handle->gain, mapping_family);
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
      handle->keep_ogg_header = false;
//    handle->ogg_format = FLAC_OGG_FILE;
      handle->format_type = _FMT_FLAC;
      handle->format = AAX_PCM16S;
      rv = 0;
   }
   else if ((len > 7) && !strncmp(h, "\x01vorbis", 7))
   {
      handle->keep_ogg_header = true;
      handle->format_type = _FMT_VORBIS;
//    handle->ogg_format = VORBIS_OGG_FILE;
      rv = _aaxFormatDriverReadVorbisHeader(handle, ch, len);
   }
   else if ((len > 8) && !strncmp(h, "OpusHead", 8))
   {
      handle->keep_ogg_header = false;
      handle->format_type = _FMT_OPUS;
//    handle->ogg_format = OPUS_OGG_FILE;
      rv = _aaxFormatDriverReadOpusHeader(handle, h, len);
   }
#if 0
   else if ((len > 8) && !strncmp(h, "PCM     ", 8))
   {
      handle->keep_ogg_header = false;
      handle->format_type = _FMT_PCM;
      handle->ogg_format = PCM_OGG_FILE;
      rv = _aaxFormatDriverReadPCMHeader(handle, h, len);
   }
   else if ((len > 8) && !strncmp(h, "Speex   ", 8))
   {
      handle->keep_ogg_header = false;
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
static int
_getOggOpusComment(_driver_t *handle, unsigned char *h, size_t len)
{
   char field[COMMENT_SIZE+1];
   uint8_t *ch = h;
   size_t i, size;
   int rv = len;

   if (len > 12 && *ch++ == 'O' && *ch++ == 'p' && *ch++ == 'u' &&
       *ch++ == 's' && *ch++ == 'T' && *ch++ == 'a' && *ch++ == 'g' &&
       *ch++ == 's')
   {
      size_t clen;
#if 0
 uint32_t *header = (uint32_t*)h;
 unsigned char *ptr;
 printf("Opus Comment:\n");
 printf("  0: %08x %08x (Magic number: \"%c%c%c%c%c%c%c%c\")\n", header[0], header[1], h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7]);

 size = header[2];
 snprintf(field, _MIN(size+1, COMMENT_SIZE), "%s", h+12);
 printf("  2: %08x Vendor: '%s'\n", header[2], field);

 i = 12+size;
 ptr = h+i;
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

      clen =  COMMENT_SIZE;
      size = read32le(&ch, &len);
      readstr(&ch, field, size, &clen);
//    handle->vendor = strdup(field);

      size = read32le(&ch, &len);
      for (i=0; i<size; i++)
      {
         uint32_t slen = read32le(&ch, &len);

         if ((size_t)(ch+slen-h) > len) {
            return __F_NEED_MORE;
         }

         clen = COMMENT_SIZE;
         readstr(&ch, field, slen, &clen);
         if (!STRCMP(field, "TITLE"))
         {
            handle->meta.title = stradd(handle->meta.title, field+strlen("TITLE="));
            handle->meta.title_changed = true;
         }
         else if (!STRCMP(field, "ARTIST")) {
            handle->meta.artist = stradd(handle->meta.artist, field+strlen("ARTIST="));
            handle->meta.artist_changed = true;
         }
//       else if (!STRCMP(field, "PERFORMER"))
//       {
//          handle->artist = stradd(handle->artist, field+strlen("PERFORMER="));
//          handle->artist_changed = true;
//       }
         else if (!STRCMP(field, "ALBUM")) {
            handle->meta.album = stradd(handle->meta.album, field+strlen("ALBUM="));
         } else if (!STRCMP(field, "TRACKNUMBER")) {
            handle->meta.trackno = stradd(handle->meta.trackno, field+strlen("TRACKNUMBER="));
         } else if (!STRCMP(field, "COPYRIGHT")) {
            handle->meta.copyright = stradd(handle->meta.copyright, field+strlen("COPYRIGHT="));
         } else if (!STRCMP(field, "GENRE")) {
            handle->meta.genre = stradd(handle->meta.genre, field+strlen("GENRE="));
         } else if (!STRCMP(field, "DATE")) {
            handle->meta.date = stradd(handle->meta.date, field+strlen("DATE="));
         } else if (!STRCMP(field, "CONTACT")) {
            handle->meta.website = stradd(handle->meta.website, field+strlen("CONTACT="));
         } else if (!STRCMP(field, "DESCRIPTION")) {
            handle->meta.comments = stradd(handle->meta.comments, field+strlen("DESCRIPTION="));
         }
         else if (!STRCMP(field, "R128_TRACK_GAIN"))
         {
            int gain = atoi(field+strlen("R128_TRACK_GAIN="));
            handle->gain = pow(10, (float)gain/(20.0f*256.0f));
         }
         else {
            handle->meta.comments = stradd(handle->meta.comments, field);
         }
      }
      rv = ch-h;
   }

   return rv;
}

static int
_getOggVorbisComment(_driver_t *handle, unsigned char *h, size_t len)
{
   char field[COMMENT_SIZE+1];
   uint8_t *ch = h;
   size_t i, size;
   int rv = len;

   // 3 is the packet number for Vorbis Comments
   if (len > 12 && *ch++ == 3 && *ch++ == 'v' && *ch++ == 'o' && *ch++ == 'r' &&
       *ch++ == 'b' && *ch++ == 'i' && *ch++ == 's')
   {
      size_t clen;
#if 0
 uint32_t *header = (uint32_t*)h;
 unsigned char *ptr;
 printf("\n--Vorbis Comment Header:\n");
 printf("  0: %08x %08x (packet no.: %i, \"%c%c%c%c%c%c\")\n", header[0], header[1], h[0], h[1], h[2], h[3], h[4], h[5], h[6]);

 size = (header[1] >> 24) | (header[2] << 8);
 snprintf(field, _MIN(size+1, COMMENT_SIZE), "%s", h+11);
 printf("  2: %08x Vendor: '%s'\n", header[2], field);

 i = 11+size;
 ptr = h+i;
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
 printf("framing: %i\n", *ptr & 0x1);
#endif

      field[COMMENT_SIZE] = 0;

      clen = COMMENT_SIZE;
      size = read32le(&ch, &len);
      readstr(&ch, field, size, &clen);
//    handle->vendor = strdup(field);

      size = read32le(&ch, &len);
      for (i=0; i<size; i++)
      {
         uint32_t slen = read32le(&ch, &len);

         if ((size_t)(ch+slen-h) > len) {
            return __F_NEED_MORE;
         }

         clen = COMMENT_SIZE;
         readstr(&ch, field, slen, &clen);
         if (!STRCMP(field, "TITLE"))
         {
             handle->meta.title = stradd(handle->meta.title, field+strlen("TITLE="));
             handle->meta.title_changed = true;
         }
         else if (!STRCMP(field, "ARTIST")) {
            handle->meta.artist = stradd(handle->meta.artist, field+strlen("ARTIST="));
            handle->meta.artist_changed = true;
         }
//       else if (!STRCMP(field, "PERFORMER"))
//       {
//           handle->meta.artist = stradd(handle->meta.artist, field+strlen("PERFORMER="));
//           handle->meta.artist_changed = true;
//       }
         else if (!STRCMP(field, "ALBUM")) {
             handle->meta.album = stradd(handle->meta.album, field+strlen("ALBUM="));
         } else if (!STRCMP(field, "TRACKNUMBER")) {
             handle->meta.trackno = stradd(handle->meta.trackno, field+strlen("TRACKNUMBER="));
         } else if (!STRCMP(field, "TRACK")) {
             handle->meta.trackno = stradd(handle->meta.trackno, field+strlen("TRACK="));
         } else if (!STRCMP(field, "COPYRIGHT")) {
             handle->meta.copyright = stradd(handle->meta.copyright, field+strlen("COPYRIGHT="));
         } else if (!STRCMP(field, "GENRE")) {
             handle->meta.genre = stradd(handle->meta.genre, field+strlen("GENRE="));
         } else if (!STRCMP(field, "DATE")) {
             handle->meta.date = stradd(handle->meta.date, field+strlen("DATE="));
         } else if (!STRCMP(field, "CONTACT")) {
             handle->meta.website = stradd(handle->meta.website, field+strlen("CONTACT="));
         } else if (!STRCMP(field, "DESCRIPTION")) {
             handle->meta.comments = stradd(handle->meta.comments, field+strlen("DESCRIPTION="));
         }
         // REPLAYGAIN_TRACK_PEAK
         // REPLAYGAIN_ALBUM_GAIN
         // REPLAYGAIN_ALBUM_PEAK
         else if (!STRCMP(field, "REPLAYGAIN_TRACK_GAIN"))
         {
             float gain_db = atof(field+strlen("REPLAYGAIN_TRACK_GAIN="));
             handle->gain = _db2lin(gain_db);
         }
         else {
            handle->meta.comments = stradd(handle->meta.comments, field);
         }
      }
   }

   rv = ch-h+1;

   return rv;
}

static int
_aaxFormatDriverReadHeader(_driver_t *handle)
{
   uint8_t *header;
   size_t bufsize;
   int rv = 0;

   header = _aaxDataGetData(handle->oggBuffer, 0);
   bufsize = _aaxDataGetDataAvail(handle->oggBuffer, 0);

   if (handle->page_sequence_no < 2)
   {
      do
      {
         rv = _getOggPageHeader(handle, header, bufsize, false);
         if ((rv >= 0) && (handle->segment_size > 0) &&
             (handle->page_sequence_no < 2))
         {
            unsigned char *segment = (unsigned char*)header + rv;
            ssize_t segment_size = handle->segment_size;
            ssize_t page_size = handle->page_size;

            /*
             * https://tools.ietf.org/html/rfc3533.html#section-6
             * As Ogg pages have a maximum size of about 64 kBytes, sometimes a
             * packet has to be distributed over several pages.
             *
             * if handle->continued is true then a packet spans across pages.
             *
             * To simplify that process, Ogg divides each packet into 255 byte
             * long chunks plus a final shorter chunk.  These chunks are called
             * "Ogg Segments".
             *
             * They are only a logical construct and do not have a header for
             * themselves.
             */
            if (bufsize >= page_size)
            {
               /*
                * The packets must occur in the order of identification packet,
                * followed by the comment packet (and setup packet for Vorbis).
                */
               switch(handle->page_sequence_no)
               {
               case 0: // HEADER_IDENTIFICATION
                  rv = _getOggIdentification(handle, segment, segment_size);

                  if (rv >= 0)
                  {
                     if (handle->keep_ogg_header) {
                        rv = _aaxOggInitFormat(handle, header, &page_size);
                     } else {
                        rv = _aaxOggInitFormat(handle, segment, &segment_size);
                        if (!segment_size) page_size = 0;
                     }

                     // remove the page from the stream
                     if (page_size)
                     {
                        _aaxDataMove(handle->oggBuffer, 0, NULL, page_size);
                        handle->page_size -= page_size;
                        bufsize = _aaxDataGetDataAvail(handle->oggBuffer, 0);
                     }
                     else if (rv > 0) {
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

                  if (rv >= 0)
                  {
                     void *buf;
                     if (handle->keep_ogg_header) {
                        buf = handle->fmt->open(handle->fmt, handle->mode,
                                                header, &page_size,
                                                handle->datasize);
                     } else {
                        segment += rv;
                        segment_size -= rv;
                        buf = handle->fmt->open(handle->fmt, handle->mode,
                                                segment, &segment_size,
                                                handle->datasize);
                        if (!segment_size) page_size = 0;
                     }

                     // remove the page from the stream
                     if (!buf && page_size)
                     {
                        _aaxDataMove(handle->oggBuffer, 0, NULL, page_size);
                        handle->page_size -= page_size;
                        bufsize = _aaxDataGetDataAvail(handle->oggBuffer, 0);
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
         else if (!handle->keep_ogg_header && handle->page_sequence_no > 1)
         {
            handle->page_size -= rv;
            _aaxDataMove(handle->oggBuffer, 0, NULL, rv);
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

