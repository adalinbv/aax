/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef AAX_DATABUFFER_H
#define AAX_DATABUFFER_H 1

#include <unistd.h>

typedef struct _data_st
{
   unsigned int id;

   unsigned char **data;

   unsigned char no_buffers;
   unsigned int blocksize;
   size_t *offset; // or fill-level
   size_t size;   // maximum buffer size

} _data_t;

/**
 * Create a new data structure.
 * If blocksize is zero it will be internally set to one.
 * Data will be manipulated in blocks of blocksize.
 * The number of allocated bytes will be size*blocksize.
 *
 * no_buffers: number of separate buffers each with their own offset but all
               with the same size.
 * size: total size of each sepatate buffer
 * blocksize: size of the individual blocks.
 *
 * returns an initialized data structure.
 */
_data_t* _aaxDataCreate(unsigned char no_buffers, size_t size, unsigned int blocksize);

/**
 * Destroy and clean up a data structure.
 *
 * buf: the previously created data structure.
 *
 * return true
 */
int _aaxDataDestroy(_data_t* buf);

/**
 * Add data to the end of a previously created data structure.
 *
 * buf: the previously created data structure.
 * buffer_no: buffer number to which to add the data.
 * data: a pointer to the data which should be appended.
 * size: size (in bytes) of the data to be added.
 *
 * returns the actual number of bytes that where added.
 */
size_t _aaxDataAdd(_data_t* buf, unsigned char buffer_no, const void* data, size_t size);

/**
 * Copy data from an offset to the start of the internal data buffer of a
 * previously created data structure. No data will me removed from the
 * internal data buffer.
 *
 * buf: the previously created data structure.
 * buffer_no: buffer number from which to copy the data.
 * data: the buffer to move the data to.
 * offset: offset in the internal data buffer.
 * size: size (in bytes) of the data to be (re)moved.
 *
 * returns the actual number of bytes that where copied rounded of to blocksize,
 * or 0 if dat == NULL, buffer_no >= no_buffers, size < blocksize or 
 *      offset+size > available number of bytes.
 */
size_t _aaxDataCopy(_data_t* buf, unsigned char buffer_no, void* data, size_t offset, size_t size);

/**
 * (Re)Move data from the start of a previously created data structure.
 *
 * buf: the previously created data structure.
 * buffer_no: buffer number from which to move the data.
 * data: the buffer to move the data to. If NULL the data will just be erased
 *       from the internal buffer.
 * size: size (in bytes) of the data to be (re)moved.
 *
 * returns the actual number of bytes that where moved rounded of to blocksize,
 * or 0 if buffer_no >= no_buffers, size < blocksize or 
 *      offset+size > available number of bytes.
 */
size_t _aaxDataMove(_data_t* buf, unsigned char buffer_no, void* data, size_t size);

/**
 * (Re)Move data from an offset to the start of the internal data buffer of a
 * previously created data structure.
 *
 * buf: the previously created data structure.
 * buffer_no: buffer number from which to (re)move the data.
 * data: the buffer to move the data to. If NULL the data will just be erased
 *       from the internal buffer.
 * offset: offset in the data buffer.
 * size: size (in bytes) of the data to be (re)moved.
 *
 * returns the actual number of bytes that where moved rounded of to blocksize,
 * or 0 if buffer_no >= no_buffers, size < blocksize or 
 *      offset+size > available number of bytes.
 */
size_t _aaxDataMoveOffset(_data_t* buf, unsigned char buffer_no, void* data, size_t offset, size_t size);

/**
 * Move data from one previously created data structure to another.
 *
 * src: the previously created destinion data structure.
 * src_no: buffer number to which to add the data. 
 * dst: the previously created source data structure.
 * dst_no: buffer number  from which to move the data.
 * size: size (in bytes) of the data to be moved.
 *
 * returns the actual number of bytes that where moved rounded of to blocksize,
 * or 0 if src_no > no_buffers in the source buffer, dst_no > no_buffers in
 *         the destination buffer or size < the source or destination blocksize.
 */
size_t _aaxDataMoveData(_data_t* src, unsigned char src_no, _data_t* dst, unsigned char dst_no, size_t size);

/**
 * clear the buffer for reuse for another task.
 *
 * buf: the previously created data structure.
 * buffer_no: buffer number for which to clear the data. Use -1 for all.
 */
void _aaxDataClear(_data_t* buf, unsigned char buffer_no);

/**
 * returns the maximum allowd number of bytes for each buffer.
 *
 * buf: the previously created data structure.
 */
size_t _aaxDataGetSize(_data_t* buf);

/**
 * returns the number of bytes free to use after the assigned area.
 *
 * buf: the previously created data structure.
 * buffer_no: buffer number from which to return the free space.
 */
size_t _aaxDataGetFreeSpace(_data_t* buf, unsigned char buffer_no);

/**
 * returns the number of bytes of data in use in the buffer.
 *
 * buf: the previously created data structure.
 * buffer_no: buffer number from which to return the fill level.
 */
size_t _aaxDataGetDataAvail(_data_t* buf, unsigned char buffer_no);

/**
 * try to increase the offset-pointer by offs bytes.
 * this can be useful if additional data was added by the calling process. *
 *
 * buf: the previously created data structure.
 * buffer_no: buffer number for which to increase the offset.
 *
 * returns the actual number of bytes the offset was increased.
 */
ssize_t _aaxDataIncreaseOffset(_data_t* buf, unsigned char buffer_no, size_t offs);

/**
 * try to set the offset-pointer offs number of bytes from the start.
 * this can be useful if new data was added by the calling process.
 *
 * buf: the previously created data structure.
 * buffer_no: buffer number for which to set the offset.
 *
 * returns the actual number of bytes the offset was set.
 */
ssize_t _aaxDataSetOffset(_data_t* buf, unsigned char buffer_no, size_t offs);

/**
 * returns the current offset-pointer in bytes.
 *
 * buf: the previously created data structure.
 * buffer_no: buffer number from which to return the offset.
 */
size_t _aaxDataGetOffset(_data_t* buf, unsigned char buffer_no);


/**
 * returns a pointer to the start of the data-section.
 *
 * buf: the previously created data structure.
 * buffer_no: buffer number from which to return the data pointer.
 */
void* _aaxDataGetData(_data_t* buf, unsigned char buffer_no);

/**
 * returns a pointer to the data-section at the current offset.
 *
 * buf: the previously created data structure.
 * buffer_no: buffer number from which to return the data pointer.
 *
 * this can be used to add additional data to the buffer.
 */
void* _aaxDataGetPtr(_data_t* buf, unsigned char buffer_no);


#endif /* AAX_DATABUFFER_H */
