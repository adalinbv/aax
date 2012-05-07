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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>	/* getenv, malloc */
#include <strings.h>	/* strcasecmp */
#if HAVE_CPU_FEATURES_H
#include <machine/cpu-features.h>
#endif

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>
#include <base/types.h>
#include <base/geometry.h>

#include "arch_simd.h"


enum cpuid_requests {
  CPUID_GETVENDORSTRING = 0,
  CPUID_GETFEATURES,
  CPUID_GETEXTFEATURES = 0x80000000
};

enum {
    CPUID_FEAT_ECX_SSE3         = 1 << 0, 
    CPUID_FEAT_ECX_SSE4a        = 1 << 6,
    CPUID_FEAT_ECX_SSSE3        = 1 << 9,  
    CPUID_FEAT_ECX_CX16         = 1 << 13, 
    CPUID_FEAT_ECX_SSE4_1       = 1 << 19, 
    CPUID_FEAT_ECX_SSE4_2       = 1 << 20, 
    CPUID_FEAT_ECX_POPCNT       = 1 << 23, 
 
    CPUID_FEAT_EDX_CX8          = 1 << 8,  
    CPUID_FEAT_EDX_CMOV         = 1 << 15, 
    CPUID_FEAT_EDX_MMX          = 1 << 23, 
    CPUID_FEAT_EDX_FXSR         = 1 << 24, 
    CPUID_FEAT_EDX_SSE          = 1 << 25, 
    CPUID_FEAT_EDX_SSE2         = 1 << 26, 
};

enum {
    AAX_NO_SIMD = 0,
    AAX_NEON,

    AAX_MMX = AAX_NEON,
    AAX_SSE,
    AAX_SSE2,
    AAX_SSE3,
    AAX_SSE4
};

#define MAX_SSE_LEVEL		9
#define SSE_LEVEL_SSE4		5
#define DMAc			0x444d4163 
#define htuA			0x68747541
#define itne			0x69746e65

#if defined(__i386__)
static char check_cpuid_ecx(unsigned int);
static char check_cpuid_edx(unsigned int);
static char check_extcpuid_ecx(unsigned int);
#else
# define check_cpuid_ecx(a)
# define check_cpuid_edx(a)
# define check_extcpuid_ecx(a)
#endif

_aax_memcpy_proc _aax_memcpy = (_aax_memcpy_proc)memcpy;
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

_batch_mul_value_proc _batch_mul_value = _batch_mul_value_cpu;
_batch_fmadd_proc _batch_fmadd = _batch_fmadd_cpu;
_batch_freqfilter_proc _batch_freqfilter = _batch_freqfilter_cpu;
_batch_resample_proc _aaxBufResampleCubic = _aaxBufResampleCubic_cpu;
_batch_resample_proc _aaxBufResampleLinear = _aaxBufResampleLinear_cpu;
_batch_resample_proc _aaxBufResampleNearest = _aaxBufResampleNearest_cpu;
_batch_resample_proc _aaxBufResampleSkip = _aaxBufResampleSkip_cpu;


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
# ifdef __x86_64__
   static char res = 1;
#else
   static char res = (char)-1;
   if (res == (char)-1)
   {
      res = check_cpuid_ecx(CPUID_FEAT_ECX_SSE3);
      if (res) {
         res = check_cpuid_ecx(CPUID_FEAT_ECX_SSSE3);
      }
   }
#endif
   return res;
}

char
_aaxDetectSSE4()
{
# ifdef __x86_64__
   static char res = 1;
#else
   static char res = (char)-1;
   if (res == (char)-1)
   {
      res = check_cpuid_ecx(CPUID_FEAT_ECX_SSE4_1);
      if (res) {
         res += check_cpuid_ecx(CPUID_FEAT_ECX_SSE4_2);
      } else {
         res = check_extcpuid_ecx( CPUID_FEAT_ECX_SSE4a);
      }
   }
#endif
   return res;
}

char
_aaxGetSSELevel()
{
   static char sse_level = (char)-1;

   if (sse_level == (char)-1)
   {
      sse_level = 0;

      char *env = getenv("AAX_NO_SIMD_SUPPORT");
      if (!_oal_getbool(env))
      {
         int res;

         _aax_calloc = _aax_calloc_align16;
         _aax_malloc = _aax_malloc_align16;
         
         res = _aaxDetectMMX();
         res += _aaxDetectSSE();
         res += _aaxDetectSSE2();
#if 0
         res += _aaxDetectSSE3();
#endif
         sse_level = res;
#if 0
         if ((res = _aaxDetectSSE4()) > 0) {
            sse_level = SSE_LEVEL_SSE4 + res;
         }
         if (sse_level >= MAX_SSE_LEVEL) {
            sse_level = MAX_SSE_LEVEL-1;
         }
#endif
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
      "MMX/SSE4.1",
      "MMX/SSE4.2",
      "MMX/SSE4a",
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

      mtx4Mul = _mtx4Mul_neon;
      ivec4Add = _ivec4Add_neon;
      ivec4Sub = _ivec4Sub_neon;
      ivec4Devide = _ivec4Devide_neon;
      ivec4Mulivec4 = _ivec4Mulivec4_neon;

      _batch_fmadd = _batch_fmadd_neon;
      _batch_cvt24_16 = _batch_cvt24_16_neon;
      _batch_cvt16_24 = _batch_cvt16_24_neon;
      _batch_cvt16_intl_24 = _batch_cvt16_inl_24_neon;
      _batch_freqfilter = _batch_freqfilter_neon;

      _aaxBufResampleSkip = _aaxBufResampleSkip_neon;
      _aaxBufResampleCubic = _aaxBufResampleCubic_neon;
      _aaxBufResampleLinear = _aaxBufResampleLinear_neon;
      _aaxBufResampleNearest = _aaxBufResampleNearest_neon;
#endif
   }
   else
   {
      level = _aaxGetSSELevel();
#if defined(__i386__) || defined(__x86_64__)
      if (level >= AAX_SSE)
      {
         vec4Add = _vec4Add_sse;
         vec4Sub = _vec4Sub_sse;
         vec4Copy = _vec4Copy_sse;
         vec4Devide = _vec4Devide_sse;
         vec4Mulvec4 = _vec4Mulvec4_sse;
         vec4Matrix4 = _vec4Matrix4_sse;
         mtx4Mul = _mtx4Mul_sse;
      }
      if (level >= AAX_SSE2)
      {
         ivec4Add = _ivec4Add_sse2;
         ivec4Devide = _ivec4Devide_sse2;
         ivec4Mulivec4 = _ivec4Mulivec4_sse2;
         ivec4Sub = _ivec4Sub_sse2;

//       _aax_memcpy = _aax_memcpy_sse2;
         _batch_fmadd = _batch_fmadd_sse2;
         _batch_cvt24_16 = _batch_cvt24_16_sse2;
         _batch_cvt16_24 = _batch_cvt16_24_sse2;
         _batch_freqfilter = _batch_freqfilter_sse2;
         _batch_cvt16_intl_24 = _batch_cvt16_intl_24_sse2;

         _aaxBufResampleSkip = _aaxBufResampleSkip_sse2;
         _aaxBufResampleNearest = _aaxBufResampleNearest_sse2;
         _aaxBufResampleLinear = _aaxBufResampleLinear_sse2;
         _aaxBufResampleCubic = _aaxBufResampleCubic_sse2;
      }
#endif
   }

   return _aaxSSESupportString[level];
}


/* -------------------------------------------------------------------------- */

#if defined(__i386__) && defined(__PIC__)
/* %ebx may be the PIC register.  */
# define __cpuid(level, a, b, c, d)			\
  asm volatile ("xchgl\t%%ebx, %1\n\t"			\
           "cpuid\n\t"					\
           "xchgl\t%%ebx, %1\n\t"			\
           : "=a" (a), "=r" (b), "=c" (c), "=d" (d)	\
           : "0" (level))
#elif defined(__i386__) || defined(__x86_64__)
# define __cpuid(level, a, b, c, d)			\
  asm volatile ("cpuid\n\t"				\
           : "=a" (a), "=b" (b), "=c" (c), "=d" (d)	\
           : "0" (level))
#else
#  define __cpuid(level, a, b, c, d)
#endif

#if defined(__i386__)
static char
detect_cpuid()
{
   static char res = -1;

   if (res == -1)
   {
      int reg1, reg2;

     asm volatile
         ("pushfl; pushfl; popl %0; movl %0,%1; xorl %2,%0;"
          "pushl %0; popfl; pushfl; popl %0; popfl"
          : "=&r" (reg1), "=&r" (reg2)
          : "i" (0x00200000));

      if (((reg1 ^ reg2) & 0x00200000) == 0) {
         res = 0;
      }
   }
   return res;
}

static unsigned int _eax = 0, _ebx = 0;
static unsigned int _ecx = -1, _edx = -1;

static char
check_cpuid_ecx(unsigned int type)
{
   if (_ecx == (unsigned int)-1) {
      _ecx = 0;
      if (detect_cpuid()) {
         __cpuid(CPUID_GETFEATURES, _eax, _ebx, _ecx, _edx);
      }
   }
   return  (_ecx & type) ? 1 : 0;
}

static char
check_cpuid_edx(unsigned int type)
{
   if (_edx == (unsigned int)-1) {
      _edx = 0;
      if (detect_cpuid()) {
         __cpuid(CPUID_GETFEATURES, _eax, _ebx, _ecx, _edx);
      }
   }
   return (_edx & type) ? 1 : 0;
}

static char
check_extcpuid_ecx(unsigned int type)
{
   unsigned int eax, ebx, ecx = 0, edx;
   __cpuid(CPUID_GETEXTFEATURES, eax, ebx, ecx, edx);
   if (_eax >= 0x80000001 && ebx == htuA && ecx == DMAc && edx == itne)
   {
      __cpuid(0x80000001, eax, ebx, ecx, edx);
   }
   return (ecx & type) ? 3 : 0;
}
#endif /* __i386__ || __x86_64__ */

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

char *
_aax_strdup(const char *s)
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

#if 0
unsigned int
_aaxGetNoCores()
{
   unsigned int rv = 1;
#if defined(freebsd) || defined(bsdi) || defined(__darwin__) || defined(openbsd)
   size_t len = sizeof(rv); 
   int mib[4];

   mib[0] = CTL_HW;
   mib[1] = HW_AVAILCPU;
   sysctl(mib, 2, &rv, &len, NULL, 0);
   if(rv < 1) 
   {
      mib[1] = HW_NCPU;
      sysctl(mib, 2, &rv, &len, NULL, 0);
      if( rv < 1 ) {
         rv = 1;
      }
   }
#elif defined(__linux__) || (__linux) || (defined (__SVR4) && defined (__sun))
   // also for AIX
   rv = sysconf( _SC_NPROCESSORS_ONLN );

#elif defined(IRIX)
   rv = sysconf( _SC_NPROC_ONLN );

#elif defined(HPUX)
   rv = mpctl(MPC_GETNUMSPUS, NULL, NULL);

#elif defined( WIN32 )
   SYSTEM_INFO sysinfo;

   GetSystemInfo( &sysinfo );
   rv = sysinfo.dwNumberOfProcessors;
#endif
 
   return rv;
}
#endif
