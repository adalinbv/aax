/*
 * Copyright 2005-2016 by Erik Hofman.
 * Copyright 2009-2016 by Adalin B.V.
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

#include <strings.h>

#include "extension.h"
#include "format.h"

typedef struct
{
   void *id;

   _fmt_t *fmt;

   int mode;
   enum aaxFormat format;

#ifdef WINXP
#if 0
   HACMSTREAM acmStream;
   ACMSTREAMHEADER acmStreamHeader;

   LPBYTE pcmBuffer;
   unsigned long pcmBufPos;
   unsigned long pcmBufMax;
#endif
#endif

} _driver_t;


int
_mp3_detect(_ext_t *ext, int mode)
{
   _fmt_t *fmt = _fmt_create(_FMT_MP3, mode);
   int rv = AAX_FALSE;

   if (fmt)
   {
      _driver_t *handle = calloc(1, sizeof(_driver_t));
      if (handle)
      {
         handle->fmt = fmt;
         handle->mode = mode;
         ext->id = handle;
         rv = AAX_TRUE;
      }
      else {
         _AAX_FILEDRVLOG("MP3: Insufficient memory");
      }
   }
   return rv;
}

int
_mp3_setup(_ext_t *ext, int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
{
   _driver_t *handle = ext->id;
   unsigned bits;

   bits = aaxGetBitsPerSample(format);
   handle->format = format;

   handle->fmt->set(handle->fmt, __F_FREQ, freq);
   handle->fmt->set(handle->fmt, __F_RATE, bitrate);
   handle->fmt->set(handle->fmt, __F_TRACKS, tracks);
   handle->fmt->set(handle->fmt, __F_SAMPLES, no_samples);
   handle->fmt->set(handle->fmt, __F_BITS, bits);

   *bufsize = 0;
   if (handle->mode == 0) {
      *bufsize = (no_samples*tracks*bits)/8;
   }

   return AAX_TRUE;
}

void*
_mp3_open(_ext_t *ext, void_ptr buf, size_t *bufsize, size_t fsize)
{
   _driver_t *handle = ext->id;
   return handle->fmt->open(handle->fmt, buf, bufsize, fsize);

}

int
_mp3_close(_ext_t *ext)
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
_mp3_update(_ext_t *ext, size_t *offs, size_t *size, char close)
{
// _driver_t *handle = ext->id;
   void *rv = NULL;
   return rv;
}

size_t
_mp3_copy(_ext_t *ext, int32_ptr dptr, size_t offs, size_t num)
{
   _driver_t *handle = ext->id;
   return handle->fmt->copy(handle->fmt, dptr, offs, NULL, 0, 0, &num);
}

size_t
_mp3_process(_ext_t *ext, void_ptr sptr, size_t num)
{
   _driver_t *handle = ext->id;
   return handle->fmt->process(handle->fmt, NULL, sptr, 0, num, 0);
}

size_t
_mp3_cvt_from_intl(_ext_t *ext, int32_ptrptr dptr, size_t offset, size_t num)
{
   _driver_t *handle = ext->id;
   return handle->fmt->cvt_from_intl(handle->fmt, dptr, offset, 0, 0, 0, &num);
}

size_t
_mp3_cvt_to_intl(_ext_t *ext, void_ptr dptr, const_int32_ptrptr sptr, size_t offs, size_t num, void_ptr scratch, size_t scratchlen)
{
   _driver_t *handle = ext->id;
   return handle->fmt->cvt_to_intl(handle->fmt, dptr, sptr, offs, num, 0, scratch, scratchlen);
}

char*
_mp3_name(_ext_t *ext, enum _aaxStreamParam param)
{
   _driver_t *handle = ext->id;
   return handle->fmt->name(handle->fmt, param);
}

char*
_mp3_interfaces(int mode)
{
   static const char *rd[2] = { "*.mp3\0", "*.mp3\0" };
   return (char *)rd[mode];
}

int
_mp3_extension(char *ext)
{
   return (ext && !strcasecmp(ext, "mp3")) ? 1 : 0;
}

off_t
_mp3_get(_ext_t *ext, int type)
{
   _driver_t *handle = ext->id;
   return handle->fmt->get(handle->fmt, type);
}

off_t
_mp3_set(_ext_t *ext, int type, off_t value)
{
   _driver_t *handle = ext->id;
   return handle->fmt->set(handle->fmt, type, value);
}

