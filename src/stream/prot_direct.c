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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#include "io.h"

ssize_t
_direct_connect(_prot_t *prot, UNUSED(_data_t *buf), UNUSED(_io_t *io), UNUSED(char **server), const char *path, UNUSED(const char *agent))
{
   if (path) {
      prot->meta.comments = strdup(path);
   }
   return 0;
}

void
_direct_disconnect(UNUSED(_prot_t *prot)) {
}

int
_direct_process(UNUSED(_prot_t *prot), _data_t *buf)
{
   return _aaxDataGetDataAvail(buf, 0);
}

int
_direct_set(UNUSED(_prot_t *prot), enum _aaxStreamParam ptype, UNUSED(ssize_t param))
{
   int rv = -1;
   switch (ptype)
   {
   case __F_POSITION:
      rv = 0;
      break;
   default:
      break;
   }
   return rv;
}

int
_direct_get(_prot_t *prot, enum _aaxStreamParam ptype)
{
   char *ext = prot->meta.comments ? strrchr(prot->meta.comments, '.') : NULL;
   int rv = -1;

   if (ext++)
   {
      switch (ptype)
      {
      case __F_FMT:
         if (!strcasecmp(ext, "pcm")) {
            rv = _FMT_PCM;
         }
         else if (!strcasecmp(ext, "mp3")) {
            rv = _FMT_MP3;
         }
         else if (!strcasecmp(ext, "flac")) {
            rv = _FMT_FLAC;
         }
         else if (!strcasecmp(ext, "m3u") || !strcasecmp(ext, "m3u8") ||
                                              !strcasecmp(ext, "pls")) {
            rv = _FMT_PLAYLIST;
         }
         else {
            rv = _FMT_NONE;
         }
         break;
      case __F_EXTENSION:
         if (!strcasecmp(ext, "wav")) {
            rv = _EXT_WAV;
         }
         else if (!strcasecmp(ext, "ogg") || !strcasecmp(ext, "oga") ||
                  !strcasecmp(ext, "ogx") || !strcasecmp(ext, "spx") ||
                  !strcasecmp(ext, "opus")) {
            rv = _EXT_OGG;
         }
         else if (!strcasecmp(ext, "pcm")) {
            rv = _EXT_PCM;
         }
         else if (!strcasecmp(ext, "mp3")) {
            rv = _EXT_MP3;
         }
         else if (!strcasecmp(ext, "flac")) {
            rv = _EXT_FLAC;
         }
         else if (!strcasecmp(ext, "m3u") || !strcasecmp(ext, "m3u8") ||
                                              !strcasecmp(ext, "pls")) {
            rv = _EXT_BYTESTREAM;
         }
         else {
            rv = _EXT_NONE;
         }
         break;
      default:
         break;
      }
   }
   return rv;
}

char*
_direct_name(UNUSED(_prot_t *prot), UNUSED(enum _aaxStreamParam ptype))
{
   return NULL;
}

