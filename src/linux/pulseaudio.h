/*
 * Copyright 2017 by Erik Hofman.
 * Copyright 2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _AAX_PULSEAUDIO_H
#define _AAX_PULSEAUDIO_H 1

#include <base/types.h>

// https://freedesktop.org/software/pulseaudio/doxygen/simple.html
typedef enum
{
  PA_SAMPLE_U8 = 0,
  PA_SAMPLE_ALAW,
  PA_SAMPLE_ULAW,
  PA_SAMPLE_S16LE,
  PA_SAMPLE_S16BE,
  PA_SAMPLE_FLOAT32LE,
  PA_SAMPLE_FLOAT32BE,
  PA_SAMPLE_S32LE,
  PA_SAMPLE_S32BE,
  PA_SAMPLE_S24LE,
  PA_SAMPLE_S24BE,
  PA_SAMPLE_S24_32LE,
  PA_SAMPLE_S24_32BE,
  PA_SAMPLE_MAX
} pa_sample_format_t;

typedef enum
{
  PA_STREAM_NODIRECTION = 0,
  PA_STREAM_PLAYBACK,
  PA_STREAM_RECORD,
  PA_STREAM_UPLOAD
} pa_stream_direction_t;

typedef struct
{
  pa_sample_format_t format;
  uint32_t rate;
  uint8_t channels;

} pa_simple_spec;

typedef void pa_sample_spec;
typedef void pa_channel_map;
typedef void pa_buffer_attr;

typedef void* (*pa_simple_new_proc)(const char*, const char*, pa_stream_direction_t, const char*, const char*, const pa_sample_spec*, const pa_channel_map*, const pa_buffer_attr*, int*);
typedef void (*pa_simple_free_proc)(void*);
 
typedef int (*pa_simple_write_proc)(void*, const void*, size_t, int *);
typedef int (*pa_simple_drain_proc)(void*, int*);
typedef int (*pa_simple_read_proc)(void*, void*, size_t, int*);
typedef uint16_t (*pa_simple_get_latency_proc)(void*, int*);
typedef int (*pa_simple_flush_proc)(void*, int*);

typedef const char* (*pa_strerror_proc)(int);

#endif /* _AAX_PULSEAUDIO_H */

