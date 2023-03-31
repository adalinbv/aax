/*
 * Copyright 2005-2023 by Erik Hofman.
 * Copyright 2009-2023 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
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
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif


#include <aax/aax.h>

#include <base/timer.h>
#include "io.h"

#ifndef O_BINARY
# define O_BINARY	0
#endif

int
_file_open(_io_t *io, UNUSED(_data_t *buf), const char* pathname, UNUSED(const char *path))
{
   io->fds.fd = open(pathname, io->param[_IO_FILE_FLAGS], io->param[_IO_FILE_MODE]);
   io->timer = _aaxTimerCreate();

   return io->fds.fd;
}

int
_file_close(_io_t *io)
{
   int rv = 0;
   if (io->fds.fd >= 0)
   {
      close(io->fds.fd);
      io->fds.fd = -1;
   }

   _aaxTimerDestroy(io->timer);

   return rv;
}

ssize_t
_file_read(_io_t *io, _data_t* buf, size_t count)
{
   size_t size = _MIN(count, _aaxDataGetFreeSpace(buf, 0));
   void *ptr = _aaxDataGetPtr(buf, 0);
   ssize_t rv = 0;

   if (size)
   {
      do {
         rv = read(io->fds.fd, ptr, size);
      } while (rv < 0 && errno == EINTR);

      if (rv > 0) {
         _aaxDataIncreaseOffset(buf, 0, rv);
      } else if (rv == 0) {
         rv = __F_EOF;
      }
   }

   return rv;
}

ssize_t
_file_write(_io_t *io, _data_t *buf)
{
   ssize_t res = _aaxDataGetDataAvail(buf, 0);
   ssize_t rv = 0;

   if (io->fds.fd >= 0 && res)
   {
      void *data = _aaxDataGetData(buf, 0);

      rv = write(io->fds.fd, data, res);
      if (rv > 0) {
         rv = _aaxDataMove(buf, 0, NULL, rv);
      }
   }

   return rv;
}

ssize_t
_file_update_header(_io_t *io, void *data, size_t size)
{
   off_t off = _file_get(io,__F_POSITION);
   ssize_t rv = -1;

   if (io->fds.fd >= 0)
   {
      _file_set(io, __F_POSITION, 0L);
      rv = write(io->fds.fd, data, size);
      _file_set(io, __F_POSITION, off);
   }

   return rv;
}

void
_file_wait(UNUSED(_io_t *io), UNUSED(float timeout_sec))
{
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
      rv = lseek(io->fds.fd, param, SEEK_SET);
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
      if (fstat(io->fds.fd, &st) == 0) {
          rv = st.st_size;
      }
      break;
   }
   case __F_POSITION:
      rv = lseek(io->fds.fd, 0L, SEEK_CUR);
      break;
   default:
      break;
   }
   return rv;
}

char*
_file_name(_io_t *io, enum _aaxStreamParam ptype)
{
   return NULL;
}

