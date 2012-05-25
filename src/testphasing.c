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
#include <math.h>

#include <aaxdefs.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define FILE_PATH               SRC_PATH"/wasp.wav"
#define _DELAY			120
#define DELAY			\
    deg = 0; while(deg < _DELAY) { nanoSleep(5e7); deg++; }

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
         aaxEffect effect;
         aaxFilter filter;
         int q, deg = 0;

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

         res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
         testForState(res, "aaxEmitterSetLooping");

         pitch = getPitch(argc, argv);
         res = aaxEmitterSetPitch(emitter, pitch);
         testForState(res, "aaxEmitterSetPitch");

         res = aaxEmitterSetGain(emitter, 0.7f);
         testForState(res, "aaxEmitterSetGain");

         res = aaxMixerRegisterEmitter(config, emitter);
         testForState(res, "aaxMixerRegisterEmitter");

         /** schedule the emitter for playback */
         res = aaxEmitterSetState(emitter, AAX_PLAYING);
         testForState(res, "aaxEmitterStart");

# if 1
         /* flanging effect */
         printf("source flanging.. (envelope following)\n");
         effect = aaxEmitterGetEffect(emitter, AAX_FLANGING_EFFECT);
         effect = aaxEffectSetSlot(effect,0,AAX_LINEAR, 0.7, 0.7, 0.5, 0.0);
         effect = aaxEffectSetState(effect, AAX_ENVELOPE_FOLLOW);
         res = aaxEmitterSetEffect(emitter, effect);
         res = aaxEffectDestroy(effect);
         testForError(effect, "aaxEffectCreate");

         DELAY;

         effect = aaxEmitterGetEffect(emitter, AAX_FLANGING_EFFECT);
         effect = aaxEffectSetState(effect, AAX_FALSE);
         res = aaxEmitterSetEffect(emitter, effect);
         res = aaxEffectDestroy(effect);
         testForError(effect, "aaxEffect Disable");
# endif

# if 1
         /* phasing effect */
         printf("source phasing.. (envelope following)\n");
         effect = aaxEffectCreate(config, AAX_PHASING_EFFECT);
         effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 1.0, 8.0, 1.0, 0.0);
         effect = aaxEffectSetState(effect, AAX_ENVELOPE_FOLLOW);
         testForError(effect, "aaxEffectCreate");
         res = aaxEmitterSetEffect(emitter, effect);
         res = aaxEffectDestroy(effect);
         testForState(res, "aaxEmitterSetEffect");

         DELAY;
#else
         printf("no effect\n");
#endif

# if 1
         /* flanging effect */
         printf("source chorus.. (envelope following)\n");
         effect = aaxEmitterGetEffect(emitter, AAX_CHORUS_EFFECT);
         effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 1.0, 0.8, 1.0, 0.0);
         effect = aaxEffectSetState(effect, AAX_ENVELOPE_FOLLOW);
         res = aaxEmitterSetEffect(emitter, effect);
         res = aaxEffectDestroy(effect);
         testForError(effect, "aaxEffectCreate");

         DELAY;
# endif


#if 1
         /* source effects */
         for (q=0; q<2; q++)
         {
# if 1
            if (q == 1)
            {
               /* frequency filter; 4000Hz lowpass */
               printf("source frequency filter at 4000 Hz lowpass\n");
               filter = aaxFilterCreate(config, AAX_FREQUENCY_FILTER);
               filter=aaxFilterSetSlot(filter, 0, AAX_LINEAR, 400.0, 1.0, 0.0, 0.0);
               filter = aaxFilterSetState(filter, AAX_TRUE);
               res = aaxEmitterSetFilter(emitter, filter);
               res = aaxFilterDestroy(filter);
               testForError(filter, "aaxFilterCreate");
            }
# endif

# if 1
            /* flanging effect */
            printf("source flanging.. (sine wave)\n");
            effect = aaxEmitterGetEffect(emitter, AAX_FLANGING_EFFECT);
            effect = aaxEffectSetSlot(effect,0,AAX_LINEAR, 0.9, 0.9, 0.8, 0.0);
            effect = aaxEffectSetState(effect, AAX_SINE_WAVE);
            res = aaxEmitterSetEffect(emitter, effect);
            res = aaxEffectDestroy(effect);
            testForError(effect, "aaxEffectCreate");

            DELAY;

            effect = aaxEmitterGetEffect(emitter, AAX_FLANGING_EFFECT);
            effect = aaxEffectSetState(effect, AAX_FALSE);
            res = aaxEmitterSetEffect(emitter, effect);
            res = aaxEffectDestroy(effect);
            testForError(effect, "aaxEffect Disable");
# endif


# if 1

            /* phasing effect */
            printf("source phasing.. (triangle wave)\n");
            effect = aaxEffectCreate(config, AAX_PHASING_EFFECT);
            effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 1.0, 0.8, 1.0, 0.0);
            effect = aaxEffectSetState(effect, AAX_TRIANGLE_WAVE);
            testForError(effect, "aaxEffectCreate");
            res = aaxEmitterSetEffect(emitter, effect);
            res = aaxEffectDestroy(effect);
            testForState(res, "aaxEmitterSetEffect");

            DELAY;
#else
           printf("no effect\n");
#endif

# if 1
           /* chorus effect */
           printf("source chorus.. (sine wave)\n");
           effect = aaxEmitterGetEffect(emitter, AAX_CHORUS_EFFECT);
           effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 1.0, 0.8, 1.0, 0.0);
           effect = aaxEffectSetState(effect, AAX_SINE_WAVE);
           res = aaxEmitterSetEffect(emitter, effect);
           res = aaxEffectDestroy(effect);
           testForError(effect, "aaxEffectCreate");

           DELAY;
# endif

# if 1
            /* flanging effect */
            printf("source flanging.. (triangle wave)\n");
            effect = aaxEmitterGetEffect(emitter, AAX_FLANGING_EFFECT);
            effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 1.0, 0.8, 1.0, 0.0);
            effect = aaxEffectSetState(effect, AAX_TRIANGLE_WAVE);
            res = aaxEmitterSetEffect(emitter, effect);
            res = aaxEffectDestroy(effect);
            testForError(effect, "aaxEffectCreate");

            DELAY;
# endif

         }

         /* disable delay effects */
         effect = aaxEmitterGetEffect(emitter, AAX_PHASING_EFFECT);
         effect = aaxEffectSetState(effect, AAX_FALSE);
         res = aaxEmitterSetEffect(emitter, effect);
         res = aaxEffectDestroy(effect);
         testForError(effect, "aaxEffectCreate");

         /* disbale frequency filter */
         filter = aaxEmitterGetFilter(emitter, AAX_FREQUENCY_FILTER);
         filter = aaxFilterSetState(filter, AAX_FALSE);
         res = aaxEmitterSetFilter(emitter, filter);
         res = aaxFilterDestroy(filter);
         testForError(filter, "aaxFilterCreate");
#endif

#if 1
         /* phasing effect */
         printf("mixer phasing.. (square wave)\n");
         effect = aaxEffectCreate(config, AAX_PHASING_EFFECT);
         effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 1.0, 0.5, 1.0, 0.0);
         effect = aaxEffectSetState(effect, AAX_SQUARE_WAVE);
         testForError(effect, "aaxEffectCreate");
         res = aaxMixerSetEffect(config, effect);
         res = aaxEffectDestroy(effect);
         testForState(res, "aaxMixerSetEffect");

         DELAY;
#endif

#if 1
         /* chorus effect */
         printf("mixer chorus.. (sawtooth wave)\n");
         effect = aaxEffectCreate(config, AAX_CHORUS_EFFECT);
         effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 1.0, 0.2, 1.0, 0.0);
         effect = aaxEffectSetState(effect, AAX_SAWTOOTH_WAVE);
         testForError(effect, "aaxEffectCreate");
         res = aaxMixerSetEffect(config, effect);
         res = aaxEffectDestroy(effect);
         testForState(res, "aaxMixerSetEffect");

         DELAY;
#endif

#if 1
         /* flanging effect */
         printf("mixer flanging.. (sawtooth wave)\n");
         effect = aaxEffectCreate(config, AAX_FLANGING_EFFECT);
         effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.88, 0.08, 1.0, 0.0);
         effect = aaxEffectSetState(effect, AAX_SAWTOOTH_WAVE);
         testForError(effect, "aaxEffectCreate");
         res = aaxMixerSetEffect(config, effect);
         res = aaxEffectDestroy(effect);
         testForState(res, "aaxMixerSetEffect");

         DELAY;
#endif

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
