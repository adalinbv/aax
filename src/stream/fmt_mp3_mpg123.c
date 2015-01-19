/*
 * Copyright 2005-2015 by Erik Hofman.
 * Copyright 2009-2015 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#include "fmt_mp3_mpg123.h"

// libmpg123 for mp3 input
// liblame for mp3 output
// both Linux and Windows
static _fmt_open_fn _aaxMPG123Open;
static _fmt_close_fn _aaxMPG123Close;
static _fmt_cvt_to_fn _aaxMPG123CvtToIntl;
static _fmt_cvt_from_fn _aaxMPG123CvtFromIntl;
static _fmt_set_param_fn _aaxMPG123SetParam;

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

/* -------------------------------------------------------------------------- */

static int getFormatFromMP3Format(int);
static void detect_mpg123_song_info(_driver_t*);

static int
_aaxMPG123Detect(void *format, int m, void *_audio[2])
{
   _aaxFmtHandle *fmt = (_aaxFmtHandle*)format;
   int rv = AAX_FALSE;

   if (m == 0) /* read */
   {
      if (!_audio[m]) {
         _audio[m] = _aaxIsLibraryPresent("mpg123", "0");
      }
      if (!_audio[m]) {
         _audio[m] = _aaxIsLibraryPresent("libmpg123-0", "0");
      }

      if (_audio[m])
      {
         void *audio = _audio[m];
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
            if (error) {
               _audio[m] = NULL;
            }
            else
            {
               /* not required but useful */
               TIE_FUNCTION(mpg123_length);
               TIE_FUNCTION(mpg123_set_filesize);
               TIE_FUNCTION(mpg123_feedseek);
               TIE_FUNCTION(mpg123_meta_check);
               TIE_FUNCTION(mpg123_id3);

               fmt->open = _aaxMPG123Open;
               fmt->close = _aaxMPG123Close;
               fmt->cvt_to_intl = _aaxMPG123CvtToIntl;
               fmt->cvt_from_intl = _aaxMPG123CvtFromIntl;
               fmt->set_param = _aaxMPG123SetParam;

               rv = AAX_TRUE;
            }
         }
      }
   }
   else /* write */
   {
      if (!_audio[m]) {
         _audio[m] = _aaxIsLibraryPresent("mp3lame", "0");
      }
      if (!_audio[m]) {
         _audio[m] = _aaxIsLibraryPresent("libmp3lame", "0");
      }
      if (!_audio[m]) {
         _audio[m] = _aaxIsLibraryPresent("lame_enc", "0");
      }

      if (_audio[m])
      {
         void *audio = _audio[m];
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
            if (error) {
               _audio[m] = NULL;
            }
            else
            {
               fmt->open = _aaxMPG123Open;
               fmt->close = _aaxMPG123Close;
               fmt->cvt_to_intl = _aaxMPG123CvtToIntl;
               fmt->cvt_from_intl = _aaxMPG123CvtFromIntl;
               fmt->set_param = _aaxMPG123SetParam;

               rv = AAX_TRUE;
            }
         }
      }
   }

   return rv;
}

static void*
_aaxMPG123Open(void *id, void *buf, size_t *bufsize, size_t fsize)
{
   _driver_t *handle = (_driver_t *)id;
   void *rv = NULL;

   assert(bufsize);

   if (handle)
   {
      if (handle->capturing)
      {
         if (!handle->id)
         {
            pmpg123_init();
            handle->id = pmpg123_new(NULL, NULL);
            if (handle->id)
            {
               char *ptr = 0;

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
                  handle->mp3BufSize = 16384;
                  handle->mp3ptr = _aax_malloc(&ptr, handle->mp3BufSize);
                  handle->mp3Buffer = ptr;

                  if (pmpg123_set_filesize) {
                     pmpg123_set_filesize(handle->id, fsize);
                  }
                  if (!handle->id3_found) detect_mpg123_song_info(handle);
               }
               else
               {
                  _AAX_FILEDRVLOG("MPG123: Unable to initialize mpg123");
                  pmpg123_delete(handle->id);
                  handle->id = NULL;
                  pmpg123_exit();
               }
            }
         }

         if (handle->id)
         {
            size_t size;
            int ret;

            ret = pmpg123_decode(handle->id, buf, *bufsize, NULL, 0, &size);
            if (!handle->id3_found) detect_mpg123_song_info(handle);
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
                  handle->format = getFormatFromMP3Format(enc);
                  handle->bits_sample = aaxGetBitsPerSample(handle->format);

                  rv = buf;

                  if (pmpg123_length)
                  {
                     off_t length = pmpg123_length(handle->id);
                     handle->max_samples = (length > 0) ? length : UINT_MAX;
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
            _AAX_FILEDRVLOG("MPG123: Unable to create a handler");
         }
      }
      else
      {
         char *ptr = 0;
         /*
          * The required mp3buf_size can be computed from num_samples,
          * samplerate and encoding rate, but here is a worst case estimate:
          *
          * mp3buf_size in bytes = 1.25*num_samples + 7200
          */
         handle->mp3BufSize = 7200 + handle->no_samples*5/4;
         handle->mp3ptr = _aax_malloc(&ptr, handle->mp3BufSize);
         handle->mp3Buffer = ptr;
         if (handle->mp3ptr)
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
   else
   {
      _AAX_FILEDRVLOG("MPG123: Internal error: handle id equals 0");
   }

   return rv;
}

static int
_aaxMPG123Close(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int ret = AAX_TRUE;

   if (handle)
   {
      if (handle->capturing)
      {
         pmpg123_delete(handle->id);
         handle->id = NULL;
         pmpg123_exit();
      }
      else
      {
         plame_encode_flush(handle->id, handle->mp3Buffer, handle->mp3BufSize);
         // plame_mp3_tags_fid(handle->id, mp3);
         plame_close(handle->id);
      }
      free(handle->mp3ptr);

#ifdef WINXP
      free(handle->pcmBuffer);
#endif

      free(handle->artist);
      free(handle->title);
      free(handle->album);
      free(handle->date);
      free(handle->genre);
      free(handle->comments);
      free(handle->composer);
      free(handle->copyright);
      free(handle->original);
      free(handle->image);
      free(handle);
   }

   return ret;
}

static size_t
_aaxMPG123CvtFromIntl(void *id, int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   _driver_t *handle = (_driver_t *)id;
   int bits, ret, rv = __F_EOF;
   size_t bytes, size = 0;

   bits = handle->bits_sample;
   bytes = num*tracks*bits/8;
   if (!sptr)	/* decode from our own buffer */
   {
      unsigned char *buf = (unsigned char*)handle->mp3Buffer;
      size_t bufsize = handle->mp3BufSize;

      if (bytes > bufsize) {
         bytes = bufsize;
      }
      ret = pmpg123_read(handle->id, buf, bytes, &size);
      if (!handle->id3_found) detect_mpg123_song_info(handle);
      if (ret == MPG123_OK || ret == MPG123_NEED_MORE)
      {
         rv = size*8/(tracks*bits);
         _batch_cvt24_16_intl(dptr, buf, offset, tracks, rv);
      }
   }
   else /* provide the next chunk to our own buffer */
   {
      ret = pmpg123_feed(handle->id, sptr, bytes);
      if (!handle->id3_found) detect_mpg123_song_info(handle);
      if (ret == MPG123_OK) {
         rv = __F_PROCESS;
      }
   }

   return rv;
}

static size_t
_aaxMPG123CvtToIntl(void *id, void_ptr dptr, const_int32_ptrptr sptr, size_t offset, unsigned int tracks, size_t num, void *scratch, size_t scratchlen)
{
   _driver_t *handle = (_driver_t *)id;
   int res;

   assert(scratchlen >= num*tracks*sizeof(int32_t));

   _batch_cvt16_intl_24(scratch, sptr, offset, tracks, num);
   res = plame_encode_buffer_interleaved(handle->id, scratch, num,
                                         handle->mp3Buffer, handle->mp3BufSize);
   _aax_memcpy(dptr, handle->mp3Buffer, res);

   return res;
}

static off_t
_aaxMPG123SetParam(void *id, int type, off_t value)
{
   _driver_t *handle = (_driver_t *)id;
   off_t rv = 0;

   switch(type)
   {
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


static int
getFormatFromMP3Format(int enc)
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
detect_mpg123_song_info(_driver_t *handle)
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
               if ((genre < MAX_ID3V1_GENRES) && (*end == ')')) {
                  handle->genre = strdup(_mp3v1_genres[genre]);
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
            if (v1->genre < MAX_ID3V1_GENRES) {
               handle->genre = strdup(_mp3v1_genres[v1->genre]);
            }
            handle->id3_found = AAX_TRUE;
         }
      }
   }
}

