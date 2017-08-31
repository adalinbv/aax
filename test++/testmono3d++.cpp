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

#include <aax/aeonwave.hpp>

#include "base/types.h"
#include "base/geometry.h"
#include "driver.h"
#include "wavfile.h"

#define RADIUS			20.0f
#define FILE_PATH		SRC_PATH"/sine-440Hz-1period.wav"

#define XEPOS		00000.0
#define YEPOS		-1000.0
#define ZEPOS		00.0

aax::Vector64 EmitterPos(   XEPOS ,   YEPOS, ZEPOS);
aax::Vector EmitterDir(    0.0f,     0.0f,  1.0f);
aax::Vector EmitterVel(    0.0f,     0.0f,  0.0f);

aax::Vector64 SensorPos( 00000.0,   YEPOS, 00.0);
aax::Vector SensorAt(      0.0f,     0.0f, -1.0f);
aax::Vector SensorUp(      0.0f,     1.0f,  0.0f);
aax::Vector SensorVel(     0.0f,     0.0f,  0.0f);

int main(int argc, char **argv)
{
    int res;
    enum aaxRenderMode mode = aaxRenderMode(getMode(argc, argv));
    char* devname = getDeviceName(argc, argv);
    char* infile = getInputFile(argc, argv, FILE_PATH);

    aax::AeonWave config(devname, mode);
    testForError(config, "No default audio device available.");

    if (config)
    {
        aax::Buffer buffer = config.buffer(infile);
        if (buffer)
        {
            aax::Emitter emitter[256];
            aax::Matrix64 mtx;
            aax::dsp dsp;

            /** mixer */
            res = config.set(AAX_INITIALIZED);
            testForState(res, "aaxMixerInit");

            res = config.set(AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /** scenery settings */
            dsp = config.get(AAX_DISTANCE_FILTER);
            dsp.set(AAX_EXPONENTIAL_DISTANCE_DELAY);
            res = config.set(dsp);
            testForState(res, "aaxScenerySetDistanceModel");

            /** sensor settings */
            res = mtx.set(SensorPos, SensorAt, SensorUp);
            testForState(res, "aaxSensorSetOrientation");
 
            res = mtx.inverse();
            testForState(res, "aaxMatrixInverse");

            res = config.sensor_matrix(mtx);
            testForState(res, "aaxSensorSetMatrix");

            res = config.sensor_velocity(SensorVel);
            testForState(res, "aaxSensorSetVelocity");

            /** emitter */
            float pitch = getPitch(argc, argv);
            int num = getNumEmitters(argc, argv);

            /* Set emitters to located in a circle around the sensor */
            float anglestep = (2 * GMATH_PI) / (float)num;
            printf("Starting %i emitters\n", num);
            int i = 0;
            do
            {
                static float mul = 1.0f;

                emitter[i] = aax::Emitter(AAX_ABSOLUTE);
                res = emitter[i].add(buffer);
                testForState(res, "aaxEmitterAddBuffer");

                mtx.set(EmitterPos, EmitterDir);
                mtx.rotate(anglestep, 0.0f, 1.0f, 0.0f);
                res = emitter[i].matrix(mtx);
                testForState(res, "aaxEmitterSetIdentityMatrix");
                mul *= -1.0f;

                res = emitter[i].set(AAX_LOOPING, AAX_TRUE);
                testForState(res, "aaxEmitterSetLooping");

                aax::dsp dsp = emitter[i].get(AAX_DISTANCE_FILTER);
                dsp.set(AAX_REF_DISTANCE, 3.0f);
                res = emitter[i].set(dsp);
                testForState(res, "aaxEmitterSetReferenceDistance");

                dsp = emitter[i].get(AAX_PITCH_EFFECT);
                dsp.set(AAX_PITCH, pitch);
                res = emitter[i].set(dsp);
                testForState(res, "aaxEmitterSetPitch");

                config.add(emitter[i]);
                testForState(res, "aaxMixerRegisterEmitter");

                /** schedule the emitter for playback */
                res = emitter[i].set(AAX_PLAYING);
                testForState(res, "aaxEmitterStart");

                msecSleep(15);
            }
            while (++i < num);

            int frame_timing = config.get(AAX_FRAME_TIMING);
            printf("frame rendering time: %f ms\n", frame_timing/1000.0f);

            int deg = 0;
            while(deg < 360)
            {
                float ang = (float)deg * GMATH_DEG_TO_RAD;

                msecSleep(50);

                EmitterPos[0] = XEPOS + RADIUS * sinf(ang);
                EmitterPos[2] = ZEPOS + -RADIUS * cosf(ang);
 //             EmitterPos[1] = YEPOS + RADIUS * sinf(ang);

                printf("deg: %03u\tpos (% f, % f, % f)\n", deg,
                            EmitterPos[0], EmitterPos[1], EmitterPos[2]);

                res = mtx.set( EmitterPos, EmitterDir);
                testForState(res, "aaxMatrixSetDirection");

                i = 0;
                do
                {
                    res = emitter[i].matrix(mtx);
                    testForState(res, "aaxSensorSetMatrix");
                }
                while (++i < num);

                deg += 1;
            }

            i = 0;
            do
            {
                res = emitter[i].set(AAX_STOPPED);
                testForState(res, "aaxEmitterStop");

                res = config.remove(emitter[i]);
                testForState(res, "aaxMixerDeregisterEmitter");
            }
            while (++i < num);

            res = config.set(AAX_STOPPED);
            testForState(res, "aaxMixerStop");

            config.destroy(buffer);
        }
    }

    return 0;
}
