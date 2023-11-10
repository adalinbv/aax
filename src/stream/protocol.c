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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
# ifdef HAVE_UNISTD_H
#  include <unistd.h>	// access
# endif
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
   case PROTOCOL_HTTPS:
      rv = calloc(1, sizeof(_prot_t));
      if (rv)
      {
         rv->connect = _http_connect;
         rv->disconnect = _http_disconnect;
         rv->process = _http_process;
         rv->name = _http_name;
         rv->set_param = _http_set;
         rv->get_param = _http_get;

         rv->protocol = protocol;
      }
      break;
   case PROTOCOL_DIRECT:
      rv = calloc(1, sizeof(_prot_t));
      if (rv)
      {
         rv->connect = _direct_connect;
         rv->disconnect = _direct_disconnect;
         rv->process = _direct_process;
         rv->name = _direct_name;
         rv->set_param = _direct_set;
         rv->get_param = _direct_get;

         rv->protocol = protocol;
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
   _aax_free_meta(&prot->meta);
   free(prot);
   return 0;
}

/* NOTE: modifies url, make sure to strdup it before calling this function */
_protocol_t
_url_split(char *url, char **protocol, char **server, char **path, char **extension, int *port)
{
   _protocol_t rv = PROTOCOL_DIRECT;

   _aaxURLSplit(url, protocol, server, path, extension, port);

   if (*protocol)
   {
      if (*server && **server != 0)
      {
         if (!strcasecmp(*protocol, "http"))
         {
            if (!(*port)) *port = 80;
            rv = PROTOCOL_HTTP;
         }
         else if (!strcasecmp(*protocol, "https"))
         {
            if (!(*port)) *port = 443;
            rv = PROTOCOL_HTTPS;
         }
         else {
            rv = PROTOCOL_UNSUPPORTED;
         }
      }
      else if (!strcasecmp(*protocol, "file")) {
         rv = PROTOCOL_DIRECT;
      }
      else {
         rv = PROTOCOL_UNSUPPORTED;
      }
   }

   return rv;
}

char*
_url_get_param(char *url, const char *name, size_t *len)
{
   char *rv = strchr(url, '?');
   if (rv)
   {
      char pname[64];
      char *ptr;

      snprintf(pname, 63, "?%s=", name);

      ptr = strstr(rv, pname);
      if (ptr) {
         rv = ptr;
      }
      else
      {
         snprintf(pname, 63, "&%s=", name);
         rv = strstr(rv, pname);
      }

      if (rv)
      {
         rv += strlen(pname);
         if (len)
         {
            ptr = strchr(rv, '&');
            if (ptr) *len = (ptr-rv);
            else *len = strlen(rv);
         }
      }
   }

   return rv;
}
