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
#include <math.h>	/* floor */

#include "base/types.h"
#include "backend/ringbuffer.h"
#include "driver.h"
#include "wavfile.h"

#define LOOP_TIME	0.5
#define FORMAT		FORMAT_PCM32

static const char FILE_PATH[] = SRC_PATH"/stereo.wav";

int main(int argc, char **argv)
{
   char *infile = getInputFile(argc, argv, FILE_PATH);

   do
   {
      float sduration, dduration;
      _oalDriverBackend *backend;
      _oalCodec *codec;
      _oalRingBuffer *rbs;
      unsigned int i, dmax;
      char *renderer, bps, channels;
      ALsizei freq, tracks;
      uint8_t *d1;
      enum _oalDriverFmt fmt;

      d1 = fileLoad(infile, &i, &freq, &bps, &channels);
      testForError(d1, "Input file not found.\naborted.\n\n");
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

      rbs = _oalRingBufferInitBytes(0, 0, 0, tracks, freq, fmt, i*bps/8, 0);
      sduration = _oalRingBufferGetDuration(rbs);
      printf("source duration: %6.5f\n", sduration);

      backend = getDriverBackend(argc, argv, &renderer);
      if (rbs && backend)
      {
         _oalRingBuffer *rb32;
         char src_loops=1;
         void *id = 0;
         ALenum rbfmt;

         _oalRingBufferFillInterleaved(rbs, d1, 0);
         if (src_loops)
            _oalRingBufferAddLooping(rbs);

         bps = _oalRingBufferGetBytesPerSample(rbs);

         rbfmt = _oalRingBufferFormatToFormat(FORMAT_PCM16, backend->tracks);
         printf("Device: %s\n", backend->driver);
         printf("Vendor: %s\n", backend->vendor);

         dduration = LOOP_TIME;
         rb32 = _oalRingBufferInitSec(0, backend->formats,  backend->codecs,
                                         backend->tracks,
                                         freq, FORMAT, dduration, 0);
         if (rb32 && backend->detect())
         {
            unsigned int no_loops = sduration/dduration;
            unsigned int smin, no_samples;
            unsigned int track, rbd_tracks;
            const void **rbsb, **rb32b;
            float vmin, vstep, pos;
            int tracks;

            id = backend->connect(id, 0, renderer, MODE_WRITE_STEREO);
            backend->setup(id, 0, rbfmt, &tracks, &freq, 1);

            smin = 0;
            dmax = _oalRingBufferGetNoSamples(rb32);
            no_samples = _oalRingBufferGetNoSamples(rbs);
            rbsb = rbs->sample->track;
            rb32b = rb32->sample->track;
            pos = 0.0f;

            tracks = rbd_tracks = rb32->sample->no_tracks;
            if (tracks > tracks)
            {
               tracks = tracks;
            }

            vmin = 0.5;
            vstep = 0.5 / no_loops;
            for (i=0; i<no_loops; i++)
            {
               unsigned char sbps;
               void **ds;

               _oalRingBufferClear(rb32);

               sbps = bps>>3;
               if (dmax > no_samples) dmax = no_samples;

               ds = _oalProcessCodec(codec, (const void**)rbsb, &smin, &dmax,
                                    &sbps, tracks, 0, no_samples, src_loops);

               for (track=0; track<tracks; track++)
               {
                  int rbd_track = track % rbd_tracks;
                  int rbs_track = track % tracks;
                  int32_t *s = (int32_t *)ds[rbs_track];
#if 0
# define INT_T	int32_t
                  /*
                   * Simulate the behaviour of bufMixNearest
                   * to test wether it contains bugs or not.
                   */
                  INT_T *d = (INT_T *)rb32b[rbd_track];
                  int q;

                  for (q=0; q<dmax; q++)
                  {
                     INT_T tmp;

                     tmp = *s++ << 8;
                     *d++ += tmp;
                  }
#else
                  int32_t *d = (int32_t *)rb32b[rbd_track];
                  bufResampleCubic(d, s, 0, dmax, 0, 0, 1);
#endif
               }
               free(ds);

               pos += dduration;
               backend->play(id, 0, rb32, 1.0, 1.0);

               _oalRingBufferSetOffsetSec(rbs, pos);
            }

            backend->disconnect(id);

            _oalRingBufferDelete(rb32);
         }

         _oalRingBufferDelete(rbs);
      }
      else
      {
         printf("ERROR: Unable to initialize ringbuffer.\n");
         printf("\ttracks : %i\n\tfrequency: %6.1f\n\tduration : %6.5f\n\tbytes/sample: %i\n\tno_samples: %i\n\n",
                tracks, (float)freq, sduration, bps, dmax);
      }

      free(d1);

   } while(0);

   return 0;
}
