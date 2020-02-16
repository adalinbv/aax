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

#ifndef AAX_MIDI_HPP
#define AAX_MIDI_HPP 1

#include <cstdint>

#include <aax/midi.h>
#include <aax/instrument.hpp>


namespace aax
{

namespace MIDI
{
#define DRUMS_CHANNEL		0x9
#define FILE_FORMAT_MAX		0x3

static Buffer nullBuffer;

class MIDI : public Mixer
{
public:
    MIDI(AeonWave& ptr) : config(ptr) {}

    virtual ~MIDI() {
        for(auto it=buffers.begin(); it!=buffers.end(); ++it) {
            aaxBufferDestroy(*it->second.second); it->second.first = 0;
        }
        buffers.clear();
    }

    // ** buffer management ******
    Buffer& buffer(std::string& name, int level=0) {
        if (level) { name = name + "?patch=" + std::to_string(level); }
        auto it = buffers.find(name);
        if (it == buffers.end()) {
           Buffer *b = new Buffer(config, name.c_str(), false, true);
            if (b) {
                auto ret = buffers.insert({name,{0,b}});
                it = ret.first;
            } else {
                delete b;
                return nullBuffer;
            }
        }
        it->second.first++;
        return *it->second.second;
    }
    void destroy(Buffer& b) {
        for(auto it=buffers.begin(); it!=buffers.end(); ++it)
        {
            if ((it->second.second == &b) && it->second.first && !(--it->second.first)) {
                aaxBufferDestroy(*it->second.second);
                buffers.erase(it); break;
            }
        }
    }
    bool buffer_avail(std::string &name) {
        auto it = buffers.find(name);
        if (it == buffers.end()) return false;
        return true;
    }


    inline auto& get_patches() { return patches; }
    inline auto& get_config() const { return config; }

private:
    AeonWave& config;

    typedef std::map<uint32_t,std::pair<uint32_t,std::string>> _patch_t;
    std::map<std::string,_patch_t> patches;

    std::unordered_map<std::string,std::pair<size_t,Buffer*>> buffers;

}; // class MIDI


class Channel : public Instrument
{
public:
    Channel(MIDI& ptr, Buffer &buffer, uint32_t channel,
            uint32_t bank, uint32_t program, bool is_drums)
       : Instrument(ptr.get_config(), channel == DRUMS_CHANNEL), midi(ptr),
         channel_no(channel), bank_no(bank), program_no(program),
         drum_channel(channel == DRUMS_CHANNEL ? true : is_drums)
    {
        if (drum_channel && buffer) {
           Mixer::add(buffer);
        }
        Mixer::set(AAX_PLAYING);
    }

    Channel(Channel&&) = default;
    Channel& operator=(Channel&&) = default;

    void play(uint32_t key_no, uint32_t velocity, float pitch);

    inline void set_drums(bool d = true) { drum_channel = d; }
    inline bool is_drums() { return drum_channel; }

    inline uint32_t get_channel_no() { return channel_no; }
    inline uint32_t get_program_no() { return program_no; }
    inline uint32_t get_bank_no() { return bank_no; }

    inline void set_tuning(float pitch) { tuning = powf(2.0f, pitch/12.0f); }
    inline float get_tuning() { return tuning; }

    inline void set_semi_tones(float s) { semi_tones = s; }
    inline float get_semi_tones() { return semi_tones; }

    inline void set_modulation_depth(float d) { modulation_range = d; }
    inline float get_modulation_depth() { return modulation_range; }

    inline bool get_pressure_volume_bend() { return pressure_volume_bend; }
    inline bool get_pressure_pitch_bend() { return pressure_pitch_bend; }
    inline float get_aftertouch_sensitivity() { return pressure_sensitivity; }

    inline void set_track_name(std::string& tname) { track_name = tname; }

private:
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    MIDI &midi;

    std::pair<uint32_t,std::string> get_patch(std::string& name, uint32_t& key);
    std::map<uint32_t,Buffer&> name_map;
    std::string track_name;

    float tuning = 1.0f;
    float modulation_range = 2.0f;
    float pressure_sensitivity = 1.0f;
    float semi_tones = 2.0f;

    uint32_t channel_no = 0;
    uint32_t bank_no = 0;
    uint32_t program_no = 0;

    bool drum_channel = false;
    bool pressure_volume_bend = true;
    bool pressure_pitch_bend = false;
}; // class Channel

}; // namespace MIDI

}; // namespace aax

#endif /* AAX_MIDI_HPP */
