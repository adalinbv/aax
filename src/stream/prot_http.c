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

#include <stdio.h>
#include <errno.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#include <base/memory.h>
#include <api.h>

#include "device.h"
#include "audio.h"
#include "arch.h"
#include "io.h"

#ifndef ENODATA
# define ENODATA	61
#endif
#ifndef ETIME
# define ETIME		62
#endif

#define INCLUDE_ICY	0
#define MAX_HEADER	512
#define STREAMTITLE	"StreamTitle='"
#define HEADERLEN	strlen(STREAMTITLE)

static int _http_send_request(_io_t*, const char*, const char*, const char*, const char*);
static int _http_get_response(_io_t*, _data_t*, int*);
static const char *_get_yaml(_data_t*, const char*);


ssize_t
_http_connect(_prot_t *prot, _data_t *buf, _io_t *io, char **server, const char *path, const char *agent)
{
   int res = _http_send_request(io, "GET", *server, path, agent);

   if (res > 0)
   {
      int size = _http_get_response(io, buf, &res);
      if (res >= 200 && res < 300)
      {
         const char *s;

         s = _get_yaml(buf, "content-length");
         if (s)
         {
            prot->no_bytes = strtol(s, NULL, 10);
            res = prot->no_bytes;
         }

         s = _get_yaml(buf, "content-type");
         if (s) {
            prot->meta.comments = strdup(s);
         }
         if (s && _http_get(prot, __F_EXTENSION) != _EXT_NONE)
         {
            s = _get_yaml(buf, "icy-name");
            if (s)
            {
               prot->meta.artist_changed = true;
               prot->meta.artist = strdup(s);
               prot->meta.composer = strdup(s);
            }

            s = _get_yaml(buf, "icy-description");
            if (s)
            {
               prot->meta.title_changed = true;
               prot->meta.title = strdup(s);
               prot->meta.album = strdup(s);
            }

            s = _get_yaml(buf, "icy-genre");
            if (s) prot->meta.genre = strdup(s);

            s = _get_yaml(buf, "icy-url");
            if (s) prot->meta.website = strdup(s);

            s = _get_yaml(buf, "icy-metaint");
            if (s)
            {
               errno = 0;
               prot->meta_interval = strtol(s, NULL, 10);
               prot->meta_offset = prot->meta_interval;
               if (errno == ERANGE) {
                  _AAX_SYSLOG("stream meta out of range");
               }
            }
         }
      }
      else
      {
         res = -res;
         if (res <= -300 && res >= -400) // Moved
         {
            *server = (char*)_get_yaml(buf, "Location");
            errno = EREMCHG;
            res = -300;
         }
         else if (res == -400) errno = EINVAL;		// Bad Request
         else if (res == -403) errno = ECONNREFUSED;	// Forbidden
         else if (res == -404) errno = ENOENT;		// Not Found
         else if (res == -406) errno = EINVAL;		// Not Acceptable
         else if (res == -408) errno = ETIMEDOUT;	// Request Timeout
         else if (res == -410) errno = ENODATA;		// Gone
         else if (res == -414) errno = ENAMETOOLONG;	// URI Too Long
         else if (res == -500) errno = EREMOTEIO;	// Internal Server Error
         else if (res == -503) errno = EUSERS;		// Service Unavailable
         else if (res == -504) errno = ETIME;		// Gateway Timeout
         else errno = EPROTO;
      }
#if 0
 printf("server: %s\n", *server);
 printf("reponse: %i\n", res);
 printf("type: %s\n", prot->meta.comments);
 printf("artist: %s\n", prot->meta.artist);
 printf("station: %s\n", prot->meta.composer);
 printf("title: %s\n", prot->meta.title);
 printf("description: %s\n", prot->meta.album);
 printf("genre: %s\n", prot->meta.genre);
 printf("url: %s\n", prot->meta.website);
 printf("inteval %li\n", prot->meta_interval);
#endif
      _aaxDataMove(buf, 0, NULL, size);
   }
   else {
      res = -res;
   }

   return res;
}

void
_http_disconnect(_prot_t *prot)
{
   _aax_free_meta(&prot->meta);
}

/*
 * Read up to the meta-interval. If res > the remeaining bytes return the later.
 * Process the meta-data at the start of buf, remove it afterwards
 */
int
_http_process(_prot_t *prot, _data_t *buf)
{
   int rv = _aaxDataGetDataAvail(buf, 0);
   if (prot->meta_interval)
   {
      // The first byte indicates the meta length devided by 16
      // Empty meta information is indicated by a meta_len of 0
      if (!prot->meta_offset)
      {
         char *metaptr = _aaxDataGetData(buf, 0);
         int meta_len = *metaptr++;
         if (meta_len < rv)
         {
            if (meta_len > HEADERLEN+strlen("'\0"))
            {
               size_t header_len = meta_len - HEADERLEN;
               if (!strncasecmp(metaptr, STREAMTITLE, HEADERLEN))
               {
                  char *artist = metaptr + HEADERLEN;
                  char *title = strnstr(artist, " - ", header_len);
                  char *end = strnstr(artist, "\';", header_len);
                  if (!end) end = strnstr(artist, "\'\0", header_len);
                  if (title)
                  {
                     *title = '\0';
                     title += strlen(" - ");
                  }
                  if (end) {
                     *end = '\0';
                  }

                  prot->meta.artist_changed = true;
                  if (prot->meta.artist) free(prot->meta.artist);
                  if (artist && end)
                  {
                     prot->meta.artist_changed = true;
                     prot->meta.artist = strdup(artist);
                  }
                  else prot->meta.artist = strdup("");

                  prot->meta.title_changed = true;
                  if (prot->meta.title) free(prot->meta.title);
                  if (title && end)
                  {
                     prot->meta.title_changed = true;
                     prot->meta.title = strdup(title);
                  }
                  else prot->meta.title = strdup("");

                  prot->metadata_changed = true;
               }
            }

            prot->meta_offset = prot->meta_interval;
            meta_len++; // meta_len itself is one byte
            rv -= meta_len;

            _aaxDataMove(buf, 0, NULL, meta_len);
         }
         else { // meta_len > size
            rv = __F_NEED_MORE;
         }
      }
      else // prot->meta_offset > 0
      {
         rv = _MIN(rv, prot->meta_offset);
         prot->meta_offset -= rv;
      }
   }
   return rv;
}

int
_http_set(_prot_t *prot, enum _aaxStreamParam ptype, ssize_t param)
{
   int rv = -1;
   switch (ptype)
   {
   case __F_POSITION:
//    prot->meta_pos += param;
      rv = 0;
      break;
   default:
      break;
   }
   return rv;
}

int
_http_get(_prot_t *prot, enum _aaxStreamParam ptype)
{
   int rv = 0;
   if (prot && prot->meta.comments)
   {
      char *end = strchr(prot->meta.comments, ';');
      size_t len = end ? (end-prot->meta.comments) : strlen(prot->meta.comments);

      switch (ptype)
      {
      case __F_FMT:
         if (prot->meta.comments)
         {
            if (!strncasecmp(prot->meta.comments, "audio/mpeg", len)) {
               rv = _FMT_MP3;
            }
            else if (!strncasecmp(prot->meta.comments, "audio/flac", len)) {
               rv = _FMT_FLAC;
            }
            else if (!strncasecmp(prot->meta.comments, "audio/opus", len)) {
               rv = _FMT_OPUS;
            }
            else if (!strncasecmp(prot->meta.comments, "audio/vorbis", len)) {
               rv = _FMT_VORBIS;
            }
            else if (!strncasecmp(prot->meta.comments, "audio/speex", len)) {
               rv = _FMT_SPEEX;
            }
            else if (!strncasecmp(prot->meta.comments, "audio/wav", len) ||
                     !strncasecmp(prot->meta.comments, "audio/x-wav", len) ||
                     !strncasecmp(prot->meta.comments, "audio/x-pn-wav", len)) {
               rv = _EXT_WAV;
            }
            else if (!strncasecmp(prot->meta.comments, "audio/aiff", len) ||
                     !strncasecmp(prot->meta.comments, "audio/x-aiff", len) ||
                     !strncasecmp(prot->meta.comments, "audio/x-pn-aiff", len)) {
               rv = _EXT_AIFF;
            }
            else if (!strncasecmp(prot->meta.comments, "audio/basic", len) ||
                     !strncasecmp(prot->meta.comments, "audio/x-basic", len) ||
                     !strncasecmp(prot->meta.comments, "audio/x-pn-au", len)) {
               rv = _EXT_SND;
            }
            else if (!strncasecmp(prot->meta.comments, "audio/x-scpls", len) ||
                     !strncasecmp(prot->meta.comments, "audio/x-mpegurl", len) ||
                      !strncasecmp(prot->meta.comments, "audio/mpegurl", len) ||
              !strncasecmp(prot->meta.comments, "application/x-mpegurl", len) ||
                !strncasecmp(prot->meta.comments, "application/mpegurl", len) ||
+               !strncasecmp(prot->meta.comments, "application/vnd.apple.mpegurl", len) ||
               !strncasecmp(prot->meta.comments, "application/vnd.apple.mpegurl.audio", len)) {
               rv = _FMT_PLAYLIST;
            }
            else {
               rv = _FMT_NONE;
            }
         }
         else {
            rv = _direct_get(prot, ptype);
         }
         break;
      case __F_EXTENSION:
         if (prot->meta.comments)
         {
            if (!strncasecmp(prot->meta.comments, "audio/wav", len) ||
                !strncasecmp(prot->meta.comments, "audio/wave", len) ||
                !strncasecmp(prot->meta.comments, "audio/x-wav", len)) {
               rv = _EXT_WAV;
            }
            else if (!strncasecmp(prot->meta.comments, "audio/ogg", len) ||
                    !strncasecmp(prot->meta.comments, "application/ogg", len) ||
                    !strncasecmp(prot->meta.comments, "audio/x-ogg", len)) {
               rv = _EXT_OGG;
            }
            else if (!strncasecmp(prot->meta.comments, "audio/mpeg", len)) {
               rv = _EXT_MP3;
            }
            else if (!strncasecmp(prot->meta.comments, "audio/flac", len)) {
               rv = _EXT_FLAC;
            }
            else if (!strncasecmp(prot->meta.comments, "audio/x-scpls", len) ||
                    !strncasecmp(prot->meta.comments, "audio/x-mpegurl", len) ||
                      !strncasecmp(prot->meta.comments, "audio/mpegurl", len) ||
              !strncasecmp(prot->meta.comments, "application/x-mpegurl", len) ||
                !strncasecmp(prot->meta.comments, "application/mpegurl", len) ||
               !strncasecmp(prot->meta.comments, "application/vnd.apple.mpegurl", len) ||
               !strncasecmp(prot->meta.comments, "application/vnd.apple.mpegurl.audio", len)) {

            }
            else {
               rv = _EXT_NONE;
            }
         }
         else {
            rv = _direct_get(prot, ptype);
         }
         break;
      default:
         break;
      }
   }
   return rv;
}

char*
_http_name(_prot_t *prot, enum _aaxStreamParam ptype)
{
   char *ret = NULL;
   switch (ptype)
   {
   case __F_ARTIST:
      if (prot->meta.artist_changed)
      {
         ret = prot->meta.artist;
         prot->meta.artist_changed = false;
      }
      break;
   case __F_TITLE:
      if (prot->meta.title_changed)
      {
         ret = prot->meta.title;
         prot->meta.title_changed = false;
      }
      break;
   case __F_GENRE:
      ret = prot->meta.genre;
      break;
   case __F_ALBUM:
      ret = prot->meta.album;;
      break;
   case __F_COMPOSER:
      ret = prot->meta.composer;
      break;
   case __F_WEBSITE:
      ret = prot->meta.website;
      break;
   default:
      switch (ptype & ~__F_NAME_CHANGED)
      {
      case (__F_ARTIST):
         if (prot->meta.artist_changed)
         {
            ret = prot->meta.artist;
            prot->meta.artist_changed = false;
         }
         break;
      case (__F_TITLE):
         if (prot->meta.title_changed)
         {
            ret = prot->meta.title;
            prot->meta.title_changed = false;
         }
         break;
      default:
         break;
      }
      break;
   }
   return ret;
}

/* -------------------------------------------------------------------------- */

static int
_http_get_response_data(_io_t *io, _data_t *databuf)
{
   char *response = _aaxDataGetData(databuf, 0);
   size_t size = _aaxDataGetSize(databuf);
   static char end[4] = "\r\n\r\n";
   char *buf = response;
   unsigned int found = 0;
   int res, j, i = 0;

   do
   {
      i++;

      j = 50;
      do
      {
         res = io->read(io, databuf, 1);
         if (res > 0) break;
         msecSleep(1);
      }
      while (res == 0 && --j);

      if (res >= 1)
      {
         if (*buf == end[found]) found++;
         else found = 0;
      }
      else break;
      ++buf;
   }
   while ((i < size) && (found < sizeof(end)));

   if (i < size) {
      *buf = '\0';
   }
   response[size-1] = '\0';

   return i;
}

int
_http_send_request(_io_t *io, const char *command, const char *server, const char *path, const char *user_agent)
{
   _data_t *buf = _aaxDataCreate(1, MAX_HEADER, 0);
   char *header = _aaxDataGetData(buf, 0);
   int hlen, rv = 0;

   snprintf(header, MAX_HEADER+1,
            "%s /%.256s HTTP/1.0\r\n"
            "User-Agent: %s\r\n"
            "Accept: */*\r\n"
            "Host: %s\r\n"
            "Connection: Keep-Alive\r\n"
#if INCLUDE_ICY
            "Icy-MetaData:1\r\n"
#endif
            "\r\n",
            command, path, user_agent, server);

   hlen = strlen(header);
   _aaxDataIncreaseOffset(buf, 0, hlen);

   rv = io->write(io, buf);
   if (rv != hlen) {
      rv = __F_EOF;
   }

   _aaxDataDestroy(buf);

   return rv;
}

int
_http_get_response(_io_t *io, _data_t *buf, int *code)
{
   int res, rv = __F_EOF;

   *code = 0;
   rv = _http_get_response_data(io, buf);
   if (rv > 0)
   {
      char *ptr = _aaxDataGetData(buf, 0);
      res = sscanf(ptr, "HTTP/1.%*d %03d", code);
      if (res != 1)
      {
         res =  sscanf(ptr, "ICY %03d", (int*)&rv);
         if (res != 1) {
            rv = __F_EOF;
         }
      }
   }
   return rv;
}

static const char*
_get_yaml(_data_t *databuf, const char *needle)
{
   size_t haystacklen = _aaxDataGetSize(databuf);
   const char *haystack = _aaxDataGetData(databuf, 0);
   static char buf[1024];
   char *start, *end;
   size_t pos;

   buf[0] = '\0';

   start = strncasestr(haystack, needle, haystacklen);
   if (start)
   {
      start += strlen(needle);
      pos = start - haystack;

      end = memchr(haystack, '\0', haystacklen);
      if (end) haystacklen = (end-haystack);

      while ((pos++ < haystacklen) && (*start == ':' || *start == ' ')) {
         start++;
      }

      if (pos < haystacklen)
      {
         end = start;
         while ((pos++ < haystacklen) &&
                (*end != '\0' && *end != '\n' && *end != '\r')) {
            end++;
         }

         if ((end-start) > 1024) {
            end = start + 1024;
         }
         memcpy(buf, start, (end-start));
         buf[end-start] = '\0';
      }
   }

   return (buf[0] != '\0') ? buf : NULL;
}


