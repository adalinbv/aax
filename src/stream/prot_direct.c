/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#include "extension.h"
#include "protocol.h"
#include "format.h"
#include "io.h"

size_t
_direct_connect(_prot_t *prot, VOID(_io_t *io), VOID(const char *server), const char *path, VOID(const char *agent))
{
   prot->path = strdup(path);
   return 0;
}

int
_direct_process(VOID(_prot_t *prot), VOID(uint8_t *buf), VOID(size_t res), VOID(size_t bytes_avail))
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
   char *ext = strrchr(prot->path, '.');
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
_direct_name(VOID(_prot_t *prot), VOID(enum _aaxStreamParam ptype))
{
   return NULL;
}

