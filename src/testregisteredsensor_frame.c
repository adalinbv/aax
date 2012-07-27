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

#define REFRESH_RATE		  50.0f
#define RECORD_TIME_SEC		60000.0f

int main(int argc, char **argv)
{
    aaxConfig config, record;
    char *devname;
    int res;

    devname = getDeviceName(argc, argv);
    if (!devname) {
        devname = "AeonWave on File: /tmp/AeonWaveOut.wav";
    }
    printf("Playback: %s\n", devname);
    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    testForError(config, "No default audio device available.");

    devname = getCaptureName(argc, argv);
    printf("Recording: %s\n", devname);
    record = aaxDriverOpenByName(devname, AAX_MODE_READ);
    testForError(record, "Capture device is unavailable.");

    if (config && record)
    {
        enum aaxFormat format;
        aaxFrame frame;
        int channels;
        float f;

        format = AAX_PCM16S;
        channels = 2;

        /** mixer */
#if 0
        res = aaxMixerSetSetup(config, AAX_FREQUENCY, 64000);
        testForState(res, "aaxMixerSeFrequency");
#endif

        res = aaxMixerSetSetup(config, AAX_TRACKS, channels);
        testForState(res, "aaxMixerSetNoTracks");

        res = aaxMixerSetState(config, AAX_INITIALIZED);
        testForState(res, "aaxMixerInit");

        res = aaxMixerSetState(config, AAX_PLAYING);
        testForState(res, "aaxMixerStart");

        /** audio frame */
        frame = aaxAudioFrameCreate(config);
        testForError(frame, "Unable to create a new audio frame\n");

        /** register audio frame */
        res = aaxMixerRegisterAudioFrame(config, frame);
        testForState(res, "aaxMixerRegisterAudioFrame");

        res = aaxAudioFrameSetState(frame, AAX_PLAYING);
        testForState(res, "aaxAudioFrameSetState");


        /** registered sensor */
        printf("Capturing %5.1f seconds of audio\n", RECORD_TIME_SEC);
 
        res = aaxMixerSetSetup(record, AAX_TRACKS, channels);
        testForState(res, "aaxMixerSetNoTracks");

        res = aaxMixerSetSetup(record, AAX_FORMAT, format);
        testForState(res, "aaxMixerSetFormat");

        res = aaxAudioFrameRegisterSensor(frame, record);
        testForState(res, "aaxAudioFrameRegisterSensor");

        /** must be called after aaxAudioFrameRegisterSensor */
        res = aaxMixerSetState(record, AAX_INITIALIZED);
        testForState(res, "aaxMixerSetInitialize");

        res = aaxSensorSetState(record, AAX_CAPTURING);
        testForState(res, "aaxSensorCaptureStart");

        f = 0.0f;
        do
        {
            msecSleep(750);
            f += 750*1e-9;
        }
        while (f < RECORD_TIME_SEC);
        printf("\n");

        res = aaxMixerSetState(config, AAX_STOPPED);
        testForState(res, "aaxMixerSetState");

        res = aaxAudioFrameSetState(frame, AAX_STOPPED);
        testForState(res, "aaxAudioFrameSetState");

        res = aaxSensorSetState(record, AAX_STOPPED);
        testForState(res, "aaxSensorCaptureStop");

        res = aaxAudioFrameDeregisterSensor(frame, record);
        testForState(res, "aaxAudioFrameDeregisterSensor");

        res = aaxDriverClose(record);
        testForState(res, "aaxDriverClose");

        res = aaxDriverDestroy(record);
        testForState(res, "aaxDriverDestroy");

        res = aaxMixerDeregisterAudioFrame(config, frame);
        testForState(res, "aaxMixerDeregisterAudioFrame");

        res = aaxAudioFrameDestroy(frame);
        testForState(res, "aaxAudioFrameDestroy");

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

