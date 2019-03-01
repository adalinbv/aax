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
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include <aax/aax.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define IFILE_PATH		SRC_PATH"/stereo.mp3"
#define OFILE_PATH		"aaxout.wav"

void
help()
{
    aaxConfig cfgi, cfgo;

    printf("aaxplay version %i.%i.%i\n\n", AAX_UTILS_MAJOR_VERSION,
                                           AAX_UTILS_MINOR_VERSION,
                                           AAX_UTILS_MICRO_VERSION);
    printf("Usage: aaxplay [options]\n");
    printf("Plays audio from a file or from an audio input device.\n");
    printf("Optionally writes the audio to an output file.\n");

    printf("\nOptions:\n");
    printf("  -i, --input <file>\t\tplayback audio from a file\n");
    printf("  -c, --capture <device>\tcapture from an audio device\n");
    printf("  -d, --device <device>\t\tplayback device (default if not specified)\n");
    printf("  -o, --output <file>\t\talso write to an audio file (optional)\n");
    printf("  -b, --batch\t\t\tprocess as fast as possible (Audio Files only)\n");
    printf("  -v, --verbose\t\t\tshow extra playback information\n");
    printf("  -h, --help\t\t\tprint this message and exit\n");
    printf("Either --input or --capture can be used but not both.\n");
    printf("For a list of device names run: aaxinfo\n");

    printf("\nAudio will always be sent to the (default) audio device,\n");
    printf("writing to an output file is fully optional.\n");

    cfgi = aaxDriverGetByName("AeonWave on Audio Files", AAX_MODE_READ);
    cfgo = aaxDriverGetByName("AeonWave on Audio Files", AAX_MODE_WRITE_STEREO);
    if (cfgi || cfgo) {
        printf("\nSupported file formats:\n");
    }

    if (cfgi)
    {
        const char *d, *s;

        d = aaxDriverGetDeviceNameByPos(cfgi, 0, AAX_MODE_READ);
        s = aaxDriverGetInterfaceNameByPos(cfgi, d, 0, AAX_MODE_READ);
        printf("  - input : %s\n", s);
        aaxDriverDestroy(cfgi);
    }

    if (cfgo)
    {
        const char *d, *s;

        d = aaxDriverGetDeviceNameByPos(cfgo, 0, AAX_MODE_WRITE_STEREO);
        s = aaxDriverGetInterfaceNameByPos(cfgo, d, 0, AAX_MODE_WRITE_STEREO);
        printf("  - output: %s\n", s);
        aaxDriverDestroy(cfgo);
    }
    printf("\nUse aaxplaymidi for playing MIDI files.\n");

    printf("\nDuring playback the stream can be suspened and resumed using "
           "the spacebar key.\n");
    printf("\n");

    exit(-1);
}

int main(int argc, char **argv)
{
    char *devname, *idevname;
    char ibuf[256], obuf[256];
    char *infile, *outfile;
    aaxEmitter emitter = NULL;
    aaxBuffer buffer = NULL;
    aaxConfig config = NULL;
    aaxConfig record = NULL;
    aaxConfig file = NULL;
    int res, rv = 0;
    int verbose = 0;

    if (argc == 1 || getCommandLineOption(argc, argv, "-h") ||
                     getCommandLineOption(argc, argv, "--help"))
    {
        help();
    }

    if (getCommandLineOption(argc, argv, "-v") || 
        getCommandLineOption(argc, argv, "--verbose"))
    {
       verbose = 1;
    }

    idevname = getCaptureName(argc, argv);
    infile = getInputFile(argc, argv, IFILE_PATH);
    if (!idevname)
    {
       if (infile)
       {
           if (!strstr(infile+strlen(infile)-5, ".aaxs"))
           {
               snprintf(ibuf, 256, "AeonWave on Audio Files: %s", infile);
               idevname = ibuf;
           }
       }
       else {
          help();
       }
    }

    devname = getDeviceName(argc, argv);
    config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
    testForError(config, "Audio output device is not available.");

    if (config)
    {
        if (idevname)
        {
            record = aaxDriverOpenByName(idevname, AAX_MODE_READ);
            if (!record)
            {
                printf("File not found: %s\n", infile);
                exit(-1);
            }
        }
        else {
            buffer = bufferFromFile(config, infile);
        }
    }

    outfile = getOutputFile(argc, argv, NULL);
    if (outfile)
    {
        snprintf(obuf, 256, "AeonWave on Audio Files: %s", outfile);
        file = aaxDriverOpenByName(obuf, AAX_MODE_WRITE_STEREO);
    }
    else {
        file = NULL;
    }

    if (config && (record || buffer) && (rv >= 0))
    {
        char batch = getCommandLineOption(argc, argv, "-b") ||
                     getCommandLineOption(argc, argv, "--batch");
        char fparam = getCommandLineOption(argc, argv, "-f") ||
                      getCommandLineOption(argc, argv, "-frame");
        float pitch = getPitch(argc, argv);
        float dhour, hour, minutes, seconds;
        float duration, freq;
        unsigned int max_samples;
        int key, paused;
        aaxFrame frame = NULL;
        aaxEffect effect;
//      aaxFilter filter;
        char tstr[80];
        int state;
        float dt;

        if (batch && !aaxMixerGetSetup(config, AAX_BATCHED_MODE)) {
            printf("Warning: Batched mode not supported for this backend\n");
        }

        /** mixer */
        res = aaxMixerSetSetup(config, AAX_REFRESHRATE, 64);
        testForState(res, "aaxMixerSetSetup");

        res = aaxMixerSetState(config, AAX_INITIALIZED);
        testForState(res, "aaxMixerInit");

        res = aaxMixerSetState(config, AAX_PLAYING);
        testForState(res, "aaxMixerStart");

        if (fparam)
        {
            printf("  using audio-frames\n");

            /** audio frame */
            frame = aaxAudioFrameCreate(config);
            testForError(frame, "Unable to create a new audio frame\n");   

            /** register audio frame */
            res = aaxMixerRegisterAudioFrame(config, frame);
            testForState(res, "aaxMixerRegisterAudioFrame");

            res = aaxAudioFrameSetState(frame, AAX_PLAYING);
            testForState(res, "aaxAudioFrameSetState");

            if (pitch != 1.0f)
            {
#if 0
                effect = aaxAudioFrameGetEffect(frame, AAX_PITCH_EFFECT);
                testForError(effect, "aaxEffectCreate");

                aaxEffectSetParam(effect, AAX_PITCH, AAX_LINEAR, pitch);
#else
                effect = aaxAudioFrameGetEffect(frame,AAX_DYNAMIC_PITCH_EFFECT);
                testForError(effect, "aaxEffectCreate");

                res = aaxEffectSetSlot(effect, 0, AAX_LINEAR,
                                          0.0f, 0.025f, pitch, 0.0f);
                testForState(res, "aaxEffectSetSlot");

                res = aaxEffectSetState(effect, AAX_SINE_WAVE);
                testForState(res, "aaxEffectSetState");
#endif
                res = aaxAudioFrameSetEffect(frame, effect);
                testForState(res, " aaxAudioFrameSetEffect");

                aaxEffectDestroy(effect);
                testForState(res, "aaxEffectDestroy");
            }

            /** sensor */
            res = aaxAudioFrameRegisterSensor(frame, record);
            testForState(res, "aaxAudioFrameRegisterSensor");
        }
        else
        {
#if 0
            effect = aaxMixerGetEffect(config, AAX_PITCH_EFFECT);
            testForError(effect, "aaxEffectCreate");

            aaxEffectSetParam(effect, AAX_PITCH, AAX_LINEAR, pitch);

            res = aaxMixerSetEffect(record, effect);
            testForState(res, "aaxEmitterSetEffect");

            res = aaxEffectDestroy(effect);
            testForState(res, "aaxEffectDestroy");
#endif

            /** sensor */
            if (record)
            {
                res = aaxMixerRegisterSensor(config, record);
                testForState(res, "aaxMixerRegisterSensor");
            }
            else
            {
                emitter = aaxEmitterCreate();
                testForError(emitter, "Unable to create a new emitter");

                res = aaxEmitterAddBuffer(emitter, buffer);
                testForState(res, "aaxEmitterAddBuffer");

                res = aaxMixerRegisterEmitter(config, emitter);
                testForState(res, "aaxMixerRegisterEmitter");

                res = aaxEmitterSetState(emitter, AAX_PLAYING);
                testForState(res, "aaxEmitterStart");
            }
        }

        if (pitch != 1.0f)
        {
            if (record) {
                effect = aaxMixerGetEffect(record, AAX_DYNAMIC_PITCH_EFFECT);
            } else {
                effect = aaxEmitterGetEffect(emitter, AAX_DYNAMIC_PITCH_EFFECT);
            }
            testForError(effect, "aaxEffectCreate");

            res = aaxEffectSetSlot(effect, 0, AAX_LINEAR,
                                      0.0f, frame ? 0.5f : 0.06f, pitch, 0.0f);
            testForState(res, "aaxEffectSetSlot");

            res = aaxEffectSetState(effect, AAX_TRIANGLE_WAVE);
            testForState(res, "aaxEffectSetState");

            if (record) {
                res = aaxMixerSetEffect(record, effect);
            } else {
                res = aaxEmitterSetEffect(emitter, effect);
            }
            testForState(res, "aaxEmitterSetEffect");

            res = aaxEffectDestroy(effect);
            testForState(res, "aaxEffectDestroy");
        }

        if (file)
        {
            res = aaxMixerRegisterSensor(config, file);
            testForState(res, "aaxMixerRegisterSensor file out");
        }

#if 0
        /** set capturing Auto-Gain Control (AGC): 0dB */
        filter = aaxMixerGetFilter(record, AAX_VOLUME_FILTER);
        if (filter)
        {
            aaxFilterSetParam(filter, AAX_AGC_RESPONSE_RATE, AAX_LINEAR, 1.5f);
            aaxMixerSetFilter(record, filter);
            res = aaxFilterDestroy(filter);
        }
#endif

        /** must be called after aaxMixerRegisterSensor */
        if (record)
        {
            res = aaxMixerSetState(record, AAX_INITIALIZED);
            testForState(res, "aaxMixerSetInitialize");

            res = aaxSensorSetState(record, AAX_CAPTURING);
            testForState(res, "aaxSensorCaptureStart");
        }

        if (record && verbose)
        {
            unsigned int samples = aaxMixerGetSetup(record, AAX_SAMPLES_MAX);
            const char *s;

            if (samples) {
              printf(" Audio format: %i Hz, %i bits/sample, %i tracks, %i samples\n",
                     aaxMixerGetSetup(record, AAX_FREQUENCY),
                     aaxGetBitsPerSample(aaxMixerGetSetup(record, AAX_FORMAT)),
                     aaxMixerGetSetup(record, AAX_TRACKS), samples);
           } else {
              printf(" Audio format: %i Hz, %i bits/sample, %i tracks\n",
                     aaxMixerGetSetup(record, AAX_FREQUENCY),
                     aaxGetBitsPerSample(aaxMixerGetSetup(record, AAX_FORMAT)),
                     aaxMixerGetSetup(record, AAX_TRACKS));
           }

            s = aaxDriverGetSetup(record, AAX_MUSIC_PERFORMER_STRING);
            if (s) printf(" Performer: %s\n", s);

            s = aaxDriverGetSetup(record, AAX_TRACK_TITLE_STRING);
            if (s) printf(" Title    : %s\n", s);

            s = aaxDriverGetSetup(record, AAX_ALBUM_NAME_STRING);
            if (s) printf(" Album    : %s\n", s);

            s = aaxDriverGetSetup(record, AAX_SONG_COMPOSER_STRING);
            if (s) printf(" Composer : %s\n", s);

            s = aaxDriverGetSetup(record, AAX_ORIGINAL_PERFORMER_STRING);
            if (s) printf(" Original : %s\n", s);

            s = aaxDriverGetSetup(record, AAX_MUSIC_GENRE_STRING);
            if (s) printf(" Genre    : %s\n", s);
            s = aaxDriverGetSetup(record, AAX_RELEASE_DATE_STRING);
            if (s) printf(" Release date: %s\n", s);

            s = aaxDriverGetSetup(record, AAX_TRACK_NUMBER_STRING);
            if (s) printf(" Track number: %s\n", s);

            s = aaxDriverGetSetup(record, AAX_SONG_COPYRIGHT_STRING);
            if (s) printf(" Copyright:  %s\n", s);

            s = aaxDriverGetSetup(record, AAX_WEBSITE_STRING);
            if (s) printf(" Website  : %s\n", s);
        }

        if (file)
        {
            res = aaxMixerSetState(file, AAX_INITIALIZED);
            testForState(res, "aaxMixerSetInitialize");

            res = aaxMixerSetState(file, AAX_PLAYING);
            testForState(res, "aaxSensorCaptureStart");
        }

        freq = 0;
        max_samples = 0;
        if (record)
        {
            freq = (float)aaxMixerGetSetup(record, AAX_FREQUENCY);
            max_samples = aaxMixerGetSetup(record, AAX_SAMPLES_MAX);
        }
        if (max_samples)
        {
            duration = (float)max_samples/freq;
            seconds = duration;
            dhour = floorf(seconds/(60.0f*60.0f));
            seconds -= dhour*60.0f*60.0f;
            minutes = floorf(seconds/60.0f);
            seconds -= minutes*60.0f;
            if (dhour) {
               snprintf(tstr, 80, "%s  %02.0f:%02.0f:%02.0f %s\r",
                                  "pos: % 5.1f (%02.0f:%02.0f:%04.1f) of ",
                                  dhour, minutes, seconds, " % 3.0f%");
            } else {
               snprintf(tstr, 80, "%s  %02.0f:%02.0f %s\r",
                                  "pos: % 5.1f (%02.0f:%04.1f) of ",
                                  minutes, seconds, " % 3.0f%");
           }
        }
        else
        {
           dhour = duration = AAX_FPINFINITE;
           snprintf(tstr, 80, "%s\r", "pos: % 5.1f (%02.0f:%02.0f:%04.1f)");
        }

        dt = 0.0f;
        paused = AAX_FALSE;
        set_mode(1);
        do
        {
            if (verbose)
            {
                float pos;

                if (record)
                {
                    const char *p, *t;

                    pos = (float)aaxSensorGetOffset(record, AAX_SAMPLES)/freq;

                    p = aaxDriverGetSetup(record, AAX_MUSIC_PERFORMER_UPDATE);
                    t = aaxDriverGetSetup(record, AAX_TRACK_TITLE_UPDATE);
                    if (p && t) {
                        printf("\r\033[K Playing  : %s - %s\n", p, t);
                    } else if (p) {
                        printf("\r\033[K Performer: %s\n", p);
                    } else if (t) {
                        printf("\r\033[K Title    : %s\n", t);
                    }
                }
                else {
                    pos = (float)aaxEmitterGetOffsetSec(emitter);
                }

                if (duration != AAX_FPINFINITE)
                {
                    seconds = duration-pos;
                    hour = floorf(seconds/(60.0f*60.0f));
                    seconds -= hour*60.0f*60.0f;
                    minutes = floorf(seconds/60.0f);
                    seconds -= minutes*60.0f;
                    if (dhour) {
                       printf(tstr, pos, hour, minutes, seconds, 100*pos/duration);
                    } else {
                       printf(tstr, pos, minutes, seconds, 100*pos/duration);
                    }
                }
                else
                {
                    seconds = pos;
                    hour = floorf(seconds/(60.0f*60.0f));
                    seconds -= hour*60.0f*60.0f;
                    minutes = floorf(seconds/60.0f);
                    seconds -= minutes*60.0f;
                    printf(tstr, pos, hour, minutes, seconds);
                }
                fflush(stdout);
            }

            key = get_key();
            if (key)
            {
               if (key == ' ')
               {
                  if (paused)
                  {
                     aaxMixerSetState(config, AAX_PLAYING);
                     printf("\nRestart playback.\n");
                     paused = AAX_FALSE;
                  }
                  else
                  {
                     aaxMixerSetState(config, AAX_SUSPENDED);
                     printf("\nPause playback.\n");
                     paused = AAX_TRUE;
                  }
               }
               else {
                  break;
               }
            }

            if (batch) {
               res = aaxMixerSetState(config, AAX_UPDATE);
            } else {
                msecSleep(250);
            }

            if (record) {
                state = aaxMixerGetState(record);
            }
            else {
                state = aaxEmitterGetState(emitter);
                dt += 0.25f;
            }
        }
        while (state == AAX_PLAYING && dt < 30.0f);
        printf("\n");
        set_mode(0);

        res = aaxMixerSetState(config, AAX_STOPPED);
        testForState(res, "aaxMixerSetState");

        if (frame)
        {
            res = aaxAudioFrameSetState(frame, AAX_STOPPED);
            testForState(res, "aaxAudioFrameSetState");
        }

        if (record)
        {
            res = aaxSensorSetState(record, AAX_STOPPED);
            testForState(res, "aaxSensorCaptureStop");
        }
        else
        {
            res = aaxEmitterSetState(emitter, AAX_PROCESSED);
            testForState(res, "aaxEmitterStop");
        }

        if (frame)
        {
            res = aaxAudioFrameDeregisterSensor(frame, record);
            testForState(res, "aaxAudioFrameDeregisterSensor");

            res = aaxMixerDeregisterAudioFrame(config, frame);
            testForState(res, "aaxMixerDeregisterAudioFrame");

            res = aaxAudioFrameDestroy(frame);
            testForState(res, "aaxAudioFrameDestroy");
        }
        else
        {
            if (record)
            {
                res = aaxMixerDeregisterSensor(config, record);
                testForState(res, "aaxMixerDeregisterSensor");
            }
            else
            {
                res = aaxMixerDeregisterEmitter(config, emitter);
                testForState(res, "aaxMixerDeregisterEmitter");
            }
        }
    }
    else {
        printf("Unable to open capture device.\n");
    }

    if (record)
    {
        res = aaxDriverClose(record);
        res = aaxDriverDestroy(record);
    }
    else
    {
        res = aaxEmitterDestroy(emitter);
        res = aaxBufferDestroy(buffer);
    }

    if (file)
    {
        res = aaxMixerSetState(file, AAX_STOPPED);
        testForState(res, "aaxMixerSetState");

        res = aaxMixerDeregisterSensor(config, file);
        testForState(res, "aaxMixerDeregisterSensor file out");

        res = aaxDriverClose(file);
        testForState(res, "aaxDriverClose");

        res = aaxDriverDestroy(file);
        testForState(res, "aaxDriverDestroy");
    }

    res = aaxDriverClose(config);
    testForState(res, "aaxDriverClose");

    res = aaxDriverDestroy(config);
    testForState(res, "aaxDriverDestroy");

    return rv;
}

