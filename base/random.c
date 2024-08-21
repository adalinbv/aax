/*
 * SPDX-FileCopyrightText: Copyright © 2017-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2017-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#endif
#ifdef HAVE_SYS_RANDOM_H
# include <sys/random.h>
#endif
#include <sys/time.h>

#include <base/types.h>

static inline int
_aax_hash3(unsigned int h1, unsigned int h2, unsigned int h3) {
    return (((h1 * 2654435789U) + h2) * 2654435789U) + h3;
}

// https://en.wikipedia.org/wiki/Xorshift#xorshift+
/* This generator is one of the fastest generators passing BigCrush */
/* The state must be seeded so that it is not all zero */
union
{
   uint64_t xs[2];
   uint32_t s[4];
} _xor;

uint64_t
xorshift128plus()
{
   uint64_t x = _xor.xs[0];
   uint64_t const y = _xor.xs[1];

   _xor.xs[0] = y;
   x ^= x << 23;
   _xor.xs[1] = x ^ y ^ (x >> 17) ^ (y >> 26);
   return _xor.xs[1] + y;
}

static inline uint64_t
rotl(const uint64_t x, int k) {
   return (x << k) | (x >> (64 - k));
}

// https://en.wikipedia.org/wiki/Xoroshiro128%2B
// xoroshiro128+ (named after its operations: XOR, rotate, shift, rotate)
// is a pseudorandom number generator intended as a successor to xorshift+
uint64_t
xoroshiro128plus(void)
{
   const uint64_t s0 = _xor.xs[0];
   uint64_t s1 = _xor.xs[1];
   const uint64_t result = s0 + s1;

   s1 ^= s0;
   _xor.xs[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16); // a, b
   _xor.xs[1] = rotl(s1, 37); // c

   return result;
}

float // range: 0.0 .. 1.0
_aax_rand_sample()
{
   // prefered method for conversion to float [0.0-1.0]:
   // https://prng.di.unimi.it/
   return (xoroshiro128plus() >> 11) * 0x1.0p-53;
}

float // range: -1.0 .. 1.0
_aax_random() {
   return (xoroshiro128plus() >> 10) * 0x1.0p-53 - 1.0f;
}

void
_aax_rand_sample8(float r[8])
{
   int i;
   for (i=0; i<8; ++i) {
      r[i] = (xoroshiro128plus() >> 11) * 0x1.0p-53;
   }
}

void
_aax_srandom()
{
   static int init = -1;
   if (init < 0)
   {
      unsigned int size = 2*sizeof(uint64_t);
      unsigned int num = 0;

#ifdef HAVE_SYS_RANDOM_H
      num = getrandom(_xor.xs, size, 0);
#endif

      if (num < size)
      {
         struct timeval time;

         gettimeofday(&time, NULL);
         srand(_aax_hash3(time.tv_sec, time.tv_usec, getpid()));
         _xor.xs[0] = (uint64_t)rand() * rand();
         _xor.xs[1] = ((uint64_t)rand() * rand()) ^ _xor.xs[0];
      }
   }
}


static uint64_t _aax_seed;

// Warning: do not seed with 0
void
_aax_srand(uint64_t a) {
   _aax_seed = a;
}

// xorshift64*
uint64_t
_aax_rand()
{
    uint64_t x = _aax_seed;	/* Must be seeded with a nonzero value. */
    x ^= x >> 12; // a
    x ^= x << 25; // b
    x ^= x >> 27; // c
    _aax_seed = x;
    return x * UINT64_C(0x2545F4914F6CDD1D);
}

float // range: -1.0 .. 1.0
_aax_seeded_random() {
   return (float)((double)_aax_rand()/(double)INT64_MAX) - 1.0f;
}
