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

#ifndef _AAX_STREAM_DRIVER_H
#define _AAX_STREAM_DRIVER_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif


#include <driver.h>

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
void _wav_cvt_msadpcm_to_ima4(void*, size_t, int, size_t*);
void _pcm_cvt_lin_to_ima4_block(uint8_t*, int32_t*, unsigned, int16_t*, uint8_t*, short);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_STREAM_DRIVER_H */

