/*
 * Copyright 2005-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _ALSASOFT_DRIVER_H
#define _ALSASOFT_DRIVER_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <driver.h>

extern const _aaxDriverBackend _aaxLinuxDriverBackend;
extern const _aaxDriverBackend _aaxALSADriverBackend;

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_ALSASOFT_DRIVER_H */

