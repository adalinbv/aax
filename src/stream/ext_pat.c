/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#endif

#include "extension.h"
#include "format.h"
#include "ext_pat.h"


typedef struct
{
   _fmt_t *fmt;

   char *comments;

   int capturing;
   int mode;

   int no_tracks;
   int bits_sample;
   int frequency;
   int bitrate;
   float loop_start_sec;
   float loop_end_sec;
   enum aaxFormat format;
   size_t blocksize;
   size_t no_samples;
   size_t max_samples;

   char copy_to_buffer;

   // We only support one instrument with one layer and one patch waveform
   // per file. So we take the first of all of them.
   _patch_header_t header;
   _instrument_data_t instrument;
   _layer_data_t layer;
   _patch_data patch;

} _driver_t;

static float env_rate_to_time(unsigned char);
static float env_offset_to_level(unsigned char);
static int _aaxFormatDriverReadHeader(_driver_t *, unsigned char*);


int
_pat_detect(UNUSED(_ext_t *ext), int mode)
{
   if (!mode) return AAX_TRUE;
   return AAX_FALSE;
}

int
_pat_setup(_ext_t *ext, int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
{
   int bits_sample = aaxGetBitsPerSample(format);
   int rv = AAX_FALSE;

   if (bits_sample)
   {
      _driver_t *handle = calloc(1, sizeof(_driver_t));
      if (handle)
      {
         handle->mode = mode;
         handle->capturing = (mode > 0) ? 0 : 1;
         handle->bits_sample = bits_sample;
         handle->blocksize = tracks*bits_sample/8;
         handle->frequency = freq;
         handle->no_tracks = 1;
         handle->format = format;
         handle->bitrate = bitrate;
         handle->no_samples = no_samples;
         handle->max_samples = 0;

         if (handle->capturing)
         {
            handle->no_samples = UINT_MAX;
            *bufsize = FILE_HEADER_SIZE;
         }
         else {
            *bufsize = 0;
         }
         ext->id = handle;
         rv = AAX_TRUE;
      }
      else {
         _AAX_FILEDRVLOG("PAT: Insufficient memory");
      }
   }
   else {
      _AAX_FILEDRVLOG("PAT: Unsupported format");
   }

   return rv;
}

void*
_pat_open(_ext_t *ext, void_ptr buf, size_t *bufsize, size_t fsize)
{
   _driver_t *handle = ext->id;
   void *rv = NULL;

   if (handle)
   {
      if (!handle->capturing)	/* write */
      {
         *bufsize = 0;
      }

				/* read: handle->capturing */
      else if (!handle->fmt || !handle->fmt->open)
      {
         if (*bufsize >= FILE_HEADER_SIZE)
         {
            int res;

            res = _aaxFormatDriverReadHeader(handle, buf);
            if (res > 0)
            {
               if (!handle->fmt)
               {
                  _fmt_type_t fmt = _FMT_PCM;

                  handle->fmt = _fmt_create(fmt, handle->mode);
                  if (!handle->fmt) {
                     *bufsize = 0;
                     return rv;
                  }

                  handle->fmt->open(handle->fmt, handle->mode, NULL, NULL, 0);
                  handle->fmt->set(handle->fmt, __F_TRACKS, handle->no_tracks);
                  handle->fmt->set(handle->fmt, __F_COPY_DATA, handle->copy_to_buffer);
                  if (!handle->fmt->setup(handle->fmt, fmt, handle->format))
                  {
                     *bufsize = 0;
                     handle->fmt = _fmt_free(handle->fmt);
                     return rv;
                  }

                  handle->fmt->set(handle->fmt, __F_FREQUENCY, handle->frequency);
                  handle->fmt->set(handle->fmt, __F_RATE, handle->bitrate);
                  handle->fmt->set(handle->fmt, __F_TRACKS, handle->no_tracks);
                  handle->fmt->set(handle->fmt,__F_NO_SAMPLES, handle->no_samples);
                  handle->fmt->set(handle->fmt, __F_BITS_PER_SAMPLE, handle->bits_sample);
                  handle->fmt->set(handle->fmt, __F_BLOCK_SIZE, handle->blocksize);
               }

               if (handle->fmt)
               {
                  size_t size = 0;
                  handle->fmt->open(handle->fmt, handle->mode, buf, &size, fsize);
               }
            }
            else
            {
               if (res == __F_EOF) *bufsize = 0;
               rv = buf;
            }
         }
         else
         {
            _AAX_FILEDRVLOG("PAT: Incorrect format");
            return rv;
         }
      }
      else _AAX_FILEDRVLOG("PAT: Unknown opening error");
   }
   else {
      _AAX_FILEDRVLOG("PAT: Internal error: handle id equals 0");
   }

   return rv;
}

int
_pat_close(_ext_t *ext)
{
   _driver_t *handle = ext->id;
   int res = AAX_TRUE;

   if (handle)
   {
      if (handle->comments) free(handle->comments);

      if (handle->fmt)
      {
         handle->fmt->close(handle->fmt);
         _fmt_free(handle->fmt);
      }
      free(handle);
   }

   return res;
}

void*
_pat_update(_ext_t *ext, size_t *offs, size_t *size, char close)
{
   return NULL;
}

size_t
_pat_copy(_ext_t *ext, int32_ptr dptr, size_t offset, size_t *num)
{
   _driver_t *handle = ext->id;
   return handle->fmt->copy(handle->fmt, dptr, offset, num);
}

size_t
_pat_fill(_ext_t *ext, void_ptr sptr, size_t *bytes)
{
   _driver_t *handle = ext->id;
   return handle->fmt->fill(handle->fmt, sptr, bytes);
}

size_t
_pat_cvt_from_intl(_ext_t *ext, int32_ptrptr dptr, size_t offset, size_t *num)
{
   _driver_t *handle = ext->id;
   return handle->fmt->cvt_from_intl(handle->fmt, dptr, offset, num);
}

size_t
_pat_cvt_to_intl(_ext_t *ext, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = ext->id;
   return handle->fmt->cvt_to_intl(handle->fmt, dptr, sptr, offs, num, scratch, scratchlen);
}

char*
_pat_name(_ext_t *ext, enum _aaxStreamParam param)
{
   _driver_t *handle = ext->id;
   char *rv = handle->fmt->name(handle->fmt, param);

   if (!rv)
   {
      switch(param)
      {
      case __F_COMMENT:
         rv = handle->comments;
         break;
      default:
         break;
      }
   }
   return rv;
}

char*
_pat_interfaces(UNUSED(int ext), int mode)
{	// silently support patch files
   static const char *rd[2] = { NULL, NULL };
   return (char *)rd[mode];
}

int
_pat_extension(char *ext)
{
   return (ext && !strcasecmp(ext, "pat")) ? 1 : 0;
}

off_t
_pat_get(_ext_t *ext, int type)
{
   _driver_t *handle = ext->id;
   return handle->fmt->get(handle->fmt, type);
}

off_t
_pat_set(_ext_t *ext, int type, off_t value)
{
   _driver_t *handle = ext->id;
   off_t rv = 0;

   switch (type)
   {
   case __F_COPY_DATA:
      handle->copy_to_buffer = value;
      break;
   default:
      if (handle->fmt) {
         rv = handle->fmt->set(handle->fmt, type, value);
      }
      break;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

static float
env_rate_to_time(unsigned char rate)
{
   // rate is defined as RRMMMMMM
   // where RR is the rate and MMMMMM the mantissa

   float FUR = 1.0f/(1.6f*14.0f); // 14 voices
   float VUR = FUR/(float)(1 << 3*(rate >> 6)); // Volume Update Rate
   float mantissa = (float)(rate & 0x3f);	// Volume Increase
   float rv = 1.0f/(244.0f*VUR)/mantissa;

   return rv;
}

static float
env_offset_to_level(unsigned char offset)
{
   // offset is defined as: EEEEMMMM
   // where EEEE is the exponent and MMMM is the mantissa
   // val = mantissa * 2^exponent

   int mantissa = offset & 0xF;
   int exponent = offset >> 4;
   int rv = mantissa*(1 << exponent);
   return 2.5f*rv/491520.0;
}

static int
_aaxFormatDriverReadHeader(_driver_t *handle, unsigned char *header)
{
   int rv = 0;

   if (!memcmp(header, GF1_HEADER_TEXT, HEADER_SIZE))
   {
      // Patch Header
      memcpy(handle->header.header, header, HEADER_SIZE);
      header += HEADER_SIZE;

      memcpy(handle->header.gravis_id, header, ID_SIZE);
      header += ID_SIZE;

      memcpy(handle->header.description, header, DESC_SIZE);
      header += DESC_SIZE;

      handle->header.instruments = *header++;
      handle->header.voices = *header++;
      handle->header.channels= *header++;

      handle->header.waveforms += *header++;
      handle->header.waveforms += *header++ << 8;

      handle->header.master_volume += *header++;
      handle->header.master_volume += *header++ << 8;

      handle->header.data_size += *header++;
      handle->header.data_size += *header++ << 8;
      handle->header.data_size += *header++ << 16;
      handle->header.data_size += *header++ << 24;
      header += PATCH_RESERVED_SIZE;

      // Instrument Header
      handle->instrument.instrument += *header++;
      handle->instrument.instrument += *header++ << 8;

      memcpy(handle->instrument.name, header, INST_NAME_SIZE);
      header += INST_NAME_SIZE;

      handle->instrument.size += *header++;
      handle->instrument.size += *header++ << 8;
      handle->instrument.size += *header++ << 16;
      handle->instrument.size += *header++ << 24;

      handle->instrument.layers = *header++;
      header += INSTRUMENT_RESERVED_SIZE;

      // Layer Header
      handle->layer.layer_duplicate = *header++;
      handle->layer.layer = *header++;

      handle->layer.size += *header++;
      handle->layer.size += *header++ << 8;
      handle->layer.size += *header++ << 16;
      handle->layer.size += *header++ << 24;

      handle->layer.samples = *header++;
      header += LAYER_RESERVED_SIZE;

      // Wave Header
      memcpy(handle->patch.wave_name, header, WAV_NAME_SIZE);
      header += WAV_NAME_SIZE;

      handle->patch.fractions = *header++;

      handle->patch.wave_size += *header++;
      handle->patch.wave_size += *header++ << 8;
      handle->patch.wave_size += *header++ << 16;
      handle->patch.wave_size += *header++ << 24;

      handle->patch.start_loop += *header++;
      handle->patch.start_loop += *header++ << 8;
      handle->patch.start_loop += *header++ << 16;
      handle->patch.start_loop += *header++ << 24;

      handle->patch.end_loop += *header++;
      handle->patch.end_loop += *header++ << 8;
      handle->patch.end_loop += *header++ << 16;
      handle->patch.end_loop += *header++ << 24;

      handle->patch.sample_rate += *header++;
      handle->patch.sample_rate += *header++ << 8;

      handle->patch.low_frequency += *header++;
      handle->patch.low_frequency += *header++ << 8;
      handle->patch.low_frequency += *header++ << 16;
      handle->patch.low_frequency += *header++ << 24;

      handle->patch.high_frequency += *header++;
      handle->patch.high_frequency += *header++ << 8;
      handle->patch.high_frequency += *header++ << 16;
      handle->patch.high_frequency += *header++ << 24;

      handle->patch.root_frequency += *header++;
      handle->patch.root_frequency += *header++ << 8;
      handle->patch.root_frequency += *header++ << 16;
      handle->patch.root_frequency += *header++ << 24;

      handle->patch.tune += *header++;
      handle->patch.tune += *header++ << 8;

      handle->patch.balance = *header++;

      memcpy(handle->patch.envelope_rate, header, ENVELOPES);
      header += ENVELOPES;

      memcpy(handle->patch.envelope_offset, header, ENVELOPES);
      header += ENVELOPES;

      handle->patch.tremolo_sweep = *header++;
      handle->patch.tremolo_rate = *header++;
      handle->patch.tremolo_depth = *header++;

      handle->patch.vibrato_sweep= *header++;
      handle->patch.vibrato_rate = *header++;
      handle->patch.vibrato_depth = *header++;

      handle->patch.modes = *header++;

      handle->patch.scale_frequency += *header++;
      handle->patch.scale_frequency += *header++ << 8;

      handle->patch.scale_factor += *header++;
      handle->patch.scale_factor += *header++ << 8;

      switch (handle->patch.modes & 0x3)
      {
      case 0:
         handle->format = AAX_PCM8S;
         handle->bits_sample = 8;
         break;
      case 1:
         handle->format = AAX_PCM16S;
         handle->bits_sample = 16;
         break;
      case 2:
         handle->format = AAX_PCM8U;
         handle->bits_sample = 8;
         break;
      case 3:
         handle->format = AAX_PCM16U;
         handle->bits_sample = 16;
         break;
      default:
         break;
      }
      handle->blocksize = handle->no_tracks*handle->bits_sample/8;
      handle->no_samples = 8*handle->patch.wave_size/handle->bits_sample;
      handle->frequency = handle->patch.sample_rate;

      handle->loop_start_sec = SIZE2TIME(handle,(float)handle->patch.start_loop + (handle->patch.fractions >> 4)/15.0f);
      handle->loop_end_sec = SIZE2TIME(handle,(float)handle->patch.end_loop + (handle->patch.fractions && 0xF)/15.0f);

#if 1
 printf("Header:\t\t\t%s\n", handle->header.header);
 printf("Gravis id:\t\t%s\n", handle->header.gravis_id);
 printf("Description:\t\t%s\n", handle->header.description);
 printf("Instruments:\t\t%i\n", handle->header.instruments);
 printf("Voices:\t\t\t%i\n", handle->header.voices);
 printf("Channels:\t\t%i\n", handle->header.channels);
 printf("Waveforms:\t\t%i\n", handle->header.waveforms);
 printf("MasterVolume:\t\t%i\n\n", handle->header.master_volume);
 printf("DataSize:\t\t%i\n", handle->header.data_size);

 printf("Instrument name:\t%s\n", handle->instrument.name);
 printf("Instrument number:\t%i\n", handle->instrument.instrument);
 printf("Instrument size:\t%i\n", handle->instrument.size);
 printf("Instrument layers:\t%i\n\n", handle->instrument.layers);
  

 printf("Layer dupplicate:\t%i\n", handle->layer.layer_duplicate);
 printf("Layer number:\t\t%i\n", handle->layer.layer);
 printf("Layer size:\t\t%i\n", handle->layer.size);
 printf("Samples:\t\t%i\n\n", handle->layer.samples);

 printf("Wave name:\t\t%s\n", handle->patch.wave_name);
 printf("Loop start:\t\t%g (%gs)\n", SIZE2SAMPLES(handle,(float)handle->patch.start_loop + (handle->patch.fractions >> 4)/16.0f), handle->loop_start_sec);
 printf("Loop end:\t\t%g (%gs)\n", SIZE2SAMPLES(handle,(float)handle->patch.end_loop + (handle->patch.fractions && 0xF)/16.0f), handle->loop_end_sec);
 printf("Sample size:\t\t%i (%gs)\n", SIZE2SAMPLES(handle,handle->patch.wave_size), SIZE2TIME(handle,handle->patch.wave_size));
 printf("Sample rate:\t\t%i Hz\n", handle->patch.sample_rate);
 printf("Low Frequency:\t\t%g Hz\n", 0.001f*handle->patch.low_frequency);
 printf("High Frequency:\t\t%g Hz\n", 0.001f*handle->patch.high_frequency);
 printf("Root Frequency:\t\t%g Hz\n", 0.001f*handle->patch.root_frequency);
 printf("Panning:\t\t%.1f\n", (float)(handle->patch.balance - 7)/16.0f);

 printf("Envelope Rates:\t\t");
 for (int i=0; i<6; ++i) {
  float v = env_rate_to_time(handle->patch.envelope_rate[i]);
  if (v < 0.1f) printf ("%4.2fms\t", v*1000.0f);
  else printf("%4.2fs\t", v);
 }
 printf("\n");

 printf("Envelope Offsets:\t");
 for (int i=0; i<6; ++i) {
  float v = env_offset_to_level(handle->patch.envelope_offset[i]);
  printf("%6.4f\t", v);
 }
 printf("\n");

 printf("Tremolo Sweep:\t\t%3i (%.3g Hz)\n", handle->patch.tremolo_sweep,
                                         CVTSWEEP(handle->patch.tremolo_sweep));
 printf("Tremolo Rate:\t\t%3i (%.3g Hz)\n", handle->patch.tremolo_rate,
                                           CVTRATE(handle->patch.tremolo_rate));
 printf("Tremolo Depth:\t\t%3i (%.2g, %.3gdB)\n", handle->patch.tremolo_depth,
                                          CVTDEPTH(handle->patch.tremolo_depth),
                                       CVTDEPT2DB(handle->patch.tremolo_depth));

 printf("Vibrato Sweep:\t\t%3i (%.3g Hz)\n", handle->patch.vibrato_sweep,
                                         CVTSWEEP(handle->patch.vibrato_sweep));
 printf("Vibrato Rate:\t\t%3i (%.3g Hz)\n", handle->patch.vibrato_rate,
                                           CVTRATE(handle->patch.vibrato_rate));
 printf("Vibrato Depth:\t\t%3i (%.3g octave, %g cents)\n",
                                                  handle->patch.vibrato_depth,
                                         CVTDEPTH(handle->patch.vibrato_depth),
                                    CVTDEPT2CENTS(handle->patch.vibrato_depth));

 printf("Modes:\t\t\t0x%x\n", handle->patch.modes);
 printf(" - Sample Format:\t%i-bit %s\n",
            (handle->patch.modes & 0x1) ? 16 : 8,
            (handle->patch.modes & 0x2) ? "unsigned" : "signed");
 printf(" - Looping:\t\t%s (%s-directional %s)\n",
            (handle->patch.modes & 0x4) ? "yes" : "no",
            (handle->patch.modes & 0x8) ? "bi" : "uni",
            (handle->patch.modes & 0x10) ? "backwards" : "forward");
 printf(" - Envelope:\t\tsustain: %s, release: %s, fast-release: %s\n",
            (handle->patch.modes & 0x20) ? "yes" : "no",
            (handle->patch.modes & 0x40) ? "envelope" : "note-off",
            (handle->patch.modes & 0x80) ? "yes" : "no");
 printf("Scale Frequency:\t%i\n", handle->patch.scale_frequency);
 printf("Scale Factor:\t\t%i\n\n", handle->patch.scale_factor);
#endif

      rv = FILE_HEADER_SIZE;
   }
   else {
      return __F_EOF;
   }

   return rv;
}
