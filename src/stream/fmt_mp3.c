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

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#endif
#include <stdarg.h>
#include <time.h>
#include <assert.h>
#include <stdio.h>

#include <xml.h>

#include <base/databuffer.h>
#include <base/dlsym.h>

#include <api.h>
#include <arch.h>

#include "audio.h"
#include "fmt_mp3.h"

DECL_FUNCTION(mp3_init);
DECL_FUNCTION(mp3_exit);
DECL_FUNCTION(mp3_new);
DECL_FUNCTION(mp3_param);
DECL_FUNCTION(mp3_open_feed);
DECL_FUNCTION(mp3_decode);
DECL_FUNCTION(mp3_feed);
DECL_FUNCTION(mp3_read);
DECL_FUNCTION(mp3_delete);
DECL_FUNCTION(mp3_format);
DECL_FUNCTION(mp3_info);
DECL_FUNCTION(mp3_getformat);
DECL_FUNCTION(mp3_length);
DECL_FUNCTION(mp3_set_filesize);
DECL_FUNCTION(mp3_feedseek);
DECL_FUNCTION(mp3_meta_check);
DECL_FUNCTION(mp3_id3);
DECL_FUNCTION(mp3_plain_strerror);

// libmpg123 for mp3 input
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
DECL_FUNCTION(mpg123_info);
DECL_FUNCTION(mpg123_getformat);
DECL_FUNCTION(mpg123_length);
DECL_FUNCTION(mpg123_set_filesize);
DECL_FUNCTION(mpg123_feedseek);
DECL_FUNCTION(mpg123_meta_check);
DECL_FUNCTION(mpg123_id3);
DECL_FUNCTION(mpg123_plain_strerror);

// liblame for mp3 output
DECL_FUNCTION(lame_init);
DECL_FUNCTION(lame_init_params);
DECL_FUNCTION(lame_close);
DECL_FUNCTION(lame_set_num_samples);
DECL_FUNCTION(lame_set_in_samplerate);
DECL_FUNCTION(lame_set_num_channels);
DECL_FUNCTION(lame_set_brate);
DECL_FUNCTION(lame_set_VBR);
DECL_FUNCTION(lame_set_quality);
DECL_FUNCTION(lame_encode_buffer_interleaved);
DECL_FUNCTION(lame_encode_flush_nogap);

DECL_FUNCTION(lame_get_lametag_frame);
DECL_FUNCTION(lame_set_write_id3tag_automatic);
DECL_FUNCTION(lame_get_id3v1_tag);
DECL_FUNCTION(lame_get_id3v2_tag);
DECL_FUNCTION(id3tag_set_title);
DECL_FUNCTION(id3tag_set_artist);
DECL_FUNCTION(id3tag_set_album);
DECL_FUNCTION(id3tag_set_genre);
DECL_FUNCTION(id3tag_set_track);
DECL_FUNCTION(id3tag_set_albumart);
DECL_FUNCTION(id3tag_set_year);
DECL_FUNCTION(id3tag_set_comment);
DECL_FUNCTION(id3tag_add_v2);
#ifndef NDEBUG
DECL_FUNCTION(lame_set_errorf);
#endif


#define BUFFER_SIZE	256

typedef struct
{
   void *id;

   int mode;
   char capturing;
   char id3_found;
   char streaming;
   char internal;

   uint8_t no_tracks;
   uint8_t bits_sample;
   int blocksize;
   enum aaxFormat format;
   size_t no_samples;
   size_t max_samples;
   size_t file_size;

   struct mp3_frameinfo info;
   struct _meta_t meta;

   _data_t *mp3Buffer;

} _driver_t;

static int _getFormatFromMP3Format(int);
static void _detect_mp3_song_info(_driver_t*);
#ifndef NDEBUG
static void _aax_lame_log(const char*, va_list);
#endif

static int _aax_mp3_init = false;
static void *audio = NULL;

int
_mp3_detect(UNUSED(_fmt_t *fmt), int mode)
{
   int rv = false;

   if (mode == 0) /* read */
   {
      const char *env = getenv("AAX_USE_PDMP3");
      if (!env || !_aax_getbool(env))
      {
         audio = _aaxIsLibraryPresent("mpg123", "0");
         if (!audio) {
            audio = _aaxIsLibraryPresent("libmpg123", "0");
         }
         if (!audio) {
            audio = _aaxIsLibraryPresent("libmpg123-0", "0");
         }
      }

      if (!audio) /* libmpg123 was not found, switch to pdmp3 */
      {
         pmp3_new = (mp3_new_proc)pdmp3_new;
         pmp3_open_feed = (mp3_open_feed_proc)pdmp3_open_feed;
         pmp3_decode = (mp3_decode_proc)pdmp3_decode;
         pmp3_feed = (mp3_feed_proc)pdmp3_feed;
         pmp3_read = (mp3_read_proc)pdmp3_read;
         pmp3_delete = (mp3_delete_proc)pdmp3_delete;
         pmp3_info = (mp3_info_proc)pdmp3_info;
         pmp3_getformat = (mp3_getformat_proc)pdmp3_getformat;
         pmp3_meta_check = (mp3_meta_check_proc)pdmp3_meta_check;
         pmp3_id3 = (mp3_id3_proc)pdmp3_id3;
         rv = true;
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
            TIE_FUNCTION(mpg123_info);
            TIE_FUNCTION(mpg123_getformat);

            pmp3_init = pmpg123_init;
            pmp3_exit = pmpg123_exit;
            pmp3_new = pmpg123_new;
            pmp3_param = pmpg123_param;
            pmp3_open_feed = pmpg123_open_feed;
            pmp3_decode = pmpg123_decode;
            pmp3_feed = pmpg123_feed;
            pmp3_read = pmpg123_read;
            pmp3_delete = pmpg123_delete;
            pmp3_format = pmpg123_format;
            pmp3_info = pmpg123_info;
            pmp3_getformat = pmpg123_getformat;

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

               pmp3_length = pmpg123_length;
               pmp3_set_filesize = pmpg123_set_filesize;
               pmp3_feedseek = pmpg123_feedseek;
               pmp3_meta_check = pmpg123_meta_check;
               pmp3_id3 = pmpg123_id3;
               pmp3_plain_strerror = pmpg123_plain_strerror;
               rv = true;
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
            TIE_FUNCTION(lame_set_quality);
            TIE_FUNCTION(lame_encode_buffer_interleaved);
            TIE_FUNCTION(lame_encode_flush_nogap);
            TIE_FUNCTION(lame_get_lametag_frame);
            TIE_FUNCTION(lame_set_write_id3tag_automatic);
            TIE_FUNCTION(lame_get_id3v1_tag);
            TIE_FUNCTION(lame_get_id3v2_tag);
            TIE_FUNCTION(id3tag_set_title);
            TIE_FUNCTION(id3tag_set_artist);
            TIE_FUNCTION(id3tag_set_album);
            TIE_FUNCTION(id3tag_set_genre);
            TIE_FUNCTION(id3tag_set_track);
            TIE_FUNCTION(id3tag_set_albumart);
            TIE_FUNCTION(id3tag_set_year);
            TIE_FUNCTION(id3tag_set_comment);
            TIE_FUNCTION(id3tag_add_v2);
#ifndef NDEBUG
            TIE_FUNCTION(lame_set_errorf);
#endif

            error = _aaxGetSymError(0);
            if (!error) {
               rv = true;
            }
         }
      }
   }

   return rv;
}

void*
_mp3_open(_fmt_t *fmt, int mode, void *buf, ssize_t *bufsize, size_t fsize)
{
   _driver_t *handle = fmt->id;
   void *rv = NULL;

   if (!handle)
   {
      handle = fmt->id = calloc(1, sizeof(_driver_t));
      if (fmt->id)
      {
         handle->mode = mode;
         handle->info.rate = 44100;
         handle->info.bitrate = 320000;
         handle->file_size = fsize;
         handle->capturing = (mode == 0) ? 1 : 0;
         handle->blocksize = sizeof(int16_t);
         handle->internal = audio ? false : true;
      }
      else {
         _AAX_FILEDRVLOG("MP3: Insufficient memory");
      }
   }

   if (handle)
   {
      if (handle->capturing && buf && bufsize)
      {
         if (!handle->id && handle->internal)
         {
            int error_no;

            handle->id = pmp3_new(NULL, &error_no);
            if (handle->id)
            {
               if (pmp3_open_feed(handle->id) == MP3_OK) {
                  handle->mp3Buffer = _aaxDataCreate(1, 16384, 1);
               }
               else
               {
                  _AAX_FILEDRVLOG("MP3: Unable to initialize");
                  pmp3_delete(handle->id);
                  handle->id = NULL;
               }
            }
            else
            {
               if (pmp3_plain_strerror) {
                   _AAX_FILEDRVLOG(pmp3_plain_strerror(error_no));
               } else {
                  _AAX_FILEDRVLOG("MP3: Unable to create a handle");
               }
               handle->id = NULL;
            }
         }
         else if (!handle->id)
         {
            if (!_aax_mp3_init)
            {
               pmp3_init();
               _aax_mp3_init = true;
            }

            handle->id = pmp3_new(NULL, NULL);
            if (handle->id)
            {
#ifdef NDEBUG
               pmp3_param(handle->id, MP3_ADD_FLAGS, MP3_QUIET, 1);
#endif
               // http://sourceforge.net/p/mp3/mailman/message/26864747/
               // MP3_GAPLESS could be bad for http streams
               if (handle->streaming) {
                  pmp3_param(handle->id, MP3_ADD_FLAGS, MP3_GAPLESS,0);
               } else {
                  pmp3_param(handle->id, MP3_ADD_FLAGS, MP3_GAPLESS,1);
               }

               pmp3_param(handle->id, MP3_ADD_FLAGS, MP3_SEEKBUFFER,1);
               pmp3_param(handle->id, MP3_ADD_FLAGS, MP3_FUZZY, 1);
               pmp3_param(handle->id, MP3_ADD_FLAGS, MP3_PICTURE, 1);
               pmp3_param(handle->id, MP3_RESYNC_LIMIT, -1, 0.0);
               pmp3_param(handle->id, MP3_REMOVE_FLAGS,
                                         MP3_AUTO_RESAMPLE, 0);
               pmp3_param(handle->id, MP3_RVA, MP3_RVA_MIX, 0.0);

               pmp3_format(handle->id, handle->info.rate,
                                    MP3_MONO | MP3_STEREO,
                                    MP3_ENC_SIGNED_16);

               if (pmp3_open_feed(handle->id) == MP3_OK)
               {
                  handle->mp3Buffer = _aaxDataCreate(1, 16384, 1);
                  if (pmp3_set_filesize) {
                     pmp3_set_filesize(handle->id, fsize);
                  }
               }
               else
               {
                  _AAX_FILEDRVLOG("MP3: Unable to initialize mp3");
                  pmp3_delete(handle->id);
                  handle->id = NULL;

                  if (_aax_mp3_init)
                  {
                     pmp3_exit();
                     _aax_mp3_init = false;
                  }
               }
            }
         }

         if (handle->id && handle->internal && buf && bufsize)
         {
            size_t size;
            int ret = pmp3_decode(handle->id, buf, *bufsize, NULL, 0, &size);
            if (!handle->id3_found) {
               _detect_mp3_song_info(handle);
            }

            if (ret == MP3_NEW_FORMAT)
            {
               int enc, channels;
               long rate;

               ret = pmp3_getformat(handle->id, &rate, &channels, &enc);
               if ((ret == MP3_OK) &&
                      (1000 <= rate) && (rate <= 192000) &&
                      (1 <= channels) && (channels <= _AAX_MAX_SPEAKERS))
               {
                  handle->info.rate = rate;
                  handle->no_tracks = channels;
                  handle->format = _getFormatFromMP3Format(enc);
                  handle->bits_sample = aaxGetBitsPerSample(handle->format);
                  handle->blocksize = handle->no_tracks*handle->bits_sample/8;

                  if (pmp3_info(handle->id,&handle->info) == MP3_OK)
                  {
                     int bitrate = (handle->info.bitrate > 0) ? handle->info.bitrate : 320000;
                     double q = (double)rate*1000/(bitrate/8.0) * fsize;
                     handle->max_samples = q;
                  }
               }
            }
            else if (ret == MP3_NEED_MORE) {
               rv = buf;
            }
         }
         else if (handle->id && buf && bufsize)
         {
            size_t size;
            int ret;

            ret = pmp3_decode(handle->id, buf, *bufsize, NULL, 0, &size);
            if (!handle->id3_found) {
               _detect_mp3_song_info(handle);
            }

            if (ret == MP3_NEW_FORMAT)
            {
               int enc, channels;
               long rate;

               ret = pmp3_getformat(handle->id, &rate, &channels, &enc);
               if ((ret == MP3_OK) &&
                      (1000 <= rate) && (rate <= 192000) &&
                      (1 <= channels) && (channels <= _AAX_MAX_SPEAKERS))
               {
                  handle->info.rate = rate;
                  handle->no_tracks = channels;
                  handle->format = _getFormatFromMP3Format(enc);
                  handle->bits_sample = aaxGetBitsPerSample(handle->format);
                  handle->blocksize = handle->no_tracks*handle->bits_sample/8;

                  rv = buf;

                  if (pmp3_length)
                  {
                     off_t length = pmp3_length(handle->id);
                     handle->max_samples = (length > 0) ? length : 0;
                  }
               }
               else {
                  _AAX_FILEDRVLOG("MP3: file may be corrupt");
               }
            }
            else if (ret == MP3_NEED_MORE) {
               rv = buf;
            }
            // else we're done decoding, return NULL
         }
         else {
            _AAX_FILEDRVLOG("MP3: Unable to create a handle");
         }
      }
      else if (!handle->capturing)	// playback
      {
         if (!handle->mp3Buffer)
         {
            /*
             * The required mp3buf_size can be computed from num_samples,
             * samplerate and encoding rate, but here is a worst case estimate:
             *
             * mp3buf_size in bytes = 1.25*num_samples + 7200
             */
            handle->mp3Buffer = _aaxDataCreate(1, 7200+handle->no_samples*5/4, 1);
         }

         if (handle->mp3Buffer)
         {
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            char year[16];
            int ret;

            snprintf(year, 16, "%d", tm.tm_year + 1900);

            if (!handle->id) {
               handle->id = plame_init();
            }
#ifndef NDEBUG
            plame_set_errorf(handle->id, _aax_lame_log);
#endif
            pid3tag_add_v2(handle->id);
            pid3tag_set_title(handle->id, handle->meta.title);
            pid3tag_set_artist(handle->id, handle->meta.artist);
            pid3tag_set_album(handle->id, handle->meta.album);
            pid3tag_set_track(handle->id, handle->meta.trackno);
            pid3tag_set_genre(handle->id, handle->meta.genre);
            pid3tag_set_year(handle->id, handle->meta.date ? handle->meta.date : year);
            pid3tag_set_comment(handle->id, aaxGetString(AAX_VERSION_STRING));
            plame_set_num_samples(handle->id, handle->no_samples);
            plame_set_in_samplerate(handle->id, handle->info.rate);

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

            plame_set_write_id3tag_automatic(handle->id, 0);
//          plame_init_params(handle->id);
         }
      }
   }
   else if (!handle) {
      _AAX_FILEDRVLOG("MP3: Internal error: handle id equals 0");
   }

   return rv;
}

void
_mp3_close(_fmt_t *fmt)
{
   _driver_t *handle = fmt->id;

   if (handle)
   {
      if (handle->capturing)
      {
         pmp3_delete(handle->id);
         handle->id = NULL;
         if (_aax_mp3_init && pmp3_exit)
         {
            pmp3_exit();
            _aax_mp3_init = false;
         }
      }
      else {
         plame_close(handle->id);
      }
      _aaxDataDestroy(handle->mp3Buffer);

      _aax_free_meta(&handle->meta);
      free(handle);
   }

   _aaxCloseLibrary(audio);
   audio = NULL;
}

int
_mp3_setup(_fmt_t *fmt, UNUSED(_fmt_type_t pcm_fmt), UNUSED(enum aaxFormat aax_fmt))
{
   _driver_t *handle = fmt->id;

   if (!handle->capturing)
   {
      if (handle->info.bitrate > 0)
      {
          plame_set_VBR(handle->id, vbr_off);
          plame_set_brate(handle->id, handle->info.bitrate/1000);
      }
      else
      {
          plame_set_VBR(handle->id, vbr_off); //vbr_default);
          plame_set_brate(handle->id, 320);
      }
      plame_set_quality(handle->id, 2); // 2=high  5 = medium  7=low

      plame_init_params(handle->id);
   }

   return true;
}

size_t
_mp3_fill(_fmt_t *fmt, void_ptr sptr, ssize_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t rv = __F_PROCESS;
   int ret;

   ret = pmp3_feed(handle->id, sptr, *bytes);
   if (!handle->id3_found) {
      _detect_mp3_song_info(handle);
   }

   if (ret != MP3_OK) {
      *bytes = 0;
   }

   return rv;
}

void*
_mp3_update(_fmt_t *fmt, size_t *offs, ssize_t *size, char close)
{
   _driver_t *handle = fmt->id;
   void *rv = NULL;

   *offs = 0;
   *size = 0;
   if (close && !handle->capturing)
   {
      unsigned char *buf = _aaxDataGetPtr(handle->mp3Buffer, 0);
      size_t avail = _aaxDataGetDataAvail(handle->mp3Buffer, 0);
      size_t res = 0;

      if (avail >= 7200) {
         res = plame_encode_flush_nogap(handle->id, buf, avail);
      }
      if (res > 0)
      {
         _aaxDataIncreaseOffset(handle->mp3Buffer, 0, res);
         *offs = res;
         *size += res;

         buf = _aaxDataGetPtr(handle->mp3Buffer, 0);
         avail = _aaxDataGetFreeSpace(handle->mp3Buffer, 0);
         res = plame_get_lametag_frame(handle->id, buf, avail);
         if (res > 0 && res < avail)
         {
            _aaxDataIncreaseOffset(handle->mp3Buffer, 0, res);
            *size += res;
         }

         rv = _aaxDataGetData(handle->mp3Buffer, 0);
      }
      else
      {
         buf = _aaxDataGetData(handle->mp3Buffer, 0);
         avail = _aaxDataGetSize(handle->mp3Buffer);
         res = plame_get_id3v2_tag(handle->id, buf, avail);
         if (res > 0 && res < avail) {
            *size = res;
         }

         rv = _aaxDataGetData(handle->mp3Buffer, 0);
      }
   }

   return rv;
}

size_t
_mp3_copy(_fmt_t *fmt, int32_ptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bytes, bufsize, size = 0;
   unsigned int blocksize;
   unsigned char *buf;
   size_t rv = __F_NEED_MORE;
   int ret;

   blocksize = handle->blocksize;
   bytes = *num*blocksize;

   buf = _aaxDataGetData(handle->mp3Buffer, 0);
   bufsize = _aaxDataGetSize(handle->mp3Buffer);

   if (bytes > bufsize) {
      bytes = bufsize;
   }

   ret = pmp3_read(handle->id, buf, bytes, &size);
   if (!handle->id3_found) {
      _detect_mp3_song_info(handle);
   }

   if (ret == MP3_NEW_FORMAT)
   {
      int enc, channels;
      long rate;

      ret = pmp3_getformat(handle->id, &rate, &channels, &enc);
      if ((ret == MP3_OK) &&
             (1000 <= rate) && (rate <= 192000) &&
             (1 <= channels) && (channels <= _AAX_MAX_SPEAKERS))
      {
         handle->info.rate = rate;
         handle->no_tracks = channels;
         handle->format = _getFormatFromMP3Format(enc);
         handle->bits_sample = aaxGetBitsPerSample(handle->format);
         handle->blocksize = handle->no_tracks*handle->bits_sample/8;

         if (pmp3_info(handle->id,&handle->info) == MP3_OK)
         {
            int bitrate = (handle->info.bitrate > 0) ? handle->info.bitrate : 320000;
            double q = (double)rate*1000/(bitrate/8.0) * handle->file_size;
            handle->max_samples = q;
         }
      }
   }
   else if (ret == MP3_OK || ret == MP3_NEED_MORE)
   {
      unsigned char *ptr = (unsigned char*)dptr;

      ptr += dptr_offs*blocksize;
      memcpy(ptr, buf, size);

      *num = size/blocksize;
      handle->no_samples += *num;
      if (ret == MP3_OK) {
         rv = size;
      }
   }
   else if (ret != MP3_DONE && pmp3_plain_strerror) {
      _AAX_FILEDRVLOG(pmp3_plain_strerror(ret));
   }

   return rv;
}

size_t
_mp3_cvt_from_intl(_fmt_t *fmt, int32_ptrptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bytes, bufsize, size = 0;
   unsigned int blocksize, tracks;
   unsigned char *buf;
   size_t rv = __F_NEED_MORE;
   int ret;

   tracks = handle->no_tracks;
   blocksize = handle->blocksize;
   bytes = *num*blocksize;

   buf = _aaxDataGetData(handle->mp3Buffer, 0);
   bufsize = _aaxDataGetSize(handle->mp3Buffer);

   if (bytes > bufsize) {
      bytes = bufsize;
   }

   ret = pmp3_read(handle->id, buf, bytes, &size);
   if (!handle->id3_found) {
      _detect_mp3_song_info(handle);
   }

   if (ret == MP3_NEW_FORMAT)
   {
      int enc, channels;
      long rate;

      ret = pmp3_getformat(handle->id, &rate, &channels, &enc);
      if ((ret == MP3_OK) &&
             (1000 <= rate) && (rate <= 192000) &&
             (1 <= channels) && (channels <= _AAX_MAX_SPEAKERS))
      {
         handle->info.rate = rate;
         handle->no_tracks = channels;
         handle->format = _getFormatFromMP3Format(enc);
         handle->bits_sample = aaxGetBitsPerSample(handle->format);
         handle->blocksize = handle->no_tracks*handle->bits_sample/8;

         if (pmp3_info(handle->id,&handle->info) == MP3_OK)
         {
            int bitrate = (handle->info.bitrate > 0) ? handle->info.bitrate : 320000;
            double q = (double)rate*1000/(bitrate/8.0) * handle->file_size;
            handle->max_samples = q;
         }
      }
   }
   else if (ret == MP3_OK || ret == MP3_NEED_MORE)
   {
      *num = size/blocksize;
      _batch_cvt24_16_intl(dptr, buf, dptr_offs, tracks, *num);

      handle->no_samples += *num;
      if (ret == MP3_OK) {
         rv = size;
      }
   }
   else if (ret != MP3_DONE && pmp3_plain_strerror) {
      _AAX_FILEDRVLOG(pmp3_plain_strerror(ret));
   }

   return rv;
}

size_t
_mp3_cvt_to_intl(_fmt_t *fmt, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = fmt->id;
   void *buf = _aaxDataGetData(handle->mp3Buffer, 0);
   size_t bufsize = _aaxDataGetSize(handle->mp3Buffer);
   int t, res;

   assert(scratchlen >= *num*handle->no_tracks*sizeof(int32_t));

   handle->no_samples += *num;
#if 1
   for (t=0; t<handle->no_tracks; t++)
   {
      int32_t *ptr = (int32_t*)sptr[t]+offs;
      _batch_imul_value(ptr, ptr, sizeof(int32_t), *num, 0.5f);
   }
#endif
   _batch_cvt16_intl_24(scratch, sptr, offs, handle->no_tracks, *num);
   res = plame_encode_buffer_interleaved(handle->id, scratch, *num,
                                         buf, bufsize);
   _aax_memcpy(dptr, buf, res);

   return res;
}

int
_mp3_set_name(_fmt_t *fmt, enum _aaxStreamParam param, const char *desc)
{
   _driver_t *handle = fmt->id;
   int rv = false;

   switch(param)
   {
   case __F_ARTIST:
      handle->meta.artist = (char*)desc;
      rv = true;
      break;
   case __F_TITLE:
      handle->meta.title = (char*)desc;
      pid3tag_set_title(handle->id, handle->meta.title);
      rv = true;
      break;
   case __F_GENRE:
      handle->meta.genre = (char*)desc;
      pid3tag_set_genre(handle->id, handle->meta.genre);
      rv = true;
      break;
   case __F_TRACKNO:
      handle->meta.trackno = (char*)desc;
      pid3tag_set_track(handle->id, handle->meta.trackno);
      rv = true;
      break;
   case __F_ALBUM:
      handle->meta.album = (char*)desc;
      pid3tag_set_album(handle->id, handle->meta.album);
      rv = true;
      break;
   case __F_DATE:
      handle->meta.date = (char*)desc;
      pid3tag_set_year(handle->id, handle->meta.date);
      rv = true;
      break;
   case __F_COMPOSER:
      handle->meta.composer = (char*)desc;
      rv = true;
      break;
   case __F_COMMENT:
      handle->meta.comments = (char*)desc;
      pid3tag_set_comment(handle->id, handle->meta.comments);
      rv = true;
      break;
   case __F_COPYRIGHT:
      handle->meta.copyright = (char*)desc;
      rv = true;
      break;
   case __F_ORIGINAL:
      handle->meta.original = (char*)desc;
      rv = true;
      break;
   case __F_WEBSITE:
      handle->meta.website = (char*)desc;
      rv = true;
      break;
   default:
      break;
   }
   return rv;
}

char*
_mp3_name(_fmt_t *fmt, enum _aaxStreamParam param)
{
   _driver_t *handle = fmt->id;
   char *rv = NULL;

   switch(param)
   {
   case __F_ARTIST:
      rv = handle->meta.artist;
      break;
   case __F_TITLE:
      rv = handle->meta.title;
      break;
   case __F_COMPOSER:
      rv = handle->meta.composer;
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
   case __F_IMAGE:
      rv = handle->meta.image;
      break;
   default:
      break;
   }
   return rv;
}

float
_mp3_get(_fmt_t *fmt, int type)
{
   _driver_t *handle = fmt->id;
   float rv = 0.0f;

   switch(type)
   {
   case __F_FMT:
      rv = handle->format;
      break;
   case __F_TRACKS:
      rv = handle->no_tracks;
      break;
   case __F_FREQUENCY:
      rv = handle->info.rate;
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
   case __F_BITRATE:
      rv = (handle->info.vbr) ? -handle->info.bitrate : handle->info.bitrate;
      break;
   case __F_POSITION:
      if (pmp3_feedseek) {
         rv = true;
      }
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

float
_mp3_set(_fmt_t *fmt, int type, float value)
{
   _driver_t *handle = fmt->id;
   float rv = 0.0f;

   switch(type)
   {
   case __F_FREQUENCY:
      handle->info.rate = rv = value;
      break;
   case __F_TRACKS:
      handle->no_tracks = rv = value;
      break;
   case __F_NO_SAMPLES:
      handle->no_samples = value;
      handle->max_samples = rv = value;
      break;
   case __F_BITS_PER_SAMPLE:
      handle->bits_sample = rv = value;
      break;
   case __F_BLOCK_SIZE:
      handle->blocksize = rv = value;
      break;
   case __F_BITRATE:
       handle->info.bitrate = rv = value;
      break;
   case __F_IS_STREAM:
      handle->streaming = true;
      break;
   case __F_POSITION:
      if (pmp3_feedseek)
      {
         off_t inoffset;
         rv = pmp3_feedseek(handle->id, value, SEEK_SET, &inoffset);
         if (rv ==  MP3_NEED_MORE) rv = __F_PROCESS;
         else if (rv < 0) rv = __F_EOF;
         else rv = inoffset;
      }
      break;
   default:
      break;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */
#define MAX_ID3V1_GENRES	192
#define __DUP(a, b)	if ((b) != NULL && (b)->fill) { if (a) free(a); a = strdup((b)->p); }
#define __COPY(a, b)	do { int s = sizeof(b); \
      a = calloc(1, s+1); if (a) memcpy(a,b,s); \
   } while(0);

#ifndef NDEBUG
void _aax_lame_log(const char *format, va_list ap)
{
   (void) vfprintf(stdout, format, ap);
}
#endif

static int
_getFormatFromMP3Format(int enc)
{
   int rv;
   switch (enc)
   {
   case MP3_ENC_8:
      rv = AAX_PCM8S;
      break;
   case MP3_ENC_ULAW_8:
      rv = AAX_MULAW;
      break;
   case MP3_ENC_ALAW_8:
      rv = AAX_ALAW;
      break;
   case MP3_ENC_SIGNED_16:
      rv = AAX_PCM16S;
      break;
   case MP3_ENC_SIGNED_24:
      rv = AAX_PCM24S;
      break;
   case MP3_ENC_SIGNED_32:
      rv = AAX_PCM32S;
      break;
   default:
      rv = AAX_FORMAT_NONE;
   }
   return rv;
}

static void
_detect_mp3_song_info(_driver_t *handle)
{
   if (!pmp3_meta_check || !pmp3_id3) {
      handle->id3_found = true;
   }

   if (!handle->id3_found)
   {
      int meta = pmp3_meta_check(handle->id);
      mp3_id3v1 *v1 = NULL;
      mp3_id3v2 *v2 = NULL;
      if ((meta & MP3_ID3) && (pmp3_id3(handle->id, &v1, &v2) == MP3_OK))
      {
         xmlId *xid = NULL, *xmid = NULL, *xgid = NULL;
         char *lang = systemLanguage(NULL);
         char *path, fname[81];

         snprintf(fname, 80, "genres-%s.xml", lang);
         path = systemDataFile(fname);
         xid = xmlOpen(path);
         free(path);
         if (!xid)
         {
            path = systemDataFile("genres.xml");
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

            __DUP(handle->meta.artist, v2->artist);
            __DUP(handle->meta.title, v2->title);
            __DUP(handle->meta.album, v2->album);
            __DUP(handle->meta.date, v2->year);
            __DUP(handle->meta.comments, v2->comment);
            if (v2->genre && v2->genre->fill && (v2->genre->p[0] == '('))
            {
               char *end;
               unsigned char genre = strtol((char*)&v2->genre->p[1], &end, 10);
               if (xgid && (genre < MAX_ID3V1_GENRES) && (*end == ')'))
               {
                  xmlId *xnid = xmlNodeGetPos(xmid, xgid, "name", genre);
                  char *g = xmlGetString(xnid);
                  handle->meta.genre = strdup(g);
                  xmlFree(g);
               }
               else handle->meta.genre = strdup(v2->genre->p);
            }
            else __DUP(handle->meta.genre, v2->genre);

            for (i=0; i<v2->texts; i++)
            {
               if (v2->text[i].text.p != NULL)
               {
                  if (v2->text[i].id[0] == 'T' && v2->text[i].id[1] == 'R' &&
                      v2->text[i].id[2] == 'C' && v2->text[i].id[3] == 'K')
                  {
                     handle->meta.trackno = strdup(v2->text[i].text.p);
                  } else
                  if (v2->text[i].id[0] == 'T' && v2->text[i].id[1] == 'C'  &&
                      v2->text[i].id[2] == 'O' && v2->text[i].id[3] == 'M')
                  {
                     handle->meta.composer = strdup(v2->text[i].text.p);
                  } else
                  if (v2->text[i].id[0] == 'T' && v2->text[i].id[1] == 'O' &&
                      v2->text[i].id[2] == 'P' && v2->text[i].id[3] == 'E')
                  {
                     handle->meta.original = strdup(v2->text[i].text.p);
                  } else
                  if (v2->text[i].id[0] == 'W' && v2->text[i].id[1] == 'C' &&
                      v2->text[i].id[2] == 'O' && v2->text[i].id[3] == 'P')
                  {
                     handle->meta.copyright = strdup(v2->text[i].text.p);
                  } else
                  if (v2->text[i].id[0] == 'W' && v2->text[i].id[1] == 'O' &&
                      v2->text[i].id[2] == 'A' && v2->text[i].id[3] == 'R')
                  {
                     free(handle->meta.website);
                     handle->meta.website = strdup(v2->text[i].text.p);
                  }
                  else if (!handle->meta.website &&
                         v2->text[i].id[0] == 'W' && v2->text[i].id[1] == 'P' &&
                         v2->text[i].id[2] == 'U' && v2->text[i].id[3] == 'B')
                  {
                     free(handle->meta.website);
                     handle->meta.website = strdup(v2->text[i].text.p);
                  }
                  else if (!handle->meta.website &&
                         v2->text[i].id[0] == 'W' && v2->text[i].id[1] == 'O' &&
                         v2->text[i].id[2] == 'R' && v2->text[i].id[3] == 'S')
                  {
                     free(handle->meta.website);
                     handle->meta.website = strdup(v2->text[i].text.p);
                  }
               }
            }
#if 0
            for (i=0; i<v2->pictures; i++)
            {
               if (v2->picture[i].data != NULL) {};
            }
#endif
            handle->meta.id3_found = true;
         }
         else if (v1)
         {
            __COPY(handle->meta.artist, v1->artist);
            __COPY(handle->meta.title, v1->title);
            __COPY(handle->meta.album, v1->album);
            __COPY(handle->meta.date, v1->year);
            __COPY(handle->meta.comments, v1->comment);
            if (v1->comment[28] == '\0') {
               __COPY(handle->meta.trackno, (char*)&v1->comment[29]);
            }
            if (xgid && v1->genre < MAX_ID3V1_GENRES) {
               xmlId *xnid = xmlNodeGetPos(xid, xgid, "name", v1->genre);
               char *g = xmlGetString(xnid);
               handle->meta.genre = strdup(g);
               xmlFree(g);
            }
            handle->meta.id3_found = true;
         }
         if (xid)
         {
            xmlFree(xgid);
            xmlFree(xmid);
            xmlClose(xid);
         }
      }
   }

   pmp3_info(handle->id,&handle->info);
#if 0
 printf("mp3 info\n");
 printf(" - version: %i\n", handle->info.version);
 printf(" - layer: %i\n", handle->info.layer);
 printf(" - rate: %li\n", handle->info.rate);
 printf(" - mode: %i\n", handle->info.mode);
 printf(" - bitrate: %i\n", handle->info.bitrate);
 printf(" - abr_rate: %i\n", handle->info.abr_rate);
 printf(" - vbr: %i\n", handle->info.vbr);
#endif
}

