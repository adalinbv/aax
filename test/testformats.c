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

#define IMA4_BLOCKSIZE		36
#define FILE_PATH		SRC_PATH"/tictac.wav"

#define MAX_LOOPS		6
static int _mask_t[MAX_LOOPS] = {
    0,
    AAX_FORMAT_UNSIGNED,
    AAX_FORMAT_LE,
    AAX_FORMAT_LE_UNSIGNED,
    AAX_FORMAT_BE,
    AAX_FORMAT_BE_UNSIGNED
};

static const char* _format_s[] = {
    "AAX_PCM8S:  signed, 8-bits per sample",
    "AAX_PCM16S: signed, 16-bits per sample",
    "AAX_PCM24S: signed, 24-bits per sample, 32-bit encoded",
    "AAX_PCM32S: signed, 32-bits per sample",
    "AAX_FLOAT:  32-bit floating point, -1.0f to 1.0f",
    "AAX_DOUBLE: 64-bit floating point, -1.0f to 1.0f",
    "AAX_MULAW",
    "AAX_ALAW",
    "AAX_IMA4_ADPCM"
};

static const char* _format_us[] = {
    "AAX_PCM8U:  unsigned, 8-bits per sample",
    "AAX_PCM16U: unsigned, 16-bits per sample",
    "AAX_PCM24U: unsigned, 24-bits per sample, 32-bit encoded",
    "AAX_PCM32U: unsigned, 32-bits per sample",
};

static const char* _mask_s[MAX_LOOPS] = {
    "Native format",
    "Unigned format",
    "Little endian, signed",
    "Little endian, unsigned",
    "Big endian, signed",
    "Big endian, unsigned"
};

int main(int argc, char **argv)
{
    char *devname, *infile;
    aaxConfig config;
    int res;

    devname = getDeviceName(argc, argv);
    infile = getInputFile(argc, argv, FILE_PATH);
    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    testForError(config, "No default audio device available.");

    if (config)
    {
        aaxBuffer buf = bufferFromFile(config, infile);
        if (buf)
        {
            int no_samples, no_tracks, freq;
            aaxEmitter emitter;
            int q, state, fmt;

            /** emitter */
            emitter = aaxEmitterCreate();
            testForError(emitter, "Unable to create a new emitter");

            res = aaxEmitterAddBuffer(emitter, buf);
            testForState(res, "aaxEmitterAddBuffer");
            printf("original format\n");

            /** mixer */
            res = aaxMixerInit(config);
            testForState(res, "aaxMixerInit");

            res = aaxMixerRegisterEmitter(config, emitter);
            testForState(res, "aaxMixerRegisterEmitter");

            res = aaxMixerSetState(config, AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            no_samples = aaxBufferGetSetup(buf, AAX_NO_SAMPLES);
            no_tracks = aaxBufferGetSetup(buf, AAX_TRACKS);
            freq = 0.6f*(float)aaxBufferGetSetup(buf, AAX_FREQUENCY);

            res = aaxBufferSetSetup(buf, AAX_FREQUENCY, freq);
            no_samples = aaxBufferGetSetup(buf, AAX_NO_SAMPLES);
            for (q=0; q<MAX_LOOPS; q++)
            {
                fmt = 0;
                printf("%s\t\t(%x)\n", _mask_s[q], fmt + _mask_t[q]);
                do
                {
                    int blocksz, nfmt = fmt + _mask_t[q];
                    aaxBuffer buffer;
                    void** data;

                    if (q)
                    {
                        if (q > 1 && fmt == 0) {
                            continue;
                        }

                        if (fmt >= AAX_FLOAT && nfmt & AAX_FORMAT_UNSIGNED) {
                            break;
                        }
                        if (fmt >= AAX_MULAW) {
                            break;
                        }
                    }

                    /** schedule the emitter for playback */
                    res = aaxEmitterSetState(emitter, AAX_PLAYING);
                    testForState(res, "aaxEmitterStart");
            
                    /** simultaniously convert the buffer format */
                    res = aaxBufferSetSetup(buf, AAX_FORMAT, nfmt);
                    testForState(res, "aaxBufferSetup(AAX_FORMAT)");

                    blocksz = aaxBufferGetSetup(buf, AAX_BLOCK_ALIGNMENT);
#if 0
{
char s[256];	
snprintf(s, 256,"/tmp/%s.wav", (nfmt & AAX_FORMAT_UNSIGNED)
                                       ? _format_us[fmt & AAX_FORMAT_NATIVE]
                                       : _format_s[fmt & AAX_FORMAT_NATIVE]);
aaxBufferWriteToFile(buf, s, AAX_OVERWRITE);
	// NOTE: aaxBufferWriteToFile also uses data = aaxBufferGetData(buf);
        //       and it does work properly.
}
#endif
                    data = aaxBufferGetData(buf);
                    testForError(data, "aaxBufferGetData");

                    buffer=aaxBufferCreate(config, no_samples, no_tracks, nfmt);
                    testForError(buffer, "aaxBufferCreate");

                    res = aaxBufferSetSetup(buffer, AAX_FREQUENCY, freq);
                    testForState(res, "aaxBufferSetSetup(AAX_FREQUENCY)");

                    res=aaxBufferSetSetup(buffer, AAX_BLOCK_ALIGNMENT, blocksz);
                    testForState(res, "aaxBufferSetSetup(AAX_BLOCK_ALIGNMENT)");

                    res = aaxBufferSetData(buffer, *data);
                    testForState(res, "aaxBufferSetData");
                    aaxFree(data);
#if 0
{
char s[256];
snprintf(s, 256,"/tmp/%s.wav", (nfmt & AAX_FORMAT_UNSIGNED)
                                       ? _format_us[fmt & AAX_FORMAT_NATIVE]
                                       : _format_s[fmt & AAX_FORMAT_NATIVE]);
aaxBufferWriteToFile(buffer, s, AAX_OVERWRITE);
        // NOTE: aaxBufferWriteToFile also uses data = aaxBufferGetData(buf);
        //       and it does work properly.
}
#endif


                    res = aaxEmitterAddBuffer(emitter, buffer);
                    testForState(res, "aaxEmitterAddBuffer");

                    printf("  0x%03x:  %s\n", nfmt, (nfmt & AAX_FORMAT_UNSIGNED)
                                       ? _format_us[fmt & AAX_FORMAT_NATIVE]
                                       : _format_s[fmt & AAX_FORMAT_NATIVE]);
                    do
                    {
                        msecSleep(50);
                        state = aaxEmitterGetNoBuffers(emitter, AAX_PROCESSED);
                    }
                    while (state == 0);

                    res = aaxEmitterRemoveBuffer(emitter);
                    testForState(res, "aaxEmitterRemoveBuffer");

                    res = aaxBufferDestroy(buffer);
                }
                while (++fmt < AAX_FORMAT_MAX);
            }

            res = aaxMixerDeregisterEmitter(config, emitter);
            res = aaxMixerSetState(config, AAX_STOPPED);
            res = aaxEmitterDestroy(emitter);
            res = aaxBufferDestroy(buf);
        }
    }

    res = aaxDriverClose(config);
    res = aaxDriverDestroy(config);

    return 0;
}
