/*
 * SPDX-FileCopyrightText: Copyright © 2017-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2017-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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

#include "api.h"
#include "arch.h"
#include "memory.h"
#include "databuffer.h"

#define DATA_ID	0xDFA82736

static unsigned char**
_aaxDataAlloc(unsigned char no_buffers, size_t buffersize)
{
   unsigned char** rv;
   char *ptr;

   buffersize = SIZE_ALIGNED(buffersize);
   rv = (unsigned char**)_aax_malloc(&ptr, no_buffers*sizeof(unsigned char*),
                                           no_buffers*buffersize);
   if (rv)
   {
      int t;
      for (t=0; t<no_buffers; ++t)
      {
         rv[t] = (unsigned char*)ptr;
         ptr += buffersize;
      }
   }

   return rv;
}

_data_t*
_aaxDataCreate(unsigned char no_buffers, size_t size, unsigned int blocksize)
{
   _data_t* rv = malloc(sizeof(_data_t));
   if (rv)
   {
      if (!blocksize) ++blocksize;
      size *= blocksize;

      rv->data = _aaxDataAlloc(no_buffers, size);
      if (rv->data)
      {
         rv->offset = calloc(no_buffers, sizeof(size_t));
         if (rv->offset)
         {
            rv->id = DATA_ID;
            rv->size = size;
            rv->no_buffers = no_buffers;
            rv->blocksize = blocksize;
         }
         else
         {
            free(rv);
            rv = NULL;
         }
      }
      else
      {
         free(rv);
         rv = NULL;
      }
   }
   return rv;
}

void
_aaxDataClear(_data_t* buf, unsigned char buffer_no)
{
   if (buffer_no < buf->no_buffers) {
      buf->offset[buffer_no] = 0;
   } else if (buffer_no == (unsigned char)-1) {
      memset(buf->offset, 0, buf->no_buffers*sizeof(size_t));
   }
}

int
_aaxDataDestroy(_data_t* buf)
{
   if (buf)
   {
      assert(buf->id == DATA_ID);

      buf->id = FADEDBAD;

      free(buf->offset);
      free(buf->data);
      free(buf);
   }

   return true;
}

size_t
_aaxDataAdd(_data_t* buf, unsigned char buffer_no, const void* data, size_t size)
{
   size_t free, rv = 0;

   assert(buf);
   assert(buf->id == DATA_ID);
   assert(buf->no_buffers > buffer_no);
   assert(data);

   if (buffer_no < buf->no_buffers)
   {
      free = buf->size - buf->offset[buffer_no];
      if (size > free) rv = free;
      else rv = size;

      if (rv)
      {
         memcpy(buf->data[buffer_no]+buf->offset[buffer_no], data, rv);
         buf->offset[buffer_no] += rv;
      }
   }

   return rv;
}

size_t
_aaxDataCopy(_data_t* buf, unsigned char buffer_no, void* data, size_t offset, size_t size)
{
   size_t rv = 0;

   assert(buf);
   assert(buf->id == DATA_ID);
   assert(buf->no_buffers > buffer_no);
   assert(data);

   if (data && buffer_no < buf->no_buffers && size >= buf->blocksize &&
       (offset+size) <= buf->offset[buffer_no])
   {
      size_t remain = buf->offset[buffer_no] - offset;

      rv = _MIN((size/buf->blocksize)*buf->blocksize, remain);
      if (rv) {
         memcpy(data, buf->data[buffer_no]+offset, rv);
      }
   }

   return rv;
}

size_t
_aaxDataMove(_data_t* buf, unsigned char buffer_no, void* data, size_t size)
{
   size_t rv = 0;

   assert(buf);
   assert(buf->id == DATA_ID);
   assert(buf->no_buffers > buffer_no);

   if (buffer_no < buf->no_buffers && size >= buf->blocksize)
   {
      size_t remain = buf->offset[buffer_no];

      rv = _MIN((size/buf->blocksize)*buf->blocksize, remain);
      if (rv)
      {
         if (data) {
            memcpy(data, buf->data[buffer_no], rv);
         }

         buf->offset[buffer_no] -= rv;
         if (buf->offset[buffer_no] > 0) {
            memmove(buf->data[buffer_no], buf->data[buffer_no]+rv, buf->offset[buffer_no]);
         }
      }
   }

   return rv;
}

size_t
_aaxDataMoveOffset(_data_t* buf, unsigned char buffer_no, void* data, size_t offset, size_t size)
{
   size_t rv = 0;

   assert(buf);
   assert(buf->id == DATA_ID);
   assert(buf->no_buffers > buffer_no);

   if (buffer_no < buf->no_buffers && size >= buf->blocksize &&
       (offset+size) <= buf->offset[buffer_no])
   {
      ssize_t remain = buf->offset[buffer_no] - offset;

      rv = _MIN((size/buf->blocksize)*buf->blocksize, remain);
      if (rv)
      {
         if (data) {
            memcpy(data, buf->data[buffer_no]+offset, rv);
         }

         remain -= rv;
         buf->offset[buffer_no] -= rv;
         if (buf->offset[buffer_no] > 0 && remain > 0) {
            memmove(buf->data[buffer_no]+offset, buf->data[buffer_no]+offset+rv, remain);
         }
      }
   }

   return rv;
}

size_t
_aaxDataMoveData(_data_t* src, unsigned char src_no, _data_t* dst, unsigned char dst_no, size_t size)
{
   size_t rv = 0;

   assert(src);
   assert(src->id == DATA_ID);
   assert(src->no_buffers > src_no);

   assert(dst);
   assert(dst->id == DATA_ID);
   assert(dst->no_buffers > dst_no);

   if (src->no_buffers > src_no && dst->no_buffers > dst_no &&
       src->blocksize < size    && dst->blocksize < size)
   {
      rv = _MIN((size/src->blocksize)*src->blocksize, src->offset[src_no]);
      if (rv > (dst->size - dst->offset[dst_no])) {
         rv = dst->size - dst->offset[dst_no];
      }

      memcpy(dst->data[dst_no]+dst->offset[dst_no], src->data[src_no], rv);

      src->offset[src_no] -= rv;
      dst->offset[dst_no] += rv;
      if (src->offset[src_no] > 0) {
         memmove(src->data[src_no], src->data[src_no]+rv, src->offset[src_no]);
      }
   }

   return rv;
}

void*
_aaxDataGetData(_data_t *buf, unsigned char buffer_no)
{
   void *rv = NULL;

   assert(buf->no_buffers > buffer_no);

   if (buf->no_buffers > buffer_no) {
      rv = buf->data[buffer_no];
   }

   return rv;
}

void*
_aaxDataGetPtr(_data_t *buf, unsigned char buffer_no)
{
   void *rv = NULL;

   assert(buf->no_buffers > buffer_no);

   if (buf->no_buffers > buffer_no) {
      rv = buf->data[buffer_no] + buf->offset[buffer_no];
   }

   return rv;
}

size_t
_aaxDataGetSize(_data_t *buf)
{
   return buf->size;
}

ssize_t
_aaxDataSetOffset(_data_t *buf, unsigned char buffer_no, size_t offs)
{
   ssize_t rv = 0;

   assert(buf->no_buffers > buffer_no);

   if (buf->no_buffers > buffer_no)
   {
      if (buf->offset[buffer_no] + offs <= buf->size) {
         buf->offset[buffer_no] = offs;
      }
      else
      {
         rv = buf->size - (buf->offset[buffer_no] + offs);
         buf->offset[buffer_no] = buf->size;
      }
   }

   return rv;
}

ssize_t
_aaxDataIncreaseOffset(_data_t *buf, unsigned char buffer_no, size_t offs)
{
   ssize_t rv = 0;

   assert(buf->no_buffers > buffer_no);

   if (buf->no_buffers > buffer_no)
   {
      if (buf->offset[buffer_no] + offs <= buf->size) {
         buf->offset[buffer_no] += offs;
      }
      else
      {
         rv = buf->size - (buf->offset[buffer_no] + offs);
         buf->offset[buffer_no] = buf->size;
      }
   }

   return rv;
}

size_t
_aaxDataGetOffset(_data_t *buf, unsigned char buffer_no)
{
   size_t rv = 0;

   assert(buf->no_buffers > buffer_no);

   if (buf->no_buffers > buffer_no) {
      rv = buf->offset[buffer_no];
   }

   return rv;
}

size_t
_aaxDataGetDataAvail(_data_t *buf, unsigned char buffer_no)
{
   size_t rv = 0;

   assert(buf->no_buffers > buffer_no);

   if (buf->no_buffers > buffer_no) {
      rv = buf->offset[buffer_no];
   }

   return rv;
}

size_t
_aaxDataGetFreeSpace(_data_t *buf, unsigned char buffer_no)
{
   size_t rv = 0;

   assert(buf->no_buffers > buffer_no);

   if (buf->no_buffers > buffer_no) {
      rv = buf->size - buf->offset[buffer_no];
   }

   return rv;
}

