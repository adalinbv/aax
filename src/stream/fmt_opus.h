/*
 * Copyright 2016-2017 by Erik Hofman.
 * Copyright 2016-2017. by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef __FILE_FMT_OPUS_H
#define __FILE_FMT_OPUS_H 1

/* opus */
typedef void* (*opus_decoder_create_proc)(int32_t, int, int*);
typedef void (*opus_decoder_destroy_proc)(void*);
typedef int (*opus_decode_float_proc)(void*, const unsigned char*, int32_t, float*, int, int);
typedef int (*opus_decode_proc)(void*, const unsigned char*, int32_t, int16_t*, int, int);

typedef void* (*opus_encoder_create_proc)(int32_t, int, int*);
typedef void (*opus_encoder_destroy_proc)(void*);
typedef int (*opus_encoder_ctl_proc) (void*, int, ...);
typedef int32_t (*opus_encode_proc)(void*, const int16_t*, int, unsigned char*, int32_t);

typedef const char* (*opus_strerror_proc)(int);
typedef const char* (*opus_get_version_string_proc)(void);
typedef const char* (*opus_tags_query_proc)(void*, const char*, int);


#endif /* __FILE_FMT_OPUS_H */

