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
      case PROTOCOL_HTTPS:
         rv->open = _socket_open;
         rv->close = _socket_close;
         rv->read = _socket_read;
         rv->write = _socket_write;
         rv->set_param = _socket_set;
         rv->get_param = _socket_get;
         rv->name = _socket_name;
         rv->wait = _socket_wait;

         rv->param[_IO_SOCKET_SIZE] = 2048;
         rv->param[_IO_SOCKET_PORT] = 80;
         rv->param[_IO_SOCKET_TIMEOUT] = 10000;
         break;
      case PROTOCOL_DIRECT:
         rv->open = _file_open;
         rv->close = _file_close;
         rv->read = _file_read;
         rv->write = _file_write;
         rv->update_header = _file_update_header;
         rv->set_param = _file_set;
         rv->get_param = _file_get;
         rv->name = _file_name;
         rv->wait = _file_wait;

         rv->param[_IO_FILE_FLAGS] = 0;
         rv->param[_IO_FILE_MODE] = 0644;
         break;
      default:
         free(rv);
         return NULL;
         break;
      }

      rv->prot = _prot_create(protocol);
      if (rv->prot)
      {
         rv->protocol = protocol;
         rv->fds.fd = -1;
         rv->fds.events = POLLIN;
      }
   }
   return rv;
}

void*
_io_free(_io_t* io)
{
   if (io->prot) {
      io->prot = _prot_free(io->prot);
   }
   free(io);
   return 0;
}
