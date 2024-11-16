/*
 * SPDX-FileCopyrightText: Copyright © 2005-2024 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2024 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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
#elif defined(__GNUC__) && defined(__ARM_NEON)
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
#include "waveforms.h"

void _aax_init_SSE(void);
void _aax_init_NEON64(void);

typedef float*(*_aax_generate_waveform_proc)(float32_ptr, size_t, float, float, enum aaxSourceType);
typedef float*(*_aax_generate_noise_proc)(float32_ptr, size_t, uint64_t, unsigned char, float);


extern _aax_generate_waveform_proc _aax_generate_waveform_float;
extern _aax_generate_noise_proc _aax_generate_noise_float;

/* CPU*/
void _batch_cvt24_24_cpu(void_ptr, const void*, size_t);
void _batch_roundps_cpu(void_ptr, const_void_ptr, size_t);
void _batch_atanps_cpu(void_ptr, const_void_ptr, size_t);
void _batch_atan_cpu(void_ptr, const_void_ptr, size_t);

float* _aax_generate_waveform_cpu(float32_ptr, size_t, float, float, enum aaxSourceType);
float* _aax_generate_noise_cpu(float32_ptr, size_t, uint64_t, unsigned char, float);

void _batch_fmul_cpu(void_ptr, const_void_ptr, size_t);
void _batch_imul_value_cpu(void_ptr, const_void_ptr, unsigned, size_t, float);
void _batch_fmul_value_cpu(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_imadd_cpu(int32_ptr, const_int32_ptr, size_t, float, float);
void _batch_fmadd_cpu(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_dc_shift_cpu(float32_ptr, const_float32_ptr, size_t, float);
void _batch_wavefold_cpu(float32_ptr, const_float32_ptr, size_t, float);
void _batch_iir_allpass_float_cpu(float32_ptr, const_float32_ptr, size_t, float*, float);
void _batch_ema_iir_float_cpu(float32_ptr, const_float32_ptr, size_t, float*, float);
void _batch_freqfilter_float_cpu(float32_ptr, const_float32_ptr, int, size_t, void*);
void _batch_cvt24_ps24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_cvtps24_24_cpu(void_ptr, const_void_ptr, size_t);
void _batch_resample_float_cpu(float32_ptr, const_float32_ptr, size_t, size_t, float, float);
void _batch_convolution_cpu(float32_ptr, const_float32_ptr, const_float32_ptr, unsigned int, unsigned int, int, float, float);

void _batch_get_average_rms_cpu(const_float32_ptr, size_t, float*, float*);
void _batch_dither_cpu(int32_t*, unsigned, size_t);
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
void _batch_dc_shift_sse2(float32_ptr, const_float32_ptr, size_t, float);
void _batch_wavefold_sse2(float32_ptr, const_float32_ptr, size_t, float);
float* _aax_generate_waveform_sse2(float32_ptr, size_t, float, float, enum aaxSourceType);
float* _aax_generate_noise_sse2(float32_ptr, size_t, uint64_t, unsigned char, float);

void _batch_get_average_rms_sse2(const_float32_ptr, size_t, float*, float*);
void _batch_saturate24_sse2(void*, size_t);

void _batch_fmul_sse2(void_ptr, const_void_ptr, size_t);
void _batch_fmul_value_sse2(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_imadd_sse2(int32_ptr, const_int32_ptr, size_t, float, float);
void _batch_fmadd_sse2(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_ema_iir_float_sse2(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float a1);
void _batch_freqfilter_float_sse2(float32_ptr, const_float32_ptr, int, size_t, void*);
void _batch_atanps_sse2(void_ptr, const_void_ptr, size_t);
void _batch_cvtps24_24_sse2(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps24_sse2(void_ptr, const_void_ptr, size_t);
void _batch_resample_float_sse2(float32_ptr, const_float32_ptr, size_t, size_t, float, float);

void _batch_cvtps_24_sse2(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps_sse2(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_16_sse2(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_24_sse2(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_intl_24_sse2(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);

/* SSE3 */
void _batch_imul_value_sse3(void*, const void*, unsigned, size_t, float);

/* SSE4 */
void _batch_wavefold_sse4(float32_ptr, const_float32_ptr, size_t, float);

/* SSE/VEX */
float fast_sin_sse_vex(float);
void _batch_dc_shift_sse_vex(float32_ptr, const_float32_ptr, size_t, float);
void _batch_wavefold_sse_vex(float32_ptr, const_float32_ptr, size_t, float);
float* _aax_generate_waveform_sse_vex(float32_ptr, size_t, float, float, enum aaxSourceType);
float* _aax_generate_noise_sse_vex(float32_ptr, size_t, uint64_t, unsigned char, float);

void _batch_get_average_rms_sse_vex(const_float32_ptr, size_t, float*, float*);
void _batch_saturate24_sse_vex(void*, size_t);

void _batch_fmul_sse_vex(void_ptr, const_void_ptr, size_t);
void _batch_fmul_value_sse_vex(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_imadd_sse_vex(int32_ptr, const_int32_ptr, size_t, float, float);
void _batch_fmadd_sse_vex(float32_ptr, const_float32_ptr, size_t, float, float);void _batch_ema_iir_float_sse_vex(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float a1);
void _batch_freqfilter_float_sse_vex(float32_ptr, const_float32_ptr, int, size_t, void*);
void _batch_atanps_sse_vex(void_ptr, const_void_ptr, size_t);
void _batch_cvtps24_24_sse_vex(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps24_sse_vex(void_ptr, const_void_ptr, size_t);
void _batch_resample_float_sse_vex(float32_ptr, const_float32_ptr, size_t, size_t, float, float);

void _batch_cvtps_24_sse_vex(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps_sse_vex(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_16_sse_vex(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_24_sse_vex(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_intl_24_sse_vex(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);

/* AVX */
void _batch_fmul_avx(void_ptr, const_void_ptr, size_t);
void _batch_atanps_avx(void_ptr, const_void_ptr, size_t);
void _batch_fmul_value_avx(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_fmadd_avx(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_fadd_avx(float32_ptr, const_float32_ptr, size_t);
void _batch_dc_shift_avx(float32_ptr, const_float32_ptr, size_t, float);
void _batch_wavefold_avx(float32_ptr, const_float32_ptr, size_t, float);
void _batch_cvtps24_24_avx(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps24_avx(void_ptr, const_void_ptr, size_t);

void _batch_cvtps_24_avx(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps_avx(void_ptr, const_void_ptr, size_t);

float* _aax_generate_waveform_avx(float32_ptr, size_t, float, float, enum aaxSourceType);
float* _aax_generate_noise_avx(float32_ptr, size_t, uint64_t, unsigned char, float);
void _batch_get_average_rms_avx(const_float32_ptr, size_t, float*, float*);

/* FMA3 */
void _batch_fmadd_fma3(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_freqfilter_float_fma3(float32_ptr, const_float32_ptr, int, size_t, void*);
void _batch_resample_float_fma3(float32_ptr, const_float32_ptr, size_t, size_t, float, float);
void _batch_get_average_rms_fma3(const_float32_ptr, size_t, float*, float*);
void _batch_atanps_fma3(void_ptr, const_void_ptr, size_t);
float* _aax_generate_waveform_fma3(float32_ptr, size_t, float, float, enum aaxSourceType);
float* _aax_generate_noise_fma3(float32_ptr, size_t, uint64_t, unsigned char, float);

/* VFPv2 */
void _batch_cvt24_24_vfpv2(void_ptr, const void*, size_t);
void _batch_atanps_vfpv2(void_ptr, const_void_ptr, size_t);

float* _aax_generate_waveform_vfpv2(float32_ptr, size_t, float, float, enum aaxSourceType);
float* _aax_generate_noise_vfpv2(float32_ptr, size_t, uint64_t, unsigned char, float);

void _batch_fmul_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_imul_value_vfpv2(void_ptr, const_void_ptr, unsigned, size_t, float);
void _batch_fmul_value_vfpv2(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_imadd_vfpv2(int32_ptr, const_int32_ptr, size_t, float, float);
void _batch_fmadd_vfpv2(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_ema_iir_float_vfpv2(float32_ptr, const_float32_ptr, size_t, float*, float);
void _batch_freqfilter_float_vfpv2(float32_ptr, const_float32_ptr, int, size_t, void*);
void _batch_cvt24_ps24_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvtps24_24_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_resample_float_vfpv2(float32_ptr, const_float32_ptr, size_t, size_t, float, float);

void _batch_get_average_rms_vfpv2(const_float32_ptr, size_t, float*, float*);
void _batch_dither_vfpv2(int32_t*, unsigned, size_t);
void _batch_saturate24_vfpv2(void*, size_t);

void _batch_cvt8u_8s_vfpv2(void*, size_t);
void _batch_cvt8s_8u_vfpv2(void*, size_t);
void _batch_cvt16u_16s_vfpv2(void*, size_t);
void _batch_cvt16s_16u_vfpv2(void*, size_t);
void _batch_cvt24u_24s_vfpv2(void*, size_t);
void _batch_cvt24s_24u_vfpv2(void*, size_t);
void _batch_cvt32u_32s_vfpv2(void*, size_t);
void _batch_cvt32s_32u_vfpv2(void*, size_t);

void _batch_endianswap16_vfpv2(void*, size_t);
void _batch_endianswap24_vfpv2(void*, size_t);
void _batch_endianswap32_vfpv2(void*, size_t);
void _batch_endianswap64_vfpv2(void*, size_t);

void _batch_cvt24_8_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_16_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_24_3_vfpv2(void_ptr, const_void_ptr, size_t);
//void _batch_cvt24_ph_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_pd_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_32_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_8_intl_vfpv2(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_16_intl_vfpv2(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_24_3intl_vfpv2(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_24_intl_vfpv2(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_32_intl_vfpv2(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
//void _batch_cvt24_ph_intl_vfpv2(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_ps_intl_vfpv2(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_pd_intl_vfpv2(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);

void _batch_cvt8_24_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_24_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_3_24_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvt32_24_vfpv2(void_ptr, const_void_ptr, size_t);
//void _batch_cvtph_24_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvtps_24_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvtpd_24_vfpv2(void_ptr, const_void_ptr, size_t);
void _batch_cvt8_intl_24_vfpv2(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt16_intl_24_vfpv2(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt24_3intl_24_vfpv2(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt24_intl_24_vfpv2(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt24_intl_ps_vfpv2(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt32_intl_24_vfpv2(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvtps_intl_24_vfpv2(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvtpd_intl_24_vfpv2(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);

/* VFPv4 */
void _batch_cvt24_24_vfpv4(void_ptr, const void*, size_t);
void _batch_atanps_vfpv4(void_ptr, const_void_ptr, size_t);

float* _aax_generate_waveform_vfpv4(float32_ptr, size_t, float, float, enum aaxSourceType);
float* _aax_generate_noise_vfpv4(float32_ptr, size_t, uint64_t, unsigned char, float);

void _batch_fmul_vfpv4(void_ptr, const_void_ptr, size_t);
void _batch_imul_value_vfpv4(void_ptr, const_void_ptr, unsigned, size_t, float);
void _batch_fmul_value_vfpv4(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_imadd_vfpv4(int32_ptr, const_int32_ptr, size_t, float, float);
void _batch_fmadd_vfpv4(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_dc_shift_vfpv4(float32_ptr, const_float32_ptr, size_t, float);
void _batch_wavefold_vfpv4(float32_ptr, const_float32_ptr, size_t, float);
void _batch_ema_iir_float_vfpv4(float32_ptr, const_float32_ptr, size_t, float*, float);
void _batch_freqfilter_float_vfpv4(float32_ptr, const_float32_ptr, int, size_t, void*);
void _batch_cvt24_ps24_vfpv4(void_ptr, const_void_ptr, size_t);
void _batch_cvtps24_24_vfpv4(void_ptr, const_void_ptr, size_t);
void _batch_resample_float_vfpv4(float32_ptr, const_float32_ptr, size_t, size_t, float, float);

void _batch_get_average_rms_vfpv4(const_float32_ptr, size_t, float*, float*);
void _batch_dither_vfpv4(int32_t*, unsigned, size_t);
void _batch_saturate24_vfpv4(void*, size_t);

void _batch_cvt8u_8s_vfpv4(void*, size_t);
void _batch_cvt8s_8u_vfpv4(void*, size_t);
void _batch_cvt16u_16s_vfpv4(void*, size_t);
void _batch_cvt16s_16u_vfpv4(void*, size_t);
void _batch_cvt24u_24s_vfpv4(void*, size_t);
void _batch_cvt24s_24u_vfpv4(void*, size_t);
void _batch_cvt32u_32s_vfpv4(void*, size_t);
void _batch_cvt32s_32u_vfpv4(void*, size_t);

void _batch_endianswap16_vfpv4(void*, size_t);
void _batch_endianswap24_vfpv4(void*, size_t);
void _batch_endianswap32_vfpv4(void*, size_t);
void _batch_endianswap64_vfpv4(void*, size_t);

void _batch_cvt24_8_vfpv4(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_16_vfpv4(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_24_3_vfpv4(void_ptr, const_void_ptr, size_t);
//void _batch_cvt24_ph_vfpv4(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps_vfpv4(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_pd_vfpv4(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_32_vfpv4(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_8_intl_vfpv4(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_16_intl_vfpv4(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_24_3intl_vfpv4(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_24_intl_vfpv4(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_32_intl_vfpv4(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
//void _batch_cvt24_ph_intl_vfpv4(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_ps_intl_vfpv4(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);
void _batch_cvt24_pd_intl_vfpv4(int32_ptrptr, const_void_ptr, size_t, unsigned int, size_t);

void _batch_cvt8_24_vfpv4(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_24_vfpv4(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_3_24_vfpv4(void_ptr, const_void_ptr, size_t);
void _batch_cvt32_24_vfpv4(void_ptr, const_void_ptr, size_t);
//void _batch_cvtph_24_vfpv4(void_ptr, const_void_ptr, size_t);
void _batch_cvtps_24_vfpv4(void_ptr, const_void_ptr, size_t);
void _batch_cvtpd_24_vfpv4(void_ptr, const_void_ptr, size_t);
void _batch_cvt8_intl_24_vfpv4(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt16_intl_24_vfpv4(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt24_3intl_24_vfpv4(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt24_intl_24_vfpv4(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt24_intl_ps_vfpv4(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvt32_intl_24_vfpv4(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvtps_intl_24_vfpv4(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);
void _batch_cvtpd_intl_24_vfpv4(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);


/* NEON */
float fast_sin_neon(float);
float* _aax_generate_waveform_neon(float32_ptr, size_t, float, float, enum aaxSourceType);
float* _aax_generate_noise_neon(float32_ptr, size_t, uint64_t, unsigned char, float);
void _batch_get_average_rms_neon(const_float32_ptr, size_t, float*, float*);
void _batch_imadd_neon(int32_ptr, const_int32_ptr, size_t, float, float);
// void _batch_hmadd_neon(float32_ptr, const_float16_ptr, unsigned in, float, float);
void _batch_fmadd_neon(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_dc_shift_neon(float32_ptr, const_float32_ptr, size_t, float);
void _batch_wavefold_neon(float32_ptr, const_float32_ptr, size_t, float);
void _batch_ema_iir_float_neon(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float a1);
void _batch_freqfilter_float_neon(float32_ptr, const_float32_ptr, int, size_t, void*);
void _batch_fmul_value_neon(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_fmul_neon(void_ptr, const_void_ptr, size_t);

void _batch_atanps_neon(void_ptr, const_void_ptr, size_t);
void _batch_cvtps24_24_neon(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps24_neon(void_ptr, const_void_ptr, size_t);
void _batch_resample_float_neon(float32_ptr, const_float32_ptr, size_t, size_t, float, float);

void _batch_cvt24_16_neon(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_24_neon(void_ptr, const_void_ptr, size_t);
void _batch_cvt16_intl_24_neon(void_ptr, const_int32_ptrptr, size_t, unsigned int, size_t);

/* NEON64 */
float* _aax_generate_waveform_neon64(float32_ptr, size_t, float, float, enum aaxSourceType);
float* _aax_generate_noise_neon64(float32_ptr, size_t, uint64_t, unsigned char, float);
void _batch_get_average_rms_neon64(const_float32_ptr, size_t, float*, float*);
void _batch_fmadd_neon64(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_dc_shift_neon64(float32_ptr, const_float32_ptr, size_t, float);
void _batch_wavefold_neon64(float32_ptr, const_float32_ptr, size_t, float);
void _batch_freqfilter_float_neon64(float32_ptr, const_float32_ptr, int, size_t, void*);
void _batch_fmul_value_neon64(float32_ptr, const_float32_ptr, size_t, float, float);
void _batch_fmul_neon64(void_ptr, const_void_ptr, size_t);

void _batch_atanps_neon64(void_ptr, const_void_ptr, size_t);
void _batch_cvtps24_24_neon64(void_ptr, const_void_ptr, size_t);
void _batch_cvt24_ps24_neon64(void_ptr, const_void_ptr, size_t);
void _batch_resample_float_neon64(float32_ptr, const_float32_ptr, size_t, size_t, float, float);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_SIMD2D_H */

