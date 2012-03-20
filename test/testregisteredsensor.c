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
#define RECORD_TIME_SEC		60000.0f

int main(int argc, char **argv)
{
   aaxConfig config, record;
   char *devname;
   int res;

   devname = getDeviceName(argc, argv);
   if (!devname) {
      devname = "AeonWave on File: /tmp/AeonWaveOut.wav";
   }
   printf("Playback: %s\n", devname);
   config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
   testForError(config, "No default audio device available.");

   devname = getCaptureName(argc, argv);
   printf("Recording: %s\n", devname);
   record = aaxDriverOpenByName(devname, AAX_MODE_READ);
   testForError(record, "Capture device is unavailable.");

   if (config && record)
   {
      enum aaxFormat format;
      int channels;
      float f;

      format = AAX_PCM16S;
      channels = 2;

      /** mixer */
#if 0
      res = aaxMixerSetSetup(config, AAX_FREQUENCY, 64000);
      testForState(res, "aaxMixerSeFrequency");
#endif

      res = aaxMixerSetState(config, AAX_INITIALIZED);
      testForState(res, "aaxMixerInit");

      res = aaxMixerSetState(config, AAX_PLAYING);
      testForState(res, "aaxMixerStart");

      /** registered sensor */
      printf("Capturing %5.1f seconds of audio\n", RECORD_TIME_SEC);
 
      res = aaxMixerSetSetup(record, AAX_TRACKS, channels);
      testForState(res, "aaxMixerSetNoTracks");

      res = aaxMixerSetSetup(record, AAX_FORMAT, format);
      testForState(res, "aaxMixerSetFormat");

      res = aaxMixerRegisterSensor(config, record);
      testForState(res, "aaxMixerRegisterSensor");

      /** must be called after aaxMixerRegisterSensor */
      res = aaxMixerSetState(record, AAX_INITIALIZED);
      testForState(res, "aaxMixerSetInitialize");

      res = aaxSensorSetState(record, AAX_CAPTURING);
      testForState(res, "aaxSensorCaptureStart");

      f = 0.0f;
      do
      {
         nanoSleep(7e7);
         f += 7e7*1e-9;
      }
      while (f < RECORD_TIME_SEC);
      printf("\n");

      res = aaxMixerSetState(config, AAX_STOPPED);
      testForState(res, "aaxMixerSetState");

      res = aaxSensorSetState(record, AAX_STOPPED);
      testForState(res, "aaxSensorCaptureStop");

      res = aaxMixerDeregisterSensor(config, record);
      testForState(res, "aaxMixerDeregisterSensor");

      res = aaxDriverClose(record);
      testForState(res, "aaxDriverClose");

      res = aaxDriverDestroy(record);
      testForState(res, "aaxDriverDestroy");

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

