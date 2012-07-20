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

#include <aax/aax.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define FILE_PATH		SRC_PATH"/stereo.mp3"

int main(int argc, char **argv)
{
   aaxConfig config, record;
   char *devname, *infile;
   char indevname[4096];
   int res;

   devname = getDeviceName(argc, argv);
   infile = getInputFile(argc, argv, FILE_PATH);
   snprintf(indevname, 4096, "AeonWave on Audio Files: %s", infile);
   record = aaxDriverOpenByName(indevname, AAX_MODE_READ);
   if (!record)
   {
      printf("File not found: %s\n");
      exit(-1);
   }
   printf("Playing: %s\n", infile);

   config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
   testForError(config, "Audio output device is not available.");

   if (config && record)
   {
      int state;

      /** mixer */
      res = aaxMixerSetState(config, AAX_INITIALIZED);
      testForState(res, "aaxMixerInit");

      res = aaxMixerSetState(config, AAX_PLAYING);
      testForState(res, "aaxMixerStart");

      /** sensor */
      res = aaxMixerRegisterSensor(config, record);
      testForState(res, "aaxMixerRegisterSensor");

      /** must be called after aaxMixerRegisterSensor */
      res = aaxMixerSetState(record, AAX_INITIALIZED);
      testForState(res, "aaxMixerSetInitialize");

      res = aaxSensorSetState(record, AAX_CAPTURING);
      testForState(res, "aaxSensorCaptureStart");
      do
      {
         msecSleep(100);
         state = aaxMixerGetState(record);
      }
      while (state == AAX_PLAYING);

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

