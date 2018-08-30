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


int main(int argc, char **argv)
{
    char *devname = getDeviceName(argc, argv);
    char *infile = getInputFile(argc, argv, IFILE_PATH);

    try
    {
        aax::MIDIFile midi(devname, infile);
        if (midi)
        {
            int64_t sleep_us, dt_us;
            uint64_t time_us = 0;
            uint32_t wait_us;
            struct timeval now;

#if 1
            midi.set_file_path(SRC_PATH"/midi");
            midi.set_instrument_file(INSTRUMENTS);
            midi.set_drum_file(DRUMS);
#endif

            midi.set(AAX_REFRESH_RATE, 90.0f);
            midi.set(AAX_INITIALIZED);
            midi.set(AAX_PLAYING);

            wait_us = 1000;
            set_mode(1);

            gettimeofday(&now, NULL);
            dt_us = -(now.tv_sec * 1000000 + now.tv_usec);
            do
            {
                if (!midi.process(time_us, wait_us)) break;

                if (wait_us > 0)
                {
                    gettimeofday(&now, NULL);
                    dt_us += now.tv_sec * 1000000 + now.tv_usec;

                    sleep_us = wait_us - dt_us;
                    if (sleep_us > 0) {
                       usecSleep(sleep_us);
                    }

                    gettimeofday(&now, NULL);
                    dt_us = -(now.tv_sec * 1000000 + now.tv_usec);

                    time_us += wait_us;
                }
            }
            while(!get_key());
            set_mode(0);

            midi.set(AAX_PROCESSED);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error while processing the MIDI file: "
                  << e.what() << std::endl;
    }


    return 0;
}


