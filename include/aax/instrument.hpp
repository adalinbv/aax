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

#include <aax/aeonwave.hpp> 

namespace aax
{

class Note : public Emitter
{
public:
    Note(Buffer& buffer) : Emitter(AAX_RELATIVE), pitch(1.0f), gain(1.0f)
    {
        add(buffer);
        tie(pitch, AAX_PITCH_EFFECT, AAX_PITCH);
        tie(gain, AAX_VOLUME_FILTER, AAX_GAIN);
    }

    Note(const Note& n) = default;

    friend void swap(Note& n1, Note& n2) noexcept {
        std::swap(static_cast<Emitter&>(n1), static_cast<Emitter&>(n2));
        std::swap(n1.pitch, n2.pitch);
        std::swap(n1.gain, n2.gain);
    }

    operator Emitter&() {
        return *this;
    }

    void play(unsigned char note)
    {
        pitch = NoteToPitch(note);
        set(AAX_PLAYING);
    }

    void stop() {
        set(AAX_STOPPED);
    }

private:
    inline float NoteToPitch(unsigned char note) {
        return 2.0f*powf(2.0f, (note-49)/12.0f);
    }

    Param pitch, gain;
};


class Instrument : public Mixer
{
public:
    Instrument(AeonWave& ptr, std::string& name)
        : Mixer(ptr), buffer(ptr.buffer(name)), aax(ptr)
    {
        add(buffer);
        set(AAX_PLAYING);
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
        std::swap(i1.notes, i2.notes);
        std::swap(i1.buffer, i2.buffer);
        std::swap(i1.aax, i2.aax);
    }

    operator Frame&() {
        return *this;
    }

    size_t create()
    {
        notes.push_back(Note(buffer));
        add(notes.back());
        return notes.size()+1;
    }

    void remove(size_t id)
    {
        if (id) {
            Mixer::remove(notes.at(--id));
            notes.erase(notes.begin()+id);
        }
    }

    inline void play(size_t id, unsigned char note) {
        if (id) notes.at(id-1).play(note);
    }

    inline void stop(size_t id) {
        if (id) notes.at(id-1).stop();
    }

private:
    std::vector<Note> notes;
    Buffer buffer;
    AeonWave aax;
};

} // namespace aax

#endif

