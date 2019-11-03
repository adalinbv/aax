
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <base/types.h>
#include <base/geometry.h>
#include <base/timer.h>
#include <src/ringbuffer.h>
#include <src/dsp/dsp.h>
#include <src/software/rbuf_int.h>
#include <src/software/cpu/arch2d_simd.h>
#include <arch.h>

#define WAVE_TYPE	_SAWTOOTH_WAVE
#define VSTEP		-0.000039f
#define FACTOR		0.7723678263f

// Depends on the timer resolution for an acurate comparison
#if defined(__arm__) || defined(_M_ARM)
# define MAXNUM		(199*4096*10)
#else
# define MAXNUM		(4096*10)
#endif


#define TESTFN(a,d1,d2,m) { int n = 0; \
   for (i=0; i<MAXNUM; ++i) { \
      if (fabsf(d1[i]-d2[i]) > m) { \
         printf(" # %s %i: %10.1f != %10.1f (%5.4f%%)\n", a, i, d1[i], d2[i], 100.0f*fabsf((d1[i]-d2[i])/d1[i])); \
         if (++n == 8) break; \
      } } }
#define TESTF(a,d1,d2)	TESTFN(a,d1,d2,4.0f)

#define TESTLF(a,d1,d2) { int n = 0; \
   for (i=0; i<MAXNUM; ++i) { \
      if (fabsf(d1[i]-d2[i]) > 4) { \
         printf(" # %s %i: %10.1f != %10.1f (%5.4f%%)\n", a, i, d1[i], d2[i], 100.0f*fabsf((d1[i]-d2[i])/d1[i])); \
         if (++n == 8) break; \
      } } }

#if defined(__i386__)
# define SIMD	sse2
# define SIMD1	sse2
# define SIMD2	sse2
# define SIMD4	sse4
# define SIMD5	sse2
char _aaxArchDetectSSE2();
char _aaxArchDetectSSE4();
#elif defined(__x86_64__)
# define SIMD   sse2
# define SIMD1	sse_vex
# define SIMD2	avx
# define SIMD4	sse4
# define SIMD5	avx2
# define FMA3	fma3
# define FMA4	fma4
# define CPUID_FEAT_ECX_FMA3	(1 << 12)
# define CPUID_FEAT_ECX_FMA4	(1 << 16)
char _aaxArchDetectSSE4();
char _aaxArchDetectAVX();
char _aaxArchDetectAVX2();
char check_extcpuid_ecx(unsigned int);
char check_cpuid_ecx(unsigned int);
#elif defined(__arm__) || defined(_M_ARM)
# define SIMD	neon
# define SIMD1	neon
# define SIMD2	neon
# define SIMD4  neon
# define SIMD5	neon
# define AAX_ARCH_NEON	0x00000008
char _aaxArchDetectFeatures();
extern uint32_t _aax_arch_capabilities;
int _aaxArchDetectNEON()
{
   _aaxArchDetectFeatures();
   if (_aax_arch_capabilities & AAX_ARCH_NEON) return 1;
   return 0;
}
#endif

#define PHASE			0.1f
#define FREQ			220.0f

#define __MKSTR(X)		#X
#define MKSTR(X)		__MKSTR(X)
#define __GLUE(FUNC,NAME)	FUNC ## _ ## NAME
#define GLUE(FUNC,NAME)		__GLUE(FUNC,NAME)

extern _batch_fmadd_proc _batch_fmadd;
extern _batch_cvt_to_proc _batch_roundps;
extern _batch_mul_value_proc _batch_fmul_value;
extern _batch_get_average_rms_proc _batch_get_average_rms;
extern _batch_freqfilter_float_proc _batch_freqfilter_float;
extern _aax_generate_waveform_proc _aax_generate_waveform_float;

void _batch_freqfilter_float_sse_vex(float32_ptr dptr, const_float32_ptr sptr, int t, size_t num, void *flt);


int main()
{
   float *src, *dst1, *dst2;
   char simd = 0;
   char simd2 = 0;
   char simd3 = 0;
   char simd4 = 0;
   char simd5 = 0;
   clock_t t;

#if defined(__i386__)
   simd = _aaxArchDetectSSE2();
   simd4 = _aaxArchDetectSSE4();
#elif defined(__x86_64__)
   simd = 1;
   simd2 = _aaxArchDetectAVX();
   simd4 = _aaxArchDetectSSE4();
   simd5 = _aaxArchDetectAVX2();
   if (check_extcpuid_ecx(CPUID_FEAT_ECX_FMA4)) {
      simd3 = 4;
   }
   if (check_cpuid_ecx(CPUID_FEAT_ECX_FMA3)) {
      simd3 = 3;
   }
#elif defined(__arm__) || defined(_M_ARM)
   simd = _aaxArchDetectNEON();
#endif

   srand(time(NULL));

   src = (float*)_aax_aligned_alloc(MAXNUM*sizeof(double));
   dst1 = (float*)_aax_aligned_alloc(MAXNUM*sizeof(double));
   dst2 = (float*)_aax_aligned_alloc(MAXNUM*sizeof(double));

   if (src && dst1 && dst2)
   {
      _aaxRingBufferFreqFilterHistoryData history;
      _aaxRingBufferFreqFilterData flt;
      float rms1, rms2, peak1, peak2;
      double *dsrc, *ddst1, *ddst2;
      double cpu, eps;
      int i;

      for (i=0; i<MAXNUM; ++i) {
         src[i] = (float)(1<<23) * (float)rand()/(float)(RAND_MAX);
      }
      memset(dst1, 0, MAXNUM*sizeof(double));
      memset(dst2, 0, MAXNUM*sizeof(double));

      /*
       * batch fadd by a value
       */
      memcpy(dst1, src, MAXNUM*sizeof(float));
      _batch_fmadd = _batch_fmadd_cpu;
      t = clock();
        _batch_fmadd(dst1, dst1, MAXNUM, 1.0, 0.0f);
        cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
      printf("\nfadd cpu:  %f ms\n", cpu*1000.0f);

      if (simd)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmadd = GLUE(_batch_fmadd, SIMD);
         t = clock();
           _batch_fmadd(dst2, dst2, MAXNUM, 1.0f, 0.0f);
           eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
         printf("fadd %s:  %f ms - cpu x %2.1f\n",  MKSTR(SIMD), eps*1000.0f, cpu/eps);
         TESTF("float fadd "MKSTR(SIMD), dst1, dst2);
      }
      if (simd2)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmadd = GLUE(_batch_fmadd, SIMD2);
         t = clock();
           _batch_fmadd(dst2, dst2, MAXNUM, 1.0f, 0.0f);
           eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
         printf("fadd "MKSTR(SIMD2)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
         TESTF("float fadd "MKSTR(SIMD2), dst1, dst2);
      }
#if defined(__x86_64__)
      if (simd3)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         if (simd3 == 3) _batch_fmadd = GLUE(_batch_fmadd, FMA3);
         else _batch_fmadd = GLUE(_batch_fmadd, FMA4);
         t = clock();
           _batch_fmadd(dst2, dst2, MAXNUM, 1.0f, 0.0f);
           eps = (double)(clock() - t)/ CLOCKS_PER_SEC;

         if (simd3 == 3) {
            printf("fadd "MKSTR(FMA3)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
            TESTF("fadd "MKSTR(FMA3), dst1, dst2);
         } else {
            printf("fadd "MKSTR(FMA4)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
            TESTF("fadd "MKSTR(FMA4), dst1, dst2);
         }
      }
#endif

      /*
       * batch fmadd by a value
       */
      printf("\nfixed volume:");
      memcpy(dst1, src, MAXNUM*sizeof(float));
      _batch_fmadd = _batch_fmadd_cpu;
      t = clock();
        _batch_fmadd(dst1, dst1, MAXNUM, FACTOR, 0.0f);
        cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
      printf("\nfmadd cpu: %f ms\n", cpu*1000.0f);

      if (simd)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmadd = GLUE(_batch_fmadd, SIMD);
         t = clock();
           _batch_fmadd(dst2, dst2, MAXNUM, FACTOR, 0.0f);
           eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
         printf("fmadd %s: %f ms - cpu x %2.1f\n", MKSTR(SIMD), eps*1000.0f, cpu/eps);
         TESTF("float fadd+fmadd simd", dst1, dst2);
      }
      if (simd2)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmadd = GLUE(_batch_fmadd, SIMD2);
         t = clock();
           _batch_fmadd(dst2, dst2, MAXNUM, FACTOR, 0.0f);
           eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
         printf("fmadd "MKSTR(SIMD2)": %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
         TESTF("float fadd+fmadd "MKSTR(SIMD2), dst1, dst2);
      }
#if defined(__x86_64__)
      if (simd3)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         if (simd3 == 3) _batch_fmadd = GLUE(_batch_fmadd, FMA3);
         else _batch_fmadd = GLUE(_batch_fmadd, FMA4);
         t = clock();
           _batch_fmadd(dst2, dst2, MAXNUM, FACTOR, 0.0f);
           eps = (double)(clock() - t)/ CLOCKS_PER_SEC;

         if (simd3 == 3) {
            printf("float fadd+fmadd "MKSTR(FMA3)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
            TESTF("float fadd+fmadd "MKSTR(FMA3), dst1, dst2);
         } else {
            printf("float fadd+fmadd "MKSTR(FMA4)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
            TESTF("float fadd+fmadd "MKSTR(FMA4), dst1, dst2);
         }
      }
#endif

      /*
       * batch fmadd by a value, with a volume step
       */
      printf("\nwith a volume ramp:");
      memcpy(dst1, src, MAXNUM*sizeof(float));
      _batch_fmadd = _batch_fmadd_cpu;
      t = clock();
        _batch_fmadd(dst1, dst1, MAXNUM, FACTOR, VSTEP);
        cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
      printf("\nfmadd cpu: %f ms\n", cpu*1000.0f);

      if (simd)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmadd = GLUE(_batch_fmadd, SIMD);
         t = clock();
           _batch_fmadd(dst2, dst2, MAXNUM, FACTOR, VSTEP);
           eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
         printf("fmadd %s: %f ms - cpu x %2.1f\n", MKSTR(SIMD), eps*1000.0f, cpu/eps);
         TESTF("float fadd+fmadd simd", dst1, dst2);
      }
      if (simd2)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmadd = GLUE(_batch_fmadd, SIMD2);
         t = clock();
           _batch_fmadd(dst2, dst2, MAXNUM, FACTOR, VSTEP);
           eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
         printf("fmadd "MKSTR(SIMD2)": %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
         TESTF("float fadd+fmadd "MKSTR(SIMD2), dst1, dst2);
      }
#if defined(__x86_64__)
      if (simd3)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         if (simd3 == 3) _batch_fmadd = GLUE(_batch_fmadd, FMA3);
         else _batch_fmadd = GLUE(_batch_fmadd, FMA4);
         t = clock();
           _batch_fmadd(dst2, dst2, MAXNUM, FACTOR, VSTEP);
           eps = (double)(clock() - t)/ CLOCKS_PER_SEC;

         if (simd3 == 3) {
            printf("float fadd+fmadd "MKSTR(FMA3)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
            TESTF("float fadd+fmadd "MKSTR(FMA3), dst1, dst2);
         } else {
            printf("float fadd+fmadd "MKSTR(FMA4)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
            TESTF("float fadd+fmadd "MKSTR(FMA4), dst1, dst2);
         }
      }
#endif

      /*
       * batch fmul by a value for floats
       */
      memcpy(dst1, src, MAXNUM*sizeof(float));
      _batch_fmul_value = _batch_fmul_value_cpu;
      t = clock();
        _batch_fmul_value(dst1, dst1, sizeof(float), MAXNUM, FACTOR);
        cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
      printf("\nfmul cpu:  %f ms\n", cpu*1000.0f);

      if (simd)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmul_value = GLUE(_batch_fmul_value, SIMD);
         t = clock();
           _batch_fmul_value(dst2, dst2, sizeof(float), MAXNUM, FACTOR);
           eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
         printf("fmul %s:  %f ms - cpu x %2.1f\n", MKSTR(SIMD), eps*1000.0f, cpu/eps);
         TESTF("float fmul simd", dst1, dst2);
      }
      if (simd2)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmul_value = GLUE(_batch_fmul_value, SIMD2);
         t = clock();
           _batch_fmul_value(dst2, dst2, sizeof(float), MAXNUM, FACTOR);
           eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
         printf("fmul "MKSTR(SIMD2)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
         TESTF("float fmul "MKSTR(SIMD2), dst1, dst2);
      }

      /*
       * batch round floats
       */
      memcpy(dst1, src, MAXNUM*sizeof(float));
      _batch_roundps = _batch_roundps_cpu;
      t = clock();
        _batch_roundps(dst1, dst1, MAXNUM);
        cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
      printf("\nround cpu:  %f\n", cpu*1000.0f);
      if (simd)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_roundps = GLUE(_batch_roundps, SIMD);
         t = clock();
           _batch_roundps(dst2, dst2, MAXNUM);
           eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
         printf("round %s:  %f ms - cpu x %2.1f\n", MKSTR(SIMD), eps*1000.0f, cpu/eps);
         TESTF("round "MKSTR(SIMD), dst1, dst2);
      }
      if (simd4)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_roundps = GLUE(_batch_roundps, SIMD4);
         t = clock();
           _batch_roundps(dst2, dst2, MAXNUM);
           eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
         printf("round %s:  %f ms - cpu x %2.1f\n", MKSTR(SIMD4), eps*1000.0f, cpu/eps);
         TESTF("round "MKSTR(SIMD4), dst1, dst2);
      }

      /*
       * batch RMS calulculation
       */
      _batch_get_average_rms = _batch_get_average_rms_cpu;
      t = clock();
        _batch_get_average_rms(src, MAXNUM, &rms1, &peak1);
        cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
      printf("\nrms cpu:  %f\n", cpu*1000.0f);

	if (simd)
      {
         _batch_get_average_rms = GLUE(_batch_get_average_rms, SIMD);
         t = clock();
           _batch_get_average_rms(src, MAXNUM, &rms1, &peak1);
           eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
         printf("rms %s:  %f ms - cpu x %2.1f\n", MKSTR(SIMD), eps*1000.0f, cpu/eps);

         if (simd2)
         {
            _batch_get_average_rms = GLUE(_batch_get_average_rms, SIMD1);
            t = clock();
              _batch_get_average_rms(src, MAXNUM, &rms2, &peak2);
              eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("rms "MKSTR(SIMD1)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
            if (rms1 != rms2) {
               printf(" | rms1: %f, rms2: %f - %f (%5.4f%%)\n", rms1, rms2, rms1-rms2, 100.0f*fabsf((rms1-rms2)/rms1));
            }
            if (peak1 != peak2) {
               printf(" | peak1: %f, peak2: %f - %f (%5.4f%%)\n", peak1, peak2, peak1-peak2, fabsf((peak1-peak2)/peak1));
            }
         }
      }

      /*
       * batch freqfilter calulculation
       */
      memset(&flt, 0, sizeof(_aaxRingBufferFreqFilterData));
      flt.freqfilter = &history;
      flt.fs = 44100.0f;
      flt.run = _freqfilter_run;
      flt.high_gain = 1.0f;
      flt.low_gain = 0.0f;
      flt.no_stages = 4;
      flt.Q = 2.5f;
      flt.type = LOWPASS;
#if 1
      flt.state = AAX_BUTTERWORTH;
      _aax_butterworth_compute(2200.0f, &flt);
#else
      flt.state = AAX_BESSEL;
      _aax_bessel_compute(2200.0f, &flt);
#endif

      memset(&history, 0, sizeof(history));
      _batch_freqfilter_float = _batch_freqfilter_iir_float_cpu;
      t = clock();
        _batch_freqfilter_float(dst1, src, 0, MAXNUM, &flt);
        cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
      printf("\nfreqfilter cpu:  %f\n", cpu*1000.0f);

      if (simd)
      {
         memset(&history, 0, sizeof(history));
         _batch_freqfilter_float = GLUE(_batch_freqfilter_float, SIMD);
         t = clock();
           _batch_freqfilter_float(dst2, src, 0, MAXNUM, &flt);
           eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
         printf("freqfilter %s:  %f ms - cpu x %2.1f\n", MKSTR(SIMD), eps*1000.0f, cpu/eps);
         TESTF("freqfilter "MKSTR(SIMD), dst1, dst2);

         if (simd2)
         {
            memset(&history, 0,sizeof(history));
            _batch_freqfilter_float = GLUE(_batch_freqfilter_float, SIMD1);
            t = clock();
              _batch_freqfilter_float(dst2, src, 0, MAXNUM, &flt);
              eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("freqfilter "MKSTR(SIMD1)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
            TESTF("freqfilter "MKSTR(SIMD1), dst1, dst2);
         }
      }

      /*
       * batch fmul by a value for doubles
       */
      dsrc = (double*)src;
      ddst1 = (double*)dst1;
      ddst2 = (double*)dst2;

      for (i=0; i<MAXNUM; ++i) {
         dsrc[i] = (double)rand()/(double)(RAND_MAX/(1<<24));
      }
      memcpy(ddst1, dsrc, MAXNUM*sizeof(double));

      _batch_fmul_value_cpu(ddst1, ddst1, sizeof(double), MAXNUM, FACTOR);
      if (simd2)
      {
         memcpy(ddst2, dsrc, MAXNUM*sizeof(double));
         GLUE(_batch_fmul_value, SIMD2)(ddst2, ddst2, sizeof(double), MAXNUM, FACTOR);
         TESTLF("double fmul "MKSTR(SIMD2), (float)ddst1, (float)ddst2);
      }

      /*
       * waveform generation 
       */
      t = clock();
      _aax_generate_waveform_float = _aax_generate_waveform_cpu;
      _aax_generate_waveform_float(dst1, MAXNUM, FREQ, PHASE, WAVE_TYPE);
      cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
      printf("\ngenerate waveform cpu:  %f ms\n", cpu*1000.0f);

      if (simd)
      {
         t = clock();
         _aax_generate_waveform_float = GLUE(_aax_generate_waveform, SIMD);
         _aax_generate_waveform_float(dst2, MAXNUM, FREQ, PHASE, WAVE_TYPE);
         eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
         printf("generate waveform "MKSTR(SIMD)": %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
         TESTFN("waveform "MKSTR(SIMD), dst1, dst2, 1e-3f);
      }

      if (simd2)
      {
         t = clock();
         _aax_generate_waveform_float = GLUE(_aax_generate_waveform, SIMD1);
         _aax_generate_waveform_float(dst2, MAXNUM, FREQ, PHASE, WAVE_TYPE);
         eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
         printf("generate waveform "MKSTR(SIMD1)": %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
         TESTFN("waveform "MKSTR(SIMD1), dst1, dst2, 1e-3f);
      }

      if (simd5)
      {
         t = clock();
         _aax_generate_waveform_float = GLUE(_aax_generate_waveform, SIMD5);
         _aax_generate_waveform_float(dst2, MAXNUM, FREQ, PHASE, WAVE_TYPE);
         eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
         printf("generate waveform "MKSTR(SIMD5)": %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
         TESTFN("waveform "MKSTR(SIMD5), dst1, dst2, 1e-3f);
      }
   }

   _aax_aligned_free(dst2);
   _aax_aligned_free(dst1);
   _aax_aligned_free(src);

   return 0;
}
