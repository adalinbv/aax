/*
 * Copyright (C) 2018-2019 by Erik Hofman.
 * Copyright (C) 2018-2019 by Adalin B.V.
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
#include <algorithm>

#include <aax/aeonwave.hpp>

namespace aax
{

class Note : public Emitter
{
private:
    Note(const Note&) = delete;

    Note& operator=(const Note&) = delete;

public:
    Note(float f, float p, bool is_wide = false)
        : Emitter(is_wide ? AAX_ABSOLUTE : AAX_RELATIVE), frequency(f), pitch(p)
    {
        pitch_param = p;
        tie(pitch_param, AAX_PITCH_EFFECT, AAX_PITCH);

        tie(gain_param, AAX_VOLUME_FILTER, AAX_GAIN);

        tie(filter_cutoff, AAX_FREQUENCY_FILTER, AAX_CUTOFF_FREQUENCY);
        tie(filter_resonance, AAX_FREQUENCY_FILTER, AAX_RESONANCE);
        tie(filter_state, AAX_FREQUENCY_FILTER);

        if (is_wide) {
            Vector dir = Vector(0.0f, 0.0f, -1.0f);
            Vector64 pos = Vector64(0.0, 1.0, -2.75);
            Matrix64 mtx = Matrix64(pos, dir);
            Matrix64 m;
            p = 2.17f - _lin2log(f*p);
            m.rotate(p, 0.0, 1.0, 0.0);
            m.multiply(mtx);
            Emitter::matrix(m);
        } else {
            Emitter::matrix(mtx);
        }
    }

    friend void swap(Note& n1, Note& n2) noexcept {
        std::swap(static_cast<Emitter&>(n1), static_cast<Emitter&>(n2));
        n1.pitch_param = std::move(n2.pitch_param);
        n1.gain_param = std::move(n2.gain_param);
        n1.pitch = std::move(n2.pitch);
        n1.gain = std::move(n2.gain);
        n1.playing = std::move(n2.playing);
    }

    Note& operator=(Note&&) = default;

    operator Emitter&() {
        return *this;
    }

    void matrix(Matrix64& m) {
        Emitter::set(AAX_POSITION, AAX_ABSOLUTE);
        Emitter::matrix(m);
    }

    bool play(float g, float start_pitch = 1.0f, float slide = 0.0f) {
        hold = false;
        gain_param = gain = g;
        if (slide > 0.0f && start_pitch != 1.0f) {
           aax::dsp dsp = Emitter::get(AAX_PITCH_EFFECT);
           dsp.set(AAX_PITCH_START, start_pitch);
           dsp.set(AAX_PITCH_SLIDE, slide);
           dsp.set(AAX_TRUE|AAX_ENVELOPE_FOLLOW);
           Emitter::set(dsp);
        }
        Emitter::set(AAX_INITIALIZED);
        if (!playing) playing = Emitter::set(AAX_PLAYING);
        return playing;
    }

    bool finished() {
        aaxState s = Emitter::state();
        return (s == AAX_PROCESSED || s == AAX_INITIALIZED);
    }

    bool stop(float g = 1.0f) {
        playing = false;
        if (fabsf(g - 1.0f) > 0.1f) gain_param = (gain *= g);
        return hold ? true : Emitter::set(AAX_STOPPED);
    }

    // notes hold until hold becomes false, even after a stop message
    void set_hold(bool h) {
        if (hold && !h) Emitter::set(AAX_STOPPED);
        hold = h;
    }

    // only notes started before this command should hold until stop arrives
    inline void set_sustain(bool s) { hold = s; }

    // envelope control
    inline void set_attack_time(unsigned t) { set(AAX_ATTACK_FACTOR, t); }
    inline void set_release_time(unsigned t) { set(AAX_RELEASE_FACTOR, t); }
    inline void set_decay_time(unsigned t) { set(AAX_DECAY_FACTOR, t); }


    inline void set_filter_state() {
        if (filter_cutoff > 32.f && filter_cutoff <= 10000.f) {
            if (!filter_state) filter_state = AAX_TRUE;
        }
        else if (filter_state) filter_state = AAX_FALSE;
    }

    void set_filter_cutoff(float dfc) {
            if (!fc) {
                if (filter_cutoff == 22050.0f) {
                    fc = _lin2log(2.0f*frequency*pitch);
                    aax::dsp dsp = Emitter::get(AAX_FREQUENCY_FILTER);
                    dsp.set(AAX_HF_GAIN, 0.0f);
                    Emitter::set(dsp);
                } else {
                    fc = _lin2log(2.0f*filter_cutoff);
                }
            }
            filter_cutoff = _log2lin(fc + _lin2log(0.01f+0.99f*dfc));
            set_filter_state();
    }

    void set_filter_resonance(float dQ) {
            if (!Q) Q = filter_resonance;
            filter_resonance = 0.25f*Q+Q*2.0f*dQ;
            set_filter_state();
    }

    bool buffer(Buffer& buffer) {
        Emitter::remove_buffer();
        return Emitter::add(buffer);
    }

    inline void set_gain(float expr) { gain_param = expr*gain; }
    inline void set_pitch(float bend) { pitch_param = bend*pitch; }

private:
    inline float _lin2log(float v) { return log10f(v); }
    inline float _log2lin(float v) { return powf(10.0f,v); }

    Matrix64 mtx;

    Param gain_param = 1.0f;
    Param pitch_param = 1.0f;

    Param filter_cutoff = 22050.0f;
    Param filter_resonance = 1.0f;
    Param filter_state = AAX_FALSE;

    float fc = 0.0f;
    float Q = 0.0f;

    float frequency;
    float pitch;
    float gain = 1.0f;
    bool playing = false;
    bool hold = true;
};


class Instrument : public Mixer
{
private:
    Instrument(const Instrument& i) = delete;

    Instrument& operator=(const Instrument&) = delete;

public:
    Instrument(AeonWave& ptr, bool drums = false, bool wide = false)
        : Mixer(ptr), aax(&ptr), is_drums(drums), is_wide(wide)
    {
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

        tie(reverb_level, AAX_REVERB_EFFECT, AAX_DECAY_LEVEL);
        tie(reverb_delay_depth, AAX_REVERB_EFFECT, AAX_DELAY_DEPTH);
        tie(reverb_decay_depth, AAX_REVERB_EFFECT, AAX_DECAY_DEPTH);
        tie(reverb_cutoff_frequency, AAX_REVERB_EFFECT, AAX_CUTOFF_FREQUENCY);
//      tie(reverb_state, AAX_REVERB_EFFECT);

        Mixer::matrix(mtx);
        Mixer::set(AAX_POSITION, AAX_RELATIVE);
        Mixer::set(AAX_PLAYING);
        if (is_drums) {
            Mixer::set(AAX_MONO_EMITTERS, 10);
        }
    }

    friend void swap(Instrument& i1, Instrument& i2) noexcept {
        i1.key = std::move(i2.key);
        i1.aax = std::move(i2.aax);
        i1.playing = std::move(i2.playing);
    }

    Instrument& operator=(Instrument&&) = default;

    operator Mixer&() {
        return *this;
    }

    void finish() {
        for (auto& it : key) {
            it.second->stop();
        }
    }

    bool finished() {
        for (auto& it : key) {
            if (!it.second->finished()) return false;
        }
        return true;
    }

    void play(uint8_t key_no, uint8_t velocity, Buffer& buffer, float pitch = 1.0f)
    {
        float frequency = buffer.get(AAX_UPDATE_RATE);
        if (!is_drums) pitch *= note2freq(key_no)/float(frequency);
        if (monophonic) {
            auto it = key.find(key_prev);
            if (it != key.end()) it->second->stop();
            key_prev = key_no;
        }
        auto it = key.find(key_no);
        if (it == key.end()) {
            auto ret = key.insert({key_no, new Note(frequency,pitch,is_wide)});
            it = ret.first;
            if (!playing && !is_drums) {
                Mixer::add(buffer);
                playing = true;
            }
            if (is_drums && !panned) it->second->matrix(mtx);
            if (!is_drums && fc) {
                it->second->set_filter_cutoff(fc);
                it->second->set_filter_resonance(Q);
            }
            it->second->buffer(buffer);
        }
        Mixer::add(*it->second);
        it->second->set_attack_time(attack_time);
        it->second->set_release_time(release_time);
        float g = 3.321928f*log10f(1.0f+(1+velocity)/128.0f);
        it->second->play(volume*g*soft, pitch_last, pitch_slide_state ? pitch_slide : 0.0f);
        pitch_last = pitch;
    }

    void stop(uint8_t key_no, uint8_t velocity) {
        auto it = key.find(key_no);
        if (it != key.end()) {
            float g = std::min(0.333f + 0.667f*2.0f*velocity/128.0f, 1.0f);
            it->second->stop(volume*g*soft);
        }
    }

    inline void set_monophonic(bool m) { if (!is_drums) monophonic = m; }

    inline void set_detune(float level) {
        delay_level = level;
    }

    inline void set_pitch(float pitch) {
        for (auto& it : key) {
            it.second->set_pitch(pitch);
        }
    }

    inline void set_pitch(uint8_t key_no, float pitch) {
        auto it = key.find(key_no);
        if (it != key.end()) {
            it->second->set_pitch(pitch);
        }
    }

    inline void set_gain(float v) { volume = v; }

    inline void set_pressure(float p) { pressure = p; }
    inline void set_pressure(uint8_t key_no, float p) {
        auto it = key.find(key_no);
        if (it != key.end()) {
            it->second->set_gain(p*pressure*soft*volume);
        }
    }

    inline void set_expression(float e) {
        for (auto& it : key) {
            it.second->set_gain(e*pressure*soft*volume);
        }
    }

    void set_pan(float p) {
        if (is_wide) return;
        Matrix64 m; panned = true;
        m.rotate(1.57*p, 0.0, 1.0, 0.0);
        m.multiply(mtx);
        if (!is_drums) {
            Mixer::matrix(m);
        } else {
             for (auto& it : key) {
                 it.second->matrix(m);
             }
        }
    }

    inline void set_soft(bool s) { soft = (s && !is_drums) ? 0.5f : 1.0f; }

    inline void set_hold(bool h) {
        for (auto& it : key) {
            it.second->set_hold(h);
        }
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

    inline void set_pitch_slide(bool s) {
        if (!is_drums) { pitch_slide_state = s; }
    }
    inline void set_pitch_slide(float t) {
        if (!is_drums) { pitch_slide = t; }
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

    // The whole device must have one chorus effect and one reverb effect.
    // Each Channel must have its own adjustable send levels to the chorus
    // and the reverb. A connection from chorus to reverb must be provided.
    void set_reverb_level(float lvl) {
        if (lvl > 0) {
            reverb_level = lvl;
            if (!reverb_state) reverb_state = AAX_TRUE;
        } else if (reverb_state) reverb_state = AAX_FALSE;
    }

    void set_chorus_level(float lvl) {
        if (lvl > 0) {
            chorus_level = lvl;
            if (!chorus_state) chorus_state = AAX_TRUE;
        } else if (chorus_state) chorus_state = AAX_FALSE;
    }

    inline void set_chorus_depth(float depth) { chorus_depth = depth; }
    inline void set_chorus_rate(float rate) { chorus_rate = rate; }

    inline void set_filter_cutoff(float dfc) {
        if (!is_drums) { fc = dfc;
            for (auto& it : key) it.second->set_filter_cutoff(dfc);
        }
    }

    inline void set_filter_resonance(float dQ) {
        if (!is_drums) { Q = dQ;
            for (auto& it : key) it.second->set_filter_resonance(dQ);
        }
    }

    inline void set_wide(bool w = true) { is_wide = w; }
    inline bool get_wide() { return is_wide; }

private:
    inline float note2freq(uint8_t d) {
        return 440.0f*powf(2.0f, (float(d)-69.0f)/12.0f);
    }

    std::map<uint8_t,Note*> key;
    AeonWave* aax;

    Vector dir = Vector(0.0f, 0.0f, -1.0f);
    Vector64 pos = Vector64(0.0, 1.0, -2.75);
    Matrix64 mtx = Matrix64(pos, dir);

    Param vibrato_freq = 5.0f;
    Param vibrato_depth = 0.0f;
    Status vibrato_state = AAX_FALSE;

    Param tremolo_freq = 5.0f;
    Param tremolo_depth = 0.0f;
    Status tremolo_state = AAX_FALSE;

    Param chorus_rate = 0.0f;
    Param chorus_level = 0.0f;
    Param chorus_depth = 0.4f;
    Status chorus_state = AAX_FALSE;

    Param reverb_level = 40.0f/127.0f;
    Param reverb_delay_depth = 0.035f;
    Param reverb_decay_depth = 0.15f;
    Param reverb_cutoff_frequency = 790.0f;
    Status reverb_state = AAX_FALSE;

    unsigned attack_time = 64;
    unsigned release_time = 64;
    unsigned decay_time = 64;

    float delay_level = 0.0f;

    float mfreq = 1.5f;
    float mrange = 1.0f;

    float fc = 0.0f;
    float Q = 0.0f;

    float soft = 1.0f;
    float volume = 1.0f;
    float pressure = 1.0f;

    float pitch_slide = 0.0f;
    float pitch_last = 1.0f;
    uint8_t key_prev = 0;

    bool is_wide;
    bool is_drums;
    bool panned = false;
    bool monophonic = false;
    bool playing = false;
    bool pitch_slide_state = false;
};

} // namespace aax

#endif

