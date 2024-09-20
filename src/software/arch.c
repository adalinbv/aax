/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>	/* fopen, fclose */
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#endif
#if __MINGW32__
# include <mm_malloc.h>
#endif

#include <base/memory.h>

#include <api.h>
#include <objects.h>
#include <arch.h>
#include <ringbuffer.h>

#include "rbuf_int.h"
#include "cpu/arch2d_simd.h"
#include "cpu/arch3d_simd.h"
#include "audio.h"

_aax_free_proc _aax_free = (_aax_free_proc)_aax_free_aligned;
_aax_calloc_proc _aax_calloc = (_aax_calloc_proc)_aax_calloc_aligned;
_aax_malloc_proc _aax_malloc = (_aax_malloc_proc)_aax_malloc_aligned;

_batch_cvt_from_proc _batch_cvt24_8 = _batch_cvt24_8_cpu;
_batch_cvt_from_proc _batch_cvt24_16 = _batch_cvt24_16_cpu;
_batch_cvt_from_proc _batch_cvt24_24_3 = _batch_cvt24_24_3_cpu;
_batch_cvt_from_proc _batch_cvt24_32 = _batch_cvt24_32_cpu;
_batch_cvt_from_proc _batch_cvt24_ps = _batch_cvt24_ps_cpu;
_batch_cvt_from_proc _batch_cvt24_pd = _batch_cvt24_pd_cpu;
_batch_cvt_from_intl_proc _batch_cvt24_8_intl = _batch_cvt24_8_intl_cpu;
_batch_cvt_from_intl_proc _batch_cvt24_16_intl = _batch_cvt24_16_intl_cpu;
_batch_cvt_from_intl_proc _batch_cvt24_24_3intl = _batch_cvt24_24_3intl_cpu;
_batch_cvt_from_intl_proc _batch_cvt24_24_intl = _batch_cvt24_24_intl_cpu;
_batch_cvt_from_intl_proc _batch_cvt24_32_intl = _batch_cvt24_32_intl_cpu;
_batch_cvt_from_intl_proc _batch_cvt24_ps_intl = _batch_cvt24_ps_intl_cpu;
_batch_cvt_from_intl_proc _batch_cvt24_pd_intl = _batch_cvt24_pd_intl_cpu;

_batch_get_average_rms_proc _batch_get_average_rms = _batch_get_average_rms_cpu;
_batch_cvt_proc _batch_saturate24 = _batch_saturate24_cpu;

_batch_cvt_proc _batch_cvt8u_8s = _batch_cvt8u_8s_cpu;
_batch_cvt_proc _batch_cvt8s_8u = _batch_cvt8s_8u_cpu;
_batch_cvt_proc _batch_cvt16u_16s = _batch_cvt16u_16s_cpu;
_batch_cvt_proc _batch_cvt16s_16u = _batch_cvt16s_16u_cpu;
_batch_cvt_proc _batch_cvt24u_24s = _batch_cvt24u_24s_cpu;
_batch_cvt_proc _batch_cvt24s_24u = _batch_cvt24s_24u_cpu;
_batch_cvt_proc _batch_cvt32u_32s = _batch_cvt32u_32s_cpu;
_batch_cvt_proc _batch_cvt32s_32u = _batch_cvt32s_32u_cpu;

_batch_dither_proc _batch_dither = _batch_dither_cpu;

_batch_cvt_proc _batch_endianswap16 = _batch_endianswap16_cpu;
_batch_cvt_proc _batch_endianswap24 = _batch_endianswap24_cpu;
_batch_cvt_proc _batch_endianswap32 = _batch_endianswap32_cpu;
_batch_cvt_proc _batch_endianswap64 = _batch_endianswap64_cpu;

_batch_cvt_to_proc _batch_cvt8_24 = _batch_cvt8_24_cpu;
_batch_cvt_to_proc _batch_cvt16_24 = _batch_cvt16_24_cpu;
_batch_cvt_to_proc _batch_cvt24_3_24 = _batch_cvt24_3_24_cpu;
_batch_cvt_to_proc _batch_cvt32_24 = _batch_cvt32_24_cpu;
_batch_cvt_to_proc _batch_cvtps_24 = _batch_cvtps_24_cpu;
_batch_cvt_to_proc _batch_cvtpd_24 = _batch_cvtpd_24_cpu;
_batch_cvt_to_proc _batch_cvt24_24 = _batch_cvt24_24_cpu;
_batch_cvt_to_proc _batch_atanps = _batch_atanps_cpu;
_batch_cvt_to_proc _batch_roundps = _batch_roundps_cpu;
_batch_cvt_to_intl_proc _batch_cvt8_intl_24 = _batch_cvt8_intl_24_cpu;
_batch_cvt_to_intl_proc _batch_cvt16_intl_24 = _batch_cvt16_intl_24_cpu;
_batch_cvt_to_intl_proc _batch_cvt24_3intl_24 = _batch_cvt24_3intl_24_cpu;
_batch_cvt_to_intl_proc _batch_cvt24_intl_24 = _batch_cvt24_intl_24_cpu;
_batch_cvt_to_intl_proc _batch_cvt24_intl_ps = _batch_cvt24_intl_ps_cpu;
_batch_cvt_to_intl_proc _batch_cvt32_intl_24 = _batch_cvt32_intl_24_cpu;
_batch_cvt_to_intl_proc _batch_cvtps_intl_24 = _batch_cvtps_intl_24_cpu;
_batch_cvt_to_intl_proc _batch_cvtpd_intl_24 = _batch_cvtpd_intl_24_cpu;

_batch_imadd_proc _batch_imadd = _batch_imadd_cpu;
_batch_fmadd_proc _batch_fmadd = _batch_fmadd_cpu;
_batch_cvt_to_proc _batch_fmul = _batch_fmul_cpu;
_batch_dsp_1param_proc _batch_dc_shift = _batch_dc_shift_cpu;
_batch_dsp_1param_proc _batch_wavefold = _batch_wavefold_cpu;
_batch_mul_value_proc _batch_imul_value = _batch_imul_value_cpu;
_batch_mul_value_proc _batch_fmul_value = _batch_fmul_value_cpu;
_batch_ema_float_proc _batch_allpass_float = _batch_iir_allpass_float_cpu;
_batch_ema_float_proc _batch_movingaverage_float = _batch_ema_iir_float_cpu;
_batch_freqfilter_float_proc _batch_freqfilter_float = _batch_freqfilter_float_cpu;


_batch_cvt_from_proc _batch_cvt24_ps24 = _batch_cvt24_ps24_cpu;
_batch_cvt_to_proc _batch_cvtps24_24 = _batch_cvtps24_24_cpu;
_batch_resample_float_proc _batch_resample_float = _batch_resample_float_cpu;
_batch_convolution_proc _batch_convolution = _batch_convolution_cpu;


/* -------------------------------------------------------------------------- */

void *
_aax_aligned_alloc(size_t size)
{
   void *rv = NULL;

   size = SIZE_ALIGNED(size);

#if __MINGW32__
# if defined(_aligned_malloc)
   rv = _aligned_malloc(size, MEMALIGN);
# else
   rv = _mm_malloc(size, MEMALIGN);
# endif
#elif ISOC11_SOURCE 
   rv = aligned_alloc(MEMALIGN, size);
#elif  _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600
   if (posix_memalign(&rv, MEMALIGN, size) != 0) {
      rv = NULL;
   }
#elif _MSC_VER
   rv = _aligned_malloc(size, MEMALIGN);
#else
   assert(1 == 0);
#endif
   return rv;
}

#if defined(__MINGW32__)
# if defined(_aligned_malloc)
void _simd_free(void *ptr) { _aligned_free(ptr); }
# else
void _simd_free(void *ptr) { _mm_free(ptr); }
# endif
_aax_aligned_free_proc _aax_aligned_free = (_aax_aligned_free_proc)_simd_free;
#elif ISOC11_SOURCE 
_aax_aligned_free_proc _aax_aligned_free = (_aax_aligned_free_proc)free;
#elif  _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600
_aax_aligned_free_proc _aax_aligned_free = (_aax_aligned_free_proc)free;
#elif _MSC_VER
_aax_aligned_free_proc _aax_aligned_free= (_aax_aligned_free_proc)_aligned_free;
#else
# pragma warnig _aax_aligned_alloc needs implementing
_aax_aligned_free_proc _aax_aligned_free = (_aax_aligned_free_proc)free;
#endif

