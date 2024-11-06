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

#include <stdio.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <string.h>
#endif
#include <ctype.h>	// toupper
#include <assert.h>

#include "logging.h"
#include "types.h"	// _MIN
#include "memory.h"

#define _DATA_SYSLOG(a)	__aax_log(LOG_SYSLOG, 0, (a), 0, LOG_SYSLOG);

char *
_aax_malloc_aligned(char **start, size_t offs, size_t size)
{
#if !BYTE_ALIGN
   char *rv = malloc(size);
   *start = rv+offs;
   return rv;
#else
   int ctr = 3;
   char *ptr;

   size += offs;
   size = SIZE_ALIGNED(size + MEMALIGN);
   do
   {
      ptr = (char *)malloc(size);
      if (ptr)
      {
         char *s = ptr + offs;
         size_t tmp;

         tmp = (size_t)s & MEMMASK;
         if (tmp)
         {
            tmp = MEMALIGN - tmp;
            s += tmp;
         }
         *start = s;
      }
   }
   while (!ptr && --ctr);

   if (!ptr) {
      _DATA_SYSLOG("Unable to allocate enough memory, giving up after 3 tries");
   }

   return ptr;
#endif
}

char *
_aax_calloc_aligned(char **start, size_t offs, size_t num, size_t size)
{
#if !BYTE_ALIGN

   char *rv = calloc(num, offs+num*size);
   *start = rv+offs;
   return rv;
#else
   int ctr = 3;
   char *ptr;

   size += offs;
   size = SIZE_ALIGNED(size + MEMALIGN);
   do
   {
      ptr = (char*)calloc(1, num*size);
      if (ptr)
      {
         char *s = ptr + offs;
         size_t tmp;

         tmp = (size_t)s & MEMMASK;
         if (tmp)
         {
            tmp = MEMALIGN - tmp;
            s += tmp;
         }
         *start = s;
      }
   }
   while (!ptr && --ctr);

   if (!ptr) {
      _DATA_SYSLOG("Unable to allocate enough memory, giving up after 3 tries");
   }

   return ptr;
#endif
}

void
_aax_free_aligned(void *ptr)
{
   free(ptr);
}

char *
_aax_strdup(const_char_ptr s)
{
   char *ret = 0;
   if (s) {
      size_t len = strlen(s);
      ret = malloc(len+1);
      if (ret) {
         memcpy(ret, s, len+1);
      }
   }
   return ret;
}


char
is_bigendian()
{
   static char __big_endian = 0;
   static char detect = 0;
   if (!detect)
   {
      unsigned int _t = 1;
      __big_endian = (*(char *)&_t == 0);
      detect = 1;
   }
   return __big_endian;
}

// Write data to a stream, byte by byte
void
write8(uint8_t **ptr, uint8_t i, size_t *buflen)
{
   if (*buflen >= 1)
   {
      uint8_t *ch = *ptr;
      *ch++ = i;
      *ptr = ch;
      *buflen -= 1;
   }
}

// Just a series of characters
void
writestr(uint8_t **ptr, const char *s, size_t slen, size_t *buflen)
{
   if (*buflen >= slen)
   {
      uint8_t *ch = *ptr;
      int i;

      for (i=0; i<slen; ++i) {
         *ch++ = (uint8_t)*s++;
      }
      *ptr = ch;
      buflen -= slen;
   }
}

// Pascal-style string: one byte count followed by the text
void
writepstr(uint8_t **ptr, const char *s, size_t slen, size_t *buflen)
{
   write8(ptr, slen, buflen);
   writestr(ptr, s, slen, buflen);
}


void
write16le(uint8_t **ptr, uint16_t i, size_t *buflen)
{
   if (*buflen >= 2)
   {
      uint8_t *ch = *ptr;
      *ch++ = i & 0xFF; i >>= 8;
      *ch++ = i;
      *ptr = ch;
      *buflen -= 2;
   }
}

void
write32le(uint8_t **ptr, uint32_t i, size_t *buflen)
{
   if (*buflen >= 4)
   {
      uint8_t *ch = *ptr;
      *ch++ = i & 0xFF; i >>= 8;
      *ch++ = i & 0xFF; i >>= 8;
      *ch++ = i & 0xFF; i >>= 8;
      *ch++ = i;
      *ptr = ch;
      *buflen -= 4;
   }
}

void
write64le(uint8_t **ptr, uint64_t i, size_t *buflen)
{
   if (*buflen >= 8)
   {
      uint8_t *ch = *ptr;
      *ch++ = i & 0xFF; i >>= 8;
      *ch++ = i & 0xFF; i >>= 8;
      *ch++ = i & 0xFF; i >>= 8;
      *ch++ = i & 0xFF; i >>= 8;
      *ch++ = i & 0xFF; i >>= 8;
      *ch++ = i & 0xFF; i >>= 8;
      *ch++ = i & 0xFF; i >>= 8;
      *ch++ = i;
      *ptr = ch;
      *buflen -= 8;
   }
}

void
writefp80le(uint8_t **ptr, double val, size_t *buflen)
{
   if (*buflen >= 10)
   {
      uint16_t sign = (val < 0) ? 0x800 : 0x000;
      uint64_t d, mantissa;
      uint16_t exponent;

      // Warning: nan becomes inf, not important for us.
      memcpy(&d, &val, sizeof(val));
      exponent = (sign | d >> 60) << 12 | ((d >> 52) & 0xff);
      if (val < 2.0 && val > -2.0) exponent |= 0xF00;
      mantissa = ((d & 0xFFFFFFFFFFFFF) << 11) | 0x8000000000000000;

      write64le(ptr, mantissa, buflen);
      write16le(ptr, exponent, buflen);
   }
}


void
write16be(uint8_t **ptr, uint16_t i, size_t *buflen)
{
   if (*buflen >= 2)
   {
      uint8_t *ch = *ptr;
      *ch++ = i >> 8;
      *ch++ = i & 0xFF;
      *ptr = ch;
      *buflen -= 2;
   }
}

void
write32be(uint8_t **ptr, uint32_t i, size_t *buflen)
{
   if (*buflen >= 4)
   {
      uint8_t *ch = *ptr;
      *ch++ = i >> 24;
      *ch++ = (i >> 16) & 0xFF;
      *ch++ = (i >> 8) & 0xFF;
      *ch++ = i & 0xFF;
      *ptr = ch;
      *buflen -= 4;
   }
}

void
write64be(uint8_t **ptr, uint64_t i, size_t *buflen)
{
   if (*buflen >= 8)
   {
      uint8_t *ch = *ptr;
      *ch++ = i >> 56;
      *ch++ = (i >> 48) & 0xFF;
      *ch++ = (i >> 40) & 0xFF;
      *ch++ = (i >> 32) & 0xFF;
      *ch++ = (i >> 24) & 0xFF;
      *ch++ = (i >> 16) & 0xFF;
      *ch++ = (i >> 8) & 0xFF;
      *ch++ = i & 0xFF;
      *ptr = ch;
      *buflen -= 8;
   }
}

void
writefp80be(uint8_t **ptr, double val, size_t *buflen)
{
   if (*buflen >= 10)
   {
      uint16_t sign = (val < 0) ? 0x800 : 0x000;
      uint64_t d, mantissa;
      uint16_t exponent;

      // Warning: nan becomes inf, not important for us.
      memcpy(&d, &val, sizeof(val));
      exponent = (sign | d >> 60) << 12 | ((d >> 52) & 0xff);
      if (val < 2.0 && val > -2.0) exponent |= 0xF00;
      mantissa = ((d & 0xFFFFFFFFFFFFF) << 11) | 0x8000000000000000;

      write16be(ptr, exponent, buflen);
      write64be(ptr, mantissa, buflen);
   }
}

// Read data from a stream, byte by byte
uint8_t
read8(uint8_t **ptr, size_t *buflen)
{
   uint8_t u8 = 0;
   if (*buflen > 1)
   {
      uint8_t *ch = *ptr;
      u8 = *ch++;
      *buflen -= 1;
      *ptr = ch;
   }
   return u8;
}

uint16_t
read16le(uint8_t **ptr, size_t *buflen)
{
   uint16_t u16 = 0;
   if (*buflen > 2)
   {
      uint8_t *ch = *ptr;
      u16 = (uint64_t)*ch++;
      u16 |= (uint64_t)*ch++ << 8;
      *buflen -= 2;
      *ptr = ch;
   }
   return u16;
}

uint32_t
read32le(uint8_t **ptr, size_t *buflen)
{
   uint32_t u32 = 0;
   if (*buflen > 4)
   {
      uint8_t *ch = *ptr;
      u32 = (uint64_t)*ch++;
      u32 |= (uint64_t)*ch++ << 8;
      u32 |= (uint64_t)*ch++ << 16;
      u32 |= (uint64_t)*ch++ << 24;
      *buflen -= 4;
      *ptr = ch;
   }
   return u32;
}

uint64_t
read64le(uint8_t **ptr, size_t *buflen)
{
   uint64_t u64 = 0;
   if (*buflen > 8)
   {
      uint8_t *ch = *ptr;
      u64 = (uint64_t)*ch++;
      u64 |= (uint64_t)*ch++ << 8;
      u64 |= (uint64_t)*ch++ << 16;
      u64 |= (uint64_t)*ch++ << 24;
      u64 |= (uint64_t)*ch++ << 32;
      u64 |= (uint64_t)*ch++ << 40;
      u64 |= (uint64_t)*ch++ << 48;
      u64 |= (uint64_t)*ch++ << 56;
      *buflen -= 8;
      *ptr = ch;
   }
   return u64;
}

double // https://en.wikipedia.org/wiki/Extended_precision
readfp80le(uint8_t **ptr, size_t *buflen)
{
   double d = 0.0;
   if (*buflen >= 10)
   {
      uint64_t mantissa = read64le(ptr, buflen);
      uint32_t exponent = read16le(ptr, buflen);
      double sign = (exponent & 0x8000) ? -1 : 1;
      double normalized = (mantissa & 0x8000000000000000) ? 1 : 0;
      mantissa &= 0x7FFFFFFFFFFFFFFF;
      exponent &= 0x7FFF;

      // construct the double precision floating point value:
      // Warning: nan becomes inf, not important for us.
      d = (sign * (normalized + (double)mantissa /
                 ((uint64_t)1 << 63)) * pow(2.0, ((int32_t)exponent - 16383)));
   }
   return d;
}

uint16_t
read16be(uint8_t **ptr, size_t *buflen)
{
   uint16_t u16 = 0;
   if (*buflen > 2)
   {
      uint8_t *ch = *ptr;
      u16 = (uint64_t)*ch++ << 8;
      u16 |= (uint64_t)*ch++;
      *buflen -= 2;
      *ptr = ch;
   }
   return u16;
}

uint32_t
read32be(uint8_t **ptr, size_t *buflen)
{
   uint32_t u32 = 0;
   if (*buflen > 4)
   {
      uint8_t *ch = *ptr;
      u32 = (uint64_t)*ch++ << 24;
      u32 |= (uint64_t)*ch++ << 16;
      u32 |= (uint64_t)*ch++ << 8;
      u32 |= (uint64_t)*ch++;
      *buflen -= 4;
      *ptr = ch;
   }
   return u32;
}

uint64_t
read64be(uint8_t **ptr, size_t *buflen)
{
   uint64_t u64 = 0;
   if (*buflen > 8)
   {
      uint8_t *ch = *ptr;
      u64 = (uint64_t)*ch++ << 56;
      u64 |= (uint64_t)*ch++ << 48;
      u64 |= (uint64_t)*ch++ << 40;
      u64 |= (uint64_t)*ch++ << 32;
      u64 |= (uint64_t)*ch++ << 24;
      u64 |= (uint64_t)*ch++ << 16;
      u64 |= (uint64_t)*ch++ << 8;
      u64 |= (uint64_t)*ch++;
      *buflen -= 8;
      *ptr = ch;
   }
   return u64;
}

double
readfp80be(uint8_t **ptr, size_t *buflen)
{
   double d = 0.0;
   if (*buflen >= 10)
   {
      uint32_t exponent = read16be(ptr, buflen);
      uint64_t mantissa = read64be(ptr, buflen);
      double sign = (exponent & 0x8000) ? -1 : 1;
      double normalized = (mantissa & 0x8000000000000000) ? 1 : 0;
      mantissa &= 0x7FFFFFFFFFFFFFFF;
      exponent &= 0x7FFF;

      // construct the double precision floating point value:
      // Warning: nan becomes inf, not important for us.
      d = (sign * (normalized + (double)mantissa /
                 ((uint64_t)1 << 63)) * pow(2.0, ((int32_t)exponent - 16383)));
   }
   return d;
}

// C-style string: text followed by a NULL character
size_t
readstr(uint8_t **ptr, char *buf, size_t len, size_t *buflen)
{
   size_t max = _MIN(len, *buflen);
   uint8_t *ch = *ptr;
   size_t i;

   for (i=0; i<max; ++i) {
      *buf++ = *ch++;
   }
   *buflen -= max;
   *ptr = ch;
   *buf = '\0';

   return max;
}

// Pascal-style string: one byte count followed by the text
size_t
readpstr(uint8_t **ptr, char *buf, size_t len, size_t *buflen)
{
   size_t max = _MIN(len, *buflen);
   uint8_t *ch = *ptr;
   uint8_t i;

   buf[*buflen] = '\0';

   i = *ch++;
   max = _MIN(i, max);
   for (i=0; i<max; ++i) {
      *buf++ = *ch++;
   }
   *buflen -= max;
   *ptr = ch;
   *buf = '\0';

   return max;
}

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

/* a case insensitive strnstr function */
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

/* replace dst with src and reallocate a larger buffer for dst if nequired */
char*
strreplace(char *dst, const char *src)
{
   char *rv;
   if (dst)
   {
      size_t slen = strlen(src);

      if (slen > strlen(dst)) rv = realloc(dst, slen+1);
      else rv = dst;

      if (rv) {
         memcpy(rv, src, slen);
      }
   }
   else {
      rv = strdup(src);
   }
   return rv;
}

/* add the contents of src to dst, possibly separated with a comma */
char*
stradd(char *dst, const char *src)
{
   char *rv;
   if (dst)
   {
      rv = realloc(dst, strlen(src)+strlen(dst)+3);
      if (rv)
      {
         if (*rv) strcat(rv, ", ");
         strcat(rv, src);
     }
     else {
        rv = dst;
     }
   }
   else {
      rv = strdup(src);
   }
   return rv;
}

/* append src to dst which is a pre-allocated buffer of dlen size. */
int
strappend(char *dst, const char *src, ssize_t dlen)
{
   int rv = -1;
   if (dst && dlen)
   {
      ssize_t slen = strlen(src);
      if (dlen > slen)
      {
         char *ptr = dst+strlen(dst);
         memcpy(ptr, src, slen+1);
         rv = slen;
      }
   }
   return rv;
}

#ifndef HAVE_STRLCPY
size_t
strlcpy(char *dst, const char *src, size_t n)
{
   size_t rv = 0;
   if (n > 0)
   {
      memcpy(dst, src, _MIN(strlen(src)+1, n-1));
      dst[n-1] = '\0';
      rv = strlen(dst);
   }
   return rv;
}
#endif
