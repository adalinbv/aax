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
   float pitch;
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
         int state;

         /** mixer */
         res = aaxMixerInit(config);
         testForState(res, "aaxMixerInit");

         res = aaxMixerSetState(config, AAX_PLAYING);
         testForState(res, "aaxMixerStart");

         /** emitter */
         emitter = aaxEmitterCreate();
         testForError(emitter, "Unable to create a new emitter\n");

         res = aaxEmitterAddBuffer(emitter, buffer);
         testForState(res, "aaxEmitterAddBuffer");

         res = aaxEmitterSetMode(emitter, AAX_POSITION, AAX_RELATIVE);
         testForState(res, "aaxEmitterSetMode");

         res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_FALSE);
         testForState(res, "aaxEmitterSetLooping");

         pitch = getPitch(argc, argv);
         res = aaxEmitterSetPitch(emitter, pitch);
         testForState(res, "aaxEmitterSetPitch");

         res = aaxMixerRegisterEmitter(config, emitter);
         testForState(res, "aaxMixerRegisterEmitter");

         /** schedule the emitter for playback */
         res = aaxEmitterSetState(emitter, AAX_PLAYING);
         testForState(res, "aaxEmitterStart");

         do
         {
            nanoSleep(5e7);
            state = aaxEmitterGetState(emitter);
         }
         while (state == AAX_PLAYING);

         res = aaxEmitterSetState(emitter, AAX_STOPPED);
         testForState(res, "aaxEmitterStop");

         printf("emitter stopped\n");
         state = 0;
         do {
            nanoSleep(5e7);
         } while (state++ < 50);

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
