/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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
