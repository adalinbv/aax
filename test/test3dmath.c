
#include <stdio.h>

#ifndef __AVX__
// # include "avxintrin-emu.h"
#endif

#include <base/types.h>
#include <base/geometry.h>
#include <src/software/cpu/arch3d_simd.h>

_aax_memcpy_proc _aax_memcpy = (_aax_memcpy_proc)memcpy;

#define F(a,b)	(fabs((a)-(b))>FLT_EPSILON)

#define TEST3(a,b) \
  if (F(a.v3[0],b.v3[0]) || F(a.v3[1],b.v3[1]) || F(a.v3[2],b.v3[2])) { \
    printf("line %i, %7.6f, %7.6f, %7.6f != %7.6f, %7.6f, %7.6f\n", __LINE__, a.v3[0],a.v3[1],a.v3[2],b.v3[0],b.v3[1],b.v3[2]); \
    return __LINE__; }

#define TEST4(a,b) \
  if (F(a.v4[0],b.v4[0]) || F(a.v4[1],b.v4[1]) || F(a.v4[2],b.v4[2]) || F(a.v4[3],b.v4[3])) { \
    printf("line %i, %7.6f, %7.6f, %7.6f, %7.6f \n      != %7.6f, %7.6f, %7.6f, %7.6f\n", __LINE__, a.v4[0],a.v4[1],a.v4[2],a.v4[3],b.v4[0],b.v4[1],b.v4[2],b.v4[3]); \
    return __LINE__; }

static float t1[4] = {  0.303f, -1.13f, -0.078f,  0.519f };
static float t2[4] = {  0.593f,  0.13f,  1.078f, -0.017f };

int main()
{
    vec3f_t a3, b3, c3, x3, y3, z3;
    vec4f_t a4, b4, c4, x4, y4, z4;
    mtx4f_t k, l, m, n;
    float f;

    vec3fFill(a3.v3, t1); vec3fFill(b3.v3, t2);
    vec3fFill(x3.v3, t1); vec3fFill(y3.v3, t2);

    vec3fCopy(&c3, &a3); vec3fCopy(&z3, &x3);
    _vec3fDotProduct_cpu(&c3, &b3);
    _vec3fDotProduct_sse(&z3, &y3);
    TEST3(c3,z3);

    vec3fCopy(&c3, &a3); vec3fCopy(&z3, &x3);
    _vec3fDotProduct_cpu(&c3, &b3);
    _vec3fDotProduct_sse3(&z3, &y3);
    TEST3(c3,z3);

    vec3fCopy(&c3, &a3); vec3fCopy(&z3, &x3);
    _vec3fCrossProduct_cpu(&c3, &a3, &b3);
    _vec3fCrossProduct_sse(&z3, &x3, &y3);
    TEST3(c3,z3);


    vec4fFill(a4.v4, t1); vec3fFill(b4.v4, t2);
    vec4fFill(x4.v4, t1); vec3fFill(y4.v4, t2);

    f = vec3fNormalize(&z3, &x3);
    mtx4fFill(m.m4, aaxIdentityMatrix);
    mtx4fRotate(&m, f, z3.v3[0], z3.v3[1], z3.v3[2]);
    mtx4fTranslate(&m, 3.3f, -1.07f, 0.87f);

    _vec4fMatrix4_cpu(&c4, &a4, &m);
    _vec4fMatrix4_sse(&z4, &x4, &m);
    TEST4(c4,z4);

    _vec4fMatrix4_sse3(&z4, &x4, &m);
    TEST4(c4,z4)

    _pt4fMatrix4_cpu(&c4, &b4, &m);
    _pt4fMatrix4_sse(&z4, &y4, &m);
    TEST4(c4,z4);

    f = vec3fNormalize(&z3, &y3);
    mtx4fFill(n.m4, aaxIdentityMatrix);
    mtx4fRotate(&n, f, z3.v3[0], z3.v3[1], z3.v3[2]);
    mtx4fTranslate(&n, -.2f, 4.507f, 1.39f);

    _mtx4fMul_cpu(&k, &m, &n);
    _mtx4fMul_sse(&l, &m, &n);
//  TEST4(k[0],l[0]);
//  TEST4(k[1],l[1]);
//  TEST4(k[2],l[2]);
//  TEST4(k[3],l[3]);

    return 0;
}
