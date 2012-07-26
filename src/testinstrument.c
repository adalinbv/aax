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

#include <base/geometry.h>
#include <base/types.h>
#include "driver.h"
#include "wavfile.h"

#define SAMPLE_FREQUENCY	16000

int main(int argc, char **argv)
{
    aaxEmitter emitter;
    aaxBuffer buffer;
    aaxConfig config;
    aaxFilter filter;
    aaxEffect effect;
    char *devname;
    int state, res;
    float pitch;

    devname = getDeviceName(argc, argv);
    pitch = getPitch(argc, argv);

// aaxSynthCreateEmitterByName("/home/erik/.aaxsynth.xml", "piano");

    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    testForError(config, "No default audio device available.");

    buffer = aaxBufferCreate(config, (unsigned int)(0.3f*SAMPLE_FREQUENCY), 1, AAX_PCM16S);
    testForError(buffer, "Unable to generate buffer\n");

    res = aaxBufferSetFrequency(buffer, SAMPLE_FREQUENCY);
    testForState(res, "aaxBufferSetFrequency");

    res = aaxBufferSetWaveform(buffer, 220.0f*pitch, AAX_SAWTOOTH_WAVE);
    res = aaxBufferMixWaveform(buffer, 880.0f*pitch, AAX_SINE_WAVE, 0.2f);
    testForState(res, "aaxBufferProcessWaveform");

    /** emitter */
    emitter = aaxEmitterCreate();
    testForError(emitter, "Unable to create a new emitter\n");

    res = aaxEmitterAddBuffer(emitter, buffer);
    testForState(res, "aaxEmitterAddBuffer");

    res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
    testForState(res, "aaxEmitterSetLooping");

    /* time filter for emitter */
#if 1
	/* time filter for emitter */
    filter = aaxFilterCreate(config, AAX_TIMED_GAIN_FILTER);
    testForError(filter, "aaxFilterCreate");

    filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0f, 0.05f, 1.0f, 0.05f);
    testForError(filter, "aaxFilterSetSlot 0");
    filter = aaxFilterSetSlot(filter, 1, AAX_LINEAR, 0.9f, 1.0f, 0.8f, 0.2f);
    testForError(filter, "aaxFilterSetSlot 1");
#if 0
    filter = aaxFilterSetSlot(filter, 2, AAX_LINEAR, 0.0, 0.0, 0.0, 0.0);
    testForError(filter, "aaxFilterSetSlot 2");
#endif

    filter = aaxFilterSetState(filter, AAX_TRUE);
    testForError(filter, "aaxFilterSetState");

    res = aaxEmitterSetFilter(emitter, filter);
    testForState(res, "aaxEmitterSetFilter");

    res = aaxFilterDestroy(filter);
    testForState(res, "aaxFilterDestroy");
#endif

#if 1
	/* time effect for emitter */
    effect = aaxEffectCreate(config, AAX_TIMED_PITCH_EFFECT);
    testForError(effect, "aaxFilterCreate");

    effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.995f, 0.05f, 1.05f, 0.08f);
    testForError(effect, "aaxFilterSetSlot 0");
    effect = aaxEffectSetSlot(effect, 1, AAX_LINEAR, 1.0f, 0.1f, 0.99f, 0.0f);
    testForError(filter, "aaxFilterSetSlot 1");
#if 0
    effect = aaxEffectSetSlot(effect, 2, AAX_LINEAR, 1.05, 0.0, 1.0, 0.0);
    testForError(filter, "aaxFilterSetSlot 2");
#endif

    effect = aaxEffectSetState(effect, AAX_TRUE);
    testForError(filter, "aaxFilterSetState");

    res = aaxEmitterSetEffect(emitter, effect);
    testForState(res, "aaxEmitterSetFilter");

    res = aaxEffectDestroy(effect);
    testForState(res, "aaxFilterDestroy");
#endif

#if 0
	/* tremolo filter for emitter */
    filter = aaxFilterCreate(config, AAX_TREMOLO_FILTER);
    testForError(filter, "aaxFilterCreate");

    filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0, 4.0, 0.1, 0.0);
    testForError(filter, "aaxFilterSetSlot");

    filter = aaxFilterSetState(filter, AAX_TRIANGLE_WAVE);
    testForError(filter, "aaxFilterSetState");

    res = aaxEmitterSetFilter(emitter, filter);
    testForState(res, "aaxEmitterSetFilter");

    res = aaxFilterDestroy(filter);
    testForState(res, "aaxFilterDestroy");
#endif

#if 0
	/* vibrato effect for emitter */
    effect = aaxEffectCreate(config, AAX_VIBRATO_EFFECT);
    testForError(filter, "aaxEffectCreate");

    effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.0, 4.0, 0.08, 0.0);
    testForError(filter, "aaxEffectSetSlot");

    effect = aaxEffectSetState(effect, AAX_SINE_WAVE);
    testForError(filter, "aaxEffectSetState");

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

    res = aaxMixerSetState(config, AAX_PLAYING);
    testForState(res, "aaxMixerStart");

    /* tremolo filter for mixer */
#if 0
    filter = aaxFilterCreate(config, AAX_TREMOLO_FILTER);
    testForError(filter, "aaxFilterCreate");

    filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0, 0.9, 0.2, 0.0);
    testForError(filter, "aaxFilterSetSlot");

    filter = aaxFilterSetState(filter, AAX_TRIANGLE_WAVE);
    testForError(filter, "aaxFilterSetState");

    res = aaxMixerSetFilter(config, filter);
    testForState(res, "aaxMixerSetFilter");

    res = aaxFilterDestroy(filter);
    testForState(res, "aaxFilterDestroy");
#endif

    /** schedule the emitter for playback */
    res = aaxEmitterSetState(emitter, AAX_PLAYING);
    testForState(res, "aaxEmitterStart");

    do
    {
        static int i = 0;

        nanoSleep(5e7);
        if (++i == 50) break;
        state = aaxEmitterGetState(emitter);

        if (i == 10) 
        {
            res = aaxEmitterStop(emitter);
            res = aaxEmitterRewind(emitter);
            res = aaxEmitterSetPitch(emitter, 0.87f);
            res = aaxEmitterStart(emitter);
        }
    }
    while (state == AAX_PLAYING);

    res = aaxEmitterSetState(emitter, AAX_STOPPED);
    do
    {
        nanoSleep(5e7);
        state = aaxEmitterGetState(emitter);
    }
    while (state == AAX_PLAYING);


    res = aaxEmitterStop(emitter);
    res = aaxEmitterRemoveBuffer(emitter);
    testForState(res, "aaxEmitterRemoveBuffer");

    res = aaxBufferDestroy(buffer);
    testForState(res, "aaxBufferDestroy");

    res = aaxMixerDeregisterEmitter(config, emitter);
    res = aaxMixerSetState(config, AAX_STOPPED);
    res = aaxEmitterDestroy(emitter);

    res = aaxDriverClose(config);
    res = aaxDriverDestroy(config);


    return 0;
}
