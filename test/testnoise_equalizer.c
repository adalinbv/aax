/*
 * Copyright (C) 2008-2016 by Erik Hofman.
 * Copyright (C) 2009-2016 by Adalin B.V.
 * All rights reserved.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>	// getenv
#include <errno.h>	// EINTR

#include <aax/aax.h>

#include "base/timer.h"
#include "driver.h"

#define	SAMPLE_FREQ		48000

#define LF_FILTER_FREQUENCY	 100
#define LF_GAIN 		0.0f
#define LF_Q			5.0f

#define MF_GAIN			1.0f

#define HF_FILTER_FREQUENCY	1000
#define HF_GAIN			0.2f
#define HF_Q			5.0f


int main(int argc, char **argv)
{
    char *tmp, devname[128], filename[64];
    aaxConfig config;
    int res = 0;

    tmp = getenv("TEMP");
    if (!tmp) tmp = getenv("TMP");
    if (!tmp) tmp = "/tmp";

    snprintf(filename, 64, "%s/whitenoise.wav", tmp);
    snprintf(devname, 128, "AeonWave on Audio Files: %s", filename);

    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    testForError(config, "No default audio device available.");

    if (config)
    {
        int no_samples; // state
        aaxEmitter emitter;
        aaxBuffer buffer;
        aaxFilter filter;
        float dt;

        no_samples = (unsigned int)(4*SAMPLE_FREQ);
        buffer = aaxBufferCreate(config, no_samples, 1, AAX_PCM16S);
        testForError(buffer, "Unable to generate buffer\n");

        res = aaxBufferSetSetup(buffer, AAX_FREQUENCY, SAMPLE_FREQ);
        testForState(res, "aaxBufferSetFrequency");

        res = aaxBufferProcessWaveform(buffer, 0.0f, AAX_WHITE_NOISE, 1.0f,
                                       AAX_OVERWRITE);
        testForState(res, "aaxBufferProcessWaveform");

        /** mixer */
        res = aaxMixerSetState(config, AAX_INITIALIZED);
        testForState(res, "aaxMixerInit");

        res = aaxMixerSetState(config, AAX_PLAYING);
        testForState(res, "aaxMixerStart");

        /** emitter */
        emitter = aaxEmitterCreate();
        testForError(emitter, "Unable to create a new emitter\n");

        res = aaxEmitterAddBuffer(emitter, buffer);
        testForState(res, "aaxEmitterAddBuffer");

        res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
        testForState(res, "aaxEmitterSetMode");

        /* equalizer */
        filter = aaxFilterCreate(config, AAX_EQUALIZER);
        testForError(filter, "aaxFilterCreate");

        res = aaxFilterSetSlot(filter, 0, AAX_LINEAR, LF_FILTER_FREQUENCY,
                                             LF_GAIN, MF_GAIN, LF_Q);
        testForState(res, "aaxFilterSetSlot 0");

        res = aaxFilterSetSlot(filter, 1, AAX_LINEAR, HF_FILTER_FREQUENCY,
                                             MF_GAIN , HF_GAIN, HF_Q);
        testForState(res, "aaxFilterSetSlot 1");

        res = aaxFilterSetState(filter, AAX_TRUE);
        testForState(res, "aaxFilterSetState");

        res = aaxMixerSetFilter(config, filter);
        testForState(res, "aaxMixerSetFilter");

        res = aaxFilterDestroy(filter);

        res = aaxMixerRegisterEmitter(config, emitter);
        testForState(res, "aaxMixerRegisterEmitter");

        printf("writing white noise to: %s\n", filename);
        res = aaxEmitterSetState(emitter, AAX_PLAYING);
        testForState(res, "aaxEmitterStart");

        dt = 0.0f;
        do
        {
            dt += 0.05f;
            msecSleep(50);
//          state = aaxEmitterGetState(emitter);
        }
        while (dt < 1.0f); // state == AAX_PLAYING);

        res = aaxEmitterSetState(emitter, AAX_PROCESSED);
        testForState(res, "aaxEmitterStop");

        res = aaxEmitterRemoveBuffer(emitter);
        testForState(res, "aaxEmitterRemoveBuffer");

        res = aaxBufferDestroy(buffer);
        testForState(res, "aaxBufferDestroy");

        res = aaxMixerDeregisterEmitter(config, emitter);
        res = aaxMixerSetState(config, AAX_STOPPED);
        res = aaxEmitterDestroy(emitter);
    }

    res = aaxDriverClose(config);
    res = aaxDriverDestroy(config);

    return res;
}
