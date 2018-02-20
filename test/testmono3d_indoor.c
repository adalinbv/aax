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

aaxVec3f WorldAt =    {  0.0f, 0.0f,  1.0f };
aaxVec3f WorldUp =    {  0.0f, 1.0f,  0.0f };

aaxVec3f EmitterDir = { 0.0f,  0.0f,  1.0f };
aaxVec3d EmitterPos = { 15.0,  0.5,   2.0  };	// in metres (right, up, back)

aaxVec3d RoomPos =    { 15.0,  1.0,  -4.0  };
aaxVec3d SectionPos = { 15.0,  1.0,  -5.0  };
aaxVec3d HallwayPos = {  0.0,  1.0,  -4.0  };

aaxVec3d SensorPos =  {  0.0,  1.7,   0.0  };
aaxVec3f SensorAt =   {  0.0f, 0.0f, -1.0f };
aaxVec3f SensorUp =   {  0.0f, 1.0f,  0.0f };

const char *room_reverb_aaxs = "<?xml version='1.0'?> \
<aeonwave> 				\
 <audioframe> 				\
  <effect type='reverb' src='inverse'> 	\
   <slot n='0'> 			\
    <param n='0'>8500.0</param> 	\
    <param n='1'>0.007</param> 		\
    <param n='2'>0.93</param> 		\
    <param n='3'>0.049</param> 		\
   </slot> 				\
   <slot n='1'>                         \
    <param n='0'>0.8</param>            \
    <param n='1'>2.1</param>            \
    <param n='2'>0.1</param>            \
    <param n='3'>1.0</param>            \
   </slot>                              \
  </effect> 				\
 </audioframe> 				\
</aeonwave>";

const char *hallway_reverb_aaxs = "<?xml version='1.0'?> \
<aeonwave> 				\
 <audioframe> 				\
  <effect type='reverb' src='inverse'> 	\
   <slot n='0'> 			\
    <param n='0'>790.0</param> 		\
    <param n='1'>0.035</param> 		\
    <param n='2'>0.89</param> 		\
    <param n='3'>0.150</param> 		\
   </slot> 				\
   <slot n='1'>                         \
    <param n='0'>2.0</param>            \
    <param n='1'>2.1</param>            \
    <param n='2'>0.1</param>            \
    <param n='3'>1.0</param>            \
   </slot>                              \
  </effect> 				\
 </audioframe> 				\
</aeonwave>";

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
        aaxBuffer buffer, reverb;

        buffer = bufferFromFile(config, infile);
        if (buffer)
        {
            aaxFrame room, section, hallway;
            aaxEmitter emitter;
            aaxFilter filter;
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

            res = aaxMixerRegisterAudioFrame(config, hallway);
            testForState(res, "aaxMixerRegisterAudioFrame: Hallway");

            // hallway reverb
            reverb = aaxBufferCreate(config, 1, 1, AAX_AAXS16S);
            testForError(reverb, "aaxBufferCreate: Hallway\n");

            res = aaxBufferSetData(reverb, hallway_reverb_aaxs);
            testForState(res, "aaxBufferSetData: Hallway");

            res = aaxAudioFrameAddBuffer(hallway, reverb);
            testForState(res, "aaxMixerAddBuffer: Hallway");

            aaxBufferDestroy(reverb);

            /** intersection frame */
            section = aaxAudioFrameCreate(config);
            testForError(section, "Unable to create a new audio frame\n");

            res = aaxMatrix64SetOrientation(mtx64, SectionPos, WorldAt, WorldUp);
            testForState(res, "aaxAudioFrameSetOrientation: Inetrsection");

            res = aaxAudioFrameSetMatrix64(section, mtx64);
            testForState(res, "aaxAudioFrameSetMatrix64");

            res = aaxAudioFrameSetMode(section, AAX_POSITION, AAX_INDOOR);
            testForState(res, "aaxAudioFrameSetMode");

            res = aaxAudioFrameRegisterAudioFrame(hallway, section);
            testForState(res, "aaxAudioFrameRegisterAudioFrame: Intersection");

            /** room audio frame */
            room = aaxAudioFrameCreate(config);
            testForError(room, "Unable to create a new audio frame\n");

            res = aaxMatrix64SetOrientation(mtx64, RoomPos, WorldAt, WorldUp);
            testForState(res, "aaxAudioFrameSetOrientation: Room");

            res = aaxAudioFrameSetMatrix64(room, mtx64);
            testForState(res, "aaxAudioFrameSetMatrix64");

            res = aaxAudioFrameSetMode(room, AAX_POSITION, AAX_INDOOR);
            testForState(res, "aaxAudioFrameSetMode");

            res = aaxAudioFrameRegisterAudioFrame(section, room);
            testForState(res, "aaxAudioFrameRegisterAudioFrame: Room");

            // room reverb
            reverb = aaxBufferCreate(config, 1, 1, AAX_AAXS16S);
            testForError(reverb, "aaxBufferCreate: Room\n");

            res = aaxBufferSetData(reverb, room_reverb_aaxs);
            testForState(res, "aaxBufferSetData: Room");

            res = aaxAudioFrameAddBuffer(room, reverb);
            testForState(res, "aaxMixerAddBuffer: Room");

            aaxBufferDestroy(reverb);

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
            set_mode(1);
            while(deg < 360)
            {
                msecSleep(50);
                deg += 1;

                if (get_key()) break;
            }
            set_mode(0);

            res = aaxAudioFrameSetState(hallway, AAX_STOPPED);
            testForState(res, "aaxAudioFrameStop: Hallway");

            res = aaxAudioFrameSetState(section, AAX_STOPPED);
            testForState(res, "aaxAudioFrameStop: Intersection");

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

            res = aaxAudioFrameDeregisterAudioFrame(section, room);
            testForState(res, "aaxAudioFrameDeregisterAudioFrame");

            res = aaxAudioFrameDestroy(room);
            testForState(res, "aaxAudioFrameDestroy: Room");

            res = aaxAudioFrameDeregisterAudioFrame(hallway, section);
            testForState(res, "aaxAudioFrameDeregisterAudioFrame");

            res = aaxAudioFrameDestroy(section);
            testForState(res, "aaxAudioFrameDestroy: Intersection");

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
