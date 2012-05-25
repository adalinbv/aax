/*
 * Copyright (C) 2005-2011 by Erik Hofman.
 *a Copyright (C) 2007-2011 by Adalin B.V.
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

#include <assert.h>
#ifdef HAVE_SYSLOG_H
# include <strings.h>	/* for strcasecmp */
#endif

#include "logging.h"

void __oal_log(int level, int id, const char *s, const char *id_s[], int current_level)
{
   static char been_here = 0;
   static char enabled = 0;

   assert(id >= 0);

#ifdef HAVE_SYSLOG_H
   if (!been_here)
   {
      been_here = 1;
      if (1) /* (level == LOG_SYSLOG) */
      {
         const char *env = getenv("OPENAL_ENABLE_LOGGING");
         enabled = 0;
         if (env)
         {
            enabled = 1;
            if (strlen(env) > 0 && !(!strcasecmp(env, "true") || atoi(env))) {
               enabled = 0;
            }
         }
         if (enabled) openlog("OpenAL", LOG_ODELAY, LOG_USER);
      }
   }
   else if (level == LOG_SYSLOG && !id && !s)
   {
      closelog();
      been_here = 0;
      return;
   }
#endif

#ifdef HAVE_SYSLOG_H
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

