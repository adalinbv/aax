/*
 * Copyright 2005-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AL_SOFTWARE_DRIVER_H
#define _AL_SOFTWARE_DRIVER_H 1

#include <driver.h>

extern const _aaxDriverBackend _aaxNoneDriverBackend;
extern const _aaxDriverBackend _aaxLoopbackDriverBackend;
extern _aaxDriverBackend _aaxSoftwareDriverBackend;
extern const _intBuffers _aaxSoftwareDriverExtensionString;
extern const _intBuffers _aaxSoftwareDriverEnumValues;

#endif /* !_AL_SOFTWARE_DRIVER_H */

