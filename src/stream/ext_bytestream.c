/*
 * Copyright 2005-2020 by Erik Hofman.
 * Copyright 2009-2020 by Adalin B.V.
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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#include "extension.h"
#include "format.h"

typedef struct
{
   _fmt_t *fmt;

   int mode;
   enum aaxFormat format;

} _driver_t;


int
_pls_detect(UNUSED(_ext_t *ext), UNUSED(int mode))
{
   return AAX_TRUE;
}

int
_pls_setup(_ext_t *ext, int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
{
   return AAX_TRUE;
}

void*
_pls_open(_ext_t *ext, void_ptr buf, ssize_t *bufsize, size_t fsize)
{
   _driver_t *handle = ext->id;
   return handle->fmt->open(handle->fmt, handle->mode, buf, bufsize, fsize);
}

int
_pls_close(_ext_t *ext)
{
   return  AAX_TRUE;
}

void*
_pls_update(_ext_t *ext, size_t *offs, ssize_t *size, char close)
{
   return NULL;
}

size_t
_pls_copy(_ext_t *ext, int32_ptr dptr, size_t offs, size_t *num)
{
   return 0;
}

size_t
_pls_fill(_ext_t *ext, void_ptr sptr, ssize_t *num)
{
   return 0;
}

size_t
_pls_cvt_from_intl(_ext_t *ext, int32_ptrptr dptr, size_t offset, size_t *num)
{
   return 0;
}

size_t
_pls_cvt_to_intl(_ext_t *ext, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   return 0;
}

int
_pls_set_name(_ext_t *ext, enum _aaxStreamParam param, const char *desc)
{
   return 0;
}

char*
_pls_name(_ext_t *ext, enum _aaxStreamParam param)
{
   return NULL;
}

char*
_pls_interfaces(int ext, int mode)
{
   static const char *raw_exts[_EXT_MAX - _EXT_PCM] = {
      "*.pls", "*.m3u", "*.m3u8"
   };
   static char *rd[2][_EXT_MAX - _EXT_PCM] = {
      { NULL, NULL, NULL },
      { NULL, NULL, NULL }
   };
   char *rv = NULL;

   if (ext == _EXT_BYTESTREAM)
   {
      int m = mode > 0 ? 1 : 0;
      int pos = ext - _EXT_PCM;

      if (rd[m][pos] == NULL)
      {
         int format = _FMT_MAX;
    
         switch(ext)
         {
         case _EXT_PCM:
//          format = _FMT_PCM;
            break;
         case _EXT_MP3:
            format = _FMT_MP3;
            break;
         case _EXT_FLAC:
            format = _FMT_FLAC;
            break;
         default:
            break;
         }

         _fmt_t *fmt = _fmt_create(format, m);
         if (fmt)
         {
            _fmt_free(fmt);
            rd[m][pos] = (char*)raw_exts[pos];
         }
      }
      rv = rd[mode][pos];
   }
   return rv;
}

int
_pls_extension(char *ext)
{
   return _FMT_NONE;
}

off_t
_pls_get(_ext_t *ext, int type)
{
   return 0;
}

off_t
_pls_set(_ext_t *ext, int type, off_t value)
{
   return 0;
}

