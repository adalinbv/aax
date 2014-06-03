/*
 * Copyright 2011-2014 by Erik Hofman.
 * Copyright 2011-2014 by Adalin B.V.
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

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif
#include <errno.h>
#include <assert.h>

#include <base/geometry.h>
#include <base/threads.h>
#include <base/types.h>

#include <filters/effects.h>
#include "api.h"


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


float _lin(float v) { return v; }
float _square(float v) { return v*v; }
float _lin2log(float v) { return log10f(v); }
float _log2lin(float v) { return powf(10.0f,v); }
float _lin2db(float v) { return 20.0f*log10f(v); }
float _db2lin(float v) { return _MINMAX(powf(10.0f,v/20.0f),0.0f,10.0f); }
float _rad2deg(float v) { return v*GMATH_RAD_TO_DEG; }
float _deg2rad(float v) { return fmodf(v, 360.0f)*GMATH_DEG_TO_RAD; }
float _cos_deg2rad_2(float v) { return cosf(_deg2rad(v)/2); }
float _2acos_rad2deg(float v) { return 2*acosf(_rad2deg(v)); }
float _cos_2(float v) { return cosf(v/2); }
float _2acos(float v) { return 2*acosf(v); }


#ifndef WIN32
# include <stdio.h>
# include <sys/types.h>
# include <sys/stat.h>
# ifdef HAVE_RMALLOC_H
#  include <rmalloc.h>
# endif

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
      size_t len;

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
      size_t len;

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

#ifdef WIN32
char*
_aaxGetEnv(const char*name)
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
_aaxSetEnv(const char *name, const char *value, int overwrite)
{
   return (SetEnvironmentVariable(name, value) == 0) ? AAX_TRUE : AAX_FALSE;
}

int
_aaxUnsetEnv(const char *name)
{
   return (SetEnvironmentVariable(name, NULL) == 0) ? AAX_TRUE : AAX_FALSE;
}
#endif


/*
 * Input: Backend (ALSA/WASAP/OSS) and driver (<card>: <interface>)
 * Output: Device (<backend> on <card>) and interface
 */
void
_aaxBackendDriverToDeviceConnector(char **backend, char **driver)
{
   char _str[2048], *ptr;
   size_t len;

   assert(backend && *backend);
   assert(driver && *driver);

   snprintf(_str, 2048, "%s on %s", *backend, *driver);
   ptr = strstr(_str, ": ");

   len = ptr - _str;
   if (len > strlen(*backend))
   {
      char *tmp = realloc(*backend, len+1);
      if (!tmp) return;

      *backend = tmp;
   }

   *ptr++ = 0;
   memcpy(*backend, _str, ptr - _str);
   memcpy(*driver, (ptr+1), strlen(ptr));
}

/*
 * Input: Device (<backend> on <card>) and interface
 * Output: Backend (ALSA/WASAP/OSS) and driver (<card>: <interface>)
 */
void
_aaxDeviceConnectorToBackendDriver(char **device, char **iface)
{
   char _str[2048], *ptr;
   size_t len;

   assert(device && *device);
   assert(iface && *iface);

   snprintf(_str, 2048, "%s: %s", *device, *iface);
   ptr = strstr(_str, " on ");

   len = strlen(ptr)-strlen(" on ");
   if (len > strlen(*iface))
   {
      char *tmp = realloc(*iface, len+1);
      if (!tmp) return;

      *iface = tmp;
   }

   *ptr = 0;
   memcpy(*device, _str, ptr - _str);

   ptr += strlen(" on ");
   memcpy(*iface, ptr, strlen(ptr));
}

