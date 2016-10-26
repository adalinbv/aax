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

#include <string.h>
#include <assert.h>
#include <xml.h>

#include <base/dlsym.h>

#include <api.h>
#include <arch.h>

#include "extension.h"
#include "format.h"
#include "fmt_vorbis.h"


typedef struct
{
   void *id;
   void *audio;

   int mode;

   char capturing;

   uint8_t no_tracks;
   uint8_t bits_sample;
   int frequency;
   int bitrate;
   int blocksize;
   enum aaxFormat format;
   size_t no_samples;
   size_t max_samples;

   ssize_t vorbisBufPos;
   size_t vorbisBufSize;
   unsigned char *vorbisBuffer;
   void *vorbisptr;

   float **outputs;
   unsigned int out_pos;
   unsigned int out_size;

} _driver_t;


#define FRAME_SIZE		4096


int
_vorbis_detect(_fmt_t *fmt, int mode)
{
   void *audio = NULL;
   int rv = AAX_FALSE;

   /* not required but useful */
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
      _AAX_FILEDRVLOG("VORBIS: Insufficient memory");
   }

   return rv;
}

void*
_vorbis_open(_fmt_t *fmt, void *buf, size_t *bufsize, size_t fsize)
{
   _driver_t *handle = fmt->id;
   void *rv = NULL;

   assert(bufsize);

   if (handle)
   {
      if (!handle->vorbisptr)
      {
         char *ptr = 0;

         handle->vorbisBufPos = 0;
         handle->vorbisBufSize = 16384;
         handle->vorbisptr = _aax_malloc(&ptr, handle->vorbisBufSize);
         handle->vorbisBuffer = ptr;
      }

      if (handle->vorbisptr)
      {
         if (handle->capturing)
         {
            int err = VORBIS__no_error;
            int used = 0;

            memcpy((char*)handle->vorbisBuffer+handle->vorbisBufPos, buf, *bufsize);
            handle->vorbisBufPos += *bufsize;
            buf = (char*)handle->vorbisBuffer;

            if (!handle->id)
            {
               int max = handle->vorbisBufPos;
               handle->id = stb_vorbis_open_pushdata(buf, max, &used, &err, 0);
            }

            if (handle->id)
            {
               stb_vorbis_info info = stb_vorbis_get_info(handle->id);

               handle->no_tracks = info.channels;
               handle->frequency = info.sample_rate;
               handle->blocksize = info.max_frame_size;

               handle->format = AAX_FLOAT;
               handle->bits_sample = aaxGetBitsPerSample(handle->format);
#if 0
  printf("%d channels, %d samples/sec\n", info.channels, info.sample_rate);
  printf("Predicted memory needed: %d (%d + %d)\n", info.setup_memory_required + info.temp_memory_required,
                info.setup_memory_required, info.temp_memory_required);
#endif
               handle->vorbisBufPos -= used;
               if (handle->vorbisBufPos > 0) {
                  memmove(buf, buf+used, handle->vorbisBufPos);
               }
               // we're done decoding, return NULL
            }
            else
            {
               switch (err)
               {
               case VORBIS_missing_capture_pattern:
                  _AAX_FILEDRVLOG("VORBIS: missing capture pattern");
                  break;
               case VORBIS_invalid_stream_structure_version:
                  _AAX_FILEDRVLOG("VORBIS: invalid stream structure version");
                  break;
               case VORBIS_continued_packet_flag_invalid:
                  _AAX_FILEDRVLOG("VORBIS: continued packet flag invalid");
                  break;
               case VORBIS_incorrect_stream_serial_number:
                  _AAX_FILEDRVLOG("VORBIS: incorrect stream serial number");
                  break;
               case VORBIS_invalid_first_page:
                  _AAX_FILEDRVLOG("VORBIS: invalid first page");
                  break;
               case VORBIS_bad_packet_type:
                  _AAX_FILEDRVLOG("VORBIS: bad packet type");
                  break;
               case VORBIS_cant_find_last_page:
                  _AAX_FILEDRVLOG("VORBIS: cant find last page");
                  break;
               default:
                  rv = buf;
                  break;
               }
            }
         } /* handle->capturing */
      }
      else {
         _AAX_FILEDRVLOG("VORBIS: Unable to allocate the audio buffer");
      }
   }
   else
   {
      _AAX_FILEDRVLOG("VORBIS: Internal error: handle id equals 0");
   }

   return rv;
}

void
_vorbis_close(_fmt_t *fmt)
{
   _driver_t *handle = fmt->id;

   if (handle)
   {
      stb_vorbis_close(handle->id);
      handle->id = NULL;

      free(handle->vorbisptr);
      free(handle);
   }
}

int
_vorbis_setup(_fmt_t *fmt, _fmt_type_t pcm_fmt, enum aaxFormat aax_fmt)
{
   return AAX_TRUE;
}

size_t
_vorbis_fill(_fmt_t *fmt, void_ptr sptr, size_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t bufpos, bufsize, size;
   size_t rv = 0;

   size = *bytes;
   bufpos = handle->vorbisBufPos;
   bufsize = handle->vorbisBufSize;
   if ((size+bufpos) <= bufsize)
   {
      unsigned char *buf = handle->vorbisBuffer + bufpos;

      memcpy(buf, sptr, size);
      handle->vorbisBufPos += size;
printf(">>  _vorbis_fill: size: %i\n", handle->vorbisBufPos);
      rv = size;
   }

   return rv;
}

size_t
_vorbis_copy(_fmt_t *fmt, int32_ptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bytes, bufsize, size = 0;
   unsigned int bits, tracks;
   int ret, decode_fec = 0;
   size_t rv = __F_EOF;
   unsigned char *buf;

   tracks = handle->no_tracks;
   bits = handle->bits_sample;
   bytes = *num*tracks*bits/8;

   buf = handle->vorbisBuffer;
   bufsize = handle->vorbisBufSize;

   if (bytes > bufsize) {
      bytes = bufsize;
   }

#if 0
   ret = vorbis_decode(handle->id, buf, bytes, (int16_t*)dptr, *num, decode_fec);
   if (ret >= 0)
   {
      unsigned int framesize = tracks*bits/8;

      *num = size/framesize;
      handle->no_samples += *num;

      rv = size;
   }
#endif
   return rv;
}

size_t
_vorbis_cvt_from_intl(_fmt_t *fmt, int32_ptrptr dptr, size_t offset, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bytes, bufsize, req;
   unsigned int bits, tracks;
   size_t rv = __F_EOF;
   unsigned char *buf;
   int i, ret, n;

   req = *num;
   tracks = handle->no_tracks;
   bits = handle->bits_sample;
   bytes = req*tracks*bits/8;
   *num = 0;
printf("\n>> req: %i, tracks: %i, bits: %i, bytes: %i\n", req, tracks, bits, bytes);

   buf = handle->vorbisBuffer;
   bufsize = handle->vorbisBufPos;

   if (bytes > bufsize) {
      bytes = bufsize;
   }

// TODO: vorbis decodes blocks of max. handle->blocksize at a time
//       it may happen that more data is decoded then requested, so
//       keep them available for a next run and copy that before a
//       new decode session starts.

   /* there is still data left in the buffer from the previous run */
   if (handle->out_pos > 0)
   {
      unsigned int pos = handle->out_pos;
      unsigned int max = _MIN(req, handle->out_size - pos);

printf(">>  _vorbis copy from outputs: pos: %i, max: %i, req: %i (%i)\n", pos, max, req, handle->out_size);

      for (i=0; i<tracks; i++) {
         memcpy(dptr[i]+offset, handle->outputs[i]+pos, max*bits/8);
      }
      offset += max;
      handle->out_pos += max;
      if (handle->out_pos == handle->out_size) {
         handle->out_pos = 0;
      }
      req -= max;
      bytes = req*tracks*bits/8;
      *num = max;
      rv = 0;
   }

   if (req > 0)
   {
      ret = 0;
      do
      {
         if (ret > 0)
         {
            bytes -= ret;
            handle->vorbisBufPos -= ret;
            if (handle->vorbisBufPos > 0) {
               memmove(buf, buf+ret, handle->vorbisBufPos);
            }
         }
         ret = stb_vorbis_decode_frame_pushdata(handle->id, buf, bytes, &tracks,
                                                         &handle->outputs, &n);
         if (tracks > _AAX_MAX_SPEAKERS) {
            tracks = _AAX_MAX_SPEAKERS;
         }
printf(">>  _vorbis_cvt_from_intl: ret: %i, n: %i\n", ret, n);
      }
      while (ret && n == 0);

      if (ret > 0)
      {
         /* skip processed data */
         handle->vorbisBufPos -= ret;
         if (handle->vorbisBufPos > 0) {
            memmove(buf, buf+ret, handle->vorbisBufPos);
         }

printf(">>    n: %i, req: %i\n", n, req);
         if (n > req)
         {
            handle->out_size = n;
            n -= req;
            handle->out_pos = n;
         }
         else {
            assert(handle->out_pos == 0);
         }

         *num += n;
         handle->no_samples += n;
         rv = ret;

         for (i=0; i<tracks; i++) {
            memcpy(dptr[i]+offset, handle->outputs[i], n*bits/8);
         }
      }
   }

printf(">>      rv: %i, *num: %i\n\n", rv, *num);
   return rv;
}

size_t
_vorbis_cvt_to_intl(_fmt_t *fmt, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = fmt->id;
   int res;

   assert(scratchlen >= *num*handle->no_tracks*sizeof(int32_t));

   handle->no_samples += *num;
   _batch_cvt16_intl_24(scratch, sptr, offs, handle->no_tracks, *num);

#if 0
   res = vorbis_encode(handle->id, scratch, *num,
                                  handle->vorbisBuffer, handle->vorbisBufSize);
   _aax_memcpy(dptr, handle->vorbisBuffer, res);
#endif

   return res;
}

char*
_vorbis_name(_fmt_t *fmt, enum _aaxStreamParam param)
{
   return NULL;
}

off_t
_vorbis_get(_fmt_t *fmt, int type)
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
_vorbis_set(_fmt_t *fmt, int type, off_t value)
{
   _driver_t *handle = fmt->id;
   off_t rv = 0;

   switch(type)
   {
   case __F_FREQ:
      handle->frequency = value;
      break;
   case __F_RATE:
      handle->bitrate = value;
      break;
   case __F_TRACKS:
      handle->no_tracks = value;
      break;
   case __F_SAMPLES:
      handle->no_samples = value;
      handle->max_samples = value;
      break;
   case __F_BITS:
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

