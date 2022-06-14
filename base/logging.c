/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2007-2017 by Adalin B.V.
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
#include <config.h>
#endif

/* Note: Must be declared before any inclussion of rmalloc.h */
extern void free(void*);
void (*_sys_free)(void*) = free;


#include <unistd.h>
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

void __aax_log(int level, int id, const char *s, const char *id_s[], int current_level)
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
_aax_getbool(const char *start)
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

#if defined(__linux__)
# define PROC_PATH 	"/proc/self/exe"
#elif defined(__NetBSD__) || defined(__DragonFly__)
# define PROC_PATH	"/proc/curproc/file"
#elif defined(__NetBSD__)
# define PROC_PATH      "/proc/curproc/exe"
#endif
const char*
_aax_get_binary_name(const char *defname)
{
   static char exe[1024];
   const char *rv = (defname) ? defname : "AeonWave Audio";
   ssize_t ret = sizeof(exe);

#if defined(__FreeBSD__)
   int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
   if (sysctl(mib, 4, exe, &ret, NULL, 0) == 0)
   {
      rv = strrchr(exe, '/');
      if (rv) rv++;
   }
#elif defined(PROC_PATH)
   ret = readlink(PROC_PATH, exe, sizeof(exe)-1);
   if(ret != -1)
   {  
      exe[ret] = 0;
      rv = strrchr(exe, '/');
      if (rv) rv++;
   }
#elif defined(__APPLE__) && defined(__MACH__)
   if (_NSGetExecutablePath(exe, &ret) == 0)
   {
      exe[ret] = 0;
      rv = strrchr(exe, '/');
      if (rv) rv++;
   }
#elif defined(WIN32)
   ret = GetModuleFileName(NULL, exe, sizeof(exe)-1);
   if (ret)
   {
      exe[ret] = 0;
      rv = strrchr(exe, '\\');
      if (rv) rv++;
   }
#endif
   return rv;
}
