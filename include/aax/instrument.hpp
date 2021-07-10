/*
 * Copyright (C) 2018-2021 by Erik Hofman.
 * Copyright (C) 2018-2021 by Adalin B.V.
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
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef AEONWAVE_INSTRUMENT_HPP
#define AEONWAVE_INSTRUMENT_HPP 1

#include <map>
#include <memory>
#include <utility>
#include <algorithm>

#include <aax/aeonwave.hpp>

namespace aax
{

#define GAIN_FACTOR			0.5f
#define INSTRUMENT_DISTANCE		1.0f
#define PAN_LEVELS			128.0f
#define LEVEL_60DB			0.001f

inline float lin2log(float v) { return log10f(v); }
inline float log2lin(float v) { return powf(10.0f,v); }

class Panning
{
public:
    Panning() = default;

    virtual ~Panning() = default;

    void set(float p, bool init=false) {
        pan = p;
        panned = !init;

        if (!init && abs(wide) > 0) {
            p = floorf(pan*wide)/abs(wide);
        }
        if (p != 0.0f) {
            p *= spread;
            int pos = floorf(p*PAN_LEVELS);
            auto it = matrices.find(pos);
            if (it != matrices.end()) {
                mtx = it->second;
            } else {
                Matrix64 m;
                m.rotate(-1.57*p, 0.0, 1.0, 0.0);
                m.multiply(mtx_init);
                matrices[pos] = m;
                mtx = m;
            }
        } else {
            mtx = mtx_init;
        }
    }

public:
    std::map<int,Matrix64> matrices;
    Vector at = Vector(0.0f, 0.0f, -1.0f);
    Vector up = Vector(0.0f, 1.0f, 0.0f);
    Vector64 pos = Vector64(0.0, 1.0, -INSTRUMENT_DISTANCE);
    Matrix64 mtx_init = Matrix64(pos, at, up);
    Matrix64 mtx = mtx_init;
    float spread = 1.0f;
    float pan = 0.0f;
    int wide = 0;
    bool panned = false;
};

class Note : public Emitter
{
private:
    Note(const Note&) = delete;

    Note& operator=(const Note&) = delete;

public:
    Note(float f, float p, Panning& pan)
        : Emitter(pan.wide ? AAX_ABSOLUTE : AAX_RELATIVE),
          frequency(f), pitch(p)
    {
        pitch = p; set_pitch();
        tie(pitch_param, AAX_PITCH_EFFECT, AAX_PITCH);

        tie(volume_param, AAX_VOLUME_FILTER, AAX_GAIN);
        if (pan.wide) {
            // pitch*frequency ranges from: 8 - 12544 Hz,
            // log(20) = 1.3, log(12544) = 4.1
            float p = (lin2log(pitch*frequency) - 1.3f)/2.8f; // 0.0f .. 1.0f
            p = floorf(-2.0f*(p-0.5f)*PAN_LEVELS)/PAN_LEVELS;
            if (p != pan_prev) {
                pan.set(p, true);
                Emitter::matrix(pan.mtx);
                pan_prev = p;
            }
        } else {
            Emitter::matrix(mtx);
        }

        aax::dsp dsp = Emitter::get(AAX_VOLUME_FILTER);
        dsp.set(AAX_MAX_GAIN, 2.56f);
        Emitter::set(dsp);
    }

    virtual ~Note() = default;

    friend void swap(Note& n1, Note& n2) noexcept {
        std::swap(static_cast<Emitter&>(n1), static_cast<Emitter&>(n2));

        n1.mtx = std::move(n2.mtx);
        n1.pan_prev = n2.pan_prev;

        n1.playing = n2.playing;
        n1.hold = n2.hold;

        n1.volume_param = std::move(n2.volume_param);

        n1.pitch_param = std::move(n2.pitch_param);
        n1.pitch_bend = n2.pitch_bend;
        n1.frequency = n2.frequency;
        n1.pitch = n2.pitch;
    }

    Note& operator=(Note&&) = default;

    void matrix(Matrix64& m) {
        Emitter::set(AAX_POSITION, AAX_ABSOLUTE);
        Emitter::matrix(m);
    }

    bool play(float velocity, float start_pitch = 1.0f, float time = 0.0f) {
        hold = false;
        if (time > 0.0f && start_pitch != pitch) {
           aax::dsp dsp = Emitter::get(AAX_PITCH_EFFECT);
           dsp.set(AAX_PITCH_START, start_pitch);
           dsp.set(AAX_TRANSITION_TIME, time);
           dsp.set(AAX_TRUE|AAX_ENVELOPE_FOLLOW);
           Emitter::set(dsp);
        }
        Emitter::set(AAX_INITIALIZED);
        Emitter::set(AAX_MIDI_ATTACK_VELOCITY_FACTOR, 127.0f*velocity);
        if (!playing) playing = Emitter::set(AAX_PLAYING);
        return playing;
    }

    bool stop(float velocity = 1.0f) {
        playing = false;
        Emitter::set(AAX_MIDI_RELEASE_VELOCITY_FACTOR, 127.0f*velocity);
        return hold ? true : Emitter::set(AAX_STOPPED);
    }

    bool finish(void) {
        hold = false; playing = false;
        return Emitter::set(AAX_STOPPED);
    }

    bool finished(void) {
        aaxState s = Emitter::state();
        return (s == AAX_PROCESSED || s == AAX_INITIALIZED);
    }

    bool stopped(void) {
        aaxState s = Emitter::state();
        return (s != AAX_PLAYING);
    }

    // notes hold until hold becomes false, even after a stop message.
    // already stopped notes can be caught by hold again.
    void set_hold(bool h) {
        if (!h && hold) Emitter::set(AAX_STOPPED);
        else if (h && Emitter::state() == AAX_STOPPED) {
            Emitter::set(AAX_PLAYING);
        }
        hold = h;
    }

    // only notes started before this command should hold until stop arrives
    inline void set_sustain(bool s) { hold = s; }

    inline void set_soft(float soft) {
        Emitter::set(AAX_MIDI_SOFT_FACTOR, 127.0f*soft);
    }
    inline void set_pressure(float pressure) {
        Emitter::set(AAX_MIDI_PRESSURE_FACTOR, 127.0f*pressure);
    }
    inline void set_pitch(float b) {
        pitch_bend = b; set_pitch();
    }

    // envelope control
    inline void set_attack_time(unsigned t) { set(AAX_MIDI_ATTACK_FACTOR, t); }
    inline void set_release_time(unsigned t) { set(AAX_MIDI_RELEASE_FACTOR,t); }
    inline void set_decay_time(unsigned t) { set(AAX_MIDI_DECAY_FACTOR, t); }
    inline void set_legato(bool l) { set(AAX_MIDI_LEGATO_MODE, l); };

    bool buffer(Buffer& buffer) {
        Emitter::remove_buffer();
        return Emitter::add(buffer);
    }

private:
    inline void set_pitch() {
        pitch_param = pitch*pitch_bend;
    }

    Matrix64 mtx;
    float pan_prev = -1000.0f;

    bool playing = false;
    bool hold = true;

    Param volume_param = GAIN_FACTOR;

    Param pitch_param = 1.0f;
    float pitch_bend = 1.0f;
    float frequency;
    float pitch;
};


class Instrument : public Mixer
{
private:
    Instrument(const Instrument& i) = delete;

    Instrument& operator=(const Instrument&) = delete;

public:
    Instrument(AeonWave& ptr, bool drums = false, int wide = 0)
        : Mixer(ptr), aax(&ptr), is_drums(drums)
    {
        pan.wide = wide;

        tie(volume, AAX_VOLUME_FILTER, AAX_GAIN);

        tie(vibrato_freq, AAX_DYNAMIC_PITCH_EFFECT, AAX_LFO_FREQUENCY);
        tie(vibrato_depth, AAX_DYNAMIC_PITCH_EFFECT, AAX_LFO_DEPTH);
        tie(vibrato_state, AAX_DYNAMIC_PITCH_EFFECT);

        tie(tremolo_freq, AAX_DYNAMIC_GAIN_FILTER, AAX_LFO_FREQUENCY);
        tie(tremolo_depth, AAX_DYNAMIC_GAIN_FILTER, AAX_LFO_DEPTH);
        tie(tremolo_state, AAX_DYNAMIC_GAIN_FILTER);

        tie(chorus_level, AAX_CHORUS_EFFECT, AAX_DELAY_GAIN);
        tie(chorus_depth, AAX_CHORUS_EFFECT, AAX_LFO_OFFSET);
        tie(chorus_rate, AAX_CHORUS_EFFECT, AAX_LFO_FREQUENCY);
        tie(chorus_state, AAX_CHORUS_EFFECT);

        tie(filter_cutoff, AAX_FREQUENCY_FILTER, AAX_CUTOFF_FREQUENCY);
        tie(filter_resonance, AAX_FREQUENCY_FILTER, AAX_RESONANCE);
        tie(filter_state, AAX_FREQUENCY_FILTER);

        tie(reverb_level, AAX_REVERB_EFFECT, AAX_DECAY_LEVEL);
        tie(reverb_delay_depth, AAX_REVERB_EFFECT, AAX_DELAY_DEPTH);
        tie(reverb_decay_level, AAX_REVERB_EFFECT, AAX_DECAY_LEVEL);
        tie(reverb_decay_depth, AAX_REVERB_EFFECT, AAX_DECAY_DEPTH);
        tie(reverb_cutoff, AAX_REVERB_EFFECT, AAX_CUTOFF_FREQUENCY);
        tie(reverb_state, AAX_REVERB_EFFECT);

        Mixer::matrix(pan.mtx_init);
        Mixer::set(AAX_POSITION, AAX_RELATIVE);
        Mixer::set(AAX_PLAYING);
        if (is_drums) {
            Mixer::set(AAX_MONO_EMITTERS, 10);
        }
    }

    virtual ~Instrument() {
        for (auto it : key) {
            Mixer::remove(*it.second);
        }
    }

    friend void swap(Instrument& i1, Instrument& i2) noexcept {
        std::swap(static_cast<Mixer&>(i1), static_cast<Mixer&>(i2));

        i1.aax = std::move(i2.aax);

        i1.pan = std::move(i2.pan);

        i1.volume = std::move(i2.volume);

        i1.vibrato_freq = std::move(i2.vibrato_freq);
        i1.vibrato_depth = std::move(i2.vibrato_depth);
        i1.vibrato_state = std::move(i2.vibrato_state);

        i1.tremolo_freq = std::move(i2.tremolo_freq);
        i1.tremolo_depth = std::move(i2.tremolo_depth);
        i1.tremolo_state = std::move(i2.tremolo_state);

        i1.chorus_rate = std::move(i2.chorus_rate);
        i1.chorus_level = std::move(i2.chorus_level);
        i1.chorus_depth = std::move(i2.chorus_depth);
        i1.chorus_state = std::move(i2.chorus_state);

        i1.filter_cutoff = std::move(i2.filter_cutoff);
        i1.filter_resonance = std::move(i2.filter_resonance);
        i1.filter_state = std::move(i2.filter_state);

        i1.reverb_level = std::move(i2.reverb_level);
        i1.reverb_delay_depth = std::move(i2.reverb_delay_depth);
        i1.reverb_decay_level = std::move(i2.reverb_decay_level);
        i1.reverb_decay_depth = std::move(i2.reverb_decay_depth);
        i1.reverb_cutoff = std::move(i2.reverb_cutoff);
        i1.reverb_state = std::move(i2.reverb_state);

        i1.attack_time = i2.attack_time;
        i1.release_time = i2.release_time;
        i1.decay_time = i2.decay_time;

        i1.mfreq = i2.mfreq;
        i1.mrange = i2.mrange;

        i1.cutoff = i2.cutoff;
        i1.fc = i2.fc;
        i1.Q = i2.Q;

        i1.soft = i2.soft;
        i1.gain = i2.gain;
        i1.expression = i2.expression;

        i1.pan_prev = i2.pan_prev;

        i1.transition_time = i2.transition_time;
        i1.pitch_start = i2.pitch_start;
        i1.key_prev = i2.key_prev;

        i1.is_drums = i2.is_drums;
        i1.monophonic = i2.monophonic;
        i1.playing = i2.playing;
        i1.slide_state = i2.slide_state;
        i1.legato = i2.legato;

        i1.key_finish = i2.key_finish;
        i1.key_stopped = std::move(i2.key_stopped);
        i1.key = std::move(i2.key);
    }

    Instrument& operator=(Instrument&&) = default;

    void finish(void) {
        for (auto& it : key) it.second->stop();
    }

    bool finished(void) {
        for (auto& it : key) {
            if (!it.second->finished()) return false;
        }
        return true;
    }

    void play(uint32_t key_no, float velocity, Buffer& buffer, float pitch=1.0f)
    {
        float frequency = buffer.get(AAX_UPDATE_RATE);
        if (!is_drums) pitch *= note2freq(key_no)/frequency;
        if (monophonic) {
            auto it = key.find(key_prev);
            if (it != key.end()) it->second->stop();
            key_prev = key_no;
        }
        std::shared_ptr<Note> note;
        auto it = key.find(key_no);
        if (it != key.end()) {
            note = it->second;
            if (key_finish && !note->finished()) {
                note->finish();
                key_stopped[key_no] = std::move(key.at(key_no));
                key.erase(key_no);
                it = key.end();
            }
        }
        if (it == key.end()) {
            auto ret = key.insert({key_no, std::shared_ptr<Note>{new Note(frequency,pitch,pan)}});
            note = ret.first->second;
            if (!playing && !is_drums) {
                Mixer::add(buffer);
                playing = true;
            }
            if (is_drums && !pan.panned) note->matrix(pan.mtx_init);
            else if (pan.panned && abs(pan.wide) > 1) note->matrix(pan.mtx);
            note->buffer(buffer);
        }
        Mixer::add(*note);
        note->set_attack_time(attack_time);
        note->set_release_time(release_time);
        note->set_legato(legato);
        note->set_soft(soft);
        note->play(velocity, pitch_start, slide_state ? transition_time : 0.0f);
        pitch_start = pitch;
        for (auto it = key_stopped.begin(), next = it; it != key_stopped.end();
             it = next)
        {
            ++next;
            if (it->second->finished()) {
                key_stopped.erase(it);
            }
        }
    }

    void stop(uint32_t key_no, float velocity = 0) {
        auto it = key.find(key_no);
        if (it != key.end()) {
            it->second->stop(velocity);
        }
    }

    inline void set_key_finish(bool finish) { key_finish = finish; }

    inline void set_monophonic(bool m) { if (!is_drums) monophonic = m; }

    inline void set_detune(float level) {
    }

    inline void set_pitch(float pitch) {
        for (auto& it : key) it.second->set_pitch(pitch);
    }

    inline void set_pitch(uint32_t key_no, float pitch) {
        auto it = key.find(key_no);
        if (it != key.end()) {
            it->second->set_pitch(pitch);
        }
    }

    inline void set_gain(float v) {
        gain = v; set_volume();
    }

    inline void set_expression(float e) {
        expression = e; set_volume();
    }

    inline void set_soft(float s) {
        soft = (!is_drums) ? 1.0f - 0.5f*s : 1.0f; set_filter_cutoff();
        for (auto& it : key) it.second->set_soft(soft);
    }

    inline void set_pressure(float p) {
        for (auto& it : key) it.second->set_pressure(p);
    }

    inline void set_pressure(uint32_t key_no, float p) {
        auto it = key.find(key_no);
        if (it != key.end()) {
            it->second->set_pressure(p);
        }
    }

    void set_pan(float p) {
        p = floorf(p*PAN_LEVELS)/PAN_LEVELS;
        if (p != pan_prev) {
            pan.set(p);
            if (!is_drums && !pan.wide) {
                Mixer::matrix(pan.mtx);
            } else {
                for (auto& it : key) it.second->matrix(pan.mtx);
            }
            pan_prev = p;
        }
    }

    inline void set_hold(uint32_t key_no, bool h) {
       auto it = key.find(key_no);
       if (it != key.end()) {
            it->second->set_hold(h);
        }
    }

    inline void set_hold(bool h) {
        for (auto& it : key) it.second->set_hold(h);
    }

    inline void set_sustain(bool s) {
        if (!is_drums) {
            for (auto& it : key) it.second->set_sustain(s);
        }
    }

    void set_modulation(float m) {
        if (!is_drums) {
            bool enabled = (m != 0.0f);
            vibrato_depth = m; tremolo_depth = m;
            if (enabled)
            {
                if (!vibrato_state) {
                    vibrato_state = tremolo_state = AAX_SINE_WAVE;
                }
            } else if (vibrato_state) {
                vibrato_state = tremolo_state = AAX_FALSE;
            }
        }
    }

    // set_pitch_rate(bool) is deprecated in favor of set_pitch_slide_state
    // set_pitch_rate(float) is deprecated in favor of set_pitch_transition_time
    inline void set_pitch_start(float p) {
        if (!is_drums) { pitch_start = p; }
    }
    inline void set_pitch_slide_state(bool s) {
        if (!is_drums) { slide_state = s; }
    }
    inline void set_pitch_transition_time(float t) {
        if (!is_drums) { transition_time = t; }
    }

    void set_vibrato_rate(float r) {}
    void set_vibrato_depth(float d) {}
    void set_vibrato_delay(float d) {}

    void set_tremolo_depth(float d) {}
    void set_phaser_depth(float d) {}

    void set_attack_time(unsigned t) {
        if (!is_drums) { attack_time = t;
            for (auto& it : key) it.second->set_attack_time(t);
        }
    }
    void set_release_time(unsigned t) {
        if (!is_drums) { release_time = t;
            for (auto& it : key) it.second->set_release_time(t);
        }
    }
    void set_decay_time(unsigned t) {
        if (!is_drums) { decay_time = t;
            for (auto& it : key) it.second->set_decay_time(t);
        }
    }

    inline void set_legato(bool l) {
        if (!is_drums) { legato = l;
            for (auto& it : key) it.second->set_legato(l);
        }
    }

    // The whole device must have one chorus effect and one reverb effect.
    // Each Channel must have its own adjustable send levels to the chorus
    // and the reverb. A connection from chorus to reverb must be provided.
    void set_reverb(Buffer& buf) {
        Mixer::add(buf);
        aax::dsp dsp = Mixer::get(AAX_REVERB_EFFECT);
        reverb_decay_level = dsp.get(AAX_DECAY_LEVEL);
    }
    void set_reverb_level(float lvl) {
        if (lvl > 1e-5f) {
            reverb_level = lvl*reverb_decay_level;
            if (!reverb_state) reverb_state = AAX_REVERB_1ST_ORDER;
        } else if (reverb_state) reverb_state = AAX_FALSE;
    }
    inline void set_reverb_cutoff(float fc) { reverb_cutoff = fc; }
    inline void set_reverb_delay_depth(float v) { reverb_delay_depth = v; }
    inline void set_reverb_decay_level(float v) { reverb_decay_level = v; }
    inline void set_reverb_decay_depth(float v) { reverb_decay_depth = v; }
    inline void set_reverb_time_rt60(float v) {
        reverb_decay_level = powf(LEVEL_60DB, 0.5f*reverb_decay_depth/v);
    }

    void set_chorus_level(float lvl) {
        if (lvl > 0) {
            chorus_level = lvl;
            if (!chorus_state) chorus_state = AAX_TRUE;
        } else if (chorus_state) chorus_state = AAX_FALSE;
    }

    inline void set_chorus_depth(float depth) { chorus_depth = depth; }
    inline void set_chorus_rate(float rate) { chorus_rate = rate; }

    void set_filter_cutoff(float dfc) {
        cutoff = dfc; set_filter_cutoff();
        if (!filter_state) filter_state = AAX_TRUE;
    }

    inline void set_filter_resonance(float dQ) {
        filter_resonance = Q + dQ;
        if (!filter_state) filter_state = AAX_TRUE;
    }

    inline void set_spread(float s = 1.0f) { pan.spread = s; }
    inline float get_spread(void) { return pan.spread; }

    inline void set_wide(int s = 1) { pan.wide = s; }
    inline int get_wide(void) { return pan.wide; }

private:
    inline float note2freq(uint32_t d) {
        return 440.0f*powf(2.0f, (float(d)-69.0f)/12.0f);
    }
    inline void set_filter_cutoff() {
        filter_cutoff = soft*log2lin(cutoff*fc);
    }
    inline void set_volume() {
        volume = gain*expression;
    }

    AeonWave* aax;

    Panning pan;

    Param volume = 1.0f;

    Param vibrato_freq = 5.0f;
    Param vibrato_depth = 0.0f;
    Status vibrato_state = AAX_FALSE;

    Param tremolo_freq = 5.0f;
    Param tremolo_depth = 0.0f;
    Status tremolo_state = AAX_FALSE;

    Param chorus_rate = 0.0f;
    Param chorus_level = 0.0f;
    Param chorus_depth = Param(1900.0f, AAX_MICROSECONDS);
    Status chorus_state = AAX_FALSE;

    Param filter_cutoff = 2048.0f;
    Param filter_resonance = 1.0f;
    Param filter_state = AAX_FALSE;

    Param reverb_level = 40.0f/127.0f;
    Param reverb_delay_depth = 0.035f;
    Param reverb_decay_level = 0.0f;
    Param reverb_decay_depth = 0.0f;
    Param reverb_cutoff = 22000.0f;
    Status reverb_state = AAX_FALSE;

    unsigned attack_time = 64;
    unsigned release_time = 64;
    unsigned decay_time = 64;

    float mfreq = 1.5f;
    float mrange = 1.0f;

    float cutoff = 1.0f;
    float fc = lin2log(float(filter_cutoff));
    float Q = float(filter_resonance);

    float soft = 1.0f;
    float gain = 1.0f;
    float expression = 1.0f;

    float pan_prev = 0.0f;

    float transition_time = 0.0f;
    float pitch_start = 1.0f;
    uint32_t key_prev = 0;

    bool is_drums;
    bool monophonic = false;
    bool playing = false;
    bool slide_state = false;
    bool legato = false;

    bool key_finish = false;
    std::map<uint32_t,std::shared_ptr<Note>> key_stopped;
    std::map<uint32_t,std::shared_ptr<Note>> key;
};

} // namespace aax

#endif

