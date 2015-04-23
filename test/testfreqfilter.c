/*
 * Copyright (C) 2008-2015 by Erik Hofman.
 * Copyright (C) 2009-2015 by Adalin B.V.
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
#include <stdlib.h>	// getenv

#include <aax/aax.h>

#define	SAMPLE_FREQ		48000

void
testForError(void *p, char *s)
{
    if (p == NULL)
    {
        int err = aaxGetErrorNo();
        printf("\nError: %s\n", s);
        if (err) {
            printf("%s\n\n", aaxGetErrorString(err));
        }
        exit(-1);
    }
}

void
testForState(int res, const char *func)
{
    if (res != AAX_TRUE)
    {
        int err = aaxGetErrorNo();
        printf("%s:\t\t%i\n", func, res);
        printf("(%i) %s\n\n", err, aaxGetErrorString(err));
        exit(-1);
    }
}

#ifndef _WIN32
# include <termios.h>

# if USE_NANOSLEEP
#  include <time.h>             /* for nanosleep */
#  include <sys/time.h>         /* for struct timeval */
#  include <errno.h>
int msecSleep(unsigned long dt_ms)
   {
       static struct timespec s;
       s.tv_sec = (dt_ms/1000);
       s.tv_nsec = (dt_ms % 1000)*1000000L;
       while(nanosleep(&s,&s)==-1 && errno == EINTR)
            continue;
       return errno;
   }

# else
#  include <unistd.h>           /* usleep */
int msecSleep(unsigned long dt_ms)
   {
       return usleep(dt_ms*1000);
   }
# endif
#endif

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
        int state, no_samples;
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

        /** emitter */
        emitter = aaxEmitterCreate();
        testForError(emitter, "Unable to create a new emitter\n");

        res = aaxEmitterAddBuffer(emitter, buffer);
        testForState(res, "aaxEmitterAddBuffer");

        res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
        testForState(res, "aaxEmitterSetMode");

        /** mixer */
        res = aaxMixerSetState(config, AAX_INITIALIZED);
        testForState(res, "aaxMixerInit");

        res = aaxMixerRegisterEmitter(config, emitter);
        testForState(res, "aaxMixerRegisterEmitter");

        res = aaxMixerSetState(config, AAX_PLAYING);
        testForState(res, "aaxMixerStart");


        printf("writing white noise to: %s\n", filename);
        res = aaxEmitterSetState(emitter, AAX_PLAYING);
        testForState(res, "aaxEmitterStart");

        dt = 0.0f;
        do
        {
            dt += 0.05f;
            msecSleep(50);
            state = aaxEmitterGetState(emitter);
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
