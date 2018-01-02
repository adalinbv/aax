
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <base/types.h>
#include <base/geometry.h>
#include <base/timer.h>
#include <src/software/cpu/arch2d_simd.h>
#include <arch.h>

#define MAXNUM		(199*4096)
#define TEST(a,d1,d2) \
    for (i=0; i<MAXNUM; ++i) { \
        if (fabsf(d1[i]-d2[i]) > 4) { \
            printf(" # %s %i: %lf != %lf\n", a, i, (double)d1[i], (double)d2[i]); \
            break; \
        } }

#if defined(__i386__)
# define SIMD	sse2
# define SIMD2	sse2
char _aaxArchDetectSSE2();
#elif defined(__x86_64__)
# define SIMD   sse_vex
# define SIMD2	avx
char _aaxArchDetectAVX();
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

#define __MKSTR(X)		#X
#define MKSTR(X)		__MKSTR(X)
# define __GLUE(FUNC,NAME)	FUNC ## _ ## NAME
# define GLUE(FUNC,NAME)	__GLUE(FUNC,NAME)

_batch_fmadd_proc _batch_fmadd;
_batch_mul_value_proc _batch_fmul_value;
_batch_get_average_rms_proc _batch_get_average_rms;


int main()
{
    float *src, *dst1, *dst2;
    char simd = 0;
    clock_t t;

#if defined(__i386__)
    simd = _aaxArchDetectSSE2();
#elif defined(__x86_64__)
    simd = _aaxArchDetectAVX();
#elif defined(__arm__) || defined(_M_ARM)
    simd = _aaxArchDetectNEON();
#endif

    srand(time(NULL));

    src = (float*)_aax_aligned_alloc(MAXNUM*sizeof(double));
    dst1 = (float*)_aax_aligned_alloc(MAXNUM*sizeof(double));
    dst2 = (float*)_aax_aligned_alloc(MAXNUM*sizeof(double));

    if (src && dst1 && dst2)
    {
        float rms1, rms2, peak1, peak2;
        double *dsrc, *ddst1, *ddst2;
        double cpu, eps;
        int i;

        for (i=0; i<MAXNUM; ++i) {
            src[i] = (float)(1<<23) * (float)rand()/(float)(RAND_MAX);
        }


        /*
         * batch fadd by a value
         */
        memcpy(dst1, src, MAXNUM*sizeof(float));
        t = clock();
          for(i=0; i<MAXNUM; ++i) {
            dst1[i] += dst1[i];
          }
          cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("fadd cpu:  %f ms\n", cpu*1000.0f);

        memcpy(dst2, src, MAXNUM*sizeof(float));
        _batch_fmadd = _batch_fmadd_cpu;
        t = clock();
          _batch_fmadd(dst2, dst2, MAXNUM, 1.0, 0.0f);
          eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("fadd cpu:  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);

        if (simd)
        {
            memcpy(dst2, src, MAXNUM*sizeof(float));
            _batch_fmadd = GLUE(_batch_fmadd, SIMD2);
            t = clock();
              _batch_fmadd(dst2, dst2, MAXNUM, 1.0f, 0.0f);
              eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("fadd "MKSTR(SIMD2)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);

#ifdef __x86_64__
            memcpy(dst2, src, MAXNUM*sizeof(float));
            _batch_fmadd = _batch_fmadd_sse2;
            t = clock();
              _batch_fmadd(dst2, dst2, MAXNUM, 1.0f, 0.0f);
              eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("fadd sse2:  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
#endif
        }

        /*
         * batch fmadd by a value
         */
        memcpy(dst1, src, MAXNUM*sizeof(float));
        t = clock();
          for(i=0; i<MAXNUM; ++i) {
            dst1[i] += dst1[i] * 0.8723678263f;
          }
          cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("\nfmadd cpu: %f ms\n", cpu*1000.0f);

        memcpy(dst1, src, MAXNUM*sizeof(float));
        _batch_fmadd = _batch_fmadd_cpu;
        t = clock();
          _batch_fmadd(dst1, dst1, MAXNUM, 0.8723678263f, 0.0f);
          eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("fmadd cpu: %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);

        if (simd)
        {
            memcpy(dst2, src, MAXNUM*sizeof(float));
            _batch_fmadd = GLUE(_batch_fmadd, SIMD2);
            t = clock();
              _batch_fmadd(dst2, dst2, MAXNUM, 0.8723678263f, 0.0f);
              eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("fmadd "MKSTR(SIMD2)": %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
            TEST("float fadd+fmadd "MKSTR(SIMD2), dst1, dst2);

#ifdef __x86_64__
            memcpy(dst2, src, MAXNUM*sizeof(float));
            _batch_fmadd = _batch_fmadd_sse2;
            t = clock();
              _batch_fmadd(dst2, dst2, MAXNUM, 0.8723678263f, 0.0f);
              eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("fmadd sse2: %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
            TEST("float fadd+fmadd sse2", dst1, dst2);
#endif
        }

        /*
         * batch fmul by a value for floats
         */
        memcpy(dst1, src, MAXNUM*sizeof(float));
        t = clock();
          for(i=0; i<MAXNUM; ++i) {
            dst1[i] *= 0.8723678263f;
          }
          cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("\nfmul cpu:  %f ms\n", cpu*1000.0f);

        memcpy(dst1, src, MAXNUM*sizeof(float));
        _batch_fmul_value = _batch_fmul_value_cpu;
        t = clock();
          _batch_fmul_value(dst1, sizeof(float), MAXNUM, 0.8723678263f);
          eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("fmul cpu:  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);

        if (simd)
        {
            memcpy(dst2, src, MAXNUM*sizeof(float));
            _batch_fmul_value = GLUE(_batch_fmul_value, SIMD2);
            t = clock();
              _batch_fmul_value(dst2, sizeof(float), MAXNUM, 0.8723678263f);
              eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("fmul "MKSTR(SIMD2)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
            TEST("float fmul "MKSTR(SIMD2), dst1, dst2);

#ifdef __x86_64__
            memcpy(dst2, src, MAXNUM*sizeof(float));
            _batch_fmul_value = _batch_fmul_value_sse2;
            t = clock();
              _batch_fmul_value(dst2, sizeof(float), MAXNUM, 0.8723678263f);
              eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("fmul sse2:  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
            TEST("float fmul sse2", dst1, dst2);
#endif
        }

        /*
         * batch RMS calulculation
         */
        _batch_get_average_rms = _batch_get_average_rms_cpu;
        t = clock();
          _batch_get_average_rms(src, MAXNUM, &rms1, &peak1);
          cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("\nrms cpu:  %f\n", cpu*1000.0f);

        _batch_get_average_rms = GLUE(_batch_get_average_rms, SIMD);
        t = clock();
          _batch_get_average_rms(src, MAXNUM, &rms2, &peak2);
          cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("rms "MKSTR(SIMD)":  %f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
        printf(" | rms1: %f, rms2: %f - %f\n", rms1, rms2, rms1-rms2);
        printf(" | peak1: %f, peak2: %f - %f\n", peak1, peak2, peak1-peak2);


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

        _batch_fmul_value_cpu(ddst1, sizeof(double), MAXNUM, 0.8723678263f);
        memcpy(ddst2, dsrc, MAXNUM*sizeof(double));
        GLUE(_batch_fmul_value, SIMD2)(ddst2, sizeof(double), MAXNUM, 0.8723678263f);
        TEST("double fmul "MKSTR(SIMD2), (float)ddst1, (float)ddst2);
    }

    _aax_aligned_free(dst2);
    _aax_aligned_free(dst1);
    _aax_aligned_free(src);

    return 0;
}
