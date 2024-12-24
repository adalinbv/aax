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
         rv->flush = _wav_flush;
         rv->update = _wav_update;
         rv->name = _wav_name;
         rv->set_name = _wav_set_name;

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
   case _EXT_AIFF:
      rv = calloc(1, sizeof(_ext_t));
      if (rv)
      {
         rv->id = NULL;
         rv->detect = _aiff_detect;
         rv->setup = _aiff_setup;
         rv->open = _aiff_open;
         rv->close = _aiff_close;
         rv->flush = _aiff_flush;
         rv->update = _aiff_update;
         rv->name = _aiff_name;
         rv->set_name = _aiff_set_name;

         rv->supported = _aiff_extension;
         rv->interfaces = _aiff_interfaces;

         rv->get_param = _aiff_get;
         rv->set_param = _aiff_set;

         rv->copy = _aiff_copy;
         rv->fill = _aiff_fill;
         rv->cvt_from_intl = _aiff_cvt_from_intl;
         rv->cvt_to_intl = _aiff_cvt_to_intl;
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
         rv->set_name = _ogg_set_name;

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
    case _EXT_SND:
      rv = calloc(1, sizeof(_ext_t));
      if (rv)
      {
         rv->id = NULL;
         rv->detect = _snd_detect;
         rv->setup = _snd_setup;
         rv->open = _snd_open;
         rv->close = _snd_close;
         rv->flush = _snd_flush;
         rv->update = _snd_update;
         rv->name = _snd_name;
         rv->set_name = _snd_set_name;

         rv->supported = _snd_extension;
         rv->interfaces = _snd_interfaces;

         rv->get_param = _snd_get;
         rv->set_param = _snd_set;

         rv->copy = _snd_copy;
         rv->fill = _snd_fill;
         rv->cvt_from_intl = _snd_cvt_from_intl;
         rv->cvt_to_intl = _snd_cvt_to_intl;
      }
      break;
    case _EXT_PATCH:
      rv = calloc(1, sizeof(_ext_t));
      if (rv)
      {
         rv->id = NULL;
         rv->detect = _pat_detect;
         rv->setup = _pat_setup;
         rv->open = _pat_open;
         rv->close = _pat_close;
         rv->update = _pat_update;
         rv->name = _pat_name;
         rv->set_name = _pat_set_name;

         rv->supported = _pat_extension;
         rv->interfaces = _pat_interfaces;

         rv->get_param = _pat_get;
         rv->set_param = _pat_set;

         rv->copy = _pat_copy;
         rv->fill = _pat_fill;
         rv->cvt_from_intl = _pat_cvt_from_intl;
         rv->cvt_to_intl = _pat_cvt_to_intl;
      }
      break;
   case _EXT_BYTESTREAM:
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
         rv->set_name = _raw_set_name;

         rv->supported = _raw_extension;
         rv->interfaces = _raw_interfaces;

         rv->get_param = _raw_get;
         rv->set_param = _raw_set;

         rv->copy = _raw_copy;
         rv->fill = _raw_fill;
         rv->cvt_from_intl = _raw_cvt_from_intl;
         rv->cvt_to_intl = _raw_cvt_to_intl;
         rv->cvt_to_intl_float = _raw_cvt_to_intl_float;
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
   if (ext) free(ext);
   return NULL;
}

