/*
 * Copyright 2005-2019 by Erik Hofman.
 * Copyright 2009-2019 by Adalin B.V.
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

// must be a multiple of 4 because of SIMD optimizations
#define MAX_HARMONICS		16
#define CUBIC_TRESHOLD		0.25f

typedef float* (*_aax_generate_waveform_proc)(float*, size_t, float, float, float*);
extern _aax_generate_waveform_proc _aax_generate_waveform_float;

/* CPU*/
void _aax_free_aligned(void*);
char* _aax_calloc_aligned(char**, size_t, size_t, size_t);
char* _aax_malloc_aligned(char**, size_t, size_t);
void _batch_cvt24_24_cpu(void_ptr, const void*, size_t);

float* _aax_generate_waveform_cpu(float*, size_t, float, float, float*);

void _batch_imul_value_cpu(void*, const void*, unsigned, size_t, float);
void _batch_fmul_value_cpu(void*, const void*, unsigned, size_t, float);
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

void _batch_get_average_rms_cpu(const_float32_ptr, size_t, float*, float*);
void _batch_dither_cpu(void*, unsigned, size_t);
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
void _batch_endianswap24_cpu(void*, size_t);
void _batch_endianswap32_cpu(void*, size_t);
void _batch_endianswap64_cpu(void*, size_t);

void _batch_cvt24_8_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_16_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_24_3_cpu(void_ptr, const_void_ptr, size_t);
//void _batch_cvt24_ph_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_pd_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_32_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_8_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_16_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_24_3intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_24_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_32_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
//void _batch_cvt24_ph_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_ps_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_pd_intl_cpu(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);

void _batch_cvt8_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_3_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvt32_24_cpu(void_ptr, const_void_ptr, size_t);
//void _batch_cvtph_24_cpu(void_ptr, const_void_ptr, size_t);
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


/* SSE2*/
float fast_sin_sse2(float);
float* _aax_generate_waveform_sse2(float*, size_t, float, float, float*);
void* _aax_memcpy_sse2(void_ptr, const_void_ptr, size_t);

void _batch_get_average_rms_sse2(const_float32_ptr, size_t, float*, float*);
void _batch_saturate24_sse2(void*, size_t);

void _batch_fmul_value_sse2(void*, const void*, unsigned, size_t, float);
void _batch_imadd_sse2(int32_ptr, const_int32_ptr, size_t, float, float);
void _batch_fmadd_sse2(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_ema_iir_float_sse2(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float a1);
void _batch_freqfilter_sse2(int32_ptr, const_int32_ptr, int, size_t, void*);
void _batch_freqfilter_float_sse2(float32_ptr, const_float32_ptr, int, size_t, void*);
void _batch_roundps_cpu(void_ptr, const_void_ptr, size_t);
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
void _batch_imul_value_sse3(void*, const void*, unsigned, size_t, float);
// void _batch_fmul_value_sse3(void*, unsigned, size_t, float);
#if RB_FLOAT_DATA
void _batch_resample_float_sse3(float32_ptr, const_float32_ptr, size_t, size_t, float, float);
#else
void _batch_resample_sse3(int32_ptr, const_int32_ptr, size_t, size_t, float, float);
#endif

/* SSE4 */
void _batch_roundps_sse4(void_ptr, const_void_ptr, size_t);

/* AVX & SSE/VEX */
float fast_sin_sse_vex(float);
float* _aax_generate_waveform_sse_vex(float*, size_t, float, float, float*);
void _batch_get_average_rms_sse_vex(const_float32_ptr, size_t, float*, float*);
void _batch_ema_iir_float_sse_vex(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float a1);
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
void _batch_fmul_value_avx(void*, const void*, unsigned, size_t, float);
void _batch_fadd_avx(float32_ptr, const_float32_ptr, size_t);
// void _batch_hmadd_avx(float32_ptr, const_float16_ptr, size_t, float, float);
void _batch_fmadd_avx(float32_ptr, const_float32_ptr, size_t, float, float);
#if RB_FLOAT_DATA
void _batch_cvtps24_24_avx(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps24_avx(void_ptr, const_void_ptr, size_t);
#else
#endif

void _batch_cvtps_24_avx(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps_avx(void_ptr, const_void_ptr, size_t);

void _batch_fmadd_fma3(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_fmadd_fma4(float32_ptr, const_float32_ptr, size_t, float, float);

/* VFPv2 */
void _batch_imul_value_vfpv2(void*, const void*, unsigned, size_t, float);
void _batch_fmul_value_vfpv2(void*, const void*, unsigned, size_t, float);
// void _batch_hmadd_vfpv2(float32_ptr, const_float16_ptr, size_t, float, float);
void _batch_fmadd_vfpv2(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_ema_iir_float_vfpv2(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float a1);
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
float* _aax_generate_waveform_vfpv3(float*, size_t, float, float, float*);
void _batch_imul_value_vfpv3(void*, const void*, unsigned, size_t, float);
void _batch_fmul_value_vfpv3(void*, const void*, unsigned, size_t, float);
// void _batch_hmadd_vfpv3(float32_ptr, const_float16_ptr, size_t, float, float);
void _batch_fmadd_vfpv3(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_ema_iir_float_vfpv3(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float a1);
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
float fast_sin_neon(float);
float* _aax_generate_waveform_neon(float*, size_t, float, float, float*);
void _batch_get_average_rms_neon(const_float32_ptr, size_t, float*, float*);
void _batch_imadd_neon(int32_ptr, const_int32_ptr, size_t, float, float);
// void _batch_hmadd_neon(float32_ptr, const_float16_ptr, unsigned in, float, float);
void _batch_fmadd_neon(float32_ptr, const_float32_ptr, unsigned in, float, float);
void _batch_ema_iir_float_neon(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float a1);
void _batch_freqfilter_neon(int32_ptr, const_int32_ptr, int, size_t, void*);
void _batch_freqfilter_float_neon(float32_ptr, const_float32_ptr, int, size_t, void*);
void _batch_fmul_value_neon(void*, const void*, unsigned, size_t, float);

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

