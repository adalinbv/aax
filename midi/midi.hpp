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

#include <stdexcept>
#include <vector>
#include <map>

#include <aax/aeonwave.hpp>
#include <aax/instrument.hpp>
#include <aax/buffer_map.hpp>
#include <aax/byte_stream.hpp>

namespace aax
{

/* status messages */
// https://learn.sparkfun.com/tutorials/midi-tutorial/advanced-messages
#define MIDI_EXCLUSIVE_MESSAGE		0xf0
#define MIDI_SYSTEM_MESSAGE		0xff

/* midi exclusive non-real time message */
#define MIDI_EOF			0x7b
#define MIDI_WAIT			0x7c
#define MIDI_CANCEL			0x7d
#define MIDI_NAK			0x7e
#define MIDI_ACK			0x7f
#define MIDI_EXCLUSIVE_MESSAGE_END	0xf7

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
// https://www.recordingblogs.com/wiki/midi-controller-message
#define MIDI_COARSE			0x00
#define MIDI_FINE			0x20

#define MIDI_BANK_SELECT		0x00
#define MIDI_MODULATION_WHEEL		0x01
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
#define MIDI_FOOT_PEDAL			0x24
#define MIDI_HOLD_PEDAL1		0x40
#define MIDI_PORTAMENTO_PEDAL		0x41
#define MIDI_SOSTENUTO_PEDAL		0x42
#define MIDI_SOFT_PEDAL			0x43
#define MIDI_LEGATO_PEDAL		0x44
#define MIDI_HOLD_PEDAL2		0x45
#define MIDI_SOUND_CONTROL1		0x46
#define MIDI_SOUND_CONTROL2		0x47
#define MIDI_SOUND_CONTROL3		0x49
#define MIDI_SOUND_CONTROL4		0x49
#define MIDI_SOUND_CONTROL5		0x4a
#define MIDI_SOUND_CONTROL6		0x4b
#define MIDI_SOUND_CONTROL7		0x4c
#define MIDI_SOUND_CONTROL8		0x4d
#define MIDI_SOUND_CONTROL9		0x4e
#define MIDI_SOUND_CONTROL10		0x4f
#define MIDI_GENERAL_PURPOSE_CONTROL5	0x50
#define MIDI_GENERAL_PURPOSE_CONTROL6	0x51
#define MIDI_GENERAL_PURPOSE_CONTROL7	0x52
#define MIDI_GENERAL_PURPOSE_CONTROL8	0x53
#define MIDI_PORTAMENTO_CONTROL		0x54
#define MIDI_HIGHRES_VELOCITY_PREFIX	0x58
#define MIDI_EFFECT1_DEPTH		0x5b
#define MIDI_EFFECT2_DEPTH		0x5c
#define MIDI_EFFECT3_DEPTH		0x5d
#define MIDI_EFFECT4_DEPTH		0x5e
#define MIDI_EFFECT5_DEPTH		0x5f
#define MIDI_DATA_INCREMENT		0x60
#define MIDI_DATA_DECREMENT		0x61
#define MIDI_UNREGISTERED_PARAM_FINE	0x62
#define MIDI_UNREGISTERED_PARAM_COARSE	0x63
#define MIDI_REGISTERED_PARAM_FINE	0x64
#define MIDI_REGISTERED_PARAM_COARSE	0x65
#define MIDI_ALL_SOUND_OFF		0x78
#define MIDI_ALL_CONTROLLERS_OFF	0x79
#define MIDI_LOCAL_CONTROLL		0x7a
#define MIDI_ALL_NOTES_OFF		0x7b
#define MIDI_OMNI_OFF			0x7c
#define MIDI_OMNI_ON			0x7d
#define MIDI_MONO_ALL_NOTES_OFF		0x7e
#define MIDI_POLY_ALL_NOTES_OFF		0x7f

/* RPN messages */
#define MIDI_PITCH_BEND_RANGE		0x0000
#define MIDI_FINE_TUNING		0x0001
#define MIDI_COARSE_TUNING		0x0002
#define MIDI_TUNING_PROGRAM_CHANGE	0x0003
#define MIDI_TUNING_BANK_SELECT		0x0004
#define MIDI_MODULATION_DEPTH_RANGE	0x0005
#define MIDI_PARAMETER_RESET		0x7f7f

/* real-time messages */
#define MIDI_TIMING_CLOCK		0x08
#define MIDI_START			0x0a
#define MIDI_CONTINUE			0x0b
#define MIDI_STOP			0x0c
#define MIDI_ACTIVE_SENSE		0x0e
#define MIDI_SYSTEM_RESET		0x0f

/* our own */
#define MIDI_DRUMS_CHANNEL		0x9

struct param_t
{
   uint8_t coarse;
   uint8_t fine;
};

class MIDIChannel;

class MIDI : public AeonWave
{
public:
    MIDI(const char* n) : AeonWave(n) {
        if (*this) {
            path = AeonWave::info(AAX_SHARED_DATA_DIR);
        } else {
            throw(std::runtime_error("Unable to open device "+std::string(n)));
        }
    }

    bool process(uint8_t channel, uint8_t message, uint8_t key, uint8_t velocity, bool omni);

    MIDIChannel& new_channel(uint8_t channel, uint8_t bank, uint8_t program);

    MIDIChannel& channel(uint8_t channel_no);

    inline std::map<uint8_t,MIDIChannel*>& channel() {
        return channels;
    }

    inline void set_drum_file(std::string p) { drum = p; }
    inline void set_instrument_file(std::string p) { instr = p; }
    inline void set_file_path(std::string p) {
        set(AAX_SHARED_DATA_DIR, p.c_str()); path = p;
    }

    void read_instruments();

    void rewind();

    std::string get_drum(uint8_t bank, uint8_t key);
    std::string get_instrument(uint8_t bank, uint8_t program);

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


private:
    std::map<uint8_t,MIDIChannel*> channels;
    std::map<uint8_t,std::map<uint8_t,std::string>> drums;
    std::map<uint8_t,std::map<uint8_t,std::string>> instruments;

    std::string empty_str = "";
    std::string instr = "gmmidi.xml";
    std::string drum = "gmdrums.xml";
    std::string path;

    uint16_t format = 0;
    uint16_t PPQN = 24;
    uint32_t uSPP = 500000/24;

    bool initialize = false;
    bool verbose = false;
    bool lyrics = false;
};


class MIDIChannel : public Instrument
{
private:
    MIDIChannel(const MIDIChannel&) = delete;

    MIDIChannel& operator=(const MIDIChannel&) = delete;

public:
    MIDIChannel(MIDI& ptr, std::string& dir, std::string& ifile, std::string& dfile, uint8_t channel, uint8_t bank, uint8_t program)
       : Instrument(ptr, channel == MIDI_DRUMS_CHANNEL), midi(ptr),
         channel_no(channel), bank_no(bank), program_no(program)
    {
        Mixer::set(AAX_PLAYING);
    }

    MIDIChannel(MIDIChannel&&) = default;

    MIDIChannel& operator=(MIDIChannel&&) = default;

    void play(uint8_t key_no, uint8_t velocity);

    inline void set_semi_tones(float s) { semi_tones = s; }
    inline float get_semi_tones() { return semi_tones; }

private:
    std::map<uint8_t,Buffer&> name_map;

    MIDI &midi;
    float semi_tones = 2.0f;
    uint8_t channel_no = 0;
    uint8_t program_no = 0;
    uint8_t bank_no = 0;
};


#define MAX_REGISTERED_PARAM 	6

class MIDITrack : public byte_stream
{
public:
    MIDITrack() = default;

    MIDITrack(MIDI& ptr, byte_stream& stream, size_t len,  uint16_t track)
        : byte_stream(stream, len), midi(ptr), channel_no(track)
    {
        abstime = pull_message()*24/600000;
    }

    MIDITrack(const MIDITrack&) = default;

    ~MIDITrack() = default;

    void rewind();
    bool process(uint64_t, uint32_t&, uint32_t&);

private:
    uint32_t pull_message();
    float registered_param(uint8_t);

    MIDI& midi;

    uint8_t channel_no = 0;
    uint8_t program_no = 0;
    uint8_t bank_no = 0;

    uint8_t previous = 0;
    uint32_t tlapse = 0;
    uint64_t abstime = 0;
    bool polyphony = true;
    bool omni = false;

    uint16_t msb_type = 0;
    uint16_t lsb_type = 0;
    struct param_t param[MAX_REGISTERED_PARAM+1] = {
        { 2, 0 }, { 0x20, 0 }, { 0x20, 0 }, { 0, 0 }, { 0, 0 }, { 1, 0 }
    };

    const std::string type_name[5] = {
        "Text", "Copyright", "Track", "Instrument", "Lyrics"
    };
};


class MIDIFile : public MIDI
{
public:
    MIDIFile(const char *filename) : MIDIFile(nullptr, filename) {}

    MIDIFile(const char *devname, const char *filename);

    MIDIFile(std::string& devname, std::string& filename)
       :  MIDIFile(devname.c_str(), filename.c_str()) {}

    inline operator bool() {
        return midi_data.capacity();
    }

    void initialize();
    inline void start() { MIDI::set(AAX_PLAYING); }
    inline void stop() { MIDI::set(AAX_PROCESSED); }
    void rewind();

    inline float get_duration_sec() { return duration_sec; }
    inline float get_pos_sec() { return pos_sec; }

    bool process(uint64_t, uint32_t&);

private:
    std::vector<uint8_t> midi_data;
    std::vector<MIDITrack*> track;

    uint16_t no_tracks = 0;
    float duration_sec = 0.0f;
    float pos_sec = 0.0f;
};

} // namespace aax


#endif
