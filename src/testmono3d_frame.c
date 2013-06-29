/*
 * Copyright (C) 2008-2012 by Erik Hofman.
 * Copyright (C) 2009-2012 by Adalin B.V.
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

#include "base/geometry.h"
#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define FRAMES			2
#define SUBFRAMES		8
#define RADIUS			15
#define FILE_PATH		SRC_PATH"/tictac.wav"

aaxVec3f EmitterPos = { 0.0f, 0.0f, 0.0f };
aaxVec3f EmitterDir = { 0.0f, 0.0f, 1.0f };
aaxVec3f EmitterVel = { 0.0f, 0.0f, 0.0f };

aaxVec3f FramePos = { 0.0f, 0.0f, 0.0f };
aaxVec3f FrameAt = {  0.0f, 0.0f, 1.0f };
aaxVec3f FrameUp = {  0.0f, 1.0f, 0.0f };
aaxVec3f FrameVel = { 0.0f, 0.0f, 0.0f };


aaxVec3f SensorPos = { 10000.0f, -1000.0f,  0.0f };
aaxVec3f SensorAt = {      0.0f,     0.0f, -1.0f };
aaxVec3f SensorUp = {      0.0f,     1.0f,  0.0f };
aaxVec3f SensorVel = {     0.0f,     0.0f,  0.0f };

int main(int argc, char **argv)
{
    enum aaxRenderMode mode;
    char *devname, *infile;
    aaxConfig config;
    int num, res;
    int rv = 0;

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
        aaxFrame subframe[FRAMES][SUBFRAMES];
        aaxFrame frame[FRAMES];
        aaxBuffer buffer;

        buffer = bufferFromFile(config, infile);
        if (buffer)
        {
            float pitch, anglestep;
            aaxEmitter *emitter;
            int i, j, deg = 0;
            aaxMtx4f mtx;

            num = ((getNumEmitters(argc, argv)-1)/(FRAMES*SUBFRAMES))+1;
            emitter = calloc(num*SUBFRAMES*FRAMES, sizeof(aaxEmitter));

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


            /** audio frame */
            pitch = getPitch(argc, argv);
            printf("starting %i frames with %i subframes ", FRAMES, SUBFRAMES);
            printf("and %i emitters per subframe\nfor a total of %i "
                   "emitters\n", num, num*SUBFRAMES*FRAMES);

            res = aaxMatrixSetOrientation(mtx, FramePos, FrameAt, FrameUp);
            testForState(res, "aaxAudioFrameSetOrientation");

            for (j=0; j<FRAMES; j++)
            {
                int k;

                frame[j] = aaxAudioFrameCreate(config);
                testForError(frame[j], "Unable to create a new audio frame\n");

                res = aaxAudioFrameSetMatrix(frame[j], mtx);
                testForState(res, "aaxAudioFrameSetMatrix");

                res=aaxAudioFrameSetMode(frame[j], AAX_POSITION, AAX_RELATIVE);
                testForState(res, "aaxAudioFrameSetMode");

                res = aaxAudioFrameSetVelocity(frame[j], FrameVel);
                testForState(res, "aaxAudioFrameSetVelocity");

                /** register audio frame */
                res = aaxMixerRegisterAudioFrame(config, frame[j]);
                testForState(res, "aaxMixerRegisterAudioFrame");

                /** schedule the audioframe for playback */
                res = aaxAudioFrameSetState(frame[j], AAX_PLAYING);
                testForState(res, "aaxAudioFrameStart");

                for (k=0; k<SUBFRAMES; k++)
                {
                    subframe[j][k] = aaxAudioFrameCreate(config);
                    testForError(subframe[j][k],
                                 "Unable to create a new sub-frame\n");

                    res = aaxAudioFrameSetMatrix(subframe[j][k], mtx);
                    testForState(res, "aaxAudioFrameSetMatrix");

                    res = aaxAudioFrameSetMode(subframe[j][k], AAX_POSITION,
                                                               AAX_RELATIVE);
                    testForState(res, "aaxAudioFrameSetMode");

                    res = aaxAudioFrameSetVelocity(subframe[j][k], FrameVel);
                    testForState(res, "aaxAudioFrameSetVelocity");

                    /** register audio frame */
                    res = aaxAudioFrameRegisterAudioFrame(frame[j],
                                                          subframe[j][k]);
                    testForState(res, "aaxAudioFrameRegisterAudioFrame");

                    /** schedule the audioframe for playback */
                    res = aaxAudioFrameSetState(subframe[j][k], AAX_PLAYING);
                    testForState(res, "aaxAudioFrameStart");

                    /** emitter */
                    /* Set emitters to located in a circle around the sensor */
                    anglestep = (2 * GMATH_PI) / (float)(FRAMES*SUBFRAMES*num);
                    printf("Starting %i emitters\n", num);
                    i = 0;
                    do
                    {
                        unsigned int p = (j*SUBFRAMES+k)*num + i;
                        static float mul = 1.0f;
                        aaxVec3f pos;

                        emitter[p] = aaxEmitterCreate();
                        testForError(emitter[p],
                                     "Unable to create a new emitter");

                        res = aaxEmitterAddBuffer(emitter[p], buffer);
                        testForState(res, "aaxEmitterAddBuffer");

                        pos[1] = EmitterPos[1] + cosf(anglestep * p)*RADIUS;
                        pos[0] = EmitterPos[0] + mul*cosf(anglestep * p)*RADIUS;
                        pos[2] = EmitterPos[2] + sinf(anglestep * p)*RADIUS;
                        aaxMatrixSetDirection(mtx, pos, EmitterDir);
                        res = aaxEmitterSetMatrix(emitter[p], mtx);
                        testForState(res, "aaxEmitterSetIdentityMatrix");
                        mul *= -1.0f;

                        res = aaxEmitterSetMode(emitter[p], AAX_POSITION,
                                                            AAX_ABSOLUTE);
                        testForState(res, "aaxEmitterSetMode");

                        res=aaxEmitterSetMode(emitter[p],AAX_LOOPING,AAX_TRUE);
                        testForState(res, "aaxEmitterSetLooping");

                        res = aaxEmitterSetReferenceDistance(emitter[p], 6.3f);
                        testForState(res, "aaxEmitterSetReferenceDistance");

                        res = aaxEmitterSetPitch(emitter[p], pitch);
                        testForState(res, "aaxEmitterSetPitch");

                        /** register the emitter to the audio-frame */
                        res = aaxAudioFrameRegisterEmitter(subframe[j][k],
                                                           emitter[p]);
                        testForState(res, "aaxMixerRegisterEmitter");

                        /** schedule the emitter for playback */
                        res = aaxEmitterSetState(emitter[p], AAX_PLAYING);
                        testForState(res, "aaxEmitterStart");

                        msecSleep(750);
                    }
                    while (++i < num);

                    msecSleep(50);
                }
            }

            deg = 0;
            while(deg < 360)
            {
                msecSleep(50);
                deg += 1;
            }

            for (j=0; j<FRAMES; j++)
            {
                int k;

                res = aaxAudioFrameSetState(frame[j], AAX_STOPPED);
                testForState(res, "aaxAudioFrameStop");

                for (k=0; k<SUBFRAMES; k++)
                {
                    res = aaxAudioFrameSetState(subframe[j][k], AAX_STOPPED);
                    testForState(res, "aaxAudioFrameStop");            

                    for(i=0; i<num; i++)
                    {
                        unsigned int p = (j*SUBFRAMES+k)*num + i;

                        res = aaxEmitterSetState(emitter[p], AAX_STOPPED);
                        testForState(res, "aaxEmitterStop");

                        res = aaxAudioFrameDeregisterEmitter(subframe[j][k],
                                                             emitter[p]);
                        testForState(res, "aaxMixerDeregisterEmitter");

                        res = aaxEmitterDestroy(emitter[p]);
                        testForState(res, "aaxEmitterDestroy");
                    }

                    res = aaxAudioFrameDeregisterAudioFrame(frame[j],
                                                            subframe[j][k]);
                    testForState(res, "aaxMixerDeregisterAudioFrame");

                    res = aaxAudioFrameDestroy(subframe[j][k]);
                    testForState(res, "aaxAudioFrameStop");
                }

                res = aaxMixerDeregisterAudioFrame(config, frame[j]);
                testForState(res, "aaxMixerDeregisterAudioFrame");

                res = aaxAudioFrameDestroy(frame[j]);
                testForState(res, "aaxAudioFrameStop");
            }

            free(emitter);

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

    return rv;
}
