/*
 * Copyright 2005-2020 by Erik Hofman.
 * Copyright 2009-2020 by Adalin B.V.
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
#include "audio.h"

#ifndef O_BINARY
# define O_BINARY	0
#endif

#define IOBUF_SIZE	(1024*1024)
#define THRESHOLD	(2*IOBUF_SIZE/3)

int
_file_open(_io_t *io, const char* pathname)
{
   io->fds.fd = open(pathname, io->param[_IO_FILE_FLAGS], io->param[_IO_FILE_MODE]);
   io->timer = _aaxTimerCreate();
   io->dataBuffer = _aaxDataCreate(IOBUF_SIZE, 1);

   return io->fds.fd;
}

int
_file_close(_io_t *io)
{
   int rv = 0;

   ssize_t avail = _aaxDataGetDataAvail(io->dataBuffer);
   void *data = _aaxDataGetData(io->dataBuffer);
   ssize_t res = write(io->fds.fd, data, avail);
   if (res == EINTR) rv = write(io->fds.fd, data, avail);

   _aaxDataDestroy(io->dataBuffer);
   _aaxTimerDestroy(io->timer);

   if (io->fds.fd != -1) close(io->fds.fd);
   io->fds.fd = -1;

   return rv;
}

ssize_t
_file_read(_io_t *io, void* buf, size_t count)
{
   ssize_t rv;

   do {
      rv  = read(io->fds.fd, buf, count);
   } while (rv < 0 && errno == EINTR);
   if (rv == 0) rv = -1;

   return rv;
}

ssize_t
_file_write(_io_t *io, const void* buf, size_t count)
{
   ssize_t rv = _aaxDataAdd(io->dataBuffer, buf, count);

   if (_aaxDataGetOffset(io->dataBuffer) >= THRESHOLD)
   {
      void *data = _aaxDataGetData(io->dataBuffer);
      ssize_t res = write(io->fds.fd, data, THRESHOLD);
      if (res == EINTR) rv = write(io->fds.fd, data, THRESHOLD);

      if (res > 0) {
         res = _aaxDataMove(io->dataBuffer, NULL, res);
      }
   }

   if (rv < count) {
      rv += _aaxDataAdd(io->dataBuffer, (char*)buf+count-rv, rv);
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
