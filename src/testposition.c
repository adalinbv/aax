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

#include <aaxdefs.h>

#include <base/geometry.h>
#include <base/types.h>
#include "driver.h"
#include "wavfile.h"

#define SAMPLE_FREQUENCY	16000

aaxVec3f EmitterPos = { 0.0f, 0.0f, -1.0f };
aaxVec3f EmitterDir = { 0.0f, 0.0f,  1.0f };

aaxVec3f SensorPos = { 0.0f, 0.0f,  0.0f };
aaxVec3f SensorAt  = { 0.0f, 0.0f, -1.0f };
aaxVec3f SensorUp  = { 0.0f, 1.0f,  0.0f };

int main(int argc, char **argv)
{
   aaxEmitter emitter;
   aaxBuffer buffer;
   aaxConfig config;
   aaxFilter filter;
   aaxMtx4f mtx;
   char *devname;
   float pitch;
   int res;

   devname = getDeviceName(argc, argv);

   config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
   testForError(config, "No default audio device available.");

   buffer = aaxBufferCreate(config, (unsigned int)(1.1f*SAMPLE_FREQUENCY), 1, AAX_PCM16S);
   testForError(buffer, "Unable to generate buffer\n");

   res = aaxBufferSetFrequency(buffer, SAMPLE_FREQUENCY);
   testForState(res, "aaxBufferSetFrequency");

   res = aaxBufferSetWaveform(buffer, 1250.0f, AAX_TRIANGLE_WAVE);
   res = aaxBufferMixWaveform(buffer, 1500.0f, AAX_SINE_WAVE, 0.6f);
#if 1
   res = aaxBufferRingmodulateWaveform(buffer, 500.0f, AAX_SINE_WAVE, 0.6f);
#endif
// res = aaxBufferAddWaveform(buffer, 0.0f, AAX_PINK_NOISE, 0.08);
   testForState(res, "aaxBufferProcessWaveform");

   /** emitter */
   emitter = aaxEmitterCreate();
   testForError(emitter, "Unable to create a new emitter\n");

   res = aaxEmitterAddBuffer(emitter, buffer);
   testForState(res, "aaxEmitterAddBuffer");

   pitch = getPitch(argc, argv);
   res = aaxEmitterSetPitch(emitter, pitch);
   testForState(res, "aaxEmitterSetPitch");

   printf("Locate the emitter 5 meters in front of the sensor\n");
   res = aaxMatrixSetDirection(mtx, EmitterPos, EmitterDir);
   testForState(res, "aaxMatrixSetDirection");

   res = aaxEmitterSetMatrix(emitter, mtx);
   testForState(res, "aaxEmitterSetMatrix");

   res = aaxEmitterSetMode(emitter, AAX_POSITION, AAX_ABSOLUTE);
   testForState(res, "aaxEmitterSetMode");

   res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
   testForState(res, "aaxEmitterSetLooping");

   /* tremolo filter for emitter */
   filter = aaxFilterCreate(config, AAX_TREMOLO_FILTER);
   testForError(filter, "aaxFilterCreate");

   filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0f, 2.8f, 0.4f, 0.0f);
   testForError(filter, "aaxFilterSetSlot");

   filter = aaxFilterSetState(filter, AAX_SINE_WAVE);
   testForError(filter, "aaxFilterSetState");

   res = aaxEmitterSetFilter(emitter, filter);
   testForState(res, "aaxEmitterSetFilter");

   res = aaxFilterDestroy(filter);
   testForState(res, "aaxFilterDestroy");

   /** mixer */
   res = aaxMixerInit(config);
   testForState(res, "aaxMixerInit");

   res = aaxMixerRegisterEmitter(config, emitter);
   testForState(res, "aaxMixerRegisterEmitter");

   res = aaxMixerSetState(config, AAX_PLAYING);
   testForState(res, "aaxMixerStart");

   res = aaxMatrixSetOrientation(mtx, SensorPos, SensorAt, SensorUp);
   testForState(res, "aaxMatrixSetOrientation");

    /* tremolo filter for mixer */
   filter = aaxFilterCreate(config, AAX_TREMOLO_FILTER);
   testForError(filter, "aaxFilterCreate");

   filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0f, 0.9f, 0.5f, 0.0f);
   testForError(filter, "aaxFilterSetSlot");

   filter = aaxFilterSetState(filter, AAX_TRIANGLE_WAVE);
   testForError(filter, "aaxFilterSetState");

   res = aaxMixerSetFilter(config, filter);
   testForState(res, "aaxMixerSetFilter");

   res = aaxFilterDestroy(filter);
   testForState(res, "aaxFilterDestroy");


   /** sensor */
   res = aaxMatrixInverse(mtx);
   res = aaxSensorSetMatrix(config, mtx);
   testForState(res, "aaxSensorSetMatrix");

   /** schedule the emitter for playback */
   res = aaxEmitterSetState(emitter, AAX_PLAYING);
   testForState(res, "aaxEmitterStart");

   do
   {
      static int i = 0;

      nanoSleep(9e8);	/* 2.0 seconds */

      switch(i)
      {
      case 0:
         printf("rotate the sensor 90 degrees counter-clockwise\n");
         printf("the emitter is now 5 meters to the left\n");
         SensorAt[0] =  1.0f;
         SensorAt[1] =  0.0f;
         SensorAt[2] =  0.0f;
         res = aaxMatrixSetOrientation(mtx, SensorPos, SensorAt, SensorUp);
         break;
      case 1:
         printf("translate the sensor 5 meters backwards\n");
         printf("the emitter is now 5 meters the left and 5 meters in front\n");
         res = aaxMatrixSetOrientation(mtx, SensorPos, SensorAt, SensorUp);
         res = aaxMatrixTranslate(mtx, 0.0f, 0.0f, -5.0f);
         break;
      case 2:
         printf("Rotate the sensor 90 degrees counter clockwise\n");
         res = aaxMatrixSetOrientation(mtx, SensorPos, SensorAt, SensorUp);
         res = aaxMatrixTranslate(mtx, 0.0f, 0.0f, -5.0f);
         res = aaxMatrixRotate(mtx, -90.0f*GMATH_DEG_TO_RAD, 0.0f, 1.0f, 0.0f);
         break;
      default:
         break;
      }
      res = aaxMatrixInverse(mtx);
      res = aaxSensorSetMatrix(config, mtx);

      if (i == 3) break;
      i++;
   }
   while (1);

   res = aaxEmitterStop(emitter);
   res = aaxEmitterRemoveBuffer(emitter);
   testForState(res, "aaxEmitterRemoveBuffer");

   res = aaxBufferDestroy(buffer);
   testForState(res, "aaxBufferDestroy");

   res = aaxMixerDeregisterEmitter(config, emitter);
   res = aaxMixerSetState(config, AAX_STOPPED);
   res = aaxEmitterDestroy(emitter);

   res = aaxDriverClose(config);
   res = aaxDriverDestroy(config);


   return 0;
}
