/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif


#include <aax/aax.h>

#include "audio.h"

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
   ssize_t rv;

   do {
      rv  = read(io->fd, buf, count);
   } while (rv < 0 && errno == EINTR);
   if (rv == 0) rv = -1;

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
