/*
 * Copyright 2017 by Erik Hofman.
 * Copyright 2017 by Adalin B.V.
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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif
#endif
#include <assert.h>

#include "api.h"
#include "arch.h"


/**
 * Create a new data structure.
 * Data will be manipulated in blocks of blocksize.
 *
 * size: total size of the buffer
 * blocksize: size of the individual blocks.
 *
 * returns an initialized data structure.
 */
_data_t*
_aaxDataCreate(size_t size, unsigned int blocksize)
{
   _data_t* rv = malloc(sizeof(_data_t));
   if (rv)
   {
      rv->data = _aax_aligned_alloc(size);
      if (rv->data)
      {
         rv->id = DATA_ID;
         rv->size = size;
         rv->avail = 0;
         rv->blocksize = blocksize ? blocksize : 1;
      }
      else
      {
         free(rv);
         rv = NULL;
      }
   }
   return rv;
}

/**
 * Destroy and clean up a data structure.
 *
 * buf: the previously created data structure.
 *
 * return AAX_TRUE
 */
int
_aaxDataDestroy(_data_t* buf)
{
   if (buf)
   {
      assert(buf->id == DATA_ID);

      buf->id = FADEDBAD;

      _aax_aligned_free(buf->data);
      free(buf);
   }

   return AAX_TRUE;
}

/**
 * Add data to the end of a previously created data structure.
 *
 * buf: the previously created data structure.
 * data: a pointer to the data which should be appended.
 * size: size (in bytes) of the data to be added.
 *
 * returns the actual number of bytes that where added.
 */
size_t
_aaxDataAdd(_data_t* buf, void* data, size_t size)
{
   size_t free, rv = 0;

   assert(buf);
   assert(buf->id == DATA_ID);
   assert(data);

   free = buf->size - buf->avail;
   if (size > free) rv = free;
   else rv = size;

   if (rv)
   {
      memcpy(buf->data+buf->avail, data, rv);
      buf->avail += rv;
   }

   return rv;
}

/**
 * Copy data from the start of a previously created data structure.
 *
 * buf: the previously created data structure.
 * data: the buffer to move the data to. If NULL the data will just be erased.
 * size: size (in bytes) of the data to be (re)moved.
 *
 * returns the actual number of bytes that where moved.
 */
size_t
_aaxDataCopy(_data_t* buf, void* data, size_t offset, size_t size)
{
   size_t rv = size;

   assert(buf);
   assert(buf->id == DATA_ID);
   assert(offset+size < buf->avail);

   if (offset+size >= buf->avail) {
      rv = 0;
   }
   else if (size >= buf->blocksize)
   {
      size_t remain = buf->avail - offset;

      rv = _MIN((size/buf->blocksize)*buf->blocksize, remain);
      if (data) {
         memcpy(data, buf->data+offset, rv);
      }
   }

   return rv;
}

/**
 * (Re)Move data from the start of a previously created data structure.
 *
 * buf: the previously created data structure.
 * data: the buffer to move the data to. If NULL the data will just be erased.
 * size: size (in bytes) of the data to be (re)moved.
 *
 * returns the actual number of bytes that where moved.
 */
size_t
_aaxDataMove(_data_t* buf, void* data, size_t size)
{
   size_t rv = size;

   assert(buf);
   assert(buf->id == DATA_ID);

   if (size >= buf->blocksize)
   {
      rv = _MIN((size/buf->blocksize)*buf->blocksize, buf->avail);
      if (data) {
         memcpy(data, buf->data, rv);
      }

      buf->avail -= rv;
      if (buf->avail > 0) {
         memmove(buf->data, buf->data+rv, buf->avail);
      }
   }

   return rv;
}

/**
 * (Re)Move data from an offset of a previously created data structure.
 *
 * buf: the previously created data structure.
 * data: the buffer to move the data to. If NULL the data will just be erased.
 * size: size (in bytes) of the data to be (re)moved.
 * offset: offset in the data buffer.
 *
 * returns the actual number of bytes that where moved.
 */
size_t
_aaxDataMoveOffset(_data_t* buf, void* data, size_t offset, size_t size)
{
   size_t rv = size;

   assert(buf);
   assert(buf->id == DATA_ID);
   assert(offset+size < buf->avail);

   if (offset+size >= buf->avail) {
      rv = 0;
   }
   else if (size >= buf->blocksize)
   {
      size_t remain = buf->avail - offset;

      rv = _MIN((size/buf->blocksize)*buf->blocksize, remain);
      if (data) {
         memcpy(data, buf->data+offset, rv);
      }

      remain -= rv;
      buf->avail -= rv;
      if (buf->avail > 0) { 
         memmove(buf->data, buf->data+offset+rv, remain);
      }
   }

   return rv;
}

/**
 * Move data from one previously created data structure to another.
 *
 * dst: the previously created destinion data structure.
 * src: the previously created source data structure.
 * size: size (in bytes) of the data to be moved.
 *
 * returns the actual number of bytes that where moved.
 */

size_t
_aaxDataMoveData(_data_t* src, _data_t* dst, size_t size)
{
   size_t rv = size;

   assert(src);
   assert(src->id == DATA_ID);

   assert(dst);
   assert(dst->id == DATA_ID);

   if (size >= src->blocksize && size > dst->blocksize)
   {
      rv = _MIN((size/src->blocksize)*src->blocksize, src->avail);
      if (rv > (dst->size - dst->avail)) {
         rv = dst->size - dst->avail;
      }

      memcpy(dst->data+dst->avail, src->data, rv);

      src->avail -= rv;
      dst->avail += rv;
      if (src->avail > 0) {
         memmove(src->data, src->data+rv, src->avail);
      }
   }

   return rv;
}
