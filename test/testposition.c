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

#include <aax/defines.h>

#include <base/geometry.h>
#include <base/types.h>
#include "driver.h"
#include "wavfile.h"

/*
 * Attach an emitter to a sub-frame which is attached to an audio-frame,
 * in such a way that rotating on or more of them for 90 degrees moves the
 * emitter from left to right (or forn or back) of the sensor.
 * All positions are relative.
 *
 * X: positive to the right,
 * Y: positive upwards,
 * Z: positibe backwards.
 */

#define FILE_PATH		SRC_PATH"/tictac.wav"

aaxVec3f SensorPos =  { 9.0f, 9.0f,  9.0f };
aaxVec3f SensorAt  =  { 0.0f, 0.0f, -1.0f };
aaxVec3f SensorUp  =  { 0.0f, 1.0f,  0.0f };

aaxVec3f Frame1Pos =  { -0.5f, 0.0f, 0.0f };		/* 0.5m to the left */
aaxVec3f Frame1At =   {  0.0f, 0.0f, 1.0f };
aaxVec3f Frame1Up =   {  0.0f, 1.0f, 0.0f };

aaxVec3f Frame2Pos =  {  1.0f, 0.0f, 0.0f };		/* 1m to the right */
aaxVec3f Frame2At =   {  0.0f, 0.0f, 1.0f };
aaxVec3f Frame2Up =   {  0.0f, 1.0f, 0.0f };

aaxVec3f EmitterPos = { -1.0f, 0.0f, 0.0f };		/* 1m to the left */
aaxVec3f EmitterDir = {  0.0f, 0.0f, 1.0f };

int main(int argc, char **argv)
{
    enum aaxRenderMode mode;
    char *devname, *infile;
    aaxConfig config;
    int res, rv = 0;

    mode = getMode(argc, argv);
    devname = getDeviceName(argc, argv);
    infile = getInputFile(argc, argv, FILE_PATH);
    config = aaxDriverOpenByName(devname, mode);
    testForError(config, "No default audio device available.");

    if (!aaxIsValid(config, AAX_CONFIG_HD))
    {
        printf("Warning:\n");
        printf("  %s requires a registered version of AeonWave\n", argv[0]);
        printf("  Please visit http://www.adalin.com/buy_aeonwaveHD.html to ");
        printf("obtain\n  a product-key.\n\n");
        rv = -1;
    }

    if (config && (rv >= 0))
    {
        aaxEmitter emitter;
        aaxFrame frame[2];
        aaxBuffer buffer;
        aaxMtx4f mtx;
        float pitch;

        buffer = bufferFromFile(config, infile);
        testForError(buffer, "Unable to generate buffer\n");

        /** mixer */
        res = aaxMixerInit(config);
        testForState(res, "aaxMixerInit");

        res = aaxMixerSetState(config, AAX_PLAYING);
        testForState(res, "aaxMixerStart");

        /** sensor */
        res = aaxMatrixSetOrientation(mtx, SensorPos, SensorAt, SensorUp);
        testForState(res, "aaxSensorSetOrientation");

        res = aaxMatrixTranslate(mtx, 5.0f, 5.0f, 5.0f);
        testForState(res, "aaxMatrixTranslate");

        res = aaxMatrixInverse(mtx);
        testForState(res, "aaxMatrixInverse");

        res = aaxSensorSetMatrix(config, mtx);
        testForState(res, "aaxSensorSetMatrix");

        /* frame */
        frame[0] = aaxAudioFrameCreate(config);
        testForError(frame[0], "Unable to create a new audio frame\n");

        res = aaxMatrixSetOrientation(mtx, Frame1Pos, Frame1At, Frame1Up);
        testForState(res, "aaxAudioFrameSetOrientation");

        res = aaxAudioFrameSetMatrix(frame[0], mtx);
        testForState(res, "aaxAudioFrameSetMatrix");

        res=aaxAudioFrameSetMode(frame[0], AAX_POSITION, AAX_RELATIVE);
        testForState(res, "aaxAudioFrameSetMode");

        res = aaxMixerRegisterAudioFrame(config, frame[0]);
        testForState(res, "aaxMixerRegisterAudioFrame");

        res = aaxAudioFrameSetState(frame[0], AAX_PLAYING);
        testForState(res, "aaxAudioFrameStart");

        /* sub-frame */
        frame[1] = aaxAudioFrameCreate(config);
        testForError(frame[1], "Unable to create a new audio frame\n");

        res = aaxMatrixSetOrientation(mtx, Frame2Pos, Frame2At, Frame2Up);
        testForState(res, "aaxAudioFrameSetOrientation");

        res = aaxAudioFrameSetMatrix(frame[1], mtx);
        testForState(res, "aaxAudioFrameSetMatrix");

        res=aaxAudioFrameSetMode(frame[1], AAX_POSITION, AAX_RELATIVE);
        testForState(res, "aaxAudioFrameSetMode");

        res = aaxAudioFrameRegisterAudioFrame(frame[0], frame[1]);
        testForState(res, "aaxAudioFrameRegisterAudioFrame");

        res = aaxAudioFrameSetState(frame[1], AAX_PLAYING);
        testForState(res, "aaxAudioFrameStart");

        /** emitter */
        emitter = aaxEmitterCreate();
        testForError(emitter, "Unable to create a new emitter\n");

        res = aaxEmitterAddBuffer(emitter, buffer);
        testForState(res, "aaxEmitterAddBuffer");

        pitch = getPitch(argc, argv);
        res = aaxEmitterSetPitch(emitter, pitch);
        testForState(res, "aaxEmitterSetPitch");

        res = aaxMatrixSetDirection(mtx, EmitterPos, EmitterDir);
        testForState(res, "aaxMatrixSetDirection");

        res = aaxEmitterSetMatrix(emitter, mtx);
        testForState(res, "aaxEmitterSetMatrix");

        res = aaxEmitterSetMode(emitter, AAX_POSITION, AAX_RELATIVE);
        testForState(res, "aaxEmitterSetMode");

        res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
        testForState(res, "aaxEmitterSetLooping");

        res = aaxAudioFrameRegisterEmitter(frame[1], emitter);
        testForState(res, "aaxMixerRegisterEmitter");

        res = aaxEmitterSetState(emitter, AAX_PLAYING);
        testForState(res, "aaxEmitterStart");

        printf("Starting playback.\n");
        printf("The emitter is now positioned 90 degrees to the left.\n");
        do
        {
            static int i = 0;

            msecSleep(2000);	/* 2 seconds */

            switch(i)
            {
            case 0:
                printf("Rotating the sensor 180 degrees.\n");
                printf("The emitter is now 90 degrees to the right.\n");
                res=aaxMatrixSetOrientation(mtx, SensorPos, SensorAt, SensorUp);
                testForState(res, "aaxSensorSetOrientation");

                res=aaxMatrixRotate(mtx,180.0f*GMATH_DEG_TO_RAD,0.0f,1.0f,0.0f);
                testForState(res, "aaxMatrixRotate");
                res = aaxMatrixInverse(mtx);
                testForState(res, "aaxMatrixInverse");

                res = aaxSensorSetMatrix(config, mtx);
                testForState(res, "aaxSensorSetMatrix");
                break;
            case 1:
                printf("Rotating the audio-frame 180 degrees.\n");
                printf("The emitter is now positioned 90 degrees to the left.\n");
                res=aaxMatrixSetOrientation(mtx, Frame1Pos, Frame1At, Frame1Up);
                res=aaxMatrixRotate(mtx,180.0f*GMATH_DEG_TO_RAD,0.0f,1.0f,0.0f);
                testForState(res, "aaxMatrixRotate");
                res = aaxAudioFrameSetMatrix(frame[0], mtx);
                testForState(res, "aaxAudioFrameSetMatrix");
                break;
            case 2:
                printf("Rotating the sub-frame 180 degrees.\n");
                printf("The emitter is now back to 90 degrees to the right.\n");
                res=aaxMatrixSetOrientation(mtx, Frame2Pos, Frame2At, Frame2Up);
                res=aaxMatrixRotate(mtx,180.0f*GMATH_DEG_TO_RAD,0.0f,1.0f,0.0f);
                testForState(res, "aaxMatrixRotate");
                res = aaxAudioFrameSetMatrix(frame[1], mtx);
                testForState(res, "aaxAudioFrameSetMatrix");
                break;
            default:
                break;
            }

            if (++i == 4) break;
        }
        while (1);

        res = aaxEmitterStop(emitter);
        res = aaxEmitterRemoveBuffer(emitter);
        testForState(res, "aaxEmitterRemoveBuffer");

        res = aaxAudioFrameDeregisterEmitter(frame[1], emitter);
        res = aaxBufferDestroy(buffer);
        testForState(res, "aaxBufferDestroy");

        res = aaxAudioFrameSetState(frame[1], AAX_STOPPED);
        res = aaxAudioFrameDeregisterAudioFrame(frame[0], frame[1]);
        aaxAudioFrameDestroy(frame[1]);

        res = aaxAudioFrameSetState(frame[0], AAX_STOPPED);
        res = aaxMixerDeregisterAudioFrame(config, frame[0]);
        aaxAudioFrameDestroy(frame[0]);

        res = aaxMixerDeregisterEmitter(config, emitter);
        res = aaxMixerSetState(config, AAX_STOPPED);
        res = aaxEmitterDestroy(emitter);
    }

    res = aaxDriverClose(config);
    res = aaxDriverDestroy(config);

    return rv;
}
