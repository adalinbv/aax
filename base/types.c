/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2007-2017 by Adalin B.V.
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>	// _byteswap_ushort, etc
#include <errno.h>

#include "types.h"

#if 0
uint32_t _mem_size(void *p)
{
#if defined(__sgi)
   return mallocblksize(p);
#elif defined(__LINUX__)
   return malloc_usable_size(p);
#elif defined(WIN32)
   return _msize(p);
#endif
};
#endif

uint16_t _aax_bswap16(uint16_t x)
{
#if defined(__llvm__) || \
 (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)) && !defined(__ICC)
   return __builtin_bswap16(x);

#elif defined(_MSC_VER) && !defined(_DEBUG)
   return _byteswap_ushort(x);

#else
   return (x >> 8) | (x << 8);
#endif
}

uint32_t _aax_bswap32(uint32_t x)
{
#if defined(__llvm__) || \
 (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)) && !defined(__ICC)
   return __builtin_bswap32(x);

#elif defined(_MSC_VER) && !defined(_DEBUG)
   return _byteswap_ulong(x);

#else
   x = ((x >>  8) & 0x00FF00FFL) | ((x <<  8) & 0xFF00FF00L);
   return (x >> 16) | (x << 16);
#endif
}

uint32_t _aax_bswap32h(uint32_t x)
{
   return ((x >>  8) & 0x00FF00FFL) | ((x <<  8) & 0xFF00FF00L);
}

uint32_t _aax_bswap32w(uint32_t x)
{
   return (x >> 16) | (x << 16);
}

uint64_t _aax_bswap64(uint64_t x)
{
#if defined(__llvm__) || \
 (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)) && !defined(__ICC)
   return __builtin_bswap64(x);

#elif defined(_MSC_VER) && !defined(_DEBUG)
   return _byteswap_uint64(x);

#else
   x = ((x >>  8) & 0x00FF00FF00FF00FFLL) | ((x <<  8) & 0xFF00FF00FF00FF00LL);
   x = ((x >> 16) & 0x0000FFFF0000FFFFLL) | ((x << 16) & 0xFFFF0000FFFF0000LL);
   return (x >> 32) | (x << 32);
#endif
}

#ifdef WIN32
int _aax_snprintf(char *str, size_t size, const char *fmt, ...)
{
   int ret;
   va_list ap;

   va_start(ap,fmt);
   ret = vsnprintf(str,size,fmt,ap);
   // Whatever happen in vsnprintf, what i'll do is just to null terminate it
   str[size-1] = '\0';
   va_end(ap);
   return ret;
}

char*
_aax_strcasestr(const char *dst, const char *src)
{
   int len, dc, sc;

   if(src[0] == '\0')
      return (char*)(uintptr_t)dst;

   len = strlen(src) - 1;
   sc  = tolower(src[0]);
   for(; (dc = *dst); dst++)
   {
      dc = tolower(dc);
      if(sc == dc && (len == 0 || !strncasecmp(dst+1, src+1, len)))
         return (char*)(uintptr_t)dst;
   }
   return NULL;
}

#endif  /* if defined(WIN32) */

