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
#include "driver.h"
#include "wavfile.h"

#define MODE			AAX_RELATIVE

/* 50m/s (0.145*Vsound) = about 100kts */
#define UPDATE_DELAY		0.00033f
#define SPEED_OF_SOUND		343.0f
#define SPEED			(0.145f*SPEED_OF_SOUND)
#define STEP			(SPEED*UPDATE_DELAY)

#define FILE_PATH		SRC_PATH"/wasp.wav"
#define SXPOS			1000.0f
#define SYPOS			1000.0f
#define SZPOS			-500.0f

#define INITIAL_DIST		150.0f
#define FXPOS			(-INITIAL_DIST)
#define FYPOS			(30.0f)
#define FZPOS			(15.0f)

#define EXPOS			(-1.0f)
#define EYPOS			(0.5f)
#define EZPOS			(0.5f)

aaxVec3f EmitterDir = {  1.0f,  0.0f,  0.0f };
aaxVec3f EmitterVel = {  0.0f,  0.0f,  0.0f };
aaxVec3d EmitterPos = { EXPOS, EYPOS, EZPOS };

aaxVec3d FramePos   = { FXPOS, FYPOS, FZPOS };
aaxVec3f FrameVel   = { SPEED,  0.0f,  0.0f };
aaxVec3f FrameDir   = {  1.0f,  0.0f,  0.0f };

aaxVec3d SensorPos  = { SXPOS, SYPOS, SZPOS };
aaxVec3f SensorVel  = {  0.0f,  0.0f, SPEED };
aaxVec3f SensorAt   = {  0.0f,  0.0f, -1.0f };
aaxVec3f SensorUp   = {  0.0f,  1.0f,  0.0f };

int main(int argc, char **argv)
{
    enum aaxRenderMode mode;
    char *devname, *infile;
    aaxConfig config;
    int res;

    mode = getMode(argc, argv);
    devname = getDeviceName(argc, argv);
    infile = getInputFile(argc, argv, FILE_PATH);
    config = aaxDriverOpenByName(devname, mode);
    if (config)
    {
        aaxBuffer buffer = bufferFromFile(config, infile);
        if (buffer)
        {
            aaxEmitter emitter;
            aaxFilter filter;
            aaxEffect effect;
            aaxFrame frame;
            aaxMtx4d mtx64;
            float dist;

            /** mixer */
            res = aaxMixerSetState(config, AAX_INITIALIZED);
            testForState(res, "aaxMixerInit");

            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /** scenery settings */

            /** doppler settings */
            effect = aaxEffectCreate(config, AAX_VELOCITY_EFFECT);
            testForError(effect, "Unable to create the velocity effect");

            res = aaxEffectSetParam(effect, AAX_SOUND_VELOCITY, AAX_LINEAR, SPEED_OF_SOUND);
            testForState(res, "aaxScenerySetSoundVelocity");

            res = aaxScenerySetEffect(config, effect);
            testForState(res, "aaxScenerySetEffect");
            aaxEffectDestroy(effect);

            /** sensor settings */
            res = aaxMatrix64SetOrientation(mtx64, SensorPos,
                                               SensorAt, SensorUp);
            testForState(res, "aaxMatrix64SetOrientation");

            res = aaxMatrix64Inverse(mtx64);
            testForState(res, "aaxMatrix64Inverse");

            res = aaxSensorSetMatrix64(config, mtx64);
            testForState(res, "aaxSensorSetMatrix64");

            res = aaxSensorSetVelocity(config, SensorVel);
            testForState(res, "aaxSensorSetVelocity");

            /** emitter */
            emitter = aaxEmitterCreate();
            testForError(emitter, "Unable to create a new emitter\n");

            res = aaxEmitterAddBuffer(emitter, buffer);
            testForState(res, "aaxEmitterAddBuffer");

            res = aaxEmitterSetMode(emitter, AAX_POSITION, MODE);
            testForState(res, "aaxEmitterSetMode");

            res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
            testForState(res, "aaxEmitterSetLooping");

            res = aaxEmitterSetVelocity(emitter, EmitterVel);
            testForState(res, "aaxEmitterSetVelocity");

            /* distance filter */
            filter = aaxFilterCreate(config, AAX_DISTANCE_FILTER);
            testForError(filter, "Unable to create the distance filter");

            res = aaxFilterSetParam(filter, AAX_REF_DISTANCE, AAX_LINEAR, 90.0f);
            testForState(res, "aaxEmitterSetReferenceDistance");

            res = aaxFilterSetParam(filter, AAX_MAX_DISTANCE, AAX_LINEAR, 5000.0f);
            testForState(res, "aaxEmitterSetMaxDistance");

            res = aaxEmitterSetFilter(emitter, filter);
            testForState(res, "aaxScenerySetDistanceModel");
            aaxFilterDestroy(filter);

            res = aaxMatrix64SetDirection(mtx64, EmitterPos, EmitterDir);
            testForState(res, "aaxMatrix64SetDirection");

            res = aaxEmitterSetMatrix64(emitter, mtx64);
            testForState(res, "aaxEmitterSetMatrix64");

            /** audio frame */
            frame = aaxAudioFrameCreate(config);
            testForError(frame, "Unable to create a new frame");

            res = aaxAudioFrameSetMode(frame, AAX_POSITION, MODE);
            testForState(res, "aaxAudioFrameSetMode");

            res = aaxMixerRegisterAudioFrame(config, frame);
            testForState(res, "aaxMixerRegisterAudioFrame");

            res = aaxAudioFrameRegisterEmitter(frame, emitter);
            testForState(res, "aaxAudioFrameRegisterEmitter");

            res = aaxMatrix64SetDirection(mtx64, FramePos, FrameDir);
            testForState(res, "aaxMatrix64SetDirection");

            res = aaxAudioFrameSetMatrix64(frame, mtx64);
            testForState(res, "aaxAudioFrameSetMatrix64");

            res = aaxAudioFrameSetVelocity(frame, FrameVel);
            testForState(res, "aaxAudioFrameSetVelocity");

            res = aaxAudioFrameSetState(frame, AAX_PLAYING);
            testForState(res, "aaxAudioFrameSetState");

            /** schedule the emitter for playback */
            printf("Engine start\n");
            res = aaxEmitterSetState(emitter, AAX_PLAYING);
            testForState(res, "aaxEmitterStart");

            dist = INITIAL_DIST;
            while(dist > -INITIAL_DIST)
            {
                msecSleep((int)(ceilf(UPDATE_DELAY*1000.0f)));

                FramePos[0] = (-dist);
                dist -= STEP;
#if 1
                printf("dist: %5.4f\tpos (% f, % f, % f)\n",
                        _vec3dMagnitude(FramePos),
                        FramePos[0],
                        FramePos[1],
                        FramePos[2]);
#endif
                res = aaxMatrix64SetDirection(mtx64, FramePos, FrameDir);
                testForState(res, "aaxMatrix64SetDirection");

                res = aaxAudioFrameSetMatrix64(frame, mtx64);
                testForState(res, "aaxEmitterSetMatrix64");
            }

            res = aaxEmitterSetState(emitter, AAX_STOPPED);
            testForState(res, "aaxEmitterStop");

            /*
             * We need to wait until the emitter has been processed.
             * This is necessary because the sound could still be playing
             * due to distance delay.
             */
            while (aaxEmitterGetState(emitter) != AAX_PROCESSED) {
               msecSleep(100);
            }

            res = aaxAudioFrameSetState(frame, AAX_STOPPED);
            testForState(res, "aaxAudioFrameStop");

            res = aaxAudioFrameDeregisterEmitter(frame, emitter);
            testForState(res, "aaxAudioFrameDeregisterEmitter");

            res = aaxEmitterRemoveBuffer(emitter);
            testForState(res, "aaxEmitterRemoveBuffer");

            res = aaxEmitterDestroy(emitter);
            testForState(res, "aaxEmitterDestroy");

//          res = aaxBufferDestroy(buffer);
            testForState(res, "aaxBufferDestroy");

            res = aaxMixerSetState(config, AAX_STOPPED);
            testForState(res, "aaxMixerStop");

            res = aaxMixerDeregisterAudioFrame(config, frame);
            testForState(res, "aaxMixerDeregisterAudioFrame");

            res = aaxAudioFrameDestroy(frame);
            testForState(res, "aaxAudioFrameDestroyr");
        }
    }

    res = aaxDriverClose(config);
    testForState(res, "aaxDriverClose");

    res = aaxDriverDestroy(config);
    testForState(res, "aaxDriverDestroy");

    return 0;
}
