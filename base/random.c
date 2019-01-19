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

static int
_aax_hash3(unsigned int h1, unsigned int h2, unsigned int h3) {
    return (((h1 * 2654435789U) + h2) * 2654435789U) + h3;
}

// https://en.wikipedia.org/wiki/Xorshift#xorshift+
/* This generator is one of the fastest generators passing BigCrush */
/* The state must be seeded so that it is not all zero */
static uint64_t _xor_s[2];

uint64_t
xorshift128plus()
{
   uint64_t x = _xor_s[0];
   uint64_t const y = _xor_s[1];

   _xor_s[0] = y;
   x ^= x << 23;
   _xor_s[1] = x ^ y ^ (x >> 17) ^ (y >> 26);
   return _xor_s[1] + y;
}

float
_aax_rand_sample()
{
   float r = (double)(int64_t)xorshift128plus()/(double)INT64_MAX;
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
      num = getrandom(_xor_s, size, 0);
#endif

      if (num < size)
      {
         struct timeval time;

         gettimeofday(&time, NULL);
         srand(_aax_hash3(time.tv_sec, time.tv_usec, getpid()));
         _xor_s[0] = (uint64_t)rand() * rand();
         _xor_s[1] = ((uint64_t)rand() * rand()) ^ _xor_s[0];
      }
   }
}

// https://en.wikipedia.org/wiki/MurmurHash
// A sample C implementation follows (for little-endian CPUs) 
uint32_t
_aax_murmur3_32(const uint8_t* key, size_t len, uint32_t seed)
{
    uint32_t h = seed;
    if (len > 3) {
        const uint32_t* key_x4 = (const uint32_t*) key;
        size_t i = len >> 2;
        do {
            uint32_t k = *key_x4++;
            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
            h = (h << 13) | (h >> 19);
            h = (h * 5) + 0xe6546b64;
        } while (--i);
        key = (const uint8_t*) key_x4;
    }
      if (len & 3) {
        size_t i = len & 3;
        uint32_t k = 0;
        key = &key[i - 1];
        do {
            k <<= 8;
            k |= *key--;
        } while (--i);
        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        h ^= k;
    }
    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

