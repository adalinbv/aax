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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "base/types.h"
#include "backend/ringbuffer.h"
#include "driver.h"
#include "wavfile.h"

#undef	MIX2D
#define NO_PLAY_LOOPS	50

static const char FILE_PATH[] = SRC_PATH"/stereo.wav";

int main(int argc, char **argv)
{
   char *infile = getInputFile(argc, argv, FILE_PATH);

   do
   {
      float sduration, dduration;
      _oalDriverBackend *backend;
      _oalRingBuffer *rbs;
      unsigned int no_samples;
      char *renderer, bps, channels;
      ALsizei freq, tracks;
      uint8_t *d1;
      enum _oalDriverFmt fmt;

      d1 = fileLoad(infile, &no_samples, &freq, &bps, &channels);
      testForError(d1, "Input file not found.\naborted.\n\n");
      tracks = channels;

      if (bps == 8) fmt = FORMAT_PCM8;
      else if (bps == 16) fmt = FORMAT_PCM16;
      else break;

      rbs = _oalRingBufferInitBytes(0, 0, 0, tracks, freq, fmt,
                                             no_samples*bps/8, 0);
   
      dduration = 30.0f;
      sduration = _oalRingBufferGetDuration(rbs);
      if (sduration > dduration) dduration = sduration;
      printf("source duration: %6.5f\n", sduration);
      printf("destination duration: %6.5f\n", dduration);
      
      backend = getDriverBackend(argc, argv, &renderer);
      if (rbs && backend)
      {
         _oalRingBuffer *rb32;
         unsigned int src_loops=1;
         void *id = 0;
         ALenum fmt;

         _oalRingBufferFillInterleaved(rbs, d1, 0);
         if (src_loops)
            _oalRingBufferAddLooping(rbs);

         printf("Device: %s\n", backend->driver);
         printf("Vendor: %s\n", backend->vendor);

         fmt = backend->format;
         dduration /= NO_PLAY_LOOPS;
         rb32 = _oalRingBufferInitSec(0, backend->formats, backend->codecs,
                                         backend->tracks, backend->rate,
                                         FORMAT_PCM32, dduration, 0);
         if (rb32 && backend->detect())
         {
            _oalRingBuffer2dProps p2d;
            unsigned int i;
            ALsizei rate = backend->rate;

            _oalRingBufferStart(rbs);
            _oalRingBufferStart(rb32);

            id = backend->connect(id, 0, renderer, MODE_WRITE_STEREO);
            backend->setup(id, 0, fmt, &tracks, &rate, 1);

            p2d.dir[0][0] = p2d.dir[1][0] = 0.0f;
            p2d.dir[0][1] = p2d.dir[1][1] = 0.0f;
            p2d.dir[0][2] = p2d.dir[1][2] = 0.0f;
            p2d.pitch_norm        = 1.0f;
            p2d.gain_norm         = 1.0f;
            p2d.speed_of_sound  = 343.3f;

            for (i=0; i<NO_PLAY_LOOPS; i++)
            {
               _oalRingBufferClear(rb32);
#ifdef MIX2D
               backend->mix2d(id, rb32, rbs, 1.0, 1.0);
#else
               backend->mix3d(id, rb32, rbs, &p2d, i);
#endif

               backend->play(id, 0, rb32, 1.0, 1.0);
            }

            backend->disconnect(id);

            _oalRingBufferDelete(rb32);
         }

         _oalRingBufferDelete(rbs);
      }
      else
      {
         printf("ERROR: Unable to initialize ringbuffer.\n");
         printf("\ttracks : %i\n\tfrequency: %6.1f\n\tduration : %6.5f\n"\
                "\tbytes/sample: %i\n\tno_samples: %i\n\n",
                tracks, (float)freq, sduration, bps, no_samples);
      }

      free(d1);

   } while(0);

   return 0;
}
