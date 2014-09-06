/*
 * Copyright 2014 by Erik Hofman.
 * Copyright 2014 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
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

