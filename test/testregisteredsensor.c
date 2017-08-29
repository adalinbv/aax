/*
 * Copyright (C) 2008-2016 by Erik Hofman.
 * Copyright (C) 2009-2016 by Adalin B.V.
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

#include <aax/aax.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define REFRESH_RATE		50.0f
#define RECORD_TIME_SEC		15.0f
#define MIXER_FREQUENCY		48000

int main(int argc, char **argv)
{
    aaxConfig config = NULL;
    aaxConfig record = NULL;
    int res, rv = 0;
    char *devname;

    devname = getDeviceName(argc, argv);
    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    printf("Playback: %s\n", aaxDriverGetSetup(config, AAX_RENDERER_STRING));
    testForError(config, "Audio output device  is not available.");

    if (!aaxIsValid(config, AAX_CONFIG_HD))
    {
        printf("Warning:\n");
        printf("  %s requires a registered version of AeonWave\n", argv[0]);
        printf("  Please visit http://www.adalin.com/buy_aeonwaveHD.html to ");
        printf("obtain\n  a product-key.\n\n");
        rv = -1;
    }
    else
    {
        devname = getCaptureName(argc, argv);
        record = aaxDriverOpenByName(devname, AAX_MODE_READ);
        printf("Recording: %s\n",aaxDriverGetSetup(record,AAX_RENDERER_STRING));
        testForError(record, "Audio capture device is not available.");
    }

    if (config && record && (rv >= 0))
    {
        enum aaxFormat format;
        int channels;
        float dt;

        format = AAX_PCM16S;
        channels = 2;

        /** mixer */
        res = aaxMixerSetSetup(config, AAX_FREQUENCY, MIXER_FREQUENCY);
        testForState(res, "aaxMixerSeFrequency");

        res = aaxMixerSetSetup(config, AAX_TRACKS, channels);
        testForState(res, "aaxMixerSetNoTracks");

        res = aaxMixerSetState(config, AAX_INITIALIZED);
        testForState(res, "aaxMixerInit");

        res = aaxMixerSetState(config, AAX_PLAYING);
        testForState(res, "aaxMixerStart");

        /** registered sensor */
        printf("Capturing %5.1f seconds of audio\n", RECORD_TIME_SEC);
 
        res = aaxMixerSetSetup(record, AAX_TRACKS, channels);
        testForState(res, "aaxMixerSetNoTracks");

        res = aaxMixerSetSetup(record, AAX_FORMAT, format);
        testForState(res, "aaxMixerSetFormat");

        res = aaxMixerRegisterSensor(config, record);
        testForState(res, "aaxMixerRegisterSensor");

        /** must be called after aaxMixerRegisterSensor */
        res = aaxMixerSetState(record, AAX_INITIALIZED);
        testForState(res, "aaxMixerSetInitialize");

        res = aaxSensorSetState(record, AAX_CAPTURING);
        testForState(res, "aaxSensorCaptureStart");

        dt = 0.0f;
        do
        {
            msecSleep(500);
            dt += 0.5f;
        }
        while (dt < RECORD_TIME_SEC);
        printf("\n");

        res = aaxMixerSetState(config, AAX_STOPPED);
        testForState(res, "aaxMixerSetState");

        res = aaxSensorSetState(record, AAX_STOPPED);
        testForState(res, "aaxSensorCaptureStop");

        res = aaxMixerDeregisterSensor(config, record);
        testForState(res, "aaxMixerDeregisterSensor");

        res = aaxDriverClose(record);
        testForState(res, "aaxDriverClose");

        res = aaxDriverDestroy(record);
        testForState(res, "aaxDriverDestroy");
    }
    else {
        printf("Unable to open capture device.\n");
    }

    res = aaxDriverClose(config);
    testForState(res, "aaxDriverClose");

    res = aaxDriverDestroy(config);
    testForState(res, "aaxDriverDestroy");

    return rv;
}

