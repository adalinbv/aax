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

#include <unistd.h>	// access
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif


#include "api.h"
#include "protocol.h"

_prot_t*
_prot_create(_protocol_t protocol)
{
   _prot_t* rv = 0;
   switch(protocol)
   {
   case PROTOCOL_HTTP:
      rv = calloc(1, sizeof(_prot_t));
      if (rv)
      {
         rv->connect = _http_connect;
         rv->process = _http_process;
         rv->name = _http_name;
         rv->set_param = _http_set;
         rv->get_param = _http_get;

         rv->protocol = protocol;
         rv->meta_interval = 0;
         rv->meta_pos = 0;
      }
      break;
   case PROTOCOL_DIRECT:
      rv = calloc(1, sizeof(_prot_t));
      if (rv)
      {
         rv->connect = _direct_connect;
         rv->process = _direct_process;
         rv->name = _direct_name;
         rv->set_param = _direct_set;
         rv->get_param = _direct_get;

         rv->protocol = protocol;
         rv->meta_interval = 0;
         rv->meta_pos = 0;
      }
      break;
   default:
      break;
   }
   return rv;
}

void*
_prot_free(_prot_t *prot)
{
   free(prot->path);
   free(prot->content_type);
   free(prot->station);
   free(prot->description);
   free(prot->genre);
   free(prot->website);
   free(prot);
   return 0;
}

/* NOTE: modifies url, make sure to strdup it before calling this function */
_protocol_t
_url_split(char *url, char **protocol, char **server, char **path, char **extension, int *port)
{
   _protocol_t rv;

   _aaxURLSplit(url, protocol, server, path, extension, port);

   if ((*protocol && !strcasecmp(*protocol, "http")) ||
       (*server && **server != 0))
   {
      if (!(*port)) *port = 80;
      rv = PROTOCOL_HTTP;
   }
   else if (!*protocol || !strcasecmp(*protocol, "file")) {
      rv = PROTOCOL_DIRECT;
   } else {
      rv = PROTOCOL_UNSUPPORTED;
   }

   return rv;
}
