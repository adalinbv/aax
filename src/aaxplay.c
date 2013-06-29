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
#include <stdlib.h>

#include <aax/aax.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define FILE_PATH		SRC_PATH"/stereo.mp3"

int main(int argc, char **argv)
{
    aaxConfig config, record;
    char *devname, *infile;
    char indevname[4096];
    int res, rv = 0;

    infile = getInputFile(argc, argv, FILE_PATH);
    if (infile) {
       snprintf(indevname, 4096, "AeonWave on Audio Files: %s", infile);
    }
    else
    {
       char *capture_name = getCaptureName(argc, argv);
       if (capture_name) {
          snprintf(indevname, 4096, "%s", capture_name);
       }
       else
       {
          printf("Usage:\n");
          printf("  aaxplay -i <filename> (-d <playcbakc device>)\n");
          printf("  aaxplay -c <capture device> (-d <playcbakc device>)\n\n");
          exit(-1);
       }
    }

    devname = getDeviceName(argc, argv);

    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    testForError(config, "Audio output device is not available.");
    if (!config || !aaxIsValid(config, AAX_CONFIG_HD))
    {
    // TODO: fall back to buffer streaming mode
        printf("Warning:\n");
        printf("  %s requires a registered version of AeonWave\n", argv[0]);
        printf("  Please visit http://www.adalin.com/buy_aeonwaveHD.html to ");
        printf("obtain\n  a product-key.\n\n");
        rv = -2;
    }
    else
    {
        record = aaxDriverOpenByName(indevname, AAX_MODE_READ);
        if (!record)
        {
            printf("File not found: %s\n", infile);
            exit(-1);
        }
        printf("Playing: %s\n", infile);
    }

    if (config && record && (rv >= 0))
    {
        char *fparam = getCommandLineOption(argc, argv, "-f");
        aaxFrame frame = NULL;
        int state;

        /** mixer */
        res = aaxMixerSetState(config, AAX_INITIALIZED);
        testForState(res, "aaxMixerInit");

        res = aaxMixerSetState(config, AAX_PLAYING);
        testForState(res, "aaxMixerStart");

        if (fparam)
        {
            printf("  using audio-frames\n");

            /** audio frame */
            frame = aaxAudioFrameCreate(config);
            testForError(frame, "Unable to create a new audio frame\n");   

            /** register audio frame */
            res = aaxMixerRegisterAudioFrame(config, frame);
            testForState(res, "aaxMixerRegisterAudioFrame");

            res = aaxAudioFrameSetState(frame, AAX_PLAYING);
            testForState(res, "aaxAudioFrameSetState");

            /** sensor */
            res = aaxAudioFrameRegisterSensor(frame, record);
            testForState(res, "aaxAudioFrameRegisterSensor");
        }
        else
        {
            /** sensor */
            res = aaxMixerRegisterSensor(config, record);
            testForState(res, "aaxMixerRegisterSensor");
        }

        /** must be called after aaxMixerRegisterSensor */
        res = aaxMixerSetState(record, AAX_INITIALIZED);
        testForState(res, "aaxMixerSetInitialize");

        res = aaxSensorSetState(record, AAX_CAPTURING);
        testForState(res, "aaxSensorCaptureStart");

        set_mode(1);
        do
        {
            if (get_key()) break;

            msecSleep(100);
            state = aaxMixerGetState(record);
        }
        while (state == AAX_PLAYING);
        set_mode(0);

        res = aaxMixerSetState(config, AAX_STOPPED);
        testForState(res, "aaxMixerSetState");

        if (frame)
        {
            res = aaxAudioFrameSetState(frame, AAX_STOPPED);
            testForState(res, "aaxAudioFrameSetState");
        }

        res = aaxSensorSetState(record, AAX_STOPPED);
        testForState(res, "aaxSensorCaptureStop");

        if (frame)
        {
            res = aaxAudioFrameDeregisterSensor(frame, record);
            testForState(res, "aaxAudioFrameDeregisterSensor");

            res = aaxMixerDeregisterAudioFrame(config, frame);
            testForState(res, "aaxMixerDeregisterAudioFrame");

            res = aaxAudioFrameDestroy(frame);
            testForState(res, "aaxAudioFrameDestroy");
        }
        else
        {
            res = aaxMixerDeregisterSensor(config, record);
            testForState(res, "aaxMixerDeregisterSensor");
        }

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

