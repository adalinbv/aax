/*
 * Copyright (C) 2008-2014 by Erik Hofman.
 * Copyright (C) 2009-2014 by Adalin B.V.
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

#define ENABLE_EMITTER_FREQFILTER	1
#define ENABLE_SCENERY_FREQFILTER	1
#define FILE_PATH			SRC_PATH"/wasp.wav"
#define	FEMITTER			 400.0f
#define FSCENE				4000.0f
#define DEG				(360.0f/5)

int main(int argc, char **argv)
{
    char *devname, *infile;
    aaxConfig config;
    int res;

    devname = getDeviceName(argc, argv);
    infile = getInputFile(argc, argv, FILE_PATH);
    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    testForError(config, "No default audio device available.");

    if (config)
    {
        aaxBuffer buffer = bufferFromFile(config, infile);
        if (buffer)
        {
            aaxFilter fscene, femitter;
            aaxEmitter emitter;
            float pitch;
            int deg = 0;

            /** mixer */
            res = aaxMixerInit(config);
            testForState(res, "aaxMixerInit");

            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

#if ENABLE_SCENERY_FREQFILTER
            /* frequency filter */
            fscene = aaxFilterCreate(config, AAX_FREQUENCY_FILTER);
            fscene = aaxFilterSetSlot(fscene, 0, AAX_LINEAR,
                                              FSCENE, 0.0f, 1.0f, 1.0f);
            fscene = aaxFilterSetState(fscene, AAX_FALSE);
            res = aaxScenerySetFilter(config, fscene);
            res = aaxFilterDestroy(fscene);
#endif

            /** emitter */
            pitch = getPitch(argc, argv);
            emitter = aaxEmitterCreate();
            testForError(emitter, "Unable to create a new emitter\n");

            res = aaxEmitterAddBuffer(emitter, buffer);
            testForState(res, "aaxEmitterAddBuffer");

            res = aaxEmitterSetMode(emitter, AAX_POSITION, AAX_RELATIVE);
            testForState(res, "aaxEmitterSetMode");

            res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
            testForState(res, "aaxEmitterSetLooping");

            res = aaxEmitterSetPitch(emitter, pitch);
            testForState(res, "aaxEmitterSetPitch");

#if ENABLE_EMITTER_FREQFILTER
            /* frequency filter */
            femitter = aaxFilterCreate(config, AAX_FREQUENCY_FILTER);
            femitter = aaxFilterSetSlot(femitter, 0, AAX_LINEAR,
                                                  400.0f, 1.0f, 0.0f, 1.0f);
            femitter = aaxFilterSetState(femitter, AAX_FALSE);
            res = aaxEmitterSetFilter(emitter, femitter);
            res = aaxFilterDestroy(femitter);
            testForError(femitter, "aaxFilterCreate");
#endif

            res = aaxMixerRegisterEmitter(config, emitter);
            testForState(res, "aaxMixerRegisterEmitter");

            /** schedule the emitter for playback */
            res = aaxEmitterSetState(emitter, AAX_PLAYING);
            testForState(res, "aaxEmitterStart");

            printf("No filtering\n");
            deg = 0;
            while(deg < 2*360)
            {
                msecSleep(50);

                deg += 3;
                if ((deg > DEG) && (deg < (DEG+4)))
                {
                    printf("scenery highpass filter at %3.1f Hz\n", FSCENE);
                    fscene = aaxSceneryGetFilter(config, AAX_FREQUENCY_FILTER);
                    fscene = aaxFilterSetState(fscene, AAX_TRUE);
                    res = aaxScenerySetFilter(config, fscene);
                    testForState(res, "aaxScenerySetFilter\n");
                }
                else if ((deg > 2*DEG) && (deg < (2*DEG+4)))
                {
                    printf("add emitter lowpass at %3.1f Hz\n", FEMITTER);
                    printf("\tband stop filter between %3.1f Hz and %3.1f Hz\n",
                            FEMITTER, FSCENE);
                    femitter=aaxEmitterGetFilter(emitter, AAX_FREQUENCY_FILTER);
                    femitter = aaxFilterSetState(femitter, AAX_TRUE);
                    res = aaxEmitterSetFilter(emitter, femitter);
                }
                else if ((deg > 3*DEG) && (deg < (3*DEG+4)))
                {
                    printf("disable scenery highpass filter\n");
                    printf("\temitter lowpass filter at %3.1f Hz\n", FEMITTER);
                    fscene = aaxFilterSetState(fscene, AAX_FALSE);
                    res = aaxScenerySetFilter(config, fscene);
                }
                else if ((deg > 4*DEG) && (deg < (4*DEG+4)))
                {
                    printf("envelope following filtering (auto wah)\n");
                    femitter=aaxEmitterGetFilter(emitter, AAX_FREQUENCY_FILTER);
                    femitter = aaxFilterSetSlot(femitter, 0, AAX_LINEAR,
                                              300.0f, 0.4f, 1.0f, 12.0f);
                    femitter = aaxFilterSetSlot(femitter, 1, AAX_LINEAR,
                                              600.0f, 0.0f, 0.0f, 0.2f);
                    femitter = aaxFilterSetState(femitter, AAX_ENVELOPE_FOLLOW);
                    res = aaxEmitterSetFilter(emitter, femitter);
                }
            }

            res = aaxEmitterSetState(emitter, AAX_STOPPED);
            testForState(res, "aaxEmitterStop");

            res = aaxMixerDeregisterEmitter(config, emitter);
            testForState(res, "aaxMixerDeregisterEmitter");

            res = aaxEmitterDestroy(emitter);
            testForState(res, "aaxEmitterDestroy");

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
