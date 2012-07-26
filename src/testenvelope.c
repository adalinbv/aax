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

#include <aaxdefs.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define SAMPLE_FREQUENCY	16000

int main(int argc, char **argv)
{
    aaxConfig config;
    char *devname;
    int res;

    devname = getDeviceName(argc, argv);
    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    testForError(config, "No default audio device available.");

    do {
        aaxEmitter emitter;
        aaxBuffer buffer;
        aaxEffect effect;
        aaxFilter filter;
        int q, state;
        float dt = 0.0f;
        float pitch;

        /** buffer */
        buffer = aaxBufferCreate(config, (unsigned int)(1.1f*SAMPLE_FREQUENCY), 1, AAX_PCM16S);
        testForError(buffer, "Unable to create a new buffer\n");

        res = aaxBufferSetFrequency(buffer, SAMPLE_FREQUENCY);
        testForState(res, "aaxBufferSetFrequency");

        res = aaxBufferMixWaveform(buffer, 660.0f, AAX_SINE_WAVE, 0.4f);
        testForState(res, "aaxBufferProcessWaveform");

        /** emitter */
        emitter = aaxEmitterCreate();
        testForError(emitter, "Unable to create a new emitter\n");

        res = aaxEmitterAddBuffer(emitter, buffer);
        testForState(res, "aaxEmitterAddBuffer");

        res = aaxEmitterSetMode(emitter, AAX_POSITION, AAX_ABSOLUTE);
        testForState(res, "aaxEmitterSetMode");

        res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
        testForState(res, "aaxEmitterSetLooping");

        pitch = getPitch(argc, argv);
        res = aaxEmitterSetPitch(emitter, pitch);
        testForState(res, "aaxEmitterSetPitch");

        /* time filter for emitter */
        filter = aaxFilterCreate(config, AAX_TIMED_GAIN_FILTER);
        testForError(filter, "aaxFilterCreate");

        filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0f, 0.07f, 10.0f, 0.01f);
        testForError(filter, "aaxFilterSetSlot 0");
        filter = aaxFilterSetSlot(filter, 1, AAX_LINEAR, 0.7f, AAX_FPINFINITE, 0.7f, 1.0f);
        testForError(filter, "aaxFilterSetSlot 1");

        filter = aaxFilterSetState(filter, AAX_TRUE);
        testForError(filter, "aaxFilterSetState");

        res = aaxEmitterSetFilter(emitter, filter);
        testForState(res, "aaxEmitterSetFilter");

        res = aaxFilterDestroy(filter);
        testForState(res, "aaxFilterDestroy");

        /* time effect for emitter */
        effect = aaxEffectCreate(config, AAX_TIMED_PITCH_EFFECT);
        testForError(effect, "aaxEffectCreate");

        effect  = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 1.0f, 0.2f, 4.0f, 0.2f);
        testForError(effect, "aaxEffectSetSlot 0");
        effect  = aaxEffectSetSlot(effect, 1, AAX_LINEAR, 0.5f, 0.2f, 1.0f, 0.0f);
        testForError(effect, "aaxEffectSetSlot 1");

        effect = aaxEffectSetState(effect, AAX_TRUE);
        testForError(effect, "aaxEffectSetState");

        res = aaxEmitterSetEffect(emitter, effect);
        testForState(res, "aaxEmitterSetEffect");

        res = aaxEffectDestroy(effect);
        testForState(res, "aaxEffectDestroy");

        /* tremolo filter for emitter */
        filter = aaxFilterCreate(config, AAX_DYNAMIC_GAIN_FILTER);
        testForError(filter, "aaxFilterCreate");

        filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0, 6.0, 1.0, 0.0);
        testForError(filter, "aaxFilterSetSlot");

        filter = aaxFilterSetState(filter, AAX_SINE_WAVE);
        testForError(filter, "aaxFilterSetState");

        res = aaxEmitterSetFilter(emitter, filter);
        testForState(res, "aaxEmitterSetFilter");

        res = aaxFilterDestroy(filter);
        testForState(res, "aaxFilterDestroy");

        /** mixer */
        res = aaxMixerInit(config);
        testForState(res, "aaxMixerInit");

        res = aaxMixerSetGain(config, 1.0f);
        testForState(res, "aaxMixerSetGain");

        res = aaxMixerRegisterEmitter(config, emitter);
        testForState(res, "aaxMixerRegisterEmitter");

        res = aaxMixerSetState(config, AAX_PLAYING);
        testForState(res, "aaxMixerStart");

        /** schedule the emitter for playback */
        res = aaxEmitterSetState(emitter, AAX_PLAYING);
        testForState(res, "aaxEmitterStart");

#if 1
        /* tremolo effect for mixer*/
        filter = aaxFilterCreate(config, AAX_DYNAMIC_GAIN_FILTER);
        testForError(effect, "aaxEffectCreate");

        filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0f, 0.9f, 1.0f, 0.0f);
        testForError(effect, "aaxEffectSetSlot");

        filter = aaxFilterSetState(filter, AAX_SINE_WAVE);
        testForError(effect, "aaxEffectSetState");

        res = aaxMixerSetFilter(config, filter);
        testForState(res, "aaxMixerSetEffect");

        res = aaxFilterDestroy(filter);
        testForState(res, "aaxEffectDestroy");
#endif

        /* vibrato effect for emitter */
#if 1
        effect = aaxEffectCreate(config, AAX_DYNAMIC_PITCH_EFFECT);
        testForError(effect, "aaxEffectCreate");

        effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.0, 4.0f, 0.4f, 0.0);
        testForError(effect, "aaxEffectSetSlot");

        effect = aaxEffectSetState(effect, AAX_TRIANGLE_WAVE);
        testForError(effect, "aaxEffectSetState");

        res = aaxEmitterSetEffect(emitter, effect);
        testForState(res, "aaxEmitterSetEffect");

        res = aaxEffectDestroy(effect);
        testForState(res, "aaxEffectDestroy");
#endif

        q = 0;
        do
        {
            msecSleep(50);
            dt += 0.05f;

            q++;
#if 1
            if (q == 25) 
            {
                printf("Enterning StandBy mode\n");
                aaxMixerSetState(config, AAX_STANDBY);
            }
            else if (q == 40)
            {
                printf("Enterning Playback mode again\n");
                aaxMixerSetState(config, AAX_PLAYING);
            }
#endif
            state = aaxEmitterGetState(emitter);
        }
        while (state == AAX_PLAYING && dt < 3.33);

        res = aaxEmitterSetState(emitter, AAX_STOPPED);
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

    } while(0);

    res = aaxDriverClose(config);
    res = aaxDriverDestroy(config);


    return 0;
}
