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
#include <math.h>	/* ceil, floor */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "base/types.h"
#include "backend/driver.h"
#include "driver.h"
#include "wavfile.h"

#define LOOP_TIME		0.4978
#define PLAYBACK_DURATION	30.0f

static const char FILE_PATH[] = SRC_PATH"/stereo.wav";

int main(int argc, char **argv)
{
   char *infile = getInputFile(argc, argv, FILE_PATH);

   do {
      unsigned int no_loops, no_samples;
      float sduration, dduration;
      _oalDriverBackend *backend;
      _oalRingBuffer *rbs, *rb16;
      _oalCodec *codec;
      char *renderer, channels, bps;
      ALsizei freq, tracks;
      uint8_t *data;
      enum _oalDriverFmt fmt;

      data = fileLoad(infile, &no_samples, &freq, &bps, &channels);
      testForError(data, "Input file not found.\naborted.\n\n");
      tracks = channels;

      if (bps == 8)
      {
         fmt = FORMAT_PCM8;
         codec = _oalRingBufferCodecs[0];
      }
      else if (bps == 16)
      {
         fmt = FORMAT_PCM16;
         codec = _oalRingBufferCodecs[1];
      }
      else break;

      rbs = _oalRingBufferInitBytes(0, 0, 0, tracks, freq, fmt,
                                             no_samples*bps/8, 0);
      sduration = _oalRingBufferGetDuration(rbs);

      dduration = LOOP_TIME;
      printf("source duration: %6.5f\n", sduration);
      printf("destination duration: %6.5f\n", dduration);
      printf("playback duration: %6.5f\n", PLAYBACK_DURATION);

      rb16 = _oalRingBufferInitSec(0, 0, 0, tracks, freq, FORMAT_PCM16,
                                           dduration, 0);

      backend = getDriverBackend(argc, argv, &renderer);
      if (rbs && rb16 && backend && backend->detect())
      {
         unsigned int i, smin, dmax, smax;
         const void **rbsb;
         void *id = 0;

         _oalRingBufferFillInterleaved(rbs, data, 0);
         _oalRingBufferAddLooping(rbs);
         no_loops = PLAYBACK_DURATION/LOOP_TIME;

         fmt = FORMAT_PCM16;
         printf("Device: %s\n", backend->driver);
         printf("Vendor: %s\n", backend->vendor);

         id = backend->connect(id, 0, renderer, MODE_WRITE_STEREO);
         backend->setup(id, 0, fmt, &tracks, &freq, 1);

         smin = 0;
         dmax = _oalRingBufferGetNoSamples(rb16);
         smax = _oalRingBufferGetNoSamples(rbs);
         rbsb = rbs->sample->buffer;

         for (i=0; i<no_loops; i++)
         {
            unsigned char sbps;
            char src_loops = 1;
            void **ds;

            sbps = bps>>3;
            if (dmax > smax) dmax = smax;

            ds = _oalProcessCodec(codec, rbsb, &smin, &dmax, &sbps, tracks,
                                         0, smax, src_loops);
            _oalRingBufferFillNonInterleaved(rb16, (void *)ds[0], 0);
            free(ds);

            backend->play(id, 0, rb16, 1.0, 1.0);
         }

         backend->disconnect(id);

         _oalRingBufferDelete(rbs);
         _oalRingBufferDelete(rb16);
      }
      else
      {
         if (!rbs)
         {
            printf("ERROR: Unable to initialize ringbuffer.\n");
            printf("\ttracks : %i\n\tfrequency: %6.1f\n\tduration : %6.5f\n"
                   "\tbytes/sample: %i\n\tno_samples: %i\n\n",
                    tracks, (float)freq, sduration, bps, no_samples);
        }
        else
        {
           _oalRingBufferDelete(rbs);
           printf("ERROR: Unable to detect driver.\n");
        }
        printf("aborting.\n\n");
      }

      free(data);

   } while(0);

   return 0;
}
