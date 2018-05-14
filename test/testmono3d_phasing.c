/*
 * Copyright (C) 2008-2018 by Erik Hofman.
 * Copyright (C) 2009-2018 by Adalin B.V.
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

#include <aax/aax.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define ENABLE_EMITTER_FILTERS		1
#define ENABLE_EMITTER_FREQFILTER	1
#define ENABLE_EMITTER_DISTORTION	1
#define ENABLE_EMITTER_PHASING		1
#define ENABLE_EMITTER_CHORUS		1
#define ENABLE_EMITTER_FLANGING		1
#define ENABLE_MIXER_PHASING		1
#define ENABLE_MIXER_CHORUS		1
#define ENABLE_MIXER_FlANGING		1
#define FILE_PATH			SRC_PATH"/tictac.wav"
#define _DELAY				120
#define DELAY				\
    deg = 0; while(deg < _DELAY) { msecSleep(50); deg++; }

int main(int argc, char **argv)
{
    char *devname, *infile;
    aaxConfig config;
    float pitch;
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
            int i, deg = 0;

            /** mixer */
            res = aaxMixerSetState(config, AAX_INITIALIZED);
            testForState(res, "aaxMixerInit");

            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /** emitter */
            emitter = aaxEmitterCreate();
            testForError(emitter, "Unable to create a new emitter\n");

            /** buffer */
            res = aaxEmitterAddBuffer(emitter, buffer);

            res = aaxEmitterSetMode(emitter, AAX_POSITION, AAX_RELATIVE);
            testForState(res, "aaxEmitterSetMode");

            res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
            testForState(res, "aaxEmitterSetLooping");

            /* pitch */
            pitch = getPitch(argc, argv);
            effect = aaxEffectCreate(config, AAX_PITCH_EFFECT);
            testForError(effect, "Unable to create the pitch effect");

            res = aaxEffectSetParam(effect, AAX_PITCH, AAX_LINEAR, pitch);
            testForState(res, "aaxEffectSetParam");

            res = aaxEmitterSetEffect(emitter, effect);
            testForState(res, "aaxEmitterSetPitch");
            aaxEffectDestroy(effect);

            /* gain*/
            filter = aaxFilterCreate(config, AAX_VOLUME_FILTER);
            testForError(filter, "Unable to create the volume filter");

            res = aaxFilterSetParam(filter, AAX_GAIN, AAX_LINEAR, 0.7f);
            testForState(res, "aaxFilterSetParam");

            res = aaxEmitterSetFilter(emitter, filter);
            testForState(res, "aaxEmitterSetGain");
            aaxFilterDestroy(filter);

            res = aaxMixerRegisterEmitter(config, emitter);
            testForState(res, "aaxMixerRegisterEmitter");

            /** schedule the emitter for playback */
            res = aaxEmitterSetState(emitter, AAX_PLAYING);
            testForState(res, "aaxEmitterStart");

#if ENABLE_EMITTER_FLANGING
            /* flanging effect */
            printf("emitter flanging.. (envelope following)\n");
            effect = aaxEmitterGetEffect(emitter, AAX_FLANGING_EFFECT);
            res = aaxEffectSetSlot(effect,0,AAX_LINEAR, 0.7f, 1.0f, 0.2f, 0.0f);
            res = aaxEffectSetState(effect, AAX_ENVELOPE_FOLLOW);
            res = aaxEmitterSetEffect(emitter, effect);
            res = aaxEffectDestroy(effect);
            testForError(effect, "aaxEffectCreate");

            DELAY;

            effect = aaxEmitterGetEffect(emitter, AAX_FLANGING_EFFECT);
            res = aaxEffectSetState(effect, AAX_FALSE);
            res = aaxEmitterSetEffect(emitter, effect);
            res = aaxEffectDestroy(effect);
            testForError(effect, "aaxEffect Disable");
#endif

#if ENABLE_EMITTER_PHASING
            /* phasing effect */
            printf("emitter phasing.. (inverse envelope following)\n");
            effect = aaxEffectCreate(config, AAX_PHASING_EFFECT);
            res = aaxEffectSetSlot(effect, 0, AAX_LINEAR,
                                              1.0f, 8.0f, 1.0f, 0.0f);
            res = aaxEffectSetState(effect, AAX_ENVELOPE_FOLLOW);
            testForError(effect, "aaxEffectCreate");
            res = aaxEmitterSetEffect(emitter, effect);
            res = aaxEffectDestroy(effect);
            testForState(res, "aaxEmitterSetEffect");

            DELAY;
#else
            printf("no effect\n");
#endif


#if ENABLE_EMITTER_CHORUS
            /* flanging effect */
            printf("emitter chorus.. (envelope following)\n");
            effect = aaxEmitterGetEffect(emitter, AAX_CHORUS_EFFECT);
            testForError(effect, "aaxEmitterGetEffect");
            res = aaxEffectSetSlot(effect, 0, AAX_LINEAR,
                                              1.0f, 0.8f, 1.0f, 0.0f);
            testForError(effect, "aaxEffectSetSlot");
            res = aaxEffectSetState(effect, AAX_ENVELOPE_FOLLOW);
            testForError(effect, "aaxEffectSetState");
            res = aaxEmitterSetEffect(emitter, effect);
            testForState(res, "aaxEmitterSetEffect");
            res = aaxEffectDestroy(effect);
            testForState(res, "aaxEffectDestroy");

            DELAY;
# endif

#if ENABLE_EMITTER_FILTERS
            for (i=0; i<2; i++)
            {
# if ENABLE_EMITTER_FREQFILTER
                if (i == 1)
                {
                    /* frequency filter; 4000Hz lowpass */
                    printf("emitter frequency filter at 4000 Hz lowpass\n");
                    filter = aaxFilterCreate(config, AAX_FREQUENCY_FILTER);
                    res = aaxFilterSetSlot(filter, 0, AAX_LINEAR,
                                                      400.0f, 1.0f, 0.0f, 0.0f);
                    res = aaxFilterSetState(filter, AAX_TRUE);
                    res = aaxEmitterSetFilter(emitter, filter);
                    res = aaxFilterDestroy(filter);
                    testForError(filter, "aaxFilterCreate");
                }
# endif

# if ENABLE_EMITTER_FLANGING
                /* flanging effect */
                printf("emitter flanging..\n");
                effect = aaxEmitterGetEffect(emitter, AAX_FLANGING_EFFECT);
                testForError(effect, "aaxEmitterGetEffect");
                res = aaxEffectSetSlot(effect, 0, AAX_LINEAR,
                                                  0.88f, 0.08f, 1.0f, 0.0f);
                testForState(res, "aaxEffectSetSlot");
                res = aaxEffectSetState(effect, AAX_TRUE);
                testForState(res, "aaxEffectSetState");
                res = aaxEmitterSetEffect(emitter, effect);
                testForState(res, "aaxEmitterSetEffect");
                res = aaxEffectDestroy(effect);
                testForError(effect, "aaxEffectDestroy");

                DELAY;
# endif

# if ENABLE_EMITTER_PHASING
                /* phasing effect */
                printf("emitter phasing..\n");
#if 0
                effect = aaxEffectCreate(config, AAX_PHASING_EFFECT);
                testForError(effect, "aaxEffectCreate");
#else
                effect = aaxEmitterGetEffect(emitter, AAX_PHASING_EFFECT);
                testForError(effect, "aaxEmitterGetEffect");
#endif
                res = aaxEffectSetSlot(effect, 0, AAX_LINEAR,
                                                  1.0f, 0.08f, 1.0f, 0.0f);
                testForState(res, "aaxEffectSetSlot");
                res = aaxEffectSetState(effect, AAX_TRUE);
                testForState(res, "aaxEffectSetState");
                res = aaxEmitterSetEffect(emitter, effect);
                testForState(res, "aaxEmitterSetEffect");
                res = aaxEffectDestroy(effect);
                testForState(res, "aaxEffectDestroy");

                DELAY;
#else
                printf("no effect\n");
#endif

# if ENABLE_EMITTER_CHORUS
                /* chorus effect */
                printf("emitter chorus..\n");
                effect = aaxEmitterGetEffect(emitter, AAX_CHORUS_EFFECT);
                testForError(effect, "aaxEmitterGetEffect");
                res = aaxEffectSetSlot(effect, 0, AAX_LINEAR,
                                                  1.0f, 0.08f, 1.0f, 0.0f);
                testForState(res, "aaxEffectSetSlot");
                res = aaxEffectSetState(effect, AAX_TRUE);
                testForState(res, "aaxEffectSetState");
                res = aaxEmitterSetEffect(emitter, effect);
                testForState(res, "aaxEmitterSetEffect");
                res = aaxEffectDestroy(effect);
                testForState(res, "aaxEffectDestroy");

                DELAY;
# endif

# if ENABLE_EMITTER_FLANGING
                /* flanging effect */
                printf("emitter flanging..\n");
                effect = aaxEmitterGetEffect(emitter, AAX_FLANGING_EFFECT);
                testForError(effect, "aaxEmitterGetEffect");
                res = aaxEffectSetSlot(effect, 0, AAX_LINEAR,
                                                  0.88f, 0.08f, 1.0f, 0.0f);
                testForState(res, "aaxEffectSetSlot");
                res = aaxEffectSetState(effect, AAX_TRUE);
                testForState(res, "aaxEffectSetState");
                res = aaxEmitterSetEffect(emitter, effect);
                testForState(res, "aaxEmitterSetEffect");
                res = aaxEffectDestroy(effect);
                testForState(res, "aaxEffectDestroy");

                DELAY;
# endif
            }

            /* disable delay effects */
            effect = aaxEmitterGetEffect(emitter, AAX_FLANGING_EFFECT);
            res = aaxEffectSetState(effect, AAX_FALSE);
            res = aaxEmitterSetEffect(emitter, effect);
            res = aaxEffectDestroy(effect);
            testForError(effect, "aaxEffectCreate");

            /* disbale frequency filter */
            filter = aaxEmitterGetFilter(emitter, AAX_FREQUENCY_FILTER);
            res = aaxFilterSetState(filter, AAX_FALSE);
            res = aaxEmitterSetFilter(emitter, filter);
            res = aaxFilterDestroy(filter);
            testForError(filter, "aaxFilterCreate");
#endif

#if ENABLE_MIXER_PHASING
            /* phasing effect */
            printf("mixer phasing..\n");
            effect = aaxEffectCreate(config, AAX_PHASING_EFFECT);
            res = aaxEffectSetSlot(effect, 0, AAX_LINEAR,
                                              1.0f, 0.08f, 1.0f, 0.0f);
            res= aaxEffectSetState(effect, AAX_TRUE);
            testForError(effect, "aaxEffectCreate");
            res = aaxMixerSetEffect(config, effect);
            res = aaxEffectDestroy(effect);
            testForState(res, "aaxMixerSetEffect");

            DELAY;
#endif

#if ENABLE_MIXER_CHORUS
            /* chorus effect */
            printf("mixer chorus..\n");
            effect = aaxEffectCreate(config, AAX_CHORUS_EFFECT);
            res = aaxEffectSetSlot(effect, 0, AAX_LINEAR,
                                              1.0f, 0.08f, 1.0f, 0.0f);
            res = aaxEffectSetState(effect, AAX_TRUE);
            testForError(effect, "aaxEffectCreate");
            res = aaxMixerSetEffect(config, effect);
            res = aaxEffectDestroy(effect);
            testForState(res, "aaxMixerSetEffect");

            DELAY;
#endif

#if ENABLE_MIXER_FlANGING
            /* flanging effect */
            printf("mixer flanging..\n");
            effect = aaxEffectCreate(config, AAX_FLANGING_EFFECT);
            res = aaxEffectSetSlot(effect, 0, AAX_LINEAR,
                                              0.7f, 1.0f, 0.2f, 0.0f);
            res = aaxEffectSetState(effect, AAX_TRUE);
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

            res = aaxEmitterRemoveBuffer(emitter);
            testForState(res, "aaxEmitterRemoveBuffer");

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
