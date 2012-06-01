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

#include <aaxdefs.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define FILE_PATH		SRC_PATH"/loop.wav"
#define LOOP_START_SEC		0.5750625f
#define LOOP_END_SEC		1.9775625f

int main(int argc, char **argv)
{
   char *devname, *infile;
   aaxConfig config;
   int res;

   infile = getInputFile(argc, argv, FILE_PATH);
   devname = getDeviceName(argc, argv);

   config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
   testForError(config, "No default audio device available.");

   do {
      aaxBuffer buffer = bufferFromFile(config, infile);
      if (buffer)
      {
         aaxEmitter emitter;
         float freq, dt = 0.0f;
         int q, state;

         freq = (float)aaxBufferGetSetup(buffer, AAX_FREQUENCY);
         res = aaxBufferSetLoopPoints(buffer,
                                      (unsigned int)(LOOP_START_SEC*freq),
                                      (unsigned int)(LOOP_END_SEC*freq));
         testForState(res, "aaxBufferSetLoopPoints");

         /** emitter */
         emitter = aaxEmitterCreate();
         testForError(emitter, "Unable to create a new emitter\n");

         res = aaxEmitterAddBuffer(emitter, buffer);
         testForState(res, "aaxEmitterAddBuffer");

         res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
         testForState(res, "aaxEmitterSetLooping");

         /** mixer */
         res = aaxMixerInit(config);
         testForState(res, "aaxMixerInit");

         res = aaxMixerRegisterEmitter(config, emitter);
         testForState(res, "aaxMixerRegisterEmitter");

         res = aaxMixerSetState(config, AAX_PLAYING);
         testForState(res, "aaxMixerStart");

         /** schedule the emitter for playback */
         res = aaxEmitterSetState(emitter, AAX_PLAYING);
         testForState(res, "aaxEmitterStart");

         q = 0;
         do
         {
            nanoSleep(5e7);
            dt += 5e7f*1e-9f;

            if (dt > (LOOP_START_SEC+3*LOOP_END_SEC))
            {
               res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_FALSE);
               testForState(res, "aaxEmitterSetLooping");
            }

#if 1
            q++;
            if (q > 10)
            {
               unsigned long offs;
               float off_s;
               q = 0;

               off_s = aaxEmitterGetOffsetSec(emitter);
               offs = aaxEmitterGetOffset(emitter, AAX_SAMPLES);

               printf("playing time: %5.2f, buffer position: %5.2f (%li samples)\n", dt, off_s, offs);
            }
#endif
            state = aaxEmitterGetState(emitter);
         }
         while (state == AAX_PLAYING);

         res = aaxMixerDeregisterEmitter(config, emitter);
         res = aaxMixerSetState(config, AAX_STOPPED);
         res = aaxEmitterDestroy(emitter);
         res = aaxBufferDestroy(buffer);
      }
   }
   while(0);

   res = aaxDriverClose(config);
   res = aaxDriverDestroy(config);


   return 0;
}
