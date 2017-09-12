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
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
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

typedef enum
{
  PA_SEEK_RELATIVE  = 0,
  PA_SEEK_ABSOLUTE,
  PA_SEEK_RELATIVE_ON_READ,
  PA_SEEK_RELATIVE_END
} pa_seek_mode_t;

typedef enum
{
  PA_CONTEXT_UNCONNECTED = 0,
  PA_CONTEXT_CONNECTING,
  PA_CONTEXT_AUTHORIZING,
  PA_CONTEXT_SETTING_NAME,
  PA_CONTEXT_READY,
  PA_CONTEXT_FAILED,
  PA_CONTEXT_TERMINATED
} pa_context_state_t;

typedef enum
{
  PA_CONTEXT_NOFLAGS = 0,
  PA_CONTEXT_NOAUTOSPAWN,
  PA_CONTEXT_NOFAIL
} pa_context_flags_t;

typedef enum
{
  PA_STREAM_UNCONNECTED = 0,
  PA_STREAM_CREATING,
  PA_STREAM_READY,
  PA_STREAM_FAILED,
  PA_STREAM_TERMINATED
} pa_stream_state_t;

typedef struct
{
  pa_sample_format_t format;
  uint32_t rate;
  uint8_t channels;
} pa_sample_spec;

typedef void (*pa_state_callback_fn)(void*, void*);
typedef void (*pa_write_callback_fn)(void*, size_t, void*);
typedef void (*pa_signal_callback_fn)(void*, void*, int, void*);

typedef int (*pa_get_binary_name_proc)(const char*, size_t);
typedef const char* (*pa_path_get_filename_proc)(const char*);
typedef const char* (*pa_get_library_version_proc)(void);
typedef const char* (*pa_strerror_proc)(int);
typedef void (*pa_xfree_proc)(void*);

typedef void* (*pa_threaded_mainloop_new_proc)(void);
typedef int (*pa_threaded_mainloop_free_proc)(void*);
typedef int (*pa_threaded_mainloop_start_proc)(void*);
typedef int (*pa_threaded_mainloop_stop_proc)(void*);
typedef int (*pa_threaded_mainloop_lock_proc)(void*);
typedef int (*pa_threaded_mainloop_unlock_proc)(void*);
typedef int (*pa_threaded_mainloop_wait_proc)(void*);
typedef void (*pa_threaded_mainloop_signal_proc)(void*, int);
typedef void* (*pa_threaded_mainloop_get_api_proc)(void*);

typedef void* (*pa_context_new_proc)(void*, const char*);
typedef void* (*pa_context_set_state_callback_proc)(void*, pa_state_callback_fn, void*);
typedef int (*pa_context_connect_proc)(void*, const char*, int, const void*);
typedef void (*pa_context_disconnect_proc)(void*);
typedef int (*pa_context_get_state_proc)(void*);
typedef void (*pa_context_unref_proc)(void*);
typedef int (*pa_context_errno_proc)(void*);

typedef void* (*pa_stream_new_proc)(void*, const char*, const void*, const void*);
typedef void (*pa_stream_set_state_callback_proc)(void*, pa_state_callback_fn, void*);
typedef void (*pa_stream_connect_playback_proc)(void*, const char*, const void*, int, void*, void*);
typedef void (*pa_stream_disconnect_proc)(void*);
typedef void (*pa_stream_unref_proc)(void*);
typedef int (*pa_stream_begin_write_proc)(void*, void**, size_t*);
typedef int (*pa_stream_write_proc)(void*, const void*, size_t, pa_xfree_proc, int64_t, int);
typedef int (*pa_stream_get_state_proc)(void*);

typedef void (*pa_signal_new_proc)(int, pa_signal_callback_fn*, void*);
typedef void (*pa_signal_done_proc)(void);

#endif /* _AAX_PULSEAUDIO_H */

