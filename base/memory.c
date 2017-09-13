/*
 * Copyright 2007-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <string.h>
#endif
#include <ctype.h>	// toupper
#include <assert.h>

#include "memory.h"

/*
 * Taken from FreeBSD:
 * http://src.gnu-darwin.org/src/lib/libc/string/strnstr.c.html
 */
char *
strnstr(const char *s, const char *find, size_t slen)
{
   char c, sc;
   size_t len;

   if ((c = *find++) != '\0')
   {
      len = strlen(find);
      do
      {
         do
         {
            if (slen-- < 1 || (sc = *s++) == '\0') {
               return (NULL);
            }
         }
         while (sc != c);

         if (len > slen) {
            return (NULL);
         }
      }
      while (strncmp(s, find, len) != 0);
      s--;
   }
   return ((char *)s);
}

char *
strncasestr(const char *s, const char *find, size_t slen)
{
   char c, sc;
   size_t len;

   if ((c = *find++) != '\0')
   {
      len = strlen(find);
      do
      {
         do
         {
            if (slen-- < 1 || (sc = *s++) == '\0') {
               return (NULL);
            }
         }
         while (toupper(sc) != toupper(c));

         if (len > slen) {
            return (NULL);
         }
      }
      while (strncasecmp(s, find, len) != 0);
      s--;
   }
   return ((char *)s);
}

char*
stradd(char *src, char *dest)
{
   char *rv;
   if (src)
   {
      rv = realloc(src, strlen(src)+strlen(dest)+3);
      if (rv)
      {
         strcat(rv, ", ");
         strcat(rv, dest);
     }
     else {
        rv = src;
     }
   }
   else {
      rv = strdup(dest);
   }
   return rv;
}

#if 0
#define NTID		0x5adda2d9

static void *
_nt_memptr(const void *ptr, off_t offs, size_t n)
{
    _nt_info *info = (_nt_info *)ptr;

    assert(offs >= 0);
    assert(info != NULL);
    assert(info->id == NTID);
    assert(info->start+(offs+n) < info->end);
   
    return info->start+offs;
}

static void *
_nt_alloc(size_t size, char clear, char *caller)
{
    _nt_info *info = NULL;
    size_t alloc_sz;

    alloc_sz = size + sizeof(_nt_info);
    alloc_sz += NT_UNDERFLOWLEN + NT_OVERFLOWLEN;

    info = malloc(alloc_sz);
    if (info)
    {
        char *bptr;

        info->id = NTID;
        info->alloc_fn = caller;
        info->start = _nt_memptr(info, 0, 0);
        info->end = _nt_memptr(info, size, 0);

        bptr = (char *)info + sizeof(_nt_info);
        memset(bptr, 0x55, NT_UNDERFLOWLEN);
        if (clear) {
            memset(info->start, 0, size);
        }
        memset(info->end, 0x55, NT_OVERFLOWLEN);
    }

    return info;
}

void
nt_free(void *ptr, const char *fname, unsigned int line)
{
    if (ptr)
    {
        _nt_info *info = (_nt_info *)ptr;
        if (info->id == NTID)
        {
            info->id = 0xdeadbeef;
            free(info);
        }
        else
        {
            fprintf(stderr, "MEMORY: bad memory id\n");
            assert(info->id == NTID);
        }
    }
}

void *
nt_malloc(size_t size, const char *fname, unsigned int line)
{
    return _nt_alloc(size, 0, NULL);
}

void *
nt_calloc(size_t nmemb, size_t size, const char *fname, unsigned int line)
{
    return _nt_alloc(nmemb*size, 1, NULL);
}

void *
nt_realloc(void *ptr, size_t size, const char *fname, unsigned int line)
{
    return 0;
}

void *
nt_memcpy(void *dest, off_t o1, const void *src, off_t o2, size_t n, const char *fname, unsigned int line)
{
    void *rv = NULL;
    if (memcpy(_nt_memptr(dest, o1, n), _nt_memptr(src, o2, n), n) != NULL) {
        rv = dest;
    }
    return rv;
}

void *
nt_memset(void *s, off_t o, int c, size_t n, const char *fname, unsigned int line)
{
    void *rv = NULL;
    if (memset(_nt_memptr(s, o, n), c, n) != NULL) {
        rv  = s;
    }
    return rv;
}

int
nt_memcmp(const void *s1, off_t o1, const void *s2, off_t o2, size_t n, const char *fname, unsigned int line)
{
    return memcmp(_nt_memptr(s1, o1, n), _nt_memptr(s2, o2, n), n);
}

char *
nt_strdup(const char *s, off_t o, const char *fname, unsigned int line)
{
    char *rv = NULL;
    if (s)
    {
        size_t slen = strlen(_nt_memptr(s, o, 0));
        rv = nt_strndup(s, o, slen, fname, line);
    }
    return rv;
}

char *
nt_strndup(const char *s, off_t o, size_t n, const char *fname, unsigned int line)
{
    char *rv = NULL;
    if (s && n)
    {
        rv = nt_malloc(n, fname, line);
        if (rv)
        {
            if (nt_memcpy(rv, 0, s, o, n, fname, line) == NULL)
            {
                nt_free(rv, fname, line);
                rv = NULL;
            }
        }
    }
    return rv;
}

char *
nt_strcpy(char *dest, off_t o1,  const char *src, off_t o2, const char *fname, unsigned int line)
{
    char *rv = NULL;
    if (strcpy(_nt_memptr(dest, o1, 0), _nt_memptr(src, o2, 0)) != NULL) {
        rv = dest;
    }
    return rv;
}

char *
nt_strncpy(char *dest, off_t o1, const char *src, off_t o2, size_t n, const char *fname, unsigned int line)
{
    char *rv = NULL;
    if (strncpy(_nt_memptr(dest, o1, 0), _nt_memptr(src, o2, 0), n) != NULL) {
        rv = dest;
    }
    return rv;
}

int
nt_strcmp(const char *s1, off_t o1, const char *s2, off_t o2, const char *fname, unsigned int line)
{
    return strcmp(_nt_memptr(s1, o1, 0), _nt_memptr(s2, o2, 0));
}

int
nt_strncmp(const char *s1, off_t o1, const char *s2, off_t o2, size_t n, const char *fname, unsigned int line)
{
    return strncmp(_nt_memptr(s1, o1, n), _nt_memptr(s2, o2, n), n);
}
#endif

