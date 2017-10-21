/*
 * Copyright 2017 by Erik Hofman.
 * Copyright 2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif
#include <assert.h>

#include "api.h"
#include "arch.h"

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

