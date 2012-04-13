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

#ifndef _AL_BUFFERS_H
#define _AL_BUFFERS_H 1

#if HAVE_CONFIG_H
#include <config.h>
#endif


#include <limits.h>		/* for UINT_MAX */

typedef struct
{
   void *mutex;

   unsigned int reference_ctr;
   const void *ptr;

} _intBufferData;

typedef struct
{
   void *mutex;
   unsigned int id;

   unsigned int first_free;
   unsigned int num_allocated;
   unsigned int max_allocations;

   _intBufferData **data;

} _intBuffers;

typedef void _intBufFreeCallback(void *, unsigned int);


#ifndef NDEBUG
# define BUFFER_DEBUG 		1
#endif

#define _intBufPosToId(a)	((a) == UINT_MAX) ? 0 : (((a) + 1) << 4)
#define _intBufIdToPos(a)	(((a) == 0) || ((a) & 0xF))  ? UINT_MAX : (((a) >> 4) - 1)
	

/**
 * Setup the structure for an internal buffer array
 *
 * Initially 'num' number of object pointers will be allocated.
 *
 * If 'size' is greater than zero enough memory will be allocated for 'num'
 * new objects which will get linked from the array. These objects will be
 * empty though and should be filled by calling _intBufReplace instead of
 * _intBufAddData.
 *
 * This function needs to be called just once. The object array pointer list
 * will be extended automatically by adding new objects to the array.
 *
 * @param buffer pointer to the root of the internal buffer structure
 * @param id the id of the buffer this array should represent
 * @return the position of the new object in the array upon success,
           UINT_MAX otherwise.
 */
#ifdef BUFFER_DEBUG
# define _intBufCreate(a, b) __intBufCreate(a, b, __FILE__, __LINE__)

unsigned int
__intBufCreate(_intBuffers **, unsigned int, char *, int);
#else
# define _intBufCreate(a, b) int_intBufCreate(a, b);
#endif

unsigned int
int_intBufCreate(_intBuffers **, unsigned int);

/**
 * Add a new object to the buffer list.
 *
 * The referer has to make sure that buffer->mutex is locked before
 * calling this function.
 *
 * @param buffer the buffer to add the data to
 * @param id the id of the buffer this array should represent
 * @param data pointer to the object to add
 * @return the position in the array upon success, UINT_MAX otherwise.
 */
unsigned int
_intBufAddData(_intBuffers *, unsigned int, const void *);

/**
 * Refer a buffer from another buffer list. No data is copied.
 *
 * The referer has to make sure that buffer->mutex and data->mutex are locked
 * prior to calling this function.
 *
 * @param buffer the buffer to add the reference to
 * @param id the id of the buffer this array should represent
 * @param data pointer to the buffer array to reference
 * @param pos the position in 'buffer' to refer to
 * @return the position in the array upon success, UINT_MAX otherwise.
 */
unsigned int
_intBufAddReference(_intBuffers *, unsigned int, const _intBuffers *,
                                                  unsigned int);

/**
 * Replace the content of one buffer position with a new object.
 * Free the old object from memory.
 *
 * @param buffer the buffer to add the data to
 * @param id the id of the buffer this array should represent
 * @param pos the position in the array of the object to replace
 * @param data pointer to the new object data
 */
const void *
_intBufReplace(_intBuffers *, unsigned int, unsigned int, void *);

/**
 * Get the data from a specific object in the array.
 * Lock the data before returning.
 *
 * The referer has to make sure that buffer->mutex is locked before
 * calling this function.
 *
 * @param buffer the buffer to get the data from
 * @param id the id of the buffer this array should represent
 * @param pos the position in the array of the opbject to get
 * @return the object data
 */
#ifdef BUFFER_DEBUG
# define _intBufGet(a, b, c)  _intBufGetDebug(a, b, c, __FILE__, __LINE__)
 _intBufferData *
 _intBufGetDebug(const _intBuffers *, unsigned int, unsigned int,
                                            char *, int);
#else
# define _intBufGet(a, b, c)  _intBufGetNormal(a, b, c)
 _intBufferData *
 _intBufGetNormal(const _intBuffers *, unsigned int, unsigned int);
#endif

/**
 * Get the data from a specific object in the array.
 * Do not lock the data before returning.
 *
 * @param buffer the buffer to get the data from
 * @param id the id of the buffer this array should represent
 * @param pos the position in the array of the opbject to get
 * @return the object data
 */
_intBufferData *
_intBufGetNoLock(const _intBuffers *, unsigned int, unsigned int);

/**
 * Lock a data entry for changing or deletion until it gets released again.
 *
 * @param buffer the buffer to get the data from
 * @param id the id of the buffer this array should represent
 * @param pos the position in the array of the opbject to lock
 */
void
_intBufLockData(_intBuffers *, unsigned int, unsigned int);

/**
 * Unlock the data associated with a specific buffer position
 *
 * @param buffer the buffer to get the data from
 * @param id the id of the buffer this array should represent
 * @param pos the position in the array of the opbject to unlock
 */
void
_intBufRelease(const _intBuffers *, unsigned int, unsigned int);

/**
 * Release a previously locked data entry.
 *
 * @param object the object to unlock
 * @param id the id of the buffer this array should represent
 */
#ifndef NDEBUG
# define _intBufReleaseData(a, b)       _intBufReleaseDataDebug((a), (b), __FILE__, __LINE__)
void
_intBufReleaseDataDebug(const _intBufferData*, unsigned int, char*, int);
#else
void
_intBufReleaseData(const _intBufferData *, unsigned int);
#endif

/**
 * Lock the buffer and return the number of allocated objects.
 *
 * @param buffer the buffer to get the data from
 * @param id the id of the buffer this array should represent
 * @return the number of allocated objects in the buffer array
 */
unsigned int
_intBufGetNum(const _intBuffers *, unsigned int);

/**
 * Lock the buffer and return the maximum possible number of allocated objects.
 *
 * To allow for the buffer to grow easily the buffer might allocates more
 * positions than requested. In contrast to _intBufGetNum, which returns
 * the number currently allocated positions, this function returns the number
 * of prealocated positions instead.
 *
 * @param buffer the buffer to get the data from
 * @param id the id of the buffer this array should represent
 * @return the number of allocated objects in the buffer array
 */
unsigned int
_intBufGetMaxNum(const _intBuffers *, unsigned int);

/**
 * Return the number of allocated objects.
 *
 * @param buffer the buffer to get the data from
 * @param id the id of the buffer this array should represent
 * @return the number of allocated objects in the buffer array
 */
unsigned int
_intBufGetNumNoLock(const _intBuffers *, unsigned int);

/**
 * Return the maximum possible number of allocated objects.
 *
 * To allow for the buffer to grow easily the buffer might allocates more
 * positions than requested. In contrast to _intBufGetNum, which returns
 * the number currently allocated positions, this function returns the number
 * of prealocated positions instead.
 *
 * @param buffer the buffer to get the data from
 * @param id the id of the buffer this array should represent
 * @return the number of allocated objects in the buffer array
 */
unsigned int
_intBufGetMaxNumNoLock(const _intBuffers *, unsigned int);

/**
 * Return the position of an object.
 *
 * @param buffer the buffer to get the data from
 * @param id the id of the buffer this array should represent
 * @param data the object to search for
 * @return the position of the object in the buffer array
 */
unsigned int
_intBufGetPosNoLock(const _intBuffers *, unsigned int, void *);

/**
 * Unlock the buffer
 *
 * @param buffer the buffer to unlock
 * @param id the id of the buffer this array should represent
 */
void 
_intBufReleaseNum(const _intBuffers *, unsigned int);

/**
 * Lock a complete buffer to prevent any changes to it.
 *
 * @param buffer the buffer to lock
 * @param id the id of the buffer this array should represent
 */
void
_intBufLock(_intBuffers *, unsigned int);

/**
 * Unlock a complete buffer.
 *
 * @param buffer the buffer to lock
 * @param id the id of the buffer this array should represent
 */
void
_intBufUnlock(_intBuffers *, unsigned int);

/**
 * Remove a buffer from the array.
 * 
 * This function returns the user data associated with the object
 * and it's up to the developer to use it or free it from memory.
 *
 * The buffer at position pos should be locked before calling this function.
 *
 * @param buffer the buffer to remove the object from
 * @param id the id of the buffer this array should represent
 * @param pos the position of the buffer to remove
 * @param locked indicate whether te buffer is locked or not
 * @return the user data from the object
 */
#ifdef BUFFER_DEBUG
# define _intBufRemove(a, b, c, d) __intBufRemove(a, b, c, d, __FILE__, __LINE__)

void *
__intBufRemove(_intBuffers *, unsigned int, unsigned int, char, char *, int);
#else
# define _intBufRemove(a, b, c, d) int_intBufRemove(a, b, c, d);
#endif

void *
int_intBufRemove(_intBuffers *, unsigned int, unsigned int, char);


/**
 * Remove some buffers from the array and shift the remaining buffers 
 * (src-dst) positions forward.
 *
 * Not only is this a rather heavy operation but it also messes with the
 * buffer index array so positional refferences get mixed up. Use only when
 * the buffer acts as a stack array where just the first entry from the array
 * is required.
 *
 * This function returns an aray containing the user data associated with the
 * object and it's up to the developer to use it or free it from memory.
 * It is up to the caller to free the returned array from memory.
 *
 * @param buffer the buffer to remove the object from
 * @param id the id of the buffer this array should represent
 * @param dst the destination position within the array to move to
 * @param src the source position within the array to move from
 * @return an array of pointers refferencing the user data from the objects
 */
void **
_intBufShiftIndex(_intBuffers *, unsigned int, unsigned int, unsigned int);


/**
 * Remove the first buffer from the array and shift the remaining buffers 
 * one position forward.
 *
 * Not only is this a moderately heavy operation but it also messes with the
 * buffer index array so positional refferences get mixed up. Use only when
 * the buffer acts as a stack array where just the first entry from the array
 * is required.
 *
 * This function return a pointer to the user data associated with the
 * object and it's up to the developer to use it or free it from memory.
 *
 * @param buffer the buffer to remove the object from
 * @param id the id of the buffer this array should represent
 * @return te user data from the first object in the array
 */
_intBufferData *
_intBufPopData(_intBuffers *, unsigned int);


/**
 * Add an existing buffer to the end of the buffer array.
 *
 * @param buffer the buffer to remove the object from
 * @param id the id of the buffer this array should represent
 * @param buffer the buffer to add to the array
 * @return te user data from the first object in the array
 */
void
_intBufPushData(_intBuffers *, unsigned int, const _intBufferData *);


/**
 * Free all entries form the array by calling a callback funtion to remove
 * the objects. This function is non recursive.
 *
 * @param buffer the buffer to remove the object from
 * @param id the id of the buffer this array should represent
 * @param function a pointer to the callback function
 * @param data user data which will be passed to the callback function
 */
#ifdef BUFFER_DEBUG
# define _intBufClear(a, b, c, d) __intBufClear(a, b, c, d, __FILE__, __LINE__)

void
__intBufClear(_intBuffers *, unsigned int, _intBufFreeCallback, void *, char *, int);
#else
# define _intBufClear(a, b, c, d) int_intBufClear(a, b, c, d);
#endif

void
int_intBufClear(_intBuffers *, unsigned int, _intBufFreeCallback, void *);


/**
 * Free all entries form the array by calling a callback funtion to remove
 * the objects. This function is non recursive.
 * Clean up dat buffer afterwards.
 *
 * @param buffer the buffer to remove the object from
 * @param id the id of the buffer this array should represent
 * @param function a pointer to the callback function
 * @param data user data which will be passed to the callback function
 */
#ifdef BUFFER_DEBUG
# define _intBufErase(a, b, c, d) \
	_intBufEraseDebug(a, b, c, d, __FILE__, __LINE__)

void
_intBufEraseDebug(_intBuffers **, unsigned int,  _intBufFreeCallback, void *, char *, int);
#else
# define _intBufErase(a, b, c, d) _intBufEraseNormal(a, b, c, d);
#endif

void
_intBufEraseNormal(_intBuffers **, unsigned int, _intBufFreeCallback, void *);


/**
 * Get the pointer to the user data from an object
 *
 * @param object a pointer tot the object
 * @return a pointer to the user data.
 */
void *
_intBufGetDataPtr(const _intBufferData *);

/**
 * Set the pointer to new user data from an object
 *
 * @param object a pointer tot the object
 * @param data a pointer to the new user data
 * @return a pointer to the old user data.
 */
void *
_intBufSetDataPtr(_intBufferData *, void*);

#endif /* !_AL_BUFFERS_H */

