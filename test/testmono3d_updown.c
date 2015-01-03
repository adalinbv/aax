/*
 * Copyright (C) 2008-2015 by Erik Hofman.
 * Copyright (C) 2009-2015 by Adalin B.V.
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
#include <math.h>

#include <aax/defines.h>

#include "base/types.h"
#include "base/geometry.h"
#include "driver.h"
#include "wavfile.h"

#define RADIUS			20.0f
#define FILE_PATH		SRC_PATH"/tictac.wav"

#define XEPOS		00000.0f
#define YEPOS		-0000.0f
#define ZEPOS		00.0f

aaxVec3f EmitterPos = {    XEPOS,    YEPOS, ZEPOS };
aaxVec3f EmitterDir = {     0.0f,     0.0f, 1.0f };
aaxVec3f EmitterVel = {     0.0f,     0.0f, 0.0f };

aaxVec3f SensorPos = { 00000.0f, -0000.0f, 00.0f };
aaxVec3f SensorAt = {      0.0f,     0.0f, -1.0f };
aaxVec3f SensorUp = {      0.0f,     1.0f,  0.0f };
aaxVec3f SensorVel = {     0.0f,     0.0f,  0.0f };

int main(int argc, char **argv)
{
    char *devname, *infile;
    enum aaxRenderMode mode;
    aaxConfig config;
    int num, res;

    mode = getMode(argc, argv);
    devname = getDeviceName(argc, argv);
    infile = getInputFile(argc, argv, FILE_PATH);
    config = aaxDriverOpenByName(devname, mode);
    testForError(config, "No default audio device available.");

    if (config)
    {
        aaxBuffer buffer = bufferFromFile(config, infile);
        if (buffer)
        {
            aaxEmitter emitter[256];
            float pitch, anglestep;
            int i, deg = 0;
            aaxMtx4f mtx;

            /** mixer */
            res = aaxMixerInit(config);
            testForState(res, "aaxMixerInit");

            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /** scenery settings */
            res=aaxScenerySetDistanceModel(config,
                                           AAX_EXPONENTIAL_DISTANCE_DELAY);
            testForState(res, "aaxScenerySetDistanceModel");

            /** sensor settings */
            res = aaxMatrixSetOrientation(mtx, SensorPos, SensorAt, SensorUp);
            testForState(res, "aaxSensorSetOrientation");
 
            res = aaxMatrixInverse(mtx);
            testForState(res, "aaxMatrixInverse");

            res = aaxSensorSetMatrix(config, mtx);
            testForState(res, "aaxSensorSetMatrix");

            res = aaxSensorSetVelocity(config, SensorVel);
            testForState(res, "aaxSensorSetVelocity");

            /** emitter */
            pitch = getPitch(argc, argv);
            num = getNumEmitters(argc, argv);

            /* Set emitters to located in a circle around the sensor */
            anglestep = (2 * GMATH_PI) / (float)num;
            printf("Starting %i emitters\n", num);
            i = 0;
            do
            {
                static float mul = 1.0f;

                emitter[i] = aaxEmitterCreate();
                testForError(emitter[i], "Unable to create a new emitter\n");

                res = aaxEmitterAddBuffer(emitter[i], buffer);
                testForState(res, "aaxEmitterAddBuffer");

                aaxMatrixSetDirection(mtx, EmitterPos, EmitterDir);
                aaxMatrixRotate(mtx, anglestep, 0.0f, 1.0f, 0.0f);
                res = aaxEmitterSetMatrix(emitter[i], mtx);
                testForState(res, "aaxEmitterSetIdentityMatrix");
                mul *= -1.0f;

                res = aaxEmitterSetMode(emitter[i], AAX_POSITION, AAX_ABSOLUTE);
                testForState(res, "aaxEmitterSetMode");

                res = aaxEmitterSetMode(emitter[i], AAX_LOOPING, AAX_TRUE);
                testForState(res, "aaxEmitterSetLooping");

                res = aaxEmitterSetReferenceDistance(emitter[i], 3.0f);
                testForState(res, "aaxEmitterSetReferenceDistance");

                res = aaxEmitterSetPitch(emitter[i], pitch);
                testForState(res, "aaxEmitterSetPitch");

                res = aaxMixerRegisterEmitter(config, emitter[i]);
                testForState(res, "aaxMixerRegisterEmitter");

                /** schedule the emitter for playback */
                res = aaxEmitterSetState(emitter[i], AAX_PLAYING);
                testForState(res, "aaxEmitterStart");

                msecSleep(750);
            }
            while (++i < num);

            deg = 0;
            while(deg < 360)
            {
                float ang = (float)deg * GMATH_DEG_TO_RAD;

                msecSleep(50);

                EmitterPos[0] = XEPOS + RADIUS * cosf(ang);
//              EmitterPos[2] = ZEPOS + -RADIUS * cosf(ang);
                EmitterPos[1] = YEPOS + RADIUS * sinf(ang);

                printf("deg: %03u\tpos (% f, % f, % f)\n", deg,
                            EmitterPos[0], EmitterPos[1], EmitterPos[2]);

                res = aaxMatrixSetDirection(mtx, EmitterPos, EmitterDir);
                testForState(res, "aaxMatrixSetDirection");

                i = 0;
                do
                {
                    res = aaxEmitterSetMatrix(emitter[i], mtx);
                    testForState(res, "aaxSensorSetMatrix");
                }
                while (++i < num);

                deg += 1;
            }

            i = 0;
            do
            {
                res = aaxEmitterSetState(emitter[i], AAX_STOPPED);
                testForState(res, "aaxEmitterStop");

                res = aaxMixerDeregisterEmitter(config, emitter[i]);
                testForState(res, "aaxMixerDeregisterEmitter");

                res = aaxEmitterDestroy(emitter[i]);
                testForState(res, "aaxEmitterDestroy");
            }
            while (++i < num);

            res = aaxBufferDestroy(buffer);
            testForState(res, "aaxBufferDestroy");

            res = aaxMixerSetState(config, AAX_STOPPED);
            testForState(res, "aaxMixerStop");
        }
    }

    res = aaxDriverClose(config);
    testForState(res, "aaxDriverClose");

    res = aaxDriverDestroy(config);
    testForState(res, "aaxDriverDestroy");

    return 0;
}
