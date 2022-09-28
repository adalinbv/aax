/*
 * Copyright 2007-2022 by Erik Hofman.
 * Copyright 2009-2022 by Adalin B.V.
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

#ifndef AAX_DATABUFFER_H
#define AAX_DATABUFFER_H 1

#include <unistd.h>

#define DATA_ID 0xDFA82736
typedef struct _data_st
{
   unsigned int id;

   unsigned int blocksize;
   unsigned char *data;
   size_t offset;
   size_t size;

} _data_t;

_data_t* _aaxDataCreate(size_t, unsigned int);
int _aaxDataDestroy(_data_t*);
size_t _aaxDataAdd(_data_t*, const void*, size_t);
size_t _aaxDataCopy(_data_t*, void*, size_t, size_t);
size_t _aaxDataMove(_data_t*, void*, size_t);
size_t _aaxDataMoveOffset(_data_t*, void*, size_t, size_t);
size_t _aaxDataMoveData(_data_t*, _data_t*, size_t);
void _aaxDataClear(_data_t*);

size_t _aaxDataGetSize(_data_t*);
size_t _aaxDataGetFreeSpace(_data_t*);
size_t _aaxDataGetDataAvail(_data_t*);
ssize_t _aaxDataIncreaseOffset(_data_t*, size_t);
ssize_t _aaxDataSetOffset(_data_t*, size_t);
size_t _aaxDataGetOffset(_data_t*);

void* _aaxDataGetData(_data_t*);
void* _aaxDataGetPtr(_data_t*);


#endif /* AAX_DATABUFFER_H */
