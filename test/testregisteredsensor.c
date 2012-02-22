/* -*- mode: C; tab-width:8; c-basic-offset:8 -*-
 * vi:set ts=8:
 *
 * This file is in the Public Domain and comes with no warranty.
 * Erik Hofman <erik@ehofman.com>
 *
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <aaxdefs.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define REFRESH_RATE		  50.0f
#define RECORD_TIME_SEC		  5.0f

int main(int argc, char **argv)
{
   aaxConfig config, record;
   char *devname;
   int res;

   devname = "AeonWave on File: /tmp/AeonWaveOut.wav";
   printf("Play back the recorded audio buffers: %s\n", devname);
   config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
   testForError(config, "No default audio device available.");

   devname = getDeviceName(argc, argv);
   record = aaxDriverOpenByName(devname, AAX_MODE_READ);
   testForError(record, "Capture device is unavailable.");

   if (config && record)
   {
      enum aaxFormat format;
      aaxFrame frame;
      int channels;
      float f, freq;

      format = AAX_PCM16S;
      freq = 16000.0;
      channels = 2;

      /** mixer */
#if 0
      res = aaxMixerSetNoTracks(config, channels);
      testForState(res, "aaxMixerSetNoTracks");
#endif
      res = aaxMixerInit(config);
      testForState(res, "aaxMixerInit");

      res = aaxMixerSetState(config, AAX_PLAYING);
      testForState(res, "aaxMixerStart");

      /** audio frame */
      frame = aaxAudioFrameCreate(config);
      testForError(frame, "Unable to create a new audio frame\n");

      /** register audio frame */
      res = aaxMixerRegisterAudioFrame(config, frame);
      testForState(res, "aaxMixerRegisterAudioFrame");

      res = aaxAudioFrameSetState(frame, AAX_PLAYING);
      testForState(res, "aaxAudioFrameSetState");


      /** registered sensor */
      printf("Capturing %5.1f seconds of audio\n", RECORD_TIME_SEC);
      res = aaxMixerSetSetup(record, AAX_FREQUENCY, freq);
      testForState(res, "aaxMixerSeFrequency");
 
      res = aaxMixerSetSetup(record, AAX_TRACKS, channels);
      testForState(res, "aaxMixerSetNoTracks");

      res = aaxMixerSetSetup(record, AAX_FORMAT, format);
      testForState(res, "aaxMixerSetFormat");

//    res = aaxMixerSetState(record, AAX_INITIALIZED);
//    testForState(res, "aaxMixerSetInitialize");

      res = aaxAudioFrameRegisterSensor(frame, record);
      testForState(res, "aaxAudioFrameRegisterSensor");

      res = aaxMixerSetState(record, AAX_INITIALIZED);
      testForState(res, "aaxMixerSetInitialize");

      res = aaxSensorSetState(record, AAX_CAPTURING);
      testForState(res, "aaxSensorCaptureStart");

      f = 0.0f;
      do
      {
#if 0
         aaxBuffer buffer;
         unsigned long ul;

         res = aaxSensorWaitForBuffer(record, 3.0);
         testForState(res, "aaxSensorWaitForBuffer");

         ul = aaxSensorGetOffset(record, AAX_MICROSECONDS);
         f = (float)ul * 1e-6;
#if 1
         printf("Record buffer position: %5.2f sec\r", f);
#endif

         buffer = aaxSensorGetBuffer(record);
         testForError(buffer, "aaxSensorGetBuffer");

         res = aaxEmitterAddBuffer(emitter, buffer);
         testForState(res, "aaxEmitterAddBuffer");

         res = aaxBufferDestroy(buffer);
         testForState(res, "aaxBufferDestroy");
#endif

         nanoSleep(7e7);
         f += 7e7*1e-9;
      }
      while (f < RECORD_TIME_SEC);
      printf("\n");

      res = aaxMixerSetState(config, AAX_STOPPED);
      testForState(res, "aaxMixerSetState");

      res = aaxAudioFrameSetState(frame, AAX_STOPPED);
      testForState(res, "aaxAudioFrameSetState");

      res = aaxSensorSetState(record, AAX_STOPPED);
      testForState(res, "aaxSensorCaptureStop");

      res = aaxAudioFrameDeregisterSensor(frame, record);
      testForState(res, "aaxAudioFrameDeregisterSensor");

      res = aaxDriverClose(record);
      testForState(res, "aaxDriverClose");

      res = aaxDriverDestroy(record);
      testForState(res, "aaxDriverDestroy");

      res = aaxMixerDeregisterAudioFrame(config, frame);
      testForState(res, "aaxMixerDeregisterAudioFrame");

      res = aaxAudioFrameDestroy(frame);
      testForState(res, "aaxAudioFrameDestroy");

      res = aaxDriverClose(config);
      testForState(res, "aaxDriverClose");

      res = aaxDriverDestroy(config);
      testForState(res, "aaxDriverDestroy");
   }
   else {
      printf("Unable to open capture device.\n");
   }

   return 0;
}

