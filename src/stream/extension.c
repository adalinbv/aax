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
   case _EXT_OGG:
   case _EXT_OPUS:
      rv = calloc(1, sizeof(_ext_t));
      if (rv)
      {
         rv->id = NULL;
         rv->detect = _ogg_detect;
         rv->setup = _ogg_setup;
         rv->open = _ogg_open;
         rv->close = _ogg_close;
         rv->update = _ogg_update;
         rv->name = _ogg_name;

         rv->supported = _ogg_extension;
         rv->interfaces = _ogg_interfaces;

         rv->get_param = _ogg_get;
         rv->set_param = _ogg_set;

         rv->copy = _ogg_copy;
         rv->fill = _ogg_fill;
         rv->cvt_from_intl = _ogg_cvt_from_intl;
         rv->cvt_to_intl = _ogg_cvt_to_intl;
      }
      break;
   case _EXT_PCM:
   case _EXT_MP3:
   case _EXT_FLAC:
      rv = calloc(1, sizeof(_ext_t));
      if (rv)
      {
         rv->id = NULL;
         rv->detect = _raw_detect;
         rv->setup = _raw_setup;
         rv->open = _raw_open;
         rv->close = _raw_close;
         rv->update = _raw_update;
         rv->name = _raw_name;

         rv->supported = _raw_extension;
         rv->interfaces = _raw_interfaces;

         rv->get_param = _raw_get;
         rv->set_param = _raw_set;

         rv->copy = _raw_copy;
         rv->fill = _raw_fill;
         rv->cvt_from_intl = _raw_cvt_from_intl;
         rv->cvt_to_intl = _raw_cvt_to_intl;
      }
      break;
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

