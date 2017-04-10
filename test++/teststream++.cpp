/*
 * Copyright (C) 2008-2016 by Erik Hofman.
 * Copyright (C) 2009-2016 by Adalin B.V.
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
#include "config.h"
#endif

#include <stdio.h>

#include <aax/aeonwave.hpp>

#include "base/types.h"
#include "driver.h"

#define FILE_PATH		SRC_PATH"/tictac.wav"

int main(int argc, char **argv)
{
    int res;
    enum aaxRenderMode mode = aaxRenderMode(getMode(argc, argv));
    char* devname = getDeviceName(argc, argv);
    char* infile = getInputFile(argc, argv, FILE_PATH);

    aax::AeonWave config(devname, mode);
    testForError(config, "No default audio device available.");

    if (config)
    {
        aax::Buffer buffer = config.buffer(infile);
        if (buffer)
        {
            aax::Emitter emitter(AAX_ABSOLUTE);
            const int NUM_BUFFERS = 8;
            aax::dsp dsp;
            float pitch;
            int i, num;

            /** emitter */
            if ((pitch = getPitch(argc, argv)) > 0.0f)
            {
                dsp = emitter.get(AAX_PITCH_EFFECT);
                dsp.set(AAX_PITCH, pitch);
                res = emitter.set(dsp);
                testForState(res, "aaxEmitterSetPitch");
            }

            dsp = emitter.get(AAX_VOLUME_FILTER);
            dsp.set(AAX_GAIN, 0.5f);
            res = emitter.set(dsp);
            testForState(res, "aaxEmitterSetGain");

            /** buffer */
            for (i=0; i<NUM_BUFFERS; i++)
            {
                res = emitter.add(buffer);
                testForState(res, "aaxEmitterAddBuffer");
            }

            /** mixer */
            res = config.set(AAX_INITIALIZED);
            testForState(res, "aaxMixerInit");

            res = config.add(emitter);
            testForState(res, "aaxMixerRegisterEmitter");

            res = config.set(AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /** schedule the emitter for playback */
            res = emitter.set(AAX_PLAYING);
            testForState(res, "aaxEmitterStart");

            num = 0;
            printf("playing buffer #%i\n", num);
            while (num < 10)
            {
                if (emitter.get(AAX_PROCESSED) > 1)
                {
                    aax::Buffer buffer = emitter.get(0);
                    emitter.remove_buffer();
                    emitter.add(buffer);
                    num += 1;
                    printf("playing buffer #%i\n", num);
                }

                msecSleep(50);
            }

            res = config.remove(emitter);
            res = config.set(AAX_STOPPED);
        }
    }

    return 0;
}
