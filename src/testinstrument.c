/*
 * Copyright (C) 2008-2014 by Erik Hofman.
 * Copyright (C) 2009-2014 by Adalin B.V.
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

#include <base/geometry.h>
#include <base/types.h>
#include "driver.h"
#include "wavfile.h"

#define SAX	1
#define ENABLE_TIMED_GAIN_FILTER	1
#define ENABLE_TIMED_PITCH_EFFECT	SAX
#define ENABLE_EMITTER_DYNAMIC_GAIN	0
#define ENABLE_EMITTER_DYNAMIC_PITCH	0
#define ENABLE_MIXER_DYNAMIC_GAIN	0
#define SAMPLE_FREQUENCY		22050

static const char* aaxs_data_sax =   // A2, 200Hz
#if SAX
#if 1
"    <sound frequency=\"220\">			\
       <waveform src=\"sawtooth\"/>		\
       <waveform src=\"sine\">			\
         <processing>mix</processing>		\
         <pitch>3.535</pitch>			\
         <ratio>-0.4</ratio>			\
       </waveform>				\
     </sound>";
#else
"    <sound frequency=\"23\">                    \
       <waveform src=\"brownian-noise\">        \
         <pitch>0.3</pitch>                    \
         <ratio>0.73</ratio>                    \
         <staticity>0.1</staticity> 		\
       </waveform>                              \
       <waveform src=\"sawtooth\">		\
         <processing>mix</processing>		\
         <ratio>0.73</ratio>			\
       </waveform>				\
       <waveform src=\"sine\">                  \
         <processing>modulate</processing>      \
         <pitch>2.9</pitch>			\
         <ratio>1.0</ratio>			\
       </waveform>                              \
     </sound>";
#endif
#else
"    <sound frequency=\"110\">			\
      <waveform src=\"triangle\"/>		\
      <waveform src=\"sine\">			\
        <processing>mix</processing>		\
        <pitch>4.0</pitch>			\
        <ratio>-0.2</ratio>			\
      </waveform>				\
      <waveform src=\"triangle\">		\
        <processing>mix</processing>		\
        <pitch>2.0</pitch>			\
        <ratio>0.333</ratio>>			\
      </waveform>				\
    </sound>";
#endif

int main(int argc, char **argv)
{
    aaxConfig config;
    int state, res;
    char *devname;
    int rv = 0;

    devname = getDeviceName(argc, argv);
    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
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
        unsigned int no_samples;
        aaxEmitter emitter;
        aaxBuffer buffer;
        aaxFilter filter;
        aaxEffect effect;
        int i;

        no_samples = (unsigned int)(1.0f*SAMPLE_FREQUENCY);
        buffer = aaxBufferCreate(config, no_samples, 1, AAX_AAXS16S);
        testForError(buffer, "Unable to generate buffer\n");

        res = aaxBufferSetFrequency(buffer, SAMPLE_FREQUENCY);
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

#if ENABLE_TIMED_GAIN_FILTER
	/* time filter for emitter */
        filter = aaxFilterCreate(config, AAX_TIMED_GAIN_FILTER);
        testForError(filter, "aaxFilterCreate");

#if SAX
        filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR,
                                          0.0f, 0.05f, 1.0f, 0.05f);
        testForError(filter, "aaxFilterSetSlot 0");
        filter = aaxFilterSetSlot(filter, 1, AAX_LINEAR,
                                          0.9f, 9.0f, 0.8f, 0.2f);
        testForError(filter, "aaxFilterSetSlot 1");
        filter = aaxFilterSetSlot(filter, 2, AAX_LINEAR,
                                          0.0f, 0.0f, 0.0f, 0.0f);
        testForError(filter, "aaxFilterSetSlot 2");
#else
        filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR,
                                          0.0f, 0.01f, 1.0f, 0.05f);
        testForError(filter, "aaxFilterSetSlot 0");
        filter = aaxFilterSetSlot(filter, 1, AAX_LINEAR,
                                          0.7f, 0.1f, 0.6f, 0.05f);
        testForError(filter, "aaxFilterSetSlot 1");
        filter = aaxFilterSetSlot(filter, 2, AAX_LINEAR,
                                          0.45f, 1.5f, 0.0f, 0.0f);
        testForError(filter, "aaxFilterSetSlot 2");
#endif

        filter = aaxFilterSetState(filter, AAX_TRUE);
        testForError(filter, "aaxFilterSetState");

        res = aaxEmitterSetFilter(emitter, filter);
        testForState(res, "aaxEmitterSetFilter");

        res = aaxFilterDestroy(filter);
        testForState(res, "aaxFilterDestroy");
#endif

#if ENABLE_TIMED_PITCH_EFFECT
	/* time effect for emitter */
        effect = aaxEffectCreate(config, AAX_TIMED_PITCH_EFFECT);
        testForError(effect, "aaxFilterCreate");

        effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR,
                                              0.995f, 0.05f, 1.05f, 0.08f);
        testForError(effect, "aaxFilterSetSlot 0");
        effect = aaxEffectSetSlot(effect, 1, AAX_LINEAR,
                                              1.0f, 0.1f, 0.99f, 0.0f);
        testForError(filter, "aaxFilterSetSlot 1");
#if 0
        effect = aaxEffectSetSlot(effect, 2, AAX_LINEAR,
                                          1.05f, 0.0f, 1.0f, 0.0f);
        testForError(filter, "aaxFilterSetSlot 2");
#endif

        effect = aaxEffectSetState(effect, AAX_TRUE);
        testForError(filter, "aaxFilterSetState");

        res = aaxEmitterSetEffect(emitter, effect);
        testForState(res, "aaxEmitterSetFilter");

        res = aaxEffectDestroy(effect);
        testForState(res, "aaxFilterDestroy");
#endif

#if ENABLE_EMITTER_DYNAMIC_GAIN
	/* tremolo filter for emitter */
        filter = aaxFilterCreate(config, AAX_TREMOLO_FILTER);
        testForError(filter, "aaxFilterCreate");

        filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR,
                                              0.0f, 15.0f, 0.15f, 0.0f);
        testForError(filter, "aaxFilterSetSlot");

        filter = aaxFilterSetState(filter, AAX_SINE_WAVE);
        testForError(filter, "aaxFilterSetState");

        res = aaxEmitterSetFilter(emitter, filter);
        testForState(res, "aaxEmitterSetFilter");

        res = aaxFilterDestroy(filter);
        testForState(res, "aaxFilterDestroy");
#endif

#if ENABLE_EMITTER_DYNAMIC_PITCH
	/* vibrato effect for emitter */
        effect = aaxEffectCreate(config, AAX_VIBRATO_EFFECT);
        testForError(filter, "aaxEffectCreate");

        effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR,
                                              0.0f, 15.0f, 0.03f, 0.0f);
        testForError(filter, "aaxEffectSetSlot");

        effect = aaxEffectSetState(effect, AAX_SINE_WAVE);
        testForError(filter, "aaxEffectSetState");

        res = aaxEmitterSetEffect(emitter, effect);
        testForState(res, "aaxEmitterSetEffect");

        res = aaxEffectDestroy(effect);
        testForState(res, "aaxEffectDestroy");
#endif

        /** mixer */
        res = aaxMixerInit(config);
        testForState(res, "aaxMixerInit");

        res = aaxMixerRegisterEmitter(config, emitter);
        testForState(res, "aaxMixerRegisterEmitter");

        res = aaxMixerSetState(config, AAX_PLAYING);
        testForState(res, "aaxMixerStart");

#if ENABLE_MIXER_DYNAMIC_GAIN
        /* tremolo filter for mixer */
        filter = aaxFilterCreate(config, AAX_TREMOLO_FILTER);
        testForError(filter, "aaxFilterCreate");

        filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR,
                                              0.0f, 0.9f, 0.2f, 0.0f);
        testForError(filter, "aaxFilterSetSlot");

        filter = aaxFilterSetState(filter, AAX_TRIANGLE_WAVE);
        testForError(filter, "aaxFilterSetState");

        res = aaxMixerSetFilter(config, filter);
        testForState(res, "aaxMixerSetFilter");

        res = aaxFilterDestroy(filter);
        testForState(res, "aaxFilterDestroy");
#endif

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
                res = aaxEmitterStop(emitter);
                res = aaxEmitterRewind(emitter);
                res = aaxEmitterSetPitch(emitter, 0.8909f);	// G2
                res = aaxEmitterStart(emitter);
            }
            else if (i == 27)
            {
                res = aaxEmitterStop(emitter);
                res = aaxEmitterRewind(emitter);
                res = aaxEmitterSetPitch(emitter, 0.2973f);	// C1
                res = aaxEmitterStart(emitter);
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


        res = aaxEmitterStop(emitter);
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
