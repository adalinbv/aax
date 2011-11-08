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

#include <AL/al.h>

#include <base/types.h>
#include <driver.h>
// #include <al/aax_support.h>
#include "driver.h"
#include "wavfile.h"

static const char FILE_PATH[] = SRC_PATH"/stereo.wav";


int main(int argc, char **argv)
{
   char *infile = getInputFile(argc, argv, FILE_PATH);

   do {
      unsigned int no_loops, no_samples;
      char *renderer, bps, no_tracks;
      _oalDriverBackend *backend;
      enum aaxFormat fmt;
      _oalRingBuffer *rb;
      ALsizei freq;
      ALsizei tracks;
      uint8_t *data;

      data = fileLoad(infile, &no_samples, &freq, &bps, &no_tracks);
      testForError(data, "Input file not found.\naborted.\n\n");

      if (bps == 8) fmt = AAX_FORMAT_PCM8;
      else if (bps == 16) fmt = AAX_FORMAT_PCM16;
      else break;

      tracks = no_tracks;
      rb = _oalRingBufferInitBytes(0, 0, 0, tracks, (float)freq, fmt,
                                            no_samples*bps/8, 0);

      backend = getDriverBackend(argc, argv, &renderer);
      if (rb && backend && backend->detect())
      {
         float sduration, dduration = 15.0f;
         unsigned int i;
         ALenum format;
         void *id = 0;

         _oalRingBufferFillInterleaved(rb, data, 0);

         sduration = _oalRingBufferGetDuration(rb);
         bps = _oalRingBufferGetBytesPerSample(rb);
         no_samples = _oalRingBufferGetNoSamples(rb);

         no_loops = (dduration / sduration);
         if (no_loops == 0) no_loops = 1;
         printf("source duration: %6.5f, no_loops: %u\n", sduration, no_loops);

         format = _oalAAXFormatToFormat(fmt, tracks);
         printf("Device: %s\n", backend->driver);
         printf("Vendor: %s\n", backend->vendor);

         id = backend->connect(id, 0, renderer, AAX_MODE_WRITE_STEREO);
         backend->setup(id, 0, format, &tracks, &freq);

         for (i=0; i<no_loops; i++)
         {
            printf("playback #%i\n", i);
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
            printf("\ttracks : %i\n\tfrequency: %6.1f\n\t\n"
                   "\tbytes/sample: %i\n\tno_samples: %i\n\n",
                    tracks, (float)freq, bps, no_samples);
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
