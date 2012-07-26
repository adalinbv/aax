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

#include <aaxdefs.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define	SAMPLE_FREQ		16000
#define SAMPLE_FORMAT		AAX_PCM16S
#define FILE_PATH		SRC_PATH"/stereo.wav"
#define MAX_WAVES		AAX_MAX_WAVE

static struct {
    char* name;
    float rate;
    enum aaxWaveformType type;
} buf_info[] =
{
  { "triangle wave",    440.0f, AAX_TRIANGLE_WAVE  },
  { "sine wave",        440.0f, AAX_SINE_WAVE        },
  { "square wave",        440.0f, AAX_SQUARE_WAVE    },
  { "sawtooth",            440.0f, AAX_SAWTOOTH_WAVE  },
  { "impulse",            440.0f, AAX_IMPULSE_WAVE    },
  { "white noise",        0.0f, AAX_WHITE_NOISE    },
  { "pink noise",            0.0f, AAX_PINK_NOISE    },
  { "brownian noise",    0.0f, AAX_BROWNIAN_NOISE },
  { "static white noise", 1.0f, AAX_WHITE_NOISE    }
};

aaxVec3f SourcePos = { 0.0,  0.0, -3.0 };
aaxVec3f SourceDir = { 0.0,  0.0,  1.0 };

int main(int argc, char **argv)
{
    aaxBuffer buffer[MAX_WAVES];
    int res, state, buf, i;
    aaxEmitter emitter;
    aaxConfig config;
    char *devname;
    aaxMtx4f mtx;

    devname = getDeviceName(argc, argv);

    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    testForError(config, "No default audio device available.");

    for (i=0; i<MAX_WAVES; i++)
    {
        float rate = buf_info[i].rate;
        int type = buf_info[i].type;

        buffer[i] = aaxBufferCreate(config, (unsigned int)(1.1f*SAMPLE_FREQ), 1, SAMPLE_FORMAT);
        testForError(buffer, "Unable to generate buffer\n");

        res = aaxBufferSetFrequency(buffer[i], SAMPLE_FREQ);
        testForState(res, "aaxBufferSetFrequency");

        res = aaxBufferProcessWaveform(buffer[i], rate, type, 1.0, AAX_OVERWRITE);
        testForState(res, "aaxBufferProcessWaveform");
    }

    /** emitter */
    emitter = aaxEmitterCreate();
    testForError(emitter, "Unable to create a new emitter\n");

    res = aaxEmitterSetMode(emitter, AAX_POSITION, AAX_ABSOLUTE);
    testForState(res, "aaxEmitterSetMode");

    res = aaxMatrixSetDirection(mtx, SourcePos, SourceDir);
    testForState(res, "aaxMatrixSetDirection");

    res = aaxEmitterSetMatrix(emitter, mtx);
    testForState(res, "aaxSensorSetMatrix");

    /** mixer */
    res = aaxMixerInit(config);
    testForState(res, "aaxMixerInit");

    res = aaxScenerySetDistanceModel(config, AAX_EXPONENTIAL_DISTANCE_DELAY);
    testForState(res, "aaxScenerySetDistanceModel");

    res = aaxMixerRegisterEmitter(config, emitter);
    testForState(res, "aaxMixerRegisterEmitter");

    res = aaxMixerSetState(config, AAX_PLAYING);
    testForState(res, "aaxMixerStart");

#if 1
    for (buf=0; buf<MAX_WAVES; buf++)
    {
        float dt = 0.0f;

        res = aaxEmitterAddBuffer(emitter, buffer[buf]);
        testForState(res, "aaxEmitterAddBuffer");

        res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
        testForState(res, "aaxEmitterSetMode");

        /** schedule the emitter for playback */
        res = aaxEmitterSetState(emitter, AAX_PLAYING);
        testForState(res, "aaxEmitterStart");

        printf("playing buffer #%i: %s\n", buf, buf_info[buf].name);
        do
        {
            dt += 5e-2f;
            nanoSleep(5e7);
            state = aaxEmitterGetState(emitter);
        }
        while (dt < 1.0f); // state == AAX_PLAYING);

        res = aaxEmitterSetState(emitter, AAX_STOPPED);
        testForState(res, "aaxEmitterStop");

        do
        {
            nanoSleep(5e7);
            state = aaxEmitterGetState(emitter);
        }
        while (state == AAX_PLAYING);

        res = aaxEmitterRemoveBuffer(emitter);
        testForState(res, "aaxEmitterRemoveBuffer");

        res = aaxBufferDestroy(buffer[buf]);
        testForState(res, "aaxBufferDestroy");
    }
#endif

    res = aaxMixerDeregisterEmitter(config, emitter);
    res = aaxMixerSetState(config, AAX_STOPPED);
    res = aaxEmitterDestroy(emitter);

    res = aaxDriverClose(config);
    res = aaxDriverDestroy(config);


    return 0;
}
