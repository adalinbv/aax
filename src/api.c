/*
 * SPDX-FileCopyrightText: Copyright © 2011-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2011-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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
# include <strings.h>
#endif
#include <stdlib.h>
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

#ifndef PATH_MAX
# ifdef MAX_PATH
#  define PATH_MAX		MAX_PATH
# else
#  define PATH_MAX		1024
# endif
#endif

#define CONFIG_FILE             "config.xml"

#if defined(WIN32)
# define TEMP_DIR		getenv("TEMP")
# define SYSTEM_DIR		getenv("PROGRAMFILES")
# define USR_SYSTEM_DIR		getenv("PROGRAMFILES")
# define AAX_DIR		"\\"AEONWAVE_DIR"\\"

# define LOCALAPP_DIR		getenv("LOCALAPPDATA")
# define VENDOR_DIR             "\\Adalin\\"
# define USER_AAX_DIR		"\\Adalin\\"AEONWAVE_DIR"\\"

# define USER_DIR		getenv("USERPROFILE")

#else	/* !WIN32 */
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
# define TEMP_DIR		"/tmp"
# define SYSTEM_DIR		"/etc"
# define USR_SYSTEM_DIR		"/usr"SYSTEM_DIR
# define AAX_DIR		"/"AEONWAVE_DIR"/"

# define LOCALAPP_DIR		getenv("HOME")
# define USER_AAX_DIR		"/."AEONWAVE_DIR"/"

# define USER_DIR		getenv("HOME")
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
userCacheFile(const char *file)
{
   const char *app_path = LOCALAPP_DIR;
   char *rv = NULL;

   if (app_path)
   {
      size_t len;

      len = strlen(app_path);
      len += strlen(USER_AAX_DIR);
      len += strlen("cache/");
      len += strlen(file);
      len++;

      rv = malloc(len);
      if (rv) {
         snprintf(rv, len, "%s%scache", app_path, USER_AAX_DIR);
         mkDir(rv);

         snprintf(rv, len, "%s%scache/%s", app_path, USER_AAX_DIR, file);
      }
   }

   return rv;
}

char*
userConfigFile()
{
   const char *app_path = LOCALAPP_DIR;
   char *rv = NULL;

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

char *
systemDataFile(const char *file)
{
   char *global_path = APP_DATA_DIR;
   char *rv = NULL;

   if (global_path && file)
   {
      size_t len;

      len = strlen(global_path);
      len += strlen(file);
      len++;

      rv = malloc(len);
      if (rv) {
         snprintf(rv, len, "%s%s", global_path, file);
      }
   }

   return rv;
}

size_t getFileSize(const char *fname)
{
#ifndef WIN32
   struct stat st;
   if (!stat(fname, &st)) return st.st_size;
   return 0;
#else
   size_t rv = 0;
   HANDLE hFile;

   hFile = CreateFile(fname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL, NULL);
   if (hFile != INVALID_HANDLE_VALUE)
   {
      LARGE_INTEGER size;
      if (!GetFileSizeEx(hFile, &size)) {
         rv = size.QuadPart;
      }
      CloseHandle(hFile);
   }
   return rv;
#endif
}

#if defined(WIN32)
#include <sddl.h>

int
createDACL(SECURITY_ATTRIBUTES * pSA)
{
    PULONG nSize = 0;
    TCHAR *szSD = TEXT("D:")
       TEXT("(D;OICI;GA;;;BG)") /* Allow all to built-in Administrators group */
       TEXT("(D;OICI;GRGX;;;AN)") /* Allow all to Authenticated users         */
       TEXT("(A;OICI;GA;;;AU)")   /* Allow read/execute to anonymous logon    */
       TEXT("(A;OICI;GA;;;BA)");  /* Deny all for built-in guests             */

    if (pSA == NULL) {
        return -1;
    }

    return ConvertStringSecurityDescriptorToSecurityDescriptor(
                            szSD, SDDL_REVISION_1,
                            (PSECURITY_DESCRIPTOR*)&(pSA->lpSecurityDescriptor),
                            nSize);
}
#endif

/*
 * Consider paths within the users home directory, except the AeonWave config
 * directory, and the temp directory safe for writing.
 *
 * @mode directory for testing
 * @mode 0 for reading, 1 for writing
 */
int
isSafeDir(const char *directory, char mode)
{
   char abspath[PATH_MAX+1];
   const char *path = abspath;
   int rv = false;
#ifdef WIN32
   char **file;

   if (GetFullPathName(directory, PATH_MAX, abspath, file) == 0) {
      path = directory;
   }
#else
   if (realpath(directory, abspath) == NULL) {
      path = directory;
   }
#endif

   if (mode) // writing
   {
      if (!strncmp(path, USER_DIR, strlen(USER_DIR)))
      {
         path += strlen(USER_DIR);
         if (strncmp(path, USER_AAX_DIR, strlen(USER_AAX_DIR)) != 0) {
            rv = true;
         }
      }
      else if (!strncmp(path, TEMP_DIR, strlen(TEMP_DIR))) {
         rv = true;
      }
   }
   else // reading
   {
      if (strncmp(path, SYSTEM_DIR, strlen(SYSTEM_DIR)) != 0) {
         rv = true;
      }
   }

   return rv;
}

int
mkDir(const char *directory)
{
#ifndef WIN32
   int mode = strtol("0700", 0, 8);
   char path[1025];

   snprintf(path, 1024, "%s%s", USER_DIR, USER_AAX_DIR);
   assert(!strncmp(directory, path, strlen(path)));

   mkdir(path, mode);
   return mkdir(directory, mode);
#else
  /*
   * Note:
   * There is a default string size limit for paths of 248 characters.
   * This limit is related to how the CreateDirectory function parses
   * paths.
   */
   SECURITY_ATTRIBUTES sa;
   char path[256];
   int rv = 0;

   sa.nLength = sizeof(SECURITY_ATTRIBUTES);
   sa.bInheritHandle = FALSE;
   if(createDACL(&sa))
   {
       snprintf(path, 248, "%s", LOCALAPP_DIR);
       assert(!strncasecmp(directory, path, strlen(path)));

       CreateDirectory(path, &sa);

       snprintf(path, 248, "%s%s", LOCALAPP_DIR, VENDOR_DIR);
       assert(!strncasecmp(directory, path, strlen(path)));

       CreateDirectory(path, &sa);
       rv = CreateDirectory(directory, &sa);

       LocalFree(sa.lpSecurityDescriptor);
   }
   return !rv;
#endif
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
   return (SetEnvironmentVariable(name, value) == 0) ? true : false;
}

int
_aaxUnsetEnv(const char *name)
{
   return (SetEnvironmentVariable(name, NULL) == 0) ? true : false;
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
   char *params = strchr(url, '?');
   char *ptr;

   if (params) *params = '\0';

   *extension = NULL;
   *protocol = NULL;
   *server = NULL;
   *path = NULL;
   *port = 0;

   ptr = url;
   while ((ptr = strchr(ptr, '\\')) != NULL) *ptr++ = '/';

   ptr = strstr(url, "://");
   if (ptr)
   {
      *protocol = (char*)url;
      *ptr = '\0';
      url = ptr + strlen("://");
   }
   else if ((strrchr(url, '/') < strchr(url, '.')) || url[0] == '.')
   {
      *path = url;
      *extension = strrchr(url, '.');
      if (*extension) (*extension)++;
   }
   else if (strchr(url, '.')) /* 'example.com', 'dir.ext/file' or 'file.ext' */
   {
      ptr = strchr(url, '/');
      if (ptr) *ptr = '\0';

      if (!strchr(url, ':'))
      {
         struct addrinfo* res;
         int file_not_server = getaddrinfo(url, NULL, NULL, &res);
         if (ptr) *ptr = '/';

         if (file_not_server) /* not a server so it must be a file */
         {
            *path = url;
            *extension = strrchr(url, '.');
            if (*extension) (*extension)++;
         }
      }
   }
   else {
      *path = url;
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

#if 0
 printf("\nprotocol: '%s'\n", protocol[0]);
 printf("server: '%s'\n", server[0]);
 printf("path: '%s'\n", path[0]);
 printf("ext: '%s'\n", extension[0]);
 printf("port: %i\n", port[0]);
#endif
}

char *
_aaxURLConstruct(char *base, char *url2)
{
   char *url1 = strdup(base);
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

   free(url1);
   return strdup(url);
}

void
_aax_free_meta(struct _meta_t *meta)
{
   if (meta->trackno) free(meta->trackno);
   if (meta->artist) free(meta->artist);
   if (meta->title) free(meta->title);
   if (meta->album) free(meta->album);
   if (meta->date) free(meta->date);
   if (meta->genre) free(meta->genre);
   if (meta->comments) free(meta->comments);
   if (meta->composer) free(meta->composer);
   if (meta->copyright) free(meta->copyright);
   if (meta->original) free(meta->original);
   if (meta->website) free(meta->website);
   if (meta->contact) free(meta->contact);
   if (meta->image) free(meta->image);
   memset(meta, 0, sizeof(struct _meta_t));
}
