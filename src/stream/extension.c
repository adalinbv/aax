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

#include "extension.h"

_ext_t*
_ext_create(_ext_type_t extension)
{
   _ext_t *rv = NULL;
   switch(extension)
   {
   case _EXT_WAV:
      rv = calloc(1, sizeof(_ext_t));
      if (rv)
      {
         rv->id = NULL;
         rv->detect = _wav_detect;
         rv->setup = _wav_setup;
         rv->open = _wav_open;
         rv->close = _wav_close;
         rv->update = _wav_update;
         rv->name = _wav_name;

         rv->supported = _wav_extension;
         rv->interfaces = _wav_interfaces;

         rv->get_param = _wav_get;
         rv->set_param = _wav_set;

         rv->copy = _wav_copy;
         rv->fill = _wav_fill;
         rv->cvt_from_intl = _wav_cvt_from_intl;
         rv->cvt_to_intl = _wav_cvt_to_intl;
      }
      break;
   case _EXT_MP3:
   rv = calloc(1, sizeof(_ext_t));
      if (rv)
      {
         rv->id = NULL;
         rv->detect = _mp3_detect;
         rv->setup = _mp3_setup;
         rv->open = _mp3_open;
         rv->close = _mp3_close;
         rv->update = _mp3_update;
         rv->name = _mp3_name;

         rv->supported = _mp3_extension;
         rv->interfaces = _mp3_interfaces;

         rv->get_param = _mp3_get;
         rv->set_param = _mp3_set;

         rv->copy = _mp3_copy;
         rv->fill = _mp3_fill;
         rv->cvt_from_intl = _mp3_cvt_from_intl;
         rv->cvt_to_intl = _mp3_cvt_to_intl;
      }
      break;
   case _EXT_OGG:
   case _EXT_FLAC:
   default:
      break;
   }
   return rv;
}

void*
_ext_free(_ext_t *ext)
{
   free(ext);
   return NULL;
}

