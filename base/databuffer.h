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
   size_t offset; // or fill-level
   size_t size;   // maximum buffer size

} _data_t;

/**
 * Create a new data structure.
 * Data will be manipulated in blocks of blocksize.
 *
 * size: total size of the buffer
 * blocksize: size of the individual blocks.
 *
 * returns an initialized data structure.
 */
_data_t* _aaxDataCreate(size_t, unsigned int);

/**
 * Destroy and clean up a data structure.
 *
 * buf: the previously created data structure.
 *
 * return AAX_TRUE
 */
int _aaxDataDestroy(_data_t*);

/**
 * Add data to the end of a previously created data structure.
 *
 * buf: the previously created data structure.
 * data: a pointer to the data which should be appended.
 * size: size (in bytes) of the data to be added.
 *
 * returns the actual number of bytes that where added.
 */
size_t _aaxDataAdd(_data_t*, const void*, size_t);

/**
 * Copy data from an offset of a previously created data structure.
 *
 * buf: the previously created data structure.
 * data: the buffer to move the data to. If NULL the data will just be erased.
 * offset: offset in the data buffer.
 * size: size (in bytes) of the data to be (re)moved.
 *
 * returns the actual number of bytes that where moved.
 */
size_t _aaxDataCopy(_data_t*, void*, size_t, size_t);

/**
 * (Re)Move data from the start of a previously created data structure.
 *
 * buf: the previously created data structure.
 * data: the buffer to move the data to. If NULL the data will just be erased.
 * size: size (in bytes) of the data to be (re)moved.
 *
 * returns the actual number of bytes that where moved.
 */
size_t _aaxDataMove(_data_t*, void*, size_t);

/**
 * (Re)Move data from an offset of a previously created data structure.
 *
 * buf: the previously created data structure.
 * data: the buffer to move the data to. If NULL the data will just be erased.
 * offset: offset in the data buffer.
 * size: size (in bytes) of the data to be (re)moved.
 *
 * returns the actual number of bytes that where moved.
 */
size_t _aaxDataMoveOffset(_data_t*, void*, size_t, size_t);

/**
 * Move data from one previously created data structure to another.
 *
 * dst: the previously created destinion data structure.
 * src: the previously created source data structure.
 * size: size (in bytes) of the data to be moved.
 *
 * returns the actual number of bytes that where moved.
 */
size_t _aaxDataMoveData(_data_t*, _data_t*, size_t);

/**
 * clear the buffer for reuse for another task.
 */
void _aaxDataClear(_data_t*);

/**
 * returns the maximum allowd number of bytes for the buffer.
 */
size_t _aaxDataGetSize(_data_t*);

/**
 * returns the number of bytes free to use after the assigned area.
 */
size_t _aaxDataGetFreeSpace(_data_t*);

/**
 * returns the number of bytes of data in use in the buffer.
 */
size_t _aaxDataGetDataAvail(_data_t*);

/**
 * try to increase the offset-pointer by offs bytes.
 * this can be useful if additional data was added by the calling process.
 *
 * returns the actual number of bytes the offset was increased.
 */
ssize_t _aaxDataIncreaseOffset(_data_t*, size_t offs);

/**
 * try to set the offset-pointer offs number of bytes from the start.
 * this can be useful if new data was added by the calling process.
 *
 * returns the actual number of bytes the offset was set.
 */
ssize_t _aaxDataSetOffset(_data_t*, size_t offs);

/**
 * returns the current offset-pointer in bytes.
 */
size_t _aaxDataGetOffset(_data_t*);


/**
 * returns a pointer to the start of the data-section.
 */
void* _aaxDataGetData(_data_t*);

/**
 * returns a pointer to the data-section at the current offset.
 *
 * this can be used to add additional data to the buffer.
 */
void* _aaxDataGetPtr(_data_t*);


#endif /* AAX_DATABUFFER_H */
