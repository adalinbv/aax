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

#define FILE_PATH               SRC_PATH"/wasp.wav"
#define FFRAME			400.0
#define DEG			(360.0/2)

aaxVec3f NullPos = { 0.0f, 0.0f, 0.0f };
aaxVec3f NullDir = { 0.0f, 0.0f, 0.0f };
aaxVec3f NulVel = { 0.0f, 0.0f, 0.0f };

aaxVec3f EmitterPos = { 0.0f, 0.0f, -1.0f };
aaxVec3f EmitterDir = { 0.0f, 0.0f, 1.0f };
aaxVec3f EmitterVel = { 0.0f, 0.0f, 0.0f };

aaxVec3f FramePos = { 10000.0, -1000.0, -10.0f };
aaxVec3f FrameAt = {  0.0f, 0.0f,  1.0f };
aaxVec3f FrameUp = {  0.0f, 1.0f,  0.0f };
aaxVec3f FrameVel = { 0.0f, 0.0f,  0.0f };

aaxVec3f SensorPos = { 10000.0, -1000.0,  0.0f };
aaxVec3f SensorAt = {  0.0f, 0.0f, -1.0f };
aaxVec3f SensorUp = {  0.0f, 1.0f,  0.0f };
aaxVec3f SensorVel = { 0.0f, 0.0f,  0.0f };

#define MIXER		0
#define FRAME		(MIXER+1)

int main(int argc, char **argv)
{
   char *devname, *infile;
   enum aaxRenderMode mode;
   aaxConfig config;
   aaxFrame frame;
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
         float ang, pitch, r;
         aaxEmitter emitter[2];
         aaxMtx4f mtx;
         int deg[2];
         int state;

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
         testForState(res, "aaxSensorSetOrientation");

         res = aaxSensorSetVelocity(config, SensorVel);
         testForState(res, "aaxSensorSetVelocity");

         /** audio frame */
         frame = aaxAudioFrameCreate(config);
         testForError(frame, "Unable to create a new audio frame\n");

         res = aaxMatrixSetOrientation(mtx, FramePos, FrameAt, FrameUp);
         testForState(res, "aaxAudioFrameSetOrientation");

         res = aaxAudioFrameSetMatrix(frame, mtx);
         testForState(res, "aaxAudioFrameSetMatrix");

         res = aaxAudioFrameSetMode(frame, AAX_POSITION, AAX_ABSOLUTE);
         testForState(res, "aaxAudioFrameSetMode");

         res = aaxAudioFrameSetVelocity(frame, FrameVel);
         testForState(res, "aaxAudioFrameSetVelocity");

         pitch = getPitch(argc, argv);
         res = aaxAudioFrameSetPitch(frame, pitch);
         testForState(res, "aaxAudioFrameSetPitch");

#if 0
         /** don't register audio frame */
         res = aaxMixerRegisterAudioFrame(config, frame);
         testForState(res, "aaxMixerRegisterAudioFrame");
#endif

         /** schedule the audioframe for playback */
         res = aaxAudioFrameSetState(frame, AAX_PLAYING);
         testForState(res, "aaxAudioFrameStart");


         /** emitter attached to the mixer */
         emitter[MIXER] = aaxEmitterCreate();
         testForError(emitter[MIXER], "Unable to create a new emitter\n");

         res = aaxMatrixSetDirection(mtx, EmitterPos, EmitterDir);
         testForState(res, "aaxMatrixSetDirection");

         res = aaxEmitterSetMatrix(emitter[MIXER], mtx);
         testForState(res, "aaxEmitterSetMatrix");

         res = aaxEmitterSetMode(emitter[MIXER], AAX_POSITION, AAX_RELATIVE);
         testForState(res, "aaxEmitterSetMode");

         res = aaxEmitterSetMode(emitter[MIXER], AAX_LOOPING, AAX_FALSE);
         testForState(res, "aaxEmitterSetLooping");

         /** register the emitter to the audio-frame */
         res = aaxMixerRegisterEmitter(config, emitter[MIXER]);
         testForState(res, "aaxMixerRegisterEmitter");

         /** emitter attached to the frame */
         emitter[FRAME] = aaxEmitterCreate();
         testForError(emitter[FRAME], "Unable to create a new emitter\n");

         res = aaxMatrixSetDirection(mtx, NullPos, NullDir);
         testForState(res, "aaxMatrixSetDirection");

         res = aaxEmitterSetMatrix(emitter[FRAME], mtx);
         testForState(res, "aaxEmitterSetMatrix");

         res = aaxEmitterAddBuffer(emitter[FRAME], buffer);
         testForState(res, "aaxEmitterAddBuffer");

         res = aaxEmitterSetMode(emitter[FRAME], AAX_POSITION, AAX_ABSOLUTE);
         testForState(res, "aaxEmitterSetMode");

         res = aaxEmitterSetMode(emitter[FRAME], AAX_LOOPING, AAX_TRUE);
         testForState(res, "aaxEmitterSetLooping");

         /** register the emitter to the audio-frame */
         res = aaxAudioFrameRegisterEmitter(frame, emitter[FRAME]);
         testForState(res, "aaxMixerRegisterEmitter");

         /** schedule the emitter for playback */
         res = aaxEmitterSetState(emitter[FRAME], AAX_PLAYING);
         testForState(res, "aaxEmitterStart");

         printf("\nAudioFrame detached from the Mixer\n");
         printf("\n---------------------------------------------------\n\n");
         printf("Emiter moving around using audio frame buffers\n");
         r = 10.0;
         deg[MIXER] = deg[FRAME]  = 0;
         do
         {
            static float dt = 0.0;
            static int play = 0;
            aaxBuffer buf;

            nanoSleep(5e5);

            if (deg[FRAME] < 360)
            {
               /*
                * generate time-frames at a faster rate and add them to the
                * mixer-attached emitter
                */
               buf = aaxAudioFrameGetBuffer(frame);
               if (buf == NULL)
               {
                  res = aaxAudioFrameSetState(frame, AAX_UPDATE);
                  testForState(res, "aaxAudioFrameUpdate");
               }
               else
               {
                  res = aaxEmitterAddBuffer(emitter[MIXER], buf);
                  testForState(res, "aaxEmitterAddBuffer");

                  if (!play)
                  {
                     /** schedule the emitter for playback */
                     res = aaxEmitterSetState(emitter[MIXER], AAX_PLAYING);
                     testForState(res, "aaxEmitterStart");
                     play = 1;
                  }
                  deg[FRAME] += 3;
               }
            }

            dt += 5e5*1e-9;
            if (dt >= 5e7*1e-9)
            {
               /*
                * stream the pre-generated buffers at a slower rate
                */
               ang = (float)deg[MIXER] / 180.0f * GMATH_PI;
               FramePos[0] = 10000.0 +r * sinf(ang);
               FramePos[2] = -r * cosf(ang);
#if 1
# if 0
               printf("buffers playing: %i, processed: %i, total: %i\n", 
                  aaxEmitterGetNoBuffers(emitter[MIXER], AAX_PLAYING),
                  aaxEmitterGetNoBuffers(emitter[MIXER], AAX_PROCESSED),
                  aaxEmitterGetNoBuffers(emitter[MIXER], AAX_MAXIMUM));
# endif
               printf("frame deg: %03u\tpos (% f, % f, % f)\n", deg[MIXER],
                        FramePos[0], FramePos[1], FramePos[2]);
#endif
               res= aaxMatrixSetOrientation(mtx, FramePos, FrameAt, FrameUp);
               testForState(res, "aaxMatrixSetDirection");

               res = aaxEmitterSetMatrix(emitter[MIXER], mtx);
               testForState(res, "aaxAudioFrameSetMatrix");

               deg[MIXER] += 3;
               dt -= 5e7*1e-9;
            }

            if (play) state = aaxEmitterGetState(emitter[MIXER]);
            else state = AAX_PLAYING;
         }
         while (state == AAX_PLAYING);

         res = aaxAudioFrameSetState(frame, AAX_STOPPED);
         testForState(res, "aaxAudioFrameStop");

#if 0
         /* frame was not registered */
         res = aaxMixerDeregisterAudioFrame(config, frame);
         testForState(res, "aaxMixerDeregisterAudioFrame");
#endif

         res = aaxEmitterSetState(emitter[MIXER], AAX_STOPPED);
         testForState(res, "aaxEmitterStop");

         res = aaxMixerDeregisterEmitter(config, emitter[MIXER]);
         testForState(res, "aaxMixerDeregisterEmitter");

         res = aaxEmitterDestroy(emitter[MIXER]);
         testForState(res, "aaxEmitterDestroy");

         res = aaxEmitterSetState(emitter[FRAME], AAX_STOPPED);
         testForState(res, "aaxEmitterStop");

         res = aaxAudioFrameDeregisterEmitter(frame, emitter[FRAME]);
         testForState(res, "aaxMixerDeregisterEmitter");

         res = aaxEmitterDestroy(emitter[FRAME]);
         testForState(res, "aaxEmitterDestroy");

         res = aaxAudioFrameDestroy(frame);
         testForState(res, "aaxAudioFrameStop");

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
