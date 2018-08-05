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

#include <stdio.h>		/* for NULL */
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

void printNote(unsigned char note)
{
    const char *notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    LOG("%s%i", notes[note % 12], (note / 12)-1);
}

///////////////////////////////////////////////////////////////
// MidiStream 
///////////////////////////////////////////////////////////////
class MidiStream
{
public:
    unsigned char *m_pTrack = NULL;
    unsigned char *m_pTrackFin = NULL;

    MidiStream(unsigned char* ini, size_t& length)
    {
        m_pTrack = ini;
        m_pTrackFin = ini+length;
    }

    uint32_t GetVar()
    {
        uint32_t out = 0;

        for (;;)
        {
            out <<= 7;
            out += *m_pTrack & 0x7f;

            if ((*m_pTrack & 0x80) == 0x00)
                break;

            m_pTrack++;
        }

        m_pTrack++;

        return out;
    }

    void Back()
    {
        m_pTrack--;
    }

    unsigned char GetByte()
    {
        return *m_pTrack++;
    }

    void Skip(int count)
    {
        m_pTrack+= count;
    }

    uint32_t GetLength(int bytes)
    {
        uint32_t val = 0;
        for (int i = 0; i < bytes; i++)
            val = (val << 8) + GetByte();

        return val;
    }

    bool done()
    {
        int left = m_pTrackFin - m_pTrack;
        return left <= 0;
    }
};

///////////////////////////////////////////////////////////////
// MidiTrack
///////////////////////////////////////////////////////////////

uint32_t ticksPerQuarterNote=24;
float quarterNote = 24;
uint32_t bpm = 120;

class MidiTrack : public MidiStream
{
    bool m_omni;
    float m_nextTime;
    uint32_t m_channel;
    unsigned char m_lastType;
public:

    MidiTrack(uint32_t channel, unsigned char *ini, size_t length)
        : MidiStream(ini, length)
    {
        m_nextTime = (float)GetVar();
        m_channel = channel;
        m_omni = false;
    }

    void play(float seconds)
    {
        while (m_nextTime <= seconds)
        {
            if (done() == true)
            {
                break;
            }

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

                    if (subType == 8)
                    {
                        i = instr.release(seconds, m_omni ? channel : -1, key, speed);
                    }
                    else if (subType == 9)
                    {
                        i = instr.push(seconds, m_omni ? channel : -1, key, speed);
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
                else {
                    LOG("    unknown type: 0x%02x", type);
                }
            }            

            LOG("\n");

            if (done())
                break;

            float c = (float)(bpm * ticksPerQuarterNote) / 60.0f;

            uint32_t deltaTicks = GetVar();
            if (c >0)
                m_nextTime += deltaTicks / c;
        }
    }
};

///////////////////////////////////////////////////////////////
// Midi
///////////////////////////////////////////////////////////////

class Midi
{
public:
    Midi() : tracks(0), time(0.0f) {}

    Midi(const char *filename) : Midi()
    {
        std::ifstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);
        size_t size = file.tellg();
        std::streamsize fileSize = size;
        file.seekg(0, std::ios::beg);

        midi.resize(size);
        if (midi.size() && file.read((char*)midi.data(), fileSize))
        {
            MidiStream ch(midi.data(), size);

            if (ch.GetByte() == 'M' && ch.GetByte() == 'T' &&
                ch.GetByte() == 'h' && ch.GetByte() == 'd')
            {
                ch.GetLength(4);

                uint32_t format = (ch.GetByte() << 8) + ch.GetByte();
                uint32_t tracks = (ch.GetByte() << 8) + ch.GetByte();
                ticksPerQuarterNote = (ch.GetByte() << 8) + ch.GetByte();

                LOG("format %i, tracks %i, ticksPerQuarterNote %i\n", format, tracks, ticksPerQuarterNote);
            }

            while (ch.done() == false)
            {
                if (ch.GetByte() == 'M' && ch.GetByte() == 'T' &&
                    ch.GetByte() == 'r' && ch.GetByte() == 'k')
                {
                    int length = ch.GetLength(4);
                    pTrack[tracks] = new MidiTrack(tracks, ch.m_pTrack, length);
                    ch.Skip(length);
                    tracks++;
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

private:
    std::vector<unsigned char> midi;

    MidiTrack *pTrack[50];
    uint32_t tracks;
    float time;
};


int main(int argc, char **argv)
{
    char *infile = getInputFile(argc, argv, IFILE_PATH);

    Midi midi(infile);
    if (midi)
    {
        aax::AeonWave aax(AAX_MODE_WRITE_STEREO);
        aax.set(AAX_INITIALIZED);
        aax.set(AAX_PLAYING);

        while(midi.render(aax)) {}

        aax.set(AAX_PROCESSED);
    }
    return 0;
}


