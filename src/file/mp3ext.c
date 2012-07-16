/*
 * Copyright 2005-2012 by Erik Hofman.
 * Copyright 2009-2012 by Adalin B.V.
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

#include <stdlib.h>		/* for malloc */
#include <string.h>		/* for strdup */
#include <fcntl.h>		/* SEEK_*, O_* */
#include <assert.h>		/* assert */
#include <errno.h>
#if HAVE_STRINGS_H
# include <strings.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif

#include <aax/aax.h>

#include <ringbuffer.h>
#include <base/dlsym.h>
#include <arch.h>

#include "filetype.h"
#include "audio.h"

DECL_FUNCTION(mpg123_init);
DECL_FUNCTION(mpg123_exit);
DECL_FUNCTION(mpg123_new);
DECL_FUNCTION(mpg123_open_fd);
DECL_FUNCTION(mpg123_read);
DECL_FUNCTION(mpg123_delete);
DECL_FUNCTION(mpg123_format);
DECL_FUNCTION(mpg123_getformat);

static _detect_fn _aaxMP3FileDetect;

static _default_fname_fn _aaxMP3FileInterfaces;
static _extension_fn _aaxMP3FileExtension;

static _get_param_fn _aaxMP3FileGetFormat;
static _get_param_fn _aaxMP3FileGetNoTracks;
static _get_param_fn _aaxMP3FileGetFrequency;
static _get_param_fn _aaxMP3FileGetBitsPerSample;

	/** libmpg123 */
static _new_hanle_fn _aaxMPG123FileSetup;
static _open_fn _aaxMPG123FileOpen;
static _close_fn _aaxMPG123FileClose;
static _update_fn _aaxMPG123FileReadWrite;


_aaxFileHandle*
_aaxDetectMP3File()
{
   _aaxFileHandle* rv = NULL;

   rv = calloc(1, sizeof(_aaxFileHandle));
   if (rv)
   {
      rv->detect = (_detect_fn*)&_aaxMP3FileDetect;
      rv->setup = (_new_hanle_fn*)&_aaxMPG123FileSetup;
      rv->open = (_open_fn*)&_aaxMPG123FileOpen;
      rv->close = (_close_fn*)&_aaxMPG123FileClose;
      rv->update = (_update_fn*)&_aaxMPG123FileReadWrite;

      rv->supported = (_extension_fn*)&_aaxMP3FileExtension;
      rv->interfaces = (_default_fname_fn*)&_aaxMP3FileInterfaces;

      rv->get_format = (_get_param_fn*)&_aaxMP3FileGetFormat;
      rv->get_no_tracks = (_get_param_fn*)&_aaxMP3FileGetNoTracks;
      rv->get_frequency = (_get_param_fn*)&_aaxMP3FileGetFrequency;
      rv->get_bits_per_sample = (_get_param_fn*)&_aaxMP3FileGetBitsPerSample;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

typedef struct
{
   char *name;
   void *id;

   int fd;
   int mode;
   int capturing;

   int frequency;
   enum aaxFormat format;
// uint16_t blocksize;
   uint8_t no_tracks;
   uint8_t bits_sample;

   unsigned char *blockbuf;
   unsigned int offset_samples;

} _handle_t;

static int
_aaxMP3FileDetect(int mode)
{
   static void *audio = NULL;
   int rv = AAX_FALSE;

   if (!audio) {
      audio = _oalIsLibraryPresent("mpg123", "0");
   }
   if (!audio) {
      audio = _oalIsLibraryPresent("libmpg123-0", "0");
   }

   if (audio)
   {
      char *error;
      _oalGetSymError(0);

      TIE_FUNCTION(mpg123_init);
      if (pmpg123_init)
      {
         TIE_FUNCTION(mpg123_exit);
         TIE_FUNCTION(mpg123_new);
         TIE_FUNCTION(mpg123_open_fd);
         TIE_FUNCTION(mpg123_read);
         TIE_FUNCTION(mpg123_delete);
         TIE_FUNCTION(mpg123_format);
         TIE_FUNCTION(mpg123_getformat);

         error = _oalGetSymError(0);
         if (!error) {
            rv = AAX_TRUE;
         }
      }
   }
   return rv;
}

static int
_aaxMP3FileExtension(char *ext) {
   return !strcasecmp(ext, "mp3");
}

static char*
_aaxMP3FileInterfaces(int mode)
{
   static const char *rd[2] = { "*.mp3\0", "\0" };
   return (char *)rd[mode];
}

static unsigned int
_aaxMP3FileGetFormat(void *id)
{
   _handle_t *handle = (_handle_t *)id;
   return handle->format;
}

static unsigned int
_aaxMP3FileGetNoTracks(void *id)
{
   _handle_t *handle = (_handle_t *)id;
   return handle->no_tracks;
}

static unsigned int
_aaxMP3FileGetFrequency(void *id)
{
   _handle_t *handle = (_handle_t *)id;
   return handle->frequency;
}

static unsigned int
_aaxMP3FileGetBitsPerSample(void *id)
{
   _handle_t *handle = (_handle_t *)id;
   return handle->bits_sample;
}


	/** libmpg123 */

#define BLOCKSIZE		32768

int
getFormatFromFileFormat(int enc)
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

static int
_aaxMPG123FileOpen(void *id, const char* fname)
{
   _handle_t *handle = (_handle_t *)id;
   int res = -1;

   if (handle && fname)
   {
      handle->fd = open(fname, handle->mode, 0644);
      if (handle->fd >= 0)
      {
         handle->name = strdup(fname);
         if (handle->capturing)
         {
            pmpg123_init();
            handle->id = pmpg123_new(NULL, NULL);
            if (handle->id)
            {
               int enc, channels;
               long rate;

               pmpg123_open_fd(handle->id, handle->fd); 

               pmpg123_format(handle->id, handle->frequency,
                              MPG123_MONO | MPG123_STEREO,
                              MPG123_ENC_SIGNED_16);
               pmpg123_getformat(handle->id, &rate, &channels, &enc);

               handle->frequency = rate;
               handle->no_tracks = channels;
               handle->format = getFormatFromFileFormat(enc);
               handle->bits_sample = aaxGetBitsPerSample(handle->format);

               res = AAX_TRUE;
            }
         }
         else {
            close(handle->fd); /* no mp3 write support (yet) */
         }
      }
   }

   return (res >= 0) ? AAX_TRUE : AAX_FALSE;
}

static int
_aaxMPG123FileClose(void *id)
{
   _handle_t *handle = (_handle_t *)id;
   int ret = AAX_TRUE;

   if (handle)
   {
      if (handle->capturing)
      {
         pmpg123_delete(handle->id);
         handle->id = NULL;
         pmpg123_exit();

         free(handle->blockbuf);
      }
      close(handle->fd);
      free(handle->name);
      free(handle);
   }

   return ret;
}

static void*
_aaxMPG123FileSetup(int mode, int freq, int tracks, int format)
{
   _handle_t *handle = NULL;
   handle = calloc(1, sizeof(_handle_t));
   if (handle && mode == 0)
   {
      static const int _mode[] = {
         0,
         O_RDONLY|O_BINARY
      };
      handle->capturing = 1;
      handle->mode = _mode[handle->capturing];
      handle->frequency = freq;
      handle->no_tracks = tracks;
      handle->format = format;
      handle->bits_sample = aaxGetBitsPerSample(handle->format);
   }

   return (void*)handle;
}

static int
_aaxMPG123FileReadWrite(void *id, void *data, unsigned int no_frames)
{
   _handle_t *handle = (_handle_t *)id;
   int rv = 0;

   if (handle->capturing)
   {
      int framesz_bits = handle->no_tracks*handle->bits_sample;
#if 1
      size_t blocksize = (no_frames*framesz_bits)/8;
      size_t size = 0;
      int ret;

      ret = pmpg123_read(handle->id, data, blocksize, &size);
      if (ret == MPG123_OK) {
         rv = size;
      }
      else
      {
//       _AAX_SYSLOG("mp3; unable to read data");
         rv = -1;
      }
#else
      unsigned char *ptr = (unsigned char*)data;

      if (!handle->blockbuf) {
         handle->blockbuf = malloc(BLOCKSIZE);
      }

      if (handle->offset_samples)	/* there is some old data available */
      {
         unsigned int max_samples = BLOCKSIZE*8/framesz_bits;
         unsigned int offset = handle->offset_samples;
         unsigned int samples, size;

         samples = _MIN(max_samples - offset, no_frames);
         no_frames -= samples;

         handle->offset_samples += samples;
         if (handle->offset_samples >= max_samples) {
            handle->offset_samples = 0;
         }

         size = (samples*framesz_bits)/8;
         offset = (offset*framesz_bits)/8;
         _aax_memcpy(ptr, handle->blockbuf+offset, size);
         ptr += size;
         rv = size;
      }

      while (no_frames)			/* need to decode new block(s) */
      {
         size_t size = 0;
         int ret;

         ret = pmpg123_read(handle->id, handle->blockbuf, BLOCKSIZE, &size);
         if (ret == MPG123_OK)
         {
            size_t frames = _MIN(size*8/framesz_bits, no_frames);
            unsigned int offset = handle->offset_samples;

            handle->offset_samples += frames;
            no_frames -= frames;

            size = (frames*framesz_bits)/8;
            offset = (offset*framesz_bits)/8;
            _aax_memcpy(ptr, handle->blockbuf+offset, size);
            ptr += size;
            rv += size;
         }
         else
         {
//          _AAX_SYSLOG("mp3; unable to read data");
            rv = 0;
         }
      }
#endif
   }

   return rv;
}

