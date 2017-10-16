/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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


#include <driver.h>

#define COPY_TO_BUFFER	"_ctb237676265365"

#define MAX_ID_STRLEN	64
#define PERIOD_SIZE	4096
#define IOBUF_THRESHOLD	(2*PERIOD_SIZE)
#define IOBUF_SIZE	(2*IOBUF_THRESHOLD)

extern const _aaxDriverBackend _aaxStreamDriverBackend;
extern const _intBuffers _aaxStreamDriverExtensionString;
extern const _intBuffers _aaxStreamDriverEnumValues;

/* Support for IMA4 in wav files */
#define MSIMA_BLOCKSIZE_TO_SMP(b, t)	(((b)-4*(t))*2)/(t)
#define SMP_TO_MSBLOCKSIZE(s, t)	(((s)*(t)/2)+4*(t))
void _wav_cvt_msadpcm_to_ima4(void*, size_t, unsigned int, size_t*);
void _pcm_cvt_lin_to_ima4_block(uint8_t*, int32_t*, unsigned, int16_t*, uint8_t*, short);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_STREAM_DRIVER_H */

