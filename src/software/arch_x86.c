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

#include <base/memory.h>
#include <base/types.h>
#include <base/logging.h>
#include <base/geometry.h>

#include <arch.h>

#include "rbuf_int.h"
#include "cpu/arch2d_simd.h"
#include "cpu/arch3d_simd.h"

#if !defined(__i386__) && defined(_M_IX86)
# define __i386__
#endif
#if !defined(__x86_64__) && defined(_M_X64)
# define __x86_64__
#endif

#if defined(__i386__) || defined(__x86_64__)

#define DMAc		0x444d4163
#define htuA		0x68747541
#define itne		0x69746e65

#define CPUID_GETVENDORSTRING	0
#define CPUID_GETFEATURES	1
#define CPUID_GETEXTFEATURES	0x7
#define CPUID_GETEXTCPUINFO	0x80000000

// https://msdn.microsoft.com/en-us/library/hskdteyh.aspx
enum {
    CPUID_FEAT_EBX_AVX2        	= 1 << 5,
    CPUID_FEAT_EBX_AVX512F      = 1 << 16, //AVX-512 Foundation
    CPUID_FEAT_EBX_AVX512PF     = 1 << 26,
    CPUID_FEAT_EBX_AVX512ER     = 1 << 27,
    CPUID_FEAT_EBX_AVX512CD     = 1 << 28,

    CPUID_FEAT_ECX_SSE3         = 1 << 0,
    CPUID_FEAT_ECX_SSE4a        = 1 << 6,
    CPUID_FEAT_ECX_SSSE3        = 1 << 9,
    CPUID_FEAT_ECX_FMA3         = 1 << 12,
    CPUID_FEAT_ECX_CX16         = 1 << 13,
    CPUID_FEAT_ECX_SSE4_1       = 1 << 19,
    CPUID_FEAT_ECX_SSE4_2       = 1 << 20,
    CPUID_FEAT_ECX_POPCNT       = 1 << 23,
    CPUID_FEAT_ECX_AVX          = 1 << 28,
    CPUID_FEAT_ECX_F16C		= 1 << 29,

    CPUID_FEAT_EDX_CX8          = 1 << 8,
    CPUID_FEAT_EDX_CMOV         = 1 << 15,
    CPUID_FEAT_EDX_MMX          = 1 << 23,
    CPUID_FEAT_EDX_FXSR         = 1 << 24,
    CPUID_FEAT_EDX_SSE          = 1 << 25,
    CPUID_FEAT_EDX_SSE2         = 1 << 26,
    CPUID_FEAT_EDX_HT           = 1 << 28
};

enum {
    AAX_NO_SIMD      = 0,

    AAX_ARCH_MMX     = 0x00000001,
    AAX_ARCH_SSE     = 0x00000002,
    AAX_ARCH_SSE2    = 0x00000004,
    AAX_ARCH_SSE3    = 0x00000008,
    AAX_ARCH_SSSE3   = 0x00000010,
    AAX_ARCH_SSE4A   = 0x00000020,
    AAX_ARCH_SSE41   = 0x00000040,
    AAX_ARCH_SSE42   = 0x00000080,
    AAX_ARCH_AVX     = 0x00000100,
    AAX_ARCH_AVX2    = 0x00000200,
    AAX_ARCH_FMA3    = 0x00000400,
    AAX_ARCH_AVX512  = 0x00000800
};

enum {
   AAX_SIMD_NONE = 0,
   AAX_SIMD_MMX,
   AAX_SIMD_SSE,
   AAX_SIMD_SSE2,
   AAX_SIMD_SSE3,
   AAX_SIMD_SSSE3,
   AAX_SIMD_SSE4A,
   AAX_SIMD_SSE41,
   AAX_SIMD_SSE42,
   AAX_SIMD_AVX,
   AAX_SIMD_AVX2,
   AAX_SIMD_FMA3,
   AAX_SIMD_AVX512,
   AAX_SIMD_MAX
};

static uint32_t _aax_arch_capabilities = AAX_NO_SIMD;
static const char *_aaxArchSIMDSupportString[AAX_SIMD_MAX] =
{
   "FP",
   "MMX",
   "SSE",
   "SSE2",
   "SSE3",
   "SSSE3",
   "SSE4a",
   "SSE4.1",
   "SSE4.2",
   "SSE/AVX",
   "SSE/AVX2",
   "SSE/FMA3"
};

static char check_cpuid_ebx(unsigned int);
char check_cpuid_ecx(unsigned int);
char check_extcpuid_ecx(unsigned int);
# ifndef __x86_64__
static char check_cpuid_edx(unsigned int);
# endif

char
_aaxArchDetectSSE()
{
# ifdef __x86_64__
   static char res = AAX_SIMD_SSE;
# else
   static char res = 0;
   static int8_t init = -1;
   if (init)
   {
      init = 0;
      res = check_cpuid_edx(CPUID_FEAT_EDX_SSE) ? AAX_SIMD_SSE : 0;
   }
# endif
   if (res) _aax_arch_capabilities |= AAX_ARCH_SSE;
   return res;
}

char
_aaxArchDetectSSE2()
{
# ifdef __x86_64__
   static char res = AAX_SIMD_SSE2;
# else
   static char res = 0;
   static int8_t init = -1;
   if (init)
   {
      init = 0;
      res = check_cpuid_edx(CPUID_FEAT_EDX_SSE2) ? AAX_SIMD_SSE2 : 0;
   }
# endif
   if (res) _aax_arch_capabilities |= AAX_ARCH_SSE2;
   return res;
}

char
_aaxArchDetectSSE3()
{
   static uint32_t res = AAX_SIMD_SSE3;
   static int8_t init = -1;
   if (init)
   {
      init = 0;
      res = check_cpuid_ecx(CPUID_FEAT_ECX_SSE3) ? AAX_SIMD_SSE3 : 0;
      if (res)
      {
         _aax_arch_capabilities |= AAX_ARCH_SSE3;
         if (check_cpuid_ecx(CPUID_FEAT_ECX_SSSE3))
         {
             _aax_arch_capabilities |= AAX_ARCH_SSSE3;
             res = AAX_SIMD_SSSE3;
         }
      }
   }
   return res;
}

char
_aaxArchDetectSSE4()
{
   static uint32_t res = 0;
   static int8_t init = -1;
   if (init)
   {
      init = 0;
      if (check_extcpuid_ecx(CPUID_FEAT_ECX_SSE4a))
      {
         _aax_arch_capabilities |= AAX_ARCH_SSE4A;
         res = AAX_SIMD_SSE4A;
      }

      if (check_cpuid_ecx(CPUID_FEAT_ECX_SSE4_1))
      {
         _aax_arch_capabilities |= AAX_ARCH_SSE41;
         res = AAX_SIMD_SSE41;
      }

      if (check_cpuid_ecx(CPUID_FEAT_ECX_SSE4_2))
      {
         _aax_arch_capabilities |= AAX_ARCH_SSE42;
         res = AAX_SIMD_SSE42;
      }
   }
   return res;
}

char
_aaxArchDetectAVX()
{
   static uint32_t res = 0;
   static int8_t init = -1;
   if (init)
   {
      init = 0;
      res = check_cpuid_ecx(CPUID_FEAT_ECX_AVX) ? AAX_SIMD_AVX : 0;
      if (res) _aax_arch_capabilities |= AAX_ARCH_AVX;
   }
   return res;
}

char
_aaxArchDetectFMA3()
{
   static uint32_t res = 0;
   static int8_t init = -1;
   if (init)
   {
      init = 0;
      res = check_cpuid_ecx(CPUID_FEAT_ECX_FMA3) ? AAX_SIMD_FMA3 : 0;
      if (res) _aax_arch_capabilities |= AAX_ARCH_FMA3;
   }
   return res;
}


char
_aaxArchDetectAVX2()
{
   static uint32_t res = 0;
   static int8_t init = -1;
   if (init)
   {
      init = 0;
      res = check_cpuid_ebx(CPUID_FEAT_EBX_AVX2) ? AAX_SIMD_AVX2 : 0;
      if (res) _aax_arch_capabilities |= AAX_ARCH_AVX2;
   }
   return res;
}

char
_aaxArchDetectAVX512F()
{
   static uint32_t res = 0;
   static int8_t init = -1;
   if (init)
   {
      init = 0;
      res = check_cpuid_ebx(CPUID_FEAT_EBX_AVX512F) ? AAX_SIMD_AVX512 : 0;
      if (res) _aax_arch_capabilities |= AAX_ARCH_AVX512;
   }
   return res;
}

char
_aaxGetSSELevel()
{
   static uint32_t sse_level = AAX_NO_SIMD;
   static int8_t init = -1;

   if (init)
   {
      int capabilities = _info->capabilities;
      int res;

      _aax_calloc = _aax_calloc_aligned;
      _aax_malloc = _aax_malloc_aligned;

      if (capabilities & AAX_SIMD)
      {
         res = _aaxArchDetectSSE();
         if (res) sse_level = res;

         res = _aaxArchDetectSSE2();
         if (res) sse_level = res;

         res = _aaxArchDetectSSE3();
         if (res) sse_level = res;

         res = _aaxArchDetectSSE4();
         if (res) sse_level = res;
      }

      if (capabilities & AAX_SIMD256)
      {
         res = _aaxArchDetectAVX();
         if (res) sse_level = res;
      }

      if (capabilities & AAX_SIMD256_2)
      {
         res = _aaxArchDetectAVX2();
         if (res) sse_level = res;

         res = _aaxArchDetectFMA3();
         if (res) sse_level = res;
      }

      if (capabilities & AAX_SIMD512)
      {
         res = _aaxArchDetectAVX512F();
         if (res) sse_level = res;
      }
   }

   return sse_level;
}

uint32_t
_aaxGetSIMDSupportLevel()
{
   static bool support_simd256 = false;
   static bool support_simd = false;
   static uint32_t rv = AAX_SIMD_NONE;
   static bool init = true;

# ifndef __TINYC__
   if (init)
   {
      init = false;
      rv = _aaxGetSSELevel();

      support_simd = _info->capabilities & 0xF00;
      support_simd256 = _info->capabilities & (AAX_SIMD256|AAX_SIMD256_2);
      if (support_simd)
      {
         if (_aax_arch_capabilities & AAX_ARCH_SSE)
         {
            _aax_init_SSE();

            vec3fMagnitude = _vec3fMagnitude_sse;
            vec3fMagnitudeSquared = _vec3fMagnitudeSquared_sse;
            vec3fDotProduct = _vec3fDotProduct_sse;
            vec3fCrossProduct = _vec3fCrossProduct_sse;
            vec3fAbsolute = _vec3fAbsolute_sse;
            vec4fCopy = _vec4fCopy_sse;
            vec4fMulVec4 = _vec4fMulVec4_sse;
            mtx4fMul = _mtx4fMul_sse;
            mtx4fMulVec4 = _mtx4fMulVec4_sse;
            vec3fAltitudeVector = _vec3fAltitudeVector_sse;
         }
         if (_aax_arch_capabilities & AAX_ARCH_SSE2)
         {
//          _aax_memcpy = _aax_memcpy_sse2;

            mtx4dMul = _mtx4dMul_sse2;
            mtx4dMulVec4 = _mtx4dMulVec4_sse2;
            vec3dAltitudeVector = _vec3dAltitudeVector_sse2;

            _aax_generate_waveform_float = _aax_generate_waveform_sse2;

            _batch_get_average_rms = _batch_get_average_rms_sse2;
            _batch_saturate24 = _batch_saturate24_sse2;

            _batch_roundps = _batch_roundps_sse2;
            _batch_atanps = _batch_atanps_sse2;
            _batch_cvtps_24 = _batch_cvtps_24_sse2;
            _batch_cvt24_ps = _batch_cvt24_ps_sse2;
            _batch_cvt24_16 = _batch_cvt24_16_sse2;
            _batch_cvt16_24 = _batch_cvt16_24_sse2;
            _batch_cvt16_intl_24 = _batch_cvt16_intl_24_sse2;

            _batch_fmadd = _batch_fmadd_sse2;
            _batch_fmul = _batch_fmul_sse2;
            _batch_fmul_value = _batch_fmul_value_sse2;
            _batch_cvtps24_24 = _batch_cvtps24_24_sse2;
            _batch_cvt24_ps24 = _batch_cvt24_ps24_sse2;
            _batch_movingaverage_float = _batch_ema_iir_float_sse2;
            _batch_freqfilter_float = _batch_freqfilter_float_sse2;
            _batch_resample_float = _batch_resample_float_sse2;
         }
         if (_aax_arch_capabilities & AAX_ARCH_SSE3)
         {
            vec3fMagnitude = _vec3fMagnitude_sse3;
            vec3fMagnitudeSquared = _vec3fMagnitudeSquared_sse3;
            vec3fDotProduct = _vec3fDotProduct_sse3;
            _batch_imul_value = _batch_imul_value_sse3;
         }

         if (_aax_arch_capabilities & AAX_ARCH_SSE41)
         {
            _batch_roundps = _batch_roundps_sse4;
         }

         if (support_simd256)
         {
            if (_aax_arch_capabilities & AAX_ARCH_AVX)
            {
//             _batch_convolution = _batch_convolution_sse_vex;

               /* SSE/VEX */
               vec3fMagnitude = _vec3fMagnitude_sse_vex;
               vec3fMagnitudeSquared = _vec3fMagnitudeSquared_sse_vex;
               vec3fDotProduct = _vec3fDotProduct_sse_vex;
               vec3fCrossProduct = _vec3fCrossProduct_sse_vex;
               vec3fAbsolute = _vec3fAbsolute_sse_vex;
               vec4fCopy = _vec4fCopy_sse_vex;
               vec4fMulVec4 = _vec4fMulVec4_sse_vex;
               mtx4fMul = _mtx4fMul_sse_vex;
               mtx4fMulVec4 = _mtx4fMulVec4_sse_vex;
               vec3fAltitudeVector = _vec3fAltitudeVector_sse_vex;

               /* AVX */
               mtx4dMul = _mtx4dMul_avx;
               mtx4dMulVec4 = _mtx4dMulVec4_avx;
               vec3dAltitudeVector = _vec3dAltitudeVector_avx;

               _aax_generate_waveform_float = _aax_generate_waveform_avx;
               _batch_get_average_rms = _batch_get_average_rms_avx;

               _batch_cvt24_16 = _batch_cvt24_16_sse_vex;
               _batch_cvt16_24 = _batch_cvt16_24_sse_vex;
               _batch_cvt16_intl_24 = _batch_cvt16_intl_24_sse_vex;

               // CPU is faster on __x86_64__ as it already supports SSE2
//             _batch_roundps = _batch_roundps_avx;
               _batch_roundps = _batch_roundps_cpu;

               _batch_atanps = _batch_atanps_avx;
               _batch_movingaverage_float = _batch_ema_iir_float_sse_vex;
               _batch_freqfilter_float = _batch_freqfilter_float_sse_vex;
               _batch_resample_float = _batch_resample_float_sse_vex;

//             _aax_memcpy = _aax_memcpy_avx;
               _batch_cvtps_24 = _batch_cvtps_24_avx;
               _batch_cvt24_ps = _batch_cvt24_ps_avx;

               _batch_fmul = _batch_fmul_avx;
               _batch_fmul_value = _batch_fmul_value_avx;
               _batch_fmadd = _batch_fmadd_avx;
               _batch_cvtps24_24 = _batch_cvtps24_24_avx;
               _batch_cvt24_ps24 = _batch_cvt24_ps24_avx;
            }

            if (_aax_arch_capabilities & AAX_ARCH_FMA3)
            {
               _batch_fmadd = _batch_fmadd_fma3;

               _aax_generate_waveform_float = _aax_generate_waveform_fma3;
               _batch_get_average_rms = _batch_get_average_rms_fma3;

               _batch_atanps = _batch_atanps_fma3;
               _batch_freqfilter_float = _batch_freqfilter_float_fma3;
               _batch_resample_float = _batch_resample_float_fma3;

               mtx4fMul = _mtx4fMul_fma3;
               mtx4fMulVec4 = _mtx4fMulVec4_fma3;
               mtx4dMul = _mtx4dMul_fma3;
               mtx4dMulVec4 = _mtx4dMulVec4_fma3;
               vec3dAltitudeVector = _vec3dAltitudeVector_fma3;
            }
         }
      }
   }
# endif // __TINYC__

   return rv;
}

const char *
_aaxGetSIMDSupportString()
{
   uint32_t level = _aaxGetSIMDSupportLevel();
   return _aaxArchSIMDSupportString[level];
}

#endif

/* -------------------------------------------------------------------------- */

#if defined(__i386__) && defined(__PIC__)
/* %ebx may be the PIC register.  */
# define __cpuid(regs, level)	\
  regs[EAX] = level; regs[ECX] = 0; \
  ASM ("movl %%ebx, %%edi \n\t cpuid \n\t xchgl %%ebx, %%edi" \
           : "=D" (regs[EBX]), "+a" (regs[EAX]), "+c" (regs[ECX]), "=d" (regs[EDX]) )

#elif defined(__i386__) || defined(__x86_64__) && !defined(_MSC_VER)
# define __cpuid(regs, level)				\
  regs[EAX] = level; regs[ECX] = 0; \
  ASM ("cpuid\n\t" \
           : "+b" (regs[EBX]), "+a" (regs[EAX]), "+c" (regs[ECX]), "=d" (regs[EDX]) )
#else
#  define __cpuid(regs, level)
#endif

#if defined(__i386__) && !defined(_MSC_VER)
static char
detect_cpuid()
{
   static char res = -1;

   if (res == -1)
   {
      int reg1, reg2;


     ASM ("pushfl; pushfl; popl %0; movl %0,%1; xorl %2,%0;"
          "pushl %0; popfl; pushfl; popl %0; popfl"
          : "=&r" (reg1), "=&r" (reg2)
          : "i" (0x00200000));

      if (((reg1 ^ reg2) & 0x00200000) == 0) {
         res = 0;
      }
   }
   return res;
}
#else
#define detect_cpuid()  1
#endif

#if defined(__i386__) || defined(__x86_64__)
enum {
  EAX=0, EBX, ECX, EDX
};
static unsigned int regs[4];

static char
check_cpuid_ebx(unsigned int type)
{
   if (detect_cpuid()) {
      __cpuid(regs, CPUID_GETEXTFEATURES);
   }
   return (regs[EBX] & type) ? 1 : 0;
}

char
check_cpuid_ecx(unsigned int type)
{
   if (detect_cpuid()) {
      __cpuid(regs, CPUID_GETFEATURES);
   }
   return  (regs[ECX] & type) ? 1 : 0;
}

# ifndef __x86_64__
static char
check_cpuid_edx(unsigned int type)
{
   if (detect_cpuid()) {
      __cpuid(regs, CPUID_GETFEATURES);
   }
   return (regs[EDX] & type) ? 1 : 0;
}
# endif

char
check_extcpuid_ecx(unsigned int type)
{
   regs[ECX] = 0;
   __cpuid(regs, CPUID_GETEXTCPUINFO);
   if (regs[EAX] >= 0x80000001 && regs[EBX] == htuA &&
       regs[ECX] == DMAc && regs[EDX] == itne)
   {
      __cpuid(regs, 0x80000001);
   }
   return (regs[ECX] & type) ? 3 : 0;
}

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
# elif defined(__linux__) || (__linux)
   uint32_t lcores = 0, tsibs = 0;
   char buff[32];
   char path[64];

   for (lcores=0; ; ++lcores)
   {
      FILE *cpu;

      snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%u/topology/thread_siblings_list", lcores);

      cpu = fopen(path, "r");
      if (!cpu) break;

      while (fscanf(cpu, "%[0-9]", buff))
      {
         tsibs++;
         if (fgetc(cpu) != ',') break;
      }
      fclose(cpu);
    }

    cores = lcores/(tsibs/lcores);

#elif (defined (__SVR4) && defined (__sun))
   // also for AIX
   cores = sysconf( _SC_NPROCESSORS_ONLN );

# elif defined( WIN32 )
   SYSTEM_INFO sysinfo;

   GetSystemInfo( &sysinfo );
   cores = sysinfo.dwNumberOfProcessors;
# endif

#if 0
   do
   {
      int hyper_threading = check_cpuid_edx(CPUID_FEAT_EDX_HT);
      char vendor_str[13];

      __cpuid(regs, CPUID_GETVENDORSTRING);

      memcpy(&vendor_str[0], &regs[EBX], sizeof(int));
      memcpy(&vendor_str[4], &regs[EDX], sizeof(int));
      memcpy(&vendor_str[8], &regs[ECX], sizeof(int));
      vendor_str[12] = 0;

      if (hyper_threading && !strncmp(vendor_str, "GenuineIntel", 12))
      {
         int logical;

         __cpuid(regs, CPUID_GETFEATURES);
         logical = (regs[EBX] >> 16) & 0xFF;    // EBX[23:16]
         if (logical) cores /= logical;
      }
   }
   while (0);
#endif

   return (cores > 0) ? cores : 1;
}

#else	/* __i386__ || __x86_64__ */

char
_aaxArchDetectSSE() {
   return 0;
}

char
_aaxArchDetectSSE2() {
   return 0;
}

char
_aaxArchDetectSSE3() {
   return 0;
}

char
_aaxArchDetectSSE4() {
   return 0;
}

char
_aaxArchDetectAVX() {
   return 0;
}

char
_aaxArchDetectFMA3() {
   return 0;
}

char
_aaxArchDetectAVX2() {
   return 0;
}

#endif /* __i386__ || __x86_64__ */

