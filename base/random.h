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

extern union
{
   uint64_t xs[2];
   uint32_t s[4];
} _xor;

void _aax_srandom(void);
uint64_t xorshift128plus(void);
uint64_t xoroshiro128plus(void);

float _aax_random(void);
float _aax_seeded_random(void);
float _aax_rand_sample(void);
void _aax_rand_sample8(float[8]);

uint64_t _aax_rand(void);
void _aax_srand(uint64_t);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !__AAX_RANDOM_H */

