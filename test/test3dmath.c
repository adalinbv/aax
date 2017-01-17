
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
  if (F(a[0],b[0]) || F(a[1],b[1]) || F(a[2],b[2])) { \
    printf("line %i, %7.6f, %7.6f, %7.6f != %7.6f, %7.6f, %7.6f\n", __LINE__, a[0],a[1],a[2],b[0],b[1],b[2]); \
    return __LINE__; }

#define TEST4(a,b) \
  if (F(a[0],b[0]) || F(a[1],b[1]) || F(a[2],b[2]) || F(a[3],b[3])) { \
    printf("line %i, %7.6f, %7.6f, %7.6f, %7.6f \n      != %7.6f, %7.6f, %7.6f, %7.6f\n", __LINE__, a[0],a[1],a[2],a[3],b[0],b[1],b[2],b[3]); \
    return __LINE__; }

static float t1[4] = {  0.303f, -1.13f, -0.078f,  0.519f };
static float t2[4] = {  0.593f,  0.13f,  1.078f, -0.017f };

int main()
{
    vec3_t a3, b3, c3, x3, y3, z3;
    vec4_t a4, b4, c4, x4, y4, z4;
    mtx4_t k, l, m, n;
    float f;

    vec3Copy(a3, t1); vec3Copy(b3, t2);
    vec3Copy(x3, t1); vec3Copy(y3, t2);

    vec3Copy(c3, a3); vec3Copy(z3, x3);
    vec3DotProduct(c3, b3);
    _vec3DotProduct_sse(z3, y3);
    TEST3(c3,z3);

    vec3Copy(c3, a3); vec3Copy(z3, x3);
    vec3DotProduct(c3, b3);
    _vec3DotProduct_sse3(z3, y3);
    TEST3(c3,z3);

    vec3Copy(c3, a3); vec3Copy(z3, x3);
    vec3CrossProduct(c3, a3, b3);
    _vec3CrossProduct_sse(z3, x3, y3);
    TEST3(c3,z3);


    vec4Copy(a4, t1); vec3Copy(b4, t2);
    vec4Copy(x4, t1); vec3Copy(y4, t2);

    f = vec3Normalize(z3, x3);
    mtx4Copy(m, aaxIdentityMatrix);
    mtx4Rotate(m, f, z3[0], z3[1], z3[2]);
    mtx4Translate(m, 3.3f, -1.07f, 0.87f);

    vec4Matrix4(c4, a4, m);
    _vec4Matrix4_sse(z4, x4, m);
    TEST4(c4,z4);

    _vec4Matrix4_sse3(z4, x4, m);
    TEST4(c4,z4)

    pt4Matrix4(c4, b4, m);
    _pt4Matrix4_sse(z4, y4, m);
    TEST4(c4,z4);

    f = vec3Normalize(z3, y3);
    mtx4Copy(n, aaxIdentityMatrix);
    mtx4Rotate(n, f, z3[0], z3[1], z3[2]);
    mtx4Translate(n, -.2f, 4.507f, 1.39f);

    mtx4Mul(k, m, n);
    _mtx4Mul_sse(l, m, n);
    TEST4(k[0],l[0]);
    TEST4(k[1],l[1]);
    TEST4(k[2],l[2]);
    TEST4(k[3],l[3]);

    return 0;
}
