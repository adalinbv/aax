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

