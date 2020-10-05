/*
 * Copyright 2017-2018 by Erik Hofman.
 * Copyright 2017-2018 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
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

static inline uint64_t
rotl(const uint64_t x, int k) {
   return (x << k) | (x >> (64 - k));
}

// https://en.wikipedia.org/wiki/Xorshift#xorshift+
/* This generator is one of the fastest generators passing BigCrush */
/* The state must be seeded so that it is not all zero */
static union
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

// http://xoshiro.di.unimi.it/xoshiro128plus.c
uint32_t
xoshiro128plus(void)
{
   const uint32_t result = _xor.s[0] + _xor.s[3];
   const uint32_t t = _xor.s[1] << 9;

   _xor.s[2] ^= _xor.s[0];
   _xor.s[3] ^= _xor.s[1];
   _xor.s[1] ^= _xor.s[2];
   _xor.s[0] ^= _xor.s[3];

   _xor.s[2] ^= t;

   _xor.s[3] = rotl(_xor.s[3], 11);

   return result;
}


float
_aax_rand_sample()
{
   float r = (double)(int64_t)xoroshiro128plus()/(double)INT64_MAX;
   return r;
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

void
_aax_srand(uint64_t a) {
   _aax_seed = a;
}

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
