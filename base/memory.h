/*
 * Copyright 2007-2021 by Erik Hofman.
 * Copyright 2009-2021 by Adalin B.V.
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

#ifndef MEMEORY_H
#define MEMORY_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

char is_bigendian();

/* write */
void write8(uint8_t**, uint8_t, size_t*);
void writestr(uint8_t**, const char*, size_t, size_t*);
void writepstr(uint8_t**, const char*, size_t, size_t*);

void write16le(uint8_t**, uint16_t, size_t*);
void write32le(uint8_t**, uint32_t, size_t*);
void write64le(uint8_t**, uint64_t, size_t*);
void writefp80le(uint8_t**, double, size_t*);

void write16be(uint8_t**, uint16_t, size_t*);
void write32be(uint8_t**, uint32_t, size_t*);
void write64be(uint8_t**, uint64_t, size_t*);
void writefp80be(uint8_t**, double, size_t*);

/* read */
uint8_t read8(uint8_t**, size_t*);
size_t readstr(uint8_t**, char*, size_t, size_t*);
size_t readpstr(uint8_t**, char*, size_t, size_t*);

uint16_t read16le(uint8_t**, size_t*);
uint32_t read32le(uint8_t**, size_t*);
uint64_t read64le(uint8_t**, size_t*);
double readfp80le(uint8_t**, size_t*);

uint16_t read16be(uint8_t**, size_t*);
uint32_t read32be(uint8_t**, size_t*);
uint64_t read64be(uint8_t**, size_t*);
double readfp80be(uint8_t**, size_t*);

char *strnstr(const char*, const char*, size_t);
char *strncasestr(const char*, const char*, size_t);
char* stradd(char*, char*);

char* strlcpy(char*, const char*, size_t);


#if defined(__cplusplus)
}  /* extern "C" */
#endif


#endif /* MEMORY_H */

