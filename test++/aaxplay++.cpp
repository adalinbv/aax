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

#include <math.h>
#include <iostream>

#include <aax/AeonWave.hpp>

#include <driver.h>

#define IFILE_PATH		SRC_PATH"/stereo.mp3"
#define OFILE_PATH		"aaxout.wav"

void
help()
{
    std::cout << "aaxplay version " << AAX_UTILS_MAJOR_VERSION << AAX_UTILS_MINOR_VERSION << AAX_UTILS_MICRO_VERSION << std::endl;
    std::cout << "Usage: aaxplay [options]" << std::endl;
    std::cout << "Plays audio from a file or from an audio input device." << std::endl;
    std::cout << "Optionally writes the audio to an output file." << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -i, --input <file>\t\tplayback audio from a file" << std::endl;
    std::cout << "  -c, --capture <device>\tcapture from an audio device" << std::endl;
    std::cout << "  -d, --device <device>\t\tplayback device (default if not specified)" << std::endl;
    std::cout << "  -o, --output <file>\t\talso write to an audio file (optional)" << std::endl;
    std::cout << "  -b, --batch\t\t\tprocess as fast as possible (Audio Files only)" << std::endl;
    std::cout << "  -v, --verbose\t\t\tshow extra playback information" << std::endl;
    std::cout << "  -h, --help\t\t\tprint this message and exit" << std::endl;
    std::cout << "Either --input or --capture can be used but not both." << std::endl;
    std::cout << "For a list of device names run: aaxinfo" << std::endl;
    std::cout << std::endl;
    std::cout << "Audio will always be sent to the (default) audio device," << std::endl;
    std::cout << "writing to an output file is fully optional." << std::endl;
    std::cout << std::endl;

    AAX::AeonWave rec("AeonWave on Audio Files", AAX_MODE_READ);
    AAX::AeonWave play("AeonWave on Audio Files");
    if (rec || play)
    {
        std::cout << "Supported file formats:" << std::endl;
        if (rec) {
            std::cout << "  - input : " << rec.interface(0, 0) << std::endl;
        }

        if (play) {
            std::cout << "  - output: " << play.interface(0, 0) << std::endl;
        }
        std::cout << std::endl;
    }

    exit(-1);
}

int main(int argc, char **argv)
{
    char *devname, *idevname;
    char ibuf[256], obuf[256];
    char *infile, *outfile;
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
           snprintf(ibuf, 256, "AeonWave on Audio Files: %s", infile);
           idevname = ibuf;
       }
       else {
          help();
       }
    }

    devname = getDeviceName(argc, argv);
    AAX::AeonWave config(devname);
    AAX::Sensor record;
    AAX::Sensor file;

    testForError(config, "Audio output device is not available.");
    if (!config)
    {
        if (!config) {
           std::cout << "Warning: " << config.error() << std::endl;
        }
        else
        {
            std::cout << "Warning:" << std::endl;
            std::cout << "  " << argv[0] << " requires a registered version of AeonWave" << std::endl;
            std::cout << "  Please visit http://www.adalin.com/buy_aeonwaveHD.html to obtain" << std::endl << "  a product-key." << std::endl << std::endl;
        }
        exit(-2);
    }
    else
    {
        record = config.sensor(idevname, AAX_MODE_READ);
        if (!record)
        {
            std::cout << "File not found: " << infile << std::endl;
            exit(-1);
        }
    }

    outfile = getOutputFile(argc, argv, NULL);
    if (outfile)
    {
        snprintf(obuf, 256, "AeonWave on Audio Files: %s", outfile);
        file = config.sensor(obuf);
    }

    if (config && record && (rv >= 0))
    {
        char batch = getCommandLineOption(argc, argv, "-b") ||
                     getCommandLineOption(argc, argv, "--batch");
        char fparam = getCommandLineOption(argc, argv, "-f") ||
                      getCommandLineOption(argc, argv, "-frame");
        float pitch = getPitch(argc, argv);
        float dhour, hour, minutes, seconds;
        float duration, freq;
        unsigned int max_samples;
        AAX::Mixer frame;
        aaxEffect effect;
        aaxFilter filter;
        char tstr[80];
        int state;

        if (batch && !config.get(AAX_BATCHED_MODE)) {
            std::cout << "Warning: Batched mode not supported for this backend" << std::endl;
        }

        /** mixer */
        res = config.set(AAX_REFRESHRATE, 64);
        testForState(res, "aaxMixerSetSetup");

        res = config.set(AAX_INITIALIZED);
        testForState(res, "aaxMixerInit");

        res = config.set(AAX_PLAYING);
        testForState(res, "aaxMixerStart");

        if (fparam)
        {
            std::cout << "  using audio-frames" << std::endl;

            /** audio frame */
            frame = config.mixer();
            testForError(frame, "Unable to create a new audio frame");   

            /** register audio frame */
            res = config.add(frame);
            testForState(res, "aaxMixerRegisterAudioFrame");

            res = frame.set(AAX_PLAYING);
            testForState(res, "aaxAudioFrameSetState");

            /** sensor */
            res = frame.add(record);
            testForState(res, "aaxAudioFrameRegisterSensor");
        }
        else
        {
            /** sensor */
            res = config.add(record);
            testForState(res, "aaxMixerRegisterSensor");
        }

        if (file)
        {
            res = config.add(file);
            testForState(res, "aaxMixerRegisterSensor file out");
        }

        /** must be called after aaxMixerRegisterSensor */
        res = record.set(AAX_INITIALIZED);
        testForState(res, "aaxMixerSetInitialize");

        res = record.sensor(AAX_CAPTURING);
        testForState(res, "aaxSensorCaptureStart");

        if (record && verbose)
        {
            const char *s;

            s = record.info(AAX_MUSIC_PERFORMER_STRING);
            if (s) std::cout << " Performer: " << s << std::endl;

            s = record.info(AAX_TRACK_TITLE_STRING);
            if (s) std::cout << " Title    : " << s << std::endl;

            s = record.info(AAX_ALBUM_NAME_STRING);
            if (s) std::cout << " Album    : " << s << std::endl;

            s = record.info(AAX_SONG_COMPOSER_STRING);
            if (s) std::cout << " Composer : " << s << std::endl;

            s = record.info(AAX_ORIGINAL_PERFORMER_STRING);
            if (s) std::cout << " Original : " << s << std::endl;

            s = record.info(AAX_MUSIC_GENRE_STRING);
            if (s) std::cout << " Genre    : " << s << std::endl;

            s = record.info(AAX_RELEASE_DATE_STRING);
            if (s) std::cout << " Release date: " << s << std::endl;

            s = record.info(AAX_TRACK_NUMBER_STRING);
            if (s) std::cout << " Track number: " << s << std::endl;

            s = record.info(AAX_SONG_COPYRIGHT_STRING);
            if (s) std::cout << " Copyright:  " << s << std::endl;

            s = record.info(AAX_WEBSITE_STRING);
            if (s) std::cout << " Website  : " << s << std::endl;
        }


        if (file)
        {
            res = file.set(AAX_INITIALIZED);
            testForState(res, "aaxMixerSetInitialize");

            res = file.set(AAX_PLAYING);
            testForState(res, "aaxSensorCaptureStart");
        }

        set_mode(1);
        freq = (float)record.get(AAX_FREQUENCY);
        max_samples = record.get(AAX_SAMPLES_MAX);
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

        do
        {
            if (verbose)
            {
                float pos = (float)record.offset(AAX_SAMPLES)/freq;
                const char *p, *t;

                p = record.info(AAX_MUSIC_PERFORMER_UPDATE);
                t = record.info(AAX_TRACK_TITLE_UPDATE);
                if (p && t) {
                    std::cout << "\r\033[K Playing  : " << p << " - " << t << std::endl;
                } else if (p) {
                    std::cout << "\r\033[K Performer: " << p << std::endl;
                } else if (t) {
                    std::cout << "\r\033[K Title    : " << t << std::endl;
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

            if (get_key()) break;

            if (batch) {
               res = config.set(AAX_UPDATE);
            } else {
                msecSleep(250);
            }
            state = record.get();
        }
        while (state == AAX_PLAYING);
        std::cout << "" << std::endl;
        set_mode(0);

        res = config.set(AAX_STOPPED);
        testForState(res, "aaxMixerSetState");

        if (frame)
        {
            res = frame.set(AAX_STOPPED);
            testForState(res, "aaxAudioFrameSetState");
        }

        res = record.set(AAX_STOPPED);
        testForState(res, "aaxSensorCaptureStop");

        if (frame)
        {
            res = frame.remove(record);
            testForState(res, "aaxAudioFrameDeregisterSensor");

            res = config.remove(frame);
            testForState(res, "aaxMixerDeregisterAudioFrame");
        }
        else
        {
            res = config.remove(record);
            testForState(res, "aaxMixerDeregisterSensor");
        }
    }
    else {
        std::cout << "Unable to open capture device." << std::endl;
    }

    if (file)
    {
        res = file.set(AAX_STOPPED);
        testForState(res, "aaxMixerSetState");

        res = config.remove(file);
        testForState(res, "aaxMixerDeregisterSensor file out");
    }

    return rv;
}

