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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#include "audio.h"

size_t
_direct_connect(_prot_t *prot, UNUSED(_io_t *io), UNUSED(const char *server), const char *path, UNUSED(const char *agent))
{
   if (path) {
      prot->path = strdup(path);
   }
   return 0;
}

int
_direct_process(UNUSED(_prot_t *prot), UNUSED(uint8_t *buf), UNUSED(size_t res), UNUSED(size_t bytes_avail))
{
   return 0;
}

int
_direct_set(_prot_t *prot, enum _aaxStreamParam ptype, ssize_t param)
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
_direct_get(_prot_t *prot, enum _aaxStreamParam ptype)
{
   char *ext = prot->path ? strrchr(prot->path, '.') : NULL;
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

