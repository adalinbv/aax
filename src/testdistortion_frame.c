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

#define FILE_PATH		SRC_PATH"/sine-440Hz.wav"

int main(int argc, char **argv)
{
    char *devname, *infile;
    aaxConfig config;
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
            aaxEmitter emitter;
            aaxEffect effect;
            aaxFilter filter;
            aaxFrame frame;
            float dt = 0.0f;
            int q, state;
            float pitch;

            /** mixer */
            res = aaxMixerInit(config);
            testForState(res, "aaxMixerInit");

            /** AudioFrame */
            frame = aaxAudioFrameCreate(config);

            /* register the audio-frame at the mixer */
            res = aaxMixerRegisterAudioFrame(config, frame);
            testForState(res, "aaxMixerRegisterAudioFrame");

            /* schedule the mixer for playback */
            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /* schedule the audio-frame for playback */
            res = aaxAudioFrameSetState(frame, AAX_PLAYING);
            testForState(res, "aaxAudioFrameStart");


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

            /* schedule the emitter for playback */
            res = aaxEmitterSetState(emitter, AAX_PLAYING);
            testForState(res, "aaxEmitterStart");

            /* register the emitter at the audio-frame */
            res = aaxAudioFrameRegisterEmitter(frame, emitter);
            testForState(res, "aaxAudioFrameRegisterEmitter");

#if 1
            /* emitter phasing */
            printf("emitter phasing\n");
            effect = aaxEmitterGetEffect(emitter, AAX_PHASING_EFFECT);
            testForError(effect, "aaxEmitterGetEffect");
            effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.8, 0.0, 0.0, 0.046);
            testForError(effect, "aaxEffectSetSlot");
            effect = aaxEffectSetState(effect, AAX_TRUE);
            testForError(effect, "aaxEffectSetState");
            res = aaxEmitterSetEffect(emitter, effect);
            testForState(res, "aaxEmitterSetEffect");
            res = aaxEffectDestroy(effect);
            testForState(res, "aaxEffectDestroy");
#endif


#if 0
            /* for testing purpose only!!! */
            printf("emitter distortion\n");
            effect = aaxEffectCreate(config, AAX_DISTORTION_EFFECT);
            testForError(effect, "aaxEffectCreate");

            effect  = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.8, 0.0, 1.0, 0.5);
            testForError(effect, "aaxEffectSetSlot 0");
            effect = aaxEffectSetState(effect, AAX_TRUE);
            testForError(effect, "aaxEffectSetState");
            res = aaxEmitterSetEffect(emitter, effect);
            testForState(res, "aaxEmitterSetEffect");

            res = aaxEffectDestroy(effect);
            testForState(res, "aaxEffectDestroy");
#endif

#if 1
            /* audio-frame frequency filter */
            filter = aaxFilterCreate(config, AAX_FREQUENCY_FILTER);
            testForError(filter, "aaxFilterCreate");
# if 0
	 /* straight frequency filter */
            printf("frequency filter at 200Hz\n");
            filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 200.0, 1.0, 0.5, 2.0);
            testForError(filter, "aaxFilterSetSlot");
            filter = aaxFilterSetState(filter, AAX_TRUE);
            testForError(filter, "aaxFilterSetState");
# else
            /* envelope following dynamic frequency filter (auto-wah) */
            printf("auto-wah effect\n");
            filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 100.0, 0.5, 1.0, 8.0);
            testForError(filter, "aaxFilterSetSlot 0");

            filter = aaxFilterSetSlot(filter, 1, AAX_LINEAR, 550.0, 0.0, 0.0, 1.0);
            testForError(filter, "aaxFilterSetSlot 1");
            filter = aaxFilterSetState(filter, AAX_INVERSE_ENVELOPE_FOLLOW);
            testForError(filter, "aaxFilterSetState");
# endif
            res = aaxAudioFrameSetFilter(frame, filter);
            testForState(res, "aaxAudioFrameSetFilter");

            res = aaxFilterDestroy(filter);
            testForState(res, "aaxFilterDestroy");
#endif

#if 1
            /* audio-frame distortion effect */
            printf("audio-frame distortion\n");
            effect = aaxEffectCreate(config, AAX_DISTORTION_EFFECT);
            testForError(effect, "aaxEffectCreate");

            effect  = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.8, 0.0, 1.0, 0.5);
            testForError(effect, "aaxEffectSetSlot 0");
            effect = aaxEffectSetState(effect, AAX_TRUE);
            testForError(effect, "aaxEffectSetState");
            res = aaxAudioFrameSetEffect(frame, effect);
            testForState(res, "aaxAudioFrameSetEffect");

            res = aaxEffectDestroy(effect);
            testForState(res, "aaxEffectDestroy");
#endif

# if 0
            /* audio-frame delay effect */
            printf("audio-frame delay effect\n");
            effect = aaxAudioFrameGetEffect(frame, AAX_CHORUS_EFFECT);
            effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.5, 0.1, 0.08, 0.15);
            effect = aaxEffectSetState(effect, AAX_INVERSE_ENVELOPE_FOLLOW);
            res = aaxAudioFrameSetEffect(frame, effect);
            res = aaxEffectDestroy(effect);
            testForError(effect, "aaxEffectCreate");
#endif

#if 0
            /* dynamic pitch effect for the audio-frame*/
            effect = aaxEffectCreate(config, AAX_DYNAMIC_PITCH_EFFECT);
            testForError(filter, "aaxEffectCreate");

            effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.0, 0.0, 1.0, 0.0);
            testForError(filter, "aaxEffectSetSlot");

            effect = aaxEffectSetState(effect, AAX_ENVELOPE_FOLLOW);
            testForError(effect, "aaxEffectSetState");

            res = aaxAudioFrameSetEffect(frame, effect);
            testForState(res, "aaxAudioFrameSetEffect");

            res = aaxEffectDestroy(effect);
            testForState(res, "aaxEffectDestroy");
#endif

#if 0
            /* dynamic gain filter for audio-frame (compressor) */
            filter = aaxFilterCreate(config, AAX_DYNAMIC_GAIN_FILTER);
            testForError(filter, "aaxFilterCreate");

            filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0, 1.0, 0.8, 0.2);
            testForError(filter, "aaxFilterSetSlot");

            filter = aaxFilterSetState(filter, AAX_ENVELOPE_FOLLOW);
            testForError(filter, "aaxFilterSetState");

            res = aaxAudioFrameSetFilter(frame, filter);
            testForState(res, "aaxAudioFrameSetFilter");

            res = aaxFilterDestroy(filter);
            testForState(res, "aaxFilterDestroy");
#endif


            q = 0;
            do
            {
                nanoSleep(5e7);
                dt += 5e7*1e-9;
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
            while ((dt < 15.0f) && (state == AAX_PLAYING));

            res = aaxEmitterSetState(emitter, AAX_STOPPED);
            testForState(res, "aaxEmitterStop");

            do
            {
                nanoSleep(5e7);
                state = aaxEmitterGetState(emitter);
            }
            while (state == AAX_PLAYING);

            res = aaxAudioFrameSetState(frame, AAX_STOPPED);
            res = aaxAudioFrameDeregisterEmitter(frame, emitter);
            res = aaxMixerDeregisterAudioFrame(config, frame);
            res = aaxMixerSetState(config, AAX_STOPPED);
            res = aaxAudioFrameDestroy(frame);
            res = aaxEmitterDestroy(emitter);
            res = aaxBufferDestroy(buffer);
        }
    }
    while (0);

    res = aaxDriverClose(config);
    res = aaxDriverDestroy(config);


    return 0;
}
