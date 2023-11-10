/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef _ALSA_AUDIO_H
#define _ALSA_AUDIO_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if 0
#ifndef RELEASE
# ifdef NDEBUG
#  undef NDEBUG
# endif
#endif
#endif

int _aaxPipeWireDriverDetect(int);
int _aaxPulseAudioDriverDetect(int);
int _aaxALSADriverDetect(int);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* _ALSA_AUDIO_H */

