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
#include "format.h"

typedef struct
{
   void *id;

   _fmt_t *fmt;

   int mode;
   enum aaxFormat format;

} _driver_t;


int
_raw_detect(VOID(_ext_t *ext), VOID(int mode))
{
   return AAX_TRUE;
}

int
_raw_setup(_ext_t *ext, int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
{
   _driver_t *handle;
   int rv = AAX_FALSE;

   handle = calloc(1, sizeof(_driver_t));
   if (handle)
   {
      _fmt_t *fmt;

      handle->mode = mode;
      ext->id = handle;

      fmt = _fmt_create(format, handle->mode);
      if (fmt)
      {
         unsigned bits = aaxGetBitsPerSample(format);
         handle->format = format;
         handle->fmt = fmt;

         handle->fmt->set(handle->fmt, __F_FREQUENCY, freq);
         handle->fmt->set(handle->fmt, __F_RATE, bitrate);
         handle->fmt->set(handle->fmt, __F_TRACKS, tracks);
         handle->fmt->set(handle->fmt, __F_NO_SAMPLES, no_samples);
         handle->fmt->set(handle->fmt, __F_BITS_PER_SAMPLE, bits);

         *bufsize = 0;
         if (handle->mode == 0) {
            *bufsize = (no_samples*tracks*bits)/8;
         }
         rv = AAX_TRUE;
      }  
   }
   else {
      _AAX_FILEDRVLOG("RAW: Insufficient memory");
   }

   return rv;
}

void*
_raw_open(_ext_t *ext, void_ptr buf, size_t *bufsize, size_t fsize)
{
   _driver_t *handle = ext->id;
   return handle->fmt->open(handle->fmt, buf, bufsize, fsize);

}

int
_raw_close(_ext_t *ext)
{
   _driver_t *handle = ext->id;
   int res = AAX_TRUE;

   if (handle)
   {
      if (handle->fmt)
      {
         handle->fmt->close(handle->fmt);
         handle->fmt = _fmt_free(handle->fmt);
      }
      free(handle);
   }

   return res;
}

void*
_raw_update(VOID(_ext_t *ext), VOID(size_t *offs), VOID(size_t *size), VOID(char close))
{
   return NULL;
}

size_t
_raw_copy(_ext_t *ext, int32_ptr dptr, size_t offs, size_t *num)
{
   _driver_t *handle = ext->id;
   return handle->fmt->copy(handle->fmt, dptr, offs, num);
}

size_t
_raw_fill(_ext_t *ext, void_ptr sptr, size_t *num)
{
   _driver_t *handle = ext->id;
   return handle->fmt->fill(handle->fmt, sptr, num);
}

size_t
_raw_cvt_from_intl(_ext_t *ext, int32_ptrptr dptr, size_t offset, size_t *num)
{
   _driver_t *handle = ext->id;
   return handle->fmt->cvt_from_intl(handle->fmt, dptr, offset, num);
}

size_t
_raw_cvt_to_intl(_ext_t *ext, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t *num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = ext->id;
   return handle->fmt->cvt_to_intl(handle->fmt, dptr, sptr, offs, num, scratch, scratchlen);
}

char*
_raw_name(_ext_t *ext, enum _aaxStreamParam param)
{
   _driver_t *handle = ext->id;
   return handle->fmt->name(handle->fmt, param);
}

char*
_raw_interfaces(int ext, int mode)
{
   static const char *raw_exts[_EXT_MAX - _EXT_PCM] = {
      "*.pcm", "*.mp3", "*.flac"
   };
   static char *rd[2][_EXT_MAX - _EXT_PCM] = {
      { NULL, NULL, NULL },
      { NULL, NULL, NULL }
   };
   char *rv = NULL;

   if (ext >= _EXT_PCM && ext < _EXT_MAX)
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
_raw_extension(char *ext)
{
   int rv = _FMT_NONE;

   if (ext)
   {
      if (!strcasecmp(ext, "mp3")) rv = _FMT_MP3;
      else if (!strcasecmp(ext, "flac")) rv = _FMT_FLAC;
      else if (!strcasecmp(ext, "pcm") || !strcasecmp(ext, "raw")) rv =_FMT_PCM;
   }
   return rv;
}

off_t
_raw_get(_ext_t *ext, int type)
{
   _driver_t *handle = ext->id;
   return handle->fmt->get(handle->fmt, type);
}

off_t
_raw_set(_ext_t *ext, int type, off_t value)
{
   _driver_t *handle = ext->id;
   return handle->fmt->set(handle->fmt, type, value);
}

