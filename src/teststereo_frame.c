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

#define FILE_PATH		SRC_PATH"/stereo.wav"

int main(int argc, char **argv)
{
    char *devname, *infile;
    aaxConfig config;
    int res;

    infile = getInputFile(argc, argv, FILE_PATH);
    devname = getDeviceName(argc, argv);

    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    testForError(config, "No default audio device available.");

    do {
        aaxBuffer buffer = bufferFromFile(config, infile);
        if (buffer)
        {
            aaxEmitter emitter;
            aaxFrame frame;
            aaxFilter f;
            float dt = 0.0f;
            int q, state;
            float pitch;

            /** mixer */
            res = aaxMixerInit(config);
            testForState(res, "aaxMixerInit");

            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /** audio frame */
            frame = aaxAudioFrameCreate(config);
            testForError(frame, "Unable to create a new audio frame\n");

            pitch = getPitch(argc, argv);
            res = aaxAudioFrameSetPitch(frame, pitch);
            testForState(res, "aaxAudioFrameSetPitch");

            /** register audio frame */
            res = aaxMixerRegisterAudioFrame(config, frame);
            testForState(res, "aaxMixerRegisterAudioFrame");

#if 0
            /* equalizer */
            f = aaxFilterCreate(config, AAX_EQUALIZER);
            testForError(f, "aaxFilterCreate");

            f = aaxFilterSetSlot(f, 0, AAX_LINEAR,  500.0, 1.0, 0.1, 0.0);
            testForError(f, "aaxFilterSetSlot/0");

            f = aaxFilterSetSlot(f, 1, AAX_LINEAR, 8000.0, 0.1, 0.5, 0.0);
            testForError(f, "aaxFilterSetSlot/1");

            f = aaxFilterSetState(f, AAX_TRUE);
            testForError(f, "aaxFilterSetState");

            res = aaxAudioFrameSetFilter(frame, f);
            testForState(res, "aaxAudioFrameSetFilter");

            res = aaxFilterDestroy(f);
            testForState(res, "aaxFilterDestroy");
#endif

            /** schedule the audioframe for playback */
            res = aaxAudioFrameSetState(frame, AAX_PLAYING);
            testForState(res, "aaxAudioFrameStart");

            /** emitter */
            emitter = aaxEmitterCreate();
            testForError(emitter, "Unable to create a new emitter");

            res = aaxEmitterAddBuffer(emitter, buffer);
            testForState(res, "aaxEmitterAddBuffer");

            /** register emitter */
            res = aaxAudioFrameRegisterEmitter(frame, emitter);
            testForState(res, "aaxMixerRegisterEmitter");

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

            res = aaxAudioFrameDeregisterEmitter(frame, emitter);
            res = aaxAudioFrameSetState(frame, AAX_STOPPED);
            res = aaxMixerDeregisterAudioFrame(config, frame);
            res = aaxMixerSetState(config, AAX_STOPPED);
            res = aaxEmitterDestroy(emitter);
            res = aaxBufferDestroy(buffer);
            res = aaxAudioFrameDestroy(frame);
        }
    }
    while (0);

    res = aaxDriverClose(config);
    res = aaxDriverDestroy(config);


    return 0;
}
