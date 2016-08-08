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
#include <math.h>

#include <aax/defines.h>

#include "base/types.h"
#include "base/geometry.h"
#include "driver.h"
#include "wavfile.h"

#define FILE_PATH                    SRC_PATH"/tictac.wav"

aaxVec3f EmitterPos = { 0.0f, 0.0f, 10.0f };
aaxVec3f EmitterDir = { 0.0f, 0.0f,  1.0f };
aaxVec3f EmitterVel = { 0.0f, 0.0f,  0.0f };

aaxVec3f SensorPos = { 0.0f, 0.0f, -5.0f };
aaxVec3f SensorAt = {  0.0f, 0.0f, -1.0f };
aaxVec3f SensorUp = {  0.0f, 1.0f,  0.0f };
aaxVec3f SensorVel = { 0.0f, 0.0f,  0.0f };

int main(int argc, char **argv)
{
    char *devname, *infile;
    aaxConfig config;
    int res, mode;

    devname = getDeviceName(argc, argv);
    infile = getInputFile(argc, argv, FILE_PATH);
    mode = getMode(argc, argv);
    config = aaxDriverOpenByName(devname, mode);
    testForError(config, "No default audio device available.");

    if (config)
    {
        aaxBuffer buffer = bufferFromFile(config, infile);
        if (buffer)
        {
            aaxEmitter emitter;
            aaxMtx4f mtx;
            int deg = 0;
            float ang;

            /** mixer */
            res = aaxMixerInit(config);
            testForState(res, "aaxMixerInit");

            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /** sensor settings */
            res = aaxMatrixSetOrientation(mtx, SensorPos,
                                               SensorAt, SensorUp);
            testForState(res, "aaxMatrixSetOrientation");

            res = aaxMatrixInverse(mtx);
            res |= aaxSensorSetMatrix(config, mtx);
            testForState(res, "aaxSensorSetMatrix");

            res = aaxSensorSetVelocity(config, SensorVel);
            testForState(res, "aaxSensorSetVelocity");

            /** emitter */
            emitter = aaxEmitterCreate();
            testForError(emitter, "Unable to create a new emitter\n");

            res = aaxEmitterAddBuffer(emitter, buffer);
            testForState(res, "aaxEmitterAddBuffer");

            res = aaxEmitterSetMode(emitter, AAX_POSITION, AAX_ABSOLUTE);
            testForState(res, "aaxEmitterSetMode");

            res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
            testForState(res, "aaxEmitterSetLooping");

            res = aaxMatrixSetDirection(mtx, EmitterPos, EmitterDir);
            testForState(res, "aaxMatrixSetDirection");

            res = aaxEmitterSetMatrix(emitter, mtx);
            testForState(res, "aaxEmitterSetMatrix");

            res = aaxMixerRegisterEmitter(config, emitter);
            testForState(res, "aaxMixerRegisterEmitter");

            /** schedule the emitter for playback */
            res = aaxEmitterSetState(emitter, AAX_PLAYING);
            testForState(res, "aaxEmitterStart");

            deg = 0;
            printf("sensor position:\n");
            while(deg < 360)
            {
                msecSleep(50);

                ang = (float)deg * GMATH_DEG_TO_RAD;
                SensorAt[0] = sinf(ang);
                SensorAt[2] = -cosf(ang);

                printf("deg: %03u\tdir (% f, % f, % f)\n", deg,
                            SensorAt[0], SensorAt[1], SensorAt[2]);

                res = aaxMatrixSetOrientation(mtx, SensorPos,
                                                   SensorAt, SensorUp);
                testForState(res, "aaxMatrixSetOrientation");

                res = aaxMatrixInverse(mtx);
                res |= aaxSensorSetMatrix(config, mtx);
                testForState(res, "aaxSensorSetMatrix");
                deg += 3;
            }

            res = aaxEmitterSetState(emitter, AAX_STOPPED);
            testForState(res, "aaxEmitterStop");

            res = aaxMixerDeregisterEmitter(config, emitter);
            testForState(res, "aaxMixerDeregisterEmitter");

            res = aaxEmitterDestroy(emitter);
            testForState(res, "aaxEmitterDestroy");

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
