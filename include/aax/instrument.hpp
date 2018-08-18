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

class Key : public Emitter
{
private:
    Key(const Key&) = delete;

    Key& operator=(const Key&) = delete;

public:
    Key(uint8_t key) : Emitter(AAX_STEREO), key_no(key)
    {
        tie(pitch_param, AAX_PITCH_EFFECT, AAX_PITCH);
        tie(gain_param, AAX_VOLUME_FILTER, AAX_GAIN);
    }

    friend void swap(Key& n1, Key& n2) noexcept {
        std::swap(static_cast<Emitter&>(n1), static_cast<Emitter&>(n2));
        n1.pitch_param = std::move(n2.pitch_param);
        n1.gain_param = std::move(n2.gain_param);
        n1.key_no = std::move(n2.key_no);
    }

    Key& operator=(Key&&) = default;

    operator Emitter&() {
        return *this;
    }

    bool play(uint8_t velocity, bool is_drums = false)
    {
        Emitter::set(AAX_INITIALIZED);
        gain_param = 2.0f*velocity/255.0f;
        if (!is_drums) pitch_param = pitch = note2freq(key_no)/(float)frequency;
        if (!playing) Emitter::set(AAX_PLAYING);
        playing = true;
        return true;
    }

    bool stop() {
        playing = false;
        return Emitter::set(AAX_STOPPED);
    }

    bool buffer(Buffer& buffer) {
        Emitter::remove_buffer();
        frequency = buffer.get(AAX_UPDATE_RATE);
        return Emitter::add(buffer);
    }

    inline void set_pitch(float bend) { pitch_param = bend*pitch; }
    inline void set_pressure(uint8_t p) { pressure = p; }

private:
    inline float note2freq(uint8_t d) {
        return 440.0f*powf(2.0f, ((float)d-69.0f)/12.0f);
    }

    Param pitch_param = 1.0f;
    Param gain_param = 1.0f;
    float frequency = 220.0f;
    float pitch = 1.0f;
    uint8_t key_no = 0;
    uint8_t pressure = 0;
    bool playing = false;
};


class Instrument : public Mixer
{
private:
    Instrument(const Instrument& i) = delete;

    Instrument& operator=(const Instrument&) = delete;

public:
    Instrument(AeonWave& ptr)
        : Mixer(ptr), aax(&ptr)
    {
        Mixer::set(AAX_PLAYING);
    }

    friend void swap(Instrument& i1, Instrument& i2) noexcept {
        i1.key = std::move(i2.key);
        i1.aax = std::move(i2.aax);
    }

    Instrument& operator=(Instrument&&) = default;

    operator Mixer&() {
        return *this;
    }

    void play(size_t key_no, uint8_t velocity, Buffer& buffer, bool is_drums = false) {
        auto it = key.find(key_no);
        if (it == key.end()) {
            auto ret = key.insert({key_no, new Key(key_no)});
            it = ret.first;
            Mixer::add(*it->second);
            Mixer::add(buffer);
            it->second->buffer(buffer);
        }
        it->second->play(velocity, is_drums);
    }

    void stop(size_t key_no) {
        auto it = key.find(key_no);
        if (it != key.end()) {
            it->second->stop();
        }
    }

    inline void set_pitch(float p) {
        pitch = p;
        for (auto& it : key) {
            it.second->set_pitch(pitch);
        }
    }

    inline void set_pressure(uint8_t p) { pressure = p; }

    void set_pressure(uint8_t key_no, uint8_t pressure) {
        auto it = key.find(key_no);
        if (it != key.end()) {
            it->second->set_pressure(pressure);
        }
    }

private:
    std::map<uint8_t,Key*> key;
    AeonWave* aax;

    uint8_t pressure;
    float pitch;
};

} // namespace aax

#endif

