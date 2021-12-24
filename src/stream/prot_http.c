/*
 * Copyright 2005-2021 by Erik Hofman.
 * Copyright 2009-2021 by Adalin B.V.
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

   if (path)
   {
      if (prot->path) free(prot->path);
      prot->path = strdup(path);
   }

   if (res > 0)
   {
      int size = _http_get_response(io, buf, &res);
      if (res >= 300 && res < 400) // Moved
      {
           *server = (char*)_get_yaml(buf, "Location");
           res = -300;
      }
      else if (res >= 200 && res < 300)
      {
         const char *s;
         size_t res = 0;

         s = _get_yaml(buf, "content-length");
         if (s)
         {
            prot->no_bytes = strtol(s, NULL, 10);
            res = prot->no_bytes;
         }

         s = _get_yaml(buf, "content-type");
         if (s) {
            prot->content_type = strdup(s);
         }
         if (s && _http_get(prot, __F_EXTENSION) != _EXT_NONE)
         {
            s = _get_yaml(buf, "icy-name");
            if (s)
            {
               int len = _MIN(strlen(s)+1, MAX_ID_STRLEN);
               memcpy(prot->artist+1, s, len);
               prot->artist[len] = '\0';
               prot->artist[0] = AAX_TRUE;
               prot->station = strdup(s);
            }

            s = _get_yaml(buf, "icy-description");
            if (s)
            {
               int len = _MIN(strlen(s)+1, MAX_ID_STRLEN);
               memcpy(prot->title+1, s, len);
               prot->title[len] = '\0';
               prot->title[0] = AAX_TRUE;
               prot->description = strdup(s);
            }

            s = _get_yaml(buf, "icy-genre");
            if (s) prot->genre = strdup(s);

            s = _get_yaml(buf, "icy-url");
            if (s) prot->website = strdup(s);
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
#if 0
 printf("server: %s\n", *server);
 printf("reponse: %i\n", res);
 printf("type: %s\n", prot->content_type);
 printf("artist: %s\n", prot->artist);
 printf("station: %s\n", prot->station);
 printf("title: %s\n", prot->title);
 printf("description: %s\n", prot->description);
 printf("genre: %s\n", prot->genre);
 printf("url: %s\n", prot->website);
 printf("inteval %li\n", prot->meta_interval);
#endif
      _aaxDataMove(buf, NULL, size);
   }
   else {
      res = -res;
   }

   return res;
}

/*
 * Read up to the meta-interval. If res > the remeaining bytes return the later.
 * Process the meta-data at the start of buf, remove it afterwards
 */
int
_http_process(_prot_t *prot, _data_t *buf)
{
   int rv = _aaxDataGetDataAvail(buf);
   if (prot->meta_interval)
   {
      // The first byte indicates the meta length devided by 16
      // Empty meta information is indicated by a meta_len of 0
      if (!prot->meta_offset)
      {
         char *metaptr = _aaxDataGetData(buf);
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

                  if (artist && end)
                  {
                     int len = _MIN(strlen(artist)+1, MAX_ID_STRLEN);
                     memcpy(prot->artist+1, artist, len);
                     prot->artist[len] = '\0';
                     prot->artist[0] = AAX_TRUE;
                  }
                  else if (prot->artist[1] != '\0')
                  {
                     prot->artist[1] = '\0';
                     prot->artist[0] = AAX_TRUE;
                  }

                  if (title && end)
                  {
                     int len = _MIN(strlen(title)+1, MAX_ID_STRLEN);
                     memcpy(prot->title+1, title, len);
                     prot->title[len] = '\0';
                     prot->title[0] = AAX_TRUE;
                  }
                  else if (prot->title[1] != '\0')
                  {
                     prot->title[1] = '\0';
                     prot->title[0] = AAX_TRUE;
                  }
                  prot->metadata_changed = AAX_TRUE;
               }
            }

            prot->meta_offset = prot->meta_interval;
            meta_len++; // meta_len itself is one byte
            rv -= meta_len;

            _aaxDataMove(buf, NULL, meta_len);
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
   if (prot && prot->content_type)
   {
      char *end = strchr(prot->content_type, ';');
      size_t len = end ? (end-prot->content_type) : strlen(prot->content_type);

      switch (ptype)
      {
      case __F_FMT:
         if (prot->content_type)
         {
            if (!strncasecmp(prot->content_type, "audio/mpeg", len)) {
               rv = _FMT_MP3;
            }
            else if (!strncasecmp(prot->content_type, "audio/flac", len)) {
               rv = _FMT_FLAC;
            }
            else if (!strncasecmp(prot->content_type, "audio/opus", len)) {
               rv = _FMT_OPUS;
            }
            else if (!strncasecmp(prot->content_type, "audio/vorbis", len)) {
               rv = _FMT_VORBIS;
            }
            else if (!strncasecmp(prot->content_type, "audio/speex", len)) {
               rv = _FMT_SPEEX;
            }
            else if (!strncasecmp(prot->content_type, "audio/wav", len) ||
                     !strncasecmp(prot->content_type, "audio/x-wav", len) ||
                     !strncasecmp(prot->content_type, "audio/x-pn-wav", len)) {
               rv = _EXT_WAV;
            }
            else if (!strncasecmp(prot->content_type, "audio/aiff", len) ||
                     !strncasecmp(prot->content_type, "audio/x-aiff", len) ||
                     !strncasecmp(prot->content_type, "audio/x-pn-aiff", len)) {
               rv = _EXT_AIFF;
            }
            else if (!strncasecmp(prot->content_type, "audio/basic", len) ||
                     !strncasecmp(prot->content_type, "audio/x-basic", len) ||
                     !strncasecmp(prot->content_type, "audio/x-pn-au", len)) {
               rv = _EXT_SND;
            }
            else if (!strncasecmp(prot->content_type, "audio/x-scpls", len) ||
                     !strncasecmp(prot->content_type, "audio/x-mpegurl", len) ||
                     !strncasecmp(prot->content_type, "audio/mpegurl", len) ||
               !strncasecmp(prot->content_type, "application/x-mpegurl", len) ||
               !strncasecmp(prot->content_type, "application/mpegurl", len) ||
               !strncasecmp(prot->content_type, "application/vnd.apple.mpegurl", len) ||
               !strncasecmp(prot->content_type, "application/vnd.apple.mpegurl.audio", len)) {
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
         if (prot->content_type)
         {
            if (!strncasecmp(prot->content_type, "audio/wav", len) ||
                !strncasecmp(prot->content_type, "audio/wave", len) ||
                !strncasecmp(prot->content_type, "audio/x-wav", len)) {
               rv = _EXT_WAV;
            }
            else if (!strncasecmp(prot->content_type, "audio/ogg", len) ||
                     !strncasecmp(prot->content_type, "application/ogg", len) ||
                     !strncasecmp(prot->content_type, "audio/x-ogg", len)) {
               rv = _EXT_OGG;
            }
            else if (!strncasecmp(prot->content_type, "audio/mpeg", len)) {
               rv = _EXT_MP3;
            }
            else if (!strncasecmp(prot->content_type, "audio/flac", len)) {
               rv = _EXT_FLAC;
            }
            else if (!strncasecmp(prot->content_type, "audio/x-scpls", len) ||
               !strncasecmp(prot->content_type, "audio/x-mpegurl", len) ||
               !strncasecmp(prot->content_type, "audio/mpegurl", len) ||
               !strncasecmp(prot->content_type, "application/x-mpegurl", len) ||
               !strncasecmp(prot->content_type, "application/mpegurl", len) ||
               !strncasecmp(prot->content_type, "application/vnd.apple.mpegurl", len) ||
               !strncasecmp(prot->content_type, "application/vnd.apple.mpegurl.audio", len)) {
               rv = _EXT_BYTESTREAM;
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
      if (prot->artist[1] != '\0')
      {
         ret = prot->artist+1;
         prot->artist[0] = AAX_FALSE;
      }
      break;
   case __F_TITLE:
      if (prot->title[1] != '\0')
      {
         ret = prot->title+1;
         prot->title[0] = AAX_FALSE;
      }
      break;
   case __F_GENRE:
      ret = prot->genre;
      break;
   case __F_ALBUM:
      ret = prot->description;
      break;
   case __F_COMPOSER:
      ret = prot->station;
      break;
   case __F_WEBSITE:
      ret = prot->website;
      break;
   default:
      switch (ptype & ~__F_NAME_CHANGED)
      {
      case (__F_ARTIST):
         if (prot->artist[0] == AAX_TRUE)
         {
            ret = prot->artist+1;
            prot->artist[0] = AAX_FALSE;
         }
         break;
      case (__F_TITLE):
         if (prot->title[0] == AAX_TRUE)
         {
            ret = prot->title+1;
            prot->title[0] = AAX_FALSE;
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
   char *response = _aaxDataGetData(databuf);
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
   _data_t *buf = _aaxDataCreate(MAX_HEADER, 0);
   char *header = _aaxDataGetData(buf);
   int hlen, rv = 0;

   snprintf(header, MAX_HEADER+1,
            "%s /%.256s HTTP/1.0\r\n"
            "User-Agent: %s\r\n"
            "Accept: */*\r\n"
            "Host: %s\r\n"
            "Connection: Keep-Alive\r\n"
#ifndef RELEASE
            "Icy-MetaData:1\r\n"
#endif
            "\r\n",
            command, path, user_agent, server);

   hlen = strlen(header);
   _aaxDataIncreaseOffset(buf, hlen);

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
      char *ptr = _aaxDataGetData(buf);
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
   const char *haystack = _aaxDataGetData(databuf);
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


