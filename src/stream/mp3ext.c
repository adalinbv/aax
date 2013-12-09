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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>   /* strcasecmp */
# endif
# if HAVE_UNISTD_H
#  include <unistd.h>
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

#include "filetype.h"
#include "audio.h"
#include "software/arch.h"
#include "software/ringbuffer.h"

        /** libmpg123: both Linux and Windows */
static _open_fn _aaxMPG123Open;
static _close_fn _aaxMPG123Close;
static _cvt_to_fn _aaxMPG123CvtToIntl;
static _cvt_from_fn _aaxMPG123CvtFromIntl;
	/** libmpg123 */

static _detect_fn _aaxMP3Detect;
static _new_handle_fn _aaxMP3Setup;
static _default_fname_fn _aaxMP3Interfaces;
static _extension_fn _aaxMP3Extension;
static _get_param_fn _aaxMP3GetParam;

#ifdef WINXP
	/** windows (xp and later) native */
static _open_fn _aaxMSACMFileOpen;
static _close_fn _aaxMSACMFileClose;
static _update_fn _aaxMSACMFileCvtFrom;
static _update_fn _aaxMSACMFileCvtTo;

static _open_fn *_aaxMP3Open = _aaxMSACMFileOpen;
static _close_fn *_aaxMP3Close = _aaxMSACMFileClose;
static _update_fn *_aaxMP3CvtFrom = _aaxMSACMFileCvtFrom;
static _update_fn *_aaxMP3CvtTo = _aaxMSACMFileCvtTo;

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

static _open_fn *_aaxMP3Open = _aaxMPG123Open;
static _close_fn *_aaxMP3Close = _aaxMPG123Close;
static _cvt_to_fn *_aaxMP3CvtToIntl = _aaxMPG123CvtToIntl;
static _cvt_from_fn *_aaxMP3CvtFromIntl = _aaxMPG123CvtFromIntl;
#endif

	/** libmpg123: both Linxu and Windows */
DECL_FUNCTION(mpg123_init);
DECL_FUNCTION(mpg123_exit);
DECL_FUNCTION(mpg123_new);
DECL_FUNCTION(mpg123_open_feed);
DECL_FUNCTION(mpg123_decode);
DECL_FUNCTION(mpg123_delete);
DECL_FUNCTION(mpg123_format);
DECL_FUNCTION(mpg123_format_none);
DECL_FUNCTION(mpg123_getformat);

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
      rv->update = NULL;

      rv->cvt_from_intl = _aaxMP3CvtFromIntl;
      rv->cvt_to_intl = _aaxMP3CvtToIntl;
      rv->cvt_endianness = NULL;
      rv->cvt_from_signed = NULL;
      rv->cvt_to_signed = NULL;

      rv->supported = _aaxMP3Extension;
      rv->interfaces = _aaxMP3Interfaces;

      rv->get_param = _aaxMP3GetParam;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

typedef struct
{
   void *id;

   int mode;
   int capturing;

   int frequency;
   int bitrate;
   unsigned int no_samples;
   enum aaxFormat format;
   int blocksize;
   uint8_t no_tracks;
   uint8_t bits_sample;

   unsigned int mp3BufSize;
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
      _audio[m] = _oalIsLibraryPresent("msacm32", NULL);
      if (_audio[m])
      {
         void *audio = _audio[m];
         char *error;

         _oalGetSymError(0);

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

            error = _oalGetSymError(0);
            if (!error)
            {
               if (!acmMP3Support) {
                  pacmDriverEnum(acmDriverEnumCallback, 0, 0);
               }

               if (acmMP3Support)
               {
//                _aaxMP3Open = (_open_fn*)&_aaxMSACMFileOpen;
//                _aaxMP3Close = (_close_fn*)&_aaxMSACMFileClose;
//                _aaxMP3CvtFrom = (_update_fn*)&_aaxMSACMFileCvtFrom;
//                _aaxMP3CvtTo = (_update_fn*)&_aaxMSACMFileCvtTo;
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
            _audio[m] = _oalIsLibraryPresent("mpg123", "0");
            if (!_audio[m]) {
               _audio[m] = _oalIsLibraryPresent("libmpg123-0", "0");
            }

            if (_audio[m])
            {
               void *audio = _audio[m];
               char *error;

               _oalGetSymError(0);

               TIE_FUNCTION(mpg123_init);
               if (pmpg123_init)
               {
                  TIE_FUNCTION(mpg123_exit);
                  TIE_FUNCTION(mpg123_new);
                  TIE_FUNCTION(mpg123_open_feed);
                  TIE_FUNCTION(mpg123_decode);
                  TIE_FUNCTION(mpg123_delete);
                  TIE_FUNCTION(mpg123_format);
                  TIE_FUNCTION(mpg123_format_none);
                  TIE_FUNCTION(mpg123_getformat);

                  error = _oalGetSymError(0);
                  if (error) {
                     _audio[m] = NULL;
                  } else {
                     rv = AAX_TRUE;
                  }
               }
            }
         }
         else /* write */
         {
            _audio[m] = _oalIsLibraryPresent("mp3lame", "0");
            if (!_audio[m]) {
               _audio[m] = _oalIsLibraryPresent("libmp3lame", "0");
            }
//          if (!_audio[m]) {
//             _audio[m] = _oalIsLibraryPresent("lame_enc", "0");
//          }

            if (_audio[m])
            {
               void *audio = _audio[m];
               char *error;
               _oalGetSymError(0);

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

                  error = _oalGetSymError(0);
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
_aaxMP3Setup(int mode, unsigned int *bufsize, int freq, int tracks, int format, int no_samples, int bitrate)
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
   return !strcasecmp(ext, "mp3");
}

static char*
_aaxMP3Interfaces(int mode)
{
   static const char *rd[2] = { "*.mp3\0", "*.mp3\0" };
   return (char *)rd[mode];
}

static unsigned int
_aaxMP3GetParam(void *id, int type)
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
_aaxMPG123Open(void *id, void *buf, unsigned int *bufsize)
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
               pmpg123_open_feed(handle->id);
               handle->mp3BufSize = 16384;

               handle->mp3ptr = _aax_malloc(&ptr, handle->mp3BufSize);
               handle->mp3Buffer = ptr;
            }
         }

         if (handle->id)
         {
            size_t size;
            int ret;

            ret = pmpg123_decode(handle->id, buf, *bufsize, NULL, 0, &size);
            if (ret == MPG123_NEW_FORMAT)
            {
               int enc, channels;
               long rate;

               ret = pmpg123_getformat(handle->id, &rate, &channels, &enc);
               if (ret == MPG123_OK)
               {
                  pmpg123_format_none(handle->id);
                  ret = pmpg123_format(handle->id, handle->frequency,
                                       MPG123_MONO | MPG123_STEREO,
                                       MPG123_ENC_SIGNED_16);

                  if (ret == MPG123_OK &&
                      (1000 <= rate) && (rate <= 192000) &&
                      (1 <= channels) && (channels <= _AAX_MAX_SPEAKERS))
                  {
                     handle->frequency = rate;
                     handle->no_tracks = channels;
                     handle->format = getFormatFromMP3FileFormat(enc);
                     handle->bits_sample = aaxGetBitsPerSample(handle->format);

                     *bufsize = 0;
                     rv = buf;
                  }
               }
               else {
                  _AAX_FILEDRVLOG("MP3File: file may be corrupt");
               }
            }
            else if (ret == MPG123_NEED_MORE) {
               rv = buf;
            }
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
      free(handle);
   }

   return ret;
}

static int
_aaxMPG123CvtFromIntl(void *id, int32_ptrptr dptr, const_void_ptr sptr, int offset, unsigned int tracks, unsigned int num)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int bytes, bufsize;
   int bps, ret, rv = __F_EOF;
   unsigned char *buf;
   size_t size = 0;

   buf = (unsigned char*)handle->mp3Buffer;
   bufsize = handle->mp3BufSize;

   bps = handle->bits_sample/8;
   bytes = num*tracks*bps;
   if (!sptr)
   {
      if (bytes > bufsize) {
         bytes = bufsize;
      }
      ret = pmpg123_decode(handle->id, NULL, 0, buf, bytes, &size);
      if (ret == MPG123_OK || ret == MPG123_NEED_MORE)
      {
         rv = size/(tracks*bps);
         _batch_cvt24_16_intl(dptr, buf, offset, tracks, rv);
      }
   }
   else
   {
      ret = pmpg123_decode(handle->id, sptr, bytes, NULL, 0, &size);
      if (ret == MPG123_OK) {
         rv = __F_PROCESS;
      }
   }

   return rv;
}

static int
_aaxMPG123CvtToIntl(void *id, void_ptr dptr, const_int32_ptrptr sptr, int offset, unsigned int tracks, unsigned int num, void *scratch, unsigned int scratchlen)
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

#ifdef WINXP
static int
_aaxMSACMFileOpen(void *id, const char* fname)
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
_aaxMSACMFileClose(void *id)
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
_aaxMSACMFileCvtTo(void *id, void *data, unsigned int no_frames)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int frame_sz = handle->no_tracks*handle->bits_sample/8;
   unsigned int avail_bytes;
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
_aaxMSACMFileCvtTo(void *id, void *data, unsigned int no_frames)
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
