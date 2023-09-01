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

#include "dsp/common.h"

#include "extension.h"
#include "format.h"
#include "ext_pat.h"


typedef struct
{
   _fmt_t *fmt;

   struct _meta_t meta;
   _buffer_info_t info;

   char copy_to_buffer;

   char mip_level;
   char sampled_release;

   char bits_sample;
   int rate;
   int bitrate;
   int sample_num;
   unsigned int max_samples;
   unsigned int skip;

   int capturing;
   int mode;

   // We only support one instrument with one layer and one waveform
   // per file. So we take the first insrument unless the requested patch
   // number is addedd to the device name using "?patch=<n>"
   _patch_header_t header;
   _instrument_t instrument;
   _layer_t layer;
   _wave_t wave; //current

} _driver_t;

static float env_rate_to_time(_driver_t*, unsigned char, float, float);
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
         handle->rate = freq;
         handle->max_samples = 0;
         handle->info.rate = freq;
         handle->info.no_tracks = 1;
         handle->info.fmt = format;
         handle->info.no_samples = no_samples;
         handle->info.blocksize = tracks*bits_sample/8;
         handle->meta.trackno = strdup("0");

         if (handle->capturing)
         {
            handle->info.no_samples = UINT_MAX;
            *bufsize = MAX_PATCH_SIZE;
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
            int res = _aaxFormatDriverReadHeader(handle, buf, bufsize);
            if (res <= 0)
            {
               if (res == __F_EOF) *bufsize = 0;
               rv = buf;
            }
            else
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

      _aax_free_meta(&handle->meta);
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
   _driver_t *handle = ext->id;
   int rv = handle->fmt->set_name(handle->fmt, param, desc);

   if (!rv)
   {
      switch(param)
      {
      case __F_ARTIST:
         handle->meta.artist = strreplace(handle->meta.artist, desc);
         rv = AAX_TRUE;
         break;
      case __F_TITLE:
         handle->meta.title = strreplace(handle->meta.title, desc);
         rv = AAX_TRUE;
         break;
      case __F_GENRE:
         handle->meta.genre = strreplace(handle->meta.genre, desc);
         rv = AAX_TRUE;
         break;
      case __F_TRACKNO:
         handle->meta.trackno = strreplace(handle->meta.trackno, desc);
         rv = AAX_TRUE;
         break;
      case __F_ALBUM:
         handle->meta.album = strreplace(handle->meta.album, desc);
         rv = AAX_TRUE;
         break;
      case __F_DATE:
         handle->meta.date = strreplace(handle->meta.date, desc);
         rv = AAX_TRUE;
         break;
      case __F_COMMENT:
         handle->meta.comments = strreplace(handle->meta.comments, desc);
         rv = AAX_TRUE;
         break;
      case __F_COPYRIGHT:
         handle->meta.copyright = strreplace(handle->meta.copyright, desc);
         rv = AAX_TRUE;
         break;
      case __F_COMPOSER:
         handle->meta.composer = strreplace(handle->meta.composer, desc);
         rv = AAX_TRUE;
         break;
      case __F_ORIGINAL:
         handle->meta.original = strreplace(handle->meta.original, desc);
         rv = AAX_TRUE;
         break;
      case __F_WEBSITE:
         handle->meta.website = strreplace(handle->meta.website, desc);
         rv = AAX_TRUE;
         break;
      default:
         break;
      }
   }
   return rv;
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
         rv = handle->meta.title;
         break;
      case __F_TRACKNO:
         rv = handle->meta.trackno;
         break;
      case __F_COMMENT:
         rv = handle->meta.comments;
         break;
      case __F_COPYRIGHT:
         rv = handle->meta.copyright;
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

float
_pat_get(_ext_t *ext, int type)
{
   _driver_t *handle = ext->id;
   float rv = 0.0f;

   if (type >= __F_ENVELOPE_LEVEL && type < __F_ENVELOPE_LEVEL_MAX)
   {
      unsigned pos = type & 0xF;
      if (pos < ENVELOPES) {
         rv = handle->info.volume_envelope[2*pos];
      }
   }
   else if (type >= __F_ENVELOPE_RATE && type < __F_ENVELOPE_RATE_MAX)
   {
      unsigned pos = type & 0xF;
      if (pos < ENVELOPES)
      {
         float val = handle->info.volume_envelope[2*pos+1];
         if (val == AAX_FPINFINITE) rv = OFF_T_MAX;
         else rv = val;
      }
   }
   else
   {
      switch (type)
      {
      case __F_LOOP_COUNT:
         rv = handle->info.loop_count;
         break;
      case __F_FREQUENCY:
          rv = handle->info.rate;
          break;
      case __F_NO_SAMPLES:
         rv = handle->info.no_samples;
         break;
      case __F_NO_BYTES:
          rv = handle->wave.size;
          break;
      case __F_LOOP_START:
         rv = handle->info.loop_start;
         break;
      case __F_LOOP_END:
         rv = handle->info.loop_end;
         break;
      case __F_NO_PATCHES:
         rv = handle->layer.waves;
         break;
      case __F_ENVELOPE_SUSTAIN:
         rv = (handle->wave.modes & MODE_ENVELOPE_SUSTAIN);
         break;
      case __F_SAMPLED_RELEASE:
         rv = handle->sampled_release;
         break;
      case __F_BASE_FREQUENCY:
         rv = handle->info.base_frequency;
         break;
      case __F_LOW_FREQUENCY:
         rv = handle->info.low_frequency;
         break;
      case __F_HIGH_FREQUENCY:
         rv = handle->info.high_frequency;
         break;
      case __F_PITCH_FRACTION:
         rv = handle->info.pitch_fraction;
         break;
      case __F_TREMOLO_RATE:
         rv = handle->info.tremolo.rate;
         break;
      case __F_TREMOLO_DEPTH:
         rv = handle->info.tremolo.depth;
         break;
      case __F_TREMOLO_SWEEP:
         rv = handle->info.tremolo.sweep;
         break;
      case __F_VIBRATO_RATE:
         rv = handle->info.vibrato.rate;
         break;
      case __F_VIBRATO_DEPTH:
         rv = handle->info.vibrato.depth;
         break;
      case __F_VIBRATO_SWEEP:
         rv = handle->info.vibrato.sweep;
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

float
_pat_set(_ext_t *ext, int type, float value)
{
   _driver_t *handle = ext->id;
   float rv = 0.0f;

   switch (type)
   {
   case __F_COPY_DATA:
      handle->copy_to_buffer = value;
      break;
   case __F_MIP_LEVEL:
      handle->mip_level = value;
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
env_rate_to_time(_driver_t *handle, unsigned char rate, float prev, float next)
{
   // rate is defined as RRMMMMMM
   // where RR is the rate and MMMMMM the mantissa

   float FUR = 1.0f/(1.6f*handle->header.voices); // 14 voices
   float VUR = FUR/(float)(1 << 3*(rate >> 6)); // Volume Update Rate
   float mantissa = (float)(rate & 0x3f);	// Volume Increase
   return fabsf(next-prev)/(244.0f*VUR)/mantissa;
}

static float
env_level_to_level(unsigned char level)
{
   // level is defined as: EEEEMMMM
   // where EEEE is the exponent and MMMM is the mantissa
   // val = mantissa * 2^exponent

   int mantissa = level & 0xF;
   int exponent = level >> 4;
   int prod = mantissa*(1 << exponent);
   float rv = 2.5f*prod/491520.0;
   if (rv < LEVEL_60DB) rv = 0.0f;
   return rv;
}

static int
_aaxFormatDriverReadHeader(_driver_t *handle, unsigned char *header, ssize_t *processed)
{
   unsigned char *buffer = header;
   ssize_t bufsize = *processed;
   float cents, level, prev;
   int i, pos;

   if (handle->skip > bufsize)
   {
      handle->skip -= bufsize;
      *processed = bufsize;
      return __F_NEED_MORE;
   }

   *processed = 0;
   header += handle->skip;
   handle->skip = 0;

   if (!handle->header.instruments) // First time here
   {
      if (!memcmp(header, GF1_HEADER_TEXT, GF1_HEADER_SIZE))
      {
         char field[16];

         // Patch Header
         memcpy(handle->header.header, header, GF1_HEADER_SIZE);
         handle->header.header[GF1_HEADER_SIZE] = 0;
         header += GF1_HEADER_SIZE;

         memcpy(handle->header.gravis_id, header, PATCH_ID_SIZE);
         handle->header.gravis_id[PATCH_ID_SIZE] = 0;
         header += PATCH_ID_SIZE;

         memcpy(handle->header.description, header, PATCH_DESC_SIZE);
         handle->header.description[PATCH_DESC_SIZE] = 0;
         header += PATCH_DESC_SIZE;

         handle->header.instruments = *header++;
         handle->header.voices = *header++;
         handle->header.channels= *header++;

         handle->header.waveforms = *header++;
         handle->header.waveforms |= (uint16_t)(*header++) << 8;

         handle->header.master_volume = *header++;
         handle->header.master_volume |= (uint16_t)(*header++) << 8;

         handle->header.data_size = *header++;
         handle->header.data_size |= (uint32_t)(*header++) << 8;
         handle->header.data_size |= (uint32_t)(*header++) << 16;
         handle->header.data_size |= (uint32_t)(*header++) << 24;
         header += PATCH_RESERVED_SIZE;
#if 0
 printf("= Header:\t\t%s\n", handle->header.header);
 printf("Gravis id:\t\t%s\n", handle->header.gravis_id);
 printf("Description:\t\t%s\n", handle->header.description);
 printf("Instruments:\t\t%i\n", handle->header.instruments);
 printf("Voices:\t\t\t%i\n", handle->header.voices);
 printf("Channels:\t\t%i\n", handle->header.channels);
 printf("Waveforms:\t\t%i\n", handle->header.waveforms);
 printf("MasterVolume:\t\t%i\n\n", handle->header.master_volume);
 printf("DataSize:\t\t%i\n", handle->header.data_size);
#endif

         // Instrument Header
         handle->instrument.instrument = *header++;
         handle->instrument.instrument |= (uint16_t)(*header++) << 8;

         memcpy(handle->instrument.name, header, INSTRUMENT_NAME_SIZE);
         handle->instrument.name[INSTRUMENT_NAME_SIZE] = 0;
         header += INSTRUMENT_NAME_SIZE;

         handle->instrument.size = *header++;
         handle->instrument.size |= (int32_t)(*header++) << 8;
         handle->instrument.size |= (int32_t)(*header++) << 16;
         handle->instrument.size |= (int32_t)(*header++) << 24;

         handle->instrument.layers = *header++;
         header += INSTRUMENT_RESERVED_SIZE;
#if 0
 printf("== Instrument name:\t%s\n", handle->instrument.name);
 printf("Instrument number:\t%i\n", handle->instrument.instrument);
 printf("Instrument size:\t%i\n", handle->instrument.size);
 printf("Instrument layers:\t%i\n\n", handle->instrument.layers);
#endif

         // Layer Header
         handle->layer.layer_duplicate = *header++;
         handle->layer.layer = *header++;

         handle->layer.size = *header++;
         handle->layer.size |= (int32_t)(*header++) << 8;
         handle->layer.size |= (int32_t)(*header++) << 16;
         handle->layer.size |= (int32_t)(*header++) << 24;

         handle->layer.waves = *header++;
         header += LAYER_RESERVED_SIZE;
#if 0
 printf("=== Layer dupplicate:\t%i\n", handle->layer.layer_duplicate);
 printf("Layer number:\t\t%i\n", handle->layer.layer);
 printf("Layer size:\t\t%i\n", handle->layer.size);
 printf("Waves:\t\t%i\n", handle->layer.waves);
 printf("Wave requested:\t%i\n\n", handle->mip_level+1);
#endif

         snprintf(field, 8, "%u", handle->layer.layer);
         handle->meta.trackno = strreplace(handle->meta.trackno, field);
         handle->meta.title = strreplace(handle->meta.title,
                                         handle->instrument.name);
         handle->meta.copyright = strreplace(handle->meta.copyright,
                                            handle->header.description);
      }
      else // Wrong format
      {
         *processed = bufsize;
         return __F_EOF;
      }
   }

   // Wave Header
   memcpy(handle->wave.name, header, WAVE_NAME_SIZE);
   handle->wave.name[WAVE_NAME_SIZE] = 0;
   header += WAVE_NAME_SIZE;

   handle->wave.fractions = *header++;

   handle->wave.size = *header++;
   handle->wave.size |= (int32_t)(*header++) << 8;
   handle->wave.size |= (int32_t)(*header++) << 16;
   handle->wave.size |= (int32_t)(*header++) << 24;

   handle->wave.start_loop = *header++;
   handle->wave.start_loop |= (int32_t)(*header++) << 8;
   handle->wave.start_loop |= (int32_t)(*header++) << 16;
   handle->wave.start_loop |= (int32_t)(*header++) << 24;

   handle->wave.end_loop = *header++;
   handle->wave.end_loop |= (int32_t)(*header++) << 8;
   handle->wave.end_loop |= (int32_t)(*header++) << 16;
   handle->wave.end_loop |= (int32_t)(*header++) << 24;

   handle->wave.sample_rate = *header++;
   handle->wave.sample_rate |= (uint16_t)(*header++) << 8;

   handle->wave.low_frequency = *header++;
   handle->wave.low_frequency |= (int32_t)(*header++) << 8;
   handle->wave.low_frequency |= (int32_t)(*header++) << 16;
   handle->wave.low_frequency |= (int32_t)(*header++) << 24;

   handle->wave.high_frequency = *header++;
   handle->wave.high_frequency |= (int32_t)(*header++) << 8;
   handle->wave.high_frequency |= (int32_t)(*header++) << 16;
   handle->wave.high_frequency |= (int32_t)(*header++) << 24;

   handle->wave.root_frequency = *header++;
   handle->wave.root_frequency |= (int32_t)(*header++) << 8;
   handle->wave.root_frequency |= (int32_t)(*header++) << 16;
   handle->wave.root_frequency |= (int32_t)(*header++) << 24;

   handle->wave.tune = *header++;
   handle->wave.tune |= (int16_t)(*header++) << 8;

   handle->wave.balance = *header++;

   memcpy(handle->wave.envelope_rate, header, ENVELOPES);
   header += ENVELOPES;

   memcpy(handle->wave.envelope_level, header, ENVELOPES);
   header += ENVELOPES;

   handle->wave.tremolo.sweep = *header++;
   handle->wave.tremolo.rate = *header++;
   handle->wave.tremolo.depth = *header++;

   handle->wave.vibrato.sweep= *header++;
   handle->wave.vibrato.rate = *header++;
   handle->wave.vibrato.depth = *header++;

   handle->wave.modes = *header++;

   handle->wave.scale_frequency = *header++;
   handle->wave.scale_frequency |= (int16_t)(*header++) << 8;

   handle->wave.scale_factor = *header++;
   handle->wave.scale_factor |= (int16_t)(*header++) << 8;
   header += WAVE_RESERVED_SIZE;

   *processed += (header-buffer);

   switch (handle->wave.modes & MODE_FORMAT)
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

   handle->meta.comments = strreplace(handle->meta.comments, handle->wave.name);

   handle->info.rate = handle->wave.sample_rate;
   handle->info.blocksize = handle->info.no_tracks*handle->bits_sample/8;
   handle->info.no_samples = SIZE2SAMPLES(handle, handle->wave.size);

   handle->info.loop_count = (handle->wave.modes & MODE_LOOPING)? OFF_T_MAX : 0;
   if (handle->info.loop_count)
   {
      int loop_start, loop_end;

      loop_start = handle->wave.start_loop;
      handle->info.loop_start = SIZE2SAMPLES(handle, loop_start);
      handle->info.loop_start += (float)(handle->wave.fractions >> 4)/16.0f;

      loop_end = handle->wave.end_loop;
      handle->info.loop_end = SIZE2SAMPLES(handle, loop_end);
      handle->info.loop_end += (float)(handle->wave.fractions & 0xF)/16.0f;
   }
   else
   {
      handle->info.loop_start = 0;
      handle->info.loop_end = handle->info.no_samples;
   }

   handle->info.base_frequency = 0.001f*handle->wave.root_frequency;
   handle->info.low_frequency = 0.001f*handle->wave.low_frequency;
   handle->info.high_frequency = 0.001f*handle->wave.high_frequency;

   cents = 100.0f*(handle->wave.scale_factor-1024.0f)/1024.0f;
   handle->info.pitch_fraction = cents2pitch(cents, 1.0f);

   handle->info.tremolo.rate = CVTRATE(handle->wave.tremolo.rate);
   handle->info.tremolo.depth = CVTDEPTH(handle->wave.tremolo.depth);
   handle->info.tremolo.sweep = CVTRATE(handle->wave.tremolo.sweep);

   handle->info.vibrato.rate = CVTRATE(handle->wave.vibrato.rate);
   handle->info.vibrato.depth = CVTDEPTH2PITCH(handle->wave.vibrato.depth);
   handle->info.vibrato.sweep = CVTRATE(handle->wave.vibrato.sweep);

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
   prev = level = 0.0f;
   for (i=0; i<ENVELOPES; ++i)
   {
      float rate;

      if (i == 2 && (handle->wave.modes & MODE_ENVELOPE_SUSTAIN))
      {
         level = prev;
         rate = level ? AAX_FPINFINITE: 0.0f;
      }
      else
      {
         level = env_level_to_level(handle->wave.envelope_level[i]);
         rate = env_rate_to_time(handle, handle->wave.envelope_rate[i], prev, level);
      }

      if (rate)
      {
         handle->info.volume_envelope[2*pos] = level;
         handle->info.volume_envelope[2*pos-1] = rate;
         prev = level;

         pos++;
      }
   }

   handle->sampled_release = (level > LEVEL_60DB) ? 1 : 0;

#if 0
 printf("==== Wave name:\t\t%s\n", handle->wave.name);
 printf("     Wave number: %i of %i\n", handle->sample_num+1, handle->layer.waves);
 printf("Fractions:\t\tstart: %i, end: %i\n", handle->wave.fractions >> 4, handle->wave.fractions & 0xF);
 printf("Sample size:\t\t%i bytes, %i samples, %.3g sec\n",handle->wave.size, SIZE2SAMPLES(handle,handle->wave.size), SAMPLES2TIME(handle,handle->info.no_samples));
 printf("Loop start:\t\t%i bytes, %.20g samples, %.3g sec\n", handle->wave.start_loop, handle->info.loop_start, SAMPLES2TIME(handle,handle->info.loop_start));
 printf("Loop end:\t\t%i bytes, %.20g samples, %.3g sec\n", handle->wave.end_loop, handle->info.loop_end, SAMPLES2TIME(handle,handle->info.loop_end));
 printf("Sample rate:\t\t%i Hz\n", handle->wave.sample_rate);
 printf("Low Frequency:\t\t%g Hz, note %i (%s)\n", 0.001f*handle->wave.low_frequency, _freq2note(0.001f*handle->wave.low_frequency), _note2name(_freq2note(0.001f*handle->wave.low_frequency)));
 printf("High Frequency:\t\t%g Hz, note %i (%s)\n", 0.001f*handle->wave.high_frequency, _freq2note(0.001f*handle->wave.high_frequency), _note2name(_freq2note(0.001f*handle->wave.high_frequency)));
 printf("Root Frequency:\t\t%g Hz, note %i (%s)\n", 0.001f*handle->wave.root_frequency, _freq2note(0.001f*handle->wave.root_frequency), _note2name(_freq2note(0.001f*handle->wave.root_frequency)));
 printf("Tune:\t\t\t%i\n", handle->wave.tune);
 printf("Panning:\t\t%i (%s: %.1f)\n", handle->wave.balance, (handle->wave.balance < 5) ? "Left" : (handle->wave.balance > 9) ? "Right" : "Center", (float)(handle->wave.balance - 7)/16.0f);

 printf("Envelope Levels (Raw):\t");
 for (i=0; i<6; ++i) {
  printf("%i\t", handle->wave.envelope_level[i]);
 }
 printf("\n");
 printf("Envelope Rates (Raw):\t");
 for (i=0; i<6; ++i) {
  printf("0x%0x\t", handle->wave.envelope_rate[i]);
 }
 printf("\n");
 printf("                      \t");
 printf("----------------------------------------------\n");
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

 printf("Tremolo Sweep:\t\t%3i (%.3g Hz)\n", handle->wave.tremolo.sweep,
                                             handle->info.tremolo.sweep);
 printf("Tremolo Rate:\t\t%3i (%.3g Hz)\n", handle->wave.tremolo.rate,
                                            handle->info.tremolo.rate);
 printf("Tremolo Depth:\t\t%3i (%.2g, %.3gdB)\n", handle->wave.tremolo.depth,
                                                  handle->info.tremolo.depth,
                                       CVTDEPT2DB(handle->wave.tremolo.depth));

 printf("Vibrato Sweep:\t\t%3i (%.3g Hz)\n", handle->wave.vibrato.sweep,
                                             handle->info.vibrato.sweep);
 printf("Vibrato Rate:\t\t%3i (%.3g Hz)\n", handle->wave.vibrato.rate,
                                            handle->info.vibrato.rate);
 printf("Vibrato Depth:\t\t%3i (%.3g octave, %g cents)\n",
                                                  handle->wave.vibrato.depth,
                                                  handle->info.vibrato.depth,
                                    CVTDEPT2CENTS(handle->wave.vibrato.depth));

 printf("Modes:\t\t\t0x%x\n", handle->wave.modes);
 printf(" - Sample Format:\t%i-bit %s\n",
            (handle->wave.modes & MODE_16BIT) ? 16 : 8,
            (handle->wave.modes & MODE_UNSIGNED) ? "unsigned" : "signed");
 printf(" - Looping:\t\t%s (%s-directional %s)\n",
            (handle->wave.modes & MODE_LOOPING) ? "yes" : "no",
            (handle->wave.modes & MODE_BIDIRECTIONAL) ? "bi" : "uni",
            (handle->wave.modes & MODE_REVERSE) ? "backwards" : "forward");
 printf(" - Envelope:\t\tsustain: %s, release: %s, fast-release: %s\n",
            (handle->wave.modes & MODE_ENVELOPE_SUSTAIN) ? "yes" : "no",
            (handle->wave.modes & MODE_ENVELOPE_RELEASE) ? "envelope" : "note-off",
            (handle->wave.modes & MODE_FAST_RELEASE) ? "yes" : "no");
 printf("Sampled release:\t%s\n", handle->sampled_release ? "yes" : "no");
 printf("Scale Frequency:\t%i\n", handle->wave.scale_frequency);
 printf("Scale Factor:\t\t%i (%.gx)\n\n", handle->wave.scale_factor, handle->info.pitch_fraction);
#endif

   if (handle->sample_num != handle->mip_level &&
       handle->sample_num < handle->layer.waves)
   {
      handle->sample_num++;
      handle->skip = handle->wave.size;
      return __F_NEED_MORE;
   }

   return AAX_TRUE;
}
