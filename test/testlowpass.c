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

#define	FEMITTER	 400.0
#define FSCENE		4000.0
#define DEG	      (360.0/5)

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
         aaxFilter fscene, femitter;
         float pitch;
         int deg = 0;

         /** mixer */
         res = aaxMixerInit(config);
         testForState(res, "aaxMixerInit");

         res = aaxMixerSetState(config, AAX_PLAYING);
         testForState(res, "aaxMixerStart");

#if 1
         /* frequency filter */
         fscene = aaxFilterCreate(config, AAX_FREQUENCY_FILTER);
         fscene = aaxFilterSetSlot(fscene, 0, AAX_LINEAR, FSCENE, 0.0, 1.0, 1.0);
         fscene = aaxFilterSetState(fscene, AAX_FALSE);
         res = aaxScenerySetFilter(config, fscene);
         res = aaxFilterDestroy(fscene);
#endif

         /** emitter */
         pitch = getPitch(argc, argv);
         emitter = aaxEmitterCreate();
         testForError(emitter, "Unable to create a new emitter\n");

         res = aaxEmitterAddBuffer(emitter, buffer);
         testForState(res, "aaxEmitterAddBuffer");

         res = aaxEmitterSetMode(emitter, AAX_POSITION, AAX_RELATIVE);
         testForState(res, "aaxEmitterSetMode");

         res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
         testForState(res, "aaxEmitterSetLooping");

         res = aaxEmitterSetPitch(emitter, pitch);
         testForState(res, "aaxEmitterSetPitch");

         /* frequency filter */
#if 1
         femitter = aaxFilterCreate(config, AAX_FREQUENCY_FILTER);
         femitter = aaxFilterSetSlot(femitter, 0, AAX_LINEAR, 400.0, 1.0, 0.0, 1.0);
         femitter = aaxFilterSetState(femitter, AAX_FALSE);
         res = aaxEmitterSetFilter(emitter, femitter);
         res = aaxFilterDestroy(femitter);
         testForError(femitter, "aaxFilterCreate");
#endif

         res = aaxMixerRegisterEmitter(config, emitter);
         testForState(res, "aaxMixerRegisterEmitter");

         /** schedule the emitter for playback */
         res = aaxEmitterSetState(emitter, AAX_PLAYING);
         testForState(res, "aaxEmitterStart");

         printf("No filtering\n");
         deg = 0;
         while(deg < 2*360)
         {
            nanoSleep(5e7);

            deg += 3;
            if ((deg > DEG) && (deg < (DEG+4)))
            {
               printf("scenery highpass filter at %3.1f Hz\n", FSCENE);
               fscene = aaxSceneryGetFilter(config, AAX_FREQUENCY_FILTER);
               fscene = aaxFilterSetState(fscene, AAX_TRUE);
               res = aaxScenerySetFilter(config, fscene);
               testForState(res, "aaxScenerySetFilter\n");
            }
            else if ((deg > 2*DEG) && (deg < (2*DEG+4)))
            {
               printf("add emitter lowpass at %3.1f Hz\n", FEMITTER);
               printf("\tband stop filter between %3.1f Hz and %3.1f Hz\n",
                       FEMITTER, FSCENE);
               femitter = aaxEmitterGetFilter(emitter, AAX_FREQUENCY_FILTER);
               femitter = aaxFilterSetState(femitter, AAX_TRUE);
               res = aaxEmitterSetFilter(emitter, femitter);
            }
            else if ((deg > 3*DEG) && (deg < (3*DEG+4)))
            {
               printf("disable scenery highpass filter\n");
               printf("\temitter lowpass filter at %3.1f Hz\n", FEMITTER);
               fscene = aaxFilterSetState(fscene, AAX_FALSE);
               res = aaxScenerySetFilter(config, fscene);
            }
            else if ((deg > 4*DEG) && (deg < (4*DEG+4)))
            {
               printf("envelope following filtering (auto wah)\n");
               femitter = aaxEmitterGetFilter(emitter, AAX_FREQUENCY_FILTER);
               femitter = aaxFilterSetSlot(femitter, 0, AAX_LINEAR, 300.0, 0.4, 1.0, 12.0);
               femitter = aaxFilterSetSlot(femitter, 1, AAX_LINEAR, 600.0, 0.0, 0.0, 0.2);
               femitter = aaxFilterSetState(femitter, AAX_ENVELOPE_FOLLOW);
               res = aaxEmitterSetFilter(emitter, femitter);
            }
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
