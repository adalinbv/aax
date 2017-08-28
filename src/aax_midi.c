/*
 * Copyright 2014-2017 by Erik Hofman.
 * Copyright 2014-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <aax/aax.h>
#include <aax/midi.h>

#include <base/types.h>
#include <api.h>

#if USE_MIDI

int aaxMIDINoteOn(aaxConfig config, char channel, char note, char velocity);
int aaxMIDINoteOff(aaxConfig config, char channel, char note, char velocity);
int aaxMIDIPitchBend(aaxConfig config, char channel, char data1, char data2);
int aaxMIDIControlChange(aaxConfig config, char channel, char data1);
int aaxMIDIProgramChange(aaxConfig config, char channel, char data1);
int aaxMIDISystemExclusive(aaxConfig config, int32_t *stream);

int
aaxMIDIStream(aaxConfig config, int32_t *stream)
{
   int rv = AAX_FALSE;

   if (stream)
   {
      int32_t mvalue = *stream;
      int32_t status = (mvalue & 0xF0000000);
      char channel = ((mvalue & 0x0F000000) >> 24);
      char data1 = ((mvalue & 0x00FF0000) >> 16);
      char data2 = ((mvalue & 0x0000FF00) >> 8);
//    char data3 = (mvalue & 0x000000FF);

      switch(status)
      {
         case AAX_MIDI_NOTE_ON:
         case AAX_MIDI_NOTE_OFF:
            if (status == AAX_MIDI_NOTE_OFF ||
                ((status == AAX_MIDI_NOTE_ON) && data2 == 0))
            {
               rv = aaxMIDINoteOff(config, channel, data1, data2);
            } else { 
               rv = aaxMIDINoteOn(config, channel, data1, data2);
            }
            break;
         case AAX_MIDI_PITCH_BEND:
            rv = aaxMIDIPitchBend(config, channel, data1, data2);
            break;
         case AAX_MIDI_CONTROL_CHANGE:
            rv = aaxMIDIControlChange(config, channel, data1);
            break;
         case AAX_MIDI_PROGRAM_CHANGE:
            rv = aaxMIDIProgramChange(config, channel, data1);
            break;
         case AAX_MIDI_CHANNEL_AFTERTOUCH:
         case AAX_MIDI_POLYPHONIC_AFTERTOUCH:
            rv = AAX_TRUE;
            break;
         case AAX_MIDI_SYSTEM_EXCLUSIVE:
            rv = aaxMIDISystemExclusive(config, stream);
            break;
         default:
            break;
      }
   }
   else {
   }

   return rv;
}


int
aaxMIDINoteOn(aaxConfig config, char channel, char note, char velocity)
{
    int rv = AAX_FALSE;
    return rv;
}

int
aaxMIDINoteOff(aaxConfig config, char channel, char note, char velocity)
{
    int rv = AAX_FALSE;
    return rv;
}

int
aaxMIDIPitchBend(aaxConfig config, char channel, char data1, char data2)
{
    int rv = AAX_FALSE;
    return rv;
}

int
aaxMIDIControlChange(aaxConfig config, char channel, char data1)
{
    int rv = AAX_FALSE;
    return rv;
}

int
aaxMIDIProgramChange(aaxConfig config, char channel, char data1)
{
    int rv = AAX_FALSE;
    return rv;
}

int
aaxMIDISystemExclusive(aaxConfig config, int32_t *stream)
{
    int rv = AAX_FALSE;
    return rv;
}

/* -------------------------------------------------------------------------- */

struct _midi_note_t {
   int32_t note;	// node code in hex
   float freq;		// frequency in Hz
   char code_us[5];	// American notation
   char code_de[4];	// German notation
} midi_note[] =
{
   { 0x00000000,     8.176f, "C-2",  "C3"  },
   { 0x00010000,     8.662f, "C#-2", "C#3" },
   { 0x00020000,     9.177f, "D-2",  "D3"  },
   { 0x00030000,     9.723f, "D#-2", "D#3" },
   { 0x00040000,    10.301f, "E-2",  "E3"  },
   { 0x00050000,    10.913f, "F-2",  "F3"  },
   { 0x00060000,    11.562f, "F#-2", "F#3" },
   { 0x00070000,    12.250f, "G-2",  "G3"  },
   { 0x00080000,    12.978f, "G#-2", "G#3" },
   { 0x00090000,    13.750f, "A-2",  "A3"  },
   { 0x000a0000,    14.568f, "A#-2", "A#3" },
   { 0x000b0000,    15.434f, "B-2",  "B3"  },
   { 0x000c0000,    16.352f, "C-1",  "C2"  },
   { 0x000d0000,    17.324f, "C#-1", "C#2" },
   { 0x000e0000,    18.354f, "D-1",  "D2"  },
   { 0x000f0000,    19.445f, "D#-1", "D#2" },
   { 0x00100000,    20.602f, "E-1",  "E2"  },
   { 0x00110000,    21.827f, "F-1",  "F2"  },
   { 0x00120000,    23.125f, "F#-1", "F#2" },
   { 0x00130000,    24.500f, "G-1",  "G2"  },
   { 0x00140000,    25.957f, "G#-1", "G#2" },
   { 0x00150000,    27.500f, "A-1",  "A2"  },
   { 0x00160000,    29.135f, "A#-1", "A#2" },
   { 0x00170000,    30.868f, "B-1",  "B2"  },
   { 0x00180000,    32.703f, "C0",   "C1"  },
   { 0x00190000,    34.648f, "C#0",  "C#1" },
   { 0x001a0000,    36.708f, "D0",   "D1"  },
   { 0x001b0000,    38.891f, "D#0",  "D#1" },
   { 0x001c0000,    41.203f, "E0",   "E1"  },
   { 0x001d0000,    43.654f, "F0",   "F1"  },
   { 0x001e0000,    46.249f, "F#0",  "F#1" },
   { 0x001f0000,    48.999f, "G0",   "G1"  },
   { 0x00200000,    51.913f, "G#0",  "G#1" },
   { 0x00210000,    55.000f, "A0",   "A1"  },
   { 0x00220000,    58.270f, "A#0",  "A#1" },
   { 0x00230000,    61.735f, "B0",   "B1"  },
   { 0x00240000,    65.406f, "C1"    "c0"  },
   { 0x00250000,    69.296f, "C#1",  "c#0" },
   { 0x00260000,    73.416f, "D1",   "d0"  },
   { 0x00270000,    77.782f, "D#1",  "d#0" },
   { 0x00280000,    82.407f, "E1",   "e0"  },
   { 0x00290000,    87.307f, "F1",   "f0"  },
   { 0x002a0000,    92.499f, "F#1",  "f#0" },
   { 0x002b0000,    97.999f, "G1",   "g0"  },
   { 0x002c0000,   103.826f, "G#1",  "g#0" },
   { 0x002d0000,   110.000f, "A1",   "a0"  },
   { 0x002e0000,   116.541f, "A#1",  "a#0" },
   { 0x002f0000,   123.471f, "B1",   "b0"  },
   { 0x00300000,   130.813f, "C2",   "c1"  },
   { 0x00310000,   138.591f, "C#2",  "c#1" },
   { 0x00320000,   146.832f, "D2",   "d1"  },
   { 0x00330000,   155.563f, "D#2",  "d#1" },
   { 0x00340000,   164.814f, "E2",   "e1"  },
   { 0x00350000,   174.614f, "F2",   "f1"  },
   { 0x00360000,   184.997f, "F#2",  "f#1" },
   { 0x00370000,   195.998f, "G2",   "g1"  },
   { 0x00380000,   207.652f, "G#2",  "g#1" },
   { 0x00390000,   220.000f, "A2",   "a1"  },
   { 0x003a0000,   233.082f, "A#2",  "a#1" },
   { 0x003b0000,   246.942f, "B2",   "b1"  },
   { 0x003c0000,   261.626f, "C3",   "c2"  },
   { 0x003d0000,   277.183f, "C#3",  "c#2" },
   { 0x003e0000,   293.665f, "D3",   "d2"  },
   { 0x003f0000,   311.127f, "D#3",  "d#2" },
   { 0x00400000,   329.628f, "E3",   "e2"  },
   { 0x00410000,   349.228f, "F3",   "f2"  },
   { 0x00420000,   369.994f, "F#3",  "f#2" },
   { 0x00430000,   391.995f, "G3",   "g2"  },
   { 0x00440000,   415.305f, "G#3",  "g#2" },
   { 0x00450000,   440.000f, "A3",   "a2"  },
   { 0x00460000,   466.164f, "A#3",  "a#2" },
   { 0x00470000,   493.883f, "B3",   "b2"  },
   { 0x00480000,   523.251f, "C4",   "c3"  },
   { 0x00490000,   554.365f, "C#4",  "c#3" },
   { 0x004a0000,   587.330f, "D4",   "d3"  },
   { 0x004b0000,   622.254f, "D#4",  "d#3" },
   { 0x004c0000,   659.255f, "E4",   "e3"  },
   { 0x004d0000,   698.456f, "F4",   "f3"  },
   { 0x004e0000,   739.989f, "F#4",  "f#3" },
   { 0x004f0000,   783.991f, "G4",   "g3"  },
   { 0x00500000,   830.609f, "G#4",  "g#3" },
   { 0x00510000,   880.000f, "A4",   "a3"  },
   { 0x00520000,   932.328f, "A#4",  "a#3" },
   { 0x00530000,   987.767f, "B4",   "b3"  },
   { 0x00540000,  1046.502f, "C5",   "c4"  },
   { 0x00550000,  1108.731f, "C#5",  "c#4" },
   { 0x00560000,  1174.659f, "D5",   "d4"  },
   { 0x00570000,  1244.508f, "D#5",  "d#4" },
   { 0x00580000,  1318.510f, "E5",   "e4"  },
   { 0x00590000,  1396.913f, "F5",   "f4"  },
   { 0x005a0000,  1479.978f, "F#5",  "f#4" },
   { 0x005b0000,  1567.982f, "G5",   "g4"  },
   { 0x005c0000,  1661.219f, "G#5",  "g#4" },
   { 0x005d0000,  1760.000f, "A5",   "a4"  },
   { 0x005e0000,  1864.655f, "A#5",  "a#4" },
   { 0x005f0000,  1975.533f, "B5",   "b4"  },
   { 0x00600000,  2093.005f, "C6",   "c5"  },
   { 0x00610000,  2217.461f, "C#6",  "c#5" },
   { 0x00620000,  2349.318f, "D6",   "d5"  },
   { 0x00630000,  2489.016f, "D#6",  "d#5" },
   { 0x00640000,  2637.030f, "E6",   "e5"  },
   { 0x00650000,  2793.826f, "F6",   "f5"  },
   { 0x00660000,  2959.955f, "F#6",  "f#5" },
   { 0x00670000,  3135.963f, "G6",   "g5"  },
   { 0x00680000,  3322.438f, "G#6",  "g#5" },
   { 0x00690000,  3520.000f, "A6",   "a5"  },
   { 0x006a0000,  3729.310f, "A#6",  "a#5" },
   { 0x006b0000,  3951.066f, "B6",   "b5"  },
   { 0x006c0000,  4186.009f, "C7",   "c6"  },
   { 0x006d0000,  4434.922f, "C#7",  "c#6" },
   { 0x006e0000,  4698.636f, "D7",   "d6"  },
   { 0x006f0000,  4978.032f, "D#7",  "d#6" },
   { 0x00700000,  5274.041f, "E7",   "e6"  },
   { 0x00710000,  5587.652f, "F7",   "f6"  },
   { 0x00720000,  5919.911f, "F#7",  "f#6" },
   { 0x00730000,  6271.927f, "G7",   "g6"  },
   { 0x00740000,  6644.875f, "G#7",  "g#6" },
   { 0x00750000,  7040.000f, "A7",   "a6"  },
   { 0x00760000,  7458.620f, "A#7",  "a#6" },
   { 0x00770000,  7902.133f, "B7",   "b6"  },
   { 0x00780000,  8372.018f, "C8",   "c7"  },
   { 0x00790000,  8869.844f, "C#8",  "c#7" },
   { 0x007a0000,  9397.273f, "D8",   "d7"  },
   { 0x007b0000,  9956.063f, "D#8",  "d#7" },
   { 0x007c0000, 10548.082f, "E8",   "e7"  },
   { 0x007d0000, 11175.303f, "F8",   "f7"  },
   { 0x007e0000, 11839.922f, "F#8",  "f#7" },
   { 0x007f0000, 12543.854f, "G8",   "g7"  }
};

static const char* _midi_gm_instrument[] =
{
   "Acoustic Grand Piano",
   "Bright Acoustic Piano",
   "Electric Grand Piano",
   "Honky-Tonk Piano",
   "Electric Piano 1 (Rhodes)",
   "Electric Piano 2",
   "Harpsichord",
   "Clavinet",

   "Celesta",
   "Glockenspiel",
   "Music Box",
   "Vibraphone",
   "Marimba",
   "Xylophone",
   "Tubular Bells",
   "Dulcimer",

   "Drawbar Organ",
   "Percussive Organ",
   "Rock Organ",
   "Church Organ",
   "Reed Organ",
   "Accordion",
   "Harmonica",
   "Tango Accordion",

   "Acoustic uitar (Nylon)",
   "Acoustic Guitar (Steel)",
   "Electric Guitar (Jazz)",
   "Electric Guitar (Clean)",
   "Electric Guitar (Muted)",
   "Overdriven Guitar",
   "Distortion Guitar",
   "Guitar Harmonics",

   "Acoustic Bass",
   "Electric Bass (Finger)",
   "Electric Bass (Pick)",
   "Fretless Bass",
   "Slap Bass 1",
   "Slap Bass 2",
   "Synth Bass 1",
   "Synth Bass 2",

   "Violin",
   "Viola",
   "Cello",
   "Contrabass",
   "Tremolo Strings",
   "Pizzicato Strings",
   "Orchestral Harp",
   "Timpani",

   "String Ensemble 1",
   "String Ensemble 2",
   "Synth Strings 1",
   "Synth Strings 2",
   "Choir Aahs",
   "Voice Oohs",
   "Synth Choir",
   "Orchestral Hit",

   "Trumpet",
   "Trombone",
   "Tuba",
   "Muted Trumpet",
   "French Horn",
   "Brass Section",
   "Synth Brass 1",
   "Synth Brass 2",

   "Soprano Sax",
   "Alto Sax",
   "Tenor Sax",
   "Baritone Sax",
   "Oboe",
   "English Horn",
   "Bassoon",
   "Clarinet",

   "Piccolo",
   "Flute",
   "Recorder",
   "Pan Flute",
   "Blown Bottle",
   "Shakuhachi",
   "Whistle",
   "Ocarina",

   "Lead 1 (Square)",
   "Lead 2 (Sawtooth)",
   "Lead 3 (Calliope)",
   "Lead 4 (Chiff)",
   "Lead 5 (Charang)",
   "Lead 6 (Voice)",
   "Lead 7 (Fifths)",
   "Lead 8 (Bass+Lead)",

   "Pad 1 (New Age)",
   "Pad 2 (Warm)",
   "Pad 3 (Polysynth)",
   "Pad 4 (Choir)",
   "Pad 5 (Bowed)",
   "Pad 6 (Metallic)",
   "Pad 7 (Halo)",
   "Pad 8 (Sweep)",

   "FX 1 (Rain)",
   "FX 2 (Soundtrack)",
   "FX 3 (Crystal)",
   "FX 4 (Atmosphere)",
   "FX 5 (Brightness)",
   "FX 6 (Goblins)",
   "FX 7 (Echoes)",
   "FX 8 (Sci-Fi)",

   "Sitar",
   "Banjo",
   "Shamisen",
   "Koto",
   "Kalimba",
   "Bagpipe",
   "Fiddle",
   "Shanai",

   "Tinkle Bell",
   "Agogo",
   "Steel Drums",
   "Woodblock",
   "Taiko Drum",
   "Melodic Tom",
   "Synth Drum",
   "Reverse Cymbal",

   "Guitar Fret Noise",
   "Breath Noise",
   "Seashore",
   "Bird Tweet",
   "Telephone Ring",
   "Helicopter",
   "Applause",
   "Gunshot",
};

static const char* _midi_gm_percussion[] = 
{
   "Accoustic Bass Drum",		/* Note: starts at 35 */
   "Bass Drum 1",
   "Side Stick/Rimshot",
   "Acoustic Snare",
   "Hand Clap",
   "Electronic Snare",
   "Low Floor Tom",
   "Closed Hi-Hat",
   "High Floor Tom",
   "Pedal Hi-Hat",
   "Low Tom",
   "Open Hi-Hat",
   "Low Mid Tom",
   "High Mid Tom",
   "Crash Cymbal 1",
   "High Tom",
   "Ride Cymbal 1",
   "Chinese Cymbal",
   "Ride Bell",
   "Tambourine",
   "Splash Cymbal 1",
   "Cowbell",
   "Crash Cymbal 2",
   "Vibra Slap",
   "Ride Cymbal 2",
   "High Bongo",
   "Low Bongo",
   "Mute Hiqh Conga",
   "Open High Conga",
   "Low Conga",
   "High Timbale",
   "Low Timbale",
   "High Agogô",
   "Low Agogô",
   "Cabasa",
   "Maracas",
   "Short Whistle",
   "Long Whistle",
   "Short Güiro",
   "Long Güiro",
   "Claves",
   "High Wood Block",
   "Low Wood Block",
   "Mute Cuíca",
   "Open Cuíca",
   "Mute Triangle",
   "Open Triangle"
};

#endif
