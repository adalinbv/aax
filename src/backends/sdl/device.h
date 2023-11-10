/*
 * SPDX-FileCopyrightText: Copyright © 2018-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2018-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef _AL_SDL_DRIVER_H
#define _AL_SDL_DRIVER_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <backends/driver.h>

extern const _aaxDriverBackend _aaxSDLDriverBackend;

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AL_SDL_DRIVER_H */

