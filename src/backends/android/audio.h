/*
 * SPDX-FileCopyrightText: Copyright © 2014-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2014-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef __SLES_AUDIO_H
#define __SLES_AUDIO_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

typedef void (*AndroidAudioCallback)(short *buffer, int num_samples);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* __SLES_AUDIO_H */

