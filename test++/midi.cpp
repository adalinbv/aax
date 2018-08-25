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

#include <assert.h>
#include <xml.h>

#include <base/timer.h>
#include "midi.hpp"

using namespace aax;

MIDIChannel&
MIDI::new_channel(uint8_t channel_no, uint8_t bank_no, uint8_t program_no)
{
    auto it = channels.find(channel_no);
    if (it != channels.end())
    {
        AeonWave::remove(*it->second);
        channels.erase(it);
    }

    try {
        auto ret = channels.insert({channel_no, new MIDIChannel(*this, path, instr, drum, channel_no, bank_no, program_no)});
        it = ret.first;
        AeonWave::add(*it->second);
    } catch(const std::invalid_argument& e) {
        throw;
    }
    return *it->second;
}

MIDIChannel&
MIDI::channel(uint8_t channel_no)
{
    auto it = channels.find(channel_no);
    if (it == channels.end()) {
        return new_channel(channel_no, 0, 0);
    }
    return *it->second;
}

bool
MIDI::process(uint8_t channel_no, uint8_t message, uint8_t key, uint8_t velocity, bool omni)
{
    if (message == MIDI_NOTE_ON && velocity)
    {
        if (omni) {
            for (auto& it : channels) {
                it.second->play(key, velocity);
            }
        } else {
            channel(channel_no).play(key, velocity);
        }
    }
    else
    {
        if (omni) {
            for (auto& it : channels) {
                it.second->stop(key);
            }
        } else {
            channel(channel_no).stop(key);
        }
    }
    return true;
}


std::string
MIDIChannel::get_name_from_xml(std::string& file, const char* type, uint8_t bank_no, uint8_t program_no)
{
    void *xid = xmlOpen(file.c_str());
    if (xid)
    {
        void *xaid = xmlNodeGet(xid, "aeonwave/midi");
        char file[64] = "";
        if (xaid)
        {
            unsigned int bnum = xmlNodeGetNum(xaid, "bank");
            void *xbid = xmlMarkId(xaid);
            for (unsigned int b=0; b<bnum; b++)
            {
                if (xmlNodeGetPos(xaid, xbid, "bank", b) != 0)
                {
                    long int n = xmlAttributeGetInt(xbid, "n");
                    if (n == bank_no)
                    {
                        unsigned int inum=xmlNodeGetNum(xbid, type);
                        void *xiid = xmlMarkId(xbid);
                        for (unsigned int i=0; i<inum; i++)
                        {
                            if (xmlNodeGetPos(xbid, xiid, type, i) != 0)
                            {
                                long int n = xmlAttributeGetInt(xiid, "n");
                                if (n == program_no)
                                {
                                    unsigned int slen;

                                    slen = xmlAttributeCopyString(xiid, "file", file, 64);
                                    if (slen) {
                                        file[slen] = 0;
                                    }
                                    break;
                                }
                            }
                        }
                        xmlFree(xiid);
                    }
                    break;
                }
            }
            xmlFree(xbid);
            xmlFree(xaid);
        }
        else {
            std::cerr << "aeonwave/midi not found in: " << file << std::endl;
        }
        xmlClose(xid);

#if 1
 printf("Loading: %s\n", file);
#endif
        if (file[0] != 0) {
            return file;
        }
    }
    else {
        std::cerr << "Unable to open: " << file << std::endl;
    }
    return ""; // "instruments/piano-acoustic"
}

std::string
MIDIChannel::get_name(uint8_t channel, uint8_t bank_no, uint8_t program_no)
{
    if (channel == MIDI_DRUMS_CHANNEL) {
        return get_name_from_xml(drum, "drum", bank_no, program_no);
    } else {
        return get_name_from_xml(instr, "instrument", bank_no, program_no);
    }
}

void
MIDIChannel::play(uint8_t key_no, uint8_t velocity)
{
    assert (velocity);

    auto it = name_map.begin();
    if (channel_no == MIDI_DRUMS_CHANNEL)
    {
        it = name_map.find(key_no);
        if (it == name_map.end())
        {
            std::string name = get_name(channel_no, bank_no, key_no);
            Buffer &buffer = midi.buffer(name, true);
            if (buffer)
            {
                auto ret = name_map.insert({key_no,buffer});
                it = ret.first;
            }
        }
    }
    else
    {
        if (it == name_map.end())
        {
            std::string name = get_name(channel_no, bank_no, program_no);
            Buffer &buffer = midi.buffer(name, true);
            if (buffer)
            {
                auto ret = name_map.insert({program_no,buffer});
                it = ret.first;
            }
        }
    }

    if (it != name_map.end()) {
        Instrument::play(key_no, velocity, it->second, is_drums);
    } else {
//      throw(std::invalid_argument("Instrument file "+name+" not found"));
    }
}


uint32_t
MIDITrack::pull_message()
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

void
MIDITrack::registered_param(uint8_t channel, uint8_t controller, uint8_t value)
{
    bool msb_sent = false, lsb_sent = false;
    uint16_t msb_type = 0, lsb_type = 0;
    uint16_t msb = 0, lsb = 0;
    uint8_t next = 0;
    uint16_t type;

#if 0
 printf("%x %x %x ", 0xb0|channel, controller, value);
 uint8_t *p = (uint8_t*)*this;
 p += offset();
 for (int i=0; i<20; ++i) printf("%x ", p[i]);
 printf("\n");
#endif

    if (controller == MIDI_REGISTERED_PARAM_COARSE)
    {
        msb_type = value << 7 || pull_byte();
        msb_sent = true;
        next = pull_byte();
        if (next == 0x00) next = pull_byte();
        if ((next & 0xf0) == MIDI_CONTROL_CHANGE) {
            next = pull_byte();
        }
        if (next == MIDI_REGISTERED_PARAM_FINE)
        {
            lsb_type = pull_byte() << 7 || pull_byte();
            lsb_sent = true;
            next = pull_byte();
        }
    }
    else if (controller == MIDI_REGISTERED_PARAM_FINE)
    {
        lsb_type = value << 7 || pull_byte();
        lsb_sent = true;
        next = pull_byte();
        if (next == 0x00) next = pull_byte();
        if ((next & 0xf0) == MIDI_CONTROL_CHANGE) {
            next = pull_byte();
        }
        if (next == MIDI_REGISTERED_PARAM_COARSE)
        {
            msb_type = pull_byte() << 7 || pull_byte();
            msb_sent = true;
            next = pull_byte();
        }
    }

    type = msb << 7 | lsb;
    if (msb_sent && lsb_sent && type == MIDI_PITCH_BEND_RANGE)
    {
        if (next == 0x00) next = pull_byte();
        if ((next & 0xf0) == MIDI_CONTROL_CHANGE) {
            next = pull_byte();
        }
        if (next == (MIDI_DATA_ENTRY|MIDI_COARSE))
        {
            msb = pull_byte();
            next = pull_byte();
        }
        else if (next == (MIDI_DATA_ENTRY|MIDI_FINE))
        {
            lsb = pull_byte();
            next = pull_byte();
        }
        else {
            push_byte();
        }

        if (next == 0x00) next = pull_byte();
        if ((next & 0xf0) == MIDI_CONTROL_CHANGE) {
            next = pull_byte();
        }
        if (next == (MIDI_DATA_ENTRY|MIDI_COARSE))
        {
            msb = pull_byte();
        }
        else if (next == (MIDI_DATA_ENTRY|MIDI_FINE))
        {
            lsb = pull_byte();
        }
        else {
            push_byte();
        }
        midi.channel(channel).set_semi_tones((float)lsb + (float)msb/100.0f);
    }

}

bool
MIDITrack::process(uint64_t time_offs_us)
{
    bool rv = !eof();

    if (eof()) return rv;

    while (!eof() && (timestamp_us <= time_offs_us))
    {
        uint32_t message = pull_byte();
        if ((message & 0x80) == 0) {
           push_byte();
           message = previous;
        } else {
           previous = message;
        }

        rv = true;
        switch(message)
        {
        case MIDI_EXCLUSIVE_MESSAGE:
        {
            uint8_t size = pull_byte();
            uint8_t byte = pull_byte();
            // GM1 reset: F0 7E 7F 09 01 F7
            // GM2 reset: F0 7E 7F 09 03 F7
            // GS  reset: F0 41 10 42 12 40 00 7F 00 41 F7
            if (byte == 0x7e && pull_byte() == 0x7f && pull_byte() == 0x09)
            {
                if (pull_byte() == 0x01) {
                    std::cout << "General MIDI 1.0" << std::endl;
                } else if (pull_byte() == 0x03) {
                    std::cout << "General MIDI 2.0" << std::endl;
                }
            }
            else if (byte == 0x41 && pull_byte() == 0x10 &&
                       pull_byte() == 0x42 && pull_byte() == 0x12 &&
                       pull_byte() == 0x40 && pull_byte() == 0x00 &&
                       pull_byte() == 0x7f && pull_byte() == 0x00 &&
                       pull_byte() == 0x41)
            {
                std::cout << "General Standard" << std::endl;
            }
            else push_byte();
            while (pull_byte() != MIDI_EXCLUSIVE_MESSAGE_END);
            break;
        }
        case MIDI_SYSTEM_MESSAGE:
        {
            uint8_t meta = pull_byte();
            uint8_t size = pull_byte();
            switch(meta)
            {
            case MIDI_TEXT:
            case MIDI_COPYRIGHT:
            case MIDI_TRACK_NAME:
            case MIDI_INSTRUMENT_NAME:
            case MIDI_LYRICS:
            case MIDI_MARKER:
            case MIDI_CUE_POINT:
            case MIDI_DEVICE_NAME:
                forward(size);			// not implemented yet
                break;
            case MIDI_CHANNEL_PREFIX:
                channel_no = (channel_no & 0xF0) | pull_byte();
                break;
            case MIDI_PORT_PREFERENCE:
                channel_no = (channel_no & 0xF) | pull_byte() << 8;
                break;
            case MIDI_END_OF_TRACK:
                forward();
                break;
            case MIDI_SET_TEMPO:
            {
                uSPP = (pull_byte() << 16) | (pull_byte() << 8) | pull_byte();
                uSPP /= PPQN;
                break;
            }
            case MIDI_SEQUENCE_NUMBER:
            case MIDI_TIME_SIGNATURE: 
            case MIDI_SMPTE_OFFSET:
            case MIDI_KEY_SIGNATURE:
            default:	// unsupported
                forward(size);
                break;
            }
        }
        default:
        {
            uint8_t channel = message & 0xf;
            switch(message & 0xf0)
            {
            case MIDI_NOTE_OFF:
            case MIDI_NOTE_ON:
            {
                uint8_t key = pull_byte();
                uint8_t velocity = pull_byte();
                 midi.process(channel, message & 0xf0, key, velocity, omni);
                break;
            }
            case MIDI_POLYPHONIC_PRESSURE:
            {
                uint8_t key = pull_byte();
                uint8_t pressure = pull_byte();
                midi.channel(channel).set_pressure(key, pressure);
                break;
            }
            case MIDI_CONTROL_CHANGE:
            {
                // http://www.lim.di.unimi.it/IEEE/MIDI/SOT5.HTM#Further
                uint8_t controller = pull_byte();
                uint8_t value = pull_byte();
                switch(controller)
                {
                case MIDI_ALL_SOUND_OFF:
                case MIDI_MONO_ALL_NOTES_OFF:
                case MIDI_POLY_ALL_NOTES_OFF:
                    midi.process(channel, MIDI_NOTE_OFF, 0, 0, true);
                    break;
                case MIDI_OMNI_OFF:
                    midi.process(channel, MIDI_NOTE_OFF, 0, 0, true);
                    omni = false;
                    break;
                case MIDI_OMNI_ON:
                    midi.process(channel, MIDI_NOTE_OFF, 0, 0, true);
                    omni = true;
                    break;
                case MIDI_CHANNEL_VOLUME:
                    midi.channel(channel).set_gain((float)value/127.0f);
                    break;
                case MIDI_REGISTERED_PARAM_COARSE:
                case MIDI_REGISTERED_PARAM_FINE:
                {
                    registered_param(channel, controller, value);
                    break;
                }
                case MIDI_SOFT_PEDAL:
                case MIDI_HOLD_PEDAL1:
                case MIDI_HOLD_PEDAL2:
                    midi.channel(channel).set_hold(value >= 64);
                    break;
                case MIDI_SOSTENUTO_PEDAL:
                    midi.channel(channel).set_sustain(value >= 64);
                    break;
                default:
                    break;
                }
                break;
            }
            case MIDI_PROGRAM_CHANGE:
            {
                uint8_t program_no = pull_byte();
                try {
                    midi.new_channel(channel, bank_no, program_no);
                } catch(const std::invalid_argument& e) {
                    std::cerr << "Error: " << e.what() << std::endl;
                }
                break;
            }
            case MIDI_CHANNEL_PRESSURE:
            {
                uint8_t pressure = pull_byte();
                midi.channel(channel).set_pressure((float)pressure/127.0f);
                break;
            }
            case MIDI_PITCH_BEND:
            {
                uint16_t pitch = pull_byte() << 7 | pull_byte();
                float semi_tones = midi.channel(channel).get_semi_tones();
                float p = semi_tones*((float)pitch-8192);
                if (p < 0) p /= 8192.0f;
                else p /= 8191.0f;
                midi.channel(channel).set_pitch(powf(2.0f, p/12.0f));
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
                    break;
                }
                break;
            default:
                break;
            }
            break;
        } // switch
        } // default

        if (!eof())
        {
            uint32_t parts = pull_message();
            if (parts > 0)
            {
                timestamp_us += parts*uSPP;
                break;
            }
        }
    }

    return rv;
}


MIDIFile::MIDIFile(const char *devname, const char *filename) : MIDI(devname)
{
    std::ifstream file(filename, std::ios::in|std::ios::binary|std::ios::ate);
    ssize_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size > 0)
    {
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
                    uint32_t size, header = stream.pull_long();
                    uint16_t track_no = 0;

                    if (header == 0x4d546864) // "MThd"
                    {
                        size = stream.pull_long();
                        if (size != 6) return;

                        format = stream.pull_word();
                        if (format != 0 && format != 1) return;

                        no_tracks = stream.pull_word();
                        if (format == 0 && no_tracks != 1) return;

                        PPQN = stream.pull_word();
                        if (PPQN & 0x8000)
                        {
                            uint8_t fps = (PPQN >> 8) & 0xff;
                            uint8_t resolution = PPQN & 0xff;
                            if (fps == 232) fps = 24;
                            else if (fps == 231) fps = 25;
                            else if (fps == 227) fps = 29;
                            else if (fps == 226) fps = 30;
                            else fps = 0;
                            PPQN = fps*resolution;
                        }
                    }
                
                    while (!stream.eof())
                    {
                        header = stream.pull_long();
                        if (header == 0x4d54726b) // "MTrk"
                        {
                            uint32_t length = stream.pull_long();
                            track.push_back(new MIDITrack(*this, stream, length, track_no++, PPQN));
                            stream.forward(length);
                        }
                    }
                    no_tracks = track_no;

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
    else {
        std::cerr << "Error: Unable to open: " << filename << std::endl;
    }
}

bool
MIDIFile::process(uint32_t time_ms)
{
    bool rv = false;
    for (size_t t=0; t<no_tracks; ++t) {
        rv |= track[t]->process(1000*time_ms);
    }
    return rv;
}

