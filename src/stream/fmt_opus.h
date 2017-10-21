/*
 * Copyright 2016-2017 by Erik Hofman.
 * Copyright 2016-2017. by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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
/* errors */
typedef const char* (*opus_strerror_proc)(int);
typedef const char* (*opus_get_version_string_proc)(void);
typedef const char* (*opus_tags_query_proc)(void*, const char*, int);


#endif /* __FILE_FMT_OPUS_H */

