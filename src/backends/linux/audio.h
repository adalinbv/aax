/*
 * Copyright 2005-2023 by Erik Hofman.
 * Copyright 2009-2023 by Adalin B.V.
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

