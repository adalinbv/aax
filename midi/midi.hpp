/*
 * Copyright (C) 2018-2019 by Erik Hofman.
 * Copyright (C) 2018-2019 by Adalin B.V.
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

#include <cstdlib>
#include <stdexcept>
#include <vector>
#include <map>

#include <aax/aeonwave.hpp>
#include <aax/instrument.hpp>
#include <aax/buffer_map.hpp>
#include <aax/byte_stream.hpp>

namespace aax
{

/* system exclusive */
#define MIDI_SYSTEM_EXCLUSIVE			0xf0
#define MIDI_SYSTEM_EXCLUSIVE_END		0xf7
#define MIDI_SYSTEM_EXCLUSIVE_NON_REALTIME	0x7e
#define MIDI_SYSTEM_EXCLUSIVE_REALTIME		0x7f

#define MIDI_SYSTEM_EXCLUSIVE_ROLAND		0x41
#define MIDI_SYSTEM_EXCLUSIVE_YAMAHA		0x43

/* system exclusive non-real time message */
#define MIDI_EOF			0x7b
#define MIDI_WAIT			0x7c
#define MIDI_CANCEL			0x7d
#define MIDI_NAK			0x7e
#define MIDI_ACK			0x7f

/* system exclusive real time message */
#define MIDI_DEVICE_CONTROL		0x04

#define MIDI_DEVICE_VOLUME		0x01
#define MIDI_DEVICE_BALANCE		0x02
#define MIDI_DEVICE_FINE_TUNING		0x03
#define MIDI_DEVICE_COARSE_TUNING	0x04
#define MIDI_GLOBAL_PARAMETER_CONTROL	0x05

/* global parameters */
#define MIDI_REVERB_PARAMETER		0x0101
#define MIDI_CHORUS_PARAMETER		0x0102


#define GENERAL_MIDI_SYSTEM		0x09
#define MIDI_CONTROLLER_DESTINATION	0x09

/* controller type */
#define MIDI_CHANNEL_PRESSURE		0x01
#define MIDI_CONTROLLER_CHANGE		0x03

/* control change */
#define MIDI_PARAM_PITCH		0x00
#define MIDI_PARAM_FILTER_CUTOFF	0x01
#define MIDI_PARAM_AMPLITUDE		0x02
#define MIDI_PARAM_LFO_PITCH_DEPTH	0x03
#define MIDI_PARAM_LFO_FILTER_DEPTH	0x04
#define MIDI_PARAM_LFO_AMPLITUDE_DEPTH	0x05

// master fine tuning
// master coarse tuning
// reverb type
// reverb time
// chorus type
// chorus mod rate
// chorus mod depth
// chorus feedback
// chorus send to reverb
// scale/octave tuning adjust
// key-based instrument controllers
// GM2 system on

#define MIDI_TUNING_STANDARD		0x08
#define MIDI_TUNING_SINGLE_NOTE		0x02
#define MIDI_TUNING_SINGLE_NOTE_BANK	0x07
#define MIDI_TUNING_OCTAVE1		0x08
#define MIDI_TUNING_OCTAVE2		0x09

/* meta messages */
// https://www.recordingblogs.com/wiki/midi-meta-messages
#define MIDI_SEQUENCE_NUMBER		0x00
#define MIDI_TEXT			0x01
#define MIDI_COPYRIGHT			0x02
#define MIDI_TRACK_NAME			0x03
#define MIDI_INSTRUMENT_NAME		0x04
#define MIDI_LYRICS			0x05
#define MIDI_MARKER			0x06
#define MIDI_CUE_POINT			0x07
#define MIDI_DEVICE_NAME		0x09
#define MIDI_CHANNEL_PREFIX		0x20
#define MIDI_PORT_PREFERENCE		0x21
#define MIDI_END_OF_TRACK		0x2f
#define MIDI_SET_TEMPO			0x51
#define MIDI_SMPTE_OFFSET		0x54
#define MIDI_TIME_SIGNATURE		0x58
#define MIDI_KEY_SIGNATURE		0x59
#define MIDI_SEQUENCERSPECIFICMETAEVENT	0x7f

/* channel messages */
// https://learn.sparkfun.com/tutorials/midi-tutorial/messages
#define MIDI_NOTE_OFF			0x80
#define MIDI_NOTE_ON			0x90
#define MIDI_POLYPHONIC_AFTERTOUCH	0xa0
#define MIDI_CONTROL_CHANGE		0xb0
#define MIDI_PROGRAM_CHANGE		0xc0
#define MIDI_CHANNEL_AFTERTOUCH		0xd0
#define MIDI_PITCH_BEND			0xe0
#define MIDI_SYSTEM			0xf0

/* controller messages */
// https://www.midi.org/specifications-old/item/table-3-control-change-messages-data-bytes-2
#define MIDI_COARSE			0x00
#define MIDI_FINE			0x20

#define MIDI_BANK_SELECT		0x00
#define MIDI_MODULATION_DEPTH		0x01
#define MIDI_BREATH_CONTROLLER		0x02
#define MIDI_FOOT_CONTROLLER		0x04
#define MIDI_PORTAMENTO_TIME		0x05
#define MIDI_DATA_ENTRY			0x06
#define MIDI_CHANNEL_VOLUME		0x07
#define MIDI_BALANCE			0x08
#define MIDI_PAN			0x0a
#define MIDI_EXPRESSION			0x0b
#define MIDI_EFFECT_CONTROL1		0x0c
#define MIDI_EFFECT_CONTROL2		0x0d
#define MIDI_GENERAL_PURPOSE_CONTROL1	0x10
#define MIDI_GENERAL_PURPOSE_CONTROL2	0x11
#define MIDI_GENERAL_PURPOSE_CONTROL3	0x12
#define MIDI_GENERAL_PURPOSE_CONTROL4	0X13
#define MIDI_DAMPER_PEDAL_SWITCH	0x40
#define MIDI_PORTAMENTO_SWITCH		0x41
#define MIDI_SOSTENUTO_SWITCH		0x42
#define MIDI_SOFT_PEDAL_SWITCH		0x43
#define MIDI_LEGATO_SWITCH		0x44
#define MIDI_HOLD2			0x45
#define MIDI_SOUND_VARIATION		0x46
#define MIDI_FILTER_RESONANCE		0x47
#define MIDI_RELEASE_TIME		0x48
#define MIDI_ATTACK_TIME		0x49
#define MIDI_CUTOFF			0x4a
#define MIDI_DECAY_TIME			0x4b
#define MIDI_VIBRATO_RATE		0x4c
#define MIDI_VIBRATO_DEPTH		0x4d
#define MIDI_VIBRATO_DELAY		0x4e
#define MIDI_SOUND_CONTROL10		0x4f
#define MIDI_GENERAL_PURPOSE_CONTROL5	0x50
#define MIDI_GENERAL_PURPOSE_CONTROL6	0x51
#define MIDI_GENERAL_PURPOSE_CONTROL7	0x52
#define MIDI_GENERAL_PURPOSE_CONTROL8	0x53
#define MIDI_PORTAMENTO_CONTROL		0x54
#define MIDI_HIGHRES_VELOCITY_PREFIX	0x58
#define MIDI_REVERB_SEND_LEVEL		0x5b
#define MIDI_TREMOLO_EFFECT_DEPTH	0x5c
#define MIDI_CHORUS_SEND_LEVEL		0x5d
#define MIDI_CELESTE_EFFECT_DEPTH	0x5e
#define MIDI_PHASER_EFFECT_DEPTH	0x5f
#define MIDI_DATA_INCREMENT		0x60
#define MIDI_DATA_DECREMENT		0x61
#define MIDI_UNREGISTERED_PARAM_FINE	0x62
#define MIDI_UNREGISTERED_PARAM_COARSE	0x63
#define MIDI_REGISTERED_PARAM_FINE	0x64
#define MIDI_REGISTERED_PARAM_COARSE	0x65
#define MIDI_ALL_SOUND_OFF		0x78
#define MIDI_ALL_CONTROLLERS_OFF	0x79
#define MIDI_LOCAL_CONTROL		0x7a
#define MIDI_ALL_NOTES_OFF		0x7b
#define MIDI_OMNI_OFF			0x7c
#define MIDI_OMNI_ON			0x7d
#define MIDI_MONO_ALL_NOTES_OFF		0x7e
#define MIDI_POLY_ALL_NOTES_OFF		0x7f

/* General MIDI 2 */
#define MIDI_BANK_RYTHM			0x78
#define MIDI_BANK_MELODY		0x79

/* RPN messages */
#define MIDI_PITCH_BEND_SENSITIVITY	0x0000
#define MIDI_CHANNEL_FINE_TUNING	0x0001
#define MIDI_CHANNEL_COARSE_TUNING	0x0002
#define MIDI_TUNING_PROGRAM_CHANGE	0x0003
#define MIDI_TUNING_BANK_SELECT		0x0004
#define MIDI_MODULATION_DEPTH_RANGE	0x0005
#define MIDI_PARAMETER_RESET		0x3fff

#define MAX_REGISTERED_PARAM		0x0006

/* system common messages */
#define MIDI_TIMING_CODE		0x01
#define MIDI_POSITION_POINTER		0x02
#define MIDI_SONG_SELECT		0x03
#define MIDI_TUNE_REQUEST		0x06

/* real-time messages */
#define MIDI_TIMING_CLOCK		0x08
#define MIDI_START			0x0a
#define MIDI_CONTINUE			0x0b
#define MIDI_STOP			0x0c
#define MIDI_ACTIVE_SENSE		0x0e
#define MIDI_SYSTEM_RESET		0x0f

/* status messages */
// https://learn.sparkfun.com/tutorials/midi-tutorial/advanced-messages
#define MIDI_FILE_META_EVENT            0xff

/* our own */
#define MIDI_DRUMS_CHANNEL		0x9
#define MIDI_FILE_FORMAT_MAX		0x3

enum {
    MIDI_MODE0 = 0,
    MIDI_GENERAL_MIDI1,
    MIDI_GENERAL_MIDI2,
    MIDI_GENERAL_STANDARD,
    MIDI_XG_MIDI,

    MIDI_MODE_MAX
};

enum {
    MIDI_POLYPHONIC = 3,
    MIDI_MONOPHONIC
};

struct param_t
{
   uint8_t coarse;
   uint8_t fine;
};

class MIDIChannel;

class MIDI : public AeonWave
{
private:
    typedef std::map<uint8_t,std::pair<uint8_t,std::string>> _patch_t;

public:
    MIDI(const char* n, const char *tnames = nullptr,
         enum aaxRenderMode m=AAX_MODE_WRITE_STEREO);
    MIDI(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        MIDI(n, nullptr, m) {}

    virtual ~MIDI() {
        for(auto it=buffers.begin(); it!=buffers.end(); ++it) {
            aaxBufferDestroy(it->second.second); it->second.first = 0;
        }
        buffers.clear();
    }

    bool process(uint8_t channel, uint8_t message, uint8_t key, uint8_t velocity, bool omni, float pitch=1.0f);

    MIDIChannel& new_channel(uint8_t channel, uint16_t bank, uint8_t program);

    MIDIChannel& channel(uint8_t channel_no);

    inline std::map<uint16_t,MIDIChannel*>& channel() {
        return channels;
    }

    inline void set_drum_file(std::string p) { drum = p; }
    inline void set_instrument_file(std::string p) { instr = p; }
    inline void set_file_path(std::string p) {
        set(AAX_SHARED_DATA_DIR, p.c_str()); path = p;
    }

    inline std::vector<std::string>& get_track_names() { return track_names; }

    inline const char* get_track_name(uint16_t t) {
        return (!t || track_names.size()<t) ? nullptr : track_names[t-1].c_str();
    }
    inline void set_track_active(uint16_t t) {
        active_track.push_back(t);
    }
    inline bool is_track_active(uint16_t t) {
        return active_track.empty() ? true : std::find(active_track.begin(), active_track.end(), t) != active_track.end();
    }

    void read_instruments();

    void grep(std::string& filename, const char *grep);
    inline void load(std::string& name) { loaded.push_back(name); }

    void rewind();

    void finish(uint8_t n);
    bool finished(uint8_t n);

    void set_gain(float);
    void set_balance(float);

    bool is_drums(uint8_t);

    inline void set_tuning(float pitch) { tuning = powf(2.0f, pitch/12.0f); }
    inline float get_tuning() { return tuning; }

    inline void set_mode(uint8_t m) { if (m > mode) mode = m; }
    inline uint8_t get_mode() { return mode; }

    inline void set_grep(bool g) { grep_mode = g; }
    inline bool get_grep() { return grep_mode; }

    const std::pair<std::string,int> get_drum(uint16_t program, uint8_t key);
    const std::pair<std::string,int> get_instrument(uint16_t bank, uint8_t program);
    std::map<std::string,_patch_t>& get_patches() { return patches; }

    inline void set_initialize(bool i) { initialize = i; };
    inline bool get_initialize() { return initialize; }

    inline void set_verbose(bool v) { verbose = v; }
    inline bool get_verbose() { return verbose; }

    inline void set_lyrics(bool v) { lyrics = v; }
    inline bool get_lyrics() { return lyrics; }

    inline void set_format(uint16_t fmt) { format = fmt; }
    inline uint16_t get_format() { return format; }

    inline void set_tempo(uint32_t tempo) { uSPP = tempo/PPQN; }

    inline void set_uspp(uint32_t uspp) { uSPP = uspp; }
    inline int32_t get_uspp() { return uSPP; }

    inline void set_ppqn(uint16_t ppqn) { PPQN = ppqn; }
    inline uint16_t get_ppqn() { return PPQN; }

    void set_chorus_level(float lvl);
    void set_chorus_depth(float depth);
    void set_chorus_rate(float rate);

    void set_reverb_type(uint8_t value);
    void set_reverb(const char *t) {
        Buffer &buf = AeonWave::buffer(t);
        Sensor::add(buf);
    }
    inline void set_decay_depth(float rt) { decay_depth = 0.1f*rt/decay_level; }

    // ** buffer management ******
    Buffer& buffer(std::string& name, int level=0) {
        if (level) { name = name + "?patch=" + std::to_string(level); }
        auto it = buffers.find(name);
        if (it == buffers.end()) {
           Buffer *b = new Buffer(ptr,name.c_str(),false,true);
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
                aaxBufferDestroy(it->second.second);
                buffers.erase(it); break;
            }
        }
    }
    bool buffer_avail(std::string &name) {
        auto it = buffers.find(name);
        if (it == buffers.end()) return false;
        return true;
    }

    MIDI &midi = *this;
private:
    std::string preset_file(aaxConfig c, std::string& name) {
        std::string rv = aaxDriverGetSetup(c, AAX_SHARED_DATA_DIR);
        rv.append("/"); rv.append(name);
        return rv;
    }

    std::string aaxs_file(aaxConfig c, std::string& name) {
        std::string rv = aaxDriverGetSetup(c, AAX_SHARED_DATA_DIR);
        rv.append("/"); rv.append(name); rv.append(".aaxs");
        return rv;
    }

    void add_patch(const char *patch);

    std::map<uint16_t,MIDIChannel*> channels;
    std::map<uint16_t,std::string> frames;
    std::map<uint16_t,std::map<uint16_t,std::pair<std::string,int>>> drums;
    std::map<uint16_t,std::map<uint16_t,std::pair<std::string,int>>> instruments;
    std::map<std::string,_patch_t> patches;

    std::unordered_map<std::string,std::pair<size_t,Buffer*>> buffers;
    Buffer nullBuffer;

    std::vector<std::string> loaded;

    std::vector<std::string> track_names;
    std::vector<uint16_t> active_track;

    std::pair<std::string,int> empty_map = {"", 0};
    std::string instr = "gmmidi.xml";
    std::string drum = "gmdrums.xml";
    std::string path;

    float tuning = 1.0f;

    uint32_t uSPP = 500000/24;
    uint16_t format = 0;
    uint16_t PPQN = 24;

    uint8_t mode = MIDI_MODE0;
    bool initialize = false;
    bool verbose = false;
    bool lyrics = false;
    bool grep_mode = false;

    uint8_t reverb_type = 4;
    float decay_level = 1.0f;
    Param decay_depth;
};


class MIDIChannel : public Instrument
{
private:
    MIDIChannel(const MIDIChannel&) = delete;

    MIDIChannel& operator=(const MIDIChannel&) = delete;

public:
    MIDIChannel(MIDI& ptr, std::string& dir, std::string& ifile, std::string& dfile, Buffer &buffer, uint8_t channel, uint16_t bank, uint8_t program, bool is_drums)
       : Instrument(ptr, channel == MIDI_DRUMS_CHANNEL), midi(ptr),
         channel_no(channel), bank_no(bank), program_no(program),
         drum_channel(channel == MIDI_DRUMS_CHANNEL ? true : is_drums)
    {
        if (drum_channel && buffer) {
           Mixer::add(buffer);
        }
        Mixer::set(AAX_PLAYING);
    }

    MIDIChannel(MIDIChannel&&) = default;

    MIDIChannel& operator=(MIDIChannel&&) = default;

    void play(uint8_t key_no, uint8_t velocity, float pitch);

    inline void set_drums(bool d = true) { drum_channel = d; }
    inline bool is_drums() { return drum_channel; }

    inline uint8_t get_channel_no() { return channel_no; }
    inline uint8_t get_program_no() { return program_no; }
    inline uint16_t get_bank_no() { return bank_no; }

    inline void set_tuning(float pitch) { tuning = powf(2.0f, pitch/12.0f); }
    inline float get_tuning() { return tuning; }

    inline void set_semi_tones(float s) { semi_tones = s; }
    inline float get_semi_tones() { return semi_tones; }

    inline void set_modulation_depth(float d) { modulation_range = d; }
    inline float get_modulation_depth() { return modulation_range; }

    inline bool get_pressure_volume_bend() { return pressure_volume_bend; }
    inline bool get_pressure_pitch_bend() { return pressure_pitch_bend; }

private:
    std::pair<uint8_t,std::string> get_patch(std::string& name, uint8_t& key);

    std::map<uint8_t,Buffer&> name_map;

    MIDI &midi;

    Buffer nullBuffer;

    float tuning = 1.0f;
    float modulation_range = 2.0f;
    float semi_tones = 2.0f;

    uint16_t bank_no = 0;
    uint8_t channel_no = 0;
    uint8_t program_no = 0;

    bool drum_channel = false;
    bool pressure_volume_bend = true;
    bool pressure_pitch_bend = false;
};


class MIDITrack : public byte_stream
{
public:
    MIDITrack() = default;

    MIDITrack(MIDI& ptr, byte_stream& stream, size_t len,  uint16_t track)
        : byte_stream(stream, len), midi(ptr), channel_no(track)
    {
        timestamp_parts = pull_message()*24/600000;
    }

    MIDITrack(const MIDITrack&) = default;

    ~MIDITrack() = default;

    void rewind();
    bool process(uint64_t, uint32_t&, uint32_t&);

    MIDI& midi;
private:
    inline float cents2pitch(float p, uint8_t channel) {
        float r = midi.channel(channel).get_semi_tones();
        return powf(2.0f, p*r/12.0f);
    }
    inline float cents2modulation(float p, uint8_t channel) {
        float r = midi.channel(channel).get_modulation_depth();
        return powf(2.0f, p*r/12.0f);
    }

    uint32_t pull_message();
    bool registered_param(uint8_t, uint8_t, uint8_t);

    uint8_t mode = 0;
    uint8_t channel_no = 0;
    uint8_t program_no = 0;
    uint16_t bank_no = 0;
    int16_t track_no = -1;

    uint8_t previous = 0;
    uint32_t wait_parts = 0;
    uint64_t timestamp_parts = 0;
    bool polyphony = true;
    bool omni = true;

    bool registered = false;
    uint16_t msb_type = 0;
    uint16_t lsb_type = 0;
    struct param_t param[MAX_REGISTERED_PARAM+1] = {
        { 2, 0 }, { 0x40, 0 }, { 0x20, 0 }, { 0, 0 }, { 0, 0 }, { 1, 0 }
    };

    const std::string type_name[5] = {
        "Text", "Copyright", "Track", "Instrument", "Lyrics"
    };
    const std::string csv_name[5] = {
        "Text_t", "Copyright_t", "Title_t", "Instrument_name_t", "Lyrics_t"
    };
};


class MIDIFile : public MIDI
{
public:
    MIDIFile(const char *filename) : MIDIFile(nullptr, filename) {}

    MIDIFile(const char *devname, const char *filename, const char *track=nullptr);

    explicit MIDIFile(std::string& devname, std::string& filename)
       :  MIDIFile(devname.c_str(), filename.c_str()) {}

    inline operator bool() {
        return midi_data.capacity();
    }

    void initialize(const char *grep = nullptr);
    inline void start() { midi.set(AAX_PLAYING); }
    inline void stop() { midi.set(AAX_PROCESSED); }
    void rewind();

    inline float get_duration_sec() { return duration_sec; }
    inline float get_pos_sec() { return pos_sec; }

    bool process(uint64_t, uint32_t&);

private:
    std::string file;
    std::vector<uint8_t> midi_data;
    std::vector<MIDITrack*> track;

    uint16_t no_tracks = 0;
    float duration_sec = 0.0f;
    float pos_sec = 0.0f;

    const std::string format_name[MIDI_FILE_FORMAT_MAX+1] = {
        "MIDI File 0", "MIDI File 1", "MIDI File 2",
        "Unknown MIDI File format"
    };
    const std::string mode_name[MIDI_MODE_MAX] = {
        "MIDI", "General MIDI 1.0", "General MIDI 2.0", "GS MIDI", "XG MIDI"
    };
};

} // namespace aax


#endif
