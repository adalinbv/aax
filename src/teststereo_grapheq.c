/*
 * Copyright (C) 2008-2012 by Erik Hofman.
 * Copyright (C) 2009-2012 by Adalin B.V.
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

#include <aax/defines.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define FILE_PATH		SRC_PATH"/stereo.wav"

int main(int argc, char **argv)
{
    char *devname, *infile;
    aaxBuffer buffer = 0;
    aaxConfig config;
    int res;

    infile = getInputFile(argc, argv, FILE_PATH);
    devname = getDeviceName(argc, argv);

    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    testForError(config, "No default audio device available.");
    do {
        buffer = bufferFromFile(config, infile);
        if (buffer)
        {
            aaxEmitter emitter;
            float dt = 0.0f;
            int q, state;
            float pitch;
            aaxFilter f;

            printf("\nPlayback stereo with 8-band graphic equalizer enabled.\n");
        /* cut-off frequencies */
//        printf("67Hz | 150Hz | 340Hz | 763Hz | 1.7fkHz | 3.9fkHz | 8.7fkHz\n\n");

            /* sqrt(fc_low*fc_high) */
            printf("44Hz | 100Hz | 220Hz | 500Hz | 1.1fkHz | 2.5fkHz | 5.7fkHz | 13kHz\n\n");
            /** emitter */
            emitter = aaxEmitterCreate();
            testForError(emitter, "Unable to create a new emitter");

            pitch = getPitch(argc, argv);
            res = aaxEmitterSetPitch(emitter, pitch);
            testForState(res, "aaxEmitterSetPitch");

            res = aaxEmitterAddBuffer(emitter, buffer);
            testForState(res, "aaxEmitterAddBuffer");

            /** mixer */
            res = aaxMixerInit(config);
            testForState(res, "aaxMixerInit");

            res = aaxMixerRegisterEmitter(config, emitter);
            testForState(res, "aaxMixerRegisterEmitter");

            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /* equalizer */
            f = aaxFilterCreate(config, AAX_GRAPHIC_EQUALIZER);
            testForError(f, "aaxFilterCreate");

            f = aaxFilterSetSlot(f, 0, AAX_LINEAR, 0.5f, 1.5f, 1.5f, 0.8f);
            testForError(f, "aaxFilterSetSlot/0");

            f = aaxFilterSetSlot(f, 1, AAX_LINEAR, 1.0f, 1.0f, 1.2f, 1.0f);
            testForError(f, "aaxFilterSetSlot/1");

            f = aaxFilterSetState(f, AAX_TRUE);
            testForError(f, "aaxFilterSetState");

            res = aaxMixerSetFilter(config, f);
            testForState(res, "aaxMixerSetFilter");

            res = aaxFilterDestroy(f);
            testForState(res, "aaxFilterDestroy");

            /** schedule the emitter for playback */
            res = aaxEmitterSetState(emitter, AAX_PLAYING);
            testForState(res, "aaxEmitterStart");

            q = 0;
            do
            {
                msecSleep(50);
                dt += 0.05f;
#if 1
                q++;
                if (q > 10)
                {
                    unsigned long offs, offs_bytes;
                    float off_s;
                    q = 0;

                    off_s = aaxEmitterGetOffsetSec(emitter);
                    offs = aaxEmitterGetOffset(emitter, AAX_SAMPLES);
                    offs_bytes = aaxEmitterGetOffset(emitter, AAX_BYTES);
                    printf("playing time: %5.2f, buffer position: %5.2f (%li samples/ %li bytes)\n", dt, off_s, offs, offs_bytes);
                }
#endif
                state = aaxEmitterGetState(emitter);
            }
            while (state == AAX_PLAYING);

            res = aaxMixerDeregisterEmitter(config, emitter);
            res = aaxMixerSetState(config, AAX_STOPPED);
            res = aaxEmitterDestroy(emitter);
        }
    }
    while (0);

    res = aaxDriverClose(config);
    res = aaxDriverDestroy(config);

    return 0;
}
