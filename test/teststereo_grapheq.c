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
   aaxBuffer buffer = 0;
   aaxConfig config;
   int res;

   infile = getInputFile(argc, argv, FILE_PATH);
   devname = getDeviceName(argc, argv);

   config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
   testForError(config, "No default audio device available.");
   do {
      buffer = bufferFromFile(config, infile);
      if (buffer)
      {
         aaxEmitter emitter;
         float dt = 0.0f;
         int q, state;
         float pitch;
         aaxFilter f;

         printf("\nPlayback stereo with 8-band graphic equalizer enabled.\n");
         printf("67Hz | 150Hz | 340Hz | 763Hz | 1.7kHz | 3.9kHz | 8.7kHz\n\n");
         /** emitter */
         emitter = aaxEmitterCreate();
         testForError(emitter, "Unable to create a new emitter");

         pitch = getPitch(argc, argv);
         res = aaxEmitterSetPitch(emitter, pitch);
         testForState(res, "aaxEmitterSetPitch");

         res = aaxEmitterAddBuffer(emitter, buffer);
         testForState(res, "aaxEmitterAddBuffer");

         /** mixer */
         res = aaxMixerInit(config);
         testForState(res, "aaxMixerInit");

         res = aaxMixerRegisterEmitter(config, emitter);
         testForState(res, "aaxMixerRegisterEmitter");

         res = aaxMixerSetState(config, AAX_PLAYING);
         testForState(res, "aaxMixerStart");

         /* equalizer */
         f = aaxFilterCreate(config, AAX_GRAPHIC_EQUALIZER);
         testForError(f, "aaxFilterCreate");

         f = aaxFilterSetSlot(f, 0, AAX_LINEAR, 1.0, 0.2, 0.1, 0.1);
         testForError(f, "aaxFilterSetSlot/0");

         f = aaxFilterSetSlot(f, 1, AAX_LINEAR, 0.2, 0.5, 1.0, 0.5);
         testForError(f, "aaxFilterSetSlot/1");

         f = aaxFilterSetState(f, AAX_TRUE);
         testForError(f, "aaxFilterSetState");

         res = aaxMixerSetFilter(config, f);
         testForState(res, "aaxMixerSetFilter");

         res = aaxFilterDestroy(f);
         testForState(res, "aaxFilterDestroy");

         /** schedule the emitter for playback */
         res = aaxEmitterSetState(emitter, AAX_PLAYING);
         testForState(res, "aaxEmitterStart");

         q = 0;
         do
         {            nanoSleep(5e7);
            dt += 5e7*1e-9;
#if 1
            q++;
            if (q > 10)
            {
               unsigned long offs, offs_bytes;
               float off_s;
               q = 0;

               off_s = aaxEmitterGetOffsetSec(emitter);
               offs = aaxEmitterGetOffset(emitter, AAX_SAMPLES);
               offs_bytes = aaxEmitterGetOffset(emitter, AAX_BYTES);
               printf("playing time: %5.2f, buffer position: %5.2f (%li samples/ %li bytes)\n", dt, off_s, offs, offs_bytes);
            }
#endif
            state = aaxEmitterGetState(emitter);
         }
         while (state == AAX_PLAYING);

         res = aaxMixerDeregisterEmitter(config, emitter);
         res = aaxMixerSetState(config, AAX_STOPPED);
         res = aaxEmitterDestroy(emitter);
      }
   }
   while (0);

   res = aaxDriverClose(config);
   res = aaxDriverDestroy(config);

   return 0;
}
