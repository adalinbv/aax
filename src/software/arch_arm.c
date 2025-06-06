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

#ifdef HAVE_UNISTD_H
# include <unistd.h>		// sysconf
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#endif
#if defined(__MINGW32__)
# include <mm_malloc.h>
#endif
#if HAVE_CPU_FEATURES_H
#include <machine/cpu-features.h>
#endif

#include <base/types.h>
#include <base/logging.h>
#include <base/geometry.h>

#include <arch.h>

#include "rbuf_int.h"
#include "cpu/arch2d_simd.h"
#include "cpu/arch3d_simd.h"

#if defined(__ARM_ARCH) || defined(_M_ARM)

#define MAX_CPUINFO	4096

enum {
    AAX_NO_SIMD       = 0,
    AAX_ARCH_VFPV2    = 0x00000001,
    AAX_ARCH_VFPV3    = 0x00000002,
    AAX_ARCH_VFPV4    = 0x00000004,
    AAX_ARCH_NEON     = 0x00000008,
    AAX_ARCH_NEON64   = 0x0000000F,

    AAX_ARCH_HF       = (AAX_ARCH_VFPV3|AAX_ARCH_VFPV4)
};


enum {
   AAX_SIMD_NONE = 0,
   AAX_SIMD_VFPV2 = 1,
   AAX_SIMD_VFPV3,
   AAX_SIMD_VFPV4,
   AAX_SIMD_NEON,
   AAX_SIMD_VFPV4_NEON,
   AAX_SIMD_NEON64,
   AAX_SIMD_VFPV4_NEON64,
   AAX_SIMD_MAX
};

uint32_t _aax_arch_capabilities = AAX_NO_SIMD;
static const char *_aaxArchSIMDSupportString[AAX_SIMD_MAX] =
{
   "FP",
   "VFPv2",
   "VFPv3",
   "VFPv4",
   "Neon",
   "VFPv4/Neon",
   "Neon64",
   "VFPv4/Neon64"
};

// Features    : fastmult vfp neon vfpv3 vfpv4 idiva idivt
char
_aaxArchDetectFeatures()
{
   static char rv = 0;
   static int8_t init = -1;
   if (init)
   {
      FILE *fp;

      init = 0;
      fp = fopen("/proc/cpuinfo", "r");
      if (fp)
      {
	 char cpuinfo[MAX_CPUINFO];
         int res;

         memset(cpuinfo, 0, MAX_CPUINFO);
         res = fread(cpuinfo, 1, MAX_CPUINFO, fp);
         fclose(fp);

         if (res > 0 && res <= MAX_CPUINFO)
         {
            char *features, *ptr;

            cpuinfo[MAX_CPUINFO-1] = '\0';
            features = strstr(cpuinfo, "Features");
            if (features)
            {
               ptr = strchr(features, '\n');
               if (ptr) ptr[0] = '\0';

               ptr = strstr(features, " vfpv4");
               if (ptr && (ptr[6] == ' ' || ptr[6] == '\0'))
               {
                  _aax_arch_capabilities |= AAX_ARCH_VFPV4;
                  rv = AAX_SIMD_VFPV4;
               }

               ptr = strstr(features, " neon");
               if (ptr && (ptr[5] == ' ' || ptr[5] == '\0'))
               {
                  _aax_arch_capabilities |= AAX_ARCH_NEON;
                  if (_aax_arch_capabilities & AAX_ARCH_VFPV4) {
                     rv = AAX_SIMD_VFPV4_NEON;
                  }
                  else rv = AAX_SIMD_NEON;
               }

               ptr = strstr(features, " asimd");
               if (ptr && (ptr[6] == ' ' || ptr[6] == '\0'))
               {
                  _aax_arch_capabilities |= AAX_ARCH_NEON64;
                  rv = AAX_SIMD_VFPV4_NEON64;
               }
            }
         }
      }
   }
   return rv;
}

bool
_aaxArchDetectVFPV4()
{
   _aaxArchDetectFeatures();
   return (_aax_arch_capabilities & AAX_ARCH_VFPV4);
}

bool
_aaxArchDetectNeon()
{
   _aaxArchDetectFeatures();
   return (_aax_arch_capabilities & AAX_ARCH_NEON);
}

bool
_aaxArchDetectNeon64()
{
   _aaxArchDetectFeatures();
   return (_aax_arch_capabilities & AAX_ARCH_NEON64);
}

uint32_t
_aaxGetSIMDSupportLevel()
{
   static bool support_simd256 = false;
   static bool support_simd = false;
   static bool init = true;
   uint32_t rv;

   rv = _aaxArchDetectFeatures();

   if (init)
   {
      int capabilities;

      init = false;

      capabilities = _info ? _info->capabilities :  _aaxGetCapabilities(NULL);
      support_simd = capabilities & 0xF00;
      support_simd256 = capabilities & (AAX_SIMD256|AAX_SIMD256_2);

      if (_aax_arch_capabilities & AAX_ARCH_VFPV4)
      {
//       _batch_imadd = _batch_imadd_vfpv4;
         _batch_fmadd = _batch_fmadd_vfpv4;
         _batch_fmul = _batch_fmul_vfpv4;
         _batch_imul_value = _batch_imul_value_vfpv4;
         _batch_fmul_value = _batch_fmul_value_vfpv4;

         _batch_dc_shift = _batch_dc_shift_vfpv4;
         _batch_wavefold = _batch_wavefold_vfpv4;
	 _aax_generate_waveform_float = _aax_generate_waveform_vfpv4;
         _aax_generate_noise_float = _aax_generate_noise_vfpv4;

//       _batch_cvt24_24 = _batch_cvt24_24_vfpv4;
//       _batch_cvt24_32 = _batch_cvt24_32_vfpv4;
//       _batch_cvt32_24 = _batch_cvt32_24_vfpv4;
         _batch_cvt24_ps24 = _batch_cvt24_ps24_vfpv4;
         _batch_cvtps24_24 = _batch_cvtps24_24_vfpv4;
         _batch_limit = _batch_limit_vfpv4;
         _batch_atanps = _batch_atanps_vfpv4;
         _batch_cvt24_ps = _batch_cvt24_ps_vfpv4;
         _batch_cvtps_24 = _batch_cvtps_24_vfpv4;
         _batch_cvt24_pd = _batch_cvt24_pd_vfpv4;
//       _batch_cvt24_8_intl = _batch_cvt24_8_intl_vfpv4;
//       _batch_cvt24_24_intl = _batch_cvt24_24_intl_vfpv4;
//       _batch_cvt24_24_3intl = _batch_cvt24_24_3intl_vfpv4;
//       _batch_cvt24_32_intl = _batch_cvt24_32_intl_vfpv4;
         _batch_cvt24_ps_intl = _batch_cvt24_ps_intl_vfpv4;
         _batch_cvt24_pd_intl = _batch_cvt24_pd_intl_vfpv4;
         _batch_cvtpd_24 = _batch_cvtpd_24_vfpv4;
//       _batch_cvt24_8 = _batch_cvt24_8_vfpv4;
//       _batch_cvt24_16 = _batch_cvt24_16_vfpv4;
//       _batch_cvt24_24_3 = _batch_cvt24_24_3_vfpv4;
//       _batch_cvt8_24 = _batch_cvt8_24_vfpv4;
//       _batch_cvt16_24 = _batch_cvt16_24_vfpv4;
//       _batch_cvt24_3_24 = _batch_cvt24_3_24_vfpv4;
//       _batch_cvt8_intl_24 = _batch_cvt8_intl_24_vfpv4;
//       _batch_cvt16_intl_24 = _batch_cvt16_intl_24_vfpv4;
//       _batch_cvt24_3intl_24 = _batch_cvt24_3intl_24_vfpv4;
//       _batch_cvt24_intl_24 = _batch_cvt24_intl_24_vfpv4;
         _batch_cvt24_intl_ps = _batch_cvt24_intl_ps_vfpv4;
//       _batch_cvt32_intl_24 = _batch_cvt32_intl_24_vfpv4;
         _batch_cvtps_intl_24 = _batch_cvtps_intl_24_vfpv4;
         _batch_cvtpd_intl_24 = _batch_cvtpd_intl_24_vfpv4;
         _batch_get_average_rms = _batch_get_average_rms_vfpv4;
         _batch_saturate24 = _batch_saturate24_vfpv4;
//       _batch_cvt8u_8s = _batch_cvt8u_8s_vfpv4;
//       _batch_cvt8s_8u = _batch_cvt8s_8u_vfpv4;
//       _batch_cvt16u_16s = _batch_cvt16u_16s_vfpv4;
//       _batch_cvt16s_16u = _batch_cvt16s_16u_vfpv4;
//       _batch_cvt24u_24s = _batch_cvt24u_24s_vfpv4;
//       _batch_cvt24s_24u = _batch_cvt24s_24u_vfpv4;
//       _batch_cvt32u_32s = _batch_cvt32u_32s_vfpv4;
//       _batch_cvt32s_32u = _batch_cvt32s_32u_vfpv4;
         _batch_dither = _batch_dither_vfpv4;
//       _batch_endianswap16 = _batch_endianswap16_vfpv4;
//       _batch_endianswap24 = _batch_endianswap24_vfpv4;
//       _batch_endianswap32 = _batch_endianswap32_vfpv4;
//       _batch_endianswap64 = _batch_endianswap64_vfpv4;
         _batch_movingaverage_float = _batch_ema_iir_float_vfpv4;
         _batch_freqfilter_float = _batch_freqfilter_float_vfpv4;
         _batch_resample_float = _batch_resample_float_vfpv4;

//       vec3fAdd = _vec3fAdd_vfpv4;
//       vec3fDevide = _vec3fDevide_vfpv4;
         vec3fMulVec3 = _vec3fMulVec3_vfpv4;
//       vec3fSub = _vec3fSub_vfpv4;

         vec3fMagnitude = _vec3fMagnitude_vfpv4;
         vec3dMagnitude = _vec3dMagnitude_vfpv4;
         vec3fMagnitudeSquared = _vec3fMagnitudeSquared_vfpv4;
         vec3fDotProduct = _vec3fDotProduct_vfpv4;
         vec3dDotProduct = _vec3dDotProduct_vfpv4;
         vec3fNormalize = _vec3fNormalize_vfpv4;
         vec3dNormalize = _vec3dNormalize_vfpv4;
         vec3fCrossProduct = _vec3fCrossProduct_vfpv4;

//       vec4fAdd = _vec4fAdd_vfpv4;
//       vec4fDevide = _vec4fDevide_vfpv4;
         vec4fMulVec4 = _vec4fMulVec4_vfpv4;
//       vec4fSub = _vec4fSub_vfpv4;
         mtx4fMulVec4 = _mtx4fMulVec4_vfpv4;
         mtx4dMulVec4 = _mtx4dMulVec4_vfpv4;
         mtx4fMul = _mtx4fMul_vfpv4;
         mtx4dMul = _mtx4dMul_vfpv4;

//       vec4iDevide = _vec4iDevide_vfpv4;
      }

      if (support_simd && (_aax_arch_capabilities & (AAX_ARCH_NEON|AAX_ARCH_NEON64)))
      {
	 mtx4fMul = _mtx4fMul_neon;
         mtx4fMulVec4 = _mtx4fMulVec4_neon;

         vec3fMagnitude = _vec3fMagnitude_neon;
         vec3fMagnitudeSquared = _vec3fMagnitudeSquared_neon;
         vec3fDotProduct = _vec3fDotProduct_neon;
         vec3fCrossProduct = _vec3fCrossProduct_neon;

//       vec4fAdd = _vec4fAdd_neon;
//       vec4fSub = _vec4fSub_neon;
         vec4fCopy = _vec4fCopy_neon;
//       vec4fDevide = _vec4fDevide_neon;
         vec4fMulVec4 = _vec4fMulVec4_neon;
         mtx4fMulVec4 = _mtx4fMulVec4_neon;
         mtx4fMul = _mtx4fMul_neon;

         _batch_cvt24_16 = _batch_cvt24_16_neon;
         _batch_cvt16_24 = _batch_cvt16_24_neon;

# if defined(__arm64__) || defined(__aarch64__)
         _aax_init_NEON64();

         mtx4dMul = _mtx4dMul_neon64;
         mtx4dMulVec4 = _mtx4dMulVec4_neon64;
         vec3dAltitudeVector = _vec3dAltitudeVector_neon64;

         vec3fMagnitude = _vec3fMagnitude_neon; // 64
         vec3fMagnitudeSquared = _vec3fMagnitudeSquared_neon; // 64
         vec3fDotProduct = _vec3fDotProduct_neon; // 64
         vec3fCrossProduct = _vec3fCrossProduct_neon; // 64

//       vec4fAdd = _vec4fAdd_neon64;
//       vec4fSub = _vec4fSub_neon64;
         vec4fCopy = _vec4fCopy_neon; // 64
//       vec4fDevide = _vec4fDevide_neon64;
         vec4fMulVec4 = _vec4fMulVec4_neon; // 64
         mtx4fMulVec4 = _mtx4fMulVec4_neon; // 64
         mtx4fMul = _mtx4fMul_neon; // 64

         _batch_cvt24_16 = _batch_cvt24_16_neon; // 64
         _batch_cvt16_24 = _batch_cvt16_24_neon; // 64
# endif

         _batch_get_average_rms = _batch_get_average_rms_neon;
         _batch_freqfilter_float = _batch_freqfilter_float_neon;

  	 _batch_fmadd = _batch_fmadd_neon;
         _batch_limit = _batch_limit_neon;
  	 _batch_atanps = _batch_atanps_neon;
         _batch_fmul = _batch_fmul_neon;
	 _batch_fmul_value = _batch_fmul_value_neon;

         _aax_generate_waveform_float = _aax_generate_waveform_neon;
         _aax_generate_noise_float = _aax_generate_noise_neon;
//       _batch_dc_shift = _batch_dc_shift_neon;
//       _batch_wavefold = _batch_wavefold_neon;

         _batch_cvtps24_24 = _batch_cvtps24_24_neon;
         _batch_cvt24_ps24 = _batch_cvt24_ps24_neon;

# if defined(__arm64__) || defined(__aarch64__)
         _batch_get_average_rms = _batch_get_average_rms_neon64;
         _batch_freqfilter_float = _batch_freqfilter_float_neon64;

  	 _batch_fmadd = _batch_fmadd_neon64;
         _batch_limit = _batch_limit_neon64;
  	 _batch_atanps = _batch_atanps_neon64;
         _batch_fmul = _batch_fmul_neon64;
         _batch_fmul_value = _batch_fmul_value_neon64;

         _aax_generate_waveform_float = _aax_generate_waveform_neon64;
         _aax_generate_noise_float = _aax_generate_noise_neon64;
//       _batch_dc_shift = _batch_dc_shift_neon64;
//       _batch_wavefold = _batch_wavefold_neon64;

         _batch_cvtps24_24 = _batch_cvtps24_24_neon64;
         _batch_cvt24_ps24 = _batch_cvt24_ps24_neon64;
# endif
      }
   }

   return rv;
}

const char *
_aaxGetSIMDSupportString()
{
   uint32_t level = _aaxGetSIMDSupportLevel();
   return _aaxArchSIMDSupportString[level];
}

/* -------------------------------------------------------------------------- */

unsigned int
_aaxGetNoCores()
{
   unsigned int cores = 1;

# if defined(__MACH__)
   sysctlbyname("hw.physicalcpu", &cores, sizeof(cores), NULL, 0);

# elif defined(freebsd) || defined(bsdi) || defined(__darwin__) || defined(openbsd)
   size_t len = sizeof(cores);
   int mib[4];

   mib[0] = CTL_HW;
   mib[1] = HW_AVAILCPU;
   sysctl(mib, 2, &cores, &len, NULL, 0);
   if(cores < 1)
   {
      mib[1] = HW_NCPU;
      sysctl(mib, 2, &cores, &len, NULL, 0);
   }
# elif defined(__linux__) || (__linux) || (defined (__SVR4) && defined (__sun))
   // also for AIX
   cores = sysconf( _SC_NPROCESSORS_ONLN );

# elif defined(IRIX)
   cores = sysconf( _SC_NPROC_ONLN );

# elif defined(HPUX)
   cores = mpctl(MPC_GETNUMSPUS, NULL, NULL);

# elif defined( WIN32 )
   SYSTEM_INFO sysinfo;

   GetSystemInfo( &sysinfo );
   cores = sysinfo.dwNumberOfProcessors;
# endif
   return (cores > 0) ? cores : 1;
}

#else // defined(__ARM_ARCH) || defined(_M_ARM)

bool
_aaxArchDetectVFPV4() {
   return false;
}

bool
_aaxArchDetectNeon() {
   return false;
}

bool
_aaxArchDetectNeon64() {
   return false;
}

#endif // defined(_ARM_ARCH) || defined(_M_ARM)
