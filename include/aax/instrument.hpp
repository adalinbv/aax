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

class Note
{
public:
    Note(AeonWave& ptr, Buffer buffer)
        : aax(ptr), emitter(Emitter(AAX_RELATIVE))
    {
        emitter.add(buffer);
    }

    ~Note() {
        emitter.set(AAX_PROCESSED);
    }

    void play(unsigned char note)
    {
        dsp pitch(aax, AAX_PITCH_EFFECT);
        pitch.set(AAX_PITCH, NoteToPitch(note));
        emitter.set(pitch);
        emitter.set(AAX_PLAYING);
    }

    void stop() {
        emitter.set(AAX_STOPPED);
    }

    friend void swap(Note& n1, Note& n2) {
        std::swap(n1.emitter, n2.emitter);
        std::swap(n1.aax, n2.aax);
    }

    Note& operator=(Note n) {
        swap(*this, n);
        return *this;
    }

    operator Emitter&() {
        return emitter;
    }

private:
    inline float NoteToPitch(unsigned char note) {
        return 2.0f*powf(2.0f, (note-49)/12.0f);
    }

    Emitter emitter;
    AeonWave &aax;
};


class Instrument
{
public:
    Instrument(AeonWave& ptr, std::string& name)
        : frame(Frame(ptr)), buffer(ptr.buffer(name)), aax(ptr)
    {
        frame.add(buffer);
        frame.set(AAX_PLAYING);
        aax.add(frame);
    }

    ~Instrument()
    {
        frame.set(AAX_PROCESSED);
        for (size_t i=0; i<notes.size(); ++i) {
            frame.remove(notes[i]);
        }
        aax.remove(frame);
        aax.destroy(buffer);
    }

    size_t create()
    {
        notes.push_back(Note(aax, buffer));
        frame.add(notes.back());
        return notes.size()+1;
    }

    void remove(size_t id)
    {
        if (id) {
            frame.remove(notes[--id]);
            notes.erase(notes.begin()+id);
        }
    }

    inline void play(size_t id, unsigned char note) {
        if (id) notes[id-1].play(note);
    }

    inline void stop(size_t id) {
        if (id) notes[id-1].stop();
    }

private:
    std::vector<Note> notes;
    Frame frame;

    Buffer& buffer;
    AeonWave &aax;
};

} // namespace aax

#endif

