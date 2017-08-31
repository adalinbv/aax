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

#include <aax/aax.h>

#include "base/types.h"
#include "base/geometry.h"
#include "driver.h"
#include "wavfile.h"

#define IRFILE_PATH		SRC_PATH"/convolution.wav"

#define RADIUS			20.0f
#define FILE_PATH		SRC_PATH"/tictac.wav"

#define XEPOS		0.0
#define YEPOS		0.0
#define ZEPOS		2.0

aaxVec3d EmitterPos = {    XEPOS,    YEPOS, ZEPOS };
aaxVec3f EmitterDir = {     0.0f,     0.0f, 1.0f };
aaxVec3f EmitterVel = {     0.0f,     0.0f, 0.0f };

aaxVec3d SensorPos = { 00000.0f,    YEPOS, 00.0f };
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
        aaxBuffer irbuffer = bufferFromFile(config, IRFILE_PATH);
        aaxBuffer buffer = bufferFromFile(config, infile);
        if (buffer && irbuffer)
        {
            aaxMtx4d mtx64, rot64;
            aaxFilter filter;
            aaxEffect effect;
            aaxEmitter emitter[256];
            float pitch, anglestep;
            int frame_timing;
            int i, deg = 0;

            /** mixer */
            res = aaxMixerSetState(config, AAX_INITIALIZED);
            testForState(res, "aaxMixerInit");

            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /** scenery settings */
            filter = aaxFilterCreate(config, AAX_DISTANCE_FILTER);
            testForError(filter, "Unable to create the distance filter");

            res = aaxFilterSetState(filter, AAX_EXPONENTIAL_DISTANCE_DELAY);
            testForState(res, "aaxFilterSetState");

            res = aaxScenerySetFilter(config, filter);
            testForState(res, "aaxScenerySetDistanceModel");
            aaxFilterDestroy(filter);

            /* convolution */
            effect = aaxEffectCreate(config, AAX_CONVOLUTION_EFFECT);
            testForError(effect, "aaxEffectCreate");

            res = aaxEffectSetParam(effect, AAX_THRESHOLD, AAX_LOGARITHMIC, -40.0f);
            testForState(res, "aaxEffectSetParam");

            res = aaxEffectAddBuffer(effect, irbuffer);
            testForState(res, "aaxEffectAddBuffer");

            res = aaxEffectSetState(effect, AAX_TRUE);
            testForState(res, "aaxEffectSetState");

            res = aaxScenerySetEffect(config, effect);
            testForState(res, "aaxMixerSetEffect");

            res = aaxEffectDestroy(effect);
            testForState(res, "aaxEffectDestroy");

            /** sensor settings */
            res = aaxMatrix64SetOrientation(mtx64, SensorPos, SensorAt, SensorUp);
            testForState(res, "aaxSensorSetOrientation");
 
            res = aaxMatrix64Inverse(mtx64);
            testForState(res, "aaxMatrixInverse");

            res = aaxSensorSetMatrix64(config, mtx64);
            testForState(res, "aaxSensorSetMatrix");

            res = aaxSensorSetVelocity(config, SensorVel);
            testForState(res, "aaxSensorSetVelocity");

            /** emitter */
            pitch = getPitch(argc, argv);
            num = getNumEmitters(argc, argv);

            aaxMatrix64SetIdentityMatrix(rot64);

            /* Set emitters to located in a circle around the sensor */
            anglestep = (GMATH_PI) / (float)num;
            printf("Starting %i emitters\n", num);
            i = 0;
            do
            {
                emitter[i] = aaxEmitterCreate();
                testForError(emitter[i], "Unable to create a new emitter\n");

                res = aaxEmitterAddBuffer(emitter[i], buffer);
                testForState(res, "aaxEmitterAddBuffer");

                res = aaxMatrix64Rotate(rot64, anglestep, 0.0f, 1.0f, 0.0f);
                testForState(res, "aaxMatrixRotate");


                EmitterPos[0] =  8.0f*sinf(anglestep*i);
                EmitterPos[2] = -8.0f*cosf(anglestep*i);
                res = aaxMatrix64SetDirection(mtx64, EmitterPos, EmitterDir);

                res = aaxEmitterSetMatrix64(emitter[i], mtx64);
                testForState(res, "aaxEmitterSetIdentityMatrix");

                res = aaxEmitterSetMode(emitter[i], AAX_POSITION, AAX_RELATIVE);
                testForState(res, "aaxEmitterSetMode");

                res = aaxEmitterSetMode(emitter[i], AAX_LOOPING, AAX_TRUE);
                testForState(res, "aaxEmitterSetLooping");

                /* distance model */
                filter = aaxFilterCreate(config, AAX_DISTANCE_FILTER);
                testForError(filter, "Unable to create the distance filter");

                res = aaxFilterSetParam(filter, AAX_ROLLOFF_FACTOR, AAX_LINEAR, 3.0f);
                testForState(res, "aaxFilterSetParam");

                res = aaxScenerySetFilter(config, filter);
                testForState(res, "aaxScenerySetDistanceModel");
                aaxFilterDestroy(filter);

                /* pitch */
                effect = aaxEffectCreate(config, AAX_PITCH_EFFECT);
                testForError(effect, "Unable to create the pitch effect");

                res = aaxEffectSetParam(effect, AAX_PITCH, AAX_LINEAR, pitch);
                testForState(res, "aaxEffectSetParam");

                res = aaxEmitterSetEffect(emitter[i], effect);
                testForState(res, "aaxEmitterSetPitch");
                aaxEffectDestroy(effect);

                res = aaxMixerRegisterEmitter(config, emitter[i]);
                testForState(res, "aaxMixerRegisterEmitter");

                /** schedule the emitter for playback */
                res = aaxEmitterSetState(emitter[i], AAX_PLAYING);
                testForState(res, "aaxEmitterStart");

                msecSleep(15);
            }
            while (++i < num);

            frame_timing = aaxMixerGetSetup(config, AAX_FRAME_TIMING);
            printf("frame rendering time: %f ms\n", frame_timing/1000.0f);

            aaxMatrix64SetOrientation(mtx64, SensorPos, SensorAt, SensorUp);
            aaxMatrix64Inverse(mtx64);


            deg = 0;
            while(deg < 360)
            {
                msecSleep(50);

                aaxMatrix64Rotate(mtx64, GMATH_DEG_TO_RAD, 0.0f, 1.0f, 0.0f);
                aaxSensorSetMatrix64(config, mtx64);

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

        aaxBufferDestroy(irbuffer);
        aaxBufferDestroy(buffer);
    }

    res = aaxDriverClose(config);
    testForState(res, "aaxDriverClose");

    res = aaxDriverDestroy(config);
    testForState(res, "aaxDriverDestroy");

    return 0;
}
