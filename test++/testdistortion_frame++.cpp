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

#include <aax/aeonwave.hpp>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define ENABLE_EMITTER_DISTORTION	1
#define ENABLE_EMITTER_PHASING		1
#define ENABLE_EMITTER_DYNAMIC_GAIN	0
#define ENABLE_FRAME_CHORUS		0
#define ENABLE_FRAME_DYNAMIC_PITCH	0
#define ENABLE_FRAME_DYNAMIC_GAIN	0
#define ENABLE_FRAME_FREQFILTER		1
#define ENABLE_FRAME_EQUALIZER		1
#define ENABLE_STATIC_FREQFILTER	0
#define FILE_PATH			SRC_PATH"/wasp.wav"

int main(int argc, char **argv)
{
    char *devname, *infile;
    int res, rv = 0;

    devname = getDeviceName(argc, argv);
    infile = getInputFile(argc, argv, FILE_PATH);

    aax::AeonWave config(devname, AAX_MODE_WRITE_STEREO);
    testForError(config, "No default audio device available.");

    if (!aax::is_valid(config, AAX_CONFIG_HD))
    {
        printf("Warning:\n");
        printf("  %s requires a registered version of AeonWave\n", argv[0]);
        printf("  Please visit http://www.adalin.com/buy_aeonwaveHD.html to ");
        printf("obtain\n  a product-key.\n\n");
        rv = -1;
    }

    if (config && (rv >= 0))
    {
        aax::Buffer buffer = config.buffer(infile);
        if (buffer)
        {
            float dt = 0.0f;
            int q, state;
            float pitch;

            /** mixer */
            res = config.set(AAX_INITIALIZED);
            testForState(res, "aaxMixerInit");

            /** AudioFrame */
            aax::Mixer mixer(config);
            testForError(mixer, "aaxAudioFrameCreate");

            /* register the audio-mixer at the mixer */
            res = config.add(mixer);
            testForState(res, "aaxMixerRegisterAudioFrame");

            /* schedule the mixer for playback */
            res = config.set(AAX_PLAYING);
            testForState(res, "aaxMixerStart");

            /* schedule the audio-mixer for playback */
            res = mixer.set(AAX_PLAYING);
            testForState(res, "aaxAudioFrameStart");

            /* equalizer */
#if ENABLE_FRAME_EQUALIZER
            aax::dsp dsp(config, AAX_EQUALIZER);
            testForError(dsp, "aaxFilterCreate");

            res = dsp.set(0, 60.0f, 0.3f, 1.0f, 1.2f);
            testForState(res, "aaxFilterSetSlot/0");

            res = dsp.set(1, 5000.0f, 1.0f, 0.0f, 6.0f);
            testForState(res, "aaxFilterSetSlot/1");

            res = dsp.set(AAX_TRUE);
            testForState(res, "aaxFilterSetState");

            res = mixer.set(dsp);
            testForState(res, "aaxMixerSetFilter");
#endif

            /** emitter */
            aax::Emitter emitter(AAX_STEREO);
            testForError(emitter, "Unable to create a new emitter");

            pitch = getPitch(argc, argv);
            dsp = aax::dsp(config, AAX_PITCH_EFFECT);
            testForError(dsp, "aaxEffectCreate");

            res = dsp.set(AAX_PITCH, pitch);
            testForState(res, "aaxEffectSetParam");

            res = emitter.set(dsp);
            testForState(res, "aaxEmitterSetPitch");

            res = emitter.add(buffer);
            testForState(res, "aaxEmitterAddBuffer");

            res = emitter.set(AAX_LOOPING, AAX_FALSE);
            testForState(res, "aaxEmitterSetMode");

            /* schedule the emitter for playback */
            res = emitter.set(AAX_PLAYING);
            testForState(res, "aaxEmitterStart");

            /* register the emitter at the audio-mixer */
            res = mixer.add(emitter);
            testForState(res, "aaxAudioFrameRegisterEmitter");

#if ENABLE_EMITTER_PHASING
            /* emitter phasing */
            printf("emitter phasing\n");
            dsp = emitter.get(AAX_PHASING_EFFECT);
            testForError(dsp, "aaxEmitterGetEffect");

            dsp.set(0, 1.0f, 0.0f, 0.0f, 0.067f);
            testForError(dsp, "aaxEffectSetSlot");

            dsp.set(AAX_TRUE);
            testForError(dsp, "aaxEffectSetState");

            res = emitter.set(dsp);
            testForState(res, "aaxEmitterSetEffect");
#endif

#if 0
            /* for testing purpose only!!! */
            printf("emitter distortion\n");
            dsp = config.get(AAX_DISTORTION_EFFECT);
            res = testForError(dsp, "aaxEffectCreate");

            res = dsp.set(0, 0.8f, 0.0f, 1.0f, 0.5f);
            testForState(res, "aaxEffectSetSlot 0");

            res = dsp.set(AAX_TRUE);
            testForState(res, "aaxEffectSetState");

            res = emitter.set(dps);
            testForState(res, "aaxEmitterSetEffect");
#endif

#if ENABLE_FRAME_FREQFILTER
            /* audio-mixer frequency dsp */
            dsp = config.get(AAX_FREQUENCY_FILTER);
            testForError(dsp, "aaxFilterCreate");
# if ENABLE_STATIC_FREQFILTER
	 /* straight frequency dsp */
            printf("frequency dsp at 200Hz\n");
            res = dsp.set(0, 12000.0f, 0.8f, 0.0f, 1.0f);
            testForState(, "aaxFilterSetSlot");

            res = dsp.set(AAX_TRUE);
            testForState(res "aaxFilterSetState");
# else
            /* envelope following dynamic frequency dsp (auto-wah) */
            printf("auto-wah dsp\n");
            res = dsp.set(0, 100.0f, 0.5f, 1.0f, 8.0f);
            testForState(res, "aaxFilterSetSlot 0");

            res = dsp.set(1, 550.0f, 0.0f, 0.0f, 1.0f);
            testForState(res, "aaxFilterSetSlot 1");

            res = dsp.set(AAX_INVERSE_ENVELOPE_FOLLOW);
            testForState(res, "aaxFilterSetState");
# endif
            res = mixer.set(dsp);
            testForState(res, "aaxAudioFrameSetFilter");
#endif

#if ENABLE_EMITTER_DISTORTION
            /* audio-mixer distortion dsp */
            printf("audio-mixer distortion\n");
            dsp = config.get(AAX_DISTORTION_EFFECT);
            testForError(dsp, "aaxEffectCreate");

            res = dsp.set(0, 0.6f, 0.2f, 1.0f, 1.0f);
            testForState(res, "aaxEffectSetSlot 0");

            res = dsp.set(AAX_TRUE);
            testForState(res, "aaxEffectSetState");

            res = mixer.set(dsp);
            testForState(res, "aaxAudioFrameSetEffect");
#endif

# if ENABLE_FRAME_CHORUS
            /* audio-mixer delay dsp */
            printf("audio-mixer delay dsp\n");
            dsp = mixer.get(AAX_CHORUS_EFFECT);
            dsp.set(0, 0.5f, 0.1f, 0.08f, 0.15f);
            dsp.set(AAX_INVERSE_ENVELOPE_FOLLOW);
            mixer.set(dsp);
#endif

#if ENABLE_FRAME_DYNAMIC_PITCH
            /* dynamic pitch dsp for the audio-mixer*/
            dsp = config.get(AAX_DYNAMIC_PITCH_EFFECT);
            testForError(dsp, "aaxEffectCreate");

            res = dsp.set(0, 0.0f, 0.0f, 1.0f, 0.0f);
            testForState(res, "aaxEffectSetSlot");

            res = dsp.set(AAX_ENVELOPE_FOLLOW);
            testForState(res, "aaxEffectSetState");

            res = mixer.set(dsp);
            testForState(res, "aaxAudioFrameSetEffect");
#endif

#if ENABLE_FRAME_DYNAMIC_GAIN
            /* dynamic gain dsp for audio-mixer (compressor) */
            dsp = config.get(AAX_DYNAMIC_GAIN_FILTER);
            testForError(dsp, "aaxFilterCreate");

            res = dsp.set(0, 0.0f, 1.0f, 0.8f, 0.2f);
            testForState(res, "aaxFilterSetSlot");

            res = dsp.set(AAX_ENVELOPE_FOLLOW);
            testForState(res, "aaxFilterSetState");

            res = mixer.set(dsp);
            testForState(res, "aaxAudioFrameSetFilter");
#endif


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

                    off_s = emitter.offset();
                    offs = emitter.offset(AAX_SAMPLES);
                    offs_bytes = emitter.offset(AAX_BYTES);
                    printf("playing time: %5.2f, buffer position: %5.2f "
                           "(%li samples/ %li bytes)\n", dt, off_s,
                           offs, offs_bytes);
                }
                state = emitter.state();
            }
            while ((dt < 60.0f) && (state == AAX_PLAYING));

            res = emitter.set(AAX_STOPPED);
            testForState(res, "aaxEmitterStop");

            do
            {
                msecSleep(50);
                state = emitter.state();
            }
            while (state == AAX_PLAYING);

            res = mixer.set(AAX_STOPPED);
            res = mixer.remove(emitter);
            res = config.remove(mixer);
            res = config.set(AAX_STOPPED);
        }
    }

    return rv;
}
