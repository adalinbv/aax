/*
 * Copyright (C) 2005-2011 by Erik Hofman.
 * Copyright (C) 2007-2011 by Adalin B.V.
 *
 * This file is part of OpenAL-AeonWave.
 *
 *  OpenAL-AeonWave is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenAL-AeonWave is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OpenAL-AeonWave.  If not, see <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

/* Note: Must be declared before any inclussion of rmalloc.h */
extern void free(void*);
void (*_sys_free)(void*) = free;


#include <assert.h>
#if WIN32
	/* See: http://support.microsoft.com/kb/815661 */
# include <tchar.h>
# if defined(__cplusplus)
#  using <system.dll>
#  using <mscorlib.dll>
# endif
#endif

#include "logging.h"
#include "types.h"

void __oal_log(int level, int id, const char *s, const char *id_s[], int current_level)
{
#if HAVE_SYSLOG_H
   static char enabled = 0;
   static char been_here = 0;

   assert(id >= 0);

   if (!been_here)
   {
      been_here = 1;
      if (1) /* (level == LOG_SYSLOG) */
      {
         const char *env = getenv("AAX_ENABLE_LOGGING");
         enabled = 0;
         if (env)
         {
            enabled = 1;
            if (strlen(env) > 0 && !(!strcasecmp(env, "true") || atoi(env))) {
               enabled = 0;
            }
         }
         if (enabled) openlog("AAX", LOG_ODELAY, LOG_USER);
      }
   }
   else if (level == LOG_SYSLOG && !id && !s)
   {
      closelog();
      been_here = 0;
      return;
   }

   if (enabled && level == LOG_SYSLOG) {
      syslog(level, "%s", s);
   }
   else
#endif
#if USE_LOGGING
   if (level >= LOG_EMERG && level <= LOG_DEBUG && level <= current_level)
   {
      if (id) {
         printf("%16s | %s\n", id_s[id], s);
      } else {
         printf("%s\n", s);
      }
   }
#else
   {}
#endif
}


int
_oal_getbool(const char *start)
{
   int rv = 0;
   if (start)
   {
      unsigned int len = strlen(start);
      char *ptr;

      ptr = (char *)start + len;
      if (strtol(start, &ptr, 10) == 0)
      {
         if (!strncasecmp(start, "on", len) || !strncasecmp(start, "yes", len)
             || !strncasecmp(start, "true", len))
         {
            rv = -1;
         }
      }
      else {
         rv = -1;
      }
   }
   return rv;
}

