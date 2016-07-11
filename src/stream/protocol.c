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

#include <unistd.h>	// access
#include <string.h>	// strstr

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
         rv->set = _http_set;

         rv->protocol = protocol;
         rv->meta_interval = 0;
         rv->meta_pos = 0;
      }
   case PROTOCOL_FILE:
   default:
      break;
   }
   return rv;
}

void*
_prot_free(_prot_t *prot)
{
   free(prot->station);
   free(prot->description);
   free(prot->genre);
   free(prot->website);
   free(prot);
   return 0;
}

/* NOTE: modifies url, make sure to strdup it before calling this function */
_protocol_t
_url_split(char *url, char **protocol, char **server, char **path, int *port)
{
   _protocol_t rv;
   char *ptr;

   *protocol = NULL;
   *server = NULL;
   *path = NULL;
   *port = 0;

   ptr = strstr(url, "://");
   if (ptr)
   {
      *protocol = (char*)url;
      *ptr = '\0';
      url = ptr + strlen("://");
   }
   else if (access(url, F_OK) != -1) {
      *path = url;
   }

   if (!*path)
   {
      *server = url;

      ptr = strchr(url, '/');
      if (ptr)
      {
         *ptr++ = '\0';
         *path = ptr;
      }

      ptr = strchr(url, ':');
      if (ptr)
      {
         *ptr++ = '\0';
         *port = strtol(ptr, NULL, 10);
      }
   }

   if ((*protocol && !strcasecmp(*protocol, "http")) ||
       (*server && **server != 0))
   {
      rv = PROTOCOL_HTTP;
      if (*port <= 0) *port = 80;
   }
   else if (!*protocol || !strcasecmp(*protocol, "file")) {
      rv = PROTOCOL_FILE;
   } else {
      rv = PROTOCOL_UNSUPPORTED;
   }

   return rv;
}
