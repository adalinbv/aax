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

void
MIDIStream::process(uint32_t time_pos)
{
    if (!eof() && (timestamp < time_pos))
    {
        uint8_t message = get_byte();
        if ((message & 0x80) == 0) {
           push_byte();
           message = previous;
        } else {
           previous = message;
        }

        // https://learn.sparkfun.com/tutorials/midi-tutorial/advanced-messages
        switch(message)
        {
        case 0xf0:	// system exclusive messages
        case 0xf7:
            uint8_t size = get_byte();
            forward(size);
            break;
        case 0xff:	// system messages
        {
            // http://mido.readthedocs.io/en/latest/meta_message_types.html
            uint8_t meta = get_byte();
            uint8_t size = get_byte();

            switch(meta)
            {
            case 0x00:	// sequence_number
            {
                uint8_t sequence = get_byte();
                break;
            }
            case 0x01:	// text
            case 0x02:  // copyright
            case 0x03:	// track_name
            case 0x04:	// instrument_name
            case 0x05:	// lyrics
            case 0x06:	// marker
            case 0x07:	// cue_marker
            case 0x09:	// device_name
                forward(size);			// not implemented yet
                // for (uint8_t i=0; i<size; ++i) printf("%c", get_byte());
                break;
            case 0x20:	// channel_prefix
                track = get_byte();
                break;
            case 0x2f:	// end_of_track
                forward();
                break;
            case 0x51:	// set_tempo
            {
                uint32_t tempo;
                tempo = (get_byte() << 16) | (get_byte() << 8) | get_byte();
                bpm = tempo2bpm(tempo);
                break;
            }
            case 0x54:	// smpte_offset
            {
                uint8_t frame_rate = get_byte();
                uint8_t hours = get_byte();
                uint8_t minutes = get_byte();
                uint8_t seconds = get_byte();
                uint8_t frames = get_byte();
                uint8_t sub_frames = get_byte();
                break;
            }
            case 0x58:	// time_signature
            {
                uint8_t numerator = get_byte();
                uint8_t denominator = get_byte();
                uint8_t clocks_per_click = get_byte();
                uint8_t notated_32nd_notes_per_beat = get_byte();
            }
            case 0x59:	// key_signature
            default:	// unsupported
                forward(size);
                break;
            }
        }
        default:
        {
            // https://learn.sparkfun.com/tutorials/midi-tutorial/messages
            uint8_t channel = message & 0xf;
            switch(message & 0xf0)
            {
            case 
            case 0x80:	// note off
            case 0x90:	// note on
            {
                uint8_t key = get_byte();
                uint8_t velocity = get_byte();
                // we now have the channel, the key and the velocity
                // and whether the note needs to sart or stop.
                break;
            }
            case 0xa0:	// polyphonic pressure
            {
                uint8_t key = get_byte();
                uint8_t pressure = get_byte();
                // we now have the channel, the key and the pressure
                break;
            }
            case 0xb0:	// control change
                uint8_t controller = get_byte();
                uint8_t value = get_byte();
                // we now have the channel, the controller and the new value
                break;
            case 0xc0:	// program change
                uint8_t program = get_byte();
                // we now have the channel and the program
                break;
            case 0xd0:	// channel pressure
                uint8_t pressure = get_byte();
                // we now have the channel and the pressure
                break;
            case 0xe0:	// pitch bend
            {
                uint16_t pitch = get_byte() << 7 | get_byte();
                // we now have the channel and the pitch
                break;
            }
            default:
                break;
            }
            break;
        }
    }
}


MIDIFile::MIDIFile(const char *filename) : MIDI()
{
    std::ifstream file(filename, std::ios::in|std::ios::binary|std::ios::ate);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    midi_data.reserve(size);
    if (midi_data.size() == size)
    {
        std::streamsize fileSize = size;
        if (file.read((char*)midi_data.data(), fileSize))
        {
            const MIDIBuffer map(midi.data(), size);
            const MIDIStream stream(map);

            try
            {
                uint32_t header = stream.get_long();
                uint16_t PPQN = 24;
                uint16_t track = 0;

                if (header == 0x8be09764) // "MThd"
                {
                    stream.forward(4); // skip the size;

                    format = stream.getWord();
                    no_tracks = stream.getWord();
                    PPQN = stream.getWord();
                }
                
                while (!stream.eof())
                {
                    header = stream.get_long();
                    if (header == 0xaae2e764) // "MTrk"
                    {
                        uint32_t size = stream.get_long();
                        tracks.push_back(new MidiStream(stream, track++, PPQN));
                        stream.forward(size);
                    }
                }
                no_tracks = track;

            } catch (const std::overflow_error& e) {
                std::cerr << "Error while processing the MIDI file: "
                          << e.what() << std::endl;
            }
        }
        else {
            std::cerr << "Error: Unable to open: " << filename << std::endl;
        }
    }
    else if (!midi_data.size()) {
        std::cerr << "Error: Out of memory." << std::endl;
    }
}

bool
MIDIFile::render(aax::AeonWave &aax)
{
   static const float dt = 1e-3f;
   _aaxTimer *timer = _aaxTimerCreate();
   _aaxTimerStartRepeatable(timer, dt*1000000);
   float time = 0.0f;

   for(;;)
   {
      for (unsigned int t=0; t<no_tracks; ++t) {
         tracks[t]->play(time);
      }
      time += dt;
      _aaxTimerWait(timer);
   }
   _aaxTimerDestroy(timer);

   return true;
}

#endif
