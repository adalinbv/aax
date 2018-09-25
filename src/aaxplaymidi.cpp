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

#include "driver.h"
#include "midi.hpp"

#define IFILE_PATH		SRC_PATH"/beethoven_opus10_3.mid"
#define INSTRUMENT		"instruments/piano-accoustic"
#define INSTRUMENTS		"gmmidi.xml"
#define DRUMS			"gmdrums.xml"

void
help()
{
    aaxConfig cfgi, cfgo;

    printf("aaxplaymidi version %i.%i.%i\n\n", AAX_UTILS_MAJOR_VERSION,
                                           AAX_UTILS_MINOR_VERSION,
                                           AAX_UTILS_MICRO_VERSION);
    printf("Usage: aaxplaymidi [options]\n");
    printf("Plays a midi file to an audio output device.\n");

    printf("\nOptions:\n");
    printf("  -i, --input <file>\t\tplayback audio from a file\n");
    printf("  -d, --device <device>\t\tplayback device (default if not specified)\n");
    printf("  -v, --verbose\t\t\tshow extra playback information\n");
    printf("  -h, --help\t\t\tprint this message and exit\n");

    printf("\n");

    exit(-1);
}

int verbose = 0;
int main(int argc, char **argv)
{
    if (argc == 1 || getCommandLineOption(argc, argv, "-h") ||
                     getCommandLineOption(argc, argv, "--help"))
    {
        help();
    }

    char *devname = getDeviceName(argc, argv);
    char *infile = getInputFile(argc, argv, IFILE_PATH);
    try
    {
        aax::MIDIFile midi(devname, infile);
        if (midi)
        {
            int64_t sleep_us, dt_us;
            uint64_t time_parts = 0;
            uint32_t wait_parts;
            struct timeval now;

            if (getCommandLineOption(argc, argv, "-v") ||
                getCommandLineOption(argc, argv, "--verbose"))
            {
               midi.set_verbose(true);
            }

#if 1
            midi.set_file_path(SRC_PATH"/midi");
            midi.set_instrument_file(INSTRUMENTS);
            midi.set_drum_file(DRUMS);
#endif

            midi.initialize();
            midi.start();

            wait_parts = 1000;
            set_mode(1);

            gettimeofday(&now, NULL);
            dt_us = -(now.tv_sec * 1000000 + now.tv_usec);
            do
            {
                if (!midi.process(time_parts, wait_parts)) break;

                if (wait_parts > 0)
                {
                    uint32_t wait_us;

                    gettimeofday(&now, NULL);
                    dt_us += now.tv_sec * 1000000 + now.tv_usec;

                    wait_us = wait_parts*midi.get_uspp();
                    sleep_us = wait_us - dt_us;
                    if (sleep_us > 0) {
                       usecSleep(sleep_us);
                    }

                    gettimeofday(&now, NULL);
                    dt_us = -(now.tv_sec * 1000000 + now.tv_usec);

                    time_parts += wait_parts;
                }
            }
            while(!get_key());
            set_mode(0);

            midi.stop();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error while processing the MIDI file: "
                  << e.what() << std::endl;
    }


    return 0;
}


