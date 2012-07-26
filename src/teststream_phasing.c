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
#include <math.h>

#include <aaxdefs.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define FILE_PATH                    SRC_PATH"/wasp.wav"
#define _DELAY			120
#define DELAY			\
    deg = 0; while(deg < _DELAY) { msecSleep(50); deg++; }

int main(int argc, char **argv)
{
    char *devname, *infile;
    aaxConfig config;
    float pitch;
    int res;

    infile = getInputFile(argc, argv, FILE_PATH);
    devname = getDeviceName(argc, argv);

    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    testForError(config, "No default audio device available.");

    if (config)
    {
        aaxBuffer buffer = bufferFromFile(config, infile);
        if (buffer)
        {
            const int NUM_BUFFERS = 32;
            aaxEmitter emitter;
            aaxEffect effect;
            aaxFilter filter;
            int i, deg = 0;

            /** mixer */
            res = aaxMixerInit(config);
            testForState(res, "aaxMixerInit");

            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /** emitter */
            emitter = aaxEmitterCreate();
            testForError(emitter, "Unable to create a new emitter\n");

            /** buffer */
            for (i=0; i<NUM_BUFFERS; i++)
            {
                res = aaxEmitterAddBuffer(emitter, buffer);
                testForState(res, "aaxEmitterAddBuffer");
            }

            res = aaxEmitterSetMode(emitter, AAX_POSITION, AAX_RELATIVE);
            testForState(res, "aaxEmitterSetMode");

            res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
            testForState(res, "aaxEmitterSetLooping");

            pitch = getPitch(argc, argv);
            res = aaxEmitterSetPitch(emitter, pitch);
            testForState(res, "aaxEmitterSetPitch");

            res = aaxEmitterSetGain(emitter, 0.7f);
            testForState(res, "aaxEmitterSetGain");

            res = aaxMixerRegisterEmitter(config, emitter);
            testForState(res, "aaxMixerRegisterEmitter");

            /** schedule the emitter for playback */
            res = aaxEmitterSetState(emitter, AAX_PLAYING);
            testForState(res, "aaxEmitterStart");

# if 1
            /* flanging effect */
            printf("source flanging.. (envelope following)\n");
            effect = aaxEmitterGetEffect(emitter, AAX_FLANGING_EFFECT);
            effect = aaxEffectSetSlot(effect,0,AAX_LINEAR, 0.7f, 1.0f, 0.2f, 0.0f);
            effect = aaxEffectSetState(effect, AAX_ENVELOPE_FOLLOW);
            res = aaxEmitterSetEffect(emitter, effect);
            res = aaxEffectDestroy(effect);
            testForError(effect, "aaxEffectCreate");

            DELAY;

            effect = aaxEmitterGetEffect(emitter, AAX_FLANGING_EFFECT);
            effect = aaxEffectSetState(effect, AAX_FALSE);
            res = aaxEmitterSetEffect(emitter, effect);
            res = aaxEffectDestroy(effect);
            testForError(effect, "aaxEffect Disable");
# endif

# if 1
            /* phasing effect */
            printf("source phasing.. (inverse envelope following)\n");
            effect = aaxEffectCreate(config, AAX_PHASING_EFFECT);
            effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 1.0f, 8.0f, 1.0f, 0.0f);
            effect = aaxEffectSetState(effect, AAX_ENVELOPE_FOLLOW);
            testForError(effect, "aaxEffectCreate");
            res = aaxEmitterSetEffect(emitter, effect);
            res = aaxEffectDestroy(effect);
            testForState(res, "aaxEmitterSetEffect");

            DELAY;
#else
            printf("no effect\n");
#endif


# if 1
            /* flanging effect */
            printf("source chorus.. (envelope following)\n");
            effect = aaxEmitterGetEffect(emitter, AAX_CHORUS_EFFECT);
            effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 1.0f, 0.8f, 1.0f, 0.0f);
            effect = aaxEffectSetState(effect, AAX_ENVELOPE_FOLLOW);
            res = aaxEmitterSetEffect(emitter, effect);
            res = aaxEffectDestroy(effect);
            testForError(effect, "aaxEffectCreate");

            DELAY;
# endif


#if 1
            for (i=0; i<2; i++)
            {
# if 1
                if (i == 1)
                {
                    /* frequency filter; 4000Hz lowpass */
                    printf("source frequency filter at 4000 Hz lowpass\n");
                    filter = aaxFilterCreate(config, AAX_FREQUENCY_FILTER);
                    filter=aaxFilterSetSlot(filter, 0, AAX_LINEAR, 400.0f, 1.0f, 0.0f, 0.0f);
                    filter = aaxFilterSetState(filter, AAX_TRUE);
                    res = aaxEmitterSetFilter(emitter, filter);
                    res = aaxFilterDestroy(filter);
                    testForError(filter, "aaxFilterCreate");
                }
# endif

# if 1
                /* flanging effect */
                printf("source flanging..\n");
                effect = aaxEmitterGetEffect(emitter, AAX_FLANGING_EFFECT);
                effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.88f, 0.08f, 1.0f, 0.0f);
                effect = aaxEffectSetState(effect, AAX_TRUE);
                res = aaxEmitterSetEffect(emitter, effect);
                res = aaxEffectDestroy(effect);
                testForError(effect, "aaxEffectCreate");

                DELAY;
# endif


# if 1

                /* phasing effect */
                printf("source phasing..\n");
                effect = aaxEffectCreate(config, AAX_PHASING_EFFECT);
                effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 1.0f, 0.08f, 1.0f, 0.0f);

                effect = aaxEffectSetState(effect, AAX_TRUE);
                testForError(effect, "aaxEffectCreate");
                res = aaxEmitterSetEffect(emitter, effect);
                res = aaxEffectDestroy(effect);
                testForState(res, "aaxEmitterSetEffect");

                DELAY;
#else
                printf("no effect\n");
#endif

# if 0
                /* chorus effect */
                printf("source chorus..\n");
                effect = aaxEmitterGetEffect(emitter, AAX_CHORUS_EFFECT);
                effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 1.0f, 0.08f, 1.0f, 0.0f);
                effect = aaxEffectSetState(effect, AAX_TRUE);
                res = aaxEmitterSetEffect(emitter, effect);
                res = aaxEffectDestroy(effect);
                testForError(effect, "aaxEffectCreate");

                DELAY;
# endif

# if 0
                /* flanging effect */
                printf("source flanging..\n");
                effect = aaxEmitterGetEffect(emitter, AAX_FLANGING_EFFECT);
                effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.88f, 0.08f, 1.0f, 0.0f);
                effect = aaxEffectSetState(effect, AAX_TRUE);
                res = aaxEmitterSetEffect(emitter, effect);
                res = aaxEffectDestroy(effect);
                testForError(effect, "aaxEffectCreate");

                DELAY;
# endif

            }

            /* disable delay effects */
            effect = aaxEmitterGetEffect(emitter, AAX_FLANGING_EFFECT);
            effect = aaxEffectSetState(effect, AAX_FALSE);
            res = aaxEmitterSetEffect(emitter, effect);
            res = aaxEffectDestroy(effect);
            testForError(effect, "aaxEffectCreate");

            /* disbale frequency filter */
            filter = aaxEmitterGetFilter(emitter, AAX_FREQUENCY_FILTER);
            filter = aaxFilterSetState(filter, AAX_FALSE);
            res = aaxEmitterSetFilter(emitter, filter);
            res = aaxFilterDestroy(filter);
            testForError(filter, "aaxFilterCreate");
#endif

#if 0
            /* phasing effect */
            printf("listener phasing..\n");
            effect = aaxEffectCreate(config, AAX_PHASING_EFFECT);
            effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 1.0f, 0.08f, 1.0f, 0.0f);
            effect = aaxEffectSetState(effect, AAX_TRUE);
            testForError(effect, "aaxEffectCreate");
            res = aaxMixerSetEffect(config, effect);
            res = aaxEffectDestroy(effect);
            testForState(res, "aaxMixerSetEffect");

            DELAY;
#endif

#if 0
            /* chorus effect */
            printf("listener chorus..\n");
            effect = aaxEffectCreate(config, AAX_CHORUS_EFFECT);
            effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 1.0f, 0.08f, 1.0f, 0.0f);
            effect = aaxEffectSetState(effect, AAX_TRUE);
            testForError(effect, "aaxEffectCreate");
            res = aaxMixerSetEffect(config, effect);
            res = aaxEffectDestroy(effect);
            testForState(res, "aaxMixerSetEffect");

            DELAY;
#endif

#if 0
            /* flanging effect */
            printf("listener flanging..\n");
            effect = aaxEffectCreate(config, AAX_FLANGING_EFFECT);
            effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.88f, 0.08f, 1.0f, 0.0f);
            effect = aaxEffectSetState(effect, AAX_TRUE);
            testForError(effect, "aaxEffectCreate");
            res = aaxMixerSetEffect(config, effect);
            res = aaxEffectDestroy(effect);
            testForState(res, "aaxMixerSetEffect");

            DELAY;
#endif

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
