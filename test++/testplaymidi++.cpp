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

#define IFILE_PATH		SRC_PATH"/DaFunk.mid"
#define INSTRUMENT		"instruments/piano-accoustic"
#define INSTRUMENTS		"gmmidi.xml"


int main(int argc, char **argv)
{
    char *infile = getInputFile(argc, argv, IFILE_PATH);

    MIDIFile file(infile);
    if (file)
    {
        _aaxTimer *timer = _aaxTimerCreate();
        _aaxTimerStartRepeatable(timer, 1000.0f);	// 1000 usec
        uint32_t time = 0;

        aax::AeonWave aax(AAX_MODE_WRITE_STEREO);
        aax.set(AAX_INITIALIZED);
        aax.set(AAX_PLAYING);

        do {
            if (!file.process(time++)) break;
        }
        while(_aaxTimerWait(timer));

        aax.set(AAX_PROCESSED);
        _aaxTimerDestroy(timer);
    }
    return 0;
}


