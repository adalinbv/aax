/*
 * SPDX-FileCopyrightText: Copyright © 2019-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2019-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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
#include "ext_bach.h"


typedef struct
{
   _fmt_t *fmt;

   _buffer_info_t info;

   bool copy_to_buffer;

   int rate;
   int sample_num;
   unsigned int max_samples;
   unsigned int skip;

   int capturing;
   int mode;

   _aaxs_t aaxs;

} _driver_t;


int
_bach_detect(UNUSED(_ext_t *ext), int mode)
{
   if (!mode) return true;
   return false;
}

int
_bach_setup(_ext_t *ext, int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
{
   int bits_sample = aaxGetBitsPerSample(format);
   int rv = false;

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
         handle->info.meta.trackno = strdup("0");

         if (handle->capturing)
         {
            handle->info.no_samples = UINT_MAX;
            *bufsize = MAX_PATCH_SIZE;
         }
         else {
            *bufsize = 0;
         }
         ext->id = handle;
         rv = true;
      }
      else {
         _AAX_FILEDRVLOG("BACH: Insufficient memory");
      }
   }
   else {
      _AAX_FILEDRVLOG("BACH: Unsupported format");
   }

   return rv;
}

void*
_bach_open(_ext_t *ext, void_ptr buf, ssize_t *bufsize, size_t fsize)
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
            _AAX_FILEDRVLOG("BACH: Incorrect format");
            return rv;
         }
      }
      else _AAX_FILEDRVLOG("BACH: Unknown opening error");
   }
   else {
      _AAX_FILEDRVLOG("BACH: Internal error: handle id equals 0");
   }

   return rv;
}

int
_bach_close(_ext_t *ext)
{
   _driver_t *handle = ext->id;
   int res = true;

   if (handle)
   {
      if (handle->fmt)
      {
         handle->fmt->close(handle->fmt);
         _fmt_free(handle->fmt);
      }

      _aax_free_info.meta(&handle->info.meta);
      free(handle);
   }

   return res;
}

void*
_bach_update(_ext_t *ext, size_t *offs, ssize_t *size, char close)
{
   return NULL;
}

size_t
_bach_copy(_ext_t *ext, int32_ptr dptr, size_t level, size_t *num)
{
   _driver_t *handle = ext->id;
   return handle->fmt->copy(handle->fmt, dptr, level, num);
}

size_t
_bach_fill(_ext_t *ext, void_ptr sptr, ssize_t *bytes)
{
   _driver_t *handle = ext->id;
   return handle->fmt->fill(handle->fmt, sptr, bytes);
}

size_t
_bach_cvt_from_intl(_ext_t *ext, int32_ptrptr dptr, size_t level, size_t *num)
{
   _driver_t *handle = ext->id;
   return handle->fmt->cvt_from_intl(handle->fmt, dptr, level, num);
}

size_t
_bach_cvt_to_intl(_ext_t *ext, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = ext->id;
   return handle->fmt->cvt_to_intl(handle->fmt, dptr, sptr, offs, num, scratch, scratchlen);
}

int
_bach_set_name(_ext_t *ext, enum _aaxStreamParam param, const char *desc)
{
   _driver_t *handle = ext->id;
   int rv = handle->fmt->set_name(handle->fmt, param, desc);

   if (!rv)
   {
      switch(param)
      {
      case __F_ARTIST:
         handle->info.meta.artist = strreplace(handle->info.meta.artist, desc);
         rv = true;
         break;
      case __F_TITLE:
         handle->info.meta.title = strreplace(handle->info.meta.title, desc);
         rv = true;
         break;
      case __F_GENRE:
         handle->info.meta.genre = strreplace(handle->info.meta.genre, desc);
         rv = true;
         break;
      case __F_TRACKNO:
         handle->info.meta.trackno = strreplace(handle->info.meta.trackno, desc);
         rv = true;
         break;
      case __F_ALBUM:
         handle->info.meta.album = strreplace(handle->info.meta.album, desc);
         rv = true;
         break;
      case __F_DATE:
         handle->info.meta.date = strreplace(handle->info.meta.date, desc);
         rv = true;
         break;
      case __F_COMMENT:
         handle->info.meta.comments = strreplace(handle->info.meta.comments, desc);
         rv = true;
         break;
      case __F_COPYRIGHT:
         handle->info.meta.copyright = strreplace(handle->info.meta.copyright, desc);
         rv = true;
         break;
      case __F_COMPOSER:
         handle->info.meta.composer = strreplace(handle->info.meta.composer, desc);
         rv = true;
         break;
      case __F_ORIGINAL:
         handle->info.meta.original = strreplace(handle->info.meta.original, desc);
         rv = true;
         break;
      case __F_WEBSITE:
         handle->info.meta.website = strreplace(handle->info.meta.website, desc);
         rv = true;
         break;
      default:
         break;
      }
   }
   return rv;
   return false;
}

char*
_bach_name(_ext_t *ext, enum _aaxStreamParam param)
{
   _driver_t *handle = ext->id;
   char *rv = handle->fmt->name(handle->fmt, param);

   if (!rv)
   {
      switch(param)
      {
      case __F_TITLE:
         rv = handle->info.meta.title;
         break;
      case __F_TRACKNO:
         rv = handle->info.meta.trackno;
         break;
      case __F_COMMENT:
         rv = handle->info.meta.comments;
         break;
      case __F_COPYRIGHT:
         rv = handle->info.meta.copyright;
         break;
      default:
         break;
      }
   }
   return rv;
}

char*
_bach_interfaces(UNUSED(int ext), int mode)
{	// silently support patch files
   static const char *rd[2] = { ".bach", NULL };
   return (char *)rd[mode];
}

int
_bach_extension(char *ext)
{
   return (ext && !strcasecmp(ext, "bach")) ? 1 : 0;
}

float
_bach_get(_ext_t *ext, int type)
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
_bach_set(_ext_t *ext, int type, float value)
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

static const char*
get_str(uint32_t **s)
{
   char *rv = NULL;
   uint32_t *buf = *s++;
   uint32_t len = *buf++;
   if (len)
   {
      s += len;
      rv = malloc(6*len+1);
      if (rv)
      {
         char *str = (char*)buf;
         _aaxStringConv(_info->cd, str, len, rv, 6*len);
      }
   }

   return rv;
}

static void
get_dsp(_dsp_t *dsp, uint32_t **s)
{
   uint32_t *buf = *s;
   uint32_t size = *buf++;
   uint32_t i, j, tmp;

   dsp->type = *buf++;
   dsp->src = *buf++;

   // slots
   tmp = *buf++;
   dsp->no_slots = tmp & 0xFF; // no_slots
   for (i=0; i<dsp->no_slots; ++i)
   {
      if (i == BACH_MAX_SLOTS) break;
      dsp->slot[i].src = *buf++;

      // param
      for (j=0; j<4; ++j)
      {
         dsp->slot[i].param[j].type = *buf++;
         dsp->slot[i].param[j].value = (float*)buf++;
         dsp->slot[i].param[j].min = (float*)buf++;
         dsp->slot[i].param[j].max = (float*)buf++;
         dsp->slot[i].param[j].adjust = (float*)buf++;
         dsp->slot[i].param[j].pitch = (float*)buf++;
         dsp->slot[i].param[j].random = (float*)buf++;
      }
   }
   *s = buf;
}

_aaxFormatDriverReadHeader(_driver_t *handle, unsigned char *header, ssize_t *processed)
{
   uint32_t *buffer = (uint32_t*)header;
   ssize_t bufsize = *processed;

   if (handle->skip > bufsize)
   {
      handle->skip -= bufsize;
      *processed = bufsize;
      return __F_NEED_MORE;
   }

   *processed = 0;
   header += handle->skip;
   handle->skip = 0;

   if (handle->aax.version == 0) // First time here
   {
      int i, j;
      if (!memcmp(header, "BACH", 4))
      {
         uint32_t size = *buffer++;
         if (size <= bufsize)
         {
            // info
            uint32_t tmp = *buffer++;
            handle->aaxs.info.version = (tmp >> 24);
            handle->aaxs.info.bank_no = (tmp >> 16) & 0xFF;
            handle->aaxs.info.program_no = (tmp >> 8) & 0xFF;

            tmp = *buffer++;
            handle->aaxs.info.note.polyphony = (tmp >> 24);
            handle->aaxs.info.note.min = (tmp >> 16) & 0xFF;
            handle->aaxs.info.note.max = (tmp >> 8) & 0xFF;
            handle->aaxs.info.note.pitch-fraction = (float*)buffer++;

            handle->aaxs.info.meta.title = get_str(&buffer);	// name
            handle->aaxs.info.meta.comments = get_str(&buffer);	// license
            handle->aaxs.info.meta.copyright = get_str(&buffer);
            handle->aaxs.info.meta.composer = get_str(&buffer);	// author
            handle->aaxs.info.meta.website = get_str(&buffer);

            // resonator
            size = *buffer++;
            handle->aaxs.resonator.frequency = (float*)buffer++;

            handle->aaxs.resonator.no_layers = *buffer++ & 0xFF;
            for (i=0; i<handle->aaxs.resonator.no_layers; ++i)
            {
               if (i == BACH_MAX_LAYERS) break;
               handle->aaxs.resonator.layer[i].loop_start = (float*)buffer++;
               handle->aaxs.resonator.layer[i].loop_end = (float*)buffer++;
               size = *buffer++;
               handle->aaxs.resonator.layer[i].data = malloc(size);
               if (handle->aaxs.resonator.layer[i].data)
               {
                   float *data = handle->aaxs.resonator.layer[i].data;
                   for (j=0; j<size; ++j) {
                       *data++ = (float*)buffer++;
                   }
               }
               else
               {
                  // not enough memory
                  return __F_EOF;
               }
            }

            // actuator
            size = *buffer++;
            tmp = *buffer++;
            handle->aaxs.actuator.looping = (tmp & 0xFF000000) ? true : false;
            size = (tmp & 0xFF);
            for (i=0; i<size; ++i)
            {
               if (i == BACH_MAX_DSP_ENTRIES) break;
               get_dsp(&handle->aaxs.actuator.dsp[i], &buffer);
            }

            // body
            size = *buffer++;
            handle->aaxs.body.mode = *buffer++;
            handle->aaxs.body.pan = (float*)buffer++;
            tmp = *buffer++;
            size = (tmp & 0xFF);
            for (i=0; i<size; ++i)
            {
               if (i == BACH_MAX_DSP_ENTRIES) break;
               get_dsp(&handle->aaxs.body.dsp[i], &buffer);
            }
         }
         else
         {
            return __F_NEED_MORE;
         }
      }
      else // Wrong format
      {
         *processed = bufsize;
         return __F_EOF;
      }
   }

   return true;
}
