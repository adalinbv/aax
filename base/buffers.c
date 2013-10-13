/*
 * Copyright (C) 2005-2013 by Erik Hofman.
 * Copyright (C) 2007-2013 by Adalin B.V.
 *
 * This file is part of AeonWave-OpenAL.
 *
 *  AeonWave-OpenAL is free software: you can redistribute it and/or modify
 *  it under the terms of the Lesser GNU General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  AeonWave-OpenAL is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the Lesser GNU General Public License
 *  along with AeonWave-OpenAL.  If not, see <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#if HAVE_ASSERT_H
#include <assert.h>
#endif

#include "buffers.h"
#ifndef _AL_NOTHREADS
# include "threads.h"
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

# define _BUF_LOG(a, b, c)
#if 0
# define PRINT(...)		printf(...)
#else
#define PRINT(...)
#endif

static int __intBufFreeSpace(_intBuffers *, int, char);


#ifdef BUFFER_DEBUG
unsigned int
_intBufCreateDebug(_intBuffers **buffer, unsigned int id, char *file, int line)
{
    unsigned int r = _intBufCreateNormal(buffer, id);
    PRINT("create: %s at line %i: %x\n", file, line, r);
    return r;
}
#endif

unsigned int
_intBufCreateNormal(_intBuffers **buffer, unsigned int id)
{
    unsigned int rv = UINT_MAX;

    _BUF_LOG(LOG_INFO, id, __FUNCTION__);

    assert(buffer != 0);
    assert(*buffer == 0);
    assert(id > 0);

    *buffer = calloc(1, sizeof(_intBuffers));
    if (*buffer)
    {
        unsigned int num = BUFFER_RESERVE;

        (*buffer)->data = calloc(num, sizeof(_intBufferData*));
        if ((*buffer)->data)
        {
#ifndef _AL_NOTHREADS
            (*buffer)->mutex =
# ifndef NDEBUG
                _aaxMutexCreateDebug(0, _intBufNames[id], __FUNCTION__);
# else
                _aaxMutexCreate(0);
# endif
#endif
            (*buffer)->lock_ctr = 0;
            (*buffer)->start = 0;
            (*buffer)->first_free = 0;		/* relative to start */
            (*buffer)->num_allocated = 0;	/* relative to start */
            (*buffer)->max_allocations = num;	/* absolute          */
            (*buffer)->id = id;
            rv = 0;
        }
        else
        {
            free(*buffer);
            *buffer = 0;
        }
    }

    return rv;
}

int
_intBufDestroyDataNoLock(_intBufferData *ptr)
{
    int rv;

    assert(ptr);

    if ((rv = --ptr->reference_ctr) == 0)
    {
#ifndef _AL_NOTHREADS
        _aaxMutexDestroy(ptr->mutex);
#endif
        free(ptr);
    }

    if (!rv) rv = UINT_MAX;

    return rv;
}

unsigned int
_intBufAddDataNormal(_intBuffers *buffer, unsigned int id, const void *data, char locked)
{
    unsigned int rv = UINT_MAX;

    _BUF_LOG(LOG_INFO, id, __FUNCTION__);

    assert(data != 0);
    assert(buffer != 0);
    assert(buffer->id == id);
    assert(buffer->data != 0);
    assert(buffer->start <= buffer->max_allocations);
    assert(buffer->start+buffer->first_free <= buffer->max_allocations);
    assert(buffer->start+buffer->num_allocated <= buffer->max_allocations);

    if (__intBufFreeSpace(buffer, id, locked))
    {
        _intBufferData *b = malloc(sizeof(_intBufferData));
        if (b)
        {
            unsigned int pos;

#ifndef _AL_NOTHREADS
            b->mutex =
# ifndef NDEBUG
                _aaxMutexCreateDebug(0, _intBufNames[id], __FUNCTION__);
# else
                _aaxMutexCreate(0);
# endif
#endif
            b->reference_ctr = 1;
            b->ptr = data;

            if (!locked) {
               _intBufGetNum(buffer, id);
            }

            rv = buffer->first_free++;
            pos = buffer->start + rv;

            assert(buffer->data[pos] == NULL);

            buffer->data[pos] = b;
            buffer->num_allocated++;

            if (buffer->data[++pos] != 0)
            {
                unsigned int i, max = buffer->max_allocations - buffer->start;
                for(i=buffer->first_free; i<max; i++)
                {
                    if (buffer->data[buffer->start+i] == 0)
                        break;
                }
                buffer->first_free = i;
            }

            if (!locked) {
               _intBufReleaseNum(buffer, id);
            }
        }
    }

    return rv;
}

unsigned int
_intBufAddReference(_intBuffers *buffer, unsigned int id,
                        const _intBuffers *data, unsigned int n)
{
    unsigned int rv = UINT_MAX;

    _BUF_LOG(LOG_INFO, id, __FUNCTION__);

    assert(buffer != 0);
    assert(buffer->id == id);
    assert(buffer->data != 0);
    assert(buffer->start <= buffer->max_allocations);
    assert(buffer->start+buffer->first_free <= buffer->max_allocations);
    assert(buffer->start+buffer->num_allocated <= buffer->max_allocations);

    assert(data != 0);
    assert(data->id == id);
    assert(data->data != 0);

    n += data->start;

    assert(data->data[n] != 0);

    if (__intBufFreeSpace(buffer, id, 0))
    {
        _intBufferData *b = data->data[n];
        if (b)
        {
            unsigned int i, num;

#ifndef _AL_NOTHREADS
            _aaxMutexLock(b->mutex);
#endif
            b->reference_ctr++;
#ifndef _AL_NOTHREADS
            _aaxMutexUnLock(b->mutex);
#endif

            _intBufGetNum(buffer, id);

            buffer->num_allocated++;
            buffer->data[buffer->start+buffer->first_free] = b;
            rv = buffer->first_free;

            num = buffer->max_allocations - buffer->start;
            for(i=buffer->first_free; i<num; i++)
            {
                if (buffer->data[buffer->start+i] == 0)
                    break;
            }
            buffer->first_free = i;

            _intBufReleaseNum(buffer, id);
        }

    }

    return rv;
}

/* Replaces the buffer's data pointer */
/* needed for OpenAL */
const void *
_intBufReplace(_intBuffers *buffer, unsigned int id, unsigned int n, void *data)
{
    _intBufferData *buf;
    const void *rv = NULL;

    _BUF_LOG(LOG_INFO, id, __FUNCTION__);

    assert(data != 0);
    assert(buffer != 0);
    assert(buffer->id == id);
    assert(buffer->start+n < buffer->max_allocations);
    assert(buffer->data[buffer->start+n] != 0);
    assert(buffer->data[buffer->start+n]->ptr != 0);

    buf = _intBufGet(buffer, id, n);
    if (buf)
    {
        rv = buf->ptr;
        buf->ptr = data;
        _intBufReleaseData(buf, id);
    }

    return rv;
}

#if !defined(NDEBUG) || defined(PRINT_FUNC)
_intBufferData *
_intBufGetDebug(_intBuffers *buffer, unsigned int id, unsigned int n, char *file, int line)
{
    _BUF_LOG(LOG_BULK, id, __FUNCTION__);

    assert(buffer != 0);
    assert(buffer->id == id);

    n += buffer->start;

    assert(n < buffer->max_allocations);
    assert(buffer->data != 0);

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
_intBufGetNormal(_intBuffers *buffer, unsigned int id, unsigned int n, char locked)
{
    _BUF_LOG(LOG_BULK, id, __FUNCTION__);

    assert(buffer);
    assert(buffer->id == id);

    n += buffer->start;

#ifndef _AL_NOTHREADS
    if (!locked && buffer->data[n] != NULL) {
        _aaxMutexLock(buffer->data[n]->mutex);
    }
#endif

    assert(n < buffer->max_allocations);
    assert(buffer->data != 0);

    return buffer->data[n];
}

#ifndef _AL_NOTHREADS
void
_intBufRelease(_intBuffers *buffer, unsigned int id, unsigned int n)
{
    _BUF_LOG(LOG_BULK, id, __FUNCTION__);

    assert(buffer != 0);
    assert(buffer->id == id);

    n += buffer->start;

    assert(n < buffer->max_allocations);
    assert(buffer->data != 0);
    assert(buffer->data[n] != 0);
    assert(buffer->data[n]->ptr != 0);

    _aaxMutexUnLock(buffer->data[n]->mutex);
}
#endif

void *
_intBufGetDataPtr(const _intBufferData *data)
{
    assert(data != 0);

    return (void*)data->ptr;
}

void *
_intBufSetDataPtr(_intBufferData *data, void *user_data)
{
    void *ret = NULL;

    assert(data != 0);
    assert(user_data != 0);

    ret = (void*)data->ptr;
    data->ptr = user_data;
    
    return ret;
}

#ifndef _AL_NOTHREADS
# ifndef NDEBUG
void
_intBufReleaseDataDebug(const _intBufferData *data, unsigned int id, char *file, int line)
{
    _BUF_LOG(LOG_BULK, id, __FUNCTION__);

    assert(data != 0);

    _aaxMutexUnLockDebug(data->mutex, file, line);
}
# endif

void
_intBufReleaseDataNormal(const _intBufferData *data, unsigned int id)
{
    _BUF_LOG(LOG_BULK, id, __FUNCTION__);

    _aaxMutexUnLock(data->mutex);
}
#endif

unsigned int
_intBufGetNumNoLock(const _intBuffers *buffer, unsigned int id)
{
    _BUF_LOG(LOG_BULK, id, __FUNCTION__);

    assert(buffer != 0);
    assert(buffer->id == id);

    return buffer->num_allocated;
}

#ifndef NDEBUG
unsigned int
_intBufGetNumDebug(_intBuffers *buffer, unsigned int id, char *file, int line)
{
    _BUF_LOG(LOG_BULK, id, __FUNCTION__);

    assert(buffer != 0);
    assert(buffer->id == id);

#ifndef _AL_NOTHREADS
    _aaxMutexLockDebug(buffer->mutex, file, line);
# ifndef _AAX_PERSISTENT_GETNUM
    if (lock)
    {
        unsigned int i = 0;
        while((buffer->lock_ctr > 0) && (++i < 20000))
        {
            _aaxMutexUnLock(buffer->mutex);
            _aaxThreadSwitch();
            _aaxMutexLock(buffer->mutex);
        }
        if (i >= 20000) {
            PRINT("_intBufGetNumNormal timeout\n");
        }
    }
    else
    {
        buffer->lock_ctr++;
        _aaxMutexUnLock(buffer->mutex);
    }
# endif
#endif

    return buffer->num_allocated;

}
#endif

unsigned int
_intBufGetNumNormal(_intBuffers *buffer, unsigned int id, char lock)
{
    _BUF_LOG(LOG_BULK, id, __FUNCTION__);

#ifndef _AL_NOTHREADS
    _aaxMutexLock(buffer->mutex);
# ifndef _AAX_PERSISTENT_GETNUM
    if (lock)
    {
        unsigned int i = 0;
        while((buffer->lock_ctr > 0) && (++i < 20000))
        {
            _aaxMutexUnLock(buffer->mutex);
            _aaxThreadSwitch();
            _aaxMutexLock(buffer->mutex);
        }
        if (i >= 20000) {
            PRINT("_intBufGetNumNormal timeout\n");
        }
    }
    else
    {
        buffer->lock_ctr++;
        _aaxMutexUnLock(buffer->mutex);
    }
# endif
#endif

    return buffer->num_allocated;
}

unsigned int
_intBufGetMaxNumNoLock(const _intBuffers *buffer, unsigned int id)
{
    _BUF_LOG(LOG_BULK, id, __FUNCTION__);

    assert(buffer != 0);
    assert(buffer->id == id);

    return buffer->max_allocations;
}

unsigned int
_intBufGetMaxNumNormal(_intBuffers *buffer, unsigned int id, char lock)
{
    _BUF_LOG(LOG_BULK, id, __FUNCTION__);

    assert(buffer != 0);
    assert(buffer->id == id);

#ifndef _AL_NOTHREADS
    _aaxMutexLock(buffer->mutex);
# ifndef _AAX_PERSISTENT_GETNUM
    if (lock)
    {
        while(buffer->lock_ctr > 0)
        {
            _aaxMutexUnLock(buffer->mutex);
            _aaxThreadSwitch();
            _aaxMutexLock(buffer->mutex);
        }
    }
    else
    {
        buffer->lock_ctr++;
        _aaxMutexUnLock(buffer->mutex);
    }
# endif
#endif

    return buffer->max_allocations;
}

#ifndef _AL_NOTHREADS
void
_intBufReleaseNumNormal(_intBuffers *buffer, unsigned int id, char lock)
{
    _BUF_LOG(LOG_BULK, id, __FUNCTION__);

    assert(buffer);
    assert(buffer->id == id);

# ifndef _AAX_PERSISTENT_GETNUM
    if (!lock)
    {
        _aaxMutexLock(buffer->mutex);

        assert(buffer->lock_ctr > 0);
        buffer->lock_ctr--;
    }
# endif
    _aaxMutexUnLock(buffer->mutex);
}
#endif


/* needed for OpenAL */
unsigned int
_intBufGetPos(_intBuffers *buffer, unsigned int id, void *data)
{
    unsigned int i, start, num, max;

    _BUF_LOG(LOG_BULK, id, __FUNCTION__);

    assert(data != 0);
    assert(buffer != 0);
    assert(buffer->id == id);

    _intBufGetNum(buffer, id);

    start = buffer->start;
    num = buffer->num_allocated;
    max = buffer->max_allocations - start;
    for (i=0; i<max; i++)
    {
        if (buffer->data[start+i] && buffer->data[start+i]->ptr == data) break;
        if (--num == 0) break;
    }
    if (!num || (i == max)) {
        i = UINT_MAX;
    }

    _intBufReleaseNum(buffer, id);

    return i;
}

#ifdef BUFFER_DEBUG
_intBufferData *
_intBufPopDebug(_intBuffers *buffer, unsigned int id, char locked, char *file, int line)
{
    _intBufferData *rv;

    assert(buffer != 0);
    assert(buffer->id == id);

    if (!locked) {
        _intBufGetNum(buffer, id);
    }

    if (((buffer->num_allocated == 0) || (buffer->data[buffer->start] == 0))
        && (buffer->num_allocated || buffer->data[buffer->start]))
    {
        unsigned int i, start = buffer->start;
        printf("start: %i, num: %i, max: %i\n", start, buffer->num_allocated,
                                                 buffer->max_allocations);
        // if (buffer->data[start] == 0)
        {
            printf("buffer->data[%i] == 0 in file '%s' at line %i\n",
                    start, file, line);
            for(i=0; i<buffer->max_allocations; i++)
                printf("%x ", (unsigned int)buffer->data[i]);
            printf("\n");
          //return NULL;
        }
    }

    rv = _intBufPopNormal(buffer, id, 1);

    if (!locked) {
        _intBufReleaseNum(buffer, id);
    }

    return rv;
}
#endif

_intBufferData *
_intBufPopNormal(_intBuffers *buffer, unsigned int id, char locked)
{
    _intBufferData *rv = NULL;

    assert(buffer != 0);
    assert(buffer->id == id);

    if (!locked) {
        _intBufGetNum(buffer, id);
    }

    if (buffer->num_allocated > 0)
    {
        unsigned int start = buffer->start;

        rv = _intBufGet(buffer, id, 0);
        buffer->data[start] = NULL;
        _intBufReleaseData(rv, id);

        /*
         * Initially the starting pointer is increased, if the buffer is full
         * shift the remaining buffers from src to dst.
         * Always decrease the num_allocated counter accordingly.
         */
        if (--buffer->num_allocated > 0)
        {
            buffer->start++;
            if (buffer->first_free) {
                buffer->first_free--;
            }
        }
        else
        {
            buffer->first_free = 0;
            buffer->start = 0;
        }
    }

    if (!locked) {
        _intBufReleaseNum(buffer, id);
    }

    return rv;
}

void
_intBufPushNormal(_intBuffers *buffer, unsigned int id, const _intBufferData *data, char locked)
{
    assert(buffer != 0);
    assert(buffer->id == id);

    if (__intBufFreeSpace(buffer, id, locked))
    {
        unsigned int pos; 

        if (!locked) {
            _intBufGetNum(buffer, id);
        }

        pos = buffer->start + buffer->first_free++;

        assert(buffer->data[pos] == NULL);

        buffer->data[pos] = (_intBufferData *)data;
        buffer->num_allocated++;

        if (buffer->data[++pos] != 0)
        {
            unsigned int i, max = buffer->max_allocations - buffer->start;
            for(i=buffer->first_free; i<max; i++)
            {
                if (buffer->data[buffer->start+i] == 0)
                    break;
            }
            buffer->first_free = i;
        }

        if (!locked) {
            _intBufReleaseNum(buffer, id);
        }
    }
}

#ifdef BUFFER_DEBUG
void *
_intBufRemoveDebug(_intBuffers *buffer, unsigned int id, unsigned int n,
                        char locked, char num_locked, char *file, int lineno)
{
    void * r = _intBufRemoveNormal(buffer, id, n, locked, num_locked);
    PRINT("remove: %s at line %i: %x\n", file, lineno, n);
    return r;
}
#endif

void *
_intBufRemoveNormal(_intBuffers *buffer, unsigned int id, unsigned int n,
                                         char locked, char num_locked)
{
    _intBufferData *buf;
    void *rv = 0;

    _BUF_LOG(LOG_INFO, id, __FUNCTION__);

    assert(buffer != 0);
    assert(buffer->id == id);
    assert(buffer->start+n < buffer->max_allocations);
    assert(buffer->data != 0);

    if (num_locked) {
        _intBufReleaseNum(buffer, id);
    }
    _intBufGetNumNormal(buffer, id, 1);

    buf = _intBufGetNormal(buffer, id, n, locked);
    if (buf)
    {
        assert(buf->reference_ctr > 0);

        /*
         * If the counter doesn't equal to zero this buffer was referenced
         * by another buffer. So just detach it and let the last referer
         * take care of it.
         */
        if (--buf->reference_ctr == 0)
        {
#ifndef _AL_NOTHREADS
            _aaxMutexDestroy(buf->mutex);
#endif
            rv = (void*)buf->ptr;
            free(buf);
            buf = 0;
        }
        else {
            _intBufReleaseData(buf, id);
        }

        buffer->data[buffer->start+n] = NULL;
        buffer->num_allocated--;
        if (buffer->first_free > n) {
            buffer->first_free = n;
        }
    }

    _intBufReleaseNumNormal(buffer, id, 1);
    if (num_locked) {
        _intBufGetNum(buffer, id);
    }
    
    return rv;
}


#ifdef BUFFER_DEBUG
void 
_intBufClearDebug(_intBuffers *buffer, unsigned int id,
                      _intBufFreeCallback cb_free,
                      char *file, int lineno)
{
    _intBufClearNormal(buffer, id, cb_free);
    PRINT("clear: %s at line %i\n", file, lineno);
}
#endif

void
_intBufClearNormal(_intBuffers *buffer, unsigned int id,
                     _intBufFreeCallback cb_free)
{
    unsigned int n, max, start;

    _BUF_LOG(LOG_INFO, id, __FUNCTION__);

    assert(buffer != 0);
    assert(buffer->id == id);
    assert(buffer->data != 0);

    _intBufGetNum(buffer, id);

    start = buffer->start;
    max = buffer->max_allocations - start;
    for (n=0; n<max; n++)
    {
        void *rv = _intBufRemoveNormal(buffer, id, n, 0, 1);
        if (rv && cb_free) cb_free(rv);
    }
    buffer->start = 0;

    _intBufReleaseNum(buffer, id);
}


#ifdef BUFFER_DEBUG
void
_intBufEraseDebug(_intBuffers **buffer, unsigned int id,
                      _intBufFreeCallback cb_free,
                      char *file, int lineno)
{
    _intBufEraseNormal(buffer, id, cb_free);
    PRINT("erase: %s at line %i\n", file, lineno);
}
#endif

void
_intBufEraseNormal(_intBuffers **buf, unsigned int id,
                     _intBufFreeCallback cb_free)
{
    _BUF_LOG(LOG_INFO, id, __FUNCTION__);

    assert(buf != 0);

    if (*buf)
    {
        _intBuffers *buffer = *buf;
#if 1
        _intBufClear(buffer, id, cb_free);
#else
        unsigned int max;

        assert(buffer->id == id);

        _intBufGetNumNormal(buffer, id, 1);

        max = buffer->max_allocations - buffer->start;
        if (max)
        {
            do
            {
                _intBufferData *dptr = _intBufPopNormal(buffer, id, 1);
                if (dptr && dptr->ptr)
                {
                    dptr->reference_ctr--;
                    if (cb_free)
                    {
                        if (!dptr->reference_ctr) {
                            cb_free((void*)dptr->ptr);
                        }
                        _intBufDestroyDataNoLock(dptr);
                    }
                }
            }
            while (--max);
        }
#endif
        free(buffer->data);

#ifndef _AL_NOTHREADS
        _aaxMutexDestroy(buffer->mutex);
#endif
        free(buffer);
        *buf = 0;
    }
}

/* -------------------------------------------------------------------------- */

#define BUFFER_INCREMENT(a)	((((a)/BUFFER_RESERVE)+1)*BUFFER_RESERVE)

static int
__intBufFreeSpace(_intBuffers *buffer, int id, char locked)
{
    unsigned int start, num, max;
    int rv = 0;

    if (!locked) {
        _intBufGetNum(buffer, id);
    }
    
    start = buffer->start;
    num = buffer->num_allocated;
    max = buffer->max_allocations;

    if (((start+num) == max) || ((start+buffer->first_free) == max))
    {
        _intBufferData **ptr = buffer->data;

#ifndef _AAX_PERSISTENT_GETNUM
        _intBufReleaseNum(buffer, id);
        _intBufGetNumNormal(buffer, id, 1);
#endif

        if (start)
        {
            max -= start;

            memmove(ptr, ptr+start, max*sizeof(void*));
            memset(ptr+max, 0, start*sizeof(void*));
            buffer->start = 0;
            rv = -1;
        }
        else			/* increment buffer size */
        {
            max = BUFFER_INCREMENT(buffer->max_allocations);

            ptr = realloc(buffer->data, max*sizeof(_intBufferData*));
            if (ptr)
            {
                unsigned int size;

                buffer->data = ptr;

                ptr += buffer->max_allocations;
                size = max - buffer->max_allocations;
                memset(ptr, 0, size*sizeof(_intBufferData*));

                buffer->max_allocations = max;
                rv = -1;
            }
        }

#ifndef _AAX_PERSISTENT_GETNUM
        _intBufReleaseNumNormal(buffer, id, 1);
        _intBufGetNum(buffer, id);
#endif
    }
    else {
        rv = -1;
    }

#ifndef NDEBUG
    if (buffer->data[buffer->start+buffer->first_free] != 0)
    {
        unsigned int i;

        printf("add, buffer->first_free: %i\n", buffer->first_free);
        for(i=0; i<buffer->max_allocations; i++)
            printf("%x ", (unsigned int)buffer->data[i]);
        printf("\n");
    }

    assert(buffer->start+buffer->first_free < buffer->max_allocations);
    assert(buffer->data[buffer->start+buffer->first_free] == 0);
#endif

    if (!locked) {
        _intBufReleaseNum(buffer, id);
    }

    return rv;
}

