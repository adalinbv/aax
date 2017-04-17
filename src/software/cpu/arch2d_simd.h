/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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
#if defined(_MSC_VER)
# include <intrin.h>
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
# include <x86intrin.h>
#elif defined(__GNUC__) && defined(__ARM_NEON__)
# include <arm_neon.h>
#endif

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif

#include <arch.h>

#include "base/types.h"
#include "base/geometry.h"

#ifdef __MINGW32__
	// Force proper stack alignment for functions that use SSE
# define FN_PREALIGN	__attribute__((force_align_arg_pointer))
#else
# define FN_PREALIGN
#endif

#define CUBIC_TRESHOLD		0.25f

int16_t fp16_offsettable[64];
int32_t fp16_exponenttable[64];
int32_t fp16_mantissatable[2048];
int16_t fp16_basetable[512];
int16_t fp16_shifttable[512];

inline float HALF2FLOAT(int16_t h16) {
   return fp16_mantissatable[fp16_offsettable[h16>>10]+(h16&0x3ff)]+fp16_exponenttable[h16>>10];
}

inline int16_t FLOAT2HALF(float f) {
   int32_t f32 = ((union { float f; int32_t i; }){ .f = f }).i;
   return fp16_basetable[(f32>>23)&0x1ff]+((f32&0x007fffff)>>fp16_shifttable[(f32>>23)&0x1ff]);
}

/* CPU*/
void _aax_free_aligned(void*);
char* _aax_calloc_aligned(char**, size_t, size_t);
char* _aax_malloc_aligned(char**, size_t);
void _batch_cvt24_24_cpu(void_ptr, const void*, size_t);

void _batch_imul_value_cpu(void*, unsigned, size_t, float);
void _batch_fmul_value_cpu(void*, unsigned, size_t, float);
void _batch_imadd_cpu(int32_ptr, const_int32_ptr, size_t, float, float);
void _batch_hmadd_cpu(float32_ptr, const_float16_ptr, size_t, float, float);
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
void _batch_cvt24_ph_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_pd_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_32_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_8_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_16_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_24_3intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_24_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_32_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_ph_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_ps_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_pd_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);

void _batch_cvt8_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_3_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt32_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvtph_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvtps_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvtpd_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt8_intl_24_cpu(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt16_intl_24_cpu(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt24_3intl_24_cpu(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt24_intl_24_cpu(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt24_intl_ps_cpu(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt32_intl_24_cpu(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvtps_intl_24_cpu(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvtpd_intl_24_cpu(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);


char* _aax_calloc_aligned(char**, size_t, size_t);
char* _aax_malloc_aligned(char**, size_t);

/* SSE2*/
void* _aax_memcpy_sse2(void_ptr, const_void_ptr, size_t);

void _batch_fmul_value_sse2(void*, unsigned, size_t, float);
void _batch_imadd_sse2(int32_ptr, const_int32_ptr, size_t, float, float);
void _batch_hmadd_sse2(float32_ptr, const_float16_ptr, size_t, float, float);
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
// void _batch_fmul_value_sse3(void*, unsigned, size_t, float);
#if RB_FLOAT_DATA
void _batch_resample_float_sse3(float32_ptr, const_float32_ptr, size_t, size_t, float, float);
#else
void _batch_resample_sse3(int32_ptr, const_int32_ptr, size_t, size_t, float, float);
#endif

/* AVX & SSE/VEX */
void _batch_fadd_sse_vex(float32_ptr, const_float32_ptr, size_t);
void _batch_hmadd_sse_vex(float32_ptr, const_float16_ptr, size_t, float, float);
void _batch_fmadd_sse_vex(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_freqfilter_sse_vex(int32_ptr, const_int32_ptr, int, size_t, void*);
void _batch_freqfilter_float_sse_vex(float32_ptr, const_float32_ptr, int, size_t, void*);
#if RB_FLOAT_DATA
void _batch_resample_float_sse_vex(float32_ptr, const_float32_ptr, size_t, size_t, float, float);
#else
void _batch_resample_sse_vex(int32_ptr, const_int32_ptr, size_t, size_t, float, float);
#endif

void _batch_cvt24_ps_sse_vex(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps24_sse_vex(void_ptr, const_void_ptr, size_t);
void _batch_cvtps24_24_sse_vex(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps24_sse_vex(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_16_sse_vex(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_24_sse_vex(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_intl_24_sse_vex(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);

void* _aax_memcpy_avx(void_ptr, const_void_ptr, size_t);
void _batch_fmul_value_avx(void*, unsigned, size_t, float);
void _batch_fadd_avx(float32_ptr, const_float32_ptr, size_t);
void _batch_hmadd_avx(float32_ptr, const_float16_ptr, size_t, float, float);
void _batch_fmadd_avx(float32_ptr, const_float32_ptr, size_t, float, float);
#if RB_FLOAT_DATA
void _batch_cvtps24_24_avx(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps24_avx(void_ptr, const_void_ptr, size_t);
#else
#endif

void _batch_cvtps_24_avx(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps_avx(void_ptr, const_void_ptr, size_t);

void _batch_fma3_float_avx(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_fma4_float_avx(float32_ptr, const_float32_ptr, size_t, float, float);

/* VFPv2 */
void _batch_imul_value_vfpv2(void*, unsigned, size_t, float);
void _batch_fmul_value_vfpv2(void*, unsigned, size_t, float);
void _batch_hmadd_vfpv2(float32_ptr, const_float16_ptr, size_t, float, float);
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
void _batch_hmadd_vfpv3(float32_ptr, const_float16_ptr, size_t, float, float);
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
void _batch_hmadd_neon(float32_ptr, const_float16_ptr, unsigned in, float, float);
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

