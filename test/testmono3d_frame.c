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

#define FRAMES			4
#define SUBFRAMES		4
#define RADIUS			15
#define FILE_PATH               SRC_PATH"/tictac.wav"

aaxVec3f EmitterPos = { 0.0, 0.0, 0.0 };
aaxVec3f EmitterDir = { 0.0, 0.0, 1.0 };
aaxVec3f EmitterVel = { 0.0, 0.0, 0.0 };

aaxVec3f FramePos = { 0.0,  0.0,   0.0f };
aaxVec3f FrameAt = {  0.0f, 0.0f,  1.0f };
aaxVec3f FrameUp = {  0.0f, 1.0f,  0.0f };
aaxVec3f FrameVel = { 0.0f, 0.0f,  0.0f };


aaxVec3f SensorPos = { 10000.0, -1000.0, 0.0 };
aaxVec3f SensorAt = {  0.0f, 0.0f, -1.0f };
aaxVec3f SensorUp = {  0.0f, 1.0f, 0.0f };
aaxVec3f SensorVel = { 0.0f, 0.0f, 0.0f };

int main(int argc, char **argv)
{
   aaxFrame subframe[FRAMES][SUBFRAMES];
   aaxFrame frame[FRAMES];
   char *devname, *infile;
   enum aaxRenderMode mode;
   aaxConfig config;
   int num, res;

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
         aaxEmitter emitter[256];
         float pitch, anglestep;
         int i, j, deg = 0;
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


         /** audio frame */
         pitch = getPitch(argc, argv);
         num = ((getNumSources(argc, argv)-1)/(FRAMES*SUBFRAMES))+1;
         printf("starting %i frames with %i subframes ", FRAMES, SUBFRAMES);
         printf("and %i emitters per subframe\nfor a total of %i emitters\n", num, num*SUBFRAMES*FRAMES);

         res = aaxMatrixSetOrientation(mtx, FramePos, FrameAt, FrameUp);
         testForState(res, "aaxAudioFrameSetOrientation");

         for (j=0; j<FRAMES; j++)
         {
            int k;

            frame[j] = aaxAudioFrameCreate(config);
            testForError(frame[j], "Unable to create a new audio frame\n");

            res = aaxAudioFrameSetMatrix(frame[j], mtx);
            testForState(res, "aaxAudioFrameSetMatrix");

            res = aaxAudioFrameSetMode(frame[j], AAX_POSITION, AAX_RELATIVE);
            testForState(res, "aaxAudioFrameSetMode");

            res = aaxAudioFrameSetVelocity(frame[j], FrameVel);
            testForState(res, "aaxAudioFrameSetVelocity");

            /** register audio frame */
            res = aaxMixerRegisterAudioFrame(config, frame[j]);
            testForState(res, "aaxMixerRegisterAudioFrame");

            /** schedule the audioframe for playback */
            res = aaxAudioFrameSetState(frame[j], AAX_PLAYING);
            testForState(res, "aaxAudioFrameStart");

            for (k=0; k<SUBFRAMES; k++)
            {
               subframe[j][k] = aaxAudioFrameCreate(config);
               testForError(subframe[j][k], "Unable to create a new sub-frame\n");

               res = aaxAudioFrameSetMatrix(subframe[j][k], mtx);
               testForState(res, "aaxAudioFrameSetMatrix");

               res = aaxAudioFrameSetMode(subframe[j][k], AAX_POSITION, AAX_RELATIVE);
               testForState(res, "aaxAudioFrameSetMode");

               res = aaxAudioFrameSetVelocity(subframe[j][k], FrameVel);
               testForState(res, "aaxAudioFrameSetVelocity");

               /** register audio frame */
               res = aaxAudioFrameRegisterAudioFrame(frame[j], subframe[j][k]);
               testForState(res, "aaxAudioFrameRegisterAudioFrame");

               /** schedule the audioframe for playback */
               res = aaxAudioFrameSetState(subframe[j][k], AAX_PLAYING);
               testForState(res, "aaxAudioFrameStart");

               /** emitter */
               /* Set sources to located in a circle around the listener */
               anglestep = (2 * 3.1416) / (float)(FRAMES*SUBFRAMES*num);
               printf("Starting %i emitters\n", num);
               i = 0;
               do
               {
                  unsigned int p = (j*FRAMES+k)*num + i;
                  static float mul = 1.0;
                  aaxVec3f pos;

                  emitter[p] = aaxEmitterCreate();
                  testForError(emitter[p], "Unable to create a new emitter\n");

                  res = aaxEmitterAddBuffer(emitter[p], buffer);
                  testForState(res, "aaxEmitterAddBuffer");

                  pos[1] = EmitterPos[1] + cos(anglestep * p) * RADIUS;
                  pos[0] = EmitterPos[0] + mul*cos(anglestep * p) * RADIUS;
                  pos[2] = EmitterPos[2] + sin(anglestep * p) * RADIUS;
                  aaxMatrixSetDirection(mtx, pos, EmitterDir);
                  res = aaxEmitterSetMatrix(emitter[p], mtx);
                  testForState(res, "aaxEmitterSetIdentityMatrix");
                  mul *= -1.0;

                  res = aaxEmitterSetMode(emitter[p], AAX_POSITION, AAX_ABSOLUTE);
                  testForState(res, "aaxEmitterSetMode");

                  res = aaxEmitterSetMode(emitter[p], AAX_LOOPING, AAX_TRUE);
                  testForState(res, "aaxEmitterSetLooping");

                  res = aaxEmitterSetReferenceDistance(emitter[p], 3.3f);
                  testForState(res, "aaxEmitterSetReferenceDistance");

                  res = aaxEmitterSetPitch(emitter[p], pitch);
                  testForState(res, "aaxEmitterSetPitch");

                  /** register the emitter to the audio-frame */
                  res = aaxAudioFrameRegisterEmitter(subframe[j][k], emitter[p]);
                  testForState(res, "aaxMixerRegisterEmitter");

                  /** schedule the emitter for playback */
                  res = aaxEmitterSetState(emitter[p], AAX_PLAYING);
                  testForState(res, "aaxEmitterStart");

                  nanoSleep(7e7);
               }
               while (++i < num);

               nanoSleep(5e7);
            }
         }

         deg = 0;
         while(deg < 360)
         {
            nanoSleep(5e7);
            deg += 1;
         }

         for (j=0; j<FRAMES; j++)
         {
            int k;

            res = aaxAudioFrameSetState(frame[j], AAX_STOPPED);
            testForState(res, "aaxAudioFrameStop");

            for (k=0; k<SUBFRAMES; k++)
            {
                res = aaxAudioFrameSetState(subframe[j][k], AAX_STOPPED);
                testForState(res, "aaxAudioFrameStop");           

               for(i=0; i<num; i++)
               {
                  unsigned int p = (j*FRAMES+k)*num + i;
                  res = aaxEmitterSetState(emitter[p], AAX_STOPPED);
                  testForState(res, "aaxEmitterStop");

                  res = aaxAudioFrameDeregisterEmitter(subframe[j][k], emitter[p]);
                  testForState(res, "aaxMixerDeregisterEmitter");

                  res = aaxEmitterDestroy(emitter[p]);
                  testForState(res, "aaxEmitterDestroy");
               }

               res = aaxAudioFrameDeregisterAudioFrame(frame[j], subframe[j][k]);
               testForState(res, "aaxMixerDeregisterAudioFrame");

               res = aaxAudioFrameDestroy(subframe[j][k]);
               testForState(res, "aaxAudioFrameStop");
            }

            res = aaxMixerDeregisterAudioFrame(config, frame[j]);
               testForState(res, "aaxMixerDeregisterAudioFrame");

               res = aaxAudioFrameDestroy(frame[j]);
               testForState(res, "aaxAudioFrameStop");
         }

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
