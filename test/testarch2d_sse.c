
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <base/types.h>
#include <base/geometry.h>
#include <src/software/cpu/arch2d_simd.h>

#define MAXNUM		4096
#define TEST(a,d1,d2) \
    for (i=0; i<MAXNUM; ++i) { \
        if (fabsf(d1[i]-d2[i]) > 4) { \
            printf("%s %i: %lf != %lf\n", a, i, (double)d1[i], (double)d2[i]); \
            break; \
        } }

int main()
{
    float *src, *dst1, *dst2;

    srand(time(NULL));

    src = (float*)malloc(MAXNUM*sizeof(double));
    dst1 = (float*)malloc(MAXNUM*sizeof(double));
    dst2 = (float*)malloc(MAXNUM*sizeof(double));

    if (src && dst1 && dst2)
    {
        double *dsrc, *ddst1, *ddst2;
        int i;

        for (i=0; i<MAXNUM; ++i) {
            src[i] = (float)rand()/(float)(RAND_MAX/(1<<23));
        }
        memcpy(dst1, src, MAXNUM*sizeof(float));

        /*
         * batch fmadd by a value
         */
        memcpy(dst1, src, MAXNUM*sizeof(float));
        _batch_fmadd_cpu(dst1, dst1, MAXNUM, 1.0, 0.0f);
        _batch_fmadd_cpu(dst1, dst1, MAXNUM, 0.8723678263f, 0.0f);
#ifdef __AVX__
        memcpy(dst2, src, MAXNUM*sizeof(float));
        _batch_fmadd_avx(dst2, dst2, MAXNUM, 1.0f, 0.0f);
        _batch_fmadd_avx(dst2, dst2, MAXNUM, 0.8723678263f, 0.0f);
        TEST("float fadd+fmadd avx", dst1, dst2);
#endif
        memcpy(dst2, src, MAXNUM*sizeof(float));
        _batch_fmadd_sse2(dst2, dst2, MAXNUM, 1.0f, 0.0f);
        _batch_fmadd_sse2(dst2, dst2, MAXNUM, 0.8723678263f, 0.0f);
        TEST("float fadd+fmadd sse2", dst1, dst2);

        /*
         * batch fmul by a value for floats
         */
        memcpy(dst1, src, MAXNUM*sizeof(float));
        memcpy(dst2, src, MAXNUM*sizeof(float));
        _batch_fmul_value_cpu(dst1, sizeof(float), MAXNUM, 0.8723678263f);
#ifdef __AVX__
        _batch_fmul_value_avx(dst2, sizeof(float), MAXNUM, 0.8723678263f);
        TEST("float fmul avx", dst1, dst2);
#endif
        memcpy(dst2, src, MAXNUM*sizeof(float));
        _batch_fmul_value_sse2(dst2, sizeof(float), MAXNUM, 0.8723678263f);
        TEST("float fmul sse2", dst1, dst2);

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

        memcpy(ddst2, dsrc, MAXNUM*sizeof(double));
        _batch_fmul_value_cpu(ddst1, sizeof(double), MAXNUM, 0.8723678263f);
#ifdef __AVX__
        _batch_fmul_value_avx(ddst2, sizeof(double), MAXNUM, 0.8723678263f);
        TEST("double fmul avx", ddst1, ddst2);
#endif
        memcpy(ddst2, dsrc, MAXNUM*sizeof(double));
        _batch_fmul_value_sse2(ddst2, sizeof(double), MAXNUM, 0.8723678263f);
        TEST("double fmul sse2", ddst1, ddst2);
    }

    free(dst2);
    free(dst1);
    free(src);

    return 0;
}
