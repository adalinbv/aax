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

#ifndef __AAX_RANDOM_H
#define __AAX_RANDOM_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include <base/types.h>

#define _aax_random()	((double)xoroshiro128plus()/(double)UINT64_MAX)

void _aax_srandom();
uint64_t xorshift128plus();
uint64_t xoroshiro128plus();
uint32_t xoshiro128plus();

float _aax_rand_sample();


void _aax_srand(uint64_t);
uint64_t _aax_rand();

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !__AAX_RANDOM_H */

