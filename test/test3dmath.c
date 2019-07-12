
#include <stdio.h>
#include <time.h>

#ifndef __AVX__
// # include "avxintrin-emu.h"
#endif

#include <aax/aax.h>

#include <base/timer.h>
#include <base/types.h>
#include <base/geometry.h>
#include <base/logging.h>
#include <src/software/cpu/arch3d_simd.h>

// _aax_memcpy_proc _aax_memcpy = (_aax_memcpy_proc)memcpy;

#if defined(__i386__)
# define SIMD   sse
# define SIMD2  sse2
# define SIMD3  sse3
# define SIMD4  sse2
char _aaxArchDetectSSE();
char _aaxArchDetectSSE2();
char _aaxArchDetectSSE3();
#elif defined(__x86_64__)
# define SIMD   sse_vex
# define SIMD2  sse_vex
# define SIMD3  sse3
# define SIMD4	avx
char _aaxArchDetectSSE();
char _aaxArchDetectSSE2();
char _aaxArchDetectSSE3();
char _aaxArchDetectAVX();
#elif defined(__arm__) || defined(_M_ARM)
# define SIMD   neon
# define SIMD2  neon
# define SIMD3  neon
# define SIMD4	vfpv3
char _aaxArchDetectVFPV3();
char _aaxArchDetectNeon();
#endif

#define __MKSTR(X)		#X
#define MKSTR(X)		__MKSTR(X)
# define __GLUE(FUNC,NAME)	FUNC ## _ ## NAME
# define GLUE(FUNC,NAME)	__GLUE(FUNC,NAME)

#define F(a,b)	(fabs((a)-(b))>FLT_EPSILON)

#define TEST3(a,b) \
  if (F(a.v3[0],b.v3[0]) || F(a.v3[1],b.v3[1]) || F(a.v3[2],b.v3[2])) { \
    printf("line %i, %7.6f, %7.6f, %7.6f != %7.6f, %7.6f, %7.6f\n", __LINE__, a.v3[0],a.v3[1],a.v3[2],b.v3[0],b.v3[1],b.v3[2]); \
    return __LINE__; }

#define TEST4(a,b) \
  if (F(a.v4[0],b.v4[0]) || F(a.v4[1],b.v4[1]) || F(a.v4[2],b.v4[2]) || F(a.v4[3],b.v4[3])) { \
    printf("line %i, %7.6f, %7.6f, %7.6f, %7.6f \n      != %7.6f, %7.6f, %7.6f, %7.6f\n", __LINE__, a.v4[0],a.v4[1],a.v4[2],a.v4[3],b.v4[0],b.v4[1],b.v4[2],b.v4[3]); \
    return __LINE__; }

#define TESTM4(a,b) { \
  int i; for (i=0; i<4; ++i) { \
  if (F(a.m4[i][0],b.m4[i][0]) || F(a.m4[i][1],b.m4[i][1]) || F(a.m4[i][2],b.m4[i][2]) || F(a.m4[i][3],b.m4[i][3])) { \
    printf("line %i, row: %i, %7.6f, %7.6f, %7.6f, %7.6f \n              != %7.6f, %7.6f, %7.6f, %7.6f\n", __LINE__, i, a.m4[i][0],a.m4[i][1],a.m4[i][2],a.m4[i][3],b.m4[i][0],b.m4[i][1],b.m4[i][2],b.m4[i][3]); \
    return __LINE__; } } }

static float t1[4] = {  0.303f, -1.13f, -0.078f,  0.519f };
static float t2[4] = {  0.593f,  0.13f,  1.078f, -0.017f };

int main()
{
    vec3f_t a3, b3, c3, x3, y3, z3;
    vec4f_t a4, b4, c4, x4, y4, z4;
    mtx4f_t k, l, m, n;
    mtx4d_t k64, l64, m64, n64;
    char simd = 0, simd2  = 0, simd3 = 0, simd4 = 0;
    double cpu, eps;
    clock_t t;
    float f;
    int i;

#if defined(__i386__)
    simd = _aaxArchDetectSSE();
    simd2 = _aaxArchDetectSSE2();
    simd3 = _aaxArchDetectSSE3();
#elif defined(__x86_64__)
    simd = simd2 = simd4 = _aaxArchDetectAVX();
    if (!simd) {
        simd = _aaxArchDetectSSE();
        simd2 = _aaxArchDetectSSE2();
    }
    simd3 = _aaxArchDetectSSE3();
#elif defined(__arm__) || defined(_M_ARM)
    simd = simd2 = simd3 = _aaxArchDetectNeon();
    simd4 = _aaxArchDetectVFPV3();
#endif

    vec3fZero(&a3); vec3fZero(&b3);
    vec3fZero(&x3); vec3fZero(&y3);
    vec3fFill(a3.v3, t1); vec3fFill(b3.v3, t2);
    vec3fFill(x3.v3, t1); vec3fFill(y3.v3, t2);
    TEST3(a3,x3);
    TEST3(b3,y3);

    if (simd) {
        vec3fCopy(&c3, &a3); vec3fCopy(&z3, &x3);
        _vec3fDotProduct_cpu(&c3, &b3);
        GLUE(_vec3fDotProduct, SIMD)(&z3, &y3);
        TEST3(c3,z3);
    }

    if (simd3) {
        vec3fCopy(&c3, &a3); vec3fCopy(&z3, &x3);
        _vec3fDotProduct_cpu(&c3, &b3);
        GLUE(_vec3fDotProduct, SIMD3)(&z3, &y3);
        TEST3(c3,z3);
    }

    if (simd) {
        vec3fCopy(&c3, &a3); vec3fCopy(&z3, &x3);
        _vec3fCrossProduct_cpu(&c3, &a3, &b3);
        GLUE(_vec3fCrossProduct, SIMD)(&z3, &x3, &y3);
        TEST3(c3,z3);
    }

    vec4fFill(a4.v4, t1); vec3fFill(b4.v4, t2);
    vec4fFill(x4.v4, t1); vec3fFill(y4.v4, t2);
    TEST4(a4,x4);
    TEST4(b4,y4);

    f = vec3fNormalize(&z3, &x3);
    mtx4fSetIdentity(m.m4);
    mtx4fRotate(&m, f, z3.v3[0], z3.v3[1], z3.v3[2]);
    mtx4fTranslate(&m, 3.3f, -1.07f, 0.87f);

    if (simd2) {
        _mtx4fMulVec4_cpu(&c4, &m, &a4);
        GLUE(_mtx4fMulVec4, SIMD)(&z4, &m, &x4);
        TEST4(c4,z4);
    }

    if (simd) {
        _mtx4fMulVec4_cpu(&c4, &m, &b4);
        GLUE(_mtx4fMulVec4, SIMD)(&z4, &m, &y4);
        TEST4(c4,z4);
    }

    f = vec3fNormalize(&z3, &y3);
    mtx4fSetIdentity(m.m4);
    mtx4fSetIdentity(n.m4);
    mtx4fRotate(&n, f, z3.v3[0], z3.v3[1], z3.v3[2]);
    mtx4fTranslate(&n, -.2f, 4.507f, 1.39f);

    if (simd)
    {
        mtx4fMul_proc m4fMul = _mtx4fMul_cpu;

        t = clock();
        for (i=0; i<1000; ++i) {
            m4fMul(&k, &m, &n);
        }
        cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("mtx4fMul cpu:\t\t%f ms\n", cpu*1000.0f);

#if defined(__arm__) || defined(_M_ARM)
        m4fMul = _mtx4fMul_vfpv3;
        t = clock();
        for (i=0; i<1000; ++i) {
            m4fMul(&l, &m, &n);
        }
        eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("mtx4fMul vfpv3:\t\t%f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
#else
        m4fMul = _mtx4fMul_sse;
        t = clock();
        for (i=0; i<1000; ++i) {
            m4fMul(&l, &m, &n);
        }
        eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("mtx4fMul sse:\t\t%f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
#endif

        if (simd4)
        {
            m4fMul = GLUE(_mtx4fMul, SIMD);
            t = clock();
            for (i=0; i<1000; ++i) {
                m4fMul(&l, &m, &n);
            }
            eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("mtx4fMul "MKSTR(SIMD)":\t%f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
        }
        TESTM4(k,l);
    }

    if (simd2)
    {
        mtx4dMul_proc m4dMul = _mtx4dMul_cpu;

        mtx4dSetIdentity(m64.m4);
        aaxMatrixToMatrix64(n64.m4, n.m4);

        t = clock();
        for (i=0; i<1000; ++i) {
            m4dMul(&k64, &m64, &n64);
        }
        cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("\nmtx4dMul cpu:\t\t%f ms\n", cpu*1000.0f);

#if defined(__arm__) || defined(_M_ARM)
        m4dMul = _mtx4dMul_vfpv3;
        t = clock();
        for (i=0; i<1000; ++i) {
            m4dMul(&l64, &m64, &n64);
        }
        eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("mtx4dMul vfpv3:\t\t%f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
#else
        m4dMul = _mtx4dMul_sse2;
        t = clock();
        for (i=0; i<1000; ++i) {
            m4dMul(&l64, &m64, &n64);
        }
        eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("mtx4dMul sse2:\t\t%f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
#endif

        if (simd4)
        {
            m4dMul = GLUE(_mtx4dMul, SIMD4);
            t = clock();
            for (i=0; i<1000; ++i) {
                m4dMul(&l64, &m64, &n64);
            }
            eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
            printf("mtx4dMul "MKSTR(SIMD4)":\t\t%f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
            TESTM4(k64,l64);
        }
    }

    return 0;
}
