/*
 * Copyright 2005-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
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

#include <arch.h>

#ifdef __MMX__
#include <mmintrin.h>
#endif

#ifdef __SSE__
#include <xmmintrin.h>
#endif

#ifdef __SSE2__
#include <emmintrin.h>
#endif

#ifdef __SSE3__
#include <pmmintrin.h>
#endif

#ifdef __SSE4A__
#include <ammintrin.h>
#endif

#if defined (__SSE4_2__) || defined (__SSE4_1__)
#include <smmintrin.h>
#endif

#if defined(__AVX__)
#include <immintrin.h>
#include <x86intrin.h>
#endif

#include "base/types.h"
#include "base/geometry.h"

#ifdef __MINGW32__
	// Force proper stack alignment for functions that use SSE
# define FN_PREALIGN	__attribute__((force_align_arg_pointer))
#else
# define FN_PREALIGN
#endif

#define CUBIC_TRESHOLD		0.25f

/* CPU*/
void _aax_free_align16(void*);
char* _aax_calloc_align16(char**, size_t, size_t);
char* _aax_malloc_align16(char**, size_t);
void _batch_cvt24_24_cpu(void_ptr, const void*, size_t);

void _batch_imul_value_cpu(void*, unsigned, size_t, float);
void _batch_fmul_value_cpu(void*, unsigned, size_t, float);
void _batch_imadd_cpu(int32_ptr, const_int32_ptr, size_t, float, float);
void _batch_fmadd_cpu(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_ema_iir_cpu(int32_ptr, const_int32_ptr, size_t, float*, float);
void _batch_freqfilter_iir_cpu(int32_ptr, const_int32_ptr, int, size_t, void*);
void _batch_ema_iir_float_cpu(float32_ptr, const_float32_ptr, size_t, float*, float);
void _batch_freqfilter_iir_float_cpu(float32_ptr, const_float32_ptr, int, size_t, void*);
#if RB_FLOAT_DATA
void _batch_cvt24_ps24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvtps24_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_resample_float_cpu(float32_ptr, const_float32_ptr, size_t, size_t, float, float);
#else
void _batch_resample_cpu(int32_ptr, const_int32_ptr, size_t, size_t, float, float);
#endif

void _batch_saturate24_cpu(void*, size_t);

void _batch_cvt8u_8s_cpu(void*, size_t);
void _batch_cvt8s_8u_cpu(void*, size_t);
void _batch_cvt16u_16s_cpu(void*, size_t);
void _batch_cvt16s_16u_cpu(void*, size_t);
void _batch_cvt24u_24s_cpu(void*, size_t);
void _batch_cvt24s_24u_cpu(void*, size_t);
void _batch_cvt32u_32s_cpu(void*, size_t);
void _batch_cvt32s_32u_cpu(void*, size_t);

void _batch_endianswap16_cpu(void*, size_t);
void _batch_endianswap32_cpu(void*, size_t);
void _batch_endianswap64_cpu(void*, size_t);

void _batch_cvt24_8_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_16_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_24_3_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_pd_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_32_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_8_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_16_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_24_3intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_24_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_32_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_ps_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_pd_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);

void _batch_cvt8_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_3_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt32_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvtps_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvtpd_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt8_intl_24_cpu(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt16_intl_24_cpu(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt24_3intl_24_cpu(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt24_intl_24_cpu(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt32_intl_24_cpu(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvtps_intl_24_cpu(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvtpd_intl_24_cpu(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);


char* _aax_calloc_align16(char**, size_t, size_t);
char* _aax_malloc_align16(char**, size_t);

/* SSE2*/
void* _aax_memcpy_sse2(void_ptr, const_void_ptr, size_t);

void _batch_imadd_sse2(int32_ptr, const_int32_ptr, size_t, float, float);
void _batch_fmadd_sse2(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_freqfilter_sse2(int32_ptr, const_int32_ptr, int, size_t, void*);
void _batch_freqfilter_float_sse2(float32_ptr, const_float32_ptr, int, size_t, void*);
#if RB_FLOAT_DATA
void _batch_cvtps24_24_sse2(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps24_sse2(void_ptr, const_void_ptr, size_t);
void _batch_resample_float_sse2(float32_ptr, const_float32_ptr, size_t, size_t, float, float);
#else
void _batch_resample_sse2(int32_ptr, const_int32_ptr, size_t, size_t, float, float);
#endif

void _batch_cvtps_24_sse2(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps_sse2(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_16_sse2(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_24_sse2(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_intl_24_sse2(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);

/* SSE3 */
void _batch_imul_value_sse3(void*, unsigned, size_t, float);
void _batch_fmul_value_sse3(void*, unsigned, size_t, float);
#if RB_FLOAT_DATA
void _batch_resample_float_sse3(float32_ptr, const_float32_ptr, size_t, size_t, float, float);
#else
void _batch_resample_sse3(int32_ptr, const_int32_ptr, size_t, size_t, float, float);
#endif

/* AVX */
#if RB_FLOAT_DATA
void _batch_resample_float_avx(float32_ptr, const_float32_ptr, size_t, size_t, float, float);
#else
void _batch_resample_avx(int32_ptr, const_int32_ptr, size_t, size_t, float, float);
#endif

void _batch_fma3_avx(int32_ptr, const_int32_ptr, size_t, float, float);
void _batch_fma4_avx(int32_ptr, const_int32_ptr, size_t, float, float);

/* VFPv2 */
void _batch_imul_value_vfpv2(void*, unsigned, size_t, float);
void _batch_fmul_value_vfpv2(void*, unsigned, size_t, float);
void _batch_fmadd_vfpv2(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_freqfilter_vfpv2(int32_ptr, const_int32_ptr, int, size_t, void*);
void _batch_freqfilter_float_vfpv2(float32_ptr, const_float32_ptr, int, size_t, void*);
#if RB_FLOAT_DATA
void _batch_cvt24_ps24_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvtps24_24_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_resample_float_vfpv2(float32_ptr, const_float32_ptr, size_t, size_t, float, float);
#else
void _batch_resample_vfpv2(int32_ptr, const_int32_ptr, size_t, size_t, float, float);
#endif
void _batch_cvt24_ps_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_pd_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps_intl_vfpv2(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_pd_intl_vfpv2(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvtps_24_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvtpd_24_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvtps_intl_24_vfpv2(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvtpd_intl_24_vfpv2(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);

/* VFPv3 */
void _batch_imul_value_vfpv3(void*, unsigned, size_t, float);
void _batch_fmul_value_vfpv3(void*, unsigned, size_t, float);
void _batch_fmadd_vfpv3(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_freqfilter_vfpv3(int32_ptr, const_int32_ptr, int, size_t, void*);
void _batch_freqfilter_float_vfpv3(float32_ptr, const_float32_ptr, int, size_t, void*);
#if RB_FLOAT_DATA
void _batch_cvt24_ps24_vfpv3(void_ptr, const_void_ptr, size_t);
void _batch_cvtps24_24_vfpv3(void_ptr, const_void_ptr, size_t);
void _batch_resample_float_vfpv3(float32_ptr, const_float32_ptr, size_t, size_t, float, float);
#else
void _batch_resample_vfpv3(int32_ptr, const_int32_ptr, size_t, size_t, float, float);
#endif
void _batch_cvt24_ps_vfpv3(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_pd_vfpv3(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps_intl_vfpv3(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_pd_intl_vfpv3(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvtps_24_vfpv3(void_ptr, const_void_ptr, size_t);
void _batch_cvtpd_24_vfpv3(void_ptr, const_void_ptr, size_t);
void _batch_cvtps_intl_24_vfpv3(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvtpd_intl_24_vfpv3(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);

/* NEON */
void _batch_imadd_neon(int32_ptr, const_int32_ptr, size_t, float, float);
void _batch_fmadd_neon(float32_ptr, const_float32_ptr, unsigned in, float, float);
void _batch_freqfilter_neon(int32_ptr, const_int32_ptr, int, size_t, void*);
void _batch_freqfilter_float_neon(float32_ptr, const_float32_ptr, int, size_t, void*);
void _batch_fmul_value_neon(void*, unsigned, size_t, float);

#if RB_FLOAT_DATA
void _batch_cvtps24_24_neon(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps24_neon(void_ptr, const_void_ptr, size_t);
void _batch_resample_float_neon(float32_ptr, const_float32_ptr, size_t, size_t, float, float);
#else
void _batch_resample_neon(int32_ptr, const_int32_ptr, size_t, size_t, float, float);
#endif

void _batch_cvt24_16_neon(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_24_neon(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_intl_24_neon(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_SIMD2D_H */

