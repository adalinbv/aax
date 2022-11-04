
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
#define MAXNUM		1024


#define DIFF(a,b)	((fabsf(a)-fabsf(b))/fabsf(a))
#define TESTFN(a,d1,d2,m) do { double max = 0.0f; \
   for (i=0; i<MAXNUM; ++i) { double diff = DIFF(d1[i], d2[i]); \
       if (diff > max) { max = diff; } } \
   if (max > 1e-4f) printf("\t| max error <= %3.2f%%\n", max*100.0f); \
   else if (max > 0) printf("\t| max error < 0.01%%\n"); else printf("\n"); \
} while(0)
#define TESTF(a,d1,d2)	TESTFN(a,d1,d2,4.0f)

#define TESTLF(a,d1,d2) do { double max = 0.0; \
   for (i=0; i<MAXNUM; ++i) { double diff = DIFF(d1[i], d2[i]); \
      if (diff > max) max = diff; } \
   if (max > 1e-4) printf("\t| error <= %3.2f%%\n", max*100.0); \
   else if (max > 0) printf("\t| error, but marginal\n"); else printf("\n"); \
} while(0)

#define TIMEFN(a,b,c) do { int i = (c); \
   _aaxTimerStart(ts); do { (a); } while(--i); (b) = _aaxTimerElapsed(ts); \
} while(0)

#define PHASE			0.1f
#define FREQ			220.0f

#define __MKSTR(X)		#X
#define MKSTR(X)		__MKSTR(X)
#define __GLUE(FUNC,NAME)	FUNC ## _ ## NAME
#define GLUE(FUNC,NAME)		__GLUE(FUNC,NAME)

extern _batch_fmadd_proc _batch_fmadd;
extern _batch_cvt_to_proc _batch_atanps;
extern _batch_cvt_to_proc _batch_roundps;
extern _batch_mul_value_proc _batch_fmul_value;
extern _batch_get_average_rms_proc _batch_get_average_rms;
extern _batch_freqfilter_float_proc _batch_freqfilter_float;
extern _aax_generate_waveform_proc _aax_generate_waveform_float;
extern _batch_convolution_proc _batch_convolution;

void _batch_atan_cpu(void_ptr, const_void_ptr, size_t);
void _batch_freqfilter_float_sse_vex(float32_ptr dptr, const_float32_ptr sptr, int t, size_t num, void *flt);

#if defined(__i386__)
# define CPU    "cpu"
# define SIMD   sse2
# define SIMD1  sse2
# define SIMD3	sse3
# define SIMD2  sse2
# define SIMD4  sse4
# define FMA3	sse2
char _aaxArchDetectSSE3();
char _aaxArchDetectSSE2();
char _aaxArchDetectSSE4();
#elif defined(__x86_64__)
# define CPU   "cpu+sse2"
# define SIMD   sse2
# define SIMD1  sse_vex
# define SIMD3	sse3
# define SIMD2  avx
# define SIMD4  sse4
# define FMA3   fma3
# define CPUID_FEAT_ECX_FMA3    (1 << 12)
char _aaxArchDetectSSE3();
char _aaxArchDetectSSE4();
char _aaxArchDetectAVX();
char _aaxArchDetectFMA3();
char check_extcpuid_ecx(unsigned int);
char check_cpuid_ecx(unsigned int);
#elif defined(__arm__) || defined(_M_ARM)
# define CPU    "cpu"
# define SIMD   vfpv3
# define SIMD1  vfpv4
# define SIMD2  vfpv4
# define SIMD3	vfpv3
# define SIMD4  neon
# define FMA3   neon
char _aaxArchDetectVFPV4();
char _aaxArchDetectNEON();
#endif

int main()		// x86		ARM
{			// -------	-------
   char simd = 0;	// SSE2		VFPV3
   char simd1 = 0;	// SSE_VEX      VFPV4
   char simd2 = 0;	// AVX		NEON
// char simd3 = 0;	// SSE3
   char simd4 = 0;	// SSE4
#if defined(__x86_64__)
#endif
   char fma = 0;	// FMA3		VFPV4
   float freq_factor;
   _aaxTimer *ts;
   _data_t *buf;

#if defined(__i386__)
   simd = _aaxArchDetectSSE2();
// simd3 = _aaxArchDetectSSE3();
   simd4 = _aaxArchDetectSSE4();
#elif defined(__x86_64__)
   simd = 1;
   simd1 = _aaxArchDetectAVX();
   simd2 = _aaxArchDetectAVX();
// simd3 = _aaxArchDetectSSE3();
   simd4 = _aaxArchDetectSSE4();
   fma = _aaxArchDetectFMA3() ? 3 : 0;
#elif defined(__arm__) || defined(_M_ARM)
   simd = _aaxArchDetectVFPV3();
   simd1 = _aaxArchDetectVFPV4();
   simd2 = _aaxArchDetectNeon();
   simd4 = _aaxArchDetectNeon();
   fma = _aaxArchDetectVFPV4();
#endif

   ts = _aaxTimerCreate();
   srand(time(NULL));

   buf = _aaxDataCreate(4, MAXNUM*sizeof(double)/sizeof(float), sizeof(float));
   if (buf)
   {
      _aaxRingBufferFreqFilterHistoryData history;
      _aaxRingBufferFreqFilterData flt;
      float *src, *dst1, *dst2, *dst3;
      float rms1, rms2, peak1, peak2;
      double cpu, cpu2, eps;
      int i;

      src = (float*)_aaxDataGetData(buf, 0);
      dst1 = (float*)_aaxDataGetData(buf, 1);
      dst2 = (float*)_aaxDataGetData(buf, 2);
      dst3 = (float*)_aaxDataGetData(buf, 3);

      for (i=0; i<MAXNUM; ++i) {
         src[i] = (double)(1<<23) * (double)rand()/(double)(RAND_MAX);
      }
      memset(dst1, 0, MAXNUM*sizeof(double));
      memset(dst2, 0, MAXNUM*sizeof(double));

      /*
       * batch fadd by a value
       */
      memcpy(dst1, src, MAXNUM*sizeof(float));
      _batch_fmadd = _batch_fmadd_cpu;

      _batch_fmadd(dst1, dst1, MAXNUM, 1.0, 0.0f);
      _batch_fmadd(dst1, dst1, MAXNUM, 1.0, 0.0f);
      memcpy(dst1, src, MAXNUM*sizeof(float));

      TIMEFN(_batch_fmadd(dst1, dst1, MAXNUM, 1.0, 0.0f), cpu, MAXNUM);
      printf("\nfadd " CPU ":\t%f ms\n", cpu*1e3);

      if (simd)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmadd = GLUE(_batch_fmadd, SIMD);

         _batch_fmadd(dst2, dst2, MAXNUM, 1.0, 0.0f);
         _batch_fmadd(dst2, dst2, MAXNUM, 1.0, 0.0f);
         memcpy(dst2, src, MAXNUM*sizeof(float));

         TIMEFN(_batch_fmadd(dst2, dst2, MAXNUM, 1.0f, 0.0f), eps, MAXNUM);
         printf("fadd %s:\t%f ms - cpu x %3.2f",  MKSTR(SIMD), eps*1e3, cpu/eps);
         TESTF("fadd "MKSTR(SIMD), dst1, dst2);
      }
      if (simd2)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmadd = GLUE(_batch_fmadd, SIMD2);

         _batch_fmadd(dst2, dst2, MAXNUM, 1.0, 0.0f);
         _batch_fmadd(dst2, dst2, MAXNUM, 1.0, 0.0f);
         memcpy(dst2, src, MAXNUM*sizeof(float));

         TIMEFN(_batch_fmadd(dst2, dst2, MAXNUM, 1.0f, 0.0f), eps, MAXNUM);
         printf("fadd "MKSTR(SIMD2)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTF("fadd "MKSTR(SIMD2), dst1, dst2);
      }
      if (fma)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmadd = GLUE(_batch_fmadd, FMA3);

         _batch_fmadd(dst2, dst2, MAXNUM, 1.0, 0.0f);
         _batch_fmadd(dst2, dst2, MAXNUM, 1.0, 0.0f);
         memcpy(dst2, src, MAXNUM*sizeof(float));

         TIMEFN(_batch_fmadd(dst2, dst2, MAXNUM, 1.0f, 0.0f), eps, MAXNUM);
         printf("fadd "MKSTR(FMA3)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTF("fadd "MKSTR(FMA3), dst1, dst2);
      }

      /*
       * batch fmadd by a value
       */
      printf("\nfixed volume:");
      memcpy(dst1, src, MAXNUM*sizeof(float));
      _batch_fmadd = _batch_fmadd_cpu;

      TIMEFN(_batch_fmadd(dst1, dst1, MAXNUM, FACTOR, 0.0f), eps, MAXNUM);
      printf("\nfmadd " CPU ":\t%f ms\n", cpu*1e3);

      if (simd)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmadd = GLUE(_batch_fmadd, SIMD);

         TIMEFN(_batch_fmadd(dst2, dst2, MAXNUM, FACTOR, 0.0f), eps, MAXNUM);
         printf("fmadd %s:\t%f ms - cpu x %3.2f", MKSTR(SIMD), eps*1e3, cpu/eps);
         TESTF("fmadd simd", dst1, dst2);
      }
      if (simd2)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmadd = GLUE(_batch_fmadd, SIMD2);

         TIMEFN(_batch_fmadd(dst2, dst2, MAXNUM, FACTOR, 0.0f), eps, MAXNUM);
         printf("fmadd "MKSTR(SIMD2)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTF("fmadd "MKSTR(SIMD2), dst1, dst2);
      }
      if (fma)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmadd = GLUE(_batch_fmadd, FMA3);

         TIMEFN(_batch_fmadd(dst2, dst2, MAXNUM, FACTOR, 0.0f), eps, MAXNUM);
         printf("fmadd "MKSTR(FMA3)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTF("fmadd "MKSTR(FMA3), dst1, dst2);
      }

      /*
       * batch fmadd by a value, with a volume step
       */
      printf("\nwith a volume ramp:");
      memcpy(dst1, src, MAXNUM*sizeof(float));
      _batch_fmadd = _batch_fmadd_cpu;

      TIMEFN(_batch_fmadd(dst1, dst1, MAXNUM, FACTOR, VSTEP), cpu, MAXNUM);
      printf("\nfmadd " CPU ":\t%f ms\n", cpu*1e3);

      if (simd)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmadd = GLUE(_batch_fmadd, SIMD);

         TIMEFN(_batch_fmadd(dst2, dst2, MAXNUM, FACTOR, VSTEP), eps, MAXNUM);
         printf("fmadd %s:\t%f ms - cpu x %3.2f", MKSTR(SIMD), eps*1e3, cpu/eps);
         TESTF("fmadd simd", dst1, dst2);
      }
      if (simd2)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmadd = GLUE(_batch_fmadd, SIMD2);

         TIMEFN(_batch_fmadd(dst2, dst2, MAXNUM, FACTOR, VSTEP), eps, MAXNUM);
         printf("fmadd "MKSTR(SIMD2)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTF("fmadd "MKSTR(SIMD2), dst1, dst2);
      }
      if (fma)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmadd = GLUE(_batch_fmadd, FMA3);

         TIMEFN(_batch_fmadd(dst2, dst2, MAXNUM, FACTOR, VSTEP), eps, MAXNUM);
         printf("fmadd "MKSTR(FMA3)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTF("fmadd "MKSTR(FMA3), dst1, dst2);
      }

      /*
       * batch fmul by a value for floats
       */
      memcpy(dst1, src, MAXNUM*sizeof(float));
      _batch_fmul_value = _batch_fmul_value_cpu;

      TIMEFN(_batch_fmul_value(dst1, dst1, sizeof(float), MAXNUM, FACTOR), cpu, MAXNUM);
      printf("\nfmul " CPU ":\t%f ms\n", cpu*1e3);

      if (simd)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmul_value = GLUE(_batch_fmul_value, SIMD);

         TIMEFN(_batch_fmul_value(dst2, dst2, sizeof(float), MAXNUM, FACTOR), eps, MAXNUM);
         printf("fmul %s:\t%f ms - cpu x %3.2f", MKSTR(SIMD), eps*1e3, cpu/eps);
         TESTF("float fmul simd", dst1, dst2);
      }
      if (simd2)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_fmul_value = GLUE(_batch_fmul_value, SIMD2);

         TIMEFN(_batch_fmul_value(dst2, dst2, sizeof(float), MAXNUM, FACTOR), eps, MAXNUM);
         printf("fmul "MKSTR(SIMD2)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTF("float fmul "MKSTR(SIMD2), dst1, dst2);
      }

      /*
       * batch fmul by a value for doubles
       */
#if 0
      dsrc = (double*)src;
      ddst1 = (double*)dst1;
      ddst2 = (double*)dst2;

      for (i=0; i<MAXNUM; ++i) {
         dsrc[i] = (double)rand()/(double)(RAND_MAX/(1<<24));
      }
      memcpy(ddst1, dsrc, MAXNUM*sizeof(double));

      TIMEFN(_batch_fmul_value_cpu(ddst1, ddst1, sizeof(double), MAXNUM, FACTOR), cpu, MAXNUM);
      printf("\ndmul " CPU ":\t%f ms\n", cpu*1e3);

      if (simd)
      {
         memcpy(ddst2, dsrc, MAXNUM*sizeof(double));
         _batch_fmul_value = GLUE(_batch_fmul_value, SIMD);

         TIMEFN(_batch_fmul_value(dst2, ddst2, sizeof(double), MAXNUM, FACTOR), eps, MAXNUM);
         printf("dmul "MKSTR(SIMD)":\%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTLF("double fmul "MKSTR(SIMD), (float)ddst1, (float)ddst2);
      }

      if (simd2)
      {
         memcpy(ddst2, dsrc, MAXNUM*sizeof(double));
         _batch_fmul_value = GLUE(_batch_fmul_value, SIMD2);

         TIMEFN(_batch_fmul_value(dst2, ddst2, sizeof(double), MAXNUM, FACTOR), ep, MAXNUM);
         printf("dmul "MKSTR(SIMD2)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTLF("double fmul "MKSTR(SIMD2), (float)ddst1, (float)ddst2);
      }
#endif

      /*
       * batch round floats
       */
      memcpy(dst1, src, MAXNUM*sizeof(float));
      _batch_roundps = _batch_roundps_cpu;

      TIMEFN(_batch_roundps(dst1, dst1, MAXNUM), cpu, MAXNUM);
      printf("\nround " CPU ":\t%f\n", cpu*1e3);
      if (simd)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_roundps = GLUE(_batch_roundps, SIMD);

         TIMEFN(_batch_roundps(dst2, dst2, MAXNUM), eps, MAXNUM);
         printf("round %s:\t%f ms - cpu x %3.2f", MKSTR(SIMD), eps*1e3, cpu/eps);
         TESTF("round "MKSTR(SIMD), dst1, dst2);
      }
      if (simd2)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_roundps = GLUE(_batch_roundps, SIMD1);

         TIMEFN(_batch_roundps(dst2, dst2, MAXNUM), eps, MAXNUM);
         printf("round %s:\t%f ms - cpu x %3.2f", MKSTR(SIMD1), eps*1e3, cpu/eps);
         TESTF("round "MKSTR(SIMD1), dst1, dst2);
      }
#if !defined(__x86_64__) && !(defined(__arm__) || defined(_M_ARM))
      if (simd2)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_roundps = GLUE(_batch_roundps, SIMD2);

         TIMEFN(_batch_roundps(dst2, dst2, MAXNUM), eps, MAXNUM);
         printf("round %s:\t%f ms - cpu x %3.2f", MKSTR(SIMD2), eps*1e3, cpu/eps);
         TESTF("round "MKSTR(SIMD2), dst1, dst2);
      }
#endif
      if (simd4)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_roundps = GLUE(_batch_roundps, SIMD4);

         TIMEFN(_batch_roundps(dst2, dst2, MAXNUM), eps, MAXNUM);
         printf("round %s:\t%f ms - cpu x %3.2f", MKSTR(SIMD4), eps*1e3, cpu/eps);
         TESTF("round "MKSTR(SIMD4), dst1, dst2);
      }

      /*
       * batch atan floats
       */
      memcpy(dst1, src, MAXNUM*sizeof(float));

      TIMEFN(_batch_atan_cpu(dst1, dst1, MAXNUM), cpu, MAXNUM);
      printf("\natanf:\t\t%f ms\n", cpu*1e3);

      memcpy(dst2, src, MAXNUM*sizeof(float));
      _batch_atanps = _batch_atanps_cpu;

      TIMEFN(_batch_atanps(dst2, dst2, MAXNUM), eps, MAXNUM);
      printf("atan " CPU ":\t%f ms - atanf x %3.2f", eps*1e3, cpu/eps);
      TESTF("atan "MKSTR(SIMD), dst1, dst2);

      if (simd)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_atanps = GLUE(_batch_atanps, SIMD);

         TIMEFN(_batch_atanps(dst2, dst2, MAXNUM), eps, MAXNUM);
         printf("atan %s:\t%f ms - atanf x %3.2f", MKSTR(SIMD), eps*1e3, cpu/eps);
         TESTF("atan "MKSTR(SIMD), dst1, dst2);
      }
      if (simd2)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_atanps = GLUE(_batch_atanps, SIMD2);

         TIMEFN(_batch_atanps(dst2, dst2, MAXNUM), eps, MAXNUM);
         printf("atan %s:\t%f ms - atanf x %3.2f", MKSTR(SIMD2), eps*1e3, cpu/eps);
         TESTF("atan "MKSTR(SIMD2), dst1, dst2);
      }
      if (fma)
      {
         memcpy(dst2, src, MAXNUM*sizeof(float));
         _batch_atanps = GLUE(_batch_atanps, FMA3);

         TIMEFN(_batch_atanps(dst2, dst2, MAXNUM), eps, MAXNUM);
         printf("atan %s:\t%f ms - atanf x %3.2f", MKSTR(FMA3), eps*1e3, cpu/eps);
         TESTF("atan "MKSTR(FMA3), dst1, dst2);
      }

      /*
       * batch RMS calulculation
       */
      _batch_get_average_rms = _batch_get_average_rms_cpu;

      TIMEFN(_batch_get_average_rms(src, MAXNUM, &rms1, &peak1), cpu, MAXNUM);
      printf("\nrms " CPU ":\t%f\n", cpu*1e3);

      if (simd)
      {
         _batch_get_average_rms = GLUE(_batch_get_average_rms, SIMD);

         TIMEFN(_batch_get_average_rms(src, MAXNUM, &rms1, &peak1), eps, MAXNUM);
         printf("rms %s:\t%f ms - cpu x %3.2f\n", MKSTR(SIMD), eps*1e3, cpu/eps);

         if (simd2)
         {
            float rmse, peake;

            _batch_get_average_rms = GLUE(_batch_get_average_rms, SIMD2);

            TIMEFN(_batch_get_average_rms(src, MAXNUM, &rms2, &peak2), eps, MAXNUM);
            printf("rms "MKSTR(SIMD2)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
            rmse = fabsf(rms1-rms2);
            peake = fabsf(peak1-peak2);
            if (rmse > 1e-4f || peake > 1e-4f)
             {
               printf("\t| error");
               if (rmse > 1e-4f) {
                  printf(" rms: %3.2f%% ", 100.0f*fabsf((rms1-rms2)/rms1));
               }
               if (rmse > 1e-4f) {
                  printf(" peak: %3.2f%%", 100.0f*fabsf((peak1-peak2)/peak1));
               }
            }
            printf("\n");
         }

         if (fma)
         {
            float rmse, peake;

            _batch_get_average_rms = GLUE(_batch_get_average_rms, FMA3);

            TIMEFN(_batch_get_average_rms(src, MAXNUM, &rms2, &peak2), eps, MAXNUM);
            printf("rms "MKSTR(FMA3)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
            rmse = fabsf(rms1-rms2);
            peake = fabsf(peak1-peak2);
            if (rmse > 1e-4f || peake > 1e-4f)
             {
               printf("\t| error");
               if (rmse > 1e-4f) {
                  printf(" rms: %3.2f%% ", 100.0f*fabsf((rms1-rms2)/rms1));
               }
               if (rmse > 1e-4f) {
                  printf(" peak: %3.2f%%", 100.0f*fabsf((peak1-peak2)/peak1));
               }
            }
            printf("\n");
         }
      }

      /*
       * resample
       * Cubic threshold is 0.25
       */
      freq_factor = 0.2f;
      _batch_resample_float = _batch_resample_float_cpu;

      printf("\n== Resample:\n");
      TIMEFN(_batch_resample_float(dst1, src, 0, MAXNUM, 0.0, freq_factor), cpu, MAXNUM);
      printf("cubic " CPU ":\t%f ms\n", cpu*1e3);

      if (simd)
      {
         _batch_resample_float = GLUE(_batch_resample_float, SIMD);

         TIMEFN(_batch_resample_float(dst2, src, 0, MAXNUM, 0.0, freq_factor), eps, MAXNUM);
         printf("cubic "MKSTR(SIMD)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTFN("cubic "MKSTR(SIMD), dst1, dst2, 1e-3f);
      }

      if (simd1)
      {
         _batch_resample_float = GLUE(_batch_resample_float, SIMD1);

         TIMEFN(_batch_resample_float(dst2, src, 0, MAXNUM, 0.0, freq_factor), eps, MAXNUM);
         printf("cubic "MKSTR(SIMD1)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTFN("cubic "MKSTR(SIMD1), dst1, dst2, 1e-3f);
      }

      if (fma)
      {
         _batch_resample_float = GLUE(_batch_resample_float, FMA3);

         TIMEFN(_batch_resample_float(dst2, src, 0, MAXNUM, 0.0, freq_factor), eps, MAXNUM);
         printf("cubic "MKSTR(FMA3)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTFN("cubic "MKSTR(FMA3), dst1, dst2, 1e-3f);
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
      flt.state = AAX_BUTTERWORTH;
      _aax_butterworth_compute(2200.0f, &flt);

      printf("\n== Butterworth filter:\n");
      memset(&history, 0, sizeof(history));
      _batch_freqfilter_float = _batch_freqfilter_float_cpu;

      TIMEFN(_batch_freqfilter_float(dst1, src, 0, MAXNUM, &flt), cpu, MAXNUM);
      printf("freq " CPU ":\t%f\n", cpu*1e3);
      cpu2 = cpu;

      if (simd)
      {
         memset(&history, 0, sizeof(history));
         _batch_freqfilter_float = GLUE(_batch_freqfilter_float, SIMD);

         TIMEFN(_batch_freqfilter_float(dst2, src, 0, MAXNUM, &flt), eps, MAXNUM);
         printf("freq %s:\t%f ms - cpu x %3.2f", MKSTR(SIMD), eps*1e3, cpu/eps);
         TESTF("freq "MKSTR(SIMD), dst1, dst2);
      }
      if (simd2)
      {
         memset(&history, 0,sizeof(history));
         _batch_freqfilter_float = GLUE(_batch_freqfilter_float, SIMD1);

         TIMEFN(_batch_freqfilter_float(dst2, src, 0, MAXNUM, &flt), eps, MAXNUM);
         printf("freq "MKSTR(SIMD1)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTF("freq "MKSTR(SIMD1), dst1, dst2);
      }
      if (fma)
      {
         memset(&history, 0,sizeof(history));
         _batch_freqfilter_float = GLUE(_batch_freqfilter_float, FMA3);

         TIMEFN(_batch_freqfilter_float(dst2, src, 0, MAXNUM, &flt), eps, MAXNUM);
         printf("freq "MKSTR(FMA3)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTF("freq "MKSTR(FMA3), dst1, dst2);
      }

      memset(&flt, 0, sizeof(_aaxRingBufferFreqFilterData));
      flt.freqfilter = &history;
      flt.fs = 44100.0f;
      flt.run = _freqfilter_run;
      flt.high_gain = 1.0f;
      flt.low_gain = 0.0f;
      flt.no_stages = 4;
      flt.Q = 2.5f;
      flt.type = LOWPASS;
      flt.state = AAX_BESSEL;
      _aax_bessel_compute(2200.0f, &flt);

      printf("\n== Bessel filter:\n");
      memset(&history, 0, sizeof(history));
      _batch_freqfilter_float = _batch_freqfilter_float_cpu;

      TIMEFN(_batch_freqfilter_float(dst1, src, 0, MAXNUM, &flt), cpu, MAXNUM);
      printf("freq " CPU ":\t%f - Butterworth x %3.2f\n", cpu*1e3, cpu2/cpu);

      if (simd)
      {
         memset(&history, 0, sizeof(history));
         _batch_freqfilter_float = GLUE(_batch_freqfilter_float, SIMD);

         TIMEFN(_batch_freqfilter_float(dst2, src, 0, MAXNUM, &flt), eps, MAXNUM);
         printf("freq %s:\t%f ms - cpu x %3.2f", MKSTR(SIMD), eps*1e3, cpu/eps);
         TESTF("freq "MKSTR(SIMD), dst1, dst2);
      }
      if (simd2)
      {
         memset(&history, 0,sizeof(history));
         _batch_freqfilter_float = GLUE(_batch_freqfilter_float, SIMD1);

         TIMEFN(_batch_freqfilter_float(dst2, src, 0, MAXNUM, &flt), eps, MAXNUM);
         printf("freq "MKSTR(SIMD1)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTF("freq "MKSTR(SIMD1), dst1, dst2);
      }
      if (fma)
      {
         memset(&history, 0,sizeof(history));
         _batch_freqfilter_float = GLUE(_batch_freqfilter_float, FMA3);

         TIMEFN(_batch_freqfilter_float(dst2, src, 0, MAXNUM, &flt), eps, MAXNUM);
         printf("freq "MKSTR(FMA3)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTF("freq "MKSTR(FMA3), dst1, dst2);
      }

      /*
       * waveform generation
       */
      printf("\n== waveform generation:\n");
      _aax_generate_waveform_float = _aax_generate_waveform_cpu;

      TIMEFN(_aax_generate_waveform_float(dst1, MAXNUM, FREQ, PHASE, WAVE_TYPE), cpu, MAXNUM);
      printf("wave " CPU ":\t%f ms\n", cpu*1e3);

      if (simd)
      {
         _aax_generate_waveform_float = GLUE(_aax_generate_waveform, SIMD);

         TIMEFN(_aax_generate_waveform_float(dst2, MAXNUM, FREQ, PHASE, WAVE_TYPE), eps, MAXNUM);
         printf("wave "MKSTR(SIMD)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTFN("wave "MKSTR(SIMD), dst1, dst2, 1e-3f);
      }

      if (simd2)
      {
         _aax_generate_waveform_float = GLUE(_aax_generate_waveform, SIMD2);

         TIMEFN(_aax_generate_waveform_float(dst2, MAXNUM, FREQ, PHASE, WAVE_TYPE), eps, MAXNUM);
         printf("wave "MKSTR(SIMD2)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTFN("wave "MKSTR(SIMD2), dst1, dst2, 1e-3f);
      }

#if 0
      if (fma)
      {
         _aax_generate_waveform_float = GLUE(_aax_generate_waveform, FMA3);

         TIMEFN(_aax_generate_waveform_float(dst2, MAXNUM, FREQ, PHASE, WAVE_TYPE), eps, MAXNUM);
         printf("wave "MKSTR(FMA3)":\t%f ms - cpu x %3.2f", eps*1e3, cpu/eps);
         TESTFN("wave "MKSTR(FMA3), dst1, dst2, 1e-3f);
      }
#endif

      /*
       * convolution
       * Note: makes use of _batch_fmadd
       */
      _batch_convolution = _batch_convolution_cpu;

      TIMEFN(_batch_convolution(dst1, dst2, src, MAXNUM/16, MAXNUM/8, 2, 1.0f, 0.0), cpu, MAXNUM);
      printf("\nconvolution (uses fastest fmadd):  %f ms\n", cpu*1e3);
   }

   _aaxDataDestroy(buf);
   _aaxTimerDestroy(ts);

   return 0;
}
