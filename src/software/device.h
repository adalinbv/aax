/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_SOFTWARE_DRIVER_H
#define _AAX_SOFTWARE_DRIVER_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif


#include <driver.h>

extern const _aaxDriverBackend _aaxNoneDriverBackend;
extern const _aaxDriverBackend _aaxLoopbackDriverBackend;

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_SOFTWARE_DRIVER_H */

