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
#include <math.h>

#include <aax.h>
#include <aaxdefs.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define FILE_PATH_LEFT		SRC_PATH"/left_channel.wav"
#define FILE_PATH_RIGHT		SRC_PATH"/right_channel.wav"

aaxVec3f EmitterPos = {-2.0f, 0.0f, 0.0f };
aaxVec3f EmitterDir = { 0.0f, 0.0f, 1.0f };
aaxVec3f EmitterVel = { 0.0f, 0.0f, 0.0f };

aaxVec3f SensorPos = { 0.0f, 0.0f, 0.0f };
aaxVec3f SensorAt = {  0.0f, 0.0f,-1.0f };
aaxVec3f SensorUp = {  0.0f, 1.0f, 0.0f };
aaxVec3f SensorVel = { 0.0f, 0.0f, 0.0f };

#define _neg(x)		x = -x
#define _swap(x, y)	do { float z=x; x=y; y=z; } while (0);

int main(int argc, char **argv)
{
   char *devname, *infile[2] = { FILE_PATH_LEFT, FILE_PATH_RIGHT };
   enum aaxRenderMode mode;
   aaxConfig config;
   int num, res;

   devname = getDeviceName(argc, argv);
   mode = getMode(argc, argv);
   config = aaxDriverOpenByName(devname, mode);
   testForError(config, "No default audio device available.");

   if (config)
   {
      aaxBuffer buffer[2];
      if (1)
      {
         aaxEmitter emitter[2];
         int i, deg = 0;
         aaxMtx4f mtx;

         /** mixer */
         res = aaxMixerInit(config);
         testForState(res, "aaxMixerInit");

         res = aaxMixerSetState(config, AAX_PLAYING);
         testForState(res, "aaxMixerStart");

         /** sensor settings */
         res = aaxMatrixSetOrientation(mtx, SensorPos, SensorAt, SensorUp);
         testForState(res, "aaxSensorSetOrientation");
 
         res = aaxMatrixInverse(mtx);
         testForState(res, "aaxMatrixInverse");

         res = aaxSensorSetMatrix(config, mtx);
         testForState(res, "aaxSensorSetMatrix");

         res = aaxSensorSetVelocity(config, SensorVel);
         testForState(res, "aaxSensorSetVelocity");

         /** emitter */
         num = 2;
         printf("Starting %i emitters\n", num);
         for (i=0; i<num; i++)
         {
            emitter[i] = aaxEmitterCreate();
            testForError(emitter[i], "Unable to create a new emitter\n");

            buffer[i] = bufferFromFile(config, infile[i]);
            testForError(buffer[i], "Failed to load sample file");

            res = aaxEmitterAddBuffer(emitter[i], buffer[i]);
            testForState(res, "aaxEmitterAddBuffer");

            res = aaxEmitterSetMode(emitter[i], AAX_BUFFER_TRACK, i);
            testForState(res, "aaxEmitterSetBufferTrack");

            if (i == 1) _neg(EmitterPos[0]);
            res = aaxMatrixSetDirection(mtx, EmitterPos, EmitterDir);
            testForState(res, "aaxMatrixSetDirection");

            res = aaxEmitterSetMatrix(emitter[i], mtx);
            testForState(res, "aaxSensorSetMatrix");

            res = aaxEmitterSetMode(emitter[i], AAX_POSITION, AAX_ABSOLUTE);
            testForState(res, "aaxEmitterSetMode");

            res = aaxEmitterSetMode(emitter[i], AAX_LOOPING, AAX_TRUE);
            testForState(res, "aaxEmitterSetLooping");

            res = aaxMixerRegisterEmitter(config, emitter[i]);
            testForState(res, "aaxMixerRegisterEmitter");

            /** schedule the emitter for playback */
            res = aaxEmitterSetState(emitter[i], AAX_PLAYING);
            testForState(res, "aaxEmitterStart");
         }

         printf("left / right\n");
         deg = 0;
         while(deg < 360)
         {
            nanoSleep(5e7);
            deg += 5;
         }

         _neg(EmitterPos[0]); /* restore original */
         _swap(EmitterPos[0], EmitterPos[2]);
         for (i=0; i<num; i++)
         {
            if (i == 1) _neg(EmitterPos[2]);
            res = aaxMatrixSetDirection(mtx, EmitterPos, EmitterDir);
            testForState(res, "aaxMatrixSetDirection");

            res = aaxEmitterSetMatrix(emitter[i], mtx);
            testForState(res, "aaxSensorSetMatrix");
         }

#if 1
         printf("front (left) / back (right)\n");
         deg = 0;
         while(deg < 360)
         {
            nanoSleep(5e7);
            deg += 5;
         }
#endif


         //_neg(EmitterPos[2]); /* restore original */
         _swap(EmitterPos[1], EmitterPos[2]);
         for (i=0; i<num; i++)
         {
            if (i == 1) _neg(EmitterPos[1]);
            res = aaxMatrixSetDirection(mtx, EmitterPos, EmitterDir);
            testForState(res, "aaxMatrixSetDirection");

            res = aaxEmitterSetMatrix(emitter[i], mtx);
            testForState(res, "aaxSensorSetMatrix");
         }

#if 1
         printf("front (up) / back (down)\n");
         deg = 0;
         while(deg < 360)
         {
            nanoSleep(5e7);
            deg += 5;
         }
#endif

         i = 0;
         do
         {
            res = aaxEmitterSetState(emitter[i], AAX_STOPPED);
            testForState(res, "aaxEmitterStop");

            res = aaxMixerDeregisterEmitter(config, emitter[i]);
            testForState(res, "aaxMixerDeregisterEmitter");

            res = aaxEmitterDestroy(emitter[i]);
            testForState(res, "aaxEmitterDestroy");

            res = aaxBufferDestroy(buffer[i]);
            testForState(res, "aaxBufferDestroy");
         } while (++i < num);

         res = aaxMixerSetState(config, AAX_STOPPED);
         testForState(res, "aaxMixerStop");
      }

   }

   res = aaxDriverClose(config);
   testForState(res, "aaxDriverClose");

   res = aaxDriverDestroy(config);
   testForState(res, "aaxDriverDestroy");

   return 0;
}
