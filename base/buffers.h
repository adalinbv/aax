/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef _AL_BUFFERS_H
#define _AL_BUFFERS_H 1

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <limits.h>		/* for UINT_MAX */

#ifndef NDEBUG
# define BUFFER_DEBUG		1
#endif

typedef struct
{
#ifndef _AL_NOTHREADS
    void *mutex;
#endif

    unsigned int reference_ctr;
    const void *ptr;

} _intBufferData;

typedef struct
{
    unsigned int id;

    void *mutex;
    unsigned int lock_ctr;

    unsigned int start;			/* start of the queue,		 */
    unsigned int first_free;		/* first free, relative to start */
    unsigned int num_allocated;		/* no. allocated buffers	 */
    unsigned int max_allocations;	/* max. no. allocations possible */
					/* without the need te resize	 */
    _intBufferData **data;

} _intBuffers;

typedef void _intBufFreeCallback(void *);


#define BUFFER_RESERVE		8
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
# define _intBufCreate(a, b)  _intBufCreateDebug(a, b, __FILE__, __LINE__)
unsigned int
_intBufCreateDebug(_intBuffers **, unsigned int, char *, int);
#else
# define _intBufCreate(a, b)  _intBufCreateNormal(a, b)
#endif

unsigned int
_intBufCreateNormal(_intBuffers **, unsigned int);


/**
 * Destroy a data object that was retreieved using _intBufPop
 * and free it's contents, provided it wasn't referenced by others buffers.
 *
 * @param data pointer to the data object to destroy
 */
int
_intBufDestroyDataNoLock(_intBufferData *);


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
#define _intBufAddData(a, b, c) _intBufAddDataNormal(a, b, c, 0)
unsigned int
_intBufAddDataNormal(_intBuffers *, unsigned int, const void *, char);


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
# define _intBufGet(a, b, c)  _intBufGetDebug(a, b, c, 0, __FILE__, __LINE__)
 _intBufferData *
 _intBufGetDebug(_intBuffers *, unsigned int, unsigned int, char, char *, int);
#else
# define _intBufGet(a, b, c)  _intBufGetNormal(a, b, c, 0)
#endif

_intBufferData *
_intBufGetNormal(_intBuffers *, unsigned int, unsigned int, char);


/**
 * Get the data from a specific object in the array.
 * Do not lock the data before returning.
 *
 * @param buffer the buffer to get the data from
 * @param id the id of the buffer this array should represent
 * @param pos the position in the array of the opbject to get
 * @return the object data
 */
#define _intBufGetNoLock(a, b, c) _intBufGetNormal(a, b, c, 1)


/**
 * Unlock the data associated with a specific buffer position
 *
 * @param buffer the buffer to get the data from
 * @param id the id of the buffer this array should represent
 * @param pos the position in the array of the opbject to unlock
 */
#ifndef _AL_NOTHREADS
void
_intBufRelease(_intBuffers *, unsigned int, unsigned int);
#else
# define _intBufRelease(a, b, c)
#endif


/**
 * Release a previously locked data entry.
 *
 * @param object the object to unlock
 * @param id the id of the buffer this array should represent
 */
#ifndef _AL_NOTHREADS
# ifndef NDEBUG
#  define _intBufReleaseData(a, b)  _intBufReleaseDataDebug((a), (b), __FILE__, __LINE__)
void
_intBufReleaseDataDebug(const _intBufferData*, unsigned int, char*, int);
# else
#  define _intBufReleaseData(a, b)  _intBufReleaseDataNormal(a, b)
# endif

void
_intBufReleaseDataNormal(const _intBufferData *, unsigned int);
#else
# define _intBufReleaseData(a, b)
#endif

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
 * Lock the buffer and return the number of allocated objects.
 *
 * @param buffer the buffer to get the data from
 * @param id the id of the buffer this array should represent
 * @param lock set to 1 for operations that mangle the pointer array
 * @return the number of allocated objects in the buffer array
 */
#ifndef NDEBUG
# define _intBufGetNum(a, b)  _intBufGetNumDebug(a, b, 0,  __FILE__, __LINE__)
unsigned int
_intBufGetNumDebug(_intBuffers *, unsigned int, char, char *, int);
#else
# define _intBufGetNum(a, b)  _intBufGetNumNormal(a, b, 0)
#endif

unsigned int
_intBufGetNumNormal(_intBuffers *, unsigned int, char);


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
 * @param lock set to 1 for operations that mangle the pointer array
 * @return the number of allocated objects in the buffer array
 */
#define _intBufGetMaxNum(a, b)	_intBufGetMaxNumNormal(a, b, 0)

unsigned int
_intBufGetMaxNumNormal(_intBuffers *, unsigned int, char);


/**
 * Unlock the buffer
 *
 * @param buffer the buffer to unlock
 * @param id the id of the buffer this array should represent
 * @param lock set to 1 for operations that mangle the pointer array
 */
#ifndef _AL_NOTHREADS
# define _intBufReleaseNum(a, b) _intBufReleaseNumNormal(a, b, 0)
void
_intBufReleaseNumNormal(_intBuffers *, unsigned int, char);
#else
# define _intBufReleaseNum(a, b)
#endif


/**
 * Return the position of an object.
 *
 * @param buffer the buffer to get the data from
 * @param id the id of the buffer this array should represent
 * @param data the object to search for
 * @return the position of the object in the buffer array
 */
unsigned int
_intBufGetPos(_intBuffers *, unsigned int, void *);


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
# define _intBufRemove(a, b, c, d)  _intBufRemoveDebug(a, b, c, d, 0, __FILE__, __LINE__)
void *
_intBufRemoveDebug(_intBuffers *, unsigned int, unsigned int, char, char, char *, int);
#else
# define _intBufRemove(a, b, c, d)  _intBufRemoveNormal(a, b, c, d, 0)
#endif

void *
 _intBufRemoveNormal(_intBuffers *, unsigned int, unsigned int, char, char);


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
#ifdef BUFFER_DEBUG
# define _intBufPop(a, b)  _intBufPopDebug(a, b, 0, __FILE__, __LINE__)
_intBufferData *
_intBufPopDebug(_intBuffers*, unsigned int, char, char*, int);
#else
# define _intBufPop(a, b)  _intBufPopNormal(a, b, 0)
#endif

_intBufferData *
_intBufPopNormal(_intBuffers *, unsigned int, char);


/**
 * Add an existing buffer to the end of the buffer array.
 *
 * @param buffer the buffer to remove the object from
 * @param id the id of the buffer this array should represent
 * @param buffer the buffer to add to the array
 * @return te user data from the first object in the array
 */
#define _intBufPush(a, b, c) _intBufPushNormal(a, b, c, 0)
void
_intBufPushNormal(_intBuffers *, unsigned int, const _intBufferData*, char);


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
# define _intBufClear(a, b, c)  _intBufClearDebug(a, b, c, __FILE__, __LINE__)
void
_intBufClearDebug(_intBuffers *, unsigned int, _intBufFreeCallback, char*, int);
#else
# define _intBufClear(a, b, c)  _intBufClearNormal(a, b, c)
#endif

void
_intBufClearNormal(_intBuffers *, unsigned int, _intBufFreeCallback);


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
# define _intBufErase(a, b, c)  _intBufEraseDebug(a, b, c, __FILE__, __LINE__)
void
_intBufEraseDebug(_intBuffers**, unsigned int, _intBufFreeCallback, char*, int);
#else
# define _intBufErase(a, b, c)  _intBufEraseNormal(a, b, c)
#endif

void
_intBufEraseNormal(_intBuffers **, unsigned int, _intBufFreeCallback);


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


#if defined(__cplusplus)
}  /* extern "C" */
#endif


#endif /* !_AL_BUFFERS_H */

