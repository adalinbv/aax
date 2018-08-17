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
    Key(uint8_t key_no, Buffer& buffer) : Emitter(AAX_STEREO), key(key_no)
    {
        Emitter::add(buffer);

        pitch = note2freq(key_no)/buffer.get(AAX_FREQUENCY);
        tie(pitch, AAX_PITCH_EFFECT, AAX_PITCH);
        tie(gain, AAX_VOLUME_FILTER, AAX_GAIN);
    }

    friend void swap(Key& n1, Key& n2) noexcept {
        std::swap(static_cast<Emitter&>(n1), static_cast<Emitter&>(n2));
        n1.pitch = std::move(n2.pitch);
        n1.gain = std::move(n2.gain);
        n1.key = std::move(n2.key);
    }

    Key& operator=(Key&&) = default;

    operator Emitter&() {
        return *this;
    }

    bool play(uint8_t velocity)
    {
        Emitter::set(AAX_PROCESSED);
        gain = 2.0f*velocity/255.0f;
        return Emitter::set(AAX_PLAYING);
    }

    bool stop() {
        return Emitter::set(AAX_STOPPED);
    }

    inline void set_pressure(uint8_t p) { pressure = p; }

private:
    inline float note2freq(uint8_t d) {
        return 440.0f*powf(2.0f, ((float)d-69.0f)/12.0f);
    }

    Param pitch = 1.0f;
    Param gain = 1.0f;
    uint8_t key = 0;
    uint8_t pressure = 0;
};


class Instrument : public Mixer
{
private:
    Instrument(const Instrument& i) = delete;

    Instrument& operator=(const Instrument&) = delete;

public:
    Instrument(AeonWave& ptr, std::string& name)
        : Mixer(ptr), buffer(ptr.buffer(name,true)), aax(&ptr)
    {
        Mixer::add(buffer);
        Mixer::set(AAX_PLAYING);
    }

    ~Instrument()
    {
        if (aax) aax->destroy(buffer);
    }

    friend void swap(Instrument& i1, Instrument& i2) noexcept {
//      std::swap(static_cast<Frame&>(i1), static_cast<Frame&>(i2));
        i1.key = std::move(i2.key);
        i1.buffer = std::move(i2.buffer);
        i1.aax = std::move(i2.aax);
    }

    Instrument& operator=(Instrument&&) = default;

    operator Mixer&() {
        return *this;
    }

    void play(size_t key_no, uint8_t velocity) {
        auto it = key.find(key_no);
        if (it == key.end()) {
            auto ret = key.insert({key_no, new Key(key_no, buffer)});
            it = ret.first;
            Mixer::add(*it->second);
        }
        it->second->play(velocity);
    }

    void stop(size_t key_no) {
        auto it = key.find(key_no);
        if (it != key.end()) {
            it->second->stop();
        }
    }

    inline void set_pitch(uint8_t p) { pitch= p; }

    inline void set_pressure(uint8_t p) { pressure = p; }

    void set_pressure(uint8_t key_no, uint8_t pressure) {
        auto it = key.find(key_no);
        if (it != key.end()) {
            it->second->set_pressure(pressure);
        }
    }

    inline bool valid_buffer() { return !!buffer; }

private:
    std::map<uint8_t,Key*> key;
    Buffer& buffer;
    AeonWave* aax;

    uint8_t pressure;
    uint8_t pitch;
};

} // namespace aax

#endif

