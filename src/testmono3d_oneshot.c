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

#define FILE_PATH               SRC_PATH"/wasp.wav"

int main(int argc, char **argv)
{
   char *devname, *infile;
   aaxConfig config;
   float gain, pitch;
   int num, res;

   infile = getInputFile(argc, argv, FILE_PATH);
   devname = getDeviceName(argc, argv);

   config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
   testForError(config, "No default audio device available.");

   if (config)
   {
      aaxBuffer buffer = bufferFromFile(config, infile);
      if (buffer)
      {
         aaxEmitter emitter[256];
         int i, state;

         /** mixer */
         res = aaxMixerInit(config);
         testForState(res, "aaxMixerInit");

         gain = getGain(argc, argv);
         pitch = getPitch(argc, argv);
         num = getNumSources(argc, argv);
         printf("Starting %i emitters. gain = %f, pitch = %f\n", num, gain, pitch);
         i = 0;
         do
         {
            /** emitters */
            emitter[i] = aaxEmitterCreate();
            testForError(emitter[i], "Unable to create a new emitter\n");

            res = aaxEmitterAddBuffer(emitter[i], buffer);
            testForState(res, "aaxEmitterAddBuffer");

            res = aaxEmitterSetMode(emitter[i], AAX_POSITION, AAX_RELATIVE);
            testForState(res, "aaxEmitterSetMode");

            res = aaxEmitterSetMode(emitter[i], AAX_LOOPING, AAX_FALSE);
            testForState(res, "aaxEmitterSetLooping");

            res = aaxEmitterSetPitch(emitter[i], pitch);
            testForState(res, "aaxEmitterSetPitch");

            res = aaxEmitterSetGain(emitter[i], gain);
            testForState(res, "aaxEmitterSetGain");

            res = aaxMixerRegisterEmitter(config, emitter[i]);
            testForState(res, "aaxMixerRegisterEmitter");

            /** schedule the emitter for playback */
            res = aaxEmitterSetState(emitter[i], AAX_PLAYING);
            testForState(res, "aaxEmitterStart");
         }
         while (++i < num);

         res = aaxMixerSetState(config, AAX_PLAYING);
         testForState(res, "aaxMixerStart");

         do
         {
            nanoSleep(5e7);
            state = aaxEmitterGetState(emitter[0]);
         }
         while (state == AAX_PLAYING);

         i = 0;
         do
         {
            res = aaxEmitterSetState(emitter[i], AAX_STOPPED);
            testForState(res, "aaxEmitterStop");
         }
         while (++i < num);

         printf("emitter stopped\n");
         state = 0;
         do {
            nanoSleep(5e7);
            res = aaxEmitterGetState(emitter[0]);
         } while ((res != AAX_PROCESSED) && (state++ < 50));

         i = 0;
         do
         {
            res = aaxMixerDeregisterEmitter(config, emitter[i]);
            testForState(res, "aaxMixerDeregisterEmitter");

            res = aaxEmitterDestroy(emitter[i]);
            testForState(res, "aaxEmitterDestroy");
         }
         while (++i < num);

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
