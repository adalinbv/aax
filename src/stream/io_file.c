/*
 * Copyright 2005-2016 by Erik Hofman.
 * Copyright 2009-2016 by Adalin B.V.
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
#include <errno.h>

#include <aax/aax.h>

#include "io.h"

int
_file_open(_io_t *io, const char* pathname)
{
   io->fd = open(pathname, io->param[_IO_FILE_FLAGS],
                               io->param[_IO_FILE_MODE]);
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

off_t
_file_seek(_io_t *io, off_t offset, int whence)
{
   return lseek(io->fd, offset, whence);
}

int
_file_stat(_io_t *io, struct stat* st)
{
   return fstat(io->fd, st);
}


#ifndef O_BINARY
# define O_BINARY       0
#endif
int
_file_set(_io_t *io, int ptype, int param)
{
   static const int _mode[] = {
         O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,
         O_RDONLY|O_BINARY
   };

   switch (ptype)
   {
   case _IO_FILE_MODE:
      io->param[ptype] = _mode[(param == AAX_MODE_READ) ? 1 : 0];
      break;
   default:
      io->param[ptype] = param;
      break;
   }
   return (_IO_PARAM_MAX - ptype);
}

