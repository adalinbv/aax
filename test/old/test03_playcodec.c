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

static const char FILE_PATH[] = SRC_PATH"/stereo.wav";

int main(int argc, char **argv)
{
   char *infile = getInputFile(argc, argv, FILE_PATH);

   do {
      _oalRingBuffer *rbs, *rb16;
      _oalDriverBackend *backend;
      unsigned int no_samples;
      float sduration, dduration;
      _oalCodec *codec;
      char *renderer, bps, channels;
      ALsizei freq, tracks;
      uint8_t *d1;
      enum _oalDriverFmt fmt;

      d1 = fileLoad(infile, &no_samples, &freq, &bps, &channels);
      testForError(d1, "Input file not found.\naborted.\n\n");

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

      tracks = channels;
      rbs = _oalRingBufferInitBytes(0, 0, 0, tracks, freq, fmt,
                                             no_samples*bps/8, 0);
      dduration = 30.0f;
      sduration = _oalRingBufferGetDuration(rbs);
      if (sduration > dduration) dduration = sduration;

      printf("source duration: %6.5f\n", sduration);
      printf("destination duration: %6.5f\n", dduration);
      
      backend = getDriverBackend(argc, argv, &renderer);
      if (rbs && backend && backend->detect())
      {
         unsigned int smin, dmax, src_loops=1;
         const void **rbdt;
         void **ds, *id = 0;
         unsigned char sbps;
         ALenum format;

         bps = _oalRingBufferGetBytesPerSample(rbs);
         no_samples = _oalRingBufferGetNoSamples(rbs);

         _oalRingBufferFillInterleaved(rbs, d1, 0);
         if (src_loops)
            _oalRingBufferAddLooping(rbs);

         smin = 0;
         sbps = bps>>3;
         dmax = dduration*freq;
         rbdt = rbs->sample->buffer;
         ds = _oalProcessCodec(codec, rbdt, &smin, &dmax, &sbps, tracks,
                                      0, no_samples, src_loops);
         _oalRingBufferDelete(rbs);         

         rb16 = _oalRingBufferInitBytes(0, 0, 0, tracks, freq, FORMAT_PCM16,
                                       dmax*sbps, 0);
         _oalRingBufferFillNonInterleaved(rb16, (void *)ds, 0);
         free(ds);

         bps = _oalRingBufferGetBytesPerSample(rb16);
         no_samples = _oalRingBufferGetNoSamples(rb16);

         format = FORMAT_PCM16;
         printf("Device: %s\n", backend->driver);
         printf("Vendor: %s\n", backend->vendor);

         id = backend->connect(id, 0, renderer, MODE_WRITE_STEREO);
         backend->setup(id, 0, format, &tracks, &freq, 1);
         backend->play(id, 0, rbs, 1.0, 1.0);
         backend->disconnect(id);

         _oalRingBufferDelete(rb16);
      }
      else
      {
         printf("ERROR: Unable to initialize ringbuffer.\n");
         printf("\ttracks : %i\n\tfrequency: %i\n\tduration : %6.5f\n\t" \
                "bytes/sample: %i\n\tno_samples: %i\n\n",
                tracks, freq, sduration, bps, no_samples);
      }

      free(d1);

   } while(0);

   return 0;
}
