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
#include <math.h>

#include <aax/defines.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define FILE_PATH                    SRC_PATH"/wasp.wav"

int main(int argc, char **argv)
{
    char *devname, *infile;
    aaxConfig config;
    int num, res;

    devname = getDeviceName(argc, argv);
    infile = getInputFile(argc, argv, FILE_PATH);
    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    if (!config) printf("For device: '%s'\n", devname);
    testForError(config, "Audio device is unavailable.");

    if (config)
    {
        aaxBuffer buffer = bufferFromFile(config, infile);
        float gain, pitch;

        if (buffer)
        {
            aaxEmitter emitter[256];
            int i, state;

            /** mixer */
            res = aaxMixerInit(config);
            testForState(res, "aaxMixerInit");

            gain = getGain(argc, argv);
            pitch = getPitch(argc, argv);
            num = getNumEmitters(argc, argv);
            printf("Starting %i emitters. gain = %f, pitch = %f\n",
                    num, gain, pitch);
            i = 0;
            do
            {
                /** emitters */
                emitter[i] = aaxEmitterCreate();
                testForError(emitter[i], "Unable to create a new emitter\n");

                res = aaxEmitterAddBuffer(emitter[i], buffer);
                testForState(res, "aaxEmitterAddBuffer");

                res = aaxEmitterSetMode(emitter[i], AAX_POSITION, AAX_RELATIVE);
                testForState(res, "aaxEmitterSetMode");

                res = aaxEmitterSetMode(emitter[i], AAX_LOOPING, AAX_FALSE);
                testForState(res, "aaxEmitterSetLooping");

                res = aaxEmitterSetPitch(emitter[i], pitch);
                testForState(res, "aaxEmitterSetPitch");

                res = aaxEmitterSetGain(emitter[i], gain);
                testForState(res, "aaxEmitterSetGain");

                res = aaxMixerRegisterEmitter(config, emitter[i]);
                testForState(res, "aaxMixerRegisterEmitter");

                /** schedule the emitter for playback */
                res = aaxEmitterSetState(emitter[i], AAX_PLAYING);
                testForState(res, "aaxEmitterStart");
            }
            while (++i < num);

            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            do
            {
                msecSleep(50);
                state = aaxEmitterGetState(emitter[0]);
            }
            while (state == AAX_PLAYING);

            i = 0;
            do
            {
                res = aaxEmitterSetState(emitter[i], AAX_STOPPED);
                testForState(res, "aaxEmitterStop");
            }
            while (++i < num);

            printf("emitter stopped\n");
            state = 0;
            do
            {
                msecSleep(50);
                res = aaxEmitterGetState(emitter[0]);
            }
            while ((res != AAX_PROCESSED) && (state++ < 50));

            i = 0;
            do
            {
                res = aaxMixerDeregisterEmitter(config, emitter[i]);
                testForState(res, "aaxMixerDeregisterEmitter");

                res = aaxEmitterDestroy(emitter[i]);
                testForState(res, "aaxEmitterDestroy");
            }
            while (++i < num);

            res = aaxBufferDestroy(buffer);
            testForState(res, "aaxBufferDestroy");

            res = aaxMixerSetState(config, AAX_STOPPED);
            testForState(res, "aaxMixerStop");
        }

    }

    res = aaxDriverClose(config);
    testForState(res, "aaxDriverClose");

    res = aaxDriverDestroy(config);
    testForState(res, "aaxDriverDestroy");

    return 0;
}
