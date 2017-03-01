/*
 * Copyright 2017 by Erik Hofman.
 * Copyright 2017 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
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
      rv = (size/buf->blocksize)*buf->blocksize;
      if (data) {
         memcpy(data, buf->data, rv);
      }

      assert(buf->avail >= rv);
      buf->avail -= rv;

      memmove(buf->data, buf->data+rv, buf->avail);
   }

   return rv;
}

