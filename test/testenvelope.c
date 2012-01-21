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

#include <aax.h>
#include <aaxdefs.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define SAMPLE_FREQUENCY	16000.0f

int main(int argc, char **argv)
{
   aaxConfig config;
   char *devname;
   int res;

   devname = getDeviceName(argc, argv);
   config = aaxDriverOpenByName(devname, AAX_MODE_WRITE_STEREO);
   testForError(config, "No default audio device available.");

   do {
      aaxEmitter emitter;
      aaxBuffer buffer;
      aaxEffect effect;
      aaxFilter filter;
      int q, state;
      float dt = 0.0f;
      float pitch;

      /** buffer */
      buffer = aaxBufferCreate(config, 1.1*SAMPLE_FREQUENCY, 1, AAX_PCM16S);
      testForError(buffer, "Unable to create a new buffer\n");

      res = aaxBufferSetFrequency(buffer, SAMPLE_FREQUENCY);
      testForState(res, "aaxBufferSetFrequency");

      res = aaxBufferMixWaveform(buffer, 660.0f, AAX_SINE_WAVE, 0.4f);
      testForState(res, "aaxBufferProcessWaveform");

      /** emitter */
      emitter = aaxEmitterCreate();
      testForError(emitter, "Unable to create a new emitter\n");

      res = aaxEmitterAddBuffer(emitter, buffer);
      testForState(res, "aaxEmitterAddBuffer");

      res = aaxEmitterSetMode(emitter, AAX_POSITION, AAX_ABSOLUTE);
      testForState(res, "aaxEmitterSetMode");

      res = aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE);
      testForState(res, "aaxEmitterSetLooping");

      pitch = getPitch(argc, argv);
      res = aaxEmitterSetPitch(emitter, pitch);
      testForState(res, "aaxEmitterSetPitch");

      /* time filter for emitter */
      filter = aaxFilterCreate(config, AAX_TIMED_GAIN_FILTER);
      testForError(filter, "aaxFilterCreate");

      filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0, 0.07, 10.0, 0.01);
      testForError(filter, "aaxFilterSetSlot 0");
      filter = aaxFilterSetSlot(filter, 1, AAX_LINEAR, 0.7, AAX_FPINFINITE, 0.7, 1.0);
      testForError(filter, "aaxFilterSetSlot 1");

      filter = aaxFilterSetState(filter, AAX_TRUE);
      testForError(filter, "aaxFilterSetState");

      res = aaxEmitterSetFilter(emitter, filter);
      testForState(res, "aaxEmitterSetFilter");

      res = aaxFilterDestroy(filter);
      testForState(res, "aaxFilterDestroy");

      /* time effect for emitter */
      effect = aaxEffectCreate(config, AAX_TIMED_PITCH_EFFECT);
      testForError(effect, "aaxEffectCreate");

      effect  = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 1.0, 0.2, 4.0, 0.2);
      testForError(effect, "aaxEffectSetSlot 0");
      effect  = aaxEffectSetSlot(effect, 1, AAX_LINEAR, 0.5, 0.2, 1.0, 0.0);
      testForError(effect, "aaxEffectSetSlot 1");

      effect = aaxEffectSetState(effect, AAX_TRUE);
      testForError(effect, "aaxEffectSetState");

      res = aaxEmitterSetEffect(emitter, effect);
      testForState(res, "aaxEmitterSetEffect");

      res = aaxEffectDestroy(effect);
      testForState(res, "aaxEffectDestroy");

      /* tremolo filter for emitter */
      filter = aaxFilterCreate(config, AAX_DYNAMIC_GAIN_FILTER);
      testForError(filter, "aaxFilterCreate");

      filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0, 6.0, 1.0, 0.0);
      testForError(filter, "aaxFilterSetSlot");

      filter = aaxFilterSetState(filter, AAX_SINE_WAVE);
      testForError(filter, "aaxFilterSetState");

      res = aaxEmitterSetFilter(emitter, filter);
      testForState(res, "aaxEmitterSetFilter");

      res = aaxFilterDestroy(filter);
      testForState(res, "aaxFilterDestroy");

      /** mixer */
      res = aaxMixerInit(config);
      testForState(res, "aaxMixerInit");

      res = aaxMixerSetGain(config, 1.0f);
      testForState(res, "aaxMixerSetGain");

      res = aaxMixerRegisterEmitter(config, emitter);
      testForState(res, "aaxMixerRegisterEmitter");

      res = aaxMixerSetState(config, AAX_PLAYING);
      testForState(res, "aaxMixerStart");

      /** schedule the emitter for playback */
      res = aaxEmitterSetState(emitter, AAX_PLAYING);
      testForState(res, "aaxEmitterStart");

#if 1
      /* tremolo effect for mixer*/
      filter = aaxFilterCreate(config, AAX_DYNAMIC_GAIN_FILTER);
      testForError(effect, "aaxEffectCreate");

      filter = aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0, 0.9, 1.0, 0.0);
      testForError(effect, "aaxEffectSetSlot");

      filter = aaxFilterSetState(filter, AAX_SINE_WAVE);
      testForError(effect, "aaxEffectSetState");

      res = aaxMixerSetFilter(config, filter);
      testForState(res, "aaxMixerSetEffect");

      res = aaxFilterDestroy(filter);
      testForState(res, "aaxEffectDestroy");
#endif

      /* vibrato effect for emitter */
#if 1
      effect = aaxEffectCreate(config, AAX_DYNAMIC_PITCH_EFFECT);
      testForError(effect, "aaxEffectCreate");

      effect = aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.0, 4.0, 0.4, 0.0);
      testForError(effect, "aaxEffectSetSlot");

      effect = aaxEffectSetState(effect, AAX_TRIANGLE_WAVE);
      testForError(effect, "aaxEffectSetState");

      res = aaxEmitterSetEffect(emitter, effect);
      testForState(res, "aaxEmitterSetEffect");

      res = aaxEffectDestroy(effect);
      testForState(res, "aaxEffectDestroy");
#endif

      q = 0;
      do
      {
         nanoSleep(5e7);
         dt += 5e7*1e-9;

         q++;
#if 1
         if (q == 25) 
         {
            printf("Enterning StandBy mode\n");
            aaxMixerSetState(config, AAX_STANDBY);
         }
         else if (q == 40)
         {
            printf("Enterning Playback mode again\n");
            aaxMixerSetState(config, AAX_PLAYING);
         }
#endif
         state = aaxEmitterGetState(emitter);
      }
      while (state == AAX_PLAYING && dt < 3.33);

      res = aaxEmitterSetState(emitter, AAX_STOPPED);
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

   } while(0);

   res = aaxDriverClose(config);
   res = aaxDriverDestroy(config);


   return 0;
}
