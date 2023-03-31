/*
 * Copyright 2019-2023 by Erik Hofman.
 * Copyright 2019-2023 by Adalin B.V.
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

   int capturing;
   int mode;

   int bitrate;
   int bits_sample;
   int patch_level;
   int sample_num;
   size_t max_samples;
   size_t skip;
   _buffer_info_t info;

   char copy_to_buffer;

   // We only support one instrument with one layer and one patch waveform
   // per file. So we take the first of all of them.
   _patch_header_t header;
   _instrument_data_t instrument;
   _layer_data_t layer;
   _patch_data patch;
   char trackno[8];

} _driver_t;

static float env_rate_to_time(unsigned char, float, float);
static float env_level_to_level(unsigned char);
static int _aaxFormatDriverReadHeader(_driver_t *, unsigned char*, ssize_t*);


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
         handle->bitrate = bitrate;
         handle->max_samples = 0;
         handle->info.rate = freq;
         handle->info.no_tracks = 1;
         handle->info.fmt = format;
         handle->info.no_samples = no_samples;
         handle->info.blocksize = tracks*bits_sample/8;

         snprintf(handle->trackno, 8, "%u", 0);

         if (handle->capturing)
         {
            handle->info.no_samples = UINT_MAX;
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
_pat_open(_ext_t *ext, void_ptr buf, ssize_t *bufsize, size_t fsize)
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
         if (*bufsize >= WAVE_HEADER_SIZE)
         {
            int res;

            res = _aaxFormatDriverReadHeader(handle, buf, bufsize);
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
                  handle->fmt->set(handle->fmt, __F_TRACKS, handle->info.no_tracks);
                  handle->fmt->set(handle->fmt, __F_COPY_DATA, handle->copy_to_buffer);
                  if (!handle->fmt->setup(handle->fmt, fmt, handle->info.fmt))
                  {
                     *bufsize = 0;
                     handle->fmt = _fmt_free(handle->fmt);
                     return rv;
                  }

                  handle->fmt->set(handle->fmt, __F_FREQUENCY, handle->info.rate);
                  handle->fmt->set(handle->fmt, __F_BITRATE, handle->bitrate);
                  handle->fmt->set(handle->fmt, __F_TRACKS, handle->info.no_tracks);
                  handle->fmt->set(handle->fmt, __F_NO_SAMPLES, handle->info.no_samples);
                  handle->fmt->set(handle->fmt, __F_BITS_PER_SAMPLE, handle->bits_sample);
                  handle->fmt->set(handle->fmt, __F_BLOCK_SIZE, handle->info.blocksize);
               }

               if (handle->fmt)
               {
                  ssize_t size = 0;
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
_pat_update(_ext_t *ext, size_t *offs, ssize_t *size, char close)
{
   return NULL;
}

size_t
_pat_copy(_ext_t *ext, int32_ptr dptr, size_t level, size_t *num)
{
   _driver_t *handle = ext->id;
   return handle->fmt->copy(handle->fmt, dptr, level, num);
}

size_t
_pat_fill(_ext_t *ext, void_ptr sptr, ssize_t *bytes)
{
   _driver_t *handle = ext->id;
   return handle->fmt->fill(handle->fmt, sptr, bytes);
}

size_t
_pat_cvt_from_intl(_ext_t *ext, int32_ptrptr dptr, size_t level, size_t *num)
{
   _driver_t *handle = ext->id;
   return handle->fmt->cvt_from_intl(handle->fmt, dptr, level, num);
}

size_t
_pat_cvt_to_intl(_ext_t *ext, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = ext->id;
   return handle->fmt->cvt_to_intl(handle->fmt, dptr, sptr, offs, num, scratch, scratchlen);
}

int
_pat_set_name(_ext_t *ext, enum _aaxStreamParam param, const char *desc)
{
   return AAX_FALSE;
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
      case __F_TITLE:
         rv = handle->instrument.name;
         break;
      case __F_COPYRIGHT:
         rv = handle->header.header;
      case __F_COMMENT:
         rv =  handle->patch.wave_name;
         break;
      case __F_TRACKNO:
         rv = handle->trackno;
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
   off_t rv = 0;

   if (type >= __F_ENVELOPE_LEVEL && type < __F_ENVELOPE_LEVEL_MAX)
   {
      unsigned pos = type & 0xF;
      if (pos < ENVELOPES) {
         rv = handle->info.volume_envelope[2*pos]*1e5f;
      }
   }
   else if (type >= __F_ENVELOPE_RATE && type < __F_ENVELOPE_RATE_MAX)
   {
      unsigned pos = type & 0xF;
      if (pos < ENVELOPES)
      {
         float val = handle->info.volume_envelope[2*pos+1];
         if (val == AAX_FPINFINITE) rv = OFF_T_MAX;
         else rv = val*1e5f;
      }
   }
   else
   {
      switch (type)
      {
      case __F_LOOP_COUNT:
         rv = (handle->patch.modes & MODE_LOOPING) ? OFF_T_MAX : 0;
         break;
      case __F_NO_SAMPLES:
         rv = handle->info.no_samples;
         break;
      case __F_LOOP_START:
         rv = handle->info.loop_start*16.0f;
         break;
      case __F_LOOP_END:
         rv = handle->info.loop_end*16.0f;
         break;
      case __F_ENVELOPE_SUSTAIN:
         rv = (handle->patch.modes & MODE_ENVELOPE_SUSTAIN);
         break;
      case __F_SAMPLED_RELEASE:
         rv = (handle->patch.modes & MODE_ENVELOPE_RELEASE) ? 0 : 1;
         break;
      case __F_BASE_FREQUENCY:
         rv = handle->info.base_frequency*(1 << 16);
         break;
      case __F_LOW_FREQUENCY:
         rv = handle->info.low_frequency*(1 << 16);
         break;
      case __F_HIGH_FREQUENCY:
         rv = handle->info.high_frequency*(1 << 16);
         break;
      case __F_PITCH_FRACTION:
         rv = handle->info.pitch_fraction*(1 << 24);
         break;
      case __F_TREMOLO_RATE:
         rv = handle->info.tremolo_rate*(1 << 24);
         break;
      case __F_TREMOLO_DEPTH:
         rv = handle->info.tremolo_depth*(1 << 24);
         break;
      case __F_TREMOLO_SWEEP:
         rv = handle->info.tremolo_sweep*(1 << 24);
         break;
      case __F_VIBRATO_RATE:
         rv = handle->info.vibrato_rate*(1 << 24);
         break;
      case __F_VIBRATO_DEPTH:
         rv = handle->info.vibrato_depth*(1 << 24);
         break;
      case __F_VIBRATO_SWEEP:
         rv = handle->info.vibrato_sweep*(1 << 24);
         break;
      default:
         if (handle->fmt) {
            rv = handle->fmt->get(handle->fmt, type);
         }
         break;
      }
   }
   return rv;
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
   case __F_PATCH_LEVEL:
      handle->patch_level = value;
      snprintf(handle->trackno, 4, "%li", value);
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
env_rate_to_time(unsigned char rate, float prev, float next)
{
   // rate is defined as RRMMMMMM
   // where RR is the rate and MMMMMM the mantissa

   float FUR = 1.0f/(1.6f*14.0f); // 14 voices
   float VUR = FUR/(float)(1 << 3*(rate >> 6)); // Volume Update Rate
   float mantissa = (float)(rate & 0x3f);	// Volume Increase
   float rv = fabsf(next-prev)/(244.0f*VUR)/mantissa;

   return rv;
}

static float
env_level_to_level(unsigned char level)
{
   // level is defined as: EEEEMMMM
   // where EEEE is the exponent and MMMM is the mantissa
   // val = mantissa * 2^exponent

   int mantissa = level & 0xF;
   int exponent = level >> 4;
   int rv = mantissa*(1 << exponent);
   return 2.5f*rv/491520.0;
}

static int
_aaxFormatDriverReadHeader(_driver_t *handle, unsigned char *header, ssize_t *processed)
{
   unsigned char *buffer = header;
   size_t offs;
   float cents;
   int i, pos;

#if 0
   if (handle->skip > WAVE_HEADER_SIZE)
   {
      handle->skip -= WAVE_HEADER_SIZE;
//    *processed = WAVE_HEADER_SIZE;
      return __F_NEED_MORE;
   }
#endif

   *processed = handle->skip;
   header += handle->skip;

   if (!handle->header.instruments)
   {
      if (!memcmp(header, GF1_HEADER_TEXT, GF1_HEADER_SIZE))
      {
         // Patch Header
         memcpy(handle->header.header, header, GF1_HEADER_SIZE);
         header += GF1_HEADER_SIZE;

         memcpy(handle->header.gravis_id, header, PATCH_ID_SIZE);
         header += PATCH_ID_SIZE;

         memcpy(handle->header.description, header, PATCH_DESC_SIZE);
         header += PATCH_DESC_SIZE;

         handle->header.instruments = *header++;
         handle->header.voices = *header++;
         handle->header.channels= *header++;

         handle->header.waveforms = *header++;
         handle->header.waveforms += *header++ << 8;

         handle->header.master_volume = *header++;
         handle->header.master_volume += *header++ << 8;

         handle->header.data_size = *header++;
         handle->header.data_size += *header++ << 8;
         handle->header.data_size += *header++ << 16;
         handle->header.data_size += *header++ << 24;
         header += PATCH_RESERVED_SIZE;

         // Instrument Header
         handle->instrument.instrument = *header++;
         handle->instrument.instrument += *header++ << 8;

         memcpy(handle->instrument.name, header, INSTRUMENT_NAME_SIZE);
         header += INSTRUMENT_NAME_SIZE;

         handle->instrument.size = *header++;
         handle->instrument.size += *header++ << 8;
         handle->instrument.size += *header++ << 16;
         handle->instrument.size += *header++ << 24;

         handle->instrument.layers = *header++;
         header += INSTRUMENT_RESERVED_SIZE;

         // Layer Header
         handle->layer.layer_duplicate = *header++;
         handle->layer.layer = *header++;
         snprintf(handle->trackno, 8, "%u", handle->layer.layer);

         handle->layer.size = *header++;
         handle->layer.size += *header++ << 8;
         handle->layer.size += *header++ << 16;
         handle->layer.size += *header++ << 24;

         handle->layer.samples = *header++;
         header += LAYER_RESERVED_SIZE;
#if 0
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
 printf("Samples:\t\t%i\n", handle->layer.samples);
 printf("Sample requested:\t%i\n\n", handle->patch_level);
#endif
      }
      else {
         *processed += header-buffer;
         return __F_EOF;
      }
   }

   // Wave Header
   memcpy(handle->patch.wave_name, header, WAVE_NAME_SIZE);
   header += WAVE_NAME_SIZE;

   handle->patch.fractions = *header++;

   handle->patch.wave_size = *header++;
   handle->patch.wave_size += *header++ << 8;
   handle->patch.wave_size += *header++ << 16;
   handle->patch.wave_size += *header++ << 24;

   handle->patch.start_loop = *header++;
   handle->patch.start_loop += *header++ << 8;
   handle->patch.start_loop += *header++ << 16;
   handle->patch.start_loop += *header++ << 24;

   handle->patch.end_loop = *header++;
   handle->patch.end_loop += *header++ << 8;
   handle->patch.end_loop += *header++ << 16;
   handle->patch.end_loop += *header++ << 24;

   handle->patch.sample_rate = *header++;
   handle->patch.sample_rate += *header++ << 8;

   handle->patch.low_frequency = *header++;
   handle->patch.low_frequency += *header++ << 8;
   handle->patch.low_frequency += *header++ << 16;
   handle->patch.low_frequency += *header++ << 24;

   handle->patch.high_frequency = *header++;
   handle->patch.high_frequency += *header++ << 8;
   handle->patch.high_frequency += *header++ << 16;
   handle->patch.high_frequency += *header++ << 24;

   handle->patch.root_frequency = *header++;
   handle->patch.root_frequency += *header++ << 8;
   handle->patch.root_frequency += *header++ << 16;
   handle->patch.root_frequency += *header++ << 24;

   handle->patch.tune = *header++;
   handle->patch.tune += *header++ << 8;

   handle->patch.balance = *header++;

   memcpy(handle->patch.envelope_rate, header, ENVELOPES);
   header += ENVELOPES;

   memcpy(handle->patch.envelope_level, header, ENVELOPES);
   header += ENVELOPES;

   handle->patch.tremolo_sweep = *header++;
   handle->patch.tremolo_rate = *header++;
   handle->patch.tremolo_depth = *header++;

   handle->patch.vibrato_sweep= *header++;
   handle->patch.vibrato_rate = *header++;
   handle->patch.vibrato_depth = *header++;

   handle->patch.modes = *header++;

   handle->patch.scale_frequency = *header++;
   handle->patch.scale_frequency += *header++ << 8;

   handle->patch.scale_factor = *header++;
   handle->patch.scale_factor += *header++ << 8;
   header += WAVE_RESERVED_SIZE;

   *processed += header-buffer;
   if (handle->sample_num != handle->patch_level &&
       handle->sample_num < handle->layer.samples)
   {
      handle->sample_num++;
      *processed = -handle->patch.wave_size - (header-buffer);

      return __F_NEED_MORE;
   }

   switch (handle->patch.modes & MODE_FORMAT)
   {
   case 0:
      handle->info.fmt = AAX_PCM8S;
      handle->bits_sample = 8;
      break;
   case 1:
      handle->info.fmt = AAX_PCM16S;
      handle->bits_sample = 16;
      break;
   case 2:
      handle->info.fmt = AAX_PCM8U;
      handle->bits_sample = 8;
      break;
   case 3:
      handle->info.fmt = AAX_PCM16U;
      handle->bits_sample = 16;
      break;
   default:
      break;
   }

   handle->info.rate = handle->patch.sample_rate;
   handle->info.blocksize = handle->info.no_tracks*handle->bits_sample/8;
   handle->info.no_samples = SIZE2SAMPLES(handle, handle->patch.wave_size);

   offs = (handle->patch.start_loop << 4) + (handle->patch.fractions >> 4);
   handle->info.loop_start = SIZE2SAMPLES(handle, offs)/16.0f;

   offs = (handle->patch.end_loop << 4) + (handle->patch.fractions & 0xF);
   handle->info.loop_end = SIZE2SAMPLES(handle, offs)/16.0f;

   handle->info.base_frequency = 0.001f*handle->patch.root_frequency;
   handle->info.low_frequency = 0.001f*handle->patch.low_frequency;
   handle->info.high_frequency = 0.001f*handle->patch.high_frequency;

   cents = 100.0f*(handle->patch.scale_factor-1024.0f)/1024.0f;
   handle->info.pitch_fraction = cents2pitch(cents, 1.0f);

   handle->info.tremolo_rate = CVTRATE(handle->patch.tremolo_rate);
   handle->info.tremolo_depth = CVTDEPTH(handle->patch.tremolo_depth);
   handle->info.tremolo_sweep = CVTSWEEP(handle->patch.tremolo_sweep);

   handle->info.vibrato_rate = CVTRATE(handle->patch.vibrato_rate);
   handle->info.vibrato_depth = CVTDEPTH(handle->patch.vibrato_depth);
   handle->info.vibrato_sweep = CVTSWEEP(handle->patch.vibrato_sweep);

   /*
    * An array of 6 rates and levels to implement a 6-point envelope.
    * The frist three stages can be used for attack and decay. If the
    * sustain flag is set, than the third envelope point will be the
    * sustain point. The last three envelpe points are for the release,
    * and an optional "echo" effect. If the last envelope point is left
    * at an audible level, then a sampled release can be heard after the
    * laste envelope point.
    */
   pos  = 1;
   for (i=0; i<6; ++i)
   {
      float level, rate;

      if (i == 2 && (handle->patch.modes & MODE_ENVELOPE_SUSTAIN))
      {
         level = handle->info.volume_envelope[2*(pos-1)];
         rate = AAX_FPINFINITE;
      }
      else
      {
         float prev = pos ? handle->info.volume_envelope[2*(pos-1)] : 0.0f;
         level = env_level_to_level(handle->patch.envelope_level[i]);
         rate = env_rate_to_time(handle->patch.envelope_rate[i], prev, level);
      }

      if (rate)
      {
         handle->info.volume_envelope[2*pos] = level;
         handle->info.volume_envelope[2*pos-1] = rate;
         pos++;
      }
   }

#if 0
 printf("Wave name:\t\t%s\n", handle->patch.wave_name);
 printf("Loop start:\t\t%g (%gs)\n", handle->info.loop_start, SIZE2TIME(handle,handle->info.loop_start));
 printf("Loop end:\t\t%g (%gs)\n", handle->info.loop_end, SIZE2TIME(handle,handle->info.loop_end));
 printf("Sample size:\t\t%i (%gs)\n", SIZE2SAMPLES(handle,handle->patch.wave_size), SIZE2TIME(handle,handle->info.no_samples));
 printf("Sample rate:\t\t%i Hz\n", handle->patch.sample_rate);
 printf("Low Frequency:\t\t%g Hz\n", 0.001f*handle->patch.low_frequency);
 printf("High Frequency:\t\t%g Hz\n", 0.001f*handle->patch.high_frequency);
 printf("Root Frequency:\t\t%g Hz\n", 0.001f*handle->patch.root_frequency);
 printf("Panning:\t\t%.1f\n", (float)(handle->patch.balance - 7)/16.0f);

 printf("Envelope Levels:\t");
 for (i=0; i<6; ++i) {
  float v = handle->info.volume_envelope[2*i];
  printf("%4.2f\t", v ? _MAX(v, 0.01f) : 0.0f);
 }
 printf("\n");

 printf("Envelope Rates:\t\t");
 for (i=0; i<6; ++i) {
  float v = handle->info.volume_envelope[2*i+1];
  if (v < 0.1f) printf ("%4.2fms\t", v*1000.0f);
  else if (v == AAX_FPINFINITE) printf("%4.2f\t", v);
  else printf("%4.2fs\t", v);
 }
 printf("\n");
 printf("Sampled release:\t%s\n", (handle->patch.envelope_level[ENVELOPES-1] > 8) ? "yes" : "no");

 printf("Tremolo Sweep:\t\t%3i (%.3g Hz)\n", handle->patch.tremolo_sweep,
                                             handle->info.tremolo_sweep);
 printf("Tremolo Rate:\t\t%3i (%.3g Hz)\n", handle->patch.tremolo_rate,
                                            handle->info.tremolo_rate);
 printf("Tremolo Depth:\t\t%3i (%.2g, %.3gdB)\n", handle->patch.tremolo_depth,
                                                  handle->info.tremolo_depth,
                                       CVTDEPT2DB(handle->patch.tremolo_depth));

 printf("Vibrato Sweep:\t\t%3i (%.3g Hz)\n", handle->patch.vibrato_sweep,
                                             handle->info.vibrato_sweep);
 printf("Vibrato Rate:\t\t%3i (%.3g Hz)\n", handle->patch.vibrato_rate,
                                            handle->info.vibrato_rate);
 printf("Vibrato Depth:\t\t%3i (%.3g octave, %g cents)\n",
                                                  handle->patch.vibrato_depth,
                                                  handle->info.vibrato_depth,
                                    CVTDEPT2CENTS(handle->patch.vibrato_depth));

 printf("Modes:\t\t\t0x%x\n", handle->patch.modes);
 printf(" - Sample Format:\t%i-bit %s\n",
            (handle->patch.modes & MODE_16BIT) ? 16 : 8,
            (handle->patch.modes & MODE_UNSIGNED) ? "unsigned" : "signed");
 printf(" - Looping:\t\t%s (%s-directional %s)\n",
            (handle->patch.modes & MODE_LOOPING) ? "yes" : "no",
            (handle->patch.modes & MODE_BIDIRECTIONAL) ? "bi" : "uni",
            (handle->patch.modes & MODE_REVERSE) ? "backwards" : "forward");
 printf(" - Envelope:\t\tsustain: %s, release: %s, fast-release: %s\n",
            (handle->patch.modes & MODE_ENVELOPE_SUSTAIN) ? "yes" : "no",
            (handle->patch.modes & MODE_ENVELOPE_RELEASE) ? "envelope" : "note-off",
            (handle->patch.modes & MODE_FAST_RELEASE) ? "yes" : "no");
 printf("Scale Frequency:\t%i\n", handle->patch.scale_frequency);
 printf("Scale Factor:\t\t%i (%gx)\n\n", handle->patch.scale_factor, handle->info.pitch_fraction);
#endif

   return AAX_TRUE;
}
