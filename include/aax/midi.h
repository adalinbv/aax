/*
 * Copyright 2005-2021 by Erik Hofman
 * Copyright 2007-2021 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* References:
 * - Universal MIDI Packet (UMP) Formatand MIDI 2.0 Protocol, Version 1.0
 *   Association of Musical Electronics Industry AMEI and
 *   MIDI Manufacturers Association MMA, M2-104-UM, http://www.midi.org
 */

#ifndef AAX_MIDI_H
#define AAX_MIDI_H 1

#if defined(__cplusplus)

extern "C" {
#endif

#include <aax/aax.h>

/* system exclusive */
#define MIDI_SYSTEM_EXCLUSIVE					0xf0	// 240
#define MIDI_SYSTEM_EXCLUSIVE_END				0xf7	// 247
#define MIDI_SYSTEM_EXCLUSIVE_NON_REALTIME			0x7e	// 126
#define MIDI_SYSTEM_EXCLUSIVE_REALTIME				0x7f	// 127

#define MIDI_SYSTEM_EXCLUSIVE_E_MU				0x18	// 24
#define MIDI_SYSTEM_EXCLUSIVE_ROLAND				0x41	// 65
#define MIDI_SYSTEM_EXCLUSIVE_KORG				0x42	// 66
#define MIDI_SYSTEM_EXCLUSIVE_YAMAHA				0x43	// 67
#define MIDI_SYSTEM_EXCLUSIVE_CASIO				0x44	// 68

/* system exclusive non-real time message */
#define MIDI_EOF						0x7b	// 123
#define MIDI_WAIT						0x7c	// 124
#define MIDI_CANCEL						0x7d	// 125
#define MIDI_NAK						0x7e	// 126
#define MIDI_ACK						0x7f	// 127

/* system exclusive real time message */
#define MIDI_BROADCAST						0x7f

#define MIDI_DEVICE_CONTROL					0x04

#define MIDI_DEVICE_VOLUME					0x01
#define MIDI_DEVICE_BALANCE					0x02
#define MIDI_DEVICE_FINE_TUNING					0x03
#define MIDI_DEVICE_COARSE_TUNING				0x04
#define MIDI_GLOBAL_PARAMETER_CONTROL				0x05

/* global parameters */
#define MIDI_REVERB_PARAMETER					0x0101	// 257
#define MIDI_CHORUS_PARAMETER					0x0102	// 258


#define GENERAL_MIDI_SYSTEM					0x09
#define MIDI_CONTROLLER_DESTINATION				0x09

/* controller type */
#define MIDI_CHANNEL_PRESSURE					0x01
#define MIDI_CONTROLLER_CHANGE					0x03

/* control change */
#define MIDI_PARAM_PITCH					0x00
#define MIDI_PARAM_FILTER_CUTOFF				0x01
#define MIDI_PARAM_AMPLITUDE					0x02
#define MIDI_PARAM_LFO_PITCH_DEPTH				0x03
#define MIDI_PARAM_LFO_FILTER_DEPTH				0x04
#define MIDI_PARAM_LFO_AMPLITUDE_DEPTH				0x05

#define MIDI_TUNING_STANDARD					0x08
#define MIDI_TUNING_SINGLE_NOTE					0x02
#define MIDI_TUNING_SINGLE_NOTE_BANK				0x07
#define MIDI_TUNING_OCTAVE1					0x08
#define MIDI_TUNING_OCTAVE2					0x09

/* meta messages */
#define MIDI_SEQUENCE_NUMBER					0x00
#define MIDI_TEXT						0x01
#define MIDI_COPYRIGHT						0x02
#define MIDI_TRACK_NAME						0x03
#define MIDI_INSTRUMENT_NAME					0x04
#define MIDI_LYRICS						0x05
#define MIDI_MARKER						0x06
#define MIDI_CUE_POINT						0x07
#define MIDI_DEVICE_NAME					0x09
#define MIDI_CHANNEL_PREFIX					0x20	// 32
#define MIDI_PORT_PREFERENCE					0x21	// 33
#define MIDI_END_OF_TRACK					0x2f	// 47
#define MIDI_SET_TEMPO						0x51	// 81
#define MIDI_SMPTE_OFFSET					0x54	// 84
#define MIDI_TIME_SIGNATURE					0x58	// 88
#define MIDI_KEY_SIGNATURE					0x59	// 89
#define MIDI_SEQUENCERSPECIFICMETAEVENT				0x7f	// 127

/* channel messages */
#define MIDI_NOTE_OFF						0x80	// 128
#define MIDI_NOTE_ON						0x90	// 144
#define MIDI_POLYPHONIC_AFTERTOUCH				0xa0	// 160
#define MIDI_CONTROL_CHANGE					0xb0	// 176
#define MIDI_PROGRAM_CHANGE					0xc0	// 192
#define MIDI_CHANNEL_AFTERTOUCH					0xd0	// 208
#define MIDI_PITCH_BEND						0xe0	// 224
#define MIDI_SYSTEM						0xf0	// 240

/* controller messages */
#define MIDI_COARSE						0x00
#define MIDI_FINE						0x20	// 32

#define MIDI_BANK_SELECT					0x00
#define MIDI_MODULATION_DEPTH					0x01
#define MIDI_BREATH_CONTROLLER					0x02
#define MIDI_FOOT_CONTROLLER					0x04
#define MIDI_PORTAMENTO_TIME					0x05
#define MIDI_DATA_ENTRY						0x06
#define MIDI_CHANNEL_VOLUME					0x07
#define MIDI_BALANCE						0x08
#define MIDI_PAN						0x0a	// 10
#define MIDI_EXPRESSION						0x0b	// 11
#define MIDI_EFFECT_CONTROL1					0x0c	// 12
#define MIDI_EFFECT_CONTROL2					0x0d	// 13
#define MIDI_MIDI_MODULATION_VELOCITY				0x0e	// 14
#define MIDI_SOFT_RELEASE					0x0f	// 15
#define MIDI_GENERAL_PURPOSE_CONTROL1				0x10	// 16
#define MIDI_GENERAL_PURPOSE_CONTROL2				0x11	// 17
#define MIDI_GENERAL_PURPOSE_CONTROL3				0x12	// 18
#define MIDI_GENERAL_PURPOSE_CONTROL4				0X13	// 19
#define MIDI_DAMPER_PEDAL_SWITCH				0x40	// 64
#define MIDI_PORTAMENTO_SWITCH					0x41	// 65
#define MIDI_SOSTENUTO_SWITCH					0x42	// 66
#define MIDI_SOFT_PEDAL_SWITCH					0x43	// 67
#define MIDI_LEGATO_SWITCH					0x44	// 68
#define MIDI_HOLD2						0x45	// 69
#define MIDI_SOUND_VARIATION					0x46	// 70
#define MIDI_FILTER_RESONANCE					0x47	// 71
#define MIDI_RELEASE_TIME					0x48	// 72
#define MIDI_ATTACK_TIME					0x49	// 73
#define MIDI_CUTOFF						0x4a	// 74
#define MIDI_DECAY_TIME						0x4b	// 75
#define MIDI_VIBRATO_RATE					0x4c	// 76
#define MIDI_VIBRATO_DEPTH					0x4d	// 77
#define MIDI_VIBRATO_DELAY					0x4e	// 78
#define MIDI_SOUND_CONTROL10					0x4f	// 79
#define MIDI_GENERAL_PURPOSE_CONTROL5				0x50	// 80
#define MIDI_GENERAL_PURPOSE_CONTROL6				0x51	// 81
#define MIDI_GENERAL_PURPOSE_CONTROL7				0x52	// 82
#define MIDI_GENERAL_PURPOSE_CONTROL8				0x53	// 83
#define MIDI_PORTAMENTO_CONTROL					0x54	// 84
#define MIDI_HIGHRES_VELOCITY_PREFIX				0x58	// 88
#define MIDI_REVERB_SEND_LEVEL					0x5b	// 91
#define MIDI_TREMOLO_EFFECT_DEPTH				0x5c	// 92
#define MIDI_CHORUS_SEND_LEVEL					0x5d	// 93
#define MIDI_CELESTE_EFFECT_DEPTH				0x5e	// 94
#define MIDI_PHASER_EFFECT_DEPTH				0x5f	// 95
#define MIDI_DATA_INCREMENT					0x60	// 96
#define MIDI_DATA_DECREMENT					0x61	// 97
#define MIDI_UNREGISTERED_PARAM_FINE				0x62	// 98
#define MIDI_UNREGISTERED_PARAM_COARSE				0x63	// 99
#define MIDI_REGISTERED_PARAM_FINE				0x64	// 100
#define MIDI_REGISTERED_PARAM_COARSE				0x65	// 101
#define MIDI_ALL_SOUND_OFF					0x78	// 120
#define MIDI_ALL_CONTROLLERS_OFF				0x79	// 121
#define MIDI_LOCAL_CONTROL					0x7a	// 122
#define MIDI_ALL_NOTES_OFF					0x7b	// 123
#define MIDI_OMNI_OFF						0x7c	// 124
#define MIDI_OMNI_ON						0x7d	// 125
#define MIDI_MONO_ALL_NOTES_OFF					0x7e	// 126
#define MIDI_POLY_ALL_NOTES_OFF					0x7f	// 127

/* General MIDI 2 */
#define MIDI_BANK_RYTHM						0x78	// 120
#define MIDI_BANK_MELODY					0x07

/* RPN messages */
#define MIDI_PITCH_BEND_SENSITIVITY				0x0000
#define MIDI_CHANNEL_FINE_TUNING				0x0001
#define MIDI_CHANNEL_COARSE_TUNING				0x0002
#define MIDI_TUNING_PROGRAM_CHANGE				0x0003
#define MIDI_TUNING_BANK_SELECT					0x0004
#define MIDI_MODULATION_DEPTH_RANGE				0x0005
#define MIDI_MPE_CONFIGURATION_MESSAGE				0x0006

/* Three Dimensional Sound Controllers */
#define MIDI_AZIMUTH_ANGLE					0x3d00
#define MIDI_ELEVATION_ANGLE					03dx01
#define MIDI_GAIN						0x3d02
#define MIDI_DISTANCE_RATIO					0x3d03
#define MIDI_MAXIMUM_DISTANCE					0x3d04
#define MIDI_GAIN_AT_MAXIMUM_DISTANCE				0x3d05
#define MIDI_REFERENCE_DISTANCE_RATIO				0x3d06
#define MIDI_PAN_SPREAD_ANGLE					0x3d07
#define MIDI_ROLL_ANGLE						0x3d08

#define MIDI_PARAMETER_RESET					0x3fff
#define MIDI_NULL_FUNCTION_NUMBER				0x7f7f

/* system common messages */
#define MIDI_TIMING_CODE					0x01
#define MIDI_POSITION_POINTER					0x02
#define MIDI_SONG_SELECT					0x03
#define MIDI_TUNE_REQUEST					0x06

/* real-time messages */
#define MIDI_TIMING_CLOCK					0x08
#define MIDI_START						0x0a	// 10
#define MIDI_CONTINUE						0x0b	// 11
#define MIDI_STOP						0x0c	// 12
#define MIDI_ACTIVE_SENSE					0x0e	// 14
#define MIDI_SYSTEM_RESET					0x0f	// 15

/* status messages */
#define MIDI_FILE_META_EVENT					0xff	// 255

/* MIDI 2.0: Universal MIDI Packet */
#define MIDI2_UTILITY_MESSAGE					0x0
#define MIDI2_SYSTEM_REAL_TIME_MESSAGE				0x1
#define MIDI2_COMMON_MESSAGE					0x1
#define MIDI1_VOICE_MESSAGE					0x2
#define MIDI2_SYSTEM_EXCLUSIVE_MESSAGE				0x3
#define MIDI2_VOICE_MESSAGE					0x4
#define MIDI2_DATA_MESSAGE					0x5

#if defined(__cplusplus)
}	/* extern "C" */

#include <functional>

#ifndef NDEBUG
# define THROW(...)	std::throw(std::domain_error(...)
#else
# define THROW(...)
#endif

namespace aax
{

namespace midi
{

// a container class to handle universal packets
class packet
{
private:
    // MIDI 2.0
    uint8_t message_type = 0;
    uint8_t group_no = 0;
    int16_t index = 0;

    // universal
    uint8_t status = 0;
    int32_t data[4] = { 0, 0, 0, 0 };

    inline uint16_t cvt2x7to14bit_unsigned(uint32_t v) {
        return (v & 0x7f) | ((v & 0x7f00) >> 1);
    }
    inline int16_t cvt2x7to14bit_signed(int32_t v) {
        return (v & 0x7f) | ((v & 0x7f00) >> 1) - 8192;
    }

protected:
    std::function<void()> update = []() { };

public:
    packet() = default;
    ~packet() = default;

    inline uint8_t get_message_type() { return message_type; }
    inline uint8_t get_group_no() { return group_no; }

    // MIDI 1.0: construct the packet ourselves
    inline void set_status_channel(uint8_t s) {
        status = s; message_type = MIDI1_VOICE_MESSAGE;
    }

    inline void set_value_msb(uint8_t m) {
        data[0] = ((data[0] & 0x00ff) | (uint32_t(m) << 8));
    }
    inline void set_value_lsb(uint8_t l) {
         data[0] = ((data[0] & 0xff00) | uint32_t(l));
    }

    // MIDI 2.0
    inline void set_message(uint32_t m) {
        message_type = (m >> 28);
        group_no = ((m >> 24) & 0xf);
        status = ((m >> 16) & 0xff);
        data[0] = ((m & 0xffff) << 16);
    }
    inline void set_message(uint64_t m) {
        data[1] = (m & 0xffffffff);
        set_message(m >> 32);
    }
    inline void set_data(uint8_t i, uint32_t d) {
        switch(message_type)
        {
        case MIDI2_UTILITY_MESSAGE:
        case MIDI2_SYSTEM_REAL_TIME_MESSAGE:
        case MIDI1_VOICE_MESSAGE:
            THROW("set_data index no. " + i + "unsupported for "
                 "message type" + message_type);
            break;
        case MIDI2_SYSTEM_EXCLUSIVE_MESSAGE:
        case MIDI2_VOICE_MESSAGE:
        case MIDI2_DATA_MESSAGE:
            if (i == 1) data[i] = d;
            else THROW("set_data index no. " + i + "unsupported for"
                 "message type " + message_type);
            break;
        default:
            THROW("set_data is not supported for message type " +
               message_type);
            break;
        }
    }

    // - note on/off
    inline uint8_t get_attribute_type() {
        if (message_type == MIDI2_VOICE_MESSAGE) {
            return (data[0] & 0xff);
        }
        THROW("get_attribute_type is not supported for message type " +
               message_type);
        return 0;
    }
    inline uint16_t get_attribute() {
        if (message_type == MIDI2_VOICE_MESSAGE) {
            return (data[1] & 0xffff);
        }
        THROW("get_attribute is not supported for message type " +
               message_type);
        return 0;
    }

    // - registered/assignable controller
    inline bool is_controller_message() {
        uint8_t s = (status >> 4);
        return (message_type == MIDI2_VOICE_MESSAGE && (s == 2 || s == 3));
    }
    inline bool is_program_change_message() {
        uint8_t s = (status >> 4);
        return (message_type == MIDI2_VOICE_MESSAGE && s == 12);
    }
    inline void set_program_no(uint8_t p) {
        data[1] = ((data[1] & 0x00ffffff) | (p << 24));
    }
    inline void set_bank_msb(uint8_t m) {
        data[1] = ((data[1] & 0xffff00ff) | (m << 8));
    }
    inline void set_bank_lsb(uint8_t l) {
        data[1] = ((data[1] & 0xffffff00) | l);
    }

    // universal
    // - note on/off
    inline bool is_voice_message() {
       return (message_type == MIDI2_VOICE_MESSAGE ||
               message_type == MIDI1_VOICE_MESSAGE);
    }
    inline bool is_note_message() {
        uint8_t s = is_voice_message() ? (status & 0xf0) : 0;
        return (s == MIDI_NOTE_ON || s == MIDI_NOTE_OFF);
    }

    inline uint8_t get_status() {
        return is_voice_message() ? (status & 0xf0) : status;
    }
    inline uint8_t get_channel_no() {
        if (is_voice_message()) return (status & 0xf);
        THROW("get_channel_no is not supported for message type " + 
              message_type);
        return 0;
    }
    inline uint16_t get_note_no() {
        if (is_note_message()) return ((data[0] >> 8) & 0x7f);
        THROW("get_note_no is not supported for message type " +
              message_type);
        return 0;
    }
    inline uint16_t get_velocity() {
        if (is_note_message()) {
            if (message_type == MIDI2_VOICE_MESSAGE) {
                return ((data[1] >> 16) & 0xffff);
            } else {
                return ((data[0] & 0x7f) << 9);
            }
        }
        THROW("get_velocity is not supported for message type " +
              message_type);
        return 0;
    }
    inline uint16_t get_unsigned_value() {
        if (is_note_message()) {
            return cvt2x7to14bit_unsigned(data[0] & 0xffff);
        }
        THROW("get_unsigned_value is not supported for message type " +
              message_type);
        return 0;
    }
    inline int16_t get_signed_value() {
        if (is_note_message()) {
            return cvt2x7to14bit_signed(data[0] & 0xffff);
        }
        THROW("get_signed_value is not supported for message type " +
              message_type);
        return 0;
    }
};

} // namespace midi

} // namespace aax
#endif	// defined(__cplusplus)

#endif /* AAX_MIDI_H */

