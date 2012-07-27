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


#define ENABLE_EMITTER_FREQFILTER	1
#define ENABLE_STATIC_FREQFILTER	1
#define ENABLE_EMITTER_DISTORTION	1
#define ENABLE_EMITTER_PHASING		1
#define ENABLE_EMITTER_DYNAMIC_GAIN	0
#define FILE_PATH			SRC_PATH"/sine-440Hz.wav"

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
            aaxEmitter emitter;
            aaxEffect effect;
            aaxFilter filter;
            float dt = 0.0f;
            int q, state;
            float pitch;

            /** emitter */
            emitter = aaxEmitterCreate();
            testForError(emitter, "Unable to create a new emitter");

            pitch = getPitch(argc, argv);
            res = aaxEmitterSetPitch(emitter, pitch);
            testForState(res, "aaxEmitterSetPitch");

            res = aaxEmitterAddBuffer(emitter, buffer);
            testForState(res, "aaxEmitterAddBuffer");

            res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
            testForState(res, "aaxEmitterSetMode");


#if ENABLE_EMITTER_FREQFILTER
            /* frequency filter */
            filter = aaxFilterCreate(config, AAX_FREQUENCY_FILTER);
            testForError(filter, "aaxFilterCreate");

# if ENABLE_STATIC_FREQFILTER
            /* straight frequency filter */
            printf("Add frequency filter at 150Hz\n");
            filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR,
                                              200.0f, 1.0f, 0.5f, 2.0f);
            testForError(filter, "aaxFilterSetSlot");
            filter = aaxFilterSetState(filter, AAX_TRUE);
            testForError(filter, "aaxFilterSetState");
# else
            /* envelope following dynamic frequency filter (auto-wah) */
            printf("Add auto-wah\n");
            filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR,
                                              100.0f, 0.5f, 1.0f, 8.0f);
            testForError(filter, "aaxFilterSetSlot 0");

            filter = aaxFilterSetSlot(filter, 1, AAX_LINEAR,
                                              550.0f, 0.0f, 0.0f, 1.0f);
            testForError(filter, "aaxFilterSetSlot 1");
            filter = aaxFilterSetState(filter, AAX_INVERSE_ENVELOPE_FOLLOW);
            testForError(filter, "aaxFilterSetState");
# endif
            res = aaxEmitterSetFilter(emitter, filter);
            testForState(res, "aaxEmitterSetFilter");

            res = aaxFilterDestroy(filter);
            testForState(res, "aaxFilterDestroy");
#endif

#if ENABLE_EMITTER_DISTORTION
            /* distortion effect for emitter */
            effect = aaxEffectCreate(config, AAX_DISTORTION_EFFECT);
            testForError(effect, "aaxEffectCreate");

            effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR,
                                              0.8f, 0.0f, 1.0f, 0.5f);
            testForError(effect, "aaxEffectSetSlot 0");

            effect = aaxEffectSetState(effect, AAX_TRUE);
            testForError(effect, "aaxEffectSetState");

            res = aaxEmitterSetEffect(emitter, effect);
            testForState(res, "aaxEmitterSetEffect");

            res = aaxEffectDestroy(effect);
            testForState(res, "aaxEffectDestroy");
#endif

            /** mixer */
            res = aaxMixerInit(config);
            testForState(res, "aaxMixerInit");

            res = aaxMixerRegisterEmitter(config, emitter);
            testForState(res, "aaxMixerRegisterEmitter");

# if ENABLE_EMITTER_PHASING
            /* phasing effect */
            printf("source phasing..\n");
            effect = aaxEmitterGetEffect(emitter, AAX_PHASING_EFFECT);
            effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR,
                                              0.8f, 0.0f, 0.0f, 0.095f);
            effect = aaxEffectSetState(effect, AAX_TRIANGLE_WAVE);
            res = aaxEmitterSetEffect(emitter, effect);
            res = aaxEffectDestroy(effect);
            testForError(effect, "aaxEffectCreate");
#endif

#if ENABLE_EMITTER_DYNAMIC_GAIN
            /* dynamic gain filter for emitter (compressor) */
            filter = aaxFilterCreate(config, AAX_DYNAMIC_GAIN_FILTER);
            testForError(filter, "aaxFilterCreate");

            filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR,
                                              0.0f, 0.2f, 2.0f, 0.5f);
            testForError(filter, "aaxFilterSetSlot");

            filter = aaxFilterSetState(filter, AAX_ENVELOPE_FOLLOW);
            testForError(filter, "aaxFilterSetState");

            res = aaxEmitterSetFilter(emitter, filter);
            testForState(res, "aaxEmitterSetFilter");

            res = aaxFilterDestroy(filter);
            testForState(res, "aaxFilterDestroy");
#endif

            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /** schedule the emitter for playback */
            res = aaxEmitterSetState(emitter, AAX_PLAYING);
            testForState(res, "aaxEmitterStart");

            q = 0;
            printf("playing distorted\n");
            do
            {
                msecSleep(50);
                dt += 0.05f;

                if (++q > 10)
                {
                    unsigned long offs, offs_bytes;
                    float off_s;
                    q = 0;

                    off_s = aaxEmitterGetOffsetSec(emitter);
                    offs = aaxEmitterGetOffset(emitter, AAX_SAMPLES);
                    offs_bytes = aaxEmitterGetOffset(emitter, AAX_BYTES);
                    printf("playing time: %5.2f, buffer position: %5.2f "
                           "(%li samples/ %li bytes)\n", dt, off_s,
                           offs, offs_bytes);
                }
                state = aaxEmitterGetState(emitter);
            }
            while ((dt < 15.0f) && (state == AAX_PLAYING));

            res = aaxEmitterSetState(emitter, AAX_STOPPED);
            testForState(res, "aaxEmitterStop");

            do
            {
                msecSleep(50);
                state = aaxEmitterGetState(emitter);
            }
            while (state == AAX_PLAYING);

            res = aaxMixerDeregisterEmitter(config, emitter);
            res = aaxMixerSetState(config, AAX_STOPPED);
            res = aaxEmitterDestroy(emitter);
            res = aaxBufferDestroy(buffer);
        }
    }

    res = aaxDriverClose(config);
    res = aaxDriverDestroy(config);


    return 0;
}
