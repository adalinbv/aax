/*
 * Copyright 2022 by Erik Hofman.
 * Copyright 2022 by Adalin B.V.
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

#ifndef _PIPEWIRE_AUDIO_H
#define _PIPEWIRE_AUDIO_H 1

#include <pipewire/pipewire.h>
#include <pipewire/extensions/metadata.h>
#include <spa/param/audio/format-utils.h>

/*
 * The following keys are defined for compatability when building against older
 * versions of Pipewire prior to their introduction and can be removed if the
 * minimum required Pipewire version is increased to or beyond their point of
 * introduction.
 */
#ifndef PW_KEY_CONFIG_NAME
#define PW_KEY_CONFIG_NAME "config.name"
#endif

#ifndef PW_KEY_NODE_RATE
#define PW_KEY_NODE_RATE "node.rate"
#endif

#define PW_ID_TO_HANDLE(x) (void *_proc)((uintptr_t)x)
#define PW_HANDLE_TO_ID(x) (uint32_t)((uintptr_t)x)

enum PW_READY_FLAGS
{
    PW_READY_FLAG_BUFFER_ADDED = 0x1,
    PW_READY_FLAG_STREAM_READY = 0x2,
    PW_READY_FLAG_ALL_BITS     = 0x3
};

// static void (*pw_init_proc)(int *, char **);
// static void (*pw_deinit_proc)(void);
// static struct pw_thread_loop *(*pw_thread_loop_new_proc)(const char *, const struct spa_dict *);
// static void (*pw_thread_loop_destroy_proc)(struct pw_thread_loop *);
// static void (*pw_thread_loop_stop_proc)(struct pw_thread_loop *);
// static struct pw_loop *(*pw_thread_loop_get_loop_proc)(struct pw_thread_loop *);
// static void (*pw_thread_loop_lock_proc)(struct pw_thread_loop *);
// static void (*pw_thread_loop_unlock_proc)(struct pw_thread_loop *);
// static void (*pw_thread_loop_signal_proc)(struct pw_thread_loop *, bool);
// static void (*pw_thread_loop_wait_proc)(struct pw_thread_loop *);
// static int (*pw_thread_loop_start_proc)(struct pw_thread_loop *);
// static struct pw_context *(*pw_context_new_proc)(struct pw_loop *, struct pw_properties *, size_t);
// static void (*pw_context_destroy_proc)(struct pw_context *);
// static struct pw_core *(*pw_context_connect_proc)(struct pw_context *, struct pw_properties *, size_t);
// static void (*pw_proxy_add_object_listener_proc)(struct pw_proxy *, struct spa_hook *, const void *, void *);
// static void *(*pw_proxy_get_user_data_proc)(struct pw_proxy *);
// static void (*pw_proxy_destroy_proc)(struct pw_proxy *);
// static int (*pw_core_disconnect_proc)(struct pw_core *);
// static struct pw_stream *(*pw_stream_new_simple_proc)(struct pw_loop *, const char *, struct pw_properties *, const struct pw_stream_events *, void *);
// static void (*pw_stream_destroy_proc)(struct pw_stream *);
// static int (*pw_stream_connect_proc)(struct pw_stream *, enum pw_direction, uint32_t, enum pw_stream_flags, const struct spa_pod **, uint32_t);
// static enum pw_stream_state (*pw_stream_get_state_proc)(struct pw_stream *stream, const char **error);
// static struct pw_buffer *(*pw_stream_dequeue_buffer_proc)(struct pw_stream *);
// static int (*pw_stream_queue_buffer_proc)(struct pw_stream *, struct pw_buffer *);
// static struct pw_properties *(*pw_properties_new_proc)(const char *, ...)SPA_SENTINEL;
// static int (*pw_properties_set_proc)(struct pw_properties *, const char *, const char *);
// static int (*pw_properties_setf_proc)(struct pw_properties *, const char *, const char *, ...) SPA_PRINTF_FUNC(3, 4);

#endif /* _PIPEWIRE_AUDIO_H */

