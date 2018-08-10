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
#include "midi.hpp"

#define LOG	1

uint32_t
MIDIStream::pull_message()
{
    uint32_t rv = 0;

    for (int i=0; i<4; ++i)
    {
        uint8_t byte = pull_byte();

        rv = (rv << 7) | (byte & 0x7f);
        if ((byte & 0x80) == 0) {
            break;
        }
    }

    return rv;
}

bool
MIDIStream::process(uint32_t time_pos)
{
    bool rv = !eof();

    while (!eof() && (timestamp <= time_pos))
    {
        uint8_t data, message = pull_byte();

        if ((message & 0x80) == 0) {
           push_byte();
           message = previous;
        } else {
           previous = message;
        }

        rv = true;

#if LOG
 printf("%02i ", channel);
 printf("%08i ms ", time_pos);
 printf("0x%02x ", message);
#endif

        // https://learn.sparkfun.com/tutorials/midi-tutorial/advanced-messages
        switch(message)
        {
//      case MIDI_SEQUENCE_NUMBER:
//      {
//          uint8_t sequence_no = pull_byte();
//      }
        case MIDI_EXCLUSIVE_MESSAGE:
        case MIDI_EXCLUSIVE_MESSAGE_END:
        {
            uint8_t size = pull_byte();
            forward(size);
            break;
        }
        case MIDI_SYSTEM_MESSAGE:
        {
            // http://mido.readthedocs.io/en/latest/meta_message_types.html
            uint8_t meta = pull_byte();
            uint8_t size = pull_byte();
#if LOG
 printf("  meta: 0x%02x", meta);
 printf("  length: 0x%02x", size);
#endif
            switch(meta)
            {
            case MIDI_SEQUENCE_NUMBER:
            {
                uint8_t sequence = pull_byte();
                break;
            }
            case MIDI_TEXT:
            case MIDI_COPYRIGHT:
            case MIDI_TRACK_NAME:
            case MIDI_INSTRUMENT_NAME:
            case MIDI_LYRICS:
            case MIDI_MARKER:
            case MIDI_CUE_POINT:
            case MIDI_DEVICE_NAME:
#if LOG
                printf("  ");
                for (uint8_t i=0; i<size; ++i) printf("%c", pull_byte());
#else
                forward(size);			// not implemented yet
#endif
                break;
            case MIDI_CHANNEL_PREFIX:
                channel = (channel & 0xff00) | pull_byte();
                break;
            case MIDI_PORT_PREFERENCE:
                channel = (channel & 0xff) | pull_byte() << 8;
                break;
            case MIDI_END_OF_TRACK:
                forward();
                break;
            case MIDI_SET_TEMPO:
            {
                uint32_t tempo;
                tempo = (pull_byte() << 16) | (pull_byte() << 8) | pull_byte();
                bpm = tempo2bpm(tempo);
#if LOG
 printf("  tempo: %i bpm", bpm);
#endif
                break;
            }
            case MIDI_SMPTE_OFFSET:
            {
//              uint8_t frame_rate = pull_byte();
                uint8_t hours = pull_byte();
                uint8_t minutes = pull_byte();
                uint8_t seconds = pull_byte();
                uint8_t frames = pull_byte();
                uint8_t sub_frames = pull_byte();
                break;
            }
            case MIDI_TIME_SIGNATURE:
            {
                uint8_t numerator = pull_byte();
                uint32_t denominator = 1 << pull_byte();
                uint8_t clocks_per_click = pull_byte();
                uint8_t notated_32nd_notes_per_beat = pull_byte();
  
                QN = 100000/clocks_per_click;
#if LOG
  printf("  time sig: %i/%i MIDI clocks per quarter-dotted %i",
              numerator, denominator, clocks_per_click);
#endif
              break;
            }
            case MIDI_KEY_SIGNATURE:
            {
                uint8_t sharps = pull_byte();
                uint8_t major = pull_byte();
                break;
            }
            default:	// unsupported
                forward(size);
#if LOG
 printf("  unknown meta: 0x%02x", meta);
#endif
                break;
            }
        }
        default:
        {
            // https://learn.sparkfun.com/tutorials/midi-tutorial/messages
            uint8_t channel = message & 0xf;
            switch(message & 0xf0)
            {
            case MIDI_NOTE_OFF:
            case MIDI_NOTE_ON:
            {
                uint8_t key = pull_byte();
                uint8_t velocity = pull_byte();
                // we now have the channel, the key and the velocity
                // and whether the note needs to sart or stop.
#if LOG
 const char *notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
 printf("  ac: %c ch: %i note: ", ((message >> 4) == 8) ? '^' : 'v', channel);
 printf("%s%i", notes[key % 12], (key / 12)-1);
#endif
                break;
            }
            case MIDI_POLYPHONIC_PRESSURE:
            {
                uint8_t key = pull_byte();
                uint8_t pressure = pull_byte();
                // we now have the channel, the key and the pressure
                break;
            }
            case MIDI_CONTROL_CHANGE:
            {
                uint8_t controller = pull_byte();
                uint8_t value = pull_byte();
                // we now have the channel, the controller and the new value
                break;
            }
            case MIDI_PROGRAM_CHANGE:
            {
                uint8_t program = pull_byte();
                // we now have the channel and the program
                break;
            }
            case MIDI_CHANNEL_PRESSURE:
            {
                uint8_t pressure = pull_byte();
                // we now have the channel and the pressure
                break;
            }
            case MIDI_PITCH_BEND:
            {
                uint16_t pitch = pull_byte() << 7 | pull_byte();
                // we now have the channel and the pitch
                break;
            }
            case MIDI_SYSTEM:
                switch(channel)
                {
                case MIDI_TIMING_CLOCK:
                case MIDI_START:
                case MIDI_CONTINUE:
                case MIDI_STOP:
                case MIDI_ACTIVE_SENSE:
                case MIDI_SYSTEM_RESET:
                    break;
                default:
#if LOG
 printf("unknown real-time message: 0x%02x", channel);
#endif
                    break;
                }
                break;
            default:
#if LOG
 printf("  unknown message: 0x%02x", message);
#endif
                break;
            }
            break;
        } // switch
        } // default

        if (!eof())
        {
            float c = (float)(bpm * PPQN) / 60.0f;
            uint32_t dT = pull_message();
            if (c > 0) {
                timestamp += 1000*dT / c;
            }
        }
#if LOG
        printf("\n");
#endif
    }

    return rv;
}

MIDIFile::MIDIFile(const char *filename)
{
    std::ifstream file(filename, std::ios::in|std::ios::binary|std::ios::ate);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    midi_data.reserve(size);
    if (midi_data.capacity() == size)
    {
        std::streamsize fileSize = size;
        if (file.read((char*)midi_data.data(), fileSize))
        {
            buffer_map<uint8_t> map(midi_data.data(), size);
            byte_stream stream(map);

            try
            {
                uint32_t header = stream.pull_long();
                uint16_t PPQN = 24;
                uint16_t track = 0;

                if (header == 0x4d546864) // "MThd"
                {
                    stream.forward(4); // skip the size;

                    format = stream.pull_word();
                    no_channels = stream.pull_word();
                    PPQN = stream.pull_word();
                }
                
                while (!stream.eof())
                {
                    header = stream.pull_long();
                    if (header == 0x4d54726b) // "MTrk"
                    {
                        uint32_t length = stream.pull_long();
                        channel.push_back(new MIDIStream(stream, length, track++, PPQN));
                        stream.forward(length);
                    }
                }
                no_channels = track;

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
MIDIFile::process(uint32_t time)
{
    bool rv = false;
    for (size_t t=0; t<no_channels; ++t) {
        rv |= channel[t]->process(time);
    }
    return rv;
}

