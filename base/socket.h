/*
 * Copyright 2012-2015 by Erik Hofman.
 * Copyright 2012-2015 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef __SOCKET_H
#define __SOCKET_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include <sys/stat.h>

#include <base/types.h>

int _socket_open(const char*, int, int, int);
int _socket_close(int);
ssize_t _socket_read(int, void*, size_t);
ssize_t _socket_write(int, const void*, size_t);
off_t _socket_seek(int, off_t, int);
int _socket_stat(int, struct stat*);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* __SOCKET_H */


