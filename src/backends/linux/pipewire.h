/*
 * SPDX-FileCopyrightText: Copyright © 2022-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2022-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef _PIPEWIRE_AUDIO_H
#define _PIPEWIRE_AUDIO_H 1

#include <pipewire/pipewire.h>

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

enum PW_READY_FLAGS
{
    PW_READY_FLAG_BUFFER_ADDED = 0x1,
    PW_READY_FLAG_STREAM_READY = 0x2,
    PW_READY_FLAG_ALL_BITS     = 0x3
};

typedef void (*pw_init_proc)(int*, char **);
typedef void (*pw_deinit_proc)(void);
typedef struct pw_thread_loop *(*pw_thread_loop_new_proc)(const char*, const struct spa_dict *);
typedef void (*pw_thread_loop_destroy_proc)(struct pw_thread_loop *);
typedef void (*pw_thread_loop_stop_proc)(struct pw_thread_loop *);
typedef struct pw_loop *(*pw_thread_loop_get_loop_proc)(struct pw_thread_loop *);
typedef void (*pw_thread_loop_lock_proc)(struct pw_thread_loop *);
typedef void (*pw_thread_loop_unlock_proc)(struct pw_thread_loop *);
typedef void (*pw_thread_loop_signal_proc)(struct pw_thread_loop*, bool);
typedef void (*pw_thread_loop_wait_proc)(struct pw_thread_loop *);
typedef int (*pw_thread_loop_start_proc)(struct pw_thread_loop *);

typedef struct pw_context *(*pw_context_new_proc)(struct pw_loop*, struct pw_properties*, size_t);
typedef void (*pw_context_destroy_proc)(struct pw_context *);
typedef struct pw_core *(*pw_context_connect_proc)(struct pw_context*, struct pw_properties*, size_t);

typedef void (*pw_proxy_add_object_listener_proc)(struct pw_proxy*, struct spa_hook*, const void*, void *);
typedef void *(*pw_proxy_get_user_data_proc)(struct pw_proxy *);
typedef void (*pw_proxy_destroy_proc)(struct pw_proxy *);

typedef int (*pw_core_disconnect_proc)(struct pw_core *);

typedef struct pw_stream *(*pw_stream_new_simple_proc)(struct pw_loop*, const char*, struct pw_properties*, const struct pw_stream_events*, void *);
typedef void (*pw_stream_destroy_proc)(struct pw_stream *);
typedef int (*pw_stream_connect_proc)(struct pw_stream*, enum pw_direction, uint32_t, enum pw_stream_flags, const struct spa_pod **, uint32_t);
typedef int (*pw_stream_set_active_proc)(struct pw_stream*, bool);
typedef enum pw_stream_state (*pw_stream_get_state_proc)(struct pw_stream *stream, const char **error);
typedef struct pw_buffer *(*pw_stream_dequeue_buffer_proc)(struct pw_stream *);
typedef int (*pw_stream_queue_buffer_proc)(struct pw_stream*, struct pw_buffer *);
typedef const struct pw_stream_control* (*pw_stream_get_control_proc)(struct pw_stream*, uint32_t);
typedef int (*pw_stream_set_control_proc)(struct pw_stream*, uint32_t, uint32_t, float*, ...);

typedef struct pw_properties *(*pw_properties_new_proc)(const char*, ...)SPA_SENTINEL;
typedef const char* (*pw_properties_get_proc)(const struct pw_properties*, const char*);
typedef int (*pw_properties_set_proc)(struct pw_properties*, const char*, const char *);
typedef int (*pw_properties_setf_proc)(struct pw_properties*, const char*, const char*, ...) SPA_PRINTF_FUNC(3, 4);

typedef const char* (*pw_get_library_version_proc)(void);
typedef const char* (*pw_get_application_name_proc)(void);
typedef const char* (*pw_get_prgname_proc)(void);

#endif /* _PIPEWIRE_AUDIO_H */

