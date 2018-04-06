/*
 * Copyright (C) 2017-2018 by Erik Hofman.
 * Copyright (C) 2017-2018 by Adalin B.V.
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

#include "base/geometry.h"
#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define ZPOS			-2.0
#define RADIUS			 1.5f
#define DIMENSION		 1.5f
#define DENSITY			 1.0f
#define FILE_PATH		SRC_PATH"/tictac.wav"

aaxVec3f WorldAt =    {  0.0f, 0.0f,  1.0f };
aaxVec3f WorldUp =    {  0.0f, 1.0f,  0.0f };

aaxVec3f EmitterDir = {  0.0f, 0.0f,  1.0f };
aaxVec3d EmitterPos = {  0.0,  1.0,   ZPOS };	// in metres (right, up, back)

aaxVec3d FramePos =   {  0.0,  1.0,   0.0  };

aaxVec3d SensorPos =  {  0.0,  1.0,   0.0  };
aaxVec3f SensorAt =   {  0.0f, 0.0f, -1.0f };
aaxVec3f SensorUp =   {  0.0f, 1.0f,  0.0f };

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

    if (config && (rv >= 0))
    {
        aaxBuffer buffer;

        buffer = bufferFromFile(config, infile);
        if (buffer)
        {
            aaxEmitter emitter;
            aaxFilter filter;
            aaxFrame frame;
            aaxMtx4d mtx64;
            float deg;

            /** mixer */
            res = aaxMixerSetState(config, AAX_INITIALIZED);
            testForState(res, "aaxMixerInit");

            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /** sensor settings */
            res = aaxMatrix64SetOrientation(mtx64, SensorPos, SensorAt, SensorUp);
            testForState(res, "aaxSensorSetOrientation");
 
            res = aaxMatrix64Inverse(mtx64);
            testForState(res, "aaxMatrix64Inverse");

            res = aaxSensorSetMatrix64(config, mtx64);
            testForState(res, "aaxSensorSetMatrix64");

            /* audio frame */
            frame = aaxAudioFrameCreate(config);
            testForError(frame, "Unable to create a new audio frame\n");

            res= aaxMatrix64SetOrientation(mtx64, FramePos, WorldAt, WorldUp);
            testForState(res, "aaxAudioFrameSetOrientation: Frame");

            res = aaxAudioFrameSetMatrix64(frame, mtx64);
            testForState(res, "aaxAudioFrameSetMatrix64");

            res = aaxAudioFrameSetMode(frame, AAX_POSITION, AAX_INDOOR);
            testForState(res, "aaxAudioFrameSetMode");

            res = aaxMixerRegisterAudioFrame(config, frame);
            testForState(res, "aaxMixerRegisterAudioFrame: Frame");

            // occlusion
            filter = aaxAudioFrameGetFilter(frame, AAX_VOLUME_FILTER);
            testForError(filter, "aaxAudioFrameGetFilter");

            res = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 1.0f, 0.0f, 1.0f, 0.0f);
            res = aaxFilterSetSlot(filter, 1, AAX_LINEAR, DIMENSION, DIMENSION, DIMENSION, DENSITY);
            testForState(res, "aaxFilterSetSlot");

            res = aaxFilterSetState(filter, AAX_INVERSE);
            testForState(res, "aaxFilterSetState");

            res = aaxAudioFrameSetFilter(frame, filter);
            testForState(res, "aaxAudioFrameSetFilter");

            res = aaxEffectDestroy(filter);
            testForError(filter, "aaxFilterDestroy");

            /** schedule the audioframe for playback */
            res = aaxAudioFrameSetState(frame, AAX_PLAYING);
            testForState(res, "aaxAudioFrameStart: Frame");

            /* emitter */
            emitter = aaxEmitterCreate();
            testForError(emitter, "Unable to create a new emitter");

            res = aaxEmitterAddBuffer(emitter, buffer);
            testForState(res, "aaxEmitterAddBuffer");

            res = aaxMatrix64SetDirection(mtx64, EmitterPos, EmitterDir);
            testForState(res, "aaxEmitterSetDirection");

            res = aaxEmitterSetMatrix64(emitter, mtx64);
            testForState(res, "aaxEmitterSetIdentityMatrix64");

            res = aaxEmitterSetMode(emitter, AAX_POSITION, AAX_ABSOLUTE);
            testForState(res, "aaxEmitterSetMode");

            res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
            testForState(res, "aaxEmitterSetLooping");

            filter = aaxFilterCreate(config, AAX_DISTANCE_FILTER);
            testForError(filter, "Unable to create the distance filter");

            res = aaxFilterSetParam(filter, AAX_REF_DISTANCE, AAX_LINEAR, 0.3f);
            testForState(res, "aaxEmitterSetReferenceDistance");

            res = aaxFilterSetState(filter, AAX_EXPONENTIAL_DISTANCE);
            testForState(res, "aaxFilterSetDistanceModel");

            res = aaxEmitterSetFilter(emitter, filter);
            testForState(res, "aaxScenerySetDistanceModel");

            aaxFilterDestroy(filter);

            /** register the emitter to the audio-frame */
            res = aaxAudioFrameRegisterEmitter(frame, emitter);
            testForState(res, "aaxMixerRegisterEmitter");

            /** schedule the emitter for playback */
            res = aaxEmitterSetState(emitter, AAX_PLAYING);
            testForState(res, "aaxEmitterStart");

            deg = 0;
            set_mode(1);
            while(deg < 360)
            {
                float ang = deg * GMATH_DEG_TO_RAD;

                EmitterPos[0] = RADIUS * sinf(ang);
                EmitterPos[2] = ZPOS - RADIUS * cosf(ang);

                printf("deg: %2.1f\tpos (% lf, % lf, % lf)\n", deg,
                            EmitterPos[0], EmitterPos[1], EmitterPos[2]);

                res = aaxMatrix64SetDirection(mtx64, EmitterPos, EmitterDir);
                testForState(res, "aaxMatrixSetDirection");

                res = aaxEmitterSetMatrix64(emitter, mtx64);
                testForState(res, "aaxSensorSetMatrix");

                msecSleep(50);
                deg += 1.0f;

                if (get_key()) break;
            }
            set_mode(0);

            res = aaxAudioFrameSetState(frame, AAX_STOPPED);
            testForState(res, "aaxAudioFrameStop: Frame");

            res = aaxEmitterSetState(emitter, AAX_STOPPED);
            testForState(res, "aaxEmitterStop");

            /* destruction */
            res = aaxAudioFrameDeregisterEmitter(frame, emitter);
            testForState(res, "aaxMixerDeregisterEmitter");

            res = aaxEmitterDestroy(emitter);
            testForState(res, "aaxEmitterDestroy");

            res = aaxBufferDestroy(buffer);
            testForState(res, "aaxBufferDestroy");

            res = aaxMixerDeregisterAudioFrame(config, frame);
            testForState(res, "aaxMixerDeregisterAudioFrame");

            res = aaxAudioFrameDestroy(frame);
            testForState(res, "aaxAudioFrameDestroy: Frame");

            res = aaxMixerSetState(config, AAX_STOPPED);
            testForState(res, "aaxMixerStop");
        }
    }

    res = aaxDriverClose(config);
    testForState(res, "aaxDriverClose");

    res = aaxDriverDestroy(config);
    testForState(res, "aaxDriverDestroy");

    return rv;
}
