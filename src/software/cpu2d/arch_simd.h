/*
 * Copyright 2005-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_SIMD2D_H
#define _AAX_SIMD2D_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif

#include "software/arch.h"

#ifdef __SSE__
#include <xmmintrin.h>
#endif

#ifdef __SSE2__
#include <emmintrin.h>
#endif

#ifdef __SSE3__
#include <pmmintrin.h>
#endif

#ifdef __SSE4__
#include <smmintrin.h>
#endif

#ifdef __AVX__
#include <immintrin.h>
#endif

#include "base/types.h"
#include "base/geometry.h"

#ifdef __MINGW32__
	// Force proper stack alignment for functions that use SSE
# define FN_PREALIGN	__attribute__((force_align_arg_pointer))
#else
# define FN_PREALIGN
#endif

/* CPU*/
void _aax_free_align16(void*);
char* _aax_calloc_align16(char**, unsigned int, unsigned int);
char* _aax_malloc_align16(char**, unsigned int);
void _batch_cvt24_24_cpu(void_ptr, const void*, unsigned int);

void _batch_mul_value_cpu(void*, unsigned, unsigned int, float);
void _batch_fmadd_cpu(int32_ptr, const_int32_ptr, unsigned int, float, float);
void _batch_freqfilter_cpu(int32_ptr, const_int32_ptr, unsigned int, float*, float, float, float, const float*);
void _aaxBufResampleCubic_cpu(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);
void _aaxBufResampleLinear_cpu(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);
void _aaxBufResampleNearest_cpu(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);
void _aaxBufResampleSkip_cpu(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);

void _batch_saturate24_cpu(void*, unsigned int);

void _batch_cvt8u_8s_cpu(void*, unsigned int);
void _batch_cvt8s_8u_cpu(void*, unsigned int);
void _batch_cvt16u_16s_cpu(void*, unsigned int);
void _batch_cvt16s_16u_cpu(void*, unsigned int);
void _batch_cvt24u_24s_cpu(void*, unsigned int);
void _batch_cvt24s_24u_cpu(void*, unsigned int);
void _batch_cvt32u_32s_cpu(void*, unsigned int);
void _batch_cvt32s_32u_cpu(void*, unsigned int);

void _batch_endianswap16_cpu(void*, unsigned int);
void _batch_endianswap32_cpu(void*, unsigned int);
void _batch_endianswap64_cpu(void*, unsigned int);

void _batch_cvt24_8_cpu(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt24_16_cpu(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt24_24_3_cpu(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt24_32_cpu(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt24_ps_cpu(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt24_pd_cpu(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt24_8_intl_cpu(int32_ptrptr, const_void_ptr, int, unsigned int, unsigned int);
void _batch_cvt24_16_intl_cpu(int32_ptrptr, const_void_ptr, int, unsigned int, unsigned int);
void _batch_cvt24_24_3intl_cpu(int32_ptrptr, const_void_ptr, int, unsigned int, unsigned int);
void _batch_cvt24_24_intl_cpu(int32_ptrptr, const_void_ptr, int, unsigned int, unsigned int);
void _batch_cvt24_32_intl_cpu(int32_ptrptr, const_void_ptr, int, unsigned int, unsigned int);
void _batch_cvt24_ps_intl_cpu(int32_ptrptr, const_void_ptr, int, unsigned int, unsigned int);
void _batch_cvt24_pd_intl_cpu(int32_ptrptr, const_void_ptr, int, unsigned int, unsigned int);

void _batch_cvt8_24_cpu(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt16_24_cpu(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt24_3_24_cpu(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt32_24_cpu(void_ptr, const_void_ptr, unsigned int);
void _batch_cvtps_24_cpu(void_ptr, const_void_ptr, unsigned int);
void _batch_cvtpd_24_cpu(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt8_intl_24_cpu(void_ptr, const_int32_ptrptr, int, unsigned int, unsigned int);
void _batch_cvt16_intl_24_cpu(void_ptr, const_int32_ptrptr, int, unsigned int, unsigned int);
void _batch_cvt24_3intl_24_cpu(void_ptr, const_int32_ptrptr, int, unsigned int, unsigned int);
void _batch_cvt24_intl_24_cpu(void_ptr, const_int32_ptrptr, int, unsigned int, unsigned int);
void _batch_cvt32_intl_24_cpu(void_ptr, const_int32_ptrptr, int, unsigned int, unsigned int);
void _batch_cvtps_intl_24_cpu(void_ptr, const_int32_ptrptr, int, unsigned int, unsigned int);
void _batch_cvtpd_intl_24_cpu(void_ptr, const_int32_ptrptr, int, unsigned int, unsigned int);


char* _aax_calloc_align16(char**, unsigned int, unsigned int);
char* _aax_malloc_align16(char**, unsigned int);

/* SSE2*/
void* _aax_memcpy_sse2(void_ptr, const_void_ptr, size_t);

void _batch_fmadd_sse2(int32_ptr, const_int32_ptr, unsigned int, float, float);
void _batch_freqfilter_sse2(int32_ptr, const_int32_ptr, unsigned int, float*, float, float, float, const float*);
void _aaxBufResampleCubic_sse2(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);
void _aaxBufResampleLinear_sse2(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);
void _aaxBufResampleNearest_sse2(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);
void _aaxBufResampleSkip_sse2(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);

void _batch_cvt24_16_sse2(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt24_ps_sse2(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt24_pd_sse2(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt16_24_sse2(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt16_intl_24_sse2(void_ptr, const_int32_ptrptr, int, unsigned int, unsigned int);

/* SSE3 */
void _batch_mul_value_sse3(void*, unsigned, unsigned int, float);

/* AVX */
void _batch_fma3_avx(int32_ptr, const_int32_ptr, unsigned int, float, float);
void _batch_fma4_avx(int32_ptr, const_int32_ptr, unsigned int, float, float);
void _aaxBufResampleCubic_avx(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);
void _aaxBufResampleLinear_avx(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);
void _aaxBufResampleNearest_avx(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);
void _aaxBufResampleSkip_avx(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);


/* NEON */
void _batch_fmadd_neon(int32_t*, const_int32_ptr, unsigned int, float, float);
void _batch_freqfilter_neon(int32_ptr, const_int32_ptr, unsigned int, float*, float, float, float, const float*);
void _aaxBufResampleCubic_neon(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);
void _aaxBufResampleLinear_neon(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);
void _aaxBufResampleNearest_neon(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);
void _aaxBufResampleSkip_neon(int32_ptr, const_int32_ptr, unsigned int, unsigned int, unsigned int, float, float);

void _batch_cvt24_16_neon(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt16_24_neon(void_ptr, const_void_ptr, unsigned int);
void _batch_cvt16_intl_24_neon(void_ptr, const_int32_ptrptr, int, unsigned int, unsigned int);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_SIMD2D_H */

