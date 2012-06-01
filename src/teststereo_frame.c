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

#define FILE_PATH		SRC_PATH"/stereo.wav"

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
         aaxFrame frame;
         aaxFilter f;
         float dt = 0.0f;
         int q, state;
         float pitch;

         /** mixer */
         res = aaxMixerInit(config);
         testForState(res, "aaxMixerInit");

         res = aaxMixerSetState(config, AAX_PLAYING);
         testForState(res, "aaxMixerStart");

         /** audio frame */
         frame = aaxAudioFrameCreate(config);
         testForError(frame, "Unable to create a new audio frame\n");

         pitch = getPitch(argc, argv);
         res = aaxAudioFrameSetPitch(frame, pitch);
         testForState(res, "aaxAudioFrameSetPitch");

         /** register audio frame */
         res = aaxMixerRegisterAudioFrame(config, frame);
         testForState(res, "aaxMixerRegisterAudioFrame");

#if 0
         /* equalizer */
         f = aaxFilterCreate(config, AAX_EQUALIZER);
         testForError(f, "aaxFilterCreate");

         f = aaxFilterSetSlot(f, 0, AAX_LINEAR,  500.0, 1.0, 0.1, 0.0);
         testForError(f, "aaxFilterSetSlot/0");

         f = aaxFilterSetSlot(f, 1, AAX_LINEAR, 8000.0, 0.1, 0.5, 0.0);
         testForError(f, "aaxFilterSetSlot/1");

         f = aaxFilterSetState(f, AAX_TRUE);
         testForError(f, "aaxFilterSetState");

         res = aaxAudioFrameSetFilter(frame, f);
         testForState(res, "aaxAudioFrameSetFilter");

         res = aaxFilterDestroy(f);
         testForState(res, "aaxFilterDestroy");
#endif

         /** schedule the audioframe for playback */
         res = aaxAudioFrameSetState(frame, AAX_PLAYING);
         testForState(res, "aaxAudioFrameStart");

         /** emitter */
         emitter = aaxEmitterCreate();
         testForError(emitter, "Unable to create a new emitter");

         res = aaxEmitterAddBuffer(emitter, buffer);
         testForState(res, "aaxEmitterAddBuffer");

         /** register emitter */
         res = aaxAudioFrameRegisterEmitter(frame, emitter);
         testForState(res, "aaxMixerRegisterEmitter");

         /** schedule the emitter for playback */
         res = aaxEmitterSetState(emitter, AAX_PLAYING);
         testForState(res, "aaxEmitterStart");

         q = 0;
         do
         {
            nanoSleep(5e7);
            dt += 5e7f*1e-9f;
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

         res = aaxAudioFrameDeregisterEmitter(frame, emitter);
         res = aaxAudioFrameSetState(frame, AAX_STOPPED);
         res = aaxMixerDeregisterAudioFrame(config, frame);
         res = aaxMixerSetState(config, AAX_STOPPED);
         res = aaxEmitterDestroy(emitter);
         res = aaxBufferDestroy(buffer);
         res = aaxAudioFrameDestroy(frame);
      }
   }
   while (0);

   res = aaxDriverClose(config);
   res = aaxDriverDestroy(config);


   return 0;
}
