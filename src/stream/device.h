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

#ifndef _AL_SOFTWARE_DRIVER_H
#define _AL_SOFTWARE_DRIVER_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif


#include <driver.h>

#define MAX_ID_STRLEN	64
#define PERIOD_SIZE	4096
#define IOBUF_SIZE	4*PERIOD_SIZE
#ifndef O_BINARY
# define O_BINARY	0
#endif

extern const _aaxDriverBackend _aaxNoneDriverBackend;
extern const _aaxDriverBackend _aaxLoopbackDriverBackend;
extern const _aaxDriverBackend _aaxStreamDriverBackend;
extern const _intBuffers _aaxStreamDriverExtensionString;
extern const _intBuffers _aaxStreamDriverEnumValues;

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AL_SOFTWARE_DRIVER_H */

