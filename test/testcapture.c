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

   devname = getDeviceName(argc, argv);

   record = aaxDriverOpenByName(devname, AAX_MODE_READ);
   testForError(record, "Capture device is unavailable.");

   if (record)
   {
      enum aaxFormat format;
      aaxEmitter emitter;
      int channels;
      float f, freq;

      format = AAX_PCM16S;
      freq = 44100.0;
      channels = 2;

      printf("Capturing %5.1f seconds of audio\n", RECORD_TIME_SEC);
      res = aaxMixerSetSetup(record, AAX_FREQUENCY, freq);
      testForState(res, "aaxMixerSeFrequency");
 
      res = aaxMixerSetSetup(record, AAX_TRACKS, channels);
      testForState(res, "aaxMixerSetNoTracks");

      res = aaxMixerSetSetup(record, AAX_FORMAT, format);
      testForState(res, "aaxMixerSetFormat");

      res = aaxMixerSetState(record, AAX_INITIALIZED);
       testForState(res, "aaxMixerSetInitialize");

      /** create the emitter */
      emitter = aaxEmitterCreate();
      testForError(emitter, "Unable to create a new emitter\n");

      res = aaxSensorSetState(record, AAX_CAPTURING);
      testForState(res, "aaxSensorCaptureStart");
      do
      {
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
      }
      while (f < RECORD_TIME_SEC);
      printf("\n");

      res = aaxSensorSetState(record, AAX_STOPPED);
      testForState(res, "aaxSensorCaptureStop");


      /** playback */
      res = aaxMixerSetState(record, AAX_STOPPED);
      res = aaxDriverClose(record);
      res = aaxDriverDestroy(record);


      devname = "AeonWave on File: /tmp/AeonWaveOut.wav";
      printf("Play back the recorded audio buffers: %s\n", devname);
      config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
      testForError(config, "No default audio device available.");

      /** mixer */
      res = aaxMixerSetNoTracks(config, 2);
      testForState(res, "aaxMixerSetNoTracks");

      res = aaxMixerInit(config);
      testForState(res, "aaxMixerInit");

      res = aaxMixerRegisterEmitter(config, emitter);
      testForState(res, "aaxMixerRegisterEmitter");

      res = aaxMixerSetState(config, AAX_PLAYING);
      testForState(res, "aaxMixerStart");

      /** schedule the emitter for playback */
      res = aaxEmitterSetState(emitter, AAX_PLAYING);
      testForState(res, "aaxEmitterStart");

      do
      {
         unsigned long offs;
         float off_s;

         off_s = aaxEmitterGetOffsetSec(emitter);
         offs = aaxEmitterGetOffset(emitter, AAX_SAMPLES);

         printf("buffer position: %5.2f (%li samples)\n", off_s, offs);
         res = aaxEmitterGetState(emitter);
         
         nanoSleep(5e8);
      }
      while (res == AAX_PLAYING);

      res = aaxMixerDeregisterEmitter(config, emitter);
      testForState(res, "aaxMixerDeregisterEmitter");

      res = aaxMixerSetState(config, AAX_STOPPED);
      testForState(res, "aaxMixerStop");

      res = aaxEmitterDestroy(emitter);
      testForState(res, "aaxEmitterDestroy");

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

