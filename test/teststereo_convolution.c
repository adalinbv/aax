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

#include <aax/defines.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define REFRATE			46
#define FILE_PATH		SRC_PATH"/stereo.wav"

// From: http://www.cksde.com/p_6_250.htm
#define IRFILE_PATH		SRC_PATH"/convolution.wav"

int main(int argc, char **argv)
{
    char *devname, *infile, *convfile;
    aaxBuffer irbuffer = 0;
    aaxBuffer buffer = 0;
    aaxConfig config;
    int res;

    devname = getDeviceName(argc, argv);
    infile = getInputFile(argc, argv, FILE_PATH);
    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    testForError(config, "No default audio device available.");

    convfile = getCaptureName(argc, argv);
    if (!convfile) convfile = IRFILE_PATH;
    irbuffer = bufferFromFile(config, convfile);
    testForError(config, "convolution file not found");

    if (config)
    {
        buffer = bufferFromFile(config, infile);
        if (buffer)
        {
            aaxEmitter emitter;
            aaxEffect effect;
            float dt = 0.0f;
            int q, state;
            float pitch;

            /** emitter */
            emitter = aaxEmitterCreate();
            testForError(emitter, "Unable to create a new emitter");

            pitch = getPitch(argc, argv);
            res = aaxEmitterSetPitch(emitter, pitch);
            testForState(res, "aaxEmitterSetPitch");

            res = aaxEmitterAddBuffer(emitter, buffer);
            testForState(res, "aaxEmitterAddBuffer");

            /** mixer */
            res = aaxMixerInit(config);
            testForState(res, "aaxMixerInit");

            res = aaxMixerSetSetup(config, AAX_REFRESHRATE, REFRATE);
            testForState(res, "aaxMixerSetSetup");

            printf("refresh rate: %i\n", aaxMixerGetSetup(config, AAX_REFRESHRATE));

            res = aaxMixerRegisterEmitter(config, emitter);
            testForState(res, "aaxMixerRegisterEmitter");

            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /* convolution effect */
            effect = aaxEffectCreate(config, AAX_CONVOLUTION_EFFECT);
            testForError(effect, "aaxEffectCreate");

            res = aaxEffectSetParam(effect, AAX_CUTOFF_FREQUENCY, AAX_LINEAR, 5000.0f);
            testForState(res, "aaxEffectSetParam");

            res = aaxEffectSetParam(effect, AAX_LF_GAIN, AAX_LINEAR, 1.0f);
            testForState(res, "aaxEffectSetParam");

            res = aaxEffectSetParam(effect, AAX_MAX_GAIN, AAX_LINEAR, 1.0f);
            testForState(res, "aaxEffectSetParam");

            res = aaxEffectSetParam(effect, AAX_THRESHOLD, AAX_LOGARITHMIC, -64.0f);
            testForState(res, "aaxEffectSetParam");

            res = aaxEffectAddBuffer(effect, irbuffer);
            testForState(res, "aaxEffectAddBuffer");

            effect = aaxEffectSetState(effect, AAX_TRUE);
            testForError(effect, "aaxEffectSetState");

            res = aaxMixerSetEffect(config, effect);
            testForState(res, "aaxMixerSetEffect");

            res = aaxEffectDestroy(effect);
            testForState(res, "aaxEffectDestroy");

            /** schedule the emitter for playback */
            res = aaxEmitterSetState(emitter, AAX_PLAYING);
            testForState(res, "aaxEmitterStart");

            q = 0;
            do
            {
                msecSleep(50);
                dt += 0.05f;

                if (++q > 10)
                {
                    unsigned long offs, offs_bytes;
                    float off_s;
                    q = 0;

                    off_s = aaxEmitterGetOffsetSec(emitter);
                    offs = aaxEmitterGetOffset(emitter, AAX_SAMPLES);
                    offs_bytes = aaxEmitterGetOffset(emitter, AAX_BYTES);
                    printf("playing time: %5.2f, buffer position: %5.2f "
                           "(%li samples/ %li bytes)\n", dt, off_s,
                           offs, offs_bytes);
                }
                state = aaxEmitterGetState(emitter);
            }
            while (state == AAX_PLAYING);

            res = aaxMixerDeregisterEmitter(config, emitter);
            res = aaxMixerSetState(config, AAX_STOPPED);
            res = aaxEmitterDestroy(emitter);
        }
    }

    aaxBufferDestroy(irbuffer);
    aaxBufferDestroy(buffer);

    res = aaxDriverClose(config);
    res = aaxDriverDestroy(config);

    return 0;
}
