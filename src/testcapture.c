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

#define REFRESH_RATE		  50.0f
#define RECORD_TIME_SEC		  5.0f
#define PLAYBACK_DEVICE		"AeonWave on File: /tmp/AeonWaveOut.wav"

int main(int argc, char **argv)
{
    aaxConfig config, record;
    char *devname;
    int res;

    devname = getCaptureName(argc, argv);
    if (!devname) {
        devname = getDeviceName(argc, argv);
    }

    printf("Capture device: '%s'\n", devname);
    record = aaxDriverOpenByName(devname, AAX_MODE_READ);
    testForError(record, "Capture device is unavailable.");

    if (record)
    {
        enum aaxFormat format;
        aaxEmitter emitter;
        int channels;
        float f, freq;

        format = AAX_PCM16S;
        freq = 44100.0;
        channels = 2;

        printf("Capturing %5.1f seconds of audio\n", RECORD_TIME_SEC);
        res = aaxMixerSetSetup(record, AAX_FREQUENCY, (unsigned int)freq);
        testForState(res, "aaxMixerSeFrequency");
 
        res = aaxMixerSetSetup(record, AAX_TRACKS, channels);
        testForState(res, "aaxMixerSetNoTracks");

        res = aaxMixerSetSetup(record, AAX_FORMAT, format);
        testForState(res, "aaxMixerSetFormat");

        res = aaxMixerSetState(record, AAX_INITIALIZED);
        testForState(res, "aaxMixerSetInitialize");

        /** create the emitter */
        emitter = aaxEmitterCreate();
        testForError(emitter, "Unable to create a new emitter\n");

        res = aaxSensorSetState(record, AAX_CAPTURING);
        testForState(res, "aaxSensorCaptureStart");
        do
        {
            aaxBuffer buffer;
            unsigned long ul;

            res = aaxSensorWaitForBuffer(record, 3.0);
            testForState(res, "aaxSensorWaitForBuffer");

            ul = aaxSensorGetOffset(record, AAX_MICROSECONDS);
            f = (float)ul * 1e-6f;
#if 1
            printf("Record buffer position: %5.2f sec\r", f);
#endif

            buffer = aaxSensorGetBuffer(record);
            testForError(buffer, "aaxSensorGetBuffer");

            res = aaxEmitterAddBuffer(emitter, buffer);
            testForState(res, "aaxEmitterAddBuffer");

            res = aaxBufferDestroy(buffer);
            testForState(res, "aaxBufferDestroy");
        }
        while (f < RECORD_TIME_SEC);
        printf("\n");

        res = aaxSensorSetState(record, AAX_STOPPED);
        testForState(res, "aaxSensorCaptureStop");


        /** playback */
        res = aaxMixerSetState(record, AAX_STOPPED);
        res = aaxDriverClose(record);
        res = aaxDriverDestroy(record);


        devname = getDeviceName(argc, argv);
        if (!devname) {
            devname = PLAYBACK_DEVICE;
        }
        printf("Playback device: '%s'\n", devname);
        config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
        testForError(config, "No default audio device available.");

        /** mixer */
        res = aaxMixerSetNoTracks(config, 2);
        testForState(res, "aaxMixerSetNoTracks");

        res = aaxMixerInit(config);
        testForState(res, "aaxMixerInit");

        res = aaxMixerRegisterEmitter(config, emitter);
        testForState(res, "aaxMixerRegisterEmitter");

        res = aaxMixerSetState(config, AAX_PLAYING);
        testForState(res, "aaxMixerStart");

        /** schedule the emitter for playback */
        res = aaxEmitterSetState(emitter, AAX_PLAYING);
        testForState(res, "aaxEmitterStart");

        do
        {
            unsigned long offs;
            float off_s;

            off_s = aaxEmitterGetOffsetSec(emitter);
            offs = aaxEmitterGetOffset(emitter, AAX_SAMPLES);

            printf("buffer position: %5.2f (%li samples)\n", off_s, offs);
            res = aaxEmitterGetState(emitter);
            
            msecSleep(5e8);
        }
        while (res == AAX_PLAYING);

        res = aaxMixerDeregisterEmitter(config, emitter);
        testForState(res, "aaxMixerDeregisterEmitter");

        res = aaxMixerSetState(config, AAX_STOPPED);
        testForState(res, "aaxMixerStop");

        res = aaxEmitterDestroy(emitter);
        testForState(res, "aaxEmitterDestroy");

        res = aaxDriverClose(config);
        testForState(res, "aaxDriverClose");

        res = aaxDriverDestroy(config);
        testForState(res, "aaxDriverDestroy");
    }
    else {
        printf("Unable to open capture device.\n");
    }

    return 0;
}

