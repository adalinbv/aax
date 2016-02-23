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

#include "format.h"

_fmt_t*
_fmt_create(_fmt_type_t format)
{
   _fmt_t *rv = NULL;
   switch(format)
   {
   case _FMT_PCM:
      rv = malloc(sizeof(_fmt_t));
      if (rv)
      {
         rv->id = NULL;
         rv->setup = _pcm_setup;
         rv->close = _pcm_close;

         rv->cvt_to_signed = _pcm_cvt_to_signed;
         rv->cvt_from_signed = _pcm_cvt_from_signed;
         rv->cvt_endianness = _pcm_cvt_endianness;
         rv->cvt_to_intl = _pcm_cvt_to_intl;
         rv->cvt_from_intl = _pcm_cvt_from_intl;
         rv->process = _pcm_process;
         rv->copy = _pcm_copy;

         rv->set = _pcm_set;
      }
      break;
   case _FMT_MP3:
   case _FMT_VORBIS:
   case _FMT_FLAC:
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

