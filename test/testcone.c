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

#include <aax.h>
#include <aaxdefs.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define FILE_PATH               SRC_PATH"/tictac.wav"

aaxVec3f SourcePos = { 0.0, 0.0, 10.0 };
aaxVec3f SourceDir = { 0.0, 0.0, 1.0 };
aaxVec3f SourceVel = { 0.0, 0.0, 0.0 };

aaxVec3f ListenerPos = { 0.0,  0.0, 0.0 };
aaxVec3f ListenerAt = {  0.0f, 0.0f, -1.0f };
aaxVec3f ListenerUp = {  0.0f, 1.0f, 0.0f };
aaxVec3f ListenerVel = { 0.0f, 0.0f, 0.0f };
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
         float ang, pitch;
         aaxEmitter emitter;
         aaxMtx4f mtx;
         int deg = 0;

         /** mixer */
         res = aaxMixerInit(config);
         testForState(res, "aaxMixerInit");

         res = aaxMixerSetState(config, AAX_PLAYING);
         testForState(res, "aaxMixerStart");

         /** sensor settings */
         res=aaxMatrixSetOrientation(mtx, ListenerPos, ListenerAt, ListenerUp);
         testForState(res, "aaxSensorSetOrientation");
 
         res = aaxMatrixInverse(mtx);
         testForState(res, "aaxMatrixInverse");

         res = aaxSensorSetMatrix(config, mtx);
         testForState(res, "aaxSensorSetMatrix");

         res = aaxSensorSetVelocity(config, ListenerVel);
         testForState(res, "aaxSensorSetVelocity");

         /** emitter */
         pitch = getPitch(argc, argv);
         emitter = aaxEmitterCreate();
         testForError(emitter, "Unable to create a new emitter\n");

         res = aaxEmitterAddBuffer(emitter, buffer);
         testForState(res, "aaxEmitterAddBuffer");

         res = aaxEmitterSetIdentityMatrix(emitter);
         testForState(res, "aaxEmitterSetIdentityMatrix");

         res = aaxEmitterSetMode(emitter, AAX_POSITION, AAX_ABSOLUTE);
         testForState(res, "aaxEmitterSetMode");

         res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
         testForState(res, "aaxEmitterSetLooping");

         res = aaxEmitterSetPitch(emitter, pitch);
         testForState(res, "aaxEmitterSetPitch");

         res = aaxEmitterSetAudioCone(emitter, 160.0*GMATH_DEG_TO_RAD,
                                               200.0*GMATH_DEG_TO_RAD, 0.0);
         testForState(res, "aaxEmitterSetAudioCone");

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
            SourceDir[0] = sinf(ang);
            SourceDir[2] = -cosf(ang);
            /* SourceDir[2] = cosf(ang); */
#if 1
            printf("deg: %03u\tdir (% f, % f, % f)\n", deg,
                     SourceDir[0], SourceDir[1], SourceDir[2]);
#endif
            res = aaxMatrixSetDirection(mtx, SourcePos, SourceDir);
            testForState(res, "aaxMatrixSetDirection");

            res = aaxEmitterSetMatrix(emitter, mtx);
            testForState(res, "aaxSensorSetMatrix");

            deg += 1;
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
