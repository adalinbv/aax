/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <aax/aax.h>

#include "io.h"
#include "extension.h"

#ifndef O_BINARY
# define O_BINARY	0
#endif

int
_file_open(_io_t *io, const char* pathname)
{
   io->fd = open(pathname, io->param[_IO_FILE_FLAGS], io->param[_IO_FILE_MODE]);
   return io->fd;
}

int
_file_close(_io_t *io)
{
   int rv = close(io->fd);
   io->fd = -1;
   return rv;
}

ssize_t
_file_read(_io_t *io, void* buf, size_t count)
{
   ssize_t rv = read(io->fd, buf, count);
   if (rv == EINTR) rv = read(io->fd, buf, count);
   return rv;
}

ssize_t
_file_write(_io_t *io, const void* buf, size_t count)
{
   ssize_t rv = write(io->fd, buf, count);
   if (rv == EINTR) rv = write(io->fd, buf, count);
   return rv;
}

int
_file_set(_io_t *io, enum _aaxStreamParam ptype, ssize_t param)
{
   static const int _flags[] = {
         O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,
         O_RDONLY|O_BINARY
   };
   int rv = -1;

   switch (ptype)
   {
   case __F_POSITION:
      rv = (lseek(io->fd, param, SEEK_SET) >= 0) ? 0 : -1;
      break;
   case __F_FLAGS:
      io->param[_IO_FILE_FLAGS] = _flags[(param == AAX_MODE_READ) ? 1 : 0];
      rv = 0;
      break;
   case __F_MODE:
      io->param[_IO_FILE_MODE] = param;
      rv = 0;
      break;
   default:
      break;
   }
   return rv;
}

ssize_t
_file_get(_io_t *io, enum _aaxStreamParam ptype)
{
   ssize_t rv = 0;
   switch (ptype)
   {
   case __F_NO_BYTES:
   {
      struct stat st;
      if (fstat(io->fd, &st) == 0) {
          rv = st.st_size;
      }
      break;
   }
   case __F_POSITION:
      rv = lseek(io->fd, 0L, SEEK_CUR);
      break;
   default:
      break;
   }
   return rv;
}
