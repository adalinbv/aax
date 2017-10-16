/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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

#include <stdlib.h>

#include "io.h"

_io_t*
_io_create(int protocol)
{
   _io_t* rv = calloc(1, sizeof(_io_t));
   if (rv)
   {
      switch(protocol)
      {
      case PROTOCOL_HTTP:
         rv->open = _socket_open;
         rv->close = _socket_close;
         rv->read = _socket_read;
         rv->write = _socket_write;
         rv->set = _socket_set;
         rv->get = _socket_get;
         rv->wait = _socket_wait;

         rv->param[_IO_SOCKET_SIZE] = 2048;
         rv->param[_IO_SOCKET_PORT] = 80;
         rv->param[_IO_SOCKET_TIMEOUT] = 100;
         break;
      case PROTOCOL_DIRECT:
         rv->open = _file_open;
         rv->close = _file_close;
         rv->read = _file_read;
         rv->write = _file_write;
         rv->set = _file_set;
         rv->get = _file_get;
         rv->wait = _file_wait;

         rv->param[_IO_FILE_FLAGS] = 0;
         rv->param[_IO_FILE_MODE] = 0644;
         break;
      default:
         break;
      }
      rv->protocol = protocol;
      rv->fds.fd = -1;
      rv->fds.events = POLLIN;
   }
   return rv;
}

void*
_io_free(_io_t* io)
{
   free(io);
   return 0;
}
