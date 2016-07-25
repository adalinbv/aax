/*
 * Copyright (C) 2015-2016 by Erik Hofman.
 * Copyright (C) 2015-2016 by Adalin B.V.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY ADALIN B.V. ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL ADALIN B.V. OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUTOF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of Adalin B.V.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <iostream>
#include <limits>

#include <aax/aeonwave.hpp>

static int maximumWidth = 80;

int main(int argc, char **argv)
{
    aax::AeonWave aax(AAX_MODE_WRITE_STEREO);

    std::cout << "AeonWave version " << aax::major_version() << "." << aax::minor_version() << "-" << aax::patch_level() << std::endl;
    std::cout << "Run aaxinfo -copyright to read the copyright information." << std::endl << std::endl;

    std::cout << "Devices that support capture:" << std::endl;
    while (const char* d = aax.drivers(AAX_MODE_READ)) {
        while (const char* r = aax.devices()) {
            while (const char* i = aax.interfaces()) {
                std::cout << " '" << d;
                if (*r) std::cout << " on " << r;
                if (*i) std::cout << ": " << i ;
                std::cout << "'" << std::endl;
            }
        }
    }
    std::cout << std::endl;

    std::cout << "Devices that support playback:" << std::endl;
    while (const char* d = aax.drivers()) {
        while (const char* r = aax.devices()) {
            while (const char* i = aax.interfaces()) {
                std::cout << " '" << d;
                if (*r) std::cout << " on " << r;
                if (*i) std::cout << ": " << i ;
                std::cout << "'" << std::endl;
            }
        }
    }
    std::cout << std::endl;

    if (aax.set(AAX_INITIALIZED))
    {
        std::cout << "Driver  : " << aax.info(AAX_DRIVER_STRING) << std::endl;
        std::cout << "Renderer: " << aax.info(AAX_RENDERER_STRING) << std::endl;
        std::cout << "Version : " << aax.version() << " (" << aax::major_version() << "." << aax::minor_version() << ")" << std::endl;
        std::cout << "Vendor  : " << aax.info(AAX_VENDOR_STRING) << std::endl;
        std::cout << "Mixer timed mode support:   " << (aax.get(AAX_TIMER_MODE) ? "yes" : "no") << std::endl;
        std::cout << "Mixer shared mode support:  " << (aax.get(AAX_SHARED_MODE) ? "yes" : "no") << std::endl;
        std::cout << "Mixer batched mode support: " << (aax.get(AAX_BATCHED_MODE) ? "yes" : "no") << std::endl;
        std::cout << "Mixer supported track range: " << aax.get(AAX_TRACKS_MIN) << " - " << aax.get(AAX_TRACKS_MAX) << " tracks" << std::endl;
        std::cout << "Mixer frequency range: " << aax.get(AAX_FREQUENCY_MIN)/1000 << "kHz - " << aax.get(AAX_FREQUENCY_MAX)/1000 << "kHz" << std::endl;
        std::cout << "Mixer frequency: " << aax.get(AAX_FREQUENCY) << " Hz" << std::endl;
        std::cout << "Mixer refresh rate: " << aax.get(AAX_REFRESHRATE) << " Hz" << std::endl;
        std::cout << "Mixer update rate:  " << aax.get(AAX_UPDATERATE) << " Hz" << std::endl;
        std::cout << "Mixer latency: " << aax.get(AAX_LATENCY)*1e-3f << " ms" << std::endl;

        unsigned int x = aax.get(AAX_MONO_EMITTERS);
        unsigned int uint_max = std::numeric_limits<unsigned int>::max();
        std::cout << "Available mono emitters:   ";
        if (x == uint_max) std::cout << "infinite" << std::endl;
        else std::cout << x << std::endl;

        x = aax.get(AAX_STEREO_EMITTERS);
        std::cout << "Available stereo emitters: "; 
        if (x == uint_max/2) std::cout << "infinite" << std::endl;
        else std::cout << x << std::endl;

        x = aax.get(AAX_AUDIO_FRAMES);
        std::cout << "Available audio-frames:    ";
        if (x == uint_max) std::cout << "infinite" << std::endl;
        else std::cout << x << std::endl;
        std::cout << std::endl;

        std::cout << "Supported Filters:" << std::endl;
        std::string str = "  ";
        size_t l = str.size();
        for (enum aaxFilterType f = aaxFilterType(AAX_FILTER_NONE+1);
             f < AAX_FILTER_MAX; f = aaxFilterType(f+1))
        {
            if (aax.supports(f))
            {
                std::string fs = aax.info(f);
                if ((l + fs.size()) > maximumWidth) {
                    fs = "\n   "+fs;
                    l = fs.size()+3;
                } else {
                    fs = " "+fs;
                    l += fs.size();
                }
                str += fs;
            }
        }
        std::cout << str << std::endl << std::endl;

        std::cout << "Supported Effects:" << std::endl;
        str = "  ";
        l = str.size();
        for (enum aaxEffectType e = aaxEffectType(AAX_EFFECT_NONE+1);
             e < AAX_EFFECT_MAX; e = aaxEffectType(e+1))
        {
            if (aax.supports(e))
            {
                std::string es = aax.info(e);
                if ((l + es.size()) > maximumWidth) {
                    es = "\n   "+es;
                    l = es.size()+3;
                } else {
                    es = " "+es;
                    l += es.size();
                }
                str += es;
            }
        }
        std::cout << str << std::endl << std::endl;
    }
    else {
        std::cout << "Error opening the default device: ";
        std::cout << aax::error() << std::endl << std::endl;
    }

    return 0;
}
