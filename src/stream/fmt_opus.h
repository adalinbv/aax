/*
 * SPDX-FileCopyrightText: Copyright © 2016-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2016-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef __FILE_FMT_OPUS_H
#define __FILE_FMT_OPUS_H 1

#define OPUS_GET_LAST_PACKET_DURATION_REQUEST 4039
#define __opus_check_int_ptr(ptr) ((ptr) + ((ptr) - (int32_t*)(ptr)))
#define OPUS_GET_LAST_PACKET_DURATION(x) OPUS_GET_LAST_PACKET_DURATION_REQUEST, __opus_check_int_ptr(x)

#define OPUS_OK			 0
#define OPUS_BAD_ARG 		-1
#define OPUS_BUFFER_TOO_SMALL	-2
#define OPUS_INTERNAL_ERROR	-3
#define OPUS_INVALID_PACKET	-4
#define OPUS_UNIMPLEMENTED	-5
#define OPUS_INVALID_STATE	-6
#define OPUS_ALLOC_FAIL		-7

/* decode */
typedef void* (*opus_decoder_create_proc)(int32_t, int, int*);
typedef void (*opus_decoder_destroy_proc)(void*);
typedef int (*opus_decoder_ctl_proc)(void*, int, ...);

typedef int (*opus_decode_proc)(void*, const unsigned char*, int, int16_t*, int, int);
typedef int (*opus_decode_float_proc)(void*, const unsigned char*, int, float*, int, int);

/* multistream decode */
typedef void* (*opus_multistream_decoder_create_proc)(int32_t, int, int, int, const unsigned char*, int*);
typedef void (*opus_multistream_decoder_destroy_proc)(void*);
typedef int (*opus_multistream_decoder_ctl_proc)(void*, int, ...);

typedef int (*opus_multistream_decode_proc)(void*, const unsigned char*, int, int16_t*, int, int);
typedef int (*opus_multistream_decode_float_proc)(void*, const unsigned char*, int, float*, int, int);

/* encode */
typedef void* (*opus_encoder_create_proc)(int32_t, int, int*);
typedef void (*opus_encoder_destroy_proc)(void*);
typedef int (*opus_encoder_ctl_proc) (void*, int, ...);
typedef int32_t (*opus_encode_proc)(void*, const int16_t*, int, unsigned char*, int32_t);
typedef int32_t (*opus_encode_float_proc)(void*, const int16_t*, int, unsigned char*, int32_t);

/* errors */
typedef const char* (*opus_strerror_proc)(int);
typedef const char* (*opus_get_version_string_proc)(void);
typedef const char* (*opus_tags_query_proc)(void*, const char*, int);


#endif /* __FILE_FMT_OPUS_H */

