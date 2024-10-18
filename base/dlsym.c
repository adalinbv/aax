/*
 * SPDX-FileCopyrightText: Copyright © 2007-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
#endif
#include <stdio.h>	/* for snprintf */

#include "dlsym.h"
#include "types.h"


char *
_aaxGetSymError(const char *err)
{
   static char *error = 0;
   char *rv = error;
   error = (char *)err;
   return rv;
}

#if defined( __APPLE__ )
#include <CoreFoundation/CoreFoundation.h>

void*
_aaxIsLibraryPresent(const char *name, const char *version)
{
   return 0;
}

void
_aaxCloseLibrary(void *lib)
{
}


DLL_RV
_aaxGetProcAddress(void *handle, const char *func)
{
   return 0;
}

void*
_aaxGetGlobalProcAddress(const char *func)
{
   static CFBundleRef bundle = 0;
   void *function = 0;

   if (!bundle)
   {
    CFURLRef bundleURL = CFURLCreateWithFileSystemPath (
                           kCFAllocatorDefault,
                           CFSTR("/System/Library/Frameworks/OpenGL.framework"),
                           kCFURLPOSIXPathStyle, true);

    bundle = CFBundleCreate (kCFAllocatorDefault, bundleURL);
    CFRelease (bundleURL);
   }

   if (!bundle)
      return 0;

   CFStringRef functionName = CFStringCreateWithCString (
                                 kCFAllocatorDefault, func,
                                 kCFStringEncodingASCII);
  
  function = CFBundleGetFunctionPointerForName (bundle, functionName);

  CFRelease (functionName);

  return function;
}

#elif defined( _WIN32 )
#include <direct.h>

void*
_aaxIsLibraryPresent(const char *name, const char *version)
{
   HINSTANCE handle;

   if (name) {
      handle = LoadLibraryA(name);
   }
   else {
      handle = GetModuleHandle(name);
   }
   if (!handle) _aaxGetSymError("Library not found.");

   return handle;
}

void
_aaxCloseLibrary(void *lib)
{
   if (lib) FreeLibrary(lib);
}

DLL_RV
_aaxGetProcAddress(void *handle, const char *func)
{
   DLL_RV rv = NULL;

   assert(handle);
   assert(func);

   rv = (DLL_RV)GetProcAddress(handle, func);
   if (!rv)
   {
      static LPTSTR Error[255];
      DWORD err = GetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, (LPTSTR)Error,
                    255, NULL);
      _aaxGetSymError((const char*)Error);
   }

   return rv;
}

/* TODO */
void*
_aaxGetGlobalProcAddress(const char *func)
{
   return 0;
}

#elif HAVE_DLFCN_H /* UNIX */
#include <dlfcn.h>

void*
_aaxIsLibraryPresent(const char *name, const char *version)
{
   const char *lib = name;
   char libname[255];

   _aaxGetSymError(dlerror());

   if (name)
   {
      if (version) { //  && atoi(version) != 0) {
         snprintf(libname, 255, "lib%s.so.%s", name, version);
      } else {
         snprintf(libname, 255, "lib%s.so", name);
      }
      lib = libname;
   }
   return dlopen(lib, RTLD_LAZY);
}

void
_aaxCloseLibrary(void *lib)
{
   if (lib) dlclose(lib);
}

DLL_RV
_aaxGetProcAddress(void *handle, const char *func)
{
   void *fptr;
   char *error;

   assert(handle);
   assert(func);

   dlerror(); // clear previous errors
   fptr = dlsym(handle, func);
   error = (char *)dlerror();
   if (error)
   {
      _aaxGetSymError(error);
      return 0;
   }

   return fptr;
}

void*
_aaxGetGlobalProcAddress(const char *func)
{
   static void *libHandle = 0;
   void *fptr = 0;

   _aaxGetSymError(dlerror());

   if (libHandle == 0)
      libHandle = dlopen(0, RTLD_LAZY);

   if (libHandle != 0)
   {
      const char *error;

      fptr = dlsym(libHandle, func);

      error = _aaxGetSymError(dlerror());
      if (error) return 0;
   }

   return fptr;
}

#else
#error Needs implementation.
#endif

