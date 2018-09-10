/*
 * Copyright (C) 2018 by Erik Hofman.
 * Copyright (C) 2018 by Adalin B.V.
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

#include <aax/aeonwave.hpp> 

namespace aax
{

class Note : public Emitter
{
private:
    Note(const Note&) = delete;

    Note& operator=(const Note&) = delete;

public:
    Note(float p) : Emitter(AAX_RELATIVE) {
        Emitter::matrix(mtx);
        pitch_param = pitch = p;
        tie(pitch_param, AAX_PITCH_EFFECT, AAX_PITCH);
        tie(gain_param, AAX_VOLUME_FILTER, AAX_GAIN);
    }

    friend void swap(Note& n1, Note& n2) noexcept {
        std::swap(static_cast<Emitter&>(n1), static_cast<Emitter&>(n2));
        n1.pitch_param = std::move(n2.pitch_param);
        n1.gain_param = std::move(n2.gain_param);
        n1.pitch = std::move(n2.pitch);
        n1.pressure = std::move(n2.pressure);
        n2.playing = std::move(n2.playing);
    }

    Note& operator=(Note&&) = default;

    operator Emitter&() {
        return *this;
    }

    bool play(float g) {
        gain_param = gain = g;
        Emitter::set(AAX_INITIALIZED);
        if (!playing) playing = Emitter::set(AAX_PLAYING);
        return playing;
    }

    bool stop() {
        playing = false;
        return Emitter::set(AAX_STOPPED);
    }

    bool buffer(Buffer& buffer) {
        Emitter::remove_buffer();
        return Emitter::add(buffer);
    }

    inline void set_pitch(float bend) { pitch_param = bend*pitch; }
    inline void set_gain(float expr) { gain_param = expr*gain; }
    inline void set_pressure(float p) { pressure = p; }

private:
    Matrix64 mtx;
    Param pitch_param = 1.0f;
    Param gain_param = 1.0f;
    float pitch = 1.0f;
    float gain = 1.0f;
    float pressure = 0.0f;
    bool playing = false;
};


class Instrument : public Mixer
{
private:
    Instrument(const Instrument& i) = delete;

    Instrument& operator=(const Instrument&) = delete;

public:
    Instrument(AeonWave& ptr, bool drums = false)
        : Mixer(ptr), aax(&ptr), is_drums(drums)
    {
        Mixer::matrix(mtx);
        Mixer::set(AAX_POSITION, AAX_RELATIVE);
        Mixer::set(AAX_PLAYING);
        if (is_drums) {
            Mixer::set(AAX_MONO_EMITTERS, 10);
        }

        dsp mod = get(AAX_RINGMODULATOR_EFFECT);
        mod.set(AAX_LFO_FREQUENCY, 0.05f);
        set(mod);
        tie(modulation_param, AAX_RINGMODULATOR_EFFECT, AAX_LFO_DEPTH);
        tie(modulation_state, AAX_RINGMODULATOR_EFFECT);
    }

    friend void swap(Instrument& i1, Instrument& i2) noexcept {
        i1.key = std::move(i2.key);
        i1.aax = std::move(i2.aax);
        i1.pressure = std::move(i2.pressure);
        i1.playing = std::move(i2.playing);
    }

    Instrument& operator=(Instrument&&) = default;

    operator Mixer&() {
        return *this;
    }

    void play(uint8_t key_no, uint8_t velocity, Buffer& buffer) {
        auto it = key.find(key_no);
        if (it == key.end()) {
            float pitch = 1.0f;
            float frequency = buffer.get(AAX_UPDATE_RATE);
            if (!is_drums) pitch = note2freq(key_no)/(float)frequency;
            auto ret = key.insert({key_no, new Note(pitch)});
            it = ret.first;
            if (!playing && !is_drums) {
                Mixer::add(buffer);
                playing = true;
            }
            it->second->buffer(buffer);
        }
        Mixer::add(*it->second);
        float g = sqrtf((1+velocity)/128.0f);
        it->second->play(gain*g*soft);
    }

    void stop(uint8_t key_no) {
        if (hold == false) {
            auto it = key.find(key_no);
            if (it != key.end()) {
                it->second->stop();
            }
        }
    }

    inline void set_pitch(float pitch) {
        for (auto& it : key) {
            it.second->set_pitch(pitch);
        }
    }

    inline void set_gain(float g) { gain = g; }

    inline void set_expression(float g) {
        for (auto& it : key) {
            it.second->set_gain(g*gain);
        }
    }

    inline void set_pan(float p) {
        Matrix64 m;
        m.rotate(p, 0.0f, 1.0f, 0.0f);
        m.multiply(mtx);
        Mixer::matrix(m);
    }

    inline void set_pressure(float p) { pressure = p; }

    inline void set_soft(bool s) { soft = s ? 0.5f : 1.0f; }

    // notes hold until sustain becomes false, even after a stop message
    void set_hold(bool h) {
        hold = h;
        if (!hold) {
            for (auto& it : key) {
                it.second->stop();
            }
        }
    }

    // only notes started before this command shold hold until stop arrives
    void set_sustain(bool s) {
    }

    void set_modulation(float m) {
        modulation_param = m;
        if ((m && !modulation_state) || (!m && modulation_state)) {
            modulation_state = m ? AAX_SINE_WAVE : AAX_FALSE;
        }
    }

    void set_pressure(uint8_t key_no, float pressure) {
        auto it = key.find(key_no);
        if (it != key.end()) {
            it->second->set_pressure(pressure);
        }
    }

private:
    inline float note2freq(uint8_t d) {
        return 440.0f*powf(2.0f, ((float)d-69.0f)/12.0f);
    }

    std::map<uint8_t,Note*> key;
    AeonWave* aax;

    Vector dir = Vector(0.0f, 0.0f, 1.0f);
    Vector64 pos = Vector64(0.0, 2.0, -3.0);
    Matrix64 mtx = Matrix64(pos, dir);

    Param modulation_param = 0.0f;
    Status modulation_state = AAX_FALSE;

    bool is_drums;
    float soft = 1.0f;
    float gain = 1.0f;
    float pressure = 0.0f;
    bool playing = false;
    bool hold = false;
};

} // namespace aax

#endif

