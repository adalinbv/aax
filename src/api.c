/*
 * Copyright 2011-2017 by Erik Hofman.
 * Copyright 2011-2017 by Adalin B.V.
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
#include <locale.h>
#if HAVE_NETDB_H
# include <netdb.h>
#endif

#include <base/geometry.h>
#include <base/threads.h>
#include <base/types.h>
#include <dsp/filters.h>
#include <dsp/effects.h>

#include "api.h"


#define CONFIG_FILE             "config.xml"

#if defined(WIN32)
# define TEMP_DIR		getenv("TEMP")
# define SYSTEM_DIR		getenv("PROGRAMFILES")
# define USR_SYSTEM_DIR		getenv("PROGRAMFILES")
# define AAX_DIR		"\\aax\\"

# define LOCALAPP_DIR		getenv("LOCALAPPDATA")
# define USER_AAX_DIR		"\\adalin\\aax\\"

# define USER_DIR		getenv("USERPROFILE")

#else	/* !WIN32 */
# define TEMP_DIR		"/tmp"
# define SYSTEM_DIR		"/etc"
# define USR_SYSTEM_DIR		"/usr"SYSTEM_DIR
# define AAX_DIR		"/aax/"

# define LOCALAPP_DIR		getenv("HOME")
# define USER_AAX_DIR		"/.aax/"

# define USER_DIR		getenv("HOME")
#endif


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

const char*
tmpDir()
{
   return TEMP_DIR;
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
systemConfigFile(const char *file)
{
   char *global_path = file ? USR_SYSTEM_DIR : SYSTEM_DIR;
   char *rv = NULL;

   if (global_path)
   {
      size_t len;

      if (!file) file = CONFIG_FILE;

      len = strlen(global_path);
      len += strlen(AAX_DIR);
      len += strlen(file);
      len++;

      rv = malloc(len);
      if (rv) {
         snprintf(rv, len, "%s%s%s", global_path, AAX_DIR, file);
      }
   }

   return rv;
}

char*
systemLanguage(char **enc)
{
   static char _locale[65] = "C";
   static char *language = NULL;
   static char *encoding = NULL;

   if (language == NULL)
   {
      setlocale(LC_ALL, "");

      language = setlocale(LC_CTYPE, NULL);
      snprintf(_locale, 64, "%s", language);

      language = _locale;
      if ((encoding = strchr(language, '.')) != NULL) {
         *encoding++ = 0;
      } else {
         encoding = "";
      }
   }
   if (enc) {
      *enc = encoding;
   }

   return _locale;
}

#ifdef WIN32
char*
_aaxGetEnv(const char*name)
{
   static char _data[256] = "";
   char *rv = NULL;
   DWORD res, err;

   res = GetEnvironmentVariable(name, (LPSTR)&_data, 256);
   err = GetLastError();
   if (res || !err) {
       rv = (char*)&_data;
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

unsigned long long _aax_get_free_memory()
{
   MEMORYSTATUSEX status;
   status.dwLength = sizeof(status);
   GlobalMemoryStatusEx(&status);
   return status.ullAvailPhys;
}
#else
unsigned long long _aax_get_free_memory()
{
   long pages = sysconf(_SC_AVPHYS_PAGES);
   long page_size = sysconf(_SC_PAGE_SIZE);
   return pages * page_size;
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
   if (ptr)
   {
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
   if (ptr)
   {
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
}

/*
 * Input: <connector> (<card>)
 * Output: <card>: <connector>
 */
void
_aaxConnectorDeviceToDeviceConnector(char *iface)
{
   char _str[2048], *ptr;

   assert(iface);

   snprintf(_str, 2048, "%s", iface);
   ptr = strstr(_str, " (");
   if (ptr)
   {
      *ptr = 0;
      ptr += strlen(ptr+1);
      if (*ptr == ')')
      {
         *ptr = 0;
         sprintf(iface, "%s: %s", ptr, _str);
      }
   }
}

void
_aaxURLSplit(char *url, char **protocol, char **server, char **path, char **extension, int *port)
{
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
   else if (!strchr(url, '/')) /* something like 'example.com' or 'test.wav' */
   {
      struct addrinfo* res;
      if (getaddrinfo(url, NULL, NULL, &res)) /* not a server, it's a file */
      {
         *path = url;
         *extension = strrchr(url, '.');
         if (*extension) (*extension)++;
      }
   }
   else if ((strrchr(url, '/') < strchr(url, '.')) ||
             url[0] == '.') // access(url, F_OK) != -1)
   {
      *path = url;
      *extension = strrchr(url, '.');
      if (*extension) (*extension)++;
   }

   if (!*path)
   {
      *server = url;

      ptr = strchr(url, '/');
      if (ptr)
      {
         if (ptr != url) *ptr++ = '\0';
         else *server = 0;

         *path = ptr;
         *extension = strrchr(ptr, '.');
         if (*extension) (*extension)++;
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
      if (*port < 0) *port = 80;
   }
}

char *
_aaxURLConstruct(char *url1, char *url2)
{
   char *prot[2], *srv[2], *path[2], *ext[2];
   char url[PATH_MAX+1];
   int abs, port[2];
   char *ptr;

   url[0] = '\0';
   _aaxURLSplit(url1, &prot[0], &srv[0], &path[0], &ext[0], &port[0]);
   _aaxURLSplit(url2, &prot[1], &srv[1], &path[1], &ext[1], &port[1]);

   if (path[0] && (ptr = strrchr(path[0], '/')) != NULL) {
      *(ptr+1) = '\0';
   }

   if (srv[1] || (path[1] && *path[1] == '/')) abs = 1;
   else abs = path[0] ? 0 : 1;

   if (srv[0] || srv[1])
   {
      if ((prot[0] || prot[1]) && ((port[0] && !srv[1]) || port[1])) {
         snprintf(url, PATH_MAX, "%s://%s:%i/%s%s",
                               prot[1] ? prot[1] : prot[0],
                               srv[1] ? srv[1] : srv[0],
                               port[1] ? port[1] : port[0],
                               abs ? "" : path[0], path[1]);
      } else if (prot[0] || prot[1]) {
         snprintf(url, PATH_MAX, "%s://%s/%s%s",
                               prot[1] ? prot[1] : prot[0],
                               srv[1] ? srv[1] : srv[0],
                               abs ? "" : path[0], path[1]);
      } else if ((port[0] && !srv[1]) || port[1]) {
         snprintf(url, PATH_MAX, "%s:%i/%s%s",
                               srv[1] ? srv[1] : srv[0],
                               port[1] ? port[1] : port[0], 
                               abs ? "" : path[0], path[1]);
     } else {
         snprintf(url, PATH_MAX, "%s/%s%s",
                               srv[1] ? srv[1] : srv[0],
                               abs ? "" : path[0], path[1]);
     }
   } else {
      snprintf(url, PATH_MAX, "%s%s", abs ? "" : path[0], path[1]);
   }

#if 0
 printf("protocol: '%s' - '%s'\n", prot[0], prot[1]);
 printf("server: '%s' - '%s'\n", srv[0], srv[1]);
 printf("path: '%s' - '%s'\n", path[0], path[1]);
 printf("ext: '%s' - '%s'\n", ext[0], ext[1]);
 printf("port: %i - %i\n", port[0], port[1]);
 printf("new url: %s\n\n", url);
#endif

   return strdup(url);
}

