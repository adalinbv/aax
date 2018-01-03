
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <base/geometry.h>

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
void (*mtx4fMul)(mtx4f_ptr, const mtx4f_ptr, const mtx4f_ptr) = _mtx4fMul;

#ifdef __AVX__
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
#else
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
#endif
void (*mtx4dMul)(mtx4d_ptr, const mtx4d_ptr, const mtx4d_ptr) = _mtx4dMul;

int main()
{
    mtx4f_t k, l, m, n;
    mtx4d_t k64, l64, m64, n64;
    float sse, avx;
    clock_t t;
    int i;

    memset(&k, 0, sizeof(mtx4f_t));
    memset(&l, 0, sizeof(mtx4f_t));
    memset(&m, 0, sizeof(mtx4f_t));

    t = clock();
    for (i=0; i<1000; ++i) {
        mtx4fMul(&k, &m, &n);
    }
    sse = (double)(clock() - t)/ CLOCKS_PER_SEC;

    memset(&k64, 0, sizeof(mtx4d_t));
    memset(&l64, 0, sizeof(mtx4d_t));
    memset(&m64, 0, sizeof(mtx4d_t));

    t = clock();
    for (i=0; i<1000; ++i) {
        mtx4dMul(&l64, &m64, &n64);
    }
    avx = (double)(clock() - t)/ CLOCKS_PER_SEC;
    printf("float:\t%f ms\ndouble:\t%f\n x %2.1f\n", sse*1000.0f, avx*1000.0f, sse/avx);

   return 0;
}
