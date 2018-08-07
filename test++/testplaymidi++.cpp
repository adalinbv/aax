/*
 *  MIT License
 * 
 * Copyright (c) 2018 
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// Based on: https://github.com/aguaviva/SimpleMidi

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#include <fstream>

#include <aax/aeonwave.hpp>
#include <aax/instrument.hpp>

#include "buffer_map.hpp"
#include "driver.h"

#ifndef _countof
#define _countof(a) (sizeof(a)/sizeof(*(a)))
#endif


#define IFILE_PATH		SRC_PATH"/DaFunk.mid"
#define INSTRUMENT		"instruments/piano-accoustic"
#define INSTRUMENTS		"gmmidi.xml"

#if 1
# define LOG			printf
#endif

typedef buffer_map<unsigned char> MidiBuffer;
int verbose = 0;

void printNote(unsigned char note)
{
    const char *notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    LOG("%s%i", notes[note % 12], (note / 12)-1);
}

class Piano
{
    struct Channel
    {
        float time;
        uint32_t velocity;
        unsigned char note;
    };

    Channel m_channel[20];
    float m_noteDuration;

public:
    Piano()
    {
        m_noteDuration = 0.3f; // in seconds

        for (int i = 0; i < _countof(m_channel); i++)
            memset(&m_channel[i], 0, sizeof(Channel));
    }

    int stop(float t, char channelId, unsigned char note, unsigned char velocity)
    {
        if (channelId == -1)
        {
            // find the requested note
            for (int i = 0; i < _countof(m_channel); i++)
            {
                if (m_channel[i].note == note)
                {
                    channelId = i;
                    break;
                }
            }
        }

        if (channelId >=0) // stop playback
        {
            m_channel[channelId].note = 0;
            m_channel[channelId].velocity = velocity;
            m_channel[channelId].time = t;
        }

        return channelId;
    }

    int start(float t, char channelId, unsigned char note, unsigned char velocity)
    {
        if (channelId == -1)
        {
            // find an empty note
            for (int i = 0; i < _countof(m_channel); i++)
            {
                if (m_channel[i].note == 0)
                {
                    channelId = i;
                    break;
                }
            }
        }

        if (channelId >= 0)     // start playback
        {
            m_channel[channelId].note = note;
            m_channel[channelId].velocity = velocity;
            m_channel[channelId].time = t;
        }

        return channelId;
    }

    float synthesize(float time)
    {
        float val = 0;

        for (int i = 0; i < _countof(m_channel); i++)
        {
            if (m_channel[i].note > 0)
            {
                float vol = ((m_channel[i].time + m_noteDuration) - time) / m_noteDuration;

                if (vol > 0)
                    val += renderNote(time, NoteToFreq(m_channel[i].note), vol);
                else
                    m_channel[i].note = 0;
            }
        }

        val /= _countof(m_channel);

        return val;
    }

private:
    float NoteToFreq(int note) {
        return (float)pow(2.0f, (note - 49) / 12.0f) * 440.0f;
    }

    float renderNote(float time, float freq, float vol)
    {
        float v = (float)(cosf(2.0f * (float)M_PI * time * freq));
        if (v > 0) {
            v = vol;
        } else if (v < 0) {
            v = -vol;
        }
        return v;
    }
};

Piano piano;


///////////////////////////////////////////////////////////////
// MidiStream 
///////////////////////////////////////////////////////////////
class MidiStream
{
public:
    MidiStream(MidiBuffer& track) : track(track), pos(0) {}

    uint32_t GetVar()
    {
        uint32_t rv = 0;

        while(pos < track.size())
        {
            rv <<= 7;
            rv += track[pos] & 0x7f;
            if ((track[pos] & 0x80) == 0x00) {
                break;
            }
            pos++;
        }
        pos++;

        return rv;
    }

    inline void Back() {
        pos--;
    }

    inline unsigned char GetByte() {
        return track[pos++];
    }

    inline void Skip(int count) {
        pos += count;
    }

    uint32_t GetLength(int bytes)
    {
        uint32_t val = 0;
        for (int i = 0; i < bytes; i++)
            val = (val << 8) + GetByte();

        return val;
    }

    inline bool done() {
        return (pos == track.size());
    }

public:
    MidiBuffer track;
    size_t pos;
};

///////////////////////////////////////////////////////////////
// MidiTrack
///////////////////////////////////////////////////////////////

class MidiTrack : public MidiStream
{
public:
    MidiTrack(uint32_t channel, MidiBuffer& track, uint32_t ppq)
        : MidiStream(track), m_nextTime(GetVar()),
          m_channel(channel), PPQ(ppq), m_omni(false) {}

    void play(float seconds)
    {
printf("m_nextTime: %f (%f)\n", m_nextTime, seconds);
        if (!done() && m_nextTime <= seconds)
        {
            unsigned char type = GetByte();
            
            if ((type & 0x80) == 0)
            {
                Back();
                type = m_lastType;
            }
            else
            {                    
                m_lastType = type;
            }

            LOG("%i ", m_channel);
            LOG("%6.3f ", m_nextTime);
            LOG("0x%02x ", type);            

            if (type == 0xff)
            {
                unsigned char metaType = GetByte();
                unsigned char length = GetByte();

                LOG("    meta: 0x%02x", metaType);
                LOG("    length: 0x%02x", length);

                if (metaType == 0x01)
                {
                    LOG("    text event: ");
                    for (int i = 0; i<length; i++)
                        LOG("%c", GetByte());
                }
                else if (metaType == 0x03)
                {
                    LOG("    track name: ");
                    for (int i = 0; i<length; i++)
                        LOG("%c", GetByte());
                }
                else if (metaType == 0x09)
                {
                    LOG("    inst: ");
                    for (int i = 0; i<length; i++)
                    LOG("%c", GetByte());
                }
                else if (metaType == 0x20)
                {
                    m_channel = GetByte();                    
                }
                else if (metaType == 0x51)
                {
                    uint32_t tempo = GetLength(3);
                    bpm =  60 * 1000000 / tempo;
                    LOG("    tempo: %i bpm", bpm);
                }
                else if (metaType == 0x54)
                {
                    unsigned char hr = GetByte();
                    unsigned char mn = GetByte();
                    unsigned char se = GetByte();
                    unsigned char fr = GetByte();
                    unsigned char ff = GetByte();
                }
                else if (metaType == 0x58)
                {
                    unsigned char nn = GetByte();
                    unsigned char dd = (unsigned char)pow(2,GetByte());
                    unsigned char cc = GetByte();
                    unsigned char bb = GetByte();
                    
                    quarterNote = 100000.0f / (float)cc;

                    LOG("    time sig: %i/%i MIDI clocks per quarter-dotted %i", nn,dd, cc);
                }
                else if (metaType == 0x59)
                {
                    unsigned char sf = GetByte();
                    unsigned char mi = GetByte();

                    //LOG("    sf: %i mi: %i", sf, mi);
                }
                else if (metaType == 0x2F)
                {
                    unsigned char nn = GetByte();
                    LOG("************ THE END ************\n");
                    return;
                }
                else
                {
                    LOG("    unknown metaType: 0x%02x", metaType);
                    Skip(length);
                }
            }
            else if (type == 0xf0)
            {
                unsigned char length = GetByte();
                Skip(length);
            }
            else if (type == 0xf7)
            {
                unsigned char length = GetByte();
                Skip(length);
            }
            else
            {
                unsigned char subType = type >> 4;
                unsigned char channel = type & 0xf;

                if (subType == 0x8 || subType == 0x9)
                {                    
                    unsigned char key = GetByte();
                    unsigned char speed = GetByte();
                    int i;

                    if (subType == 8) {
                        i = piano.stop(seconds, m_omni ? channel : -1, key, speed);
                    } else if (subType == 9) {
                        i = piano.start(seconds, m_omni ? channel : -1, key, speed);
                    }

                    //'LOG("    ac: %c ch: %i note: ", channel + 0*(subType == 8) ?'^':'v', i);
                    LOG("    ac: %c ch: %i note: ", (subType == 8) ? '^' : 'v', channel);
                    printNote(key);
                }
                else if (subType == 0xA)
                {
                    unsigned char cc = GetByte();
                    unsigned char nn = GetByte();

                    //LOG("    ch: %i ch: %i controller: %2i controller: %2i", channel, m_channel, cc, nn);
                }
                else if (subType == 0xB)
                {
                    unsigned char cc = GetByte();
                    unsigned char nn = GetByte();

                    if (cc == 0x78)
                    {
                        m_omni = false;
                    }
                    else  if (cc == 0x7c)
                    {
                        m_omni = false;
                    }
                    else if (cc == 0x7d)
                    {
                        m_omni = true;
                    }
                    //LOG("    ch: %i ch: %i controller: %2i controller: %2i", channel, m_channel, cc, nn);
                }
                else if (subType >= 0xC && subType <= 0xE)
                {
                    unsigned char val = GetByte();
                }
                else if (subType == 0xE)
                {
                    unsigned char lsb = GetByte();
                    unsigned char msb = GetByte();
                }
                else if (verbose) {
                    LOG("    unknown type: 0x%02x", type);
                }
            }            

            LOG("\n");

            if (!done())
            {
                float c = (float)(bpm * PPQ) / 60.0f;

                uint32_t deltaTicks = GetVar();
                if (c >0) {
                    m_nextTime += deltaTicks / c;
                }
            }
        }
    }

private:
    float m_nextTime;
    uint32_t m_channel;
    uint32_t PPQ;
    unsigned char m_lastType;
    bool m_omni;

    float quarterNote = 24;
    uint32_t bpm = 120;
};

///////////////////////////////////////////////////////////////
// Midi
///////////////////////////////////////////////////////////////

class Midi
{
public:
    Midi() : no_tracks(0), time(0.0f) {}

    Midi(const char *filename) : Midi()
    {
        std::ifstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);
        size_t size = file.tellg();
        std::streamsize fileSize = size;
        file.seekg(0, std::ios::beg);

        midi.resize(size);
        if (midi.size() && file.read((char*)midi.data(), fileSize))
        {
            MidiBuffer map(midi.data(), size);
            MidiStream ch(map);

            if (ch.GetByte() == 'M' && ch.GetByte() == 'T' &&
                ch.GetByte() == 'h' && ch.GetByte() == 'd')
            {
                ch.GetLength(4);

                uint32_t format = (ch.GetByte() << 8) + ch.GetByte();
                uint32_t no_tracks = (ch.GetByte() << 8) + ch.GetByte();
                PPQ = (ch.GetByte() << 8) + ch.GetByte();

                LOG("format %i, tracks %i, PPQ %i\n", format, no_tracks, PPQ);
            }

            while (no_tracks < 50 && ch.done() == false)
            {
                if (ch.GetByte() == 'M' && ch.GetByte() == 'T' &&
                    ch.GetByte() == 'r' && ch.GetByte() == 'k')
                {
                    int length = ch.GetLength(4);
                    track[no_tracks] = new MidiTrack(no_tracks, ch.track, PPQ);
                    ch.Skip(length);
                    no_tracks++;
                }
            }
        }
        else if (!midi.size()) {
            LOG("Out of memory.\n");
        }
        else {
            LOG("Can't open %s\n", filename);
        }
    }

    ~Midi() = default;

    explicit operator bool() {
        return midi.size();
    }

    bool render(aax::AeonWave &aax)
    {
       static const float dt = 1e-3f;
       _aaxTimer *timer = _aaxTimerCreate();
       _aaxTimerStartRepeatable(timer, dt*1000000);
       float time = 0.0f;

       for(;;)
       {
          for (unsigned int t=0; t<no_tracks; ++t) {
             track[t]->play(time);
          }
          time += dt;
          _aaxTimerWait(timer);
       }
       _aaxTimerDestroy(timer);

       return true;
    }

private:
    std::vector<unsigned char> midi;

    MidiTrack *track[50];
    uint32_t no_tracks;
    float time;
 
    uint32_t PPQ = 24; // ticks per quarter note
};


int main(int argc, char **argv)
{
    char *infile = getInputFile(argc, argv, IFILE_PATH);

    if (getCommandLineOption(argc, argv, "-v") ||
        getCommandLineOption(argc, argv, "--verbose"))
    {
       verbose = 1;
    }

    Midi file(infile);
    if (file)
    {
        aax::AeonWave aax(AAX_MODE_WRITE_STEREO);
        aax.set(AAX_INITIALIZED);
        aax.set(AAX_PLAYING);

        while(file.render(aax)) {}

        aax.set(AAX_PROCESSED);
    }
    return 0;
}


