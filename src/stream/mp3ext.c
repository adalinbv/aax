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

#if HAVE_UNISTD_H
# include <unistd.h>
#endif
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
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_IO_H
# include <io.h>
#endif

#include <aax/aax.h>

#include <base/dlsym.h>

#include <arch.h>
#include <ringbuffer.h>

#include "filetype.h"
#include "audio.h"

#define	MAX_ID3V1_GENRES	192
#define __DUP(a, b)	if ((b) != NULL && (b)->fill) a = strdup((b)->p);
#define __COPY(a, b)	do { int s = sizeof(b); \
      a = calloc(1, s+1); if (a) memcpy(a,b,s); \
   } while(0);
const char *_mp3v1_genres[MAX_ID3V1_GENRES];

        /** libmpg123: both Linux and Windows */
static _file_open_fn _aaxMPG123Open;
static _file_close_fn _aaxMPG123Close;
static _file_cvt_to_fn _aaxMPG123CvtToIntl;
static _file_cvt_from_fn _aaxMPG123CvtFromIntl;
static _file_set_param_fn _aaxMPG123SetParam;
	/** libmpg123 */

static _file_detect_fn _aaxMP3Detect;
static _file_new_handle_fn _aaxMP3Setup;
static _file_get_name_fn _aaxMP3GetName;
static _file_default_fname_fn _aaxMP3Interfaces;
static _file_extension_fn _aaxMP3Extension;
static _file_get_param_fn _aaxMP3GetParam;

#ifdef WINXP
	/** windows (xp and later) native */
static _file_open_fn _aaxMSACMOpen;
static _file_close_fn _aaxMSACMClose;
static _file_update_fn _aaxMSACMCvtFrom;
static _file_update_fn _aaxMSACMCvtTo;

static _file_open_fn *_aaxMP3Open = _aaxMSACMOpen;
static _file_close_fn *_aaxMP3Close = _aaxMSACMClose;
static _file_update_fn *_aaxMP3CvtFrom = _aaxMSACMCvtFrom;
static _file_update_fn *_aaxMP3CvtTo = _aaxMSACMCvtTo;
static _file_set_param_fn *_aaxM3SetParam = aaxMSACMSetParam;

DECL_FUNCTION(acmDriverOpen);
DECL_FUNCTION(acmDriverClose);
DECL_FUNCTION(acmDriverEnum);
DECL_FUNCTION(acmDriverDetailsA);
DECL_FUNCTION(acmStreamOpen);
DECL_FUNCTION(acmStreamClose);
DECL_FUNCTION(acmStreamSize);
DECL_FUNCTION(acmStreamConvert);
DECL_FUNCTION(acmStreamPrepareHeader);
DECL_FUNCTION(acmStreamUnprepareHeader);
DECL_FUNCTION(acmFormatTagDetailsA);

static int acmMP3Support = AAX_FALSE;
static BOOL CALLBACK acmDriverEnumCallback(HACMDRIVERID, DWORD, DWORD);

#else

static _file_open_fn *_aaxMP3Open = _aaxMPG123Open;
static _file_close_fn *_aaxMP3Close = _aaxMPG123Close;
static _file_cvt_to_fn *_aaxMP3CvtToIntl = _aaxMPG123CvtToIntl;
static _file_cvt_from_fn *_aaxMP3CvtFromIntl = _aaxMPG123CvtFromIntl;
static _file_set_param_fn *_aaxMP3SetParam = _aaxMPG123SetParam;

#endif

	/** libmpg123: both Linxu and Windows */
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

	/** lame: both Linxu and Windows */
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

_aaxFmtHandle*
_aaxDetectMP3File()
{
   _aaxFmtHandle* rv = NULL;

   rv = calloc(1, sizeof(_aaxFmtHandle));
   if (rv)
   {
      rv->detect = _aaxMP3Detect;
      rv->setup = _aaxMP3Setup;
      rv->open = _aaxMP3Open;
      rv->close = _aaxMP3Close;
      rv->name = _aaxMP3GetName;
      rv->update = NULL;

      rv->cvt_from_intl = _aaxMP3CvtFromIntl;
      rv->cvt_to_intl = _aaxMP3CvtToIntl;
      rv->cvt_endianness = NULL;
      rv->cvt_from_signed = NULL;
      rv->cvt_to_signed = NULL;

      rv->supported = _aaxMP3Extension;
      rv->interfaces = _aaxMP3Interfaces;

      rv->get_param = _aaxMP3GetParam;
      rv->set_param = _aaxMP3SetParam;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

typedef struct
{
   void *id;
   char *artist;
   char *title;
   char *album;
   char *trackno;
   char *date;
   char *genre;
   char *comments;
   char *image;

   int mode;
   int capturing;
   int id3_found;

   int frequency;
   int bitrate;
   size_t no_samples;
   size_t max_samples;
   enum aaxFormat format;
   int blocksize;
   uint8_t no_tracks;
   uint8_t bits_sample;

   size_t mp3BufSize;
   void *mp3Buffer;
   void *mp3ptr;

#ifdef WINXP
   HACMSTREAM acmStream;
   ACMSTREAMHEADER acmStreamHeader;

   LPBYTE pcmBuffer;
   unsigned long pcmBufPos;
   unsigned long pcmBufMax;
#endif

} _driver_t;

static void detect_mpg123_song_info(_driver_t*);

static int
_aaxMP3Detect(int mode)
{
   static void *_audio[2] = { NULL, NULL };
   int m = mode > 0 ? 1 : 0;
   int rv = AAX_FALSE;

   if (_audio[m]) {
      rv = AAX_TRUE;
   }
   else
   {
#ifdef WINXP
      _audio[m] = _aaxIsLibraryPresent("msacm32", NULL);
      if (_audio[m])
      {
         void *audio = _audio[m];
         char *error;

         _aaxGetSymError(0);

         TIE_FUNCTION(acmDriverOpen);
         if (pacmDriverOpen)
         {
            TIE_FUNCTION(acmDriverClose);
            TIE_FUNCTION(acmDriverEnum);
            TIE_FUNCTION(acmDriverDetailsA);
            TIE_FUNCTION(acmStreamOpen);
            TIE_FUNCTION(acmStreamClose);
            TIE_FUNCTION(acmStreamSize);
            TIE_FUNCTION(acmStreamConvert);
            TIE_FUNCTION(acmStreamPrepareHeader);
            TIE_FUNCTION(acmStreamUnprepareHeader);
            TIE_FUNCTION(acmFormatTagDetailsA);

            error = _aaxGetSymError(0);
            if (!error)
            {
               if (!acmMP3Support) {
                  pacmDriverEnum(acmDriverEnumCallback, 0, 0);
               }

               if (acmMP3Support)
               {
//                _aaxMP3Open = (_open_fn*)&_aaxMSACMOpen;
//                _aaxMP3Close = (_close_fn*)&_aaxMSACMClose;
//                _aaxMP3CvtFrom = (_update_fn*)&_aaxMSACMCvtFrom;
//                _aaxMP3CvtTo = (_update_fn*)&_aaxMSACMCvtTo;
                  rv = AAX_TRUE;
               }
            }
         }
      }
#endif	/* WINXP */

      /* if not found, try mpg123 */
      if (!_audio[m])
      {
         if (mode == 0) /* read */
         {
            _audio[m] = _aaxIsLibraryPresent("mpg123", "0");
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
                  } else {
                     rv = AAX_TRUE;
                  }

                  /* not required but useful */
                  TIE_FUNCTION(mpg123_length);
                  TIE_FUNCTION(mpg123_set_filesize);
                  TIE_FUNCTION(mpg123_feedseek);
                  TIE_FUNCTION(mpg123_meta_check);
                  TIE_FUNCTION(mpg123_id3);
               }
            }
         }
         else /* write */
         {
            _audio[m] = _aaxIsLibraryPresent("mp3lame", "0");
            if (!_audio[m]) {
               _audio[m] = _aaxIsLibraryPresent("libmp3lame", "0");
            }
//          if (!_audio[m]) {
//             _audio[m] = _aaxIsLibraryPresent("lame_enc", "0");
//          }

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
                  } else {
                     rv = AAX_TRUE;
                  }
               }
            }
         }
      }
   } /* !audio[m] */
   return rv;
}

static void*
_aaxMP3Setup(int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
{
   _driver_t *handle = NULL;
   
   *bufsize = 0;
   if (1) // mode == 0)
   {
      handle = calloc(1, sizeof(_driver_t));
      if (handle)
      {
         handle->capturing = (mode == 0) ? 1 : 0;
         handle->blocksize = 4096;
         handle->frequency = freq;
         handle->no_tracks = tracks;
         handle->bitrate = bitrate;
         handle->format = format;
         handle->no_samples = no_samples;
         handle->max_samples = UINT_MAX;
         handle->bits_sample = aaxGetBitsPerSample(handle->format);

         if (mode == 0) {
            *bufsize = (no_samples*tracks*handle->bits_sample)/8;
         }
      }
      else {
         _AAX_FILEDRVLOG("MP3File: Insufficient memory");
      }
   }
   else {
      _AAX_FILEDRVLOG("MP3File: playback is not supported");
   }

   return (void*)handle;
}

static int
_aaxMP3Extension(char *ext) {
   return (ext && !strcasecmp(ext, "mp3")) ? 1 : 0;
}

static char*
_aaxMP3Interfaces(int mode)
{
   static const char *rd[2] = { "*.mp3\0", "*.mp3\0" };
   return (char *)rd[mode];
}

static char*
_aaxMP3GetName(void *id, enum _aaxFileParam param)
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
   case __F_IMAGE:
      rv = handle->image;
      break;
   default:
      break;
   }
   return rv;
}

static off_t
_aaxMP3GetParam(void *id, int type)
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


static int
getFormatFromMP3FileFormat(int enc)
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
               pmpg123_param(handle->id, MPG123_ADD_FLAGS, MPG123_GAPLESS, 1);
               pmpg123_param(handle->id, MPG123_RESYNC_LIMIT, -1, 0.0);
               pmpg123_param(handle->id, MPG123_REMOVE_FLAGS,
                                         MPG123_AUTO_RESAMPLE, 0);

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
                  _AAX_FILEDRVLOG("MP3File: Unable to initialize mpg123");
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
                  handle->format = getFormatFromMP3FileFormat(enc);
                  handle->bits_sample = aaxGetBitsPerSample(handle->format);

                  rv = buf;

                  if (pmpg123_length)
                  {
                     off_t length = pmpg123_length(handle->id);
                     handle->max_samples = (length > 0) ? length : UINT_MAX;
                  }
               }
               else {
                  _AAX_FILEDRVLOG("MP3File: file may be corrupt");
               }
            }
            else if (ret == MPG123_NEED_MORE) {
               rv = buf;
            }
            // else we're done decoding, return NULL
         }
         else {
            _AAX_FILEDRVLOG("MP3File: Unable to create a handler");
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
      _AAX_FILEDRVLOG("MP3File: Internal error: handle id equals 0");
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
      free(handle->image);
      free(handle);
   }

   return ret;
}

static size_t
_aaxMPG123CvtFromIntl(void *id, int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   _driver_t *handle = (_driver_t *)id;
   int bps, ret, rv = __F_EOF;
   size_t bytes;
   size_t size = 0;

   bps = handle->bits_sample/8;
   bytes = num*tracks*bps;
   if (!sptr)
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
         rv = size/(tracks*bps);
         _batch_cvt24_16_intl(dptr, buf, offset, tracks, rv);
      }
   }
   else
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
   case __F_POSITION:
      
      if (pmpg123_feedseek)
      {
         off_t inoffset;
         rv = pmpg123_feedseek(handle->id, value, SEEK_SET, &inoffset);
         if (rv ==  MPG123_NEED_MORE) rv = __F_PROCESS;
         else if (rv < 0) rv = __F_EOF;
      }
      break;
   default:
      break;
   }
   return rv;
}


#ifdef WINXP
static int
_aaxMSACMOpen(void *id, const char* fname)
{
   _driver_t *handle = (_driver_t *)id;
   int res = __F_EOF;

   if (handle && fname)
   {
      handle->fd = open(fname, handle->mode, 0644);
      if (handle->fd >= 0)
      {
         char *ptr = 0;

         handle->name = strdup(fname);
         handl->mp3ptr = _aax_malloc(&ptr, MP3_BLOCK_SIZE);
         handle->mp3Buffer = ptr;

         if (handle->capturing)
         {
            MPEGLAYER3WAVEFORMAT mp3Fmt;
            WAVEFORMATEX pcmFmt;
            HRESULT hr;

			/** PCM */
            pcmFmt.wFormatTag = WAVE_FORMAT_PCM;
            pcmFmt.nChannels = 2; //handle->no_tracks;
            pcmFmt.nSamplesPerSec = 44100; // handle->frequency;
            pcmFmt.wBitsPerSample = 16; // handle->bits_sample;
            pcmFmt.nBlockAlign = pcmFmt.nChannels*pcmFmt.wBitsPerSample/8;
            pcmFmt.nAvgBytesPerSec= pcmFmt.nSamplesPerSec*pcmFmt.nBlockAlign;
            pcmFmt.cbSize = 0;

			/* MP3 */
            mp3Fmt.wfx.cbSize = MPEGLAYER3_WFX_EXTRA_BYTES;
            mp3Fmt.wfx.wFormatTag = WAVE_FORMAT_MPEGLAYER3;
            mp3Fmt.wfx.nChannels = 2;

            // not really used but must be one of 64, 96, 112, 128, 160kbps
            mp3Fmt.wfx.nAvgBytesPerSec = 128*1024/8;
            mp3Fmt.wfx.wBitsPerSample = 0;
            mp3Fmt.wfx.nBlockAlign = 1;

            // must be the same as pcmFmt.nSamplesPerSec
            mp3Fmt.wfx.nSamplesPerSec = 44100; // handle->frequency;
            mp3Fmt.fdwFlags = MPEGLAYER3_FLAG_PADDING_OFF;
            mp3Fmt.nBlockSize = MP3_BLOCK_SIZE;
            mp3Fmt.nFramesPerBlock = 1;
            mp3Fmt.nCodecDelay = 1393;
            mp3Fmt.wID = MPEGLAYER3_ID_MPEG;

            handle->acmStream = NULL;
            hr = pacmStreamOpen(&handle->acmStream, NULL,
                               (LPWAVEFORMATEX)&mp3Fmt,
                               &pcmFmt, NULL, 0, 0, 0);
            switch(hr)
            {
            case MMSYSERR_NOERROR:   // success
            {
               hr = pacmStreamSize(handle->acmStream, MP3_BLOCK_SIZE,
                                  &handle->pcmBufMax,
                                  ACM_STREAMSIZEF_SOURCE);
               if ((hr != 0) || (handle->pcmBufMax == 0))
               {
                  _AAX_FILEDRVLOG("MSACM MP3: Unable te retreive stream size");
                  goto MSACMStreamDone;
               }

               handle->pcmBuffer = malloc(handle->pcmBufMax);
               handle->pcmBufPos = handle->pcmBufMax;

               memset(&handle->acmStreamHeader, 0, sizeof(ACMSTREAMHEADER));
               handle->acmStreamHeader.cbStruct = sizeof(ACMSTREAMHEADER);
               handle->acmStreamHeader.pbSrc = handle->mp3Buffer;
               handle->acmStreamHeader.cbSrcLength = MP3_BLOCK_SIZE;
               handle->acmStreamHeader.pbDst = handle->pcmBuffer;
               handle->acmStreamHeader.cbDstLength = handle->pcmBufMax;
               hr = pacmStreamPrepareHeader(handle->acmStream,
                                           &handle->acmStreamHeader, 0);
               if (hr != 0)
               {
                  _AAX_FILEDRVLOG("MSACM MP3: conversion stream error");
                  goto MSACMStreamDone;
               }

               if ((1000 <= pcmFmt.nSamplesPerSec)
                   && (pcmFmt.nSamplesPerSec <= 192000)
                   && (1 <= pcmFmt.nChannels)
                   && (pcmFmt.nChannels <= _AAX_MAX_SPEAKERS))
               {
                  handle->no_tracks = pcmFmt.nChannels;
                  handle->frequency = pcmFmt.nSamplesPerSec;
                  handle->bits_sample = pcmFmt.wBitsPerSample;
                  handle->format = getFormatFromWAVFileFormat(PCM_WAVE_FILE,
                                                           handle->bits_sample);
                  res = AAX_TRUE;
               }
               else {
                  _AAX_FILEDRVLOG("MSACM MP3: file may be corrupt");
               }
MSACMStreamDone:
               break;
            }
            case MMSYSERR_INVALPARAM:
               _AAX_FILEDRVLOG("MSACM MP3: Invalid parameters passed");
               break;
            case ACMERR_NOTPOSSIBLE:
               _AAX_FILEDRVLOG("MSACM MP3: filter not found");
            default:
               break;
               _AAX_FILEDRVLOG("MSACM MP3: Unknown error when opening stream");
            }
         }
         else
         {
            _AAX_FILEDRVLOG("MP3File: playback is not supported");
            close(handle->fd); /* no mp3 write support (yet) */
         }
      }
      else {
         _AAX_FILEDRVLOG("MP3File: file not found");
      }
   }
   else
   {
      if (!fname) {
         _AAX_FILEDRVLOG("MP3File: No filename provided");
      } else {
         _AAX_FILEDRVLOG("MP3File: Internal error: handle id equals 0");
      }
   }

   return (res >= 0) ? AAX_TRUE : AAX_FALSE;
}

static int
_aaxMSACMClose(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int ret = AAX_TRUE;

   if (handle)
   {
      pacmStreamUnprepareHeader(handle->acmStream, &handle->acmStreamHeader, 0);
      pacmStreamClose(handle->acmStream, 0);

      close(handle->fd);
      free(handle->name);
      free(handle);

      CoUninitialize();
   }

   return ret;
}

static int
_aaxMSACMCvtTo(void *id, void *data, size_t no_frames)
{
   _driver_t *handle = (_driver_t *)id;
   size_t frame_sz = handle->no_tracks*handle->bits_sample/8;
   size_t avail_bytes;
   char *ptr = data;
   int rv = 0;

   if (handle->pcmBufPos < handle->pcmBufMax)
   {
      avail_bytes = handle->pcmBufMax - handle->pcmBufPos;
      if (avail_bytes > no_frames*frame_sz) {
         avail_bytes = no_frames*frame_sz;
      }

      memcpy(ptr, handle->pcmBuffer+handle->pcmBufPos, avail_bytes);
      handle->pcmBufPos += avail_bytes;
      ptr += avail_bytes;

      no_frames -= avail_bytes/frame_sz;
      rv += avail_bytes/frame_sz;
   }

   while (no_frames)
   {
      // TODO: adopt new scheme weher de file_driver does the reading.
      int res = 0; // read(handle->fd, handle->mp3Buffer, MP3_BLOCK_SIZE);
      if (res == MP3_BLOCK_SIZE)
      {
         HRESULT hr;
         hr = pacmStreamConvert(handle->acmStream, &handle->acmStreamHeader,
                                              ACM_STREAMCONVERTF_BLOCKALIGN);
         if ((hr == 0) && handle->acmStreamHeader.cbDstLengthUsed)
         {
            handle->pcmBufPos = 0;
            handle->pcmBufMax = handle->acmStreamHeader.cbDstLengthUsed;
            if (handle->pcmBufPos < handle->pcmBufMax)
            {
               avail_bytes = handle->pcmBufMax - handle->pcmBufPos;
               if (avail_bytes > no_frames*frame_sz) {
                  avail_bytes = no_frames*frame_sz;
               }

               memcpy(ptr, handle->pcmBuffer, avail_bytes);
               handle->pcmBufPos += avail_bytes;
               ptr += avail_bytes;

               no_frames -= avail_bytes/frame_sz;
               rv += avail_bytes/frame_sz;
            }
         }
         else if (hr != 0)
         {
            _AAX_FILEDRVLOG("MSACM MP3: error while converting the stream");
            handle->pcmBufPos = handle->pcmBufMax;
            break;
         }
      }
      else
      {
         rv = __F_EOF;
         break;
      }
   }	/* while (no_frames) */

   return rv;
}

static int
_aaxMSACMCvtTo(void *id, void *data, size_t no_frames)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = __F_EOF;

   return rv;
}

/* -------------------------------------------------------------------------- */

static BOOL CALLBACK
acmDriverEnumCallback(HACMDRIVERID hadid, DWORD dwInstance, DWORD fdwSupport)
{
   if (fdwSupport & ACMDRIVERDETAILS_SUPPORTF_CODEC)
   {
      ACMDRIVERDETAILS details;
      MMRESULT mmr;

      details.cbStruct = sizeof(ACMDRIVERDETAILS);
      mmr = pacmDriverDetailsA(hadid, &details, 0);
      if (!mmr)
      {
         HACMDRIVER driver;
         mmr = pacmDriverOpen(&driver, hadid, 0);
         if (!mmr)
         {
            int i;
            for(i=0; i<details.cFormatTags; i++)
            {
               ACMFORMATTAGDETAILS fmtDetails;

               memset(&fmtDetails, 0, sizeof(fmtDetails));
               fmtDetails.cbStruct = sizeof(ACMFORMATTAGDETAILS);
               fmtDetails.dwFormatTagIndex = i;
               mmr = pacmFormatTagDetailsA(driver, &fmtDetails,
                                         ACM_FORMATTAGDETAILSF_INDEX);
               if (fmtDetails.dwFormatTag == WAVE_FORMAT_MPEGLAYER3)
               {
                  acmMP3Support = AAX_TRUE;
                  break;
               }
            }
            mmr = pacmDriverClose(driver, 0);
         }
      }
   }
   return AAX_TRUE;
}
#endif /* WINXP */

void
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

      if ((meta & MPG123_ID3) && (pmpg123_id3(handle->id, &v1, &v2) == MPG123_OK))
      {
         if (v2)
         {
            __DUP(handle->artist, v2->artist);
            __DUP(handle->title, v2->title);
            __DUP(handle->album, v2->album);
            __DUP(handle->date, v2->year);
            __DUP(handle->genre, v2->genre);
            __DUP(handle->comments, v2->comment);
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

const char *
_mp3v1_genres[MAX_ID3V1_GENRES] = 
{
   "Blues",
   "Classic Rock",
   "Country",
   "Dance",
   "Disco",
   "Funk",
   "Grunge",
   "Hip-Hop",
   "Jazz",
   "Metal",
   "New Age",
   "Oldies",
   "Other",
   "Pop",
   "Rhythm and Blues",
   "Rap",
   "Reggae",
   "Rock",
   "Techno",
   "Industrial",
   "Alternative",
   "Ska",
   "Death Metal",
   "Pranks",
   "Soundtrack",
   "Euro-Techno",
   "Ambient",
   "Trip-Hop",
   "Vocal",
   "Jazz & Funk",
   "Fusion",
   "Trance",
   "Classical",
   "Instrumental",
   "Acid",
   "House",
   "Game",
   "Sound Clip",
   "Gospel",
   "Noise",
   "Alternative Rock",
   "Bass",
   "Soul",
   "Punk rock",
   "Space",
   "Meditative",
   "Instrumental Pop",
   "Instrumental Rock",
   "Ethnic",
   "Gothic",
   "Darkwave",
   "Techno-Industrial",
   "Electronic",
   "Pop-Folk",
   "Eurodance",
   "Dream",
   "Southern Rock",
   "Comedy",
   "Cult",
   "Gangsta",
   "Top 40",
   "Christian Rap",
   "Pop/Funk",
   "Jungle",
   "Native American",
   "Cabaret",
   "New Wave",
   "Psychedelic",
   "Rave",
   "Showtunes",
   "Trailer",
   "Lo-Fi",
   "Tribal",
   "Acid Punk",
   "Acid Jazz",
   "Polka",
   "Retro",
   "Musical",
   "Rock & Roll",
   "Hard Rock",
   "Folk",
   "Folk-Rock",
   "National Folk",
   "Swing",
   "Fast Fusion",
   "Bebop",
   "Latin",
   "Revival",
   "Celtic",
   "Bluegrass",
   "Avantgarde",
   "Gothic Rock",
   "Progressive Rock",
   "Psychedelic Rock",
   "Symphonic Rock",
   "Slow Rock",
   "Big Band",
   "Chorus",
   "Easy Listening",
   "Acoustic",
   "Humour",
   "Speech",
   "Chanson",
   "Opera",
   "Chamber Music",
   "Sonata",
   "Symphony",
   "Booty Bass",
   "Primus",
   "Porn groove",
   "Satire",
   "Slow Jam",
   "Club",
   "Tango",
   "Samba",
   "Folklore",
   "Ballad",
   "Power Ballad",
   "Rhythmic Soul",
   "Freestyle",
   "Duet",
   "Punk rock",
   "Drum Solo",
   "A capella",
   "Euro-House",
   "Dance Hall",
   "Goa Trance",
   "Drum & Bass",
   "Club-House",
   "Hardcore Techno",
   "Terror",
   "Indie",
   "BritPop",
   "Afro-punk",
   "Polsk Punk",
   "Beat",
   "Christian Gangsta Rap",
   "Heavy Metal",
   "Black Metal",
   "Crossover",
   "Contemporary Christian",
   "Christian Rock",
   "Merengue",
   "Salsa",
   "Thrash Metal",
   "Anime",
   "Jpop",
   "Synthpop",
   "Abstract",
   "Art Rock",
   "Baroque",
   "Bhangra",
   "Big Beat",
   "Breakbeat",
   "Chillout",
   "Downtempo",
   "Dub",
   "EBM",
   "Eclectic",
   "Electro",
   "Electroclash",
   "Emo",
   "Experimental",
   "Garage",
   "Global",
   "IDM",
   "Illbient",
   "Industro-Goth",
   "Jam Band",
   "Krautrock",
   "Leftfield",
   "Lounge",
   "Math Rock",
   "New Romantic",
   "Nu-Breakz",
   "Post-Punk",
   "Post-Rock",
   "Psytrance",
   "Shoegaze",
   "Space Rock",
   "Trop Rock",
   "World Music",
   "Neoclassical",
   "Audiobook",
   "Audio Theatre",
   "Neue Deutsche Welle",
   "Podcast",
   "Indie Rock",
   "G-Funk",
   "Dubstep",
   "Garage Rock",
   "Psybient"
};
