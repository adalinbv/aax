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

#define MAX_MIDI_TRACKS		64

typedef buffer_map<uint8_t> MIDIBuffer;

class MIDIStream : public byte_stream
{
public:
    MIDIStream(const ByteStream& stream, uint16_t channel, uint16_t ppqn)
        : track(stream.map()), track_no(channel), PPQN(ppqn), omni(false)
    {
        timestamp = get_message();
    }

    void process(uint32_t timestamp);

private:
    uint32_t get_message();
    uint32_t get_message_shifted();

    const MIDIBuffer& track;
    uint32_t timestamp;
    uint16_t track_no;
    uint16_t PPQN;
    uint16_t QN;
    uint16_t bpm;
    uint8_t previous;
    bool poly;
    bool omni;
}

class MIDIFile
{
public:
    MIDI() : time_pos(0), no_tracks(0), format(0) {}

    MIDI(const char *filename);

    MIDI(std::string& filename) : MIDI(filename.c_str());

    ~MIDI() = default;

    inline operator bool() {
        return midi_data.size();
    }

    bool render(aax::AeonWave &aax);

private:
    std::vector<uint8_t> midi_data;

    MIDITrack* track[MAX_MIDI_TRACKS];
    uint32_t time_pos;
    uint16_t no_tracks;
    uint16_t format;
};

#endif
