/*
 * Copyright (C) 2005-2011 by Erik Hofman.
 * Copyright (C) 2007-2011 by Adalin B.V.
 *
 * This file is part of OpenAL-AeonWave.
 *
 *  OpenAL-AeonWave is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenAL-AeonWave is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OpenAL-AeonWave.  If not, see <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>	/* for atoi */
#include <stdio.h>	/* for snprintf */

#include "types.h"


char *_oalGetSymError(const char *err)
{
   static char *error = 0;
   char *rv = error;
   error = (char *)err;
   return rv;
}

#if defined( __APPLE__ )
#include <CoreFoundation/CoreFoundation.h>

void *_oalIsLibraryPresent(const char *name, const char *version)
{
   return 0;
}

void *_oalGetProcAddress(void *handle, const char *func)
{
   return 0;
}

void* _oalGetGlobalProcAddress(const char *func)
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

#elif defined( WIN32 )
#include <direct.h>
#include <windows.h>

void *
_oalIsLibraryPresent(const char *name, const char *version)
{
   HINSTANCE handle;

   if (name)
   {
      char libname[255];
      snprintf(libname, 255, "%s", name); /* windows will add .dll */
      handle = LoadLibrary(TEXT(libname));
   }
   else {
      handle = GetModuleHandle(name);
   }
   if (!handle) _oalGetSymError("Library not found.");

   return handle;
}

void *
_oalGetProcAddress(void *handle, const char *func)
{
   static LPTSTR Error = 0;
   void *rv = NULL;

   assert(handle);
   assert(func);

   rv = (void *)GetProcAddress(handle, func);
   if (!rv)
   {
      DWORD err = GetLastError();
      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL, err, 0, (LPTSTR)&Error, 0, NULL);
      _oalGetSymError(Error);
   }

   return rv;
}

/* TODO */
void *_oalGetGlobalProcAddress(const char *func)
{
   return 0;
}

#elif HAVE_DLFCN_H /* UNIX */
#include <dlfcn.h>

void *
_oalIsLibraryPresent(const char *name, const char *version)
{
   const char *lib = name;
   char libname[255];

   _oalGetSymError(dlerror());

   if (name)
   {
      if (version && atoi(version) != 0) {
         snprintf(libname, 255, "lib%s.so.%s", name, version);
      } else {
         snprintf(libname, 255, "lib%s.so", name);
      }
      lib = libname;
   }
   return dlopen(lib, RTLD_LAZY);
}

void *
_oalGetProcAddress(void *handle, const char *func)
{
   void *fptr;
   char *error;

   assert(handle);
   assert(func);

   fptr = dlsym(handle, func);
   error = (char *)dlerror();
   if (error)
   {
      _oalGetSymError(error);
      return 0;
   }

   return fptr;
}

void *
_oalGetGlobalProcAddress(const char *func)
{
   static void *libHandle = 0;
   void *fptr = 0;

   _oalGetSymError(dlerror());

   if (libHandle == 0)
      libHandle = dlopen(0, RTLD_LAZY);

   if (libHandle != 0)
   {
      const char *error;

      fptr = dlsym(libHandle, func);

      error = _oalGetSymError(dlerror());
      if (error) return 0;
   }

   return fptr;
}

#else
#error Needs implementation.
#endif

