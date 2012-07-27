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
    char *device, *devname, *infile;
    aaxConfig config, record;
    aaxBuffer buffer = 0;
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

            printf("\nPlayback stereo with equalizer enabled.\n");
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
            f = aaxFilterCreate(config, AAX_EQUALIZER);
            testForError(f, "aaxFilterCreate");

            f = aaxFilterSetSlot(f, 0, AAX_LINEAR,  500.0f, 1.0f, 0.1f, 0.0f);
            testForError(f, "aaxFilterSetSlot/0");

            f = aaxFilterSetSlot(f, 1, AAX_LINEAR, 8000.0f, 0.1f, 0.5f, 0.0f);
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
            {                msecSleep(50);
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

    device = "AeonWave Loopback";
    record = aaxDriverOpenByName(device, AAX_MODE_WRITE_STEREO);
    testForError(record, "No default audio device available.");
    do {
        if (buffer)
        {
            aaxEmitter emitter, emitter2;
            int q, state;
            float dt = 0.0f;
            aaxFilter f;

            printf("\nRecord using '%s' and playback the captured buffers.\n", device);
            /** emitter */
            emitter = aaxEmitterCreate();
            testForError(emitter, "Unable to create a new emitter\n");

            emitter2 = aaxEmitterCreate();
            testForError(emitter, "Unable to create a new emitter\n");

            res = aaxEmitterAddBuffer(emitter, buffer);
            testForState(res, "aaxEmitterAddBuffer");

            /** mixer */
            res = aaxMixerInit(record);
            testForState(res, "aaxMixerInit for recording");

            res = aaxMixerRegisterEmitter(record, emitter);
            testForState(res, "aaxMixerRegisterEmitter");

            /* equalizer */
            f = aaxFilterCreate(record, AAX_EQUALIZER);
            testForError(f, "aaxFilterCreate");

            f = aaxFilterSetSlot(f, 0, AAX_LINEAR,  500.0f, 1.0f, 0.1f, 0.0f);
            testForError(f, "aaxFilterSetSlot/0");

            f = aaxFilterSetSlot(f, 1, AAX_LINEAR, 8000.0f, 0.1f, 0.5f, 0.0f);
            testForError(f, "aaxFilterSetSlot/1");

            f = aaxFilterSetState(f, AAX_TRUE);
            testForError(f, "aaxFilterSetState");

            res = aaxMixerSetFilter(record, f);
            testForState(res, "aaxMixerSetFilter");

            res = aaxFilterDestroy(f);
            testForState(res, "aaxFilterDestroy");

            /** schedule the emitter for playback */
            res = aaxEmitterSetState(emitter, AAX_PLAYING);
            testForState(res, "aaxEmitterStart");

            printf("Starting recording using the '%s' device\n", device);
            res = aaxMixerSetState(record, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            res = aaxSensorSetState(record, AAX_CAPTURING);
            testForState(res, "aaxSensorSetState");

            q = 0;
            do
            {
                msecSleep(50);
                dt += 0.05f;
#if 1
                q++;
                if (q > 10)
                {
                    unsigned long offs;
                    float off_s;
                    q = 0;

                    off_s = aaxEmitterGetOffsetSec(emitter);
                    offs = aaxEmitterGetOffset(emitter, AAX_SAMPLES);

                    printf("playing time: %5.2f, buffer position: %5.2f (%li samples)\n", dt, off_s, offs);
                }
#endif
                state = aaxEmitterGetState(emitter);
            }
            while (state == AAX_PLAYING);

            res = aaxSensorSetState(record, AAX_STOPPED);
            testForState(res, "aaxSensorSetState");

            do
            {
                aaxBuffer buffer2 = aaxSensorGetBuffer(record);
                if (!buffer2) break;

                res = aaxEmitterAddBuffer(emitter2, buffer2);
                testForState(res, "aaxEmitterAddBuffer");

                res = aaxBufferDestroy(buffer2);
                testForState(res, "aaxBufferDestroy");
            }
            while (1);

            res = aaxMixerDeregisterEmitter(record, emitter);
            res = aaxMixerSetState(record, AAX_STOPPED);
            res = aaxEmitterDestroy(emitter);
            res = aaxBufferDestroy(buffer);

            res = aaxDriverClose(record);
            res = aaxDriverDestroy(record);

            config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
            testForError(config, "No default audio device available.");

            /** mixer */
            res = aaxMixerInit(config);
            testForState(res, "aaxMixerInit for playback");

            res = aaxMixerRegisterEmitter(config, emitter2);
            testForState(res, "aaxMixerRegisterEmitter");

            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /** schedule the emitter for playback */
            res = aaxEmitterSetState(emitter2, AAX_PLAYING);
            testForState(res, "aaxEmitterStart");

            printf("Starting streaming playback of the captured buffers.\n");
            dt = 0.0f;
            do
            {
                msecSleep(50);
                dt += 0.05f;
#if 1
                q++;
                if (q > 10)
                {
                    unsigned long offs;
                    float off_s;
                    q = 0;

                    off_s = aaxEmitterGetOffsetSec(emitter2);
                    offs = aaxEmitterGetOffset(emitter2, AAX_SAMPLES);

                    printf("playing time: %5.2f, buffer position: %5.2f (%li samples)\n", dt, off_s, offs);
                }
#endif
                state = aaxEmitterGetState(emitter2);
            }
            while (state == AAX_PLAYING);

            res = aaxMixerDeregisterEmitter(config, emitter2);
            res = aaxEmitterDestroy(emitter2);
            res = aaxMixerSetState(config, AAX_STOPPED);

            res = aaxDriverClose(config);
            res = aaxDriverDestroy(config);
        }
        while(0);
    } while(0);

    return 0;
}
