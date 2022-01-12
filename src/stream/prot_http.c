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
static int _http_get_response(_io_t*, char*, int*);
static const char *_get_yaml(const char*, const char*, size_t);


ssize_t
_http_connect(_prot_t *prot, _io_t *io, char **server, const char *path, const char *agent)
{
   int res = _http_send_request(io, "GET", *server, path, agent);

   if (path)
   {
      if (prot->path) free(prot->path);
      prot->path = strdup(path);
   }

#if 0
  printf("GET: res: %i\n server: '%s'\n path: '%s'\n agent: '%s'\n", res, *server, path, agent);
#endif

   if (res > 0)
   {
      int max = 4096;
      char buf[4096];

      res = _http_get_response(io, buf, &max);
      if (res >= 200 && res < 300)
      {
         const char *s;

         res = 0;
         s = _get_yaml(buf, "content-length", max);
         if (s)
         {
            prot->no_bytes = strtol(s, NULL, 10);
            res = prot->no_bytes;
         }

         s = _get_yaml(buf, "content-type", max);
         if (s) {
            prot->content_type = strdup(s);
         }
         if (s && _http_get(prot, __F_EXTENSION) != _EXT_NONE)
         {
            s = _get_yaml(buf, "icy-name", max);
            if (s)
            {
               int len = _MIN(strlen(s)+1, MAX_ID_STRLEN);
               memcpy(prot->artist+1, s, len);
               prot->artist[len] = '\0';
               prot->artist[0] = AAX_TRUE;
               prot->station = strdup(s);
            }

            s = _get_yaml(buf, "icy-description", max);
            if (s)
            {
               int len = _MIN(strlen(s)+1, MAX_ID_STRLEN);
               memcpy(prot->title+1, s, len);
               prot->title[len] = '\0';
               prot->title[0] = AAX_TRUE;
               prot->description = strdup(s);
            }

            s = _get_yaml(buf, "icy-genre", max);
            if (s) prot->genre = strdup(s);

            s = _get_yaml(buf, "icy-url", max);
            if (s) prot->website = strdup(s);
            s = _get_yaml(buf, "icy-metaint", max);
            if (s)
            {
               errno = 0;
               prot->meta_interval = strtol(s, NULL, 10);
               prot->meta_pos = 0;
               if (errno == ERANGE) {
                  _AAX_SYSLOG("stream meta out of range");
               }
            }
         }
      }
      else
      {
         res = -res;
         if (res >= 300 && res < 400) // Moved
         {
            *server = (char*)_get_yaml(buf, "Location", max);
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
 printf("type: %s\n", prot->content_type);
 printf("artist: %s\n", prot->artist);
 printf("station: %s\n", prot->station);
 printf("title: %s\n", prot->title);
 printf("description: %s\n", prot->description);
 printf("genre: %s\n", prot->genre);
 printf("url: %s\n", prot->website);
 printf("inteval %li\n", prot->meta_interval);
#endif
   }
   else {
      res = -res;
   }
   return res;
}

int
_http_process(_prot_t *prot, _data_t *buf, size_t res)
{
   size_t buffer_avail = _aaxDataGetDataAvail(buf);
   unsigned int meta_len = 0;

   if (prot->meta_interval)
   {
      prot->meta_pos += res;
      while (prot->meta_pos >= prot->meta_interval)
      {
         ssize_t meta_offs, block_len;
         ssize_t offset = 0;
         uint8_t msize = 0;

         meta_offs = prot->meta_pos % prot->meta_interval;
         offset += buffer_avail;
         offset -= meta_offs;

         // The first byte indicates the meta length devided by 16
         // Empty meta information is indicated by a meta_len of 0
         _aaxDataCopy(buf, &msize, offset, 1);
         prot->meta_size = meta_len = msize*16;
         if (offset+meta_len >= buffer_avail) break;

         if (meta_len > 0)
         {
            char *metaptr = prot->metadata;

            if (meta_len > prot->metadata_len)
            {
               size_t len = SIZE_ALIGNED(meta_len+1);
               if ((metaptr = realloc(prot->metadata, len)) == NULL) {
                  break;
               }

               prot->metadata = metaptr;
               prot->metadata_len = len;
            }

            if (!_aaxDataMoveOffset(buf, metaptr, offset, meta_len+1)) {
               break;
            }
            metaptr = prot->metadata+1;

            // meta_len > block_len means it's not an empty stream title.
            // So we now have a continuous block of memory containing the
            // stream title data.
            block_len = HEADERLEN+1;
            if (meta_len > block_len &&
                !strncasecmp((char*)metaptr, STREAMTITLE, block_len-1))
            {
               char *artist = (char*)metaptr + HEADERLEN;
               if (artist)
               {
                  char *title = strnstr(artist, " - ", meta_len);
                  char *end = strnstr(artist, "\';", meta_len);
                  if (!end) end = strnstr(artist, "\'\0", meta_len);
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
                  prot->meta_size -= meta_len;
                  prot->metadata_changed = AAX_TRUE;
               }
            }
         } // if (meta_len > 0)
         else {
            _aaxDataMoveOffset(buf, NULL, offset, 1);
         }

         meta_len++;     // add the meta_len-byte itself
         prot->meta_pos -= (prot->meta_interval+meta_len);

      } // while (prot->meta_pos >= prot->meta_interval)
   }
   return meta_len;
}

int
_http_set(_prot_t *prot, enum _aaxStreamParam ptype, ssize_t param)
{
   int rv = -1;
   switch (ptype)
   {
   case __F_POSITION:
      prot->meta_pos += param;
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
_http_get_response_data(_io_t *io, char *response, int size)
{
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
         res = io->read(io, buf, 1);
         if (res > 0) break;
         msecSleep(10);
      }
      while (res == 0 && --j);

      if (res == 1)
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
   char header[MAX_HEADER];
   int hlen, rv = 0;

   snprintf(header, MAX_HEADER,
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
   header[MAX_HEADER-1] = '\0';

   hlen = strlen(header);
   rv = io->write(io, header, hlen);
   if (rv != hlen) {
      rv = -1;
   }

   return rv;
}

int
_http_get_response(_io_t *io, char *buf, int *size)
{
   int res, rv = -1;

   res = _http_get_response_data(io, buf, *size);
   *size = res;
   if (res > 0)
   {
      res = sscanf(buf, "HTTP/1.%*d %03d", (int*)&rv);
      if (res != 1)
      {
         res =  sscanf(buf, "ICY %03d", (int*)&rv);
         if (res != 1) {
            rv = -1;
         }
      }
   }

   return rv;
}

static const char*
_get_yaml(const char *haystack, const char *needle, size_t haystacklen)
{
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


