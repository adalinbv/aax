/*
 * Copyright 2012 by Erik Hofman.
 * Copyright 2012 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_FILETYPE_H
#define _AAX_FILETYPE_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

typedef void* (_connect_fn)(const char*, int);
typedef int (_disconnect_fn)(void*);
typedef int (_update_fn)(void*, void*);
typedef char* (_get_extension_fn)(void);

typedef struct
{
   _connect_fn *connect;
   _disconnect_fn *disconnect;
   _update_fn *update;
   _get_extension_fn *get_extension;

} _aaxFileHandle;


typedef _aaxFileHandle* (_detect_fn)(void);

_detect_fn _aaxDetectWavFile;


#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_FILETYPE_H */

