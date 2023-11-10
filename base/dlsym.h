/*
 * SPDX-FileCopyrightText: Copyright © 2007-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef _OAL_DLSYM_H
#define _OAL_DLSYM_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef _WIN32
# include "types.h"

# define DLL_API		WINAPI
# ifdef STRICT 
#  define DLL_RV		WNDPROC
# else
#  define DLL_RV		FARPROC
# endif
# define TIE_FUNCTION(f)         p##f = (f##_proc)_aaxGetProcAddress(audio, #f)

#else	/* ifdef _WIN32 */
# define DLL_API	
# define DLL_RV			void*
# define TIE_FUNCTION(f)         *(void**)(&p##f) = _aaxGetProcAddress(audio, #f)
#endif

#define DECL_FUNCTION(f)	f##_proc p##f = 0
#define DECL_STATIC_FUNCTION(f)	static f##_proc p##f = 0

#define DECL_VARIABLE(v)	static void* p##v = 0
#define TIE_VARIABLE(v)		p##v = _aaxGetProcAddress(audio, #v)

char *_aaxGetSymError(const char *error);
void *_aaxIsLibraryPresent(const char *name, const char *version);
void _aaxCloseLibrary(void*);
DLL_RV _aaxGetProcAddress(void *handle, const char *func);
void *_aaxGetGlobalProcAddress(const char *func);

#if defined(__cplusplus)
}  /* extern "C" */
#endif


#endif /* !_OAL_DLSYM_H */

