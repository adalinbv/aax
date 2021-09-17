/*
 * Copyright 2005-2021 by Erik Hofman.
 * Copyright 2009-2021 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
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
#include <software/cpu/arch2d_simd.h>

#include "audio.h"
#include "fmt_vorbis.h"

typedef struct
{
   vorbis_comment vc;
   vorbis_dsp_state vd;
   vorbis_block vb;
} _driver_write_t;


typedef struct
{
   void *id;

   char capturing;
   int mode;

   vorbis_info info;
   uint8_t bits_sample;
   int blocksize;
   enum aaxFormat format;
   size_t no_samples;
   size_t max_samples;
   size_t file_size;

   _data_t *vorbisBuffer;

   float **outputs;
   unsigned int out_pos;
   unsigned int out_size;

   _driver_write_t *out;

} _driver_t;


#define FRAME_SIZE		4096
#define VORBIS_BUFFER_SIZE	16384

DECL_FUNCTION(vorbis_info_init);
DECL_FUNCTION(vorbis_encode_setup_init);
DECL_FUNCTION(vorbis_encode_setup_managed);
DECL_FUNCTION(vorbis_encode_ctl);
DECL_FUNCTION(vorbis_info_clear);
DECL_FUNCTION(vorbis_comment_init);
DECL_FUNCTION(vorbis_comment_add_tag);
DECL_FUNCTION(vorbis_comment_clear);
DECL_FUNCTION(vorbis_analysis_init);
DECL_FUNCTION(vorbis_analysis_headerout);
DECL_FUNCTION(vorbis_analysis_buffer);
DECL_FUNCTION(vorbis_analysis_blockout);
DECL_FUNCTION(vorbis_analysis);
DECL_FUNCTION(vorbis_analysis_wrote);
DECL_FUNCTION(vorbis_bitrate_addblock);
DECL_FUNCTION(vorbis_bitrate_flushpacket);
DECL_FUNCTION(vorbis_block_init);
DECL_FUNCTION(vorbis_block_clear);
DECL_FUNCTION(vorbis_dsp_clear);

static void *audio = NULL;

int
_vorbis_detect(UNUSED(_fmt_t *fmt), int mode)
{
   int rv = AAX_FALSE;

   if (mode)
   {
      audio = _aaxIsLibraryPresent("vorbisenc", "2");
      if (audio)
      {
         char *error;

         _aaxGetSymError(0);

         TIE_FUNCTION(vorbis_info_init);
         if (pvorbis_info_init)
         {
            TIE_FUNCTION(vorbis_encode_setup_init);
            TIE_FUNCTION(vorbis_encode_setup_managed);
            TIE_FUNCTION(vorbis_encode_ctl);
            TIE_FUNCTION(vorbis_info_clear);
            TIE_FUNCTION(vorbis_comment_init);
            TIE_FUNCTION(vorbis_comment_add_tag);
            TIE_FUNCTION(vorbis_comment_clear);
            TIE_FUNCTION(vorbis_analysis_init);
            TIE_FUNCTION(vorbis_analysis_headerout);
            TIE_FUNCTION(vorbis_analysis_buffer);
            TIE_FUNCTION(vorbis_analysis_blockout);
            TIE_FUNCTION(vorbis_analysis);
            TIE_FUNCTION(vorbis_analysis_wrote);
            TIE_FUNCTION(vorbis_bitrate_addblock);
            TIE_FUNCTION(vorbis_bitrate_flushpacket);
            TIE_FUNCTION(vorbis_block_init);
            TIE_FUNCTION(vorbis_block_clear);
            TIE_FUNCTION(vorbis_dsp_clear);

            error = _aaxGetSymError(0);
            if (!error) rv = AAX_TRUE;
         }
      }
   }
   else {
      rv = AAX_TRUE;
   }

   return rv;
}

void*
_vorbis_open(_fmt_t *fmt, int mode, void *buf, ssize_t *bufsize, size_t fsize)
{
   _driver_t *handle = fmt->id;
   void *rv = NULL;

   if (!handle)
   {
      handle = fmt->id = calloc(1, sizeof(_driver_t));
      if (fmt->id)
      {
         if (mode)
         {
            handle->out = calloc(1, sizeof(_driver_write_t));
            pvorbis_info_init(&handle->info);
         }

         handle->mode = mode;
         handle->info.rate = 44100;
         handle->info.bitrate_lower = -1;
         handle->info.bitrate_upper = -1;
         handle->info.bitrate_nominal = 128000;
         handle->file_size = fsize;
         handle->capturing = (mode == 0) ? 1 : 0;
         handle->blocksize = FRAME_SIZE;
      }
      else {
         _AAX_FILEDRVLOG("VORBIS: Insufficient memory");
      }
   }

   if (handle && buf && bufsize)
   {
      if (!handle->vorbisBuffer) {
         handle->vorbisBuffer = _aaxDataCreate(VORBIS_BUFFER_SIZE, 1);
      }

      if (handle->vorbisBuffer)
      {
         if (handle->capturing)
         {
            int err = VORBIS__no_error;
            int used = 0;

            if (_vorbis_fill(fmt, buf, bufsize) > 0)
            {
               buf = _aaxDataGetData(handle->vorbisBuffer);

               if (!handle->id)
               {
                  int max = _aaxDataGetDataAvail(handle->vorbisBuffer);
                  handle->id=stb_vorbis_open_pushdata(buf, max, &used, &err, 0);
               }

               if (handle->id)
               {
                  stb_vorbis_info info = stb_vorbis_get_info(handle->id);

                  handle->info.channels = info.channels;
                  handle->info.rate = info.sample_rate;
                  handle->blocksize = info.max_frame_size;

                  handle->max_samples = 20*info.sample_rate;
                  handle->format = AAX_PCM24S;
                  handle->bits_sample = aaxGetBitsPerSample(handle->format);
#if 0
  printf("%d channels, %d samples/sec\n", info.channels, info.sample_rate);
  printf("Predicted memory needed: %d (%d + %d)\n", info.setup_memory_required + info.temp_memory_required,
                info.setup_memory_required, info.temp_memory_required);
#endif
                  _aaxDataMove(handle->vorbisBuffer, NULL, used);
                  // we're done decoding, return NULL
               }
               else if (err == VORBIS_need_more_data) {
                  rv = buf;
               }
               else
               {
                  *bufsize = 0;
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
                  case VORBIS_invalid_setup:
                  case VORBIS_invalid_stream:
                     _AAX_FILEDRVLOG("VORBIS: corrupt/invalid stream");
                     break;
                  case VORBIS_outofmem:
                     _AAX_FILEDRVLOG("VORBIS: insufficient memory");
                     break;
                  case VORBIS_invalid_api_mixing:
                  case VORBIS_feature_not_supported:
                  case VORBIS_too_many_channels:
                  case VORBIS_file_open_failure:
                  case VORBIS_seek_without_length:
                  case VORBIS_unexpected_eof:
                  case VORBIS_seek_failed:
                  default:
                     _AAX_FILEDRVLOG("VORBIS: unknown initialization error");
                     break;
                  }
               }
            } /* _buf_fill() != 0 */
         }
         else	// playback
         {
            int ret;

            // quality mode with approximate bitrate
            ret = pvorbis_encode_setup_managed(&handle->info,
                                               handle->info.channels,
                                               handle->info.rate,
                                               handle->info.bitrate_lower,
                                               handle->info.bitrate_nominal,
                                               handle->info.bitrate_upper);
#if 0
 printf("channels: %i\n", handle->info.channels);
 printf("rate: %li\n", handle->info.rate);
 printf("bitrate_lower: %li\n", handle->info.bitrate_lower);
 printf("bitrate_nominal: %li\n", handle->info.bitrate_nominal);
 printf("bitrate_upper: %li\n", handle->info.bitrate_upper);
 printf("vorbis_encode_setup_managed: %i\n", ret);
#endif
            if (!ret) {
               ret = pvorbis_encode_ctl(&handle->info,
                                        OV_ECTL_RATEMANAGE2_SET, NULL);
            }
            if (!ret) {
               ret = pvorbis_encode_setup_init(&handle->info);
            }

            if (!ret)
            {
               pvorbis_comment_init(&handle->out->vc);
               pvorbis_comment_add_tag(&handle->out->vc,
                                      "ENCODER", aaxGetVersionString(NULL));

               pvorbis_analysis_init(&handle->out->vd, &handle->info);
               pvorbis_block_init(&handle->out->vd, &handle->out->vb);

               if (buf && *bufsize == 3*sizeof(ogg_packet))
               {
                  ogg_packet *header = (ogg_packet*)buf;
                  pvorbis_analysis_headerout(&handle->out->vd, &handle->out->vc,
                                            &header[0], &header[1], &header[2]);
               }
               else {
                  *bufsize = 0;
               }
            }
            else
            {
               _AAX_FILEDRVLOG("VORBIS: Unable to create a handle for writing")
               free(handle->out);
               handle->out = NULL;
            }
         }
      }
      else
      {
         _AAX_FILEDRVLOG("VORBIS: Unable to allocate the audio buffer");
         rv = buf;	// try again
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

      if (handle->out)
      {
         pvorbis_analysis_wrote(&handle->out->vd, 0);

         pvorbis_block_clear(&handle->out->vb);
         pvorbis_dsp_clear(&handle->out->vd);
         pvorbis_comment_clear(&handle->out->vc);
         pvorbis_info_clear(&handle->info);
         free(handle->out);
      }

      _aaxDataDestroy(handle->vorbisBuffer);
      free(handle);
   }
}

int
_vorbis_setup(UNUSED(_fmt_t *fmt), UNUSED(_fmt_type_t pcm_fmt), UNUSED(enum aaxFormat aax_fmt))
{
   return AAX_TRUE;
}

size_t
_vorbis_fill(_fmt_t *fmt, void_ptr sptr, ssize_t *bytes)
{
   _driver_t *handle = fmt->id;
   size_t rv = __F_PROCESS;

   if (_aaxDataAdd(handle->vorbisBuffer, sptr, *bytes) == 0) {
      *bytes = 0;
   }

   return rv;
}

size_t
_vorbis_copy(_fmt_t *fmt, int32_ptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   int bits, tracks, framesize;
   size_t bufsize, rv = 0;
   unsigned char *buf;
   unsigned int req, ret;
   int n;

   req = *num;
   tracks = handle->info.channels;
   bits = handle->bits_sample;
   framesize = tracks*bits/8;
   *num = 0;

   buf = _aaxDataGetData(handle->vorbisBuffer);
   bufsize = _aaxDataGetDataAvail(handle->vorbisBuffer);

   /* there is still data left in the buffer from the previous run */
   if (handle->out_pos > 0)
   {
      const_int32_ptrptr outputs = (const_int32_ptrptr)handle->outputs;
      unsigned char *ptr = (unsigned char*)dptr;
      unsigned int pos = handle->out_pos;
      unsigned int max = _MIN(req, handle->out_size - pos);

      ptr += dptr_offs*framesize;
      _batch_cvt24_intl_ps(ptr, outputs, pos, tracks, max);

      dptr_offs += max;
      handle->out_pos += max;
      handle->no_samples += max;
      if (handle->out_pos == handle->out_size) {
         handle->out_pos = 0;
      }
      req -= max;
      *num = max;
   }

   while (req > 0)
   {
      do
      {
         ret = stb_vorbis_decode_frame_pushdata(handle->id, buf, bufsize, NULL,
                                                &handle->outputs, &n);
         if (ret > 0)
         {
            rv += _aaxDataMove(handle->vorbisBuffer, NULL, ret);
            bufsize = _aaxDataGetDataAvail(handle->vorbisBuffer);
         }
      }
      while (ret && n == 0);

      if (ret > 0)
      {
         const_int32_ptrptr outputs = (const_int32_ptrptr)handle->outputs;
         unsigned char *ptr = (unsigned char*)dptr;

         if (n > (int)req)
         {
            handle->out_size = n;
            handle->out_pos = req;
            n = req;
            req = 0;
         }
         else
         {
            assert(handle->out_pos == 0);
            req -= n;
         }

         *num += n;
         handle->no_samples += n;

         ptr += dptr_offs*framesize;
         _batch_cvt24_intl_ps(ptr, outputs, 0, tracks, n);
         dptr_offs += n;
      }
      else {
         break;
      }
   }
   handle->max_samples = handle->no_samples;

   return rv;
}

size_t
_vorbis_cvt_from_intl(_fmt_t *fmt, int32_ptrptr dptr, size_t dptr_offs, size_t *num)
{
   _driver_t *handle = fmt->id;
   size_t bufsize, rv = 0;
   unsigned char *buf;
   unsigned int req, ret;
   int n, i, tracks;

   req = *num;
   tracks = handle->info.channels;
   *num = 0;

   buf = _aaxDataGetData(handle->vorbisBuffer);
   bufsize = _aaxDataGetDataAvail(handle->vorbisBuffer);

   /* there is still data left in the buffer from the previous run */
   if (handle->out_pos > 0)
   {
      unsigned int pos = handle->out_pos;
      unsigned int max = _MIN(req, handle->out_size - pos);

      for (i=0; i<tracks; i++) {
         _batch_cvt24_ps(dptr[i]+dptr_offs, handle->outputs[i]+pos, max);
      }
      dptr_offs += max;
      handle->out_pos += max;
      handle->no_samples += max;
      if (handle->out_pos == handle->out_size) {
         handle->out_pos = 0;
      }
      req -= max;
      *num = max;
   }

   while (req > 0)
   {
      ret = 0;
      do
      {
         ret = stb_vorbis_decode_frame_pushdata(handle->id, buf, bufsize, NULL,
                                                &handle->outputs, &n);
         if (ret > 0)
         {
            rv += _aaxDataMove(handle->vorbisBuffer, NULL, ret);

            bufsize = _aaxDataGetDataAvail(handle->vorbisBuffer);
         }
      }
      while (ret && n == 0);

      if (ret > 0)
      {
         if (n > (int)req)
         {
            handle->out_size = n;
            handle->out_pos = req;
            n = req;
            req = 0;
         }
         else
         {
            assert(handle->out_pos == 0);
            req -= n;
         }

         *num += n;
         handle->no_samples += n;

         for (i=0; i<tracks; i++) {
            _batch_cvt24_ps(dptr[i]+dptr_offs, handle->outputs[i], n);
         }
         dptr_offs += n;
      }
      else {
         break;
      }
   }

   return rv;
}

size_t
_vorbis_cvt_to_intl(_fmt_t *fmt, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = fmt->id;
   int res = 0;

   if (num)
   {
      size_t samples = *num;
      float** buffer;
      int t;

      /* expose the buffer and submit data as float (0.0..1.0f) */
      buffer = pvorbis_analysis_buffer(&handle->out->vd, samples);
      for (t=0; t<handle->info.channels; ++t) {
         _batch_cvtps_24_cpu(buffer[t], sptr[t]+offs, samples);
      }

      /* tell the library how much we actually submitted */
      pvorbis_analysis_wrote(&handle->out->vd, samples);
      handle->no_samples += samples;

      if (pvorbis_analysis_blockout(&handle->out->vd, &handle->out->vb) == 1)
      {
         pvorbis_analysis(&handle->out->vb, NULL);
         pvorbis_bitrate_addblock(&handle->out->vb);

         *num = sizeof(ogg_packet);
         assert(*num < scratchlen);

         res = pvorbis_bitrate_flushpacket(&handle->out->vd, scratch);
      }
   }
   else if (scratch && scratchlen == sizeof(ogg_packet)) {
      res = pvorbis_bitrate_flushpacket(&handle->out->vd, scratch);
   }
   else if (pvorbis_analysis_blockout(&handle->out->vd, &handle->out->vb) == 1)
   {
      pvorbis_analysis(&handle->out->vb, NULL);
      pvorbis_bitrate_addblock(&handle->out->vb);
      res = 1;
   }

   return res;
}

char*
_vorbis_name(UNUSED(_fmt_t *fmt), UNUSED(enum _aaxStreamParam param))
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
      rv = handle->info.channels;
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
   case __F_FREQUENCY:
      handle->info.rate = value;
      break;
   case __F_BITRATE:
      handle->info.bitrate_nominal = value*1000;
      break;
   case __F_TRACKS:
      handle->info.channels = value;
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

