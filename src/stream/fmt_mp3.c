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

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif

#include <xml.h>

#include <base/dlsym.h>

#include <api.h>
#include <arch.h>

#include "extension.h"
#include "format.h"
#include "fmt_mp3.h"

// libmpg123 for mp3 input
// liblame for mp3 output
// both Linux and Windows
DECL_FUNCTION(mpg123_init);
DECL_FUNCTION(mpg123_exit);
DECL_FUNCTION(mpg123_new);
DECL_FUNCTION(mpg123_param);
DECL_FUNCTION(mpg123_open_feed);
DECL_FUNCTION(mpg123_decode);
DECL_FUNCTION(mpg123_feed);
DECL_FUNCTION(mpg123_read);
DECL_FUNCTION(mpg123_delete);
DECL_FUNCTION(mpg123_format);
DECL_FUNCTION(mpg123_getformat);
DECL_FUNCTION(mpg123_length);
DECL_FUNCTION(mpg123_set_filesize);
DECL_FUNCTION(mpg123_feedseek);
DECL_FUNCTION(mpg123_meta_check);
DECL_FUNCTION(mpg123_id3);
DECL_FUNCTION(mpg123_plain_strerror);

DECL_FUNCTION(lame_init);
DECL_FUNCTION(lame_init_params);
DECL_FUNCTION(lame_close);
DECL_FUNCTION(lame_set_num_samples);
DECL_FUNCTION(lame_set_in_samplerate);
DECL_FUNCTION(lame_set_num_channels);
DECL_FUNCTION(lame_set_brate);
DECL_FUNCTION(lame_set_VBR);
DECL_FUNCTION(lame_encode_buffer_interleaved);
DECL_FUNCTION(lame_encode_flush);


typedef struct
{
   void *id;
   char *artist;
   char *original;
   char *title;
   char *album;
   char *trackno;
   char *date;
   char *genre;
   char *composer;
   char *comments;
   char *copyright;
   char *website;
   char *image;

   void *audio;
   int mode;

   char capturing;
   char id3_found;
   char streaming;

   uint8_t no_tracks;
   uint8_t bits_sample;
   int frequency;
   int bitrate;
   int blocksize;
   enum aaxFormat format;
   size_t no_samples;
   size_t max_samples;

   _data_t *mp3Buffer;

} _driver_t;

static int _aax_mpg123_init = AAX_FALSE;
static int _getFormatFromMP3Format(int);
static void _detect_mpg123_song_info(_driver_t*);


int
_mpg123_detect(_fmt_t *fmt, int mode)
{
   void *audio = NULL;
   int rv = AAX_FALSE;

   if (mode == 0) /* read */
   {
#if 0
      audio = _aaxIsLibraryPresent("mpg123", "0");
      if (!audio) {
         audio = _aaxIsLibraryPresent("libmpg123", "0");
      }
      if (!audio) {
         audio = _aaxIsLibraryPresent("libmpg123-0", "0");
      }
#endif

      if (!audio) /* libmpg123 was not found, switch to pdmp3 */
      {
         fmt->id = calloc(1, sizeof(_driver_t));
         if (fmt->id)
         {
            _driver_t *handle = fmt->id;
            handle->mode = mode;
            handle->capturing = (mode == 0) ? 1 : 0;
            handle->blocksize = sizeof(int16_t);

            rv = AAX_TRUE;
         }
         else {
            _AAX_FILEDRVLOG("PDMP3: Insufficient memory");
         }
      }
      else /* libmpg123 was found */
      {
         char *error;

         _aaxGetSymError(0);

         TIE_FUNCTION(mpg123_init);
         if (pmpg123_init)
         {
            TIE_FUNCTION(mpg123_exit);
            TIE_FUNCTION(mpg123_new);
            TIE_FUNCTION(mpg123_param);
            TIE_FUNCTION(mpg123_open_feed);
            TIE_FUNCTION(mpg123_decode);
            TIE_FUNCTION(mpg123_feed);
            TIE_FUNCTION(mpg123_read);
            TIE_FUNCTION(mpg123_delete);
            TIE_FUNCTION(mpg123_format);
            TIE_FUNCTION(mpg123_getformat);

            error = _aaxGetSymError(0);
            if (!error)
            {
               /* not required but useful */
               TIE_FUNCTION(mpg123_length);
               TIE_FUNCTION(mpg123_set_filesize);
               TIE_FUNCTION(mpg123_feedseek);
               TIE_FUNCTION(mpg123_meta_check);
               TIE_FUNCTION(mpg123_id3);
               TIE_FUNCTION(mpg123_plain_strerror);

               fmt->id = calloc(1, sizeof(_driver_t));
               if (fmt->id)
               {
                  _driver_t *handle = fmt->id;

                  handle->audio = audio;
                  handle->mode = mode;
                  handle->capturing = (mode == 0) ? 1 : 0;
                  handle->blocksize = sizeof(int16_t);

                  rv = AAX_TRUE;
               }             
               else {
                  _AAX_FILEDRVLOG("MPG123: Insufficient memory");
               }
            }
         }
      }
   }
   else /* write */
   {
      audio = _aaxIsLibraryPresent("mp3lame", "0");
      if (!audio) {
         audio = _aaxIsLibraryPresent("libmp3lame", "0");
      }
      if (!audio) {
         audio = _aaxIsLibraryPresent("lame_enc", "0");
      }

      if (audio)
      {
         char *error;

         _aaxGetSymError(0);

         TIE_FUNCTION(lame_init);
         if (plame_init)
         {
            TIE_FUNCTION(lame_init_params);
            TIE_FUNCTION(lame_close);
            TIE_FUNCTION(lame_set_num_samples);
            TIE_FUNCTION(lame_set_in_samplerate);
            TIE_FUNCTION(lame_set_num_channels);
            TIE_FUNCTION(lame_set_brate);
            TIE_FUNCTION(lame_set_VBR);
            TIE_FUNCTION(lame_encode_buffer_interleaved);
            TIE_FUNCTION(lame_encode_flush);

            error = _aaxGetSymError(0);
            if (!error)
            {
               fmt->id = calloc(1, sizeof(_driver_t));
               if (fmt->id)
               {
                  _driver_t *handle = fmt->id;

                  handle->audio = audio;
                  handle->mode = mode;
                  handle->capturing = (mode == 0) ? 1 : 0;
                  handle->blocksize = sizeof(int16_t);

                  rv = AAX_TRUE;
               }
               else {
                  _AAX_FILEDRVLOG("MPG123: Insufficient memory");
               }
            }
         }
      }
   }

   return rv;
}

void*
_mpg123_open(_fmt_t *fmt, void *buf, size_t *bufsize, size_t fsize)
{
   _driver_t *handle = fmt->id;
   void *rv = NULL;

   assert(bufsize);

   if (handle)
   {
      if (handle->capturing)
      {
         if (!handle->id && !handle->audio)
         {
            handle->id = pdmp3_new(NULL, NULL);
            if (handle->id)
            {
               if (pdmp3_open_feed(handle->id) == MPG123_OK) {
                  handle->mp3Buffer = _aaxDataCreate(16384, 1);
               }
               else
               {
                  _AAX_FILEDRVLOG("MPG123: Unable to initialize pdmp3");
                  pdmp3_delete(handle->id);
                  handle->id = NULL;
               }
            }
         }
         else if (!handle->id)
         {
            if (!_aax_mpg123_init)
            {
               pmpg123_init();
               _aax_mpg123_init = AAX_TRUE;
            }

            handle->id = pmpg123_new(NULL, NULL);
            if (handle->id)
            {
#ifdef NDEBUG
               pmpg123_param(handle->id, MPG123_ADD_FLAGS, MPG123_QUIET, 1);
#endif
               // http://sourceforge.net/p/mpg123/mailman/message/26864747/
               // MPG123_GAPLESS could be bad for http streams
               if (handle->streaming) {
                  pmpg123_param(handle->id, MPG123_ADD_FLAGS, MPG123_GAPLESS,0);
               } else {
                  pmpg123_param(handle->id, MPG123_ADD_FLAGS, MPG123_GAPLESS,1);
               }

               pmpg123_param(handle->id, MPG123_ADD_FLAGS, MPG123_SEEKBUFFER,1);
               pmpg123_param(handle->id, MPG123_ADD_FLAGS, MPG123_FUZZY, 1);
               pmpg123_param(handle->id, MPG123_ADD_FLAGS, MPG123_PICTURE, 1);
               pmpg123_param(handle->id, MPG123_RESYNC_LIMIT, -1, 0.0);
               pmpg123_param(handle->id, MPG123_REMOVE_FLAGS,
                                         MPG123_AUTO_RESAMPLE, 0);
               pmpg123_param(handle->id, MPG123_RVA, MPG123_RVA_MIX, 0.0);

               pmpg123_format(handle->id, handle->frequency,
                                    MPG123_MONO | MPG123_STEREO,
                                    MPG123_ENC_SIGNED_16);

               if (pmpg123_open_feed(handle->id) == MPG123_OK)
               {
                  handle->mp3Buffer = _aaxDataCreate(16384, 1);

                  if (pmpg123_set_filesize) {
                     pmpg123_set_filesize(handle->id, fsize);
                  }
                  if (!handle->id3_found) {
                     _detect_mpg123_song_info(handle);
                  }
               }
               else
               {
                  _AAX_FILEDRVLOG("MPG123: Unable to initialize mpg123");
                  pmpg123_delete(handle->id);
                  handle->id = NULL;

                  if (_aax_mpg123_init)
                  {
                     pmpg123_exit();
                     _aax_mpg123_init = AAX_FALSE;
                  }
               }
            }
         }

         if (handle->id && !handle->audio)
         {
            size_t size;
            int ret = pdmp3_decode(handle->id, buf, *bufsize, NULL, 0, &size);
            if (ret == MPG123_NEW_FORMAT)
            {
               int enc, channels;
               long rate;

               ret = pdmp3_getformat(handle->id, &rate, &channels, &enc);
               if ((ret == MPG123_OK) &&
                      (1000 <= rate) && (rate <= 192000) &&
                      (1 <= channels) && (channels <= _AAX_MAX_SPEAKERS))
               {
                  handle->frequency = rate;
                  handle->no_tracks = channels;
                  handle->format = _getFormatFromMP3Format(enc);
                  handle->bits_sample = aaxGetBitsPerSample(handle->format);
//                rv = buf;
               }
            }
            else if (ret == MPG123_NEED_MORE) {
               rv = buf;
            }
         }
         else if (handle->id)
         {
            size_t size;
            int ret;

            ret = pmpg123_decode(handle->id, buf, *bufsize, NULL, 0, &size);
            if (!handle->id3_found) {
               _detect_mpg123_song_info(handle);
            }
            if (ret == MPG123_NEW_FORMAT)
            {
               int enc, channels;
               long rate;

               ret = pmpg123_getformat(handle->id, &rate, &channels, &enc);
               if ((ret == MPG123_OK) &&
                      (1000 <= rate) && (rate <= 192000) &&
                      (1 <= channels) && (channels <= _AAX_MAX_SPEAKERS))
               {
                  handle->frequency = rate;
                  handle->no_tracks = channels;
                  handle->format = _getFormatFromMP3Format(enc);
                  handle->bits_sample = aaxGetBitsPerSample(handle->format);

                  rv = buf;

                  if (pmpg123_length)
                  {
                     off_t length = pmpg123_length(handle->id);
                     handle->max_samples = (length > 0) ? length : 0;
                  }
               }
               else {
                  _AAX_FILEDRVLOG("MPG123: file may be corrupt");
               }
            }
            else if (ret == MPG123_NEED_MORE) {
               rv = buf;
            }
            // else we're done decoding, return NULL
         }
         else {
            _AAX_FILEDRVLOG("MPG123: Unable to create a handle");
         }
      }
      else	// playback
      {
         /*
          * The required mp3buf_size can be computed from num_samples,
          * samplerate and encoding rate, but here is a worst case estimate:
          *
          * mp3buf_size in bytes = 1.25*num_samples + 7200
          */
         handle->mp3Buffer = _aaxDataCreate(7200 + handle->no_samples*5/4, 1);
         if (handle->mp3Buffer)
         {
            int ret;

            handle->id = plame_init();
            plame_set_num_samples(handle->id, handle->no_samples);
            plame_set_in_samplerate(handle->id, handle->frequency);
            plame_set_brate(handle->id, handle->bitrate);
            plame_set_VBR(handle->id, VBR_OFF);
     
            do
            {
               ret = plame_set_num_channels(handle->id, handle->no_tracks);
               if (ret < 0 && handle->no_tracks > 2) {
                  handle->no_tracks -= 2;
               }
               else {
                  break;
               }
            }
            while (ret < 0);

            plame_init_params(handle->id);
         }
      }
   }
   else {
      _AAX_FILEDRVLOG("MPG123: Internal error: handle id equals 0");
   }

   return rv;
}

void
_mpg123_close(_fmt_t *fmt)
{
   _driver_t *handle = fmt->id;

   if (handle)
   {
      if (handle->capturing)
      {
         if (!handle->audio)
         {
            pdmp3_delete(handle->id);
            handle->id = NULL;
         }
         else
         {
            pmpg123_delete(handle->id);
            handle->id = NULL;
            if (_aax_mpg123_init)
            {
               pmpg123_exit();
               _aax_mpg123_init = AAX_FALSE;
            }
         }
      }
      else
      {
         plame_encode_flush(handle->id, handle->mp3Buffer->data,
                                        handle->mp3Buffer->size);
         // plame_mp3_tags_fid(handle->id, mp3);
         plame_close(handle->id);
      }
      _aaxDataDestroy(handle->mp3Buffer);

#ifdef WINXP
      free(handle->pcmBuffer);
#endif

      free(handle->trackno);
      free(handle->artist);
      free(handle->title);
      free(handle->album);
      free(handle->date);
      free(handle->genre);
      free(handle->comments);
      free(handle->composer);
      free(handle->copyright);
      free(handle->original);
      free(handle->website);
      free(handle->image);
      free(handle);
   }
}

int
_mpg123_setup(VOID(_fmt_t *fmt), VOID(_fmt_type_t pcm_fmt), VOID(enum aaxFormat aax_fmt))
{
   return AAX_TRUE;
}

size_t
_mpg123_fill(_fmt_t *fmt, void_ptr sptr, size_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t rv = __F_PROCESS;
   int ret;

   if (!handle->audio)
   {
      ret = pdmp3_feed(handle->id, sptr, *bytes);
   } else {
      ret = pmpg123_feed(handle->id, sptr, *bytes);
   }

   if (!handle->id3_found) {
      _detect_mpg123_song_info(handle);
   }
   if (ret != MPG123_OK) {
      *bytes = 0;
   }

   return rv;
}

size_t
_mpg123_copy(_fmt_t *fmt, int32_ptr dptr, size_t offset, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bytes, bufsize, size = 0;
   unsigned int bits, tracks;
   unsigned char *buf;
   size_t rv = __F_EOF;
   int ret;

   tracks = handle->no_tracks;
   bits = handle->bits_sample;
   bytes = *num*tracks*bits/8;

   buf = handle->mp3Buffer->data;
   bufsize = handle->mp3Buffer->size;

   if (bytes > bufsize) {
      bytes = bufsize;
   }

   if (!handle->audio) {
      ret = pdmp3_read(handle->id, buf, bytes, &size);
   }
   else
   {
      ret = pmpg123_read(handle->id, buf, bytes, &size);
      if (!handle->id3_found) {
         _detect_mpg123_song_info(handle);
      }
   }

   if (ret == MPG123_NEW_FORMAT)
   {
      int enc, channels;
      long rate;

      if (!handle->audio) {
         ret = pdmp3_getformat(handle->id, &rate, &channels, &enc);
      } else {
         ret = pmpg123_getformat(handle->id, &rate, &channels, &enc);
      }

      if ((ret == MPG123_OK) &&
             (1000 <= rate) && (rate <= 192000) &&
             (1 <= channels) && (channels <= _AAX_MAX_SPEAKERS))
      {
         handle->frequency = rate;
         handle->no_tracks = channels;
         handle->format = _getFormatFromMP3Format(enc);
         handle->bits_sample = aaxGetBitsPerSample(handle->format);
      }
   }

   if (ret == MPG123_OK || (ret == MPG123_NEED_MORE && handle->audio))
   {
      unsigned char *ptr = (unsigned char*)dptr;
      unsigned int framesize = tracks*bits/8;

      ptr += offset*framesize;
      memcpy(ptr, buf, size);

      *num = size/framesize;
      handle->no_samples += *num;
      rv = size;
   }
   else if (ret != MPG123_DONE && pmpg123_plain_strerror) {
      _AAX_FILEDRVLOG(pmpg123_plain_strerror(ret));
   }

   return rv;
}

size_t
_mpg123_cvt_from_intl(_fmt_t *fmt, int32_ptrptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   unsigned int bits, tracks, framesize;
   size_t bytes, bufsize, size = 0;
   size_t rv = __F_EOF;
   unsigned char *buf;
   int ret;

   tracks = handle->no_tracks;
   bits = handle->bits_sample;
   framesize = tracks*bits/8;
   bytes = *num*framesize;

   buf = handle->mp3Buffer->data;
   bufsize = handle->mp3Buffer->size;

   if (bytes > bufsize) {
      bytes = bufsize;
   }

   if (!handle->audio) {
      ret = pdmp3_read(handle->id, buf, bytes, &size);
   } else {
      ret = pmpg123_read(handle->id, buf, bytes, &size);
   }

   if (!handle->id3_found) {
      _detect_mpg123_song_info(handle);
   }

   if (ret == MPG123_OK || ret == MPG123_NEED_MORE)
   {
      *num = size/framesize;
      _batch_cvt24_16_intl(dptr, buf, dptr_offs, tracks, *num);

      handle->no_samples += *num;
      rv = size;
   }
   else if (ret != MPG123_DONE && pmpg123_plain_strerror) {
      _AAX_FILEDRVLOG(pmpg123_plain_strerror(ret));
   }

   return rv;
}

size_t
_mpg123_cvt_to_intl(_fmt_t *fmt, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = fmt->id;
   int res;

   assert(scratchlen >= *num*handle->no_tracks*sizeof(int32_t));

   handle->no_samples += *num;
   _batch_cvt16_intl_24(scratch, sptr, offs, handle->no_tracks, *num);
   res = plame_encode_buffer_interleaved(handle->id, scratch, *num,
                              handle->mp3Buffer->data, handle->mp3Buffer->size);
   _aax_memcpy(dptr, handle->mp3Buffer->data, res);

   return res;
}

char*
_mpg123_name(_fmt_t *fmt, enum _aaxStreamParam param)
{
   _driver_t *handle = fmt->id;
   char *rv = NULL;

   switch(param)
   {
   case __F_ARTIST:
      rv = handle->artist;
      break;
   case __F_TITLE:
      rv = handle->title;
      break;
   case __F_COMPOSER:
      rv = handle->composer;
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
   case __F_ORIGINAL:
      rv = handle->original;
      break;
   case __F_WEBSITE:
      rv = handle->website;
      break;
   case __F_IMAGE:
      rv = handle->image;
      break;
   default:
      break;
   }
   return rv;
}

off_t
_mpg123_get(_fmt_t *fmt, int type)
{
   _driver_t *handle = fmt->id;
   off_t rv = 0;

   switch(type)
   {
   case __F_FMT:
      rv = handle->format;
      break;
   case __F_TRACKS:
      rv = handle->no_tracks;
      break;
   case __F_FREQUENCY:
      rv = handle->frequency;
      break;
   case __F_BITS_PER_SAMPLE:
      rv = handle->bits_sample;
      break;
   case __F_BLOCK_SIZE:
      rv = handle->blocksize;
      break;
   case __F_NO_SAMPLES:
      rv = handle->max_samples;
      break;
   default:
      if (type & __F_NAME_CHANGED)
      {
         switch (type & ~__F_NAME_CHANGED)
         {
         default:
            break;
         }
      }
      break;
   }
   return rv;
}

off_t
_mpg123_set(_fmt_t *fmt, int type, off_t value)
{
   _driver_t *handle = fmt->id;
   off_t rv = 0;

   switch(type)
   {
   case __F_FREQUENCY:
      handle->frequency = value;
      break;
   case __F_RATE:
      handle->bitrate = value;
      break;
   case __F_TRACKS:
      handle->no_tracks = value;
      break;
   case __F_NO_SAMPLES:
      handle->no_samples = value;
      handle->max_samples = value;
      break;
   case __F_BITS_PER_SAMPLE:
      handle->bits_sample = value;
      break;
   case __F_IS_STREAM:
      handle->streaming = AAX_TRUE;
      break;
   case __F_POSITION:
#if 0
      if (pmpg123_feedseek)
      {
         off_t inoffset;
         rv = pmpg123_feedseek(handle->id, value, SEEK_SET, &inoffset);
         if (rv ==  MPG123_NEED_MORE) rv = __F_PROCESS;
         else if (rv < 0) rv = __F_EOF;
      }
#endif
      break;
   default:
      break;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */
#define MAX_ID3V1_GENRES	192
#define __DUP(a, b)	if ((b) != NULL && (b)->fill) a = strdup((b)->p);
#define __COPY(a, b)	do { int s = sizeof(b); \
      a = calloc(1, s+1); if (a) memcpy(a,b,s); \
   } while(0);

static int
_getFormatFromMP3Format(int enc)
{
   int rv;
   switch (enc)
   {
   case MPG123_ENC_8:
      rv = AAX_PCM8S;
      break;
   case MPG123_ENC_ULAW_8:
      rv = AAX_MULAW;
      break;
   case MPG123_ENC_ALAW_8:
      rv = AAX_ALAW;
      break;
   case MPG123_ENC_SIGNED_16:
      rv = AAX_PCM16S;
      break;
   case MPG123_ENC_SIGNED_24:
      rv = AAX_PCM24S;
      break;
   case MPG123_ENC_SIGNED_32:
      rv = AAX_PCM32S;
      break;
   default:
      rv = AAX_FORMAT_NONE;
   }
   return rv;
}

static void
_detect_mpg123_song_info(_driver_t *handle)
{
   if (!pmpg123_meta_check || !pmpg123_id3) {
      handle->id3_found = AAX_TRUE;
   }

   if (!handle->id3_found)
   {
      int meta = pmpg123_meta_check(handle->id);
      mpg123_id3v1 *v1 = NULL;
      mpg123_id3v2 *v2 = NULL;

      if ((meta & MPG123_ID3) && (pmpg123_id3(handle->id, &v1, &v2)==MPG123_OK))
      {
         void *xid = NULL, *xmid = NULL, *xgid = NULL;
         char *lang = systemLanguage(NULL);
         char *path, fname[81];

         snprintf(fname, 80, "genres-%s.xml", lang);
         path = systemConfigFile(fname);
         xid = xmlOpen(path);
         free(path);
         if (!xid)
         {
            path = systemConfigFile("genres.xml");
            xid = xmlOpen(path);
            free(path);
         }
         if (xid)
         {
            xmid = xmlNodeGet(xid, "/genres/mp3");
            if (xmid) {
               xgid = xmlMarkId(xmid);
            }
         }

         if (v2)
         {
            size_t i;

            __DUP(handle->artist, v2->artist);
            __DUP(handle->title, v2->title);
            __DUP(handle->album, v2->album);
            __DUP(handle->date, v2->year);
            __DUP(handle->comments, v2->comment);
            if (v2->genre && v2->genre->fill && (v2->genre->p[0] == '('))
            {
               char *end;
               unsigned char genre = strtol((char*)&v2->genre->p[1], &end, 10);
               if (xgid && (genre < MAX_ID3V1_GENRES) && (*end == ')'))
               {
                  void *xnid = xmlNodeGetPos(xmid, xgid, "name", genre);
                  char *g = xmlGetString(xnid);
                  handle->genre = strdup(g);
                  xmlFree(g);
               }
               else handle->genre = strdup(v2->genre->p);
            }
            else __DUP(handle->genre, v2->genre);

            for (i=0; i<v2->texts; i++)
            {
               if (v2->text[i].text.p != NULL)
               {
                  if (v2->text[i].id[0] == 'T' && v2->text[i].id[1] == 'R' &&
                      v2->text[i].id[2] == 'C' && v2->text[i].id[3] == 'K')
                  {
                     handle->trackno = strdup(v2->text[i].text.p);
                  } else
                  if (v2->text[i].id[0] == 'T' && v2->text[i].id[1] == 'C'  &&
                      v2->text[i].id[2] == 'O' && v2->text[i].id[3] == 'M')
                  {
                     handle->composer = strdup(v2->text[i].text.p);
                  } else
                  if (v2->text[i].id[0] == 'T' && v2->text[i].id[1] == 'O' &&
                      v2->text[i].id[2] == 'P' && v2->text[i].id[3] == 'E')
                  {
                     handle->original = strdup(v2->text[i].text.p);
                  } else
                  if (v2->text[i].id[0] == 'W' && v2->text[i].id[1] == 'C' &&
                      v2->text[i].id[2] == 'O' && v2->text[i].id[3] == 'P')
                  {
                     handle->copyright = strdup(v2->text[i].text.p);
                  } else
                  if (v2->text[i].id[0] == 'W' && v2->text[i].id[1] == 'O' &&
                      v2->text[i].id[2] == 'A' && v2->text[i].id[3] == 'R')
                  {
                     free(handle->website);
                     handle->website = strdup(v2->text[i].text.p);
                  }
                  else if (!handle->website &&
                         v2->text[i].id[0] == 'W' && v2->text[i].id[1] == 'P' &&
                         v2->text[i].id[2] == 'U' && v2->text[i].id[3] == 'B')
                  {
                     free(handle->website);
                     handle->website = strdup(v2->text[i].text.p);
                  }
                  else if (!handle->website &&
                         v2->text[i].id[0] == 'W' && v2->text[i].id[1] == 'O' &&
                         v2->text[i].id[2] == 'R' && v2->text[i].id[3] == 'S')
                  {
                     free(handle->website);
                     handle->website = strdup(v2->text[i].text.p);
                  }
               }
            }
#if 0
            for (i=0; i<v2->pictures; i++)
            {
               if (v2->picture[i].data != NULL) {};
            }
#endif
            handle->id3_found = AAX_TRUE;
         }
         else if (v1)
         {
            __COPY(handle->artist, v1->artist);
            __COPY(handle->title, v1->title);
            __COPY(handle->album, v1->album);
            __COPY(handle->date, v1->year);
            __COPY(handle->comments, v1->comment);
            if (v1->comment[28] == '\0') {
               __COPY(handle->trackno, (char*)&v1->comment[29]);
            }
            if (xgid && v1->genre < MAX_ID3V1_GENRES) {
               void *xnid = xmlNodeGetPos(xid, xgid, "name", v1->genre);
               char *g = xmlGetString(xnid);
               handle->genre = strdup(g);
               xmlFree(g);
            }
            handle->id3_found = AAX_TRUE;
         }
         if (xid)
         {
            xmlFree(xgid);
            xmlFree(xmid);
            xmlClose(xid);
         }
      }
   }
}

