/*
 * Copyright (C) 2008-2018 by Erik Hofman.
 * Copyright (C) 2009-2018 by Adalin B.V.
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
#include <stdlib.h>	// exit
#include <assert.h>

#include <aax/aax.h>

#include "base/timer.h"
#include <software/rbuf_int.h>

#define SAMPLE_FREQUENCY	22050


static const char *aaxs = "<aeonwave> \
 <sound ratio='3.0' frequency='440' duration='3.0'> \
  <waveform src='pure-sine' ratio='2.0'/> \
 </sound> \
</aeonwave>";

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

int main()
{
    aaxConfig config;
    int res = 0;

    config = aaxDriverOpenByName("AeonWave Loopback", AAX_MODE_WRITE_STEREO);
    testForError(config, "Loopback device is unavailable.");

    if (config)
    {
        unsigned int no_samples;
        float rms, peak, clip, asym;
        float **bufdata;
        aaxBuffer buffer;

        no_samples = (unsigned int)(1.0f*SAMPLE_FREQUENCY);
        buffer = aaxBufferCreate(config, no_samples, 1, AAX_AAXS16S);
        testForError(buffer, "Unable to generate buffer\n");

        res = aaxBufferSetSetup(buffer, AAX_FREQUENCY, SAMPLE_FREQUENCY);
        testForState(res, "aaxBufferSetFrequency");

        res = aaxBufferSetData(buffer, aaxs);
        testForError(buffer, "Unable to generate the buffer\n");

        res = aaxBufferSetSetup(buffer, AAX_FORMAT, AAX_FLOAT);
        testForState(res, "Unable to set the buffer format");

        bufdata = (float**)aaxBufferGetData(buffer);
        testForError(bufdata, "Could not get the buffer data");

        no_samples = aaxBufferGetSetup(buffer, AAX_NO_SAMPLES);

        // Buffer peak and RMS
        _batch_get_average_rms(*bufdata, no_samples, &rms, &peak);
        printf("\trms: %4.3f, peak: %4.3f\n", rms, peak);

        // atan peak and RMS
        _batch_atanps(*bufdata, *bufdata, no_samples);
        _batch_get_average_rms(*bufdata, no_samples, &rms, &peak);
        printf("atan\trms: %4.3f, peak: %4.3f\n", rms, peak);
        aaxFree(bufdata);

        // Limiter peak and RMS
        bufdata = (float**)aaxBufferGetData(buffer);
        testForError(bufdata, "Could not get the buffer data");

        clip = 1.0;
        asym = 0.0f;
        _aaxRingBufferLimiter(*bufdata, no_samples, clip, asym);
        _batch_get_average_rms(*bufdata, no_samples, &rms, &peak);
        printf("limiter\trms: %4.3f, peak: %4.3f\n", rms, peak);
        aaxFree(bufdata);

#if 0
        res = aaxBufferSetSetup(buffer, AAX_FORMAT, AAX_FLOAT);
        testForState(res, "Unable to set the buffer format");

        bufdata = (float**)aaxBufferGetData(buffer);
        testForError(bufdata, "Could not get the buffer data");

        clip = 0.0;
        asym = 0.0f;
//      _aaxRingBufferLimiter(*bufdata, no_samples, clip, asym);
//      _aaxRingBufferCompress(*bufdata, no_samples, clip, asym);
        _aaxFileDriverWrite("/tmp/test.wav", AAX_OVERWRITE, *bufdata,
                            no_samples, SAMPLE_FREQUENCY, 1, AAX_FLOAT);
        aaxFree(bufdata);
#endif

        res = aaxBufferDestroy(buffer);
        testForState(res, "aaxBufferDestroy");
    }

    res = aaxDriverClose(config);
    res = aaxDriverDestroy(config);

    return res;
}
