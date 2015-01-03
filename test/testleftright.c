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
#include "driver.h"
#include "wavfile.h"

#define FILE_PATH_LEFT		SRC_PATH"/left_channel.wav"
#define FILE_PATH_RIGHT		SRC_PATH"/right_channel.wav"

aaxVec3f EmitterPos = {-10.0f, 0.0f, 0.0f };
aaxVec3f EmitterDir = { 0.0f, 0.0f, 1.0f };
aaxVec3f EmitterVel = { 0.0f, 0.0f, 0.0f };

aaxVec3f SensorPos = { 0.0f, 0.0f, 0.0f };
aaxVec3f SensorAt = {  0.0f, 0.0f,-1.0f };
aaxVec3f SensorUp = {  0.0f, 1.0f, 0.0f };
aaxVec3f SensorVel = { 0.0f, 0.0f, 0.0f };

#define _neg(x)		x = -x
#define _swap(x, y)	do { float z=x; x=y; y=z; } while (0);

int main(int argc, char **argv)
{
    char *devname, *infile[2] = { FILE_PATH_LEFT, FILE_PATH_RIGHT };
    enum aaxRenderMode mode;
    aaxConfig config;
    int num, res;

    mode = getMode(argc, argv);
    devname = getDeviceName(argc, argv);
    config = aaxDriverOpenByName(devname, mode);
    testForError(config, "No default audio device available.");

    if (config)
    {
        aaxEmitter emitter[2];
        aaxBuffer buffer[2];
        int i, deg = 0;
        aaxMtx4f mtx;

        /** mixer */
        res = aaxMixerInit(config);
        testForState(res, "aaxMixerInit");

        res = aaxMixerSetState(config, AAX_PLAYING);
        testForState(res, "aaxMixerStart");

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
        num = 2;
        for (i=0; i<num; i++)
        {
            buffer[i] = bufferFromFile(config, infile[i]);
            testForError(buffer[i], "Failed to load sample file");
        }

        printf("Starting %i emitters\n", num);
        printf("left / right\n");
        for (i=0; i<num; i++)
        {
            emitter[i] = aaxEmitterCreate();
            testForError(emitter[i], "Unable to create a new emitter\n");

            res = aaxEmitterAddBuffer(emitter[i], buffer[i]);
            testForState(res, "aaxEmitterAddBuffer");

            res = aaxEmitterSetMode(emitter[i], AAX_BUFFER_TRACK, i);
            testForState(res, "aaxEmitterSetBufferTrack");

            if (i == 1) _neg(EmitterPos[0]);
            res = aaxMatrixSetDirection(mtx, EmitterPos, EmitterDir);
            testForState(res, "aaxMatrixSetDirection");

            res = aaxEmitterSetMatrix(emitter[i], mtx);
            testForState(res, "aaxSensorSetMatrix");

            res = aaxEmitterSetMode(emitter[i], AAX_POSITION, AAX_ABSOLUTE);
            testForState(res, "aaxEmitterSetMode");

            res = aaxEmitterSetMode(emitter[i], AAX_LOOPING, AAX_TRUE);
            testForState(res, "aaxEmitterSetLooping");

            res = aaxMixerRegisterEmitter(config, emitter[i]);
            testForState(res, "aaxMixerRegisterEmitter");

            /** schedule the emitter for playback */
            res = aaxEmitterSetState(emitter[i], AAX_PLAYING);
            testForState(res, "aaxEmitterStart");
        }

        deg = 0;
        while(deg < 360)
        {
            msecSleep(50);
            deg += 5;
        }

        _neg(EmitterPos[0]); /* restore to original after left-right swap */
        _swap(EmitterPos[2], EmitterPos[0]);
        printf("front (left) / back (right)\n");
        for (i=0; i<num; i++)
        {
            if (i == 1) _neg(EmitterPos[2]);
            res = aaxMatrixSetDirection(mtx, EmitterPos, EmitterDir);
            testForState(res, "aaxMatrixSetDirection");

            res = aaxEmitterSetMatrix(emitter[i], mtx);
            testForState(res, "aaxSensorSetMatrix");
        }

        deg = 0;
        while(deg < 360)
        {
            msecSleep(50);
            deg += 5;
        }

        /* no need to restore to original; up is negative from left/front */
        _swap(EmitterPos[1], EmitterPos[2]);
        printf("up (left) / down (right)\n");
        for (i=0; i<num; i++)
        {
            if (i == 1) _neg(EmitterPos[1]);
            res = aaxMatrixSetDirection(mtx, EmitterPos, EmitterDir);
            testForState(res, "aaxMatrixSetDirection");

            res = aaxEmitterSetMatrix(emitter[i], mtx);
            testForState(res, "aaxSensorSetMatrix");
        }

        deg = 0;
        while(deg < 360)
        {
            msecSleep(50);
            deg += 5;
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

            res = aaxBufferDestroy(buffer[i]);
            testForState(res, "aaxBufferDestroy");
        }
        while (++i < num);

        res = aaxMixerSetState(config, AAX_STOPPED);
        testForState(res, "aaxMixerStop");
    }

    res = aaxDriverClose(config);
    testForState(res, "aaxDriverClose");

    res = aaxDriverDestroy(config);
    testForState(res, "aaxDriverDestroy");

    return 0;
}
