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

#define FILE_PATH		SRC_PATH"/stereo.wav"

int main(int argc, char **argv)
{
   char *devname, *infile;
   aaxConfig config;
   int num, res;

   infile = getInputFile(argc, argv, FILE_PATH);
   devname = getDeviceName(argc, argv);
   num = getNumSources(argc, argv);
   if (num>256) num = 256;

   config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
   testForError(config, "No default audio device available.");

   do {
      aaxBuffer buffer = bufferFromFile(config, infile);
      if (buffer)
      {
         aaxEmitter emitter[256];
         float dt = 0.0f;
         int q, state;
         float pitch;

         /** mixer */
         res = aaxMixerInit(config);
         testForState(res, "aaxMixerInit");

         res = aaxMixerSetState(config, AAX_PLAYING);
         testForState(res, "aaxMixerStart");

         /** emitter */
         pitch = getPitch(argc, argv);
         for (q=0; q<num; q++)
         {
            emitter[q] = aaxEmitterCreate();
            testForError(emitter[q], "Unable to create a new emitter");

            res = aaxEmitterSetPitch(emitter[q], pitch);
            testForState(res, "aaxEmitterSetPitch");

            res = aaxEmitterAddBuffer(emitter[q], buffer);
            testForState(res, "aaxEmitterAddBuffer");

            res = aaxMixerRegisterEmitter(config, emitter[q]);
            testForState(res, "aaxMixerRegisterEmitter");

            /** schedule the emitter for playback */
            res = aaxEmitterSetState(emitter[q], AAX_PLAYING);
            testForState(res, "aaxEmitterStart");
         }

         q = 0;
         do
         {
            nanoSleep(5e7);
            dt += 5e7*1e-9;
#if 1
            q++;
            if (q > 10)
            {
               unsigned long offs, offs_bytes;
               float off_s;
               q = 0;

               off_s = aaxEmitterGetOffsetSec(emitter[q]);
               offs = aaxEmitterGetOffset(emitter[q], AAX_SAMPLES);
               offs_bytes = aaxEmitterGetOffset(emitter[q], AAX_BYTES);
               printf("playing time: %5.2f, buffer position: %5.2f (%li samples/ %li bytes)\n", dt, off_s, offs, offs_bytes);
            }
#endif
            state = aaxEmitterGetState(emitter[0]);
         }
         while (state == AAX_PLAYING);

         for (q=0; q<num; q++) {
            res = aaxMixerDeregisterEmitter(config, emitter[q]);
         }
         res = aaxMixerSetState(config, AAX_STOPPED);
         res = aaxEmitterDestroy(emitter);
         res = aaxBufferDestroy(buffer);
      }
   }
   while (0);

   res = aaxDriverClose(config);
   res = aaxDriverDestroy(config);


   return 0;
}
