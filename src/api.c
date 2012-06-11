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


#define AAX_DIR                 "/aax/"
#define CONFIG_FILE             "config.xml"

#if defined(WIN32)
# define SYSTEM_DIR		getenv("PROGRAMFILES")
# define USER_DIR               getenv("HOMEPATH")
# define USER_AAX_DIR		AAX_DIR
#else
# define SYSTEM_DIR		"/etc"
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

char*
userConfigFile()
{
   char *home_path = USER_DIR;
   char *rv = NULL;

   moveOldCfgFile();

   if (home_path)
   {
      int len;

      len = strlen(home_path);
      len += strlen(USER_AAX_DIR);
      len += strlen(CONFIG_FILE);
      len++;

      rv = malloc(len);
      if (rv) {
         snprintf(rv, len, "%s%s%s", home_path, USER_AAX_DIR, CONFIG_FILE);
      }
   }

   return rv;
}

char*
gloabalConfigFile()
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

const char*
userHomeDir()
{
   return USER_DIR;
}

