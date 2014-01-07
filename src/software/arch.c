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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_UNISTD_H
# include <unistd.h>    /* sysconf */
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

#include <api.h>

#include <base/types.h>
#include <base/geometry.h>

#include <arch.h>
#include <ringbuffer.h>

#include "cpu/arch2d_simd.h"
#include "cpu/arch3d_simd.h"


enum cpuid_requests {
  CPUID_GETVENDORSTRING = 0,
  CPUID_GETFEATURES,
  CPUID_GETEXTFEATURES = 0x80000000
};

enum {
    CPUID_FEAT_ECX_SSE3         = 1 << 0, 
    CPUID_FEAT_ECX_SSE4a        = 1 << 6,
    CPUID_FEAT_ECX_SSSE3        = 1 << 9,  
    CPUID_FEAT_ECX_FMA3		= 1 << 12,
    CPUID_FEAT_ECX_CX16         = 1 << 13, 
    CPUID_FEAT_ECX_FMA4		= 1 << 16,
    CPUID_FEAT_ECX_SSE4_1       = 1 << 19, 
    CPUID_FEAT_ECX_SSE4_2       = 1 << 20, 
    CPUID_FEAT_ECX_POPCNT       = 1 << 23, 
    CPUID_FEAT_ECX_AVX		= 1 << 28,
 
    CPUID_FEAT_EDX_CX8          = 1 << 8,  
    CPUID_FEAT_EDX_CMOV         = 1 << 15, 
    CPUID_FEAT_EDX_MMX          = 1 << 23, 
    CPUID_FEAT_EDX_FXSR         = 1 << 24, 
    CPUID_FEAT_EDX_SSE          = 1 << 25, 
    CPUID_FEAT_EDX_SSE2         = 1 << 26, 
    CPUID_FEAT_EDX_HT           = 1 << 28
};

enum {
    AAX_NO_SIMD = 0,

    AAX_MMX,
    AAX_SSE,
    AAX_SSE2,
    AAX_SSE3,
    AAX_SSSE3,
    AAX_SSE4A,
    AAX_SSE41,
    AAX_SSE42,
    AAX_AVX,

    AAX_NEON
};

#define MAX_SSE_LEVEL	10	
#define DMAc			0x444d4163 
#define htuA			0x68747541
#define itne			0x69746e65

#if !defined(__i386__) && defined(_M_IX86)
# define __i386__
#endif
#if !defined(__x86_64__) && defined(_M_X64)
# define __x86_64__
#endif

#if defined(__i386__) || defined(__x86_64__)
static char check_cpuid_ecx(unsigned int);
static char check_cpuid_edx(unsigned int);
static char check_extcpuid_ecx(unsigned int);
#else
# define check_cpuid_ecx(a)	0
# define check_cpuid_edx(a)	0
# define check_extcpuid_ecx(a)	0
#endif

_aax_memcpy_proc _aax_memcpy = (_aax_memcpy_proc)memcpy;
_aax_free_proc _aax_free = (_aax_free_proc)_aax_free_align16;
_aax_calloc_proc _aax_calloc = (_aax_calloc_proc)_aax_calloc_align16;
_aax_malloc_proc _aax_malloc = (_aax_malloc_proc)_aax_malloc_align16;
_aax_memcpy_proc _batch_cvt24_24 = (_aax_memcpy_proc)_batch_cvt24_24_cpu;

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

_batch_cvt_proc _batch_saturate24 = _batch_saturate24_cpu;

_batch_cvt_proc _batch_cvt8u_8s = _batch_cvt8u_8s_cpu;
_batch_cvt_proc _batch_cvt8s_8u = _batch_cvt8s_8u_cpu;
_batch_cvt_proc _batch_cvt16u_16s = _batch_cvt16u_16s_cpu;
_batch_cvt_proc _batch_cvt16s_16u = _batch_cvt16s_16u_cpu;
_batch_cvt_proc _batch_cvt24u_24s = _batch_cvt24u_24s_cpu;
_batch_cvt_proc _batch_cvt24s_24u = _batch_cvt24s_24u_cpu;
_batch_cvt_proc _batch_cvt32u_32s = _batch_cvt32u_32s_cpu;
_batch_cvt_proc _batch_cvt32s_32u = _batch_cvt32s_32u_cpu;

_batch_cvt_proc _batch_endianswap16 = _batch_endianswap16_cpu;
_batch_cvt_proc _batch_endianswap32 = _batch_endianswap32_cpu;
_batch_cvt_proc _batch_endianswap64 = _batch_endianswap64_cpu;

_batch_cvt_to_proc _batch_cvt8_24 = _batch_cvt8_24_cpu;
_batch_cvt_to_proc _batch_cvt16_24 = _batch_cvt16_24_cpu;
_batch_cvt_to_proc _batch_cvt24_3_24 = _batch_cvt24_3_24_cpu;
_batch_cvt_to_proc _batch_cvt32_24 = _batch_cvt32_24_cpu;
_batch_cvt_to_proc _batch_cvtps_24 = _batch_cvtps_24_cpu;
_batch_cvt_to_proc _batch_cvtpd_24 = _batch_cvtpd_24_cpu;
_batch_cvt_to_intl_proc _batch_cvt8_intl_24 = _batch_cvt8_intl_24_cpu;
_batch_cvt_to_intl_proc _batch_cvt16_intl_24 = _batch_cvt16_intl_24_cpu;
_batch_cvt_to_intl_proc _batch_cvt24_3intl_24 = _batch_cvt24_3intl_24_cpu;
_batch_cvt_to_intl_proc _batch_cvt24_intl_24 = _batch_cvt24_intl_24_cpu;
_batch_cvt_to_intl_proc _batch_cvt32_intl_24 = _batch_cvt32_intl_24_cpu;
_batch_cvt_to_intl_proc _batch_cvtps_intl_24 = _batch_cvtps_intl_24_cpu;
_batch_cvt_to_intl_proc _batch_cvtpd_intl_24 = _batch_cvtpd_intl_24_cpu;

_batch_imadd_proc _batch_imadd = _batch_imadd_cpu;
_batch_fmadd_proc _batch_fmadd = _batch_fmadd_cpu;
_batch_mul_value_proc _batch_imul_value = _batch_imul_value_cpu;
_batch_mul_value_proc _batch_fmul_value = _batch_fmul_value_cpu;
_batch_freqfilter_proc _batch_freqfilter = _batch_freqfilter_cpu;
_batch_freqfilter_float_proc _batch_freqfilter_float = _batch_freqfilter_float_cpu;


_batch_cvt_from_proc _batch_cvt24_ps24 = _batch_cvt24_ps24_cpu;
_batch_cvt_to_proc _batch_cvtps24_24 = _batch_cvtps24_24_cpu;
_batch_resample_float_proc _batch_resample_float = _batch_resample_float_cpu;
_batch_resample_proc _batch_resample = _batch_resample_cpu;


char
_aaxDetectNeon()
{
   static char res = (char)-1;
   if (res == (char)-1)
   {
      res = 0;

#if HAVE_CPU_FEATURES_H
      if (android_getCpuFamily() == ANDROID_CPU_FAMILY_ARM)
      {
         uint64_t features = android_getCpuFeatures();
         if ( (features & ANDROID_CPU_ARM_FEATURE_ARMv7)
               && (features & ANDROID_CPU_ARM_FEATURE_NEON) )
         {
            res = 1;
         }
      }
#endif
   }
   return res;
}

char
_aaxDetectMMX()
{
# ifdef __x86_64__
   static char res = 1;
#else
   static char res = (char)-1;
   if (res == (char)-1) {
      res = check_cpuid_edx(CPUID_FEAT_EDX_MMX);
   }
#endif
   return res;
}

char
_aaxDetectSSE()
{
# ifdef __x86_64__
   static char res = 1;
#else
   static char res = (char)-1;
   if (res == (char)-1) {
      res = check_cpuid_edx(CPUID_FEAT_EDX_SSE);
   }
#endif
   return res;
}

char
_aaxDetectSSE2()
{
# ifdef __x86_64__
   static char res = 1;
#else
   static char res = (char)-1;
   if (res == (char)-1) {
      res = check_cpuid_edx(CPUID_FEAT_EDX_SSE2);
   }
#endif
   return res;
}

char
_aaxDetectSSE3()
{
   static char res = (char)-1;
   if (res == (char)-1)
   {
      res = check_cpuid_ecx(CPUID_FEAT_ECX_SSE3);
      if (res) {
         res += check_cpuid_ecx(CPUID_FEAT_ECX_SSSE3);
      }
   }
   return res;
}

char
_aaxDetectSSE4()
{
   static char res = -1;
   if (res == (char)-1)
   {
      char ret;

      res = 0;
      ret = check_extcpuid_ecx(CPUID_FEAT_ECX_SSE4a);
      if (ret) res = AAX_SSE4A;

      ret = check_cpuid_ecx(CPUID_FEAT_ECX_SSE4_1);
      if (ret) res = AAX_SSE41;

      ret = check_cpuid_ecx(CPUID_FEAT_ECX_SSE4_2);
      if (ret) res = AAX_SSE42;
   }
   return res;
}

char
_aaxDetectAVX()
{
   static char res = (char)-1;
   if (res == (char)-1) {
      res = check_cpuid_ecx(CPUID_FEAT_ECX_AVX);
   }
   return res;
}

char
_aaxGetSSELevel()
{
   static char sse_level = (char)-1;

   if (sse_level == (char)-1)
   {
      char *env;

      sse_level = 0;

      env = getenv("AAX_NO_SIMD_SUPPORT");
      if (!_aax_getbool(env))
      {
         int res;

         _aax_calloc = _aax_calloc_align16;
         _aax_malloc = _aax_malloc_align16;
         
         res = _aaxDetectMMX();
         res += _aaxDetectSSE();
         res += _aaxDetectSSE2();
         res += _aaxDetectSSE3();
         sse_level = res;

#if 0
         if ((res = _aaxDetectSSE4()) > 0) {
            sse_level = res;
         }
#endif
         if (sse_level >= MAX_SSE_LEVEL) {
            sse_level = MAX_SSE_LEVEL-1;
         }

         if (_aaxDetectAVX()) {
            sse_level = AAX_AVX;
         }
      }
   }

   return sse_level;
}

const char *
_aaxGetSIMDSupportString()
{
   static const char *_aaxSSESupportString[MAX_SSE_LEVEL+1] = {
      "", 
      "MMX",
      "MMX/SSE",
      "MMX/SSE2",
      "MMX/SSE3",
      "MMX/SSSE3",
      "MMX/SSE4a",
      "MMX/SSE4.1",
      "MMX/SSE4.2",
      "SSE/AVX",
      "NEON"
   };
   int level = 0;

   if (_aaxDetectNeon())
   {
      level = MAX_SSE_LEVEL;
#if HAVE_CPU_FEATURES_H
      vec4Add = _vec4Add_neon;
      vec4Sub = _vec4Sub_neon;
      vec4Copy = _vec4Copy_neon;
      vec4Devide = _vec4Devide_neon;
      vec4Mulvec4 = _vec4Mulvec4_neon;
      vec4Matrix4 = _vec4Matrix4_neon;
      pt4Matrix4 = _pt4Matrix4_neon;

      mtx4Mul = _mtx4Mul_neon;

      _batch_imadd = _batch_imadd_neon;
      _batch_fmadd = _batch_fmadd_neon;
      _batch_cvt24_16 = _batch_cvt24_16_neon;
      _batch_cvt16_24 = _batch_cvt16_24_neon;
      _batch_cvt16_intl_24 = _batch_cvt16_inl_24_neon;
      _batch_freqfilter = _batch_freqfilter_neon;

      _batch_resample = _batch_resample_neon;
#endif
   }
   else
   {
      level = _aaxGetSSELevel();
#if defined(__i386__) || defined(__x86_64__)
      if (level >= AAX_SSE)
      {
//       vec3CrossProduct = _vec3CrossProduct_sse;
         vec4Add = _vec4Add_sse;
         vec4Sub = _vec4Sub_sse;
         vec4Copy = _vec4Copy_sse;
         vec4Devide = _vec4Devide_sse;
         vec4Mulvec4 = _vec4Mulvec4_sse;
         vec4Matrix4 = _vec4Matrix4_sse;
         pt4Matrix4 = _pt4Matrix4_sse;
         mtx4Mul = _mtx4Mul_sse;
      }
      if (level >= AAX_SSE2)
      {
         _aax_memcpy = _aax_memcpy_sse2;
         _batch_imadd = _batch_imadd_sse2;
         _batch_fmadd = _batch_fmadd_sse2;
         _batch_cvtps_24 = _batch_cvtps_24_sse2;
         _batch_cvt24_ps = _batch_cvt24_ps_sse2;
         _batch_cvt24_16 = _batch_cvt24_16_sse2;
         _batch_cvt16_24 = _batch_cvt16_24_sse2;
         _batch_freqfilter = _batch_freqfilter_sse2;
         _batch_freqfilter_float = _batch_freqfilter_float_sse2;
         _batch_cvt16_intl_24 = _batch_cvt16_intl_24_sse2;

         _batch_cvtps24_24 = _batch_cvtps24_24_sse2;
         _batch_cvt24_ps24 = _batch_cvt24_ps24_sse2;
         _batch_resample_float = _batch_resample_float_sse2;
         _batch_resample = _batch_resample_sse2;
      }
      if (level >= AAX_SSE3)
      {
         vec4Matrix4 = _vec4Matrix4_sse3;
         pt4Matrix4 = _pt4Matrix4_sse3;
         _batch_imul_value = _batch_imul_value_sse3;
         _batch_fmul_value = _batch_fmul_value_sse3;

#if !RB_FLOAT_DATA
         _batch_resample = _batch_resample_sse3;
#endif
      }

#ifdef __SSE4__
      if (level >= AAX_SSE41)
      {
         vec3Magnitude = _vec3Magnitude_sse4;
         vec3MagnitudeSquared = _vec3MagnitudeSquared_sse4;
         vec3DotProduct = _vec3DotProduct_sse4;
         vec3Normalize = _vec3Normalize_sse4;
      }
#endif

# if SIZEOF_SIZE_T == 8
      if (level >= AAX_AVX)
      {
#if 0
         /* Prefer FMA3 over FMA4 so detect FMA4 first */
#ifdef __FMA4__
         if (check_extcpuid_ecx(CPUID_FEAT_ECX_FMA4)) {
            _batch_fmadd = _batch_fma4_avx;
         }
#endif
#ifdef __FMA__
         if (check_cpuid_ecx(CPUID_FEAT_ECX_FMA3)) {
            _batch_fmadd = _batch_fma3_avx;
         }
#endif
#endif
         _batch_resample = _batch_resample_avx;
      }
# endif
#endif
   }

   return _aaxSSESupportString[level];
}


/* -------------------------------------------------------------------------- */

#if defined(__i386__) && defined(__PIC__)
/* %ebx may be the PIC register.  */
# define __cpuid(regs, level)				\
  ASM ("xchgl\t%%ebx, %1\n\t"				\
           "cpuid\n\t"					\
           "xchgl\t%%ebx, %1\n\t"			\
           : "=a" (regs[0]), "=r" (regs[1]), "=c" (regs[2]), "=d" (regs[3]) \
           : "0" (level))

#elif defined(__i386__) || defined(__x86_64__) && !defined(_MSC_VER)
# define __cpuid(regs, level)				\
      ASM ("cpuid\n\t"					\
           : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3]) \
           : "0" (level))
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
#define detect_cpuid()	1
#endif

#if defined(__i386__) || defined(__x86_64__)
enum {
  EAX=0, EBX, ECX, EDX
};
static int regs[4] = {0,0,-1,-1};

static char
check_cpuid_ecx(unsigned int type)
{
   if (regs[ECX] == -1)
   {
      regs[ECX] = 0;
      if (detect_cpuid()) {
         __cpuid(regs, CPUID_GETFEATURES);
      }
   }
   return  (regs[ECX] & type) ? 1 : 0;
}

static char
check_cpuid_edx(unsigned int type)
{
   if (regs[EDX] == -1) {
      regs[EDX] = 0;
      if (detect_cpuid()) {
         __cpuid(regs, CPUID_GETFEATURES);
      }
   }
   return (regs[EDX] & type) ? 1 : 0;
}

static char
check_extcpuid_ecx(unsigned int type)
{
   int _regs[4];

   regs[ECX] = 0;
   __cpuid(_regs, CPUID_GETEXTFEATURES);
   if (regs[EAX] >= 0x80000001 && _regs[EBX] == htuA &&
       _regs[ECX] == DMAc && _regs[EDX] == itne)
   {
      __cpuid(_regs, 0x80000001);
   }
   return (_regs[ECX] & type) ? 3 : 0;
}
#endif /* __i386__ || __x86_64__ */

void *
_aax_aligned_alloc16(size_t size)
{
   void *rv = NULL;

   if (size & 0xF)
   {
      size |= 0xF;
      size++;
   }

#if __MINGW32__
   rv = _mm_malloc(size, 16);
#elif ISOC11_SOURCE 
   rv = aligned_alloc(16, size);
#elif  _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600
   if (posix_memalign(&rv, 16, size) != 0) {
      rv = NULL;
   }
#elif _MSC_VER
   rv = _aligned_malloc(size, 16);
#else
   assert(1 == 0);
#endif
   return rv;
}

#if defined(__MINGW32__)
_aax_aligned_free_proc _aax_aligned_free = (_aax_aligned_free_proc)_mm_free;
#elif ISOC11_SOURCE 
_aax_aligned_free_proc _aax_aligned_free = (_aax_aligned_free_proc)free;
#elif  _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600
_aax_aligned_free_proc _aax_aligned_free = (_aax_aligned_free_proc)free;
#elif _MSC_VER
_aax_aligned_free_proc _aax_aligned_free= (_aax_aligned_free_proc)_aligned_free;
#else
# pragma warnig _aax_aligned_alloc16 needs implementing
_aax_aligned_free_proc _aax_aligned_free = (_aax_aligned_free_proc)free;
#endif

char *
_aax_malloc_align16(char **start, unsigned int size)
{
   int ctr = 3;
   char *ptr;

   assert((unsigned int)*start < size);

   /* 16-byte align */
   size += 16;		/* save room for a pointer shift to 16-byte align */
   if (size & 0xF)
   {
      size |= 0xF;
      size++;
   }

   do
   {
      ptr = (char *)(int64_t *)malloc(size);
      if (ptr)
      {
         char *s = ptr + (unsigned long)*start;
         unsigned int tmp;

         tmp = (long)s & 0xF;
         if (tmp)
         {
            tmp = 0x10 - tmp;
            s += tmp;
         }
         *start = s;
      }
   }
   while (!ptr && --ctr);

   if (!ptr) {
      _AAX_SYSLOG("Unable to allocate enough memory, giving up after 3 tries");
   }

   return ptr;
}

char *
_aax_calloc_align16(char **start, unsigned int num, unsigned int size)
{
   int ctr = 3;
   char *ptr;

   assert((unsigned int)*start < num*size);

   /* 16-byte align */
   size += 16;		 /* save room for a pointer shift to 16-byte align */
   if (size & 0xF)
   {
      size |= 0xF;
      size++;
   }

   do
   {
      ptr = (char *)malloc(num*size);
      if (ptr)
      {
         char *s = ptr + (unsigned long)*start;
         unsigned long tmp;

         tmp = (long)s & 0xF;
         if (tmp)
         {
            tmp = 0x10 - tmp;
            s += tmp;
         }
         *start = s;

         memset(ptr, 0, num*size);
      }
   }
   while (!ptr && --ctr);

   if (!ptr) {
      _AAX_SYSLOG("Unable to allocate enough memory, giving up after 3 tries");
   }

   return ptr;
}

void
_aax_free_align16(void *ptr)
{
   free(ptr);
}

char *
_aax_strdup(const_char_ptr s)
{
   char *ret = 0;
   if (s) {
      unsigned int len = strlen(s);
      ret = malloc(len+1);
      if (ret) {
         memcpy(ret, s, len+1);
      }
   }
   return ret;
}

unsigned int
_aaxGetNoCores()
{
   unsigned int cores = 1;

#if defined(__MACH__)
   sysctlbyname("hw.physicalcpu", &cores, sizeof(cores), NULL, 0);

#elif defined(freebsd) || defined(bsdi) || defined(__darwin__) || defined(openbsd)
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
#elif defined(__linux__) || (__linux) || (defined (__SVR4) && defined (__sun))
   // also for AIX
   cores = sysconf( _SC_NPROCESSORS_ONLN );

#elif defined(IRIX)
   cores = sysconf( _SC_NPROC_ONLN );

#elif defined(HPUX)
   cores = mpctl(MPC_GETNUMSPUS, NULL, NULL);

#elif defined( WIN32 )
// AeonWave does not use threads for audio-frames since it's thread model
// can't handle it.
# if 0
   SYSTEM_INFO sysinfo;

   GetSystemInfo( &sysinfo );
   cores = sysinfo.dwNumberOfProcessors;
# endif
#endif

#if defined(__i386__) || defined(__x86_64__)
   do
   {
      int hyper_threading = check_cpuid_edx(CPUID_FEAT_EDX_MMX);
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
         logical = (regs[EBX] >> 16) & 0x7;
         cores /= logical;
      }
   }
   while (0);
#endif

   return (cores > 0) ? cores : 1;
}

