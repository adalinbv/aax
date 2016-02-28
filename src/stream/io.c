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

#include <stdlib.h>

#include "io.h"

_io_t*
_io_create(_protocol_t protocol)
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

         rv->param[_IO_SOCKET_RATE] = 0;
         rv->param[_IO_SOCKET_PORT] = 80;
         rv->param[_IO_SOCKET_TIMEOUT] = 1000;
         break;
      case PROTOCOL_FILE:
         rv->open = _file_open;
         rv->close = _file_close;
         rv->read = _file_read;
         rv->write = _file_write;
         rv->set = _file_set;
         rv->get = _file_get;

         rv->param[_IO_FILE_FLAGS] = 0;
         rv->param[_IO_FILE_MODE] = 0644;
         break;
      default:
         break;
      }
      rv->protocol = protocol;
      rv->fd = -1;
   }
   return rv;
}

void*
_io_free(_io_t* io)
{
   free(io);
   return 0;
}
