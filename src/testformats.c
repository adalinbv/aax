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

#define BLOCKSIZE		36
#define FILE_PATH		SRC_PATH"/tictac.wav"

#define MAX_LOOPS		6
static int _mask_t[MAX_LOOPS] = {
    0,
    AAX_FORMAT_UNSIGNED,
    AAX_FORMAT_LE,
    AAX_FORMAT_LE_UNSIGNED,
    AAX_FORMAT_BE,
    AAX_FORMAT_BE_UNSIGNED
};

static const char* _format_s[] = {
   "AAX_PCM8S:  signed, 8-bits per sample",
   "AAX_PCM16S: signed, 16-bits per sample",
   "AAX_PCM24S: signed, 24-bits per sample, 32-bit encoded",
   "AAX_PCM32S: signed, 32-bits per sample",
   "AAX_FLOAT:  32-bit floating point, -1.0 to 1.0",
   "AAX_DOUBLE: 64-bit floating point, -1.0 to 1.0",
   "AAX_MULAW",
   "AAX_ALAW",
   "AAX_IMA4_ADPCM"
};

static const char* _format_us[] = {
   "AAX_PCM8U:  unsigned, 8-bits per sample",
   "AAX_PCM16U: unsigned, 16-bits per sample",
   "AAX_PCM24U: unsigned, 24-bits per sample, 32-bit encoded",
   "AAX_PCM32U: unsigned, 32-bits per sample",
};

static const char* _mask_s[MAX_LOOPS] = {
    "Native format",
    "Unigned format",
    "Little endian, signed",
    "Little endian, unsigned",
    "Big endian, signed",
    "Big endian, unsigned"
};

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
      aaxBuffer buf = bufferFromFile(config, infile);
      if (buf)
      {
         int no_samples, no_tracks, freq;
         aaxEmitter emitter;
         int q, state, fmt;

         /** emitter */
         emitter = aaxEmitterCreate();
         testForError(emitter, "Unable to create a new emitter");

         res = aaxEmitterAddBuffer(emitter, buf);
         testForState(res, "aaxEmitterAddBuffer");
         printf("original format\n");

         /** mixer */
         res = aaxMixerInit(config);
         testForState(res, "aaxMixerInit");

         res = aaxMixerRegisterEmitter(config, emitter);
         testForState(res, "aaxMixerRegisterEmitter");

         res = aaxMixerSetState(config, AAX_PLAYING);
         testForState(res, "aaxMixerStart");

         no_samples = aaxBufferGetSetup(buf, AAX_NO_SAMPLES);
         no_tracks = aaxBufferGetSetup(buf, AAX_TRACKS);
         freq = 0.6f*aaxBufferGetSetup(buf, AAX_FREQUENCY);

         res = aaxBufferSetSetup(buf, AAX_FREQUENCY, freq);
         no_samples = aaxBufferGetSetup(buf, AAX_NO_SAMPLES);
         for (q=0; q<MAX_LOOPS; q++)
         {
            fmt = 0;
            printf("%s\t\t(%x)\n", _mask_s[q], fmt + _mask_t[q]);
            do
            {
               int blocksz, nfmt = fmt + _mask_t[q];
               aaxBuffer buffer;
               void** data;
               if (q)
               {
                  if (q>1 && fmt == 0) continue;
                  if (fmt >= AAX_FLOAT && nfmt & AAX_FORMAT_UNSIGNED) break;
                  if (fmt >= AAX_MULAW) break;
               }

               /** schedule the emitter for playback */
               res = aaxEmitterSetState(emitter, AAX_PLAYING);
               testForState(res, "aaxEmitterStart");
         
               /** simultaniously convert the buffer format */
               res = aaxBufferSetSetup(buf, AAX_FORMAT, nfmt);
               testForState(res, "aaxBufferSetup(AAX_FORMAT)");

               blocksz = aaxBufferGetSetup(buf, AAX_BLOCK_ALIGNMENT);

               data = aaxBufferGetData(buf);
               testForError(data, "aaxBufferGetData");

               buffer = aaxBufferCreate(config, no_samples, no_tracks, nfmt);
               testForError(buffer, "aaxBufferCreate");

               res = aaxBufferSetSetup(buffer, AAX_FREQUENCY, freq);
               testForState(res, "aaxBufferSetSetup(AAX_FREQUENCY)");

               res = aaxBufferSetSetup(buffer, AAX_BLOCK_ALIGNMENT, blocksz);
               testForState(res, "aaxBufferSetSetup(AAX_BLOCK_ALIGNMENT)");

               res = aaxBufferSetData(buffer, *data);
               testForState(res, "aaxBufferSetData");
               free(data);

               res = aaxEmitterAddBuffer(emitter, buffer);
               testForState(res, "aaxEmitterAddBuffer");

               printf("    %s\n", (nfmt & AAX_FORMAT_UNSIGNED)
                                    ? _format_us[fmt & AAX_FORMAT_NATIVE]
                                    : _format_s[fmt & AAX_FORMAT_NATIVE]);
               do
               {
                  nanoSleep(5e7);
                  state = aaxEmitterGetNoBuffers(emitter, AAX_PROCESSED);
               }
               while (state == 0);

               res = aaxEmitterRemoveBuffer(emitter);
               testForState(res, "aaxEmitterRemoveBuffer");

               res = aaxBufferDestroy(buffer);
            }
            while (++fmt < AAX_FORMAT_MAX);
         }

         res = aaxMixerDeregisterEmitter(config, emitter);
         res = aaxMixerSetState(config, AAX_STOPPED);
         res = aaxEmitterDestroy(emitter);
         res = aaxBufferDestroy(buf);
      }
   }
   while (0);

   res = aaxDriverClose(config);
   res = aaxDriverDestroy(config);


   return 0;
}
