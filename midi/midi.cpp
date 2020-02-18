/*
 * Copyright 2018-2020 by Erik Hofman.
 * Copyright 2018-2020 by Adalin B.V.
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

#include <aax/midi.h>

#include "aax_midi.hpp"

using namespace aax::MIDI;

#if defined(__cplusplus)
extern "C" {
#endif


AAX_API aaxMidi AAX_APIENTRY
aaxMidiCreate(aaxConfig config)
{
   aax::AeonWave handle(config);
   Stream* stream = new Stream(handle);

   return (aaxMidi)stream;
}

AAX_API int AAX_APIENTRY
aaxMidiDestroy(aaxMidi handle)
{
   Stream* stream = (Stream*)handle;
   delete stream;

   return AAX_TRUE;
}

AAX_API int AAX_APIENTRY
aaxMidiSetSetup(aaxMidi handle, enum aaxMidiSetupType type, uint32_t value)
{
   Stream* stream = (Stream*)handle;
   int rv = AAX_FALSE;
   switch (type)
   {
   case AAX_MIDI_LISTEN_MASK:
      rv = stream->set_mask(value) ? AAX_TRUE : AAX_FALSE;
      break;
   default:
      break;
   }
   return rv;
}

AAX_API unsigned int AAX_APIENTRY
aaxMidiGetSetup(aaxMidi handle, enum aaxMidiSetupType type)
{
   Stream* stream = (Stream*)handle;
   (void)stream;
   return AAX_FALSE;
}

AAX_API int AAX_APIENTRY
aaxMidiSetState(aaxMidi handle, enum aaxState state)
{
   Stream* stream = (Stream*)handle;
   int rv = AAX_TRUE;
   switch (state)
   {
   case AAX_PLAYING:
      stream->start();
      break;
   case AAX_STOPPED:
   case AAX_PROCESSED:
      stream->stop();
      break;
   default:
      rv = AAX_FALSE;
      break;
   }
   return rv;
}

AAX_API enum aaxState
AAX_APIENTRY aaxMidiGetState(aaxMidi handle)
{
   Stream* stream = (Stream*)handle;
   return stream->state();
}

AAX_API int AAX_APIENTRY
aaxMidiPushMessage(aaxMidi handle, uint32_t message)
{
   Stream* stream = (Stream*)handle;
   stream->push(message);
   return AAX_TRUE;
}

#if defined(__cplusplus)
}  /* extern "C" */
#endif
