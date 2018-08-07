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

#ifndef __AAX_MIDI
#define __AAX_MIDI

#include <aax/aeonwave.hpp>
#include <aax/instrument.hpp>

#include <buffer_map.hpp>
#include <byte_stream.hpp>

#define MIDI_SEQUENCE_NUMBER		0x00
#define MIDI_TEXT			0x01
#define MIDI_COPYRIGHT			0x02
#define MIDI_TRACK_NAME			0x03
#define MIDI_INSTRUMENT_NAME		0x04
#define MIDI_LYRICS			0x05
#define MIDI_MARKER			0x06
#define MIDI_CUE_MARKER			0x07
#define MIDI_DEVICE_NAME		0x09
#define MIDI_CHANNEL_PREFIX		0x20
#define MIDI_END_OF_TRACK		0x2f
#define MIDI_SET_TEMPO			0x51
#define MIDI_SAMPLE_OFFSET		0x54
#define MIDI_TIME_SIGNATURE		0x58
#define MIDI_KEY_SIGNATURE		0x59

#define MIDI_NOTE_OFF			0x80
#define MIDI_NOTE_ON			0x90
#define MIDI_POLYPHONIC_PRESSURE	0xa0
#define MIDI_CONTROL_CHANGE		0xb0
#define MIDI_PROGRAM_CHANGE		0xc0
#define MIDI_CHANNEL_PRESSURE		0xd0
#define MIDI_PITCH_BEND			0xe0

#define MIDI_SYSTEM_MESSAGE		0xff


typedef buffer_map<uint8_t> MIDIBuffer;

class MIDIStream : public byte_stream
{
public:
    MIDIStream(const byte_stream& stream, uint16_t channel, uint16_t ppqn)
        : track(stream.map()), track_no(channel), PPQN(ppqn)
    {
        timestamp = get_message();
    }

    void process(uint32_t timestamp);

private:
    uint32_t get_message();

    inline uint16_t tempo2bpm(uint32_t tempo) {
        return (60 * 1000000 / tempo);
    }
    inline uint32_t bpm2tempo(uint16_t bpm) {
        return (60 * 1000000 / bpm);
    }

    const MIDIBuffer track;
    uint32_t timestamp = 0;
    uint16_t track_no = 0;
    uint16_t PPQN = 24;
    uint16_t QN = 24;
    uint16_t bpm = 120;
    uint8_t previous = 0;
    bool poly = true;
    bool omni = false;
}

class MIDIFile
{
public:
    MIDIFile() = default;

    MIDIFile(const char *filename);

    MIDIFile(std::string& filename) : MIDI(filename.c_str());

    ~MIDIFile() = default;

    inline operator bool() {
        return midi_data.size();
    }

    bool render(aax::AeonWave &aax);

private:
    std::vector<uint8_t> midi_data;
    std::vector<MIDITrack*> track;

    uint32_t time_pos = 0;
    uint16_t no_tracks = 0;
    uint16_t format = 0;
};

#endif
