
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <base/types.h>
#include <base/geometry.h>
#include <base/timer.h>
#include <src/ringbuffer.h>
#include <src/dsp/dsp.h>
#include <src/software/cpu/arch2d_simd.h>
#include <arch.h>

#define VSTEP		-0.000039f
#define MAXNUM		(199*4096)
#define TESTF(a,d1,d2) { int n = 0; \
    for (i=0; i<MAXNUM; ++i) { \
        if (fabsf(d1[i]-d2[i]) > 4) { \
            printf(" # %s %i: %f != %f\n", a, i, d1[i], d2[i]); \
            if (++n == 8) break; \
        } } }

#define TESTLF(a,d1,d2) { int n = 0; \
    for (i=0; i<MAXNUM; ++i) { \
        if (fabsf(d1[i]-d2[i]) > 4) { \
            printf(" # %s %i: %f != %f\n", a, i, d1[i], d2[i]); \
            if (++n == 8) break; \
        } } }

#if defined(__i386__)
# define SIMD	sse2
# define SIMD2	sse2
# define SIMD4	sse4
char _aaxArchDetectSSE2();
char _aaxArchDetectSSE4();
#elif defined(__x86_64__)
# define SIMD   sse2
# define SIMD1	sse_vex
# define SIMD2	avx
# define SIMD4	sse4
# define FMA3	fma3
# define FMA4	fma4
# define CPUID_FEAT_ECX_FMA3	(1 << 12)
# define CPUID_FEAT_ECX_FMA4	(1 << 16)
char _aaxArchDetectSSE4();
char _aaxArchDetectAVX();
char check_extcpuid_ecx(unsigned int);
char check_cpuid_ecx(unsigned int);
#elif defined(__arm__) || defined(_M_ARM)
# define SIMD	neon
# define SIMD2	neon
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

float fast_sin_cpu(float);
float fast_sin_sse2(float);
float fast_sin_sse_vex(float);

extern _batch_fmadd_proc _batch_fmadd;
extern _batch_cvt_to_proc _batch_roundps;
extern _batch_mul_value_proc _batch_fmul_value;
extern _batch_get_average_rms_proc _batch_get_average_rms;
extern _batch_freqfilter_float_proc _batch_freqfilter_float;
extern float _harmonics[AAX_MAX_WAVE][_AAX_SYNTH_MAX_HARMONICS];

void _batch_freqfilter_float_sse_vex(float32_ptr dptr, const_float32_ptr sptr, int t, size_t num, void *flt);


int main()
{
    float *src, *dst1, *dst2;
    char simd = 0;
    char simd2 = 0;
    char simd3 = 0;
    char simd4 = 0;
    clock_t t;

#if defined(__i386__)
    simd = _aaxArchDetectSSE2();
    simd4 = _aaxArchDetectSSE4();
#elif defined(__x86_64__)
    simd = 1;
    simd2 = _aaxArchDetectAVX();
    simd4 = _aaxArchDetectSSE4();
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
        memcpy(dst2, src, MAXNUM*sizeof(float));
        _batch_fmadd = _batch_fmadd_cpu;
        t = clock();
          _batch_fmadd(dst2, dst2, MAXNUM, 1.0, 0.0f);
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
        }
        if (simd2)
        {
            memcpy(dst2, src, MAXNUM*sizeof(float));
            _batch_fmadd = GLUE(_batch_fmadd, SIMD2);
            t = clock();
              _batch_fmadd(dst2, dst2, MAXNUM, 1.0f, 0.0f);
              eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("fadd "MKSTR(SIMD2)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
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

            if (simd3 == 3) printf("fadd "MKSTR(FMA3)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
            else printf("fadd "MKSTR(FMA4)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
        }
#endif

        /*
         * batch fmadd by a value
         */
        printf("\nfixed volume:");
        memcpy(dst1, src, MAXNUM*sizeof(float));
        _batch_fmadd = _batch_fmadd_cpu;
        t = clock();
          _batch_fmadd(dst1, dst1, MAXNUM, 0.8723678263f, 0.0f);
          cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("\nfmadd cpu: %f ms\n", cpu*1000.0f);

        if (simd)
        {
            memcpy(dst2, src, MAXNUM*sizeof(float));
            _batch_fmadd = GLUE(_batch_fmadd, SIMD);
            t = clock();
              _batch_fmadd(dst2, dst2, MAXNUM, 0.8723678263f, 0.0f);
              eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("fmadd %s: %f ms - cpu x %2.1f\n", MKSTR(SIMD), eps*1000.0f, cpu/eps);
            TESTF("float fadd+fmadd simd", dst1, dst2);
        }
        if (simd2)
        {
            memcpy(dst2, src, MAXNUM*sizeof(float));
            _batch_fmadd = GLUE(_batch_fmadd, SIMD2);
            t = clock();
              _batch_fmadd(dst2, dst2, MAXNUM, 0.8723678263f, 0.0f);
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
              _batch_fmadd(dst2, dst2, MAXNUM, 0.8723678263f, 0.0f);
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
          _batch_fmadd(dst1, dst1, MAXNUM, 0.8723678263f, VSTEP);
          cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("\nfmadd cpu: %f ms\n", cpu*1000.0f);

        if (simd)
        {
            memcpy(dst2, src, MAXNUM*sizeof(float));
            _batch_fmadd = GLUE(_batch_fmadd, SIMD);
            t = clock();
              _batch_fmadd(dst2, dst2, MAXNUM, 0.8723678263f, VSTEP);
              eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("fmadd %s: %f ms - cpu x %2.1f\n", MKSTR(SIMD), eps*1000.0f, cpu/eps);
            TESTF("float fadd+fmadd simd", dst1, dst2);
        }
        if (simd2)
        {
            memcpy(dst2, src, MAXNUM*sizeof(float));
            _batch_fmadd = GLUE(_batch_fmadd, SIMD2);
            t = clock();
              _batch_fmadd(dst2, dst2, MAXNUM, 0.8723678263f, VSTEP);
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
              _batch_fmadd(dst2, dst2, MAXNUM, 0.8723678263f, VSTEP);
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
          _batch_fmul_value(dst1, dst1, sizeof(float), MAXNUM, 0.8723678263f);
          cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("\nfmul cpu:  %f ms\n", cpu*1000.0f);

        if (simd)
        {
            memcpy(dst2, src, MAXNUM*sizeof(float));
            _batch_fmul_value = GLUE(_batch_fmul_value, SIMD);
            t = clock();
              _batch_fmul_value(dst2, dst2, sizeof(float), MAXNUM, 0.8723678263f);
              eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("fmul %s:  %f ms - cpu x %2.1f\n", MKSTR(SIMD), eps*1000.0f, cpu/eps);
            TESTF("float fmul simd", dst1, dst2);
        }
        if (simd2)
        {
            memcpy(dst2, src, MAXNUM*sizeof(float));
            _batch_fmul_value = GLUE(_batch_fmul_value, SIMD2);
            t = clock();
              _batch_fmul_value(dst2, dst2, sizeof(float), MAXNUM, 0.8723678263f);
              eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("fmul "MKSTR(SIMD2)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
            TESTF("float fmul "MKSTR(SIMD2), dst1, dst2);
        }

        /*
         * batch round floats
         */
        _batch_roundps = _batch_roundps_cpu;
        memcpy(dst2, src, MAXNUM*sizeof(float));
        t = clock();
          _batch_roundps(src, src, MAXNUM);
          cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("\nround cpu:  %f\n", cpu*1000.0f);
        if (simd4)
        {
           _batch_roundps = _batch_roundps_sse4;
           t = clock();
              _batch_roundps(dst2, dst2, MAXNUM);
              eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("round %s:  %f ms - cpu x %2.1f\n", MKSTR(SIMD), eps*1000.0f, cpu/eps);
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
#ifdef SIMD1
                _batch_get_average_rms = GLUE(_batch_get_average_rms, SIMD1);
#else
                _batch_get_average_rms = GLUE(_batch_get_average_rms, SIMD);
#endif
                t = clock();
                  _batch_get_average_rms(src, MAXNUM, &rms2, &peak2);
                  eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
                printf("rms "MKSTR(SIMD1)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
                if (rms1 != rms2) {
                   printf(" | rms1: %f, rms2: %f - %f\n", rms1, rms2, rms1-rms2);
                }
                if (peak1 != peak2) {
                   printf(" | peak1: %f, peak2: %f - %f\n", peak1, peak2, peak1-peak2);
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
        flt.no_stages = 2;
        flt.state = AAX_BUTTERWORTH; // or AAX_BESSEL;
        flt.Q = 2.5f;
        flt.type = HIGHPASS;
        _aax_butterworth_compute(2200.0f, &flt);

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
#ifdef SIMD1
                _batch_freqfilter_float = GLUE(_batch_freqfilter_float, SIMD1);
#else
                _batch_freqfilter_float = GLUE(_batch_freqfilter_float, SIMD);
#endif
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

        _batch_fmul_value_cpu(ddst1, ddst1, sizeof(double), MAXNUM, 0.8723678263f);
        if (simd2)
        {
            memcpy(ddst2, dsrc, MAXNUM*sizeof(double));
            GLUE(_batch_fmul_value, SIMD2)(ddst2, ddst2, sizeof(double), MAXNUM, 0.8723678263f);
            TESTLF("double fmul "MKSTR(SIMD2), (float)ddst1, (float)ddst2);
        }

        /*
         * sinf versus _aax_sin (purely for speed comparisson
         */
        t = clock();
        float p = 0.0f;
        float step = GMATH_2PI*rand()/RAND_MAX;
        for (i=0; i<MAXNUM; ++i) {
            src[i] = sinf(p);
            p += step;
        }
        cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("\nsinf:  %f ms\n", cpu*1000.0f);


        t = clock();
        p = 0.0f;
        for (i=0; i<MAXNUM; ++i) {
            src[i] = fast_sin(p);
            p = fmodf(p+step, GMATH_2PI);
        }
        eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("fast_sin:  %f ms - sinf x %2.1f\n", eps*1000.0f, cpu/eps);

        t = clock();
        p = 0.0f;
        step /= GMATH_2PI;
        for (i=0; i<MAXNUM; ++i) {
            src[i] = fast_sin(p);
            p += step;
            if (step >= 1.0f) step -= 2.0f;
        }
        eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("fast_sin cpu:  %f ms - sinf x %2.1f\n", eps*1000.0f, cpu/eps);

        if (simd)
        {
            t = clock();
            p = 0.0f; 
            for (i=0; i<MAXNUM; ++i) {
                src[i] = fast_sin_sse2(p);
                p += step;
                if (step >= 1.0f) step -= 2.0f;
            }
            eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("fast_sin sse2:  %f ms - sinf x %2.1f\n", eps*1000.0f, cpu/eps);
        }

        if (simd2)
        {
            t = clock();
            p = 0.0f; 
            for (i=0; i<MAXNUM; ++i) {
                src[i] = fast_sin_sse_vex(p);
                p += step;
                if (step >= 1.0f) step -= 2.0f;
            }
            eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("fast_sin sse_vex:  %f ms - sinf x %2.1f\n", eps*1000.0f, cpu/eps);
        }

        /*
         * waveform generation 
         */
        t = clock();
        _aax_generate_waveform_cpu(dst1, MAXNUM, FREQ, PHASE, 
                                   _harmonics[AAX_SQUARE_WAVE]);
        cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("\ngenerate_waveform cpu:  %f ms\n", cpu*1000.0f);

        if (simd)
        {
            t = clock();
            _aax_generate_waveform_sse2(dst2, MAXNUM, FREQ, PHASE,
                                        _harmonics[AAX_SQUARE_WAVE]);
            eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("generate_waveform_sse2: %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
        }

        if (simd2)
        {
            t = clock();
            _aax_generate_waveform_sse_vex(dst2, MAXNUM, FREQ, PHASE,
                                           _harmonics[AAX_SQUARE_WAVE]);
            eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("generate_waveform_sse_vex: %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
        }
    }

    _aax_aligned_free(dst2);
    _aax_aligned_free(dst1);
    _aax_aligned_free(src);

    return 0;
}
