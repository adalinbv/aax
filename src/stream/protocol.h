/*
 * Copyright 2012-2016 by Erik Hofman.
 * Copyright 2012-2016 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_PROTOCOL_H
#define _AAX_PROTOCOL_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(__cplusplus)
}  /* extern "C" */
#endif

/* I/O related: file, socket, etc */
typedef enum
{
   PROTOCOL_UNSUPPORTED = -1,
   PROTOCOL_FILE = 0,
   PROTOCOL_HTTP
} _protocol_t;

#endif /* !_AAX_PROTOCOL_H */

