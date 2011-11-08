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

#define LOOP_TIME	0.5

static const char FILE_PATH[] = SRC_PATH"/stereo.wav";

int main(int argc, char **argv)
{
   char *infile = getInputFile(argc, argv, FILE_PATH);

   do {
      char *renderer, bps, channels;
      _oalDriverBackend *backend;
      unsigned int no_samples;
      enum _oalDriverFmt fmt;
      ALsizei freq, tracks;
      _oalRingBuffer *rb;
      float sduration;
      uint8_t *data;

      data = fileLoad(infile, &no_samples, &freq, &bps, &channels);
      testForError(data, "Input file not found.\naborted.\n\n");

      if (bps == 8) fmt = FORMAT_PCM8;
      else if (bps == 16) fmt = FORMAT_PCM16;
      else break;

      tracks = channels;
      rb = _oalRingBufferInitSamples(0, 0, 0, tracks, (float)freq, fmt,
                                            no_samples, 0);
      sduration = _oalRingBufferGetDuration(rb);
      printf("source duration: %6.5f\n", sduration);

      backend = getDriverBackend(argc, argv, &renderer);
      if (rb && backend && backend->detect())
      {
         unsigned int i, no_loops;
         float pos = 0.0f;
         void *id = 0;
         ALenum format;

         _oalRingBufferFillInterleaved(rb, data, 0);
         no_loops = sduration/LOOP_TIME;

         format = fmt;
         printf("Device: %s\n", backend->driver);
         printf("Vendor: %s\n", backend->vendor);

         id = backend->connect(id, 0, renderer, MODE_WRITE_STEREO);
         backend->setup(id, 0, format, &tracks, &freq, 1);

         for (i=0; i<no_loops; i++)
         {
            _oalRingBufferSetOffsetSec(rb, pos);

            pos += LOOP_TIME;
            _oalRingBufferSetDuration(rb, pos);

            backend->play(id, 0, rb, 1.0, 1.0);
         }

         backend->disconnect(id);

         _oalRingBufferDelete(rb);
      }
      else
      {
         if (!rb)
         {
            printf("ERROR: Unable to initialize ringbuffer.\n");
            printf("\ttracks : %i\n\tfrequency: %i\n\tduration : %6.5f\n"
                   "\tbytes/sample: %i\n\tno_samples: %i\n\n",
                    tracks, freq, sduration, bps, no_samples);
        }
        else
        {
           _oalRingBufferDelete(rb);
           printf("ERROR: Unable to detect driver.\n");
        }
        printf("aborting.\n\n");
      }

      free(data);

   } while(0);

   return 0;
}
