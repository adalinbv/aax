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

#include <cstring>
#include <string>

#include <xml.h>
#include <aax/strings.hpp>

#include "aax_midi.hpp"

using namespace aax::MIDI;

namespace aax
{

namespace MIDI
{

// aax::MIDI::MIDI

MIDI::MIDI(AeonWave& ptr) : config(ptr)
{
    set_path();

    reverb.tie(reverb_decay_depth, AAX_REVERB_EFFECT, AAX_DECAY_DEPTH);
    reverb.tie(reverb_cutoff_frequency, AAX_REVERB_EFFECT, AAX_CUTOFF_FREQUENCY);
    reverb.tie(reverb_state, AAX_REVERB_EFFECT);
}

void
MIDI::set_path()
{
    path = config.info(AAX_SHARED_DATA_DIR);

    std::string name = path;
    if (instrument_mode == AAX_RENDER_NORMAL) {
        name.append("/ultrasynth/");
    }
    if (exists(name))
    {
        path = name;
        config.set(AAX_SHARED_DATA_DIR, path.c_str());
    }
}

void
MIDI::start()
{
    reverb_state = AAX_REVERB_2ND_ORDER;
    set_reverb("reverb/concerthall");
    reverb.set(AAX_INITIALIZED);
    reverb.set(AAX_PLAYING);
    Mixer::add(reverb);
    Mixer::set(AAX_PLAYING);
}

void
MIDI::stop()
{
    reverb.set(AAX_PLAYING);
    Mixer::set(AAX_PLAYING);
}

void
MIDI::rewind()
{
    channels.clear();
    uSPP = 500000/PPQN;

    for (auto it : reverb_channels)
    {
        reverb.remove(*it.second);
        Mixer::add(*it.second);
    }
    reverb_channels.clear();
}

void MIDI::finish(uint32_t n)
{
    auto it = channels.find(n);
    if (it == channels.end()) return;

    if (it->second->finished() == false) {
        it->second->finish();
    }
}

bool
MIDI::finished(uint32_t n)
{
    auto it = channels.find(n);
    if (it == channels.end()) return true;
    return it->second->finished();
}

void
MIDI::set_gain(float g)
{
    aax::dsp dsp = Mixer::get(AAX_VOLUME_FILTER);
    dsp.set(AAX_GAIN, g);
    Mixer::set(dsp);
}

bool
MIDI::is_drums(uint32_t n)
{
    auto it = channels.find(n);
    if (it == channels.end()) return false;
    return it->second->is_drums();
}

void
MIDI::set_balance(float b)
{
    Matrix64 m;
    m.rotate(1.57*b, 0.0, 1.0, 0.0);
    m.inverse();
    Mixer::matrix(m);
}

void
MIDI::set_chorus(const char *t)
{
    Buffer& buf = config.buffer(t);
    for(auto& it : channels) {
        it.second->add(buf);
    }
}

void
MIDI::set_chorus_level(float lvl)
{
    for(auto& it : channels) {
        it.second->set_chorus_level(lvl);
    }
}

void
MIDI::set_chorus_depth(float ms)
{
    float us = 1e-3f*ms;
    for(auto& it : channels) {
        it.second->set_chorus_depth(us);
    }
}

void
MIDI::set_chorus_rate(float rate)
{
    for(auto& it : channels) {
        it.second->set_chorus_rate(rate);
    }
}

void
MIDI::set_reverb(const char *t)
{
    Buffer& buf = config.buffer(t);
    reverb.add(buf);
    for(auto& it : channels) {
        it.second->set_reverb(buf);
    }
}

void
MIDI::set_reverb_type(uint32_t value)
{
    reverb_type = value;
    switch (value)
    {
    case 0:
        set_reverb("reverb/room-small");
        break;
    case 1:
        set_reverb("reverb/room-medium");
        break;
    case 2:
        set_reverb("reverb/room-large");
        break;
    case 3:
        set_reverb("reverb/concerthall");
        break;
    case 4:
        set_reverb("reverb/concerthall-large");
        break;
    case 8:
        set_reverb("reverb/plate");
        break;
    default:
        break;
    }
}

void
MIDI::set_reverb_level(uint32_t channel_no, uint32_t value)
{
    if (value)
    {
        float val = (float)value/127.0f;
        channel(channel_no).set_reverb_level(val);

        auto it = reverb_channels.find(channel_no);
        if (it == reverb_channels.end())
        {
            it = channels.find(channel_no);
            if (it != channels.end() && it->second)
            {
                Mixer::remove(*it->second);
                reverb.add(*it->second);
                reverb_channels[it->first] = it->second;
            }
        }
    }
    else
    {
        auto it = reverb_channels.find(channel_no);
        if (it != reverb_channels.end() && it->second)
        {
            reverb.remove(*it->second);
            Mixer::add(*it->second);
            reverb_channels.erase(it);
        }
    }
}

/*
 * Create map of instrument banks and program numbers with their associated
 * file names from the XML files for a quick access during playback.
 */
void
MIDI::read_instruments(std::string gmmidi, std::string gmdrums)
{
    const char *filename, *type = "instrument";
    auto map = instruments;

    std::string iname;
    if (!gmmidi.empty())
    {
        iname = gmmidi;

        struct stat buffer;
        if (stat(iname.c_str(), &buffer) != 0)
        {
           iname = path;
           iname.append("/");
           iname.append(gmmidi);
        }
    } else {
        iname = path;
        iname.append("/");
        iname.append(instr);
    }

    filename = iname.c_str();
    for(unsigned int id=0; id<2; ++id)
    {
        void *xid = xmlOpen(filename);
        if (xid)
        {
            void *xaid = xmlNodeGet(xid, "aeonwave");
            void *xmid = nullptr;
            char file[64] = "";

            if (xaid)
            {
                if (xmlAttributeExists(xaid, "rate"))
                {
                    unsigned int rate = xmlAttributeGetInt(xaid, "rate");
                    if (rate >= 25 && rate <= 200) {
                       refresh_rate = rate;
                    }
                }
                if (xmlAttributeExists(xaid, "polyphony"))
                {

                    polyphony =  xmlAttributeGetInt(xaid, "polyphony");
                    if (polyphony < 32) polyphony = 32;
                }
                xmid = xmlNodeGet(xaid, "midi");
            }

            if (xmid)
            {
                if (xmlAttributeExists(xmid, "name"))
                {
                    char *set = xmlAttributeGetString(xmid, "name");
                    if (set && strlen(set) != 0) {
                        patch_set = set;
                    }
                    xmlFree(set);
                }

                if (xmlAttributeExists(xmid, "mode"))
                {
                   if (!xmlAttributeCompareString(xmid, "mode", "synthesizer"))
                   {
                       instrument_mode = AAX_RENDER_SYNTHESIZER;
                   }
                   else if (!xmlAttributeCompareString(xmid, "mode", "arcade"))
                   {
                       instrument_mode = AAX_RENDER_ARCADE;
                   }
                }

                if (xmlAttributeExists(xmid, "version"))
                {
                    char *set = xmlAttributeGetString(xmid, "version");
                    if (set && strlen(set) != 0) {
                        patch_version = set;
                    }
                    xmlFree(set);
                }

                unsigned int bnum = xmlNodeGetNum(xmid, "bank");
                void *xbid = xmlMarkId(xmid);
                for (unsigned int b=0; b<bnum; b++)
                {
                    if (xmlNodeGetPos(xmid, xbid, "bank", b) != 0)
                    {
                        long int bank_no = xmlAttributeGetInt(xbid, "n") << 7;
                        unsigned int slen, inum = xmlNodeGetNum(xbid, type);
                        void *xiid = xmlMarkId(xbid);

                        bank_no = xmlAttributeGetInt(xbid, "n") << 7;
                        bank_no += xmlAttributeGetInt(xbid, "l");

                        // bank audio-frame filter and effects file
                        slen = xmlAttributeCopyString(xbid, "file", file, 64);
                        if (slen)
                        {
                            file[slen] = 0;
                            frames.insert({bank_no,std::string(file)});
                        }

                        auto bank = map[bank_no];
                        for (unsigned int i=0; i<inum; i++)
                        {
                            if (xmlNodeGetPos(xbid, xiid, type, i) != 0)
                            {
                                long int n = xmlAttributeGetInt(xiid, "n");
                                int stereo = xmlAttributeGetInt(xiid, "wide");
                                if (!stereo && xmlAttributeGetBool(xiid, "wide"))
                                {
                                    stereo = 1;
                                }

                                // instrument file-name
                                slen = xmlAttributeCopyString(xiid, "file",
                                                              file, 64);
                                if (slen)
                                {
                                    file[slen] = 0;
                                    bank.insert({n,{file,stereo}});

                                    _patch_t p;
                                    p.insert({0,{i,file}});

                                    patches.insert({file,p});
//                                  if (id == 0) printf("{%x, {%i, {%s, %i}}}\n", bank_no, n, file, wide);
                                }
                                else
                                {
                                    slen = xmlAttributeCopyString(xiid, "patch",
                                                                  file, 64);
                                    if (slen)
                                    {
                                        file[slen] = 0;
                                        bank.insert({n,{file,stereo}});

                                        add_patch(file);
                                    }
                                }
                            }
                        }
                        map[bank_no] = bank;
                        xmlFree(xiid);
                    }
                }
                xmlFree(xbid);
                xmlFree(xmid);
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

        if (id == 0)
        {
            instruments = std::move(map);

            // next up: drums
            if (!gmdrums.empty())
            {
                iname = gmdrums;

                struct stat buffer;
                if (stat(iname.c_str(), &buffer) != 0)
                {
                   iname = path;
                   iname.append("/");
                   iname.append(gmmidi);
                }
            } else {
                iname = path;
                iname.append("/");
                iname.append(drum);
            }
            filename = iname.c_str();
            type = "drum";
            map = drums;
        }
        else {
            drums = std::move(map);
        }
    }
}

void
MIDI::add_patch(const char *file)
{
    const char *path = config.info(AAX_SHARED_DATA_DIR);

    std::string xmlfile(path);
    xmlfile.append("/");
    xmlfile.append(file);
    xmlfile.append(".xml");

    void *xid = xmlOpen(xmlfile.c_str());
    if (xid)
    {
        void *xlid = xmlNodeGet(xid, "instrument/layer");
        if (xlid)
        {
            unsigned int pnum = xmlNodeGetNum(xlid, "patch");
            void *xpid = xmlMarkId(xlid);
            _patch_t p;
            for (unsigned int i=0; i<pnum; i++)
            {
                if (xmlNodeGetPos(xlid, xpid, "patch", i) != 0)
                {
                    unsigned int slen;
                    char file[64] = "";

                    slen = xmlAttributeCopyString(xpid, "file", file, 64);
                    if (slen)
                    {
                        uint32_t max = xmlAttributeGetInt(xpid, "max");
                        file[slen] = 0;

                        p.insert({max,{i,file}});
                    }
                }
            }
            patches.insert({file,p});

            xmlFree(xpid);
            xmlFree(xlid);
        }
        xmlFree(xid);
    }
}

/*
 * For drum mapping the program_no is stored in the bank number of the map
 * and the key_no in the program number of the map.
 */
const auto
MIDI::get_drum(uint32_t program_no, uint32_t key_no, bool all)
{
    auto itb = drums.find(program_no << 7);
    if (itb == drums.end() && program_no > 0)
    {
        if ((program_no & 0xF8) == program_no) program_no = 0;
        else program_no &= 0xF8;
        itb = drums.find(program_no << 7);
        if (itb == drums.end())
        {
            program_no = 0;
            itb = drums.find(program_no);
        }
    }

    if (itb != drums.end())
    {
        do
        {
            auto bank = itb->second;
            auto iti = bank.find(key_no);
            if (iti != bank.end()) {
                if (all || selection.empty() || std::find(selection.begin(), selection.end(), iti->second.first) != selection.end()) {
                    return iti->second;
                } else {
                    return empty_map;
                }
            }

            if (program_no > 0)
            {
                if ((program_no & 0xF8) == program_no) program_no = 0;
                else program_no &= 0xF8;
                itb = drums.find(program_no << 7);
                if (itb == drums.end())
                {
                    program_no = 0;
                    itb = drums.find(program_no);
                }
            }
            else {
               break;
            }
        }
        while (program_no >= 0);
    }
    return empty_map;
}

const auto
MIDI::get_instrument(uint32_t bank_no, uint32_t program_no, bool all)
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
            if (iti != bank.end())
            {
                if (all || selection.empty() || std::find(selection.begin(), selection.end(), iti->second.first) != selection.end()) {
                    return iti->second;
                } else {
                    return empty_map;
                }
            }

            if (bank_no & 0x7F)
            {
                bank_no &= ~0x7F;
                itb = instruments.find(bank_no);
            }
            else if (bank_no > 0)
            {
                bank_no = 0;
                itb = instruments.find(bank_no);
            }
            else {
               break;
            }
        }
        while (bank_no >= 0);
    }
    return empty_map;;
}

Channel&
MIDI::new_channel(uint32_t channel_no, uint32_t bank_no, uint32_t program_no)
{
    auto it = channels.find(channel_no);
    if (it != channels.end())
    {
        if (it->second) Mixer::remove(*it->second);
        channels.erase(it);
    }

    int level = 0;
    std::string name = "";
    bool drums = is_drums(channel_no);
    if (drums && !frames.empty())
    {
        auto it = frames.find(program_no);
        if (it != frames.end()) {
            level = it->first;
            name = it->second;
        }
    }

    Buffer& buffer = config.buffer(name, level);
    if (buffer) {
        buffer.set(AAX_CAPABILITIES, int(instrument_mode));
    }

    try {
        auto ret = channels.insert(
            { channel_no, new Channel(*this, buffer, channel_no,
                                          bank_no, program_no, drums)
            } );
        it = ret.first;
        Mixer::add(*it->second);
    } catch(const std::invalid_argument& e) {
        throw;
    }
    return *it->second;
}

Channel&
MIDI::channel(uint32_t channel_no)
{
    auto it = channels.find(channel_no);
    if (it != channels.end()) {
        return *it->second;
    }
    return new_channel(channel_no, 0, 0);
}


bool
MIDI::process(uint32_t channel_no, uint32_t message, uint32_t key, uint32_t velocity, bool omni, float pitch)
{
    // Omni mode: Device responds to MIDI data regardless of channel
    if (message == MIDI_NOTE_ON && velocity) {
        if (is_track_active(channel_no)) {
            channel(channel_no).play(key, velocity, pitch);
        }
    }
    else
    {
        if (message == MIDI_NOTE_ON) {
            velocity = 64;
        }
        channel(channel_no).stop(key, velocity);
    }
    return true;
}


// aax::MIDI::Channel

std::pair<uint32_t,std::string>
Channel::get_patch(std::string& name, uint32_t& key_no)
{
    auto patches = midi.get_patches();
    auto it = patches.find(name);
    if (it != patches.end())
    {
        auto patch = it->second.upper_bound(key_no);
        if (patch != it->second.end()) {
            return patch->second;
        }
    }

    key_no = 255;
    return {0,name};
}

void
Channel::play(uint32_t key_no, uint32_t velocity, float pitch)
{
    bool all = midi.no_active_tracks() > 0;
    auto it = name_map.begin();
    if (midi.channel(channel_no).is_drums())
    {
        it = name_map.find(key_no);
        if (it == name_map.end())
        {
            auto inst = midi.get_drum(program_no, key_no, all);
            std::string name = inst.first;
            if (!name.empty())
            {
                if (!midi.buffer_avail(name)) {
                    midi.load(name);
                }

                if (midi.get_grep())
                {
                   auto ret = name_map.insert({key_no,nullBuffer});
                   it = ret.first;
                }
                else
                {
                    Buffer& buffer = midi.buffer(name);
                    if (buffer)
                    {
                        auto ret = name_map.insert({key_no,buffer});
                        it = ret.first;
                    }
                    else {
                        throw(std::invalid_argument("Instrument file "+name+" could not load"));
                    }
                }
            }
        }
    }
    else
    {
        uint32_t key = key_no;
        it = name_map.upper_bound(key);
        if (it == name_map.end())
        {
            auto inst = midi.get_instrument(bank_no, program_no, all);
            auto patch = get_patch(inst.first, key);
            std::string patch_name = patch.second;
            uint32_t level = patch.first;
            if (!patch_name.empty())
            {
                if (!midi.buffer_avail(patch_name)) {
                    midi.load(patch_name);
                }

                if (midi.get_grep())
                {
                   auto ret = name_map.insert({key,nullBuffer});
                   it = ret.first;
                }
                else
                {
                    Buffer& buffer = midi.buffer(patch_name, level);
                    if (buffer)
                    {
                        auto ret = name_map.insert({key,buffer});
                        it = ret.first;

                        // mode == 0: volume bend only
                        // mode == 1: pitch bend only
                        // mode == 2: volume and pitch bend
                        int pressure_mode = buffer.get(AAX_PRESSURE_MODE);
                        if (pressure_mode == 0 || pressure_mode == 2) {
                           pressure_volume_bend = true;
                        }
                        if (pressure_mode > 0) {
                           pressure_pitch_bend = true;
                        }

                        // AAX_AFTERTOUCH_SENSITIVITY == AAX_VELOCITY_FACTOR
                        pressure_sensitivity = 0.01f*buffer.get(AAX_VELOCITY_FACTOR);
                    }
                    midi.channel(channel_no).set_wide(inst.second);
                }
            }
        }
    }

    if (!midi.get_initialize() && it != name_map.end())
    {
        if (midi.channel(channel_no).is_drums())
        {
            switch(program_no)
            {
            case 0:     // Standard Set
            case 16:    // Power set
            case 32:    // Jazz set
            case 40:    // Brush set
                switch(key_no)
                {
                case 29: // EXC7
                    Instrument::stop(30);
                    break;
                case 30: // EXC7
                    Instrument::stop(29);
                    break;
                case 42: // EXC1
                    Instrument::stop(44);
                    Instrument::stop(46);
                    break;
                case 44: // EXC1
                    Instrument::stop(42);
                    Instrument::stop(46);
                    break;
                case 46: // EXC1
                    Instrument::stop(42);
                    Instrument::stop(44);
                    break;
                case 71: // EXC2
                    Instrument::stop(72);
                    break;
                case 72: // EXC2
                    Instrument::stop(71);
                    break;
                case 73: // EXC3
                    Instrument::stop(74);
                    break;
                case 74: // EXC3
                    Instrument::stop(73);
                    break;
                case 78: // EXC4
                    Instrument::stop(79);
                    break;
                case 79: // EXC4
                    Instrument::stop(78);
                    break;
                case 80: // EXC5
                    Instrument::stop(81);
                    break;
                case 81: // EXC5
                    Instrument::stop(80);
                    break;
                case 86: // EXC6
                    Instrument::stop(87);
                    break;
                case 87: // EXC6
                    Instrument::stop(86);
                    break;
                default:
                    break;
                }
                break;
            case 26:    // Analog Set
                switch(key_no)
                {
                case 42: // EXC1
                    Instrument::stop(44);
                    Instrument::stop(46);
                    break;
                case 44: // EXC1
                    Instrument::stop(42);
                    Instrument::stop(46);
                    break;
                case 46: // EXC1
                    Instrument::stop(42);
                    Instrument::stop(44);
                    break;
                default:
                    break;
                }
                break;
            case 48:    // Orchestra Set
                switch(key_no)
                {
                case 27: // EXC1
                    Instrument::stop(28);
                    Instrument::stop(29);
                    break;
                case 28: // EXC1
                    Instrument::stop(27);
                    Instrument::stop(29);
                    break;
                case 29: // EXC1
                    Instrument::stop(27);
                    Instrument::stop(28);
                    break;
                case 42: // EXC1
                    Instrument::stop(44);
                    Instrument::stop(46);
                    break;
                case 44: // EXC1
                    Instrument::stop(42);
                    Instrument::stop(46);
                    break;
                case 46: // EXC1
                    Instrument::stop(42);
                    Instrument::stop(44);
                    break;
                default:
                    break;
                }
                break;
            case 57:    // SFX Set
                switch(key_no)
                {
                case 41: // EXC7
                    Instrument::stop(42);
                    break;
                case 42: // EXC7
                    Instrument::stop(41);
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }
        }

        Instrument::play(key_no, velocity/127.0f, it->second, pitch);
    } else {
//      throw(std::invalid_argument("Instrument file "+name+" not found"));
    }
}

}; // namespace MIDI

}; // namespace aax
