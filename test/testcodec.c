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
#include <ringbuffer.h>
#include <software/device.h>
#include "driver.h"
#include "wavfile.h"

#define	DBUFFER_SIZE	2500

static const char FILE_PATH[] = SRC_PATH"/stereo.wav";
static const char FILE_OUT_SINGLE[] = "/tmp/outfile_s.wav";
static const char FILE_OUT_MULTI[] = "/tmp/outfile_m.wav";
static const char FILE_OUT_LOOP_SINGLE[] = "/tmp/outfile_ls.wav";
static const char FILE_OUT_LOOP_MULTI[] = "/tmp/outfile_lm.wav";


int main(int argc, char **argv)
{
   char *infile = getInputFile(argc, argv, FILE_PATH);

   if (argc > 1)
      strncpy(infile, argv[1], 1024);
   else
      strncpy(infile, FILE_PATH, 1024);

   printf("OpenAL ringbuffer codec test utility.\n");
   printf("This utility reads a wave audio file, adds it's content to a "
          "ringbuffer and\nprocesses it by using the codec function:\n"
          "1. process the whole file as a singe buffer.\n"
          "2. process the file in multiple stages.\n"
          "3. loop te file into a 30 sec. buffer in one go.\n"
          "4. loop te file into a 30 sec. buffer in multiple stages.\n\n"
          "The resulting audio data is written to the following files "
          "afterwards:\n %s\n %s\n %s\n %s\n\n",
          FILE_OUT_SINGLE, FILE_OUT_MULTI,
          FILE_OUT_LOOP_SINGLE, FILE_OUT_LOOP_MULTI);


   do {
      enum aaxFormat format;
      _oalCodec *codec;
      _oalRingBuffer *rb;
      unsigned int no_samples;
      char bps, tracks;
      uint8_t *d1, *d2;
      unsigned block;
      float freq;
      int sf;

      d1 = fileLoad((char *)&infile, &no_samples, &sf, *block, &bps, &tracks);
      testForError(d1, "Input file not found.\naborted.\n\n");

      if (bps == 8)
      {
         format = AAX_PCM8U;
         codec = _oalRingBufferCodecs[0];

      }
      else if (bps == 16)
      {
         format = AAX_PCM16S;
         codec = _oalRingBufferCodecs[1];
      }
      else
      {
         free(d1);
         printf("Unsupported audio format.\naborted.\n\n");
         exit(-1);
      }

      freq = sf;
      rb = _oalRingBufferCreate();
      if (rb)
      {
         void **rbdt = (void **)rb->sample->buffer;
         unsigned int smin, src_loops=1;
         unsigned char sbps;
         int16_t **ds;

         _oalRingBufferSetFormat(rb, 0, 0, format);
         _oalRingBufferSetNoSamples(rb, no_samples);
         _oalRingBufferSetNoTracks(rb, tracks);
         _oalRingBufferSetFrequency(rb, freq);
         _oalRingBufferInit(rb, 0);

         bps = _oalRingBufferGetBytesPerSample(rb);
         no_samples = _oalRingBufferGetNoSamples(rb);

         _oalRingBufferFillInterleaved(rb, d1, 0);
         if (src_loops)
            _oalRingBufferAddLooping(rb);
         free(d1);

         smin = 0;
         sbps = bps>>3;

         printf("single buffer copy...\n");
         do
         {
            unsigned int smax = no_samples;

            ds = (int16_t **)_oalProcessCodec(codec, rbdt, &smin, 0, smax, 0,
                                          no_samples, &sbps, tracks, src_loops);

            d2 = fileDataConvertToInterleaved(*ds, tracks, sbps, smax);
            free(ds);

            fileWrite((char *)&FILE_OUT_SINGLE, d2, smax, sf, tracks, format);
            free(d2);
         }
         while (0);

         printf("multi stage copy...\n");
         do
         {
            unsigned int partlen, i, imax, pos, cblen;
            uint8_t *d3;

            d3 = calloc(tracks, no_samples*sizeof(int16_t));

            pos = 0;
            smin = 0;
            partlen = DBUFFER_SIZE;
            imax = no_samples / DBUFFER_SIZE;

            for (i=0; i<imax; i++)
            {
               sbps = bps>>3;
               cblen = partlen;

               ds = (int16_t**)_oalProcessCodec(codec, rbdt, &smin, 0, cblen, 0,
                                          no_samples, &sbps, tracks, src_loops);

               d2 = fileDataConvertToInterleaved(*ds, tracks, sbps, cblen);
               free(ds);

               memcpy(d3+pos, d2, tracks*sbps*cblen);
               free(d2);

               pos += sbps*cblen*tracks;
            }

            fileWrite((char *)&FILE_OUT_MULTI, d3, no_samples, sf, sbps, tracks);
            free(d3);
         }
         while (0);

         printf("single stage loop...\n");
         do
         {
            unsigned int partlen;

            smin = 0;
            sbps = bps>>3;
            partlen = 30*sf;

            ds = (int16_t**)_oalProcessCodec(codec, rbdt, &smin, 0, partlen,
                                       0, no_samples, &sbps, tracks, src_loops);

            d2 = fileDataConvertToInterleaved(*ds, tracks, sbps, partlen);
            free(ds);

            fileWrite((char *)&FILE_OUT_LOOP_SINGLE, d2, partlen, sf, sbps, tracks);
            free(d2);
         }
         while (0);

         printf("multi stage loop...\n");
         do
         {
            unsigned int partlen, i, imax, pos, cblen;
            unsigned int smax = no_samples;
            uint8_t *d3;

            no_samples = 30*sf;		/* loop for at least 30 seconds */
            d3 = calloc(tracks, no_samples*sizeof(int16_t));

            pos = 0;
            smin = 0;
            partlen = DBUFFER_SIZE;
            imax = no_samples/DBUFFER_SIZE;

            for (i=0; i<imax; i++)
            {
               sbps = bps>>3;
               cblen = partlen;

               ds = (int16_t**)_oalProcessCodec(codec, rbdt, &smin, 0, cblen,
                                             0, smax, &sbps, tracks, src_loops);

               d2 = fileDataConvertToInterleaved(*ds, tracks, sbps, cblen);
               free(ds);

               memcpy(d3+pos, d2, tracks*sbps*cblen);
               free(d2);

               pos += sbps*cblen*tracks;
            }


            fileWrite((char *)&FILE_OUT_LOOP_MULTI, d3, no_samples, sf, sbps, tracks);
            free(d3);
         }
         while (0);

         _oalRingBufferDelete(rb);
      }
      else
      {
         printf("ERROR: Unable to initialize ringbuffer.\n");
         printf("\ttracks : %i\n\tfrequency: %6.1f\n\tbytes/sample: %i\n\tsize: %i\n\n",
                tracks, freq, bps, no_samples);
      }

   } while(0);

   printf("done.\n\n");

   return 0;
}
