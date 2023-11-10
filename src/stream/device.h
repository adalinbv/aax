/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef _AAX_STREAM_DRIVER_H
#define _AAX_STREAM_DRIVER_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif


#include <backends/driver.h>

#define COPY_TO_BUFFER	"_ctb237676265365"

//#define PERIOD_SIZE	16384
#define PERIOD_SIZE	4096
#define IOBUF_THRESHOLD	(2*PERIOD_SIZE)
#define IOBUF_SIZE	(2*IOBUF_THRESHOLD)

extern const _aaxDriverBackend _aaxStreamDriverBackend;
extern const _intBuffers _aaxStreamDriverExtensionString;
extern const _intBuffers _aaxStreamDriverEnumValues;

#define MSIMA_BLOCKSIZE_TO_SMP(b, t)	(((b)-4*(t))*2)/(t)
#define SMP_TO_MSBLOCKSIZE(s, t)	(((s)*(t)/2)+4*(t))

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_STREAM_DRIVER_H */

