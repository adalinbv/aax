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
#include <vector>
#include <queue>
#include <map>

#include <aax/midi.h>
#include <aax/instrument.hpp>


namespace aax {
namespace MIDI {
#define DRUMS_CHANNEL		0x9

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
   MIDI(aaxConfig);
   virtual ~MIDI();

   void initialize();
   void start();
   void stop(bool processed=false);

   void finish(uint32_t n);
   bool finished(uint32_t n);

   void push(uint32_t message);
   bool process(uint32_t channel, uint32_t message, uint32_t key, uint32_t velocity, float pitch=1.0f);
   void process_realtime(uint32_t message);
   void process_exclusive();
   void process_common();

   void process_control_change(uint32_t channel_no);

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

   void read_instruments(std::string gmidi=std::string(), std::string gmdrums=std::string());

   void set_gain(float);
   void set_balance(float);

   bool is_drums(uint32_t);

   inline void set_capabilities(enum aaxCapabilities m) {
      mode = m; set(AAX_CAPABILITIES, m); set_path();
   }

   inline unsigned int get_refresh_rate() { return refresh_rate; }
   inline unsigned int get_polyphony() { return polyphony; }

   inline void set_tuning(float pitch) { tuning = powf(2.0f, pitch/12.0f); }
   inline float get_tuning() { return tuning; }

   const auto get_drum(uint32_t program, uint32_t key, bool all=false);
   const auto get_instrument(uint32_t bank, uint32_t program, bool all=false);
   auto& get_patches() { return patches; }

   inline void set_omni_enabled(bool o) { omni_enabled = 0; }

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

   inline auto& get_config() { return config; }

   uint32_t get_port_mask();
   void set_port_mask(uint32_t mask);

   inline uint32_t get_channel_mask(uint32_t port) {
      return channel_mask[port];
   }
   inline void set_channel_mask(uint32_t port, uint32_t mask) {
      channel_mask[port] = mask;
   }
   inline bool is_channel_active(uint32_t port, uint32_t mask) {
      return (omni_enabled || (channel_mask[port] & (1 << mask)) != 0);
   }

private:
   float cents2pitch(float p, uint32_t channel_no);
   float cents2modulation(float p, uint32_t channel_no);

   inline bool isStatus(uint32_t message) { return (message & 0x80); }
   inline bool isExclusive(uint32_t message) { return (message & 0xF0); }
   bool isRealTime(uint32_t message);

   inline void set_mode(uint32_t m) { if (m > midi_mode) midi_mode = m; }
   inline uint32_t get_mode() { return midi_mode; }

   void add_patch(const char*);
   void set_path();

   uint32_t pull_message();
   bool registered_param(uint32_t, uint32_t, uint32_t);

   inline uint32_t pull_byte() {
      uint32_t r = data.front(); data.pop_front(); return r;
   }

   AeonWave config;

   std::string patch_set = "default";
   std::string patch_version = "1.0.0";

   std::map<uint32_t,Channel*> channels;
   std::map<uint32_t,Channel*> reverb_channels;
   std::map<uint32_t,uint32_t> channel_mask;
   std::map<uint32_t,std::string> frames;

   std::map<uint32_t,std::map<uint32_t,std::pair<std::string,int>>> drums;
   std::map<uint32_t,std::map<uint32_t,std::pair<std::string,int>>> instruments;

   using _patch_t = std::map<uint32_t,std::pair<uint32_t,std::string>>;
   std::map<std::string,_patch_t> patches;

   std::unordered_map<std::string,std::pair<size_t,Buffer*>> buffers;

   std::pair<std::string,int> empty_map = {"", 0};
   std::string instr = "gmmidi.xml";
   std::string drum = "gmdrums.xml";
   std::string path;

   float tuning = 1.0f;

   uint32_t bank_no = 0;
   uint32_t program_no = 0;
   uint32_t midi_mode = MODE0;
   unsigned int refresh_rate = 0;
   unsigned int polyphony = UINT_MAX;

   enum aaxCapabilities mode = AAX_RENDER_NORMAL;
   bool polyphony_enabled = true;
   bool omni_enabled = true;

   uint32_t reverb_type = 4;
   Param reverb_decay_depth = 0.15f;
   Param reverb_cutoff_frequency = 790.0f;
   Status reverb_state = AAX_FALSE;
   aax::Mixer reverb;

   std::string gmmidi;
   std::string gmdrums;
   std::deque<uint32_t> data;

   bool registered = false;
   uint32_t msb_type = 0;
   uint32_t lsb_type = 0;
   struct param_t param[MAX_REGISTERED_PARAM+1] = {
      { 2, 0 }, { 0x40, 0 }, { 0x20, 0 }, { 0, 0 }, { 0, 0 }, { 1, 0 }
   };

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

private:
   Channel(const Channel&) = delete;
   Channel& operator=(const Channel&) = delete;

   MIDI &midi;

   std::pair<uint32_t,std::string> get_patch(std::string& name, uint32_t& key);
   std::map<uint32_t,Buffer&> name_map;

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
