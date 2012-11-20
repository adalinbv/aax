/*
 * Copyright 2011-2012 by Erik Hofman.
 * Copyright 2011-2012 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AL_MMDEVAPI_DRIVER_H
#define _AL_MMDEVAPI_DRIVER_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <driver.h>

extern const _aaxDriverBackend _aaxWASAPIDriverBackend;

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AL_MMDEVAPI_DRIVER_H */

