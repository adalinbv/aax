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

#include "format.h"

_fmt_t*
_fmt_create(_fmt_type_t format, int mode)
{
   _fmt_t *rv = NULL;
   switch(format)
   {
   case _FMT_PCM:
      rv = calloc(1, sizeof(_fmt_t));
      if (rv)
      {
         if (_pcm_detect(rv, mode))
         {
            rv->setup = _pcm_setup;
            rv->open = _pcm_open;
            rv->close = _pcm_close;
            rv->name = _pcm_name;

            rv->cvt_to_signed = _pcm_cvt_to_signed;
            rv->cvt_from_signed = _pcm_cvt_from_signed;
            rv->cvt_endianness = _pcm_cvt_endianness;
            rv->cvt_to_intl = _pcm_cvt_to_intl;
            rv->cvt_from_intl = _pcm_cvt_from_intl;
            rv->fill = _pcm_fill;
            rv->copy = _pcm_copy;

            rv->set = _pcm_set;
            rv->get = _pcm_get;
         }
         else
         {
            free(rv);
            rv = NULL;
         }
      }
      break;
   case _FMT_MP3:
      rv = calloc(1, sizeof(_fmt_t));
      if (rv)
      {
         if (_mp3_detect(rv, mode))
         {
            rv->setup = _mp3_setup;
            rv->open = _mp3_open;
            rv->close = _mp3_close;
            rv->name = _mp3_name;

            rv->cvt_to_intl = _mp3_cvt_to_intl;
            rv->cvt_from_intl = _mp3_cvt_from_intl;
            rv->fill = _mp3_fill;
            rv->copy = _mp3_copy;

            rv->set = _mp3_set;
            rv->get = _mp3_get;
         }
#ifdef WINXP
//       else if (_aaxMSACMDetect(ext, mode)) {}
#endif
         else
         {
            free(rv);
            rv = NULL;
         }
      }
      break;
   case _FMT_OPUS:
      rv = calloc(1, sizeof(_fmt_t));
      if (rv)
      {
         if (_opus_detect(rv, mode))
         {
            rv->setup = _opus_setup;
            rv->open = _opus_open;
            rv->close = _opus_close;
            rv->name = _opus_name;

            rv->cvt_to_intl = _opus_cvt_to_intl;
            rv->cvt_from_intl = _opus_cvt_from_intl;
            rv->fill = _opus_fill;
            rv->copy = _opus_copy;

            rv->set = _opus_set;
            rv->get = _opus_get;
         }
         else
         {
            free(rv);
            rv = NULL;
         }
      }
      break;
   case _FMT_VORBIS:
      rv = calloc(1, sizeof(_fmt_t));
      if (rv)
      {
         if (_vorbis_detect(rv, mode))
         {
            rv->setup = _vorbis_setup;
            rv->open = _vorbis_open;
            rv->close = _vorbis_close;
            rv->name = _vorbis_name;

            rv->cvt_to_intl = _vorbis_cvt_to_intl;
            rv->cvt_from_intl = _vorbis_cvt_from_intl;
            rv->fill = _vorbis_fill;
            rv->copy = _vorbis_copy;

            rv->set = _vorbis_set;
            rv->get = _vorbis_get;
         }
         else
         {
            free(rv);
            rv = NULL;
         }
      }
      break;
   case _FMT_FLAC:
#if 0
      rv = calloc(1, sizeof(_fmt_t));
      if (rv)
      {
         if (_flac_detect(rv, mode))
         {
            rv->setup = _flac_setup;
            rv->open = _flac_open;
            rv->close = _flac_close;
            rv->name = _flac_name;

            rv->cvt_to_intl = _flac_cvt_to_intl;
            rv->cvt_from_intl = _flac_cvt_from_intl;
            rv->fill = _flac_fill;
            rv->copy = _flac_copy;

            rv->set = _flac_set;
            rv->get = _flac_get;
         }
         else
         {
            free(rv);
            rv = NULL;
         }
      }
#endif
      break;
   case _FMT_AAXS:
      rv = calloc(1, sizeof(_fmt_t));
      if (rv)
      {
         if (_binary_detect(rv, mode))
         {
            rv->setup = _binary_setup;
            rv->open = _binary_open;
            rv->close = _binary_close;
            rv->name = _binary_name;

            rv->cvt_to_intl = _binary_cvt_to_intl;
            rv->cvt_from_intl = _binary_cvt_from_intl;
            rv->fill = _binary_fill;
            rv->copy = _binary_copy;

            rv->set = _binary_set;
            rv->get = _binary_get;
         }
         else
         {
            free(rv);
            rv = NULL;
         }
      }
      break;
   case _FMT_SPEEX:
   default:
      break;
   }
   return rv;
}

void*
_fmt_free(_fmt_t *fmt)
{
   free(fmt);
   return NULL;
}

