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

#include <base/types.h>
#include <aax/ringbuffer.h>
#include "driver.h"
#include "wavfile.h"

static const char FILE_PATH[] = SRC_PATH"/stereo.wav";
static const char FILE_OUT[] = "/tmp/outfile.wav";
static char infile[1024];


int main(int argc, char **argv)
{
   if (argc > 1)
      strncpy(infile, argv[1], 1024);
   else
      strncpy(infile, FILE_PATH, 1024);

   do {
      unsigned int i, no_samples;
      enum aaxFormat fmt;
      _oalRingBuffer *rb;
      char bps, tracks;
      float sduration = 0;
      uint8_t *d1, *d2;
      int freq;

      d1 = fileLoad((char *)&infile, &no_samples, &freq, &bps, &tracks);
      testForError(d1, "Input file not found.\naborted.\n\n");

      if      (bps == 8) fmt = AAX_PCM8U;
      else if (bps == 16) fmt = AAX_PCM16S;
      else break;

      rb = _oalRingBufferCreate();
      if (rb)
      {

         _oalRingBufferSetFormat(rb, 0, 0, fmt);
         _oalRingBufferSetNoSamples(rb, no_samples);
         _oalRingBufferSetNoTracks(rb, tracks);
         _oalRingBufferSetFrequency(rb, freq);
         _oalRingBufferInit(rb, 0);

         sduration = _oalRingBufferGetDuration(rb);
         bps = _oalRingBufferGetBytesPerSample(rb);
         no_samples = _oalRingBufferGetNoSamples(rb);

         _oalRingBufferFillInterleaved(rb, d1, 0);
         _oalRingBufferAddLooping(rb);

         d2 = _oalRingBufferGetDataInterleaved(rb);
         fileWrite((char *)&FILE_OUT, d2, no_samples, freq, tracks, fmt);

         for (i=0; i<no_samples*bps*tracks; i++)
         {
            if (d1[i] != d2[i])
            {
               printf("data mismatch at: %u of %u (%f): d1 = %x, d2 = %x\n",
                       i, no_samples, (float)(i/no_samples)*100, d1[i], d2[i]);
            }
         }
         _oalRingBufferDelete(rb);
         free(d2);
      }
      else
      {
         printf("ERROR: Unable to initialize ringbuffer.\n");
         printf("\ttracks : %i\n\tfrequency: %i\n\t\n\t\
                 bytes/sample: %i\n\tno_samples: %i\n\n",
                tracks, freq, bps, no_samples);
      }

      free(d1);

   } while(0);

   return 0;
}
