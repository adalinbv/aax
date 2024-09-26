
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <base/geometry.h>

#if defined __ARM_NEON__ || defined __aarch64__

void
_mtx4fMul(mtx4f_ptr d, const mtx4f_ptr m1, const mtx4f_ptr m2)
{
   int i;
   for (i=0; i<4; ++i) {
      float32x4_t row = vmulq_f32(m1->s4x4[0], vdupq_n_f32(m2->m4[i][0]));
      for (int j=1; j<4; ++j) {
         float32x4_t col = vdupq_n_f32(m2->m4[i][j]);
         row = vaddq_f32(row, vmulq_f32(m1->s4x4[j], col));
      }
      d->s4x4[i] = row;
   }
}

void
_mtx4dMul(mtx4d_ptr dst, const mtx4d_ptr mtx1, const mtx4d_ptr mtx2)
{
   const double *m20 = mtx2->m4[0], *m21 = mtx2->m4[1];
   const double *m22 = mtx2->m4[2], *m23 = mtx2->m4[3];
   double m10i, m11i, m12i, m13i;
   int i=4;
   do
   {
      --i;

      m10i = mtx1->m4[0][i];
      m11i = mtx1->m4[1][i];
      m12i = mtx1->m4[2][i];
      m13i = mtx1->m4[3][i];

      dst->m4[0][i] = m10i*m20[0] + m11i*m20[1] + m12i*m20[2] + m13i*m20[3];
      dst->m4[1][i] = m10i*m21[0] + m11i*m21[1] + m12i*m21[2] + m13i*m21[3];
      dst->m4[2][i] = m10i*m22[0] + m11i*m22[1] + m12i*m22[2] + m13i*m22[3];
      dst->m4[3][i] = m10i*m23[0] + m11i*m23[1] + m12i*m23[2] + m13i*m23[3];
   }
   while (i != 0);
}

#else
void
_mtx4fMul(mtx4f_ptr d, const mtx4f_ptr m1, const mtx4f_ptr m2)
{
   __m128 row, col;
   int i, j;

   for (i=0; i<4; ++i) {
      row = _mm_mul_ps(m1->s4x4[0], _mm_set1_ps(m2->m4[i][0]));
      for (j=1; j<4; ++j) {
          col = _mm_set1_ps(m2->m4[i][j]);
          row = _mm_add_ps(row, _mm_mul_ps(m1->s4x4[j], col));
      }
      d->s4x4[i] = row;
   }
}

# ifdef __AVX__
void
_mtx4dMul(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2)
{
   __m256d row, col;
   int i, j;

   for (i=0; i<4; ++i) {
      row = _mm256_mul_pd(m1->s4x4[0].avx, _mm256_set1_pd(m2->m4[i][0]));
      for (j=1; j<4; ++j) {
          col = _mm256_set1_pd(m2->m4[i][j]);
          row = _mm256_add_pd(row, _mm256_mul_pd(m1->s4x4[j].avx, col));
      }
      d->s4x4[i].avx = row;
   }
}
# else
void
_mtx4dMul(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2)
{
   int i;

   for (i=0; i<4; ++i) {
      __m128d col = _mm_set1_pd(m2->m4[i][0]);
      __m128d row1 = _mm_mul_pd(m1->s4x4[0].sse[0], col);
      __m128d row2 = _mm_mul_pd(m1->s4x4[0].sse[1], col);
      for (int j=1; j<4; ++j) {
          col = _mm_set1_pd(m2->m4[i][j]);
          row1 = _mm_add_pd(row1, _mm_mul_pd(m1->s4x4[j].sse[0], col));
          row2 = _mm_add_pd(row2, _mm_mul_pd(m1->s4x4[j].sse[1], col));
      }
      d->s4x4[i].sse[0] = row1;
      d->s4x4[i].sse[1] = row2;
   }
}
# endif
#endif

void (*mtx4dMultiply)(mtx4d_ptr, const mtx4d_ptr, const mtx4d_ptr);

int main()
{
    mtx4d_t k64, l64, m64, n64;
    float sse, avx;
    clock_t t;
    int i;

    memset(&k64, 0, sizeof(mtx4d_t));
    memset(&l64, 0, sizeof(mtx4d_t));
    memset(&m64, 0, sizeof(mtx4d_t));

    mtx4dMultiply = _mtx4dMul_cpu;
    t = clock();
    for (i=0; i<1000; ++i) {
        mtx4dMultiply(&l64, &m64, &n64);
    }
    sse = (double)(clock() - t)/ CLOCKS_PER_SEC;

    mtx4dMultiply = _mtx4dMul;
    t = clock();
    for (i=0; i<1000; ++i) {
        mtx4dMultiply(&l64, &m64, &n64);
    }
    avx = (double)(clock() - t)/ CLOCKS_PER_SEC;
    printf("cpu:\t%f ms\nSIMD:\t%f\n x %2.1f\n", sse*1000.0f, avx*1000.0f, sse/avx);

   return 0;
}
