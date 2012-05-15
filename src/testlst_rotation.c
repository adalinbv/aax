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
#include <math.h>

#include <aaxdefs.h>

#include "base/types.h"
#include "base/geometry.h"
#include "driver.h"
#include "wavfile.h"

#define FILE_PATH               SRC_PATH"/tictac.wav"

aaxVec3f SourcePos = { 0.0f, 0.0f, 10.0f };
aaxVec3f SourceDir = { 0.0f, 0.0f,  1.0f };
aaxVec3f SourceVel = { 0.0f, 0.0f,  0.0f };

aaxVec3f ListenerPos = { 0.0f, 0.0f, -5.0f };
aaxVec3f ListenerAt = {  0.0f, 0.0f, -1.0f };
aaxVec3f ListenerUp = {  0.0f, 1.0f,  0.0f };
aaxVec3f ListenerVel = { 0.0f, 0.0f,  0.0f };

int main(int argc, char **argv)
{
   char *devname, *infile;
   aaxConfig config;
   int res;

   infile = getInputFile(argc, argv, FILE_PATH);
   devname = getDeviceName(argc, argv);

   config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
   testForError(config, "No default audio device available.");

   if (config)
   {
      aaxBuffer buffer = bufferFromFile(config, infile);
      if (buffer)
      {
         aaxEmitter emitter;
         int deg = 0;
         float ang;
         aaxMtx4f mtx;

         /** mixer */
         res = aaxMixerInit(config);
         testForState(res, "aaxMixerInit");

         res = aaxMixerSetState(config, AAX_PLAYING);
         testForState(res, "aaxMixerStart");

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

         res = aaxEmitterSetMode(emitter, AAX_POSITION, AAX_ABSOLUTE);
         testForState(res, "aaxEmitterSetMode");

         res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
         testForState(res, "aaxEmitterSetLooping");

         res = aaxMatrixSetDirection(mtx, SourcePos, SourceDir);
         testForState(res, "aaxMatrixSetDirection");

         res = aaxEmitterSetMatrix(config, mtx);
         testForState(res, "aaxEmitterSetMatrix");

         res = aaxMixerRegisterEmitter(config, emitter);
         testForState(res, "aaxMixerRegisterEmitter");

         /** schedule the emitter for playback */
         res = aaxEmitterSetState(emitter, AAX_PLAYING);
         testForState(res, "aaxEmitterStart");

         deg = 0;
         while(deg < 360)
         {
            nanoSleep(5e7);

            ang = (float)deg / 180.0f * GMATH_PI;
            ListenerAt[0] = sinf(ang);
            ListenerAt[2] = -cosf(ang);

#if 1
            printf("deg: %03u\tdir (% f, % f, % f)\n", deg,
                     ListenerAt[0], ListenerAt[1], ListenerAt[2]);
#endif

            res = aaxMatrixSetOrientation(mtx, ListenerPos, ListenerAt,
                                                            ListenerUp);
            testForState(res, "aaxMatrixSetOrientation");

            res = aaxMatrixInverse(mtx);
            res |= aaxSensorSetMatrix(config, mtx);
            testForState(res, "aaxSensorSetMatrix");
            deg += 3;
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
