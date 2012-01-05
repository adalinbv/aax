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

#define FILE_PATH		SRC_PATH"/sine-440Hz.wav"

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
         aaxEffect effect;
         aaxFilter filter;
         float dt = 0.0f;
         int q, state;
         float pitch;

         /** emitter */
         emitter = aaxEmitterCreate();
         testForError(emitter, "Unable to create a new emitter");

         pitch = getPitch(argc, argv);
         res = aaxEmitterSetPitch(emitter, pitch);
         testForState(res, "aaxEmitterSetPitch");

         res = aaxEmitterAddBuffer(emitter, buffer);
         testForState(res, "aaxEmitterAddBuffer");

         res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
         testForState(res, "aaxEmitterSetMode");

         /* frequency filter */
#if 1
         printf("Add frequency filter at 150Hz\n");
         filter = aaxFilterCreate(config, AAX_FREQUENCY_FILTER);
         testForError(filter, "aaxFilterCreate");

         filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 150.0, 2.0, 0.5,0.0);
         testForError(filter, "aaxFilterSetSlot");
        
         filter = aaxFilterSetState(filter, AAX_TRUE);
         testForError(filter, "aaxFilterSetState");

         res = aaxEmitterSetFilter(emitter, filter);
         testForState(res, "aaxEmitterSetFilter");

         res = aaxFilterDestroy(filter);
         testForState(res, "aaxFilterDestroy");
#endif

         /* distortion effect for emitter */
         effect = aaxEffectCreate(config, AAX_DISTORTION_EFFECT);
         testForError(effect, "aaxEffectCreate");

         effect  = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.8, 0.2, 0.5, 0.0);
         testForError(effect, "aaxEffectSetSlot 0");

         effect = aaxEffectSetState(effect, AAX_TRUE);
         testForError(effect, "aaxEffectSetState");

         res = aaxEmitterSetEffect(emitter, effect);
         testForState(res, "aaxEmitterSetEffect");

         res = aaxEffectDestroy(effect);
         testForState(res, "aaxEffectDestroy");

         /** mixer */
         res = aaxMixerInit(config);
         testForState(res, "aaxMixerInit");

         res = aaxMixerRegisterEmitter(config, emitter);
         testForState(res, "aaxMixerRegisterEmitter");

# if 0
         /* chorus effect */
         printf("source chorus..\n");
         effect = aaxEmitterGetEffect(emitter, AAX_PHASING_EFFECT);
         effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.6, 0.08, 0.8, 0.0);
         effect = aaxEffectSetState(effect, AAX_TRUE);
         res = aaxEmitterSetEffect(emitter, effect);
         res = aaxEffectDestroy(effect);
         testForError(effect, "aaxEffectCreate");
#endif

         res = aaxMixerSetState(config, AAX_PLAYING);
         testForState(res, "aaxMixerStart");

         /** schedule the emitter for playback */
         res = aaxEmitterSetState(emitter, AAX_PLAYING);
         testForState(res, "aaxEmitterStart");

         q = 0;
         printf("playing distorted\n");
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

               off_s = aaxEmitterGetOffsetSec(emitter);
               offs = aaxEmitterGetOffset(emitter, AAX_SAMPLES);
               offs_bytes = aaxEmitterGetOffset(emitter, AAX_BYTES);
               printf("playing time: %5.2f, buffer position: %5.2f (%li samples/ %li bytes)\n", dt, off_s, offs, offs_bytes);
            }
#endif
            state = aaxEmitterGetState(emitter);
         }
         while ((dt < 15.0f) && (state == AAX_PLAYING));

         res = aaxEmitterSetState(emitter, AAX_STOPPED);
         testForState(res, "aaxEmitterStop");

         do
         {
            nanoSleep(5e7);
            state = aaxEmitterGetState(emitter);
         }
         while (state == AAX_PLAYING);

         res = aaxMixerDeregisterEmitter(config, emitter);
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
