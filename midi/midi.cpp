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

#include <limits.h>
#include <assert.h>
#include <xml.h>

#include <base/timer.h>
#include "midi.hpp"

#define DISPLAY(...)	if(midi.get_initialize()) printf(__VA_ARGS__)
#define MESSAGE(...)	if(midi.get_verbose()) printf(__VA_ARGS__)
#ifndef NDEBUG
# define LOG(...)	printf(__VA_ARGS__)
#else
# define LOG
#endif

using namespace aax;

void
MIDI::rewind()
{
    channels.clear();
    uSPP = 500000/PPQN;
}

void
MIDI::read_instruments()
{
    const char *filename, *type = "instrument";
    auto map = instruments;

    std::string iname = path;
    iname.append("/");
    iname.append(instr);

    filename = iname.c_str();
    for(unsigned int i=0; i<2; ++i)
    {
        void *xid = xmlOpen(filename);
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
                        long int bank_no = xmlAttributeGetInt(xbid, "n");
                        unsigned int inum = xmlNodeGetNum(xbid, type);
                        void *xiid = xmlMarkId(xbid);

                        std::map<uint8_t,std::string> bank;
                        for (unsigned int i=0; i<inum; i++)
                        {
                            if (xmlNodeGetPos(xbid, xiid, type, i) != 0)
                            {
                                long int n = xmlAttributeGetInt(xiid, "n");
                                unsigned int slen;

                                slen = xmlAttributeCopyString(xiid, "file", file, 64);
                                if (slen)
                                {
                                    file[slen] = 0;
                                    std::string inst(file);
                                    bank.insert({n,inst});
// pre-cache:                       AeonWave::buffer(inst, true);
                                }
                            }
                        }
                        map.insert({bank_no,bank});
                        xmlFree(xiid);
                    }
                }
                xmlFree(xbid);
                xmlFree(xaid);
            }
            else {
                std::cerr << "aeonwave/midi not found in: " << filename << std::endl;
            }
            xmlClose(xid);
        }
        else {
            std::cerr << "Unable to open: " << filename << std::endl;
        }

        if (i == 0)
        {
            instruments = map;

            iname = path;
            iname.append("/");
            iname.append(drum);
            filename = iname.c_str();
            type = "drum";
            map = drums;
        }
        else {
            drums = map;
        }
    }
}

std::string
MIDI::get_drum(uint8_t bank_no, uint8_t program_no)
{
    auto itb = drums.find(bank_no);
    if (itb == drums.end() && bank_no > 0) {
        itb = drums.find(0);
    }

    if (itb != drums.end())
    {
        do
        {
            auto bank = itb->second;
            auto iti = bank.find(program_no);
            if (iti != bank.end()) {
                return iti->second;
            }

            if (bank_no > 0) {
                itb = drums.find(0);
            } else {
                break;
            }
        }
        while (bank_no > 0);
    }
    return empty_str;
}

std::string
MIDI::get_instrument(uint8_t bank_no, uint8_t program_no)
{
    auto itb = instruments.find(bank_no);
    if (itb == instruments.end() && bank_no > 0) {
        itb = instruments.find(0);
    }

    if (itb != instruments.end())
    {
        do
        {
            auto bank = itb->second;
            auto iti = bank.find(program_no);
            if (iti != bank.end()) {
                return iti->second;
            }

            if (bank_no > 0) {
                itb = instruments.find(0);
            } else {
                break;
            }
        }
        while (bank_no > 0);
    }
    return empty_str;
}

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
            std::string name = midi.get_drum(program_no, key_no);
            if (!name.empty())
            {
                DISPLAY("Loading drum       bank: %3i, key    : %3i: %s\n",
                         program_no, key_no, name.c_str());
                Buffer &buffer = midi.buffer(name, true);
                if (buffer)
                {
                    auto ret = name_map.insert({key_no,buffer});
                    it = ret.first;
                }
            }
        }
    }
    else
    {
        it = name_map.find(program_no);
        if (it == name_map.end())
        {
            std::string name = midi.get_instrument(bank_no, program_no);
            if (!name.empty())
            {
                DISPLAY("Loading instrument bank: %3i, program: %3i: %s\n",
                         bank_no, program_no, name.c_str());
                Buffer &buffer = midi.buffer(name, true);
                if (buffer)
                {
                    auto ret = name_map.insert({program_no,buffer});
                    it = ret.first;
                }
            }
        }
    }

    if (it != name_map.end()) {
        Instrument::play(key_no, velocity, it->second);
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

float
MIDITrack::registered_param(uint8_t channel, uint8_t controller, uint8_t value)
{
    bool msb_sent = false, lsb_sent = false;
    uint16_t msb_type = 0, lsb_type = 0;
//  uint16_t msb = 0, lsb = 0;
    uint8_t next = 0;
    uint16_t type;
    float rv = 0.0f;

#if 0
 printf("%x %x %x ", 0xb0|channel, controller, value);
 uint8_t *p = (uint8_t*)*this;
 p += offset();
 for (int i=0; i<20; ++i) printf("%x ", p[i]);
 printf("\n");
#endif

    if (controller == MIDI_REGISTERED_PARAM_COARSE)
    {
        msb_type = value;
        msb_sent = true;
        next = pull_byte();
        if (next == 0x00) next = pull_byte();
        if ((next & 0xf0) == MIDI_CONTROL_CHANGE) {
            next = pull_byte();
        }
        if (next == MIDI_REGISTERED_PARAM_FINE)
        {
            lsb_type = pull_byte();
            lsb_sent = true;
            next = pull_byte();
        }
    }
    else if (controller == MIDI_REGISTERED_PARAM_FINE)
    {
        lsb_type = value;
        lsb_sent = true;
        next = pull_byte();
        if (next == 0x00) next = pull_byte();
        if ((next & 0xf0) == MIDI_CONTROL_CHANGE) {
            next = pull_byte();
        }
        if (next == MIDI_REGISTERED_PARAM_COARSE)
        {
            msb_type = pull_byte();
            msb_sent = true;
            next = pull_byte();
        }
    }


    if (msb_type >= MAX_REGISTERED_PARAM || lsb_type >= MAX_REGISTERED_PARAM) {
        return rv;
    }

    type = msb_type << 8 | lsb_type;
    if (msb_sent && lsb_sent)
    {
        if (next == 0x00) next = pull_byte();
        if ((next & 0xf0) == MIDI_CONTROL_CHANGE) {
            next = pull_byte();
        }
        if (next == (MIDI_DATA_ENTRY|MIDI_COARSE))
        {
            param[msb_type].coarse = pull_byte();
            next = pull_byte();
        }
        else if (next == (MIDI_DATA_ENTRY|MIDI_FINE))
        {
            param[lsb_type].fine = pull_byte();
            next = pull_byte();
        }
        else {
            push_byte();
        }

        if (next == 0x00) next = pull_byte();
        if ((next & 0xf0) == MIDI_CONTROL_CHANGE) {
            next = pull_byte();
        }
        if (next == (MIDI_DATA_ENTRY|MIDI_COARSE)) {
            param[msb_type].coarse = pull_byte();
        } else if (next == (MIDI_DATA_ENTRY|MIDI_FINE)) {
            param[lsb_type].fine = pull_byte();
        } else {
            push_byte();
        }

        switch(type)
        {
        case MIDI_PITCH_BEND_RANGE:
            rv = (float)param[MIDI_PITCH_BEND_RANGE].coarse +
                 (float)param[MIDI_PITCH_BEND_RANGE].fine*0.01f;
            midi.channel(channel).set_semi_tones(rv);
            break;
        case MIDI_MODULATION_DEPTH_RANGE:
            rv = (float)param[MIDI_MODULATION_DEPTH_RANGE].coarse +
                 (float)param[MIDI_MODULATION_DEPTH_RANGE].fine*0.01f;
            midi.channel(channel).set_modulation_depth(rv);
            break;
        case MIDI_PARAMETER_RESET:
            midi.channel(channel).set_semi_tones(2.0f);
            break;
        case MIDI_FINE_TUNING:
        case MIDI_COARSE_TUNING:
        case MIDI_TUNING_PROGRAM_CHANGE:
        case MIDI_TUNING_BANK_SELECT:
        default:
            LOG("Unsupported registered parameter: %x\n", type);
            break;
        }
    }
    return rv;
}

void
MIDITrack::rewind()
{
    byte_stream::rewind();
    timestamp_parts = pull_message()*24/600000;

//  channel_no = 0;
    program_no = 0;
    bank_no = 0;
    previous = 0;
    polyphony = true;
    omni = false;
}

bool
MIDITrack::process(uint64_t time_offs_parts, uint32_t& elapsed_parts, uint32_t& next)
{
    bool rv = !eof();

    if (eof()) return rv;

    if (elapsed_parts < wait_parts)
    {
        wait_parts -= elapsed_parts;
        next = wait_parts;
        return rv;
    }

    while (!eof() && (timestamp_parts <= time_offs_parts))
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
            // GM1 rewind: F0 7E 7F 09 01 F7
            // GM2 rewind: F0 7E 7F 09 03 F7
            // GS  rewind: F0 41 10 42 12 40 00 7F 00 41 F7
            if (byte == 0x7e && pull_byte() == 0x7f && pull_byte() == 0x09)
            {
                if (pull_byte() == 0x01) {
                    MESSAGE("Format    : General MIDI 1.0\n");
                } else if (pull_byte() == 0x03) {
                    MESSAGE("Format    : General MIDI 2.0\n");
                }
            }
            else if (byte == 0x41 && pull_byte() == 0x10 &&
                       pull_byte() == 0x42 && pull_byte() == 0x12 &&
                       pull_byte() == 0x40 && pull_byte() == 0x00 &&
                       pull_byte() == 0x7f && pull_byte() == 0x00 &&
                       pull_byte() == 0x41)
            {
                MESSAGE("Format    : General Standard\n");
            }

            push_byte();
            do {
                byte = pull_byte();
            } while (byte != MIDI_EXCLUSIVE_MESSAGE_END && byte != MIDI_EOF);
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
                if (midi.get_verbose()) {
                    printf("%-10s: ", type_name[meta-1].c_str());
                    for (int i=0; i<size; ++i) printf("%c", pull_byte());
                    printf("\n");
                }
                else {
                    forward(size);
                }
                break;
            case MIDI_LYRICS:
                if (midi.get_verbose()) {
                    // printf("%-10s:\n", type_name[meta-1].c_str());
                    for (int i=0; i<size; ++i) printf("%c", pull_byte());
                    fflush(stdout);
                    midi.set_lyrics(true);
                }
                else {
                    forward(size);
                }
                break;
            case MIDI_MARKER:
            case MIDI_CUE_POINT:
            case MIDI_DEVICE_NAME:
                forward(size);
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
                uint32_t tempo;
                tempo = (pull_byte() << 16) | (pull_byte() << 8) | pull_byte();
                midi.set_tempo(tempo);
                break;
            }
            case MIDI_SEQUENCE_NUMBER:	// sequencer software only
            case MIDI_TIME_SIGNATURE:
            case MIDI_SMPTE_OFFSET:
            case MIDI_KEY_SIGNATURE:
                forward(size);
                break;
            default:	// unsupported
                LOG("Unsupported system message: %x\n", meta);
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
                // http://midi.teragonaudio.com/tech/midispec/ctllist.htm
                uint8_t controller = pull_byte();
                uint8_t value = pull_byte();
                switch(controller)
                {
                case MIDI_ALL_CONTROLLERS_OFF:
                     midi.channel(channel).set_expression(1.0f);
                     midi.channel(channel).set_hold(true);
                     midi.channel(channel).set_sustain(false);
                     midi.channel(channel).set_gain(100.0f/127.0f);
                     midi.channel(channel).set_pan(0.0f);
                     midi.channel(channel).set_semi_tones(2.0f);
                     midi.channel(channel).set_pressure(0.0f);
                    // intentional falltrough
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
                case MIDI_BANK_SELECT:
                case MIDI_BANK_SELECT|MIDI_FINE:
                    bank_no = value;
                    break;
                case MIDI_PAN:
                    midi.channel(channel).set_pan(((float)value-64.0f)/64.0f);
                    break;
                case MIDI_EXPRESSION:
                    midi.channel(channel).set_expression((float)value/127.0f);
                    break;
                case MIDI_MODULATION_WHEEL:
                {
                    midi.channel(channel).set_modulation((float)(value << 7)/16383.0f);
                    break;
                }
                case MIDI_CHANNEL_VOLUME:
                    midi.channel(channel).set_gain((float)value/127.0f);
                    break;
                case MIDI_REGISTERED_PARAM_COARSE:
                case MIDI_REGISTERED_PARAM_FINE:
                    registered_param(channel, controller, value);
                    break;
                case MIDI_SOFT_PEDAL:
                    midi.channel(channel).set_soft(value >= 0x40);
                    break;
                case MIDI_HOLD_PEDAL1:
                    midi.channel(channel).set_hold(value >= 0x40);
                    break;
                case MIDI_HOLD_PEDAL2:
                    break;
                case MIDI_SOSTENUTO_PEDAL:
                    midi.channel(channel).set_sustain(value >= 0x40);
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
                float semi_tones = midi.channel(channel).get_semi_tones();
                int16_t pitchbend = pull_byte() | pull_byte() << 7;
                float pitch = semi_tones*(pitchbend-8192);
                if (pitch < 0) pitch /= 8192.0f;
                else pitch /= 8191.0f;
                midi.channel(channel).set_pitch(powf(2.0f, pitch/12.0f));
                break;
            }
            case MIDI_SYSTEM:
                switch(channel)
                {
                case MIDI_SYSTEM_RESET:
#if 0
                    omni = true;
                    polyphony = true;
                    for(auto& it : midi.channel())
                    {
                        midi.process(it.first, MIDI_NOTE_OFF, 0, 0, true);
                        midi.channel(channel).set_semi_tones(2.0f);
                    }
#endif
                    break;
                case MIDI_TIMING_CLOCK:
                case MIDI_START:
                case MIDI_CONTINUE:
                case MIDI_STOP:
                case MIDI_ACTIVE_SENSE:
                default:
                    LOG("Unsupported real-time System message: %x - %x\n", message, channel);
                    break;
                }
                break;
            default:
                LOG("Unsupported message: %x\n", message);
                break;
            }
            break;
        } // switch
        } // default

        if (!eof())
        {
            wait_parts = pull_message();
            timestamp_parts += wait_parts;
        }
    }
    next = wait_parts;

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
                    uint16_t format, track_no = 0;

                    if (header == 0x4d546864) // "MThd"
                    {
                        size = stream.pull_long();
                        if (size != 6) return;

                        format = stream.pull_word();
                        if (format != 0 && format != 1) return;

                        no_tracks = stream.pull_word();
                        if (format == 0 && no_tracks != 1) return;

                        MIDI::set_format(format);

                        uint16_t PPQN = stream.pull_word();
                        if (PPQN & 0x8000) // SMPTE
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
                        MIDI::set_ppqn(PPQN);
                    }

                    while (!stream.eof())
                    {
                        header = stream.pull_long();
                        if (header == 0x4d54726b) // "MTrk"
                        {
                            uint32_t length = stream.pull_long();
                            track.push_back(new MIDITrack(*this, stream, length, track_no++));
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

void
MIDIFile::initialize()
{
    MIDI::read_instruments();

    bool verbose = MIDI::get_verbose();
    MIDI::set_verbose(false);
    MIDI::set_initialize(verbose);

    duration_sec = 0.0f;

    uint64_t time_parts = 0;
    uint32_t wait_parts = 1000000;
    while (process(time_parts, wait_parts))
    {
        time_parts += wait_parts;
        duration_sec += wait_parts*MIDI::get_uspp()*1e-6f;
    }

    MIDI::set_initialize(false);
    MIDI::set_verbose(verbose);
    rewind();

    if (verbose)
    {
        float hour, minutes, seconds;

        seconds = duration_sec;
        hour = floorf(seconds/(60.0f*60.0f));
        seconds -= hour*60.0f*60.0f;
        minutes = floorf(seconds/60.0f);
        seconds -= minutes*60.0f;
        if (hour) {
            printf("Duration  : %02.0f:%02.0f:%02.0f hours\n", hour, minutes, seconds);
        } else {
            printf("Duration  : %02.0f:%02.0f minutes\n", minutes, seconds);
        }
    }

    MIDI::set(AAX_REFRESH_RATE, 90.0f);
    MIDI::set(AAX_INITIALIZED);
}

void
MIDIFile::rewind()
{
    MIDI::rewind();
    for (auto it : track) {
        it->rewind();
    }
}

bool
MIDIFile::process(uint64_t time_parts, uint32_t& next)
{
    uint32_t elapsed_parts = next;
    uint32_t wait_parts;
    bool rv = false;

    next = UINT_MAX;
    for (size_t t=0; t<no_tracks; ++t)
    {
        wait_parts = next;
        rv |= track[t]->process(time_parts, elapsed_parts, wait_parts);
        if (next > wait_parts) {
            next = wait_parts;
        }
    }

    if (MIDI::get_verbose() && !MIDI::get_lyrics())
    {
        float hour, minutes, seconds;

        pos_sec += elapsed_parts*MIDI::get_uspp()*1e-6f;

        seconds = pos_sec;
        hour = floorf(seconds/(60.0f*60.0f));
        seconds -= hour*60.0f*60.0f;
        minutes = floorf(seconds/60.0f);
        seconds -= minutes*60.0f;
        if (hour) {
            printf("pos: %02.0f:%02.0f:%02.0f hours\r", hour, minutes, seconds);
        } else {
            printf("pos: %02.0f:%02.0f minutes\r", minutes, seconds);
        }
        if (!rv) printf("\n\n");
        fflush(stdout);
    }

    return rv;
}
