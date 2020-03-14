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

namespace aax {
namespace MIDI {

// aax::MIDI::MIDI

MIDI::MIDI(aaxConfig ptr) : config(ptr)
{
   channel_mask[0] = 0xFFFF;
   set_path();

   reverb.tie(reverb_decay_depth, AAX_REVERB_EFFECT, AAX_DECAY_DEPTH);
   reverb.tie(reverb_cutoff_frequency, AAX_REVERB_EFFECT, AAX_CUTOFF_FREQUENCY);
   reverb.tie(reverb_state, AAX_REVERB_EFFECT);
   config.add(*this);
}

MIDI::~MIDI()
{
   for(auto it : buffers)
   {
      aaxBufferDestroy(*it.second.second);
      it.second.first = 0;
   }
   buffers.clear();

   config.remove(*this);
}

void
MIDI::initialize()
{
   set(AAX_REFRESH_RATE, 100);
   set(AAX_INITIALIZED);
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
MIDI::stop(bool processed)
{
   if (processed)
   {
      reverb.set(AAX_PROCESSED);
      Mixer::set(AAX_PROCESSED);
   }
}

void
MIDI::finish(uint32_t n)
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

bool
MIDI::isRealTime(uint32_t message)
{
   bool rv = false;
   if (message & MIDI_SYSTEM_EXCLUSIVE)
   {
      switch(message & ~MIDI_SYSTEM_EXCLUSIVE)
      {
      case MIDI_TIMING_CLOCK:
      case MIDI_START:
      case MIDI_CONTINUE:
      case MIDI_STOP:
      case MIDI_ACTIVE_SENSE:
      case MIDI_SYSTEM_RESET:
         rv = true;
         break;
      default:
         break;
      }
   }
   return rv;
}

void
MIDI::push(uint32_t message)
{
   if (isRealTime(message)) {
      process_realtime(message);
   }
   else if (data.size() || isStatus(message))
   {
      data.push_back(message);
      if (isExclusive(data[0]) && message == MIDI_SYSTEM_EXCLUSIVE_END) {
         process_exclusive();
      } else {
         process_common();
      }
   }
}

uint32_t
MIDI::pull_message()
{
   uint32_t rv = 0;

   for (int i=0; i<4; ++i)
   {
      uint32_t byte = pull_byte();

      rv = (rv << 7) | (byte & 0x7f);
      if ((byte & 0x80) == 0) {
         break;
      }
   }

   return rv;
}

bool
MIDI::registered_param(uint32_t channel_no, uint32_t controller, uint32_t value)
{
    uint16_t type = value;
    bool data = false;
    bool rv = true;
    if (type > MAX_REGISTERED_PARAM) {
        type = MAX_REGISTERED_PARAM;
    }

    switch(controller)
    {
    case MIDI_REGISTERED_PARAM_COARSE:
        msb_type = type;
        break;
    case MIDI_REGISTERED_PARAM_FINE:
        lsb_type = type;
        break;
    case MIDI_DATA_ENTRY:
        if (registered)
        {
            param[msb_type].coarse = value;
            data = true;
        }
        break;
    case MIDI_DATA_ENTRY|MIDI_FINE:
        if (registered)
        {
            param[lsb_type].fine = value;
            data = true;
        }
        break;
    case MIDI_DATA_INCREMENT:
        type = msb_type << 8 | lsb_type;
        if (++param[type].fine == 128) {
            param[type].coarse++;
            param[type].fine = 0;
        }
        break;
    case MIDI_DATA_DECREMENT:
        type = msb_type << 8 | lsb_type;
        if (param[type].fine == 0) {
            param[type].coarse--;
            param[type].fine = 127;
        } else {
            param[type].fine--;
        }
        break;
    case MIDI_UNREGISTERED_PARAM_FINE:
    case MIDI_UNREGISTERED_PARAM_COARSE:
        break;
    default:
        rv = false;
        break;
    }

    if (data)
    {
        type = msb_type << 8 | lsb_type;
        switch(type)
        {
        case MIDI_PITCH_BEND_SENSITIVITY:
        {
            float val;
            val = (float)param[MIDI_PITCH_BEND_SENSITIVITY].coarse +
                  (float)param[MIDI_PITCH_BEND_SENSITIVITY].fine*0.01f;
            channel(channel_no).set_semi_tones(val);
            break;
        }
        case MIDI_MODULATION_DEPTH_RANGE:
        {
            float val;
            val = (float)param[MIDI_MODULATION_DEPTH_RANGE].coarse +
                  (float)param[MIDI_MODULATION_DEPTH_RANGE].fine*0.01f;
            channel(channel_no).set_modulation_depth(val);
            break;
        }
        case MIDI_PARAMETER_RESET:
            channel(channel_no).set_semi_tones(2.0f);
            break;
        case MIDI_CHANNEL_FINE_TUNING:
        {
            uint16_t tuning = param[MIDI_CHANNEL_FINE_TUNING].coarse << 7
                              | param[MIDI_CHANNEL_FINE_TUNING].fine;
            float pitch = (float)tuning-8192.0f;
            if (pitch < 0) pitch /= 8192.0f;
            else pitch /= 8191.0f;
            channel(channel_no).set_tuning(pitch);
            break;
        }
        case MIDI_CHANNEL_COARSE_TUNING:
            // This is handled by MIDI_NOTE_ON and MIDI_NOTE_OFF
            break;
        case MIDI_TUNING_PROGRAM_CHANGE:
        case MIDI_TUNING_BANK_SELECT:
            break;
        default:
            break;
        }
    }

    return rv;
}

float
MIDI::cents2pitch(float p, uint32_t channel_no)
{
   float r = channel(channel_no).get_semi_tones();
   return powf(2.0f, p*r/12.0f);
}

float
MIDI::cents2modulation(float p, uint32_t channel_no)
{
   float r = channel(channel_no).get_modulation_depth();
   return powf(2.0f, p*r/12.0f);
}

bool
MIDI::process(uint32_t channel_no, uint32_t message, uint32_t key, uint32_t velocity, float pitch)
{
   // Omni mode: Device responds to MIDI data regardless of channel
   if (message == MIDI_NOTE_ON && velocity) {
      channel(channel_no).play(key, velocity, pitch);
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

void
MIDI::process_realtime(uint32_t message)
{
}

void
MIDI::process_exclusive()
{
   uint64_t size = pull_message();
   const char *s = NULL;
   uint32_t byte;

   while (data.size() && (byte = pull_byte()) != MIDI_SYSTEM_EXCLUSIVE_END)
   {
      switch(byte)
      {
      case MIDI_SYSTEM_EXCLUSIVE_ROLAND:
          byte = pull_byte();
          if (byte != 0x10) break;

          byte = pull_byte();
          if (byte != 0x42) break;

          byte = pull_byte();
          if (byte != 0x12) break;

          byte = pull_byte();
          if (byte == 0x40)
          {
              byte = pull_byte();
              switch(byte)
              {
              case 0x00:
                  byte = pull_byte();
                  if (byte != 0x7f) break;;

                  byte = pull_byte();
                  if (byte != 0x00) break;

                  byte = pull_byte();
                  if (byte == 0x41) {
                      set_mode(GENERAL_STANDARD);
                  }
                  break;
              case 0x19:
              case 0x1a:
                  byte = pull_byte();
                  if (byte != 0x15) break;

                  byte = pull_byte();
                  if (byte == 0x02)
                  {
                      byte = pull_byte();
                      if (byte == 0x10) channel(9).set_drums(true);
                      else if (byte == 0xf) channel(11).set_drums(true);
                  }
                  break;
              default:
                  break;
              }
          }
          break;
      case MIDI_SYSTEM_EXCLUSIVE_YAMAHA:
          byte = pull_byte();
          if (byte != 0x10) break;

          byte = pull_byte();
          if (byte != 0x4c) break;

          byte = pull_byte();
          if (byte != 0x00) break;

          byte = pull_byte();
          if (byte != 0x00) break;

          byte = pull_byte();
          if (byte != 0x7e) break;

          byte = pull_byte();
          if (byte == 0x00) {
              set_mode(EXTENDED_GENERAL_MIDI);
          }
          break;
      case MIDI_SYSTEM_EXCLUSIVE_NON_REALTIME:
          // GM1 rewind: F0 7E 7F 09 01 F7
          // GM2 rewind: F0 7E 7F 09 03 F7
          // GS  rewind: F0 41 10 42 12 40 00 7F 00 41 F7
          byte = pull_byte();
          if (byte == 0x7F)
          {
              byte = pull_byte();
              switch(byte)
              {
              case GENERAL_MIDI_SYSTEM:
                  byte = pull_byte();
                  set_mode(byte);
                  switch(byte)
                  {
                  case 0x01:
                      for (auto it : channels) {
                         uint32_t channel_no = it.second->get_channel_no();
                         process(channel_no, MIDI_NOTE_OFF, 0, 0, true);
                      }
                      set_mode(GENERAL_MIDI1);
                      break;
                  case 0x02:
                      // set_mode(MODE0);
                      break;
                  case 0x03:
                      for (auto it : channels) {
                         uint32_t channel_no = it.second->get_channel_no();
                         process(channel_no, MIDI_NOTE_OFF, 0, 0, true);
                      }
                      set_mode(GENERAL_MIDI2);
                      break;
                  default:
                      break;
                  }
                  break;
              case MIDI_EOF:
              case MIDI_WAIT:
              case MIDI_CANCEL:
              case MIDI_NAK:
              case MIDI_ACK:
                  break;
              default:
                  break;
              }
          }
          break;
      case MIDI_SYSTEM_EXCLUSIVE_REALTIME:
          byte = pull_byte();
          switch(byte)
          {
          case MIDI_DEVICE_CONTROL:
              byte = pull_byte();
              switch(byte)
              {
              case MIDI_DEVICE_VOLUME:
                  byte = pull_byte();
                  set_gain((float)byte/127.0f);
                  break;
              case MIDI_DEVICE_BALANCE:
                  byte = pull_byte();
                  set_balance(((float)byte-64.0f)/64.0f);
                  break;
              case MIDI_DEVICE_FINE_TUNING:
              {
                  uint32_t tuning = pull_byte() || pull_byte() << 7;
                  float pitch = (float)tuning-8192.0f;
                  if (pitch < 0) pitch /= 8192.0f;
                  else pitch /= 8191.0f;
                  set_tuning(pitch);
                  break;
              }
              case MIDI_DEVICE_COARSE_TUNING:
              {
                  float pitch;
                  byte = pull_byte();     // lsb, always zero
                  byte = pull_byte();     // msb
                  pitch = (float)byte-64.0f;
                  if (pitch < 0) pitch /= 64.0f;
                  else pitch /= 63.0f;
                  set_tuning(pitch);
                  break;
              }
              case MIDI_GLOBAL_PARAMETER_CONTROL:
              {
                  uint32_t path_len = pull_byte();
                  uint32_t id_width = pull_byte();
                  uint32_t val_width = pull_byte();
                  uint32_t slot = pull_byte() || pull_byte() << 7;
                  uint32_t param =  pull_byte();
                  uint32_t value = pull_byte();
                  switch(slot)
                  {
                  case MIDI_CHORUS_PARAMETER:
                      switch(param)
                      {
                      case 0:     // CHORUS_TYPE
                          switch(value)
                          {
                          case 0:
                              set_chorus("chorus/chorus1");
                              break;
                          case 1:
                              set_chorus("chorus/chorus2");
                              break;
                          case 2:
                              set_chorus("chorus/chorus3");
                              break;
                          case 3:
                              set_chorus("chorus/chorus4");
                              break;
                          case 4:
                              set_chorus("chorus/chorus_freedback");
                              break;
                          case 5:
                              set_chorus("chorus/flanger");
                              break;
                          default:
                              break;
                          }
                          break;
                      case 1:     // CHORUS_MOD_RATE
                      // the modulation frequency in Hz
                          set_chorus_rate(0.122f*value);
                          break;
                      case 2:     // CHORUS_MOD_DEPTH
                      {
                      // the peak-to-peak swing of the modulation in ms
                          float ms = 1e-3f*(value+1.0f)/3.2f;
                          set_chorus_depth(ms);
                          break;
                      }
                      case 3:     // CHORUS_FEEDBACK
                          set_chorus_level(0.763f*value);
                      // the amount of feedback from Chorus output in %
                          break;
                      case 4:     // CHORUS_SEND_TO_REVERB
                      // the send level from Chorus to Reverb in %
                          set_chorus_level(0.787f*value);
                      default:
                         break;
                      }
                      break;
                  case MIDI_REVERB_PARAMETER:
                      switch(param)
                      {
                      case 0:     // Reverb Typ
                          set_reverb_type(value);
                          break;
                      case 1:     //Reverb Time
                      {
                          float rt = expf((value-40)*0.025);
                          set_decay_depth(rt);
                      }
                      default:
                         break;
                      }
                      break;
                  default:
                      break;
                  }
                  break;
              }
              default:
                  byte = pull_byte();
                  break;
              }
              break;
          default:
              break;
          }
      default:
          break;
      }
   } // while
}

void
MIDI::process_common()
{
   while (data.size())
   {
      uint32_t message = pull_byte();
      uint32_t channel_no = message & 0xf;
      switch(message & 0xf0)
      {
      case MIDI_NOTE_ON:
      {
          uint32_t key = pull_byte();
          uint32_t velocity = pull_byte();
          float pitch = 1.0f;
          if (!channel(channel_no).is_drums()) {
              key = (key-0x20) + param[MIDI_CHANNEL_COARSE_TUNING].coarse;
              pitch = channel(channel_no).get_tuning();
              pitch *= get_tuning();
          }
          process(channel_no, message & 0xf0, key, velocity, pitch);
          break;
      }
      case MIDI_NOTE_OFF:
      {
          uint32_t key = pull_byte();
          uint32_t velocity = pull_byte();
          if (!channel(channel_no).is_drums()) {
              key = (key-0x20) + param[MIDI_CHANNEL_COARSE_TUNING].coarse;
          }
          process(channel_no, message & 0xf0, key, velocity, omni_enabled);
          break;
      }
      case MIDI_POLYPHONIC_AFTERTOUCH:
      {
          uint32_t key = pull_byte();
          uint32_t pressure = pull_byte();
          if (!channel(channel_no).is_drums())
          {
              float s = channel(channel_no).get_aftertouch_sensitivity();
              if (channel(channel_no).get_pressure_pitch_bend()) {
                  channel(channel_no).set_pitch(key, cents2pitch(s*pressure/127.0f, channel_no));
              }
              if (channel(channel_no).get_pressure_volume_bend()) {
                  channel(channel_no).set_pressure(key, 1.0f-0.33f*pressure/127.0f);
              }
          }
          break;
      }
      case MIDI_CHANNEL_AFTERTOUCH:
      {
          uint32_t pressure = pull_byte();
          if (!channel(channel_no).is_drums())
          {
              float s = channel(channel_no).get_aftertouch_sensitivity();
              if (channel(channel_no).get_pressure_pitch_bend()) {
                  channel(channel_no).set_pitch(cents2pitch(s*pressure/127.0f, channel_no));
              }
              if (channel(channel_no).get_pressure_volume_bend()) {
                  channel(channel_no).set_pressure(1.0f-0.33f*pressure/127.0f);
              }
          }
          break;
      }
      case MIDI_PITCH_BEND:
      {
          uint32_t pitch = pull_byte() | pull_byte() << 7;
          float pitch_bend = (float)pitch-8192.0f;
          if (pitch_bend < 0) pitch_bend /= 8192.0f;
          else pitch_bend /= 8191.0f;
          pitch_bend = cents2pitch(pitch_bend, channel_no);
          channel(channel_no).set_pitch(pitch_bend);
          break;
      }
      case MIDI_PROGRAM_CHANGE:
      {
          uint32_t program_no = pull_byte();
          try {
              new_channel(channel_no, bank_no, program_no);
          } catch(const std::invalid_argument& e) {
              std::cerr << "Error: " << e.what() << std::endl;
          }
          break;
      }
      case MIDI_CONTROL_CHANGE:
          process_control_change(channel_no);
          break;
      case MIDI_SYSTEM:
          switch(channel_no)
          {
          case MIDI_TIMING_CODE:
              pull_byte();
              break;
          case MIDI_POSITION_POINTER:
              pull_byte();
              pull_byte();
              break;
          case MIDI_SONG_SELECT:
              pull_byte();
              break;
          case MIDI_TUNE_REQUEST:
              break;
          case MIDI_SYSTEM_RESET:
#if 0
              omni_enabled = true;
              polyphony = true;
              for(auto& it : channel())
              {
                  process(it.first, MIDI_NOTE_OFF, 0, 0, true);
                  channel(channel_no).set_semi_tones(2.0f);
              }
#endif
             break;
          case MIDI_TIMING_CLOCK:
          case MIDI_START:
          case MIDI_CONTINUE:
          case MIDI_STOP:
          case MIDI_ACTIVE_SENSE:
              break;
          default:
              break;
          }
          break;
      } // switch
   } // while
}

void
MIDI::process_control_change(uint32_t channel_no)
{
   uint32_t controller = pull_byte();
   uint32_t value = pull_byte();
   switch(controller)
   {
   case MIDI_ALL_CONTROLLERS_OFF:
       channel(channel_no).set_modulation(0.0f);
       channel(channel_no).set_expression(1.0f);
       channel(channel_no).set_hold(false);
//     channel(channel_no).set_portamento(false);
       channel(channel_no).set_sustain(false);
       channel(channel_no).set_soft(false);
       channel(channel_no).set_semi_tones(2.0f);
       channel(channel_no).set_pitch(1.0f);
       msb_type = lsb_type = 0x7F;
       // channel(channel_no).set_gain(100.0f/127.0f);
       // channel(channel_no).set_pan(0.0f);
       // intentional falltrough
   case MIDI_MONO_ALL_NOTES_OFF:
       process(channel_no, MIDI_NOTE_OFF, 0, 0, true);
       if (value == 1) {
           polyphony_enabled = false;
           channel(channel_no).set_monophonic(true);
       }
       break;
   case MIDI_POLY_ALL_NOTES_OFF:
       process(channel_no, MIDI_NOTE_OFF, 0, 0, true);
       channel(channel_no).set_monophonic(false);;
       polyphony_enabled = true;
       break;
   case MIDI_ALL_SOUND_OFF:
       process(channel_no, MIDI_NOTE_OFF, 0, 0, true);
       break;
   case MIDI_OMNI_OFF:
       process(channel_no, MIDI_NOTE_OFF, 0, 0, true);
       omni_enabled = false;
       break;
   case MIDI_OMNI_ON:
       process(channel_no, MIDI_NOTE_OFF, 0, 0, true);
       omni_enabled = true;
       break;
   case MIDI_BANK_SELECT:
       if (value == MIDI_BANK_RYTHM) {
           channel(channel_no).set_drums(true);
       } else if (value == MIDI_BANK_MELODY) {
           channel(channel_no).set_drums(false);
       }
       bank_no = (uint32_t)value << 7;
       break;
   case MIDI_BANK_SELECT|MIDI_FINE:
       bank_no += value;;
       break;
   case MIDI_BREATH_CONTROLLER:
       if (!channel(channel_no).is_drums()) {
           channel(channel_no).set_pressure(1.0f-0.33f*value/127.0f);
       }
       break;
   case MIDI_BALANCE:
       // If a MultiTimbral device, then each Part usually has its
       // own Balance. This is generally when Balance becomes
       // useful, because then you can use Pan, Volume, and Balance
       // controllers to internally mix all of the Parts to the
       // device's stereo outputs
       break;
   case MIDI_PAN:
       channel(channel_no).set_pan(((float)value-64.0f)/64.0f);
       break;
   case MIDI_EXPRESSION:
       channel(channel_no).set_expression((float)value/127.0f);
       break;
   case MIDI_MODULATION_DEPTH:
   {
       float depth = (float)(value << 7)/16383.0f;
       depth = cents2modulation(depth, channel_no) - 1.0f;
       channel(channel_no).set_modulation(depth);
       break;
   }
   case MIDI_CELESTE_EFFECT_DEPTH:
   {
       float level = (float)value/127.0f;
       level = cents2pitch(level, channel_no);
       channel(channel_no).set_detune(level);
       break;
   }
   case MIDI_CHANNEL_VOLUME:
       channel(channel_no).set_gain((float)value/127.0f);
       break;
   case MIDI_ALL_NOTES_OFF:
       for(auto& it : channel())                 {
           process(it.first, MIDI_NOTE_OFF, 0, 0, true);
           channel(channel_no).set_semi_tones(2.0f);
       }
       break;
   case MIDI_UNREGISTERED_PARAM_COARSE:
   case MIDI_UNREGISTERED_PARAM_FINE:
       registered = false;
       registered_param(channel_no, controller, value);
       break;
   case MIDI_REGISTERED_PARAM_COARSE:
   case MIDI_REGISTERED_PARAM_FINE:
       registered = true;
       registered_param(channel_no, controller, value);
       break;
   case MIDI_DATA_ENTRY:
   case MIDI_DATA_ENTRY|MIDI_FINE:
   case MIDI_DATA_INCREMENT:
   case MIDI_DATA_DECREMENT:
       registered_param(channel_no, controller, value);
       break;
   case MIDI_SOFT_PEDAL_SWITCH:
       channel(channel_no).set_soft(value >= 0x40);
       break;
   case MIDI_LEGATO_SWITCH:
       break;
   case MIDI_DAMPER_PEDAL_SWITCH:
       channel(channel_no).set_hold(value >= 0x40);
       break;
   case MIDI_SOSTENUTO_SWITCH:
       channel(channel_no).set_sustain(value >= 0x40);
       break;
   case MIDI_REVERB_SEND_LEVEL:
       set_reverb_level(channel_no, value);
       break;
   case MIDI_CHORUS_SEND_LEVEL:
   {
       float val = (float)value/127.0f;
       channel(channel_no).set_chorus_level(val);
       break;
   }
   case MIDI_FILTER_RESONANCE:
   {
       float val = -1.0f+(float)value/64.0f; // relative: 0.0 - 2.0
       channel(channel_no).set_filter_resonance(val);
       break;
   }
   case MIDI_CUTOFF:       // Brightness
   {
       float val = (float)value/64.0f;
       if (val < 1.0f) val = 0.5f + 0.5f*val;
       channel(channel_no).set_filter_cutoff(val);
       break;
   }
   case MIDI_VIBRATO_RATE:
   {
       float val = 0.5f + (float)value/64.0f;
       channel(channel_no).set_vibrato_rate(val);
       break;
   }
   case MIDI_VIBRATO_DEPTH:
   {
       float val = (float)value/64.0f;
       channel(channel_no).set_vibrato_depth(val);
       break;
   }
   case MIDI_VIBRATO_DELAY:
   {
       float val = (float)value/64.0f;
       channel(channel_no).set_vibrato_delay(val);
       break;
   }
   case MIDI_PORTAMENTO_TIME:
   {
       float val = powf(10.0f, 2.0f-3.0f*value/127.0f);
       val = cents2pitch(val, channel_no)*1e-3f;
       channel(channel_no).set_pitch_rate(val);
       break;
   }

   case MIDI_PORTAMENTO_SWITCH:
       channel(channel_no).set_pitch_rate(value >= 0x40);
       break;
   case MIDI_RELEASE_TIME:
       channel(channel_no).set_release_time(value);
       break;
   case MIDI_ATTACK_TIME:
       channel(channel_no).set_attack_time(value);
       break;
   case MIDI_DECAY_TIME:
       channel(channel_no).set_decay_time(value);
       break;
   case MIDI_TREMOLO_EFFECT_DEPTH:
       channel(channel_no).set_tremolo_depth((float)value/64.0f);
       break;
   case MIDI_PHASER_EFFECT_DEPTH:
       channel(channel_no).set_phaser_depth((float)value/64.0f);
       break;
   case MIDI_PORTAMENTO_CONTROL:
   case MIDI_HOLD2:
   case MIDI_PAN|MIDI_FINE:
   case MIDI_EXPRESSION|MIDI_FINE:
   case MIDI_BREATH_CONTROLLER|MIDI_FINE:
   case MIDI_BALANCE|MIDI_FINE:
   case MIDI_SOUND_VARIATION:
   case MIDI_SOUND_CONTROL10:
   case MIDI_HIGHRES_VELOCITY_PREFIX:
   case MIDI_GENERAL_PURPOSE_CONTROL1:
   case MIDI_GENERAL_PURPOSE_CONTROL2:
   case MIDI_GENERAL_PURPOSE_CONTROL3:
   case MIDI_GENERAL_PURPOSE_CONTROL4:
   case MIDI_GENERAL_PURPOSE_CONTROL5:
   case MIDI_GENERAL_PURPOSE_CONTROL6:
   case MIDI_GENERAL_PURPOSE_CONTROL7:
   case MIDI_GENERAL_PURPOSE_CONTROL8:
       break;
   default:
       break;
   }
}

void
MIDI::set_path()
{
   path = config.info(AAX_SHARED_DATA_DIR);

   std::string name = path;
   if (mode == AAX_RENDER_NORMAL) {
      name.append("/ultrasynth/");
   }
   if (exists(name))
   {
      path = name;
      config.set(AAX_SHARED_DATA_DIR, path.c_str());
   }
}

void
MIDI::set_port_mask(uint32_t mask)
{
   int port = 0;
   while (mask != 0)
   {
      if (mask & 0x1) channel_mask[port] = 0xFFFF;
      mask >>= 1;
      port++;
   }
}

uint32_t
MIDI::get_port_mask()
{
   uint32_t rv = 0;
   for (auto it : channel_mask) {
      rv |= (1 << it.first);
   }
   return rv;
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
//  m.inverse();
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
      float val = static_cast<float>(value)/127.0f;
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
                  mode = AAX_RENDER_SYNTHESIZER;
               }
               else if (!xmlAttributeCompareString(xmid, "mode", "arcade"))
               {
                  mode = AAX_RENDER_ARCADE;
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
//          std::cerr << "aeonwave/midi not found in: " << filename << std::endl;
         }
         xmlClose(xid);
      }
      else {
//       std::cerr << "Unable to open: " << filename << std::endl;
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
         }
         else
         {
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
         if (iti != bank.end())
         {
            return iti->second;
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
            return iti->second;
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
   return empty_map;
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
      if (it != frames.end())
      {
         level = it->first;
         name = it->second;
      }
   }

   Buffer& buffer = config.buffer(name, level);
   if (buffer) {
      buffer.set(AAX_CAPABILITIES, static_cast<int>(mode));
   }

   try
   {
      auto ret = channels.insert(
         { channel_no, new Channel(*this, buffer, channel_no,
                                   bank_no, program_no, drums)
         } );
      it = ret.first;
      Mixer::add(*it->second);
   } catch(const std::invalid_argument& e) {
//     throw;
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
   auto it = name_map.begin();
   if (midi.channel(channel_no).is_drums())
   {
      it = name_map.find(key_no);
      if (it == name_map.end())
      {
         auto inst = midi.get_drum(program_no, key_no, true);
         std::string name = inst.first;
         if (!name.empty())
         {
            Buffer& buffer = midi.buffer(name);
            if (buffer)
            {
               auto ret = name_map.insert({key_no,buffer});
               it = ret.first;
            }
            else {
//              throw(std::invalid_argument("Instrument file "+name+" could not load"));
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
         auto inst = midi.get_instrument(bank_no, program_no, true);
         auto patch = get_patch(inst.first, key);
         std::string patch_name = patch.second;
         uint32_t level = patch.first;
         if (!patch_name.empty())
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

   if (it != name_map.end())
   {
      if (midi.channel(channel_no).is_drums())
      {
         switch(program_no)
         {
         case 0:    // Standard Set
         case 16:   // Power set
         case 32:   // Jazz set
         case 40:   // Brush set
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
         case 26:   // Analog Set
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
         case 48:   // Orchestra Set
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
         case 57:   // SFX Set
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
//     throw(std::invalid_argument("Instrument file "+name+" not found"));
   }
}


}; // namespace MIDI
}; // namespace aax
