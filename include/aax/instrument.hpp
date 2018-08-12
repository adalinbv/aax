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
public:
    Key(Buffer& buffer) : Emitter(AAX_STEREO), pitch(1.0f), gain(1.0f)
    {
        Emitter::add(buffer);
        tie(pitch, AAX_PITCH_EFFECT, AAX_PITCH);
        tie(gain, AAX_VOLUME_FILTER, AAX_GAIN);
    }

    Key(const Key& n) = default;

    friend void swap(Key& n1, Key& n2) noexcept {
        std::swap(static_cast<Emitter&>(n1), static_cast<Emitter&>(n2));
        std::swap(n1.pitch, n2.pitch);
        std::swap(n1.gain, n2.gain);
    }

    operator Emitter&() {
        return *this;
    }

    bool play(uint8_t note)
    {
        pitch = NoteToPitch(note);
        return Emitter::set(AAX_PLAYING);
    }

    bool stop() {
        return Emitter::set(AAX_STOPPED);
    }

private:
    inline float NoteToPitch(uint8_t note) {
        return powf(2.0f, (note-49)/12.0f);
    }

    Param pitch, gain;
};


class Instrument : public Mixer
{
public:
    Instrument(AeonWave& ptr, std::string& name)
        : Mixer(ptr), buffer(ptr.buffer(name)), aax(ptr)
    {
        Mixer::add(buffer);
        Mixer::set(AAX_PLAYING);
        aax.add(*this);
    }

    Instrument(const Instrument& i) = default;

    ~Instrument()
    {
        aax.remove(*this);
        aax.destroy(buffer);
    }

    friend void swap(Instrument& i1, Instrument& i2) noexcept {
        std::swap(static_cast<Mixer&>(i1), static_cast<Mixer&>(i2));
        std::swap(i1.key, i2.key);
        std::swap(i1.buffer, i2.buffer);
        std::swap(i1.aax, i2.aax);
    }

    operator Mixer&() {
        return *this;
    }

    void play(size_t key_no, uint8_t note) {
        auto it = key.find(key_no);
        if (it == key.end()) {
            key.insert({key_no, Key(buffer)});
            it = key.find(key_no);
        }
        it->second.play(note);
    }

    void stop(size_t key_no) {
        auto it = key.find(key_no);
        if (it != key.end()) {
            it->second.stop();
        }
    }

private:
    std::map<uint8_t,Key> key;
    Buffer buffer;
    AeonWave aax;
};

} // namespace aax

#endif

