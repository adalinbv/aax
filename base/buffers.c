/*
 * Copyright (C) 2005-2011 by Erik Hofman.
 * Copyright (C) 2007-2011 by Adalin B.V.
 *
 * This file is part of OpenAL-AeonWave.
 *
 *  OpenAL-AeonWave is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenAL-AeonWave is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OpenAL-AeonWave.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "buffers.h"

#if HAVE_ASSERT_H
#include <assert.h>
#endif

#ifndef _AL_NOTHREADS
# include "threads.h"
#endif

# define _BUF_LOG(a, b, c)
#if 0
# define PRINT(...)		printf(...)
#else
#define PRINT(...)
#endif

#ifdef NDEBUG
# include <stdlib.h>
# include <string.h>
#else
# include "logging.h"

# ifndef OPENAL_SUPPORT
const char *_intBufNames[] =
{
 "None",
 "Backend",
 "Device",
 "Buffer",
 "Emitter",
 "EmitterBuffer",
 "Sensor",
 "Frame",
 "Ringbuffer",
 "Extension",
 "Max"
};
# else
const char *_intBufNames[] =
{
 "None",
 "Backend",
 "Extension",
 "Enum",
 "Device",
 "Context",
 "State",
 "Buffer",
 "Source",
 "Listener",
 "SourceBuf",
 "Max"
};
# endif
#endif

#ifdef BUFFER_DEBUG
unsigned int
__intBufCreate(_intBuffers **buffer, unsigned int id,
                  char *file, int lineno)
{
   unsigned int r = int_intBufCreate(buffer, id);
   PRINT("create: %s at line %i: %x\n", file, lineno, r);
   return r;
}
#endif

unsigned int
int_intBufCreate(_intBuffers **buffer, unsigned int id)
{
   unsigned int retval = UINT_MAX;

   _BUF_LOG(LOG_INFO, id, "_intBufCreate");

   assert(buffer != 0);
   assert(*buffer == 0);
   assert(id > 0);

   *buffer = calloc(1, sizeof(_intBuffers));
   if (*buffer)
   {
      int num = 8;
      (*buffer)->data = calloc(num, sizeof(_intBufferData*));
      if ((*buffer)->data)
      {
#ifndef _AL_NOTHREADS
# ifndef NDEBUG
         (*buffer)->mutex = _aaxMutexCreateDebug(0, _intBufNames[id], __FUNCTION__);
# else
         (*buffer)->mutex = _aaxMutexCreate(0);
# endif
#endif
         (*buffer)->first_free = 0;
         (*buffer)->num_allocated = 0;
         (*buffer)->max_allocations = num;
         (*buffer)->id = id;
         retval = 0;
      }
      else
      {
         free(*buffer);
         *buffer = 0;
      }
   }

   return retval;
}

unsigned int
_intBufAddData(_intBuffers *buffer, unsigned int id, const void *data)
{
   _intBufferData **tmp, *b;
   unsigned int retval = UINT_MAX;
   unsigned int i, num;

   _BUF_LOG(LOG_INFO, id, "_intBufAddData");

   assert(buffer != 0);
   assert(buffer->id == id);
   assert(buffer->num_allocated <= buffer->max_allocations);
   assert(buffer->first_free <= buffer->max_allocations);
   assert(buffer->data != 0);
   assert(data != 0);

   tmp = buffer->data;
   num = buffer->num_allocated;
   if (num == buffer->max_allocations)
   {
      num = ((num >> 3) + 1) << 3;

      tmp = realloc(buffer->data, num * sizeof(_intBufferData*));
      if (tmp)
      {
         unsigned int size;

         buffer->data = tmp;
         
         tmp += buffer->num_allocated;
         size = num - buffer->num_allocated;
         memset(tmp, 0, size*sizeof(_intBufferData*));

         buffer->max_allocations = num;
      }
   }

   for(i=buffer->first_free; i<num; i++)
   {
      if (buffer->data[i] == 0)
         break;
   }
   buffer->first_free = i;

   if (tmp)
   {
      b = malloc(sizeof(_intBufferData));
      if (b)
      {
#ifndef _AL_NOTHREADS
# ifndef NDEBUG
         b->mutex = _aaxMutexCreateDebug(0, _intBufNames[id], __FUNCTION__);
# else
         b->mutex = _aaxMutexCreate(0);
# endif
#endif
         b->ptr = data;
         b->reference_ctr = 1;

         assert(buffer->data[buffer->first_free] == 0);

         buffer->num_allocated++;
         buffer->data[buffer->first_free] = b;
         retval = buffer->first_free;
      }
   }

   return retval;
}

unsigned int
_intBufAddReference(_intBuffers *buffer, unsigned int id,
                  const _intBuffers *data, unsigned int n)
{
   _intBufferData **tmp, *b;
   unsigned int retval = UINT_MAX;
   unsigned int i, num;

   _BUF_LOG(LOG_INFO, id, "_intBufAddReference");

   assert(buffer != 0);
   assert(buffer->id == id);
   assert(buffer->num_allocated <= buffer->max_allocations);
   assert(buffer->first_free <= buffer->max_allocations);
   assert(buffer->data != 0);

   assert(data != 0);
   assert(data->id == id);
   assert(n < data->max_allocations);
   assert(data->data[n] != 0);

   tmp = buffer->data;
   num = buffer->num_allocated;
   if (num == buffer->max_allocations)
   {
      num = ((num >> 3) + 1) << 3;

      tmp = realloc(buffer->data, num * sizeof(_intBufferData*));
      if (tmp)
      {
         unsigned int size;

         buffer->data = tmp;

         tmp += buffer->num_allocated;
         size = num - buffer->num_allocated;
         memset(tmp, 0, size*sizeof(_intBufferData*));

         buffer->max_allocations = num;
      }
   }

   for(i=buffer->first_free; i<num; i++)
   {
      if (buffer->data[i] == 0)
         break;
   }
   buffer->first_free = i;

   if (tmp)
   {
      b = data->data[n];
      if (b)
      {
         b->reference_ctr++;

         assert(buffer->data[buffer->first_free] == 0);

         buffer->num_allocated++;
         buffer->data[buffer->first_free] = b;
         retval = buffer->first_free;
      }
   }

   return retval;
}

const void *
_intBufReplace(_intBuffers *buffer, unsigned int id, unsigned int n,
                                                           void *data)
{
   const void *olddata;
   _BUF_LOG(LOG_INFO, id, "_intBufReplace");

   assert(buffer != 0);
   assert(n < buffer->max_allocations);
   assert(buffer->data[n] != 0);
   assert(buffer->data[n]->ptr != 0);
   assert(data != 0);

#ifndef _AL_NOTHREADS
   _aaxMutexLock(buffer->mutex);
   _aaxMutexLock(buffer->data[n]->mutex);
#endif

   olddata = buffer->data[n]->ptr;
   buffer->data[n]->ptr = data;

#ifndef _AL_NOTHREADS
   _aaxMutexUnLock(buffer->data[n]->mutex);
   _aaxMutexUnLock(buffer->mutex);
#endif

   return olddata;
}

_intBufferData *
_intBufGetNormal(const _intBuffers *buffer, unsigned int id, unsigned int n)
{
   assert(buffer->id == id);

#ifndef _AL_NOTHREADS
   if (buffer->data[n] != 0) {
      _aaxMutexLock(buffer->data[n]->mutex);
   }
#endif

   return buffer->data[n];
}

#ifndef NDEBUG
_intBufferData *
_intBufGetDebug(const _intBuffers *buffer, unsigned int id, unsigned int n, char *file, int line)
{
   _BUF_LOG(LOG_BULK, id, "_intBufGet");

   assert(buffer != 0);
   assert(buffer->id == id);
   assert(n < buffer->max_allocations);
   assert(buffer->data != 0);
// assert(buffer->data[n] != 0);

#ifndef _AL_NOTHREADS
   if (buffer->data[n] != 0)
   {
      int r = _aaxMutexLockDebug(buffer->data[n]->mutex, file, line);
      if (r < 0) PRINT("error: %i at %s line %i\n", -r, file, line);
   }
#endif

   return buffer->data[n];
}
#endif

_intBufferData *
_intBufGetNoLock(const _intBuffers *buffer, unsigned int id, unsigned int n)
{
   _BUF_LOG(LOG_BULK, id, "_intBufGetNoLock");

   assert(buffer != 0);
   assert(buffer->id == id);
   assert(n < buffer->max_allocations);
   assert(buffer->data != 0);
// assert(buffer->data[n] != 0);

   return buffer->data[n];
}

#ifndef _AL_NOTHREADS
void
_intBufRelease(const _intBuffers *buffer, unsigned int id, unsigned int n)
{
   _BUF_LOG(LOG_BULK, id, "_intBufRelease");

   assert(buffer != 0);
   assert(buffer->id == id);
   assert(n < buffer->max_allocations);
   assert(buffer->data != 0);
   assert(buffer->data[n] != 0);
   assert(buffer->data[n]->ptr != 0);

   _aaxMutexUnLock(buffer->data[n]->mutex);
}
#else
# define _intBufRelease(a, b, c)
#endif

void *
_intBufGetDataPtr(const _intBufferData *data)
{
   void *ret = NULL;
   if (data) ret = (void*)data->ptr;
   return ret;
}

void *
_intBufSetDataPtr(_intBufferData *data, void *user_data)
{
   void *ret = NULL;

   assert(user_data != 0);

   if (data)
   {
      ret = (void*)data->ptr;
      data->ptr = user_data;
   }
   return ret;
}

#ifndef _AL_NOTHREADS
# ifndef NDEBUG
void
_intBufReleaseDataDebug(const _intBufferData *data, unsigned int id, char *file, int line)
{
   _BUF_LOG(LOG_BULK, id, "_intBufReleaseData");

   assert(data != 0);

   _aaxMutexUnLockDebug(data->mutex, file, line);
}
# else
void
_intBufReleaseData(const _intBufferData *data, unsigned int id)
{
   _BUF_LOG(LOG_BULK, id, "_intBufReleaseData");

   assert(data != 0);

   _aaxMutexUnLock(data->mutex);
}
# endif
#else
# define _intBufReleaseData(a, b)
#endif

unsigned int
_intBufGetNum(const _intBuffers *buffer, unsigned int id)
{
   _BUF_LOG(LOG_BULK, id, "_intBufGetNum");

   assert(buffer != 0);
   if (!buffer) return 0;
   assert(buffer->id == id);

#ifndef _AL_NOTHREADS
   _aaxMutexLock(buffer->mutex);
#endif

   return buffer->num_allocated;
}

unsigned int
_intBufGetMaxNum(const _intBuffers *buffer, unsigned int id)
{
   _BUF_LOG(LOG_BULK, id, "_intBufGetNum");

   assert(buffer != 0);
   if (!buffer) return 0;
   assert(buffer->id == id);

#ifndef _AL_NOTHREADS
   _aaxMutexLock(buffer->mutex);
#endif

   return buffer->max_allocations;
}

unsigned int
_intBufGetNumNoLock(const _intBuffers *buffer, unsigned int id)
{
   _BUF_LOG(LOG_BULK, id, "_intBufGetNumNoLock");

   if (!buffer) return 0;

   assert(buffer->id == id);

   return buffer->num_allocated;
}

unsigned int
_intBufGetMaxNumNoLock(const _intBuffers *buffer, unsigned int id)
{
   _BUF_LOG(LOG_BULK, id, "_intBufGetNum");

   assert(buffer != 0);
   if (!buffer) return 0;
   assert(buffer->id == id);

   return buffer->max_allocations;
}

unsigned int
_intBufGetPosNoLock(const _intBuffers *buffer, unsigned int id, void *data)
{
   unsigned int i = UINT_MAX;

   _BUF_LOG(LOG_BULK, id, "_intBufGetPosNoLock");

   if (buffer && data)
   {
      assert(buffer->id == id);

      for (i=0; i<buffer->max_allocations; i++)
         if (buffer->data[i] && buffer->data[i]->ptr == data)
            break;

      if (i == buffer->max_allocations) i = UINT_MAX;
   }

   return i;
}

#ifndef _AL_NOTHREADS
void 
_intBufReleaseNum(const _intBuffers *buffer, unsigned int id)
{
   _BUF_LOG(LOG_BULK, id, "_intBufReleaseNum");

   assert(buffer);
   assert(buffer->id == id);

   _aaxMutexUnLock(buffer->mutex);
}
#else
# define _intBufReleaseNum(a, b)
#endif

#ifdef BUFFER_DEBUG
_intBufferData *
__intBufPopData(_intBuffers *buffer, unsigned int id, char *file, int line)
{
   _intBufferData *retval = NULL;

   assert(buffer != 0);
   assert(buffer->id == id);

   if (buffer->num_allocated)
   {
      unsigned int i;

      if (buffer->data[0] == 0)
      {
         buffer->num_allocated = 0;
         printf("buffer->data[0] == 0 in file '%s' at line %i\n", file, line);
         for(i=0; i<buffer->max_allocations; i++)
            printf("%x ", buffer->data[i]);
         printf("\n");
         return NULL;
      }
      assert(buffer->data[0] != 0);
      assert(buffer->data[0]->reference_ctr > 0);

      _aaxMutexLock(buffer->data[0]->mutex);
      retval = buffer->data[0];
      buffer->data[0] = NULL;
      _aaxMutexUnLock(retval->mutex);

      if (--buffer->num_allocated > 0)
      {
         unsigned int i, max = buffer->max_allocations - 1;

         /*
          * Shift the remaining buffers from src to dst and decrease the
          * num_allocated counter accordingly.
          */
#if 1
         memmove(buffer->data, buffer->data+1, max*sizeof(void*));
#else
         for (i=0; i<max; i++) {
            buffer->data[i] = buffer->data[i+1];
         }
#endif
         buffer->data[max] = NULL;
      }

      if (buffer->first_free > 0) {
         buffer->first_free--;
      }
   }

   return retval;
}

#endif

_intBufferData *
int_intBufPopData(_intBuffers *buffer, unsigned int id)
{
   _intBufferData *retval = NULL;

   assert(buffer != 0);
   assert(buffer->id == id);

   if (buffer->num_allocated > 0)
   {
      if (buffer->data[0] == 0)
      {
         buffer->num_allocated = 0;
         return NULL;
      }

      assert(buffer->data[0] != 0);
      assert(buffer->data[0]->reference_ctr > 0);

      _aaxMutexLock(buffer->data[0]->mutex);
      retval = buffer->data[0];
      buffer->data[0] = NULL;
      _aaxMutexUnLock(retval->mutex);

      if (--buffer->num_allocated > 0)
      {
         unsigned int i, max = buffer->max_allocations - 1;

         /*
          * Shift the remaining buffers from src to dst and decrease the
          * num_allocated counter accordingly.
          */
#if 1
         memmove(buffer->data, buffer->data+1, max*sizeof(void*));
#else
         for (i=0; i<max; i++) {
            buffer->data[i] = buffer->data[+1];
         }
#endif
         buffer->data[max] = NULL;
      }

      if (buffer->first_free > 0) {
         buffer->first_free--;
      }
   }

   return retval;
}

void
_intBufPushData(_intBuffers *buffer, unsigned int id, const _intBufferData *data)
{
   _intBufferData **tmp;
   unsigned int num;

   assert(buffer != 0);
   assert(buffer->id == id);

   num = buffer->num_allocated;
   if (num == buffer->max_allocations)
   {
      num = ((num >> 3) + 1) << 3;

      tmp = realloc(buffer->data, num * sizeof(_intBufferData*));
      if (tmp)
      {
         unsigned int size;

         buffer->data = tmp;

         tmp += buffer->num_allocated;
         size = num - buffer->num_allocated;
         memset(tmp, 0, size * sizeof(_intBufferData*));

         buffer->max_allocations = num;
      }
   }

   buffer->data[buffer->first_free++] = (_intBufferData *)data;
   buffer->num_allocated++;
}

void **
_intBufShiftIndex(_intBuffers *buffer, unsigned int id, unsigned int dst,
                                                        unsigned int src)
{
   void **retval = NULL;
   unsigned int i, num;

   _BUF_LOG(LOG_INFO, id, "_intBufShiftIndex");

   assert(buffer != 0);
   assert(buffer->id == id);
   assert(dst < buffer->max_allocations);
   assert(src < buffer->max_allocations);
   assert(buffer->data);
   assert(dst < src);

   num = (src-dst);
   if (buffer->num_allocated >= num) 
   {
      retval = (void **)calloc(num, sizeof(void *));
      if (retval)
      {
         for (i=dst; i<src; i++)
         {
            _intBufferData *tmp;

            assert(buffer->data[i] != 0);
            assert(buffer->data[i]->reference_ctr > 0);

#ifndef _AL_NOTHREADS
            _aaxMutexLock(buffer->data[i]->mutex);
#endif
            tmp = buffer->data[i];
            buffer->data[i] = NULL;
#ifndef _AL_NOTHREADS
            _aaxMutexUnLock(tmp->mutex);
#endif
            retval[i] = (void*)tmp->ptr;

            /*
             * If the counter doesn't equal to zero this buffer was referenced
             * by another buffer. So just detach it and let the last referer
             * take care of it.
             */
            tmp->reference_ctr--;
            if (tmp->reference_ctr == 0)
            {
#ifndef _AL_NOTHREADS
               _aaxMutexDestroy(tmp->mutex);
#endif
               free(tmp);
               tmp = 0;
            }
         }

         buffer->num_allocated -= num;
         if (buffer->num_allocated)
         {
            unsigned int max = buffer->max_allocations;

            /*
             * Shift the remaining buffers from src to dst and decrease the
             * num_allocated counter accordingly.
             */
#if 1
            memmove(buffer->data+dst, buffer->data+src,(max-src)*sizeof(void*));
#else
            for (i=dst; i<src; i++) {
               buffer->data[i] = buffer->data[i+1];
            }
#endif
            memset(buffer->data+(max-num), '\0', num*sizeof(void*));

            if (buffer->first_free >= src) {
               buffer->first_free -= num;
            }
            else
            {
               for(i=0; i<max; i++)
               {
                  if (buffer->data[i] == 0)
                     break;
               }
               buffer->first_free = i;
            }
         }
         else
         {
            memset(buffer->data+dst, '\0', num*sizeof(void*));
            buffer->first_free = 0;
         }
      }
   }

   return retval;
}


#ifdef BUFFER_DEBUG
void *
__intBufRemove(_intBuffers *buffer, unsigned int id, unsigned int n,
                  char locked, char *file, int lineno)
{
   void * r = int_intBufRemove(buffer, id, n, locked);
   PRINT("remove: %s at line %i: %x\n", file, lineno, n);
   return r;
}
#endif

void *
int_intBufRemove(_intBuffers *buffer, unsigned int id, unsigned int n,
                                         char locked)
{
   _intBufferData *tmp;
   void *retval = 0;

   _BUF_LOG(LOG_INFO, id, "_intBufRemove");

   assert(buffer != 0);
   assert(buffer->id == id);
   assert(n < buffer->max_allocations);
   assert(buffer->data != 0);
// assert(buffer->data[n] != 0);

   if (buffer->data[n] != 0)
   {
      assert(buffer->data[n]->reference_ctr > 0);

      tmp = buffer->data[n];

      /*
       * If the counter doesn't equal to zero this buffer was referenced
       * by another buffer. So just detach it and let the last referer
       * take care of it.
       */
      tmp->reference_ctr--;
      if (tmp->reference_ctr == 0)
      {
         buffer->data[n] = NULL;
#ifndef _AL_NOTHREADS
         _aaxMutexDestroy(tmp->mutex);
#endif
         retval = (void*)tmp->ptr;
         free(tmp);
         tmp = 0;

#ifndef _AL_NOTHREADS
         _aaxMutexLock(buffer->mutex);
#endif 
         buffer->num_allocated--;
         if (n < buffer->first_free) {
            buffer->first_free = n;
         }
#ifndef _AL_NOTHREADS
         _aaxMutexUnLock(buffer->mutex);
#endif
      }
#ifndef _AL_NOTHREADS
      else if (locked) {
         _aaxMutexUnLock(tmp->mutex);
      }
#endif
   }
   
   return retval;
}


#ifdef BUFFER_DEBUG
void 
__intBufClear(_intBuffers *buffer, unsigned int id,
                 _intBufFreeCallback cb_free, void *data,
                 char *file, int lineno)
{
   int_intBufClear(buffer, id, cb_free, data);
   PRINT("clear: %s at line %i\n", file, lineno);
}
#endif

void
int_intBufClear(_intBuffers *buffer, unsigned int id,
                _intBufFreeCallback cb_free, void *data)
{
   _BUF_LOG(LOG_INFO, id, "_intBufClear");

   if (buffer == 0) return;
   assert(buffer->id == id);

   if (buffer->data)
   {
      unsigned int n, num;

      num = buffer->max_allocations;
      if (cb_free != NULL)
      {
         for (n=0; n<num; n++)
            cb_free(data, n);
      }
      else
      {
         for(n=0; n<num; n++)
         {
            _intBufferData *tmp;

            tmp = buffer->data[n];
            if (tmp != 0)
            {
#ifndef _AL_NOTHREADS
               _aaxMutexLock(tmp->mutex);
#endif
               _intBufRemove(buffer, id, n, 1);
            }
         }

      }
   }
}


#ifdef BUFFER_DEBUG
void
_intBufEraseDebug(_intBuffers **buffer, unsigned int id,
                 _intBufFreeCallback cb_free, void *data,
                 char *file, int lineno)
{
   _intBufEraseNormal(buffer, id, cb_free, data);
   PRINT("erase: %s at line %i\n", file, lineno);
}
#endif

void
_intBufEraseNormal(_intBuffers **buf, unsigned int id,
                _intBufFreeCallback cb_free, void *data)
{
   _intBuffers *buffer;

   _BUF_LOG(LOG_INFO, id, "_intBufErase");

   if (buf == 0 || *buf == 0) return;

   buffer = *buf;
   assert(buffer->id == id);

   if (buffer->data)
   {
      _intBufClear(buffer, id, cb_free, data);
      free(buffer->data);
   }
#ifndef _AL_NOTHREADS
   _aaxMutexDestroy(buffer->mutex);
#endif
   free(buffer);
   *buf = 0;
}

#ifndef _AL_NOTHREADS
void
_intBufLock(_intBuffers *buffer, unsigned int id)
{
   assert(buffer != 0);
   assert(buffer->id == id);

   _aaxMutexLock(buffer->mutex);
}
#else
# define _intBufLock(a, b)
#endif

#ifndef _AL_NOTHREADS
void
_intBufUnlock(_intBuffers *buffer, unsigned int id)
{
   assert(buffer != 0);
   assert(buffer->id == id);

   _aaxMutexUnLock(buffer->mutex);
}
#else
# define _intBufUnlock(a, b)
#endif

#ifndef _AL_NOTHREADS
void
_intBufLockData(_intBuffers *buffer, unsigned int id, unsigned int n)
{
   assert(buffer != 0);
   assert(buffer->id == id);
   assert(n < buffer->max_allocations);
   assert(buffer->data[n] != 0);

   _aaxMutexLock(buffer->data[n]->mutex);
}
#else
# define _intBufLockData(a, b, c)
#endif

