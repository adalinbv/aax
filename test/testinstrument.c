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

#include <aax/aax.h>

#include <base/geometry.h>
#include <base/types.h>
#include "driver.h"
#include "wavfile.h"

#define SAMPLE_FREQUENCY		22050

static const char* aaxs_data_sax =   // A2, 200Hz
" <aeonwave>					\
    <sound frequency=\"440\">			\
      <waveform src=\"triangle\"/>		\
      <waveform src=\"triangle\" processing=\"mix\" pitch=\"2.0\" ratio=\"0.333\"/>		\
      <waveform src=\"triangle\" processing=\"add\" pitch=\"4.0\" ratio=\"-0.2\"/>		\
    </sound>					\
    <emitter>					\
      <filter type=\"envelope\">		\
       <slot n=\"0\">				\
        <p1>0.01</p1>				\
        <p2>1.2</p2>				\
        <p3>0.05</p3>				\
       </slot>					\
       <slot n=\"1\">				\
         <p0>0.7</p0>				\
         <p1>0.1</p1>				\
         <p2>0.6</p2>				\
         <p3>0.05</p3>				\
       </slot>					\
       <slot n=\"2\">				\
         <p0>0.45</p0>				\
         <p1>1.2</p1>				\
       </slot>					\
      </filter>					\
    </emitter>					\
  </aeonwave>";

int main(int argc, char **argv)
{
    aaxConfig config;
    int state, res;
    char *devname;
    int rv = 0;

    devname = getDeviceName(argc, argv);
    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    testForError(config, "No default audio device available.");

    if (config && (rv >= 0))
    {
        unsigned int no_samples;
        aaxEmitter emitter;
        aaxBuffer buffer;
        aaxEffect effect;
        int i;

        no_samples = (unsigned int)(1.0f*SAMPLE_FREQUENCY);
        buffer = aaxBufferCreate(config, no_samples, 1, AAX_AAXS16S);
        testForError(buffer, "Unable to generate buffer\n");

        res = aaxBufferSetSetup(buffer, AAX_FREQUENCY, SAMPLE_FREQUENCY);
        testForState(res, "aaxBufferSetFrequency");

        res = aaxBufferSetData(buffer, aaxs_data_sax);
        testForState(res, "aaxBufferSetData");

        /** emitter */
        emitter = aaxEmitterCreate();
        testForError(emitter, "Unable to create a new emitter\n");

        res = aaxEmitterAddBuffer(emitter, buffer);
        testForState(res, "aaxEmitterAddBuffer");

        res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
        testForState(res, "aaxEmitterSetLooping");


        /** mixer */
        res = aaxMixerSetState(config, AAX_INITIALIZED);
        testForState(res, "aaxMixerInit");

        res = aaxMixerRegisterEmitter(config, emitter);
        testForState(res, "aaxMixerRegisterEmitter");

        res = aaxMixerSetState(config, AAX_PLAYING);
        testForState(res, "aaxMixerStart");

        /** schedule the emitter for playback */
        res = aaxEmitterSetState(emitter, AAX_PLAYING);
        testForState(res, "aaxEmitterStart");

        i = 0;
        do
        {
            msecSleep(50);
            if (i == 100) break;

            if (i == 10) 
            {
                res = aaxEmitterSetState(emitter, AAX_STOPPED);
                res = aaxEmitterSetState(emitter, AAX_INITIALIZED); // rewind

                effect = aaxEffectCreate(config, AAX_PITCH_EFFECT);
                testForError(effect, "Unable to create the pitch effect");

                // G2
                res = aaxEffectSetParam(effect, AAX_PITCH, AAX_LINEAR, 0.8909f);
                testForState(res, "aaxEffectSetParam");

                res = aaxEmitterSetEffect(emitter, effect);
                testForState(res, "aaxEmitterSetPitch");
                aaxEffectDestroy(effect);

                res = aaxEmitterSetState(emitter, AAX_PLAYING);
            }
            else if (i == 27)
            {
                res = aaxEmitterSetState(emitter, AAX_STOPPED);
                res = aaxEmitterSetState(emitter, AAX_INITIALIZED); // rewind

                effect = aaxEffectCreate(config, AAX_PITCH_EFFECT);
                testForError(effect, "Unable to create the pitch effect");

                // C1
                res = aaxEffectSetParam(effect, AAX_PITCH, AAX_LINEAR, 0.2973f);
                testForState(res, "aaxEffectSetParam");

                res = aaxEmitterSetEffect(emitter, effect);
                testForState(res, "aaxEmitterSetPitch");
                aaxEffectDestroy(effect);

                res = aaxEmitterSetState(emitter, AAX_PLAYING);
            }
            i++;

            state = aaxEmitterGetState(emitter);
        }
        while (state == AAX_PLAYING);

        res = aaxEmitterSetState(emitter, AAX_STOPPED);
        do
        {
            msecSleep(50);
            state = aaxEmitterGetState(emitter);
        }
        while (state == AAX_PLAYING);


        res = aaxEmitterSetState(emitter, AAX_STOPPED);
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

    return rv;
}
