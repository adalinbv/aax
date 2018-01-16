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

#define FILE_PATH		SRC_PATH"/tictac.wav"

aaxVec3d EmitterPos = { 17.0,  0.5,  -2.0  };	// in metres (right, up, back)
aaxVec3f EmitterDir = { 0.0f,  0.0f,  1.0f };

aaxVec3d SensorPos =  {  0.0,  1.7,   0.0  };
aaxVec3f SensorAt =   {  0.0f, 0.0f, -1.0f };
aaxVec3f SensorUp =   {  0.0f, 1.0f,  0.0f };

aaxVec3d RoomPos =    { 15.0,  1.0,  -4.0  };
aaxVec3d HallwayPos = {  0.0,  1.0,  -4.0  };

aaxVec3f WorldAt =    {  0.0f, 0.0f,  1.0f };
aaxVec3f WorldUp =    {  0.0f, 1.0f,  0.0f };

aaxVec4f room_reverb =    { 8500.0f, 0.007f, 0.93f, 0.049f };
aaxVec4f hallway_reverb = {  790.0f, 0.035f, 0.89f, 0.15f  };

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
            aaxFrame room, hallway;
            aaxEmitter emitter;
            aaxFilter filter;
            aaxEffect effect;
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

            /* hallway audio frame */
            hallway = aaxAudioFrameCreate(config);
            testForError(hallway, "Unable to create a new audio frame\n");

            res= aaxMatrix64SetOrientation(mtx64, HallwayPos, WorldAt, WorldUp);
            testForState(res, "aaxAudioFrameSetOrientation: Hallway");

            res = aaxAudioFrameSetMatrix64(hallway, mtx64);
            testForState(res, "aaxAudioFrameSetMatrix64");

            res = aaxAudioFrameSetMode(hallway, AAX_POSITION, AAX_INDOOR);
            testForState(res, "aaxAudioFrameSetMode");

            effect = aaxEffectCreate(config, AAX_REVERB_EFFECT);
            testForError(effect, "aaxEffectCreate hallway");

            res = aaxEffectSetSlotParams(effect, 0, AAX_LINEAR, hallway_reverb);
            testForState(res, "aaxEffectSetSlot/0");

            res = aaxEffectSetState(effect, AAX_TRUE);
            testForState(res, "aaxEffectSetState");

            res = aaxAudioFrameSetEffect(hallway, effect);
            testForState(res, "aaxMixerSetEffect");

            res = aaxEffectDestroy(effect);
            testForState(res, "aaxEffectDestroy");

            res = aaxMixerRegisterAudioFrame(config, hallway);
            testForState(res, "aaxMixerRegisterAudioFrame: Hallway");

            /** room audio frame */
            room = aaxAudioFrameCreate(config);
            testForError(room, "Unable to create a new audio frame\n");

            res = aaxMatrix64SetOrientation(mtx64, RoomPos, WorldAt, WorldUp);
            testForState(res, "aaxAudioFrameSetOrientation: Room");

            res = aaxAudioFrameSetMatrix64(room, mtx64);
            testForState(res, "aaxAudioFrameSetMatrix64");

            res = aaxAudioFrameSetMode(room, AAX_POSITION, AAX_INDOOR);
            testForState(res, "aaxAudioFrameSetMode");

            effect = aaxEffectCreate(config, AAX_REVERB_EFFECT);
            testForError(effect, "aaxEffectCreate hallway");

            res = aaxEffectSetSlotParams(effect, 0, AAX_LINEAR, room_reverb);
            testForState(res, "aaxEffectSetSlot/0");

            res = aaxEffectSetState(effect, AAX_TRUE);
            testForState(res, "aaxEffectSetState");

            res = aaxAudioFrameSetEffect(room, effect);
            testForState(res, "aaxMixerSetEffect");

            res = aaxEffectDestroy(effect);
            testForState(res, "aaxEffectDestroy");

            res = aaxAudioFrameRegisterAudioFrame(hallway, room);
            testForState(res, "aaxAudioFrameRegisterAudioFrame: Room");

            /** schedule the audioframe for playback */
            res = aaxAudioFrameSetState(room, AAX_PLAYING);
            testForState(res, "aaxAudioFrameStart: Room");

            res = aaxAudioFrameSetState(hallway, AAX_PLAYING);
            testForState(res, "aaxAudioFrameStart: Hallway");

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
            res = aaxAudioFrameRegisterEmitter(room, emitter);
            testForState(res, "aaxMixerRegisterEmitter");

            /** schedule the emitter for playback */
            res = aaxEmitterSetState(emitter, AAX_PLAYING);
            testForState(res, "aaxEmitterStart");

            deg = 0;
            while(deg < 360)
            {
                msecSleep(50);
                deg += 1;
            }

            res = aaxAudioFrameSetState(hallway, AAX_STOPPED);
            testForState(res, "aaxAudioFrameStop: Hallway");

            res = aaxAudioFrameSetState(room, AAX_STOPPED);
            testForState(res, "aaxAudioFrameStop: Room");

            res = aaxEmitterSetState(emitter, AAX_STOPPED);
            testForState(res, "aaxEmitterStop");

            /* destruction */
            res = aaxAudioFrameDeregisterEmitter(room, emitter);
            testForState(res, "aaxMixerDeregisterEmitter");

            res = aaxEmitterDestroy(emitter);
            testForState(res, "aaxEmitterDestroy");

            res = aaxBufferDestroy(buffer);
            testForState(res, "aaxBufferDestroy");

            res = aaxAudioFrameDeregisterAudioFrame(hallway, room);
            testForState(res, "aaxMixerDeregisterAudioFrame");

            res = aaxAudioFrameDestroy(room);
            testForState(res, "aaxAudioFrameDestroy: Room");

            res = aaxMixerDeregisterAudioFrame(config, hallway);
            testForState(res, "aaxMixerDeregisterAudioFrame");

            res = aaxAudioFrameDestroy(hallway);
            testForState(res, "aaxAudioFrameDestroy: Hallway");

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
