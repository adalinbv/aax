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

// Inspired by: https://github.com/aguaviva/SimpleMidi

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <chrono>
#include <thread>

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

#include <aax/aeonwave.hpp>
#include <aax/instrument.hpp>

#include <midi/midi.hpp>

#include "driver.h"

#define IFILE_PATH		SRC_PATH"/beethoven_opus10_3.mid"
#define INSTRUMENT		"instruments/piano-accoustic"

void
help()
{
    aaxConfig cfgi, cfgo;

    printf("aaxplaymidi version %i.%i.%i\n\n", AAX_UTILS_MAJOR_VERSION,
                                           AAX_UTILS_MINOR_VERSION,
                                           AAX_UTILS_MICRO_VERSION);
    printf("Usage: aaxplaymidi [options]\n");
    printf("Plays a MIDI file to an audio output device.\n");

    printf("\nOptions:\n");
    printf("  -i, --input <file>\t\tplayback audio from a file\n");
    printf("  -d, --device <device>\t\tplayback device (default if not specified)\n");
    printf("  -s, --select <name|num>\tonly play the track with this name or number\n");
    printf("  -t, --time\t\t\tTime offset in seconds or (hh:)mm:ss\n");
//  printf("  -b, --batched\t\t\tprocess the file in batched (high-speed) mode.\n");
    printf("  -v, --verbose\t\t\tshow extra playback information\n");
    printf("  -h, --help\t\t\tprint this message and exit\n");

    printf("\nUse aaxplay for playing other audio file formats.\n");

    printf("\n");

    exit(-1);
}

static void sleep_for(float dt)
{
    if (dt > 1e-6f)
    {
        int64_t sleep_us = dt/1e-6f;
        std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
    }
    else // sub-microsecond
    {
        std::chrono::high_resolution_clock::time_point start;
        std::chrono::nanoseconds min_duration;

        min_duration = std::chrono::nanoseconds::zero();
        start = std::chrono::high_resolution_clock::now();
        while (std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - start).count() < dt) {
            std::this_thread::sleep_for(min_duration);
        }
    }
}

void play(char *devname, enum aaxRenderMode mode, char *infile, char *outfile, const char *track, float time_offs, const char *grep, bool verbose, bool batched)
{
    if (grep) devname = (char*)"None"; // fastest for searching
    aax::MIDIFile midi(devname, infile, track, mode);
    if (midi)
    {
        aax::Sensor file;
        int64_t sleep_us, dt_us;
        uint64_t time_parts = 0;
        uint32_t wait_parts;
        struct timeval now;
        char obuf[256];

        if (outfile)
        {
            snprintf(obuf, 256, "AeonWave on Audio Files: %s", outfile);

            file = aax::Sensor(obuf, AAX_MODE_WRITE_STEREO);
            midi.add(file);
            file.set(AAX_INITIALIZED);
            file.set(AAX_PLAYING);
        }

        midi.set_verbose(verbose);
        midi.initialize(grep);
        if (!grep)
        {
            midi.start();

            if (batched) {
                midi.sensor(AAX_CAPTURING);
            }


            wait_parts = 1000;
            set_mode(1);

            gettimeofday(&now, NULL);
            dt_us = -(now.tv_sec * 1000000 + now.tv_usec);
            do
            {
                if (!midi.process(time_parts, wait_parts)) break;

                if (wait_parts > 0 && midi.get_pos_sec() >= time_offs)
                {
                    uint32_t wait_us;

                    gettimeofday(&now, NULL);
                    dt_us += now.tv_sec * 1000000 + now.tv_usec;

                    wait_us = wait_parts*midi.get_uspp();
                    sleep_us = wait_us - dt_us;

                    if (sleep_us > 0)
                    {
                        if (batched)
                        {
                            midi.sensor(AAX_UPDATE);
                            midi.wait(sleep_us*1e-6f);
                            midi.get_buffer();
                        }
                        else {
                            sleep_for(sleep_us*1e-6f);
                        }
                    }

                    gettimeofday(&now, NULL);
                    dt_us = -(now.tv_sec * 1000000 + now.tv_usec);
                }
                time_parts += wait_parts;
            }
            while(!get_key());
            set_mode(0);
            midi.stop();
        }
    }
}

int verbose = 0;
int main(int argc, char **argv)
{
    if (argc == 1 || getCommandLineOption(argc, argv, "-h") ||
                     getCommandLineOption(argc, argv, "--help"))
    {
        help();
    }

    enum aaxRenderMode render_mode = aaxRenderMode(getMode(argc, argv));
    char *devname = getDeviceName(argc, argv);
    char *infile = getInputFile(argc, argv, IFILE_PATH);
    bool verbose = false;
    bool batched = false;
    try
    {
        float time_offs = getTime(argc, argv);
        char *outfile = getOutputFile(argc, argv, NULL);
        const char *track, *grep;

        track = getCommandLineOption(argc, argv, "-s");
        if (!track) {
            track = getCommandLineOption(argc, argv, "--select");
        }

        grep = getCommandLineOption(argc, argv, "-g");
        if (!grep) {
            grep = getCommandLineOption(argc, argv, "--grep");
        }

        if (getCommandLineOption(argc, argv, "-v") ||
            getCommandLineOption(argc, argv, "--verbose"))
        {
            verbose = true;
        }

        if (getCommandLineOption(argc, argv, "-b") ||
            getCommandLineOption(argc, argv, "--batched"))
        {
            batched = true;
        }

        std::thread midiThread(play, devname, render_mode, infile, outfile, track, time_offs, grep, verbose, batched);
        midiThread.join();

    } catch (const std::exception& e) {
        std::cerr << "Error while processing the MIDI file: "
                  << e.what() << std::endl;
    }

    return 0;
}

