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

#include "fmt_mp3_msacm.h"

#ifdef WINXP
	/** windows (xp and later) native */
static _fmt_open_fn _aaxMSACMOpen;
static _fmt_close_fn _aaxMSACMClose;
static _fmt_update_fn _aaxMSACMCvtFrom;
static _fmt_update_fn _aaxMSACMCvtTo;

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
#endif

/* -------------------------------------------------------------------------- */

int
_aaxMSACMDetect(void *format, int m, void *_audio[2])
{
   int rv = AAX_FALSE;

#ifdef WINXP
   _aaxFmtHandle *fmt = (_aaxFmtHandle*)format;

   if (!_audio[m]) {
      _audio[m] = _aaxIsLibraryPresent("msacm32", NULL);
   }
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
               fmt->open = _aaxMSACMOpen;
               fmt->close = _aaxMSACMClose;
               fmt->cvt_from_intl = _aaxMSACMCvtFrom;
               fmt->cvt_to_intl = _aaxMSACMCvtTo;
               fmt->set_param = aaxMSACMSetParam;
               rv = AAX_TRUE;
            }
         }
      }
   }
#endif
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
                  handle->format = getFormatFromWAVFormat(PCM_WAVE_FILE,
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
            _AAX_FILEDRVLOG("ACM: playback is not supported");
            close(handle->fd); /* no mp3 write support (yet) */
         }
      }
      else {
         _AAX_FILEDRVLOG("ACM: file not found");
      }
   }
   else
   {
      if (!fname) {
         _AAX_FILEDRVLOG("ACM: No filename provided");
      } else {
         _AAX_FILEDRVLOG("ACM: Internal error: handle id equals 0");
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

