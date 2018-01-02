
#include <stdio.h>
#include <time.h>

#ifndef __AVX__
// # include "avxintrin-emu.h"
#endif

#include <aax/aax.h>

#include <base/timer.h>
#include <base/types.h>
#include <base/geometry.h>
#include <src/software/cpu/arch3d_simd.h>

// _aax_memcpy_proc _aax_memcpy = (_aax_memcpy_proc)memcpy;

#if defined(__i386__)
# define SIMD   sse
# define SIMD2  sse2
# define SIMD3  sse3
char _aaxArchDetectSSE();
char _aaxArchDetectSSE2();
char _aaxArchDetectSSE3();
#elif defined(__x86_64__)
# define SIMD   sse_vex
# define SIMD2  sse_vex
# define SIMD3  sse3
char _aaxArchDetectSSE();
char _aaxArchDetectSSE2();
char _aaxArchDetectSSE3();
char _aaxArchDetectAVX();
#elif defined(__arm__) || defined(_M_ARM)
# define SIMD   neon
# define SIMD2  neon
# define SIMD3  neon
# define AAX_ARCH_NEON  0x00000008
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
    char simd = 0, simd2  = 0, simd3 = 0;
    double cpu, eps;
    clock_t t;
    float f;
    int i;

#if defined(__i386__)
    simd = _aaxArchDetectSSE();
    simd2 = _aaxArchDetectSSE2();
    simd3 = _aaxArchDetectSSE3();
#elif defined(__x86_64__)
    simd = simd2 = _aaxArchDetectAVX();
    if (!simd) {
        simd = _aaxArchDetectSSE();
        simd2 = _aaxArchDetectSSE2();
    }
    simd3 = _aaxArchDetectSSE3();
#elif defined(__arm__) || defined(_M_ARM)
    simd = simd2 = simd3 = aaxArchDetectNEON();
#endif

    vec3fFill(a3.v3, t1); vec3fFill(b3.v3, t2);
    vec3fFill(x3.v3, t1); vec3fFill(y3.v3, t2);

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

    f = vec3fNormalize(&z3, &x3);
    mtx4fSetIdentity(m.m4);
    mtx4fRotate(&m, f, z3.v3[0], z3.v3[1], z3.v3[2]);
    mtx4fTranslate(&m, 3.3f, -1.07f, 0.87f);

    if (simd2) {
        _vec4fMatrix4_cpu(&c4, &a4, &m);
        GLUE(_vec4fMatrix4, SIMD)(&z4, &x4, &m);
        TEST4(c4,z4);
    }

    if (simd3) {
        GLUE(_vec4fMatrix4, SIMD3)(&z4, &x4, &m);
        TEST4(c4,z4)
    }

    if (simd) {
        _pt4fMatrix4_cpu(&c4, &b4, &m);
        GLUE(_pt4fMatrix4, SIMD)(&z4, &y4, &m);
        TEST4(c4,z4);
    }

    f = vec3fNormalize(&z3, &y3);
    mtx4fSetIdentity(m.m4);
    mtx4fRotate(&n, f, z3.v3[0], z3.v3[1], z3.v3[2]);
    mtx4fTranslate(&n, -.2f, 4.507f, 1.39f);

    if (simd)
    {
        t = clock();
        for (i=0; i<1000; ++i) {
            _mtx4fMul_cpu(&k, &m, &n);
            mtx4fCopy(&n, &k);
        }
        cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("mtx4fMul cpu:\t\t%f ms\n", cpu*1000.0f);

        t = clock();
        for (i=0; i<1000; ++i) {
            GLUE(_mtx4fMul, SIMD)(&l, &m, &n);
            mtx4fCopy(&n, &l);
        }
        eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("mtx4fMul "MKSTR(SIMD)":\t%f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
        TESTM4(k,l);

        t = clock();
        for (i=0; i<1000; ++i) {
            _mtx4fMul_sse(&l, &m, &n);
            mtx4fCopy(&n, &l);
        }
        eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("mtx4fMul sse:\t\t%f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
    }

    if (simd2)
    {
        mtx4dSetIdentity(m64.m4);
        aaxMatrixToMatrix64(n64.m4, n.m4);

        t = clock();
        for (i=0; i<1000; ++i) {
            _mtx4dMul_cpu(&k64, &m64, &n64);
            mtx4dCopy(&n64, &k64);
        }
        cpu = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("\nmtx4dMul cpu:\t\t%f ms\n", cpu*1000.0f);

        t = clock();
        for (i=0; i<1000; ++i) {
            GLUE(_mtx4dMul, SIMD2)(&l64, &m64, &n64);
            mtx4dCopy(&n64, &l64);
        }
        eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("mtx4dMul "MKSTR(SIMD2)":\t%f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
        TESTM4(k64,l64);

        t = clock();
        for (i=0; i<1000; ++i) {
            _mtx4dMul_sse2(&l64, &m64, &n64);
            mtx4dCopy(&n64, &l64);
        }
        eps = (double)(clock() - t)/ CLOCKS_PER_SEC;
        printf("mtx4dMul sse2:\t\t%f ms - cpu x %2.1f\n", eps*1000.0f, cpu/eps);
    }

    return 0;
}
