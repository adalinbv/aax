/*
 * Copyright 2014-2017 by Erik Hofman.
 * Copyright 2014-2017 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef AAX_MIDI_H
#define AAX_MIDI_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(TARGET_OS_MAC) && TARGET_OS_MAC
# pragma export on
#endif

#define AAX_MIDI_NOTE_OFF                        0x80000000
#define AAX_MIDI_NOTE_ON                         0x90000000
#define AAX_MIDI_POLYPHONIC_AFTERTOUCH           0xA0000000
#define AAX_MIDI_CONTROL_CHANGE                  0xB0000000
#define AAX_MIDI_PROGRAM_CHANGE                  0xC0000000
#define AAX_MIDI_CHANNEL_AFTERTOUCH              0xD0000000
#define AAX_MIDI_PITCH_BEND                      0xE0000000

   /* system messages */
#define AAX_MIDI_SYSTEM_EXCLUSIVE                0xF0000000
#define AAX_MIDI_TIME_CODE_QUARTER_FRAME         0xF1000000
#define AAX_MIDI_SONG_POSITION_POINTER           0xF2000000
#define AAX_MIDI_SONG_SELECT                     0xF3000000
#define AAX_MIDI_TUNE_REQUEST                    0xF6000000
#define AAX_MIDI_END_OF_EXCLUSIVE                0xF7000000
#define AAX_MIDI_TIMING_CLOCK                    0xF8000000
#define AAX_MIDI_START                           0xFA000000
#define AAX_MIDI_CONDTINUE                       0xFB000000
#define AAX_MIDI_STOP                            0xFC000000
#define AAX_MIDI_ACTIVE_SENSING                  0xFE000000
#define AAX_MIDI_SYSTEM_RESET                    0xFF000000

#define AAX_MIDI_MODULATION                      0x00010000
#define AAX_MIDI_BREATH_CONTROLLER               0x00020000
#define AAX_MIDI_FOOT_CONTROLLER                 0x00040000
#define AAX_MIDI_PORTAMENTO_TIME                 0x00050000
#define AAX_MIDI_DATA_ENTRY                      0x00060000
#define AAX_MIDI_MAIN_VOLUME                     0x00070000
#define AAX_MIDI_BALANCE                         0x00080000
#define AAX_MIDI_PAN                             0x000A0000
#define AAX_MIDI_EXPRESSION_CONTROLLER           0x000B0000

#define AAX_MIDI_GENERAL_PURPOSE_14              0x00100000
#define AAX_MIDI_CONTROLLER_LSB                  0x00200000
#define AAX_MIDI_DAMPER_PEDAL                    0x00400000
#define AAX_MIDI_PORTAMENTO                      0x00410000
#define AAX_MIDI_SOSTENUTO                       0x00420000
#define AAX_MIDI_SOFT_PEDAL                      0x00430000
#define AAX_MIDI_HOLD2                           0x00450000
#define AAX_MIDI_GENERAL_PURPOSE_58              0x00500000
#define AAX_MIDI_EXTERNAL_EFFECT_DEPTH           0x005B0000
#define AAX_MIDI_TREMOLO_DEPTH                   0x005C0000
#define AAX_MIDI_CHORUS_DEPTH                    0x005D0000
#define AAX_MIDI_CELESTE_DEPTH                   0x005E0000        // detune
#define AAX_MIDI_PHASER_DEPTH                    0x005F0000
#define AAX_MIDI_DATA_INCREMENT                  0x00600000
#define AAX_MIDI_DATA_DECREMENT                  0x00610000
#define AAX_MIDI_NONREGISTERED_PARAMETER_NO_LSB  0x00620000
#define AAX_MIDI_NONREGISTERED_PARAMETER_NO_MSB  0x00630000
#define AAX_MIDI_REGISTERED_PARAMETER_NO_LSB     0x00640000
#define AAX_MIDI_REGISTERED_PARAMETER_NO_MSB     0x00650000
#define AAX_MIDI_CONTROLLER_UNDEFINED_6          0x00660000
#define AAX_MIDI_MODE_MESSAGE                    0x00790000

#if defined(TARGET_OS_MAC) && TARGET_OS_MAC
# pragma export off
#endif

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif

