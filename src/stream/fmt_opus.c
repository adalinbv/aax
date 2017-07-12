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
#include "fmt_opus.h"
#include "audio.h"

// https://android.googlesource.com/platform/external/libopus/+/refs/heads/master-soong/doc/trivial_example.c
#define FRAME_SIZE 960
#define MAX_FRAME_SIZE (6*FRAME_SIZE)
#define MAX_PACKET_SIZE (3*1276)
#define OPUS_SAMPLE_RATE 48000

DECL_FUNCTION(opus_decoder_create);
DECL_FUNCTION(opus_decoder_destroy);
DECL_FUNCTION(opus_decoder_ctl);
DECL_FUNCTION(opus_decode_float);
DECL_FUNCTION(opus_decode);

DECL_FUNCTION(opus_multistream_decoder_create);
DECL_FUNCTION(opus_multistream_decoder_destroy);
DECL_FUNCTION(opus_multistream_decoder_ctl);
DECL_FUNCTION(opus_multistream_decode_float);
DECL_FUNCTION(opus_multistream_decode);

DECL_FUNCTION(opus_encoder_create);
DECL_FUNCTION(opus_encoder_destroy);
DECL_FUNCTION(opus_encoder_ctl);
DECL_FUNCTION(opus_encode);

DECL_FUNCTION(opus_strerror);
DECL_FUNCTION(opus_get_version_string);

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

   uint8_t no_tracks;
   uint8_t bits_sample;
   unsigned int frequency;
   unsigned int bitrate;
   unsigned int blocksize;
   enum aaxFormat format;
   size_t no_samples;
   size_t max_samples;

   _data_t *opusBuffer;
   _data_t *outputBuffer;

} _driver_t;

#if 0
int opus_packet_get_samples_per_frame(const unsigned char *data,
      int32_t Fs)
{
   int audiosize;
   if (data[0]&0x80)
   {
      audiosize = ((data[0]>>3)&0x3);
      audiosize = (Fs<<audiosize)/400;
   } else if ((data[0]&0x60) == 0x60)
   {
      audiosize = (data[0]&0x08) ? Fs/50 : Fs/100;
   } else {
      audiosize = ((data[0]>>3)&0x3);
      if (audiosize == 3)
         audiosize = Fs*60/1000;
      else
         audiosize = (Fs<<audiosize)/100;
   }
   return audiosize;
}

int opus_packet_get_nb_frames(const unsigned char packet[], int32_t len)
{
   int count;
   if (len<1)
      return OPUS_BAD_ARG;
   count = packet[0]&0x3;
   if (count==0)
      return 1;
   else if (count!=3)
      return 2;
   else if (len<2)
      return OPUS_INVALID_PACKET;
   else
      return packet[1]&0x3F;
}

int opus_packet_get_nb_samples(const unsigned char packet[], int32_t len,
      int32_t Fs)
{
   int samples;
   int count = opus_packet_get_nb_frames(packet, len);

   if (count<0)
      return count;

   samples = count*opus_packet_get_samples_per_frame(packet, Fs);
   /* Can't have more than 120 ms */
   if (samples*25 > Fs*3)
      return OPUS_INVALID_PACKET;
   else
      return samples;
}
#endif

int
_opus_detect(_fmt_t *fmt, int mode)
{
   void *audio = NULL;
   int rv = AAX_FALSE;

   audio = _aaxIsLibraryPresent("opus", "0");
   if (!audio) {
      audio = _aaxIsLibraryPresent("libopus", "0");
   }

   if (audio)
   {
      char *error;

      _aaxGetSymError(0);

      TIE_FUNCTION(opus_decoder_create);
      if (popus_decoder_create)
      {
         TIE_FUNCTION(opus_decoder_destroy);
         TIE_FUNCTION(opus_decoder_ctl);
         TIE_FUNCTION(opus_decode_float);
         TIE_FUNCTION(opus_decode);

         TIE_FUNCTION(opus_multistream_decoder_destroy);
         TIE_FUNCTION(opus_multistream_decoder_ctl);
         TIE_FUNCTION(opus_multistream_decode_float);
         TIE_FUNCTION(opus_multistream_decode);

         TIE_FUNCTION(opus_encoder_create);
         TIE_FUNCTION(opus_encoder_destroy);
         TIE_FUNCTION(opus_encoder_ctl);
         TIE_FUNCTION(opus_encode);

         error = _aaxGetSymError(0);
         if (!error)
         {
            /* not required but useful */
            TIE_FUNCTION(opus_strerror);
            TIE_FUNCTION(opus_get_version_string);

            fmt->id = calloc(1, sizeof(_driver_t));
            if (fmt->id)
            {
               _driver_t *handle = fmt->id;

               handle->audio = audio;
               handle->mode = mode;
               handle->capturing = (mode == 0) ? 1 : 0;
               handle->blocksize = FRAME_SIZE;

               rv = AAX_TRUE;
            }             
            else {
               _AAX_FILEDRVLOG("OPUS: Insufficient memory");
            }
         }
      }
   }

   return rv;
}

void*
_opus_open(_fmt_t *fmt, void *buf, size_t *bufsize, VOID(size_t fsize))
{
   _driver_t *handle = fmt->id;
   void *rv = NULL;

   assert(bufsize);

   if (handle)
   {
      if (!handle->opusBuffer)
      {
         unsigned int bufsize = MAX_FRAME_SIZE*handle->no_tracks*sizeof(float);
         handle->opusBuffer = _aaxDataCreate(bufsize, 1);
      }

      if (!handle->outputBuffer)
      {
         unsigned int bufsize = MAX_PACKET_SIZE*handle->no_tracks*sizeof(float);
         handle->outputBuffer = _aaxDataCreate(2*bufsize, 1);
      }

      if (handle->opusBuffer && handle->outputBuffer)
      {
         if (handle->capturing)
         {
            if (!handle->id)
            {
               int err, tracks = handle->no_tracks;
               int32_t freq = OPUS_SAMPLE_RATE;

               /*
                * https://tools.ietf.org/html/rfc7845.html#page-12
                *
                * An Ogg Opus player SHOULD select the playback sample rate
                * according to the following procedure:
                *
                * 1. If the hardware supports 48 kHz playback, decode at 48 kHz.
                * 
                * 2. Otherwise, if the hardware's highest available sample rate
                *    is a supported rate, decode at this sample rate.

                * 3. Otherwise, if the hardware's highest available sample rate 
                *    is less than 48 kHz, decode at the next higher Opus
                *    supported rate above the highest available hardware rate
                *    and resample.
                *
                * 4.  Otherwise, decode at 48 kHz and resample/
                */
               handle->frequency = freq;
               handle->blocksize = FRAME_SIZE;
               handle->format = AAX_PCM24S;
               handle->bits_sample = aaxGetBitsPerSample(handle->format);

               handle->id = popus_decoder_create(freq, tracks, &err);
               if (!handle->id && popus_strerror)
               {
                  char s[1025];
                  snprintf(s, 1024, "OPUS: Unable to create a handle: %s",
                                     popus_strerror(err));
                  s[1024] = 0;
                  _aaxStreamDriverLog(NULL, 0, 0, s);
               }
            }
         }
      }
   }
   else
   {
      _AAX_FILEDRVLOG("OPUS: Internal error: handle id equals 0");
   }

   return rv;
}

void
_opus_close(_fmt_t *fmt)
{
   _driver_t *handle = fmt->id;

   if (handle)
   {
      popus_encoder_destroy(handle->id);
      handle->id = NULL;

      _aaxDataDestroy(handle->opusBuffer);
      _aax_aligned_free(handle->outputBuffer->data);

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
_opus_setup(VOID(_fmt_t *fmt), VOID(_fmt_type_t pcm_fmt), VOID(enum aaxFormat aax_fmt))
{
   return AAX_TRUE;
}

size_t
_opus_fill(_fmt_t *fmt, void_ptr sptr, size_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t rv = __F_PROCESS;
   int res;

   res = _aaxDataAdd(handle->opusBuffer, sptr, *bytes);
   *bytes = res;

   return rv;
}

size_t
_opus_copy(_fmt_t *fmt, int32_ptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   unsigned int bits, tracks, framesize, packet_sz;
   size_t req, avail, rv = 0;
   unsigned char *buf;
   float *outputs;
   int n;

   req = *num;
   tracks = handle->no_tracks;
   packet_sz = handle->blocksize;
   bits = handle->bits_sample;
   framesize = tracks*bits/8;
   *num = 0;

   buf = handle->opusBuffer->data;

   outputs = (float*)handle->outputBuffer->data;
   avail = handle->outputBuffer->avail;

   /* there is still data left in the buffer from the previous run */
   if (avail > 0)
   {
      unsigned int max = _MIN(req, avail);

      _batch_cvt24_ps(dptr+dptr_offs, outputs, max*tracks);
      _aaxDataMove(handle->outputBuffer, NULL, max*framesize);

      dptr_offs += max;
      handle->no_samples += max;
      req -= max;
      *num = max;
      if (req == 0) {
         rv = 1;
      }
   }

   while (req > 0)
   {
      size_t outsmp = handle->outputBuffer->size/framesize;
      n = popus_decode_float(handle->id, buf, packet_sz, outputs, outsmp, 0);
      if (n > 0)
      {
         unsigned int max;

         rv += _aaxDataMove(handle->opusBuffer, NULL, packet_sz);

         handle->outputBuffer->avail += n;
         max = _MIN(req, handle->outputBuffer->avail);

         _batch_cvt24_ps(dptr+dptr_offs, outputs, max*tracks);
         _aaxDataMove(handle->outputBuffer, NULL, max*framesize);

         handle->no_samples += max;
         dptr_offs += max;
         *num += max;
         req -= max;
      }
      else {
         break;
      }
   }

   return rv;
}

size_t
_opus_cvt_from_intl(_fmt_t *fmt, int32_ptrptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   unsigned int bits, tracks, framesize, packet_sz;
   size_t req, rv = 0;
   float *outputs;
   int n;

   req = *num;
   tracks = handle->no_tracks;
   bits = handle->bits_sample;
   framesize = tracks*bits/8;
   packet_sz = handle->blocksize;
   *num = 0;

   outputs = (float*)handle->outputBuffer->data;


   /* there is still data left in the buffer from the previous run */
   if (handle->outputBuffer->avail > 0)
   {
      unsigned int max = _MIN(req, handle->outputBuffer->avail/framesize);

      _batch_cvt24_ps_intl(dptr, outputs, dptr_offs, tracks, max);
      _aaxDataMove(handle->outputBuffer, NULL, max*framesize);

      dptr_offs += max;
      handle->no_samples += max;
      req -= max;
      *num = max;
      if (req == 0) {
         rv = 1;
      }
   }

   if (req > 0)
   {
      do
      {
         size_t bufsize = _MIN(packet_sz, handle->opusBuffer->avail);
         if (bufsize == packet_sz)
         {
            size_t outsmp = handle->outputBuffer->size/framesize;
            unsigned char *buf = handle->opusBuffer->data;
            unsigned int max;

            n = popus_decode_float(handle->id, buf, bufsize, outputs, outsmp,0);
            if (n > 0)
            {
               handle->outputBuffer->avail += n*framesize;
               max = _MIN(req, handle->outputBuffer->avail/framesize);

               _batch_cvt24_ps_intl(dptr, outputs, dptr_offs, tracks, max);
               _aaxDataMove(handle->outputBuffer, NULL, max*framesize);

               dptr_offs += max;
               handle->no_samples += max;
               req -= max;
               *num += max;

               // remove the packet from the opus buffer
               rv += _aaxDataMove(handle->opusBuffer, NULL, packet_sz);
            }
            else {
               break;
            }
         }
         else
         {
           *num = 0;
            break;
         }
      }
      while (req > 0);
   }

   return rv;
}

size_t
_opus_cvt_to_intl(_fmt_t *fmt, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = fmt->id;
   int res;

   assert(scratchlen >= *num*handle->no_tracks*sizeof(int32_t));

   handle->no_samples += *num;
   _batch_cvt16_intl_24(scratch, sptr, offs, handle->no_tracks, *num);

   /*
    * -- about *num --
    * Number of samples per channel in the input signal. This must be an Opus
    * frame size for the encoder's sampling rate. For example, at 48 kHz the
    * permitted values are 120, 240, 480, 960, 1920, and 2880. Passing in a
    * duration of less than 10 ms (480 samples at 48 kHz) will prevent the
    * encoder from using the LPC or hybrid modes. 
    */
   res = popus_encode(handle->id, scratch, *num,
                      handle->opusBuffer->data, handle->opusBuffer->avail);
   _aax_memcpy(dptr, handle->opusBuffer, res);

   return res;
}

char*
_opus_name(_fmt_t *fmt, enum _aaxStreamParam param)
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
_opus_get(_fmt_t *fmt, int type)
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
_opus_set(_fmt_t *fmt, int type, off_t value)
{
   _driver_t *handle = fmt->id;
   off_t rv = 0;

   switch(type)
   {
   case __F_BLOCK_SIZE:
      handle->blocksize = value;
      break;
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
      break;
   case __F_POSITION:
      break;
   default:
      break;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */
static uint32_t
_opus_char_to_int(unsigned char *ch)
{
    return ((uint32_t)ch[0]<<24) | ((uint32_t)ch[1]<<16)
         | ((uint32_t)ch[2]<< 8) |  (uint32_t)ch[3];
}
