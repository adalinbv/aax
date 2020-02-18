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

#include <sys/stat.h>
#include <cstdint>
#include <climits>

#include <aax/midi.h>
#include <aax/instrument.hpp>


namespace aax {
namespace MIDI {
#define DRUMS_CHANNEL		0x9
#define FILE_FORMAT_MAX		0x3
#define BUFFER_SIZE		1024

enum {
    MODE0 = 0,
    GENERAL_MIDI1,
    GENERAL_MIDI2,
    GENERAL_STANDARD,
    EXTENDED_GENERAL_MIDI,

    MODE_MAX
};

enum {
    POLYPHONIC = 3,
    MONOPHONIC
};

struct param_t
{
   uint32_t coarse;
   uint32_t fine;
};

static Buffer nullBuffer;

class Channel;

class MIDI : public Mixer
{
public:
    MIDI(AeonWave& ptr);

    virtual ~MIDI() {
        for(auto it : buffers) {
            aaxBufferDestroy(*it.second.second); it.second.first = 0;
        }
        buffers.clear();
    }

    bool process(uint32_t channel, uint32_t message, uint32_t key, uint32_t velocity, bool omni, float pitch=1.0f);

    Channel& new_channel(uint32_t channel, uint32_t bank, uint32_t program);

    Channel& channel(uint32_t channel_no);

    inline auto& channel() { return channels; }

    inline void set_drum_file(std::string p) { drum = p; }
    inline void set_instrument_file(std::string p) { instr = p; }
    inline void set_file_path(std::string p) {
        config.set(AAX_SHARED_DATA_DIR, p.c_str()); path = p;
    }

    inline const auto& get_patch_set() { return patch_set; }
    inline const auto& get_patch_version() { return patch_version; }
    inline auto& get_selections() { return selection; }

    inline void set_track_active(uint32_t t) { active_track.push_back(t); }
    inline uint32_t no_active_tracks() { return active_track.size(); }
    inline bool is_track_active(uint32_t t) {
        return active_track.empty() ? true : std::find(active_track.begin(), active_track.end(), t) != active_track.end();
    }

    void read_instruments(std::string gmidi=std::string(), std::string gmdrums=std::string());

    void grep(std::string& filename, const char *grep);
    inline void load(std::string& name) { loaded.push_back(name); }

    void start();
    void stop();
    void rewind();

    void finish(uint32_t n);
    bool finished(uint32_t n);

    void set_gain(float);
    void set_balance(float);

    bool is_drums(uint32_t);

    inline void set_capabilities(enum aaxCapabilities m) {
        instrument_mode = m; set(AAX_CAPABILITIES, m); set_path();
    }

    inline unsigned int get_refresh_rate() { return refresh_rate; }
    inline unsigned int get_polyphony() { return polyphony; }

    inline void set_tuning(float pitch) { tuning = powf(2.0f, pitch/12.0f); }
    inline float get_tuning() { return tuning; }

    inline void set_mode(uint32_t m) { if (m > mode) mode = m; }
    inline uint32_t get_mode() { return mode; }

    inline void set_grep(bool g) { grep_mode = g; }
    inline bool get_grep() { return grep_mode; }

    const auto get_drum(uint32_t program, uint32_t key, bool all=false);
    const auto get_instrument(uint32_t bank, uint32_t program, bool all=false);
    auto& get_patches() { return patches; }

    inline void set_initialize(bool i) { initialize = i; };
    inline bool get_initialize() { return initialize; }

    inline void set_verbose(bool v) { verbose = v; }
    inline bool get_verbose() { return verbose; }

    inline void set_lyrics(bool v) { lyrics = v; }
    inline bool get_lyrics() { return lyrics; }

    inline void set_format(uint32_t fmt) { format = fmt; }
    inline uint32_t get_format() { return format; }

    inline void set_tempo(uint32_t tempo) { uSPP = tempo/PPQN; }

    inline void set_uspp(uint32_t uspp) { uSPP = uspp; }
    inline int32_t get_uspp() { return uSPP; }

    inline void set_ppqn(uint32_t ppqn) { PPQN = ppqn; }
    inline uint32_t get_ppqn() { return PPQN; }

    void set_chorus(const char *t);
    void set_chorus_level(float lvl);
    void set_chorus_depth(float depth);
    void set_chorus_rate(float rate);

    void set_reverb(const char *t);
    void set_reverb_level(uint32_t channel, uint32_t value);
    void set_reverb_type(uint32_t value);
    inline void set_decay_depth(float rt) { reverb_decay_depth = 0.1f*rt; }

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

    bool exists(const std::string& path) {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    }

    inline auto& get_config() const { return config; }

private:
    void add_patch(const char*);
    void set_path();

    AeonWave& config;

    std::string patch_set = "default";
    std::string patch_version = "1.0.0";

    std::string track_name;
    std::map<uint32_t,Channel*> channels;
    std::map<uint32_t,Channel*> reverb_channels;
    std::map<uint32_t,std::string> frames;
    std::map<uint32_t,std::map<uint32_t,std::pair<std::string,int>>> drums;
    std::map<uint32_t,std::map<uint32_t,std::pair<std::string,int>>> instruments;

    typedef std::map<uint32_t,std::pair<uint32_t,std::string>> _patch_t;
    std::map<std::string,_patch_t> patches;

    std::unordered_map<std::string,std::pair<size_t,Buffer*>> buffers;

    std::vector<std::string> loaded;

    std::vector<std::string> selection;
    std::vector<uint32_t> active_track;

    std::pair<std::string,int> empty_map = {"", 0};
    std::string instr = "gmmidi.xml";
    std::string drum = "gmdrums.xml";
    std::string path;

    float tuning = 1.0f;

    unsigned int refresh_rate = 0;
    unsigned int polyphony = UINT_MAX;

    uint32_t uSPP = 500000/24;
    uint32_t format = 0;
    uint32_t PPQN = 24;

    enum aaxCapabilities instrument_mode = AAX_RENDER_NORMAL;
    uint32_t mode = MODE0;
    bool initialize = false;
    bool verbose = false;
    bool lyrics = false;
    bool grep_mode = false;

    uint32_t reverb_type = 4;
    Param reverb_decay_depth = 0.15f;
    Param reverb_cutoff_frequency = 790.0f;
    Status reverb_state = AAX_FALSE;
    aax::Mixer reverb;

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


class Track
{
public:
    Track() = default;

    Track(MIDI& ptr, size_t len,  uint32_t track)
        : midi(ptr), channel_no(track)
    {
        timestamp_parts = pull_message()*24/600000;
    }

    Track(const Track&) = default;

    ~Track() = default;

    bool process(uint64_t, uint32_t&, uint32_t&);

    // placeholders for now
    // these needs to work on a buffer which gets filled by aax::MIDI::Stream
    uint64_t offset() { return 0; }
    void forward(uint64_t offs=0) {}
    uint32_t pull_byte() { return 0; }
    void push_byte() {};
    bool eof() { return true; };

    MIDI& midi;

private:
    inline float cents2pitch(float p, uint32_t channel) {
        float r = midi.channel(channel).get_semi_tones();
        return powf(2.0f, p*r/12.0f);
    }
    inline float cents2modulation(float p, uint32_t channel) {
        float r = midi.channel(channel).get_modulation_depth();
        return powf(2.0f, p*r/12.0f);
    }

    uint32_t pull_message();
    bool registered_param(uint32_t, uint32_t, uint32_t);

    uint32_t mode = 0;
    uint32_t channel_no = 0;
    uint32_t program_no = 0;
    uint32_t bank_no = 0;
    int32_t track_no = -1;

    uint32_t previous = 0;
    uint32_t wait_parts = 1;
    uint64_t timestamp_parts = 0;
    bool polyphony = true;
    bool omni = true;

    bool registered = false;
    uint32_t msb_type = 0;
    uint32_t lsb_type = 0;
    struct param_t param[MAX_REGISTERED_PARAM+1] = {
        { 2, 0 }, { 0x40, 0 }, { 0x20, 0 }, { 0, 0 }, { 0, 0 }, { 1, 0 }
    };

    const std::string type_name[5] = {
        "Text", "Copyright", "Track", "Instrument", "Lyrics"
    };
}; // class Track


class Stream : public MIDI
{
public:
    Stream(AeonWave& config);

    inline operator bool() {
        return midi_data.capacity();
    }

    void initialize();
    inline void start() { MIDI::start(); }
    inline void stop() { MIDI::stop(); }

    inline float get_pos_sec() { return pos_sec; }

    bool process(uint64_t, uint32_t&);

private:
    std::string gmmidi;
    std::string gmdrums;
    std::vector<Track*> track;
    std::vector<uint32_t> midi_data;

    uint32_t no_tracks = 0;
    float pos_sec = 0.0f;

    const std::string mode_name[MODE_MAX] = {
        "MIDI", "General MIDI 1.0", "General MIDI 2.0", "GS MIDI", "XG MIDI"
    };
}; // class Stream


}; // namespace MIDI
}; // namespace aax

#endif /* AAX_MIDI_HPP */
