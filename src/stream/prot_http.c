/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#define MAX_BUFFER	512

static int _http_send_request(_io_t*, const char*, const char*, const char*, const char*);
static int _http_get_response(_io_t*, char*, int*);
static const char *_get_yaml(const char*, const char*, size_t);


size_t
_http_connect(_prot_t *prot, _io_t *io, const char *server, const char *path, const char *agent)
{
   int res = _http_send_request(io, "GET", server, path, agent);

   if (path) {
      prot->path = strdup(path);
   }

#if 0
  printf("GET:\n server: '%s'\n path: '%s'\n agent: '%s'\n res: %i\n", server, path, agent, res);
#endif

   if (res > 0)
   {
      int max = 4096;
      char buf[4096];

      res = _http_get_response(io, buf, &max);
      if (res == 200)
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
   }
   else {
      res = -res;
   }
   return res;
}

int
_http_process(_prot_t *prot, uint8_t *buf, size_t res, size_t bytes_avail)
{
   unsigned int slen = 0;

   if (prot->meta_interval)
   {
      prot->meta_pos += res;
      while (prot->meta_pos > prot->meta_interval)
      {
         size_t offs = prot->meta_pos - prot->meta_interval;
         uint8_t *ptr = buf;
         size_t blen;

         ptr += bytes_avail;
         ptr -= offs;

         slen = *ptr * 16;
         if ((size_t)(ptr+slen) >= (size_t)(buf+IOBUF_THRESHOLD)) {
            break;
         }

         blen = strlen("StreamTitle=''");
         if (slen > blen && !strncasecmp((char*)ptr+1, "StreamTitle='", blen-1))
         {
            char *artist = (char*)ptr+1 + strlen("StreamTitle='");
            if (artist)
            {
               char *title = strnstr(artist, " - ", slen);
               char *end = strnstr(artist, "\';", slen);
               if (!end) end = strnstr(artist, "\'\0", slen);
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

         slen++;     // add the slen-byte itself
         prot->meta_pos -= (prot->meta_interval+slen);

         /* move the rest of the buffer slen-bytes back */
         bytes_avail -= slen;
         blen = bytes_avail;
         blen -= (ptr - buf);

         memmove(ptr, ptr+slen, blen);
      }
   }
   return slen;
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
   int rv = -1;

   switch (ptype)
   {
   case __F_FMT:
      if (prot->content_type)
      {
         if (!strcasecmp(prot->content_type, "audio/mpeg")) {
            rv = _FMT_MP3;
         }
         else if (!strcasecmp(prot->content_type, "audio/flac")) {
            rv = _FMT_FLAC;
         }
         else if (!strcasecmp(prot->content_type, "audio/opus")) {
            rv = _FMT_OPUS;
         }
         else if (!strcasecmp(prot->content_type, "audio/vorbis")) {
            rv = _FMT_VORBIS;
         }
         else if (!strcasecmp(prot->content_type, "audio/speex")) {
            rv = _FMT_SPEEX;
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
         if (!strcasecmp(prot->content_type, "audio/wav") ||
             !strcasecmp(prot->content_type, "audio/wave") ||
             !strcasecmp(prot->content_type, "audio/x-wav")) {
            rv = _EXT_WAV;
         }
         else if (!strcasecmp(prot->content_type, "audio/ogg") ||
                  !strcasecmp(prot->content_type, "application/ogg") ||
                  !strcasecmp(prot->content_type, "audio/x-ogg")) {
            rv = _EXT_OGG;
         }
         else if (!strcasecmp(prot->content_type, "audio/mpeg")) {
            rv = _EXT_MP3;
         }
         else if (!strcasecmp(prot->content_type, "audio/flac")) {
            rv = _EXT_FLAC;
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

      j = 10;
      do
      {
         res = io->read(io, buf, 1);
         if (res > 0) break;
         msecSleep(1);
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
   char header[MAX_BUFFER];
   int hlen, rv = 0;

   snprintf(header, MAX_BUFFER,
            "%s /%.256s HTTP/1.0\r\n"
            "User-Agent: %s\r\n"
            "Accept: */*\r\n"
            "Host: %s\r\n"
            "Connection: Keep-Alive\r\n"
            "Icy-MetaData:1\r\n"
            "\r\n",
            command, path, user_agent, server);
   header[MAX_BUFFER-1] = '\0';

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
   static char buf[64];
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

         if ((end-start) > 63) {
            end = start + 63;
         }
         memcpy(buf, start, (end-start));
         buf[end-start] = '\0';
      }
   }

   return (buf[0] != '\0') ? buf : NULL;
}


