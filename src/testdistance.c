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

/* 50m/s = about 100kts */
#define INITIAL_DIST		300.0f
#define SPEED			15.0f
#define TIME			20.0f
#define STEP			(((2*INITIAL_DIST)/fabs(SPEED))/TIME)
#define DELAY			fabs((STEP/TIME)*1e9)
#define FILE_PATH               SRC_PATH"/wasp.wav"

aaxVec3f SourcePos = {  0.0f,    100.0f, -INITIAL_DIST };
aaxVec3f SourceDir = {  1.0f,    0.0f,   0.0f };
aaxVec3f SourceVel = {  0.0f,    0.0f,   SPEED };

aaxVec3f ListenerPos = { 0.0f, 0.0f,  0.0f };
aaxVec3f ListenerVel = { 0.0f, 0.0f,  0.0f };
aaxVec3f ListenerAt =  { 0.0f, 0.0f, -1.0f };
aaxVec3f ListenerUp =  { 0.0f, 1.0f,  0.0f };

int main(int argc, char **argv)
{
   char *devname, *infile;
   enum aaxRenderMode mode;
   aaxConfig config;
   int res;

   infile = getInputFile(argc, argv, FILE_PATH);
   devname = getDeviceName(argc, argv);
   mode = getMode(argc, argv);
   config = aaxDriverOpenByName(devname, mode);
   testForError(config, "No default audio device available.");

   if (config)
   {
      aaxBuffer buffer = bufferFromFile(config, infile);
      if (buffer)
      {
         aaxEmitter emitter;
         float dist;
         aaxMtx4f mtx;

         /** mixer */
         res = aaxMixerInit(config);
         testForState(res, "aaxMixerInit");

         res = aaxMixerSetState(config, AAX_PLAYING);
         testForState(res, "aaxMixerStart");

         /** scenery settings */
         res=aaxScenerySetDistanceModel(config, AAX_EXPONENTIAL_DISTANCE_DELAY);
         testForState(res, "aaxScenerySetDistanceModel");

         /** dopller settings */
         res = aaxScenerySetSoundVelocity(config, 333.0);
         testForState(res, "aaxScenerySetSoundVelocity");

         /** sensor settings */
         res=aaxMatrixSetOrientation(mtx, ListenerPos, ListenerAt, ListenerUp);
         testForState(res, "aaxMatrixSetOrientation");

         res = aaxMatrixInverse(mtx);
         res |= aaxSensorSetMatrix(config, mtx);
         testForState(res, "aaxSensorSetMatrix");

         res = aaxSensorSetVelocity(config, ListenerVel);
         testForState(res, "aaxSensorSetVelocity");

         /** emitter */
         emitter = aaxEmitterCreate();
         testForError(emitter, "Unable to create a new emitter\n");

         res = aaxEmitterAddBuffer(emitter, buffer);
         testForState(res, "aaxEmitterAddBuffer");

         res = aaxEmitterSetMode(emitter, AAX_POSITION, AAX_RELATIVE);
         testForState(res, "aaxEmitterSetMode");

         res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
         testForState(res, "aaxEmitterSetLooping");

         res = aaxEmitterSetVelocity(emitter, SourceVel);
         testForState(res, "aaxEmitterSetVelocity");

         res = aaxEmitterSetReferenceDistance(emitter, 50.0f);
         testForState(res, "aaxEmitterSetReferenceDistance");

         res = aaxEmitterSetMaxDistance(emitter, 500.0f);
         testForState(res, "aaxEmitterSetMaxDistance");

         res = aaxMatrixSetDirection(mtx, SourcePos, SourceDir);
         testForState(res, "aaxMatrixSetDirection");

         res = aaxEmitterSetMatrix(emitter, mtx);
         testForState(res, "aaxEmitterSetMatrix");

         res = aaxMixerRegisterEmitter(config, emitter);
         testForState(res, "aaxMixerRegisterEmitter");

         /** schedule the emitter for playback */
         printf("Engine start\n");
         res = aaxEmitterSetState(emitter, AAX_PLAYING);
         testForState(res, "aaxEmitterStart");

         dist = INITIAL_DIST;
         while(dist > -INITIAL_DIST)
         {
            nanoSleep(DELAY);

            SourcePos[2] = -dist;
            dist -= STEP;
#if 1
            printf("dist: %5.4f\tpos (% f, % f, % f)\n", dist,
                     SourcePos[0], SourcePos[1], SourcePos[2]);
#endif
            res = aaxMatrixSetDirection(mtx, SourcePos, SourceDir);
            testForState(res, "aaxMatrixSetDirection");

            res = aaxEmitterSetMatrix(emitter, mtx);
            testForState(res, "aaxEmitterSetMatrix");
         }

         res = aaxEmitterSetState(emitter, AAX_STOPPED);
         testForState(res, "aaxEmitterStop");

         res = aaxMixerDeregisterEmitter(config, emitter);
         testForState(res, "aaxMixerDeregisterEmitter");

         res = aaxEmitterDestroy(emitter);
         testForState(res, "aaxEmitterDestroy");

         res = aaxBufferDestroy(buffer);
         testForState(res, "aaxBufferDestroy");

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
