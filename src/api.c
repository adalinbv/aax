/*
 * Copyright 2011-2012 by Erik Hofman.
 * Copyright 2011-2012 by Adalin B.V.
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

#include <string.h>
#include <stdlib.h>		/* for getenv */
#include <errno.h>
#include <errno.h>
#ifndef _WIN32
# include <sys/time.h>
# include <sys/resource.h>
#endif

#include <base/types.h>
#include <api.h>


#define CONFIG_FILE             "config.xml"

#if defined(WIN32)
# define SYSTEM_DIR		getenv("PROGRAMFILES")
# define AAX_DIR		"\\aax\\"

# define LOCALAPP_DIR		getenv("LOCALAPPDATA")
# define USER_AAX_DIR		"\\adalin\\aax\\"

# define USER_DIR		getenv("USERPROFILE")

#else	/* !WIN32 */
# define SYSTEM_DIR		"/etc"
# define AAX_DIR		"/aax/"

# define LOCALAPP_DIR		getenv("HOME")
# define USER_AAX_DIR		"/.aax/"

# define USER_DIR		getenv("HOME")
#endif


#ifndef WIN32
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
# include <stdio.h>

static void
moveOldCfgFile()
{
   char *path = USER_DIR;
   if (path)
   {
      char ofile[256];
      struct stat sb;

      snprintf(ofile, 256, "%s/.aaxconfig.xml", path);
      if (!stat(ofile, &sb) && (sb.st_size > 0))
      {
         char nfile[256];

         snprintf(nfile, 256, "%s%s", path, USER_AAX_DIR);
         if (stat(nfile, &sb))
         {
            int mode = strtol("0700", 0, 8);
            mkdir(nfile, mode);       /* the new directory does not yet exist */
         }

         snprintf(nfile, 256, "%s%s%s", path, USER_AAX_DIR, CONFIG_FILE);
         if (stat(nfile, &sb)) {
            rename(ofile, nfile);   /* the new config file does not yet exist */
         }
      }
   }
}
#else
# define moveOldCfgFile()
#endif

const char*
userHomeDir()
{
   return USER_DIR;
}

char*
userConfigFile()
{
   const char *app_path = LOCALAPP_DIR;
   char *rv = NULL;

   moveOldCfgFile();

   if (app_path)
   {
      int len;

      len = strlen(app_path);
      len += strlen(USER_AAX_DIR);
      len += strlen(CONFIG_FILE);
      len++;

      rv = malloc(len);
      if (rv) {
         snprintf(rv, len, "%s%s%s", app_path, USER_AAX_DIR, CONFIG_FILE);
      }
   }

   return rv;
}

char*
systemConfigFile()
{
   char *global_path = SYSTEM_DIR;
   char *rv = NULL;

   if (global_path)
   {
      int len;

      len = strlen(global_path);
      len += strlen(AAX_DIR);
      len += strlen(CONFIG_FILE);
      len++;

      rv = malloc(len);
      if (rv) {
         snprintf(rv, len, "%s%s%s", global_path, AAX_DIR, CONFIG_FILE);
      }
   }

   return rv;
}

int
_aaxProcessSetPriority(int prio)
{
   int rv = 0;

#ifdef WIN32
   DWORD curr_priority = GetPriorityClass(GetCurrentProcess());
   DWORD new_priority = NORMAL_PRIORITY_CLASS;
   if (prio < -16)     new_priority = HIGH_PRIORITY_CLASS;
   else if (prio < -6) new_priority = ABOVE_NORMAL_PRIORITY_CLASS;
   else if (prio > 4)  new_priority = BELOW_NORMAL_PRIORITY_CLASS;
   else                new_priority = IDLE_PRIORITY_CLASS;

   if (new_priority < curr_priority)
   {
      rv = SetPriorityClass(GetCurrentProcess(), new_priority);
      if (!rv) {
         rv = GetLastError();
      }
   }

#else
   int curr_prio = getpriority(PRIO_PROCESS, getpid());
   if (curr_prio < prio)
   {
      errno = 0;
      rv = setpriority(PRIO_PROCESS, getpid(), prio);
      if (rv < 0) {
         rv = errno;
     }
   }
#endif

   return rv;
}

#ifdef WIN32
char*
aaxGetEnv(const char*name)
{
   static char _key[256] = "";
   char *rv = NULL;
   DWORD res, err;

   res = GetEnvironmentVariable(name, (LPSTR)&_key, 256);
   err = GetLastError();
   if (res || !err) {
       rv = (char*)&_key;
   }

   return rv;
}

int
aaxSetEnv(const char *name, const char *value, int overwrite)
{
   return (SetEnvironmentVariable(name, value) == 0) ? AAX_TRUE : AAX_FALSE;
}

int
aaxUnsetEnv(const char *name)
{
   return (SetEnvironmentVariable(name, NULL) == 0) ? AAX_TRUE : AAX_FALSE;
}
#endif

