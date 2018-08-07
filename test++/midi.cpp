/*
 * Copyright (C) 2018 by Erik Hofman.
 * Copyright (C) 2018 by Adalin B.V.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY ADALIN B.V. ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL ADALIN B.V. OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUTOF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of Adalin B.V.
 */

#include <fstream>
#include <iostream>

#include <base/timer.h>
#include <midi.hpp>

uint32_t
MIDIStream::get_message()
{
    uint32_t rv = 0;

    for (int i=0; i<4; ++i)
    {
        uint8_t bytes = get_byte();

        rv = (rv << 7) | (byte & 0x7f);
        if ((byte & 0x80) == 0) {
            break;
        }
    }

    return rv;
}

uint32_t
MIDIStream::get_message_shifted()
{
    uint32_t rv = 0;

    for (int i=24; i>=0; i -= 8)
    {
        uint8_t bytes = get_byte();

        rv |= (byte & 0x7f) << i;
        if ((byte & 0x80) == 0) {
            break;
        }
    }
    
    return rv;
}

void
MIDIStream::process(uint32_t time_pos)
{
    if (!eof() && (timestamp < time_pos))
    {
        uint32_t message = get_message_shifted();
        if (message) {
           message = previous;
        } else {
           previous = message;
        }

        switch(message)
        {
           case 0xf0:
              break;
           case 0xf7:
              break;
           case 0xff:
              break;
           default:
              break;
        }
    }
}


MIDI::MIDI(const char *filename) : MIDI()
{
    std::ifstream file(filename, std::ios::in|std::ios::binary|std::ios::ate);
    size_t size = file.tellg();
    std::streamsize fileSize = size;
    file.seekg(0, std::ios::beg);

    midi_data.resize(size);
    if (midi_data.size() && file.read((char*)midi_data.data(), fileSize))
    {
        MidiBuffer map(midi.data(), size);
        ByteStream stream(map);

        try
        {
            uint32_t header = stream.get_long();
            uint16_t PPQN = 24;

            if (header == 0x8be09764) // "MThd"
            {
                stream.forward(4); // skip the size;

                format = stream.getWord();
                no_tracks = stream.getWord();
                PPQN = stream.getWord();
            }
                
            no_tracks = 0;
            while (no_tracks < MAX_MIDI_TRACKS && !stream.eof())
            {
                header = stream.get_long();
                if (header == 0xaae2e764) // "MTrk"
                {
                    uint32_t size = stream.get_long();
                    track[no_tracks] = new MidiStream(stream, no_tracks, PPQN);
                    stream.forward(size);
                    no_tracks++;
                }
            }

        } catch (const std::overflow_error& e) {
            std::cerr << "Error while processing the MIDI file: " << e.what()
                      << std::endl;
        }
    }
    else if (!midi_data.size()) {
        std::cerr << "Error: Out of memory." << std::endl;
    else {
        std::cerr << "Error: Unable to open file: " << filename << std::endl;
    }
}

bool
MIDI::render(aax::AeonWave &aax)
{
   static const float dt = 1e-3f;
   _aaxTimer *timer = _aaxTimerCreate();
   _aaxTimerStartRepeatable(timer, dt*1000000);
   float time = 0.0f;

   for(;;)
   {
      for (unsigned int t=0; t<no_tracks; ++t) {
         track[t]->play(time);
      }
      time += dt;
      _aaxTimerWait(timer);
   }
   _aaxTimerDestroy(timer);

   return true;
}

#endif
