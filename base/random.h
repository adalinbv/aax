/*
 * SPDX-FileCopyrightText: Copyright © 2017-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2017-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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

