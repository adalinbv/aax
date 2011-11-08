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

#define  MIX2D

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "base/types.h"
#include "backend/ringbuffer.h"
#include "driver.h"
#include "wavfile.h"

static const char FILE_PATH[] = SRC_PATH"/stereo.wav";

int main(int argc, char **argv)
{
   char *infile = getInputFile(argc, argv, FILE_PATH);

   do {
      unsigned int no_samples;
      _oalDriverBackend *backend;
      enum _oalDriverFmt fmt;
      float sduration;
      _oalRingBuffer *rb;
      char *renderer, bps, channels;
      ALsizei freq, tracks;
      uint8_t *data;

      data = fileLoad(infile, &no_samples, &freq, &bps, &channels);
      testForError(data, "Input file not found.\naborted.\n\n");

      if (bps == 8) fmt = FORMAT_PCM8;
      else if (bps == 16) fmt = FORMAT_PCM16;
      else break;

      tracks = channels;
      rb = _oalRingBufferInitBytes(0, 0, 0, tracks, freq, fmt,
                                            no_samples*bps/8, 0);
      sduration = _oalRingBufferGetDuration(rb);
      printf("source duration: %6.5f\n", sduration);

      backend = getDriverBackend(argc, argv, &renderer);
      if (rb && backend)
      {
         _oalRingBuffer *rb32;
         ALenum fmt;
         void *id = 0;

         printf("Device: %s\n", backend->driver);
         printf("Vendor: %s\n", backend->vendor);
         printf("playback freq: %6.1f, no. tracks: %i\n", (float)freq,
                                                       backend->tracks);

         _oalRingBufferFillInterleaved(rb, data, 0);
         bps = _oalRingBufferGetBytesPerSample(rb);
         no_samples = _oalRingBufferGetNoSamples(rb);
         fmt = backend->format;

         rb32 = _oalRingBufferInitSec(0, 0, 0, backend->tracks,
                                     backend->rate, FORMAT_PCM32, sduration, 0);

         if (rb32 && backend->detect())
         {
            ALsizei rate = backend->rate;

            id = backend->connect(id, 0, renderer, MODE_WRITE_STEREO);
            backend->setup(id, 0, fmt, &tracks, &rate, 1);
            backend->mix2d(id, rb32, rb, 1.0, 1.0);
            backend->play(id, 0, rb32, 1.0, 1.0);
            backend->disconnect(id);

            _oalRingBufferDelete(rb32);
         }

         _oalRingBufferDelete(rb);
      }
      else
      {
         if (!rb)
         {
            printf("ERROR: Unable to initialize ringbuffer.\n");
            printf("\ttracks : %i\n\tfrequency: %6.1f\n\tduration : %6.5f\n"
                   "\tbytes/sample: %i\n\tno_samples: %i\n\n",
                    tracks, (float)freq, sduration, bps, no_samples);
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
